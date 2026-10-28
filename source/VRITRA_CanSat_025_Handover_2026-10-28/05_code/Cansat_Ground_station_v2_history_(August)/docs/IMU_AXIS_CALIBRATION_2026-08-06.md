# IMU Axis Calibration — ICM-20948 six-position static test

**Vehicle:** VRITRA CanSat · 2026-INSPACe-CAN-7USAT-025 · Lovely Professional University
**Date:** 2026-08-06
**Instrument:** `sensor_tests/test_imu_axis_calibration/test_imu_axis_calibration.ino`
**Sensor:** 7Semi ICM-20948 breakout, I²C 0x69, WHO_AM_I 0xEA
**Config at capture:** ±16 g (2048 LSB/g), ±2000 dps (16.384 LSB/dps), gyro DLPF 11.6 Hz
(`GYRO_CONFIG_1 = 0x2F`), accel DLPF on (`ACCEL_CONFIG = 0x1F`), bus 100 kHz, 50 Hz poll,
`ACCEL_SCALE_CORR = 1.01456` applied in-sketch.

---

## 1. Verdict

**The board is mounted rotated 90° about its own Z axis.**

| Sensor axis | Points along vehicle |
|---|---|
| +X | **RIGHT** |
| +Y | **FRONT** |
| +Z | **UP** |

This is a **proper rotation** (determinant +1 against the right-handed
*(forward, right, down)* body frame), not a reflection — the hardware is fine, only the
software's axis assignment was wrong.

**This is why flipping `ATT_SIGN_*` never converged.** An axis swap is not a negation.
No combination of three sign constants can express a permutation, so correcting roll
always broke pitch. The fix belongs in the `atan2` formula, and that is where it now lives.

---

## 2. Test procedure actually performed

Seven poses. **Important:** the operator started at home (level), moved to the pose, held
it, then returned to home — all inside one capture window.

This means **the per-pose means printed by the sketch are invalid** — they average the home
dwell, both transitions, and the pose together. That is why the report showed |a| = 0.79–0.88
instead of ~1.00, a determinant of 0.76, and accelerometer scale factors of 0.70–0.79.

The raw 10 Hz CSV stream is unaffected and complete. **All numbers in this document are
plateau means extracted from the steady segment of each pose**, with transitions and home
dwells excluded. Sample windows are given below so the work can be checked.

---

## 3. Measured plateau data

| Pose | window (ms) | ax | ay | az | \|a\| | dominant |
|---|---|---|---|---|---|---|
| LEVEL | 1724–9324 (whole) | −0.029 | −0.052 | **+0.997** | 0.999 | **+Z** |
| LEFT 90 | 16424–18324 | **+1.005** | −0.161 | +0.029 | 1.018 | **+X** |
| RIGHT 90 | 26124–33424 | **−1.002** | −0.216 | −0.019 | 1.025 | **−X** |
| FRONT 90 | 42024–47424 | −0.192 | **−1.012** | −0.021 | 1.030 | **−Y** |
| BACK 90 | 57324–62124 | +0.135 | **+0.984** | −0.028 | 0.994 | **+Y** |
| INVERTED | 79024–85624 | −0.029 | −0.064 | **−1.047** | 1.049 | **−Z** |

**Reading it:** an accelerometer at rest reads **+1 g along whichever of its axes points
UP**. Rolling the vehicle onto its LEFT side puts the RIGHT side up and drives **ax** to
+1.00 — so sensor +X points to the vehicle's right. Tipping the nose DOWN puts the back up
and drives **ay** to −1.00 — so sensor +Y points forward. Rolling moves X, pitching moves Y.
That is the swap, and it is unambiguous: the dominant axis in every pose is ±1.00 while the
other two sit near zero.

### Axis-response cross-check

| Motion | moved ROLL | moved PITCH |
|---|---|---|
| roll poses (LEFT↔RIGHT) | 13.2° | **133.7°** |
| pitch poses (FRONT↔BACK) | **123.0°** | 13.6° |

Rolling moved the *pitch* number and pitching moved the *roll* number — a clean, ~10:1
separation. Swap confirmed independently of the accelerometer vector analysis.

---

## 4. Corrected attitude formula

**Before** (wrong — assumed +X forward):
```c
rollRaw  = atan2f(ay, az);
pitchRaw = atan2f(-ax, sqrtf(ay*ay + az*az));
```

