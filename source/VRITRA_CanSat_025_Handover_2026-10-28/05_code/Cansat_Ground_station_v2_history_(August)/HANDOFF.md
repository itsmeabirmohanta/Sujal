# VRITRA — Session Handoff, 2026-08-07

**Team 2026-INSPACe-CAN-7USAT-025 · Lovely Professional University**

This file exists so a new assistant (or a teammate, or you in three weeks) can
pick this project up without re-deriving anything. It records what was changed
on 2026-08-07, **why**, what was verified, and what is still open.

If you are an assistant reading this cold: read this file, then
[`PROJECT_HISTORY.md`](PROJECT_HISTORY.md) for the longer backstory, then
[`vritra_fc/README.md`](vritra_fc/README.md) for how to actually operate the
flight computer. Do **not** start changing firmware before reading the
"Load-bearing facts" section — several things in this codebase look wrong and
are not.

---

## 1. The system in one page

| Piece | What it is |
|---|---|
| `vritra_fc/` | Flight computer. Teensy 4.1 @ 600 MHz. Sensors, radio, SD recorder, flight state machine. |
| `vritra_gs/` | Ground-station **gateway**. ESP32 + LR1121. Receives the downlink, decodes base-36, emits a 90-column CSV line on USB. Sends uplink commands. |
| `vritra_gcs.py` | Ground-station **GUI**. Single-file PyQt5, 5910 lines. Parses the gateway's CSV, drives every panel. |
| `sensor_tests/` | Standalone bring-up sketches. `test_esp32_ping_rx` / `test_teensy_ping_tx` are the "is the RF chain alive at all" pair. |

**Hardware actually fitted right now:** BMP585 barometer, ICM-20948 IMU (+AK09916
mag), DS3231 RTC, Quectel L89 GNSS (Airoha AG3335M), LR1121 radio on
Core1121-XF, TB6612FNG + N20 gyro wheel, 32 GB microSD. RadioMaster dual-band
whip antenna (3.58 dBi @ 900 MHz) on **ANT2** at both ends.

**Not fitted** (but present as columns/fields throughout): LSM6DSO32 high-g,
INA226, HDC3020, BMV080 PM, SCD41, ENS160, MQ131, BH1750, ML8511, AS7343
spectral, AS3935 lightning, servos, cameras, W25Q64 flash.

**RF plan is assigned, not chosen.** Frequency allocation plan row 025:
867.00 MHz main / 865.75 MHz backup, SF10, BW 125 kHz, CR 4/5, sync 0x98,
+10 dBm on pad / +14 dBm in flight. *Never* change these without a new
allocation document. The user's explicit instruction on 2026-08-07 was to stay
strictly compliant — no high-power bench-test profile, real frequency always.

---

## 2. What changed on 2026-08-07, and why

Ten items. Every one of these was a real defect with evidence in the repo, not
a style preference.

### 2.1 `CMD_CALIBRATE` appeared to be ignored

**Root cause:** `CAL_STILL_DPS` was `3.0` dps. `Config.h` records this exact
part's *measured* gyro noise floor as **4.3–11.2 dps pp**. The stillness gate
compares peak-to-peak spread against that threshold, so on a perfectly
motionless bench the spread is always larger than the gate. It could never be
satisfied. `CAL_TIMEOUT_MS` fired after 20 s, dropped back to `CAL_IDLE` keeping
the old bias, and **printed nothing**. The command was received and executed
every time — it just could not converge, and said so to nobody.

**Fixed:** threshold raised to `15.0` dps (≈1.34× the worst measured axis, still
well below the 10–40 dps of a deliberate hand movement). Every exit path from
the calibration state machine now prints.

**Also:** the attitude datum was waiting `ATT_ZERO_DELAY_MS` (3 s) even for a
*commanded* re-zero. That delay exists so a noisy first sample at boot isn't
frozen in as "0 forever" — it is meaningless when the operator has deliberately
placed the vehicle and the filter has been converged for minutes. Commanded
datum is now captured on the next IMU sample. `zeroAttitude(bool immediate)`.

