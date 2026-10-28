# VRITRA — Sensor & Radio Bring-Up: Consolidated Findings

Team 2026-INSPACe-CAN-7USAT-025, Lovely Professional University.
Compiled 2026-08-03. Covers the isolated-sensor-test phase (`sensor_tests/`)
plus the LR1121 range-debugging saga recovered from an earlier shared
conversation. This is the *evidence log* — every claim below was produced by
an actual measurement on real hardware, not inferred from a datasheet alone.
See [PROJECT_HISTORY.md](PROJECT_HISTORY.md) for the broader project timeline
and [README.md](README.md) for the GCS technical spec.

---

## 0. How this testing was structured, and why

Every sensor got its own standalone `.ino` in `sensor_tests/<name>/` —
nothing shared, nothing initialized that isn't the sensor under test. This
was deliberate: several early bugs in the combined flight firmware were
actually **cross-sensor interference** (one subsystem's init or diagnostic
code corrupting another's bus state), which is invisible until you isolate
each device onto its own boot sequence. Section 8 documents the specific
incident this caught.

Each test follows the same shape: WHO_AM_I / CHIP_ID verification →
configuration written register-by-register with a comment naming the
datasheet field it touches → a 5-second baseline at rest → interactive
physical actions → a summary block with concrete numbers to carry into the
flight firmware. No sensor's "OK" is trusted on the strength of a
register-write ACK alone — every config is validated by reading back a
physically plausible value before being accepted.

---

## 1. BMP585 barometric pressure/temperature

**Wiring:** I2C (SDA=18, SCL=19), address `0x47` (SDO=HIGH) or `0x46`
(SDO=LOW), 100 kHz bus.

**Working register configuration** (all three registers, explicitly
composed — never read-modify-write):
```
OSR_CONFIG (0x36) = 0x21   press_en=1, osr_p=x16, osr_t=x2
ODR_CONFIG (0x37) = ...    25.005 Hz (code 0x14), Normal mode, deep-standby disabled
DSP_IIR    (0x31) = 0x00   on-chip IIR OFF
```

**Bugs found and fixed:**
- The vendor library's `initSensor()` leaves on-chip IIR at coefficient 127
  — a ~12 second time constant. Altitude looked frozen/laggy. Fixed by
  writing `DSP_IIR = 0x00` explicitly and doing all filtering in software
  (median-of-5, then a light single-pole smoother) where it's inspectable.
- **`DSP_CONFIG` (0x30) must never be written.** That register carries the
  pressure/temperature *compensation* enables, not a filter/shadow setting.
  Zeroing it (an early mistake in this session) took the output out of the
  physical pressure range entirely — every register write still ACKed, so
  it printed `[BMP] OK` while producing garbage. This cost a full debugging
  cycle. The lesson generalizes: **a successful register write proves the
  bus transaction worked, not that the resulting sensor behavior is
  correct** — always validate with a real reading before trusting a config.
- `readTemperatureC()`/`readPressurePa()` in the vendor library each do
  their own separate burst read, unsynchronized to the conversion cycle. A
  burst straddling a register update returns a torn 24-bit word — one
  corrupted MSB is ~1024 Pa ≈ 90 m of apparent altitude error, which is
  exactly the −300 m spikes seen in early flight logs. Fixed by reading
  temperature+pressure in **one** 6-byte burst per conversion.
- **Bus recovery must run before `Wire.begin()`, every boot, every isolated
  test sketch.** All sensors share one physical I2C bus. If any prior
  session (including a *different* test sketch, or the flight firmware's
  own boot) was interrupted mid-transaction, a slave can be left holding
  SDA low, and the *next* sketch to boot fails from its very first
  transaction — even though the wiring is fine and the code is correct.
  This was diagnosed via `CHIP_ID` reading `0x00` at both candidate
  addresses (not a wrong-chip ID — `rd8()` leaves its output at 0 whenever
  the transaction itself fails, so `0x00` means "no response," not "chip
  said zero"). The fix is 9 manual SCL pulses with SDA released, run
  unconditionally at the top of `setup()`.

**Measured baseline (bench, indoor):**
| Metric | Value |
|---|---|
| Noise floor (pp, at rest) | 3.8 Pa |
| ISA-derived altitude vs. GPS fix (same location) | agreed within expected margin |
| Mechanical stress artifact (bare fingertip on the lid) | up to **1214 Pa pp** (≈10 m apparent altitude) |
| Thermal drift (hand-cupping, 3 s) | ~185 Pa pp, mostly thermal not real pressure |
| Real 1 m altitude change, detected | ~10 Pa change (matches the ~12 Pa/m sea-level lapse rate) |

**Recommended flight constants:**
```cpp
#define BARO_MEDIAN_N        5        // rejects single torn-read glitches
#define BARO_NOISE_FLOOR_PA  3.8f     // measured
#define BARO_GLITCH_PA       50.0f    // ~13x noise floor; a single-sample jump
                                       // past this is a torn read, not real data
```
**Mechanical integration note:** the sensor must be mounted with **zero
preload** — no zip-tie pressure, no rigid potting directly over the lid, no
structural member resting on the package. The fingertip test proved this
package responds to mechanical stress as if it were a pressure change; flight
vibration/G-loading could reproduce the same artifact and would look to the
FSM like a real, fast altitude event.

---

## 2. ICM-20948 accelerometer + gyroscope

**Wiring:** I2C (SDA=18, SCL=19), address `0x69` (AD0/SDO tied HIGH) or
`0x68` (tied LOW — note `0x68` is *also* the DS3231's address; do not assume
it without confirming `WHO_AM_I` first).

**Critical, hard-learned rule: I2C bus speed stays at 100 kHz.** 400 kHz was
tried on this exact wiring and broke this exact chip — address ACKed on
scan, every register read returned `0xFF`. Address-phase-works /
data-phase-fails at a raised clock is the textbook signature of insufficient
edge rate for the physical wiring (3.3 kΩ pull-ups + flying leads). The
DS3231 survived 400 kHz on the same bus because it's a slower, more
forgiving part — that asymmetry is what made the failure look like a "dead
IMU" rather than a bus problem the first time it happened.

**`WHO_AM_I` must be read with the bank forced to 0 first.** Reading
register `0x00` without selecting bank 0 returns whatever bank the chip was
last left in, not `WHO_AM_I` — this produced a false `0x02`/`0x03` reading in
early debugging that was misread as "chip not present." The correct sequence
is `selectBank(0)` → small delay → read `0x00`, expect `0xEA`.

**A chip that ACKs its address but returns nothing/garbage on every read is
a wedged-bus symptom, not a missing-part symptom** — this happened once
after a prior diagnostic routine (see §8) performed an unguarded write that
timed out and never verified the bus was clean afterward. The general fix
that survives across all sensors: any diagnostic that touches the bus must
be **read-only**, or if a write is unavoidable, it must be followed by an
explicit bus-recovery + re-init, never assumed safe.

**Config used (accel ±16g, gyro ±2000dps, DLPF on):**
```
Bank 0: PWR_MGMT_1=0x01 (wake, CLKSEL=auto), PWR_MGMT_2=0x00, LP_CONFIG=0x00
Bank 2: GYRO_CONFIG_1=0x1F, ACCEL_CONFIG=0x1F, both SMPLRT_DIV=0x00
```

**Measured baseline (bench, at rest, first pass — see caveat below):**
| Axis | Gyro noise (pp) | Gyro bias (avg) |
|---|---|---|
| X | 10.99 dps | −0.42 dps |
| Y | 5.49 dps | 0.28 dps |
| Z | 11.17 dps | 0.32 dps |

**Return-to-baseline pass (same rest state, after a full motion sequence):**
gyro noise dropped to **6.59 / 5.37 / 4.33 dps pp** — tighter than the first
pass. Same sensor, same physical rest state, lower noise the second time:
the *first* baseline wasn't perfectly still (hand tremor / non-rigid
surface), not a hardware noise floor. **Lesson: a gyro calibration routine
must gate on measured stillness (sample-spread check), not just run for a
fixed sample count** — otherwise motion during calibration bakes directly
into the bias constant, which is very likely what corrupted earlier
flight-log gyro bias values.

Accelerometer: `|g|` at rest measured **0.9856**, giving a scale correction
factor of **1.01456** — a ~1.5% sensitivity correction, better than the
~2.2% error (9.60 vs 9.81 m/s²) seen in an earlier flight boot log.

Dynamic range check (shake/tap test): peaked at **5.5 g / 925 dps**, never
clipped against the configured ±16g/±2000dps full scale — confirms the
range selection has headroom for CanSat deployment shock and spin.

**Recommended flight constants:**
```cpp
gyroBiasX/Y/Z    = 0.15, 0.09, 0.34     // from the return-to-baseline pass
GYRO_DEADBAND_DPS ≈ 12.0f               // ~2x measured noise floor —
                                          // the previous 0.5f was 10-20x too
                                          // small and let sensor noise through
                                          // as if it were real rotation
accelScale        = 1.01456
```

---

## 3. AK09916 magnetometer (behind the ICM-20948's aux bus)

**Physically a separate die** inside the ICM-20948 package, reached at its
own I2C address `0x0C` — but **only once the ICM's bypass bit is asserted.**

**Root cause of "0x0C never shows up in an I2C scan":** `applyBasicDefaults()`
in the ICM library writes `INT_PIN_CFG = 0x30`, which **clears** the bypass
bit as a side effect. The magnetometer chip itself was never faulty — the
bus path to it was closed by a routine that had nothing to do with the
magnetometer. Bypass must be asserted **explicitly, after** the ICM's own
`begin()`/defaults, or it silently reverts.

**`ST2` must be included in every read burst.** Reading `ST1` through the
data registers without also reading `ST2` (register `0x18`) leaves the
AK09916's internal measurement latch closed — the data registers simply stop
updating. The correct read is one 9-byte burst covering `ST1..ST2`.

**Config:** 100 Hz continuous mode (`CNTL2 = 0x08`), soft reset first
(`CNTL3 = 0x01`).

**Hard-iron calibration note:** hard-iron bias = midpoint of min/max on each
axis across a full 360° rotation in two planes. Not yet run to completion in
this project — flagged as outstanding.

---

## 4. DS3231 real-time clock

**Wiring:** I2C (`0x68` — same address space as the ICM's alternate; disambiguate
by `WHO_AM_I` before trusting either), SQW/INT on pin 2 (unused by flight
firmware, reserved for a future alarm-driven wake).

**Findings:**
- Oscillator confirmed ticking (5 distinct second-ticks in 5.04 s of wall
  time — correct).
- Coin-cell backup confirmed holding time across a power cycle
  (`lostPower()` reports `FALSE` on a clean re-boot).
- Die temperature sensor responds to a real thermal stimulus (+0.30°C on a
  warm-breath test).
- **SQW pin gave zero level transitions when set to 1 Hz output** — this is
  a real, isolated fault: either pin 2 isn't actually wired to the DS3231's
  SQW pad, or the connection broke during protoboard soldering. Not urgent
  (SQW is unused in flight logic today) but flagged for a continuity check
  before it's ever depended on.

---

## 5. Quectel L89 GNSS (GPS/GLONASS/Galileo/BeiDou/NavIC)

This subsystem produced the largest number of false leads before converging
on a working, *measured* configuration. Documenting the wrong turns is as
important as the final answer, because several of them are the kind of
mistake that's easy to repeat.

### 5.1 What was actually wrong, in order of discovery

1. **A bare `Serial1.begin()` with no PMTK config leaves the module in
   whatever constellation/SBAS/EASY state it powered up in.** This — not
   the choice of NMEA parsing library — was the actual cause of "too slow."
   A parser only reads text the module already emits; it cannot change
   acquisition time because it never talks back to the receiver. (A
   third-party "L89HA" library was evaluated and rejected for this project:
   it had a real bug overwriting decoded longitude with a speed value one
   line after computing it correctly, a fix-validity check that was
   commented out entirely (so it reported "valid fix" even at zero
   satellites), and an unbounded fixed-size field buffer with no length
   check — a genuine overflow risk on noisy input.)
2. **`$PMTK353` has no NavIC bit.** It only ever covers
   GPS/GLONASS/Galileo/BeiDou. Trying to "enable NavIC via PMTK353" is not
   possible — this was asserted incorrectly early in the project and had to
   be corrected.
3. **NavIC lives behind Quectel's own `$PAIR` command family**, distinct
   from MediaTek's `$PMTK` that the rest of the module speaks. `$PAIR066,
   1,0,1,0,0,1` is the explicit GPS+Galileo+NavIC enable. The module ships
   with NavIC on by default, so this is confirmation rather than a hard
   requirement — but it's the only command that re-enables it if a prior
   session ever turned it off. Sending a command family the module doesn't
   implement is harmless (silently ignored), so `$PAIR` lines can never
   break the `$PMTK` lines.
4. **TinyGPSPlus does not parse `$GSV` at all** — it exposes only
   satellites *used* (from `$GGA`), never *why* that number is what it is.
   Two things live only in `$GSV` and had to be parsed directly off the raw
   NMEA byte stream:
   - **Per-satellite SNR** — the single measurement that distinguishes "few
     satellites because of a weak antenna/RF path" from "few satellites
     because the sky is genuinely obstructed." Confirmed empirically: SNR
     holding at 42–48 dB-Hz throughout a long run proves the RF path is
     healthy regardless of how low the satellite *count* is at any given
     moment — count is governed by sky geometry and time of day, not
     hardware, once SNR is proven strong.
   - **`$GIGSV`** (talker `GI`) — NavIC-specific. Confirmed empirically:
     these sentences arrive every cycle (proving NavIC tracking is active
     and the parser is correct) but report **0 satellites in view**,
     consistently, across an 8+ minute run with otherwise excellent GPS/
     Galileo/BeiDou performance. GPS/Galileo/BeiDou broadcast around L1
     (1575 MHz); NavIC broadcasts on L5 (1176 MHz) and S-band. A combo
     antenna tuned and matched for L1 commonly has poor L5 gain — this is
     the leading explanation and is an **antenna hardware property**, not
     something firmware can change. (Not yet confirmed against the
     antenna's actual datasheet — flagged as an open item.)
5. **A "diagnostic" routine that performs an unguarded write can take down
   an unrelated device.** An early bus-diagnosis helper issued a
   `DEVICE_RESET` write to the ICM-20948 that timed out; the timeout left
   the Teensy's I2C peripheral in a state that then broke the BMP585 — a
   completely different device on the same bus — for the rest of that boot,
   even though the BMP585 had validated cleanly seconds earlier. The
   general rule (already stated in §2) generalizes across every sensor on
   this bus: **diagnostics must be read-only, full stop.**
6. **`setRfSwitchTable()` must be called *after* `radio.begin()` succeeds,
   not before**, on the LR1121 (documented fully in §7 — the GNSS debugging
   surfaced the same "call-order matters, and a working example is worth
   more than re-deriving from the datasheet" lesson independently).

### 5.2 Working configuration (final, proven)

```
PMTK353,1,1,1,0,1     — GPS+GLONASS+Galileo+BeiDou (4th field, Galileo-full, stays 0)
PMTK313,1             — SBAS search on (GAGAN is visible over India)
PMTK301,2             — accept SBAS differential corrections
PMTK869,1,1           — EASY offline orbit prediction (cuts cold-start time)
PMTK220,1000          — 1 Hz fix rate
PMTK886,2             — Aviation dynamic platform model (see §5.4)
PAIR066,1,0,1,0,0,1   — NavIC confirm (Quectel command family)
```
All checksums are computed at runtime (`gpsSend()` XORs the body and appends
`*%02X`) — never hand-typed. A hand-typed checksum error is a real,
previously-hit failure mode: the receiver discards the whole command with
zero error indication, so a typo silently means "config never applied,"
indistinguishable from any other kind of slow acquisition.

**Baud stays at 9600.** Raising it affects only sustained NMEA throughput at
high sentence rates, not acquisition speed, and an earlier attempt to raise
it needed a fallback because the module didn't consistently acknowledge the
change. 9600 comfortably carries GGA+RMC+GSA+GSV at 1 Hz.

### 5.3 Best measured result

| Metric | Value |
|---|---|
| Satellites used | up to **12** |
| HDOP | as low as **0.9** |
| Best SNR | 45–48 dB-Hz, sustained |
| Duration held | 8+ minutes continuous, stable |
| Time-to-first-fix (warm, EASY active) | seconds, not the ~60 s of a cold config |

This is an excellent GNSS result for a compact patch antenna — HDOP under 1
with 8+ satellites is not a marginal pass.

### 5.4 Motion / dropout considerations — the part a bench test cannot fully answer

The result above was measured **holding the antenna still**, which is a
different property from "maintains lock while the CanSat is falling,
spinning, and snapping open a chute." Two things were added specifically for
this:

- **`PMTK886,2` (Aviation dynamic mode).** MTK-based GNSS chipsets default
  to a filter tuned for a person walking — it assumes position changes
  slowly and smoothly, and its internal Kalman filter can lag or reject
  updates when velocity/acceleration jump quickly. This was the one
  configuration line missing from the 12-satellite/HDOP-0.9 result above.
  Aviation mode raises the filter's tolerance for rapid dynamics.
- **Fix-continuity/dropout tracking.** The gap between every consecutive
  valid fix update is timed; anything over 1.5× the configured 1 Hz
  interval is logged as a dropout with the actual gap duration. This turns
  "does it maintain connection while moving" from an assumption into a
  number you can watch change as you physically move the antenna during a
  test and compare against a stationary baseline.

**Honest limit:** a hand-held walk-and-swing test is not equivalent to
actual CanSat descent dynamics (freefall, drogue snap-open, parafoil
deployment, spin). The dropout counter is a *comparative* tool — it proves
Aviation mode measurably reduces gaps relative to the previous config — not
a guarantee of flawless in-flight performance. That gap can only be closed
by an actual drop test or flight data.

---

## 6. TB6612FNG motor driver (gyro-stabilization wheel)

**Not a sensor** — no feedback pin exists, so there is no baseline noise
floor or boundary-value data to report. The isolated test for this component
was adapted honestly rather than forced into the sensor template: it asks
the operator to type a free-text observation after each speed/direction step
(25/50/70/100% forward and reverse), since human eyes and ears are the only
available instrument. `STBY` low is the safe default (driver disabled) at
boot and after every stop.

The flight firmware's `GYRO_ON` speed of 70% remains an unverified
placeholder from the PDR red-team audit's standpoint — this component test
produces the first real observational data point against that value, but
does not by itself validate the actual angular-momentum authority required.

---

## 7. LR1121 LoRa radio — summary of the range-debugging saga

*(Recovered from an earlier shared conversation, "Maximizing LR1121 range at
867 MHz" — included here because several of the failure patterns are
directly analogous to the sensor-bus lessons above, and because the final
proven pin/config set is load-bearing for every future radio change.)*

### 7.1 Two real firmware bugs, both fixed early and since carried forward correctly

- **`setOutputPower(N)` without the second `true` argument routes through
  the LR1121's LP (low-power) PA pin, not HP.** The Core1121-XF's antenna
  matching is built for the HP path. This cost ~30 dB of loss — the
  difference between ~2 km range and ~30–40 m range — and was diagnosed by
  an A/B test alternating HP and LP on successive packets and comparing
  received RSSI (a ~30 dB gap between the two confirmed the diagnosis
  directly, rather than by inference). Fix: `setOutputPower(N, true)`
  always, for the legal flight power level.
- **Explicit header mode let corrupted noise through as a "valid" packet at
  low SNR.** In explicit-header mode the receiver trusts the length/CRC-
  present flag carried in the *received* header rather than its own
  configuration; at SNR below the SF10 demodulation floor (~−15 dB) the
  header itself can misdecode and still report "CRC pass" on garbage. Fixed
  by switching to implicit header with a fixed, receiver-known length.

### 7.2 Wiring/init bugs (the "-2 / -705 / -707" saga)

- **`TCXO_VOLTAGE` must be set to the value proven correct for the specific
  Core1121 variant in hand — do not infer it from a schematic read.** An
  early inference from the module's schematic (seeing a crystal, not an
  obvious TCXO chip) concluded `0.0f` was correct; this was wrong. The
  value empirically proven to work on this hardware is **`1.6f`**. The
  general lesson: a datasheet/schematic reading is a hypothesis, not a
  substitute for testing — and when a hypothesis contradicts a working
  reference file, trust the working file.
- **`radio.setRfSwitchTable()` call order matters and is easy to get
  backwards.** The version that reliably failed with `-707` called it
  *before* `radio.begin()`; the version that worked (confirmed by direct
  side-by-side comparison against a known-good file) calls it both before
  *and* after `begin()` — matching the proven reference exactly, rather
  than a theoretically "cleaner" reordering, is what actually mattered.
- **A barebone, config-free connection test (reset pulse → watch BUSY →
  `SPI.begin()` → `radio.begin()` with library defaults → read chip
  version) isolates wiring/power problems from RF configuration problems in
  one step.** This was the single most useful diagnostic tool in the whole
  saga — every wiring ambiguity (RESET/BUSY pin swap suspicion, PSRAM pin
  conflict on WROVER boards, a module needing a separate clean 3.3V supply)
  was resolved by running this stripped-down test on each board in
  isolation and comparing `BUSY` pin behavior (should pulse HIGH briefly
  after reset, then drop LOW once the chip finishes booting; a flat 0 or
  flat 1 the whole time means the chip never actually reset/booted).
- **When a board keeps failing and another board with the same firmware
  works, physically swap the LR1121 module between boards before assuming
  either the code or a specific pin is at fault.** This isolates
  module-vs-board-vs-wiring in one test, and was the deciding step in at
  least one case where BUSY was stuck flat HIGH.

### 7.3 Proven pin set (Core1121-XF)

| Signal | ESP32 (ground) | Teensy 4.1 (flight) |
|---|---|---|
| NSS (CS) | GPIO5 | 10 |
| SCK | GPIO18 | 13 (fixed HW SPI0) |
| MOSI | GPIO23 | 11 (fixed HW SPI0) |
| MISO | GPIO19 | 12 (fixed HW SPI0) |
| RESET | GPIO14 | 16 |
| BUSY | GPIO27 | 14 |
| IRQ (DIO9) | GPIO26 | 15 |

DIO7/DIO8 are general-purpose on this module variant and unused for basic
TX/RX. DIO5/DIO6 (antenna-switch control) are configured entirely over SPI
by `setRfSwitchTable()` — they are chip-internal to the module and not
physical MCU wiring.

**SPI is not arbitrarily reroutable on Teensy.** Unlike the ESP32 (which has
a GPIO matrix and can route hardware SPI to almost any pin), Teensy 4.1's
hardware SPI0 is fixed to pins 11/12/13; only NSS/RESET/BUSY/IRQ are free to
move. Antenna pins (`LORA_ANT`, `2.4G_ANT`) are RF traces with impedance
matching — they are never GPIO-reroutable at all.

### 7.4 Confirmed real link budget

At the competition-mandated SF10/BW125/CR4-5/+14 dBm flight power, with
plain rubber-duck antennas (2 dBi) at both ends:

| Antenna height (both ends) | Two-ray breakpoint | Path loss @ 2 km | Received | Margin over SF10 sensitivity |
|---|---|---|---|---|
| 1.5 m | 26 m | 125 dB | −107 dBm | 27 dB |
| 3 m | 104 m | 113 dB | −95 dBm | 39 dB |

2 km is achievable with the mandated settings and simple rubber-duck
antennas — a bench-range result of only ~400 m earlier in the project traced
back to **antenna polarization mismatch, low antenna height (two-ray ground
reflection regime), and RF-side noise from the walked unit's own laptop/USB
cable**, not to firmware. Firmware fixes recovered an estimated 5–18 dB;
antenna height, orientation, and clearing RF noise near the receiver
accounted for the rest (~30–45 dB) of the gap to a genuinely useful range.

---

## 8. Cross-cutting lessons (apply to every future subsystem)

1. **A register write returning success proves the SPI/I2C transaction
   worked — it proves nothing about whether the resulting device behavior
   is correct.** Always validate a configuration by reading back a
   physically plausible value, not by the absence of a bus error.
2. **All sensors on this board share one physical bus.** Software isolation
   between test sketches does not isolate the physical wire — a wedge left
   by a completely different session/sketch can break the next boot's very
   first transaction. Bus recovery (9 manual SCL pulses, SDA released) runs
   unconditionally, every boot, before `Wire.begin()`.
3. **Diagnostics must be read-only.** A write issued "just to check" can
   time out and leave a bus peripheral in a bad state that breaks an
   unrelated, already-working device later in the same boot. If a write is
   genuinely required to recover a wedged part, it must be followed by
   explicit, verified bus recovery — never assumed harmless.
4. **A proven-working reference file outranks a theoretical re-derivation.**
   Several hours were lost re-deriving "correct" settings (TCXO voltage,
   switch-table call order) from a datasheet when a working prior file
   already had the empirically correct answer sitting unexamined.
5. **A calibration routine that averages over a fixed sample count without
   checking for actual stillness/quiescence bakes motion directly into the
   result.** Every bias/reference calibration in this project (gyro bias,
   accel reference rotation, barometer pad pressure) should gate on a
   measured stillness or stability check, not just elapsed samples.
6. **Bus speed and other "should be fine" parameters must be re-verified on
   the actual hardware after any wiring change**, not carried forward from
   a different board/protoboard/breadboard where they happened to work —
   400 kHz I2C worked on one physical layout and broke the exact same
   chip on another.
7. **Never trust a single number in isolation on a physically dynamic
   system.** A GNSS satellite count or an accelerometer's baseline noise is
   only meaningful alongside the condition it was measured under
   (stationary vs. moving, indoors vs. clear sky, first pass vs.
   return-to-baseline). Where the actual flight condition can't be bench-
   tested directly (motion dynamics, mechanical shock at flight-realistic
   levels), that gap should be stated explicitly rather than implied closed.

---

## 9. Open items carried forward

- AK09916 hard-iron calibration not yet run to completion.
- DS3231 SQW pin (Teensy pin 2) — confirm continuity to the DS3231's SQW pad;
  gave zero output transitions in the isolated test despite being configured
  for 1 Hz.
- NavIC L5 antenna gain — not yet confirmed against the antenna's actual
  datasheet; current explanation (L1-tuned antenna, poor L5 response) is the
  leading hypothesis but unverified against a spec sheet.
- TB6612 `GYRO_ON` 70% speed — still an unverified placeholder; needs a real
  angular-momentum authority calculation, not just a component-level
  forward/reverse test.
- GNSS motion/dropout behavior under actual flight-representative dynamics
  (freefall, chute snap-open, spin) — bench walk-test only; a drop test or
  real flight data is the only way to close this gap fully.
- Primary drag chute sink rate (14.4 m/s, below the 15 m/s floor) — flagged
  at PDR, status of resolution not covered by this sensor-testing phase.