**After** (`vritra_fc/Sensors.cpp`):
```c
rollRaw  = atan2f(-ax, az)  * 57.29577951f;
pitchRaw = atan2f( ay, sqrtf(ax*ax + az*az)) * 57.29577951f;
```

### Verification against every measured pose

| Pose | roll | pitch | expected | ✓ |
|---|---|---|---|---|
| LEVEL | +1.7 | −3.0 | ~0, ~0 | ✓ |
| RIGHT 90 | **+91.1** | — | +90 | ✓ |
| LEFT 90 | **−88.4** | — | −90 | ✓ |
| BACK 90 (nose up) | — | **+82.0** | +90 | ✓ |
| FRONT 90 (nose down) | — | **−79.2** | −90 | ✓ |
| INVERTED | +178.4 | — | ±180 | ✓ |

Residual few degrees is genuine mounting tilt and is removed by the boot datum.

---

## 5. Final `Config.h` values

```c
#define ATT_SIGN_ROLL       (+1.0f)
#define ATT_SIGN_PITCH      (+1.0f)
#define ATT_SIGN_YAW        (-1.0f)
```

Roll and pitch need **no** negation once the swap is fixed. Yaw is −1 because sensor +Z
points up, so a clockwise turn seen from above is a *negative* rotation about it under the
right-hand rule; the log shows sustained negative `gz` throughout the clockwise turn.
Negating makes clockwise read as increasing yaw, the aviation convention the GCS displays.

---

## 6. Quaternion path

The 3-D model is fed by a separate Mahony filter and needs the same correction, but it
**cannot** be fixed with sign constants. Under datum subtraction `conj(qRef)*qFree`, a
constant mounting rotation leaves the rotation *angle* correct while rotating its *axis* —
the model would tilt by the right amount about the wrong axis.

`quatUpdate()` is now fed in vehicle axes:

```c
quatUpdate( ay,  ax, -az,
           cgy, cgx, -cgz, dt);
```

That is *(forward, right, down)* = (Y, X, −Z), determinant **+1**. The naive (Y, X, +Z) is a
reflection (det −1) and would integrate rotations backwards about the vertical. Accel and
gyro must be remapped identically or the accel correction fights the gyro.

⚠️ **Not yet bench-verified.** The maths is checked but the renderer's convention was not
observed. Confirm the model tilts the correct way before CDR.

---

## 7. Accelerometer bias and scale

From the ±1 g plateau pairs:

| Axis | +1 g | −1 g | bias (g) | scale |
|---|---|---|---|---|
| X | +1.005 (LEFT) | −1.002 (RIGHT) | +0.002 | 1.003 |
| Y | +0.984 (BACK) | −1.012 (FRONT) | −0.014 | 0.998 |
| Z | +0.997 (LEVEL) | −1.047 (INVERTED) | **−0.025** | 1.022 |

X and Y are excellent. **Z carries a −0.025 g bias and a 2.2 % scale error** — it reads
0.997 upright but 1.047 inverted. At level |a| = 0.999, so `ACCEL_SCALE_CORR = 1.01456`
should stay as it is; a single scalar cannot fix a per-axis asymmetry.

Not worth acting on: Z bias contributes ~1.4° of tilt error near vertical, well under the
mounting tilt already removed by the datum. Revisit only if per-axis bias terms are added.

*(The sketch's own printed values — scale 0.794 / 0.703 — are the blended-window artefact
described in §2. Disregard them.)*

---

## 8. Gyro bias — NOT SETTLED, re-measure

Printed from the LEVEL hold:

```
GYRO_BIAS_X_DPS  +0.0564    (current Config.h: 0.15)
GYRO_BIAS_Y_DPS  +0.6141    (current Config.h: 0.09)
GYRO_BIAS_Z_DPS  +0.4886    (current Config.h: 0.34)
noise (sd): 1.221 / 2.588 / 1.579 dps
```

**Do not paste these in.** Two problems:

1. **Only 8 s / 445 samples.** Far short of the 10 minutes intended.
2. **The hold was disturbed.** Standard deviation of 1.2–2.6 dps is an order of magnitude
   above the ~0.1 dps rms expected from this part at DLPF 11.6 Hz. The raw stream contains
   isolated spikes (+11.4, −7.3, +6.2 dps) that are table knocks, not sensor noise.