**Not touched, deliberately:** the `atan2` formula, the 90° mounting swap, and
`ATT_SIGN_*`. Those were hard-won from a six-position static test
(`docs/IMU_AXIS_CALIBRATION_2026-08-06.md`) and are correct.

### 2.2 Barometer altitude wrong and imprecise

**Root cause — the big one:** there were **two** pad-reference variables that
were never connected. `padPresPa` (persisted to EEPROM) and a file-scope
`groundPressurePa` in `Sensors.cpp` (the one `getAltitudeAGL()` actually read).
`restoreCalibration()` on boot wrote only the first. So **every reboot silently
reverted the altitude datum to `101325.0` Pa** — standard sea level. At the
logged 97.3 kPa pad pressure that is ~340 m of constant error, and it is the
direct cause of the GCS showing `ALTITUDE AGL −10.40 m` on a vehicle sitting on
the ground and `BARO VS GNSS ΔALT 263.70 m`.

Four more faults in the same function:
- **It blocked.** 16 × `bmpRead()` with `delay(20)` = 320 ms of hard stall
  inside a command handler, in a project whose stated policy is no `delay()`.
- **It averaged raw reads**, bypassing the median-of-5 and glitch gate. One torn
  I²C burst is ~1024 Pa ≈ 90 m; at 1/16 weight that bakes ~5.6 m of *permanent*
  offset into the reference.
- **It oversampled.** 20 ms spacing against a 25 Hz (40 ms) ODR means every
  second read returns the same unchanged conversion — ~8 independent samples,
  perfectly correlated, so no √N noise reduction.
- **Fixed `44330.8` coefficient.** That constant is `T0/L` at the ISA 15 °C.
  At the measured 28.9 °C the true scale height is `302.05/0.0065 = 46469`, so
  **every altitude read ~4.8 % low** — ~48 m at the 1000 m apogee, ~29 m at the
  600 m secondary-deployment trigger that guideline §5.4.vi allows ±10 m on.

**Fixed:** one owner (`setPadReference()`), non-blocking gated survey over
`BARO_CAL_MS` of *conditioned* pressure, temperature-compensated hypsometric
formula using `padTempC` captured with `padPresPa`, and the EEPROM restore wired
into the actual altitude path.

**Safety change:** the boot survey now runs **only if no valid reference was
restored**. The old code re-surveyed unconditionally, which means a brownout at
900 m would have re-zeroed the altimeter *to the pressure at 900 m* — altitude
reads 0, the FSM sees no descent, and the 600 m trigger can never fire.

### 2.3 GCS pad origin

`lat0/lon0` were assigned from the **first** GNSS fix — the least accurate one a
receiver ever produces, and if the first fix arrived after launch the "pad" was
placed wherever the vehicle happened to be. Now **surveyed**: a running mean
over fixes received while the vehicle reports `STATE=BOOT`, frozen the moment it
leaves BOOT, capped at `PAD_SURVEY_MAX = 60` samples.

`takeoff_alt` had the same shape of bug — it latched at "the first sample where
`alt > 5`", which is a sample taken *during ascent*. Now anchored on
`STATE=BOOT`.

### 2.4 Commands

Removed **CMD_ARM, CMD_DISARM, CMD_FORCE_EJECT, CMD_FORCE_SEC_EJ,
CMD_BRAKE_DEPLOY** from all three ends. Three drove actuators this vehicle does
not carry; the arming pair is superseded.

**IDs 4–8 are retired, not deleted.** The uplink carries the command as an
integer, so closing the gap would silently redefine every id above it — a
gateway and flight computer flashed from different commits would execute the
*wrong* command rather than failing. Never reuse 4–8; new commands take 19+.

**`CMD_TX_ON` now also arms the FSM.** Guideline §6.1.v already makes it the
last deliberate operator action before flight. `CMD_TX_OFF` does **not** disarm —
disarming at 800 m would strand the vehicle in `FS_LAUNCH` and lose the 600 m
transition.

