# VRITRA CDR test bench

Team 2026-INSPACe-CAN-7USAT-025 · Lovely Professional University

**One sketch per test.** Each is standalone, includes nothing from `vritra_fc/`,
and prints only its own subsystem's parameters. Flash one, read it, move on.

Nothing in `vritra_fc/`, `vritra_gs/` or `vritra_gcs.py` was modified to
produce any of this.

> **Not compiled.** There is no C++ toolchain on this machine. Every sketch is
> written and structurally checked — brace balance, pin map agreement with
> `Config.h`, and register values copied from the already-working
> `vritra_sensor_test.ino`. Build each one in the Arduino IDE before trusting a
> reading.

---

## 1. The sketches

Open the Serial Monitor at **115200**, line ending **Newline**.

| Sketch | Prints | Covers |
|---|---|---|
| `test_barometer` | pressure, temperature, height, noise spread | barometer accuracy |
| `test_imu` | accel XYZ, gyro XYZ, magnitude, tilt, die temp | inertial bring-up |
| `test_magnetometer` | field XYZ, total, heading, dip, hard-iron cal | magnetometer bring-up |
| `test_rtc` | date, time, local time, die temp, reset survival | clock across resets |
| `test_gnss` | position, sats, NavIC, best signal, HDOP/VDOP, time to first fix, drift | satellite fix |
| `test_busvoltage` | raw counts, bus volts, per cell, charge, endurance run | power endurance |
| `test_sdcard` | card info, 200-row write and read back, sustained rate | onboard backup, CSV generation |
| `test_flash` | JEDEC identity, size, erase/write/verify | 7Semi serial flash |
| `test_buzzer` | frequency sweep, search pattern, continuous tone | beacon sound level |
| `test_switch_led` | debounced switch state, indicator, bus volts | switch and LED |
| `test_motor` | power, direction, bus sag, measured spin rate | gyro wheel |
| `test_fsm` | every state transition, with injected values | state machine |
| `test_telemetry_csv` | header, one line, column-by-column audit | telemetry format, 15 fields |
| `test_baro_600m` | injected pressure profile, recovered altitude, error | 600 m simulation |
| `test_descent_rate` | live 1 Hz stream at descent altitudes, cadence stats | 10 m/s descent |
| `test_rf_tx` | real sensor telemetry, packet contents, bytes, airtime; logs every reading to the card with an RTC timestamp | CanSat-side range test |
| `test_rf_rx` | packet contents, RSSI, SNR, received/lost, best/worst RSSI; sends start / stop / calibrate | ground-side range test + commander |

`../vritra_sensor_test/` already does a combined I²C scan and quick per-sensor
check. Keep it — it is the fastest "is anything alive" test.

### RF range test — `test_rf_tx` + `test_rf_rx`

These two grew from a quick bench-link check into the pair you take to the
field for the 2 km range test. They do not need `vritra_gcs.py` running — a
laptop with two serial monitors is enough — and every RF setting, pin and the
RF switch table is copied verbatim from `Radio.cpp` and `vritra_gs.ino`, so the
numbers track the real link closely. They are still not a substitute for the
§4 procedure with the full flight pair before flight, but for measuring range
and proving the command path they are the right tool.

What the CanSat side (`test_rf_tx`, Teensy) does now:

- **Real sensors, not made-up numbers.** It reads the barometer, the inertial
  unit and the satellite receiver and builds a 99-byte packet the same size and
  shape as flight telemetry, so the airtime and error rate match the real link.
- **Logs to the SD card** — a new `RNGnnn.CSV` per run — with a real timestamp
  read from the DS3231. The clock is **only read, never written**; re-flashing
  the Teensy does not touch it either, so the time you set stays set.
- **The serial flash on SPI0 (pins 10–13) is left completely alone.** The radio
  is on SPI1, the card is on SDIO, so a range test cannot disturb it.
- **Boots silent** (guideline 6.1.v) and starts only on the `TX ON` command.

What the ground side (`test_rf_rx`, ESP32) does now:

- **Sends `TX ON` / `TX OFF` / `CALIBRATE`** and prints RSSI, SNR and packet
  loss for every packet, plus best/worst RSSI.
- **The command path is fixed.** Two things used to make commands "not take":
  the serial monitor's line-ending setting (fixed — it now also acts on a
  typing pause, so any setting works), and the flight timing that only listened
  one packet in three (the range-test CanSat now opens a listen window after
  *every* packet, at a slightly slower ~0.66 Hz slot chosen for exactly this).
- **It confirms.** The CanSat echoes the last command it obeyed inside its
  telemetry; the ground watches for that echo, prints "the CanSat confirmed …",
  and stops repeating. You get a real acknowledgement, not hope.