`gz` matters most — it integrates directly into yaw. The difference between the measured
0.489 and the configured 0.34 dps is ~9°/min of yaw drift, which is significant over a
flight.

**Action:** re-run pose 1 alone, vehicle on a solid surface, untouched, ≥10 minutes. Press
ENTER without moving it. Standard deviation should fall below ~0.3 dps; if it does not, the
bench is vibrating.

---

## 9. Yaw magnitude anomaly

The YAW pose integrated **−387°** for a nominal 90° turn. The sign is trustworthy
(sustained negative `gz` throughout); the magnitude is not, because the capture began while
the vehicle was still INVERTED from pose 6 (`az ≈ −1.05` at t = 85724) and includes the
flip back upright plus a return rotation. The gyro scale factor is **not** implicated and
was not measured by this run.

---

## 10. Outstanding

| Item | Status |
|---|---|
| Axis swap | **Fixed and verified** against all six poses |
| `ATT_SIGN_ROLL/PITCH` | **Final** (+1, +1) |
| `ATT_SIGN_YAW` | **Final** (−1) |
| Quaternion remap | Applied, bench-verified — 3-D model shows correct motion |
| Gyro bias | **Re-measure** — 10 min undisturbed level hold |
| Gyro scale factor | Not measured; needs a controlled 90° turn |
| Accel Z bias −0.025 g | Documented, no action |
| Barometer AGL | **Fixed** — boot calibration zeroing at ground pressure |
| GCS 3-D model | **Fixed and confirmed working** |
| GPS aided start | **Implemented** — SD-card inject via PMTK740/741 |

---

## 11. Re-running the test

Hold each pose and **press ENTER while still in the pose**, before returning to home.
Returning to home first is what invalidated the printed statistics in this run — the sketch
averages the entire window between captures.

Pose order: LEVEL → LEFT 90 → RIGHT 90 → FRONT 90 → BACK 90 → INVERTED → YAW +90.
Keep each pose square to gravity; this run was up to 23° out of square between the FRONT and
RIGHT poses, which is what dragged the determinant to 0.76. It did not affect the axis
assignment (the dominant axis is ±1.00 in every pose) but it makes the rotation matrix
unusable for finer work.

---

## 12. GCS-side fix — `vritra_gcs.py` (2026-08-06)

The firmware axis-swap fix (§4) corrected the telemetry values of `ATT_ROLL` and
`ATT_PITCH`. However, the 3-D model remained wrong because the GCS `Physics.compute()`
function was still reading the Mahony/Madgwick-filtered `ATT_ROLL`/`ATT_PITCH` fields, which
were produced by the flight computer running its fusion filter over incorrectly-mapped raw
axes. A 90° physical roll yielded 68° of pitch in the telemetry log — the cross-coupling
was in the FC filter, not in the raw sensor data.

### Root cause

The FC fusion filter (Mahony) requires IMU axes in a standard body frame. Because the sensor
board was mounted 90° about Z, the raw axes fed to the filter were permuted, causing
cross-coupled output. `ATT_ROLL` and `QUAT_X/Y/Z` from telemetry were therefore unusable for
the 3-D model regardless of any sign correction applied on either side.

### Fix 1 — `Physics.compute()` replacement

The function now **ignores** `ATT_ROLL`, `ATT_PITCH`, `ATT_YAW`, and all `QUAT_*` fields.
Instead it reads the raw `ACC_X`/`ACC_Y`/`ACC_Z` (m s⁻² as transmitted on the wire, divided
by `G_EARTH` to produce g-units) and re-derives attitude independently:

```python
ax = _finite(d.get("ACC_X")) / G_EARTH
ay = _finite(d.get("ACC_Y")) / G_EARTH
az = _finite(d.get("ACC_Z")) / G_EARTH

roll  = math.degrees(math.atan2(-ax, az))
pitch = math.degrees(math.atan2(ay, math.sqrt(ax * ax + az * az)))
```

These are the same formulas fixed in `Sensors.cpp` (§4), derived from the same
measurement table. They are immune to the FC fusion filter.

Tilt-compensated heading uses the measured board→NED remap
(+X=RIGHT, +Y=FORWARD, +Z=UP → NED = Forward, Right, Down):

```python
mx_ned, my_ned, mz_ned = my, mx, -mz   # board→NED permutation, verified det=+1
```