Added the six commands the GCS drop-down was missing (`GYRO_ON/OFF/SPD`,
`MAG_CAL`, `GPS_RESET`, `NEW_MISSION`) so every command the FC supports is
reachable.

### 2.5 NavIC was never enabled

`$PMTK353,1,1,1,0,1` was commented "GPS + GLONASS + Galileo + NavIC". MediaTek's
actual parameter order is `<GPS>,<GLONASS>,<GALILEO>,<GALILEO_FULL>,<BEIDOU>`.
That command enabled **BeiDou**. NavIC was never asked for — which is why
`NAVIC_SATS` read 0 on every flight.

The module answers `$PAIR021` at boot, so it is an **Airoha AG3335M speaking
PAIR**. The correct command is `$PAIR066,<GPS>,<GLONASS>,<Galileo>,<BeiDou>,
<QZSS>,<NavIC>` — NavIC is the 6th parameter. Now sent as
`PAIR066,1,1,1,0,0,1` (4 constellations, respecting the documented channel-budget
note), with `$PAIR001` ACKs parsed and reported. PMTK353 kept as a fallback.

Also fixed: the GSV cycle commit was anchored on `$GPGSV` being present, making
GPS a prerequisite for reporting *any* other constellation. Now time-anchored.

> **Hardware caveat firmware cannot fix:** NavIC's civil signal is primarily
> **L5 (1176 MHz)**, not L1. If your GNSS patch is a plain L1 ceramic, NavIC is
> unreceivable no matter what the receiver is configured to search for, and
> `NAVIC_SATS` stays 0 with every command ACKed. **Confirm the antenna is
> L1+L5 before concluding anything from a zero count.**

### 2.6 TX power step was being silently lost — cost 4 dB of every flight

`setTxPower()` early-returned on `dbm == txPower_` but only wrote the chip when
idle. At the `FS_BOOT → FS_LAUNCH` transition the radio is in `RS_TXING` roughly
19 times out of 20 (a SF10 downlink occupies ~944 ms of a 1000 ms cycle), so
`txPower_` was set to 14 while **the chip stayed at 10** — and because
`txPower_` now equalled 14, every later call hit the early-out and never
retried.

**The vehicle flew the entire mission at +10 dBm believing it was at +14, and
said so on the console.** 4 dB is a factor of 2.5 in received power and ~37 % of
range. Fixed with a `powerDirty_` flag retried at every idle pass.

### 2.7 `CMD_FAILOVER` was a one-way trip

It moved the CanSat to 865.75 MHz, but the gateway did **not** follow — the
operator had to type `FAILOVER_LOCAL` separately. Forget it, or reboot the
gateway first, and the two ends sit on different channels with every command
capable of repairing it (including `CMD_FAILOVER`) having to cross the link that
no longer exists. Unrecoverable, with two healthy radios.

Now: the gateway follows automatically after the last uplink repeat, **and** the
FC independently reverts to main if it hears no uplink on backup within
`FAILOVER_PROOF_MS` (120 s). Two independent mechanisms — one makes the normal
case work, the other makes the abnormal case survivable.

### 2.8 GNSS fast re-lock

The L89 is assumed to have **no backup supply on V_BCKP**, so every power-down
wipes ephemeris, almanac, position *and* its clock → full cold start every time.

Two things survive a power cycle and are now both used:

| what | survives in | set by |
|---|---|---|
| accurate UTC | **DS3231**, own coin cell, ±2 ppm | GNSS, on the first fix of the *previous* run |
| last position | `GPSAID.TXT` on SD | refreshed every 60 s while a good fix is held |

Both are fed to the receiver at boot (`$PMTK740` time aid, `$PMTK741` position +
time aid), paced one command per pass, no `delay()`. **Warm start: ~15–30 s
instead of 30–60 s.**

Old aid code had four faults, all fixed: `delay(20)` in the flight loop;
`remove()`-then-`create()` leaving a window where a power cut destroys the aid
file; no staleness check; no quality gate. Now atomic (temp + rename), refuses
positions older than 14 days, requires ≥4 sats and HDOP ≤ 5.