> The ~0.66 Hz slot is a deliberate range-test choice so the CanSat can listen
> every second. The graded 1 Hz link is the one in `vritra_fc`; this sketch
> trades a little cadence for a command path that lands first time at 2 km.

### Which antenna — use the RadioMaster

You have the RadioMaster dual-band whip and the little rubber duck that ships
with the Core1121-XF. **Use the RadioMaster, on the 900 MHz sub-GHz port
(ANT2), at both ends.** It is tuned for this band (3.58 dBi, SWR ~1.25 at
900 MHz), where the bundled duck is a generic, lower-gain part often cut for
915 MHz and mismatched at 867. Keep both whips vertical and parallel so they
are polarised alike — a whip has a deep null straight off its own tip, so never
point one at the other end. The frequency plan asks for a high-gain directional
antenna (a Yagi) on a mast at the ground station for 1 km+; if you get one
later, put it on the ground side and keep the RadioMaster on the CanSat. Keep
the rubber duck as the spare.

---

## 2. Wiring you have to add

### 7Semi serial flash — SPI0

The radio owns SPI1 (26 / 27 / 39). The card is on SDIO. SPI0 is free.

| Flash | Teensy 4.1 | Note |
|---|---|---|
| VCC | **3.3 V** | never 5 V |
| GND | GND | |
| CS | **10** | the only pin you may change |
| DI (MOSI) | **11** | fixed by hardware |
| DO (MISO) | **12** | fixed by hardware |
| CLK | **13** | fixed by hardware |
| WP | **3.3 V** | tie high |
| HOLD | **3.3 V** | tie high |

**Tie WP and HOLD high.** Left floating, the part reports its identity happily
and then silently refuses every write — which reads exactly like a dead chip.
If the identity is right but nothing writes, check these two pins first.

Pin 13 is also the on-board LED, so it flickers during flash access. Harmless,
and it is why `test_switch_led` uses an external LED on pin 14.

### Recovery beacon — pin 37

`Config.h` already reserves pin 37 as `PIN_BEACON`, commented out because no
sounder was fitted. **Do not drive the sounder from the pin** — a Teensy output
is 3.3 V and wants to stay under about 4 mA; anything reaching 92 dB draws far
more and the pin will fail.

```
pin 37 ──[100 Ω]──┬── gate    logic-level N-channel MOSFET
                  │           2N7002 / AO3400 / IRLML2502
                [100 kΩ]
                  │
                 GND ── source

     +5 V ── buzzer + ── buzzer − ── drain
                  └──[1N4148 across the buzzer]──┘   magnetic types only
```

The pull-down matters: without it the gate floats while the processor boots and
the sounder shrieks on every power-up.

- **Piezo** — sharply peaked at resonance; 700 Hz off can be 15 dB down, which
  is the whole margin. That is what the sweep is for.
- **Magnetic** — fit the flyback diode or the MOSFET eventually fails.
- **Active** (built-in oscillator) — ignores frequency, just sounds. Use the
  on/off pattern, skip the sweep.

If the sweep tops out below 92 dB that is a **hardware** result, not a firmware
one — you need a louder transducer or a higher rail, and the CDR should say so.

### Switch and indicator

| Signal | Pin | Wiring |
|---|---|---|
| Switch | **15** | one side to pin 15, other to GND (internal pull-up, closed = LOW) |
| Status LED | **14** | pin 14 → 330 Ω → anode, cathode → GND |

### Pin check against `Config.h`

Added here: `10, 11, 12, 13, 14, 15, 37`.
Flight map: `0, 6, 7, 8, 18, 19, 20/A6, 22, 24, 25, 26, 27, 29, 30, 33, 34, 35, 36, 39, 41`.
No overlap. Pin 37 is the commented-out `PIN_BEACON`, i.e. exactly the pin
reserved for this.

---

## 3. Pass criteria

| Test | Command | Pass |
|---|---|---|
| Barometer | — | spread < 30 Pa (≈ 0.1 m) |
| Inertial | — | total 1.00 ± 0.06 g at rest, rate < 2 °/s |
| Magnetometer | — | field 25–65 µT, no overflow |
| Clock | `s` to set | advances across a 30 s power removal |
| Satellite | `r` to reset | ≥ 6 sats, first fix < 60 s, drift ≤ 0.0001° |
| Bus voltage | `e` for endurance | ≥ 2 hours to 9.00 V |
| Card | `r`, `d` | 201 rows back, 17 fields each, no write over 200 ms |
| Flash | `r`, `d` | identity answers, erase/write/verify clean |
| Beacon | `s`, `p`, `c` | ≥ 92 dB(A) at 1 m |
| Switch | `t` | debounced change reported, indicator follows |
| Gyro wheel | `0`–`9`, `p`, `r` | ramps without the bus sagging below 9 V |
| State machine | `p`, `h`, `z` | all four transitions; secondary at 600 ± 10 m |
| Telemetry format | `f`, `n`, `s` | 17 columns, correct decimal places |
| 600 m profile | `r`, `t` | round-trip error < 1 m, trigger in window |
| Descent | `d`, `f`, `s` | ≥ 0.95 packets/s |

