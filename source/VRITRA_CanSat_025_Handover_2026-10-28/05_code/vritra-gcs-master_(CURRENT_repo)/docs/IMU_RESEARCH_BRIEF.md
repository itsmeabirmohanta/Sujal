# Research brief — ICM-20948 attitude estimation for a CanSat

Copy everything below the line into your research tool. It is written as
instructions from me to you.

---

## Who I am and what I am building

I am building the flight computer for a CanSat competing in the 2026 IN-SPACe
CAN-7USAT India Student Competition (team 025, Lovely Professional University).
It is a vertical cylinder, 130 mm outside diameter and 152 mm tall, that is
launched to ~1000 m, ejected, descends under a parachute at under 20 m/s, and
deploys a secondary descent control at 600 m. It carries a gyroscopic
stabilisation wheel driven by a TB6612FNG and an N20 motor.

My flight computer is a **Teensy 4.1** running bare-metal Arduino C++ with no
RTOS. My IMU is an **InvenSense/TDK ICM-20948** on a 7Semi breakout, on a
shared I²C bus at **100 kHz** (400 kHz was tested and broke the part on this
harness). I do not use any vendor IMU library — I write every register
directly and validate by reading back physically plausible values.

I need you to do deep, source-backed research and give me a complete,
rigorous answer. Assume I am competent but that I have got this wrong so far.
Do not simplify to spare me. I want the level of rigour a working navigation
or aerospace engineer would apply, with citations I can check.

## My current configuration, exactly

```
ICM-20948 at I2C 0x69 (AD0 high). WHO_AM_I verified = 0xEA.
DS3231 RTC also on the bus at 0x68 — so 0x68 is NOT the IMU on my board.

Bank 2, GYRO_CONFIG_1  = 0x27
    -> GYRO_DLPFCFG = 4 (23.9 Hz), GYRO_FS_SEL = 3 (+-2000 dps), GYRO_FCHOICE = 1
Bank 2, ACCEL_CONFIG   = 0x1F
    -> DLPF on, ACCEL_FS_SEL = 3 (+-16 g)
Sample rate dividers all 0. I poll at a 50 Hz service tick; I do not use
the data-ready interrupt and I do not use the on-chip DMP.

Scale factors I currently apply:
    gyro  32768 / 2000  LSB per dps
    accel 32768 / 16    LSB per g

AK09916 magnetometer reached through the ICM's I2C bypass bit
(INT_PIN_CFG = 0x02, after clearing I2C_MST_EN). Continuous mode 100 Hz
(CNTL2 = 0x08). I read 9 bytes including ST2. It has NOT been hard-iron
calibrated, so I currently do not use it for heading.

Measured gyro noise floor at rest: 4.3 to 11.2 dps peak-to-peak by axis.
Measured accel magnitude at rest: 0.9856 g (I apply a 1.01456 scale correction).
Gyro bias measured at rest: X 0.15, Y 0.09, Z 0.34 dps.
```

My attitude filter is a complementary filter running at the 50 Hz tick:

```
accRoll  = atan2(ay, az)
accPitch = atan2(-ax, sqrt(ay*ay + az*az))
roll  = a*(roll  + gx*dt) + (1-a)*accRoll      a = 0.98
pitch = a*(pitch + gy*dt) + (1-a)*accPitch
yaw   = yaw + gz*dt                             (gyro only, wrapped 0..360)
```

I only blend in the accelerometer when |a| is between 0.75 g and 1.25 g, so
that thrust and ejection shock do not get mistaken for gravity.

I transmit roll, pitch and yaw at 1 Hz to a ground station that draws a 3-D
wireframe of the vehicle. The renderer applies rotations to body points as
roll about X, then pitch about Y, then yaw about Z.

## The problem I cannot solve

The 3-D model does not move the way the vehicle moves. Specifically **left and
right are mirrored** — turning the vehicle right turns the model left. I have
tried flipping the sign of individual axes and it has not converged; fixing one
direction breaks another. I am now guessing, which is why I am asking you.

I have also observed the model reading **PITCH 84.4 degrees and TILT 88.2
degrees** during a hand test, which makes me suspect I am hitting a
mathematical degeneracy rather than a wiring sign error.

## What I need you to determine

Answer each of these separately and show your working.

### 1. The ICM-20948's true sensor axis convention

