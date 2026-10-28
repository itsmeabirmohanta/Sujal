# VRITRA flight firmware 6.1.0 — build and bring-up

Team 2026-INSPACe-CAN-7USAT-025 · Teensy 4.1 flight computer.
Gateway lives in [`../vritra_gs/`](../vritra_gs/).

**Design rationale, guideline citations and every open conflict are in
[COMPLIANCE_AUDIT.md](../COMPLIANCE_AUDIT.md).** This file is just how to build
and fly it.

---

## Modules

| File | Owns |
|---|---|
| `Config.h` | pins, assigned RF plan, wire format, every tunable |
| `Base36.h` | wire codec + scale table — **byte-identical copy in `../vritra_gs/`** |
| `Sensors.*` | BMP585, ICM-20948, AK09916, DS3231, L89 GNSS, bus sense |
| `Radio.*` | LR1121 half-duplex transceiver state machine |
| `DataLogger.*` | SdFat/SDIO block-aligned flight recorder |
| `Motor.*` | TB6612FNG gyro wheel with soft start |
| `vritra_fc.ino` | flight FSM, packet assembly, commands |

> Arduino requires a sketch file to share its folder's name, so the main file is
> `vritra_fc.ino`, not `main.ino`. Module separation is otherwise as specified.

## Build

Arduino IDE / Teensyduino, board **Teensy 4.1**, CPU **600 MHz**, USB **Serial**.

| Library | Used for |
|---|---|
| RadioLib (≥ 6.x) | LR1121 |
| SdFat | SDIO recorder |
| Watchdog_t4 | hardware WDT — optional, guarded by `__has_include`; without it the build still works and reports `WDOG_OK = 0` |

**Removed** dependencies: `TinyGPSPlus`, `7Semi_BMP585`, `7Semi_ICM20948`,
`RTClib` — all replaced by direct register access. TinyGPSPlus in particular
cannot parse `$GSV`, so per-satellite SNR and the NavIC `$GIGSV` count (the two
measurements that tell you *why* a satellite count is what it is) are
unreachable through it.

Gateway: Arduino IDE, ESP32 board, RadioLib only.

## Assigned RF plan — do not change without a new allocation document

867.00 MHz main · 865.75 MHz backup · **SF10** · BW 125 kHz · CR 4/5 ·
sync **0x98** · **+10 dBm on pad, +14 dBm in flight** (staged automatically).
Transmission stops **90 min after launch detection**; the receiver stays armed.

## Harness revision 2026-08 — check this before debugging anything

Five assignments moved and **three pins were reused for different signals**
(24, 30, 22). A board wired to the previous revision still compiles and boots,
then fails in two ways that look unrelated:

| Symptom | Cause |
|---|---|
| `[RF] begin FAILED code=-2` | LoRa on old pins (CS 24, BUSY 31, RST 30, IRQ 32) |
| `[GNSS] NO DATA`, all else healthy | GNSS still on Serial1 (RX 0 / TX 1) |

| | was | now |
|---|---|---|
| LR1121 CS | 24 | **29** |
| LR1121 BUSY | 31 | **24** |
| LR1121 RESET | 30 | **25** |
| LR1121 DIO9 | 32 | **30** |
| GNSS port | Serial1 (0/1) | **Serial2 (RX 7 / TX 8)** |
| GNSS RESET_N | 22 | **6** |
| DS3231 SQW | 2 | **22** |
| BMP585 INT | — | **0** (input only, still polled) |
| ICM-20948 INT | — | **41** (input only, still polled) |

SPI1 (MOSI 26, SCK 27, MISO 39) and the TB6612 (33/34/35/36) did not move.
The firmware prints the whole map as two `// [PINS]` lines at boot — read them
against the silkscreen before touching code.

**The gateway did not change.** `vritra_gs` is an ESP32 on its own VSPI pins
(CS 5, SCK 18, MOSI 23, MISO 19, RST 14, BUSY 27, IRQ 26) and none of the above
applies to it.

## No `delay()`

`setup()` arms non-blocking init state machines and returns; `loop()` is a flat
pump. The only busy-wait in the project is 90 µs of I²C bus-recovery clock edges
in `Sensors.cpp`, which cannot be a millis() machine. A `delay()` in setup
desynchronises the LR1121's BUSY handshake and DIO9 interrupt; a `delay()` in
loop drops NMEA bytes and stalls the 25 Hz barometer cadence.

## GNSS fast re-lock