### Two honest limits

**The endurance test cannot measure the 3.3 V rail.** The converter references
against 3.3 V, so that rail reads full scale by definition — it is the ruler and
cannot measure itself. The test monitors the **battery bus** through the 100 k /
22 k divider on A6 and stops at 9.00 V (3.00 V/cell), which is what actually
determines endurance. Put a multimeter on the 3.3 V rail for that figure; it
should sit flat until the regulator drops out, and when it sags the processor
resets anyway.

**`test_descent_rate` measures generation, not reception.** Reception needs the
gateway listening. Run both and compare the counts — the difference is the loss.

---

## 4. Tests that use the real flight pair

Flash `vritra_fc` on the Teensy, `vritra_gs` on the ESP32, run `vritra_gcs.py`.

**Both boards must print the same `[WIRE] base-36, schema sum` at boot, and must
be reflashed together** — the wire layout changed. Expected `0x4FD3`.

1. **Bench link** — 1 m apart, antennas on **ANT2** (ANT1 is the 2.4 GHz port
   and sits right next to it). Send `CMD_TX_ON`. RSSI well above −60 dBm, packet
   counter advancing with no gaps.
2. **1 km line of sight** — ground antenna as high as possible and **tilted
   30–45° off vertical**, not straight up. A vertical whip has a deep null along
   its own axis; with the vehicle whip also vertical you get a null at both ends
   simultaneously, which is the classic apogee dropout.
3. **500 m obstructed** — same, through the terrain you expect at the site.
   Record RSSI and loss at each point.
4. **Live display** — ALTITUDE AGL, VERTICAL SPEED and the INERTIAL panel
   populate in SI units with live plots.
5. **Commands** — `CMD_TX_ON` starts telemetry *and* arms the state machine;
   `CMD_CALIBRATE` zeroes attitude, barometric pad reference and gyro bias,
   reporting each separately.
6. **CSV generation** — the ground station writes
   `Flight_2026-INSPACe-CAN-7USAT-025.csv` into `~/VRITRA_GCS_logs/`.

---

## 5. Where the data lives — verified against the firmware

| File | Written by | Rate | Contents |
|---|---|---|---|
| `FLTnnn.CSV` | flight computer | 20 Hz | **103 columns** — 87 schema + 16 diagnostics |
| `FLTnnn.BIN` | flight computer | 20 Hz | 128-byte CRC-16 records, 4 per 512-byte block |
| `GPSAID.TXT` | flight computer | 60 s | last position + UTC, for warm start |
| `CDRTEST.CSV` | `test_sdcard` | on demand | format proof |
| `ENDURE.CSV` | `test_busvoltage` | 30 s | endurance log |
| `RNGnnn.CSV` | `test_rf_tx` | 1 s | range-test record: real sensors + RTC timestamp + last uplink RSSI |
| **`Flight_<TEAM_ID>.csv`** | **ground station** | 1 Hz | **the graded file**, in `~/VRITRA_GCS_logs/` |

Every fitted sensor reaches the card: barometer (pressure, temperature, AGL,
ASL, vertical speed, pad reference), inertial unit (three axes each of
acceleration, rate and field, plus spin rate and magnitude), fused attitude
(roll, pitch, yaw, quaternion), satellite receiver (position, altitude, ground
speed, time, satellites, NavIC count, HDOP, VDOP, fix quality, geoid
separation, constellation mask, best signal, dropouts, worst gap, time to first
fix, aiding flag), clock (UTC, local, die temperature, sync flag), radio (RSSI,
SNR, frequency), card utilisation, I²C fault and recovery counters, state and
flags.

**Unfitted sensors write `-`, never `0`** — so "reported zero" stays
distinguishable from "not there".

**The card file is the onboard backup required by 5.1.xi. The graded file is
the ground station's. Do not hand in the card file.**

### One gap found

The binary record carries **no attitude** — `LogRecord` in `DataLogger.h` has
acceleration, rate and field but no roll/pitch/yaw and no quaternion, with only
5 reserved bytes where three floats need 12. The 20 Hz CSV mirror *does* have
all six, so nothing is lost for analysis. Worth knowing before the CDR; not
worth changing, because 128 bytes is what makes four records fill one 512-byte
block with no read-modify-write.

---

## 6. On the readout

Everything printed for a person to read is written as a person would write it —
`Accel X`, `Bus voltage`, `Sats in use`, `Field total`. Underscored names appear
in exactly one place: inside a CSV line, where the text *is* the graded data and
`ACC_X,ACC_Y,ACC_Z` is what the judges' parser expects. Do not tidy those.