- What are the exact +X, +Y and +Z directions of the **accelerometer and
  gyroscope** relative to the physical package, per the TDK datasheet's
  orientation diagram? Give me the pin-1 reference.
- Is the frame right-handed or left-handed as drawn?
- What sign should each accelerometer axis read when that axis points
  **up** (away from the Earth), and when it points **down**? I want this
  stated explicitly because it is the single fact my calibration depends on.

### 2. The AK09916 magnetometer axis mismatch — I believe this is a real trap

I have read that inside the ICM-20948 package the AK09916 magnetometer does
**not** share the accelerometer/gyroscope axis frame, and that a fixed
transformation is required to bring it into alignment. Confirm or refute this
from primary sources, and if true give me the exact transformation.

State clearly whether the correct mapping is a swap of X and Y with a negation
of Z, or something else, and cite the datasheet section or the InvenSense
application note that establishes it. Tell me what happens to a tilt-compensated
heading calculation if this transformation is omitted — I want to know whether
this alone could produce a mirrored left/right response.

### 3. Whether I am hitting gimbal lock, and what to use instead

My vehicle is a vertical cylinder and I have measured pitch above 84 degrees.

- Explain the gimbal-lock singularity in Tait-Bryan Euler angles and at what
  pitch angle it occurs for a Z-Y-X sequence.
- Near that singularity, what happens to roll and yaw? Can it produce exactly
  the symptom I see — left and right appearing swapped, and sign flips that fix
  one orientation while breaking another?
- Compare, with real trade-offs and not just a list:
  - complementary filter on Euler angles (what I have now)
  - **Mahony** filter on quaternions
  - **Madgwick** filter on quaternions
  - a proper **Extended Kalman Filter** with gyro bias states
  For each: computational cost on a 600 MHz Cortex-M7, convergence time,
  tuning parameters and how to choose them, and behaviour under the 30 g
  ejection shock and the sustained spin my vehicle will experience.
- Give me a concrete recommendation for a CanSat that spins, tumbles, and
  passes through vertical, and justify it.

### 4. How to determine my mounting orientation empirically instead of guessing

I want to stop flipping signs and measure the actual rotation between the
sensor frame and my vehicle frame.

- Give me the standard **six-position static test** (each axis up and down) and
  exactly what I should record at each position.
- Show me how to derive, from those six measurements, the 3x3 rotation matrix
  that maps sensor axes onto vehicle axes, and how to verify it is a valid
  rotation (orthonormal, determinant +1, not a reflection).
- Explain how a **determinant of −1** would show up in a 3-D display, and
  whether that is consistent with a mirrored left/right response. I suspect
  this is my actual bug.
- Give me the same procedure for the gyroscope, using known rotations rather
  than gravity.

### 5. Full calibration, done properly

For each of the three sensors, tell me the standard procedure, the equipment
needed, and the maths:

- **Gyroscope**: bias estimation, how long to average, temperature
  sensitivity, and whether I should re-estimate bias in flight.
- **Accelerometer**: the six-position method for bias and scale factor, and
  whether cross-axis misalignment matters at my accuracy target.
- **Magnetometer**: hard-iron and soft-iron calibration. Explain the ellipsoid
  fitting method, how many samples and what motion pattern I need, and how to
  validate the result. Tell me how to do this with the magnetometer already
  mounted inside a vehicle that contains aluminium plates, a battery, and a
  brushed DC motor.

Also explain **Allan variance** — what it measures, how I would run it on my
part, and what the resulting numbers (angle random walk, bias instability)
would let me predict about my yaw drift over a 20-minute flight.

### 6. Whether I should be using the DMP

The ICM-20948 has an on-chip Digital Motion Processor that can output
quaternions directly.

- What does the DMP actually provide, at what rates, and in what quaternion
  format?
- What is genuinely involved in using it — firmware image loading, licensing,
  library dependencies?
- Is it worth it for my application versus running a Mahony or Madgwick filter
  on the Teensy, which has plenty of compute headroom?
- What do people report about DMP reliability and documentation quality?

### 7. Sanity-check my filter tuning

- Is a complementary filter coefficient of 0.98 at 50 Hz sensible? Derive the
  time constant and tell me what it should be for a vehicle descending under a
  parachute.
- Is my 23.9 Hz gyro DLPF the right choice for a 50 Hz sample rate? Address
  aliasing explicitly.