The L89 is assumed to have **no backup supply on V_BCKP**, so every power-down
wipes its ephemeris, its almanac, its last position *and* its internal clock.
Left alone it therefore performs a full cold start every single time — it does
not know the date, cannot predict which satellites are up, and has to sweep the
whole code/Doppler space blind. That is the 30–60 s wait.

Two things on this board survive a power cycle, and both are now used:

| what | where it survives | how it gets set |
|---|---|---|
| accurate UTC | **DS3231**, own coin cell, ±2 ppm | written from GNSS on the first fix of the *previous* run |
| last position | `GPSAID.TXT` on the SD card | refreshed every 60 s while a good fix is held |

At boot the flight computer reads both back and hands them to the receiver
(`$PMTK740` time aid, `$PMTK741` position + time aid) before it starts
searching, paced one command per pass with no `delay()`. Time + position is a
**warm start** — roughly 15–30 s instead of 30–60 s.

A *hot* start (1–5 s) additionally needs valid ephemeris, which expires in ~4 h
and cannot survive a power cycle without receiver backup power. Warm is the
honest ceiling on this hardware. **Fitting a supercap or coin cell to V_BCKP is
the one change that would beat anything firmware can do here.**

Guards that matter:

- A stale position is **worse** than none — a receiver aided with where it was
  two weeks and 400 km ago searches the wrong Dopplers and takes *longer*.
  Stored positions older than 14 days are refused, which is only possible
  because the aid record stamps itself with the RTC epoch.
- If the DS3231 reads something implausible (dead cell → 2000-01-01, or a torn
  I²C read), it is re-seeded from the compile-time stamp `__DATE__`/`__TIME__`,
  converted IST→UTC. Accuracy is minutes, not seconds — enough to stop aiding
  being actively harmful, and replaced by real GNSS UTC on first fix.
- **The RTC stores UTC, never local time.** GNSS time is UTC, the graded
  telemetry is UTC, and the aiding commands take UTC. `TZ_OFFSET_MIN` (330 for
  IST) derives local time for display and the CSV's `LOCAL_TIME` column only.

Whether the receiver actually honours those PMTK commands is **measured, not
assumed**: `TTFF_MS` latches the time to first fix on every boot and the console
prints whether the attempt was aided —

```
// [GNSS] FIRST FIX at 18.4 s  (AIDED warm start, 9 sats, HDOP 1.2)
```

Compare an aided boot against an unaided one and the answer is in the log.

## Storage

- `FLTnnn.BIN` — 128-byte records at 20 Hz. Four records fill one 512-byte block
  exactly, so the card never does a read-modify-write. Each record carries a
  magic and its own CRC-16, so a torn write costs one record and a decoder can
  resynchronise by scanning for the magic.
- `FLTnnn.CSV` — **20 Hz** text mirror, separate file, **106 columns**.
  Columns 0–89 are `vritra_gcs.py`'s SCHEMA in SCHEMA order, so the file drops
  straight into the same tools as the graded CSV; columns 90–105 are
  flight-computer diagnostics (`LOCAL_TIME`, `TTFF_MS`, `PAD_PRES_PA`,
  `I2C_FAULTS`, …) appended at the end so a positional reader of the first 90
  is unaffected.
  **Sensors that are not fitted are written as `-`, never `0`** — the same rule
  the GCS follows for the em-dash. The full prototype-to-final sensor set is
  present as columns either way, so the layout does not change when a sensor is
  finally fitted; only the `-` becomes a number.
  ~700 B/line at 20 Hz ≈ 50 MB/hour, i.e. 0.16 %/hour of the 32 GB card. That
  is why it is plain text and not compressed — the binary log is already the
  compact, CRC-protected record.
- `GPSAID.TXT` — last known position + the UTC epoch it was taken at, refreshed
  every 60 s while a good fix is held (≥4 sats, HDOP ≤ 5). Written to a temp
  file and renamed, so a power cut cannot leave it missing. Read back at boot
  and fed to the receiver — see **GNSS fast re-lock** below.
- 32 MB pre-allocated contiguous (≈3.6 h at 20 Hz), so no FAT write ever happens
  mid-flight.
- `sync()` every 2000 ms, plus an immediate flush on touchdown and before any
  commanded reset. That is what forces the card's volatile write cache to
  physical flash and makes the log survive a battery disconnect at chute
  snap-open.
- `SdioConfig(FIFO_SDIO)`, not DMA — it does not contend with the SPI1 DMA
  channels the LR1121 uses, and at ~2.5 kB/s we are far below the card's limit.

## Bring-up order

1. **Gateway first.** Flash `vritra_gs.ino`, confirm the `[RF] HW=… DEV=… FW=…`
   line. If that never prints, nothing else is worth trying.