**The DS3231 was never being set at all.** Now seeded from `__DATE__`/`__TIME__`
(build machine local, converted IST→UTC) when it reads something implausible,
then overwritten with real GNSS UTC on first fix. **It stores UTC, never local** —
`TZ_OFFSET_MIN = 330` derives IST for display and the CSV's `LOCAL_TIME` column.

Whether the receiver honours the PMTK aiding commands is **measured, not
assumed**: `ttffMs` latches time-to-first-fix on every boot and the console
prints whether the attempt was aided.

> A supercap or coin cell on **V_BCKP** is the one hardware change that would
> beat anything firmware can do here (would enable a 1–5 s hot start).

### 2.9 SD CSV

Was 1 Hz / 32 columns. Now **20 Hz / 106 columns**. Columns 0–89 are the GCS
SCHEMA in exact order (so the file is interchangeable with the graded CSV);
90–105 are FC diagnostics appended at the end. **Not-fitted sensors write `-`,
never `0`** — same rule the GCS follows for the em-dash. ~50 MB/hour = 0.16 %/hour
of the 32 GB card, which is why it is plain text and not compressed. The binary
log remains the primary record (CRC-16 per record, block-aligned).

### 2.10 INERTIAL panel showing em-dashes

**Not a bug — latency.** Those nine tiles read the *rotating optional* fields,
which arrive one per packet. At `PKT 3`–`PKT 10` most had genuinely never
arrived.

But it exposed real waste: **AX/AY/AZ were duplicates of the mandated
ACC_X/Y/Z** — same three variables, g vs m/s². Three of nineteen rotation slots
re-sending, once per 19 packets, a number already in every packet.

The gateway now **derives** them (÷ 9.80665). ACCEL X/Y/Z populate every packet,
and the rotation drops **19 → 16** so GX/GY/GZ and MX/MY/MZ arrive 16 % sooner.
Zero airtime cost.

Also defused: the attitude Euler scale was written `B36_OPT[5]`, borrowing the
GX row from the rotation table. That table is a *schedule* — and it was just
reordered (GX moved 5→2), so the borrow would now read MX. Now a named
`B36_ATT`.

---

## 3. Load-bearing facts — do not "clean these up"

These look wrong and are not. Each cost real debugging time.

1. **`setOutputPower(N, true)`** — the second arg selects the PA path and must
   stay `true`. Core1121-XF wires HP only; LP is physically unconnected.
2. **`setRfSwitchTable()` is called before *and* after `begin()`.** `begin()`
   clears it. Calling it only before fails with −707.
3. **TCXO is 1.6 V.** Deriving it from the schematic gives 0.0f and is wrong.
4. **SPI1 MISO is remapped to pin 39** and stays there — the harness is
   physically wired to it. The original reason (pin 1 = GNSS TX) is obsolete;
   that is not a licence to change the pin.
5. **I²C stays at 100 kHz.** 400 kHz breaks the ICM-20948 on this harness
   (ACKs its address, returns 0xFF on every register read).
6. **Only ONE IRQ action on the gateway's DIO9.** Registering both
   `setPacketReceivedAction` and `setPacketSentAction` corrupts the vector →
   `IllegalInstruction` panic. TX completion is polled via `digitalRead`.
7. **No `standby()` before `startTransmit()`** on the gateway — it leaves the
   radio in a state where every TX after the first fails with −1.
8. **The 90-byte / 99-byte airtime wall is real.** SF10 at 1 Hz cannot carry
   more. Base-36 exists for this reason.
9. **`ATT_SIGN_*` are all measured**, and roll/pitch are `+1` because the real
   error was an axis *swap* (90° mount rotation), not a sign. A swap cannot be
   expressed as a negation — that is why flipping signs never converged.
10. **The brownout detector is disabled on the ESP32 gateway** because the HP PA
    draws ~300 mA on TX. The real fix is a 220–470 µF + 100 nF cap across the
    LR1121's 3V3/GND — **the user reports this is now fitted.**

---