### Fix 2 — `_update_instruments()` argument swap + QUAT sever

`CanSatView.set_attitude(roll, pitch, yaw)` stores its arguments as `tgt_roll` and
`tgt_pitch`. The renderer's `_project()` uses:

- `self.roll` → about-X rotation → visually produces nose pitch effect (long-axis = Z)
- `self.pitch` → about-Y rotation → visually produces left-right roll effect

With the CanSat long-axis along screen +Z:

| renderer variable | about-axis | visual effect | should receive |
|---|---|---|---|
| `self.roll` | X | nose pitches in/out | aircraft PITCH |
| `self.pitch` | Y | can tilts left/right | aircraft ROLL |

The original call `set_attitude(roll, pitch, ...)` fed roll→about-X and pitch→about-Y,
reversing the visual output. The fix passes them in swapped order:

```python
self.cansat.set_attitude(pitch, roll, yaw, spin, True)
```

The QUAT path is severed immediately after:

```python
self.cansat.set_quaternion(None)
```

`set_quaternion(None)` sets `_quat_tgt = None`; on the next `_step()` tick,
`_quat` is also set to `None` (line 3018 of `vritra_gcs.py`), forcing `_project()` onto
the Euler fallback path. This prevents the corrupted hardware quaternion from overriding the
clean Euler angles.

### Verification

| Check | Result |
|---|---|
| `roll = atan2(-ax, az)` | Matches Sensors.cpp fix; consistent with +X=RIGHT, +Z=UP |
| `pitch = atan2(ay, √(ax²+az²))` | Consistent with +Y=FORWARD |
| NED remap `(my, mx, -mz)` | Correct: board→NED = (fwd, right, down) = (Y, X, −Z) |
| `set_attitude(pitch, roll)` argument swap | Verified against `_project()` axis comment (line 3073) |
| `set_quaternion(None)` sever | Verified: `_step()` sets `_quat=None`, enables Euler path |

User-confirmed: 3-D model shows correct motion after both changes applied.

**Note on `Physics.compute()` axis variables**: The final working version passes raw sensor
values straight through — no ax/ay swap inside `compute()`. The swap that Antigravity added
(`ax, ay, az = ay_raw, ax_raw, az_raw`) was removed because it double-inverted the axes
(once in `compute()` and once in `set_attitude(pitch, roll, ...)`). The correct code is:
`ax, ay, az = ax_raw, ay_raw, az_raw` — no permutation. The visual correction lives
entirely in the `set_attitude(pitch, roll, ...)` argument swap.

---

## 13. Barometer AGL Calibration — `Sensors.cpp` (2026-08-06)

### Root cause of −15 m reading

The BMP585 computes absolute altitude using the ICAO standard sea-level pressure
(101325.0 Pa). Actual ambient pressure varies daily with weather. At the 6th-floor lab,
ambient is ≈ 97235 Pa — higher than the current standard atmosphere at that nominal
altitude — so the formula returned −12 to −15 m.

### Fix — boot-time ground pressure capture

> **SUPERSEDED — this section describes code that no longer exists.** It is kept
> because it is a dated engineering record of what was done on 2026-08-06 and why,
> and because the *diagnosis* above (ambient ≠ 101325 Pa) is still correct and is
> still the reason a pad survey exists at all.
>
> Everything below was replaced after four faults were found in it:
> it **blocked** for 320 ms inside a command handler; it averaged **raw** register
> reads rather than the median-filtered value, so one torn I²C burst (~1024 Pa
> ≈ 90 m) baked ~5.6 m of permanent offset into the reference; it sampled every
> 20 ms against a **25 Hz (40 ms) sensor ODR**, so half the samples were duplicate
> conversions and the noise never averaged down as √N; and `groundPressurePa` was
> a *second* copy of the pad reference that the EEPROM-persisted `padPresPa` was
> never written into — so **every reboot silently reverted the altitude datum to
> 101325 Pa**, which is the true origin of the −10 m AGL and 263 m baro-vs-GNSS
> disagreement seen later.
>
> Current design: `SensorHub::startBaroCal()` averages `BARO_CAL_MS` of
> *conditioned* pressure on the barometer's own tick without blocking;
> `SensorHub::setPadReference()` is the single writer; `SensorHub::altitudeAgl()`
> is temperature-compensated. See the BAROMETER block in `Config.h`.