2. **Antenna on ANT2** (LORA_ANT, sub-GHz) on **both** modules. ANT1 is the
   2.4 GHz port and sits right next to it. The RadioMaster dual-band whip is
   single-feed, so it reaches one port only — ANT2, and its 2.4 GHz resonance
   goes unused. If you have the RP-SMA version you need a u.FL→RP-SMA pigtail;
   the IPEX1 version plugs straight in and saves 0.4 dB plus a joint that has
   to survive 30 g.
   **Keep the radiating element clear of the aluminium plates** — λ/4 at
   867 MHz is 86.5 mm, which does not exist inside a 150 mm tube. Mount it
   axially, outside the stack, on glass fibre. See
   [COMPLIANCE_AUDIT.md](../COMPLIANCE_AUDIT.md) §3.6.
3. Flash `vritra_fc.ino`. The `//` status line prints once a second with every
   subsystem's state: sensors, pressure, AGL, bus volts, fix quality, SNR,
   dropouts, TX/fail/cmd counts, packet length, SD state, I²C fault counters.
   **Check that both boards print the same `[WIRE] base-36, schema sum`
   value.** If they differ, the two copies of `Base36.h` have drifted and every
   decoded field is silently wrong. Re-copy the file from `vritra_fc/` to
   `vritra_gs/` and reflash both.

   What matters is that the two boards AGREE, not that they print any
   particular constant — the sum is a function of the scale tables, the frame
   map, the frame count and `B36_WIRE_LAYOUT`, so it changes legitimately
   whenever any of those change. This line used to name `0x007C` as the
   expected value; that was stale (it predates the 19 → 16 frame change) and
   an operator checking against it would have read a perfectly healthy pair as
   drifted. Record whatever the current build prints, here, at the same time as
   you change the tables — never carry a remembered value across a revision.
4. **The CanSat boots silent** (guideline §6.1.v). Send `CMD_TX_ON` from the
   gateway to start telemetry — the gateway repeats each command six times and
   falls back to a 1.2 s blind timer, which is the path that reaches a silent
   vehicle. `CMD_TX_ON` **also arms the flight state machine**; there is no
   separate `CMD_ARM` any more. Watch for `[FSM] ARMED by CMD_TX_ON`.
5. `CMD_CALIBRATE` with the vehicle standing in its launch attitude. It zeroes
   three independent things and reports each one separately:
   - the **attitude datum** immediately — whatever pose it is in becomes
     0/0/0 (`[ATT] datum CAPTURED ON COMMAND`);
   - the **barometric pad reference**, averaged over 2 s of conditioned
     pressure (`[CAL] baro pad reference = … Pa`), after which AGL reads 0.0 m;
   - **gyro bias and accel scale**, stillness-gated — hold it still. If it
     cannot settle it now says so (`[CAL] gyro TIMED OUT`) instead of failing
     in silence, and the other two still succeed.
6. `CMD_TX_OFF` then `CMD_TX_ON` proves the silent-receive path end to end.
   `CMD_TX_OFF` does **not** disarm.

Every command also works over the Teensy's own USB serial (`CMD_CALIBRATE`,
`CMD_GYRO_SPD 40`, …), so the whole flight computer is testable with no radio.

## Not flight-qualified yet

`FSM_LAUNCH_*`, `FSM_APOGEE_*` and `FSM_LANDED_*` are marked UNVERIFIED and need
your real pad profile — only `FSM_SEC_EJECT_AGL_M = 600` is fixed by the
guideline. `GYRO_WHEEL_PCT = 70` has no angular-momentum calculation behind it.
The §5.2.v audio beacon is now **driven** — an SM400204-1 speaker on a MAX98357A
I2S amplifier (SAI2, pins 2/3/4, enable on 37; see `Beacon.cpp`) — but it does
**not** meet the level the guideline asks for. The driver is rated 99 dB SPL at
100 mm, which is about 79 dB at the 1 m the requirement is measured at, and at
most ~84 dB(A) driven to the amplifier's full 3.2 W. The requirement is
**≥ 92 dB(A) at 1 m**, so there is a ~8–13 dB shortfall that no firmware change
can close: it needs a louder transducer, not a different tone. Firmware does
what it can — the 2.8/3.5 kHz warble sits near the ear's peak sensitivity, where
A-weighting is generous, instead of on the speaker's 220 Hz resonance, where it
is docked ~11 dB.
See [COMPLIANCE_AUDIT.md](../COMPLIANCE_AUDIT.md) §5 for the rest.