- I kept ±2000 dps full scale to avoid clipping at ejection. Quantify what
  that costs me in noise and resolution versus ±500 or ±1000 dps, and tell me
  whether that trade is correct for a vehicle that may spin fast only briefly.
- My accelerometer is at ±16 g. Given a ~30 g ejection shock, am I already
  clipping, and what should I do about it?

## Sources I want you to use

Prioritise primary sources. For each substantive claim, tell me which one it
came from.

**Primary — datasheets and manufacturer documents**
1. TDK InvenSense **ICM-20948 datasheet** (DS-000189), especially the
   orientation-of-axes diagram, the register map, and the electrical specs.
2. **AK09916 datasheet** from Asahi Kasei Microdevices, for the magnetometer's
   own axis definition.
3. InvenSense application notes on axis alignment and on the DMP.
4. The **7Semi ICM-20948 breakout** schematic and silkscreen, to establish how
   the package sits relative to the board's printed axis markings.

**Standards and textbooks**
5. **IEEE Std 952** on inertial sensor terminology and Allan variance methods.
6. Titterton & Weston, *Strapdown Inertial Navigation Technology* — for frames,
   rotation matrices and the mechanisation equations.
7. Groves, *Principles of GNSS, Inertial, and Multisensor Integrated
   Navigation Systems* — for filter design and error modelling.
8. Farrell, *Aided Navigation* — for practical estimator implementation.

**Papers**
9. **Madgwick (2010)**, "An efficient orientation filter for inertial and
   inertial/magnetic sensor arrays" — the original report, with the derivation.
10. **Mahony, Hamel & Pflimlin (2008)**, "Nonlinear complementary filters on
    the special orthogonal group", IEEE Trans. Automatic Control.
11. Literature on magnetometer ellipsoid fitting for hard- and soft-iron
    calibration.
12. Any published work on attitude determination for CanSat or small sounding
    payloads, including CanSat competition technical reports.

**Working code to read, not necessarily to use**
13. **SparkFun ICM-20948 Arduino Library** — how they handle axis alignment and
    the magnetometer transformation.
14. The InvenSense/TDK **eMD** reference driver.
15. **Adafruit ICM20948** library, for a second independent opinion on the axis
    handling.
16. Madgwick and Mahony reference implementations, e.g. the x-io and PX4/Ardupilot
    versions.

**Community sources — use with care, and tell me when a claim is unverified**
17. TDK InvenSense developer forum threads on ICM-20948 magnetometer axis
    alignment, since I believe this is a commonly reported trap.
18. GitHub issues on the SparkFun and Adafruit ICM-20948 libraries relating to
    axis orientation, mirrored output, or magnetometer alignment.
19. r/embedded and Electronics Stack Exchange discussions of mirrored attitude
    displays and left-handed frame mistakes.

## What I want delivered

1. **A definitive statement of the ICM-20948 accel/gyro axis frame** and the
   AK09916 frame, with the transformation between them, each cited.
2. **A verdict on my mirrored left/right symptom**: is it a left-handed frame /
   determinant −1 error, a magnetometer axis error, gimbal lock, or a renderer
   convention mismatch? Rank the candidates by how well each explains
   everything I have described, and tell me the single measurement that would
   discriminate between them.
3. **A step-by-step bench procedure** I can run in one session, with a table of
   what to record and what each result implies. Assume I have no turntable, no
   rate table, and no optical bench — just a flat desk, a spirit level, and the
   ability to hold the vehicle in known orientations.
4. **The rotation matrix method**, worked through with a numerical example.
5. **A recommended attitude estimator** with pseudocode or C, tuning values
   with justification, and an explicit statement of how it behaves at
   vertical, during spin, and during a 30 g shock.
6. **A complete calibration plan**, ordered, with time required for each step.
7. **An explicit list of what I currently have wrong**, referenced to the
   configuration I gave you above.

## Constraints on the answer

- Cite sources for factual claims. Where you cannot verify something, say so
  and label it clearly as unverified rather than presenting it as fact.
- Give me numbers, not adjectives. "Low noise" is useless; dps/√Hz is useful.
- If two sources disagree, tell me both and tell me which you trust and why.
- Assume every conclusion must survive being questioned by a competition judge.
- Where a step needs equipment I do not have, say so and give me the best
  achievable alternative and what accuracy I lose by using it.