## 4. What is verified, and what is not

**Verified by script** (all in the scratchpad, re-runnable):

| check | result |
|---|---|
| `Base36.h` byte-identical in both sketches | md5 `17643a9f57e0` |
| `B36_OPT_N` == `N_FRAMES` (fc) == `N_FRAMES` (gs) | 16 / 16 / 16 |
| `B36_OPT[]` rows == `B36_OPT_MAP[]` == `B36_OPT_N` | 16 |
| UTC frame is last, maps to schema 78 | frame 15 → 78 |
| every map destination is a real schema index | all in range |
| `buildPacket` switch covers frames 0..14 + default | yes |
| GCS SCHEMA field count | 90 |
| CSV header cols 0–89 == SCHEMA exactly | exact |
| CSV emitter column count == header | 106 == 106 |
| every CSV `if`/`else` pair emits equal columns | yes |
| brace/paren balance, 11 edited files | all zero |
| `vritra_gcs.py` parses | yes |

**NOT verified — no toolchain on the dev machine:**

- **Neither sketch has been compiled.** All checks above are structural.
- Nothing has been flashed or run on hardware since these changes.
- The `$PAIR066` NavIC command is the documented Airoha constellation selector,
  but this specific module's acceptance of it is unconfirmed — the `$PAIR001`
  ACK parsing exists precisely to answer that on first boot.
- Whether `$PMTK740`/`$PMTK741` aiding is honoured by this receiver is
  unconfirmed — `TTFF_MS` exists precisely to measure it.

---

## 5. Verification scripts

Three throwaway checkers were written this session. They are worth keeping if
you touch the wire format again:

- `balance.py` — brace/paren balance. **Note:** strings must be stripped
  *before* line comments, because nearly every log string in this project
  literally begins with `"// "`. Getting that order wrong makes every
  heavily-logging file look unbalanced.
- `wirecheck.py` — cross-checks `Base36.h` ↔ `Config.h` ↔ gateway ↔ GCS SCHEMA.
- `csvcheck.py` — counts the columns `logCsv()` actually emits vs `CSV_HEADER`,
  and checks every `if`/`else` pair emits the same count.

They lived in the session scratchpad and are not in the repo. Recreate them from
the descriptions above if needed — each is <100 lines.

---

## 6. Open items

1. **Compile and flash both boards.** *Reflash them together* — the schema sum
   changed, and it now folds in the frame count, so a mismatched pair reports it
   at boot instead of silently decoding every optional value into the wrong
   field. Confirm both print the same `[WIRE] base-36, schema sum`.
2. **`EEPROM_MAGIC` is now layout 8** (added `padTempC`). First boot discards the
   old record and surveys a fresh pad reference. Intended.
3. **Confirm the GNSS antenna is L1+L5** or NavIC will never appear (§2.5).
4. **Consider a supercap on L89 V_BCKP** — the single biggest TTFF win available
   (§2.8).
5. `FSM_LAUNCH_*`, `FSM_APOGEE_*`, `FSM_LANDED_*` are still marked **UNVERIFIED**
   and need a real pad profile. Only `FSM_SEC_EJECT_AGL_M = 600` is fixed by the
   guidelines.
6. **`BUS_V` reads wrong on USB bench power** (0 V / 7.5 V / 11 V swings are
   supply noise through the divider, not a battery). It will read clean on the
   real 3S pack. The `RED_LOW` master alarm on the bench is expected.
7. **`PIN_BEACON` is still undefined.** Guideline §5.2.v mandates a ≥92 dB SPL
   audio beacon; the flag is plumbed through the firmware but drives only the
   on-board LED. This does **not** satisfy §5.2.v.
8. Compliance docs (`ANSYS_HFSS_GUIDE.md`, `COMPLIANCE_AUDIT.md`,
   `PROJECT_HISTORY.md`) still describe the PDR's 11 dBi Yagi. That was left
   deliberately — they document the PDR-mandated configuration, not the current
   bench setup. The *firmware* was updated to the RadioMaster whip.

---

## 7. Link budget, for reference