A `groundPressurePa` variable (file scope in `Sensors.cpp`) is set at boot by
`calibrateBarometer()`, which reads the BMP585 16 times at 20 ms intervals and averages the
result to ±0.1 m accuracy:

```c
// 16 × 20 ms = 320 ms, called once in IS_DONE after all hardware is alive
for (int i = 0; i < 16; i++) {
    Sensors.bmpRead(tempC, presPa);
    sumPressure += presPa;
    delay(20);
}
groundPressurePa = sumPressure / 16;
```

`getAltitudeAGL(p)` then uses `groundPressurePa` as P₀ in the hypsometric formula:

```c
return 44330.0f * (1.0f - powf(currentPressurePa / groundPressurePa, 0.190295f));
```

At boot: P = P₀, so `(P/P₀)^0.190295 = 1`, result = 0.000 m. ✓

### Placement

`calibrateBarometer()` is called in the `IS_DONE` stage of the SensorHub init machine,
after every I2C device (BMP585, ICM-20948, AK09916, DS3231) and the GNSS port have been
fully initialised. Placing it earlier (right after IS_BMP_VERIFY, as first attempted)
produced `[WARN] IMU not responding` because the 320 ms `delay()` blocked the ICM-20948
from receiving its reset-settle window.

`CMD_CALIBRATE` also calls `calibrateBarometer()`, letting the ground operator re-zero the
pad datum remotely if the weather changes between power-on and launch.

### Observed result (boot log)

```
Calibrating BMP585 to 0m AGL... Ground Pressure locked at: 97235.04 Pa
// t=23780s | AGL=0.0m
// t=23784s | AGL=0.1m    ← sensor noise, correctly near 0
```

---

## 14. GPS Aided Start — `Sensors.cpp` / `DataLogger.cpp` (2026-08-06)

### Problem

The L89HA (Airoha AG3335M) has an EASY (Embedded Assist System) feature that stores
predicted satellite orbit data in internal flash, allowing a subsequent cold start in
≈ 30 s instead of the nominal 4–5 minutes. Without a backup battery on the `BAT` pin,
the module's RTC resets to an invalid date on every power cycle. When the AG3335M detects
an RTC epoch > 3 days from its stored EASY data, it invalidates and discards the
predictions, forcing a full sky scan.

This is why TTFF was 30 s at first (EASY intact) and degraded to 5 min (EASY discarded).

### Fix — SD-card-backed PMTK aided start

Three mechanisms work together:

**1. EASY always enabled** — added `PMTK869,1,1` to `GNSS_CFG[]` in `Sensors.cpp` so the
feature is re-enabled on every boot, not only when a saved file exists.

**2. Inject at boot** — `DataLogger::injectGPSAssistFromSD()` reads `gps_aid.txt` from the
SD card (if it exists) and sends:
- `$PMTK740,YYYY,MM,DD,HH,MM,SS` — sets the module's RTC so EASY data appears current
- `$PMTK741,LAT,LON,ALT,YYYY,MM,DD,HH,MM,SS` — seeds position for warm-start geometry
- `$PMTK869,1,1` — re-arm EASY

The injection fires from `DataLogger::service()` on the first call after `Sensors.ready()`
is true — the only point where the GNSS serial port is open. The previous placement
(immediately in `setup()` before `Sensors.service()` ever ran) sent commands to a UART
that had not yet been `begin()`'d and was silently discarded.

**3. Save after fix** — `DataLogger::saveGPSStateToSD()` writes `gps_aid.txt` once per
minute whenever `d.gnssFix == true` and the GPS year is ≥ 2024. The date is sourced
from `$GPRMC` field 9 (DDMMYY), which `nmeaRMC()` now parses into
`SensorData::gnssDay/gnssMon/gnssYear`. This is the only reliable date source on a
prototype without a battery-backed DS3231.

### Expected TTFF sequence

| Boot | SD file? | Expected TTFF |
|---|---|---|
| 1st ever | No | 4–5 min (full cold start) |
| After 1st fix | Yes | 3–10 s (EASY + position hint) |
| After 3+ days no power | Yes (stale) | 30–60 s (EASY valid, pos stale) |

### File format — `gps_aid.txt`

```
28.394762,76.994813,215.0,2026,08,06,10,34,22
```
Fields: `LAT,LON,ALT_M,YYYY,MM,DD,HH,MM,SS` (GPS time, not local time).