At +14 dBm with RadioMaster whips (3.58 dBi) both ends:

```
FSPL(2 km, 867 MHz) = 20log10(2) + 20log10(867) + 32.45 = 97.2 dB
RX = 14 + 3.58 + 3.58 − 0.6 − 2.1 − 97.2                = −78.8 dBm
margin over SF10 sensitivity (−132 dBm)                  =  53 dB
```

**2 km is comfortably inside budget with line of sight.** Observed −110 dBm on
the bench is ~31 dB below that prediction, which is obstruction (walls,
buildings), not distance. RSSI −110 dBm still leaves 22 dB of margin; PER only
starts climbing around −120 to −125 dBm. **Watch PER, not RSSI**, when range
testing.

---

*Written 2026-08-07. Raw transcript of the session that produced these changes:*
`C:\Users\kalbh\.claude\projects\C--Users-kalbh-Downloads-Cansat-Ground-station-v2\996d7d21-2f46-4907-a344-a1c880145e6c.jsonl`

---

## 8. Session 2026-08-08 — antenna investigation, real link margin test bench

Nothing in `vritra_fc/`, `vritra_gs/` or the wire format was touched this
session. All work is in `cdr_tests/test_rf_tx/` and `cdr_tests/test_rf_rx/`,
plus this file and `cdr_tests/README.md`. As always: **written and unverified,
no C++ toolchain on this machine** — except where the user has since built and
run a sketch on real hardware themselves, noted below where that happened.

### 8.1 The frequency allocation plan, read in full

`CAN-7USAT - Frequency allocation plan for teams (1).pdf` confirms team 025:
867.00 main / 865.75 backup MHz, SF10, sync `0x98` — matches `Config.h`
exactly. Two things from the PDF worth restating because they are easy to
miss on a skim:

- **The ground station is explicitly told to use a high-gain directional
  antenna (a Yagi) on a mast, for reliable reception at 1 km and beyond.**
  This is an organiser recommendation, not a team choice to second-guess — fit
  the Yagi on the ground side when it's available.
- **868 g1 sub-band ERP ceiling is +14 dBm (≈16.15 dBm EIRP).** This bounds
  *transmit* antenna gain, not receive — a high-gain Yagi on the ground
  station's receive path is free of this concern. It only matters if the
  ground station also transmits at meaningful power through the same antenna
  (it does, uplink commands at +10 dBm) — worth a compliance sanity check with
  Mission Control before flying a high-gain ground Yagi that also transmits.

### 8.2 The antenna question — corrected once, then resolved

**First pass was wrong and was corrected in-session.** Two RF tests were run
back to back with both the wall/openness *and* the antenna changed at once
(confounded), and the antenna was initially misjudged as inherently worse.
`COMPLIANCE_AUDIT.md` §3.1 already had the real numbers: RadioMaster dual-band
whip is 3.58 dBi @ 900 MHz, VSWR 1.25 — a *better* antenna on paper than the
Waveshare rubber duck (~2 dBi). A well-matched antenna cannot produce 30 dB of
extra loss from mismatch alone (VSWR 10:1 is only ~4.8 dB); 30 dB is the
Radio.cpp-documented signature of the **wrong RF path** (LP PA instead of HP,
or 2.4G_ANT instead of LORA_ANT), not a bad antenna.

**Diagnosis, confirmed by the user:**
- Both antennas were on **`LORA_ANT`** (correct sub-GHz port), not `2.4G_ANT`.
- Both connectors were confirmed correct (no RP-SMA/SMA mismatch).
- With port and connector ruled out, the remaining candidates for the earlier
  30 dB gap are a damaged coax pigtail on one antenna, or the antenna having
  been detuned by proximity to the aluminium plates (`COMPLIANCE_AUDIT.md`
  §3.6 — 86.5 mm clearance needed, cannot be achieved inside a 150 mm stack).
  **Not re-tested this session** — a clean A/B (swap one antenna at a time,
  not both) would settle it, and is still worth doing before committing to a
  flight antenna.
- A 1-metre bench test with the RadioMaster on the confirmed-correct port
  showed **zero packets lost across 22 packets, SNR 11.8–13.5 dB** — a clean,
  healthy link. The RSSI reading itself (−79 to −83 dBm) is not diagnostic at
  1 m: that range is dominated by indoor multipath and near-field coupling,
  not free-space loss, so it cannot be compared against the Friis prediction.
  **Verdict: the RadioMaster antenna, on the correct port with a correct
  connector, works.** A real range figure needs a proper-distance test (tens
  of metres or more, clear line of sight, both whips vertical, off any metal).

**Standing guidance, now in `cdr_tests/README.md` §1:** fly the RadioMaster
on `LORA_ANT` (ANT2, sub-GHz) at both ends, vertical and parallel, keep the
rubber duck as spare. Add the Yagi on the ground side per the allocation plan
when available.

### 8.3 Link margin — how to compute it, kept out of the firmware

Link margin = received power (dBm) − receiver sensitivity (dBm). Sensitivity
for SF10/BW125 with boosted RX gain was derived, not just quoted:

```
S = -174 + 10*log10(125000) + NF(5.5) + SNRmin(-15) ≈ -132 dBm
```

Two path-loss regimes matter and must not be mixed up: **free space** (20 dB
per decade — in-flight) vs **two-ray ground reflection** (40 dB per decade
beyond the breakpoint `4·h_t·h_r/λ` — ground testing and post-landing). Using
free-space loss for a ground test is the most common way a CanSat link budget
comes out wrong by tens of dB. `COMPLIANCE_AUDIT.md` §3.2–3.4 already has the
worked flight and post-landing numbers; nothing there needs to change.

**A budget-calculating version of `test_rf_rx.ino` was built and then
deliberately stripped back out**, per explicit instruction: the user wants to
do the link margin arithmetic themselves from raw readings, not have the
sketch do it. Current sketches print only packet content, byte count, RSSI,
SNR and TX power — nothing derived.

### 8.4 `test_rf_tx` / `test_rf_rx` — current state

Both were rewritten this session, twice — once to send a real-shaped packet
(matching the flight wire format's 20-field, base-36, 99-byte layout, so
airtime and error rate track the real link) instead of a filler string, and
once more to strip out all budget/prediction math and simplify the code and
serial output to plain, human-readable text with no underscores or box-art.
The user has since iterated `test_rf_rx.ino` further on their own (adding a
command-echo confirmation loop and a faster/every-packet listen window to fix
commands not landing) — see the file itself for current behaviour, this
handoff does not restate it blow-by-blow.

**A real bug was caught and fixed in-session:** the 99-byte padding was being
appended directly onto the transmit-power field with no separating comma.
Since `X` is a valid base-36 digit, the receiver would have silently decoded
a garbage power value. Fixed by inserting a comma before the padding so it
becomes its own optional field.

**Confirmed working on real hardware by the user:** both sketches built,
flashed, and exchanged packets cleanly at 1 m (22/22 received, healthy SNR).
This is the first real-hardware confirmation this session — noted here
because it's stronger evidence than "structurally checked."

### 8.5 Two corrections made mid-session, stated plainly

1. Told the user the dual-band antenna was categorically worse than the
   rubber duck — wrong, reversed after reading `COMPLIANCE_AUDIT.md` and the
   Radio.cpp load-bearing-fact comment on the ~30 dB LP/HP PA and antenna-port
   signature.
2. Gave a ground-test range table using two-ray loss stacked on top of
   free-space loss at the breakpoint — double-counted the transition and was
   about 10 dB pessimistic. Corrected: use whichever of free-space or two-ray
   predicts *more* loss, cross-over at `d_b = 4π·h_t·h_r/λ` — not
   `4·h_t·h_r/λ` used loosely.

Both are worth remembering as failure modes to check for, not just as
corrected facts: the first is "don't trust a spec claim inferred from two
confounded tests when the project's own audit already measured it," the
second is "verify which breakpoint formula variant is actually being used
before trusting a range table."

---
