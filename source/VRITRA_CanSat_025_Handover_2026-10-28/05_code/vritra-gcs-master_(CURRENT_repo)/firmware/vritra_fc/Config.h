/* =============================================================================
 * VRITRA  ·  2026-INSPACe-CAN-7USAT-025  ·  Lovely Professional University
 * Config.h — single source of truth for pins, RF plan, wire format, tunables.
 * =============================================================================
 * Nothing in this project hard-codes a pin or a threshold anywhere else. If a
 * number matters to more than one translation unit, it lives here.
 *
 * Every tunable that came from a measurement carries the measurement in its
 * comment. Values with no measurement behind them are marked UNVERIFIED so they
 * are never mistaken for flight-qualified numbers. Values traceable to the
 * competition guidelines carry their clause number.
 * ========================================================================== */
#pragma once
#include <Arduino.h>

/* ---------------------------------------------------------------------------
 * 1. IDENTITY
 * ------------------------------------------------------------------------ */
#define FW_VERSION      "6.1.0"
/* Guideline table row 1 gives the format as 2026-IN-SPACe-CAN-7USAT-XXX, but
 * every artefact this team has already submitted or built — the PDR cover page,
 * vritra_gcs.py TEAM_ID, the graded CSV filename — uses 2026-INSPACe-CAN-7USAT-025
 * with no hyphen inside INSPACe. Kept consistent with the team's own documents;
 * raise it with the organisers if they want the hyphenated spelling. Whatever
 * is chosen, this string and vritra_gcs.py TEAM_ID must match character for
 * character or the GCS parser fails to strip column 0 and every field shifts. */
#define TEAM_ID_FULL    "2026-INSPACe-CAN-7USAT-025"

/* ---------------------------------------------------------------------------
 * 2. PIN MAP  (Teensy 4.1, as built — harness revision 2026-08)
 * ------------------------------------------------------------------------ *
 * REWIRED AT REV 2026-08. Five assignments moved. If the board in front of you
 * is the older harness, this firmware will not talk to the radio and will not
 * see the GNSS. The two are independent failures with different symptoms:
 *   old radio wiring -> RADIOLIB_ERR_CHIP_NOT_FOUND (-2) at boot
 *   old GNSS wiring  -> "[GNSS] NO DATA", everything else healthy
 * Check the harness before you debug the firmware.
 *
 * WHAT MOVED                     was  ->  now
 *   LR1121 CS                     24      29
 *   LR1121 BUSY                   31      24     <- CS and BUSY effectively swapped
 *   LR1121 RESET                  30      25
 *   LR1121 DIO9 / IRQ             32      30     <- takes the old RESET pin
 *   GNSS                      Serial1     Serial2 (RX 0/TX 1 -> RX 7/TX 8)
 *   GNSS RESET_N                  22      6
 *   DS3231 SQW                     2      22     <- takes the old GNSS RESET pin
 *   BMP585 INT                    n/c     0      <- newly wired
 *   ICM-20948 INT                 n/c     41     <- newly wired
 *
 * Three of those are pin REUSE, not just reassignment: 24, 30 and 22 each now
 * carry a different signal than they did. A half-rewired harness therefore
 * shorts an output against an output. Rewire completely or not at all.
 *
 * FULL MAP, sorted — no pin appears twice:
 *    0  BMP585 INT            18  I2C0 SDA           30  LR1121 DIO9 (IRQ)
 *    6  GNSS RESET_N          19  I2C0 SCL           33  TB6612 PWMA
 *    7  Serial2 RX <- GNSS TX 20  A6 bus sense       34  TB6612 AIN2
 *    8  Serial2 TX -> GNSS RX 22  DS3231 SQW         35  TB6612 AIN1
 *                             24  LR1121 BUSY        36  TB6612 STBY
 *                             25  LR1121 RESET       39  SPI1 MISO
 *                             26  SPI1 MOSI          41  ICM-20948 INT
 *                             27  SPI1 SCK
 *                             29  LR1121 CS
 * ------------------------------------------------------------------------ */
/* I2C0 — 3.3 kohm pull-ups fitted on the harness */
#define PIN_SDA          18
#define PIN_SCL          19

/* Sensor interrupt lines, newly wired at rev 2026-08.
 * DECLARED BUT NOT YET USED AS INTERRUPTS. The firmware still polls both
 * sensors on its own 50 Hz service tick, which is deliberate: the barometer
 * runs at 25 Hz and the IMU at 1.1 kHz internally, so a data-ready interrupt
 * buys nothing at the rates this vehicle logs, and an ISR touching the I2C bus
 * would race the polling path that already owns it. They are defined here and
 * driven as inputs so the pins are reserved and cannot be handed to something
 * else, and so switching to interrupt-driven acquisition later is a Sensors.cpp
 * change only.
 * PIN_BMP_INT is pin 0, which is also SPI1's ALTERNATE hardware chip select.
 * That is harmless here because the LR1121 uses a software CS on pin 29 and
 * SPI1's hardware CS is never enabled — but do not enable it. */
#define PIN_BMP_INT      0       /* BMP585 INT  — input, polled not used */
#define PIN_ICM_INT      41      /* ICM-20948 INT — input, polled not used */

/* LR1121 on SPI1.
 * NOTE: PDR 4.x lists this on SPI0. The as-built harness and the brief both use
 * SPI1; this firmware follows the hardware. Correct the PDR at CDR.
 *
 * MISO STAYS ON 39. It was originally remapped there because SPI1's default
 * MISO is pin 1, which used to be Serial1 TX (the GNSS line) and corrupted
 * every SPI read. That collision no longer exists — the GNSS moved to Serial2
 * at rev 2026-08, so pin 1 is free. The remap is kept anyway because THE
 * HARNESS IS PHYSICALLY WIRED TO 39. Do not "simplify" this back to pin 1
 * on the strength of the old comment being obsolete; that would need a
 * rewire, and the failure mode is a silent -2 at boot. */
#define PIN_LORA_CS      29
#define PIN_LORA_IRQ     30      /* DIO9 */
#define PIN_LORA_RST     25
#define PIN_LORA_BUSY    24
#define PIN_SPI1_MOSI    26
#define PIN_SPI1_SCK     27
#define PIN_SPI1_MISO    39

/* Quectel L89 on Serial2.
 * Teensy 4.1 fixes Serial2 at RX = 7, TX = 8. Those are hardware pins, not a
 * remappable choice, so PIN_GNSS_RX/TX are documentation only — Serial2.begin()
 * claims them regardless and nothing else may use them.
 * Wiring is crossed as always: L89 TX -> Teensy RX (7), L89 RX <- Teensy TX (8).
 * WAKE_UP is strapped to 3V3 on the harness, so there is no pin for it.
 *
 * PORT AUTO-DETECTION. The firmware opens BOTH Serial2 and Serial1 at boot,
 * listens on both for up to GNSS_AUTODETECT_MS, and locks onto whichever one
 * actually produces NMEA. This exists because the rev 2026-08 rewire moved the
 * receiver from Serial1 to Serial2, and a firmware/harness mismatch produces
 * "no GNSS output at all" — a total, silent failure that looks identical to a
 * dead module, a bad antenna, or an indoor test with no sky view. Those are
 * four very different problems and guessing between them wastes hours.
 * Auto-detection removes the question: the boot log now names the port it
 * found, or says plainly that neither port produced a single byte. */
#define GNSS_PORT_A      Serial2 /* rev 2026-08 harness: RX 7 / TX 8 */
#define GNSS_PORT_B      Serial1 /* pre-rev harness:     RX 0 / TX 1 */
#define GNSS_AUTODETECT_MS  3000 /* generous: the L89 emits within ~1 s of power */
#define PIN_GNSS_RX      7       /* Serial2 RX — fixed by hardware */
#define PIN_GNSS_TX      8       /* Serial2 TX — fixed by hardware */
#define PIN_GNSS_RST     6       /* active low, idle high */
#define GNSS_BAUD        9600    /* stays at 9600 — see Sensors.cpp header */

/* SEND THE GNSS CONFIGURATION BLOCK? DEFAULT: YES.
 *
 * MODULE CONFIRMED: Quectel L89 / Airoha AG3335M (not ST Teseo III).
 * Bring-up test output: "$PAIR021,AG3335M_V2.5.0,AG3335_20221130,...".
 * PAIR is the AG3335M proprietary talker prefix; $PSTM commands were silently
 * discarded by this chip. The correct protocol is $PMTK.
 *
 * Factory defaults (GPS + Galileo + NavIC) are already good. Setting this
 * true adds GLONASS via $PMTK353,1,1,1,0,1 which takes effect immediately
 * without a receiver reset. Set false to leave factory defaults unchanged. */
#define GNSS_SEND_CONFIG true

/* TB6612FNG gyro-stabilisation wheel (GA12-N20) — unchanged at rev 2026-08 */
#define PIN_MOT_PWMA     33
#define PIN_MOT_AIN2     34
#define PIN_MOT_AIN1     35
#define PIN_MOT_STBY     36

/* Misc */
/* DS3231 SQW moved from pin 2 to pin 22. Worth knowing: on the old harness
 * pin 2 logged ZERO transitions (SENSOR_TESTING_FINDINGS §4), which was never
 * resolved and was suspected to be a continuity fault rather than a dead RTC.
 * This rewire may well fix it by accident. It is still unused by the firmware —
 * the RTC is read over I2C — so a dead SQW line costs nothing today. */
#define PIN_RTC_SQW      22      /* wired, currently unused — findings §4 */
#define PIN_BUS_V        A6      /* = digital pin 20 */
#define PIN_LED          LED_BUILTIN
/* ---------------------------------------------------------------------------
 * RECOVERY BEACON — DB Unlimited SM400204-1 speaker on a MAX98357A
 * ------------------------------------------------------------------------ *
 * THIS IS NOT A BUZZER PIN. The MAX98357A is a Class-D amplifier with an I2S
 * digital input: it needs a real BCLK/LRCLK/DIN stream, and it mutes itself
 * within milliseconds if the clocks stop. digitalWrite() and tone() BOTH
 * produce silence on this hardware — the pin they would toggle is not
 * connected to anything that makes sound. See Beacon.cpp.
 *
 * WHY SAI2 (pins 2/3/4) AND NOT THE DEFAULT AudioOutputI2S.
 * The Audio library's default output is SAI1, which on Teensy 4.1 is fixed at
 * OUT1A = pin 7, LRCLK1 = pin 20, BCLK1 = pin 21, MCLK1 = pin 23. Two of those
 * are already spoken for on this vehicle and neither can move:
 *     pin 7  = Serial2 RX, the GNSS receiver (hardware-fixed, see above)
 *     pin 20 = PIN_BUS_V / A6, the battery divider
 * Taking SAI1 would cost the GNSS fix and the only battery measurement on the
 * vehicle. AudioOutputI2S2 uses SAI2 instead, whose pins are free here.
 *
 * These three are HARDWARE-FIXED by the SAI2 peripheral, exactly like the
 * Serial2 pins — they are documentation, not a choice. Numbers are from the
 * Teensy 4.1 pinout card.
 *
 * BENCH CHECK BEFORE TRUSTING SILENCE: LRCLK and BCLK are adjacent pins and are
 * the one thing easy to swap. If the amp is powered, SD is high and the stream
 * is running but nothing comes out, swap pins 3 and 4 before suspecting the
 * amplifier. Also confirm your Teensyduino's output_i2s2 does not claim MCLK2
 * (pin 33) — that pin is PIN_MOT_PWMA here. The MAX98357A has no MCLK input
 * and does not need one, so if a version ever does claim it, move the gyro
 * wheel PWM rather than the amplifier. */
#define PIN_BEACON_DIN    2      /* SAI2 OUT2   -> MAX98357A DIN   (fixed)   */
#define PIN_BEACON_LRCLK  3      /* SAI2 LRCLK2 -> MAX98357A LRC   (fixed)   */
#define PIN_BEACON_BCLK   4      /* SAI2 BCLK2  -> MAX98357A BCLK  (fixed)   */
/* Amplifier shutdown / channel select. This IS a free choice, and it is the
 * pin the old GPIO-buzzer plan reserved, so the harness note stays valid.
 *
 * SD_MODE is not a plain enable — the MAX98357A reads its VOLTAGE to pick a
 * channel as well:  < 0.16 V shutdown · 0.16-0.77 V (L+R)/2 · 0.77-1.4 V right
 * · > 1.4 V left. Driven straight from a 3.3 V Teensy pin, HIGH therefore means
 * "enabled, LEFT channel only". Beacon.cpp puts the tone on BOTH channels so
 * the vehicle still makes noise whichever way a breakout's resistor divider
 * biases this pin — a mono amp fed only the right channel is silent, and that
 * failure looks exactly like a dead amplifier.
 *
 * LOW = shutdown, < 1 uA. That is the beacon's real off switch and why the
 * amplifier costs nothing during flight. */
#define PIN_BEACON_SD     37     /* HIGH = enabled (left), LOW = shutdown    */

/* Set to 0 to compile the beacon out entirely — the sketch then builds with no
 * dependency on the Teensy Audio library, and CMD_BEACON_ON still sets the
 * telemetry flag and blinks the LED exactly as before. Leave at 1 once the
 * amplifier is fitted. */
#define BEACON_ENABLED    1

/* TONE CHOICE IS A dB(A) DECISION, NOT A LOUDNESS ONE.
 * The requirement (cdr_tests/README.md, guideline 5.2.v) is >= 92 dB(A) at 1 m,
 * and the A-weighting curve is what makes this counter-intuitive: the
 * SM400204-1's resonance at 220 Hz is where the driver is most EFFICIENT, but
 * A-weighting docks 220 Hz by about 11 dB while giving +1 dB at 3 kHz. Sitting
 * on resonance would therefore measure roughly 12 dB WORSE on the meter that
 * grades us. 2.8/3.5 kHz is near the ear's peak sensitivity and comfortably
 * inside the driver's 220 Hz - 20 kHz response.
 *
 * TWO TONES, ALTERNATING. A steady tone is hard to localise — the ear finds a
 * changing one far faster, and a warble cuts through wind and traffic noise
 * that a fixed pitch disappears into. */
#define BEACON_TONE_A_HZ    2800.0f
#define BEACON_TONE_B_HZ    3500.0f
#define BEACON_SWAP_MS      350UL   /* how long each tone holds              */
/* Duty pattern: BEEP on, then silence. Halves the average current draw from
 * the 5 V rail and makes the sound more distinctive than a continuous siren. */
#define BEACON_ON_MS        1400UL
#define BEACON_OFF_MS       600UL
/* 0.0-1.0 into the amplifier. The MAX98357A delivers at most 3.2 W into 4 ohms
 * from 5 V and the speaker is rated 3 W (4 W max), so full scale CANNOT
 * over-drive it — the amplifier clips before the driver is at risk. Left just
 * below 1.0 so the Class-D output stage is not run into hard clipping
 * continuously. */
#define BEACON_AMPLITUDE    0.9f

/* ---------------------------------------------------------------------------
 * 3. I2C DEVICES
 * ------------------------------------------------------------------------ */
/* KEEP THE BUS AT 100 kHz.
 * 400 kHz is inside every device's datasheet spec but was tested on this exact
 * harness and broke the ICM-20948: 0x69 ACKs its address during a scan yet
 * returns 0xFF on every register read. Address phase works, data phase does
 * not — the textbook signature of insufficient edge rate for 3.3 kohm pull-ups
 * driving flying-lead capacitance. The DS3231 survived 400 kHz on the same
 * wires, which is why the failure originally looked like a dead IMU. */
#define I2C_CLOCK_HZ     100000

/* BMP585. Observed on this board's I2C scan: 0x47 present, 0x46 absent, i.e.
 * SDO is strapped HIGH, not to GND as the brief stated. Both are probed and
 * CHIP_ID is verified, so a re-strap cannot silently break the build. */
#define BMP_ADDR_A       0x47
#define BMP_ADDR_B       0x46
#define BMP_CHIP_ID      0x51

/* ICM-20948 — 0x69 (AD0 high). 0x68 is ALSO the DS3231's address, so the
 * alternate is only ever accepted after WHO_AM_I reads 0xEA. */
#define ICM_ADDR_A       0x69
#define ICM_ADDR_B       0x68
#define ICM_WHOAMI       0xEA

/* AK09916 magnetometer — separate die behind the ICM's aux bus. Never appears
 * in an I2C scan until the ICM's bypass bit is asserted. */
#define AK_ADDR          0x0C
#define AK_WIA2          0x09

#define RTC_ADDR         0x68

/* Consecutive failed transactions on one device before the bus is declared
 * wedged and recovery runs. 3 tolerates isolated noise without thrashing a
 * healthy bus. */
#define I2C_FAULTS_TO_RECOVER  3

/* ---------------------------------------------------------------------------
 * 4. SENSOR CONDITIONING  (measured — see SENSOR_TESTING_FINDINGS.md)
 * ------------------------------------------------------------------------ */
/* Barometer. Measured noise floor at rest 3.8 Pa pp; a real 1 m altitude change
 * is ~12 Pa. GLITCH_PA is ~13x the noise floor: a single-sample jump past it is
 * a torn I2C read (one corrupted MSB is ~1024 Pa ~= 90 m), never real pressure. */
#define BARO_MEDIAN_N        5
#define BARO_NOISE_FLOOR_PA  3.8f
#define BARO_GLITCH_PA       50.0f
#define BARO_SMOOTH_ALPHA    0.35f
#define BARO_MIN_PA          50000.0f
#define BARO_MAX_PA          110000.0f
#define BARO_SAMPLE_HZ       25         /* matches ODR code 0x14 = 25.005 Hz */

/* IMU. Bias/scale from the return-to-baseline pass (the tighter of the two —
 * the first pass was contaminated by hand tremor). */
#define GYRO_BIAS_X_DPS      0.15f
#define GYRO_BIAS_Y_DPS      0.09f
#define GYRO_BIAS_Z_DPS      0.34f
#define ACCEL_SCALE_CORR     1.01456f  /* measured |g| = 0.9856 at rest */
/* Measured gyro noise floor was 4.3-11.2 dps pp depending on axis. 12 dps is
 * ~2x the worst axis. The 0.5f used in earlier firmware was 10-20x too small
 * and let pure sensor noise through as if it were real rotation. */
/* APPLIES TO SPIN_RATE ONLY — never to the raw GX/GY/GZ telemetry.
 * It was previously applied to the raw axes too, and at 12 dps that erased
 * every hand-scale rotation (a demonstration twist is 10-40 dps), so the link
 * carried a hard 0.0 and the ground station reported the attitude display as
 * FROZEN/STALE while the IMU was in fact healthy. SPIN_RATE feeds the flight
 * state machine, where a noise-driven non-zero can trigger a real state
 * transition; that is worth a deadband. Raw telemetry is not. See the comment
 * at the deadband site in Sensors.cpp. */
#define GYRO_DEADBAND_DPS    12.0f

/* ---------------------------------------------------------------------------
 * CMD_CALIBRATE GATING — moved here from Sensors.cpp, and CAL_STILL_DPS FIXED.
 * ------------------------------------------------------------------------ *
 * THE BUG THIS FIXES, stated plainly because it wasted a lot of bench time:
 * CAL_STILL_DPS used to be 3.0 dps. Twelve lines above, this same file records
 * the MEASURED gyro noise floor on this exact part as 4.3-11.2 dps pp depending
 * on axis. The stillness gate compares the peak-to-peak SPREAD of the raw gyro
 * against that threshold, so on a perfectly motionless bench the spread is
 * 4.3-11.2 and the gate is 3.0 — it can NEVER be satisfied. Sensor noise alone
 * kept re-arming calStillMs_, CAL_ACCUM was never entered, and after
 * CAL_TIMEOUT_MS the state machine dropped silently back to CAL_IDLE keeping
 * the old bias, with nothing printed.
 *
 * That is exactly the reported symptom: "if I send calibrate it doesn't listen,
 * it avoids it". The command WAS received and WAS executed — it just could not
 * physically converge, and said nothing about it.
 *
 * 15.0 dps is ~1.34x the worst measured axis (11.2), so a genuinely still
 * vehicle passes on every axis with margin, while a hand-held or wind-buffeted
 * one (tens to hundreds of dps) is still correctly rejected. The gap between
 * 11.2 (noise) and the ~10-40 dps of a deliberate hand movement is narrow but
 * real, and 15 sits in it.
 *
 * CAL_REPORT: every phase transition and the final result now print. A
 * calibration that cannot converge must never again fail silently. */
#define CAL_ARM_MS        1500UL   /* settle before stillness is even judged  */
#define CAL_STILL_MS      2500UL   /* must stay still this long to commit     */
#define CAL_STILL_DPS     15.0f    /* was 3.0 — below the measured noise floor,
                                      so calibration could never converge      */
#define CAL_TIMEOUT_MS    20000UL  /* give up and keep the previous constants  */

/* ---------------------------------------------------------------------------
 * BAROMETER PAD REFERENCE — non-blocking, gated, single source of truth.
 * ------------------------------------------------------------------------ *
 * The old calibrateBarometer() was a blocking loop: 16 x bmpRead() with
 * delay(20) between them, 320 ms of hard stall inside a command handler. Four
 * separate faults, all fixed:
 *
 *  1. IT BLOCKED. 320 ms with delay() violates this project's stated no-delay
 *     policy, starves the 1 KB GNSS UART buffer (~1.06 s of slack at 9600 bd)
 *     and can walk straight over the radio's TX slot.
 *  2. IT AVERAGED RAW SAMPLES. bmpRead() is the bare register burst — no
 *     median-of-5, no glitch gate. One torn I2C read is worth ~1024 Pa (~90 m)
 *     and at 1/16 weight that is 64 Pa ~= 5.6 m of PERMANENT altitude offset
 *     baked into the pad reference.
 *  3. IT OVERSAMPLED. 20 ms spacing against a 25 Hz (40 ms) sensor ODR means
 *     every second read returns the same unchanged conversion. The effective
 *     independent sample count was ~8, not 16, and the duplicates are perfectly
 *     correlated so the noise does not average down as sqrt(N).
 *  4. IT WROTE THE WRONG VARIABLE — see the note on padPresPa in Sensors.h.
 *
 * The replacement accumulates the CONDITIONED pressure (median-filtered,
 * glitch-gated) on the barometer's own 25 Hz service tick for BARO_CAL_MS,
 * never blocking, and commits the mean to the one pad reference that
 * getAltitudeAGL() actually reads. */
#define BARO_CAL_MS       2000UL   /* 2 s at 25 Hz = 50 independent samples   */
#define BARO_CAL_MIN_N    20       /* refuse to commit a thinner average      */

/* TEMPERATURE-COMPENSATED SCALE HEIGHT.
 * The familiar 44330.8 constant in the hypsometric formula is not a constant at
 * all — it is T0/L, the ISA sea-level temperature over the tropospheric lapse
 * rate: 288.15 K / 0.0065 K/m = 44330.8 m. It is only correct when the air at
 * the reference level is at the ISA standard 15 C.
 *
 * This vehicle flies in India in August. The logged pad temperature is 28.9 C
 * = 302.05 K, giving a true scale height of 302.05 / 0.0065 = 46469 m — 4.8%
 * higher than the ISA figure. Using 44330.8 therefore reports every AGL
 * altitude about 4.8% LOW: ~48 m of error at the 1000 m apogee the mission is
 * graded against, and ~29 m at the 600 m secondary-deployment trigger that
 * carries a +-10 m tolerance in guideline 5.4.vi. That single fixed constant
 * was three times the allowed tolerance on a graded event.
 *
 * The pad temperature is captured alongside the pad pressure at calibration
 * time and used as T0. Bounds below reject an implausible reading (a wedged
 * I2C read, or the die still self-heating right after power-up) and fall back
 * to ISA rather than propagating nonsense into every altitude. */
#define BARO_LAPSE_K_M    0.0065f  /* ISA tropospheric lapse rate, K/m        */
#define BARO_T0_MIN_C     (-40.0f) /* below this, distrust and use ISA        */
#define BARO_T0_MAX_C     (60.0f)  /* above this, distrust and use ISA        */
#define BARO_ISA_T0_K     288.15f  /* ISA sea-level temperature, K            */

/* ---------------------------------------------------------------------------
 * ATTITUDE ESTIMATOR — SIMPLE: ACCEL TILT + GYRO YAW
 * ------------------------------------------------------------------------ *
 * WHY THIS REPLACED THE MAHONY QUATERNION FILTER. Because the Mahony filter
 * was the wrong tool for THIS problem, however textbook-correct it is in
 * general. Three real failures on the bench:
 *
 *   1. Sitting still, ATT_Q3 drifted from 0.008 to 0.111 over 36 s — a
 *      steady ~0.35 dps yaw drift being integrated with nothing to stop it.
 *      Mahony corrects roll and pitch against gravity, but there is NO
 *      absolute yaw reference on this vehicle (the magnetometer is not
 *      hard-iron calibrated), so any gyro-Z bias walks the yaw estimate away
 *      forever. That is "once it starts moving nothing stops" — literally.
 *
 *   2. The quaternion carries a sign ambiguity: q and -q represent the same
 *      rotation. Display them as four numbers and the reader sees jumps that
 *      are not real motion. Every attempt to convert to Euler for the numeric
 *      panel hit the same gimbal lock the whole quaternion path was supposed
 *      to avoid.
 *
 *   3. The whole complexity paid for exactly one property — behaviour at
 *      pitch +-90 deg. On a bench, tilts are <=45 deg, well clear of that.
 *      Even in flight, gimbal lock hurts a numeric display only; the CanSat
 *      does not steer, so an ambiguous yaw at zenith costs nothing.
 *
 * UPDATE — A SCOPED QUATERNION IS BACK, FOR ONE CONSUMER ONLY. The ground
 * station's 3-D model needs exactly the property item 3 says the numeric
 * panels don't: no +-90 deg pitch ceiling, so a real tumble renders
 * correctly instead of snapping at vertical. Sensors.cpp's quatUpdate() adds
 * a second, independent Mahony filter (accel+gyro, Kp only, no Ki, no
 * magnetometer) whose quaternion output feeds ONLY the 3-D model over the
 * wire — see B36_QUAT in Base36.h. It does not feed ATT_ROLL/PITCH/YAW,
 * which are computed exactly as documented above, unchanged. Failures 1 and
 * 2 above were about a quaternion feeding a NUMBER; this one only ever feeds
 * a ROTATED POINT ON SCREEN, which is insensitive to both the yaw-drift
 * magnitude (same drift as attYaw_, just not worse) and the q/-q sign
 * ambiguity (rotating a point by q or -q lands it in the same place).
 *
 * WHAT THIS DOES INSTEAD. Roll and pitch straight from atan2 on the
 * accelerometer. Exact at rest, noisy for the second an acceleration lasts,
 * then instantly settled again — anchored to gravity, cannot drift. Yaw is
 * gyro-Z integrated from zero. It DOES drift, honestly, at whatever the gyro
 * bias is; CMD_CALIBRATE resets it to zero. Yaw here is relative rotation, not
 * a heading. Absolute heading needs a magnetometer that has been calibrated
 * inside the assembled vehicle.
 *
 * THE SD LOG STAYS COMPLETE. Raw ACC_*, GX/GY/GZ, MX/MY/MZ and the derived
 * ROLL/PITCH/YAW all reach the card at 20 Hz. This is the display estimator
 * only; nothing about the ground-truth record changes.
 *
 * THE ACCELEROMETER GATE. When |a| is far from 1 g the accelerometer is
 * measuring thrust or ejection shock, not down, so it is not usable as a
 * gravity reference for that sample. In this simple estimator we still
 * compute the atan2 (numeric-panel value), and the operator sees roll and
 * pitch wobble during the acceleration second — that is the honest reading,
 * not a fiction from a filter that hides the disturbance. */
#define ATT_GATE_LO_G        0.75f
#define ATT_GATE_HI_G        1.25f

/* ---------------------------------------------------------------------------
 * ATTITUDE AXIS SIGNS — set these once, on the bench, by observation.
 * ------------------------------------------------------------------------ *
 * The filter is correct in the SENSOR's frame. Whether that matches the
 * VEHICLE's frame depends on how the ICM-20948 breakout happens to be soldered
 * and which way up the plate is fitted — a physical fact no amount of maths
 * can infer. These three constants flip an axis without touching the filter.
 *
 * TEST PROCEDURE (30 seconds, watch the BODY AXES panel on the GCS):
 *
 *   1. Stand the CanSat upright, nose up. This is home.
 *      Expect ROLL ~0, PITCH ~0, TILT ~0.
 *
 *   2. TILT THE NOSE FORWARD (away from you), about 45 deg.
 *      PITCH should go POSITIVE. If it goes negative -> flip ATT_SIGN_PITCH.
 *
 *   3. Return upright. TILT THE TOP TO YOUR RIGHT, about 45 deg.
 *      ROLL should go POSITIVE. If it goes negative -> flip ATT_SIGN_ROLL.
 *
 *   4. Return upright. ROTATE IT CLOCKWISE seen from above (turn right), 90 deg.
 *      YAW should INCREASE by ~90. If it decreases -> flip ATT_SIGN_YAW.
 *
 * Change only the sign, never the magnitude. If an axis moves the RIGHT way
 * but the WRONG axis responds — nose-up moving ROLL instead of PITCH — then
 * the breakout is mounted rotated 90 deg about the long axis and the fix is
 * mechanical or a swap in the attitude formula, not a sign here.
 *
 * ^ THAT IS EXACTLY WHAT WAS WRONG, and it is now fixed. The six-position
 * test on 2026-08-06 measured a 90 deg mount rotation about Z; the swap lives
 * in the atan2 formula in Sensors.cpp. This paragraph is kept because it
 * correctly predicted the failure mode for months before anyone ran the test.
 * See the AXIS SIGNS block below and docs/IMU_AXIS_CALIBRATION_2026-08-06.md.
 *
 * These are applied to the filter OUTPUT only, so the internal integration and
 * the SD log stay in one consistent frame. */
/* ---------------------------------------------------------------------------
 * AXIS SIGNS — MEASURED 2026-08-06, not guessed. Do not change by trial.
 * ------------------------------------------------------------------------ *
 * These flip the sign of the PUBLISHED angle only. Internal integration is
 * unchanged, so a sign here cannot destabilise anything.
 *
 * SET FROM A SIX-POSITION STATIC TEST. Full data and working:
 *   docs/IMU_AXIS_CALIBRATION_2026-08-06.md
 *   sensor_tests/test_imu_axis_calibration/
 *
 * ROLL and PITCH are +1 because the real error was NOT a sign — the breakout
 * is mounted rotated 90 deg about its own Z (sensor +X = vehicle RIGHT,
 * sensor +Y = vehicle FRONT). That is an axis SWAP, and a swap cannot be
 * expressed as a negation, which is exactly why flipping these three
 * constants never converged: correcting roll always broke pitch. The swap is
 * now handled where it belongs, in the atan2 formula in Sensors.cpp. With it
 * in place the measured plateaus give the right sense already:
 *     RIGHT 90 -> roll  +91.1     LEFT 90  -> roll  -88.4
 *     BACK 90  -> pitch +82.0     FRONT 90 -> pitch -79.2
 *
 * YAW is -1 because sensor +Z points UP, so a clockwise turn seen from above
 * is a NEGATIVE rotation about it under the right-hand rule. The measured run
 * confirmed sustained negative gz throughout a clockwise turn. Negating here
 * makes clockwise read as increasing yaw, which is the aviation convention
 * the ground station displays.
 *
 * RE-VERIFY (30 s, watch the BODY AXES panel) after any remount:
 *   Boot upright, wait for the "[ATT] datum" line, then it reads 0,0,0.
 *     1. Tilt the NOSE FORWARD (away from you) ~30 deg -> PITCH POSITIVE.
 *     2. Return upright. Tilt the TOP TO YOUR RIGHT ~30 deg -> ROLL POSITIVE.
 *     3. Return upright. Rotate CLOCKWISE from above 90 deg -> YAW INCREASES.
 *   If the WRONG angle responds (nose-up moving ROLL), the board has been
 *   remounted and the SWAP in Sensors.cpp is wrong — re-run the six-position
 *   test. Do not try to fix that case with these signs. */
#define ATT_SIGN_ROLL       (+1.0f)
#define ATT_SIGN_PITCH      (+1.0f)
#define ATT_SIGN_YAW        (-1.0f)

/* ---------------------------------------------------------------------------
 * ATTITUDE DATUM — the vehicle reads 0,0,0 at boot and moves from there.
 * ------------------------------------------------------------------------ *
 * Once the complementary filter has settled, its current attitude is captured
 * as a reference and subtracted from everything published afterwards. The
 * ground station therefore starts at 0/0/0 in whatever position the CanSat
 * happened to be sitting, and every angle it shows is movement RELATIVE to
 * that datum — which is what an operator watching a live model actually wants.
 *
 * WHY A DELAY AND NOT AN INSTANT ZERO AT t=0. At the first pass the filter has
 * no history: roll and pitch come almost entirely from one accelerometer
 * sample, and yaw is arbitrary. Zeroing then would bake that noise into the
 * datum permanently. 3000 ms is ~3x the 1 s filter cross-over, long enough for
 * gravity to have pulled roll and pitch onto the true vertical.
 *
 * WHAT THIS COSTS. Roll and pitch stop being absolute angles from vertical.
 * If the vehicle boots tilted, "0" means that tilt, not level. TILT in the GCS
 * is still computed from raw ACC_Z and remains a true angle from vertical, so
 * the absolute measurement is not lost — it moved to the field that is
 * actually defined to carry it. CMD_CALIBRATE re-captures the datum, so the
 * vehicle can be re-zeroed on the pad once it is in its real flight attitude.
 *
 * THE DELAY APPLIES TO THE AUTOMATIC BOOT DATUM ONLY — NOT TO CMD_CALIBRATE.
 * The two cases are not the same and used to share one code path, which is why
 * a commanded re-zero appeared to do nothing for three seconds:
 *
 *   BOOT (automatic).   Nobody has asserted that the vehicle is in a meaningful
 *     pose. The filter has no history, roll and pitch come from essentially one
 *     accelerometer sample, and zeroing at t=0 would bake that noise into the
 *     datum permanently. Wait ATT_ZERO_DELAY_MS (~3x the 1 s filter cross-over)
 *     for gravity to pull roll and pitch onto true vertical first.
 *
 *   CMD_CALIBRATE (commanded).  The operator has DELIBERATELY placed the
 *     vehicle where it is and is asserting "this pose is home". The filter has
 *     already been running and converged for however long the vehicle has been
 *     powered, so there is nothing left to wait for — the settling argument
 *     above simply does not apply. Capture immediately, on the very next IMU
 *     sample, and report it. Waiting here is not caution, it is just latency
 *     that makes the command look ignored. */
#define ATT_ZERO_DELAY_MS   3000UL

/* Bus sense: 100k/22k divider on A6, 12-bit ADC, 3.3 V reference.
 *   V = adc/4095 * 3.3 * (122/22)
 * Cross-checked against the logged raw ADC: 1762 -> 7.87 V. The divider maths
 * is confirmed correct; what the 7.87 V was measured ACROSS is not (see below). */
#define BUS_V_RATIO          (122.0f / 22.0f)
#define ADC_BITS             12
#define ADC_AVERAGING        16

/* BATTERY — 3S Li-ion, per PDR 5.5.1 and the Fusion model (three 21700 cells
 * behind a 3S 40 A BMS). Guideline 5.3.iii allows Li-ion in a metal can and
 * forbids LiPo; 21700 complies.
 *
 * OPEN ITEM: the bench ADC log reads a steady 7.85-7.90 V. On a 3S pack that
 * is 2.62 V/cell, below any BMS cut-off — so the bench was NOT running on the
 * flight pack. Confirm what was actually connected before flight. If the
 * vehicle really flies 2S, change BATT_CELLS here AND the BUS_V limits in
 * vritra_gcs.py together; changing one without the other produces either a
 * permanent false alarm or a silently wrong state of charge. */
#define BATT_CELLS           3
#define BATT_EMPTY_V         9.00f     /* 3.00 V/cell */
#define BATT_FULL_V          12.60f    /* 4.20 V/cell */

/* ---------------------------------------------------------------------------
 * 5. RF PLAN — ORGANISER-ASSIGNED, NOT A TEAM CHOICE
 * ------------------------------------------------------------------------ *
 * Source: "CAN7USAT Team_Frequency_Assignments_19_10" (IN-SPACe, 19-10-2026),
 * row 025 — this REPLACES the earlier plan (867.00 / 865.75 MHz, SF10, 0x98):
 *   868 Main 866.50 MHz · 868 Backup 865.25 MHz · SF9 (pad 3) · Sync 0x99
 *
 * NOTE: the airtime analysis below was written for SF10. SF9 halves the
 * symbol time, so every SF10 figure is an upper bound: a 99 B packet is
 * ~493 ms of airtime at SF9 (~985 ms at SF10). The SF10-sized guards are
 * therefore conservative and remain safe; they are left unchanged.
 *   Bandwidth 125 kHz (mandatory — India caps the carrier at 200 kHz)
 *   Coding rate 4/5 · LoRa modulation (not FSK)
 *   TX power +10 dBm on pad, ~+14 dBm in flight
 * "Never transmit on any frequency not assigned to you."
 *
 * These override BOTH the PDR (which declares 865 MHz and SF9 — that is now
 * wrong and needs a CDR correction) and any earlier team decision. Do not
 * change a single value in this block without a new allocation document.
 *
 * The organiser's sync word 0x99 already encodes the team identity (0x80 +
 * team id), which is what guideline §6.ii's NETID/PANID clause is asking for.
 *
 * AIRTIME AT SF10 / BW125 / CR4-5, explicit header, CRC on, preamble 8:
 *   Ts        = 2^10 / 125000                              = 8.192 ms
 *   preamble  = (8 + 4.25) x 8.192                         = 100.4 ms
 *   n_payload = 8 + ceil((8*PL - 4*SF + 28 + 16)/(4*SF)) * (CR+4)
 *             = 8 + ceil((8*PL + 4)/40) * 5
 *   PL =  99 B  -> 108 sym ->  885 ms + 100 =  985 ms   <- ceiling for 1 Hz
 *   PL = 113 B  -> 123 sym -> 1008 ms + 100 = 1108 ms   <- 0.90 Hz, too slow
 *
 * THE 99-BYTE WALL. At the assigned SF10 a 1 Hz downlink cannot exceed 99
 * bytes. Plain decimal ASCII does not fit inside it once every field is at
 * full width; base-36 does, with margin. MEASURED, not estimated — the codec
 * was exercised over the full physical range of all 36 fields:
 *   typical packet     73 B ->  83 sym ->  680 ms + 100 =  780 ms
 *   worst case         90 B -> 103 sym ->  844 ms + 100 =  944 ms
 *   uplink command     12 B ->  23 sym ->  189 ms + 100 =  289 ms
 * The worst case matters more than the typical one: with decimal it landed at
 * ~1108 ms, so the cadence stretched past 1 Hz exactly during ejection shock
 * when accelerometer and spin fields hit full deflection. Base-36 keeps every
 * packet under a second unconditionally.
 *
 * Only the 15 mandated fields plus one rotating optional value are sent.
 * Everything else goes to the SD card, which guideline §6.3.iii.C explicitly
 * blesses ("Only the transmitted telemetry is graded, however, the backup data
 * can be used when completing the Post Flight Review").
 */
#define FREQ_MAIN_MHZ    866.50f       /* 19-10 allocation row 025, Main */
#define FREQ_BACKUP_MHZ  865.25f       /* 19-10 allocation row 025, Backup —
                                          switch ONLY on Mission Control's
                                          call, never unilaterally        */
#define LORA_SF          9             /* 19-10 allocation row 025       */
#define LORA_BW_KHZ      125.0f        /* mandatory                      */
#define LORA_CR          5             /* 4/5, mandatory                 */
#define LORA_SYNC        0x99          /* 19-10 allocation = 0x80 + 25   */
#define LORA_PREAMBLE    8

/* TX POWER IS STAGED, per the allocation plan: "+10 dBm on pad / ~+14 dBm in
 * flight". The previous firmware ran +22 dBm everywhere, which is 8-12 dB over
 * the assigned level and would have interfered with adjacent pads. The flight
 * computer steps the power itself on the FS_BOOT -> FS_LAUNCH transition, so
 * no operator action is required and nothing can be forgotten on the pad.
 * Link budget at +14 dBm, 2 km slant, with the RadioMaster dual-band whip on
 * BOTH ends — this is the antenna actually fitted right now, at both the
 * CanSat and the ground station. The PDR's 11 dBi Yagi is not attached;
 * swap this section back once it is (that recovers ~7.4 dB of margin):
 *   FSPL(2.24 km, 867 MHz) = 98.2 dB
 *   RX = 14 + 3.58 + 3.58 - 0.6 - 2.1 - 98.2 = -79.7 dBm  vs  SF10 sens -132 dBm
 *   -> 52.3 dB raw margin; 40.3 dB after 12 dB of polarisation/blockage/fade.
 * In flight the link is horizon-limited, not budget-limited. The binding case
 * is AFTER LANDING, where two-ray ground reflection takes over — see
 * COMPLIANCE_AUDIT.md §3 for the full range table. Short version: post-landing
 * range is set by GROUND-STATION MAST HEIGHT, not by transmit power. */
#define TX_POWER_PAD_DBM     10
#define TX_POWER_FLIGHT_DBM  14

/* ---------------------------------------------------------------------------
 * 5b. ANTENNA — RadioMaster dual-band 2.4 GHz / Sub-G 900 MHz
 * ------------------------------------------------------------------------ *
 * Vendor figures: 3.58 dBi @ 900 MHz (SWR 1.25), 3.83 dBi @ 2.4 GHz (SWR 1.27),
 * 50 ohm, omnidirectional, 100 mm long, 80 mm coax, 15.78 g, RP-SMA.
 *
 * NOTHING IN FIRMWARE SELECTS AN ANTENNA. There is no register for it. The
 * numbers above exist only so the link budget in this file and in
 * COMPLIANCE_AUDIT.md stay honest. What the antenna change DOES affect is
 * physical integration, and three items there are load-bearing:
 *
 *  1. IT CONNECTS TO ANT2 ONLY. The Core1121-XF brings out two separate u.FL
 *     ports on two separate matched RF chains: ANT1 = 2.4 GHz, ANT2 = sub-GHz
 *     (LORA_ANT). This antenna is single-feed, so it physically reaches one of
 *     them. Ours is 867 MHz, so it goes on ANT2 and the 2.4 GHz resonance is
 *     simply unused — 15.78 g carried for a band the frequency allocation plan
 *     assigns to XBee, not to our LoRa link.
 *
 *  2. RP-SMA vs u.FL. The listed part is RP-SMA; the module is u.FL. A u.FL to
 *     RP-SMA pigtail costs ~0.4 dB and is budgeted above. RadioMaster also sell
 *     an IPEX1 (u.FL) version of the same antenna, which removes the adapter,
 *     the loss, and a mechanical failure point at 30 g of shock.
 *
 *  3. ALUMINIUM PLATES ARE THE REAL RISK. At 867 MHz, lambda = 345.8 mm and
 *     lambda/4 = 86.5 mm. An aluminium plate closer than that to the radiating
 *     element detunes it, becomes an uncontrolled ground plane and nulls the
 *     pattern in the plane of the plate. The vehicle is only 150 mm across, so
 *     that clearance CANNOT be achieved inside the stack: the element has to
 *     sit outside the aluminium, along the long axis. Glass-fibre rods are
 *     RF-transparent (eps_r ~4.4) and are the right thing to mount it against.
 * ------------------------------------------------------------------------ */
#define ANT_GAIN_CANSAT_DBI   3.58f    /* vendor claim, unverified at 867 MHz */
#define ANT_GAIN_GROUND_DBI   3.58f    /* RadioMaster dual-band whip — the
                                         * PDR's 11 dBi Yagi is NOT currently
                                         * attached at the ground station.
                                         * Change back to 11.0f when it is. */
#define ANT_FEED_LOSS_DB      0.6f     /* u.FL->RP-SMA pigtail + connector    */
#define GS_FEED_LOSS_DB       2.1f     /* 3 m RG58 at 867 MHz + connectors    */
/* Core1121-XF TCXO. 1.6 V is the value empirically proven on this hardware.
 * 0.0f causes PLL instability. Do NOT re-derive this from the schematic — that
 * was tried, produced 0.0f, and was wrong. */
#define TCXO_V           1.6f

/* setOutputPower's second argument selects the PA path. It MUST stay true even
 * at these lower powers. The Core1121-XF wires the HP path only (R3 = 0R,
 * R4 = NC) and its antenna matching is built for HP; the LP pin is physically
 * unconnected, so routing through LP does not attenuate the signal, it simply
 * fails to reach the antenna. The HP PA covers -9..+22 dBm, so +10 and +14 are
 * both well inside its range. */
#define TX_USE_HP_PA     true

/* Buffer sizes. The downlink is variable-length ASCII; this is the ceiling. */
#define DL_PACKET_MAX    140
#define UL_PACKET_MAX    48
/* Hard airtime guard. A packet longer than this cannot make 1 Hz at SF10 and
 * is refused at build time rather than silently stretching the cadence. */
#define DL_AIRTIME_LIMIT 99

#define TX_PERIOD_MS         1000UL    /* guideline §6.3.i */
#define TX_PERIOD_PAD_MS     2000UL    /* on the pad, commanding beats rate  */
#define TX_TIMEOUT_MS        3000UL

/* UPLINK CADENCE. A 20 B command at SF10 costs 370 ms of airtime and the
 * "received" IRQ only fires at the END of reception, so the receive window has
 * to span the whole packet. With a 944 ms downlink there is no room to listen
 * on every cycle, so:
 *   - on the pad (FS_BOOT) the downlink drops to 0.5 Hz, leaving ~1 s of
 *     listening every cycle. This is where commanding actually matters —
 *     guideline §6.1.ii/v require the vehicle to be commanded into transmit
 *     before launch.
 *   - in flight the window opens every RX_EVERY_N_CYCLES-th cycle. That cycle
 *     runs ~1.35 s, so the average downlink rate is 5/5.34 s = 0.94 Hz. The
 *     1 Hz clause in §6.3.i binds telemetry COLLECTION, which runs at 20 Hz to
 *     the SD card; this is the transmit cadence and it stays as close to 1 Hz
 *     as the assigned SF10 physically permits. */
#define RX_WINDOW_MS         400UL
#define RX_OVERRUN_MS        300UL
/* 780 ms downlink + 25 ms gap + 289 ms command = 1094 ms, so a window still
 * cannot open on every flight cycle — but at 1 in 3 the average rate is
 * 3/3094 ms = 0.97 Hz and worst-case command latency is 3 s. With plain
 * decimal this had to be 1 in 5. */
#define RX_EVERY_N_CYCLES    3

/* GUIDELINE §6.1.v: "The CAN-7USAT shall not transmit telemetry until
 * commanded by the team." The vehicle therefore boots SILENT and stays in
 * continuous receive until CMD_TX_ON arrives. This is a hard requirement, not
 * a preference — the previous firmware booted transmitting. */
#define TX_ENABLED_AT_BOOT   false

/* ---------------------------------------------------------------------------
 * 6. DOWNLINK WIRE FORMAT — base-36 ASCII CSV, guideline §6.3.i
 * ------------------------------------------------------------------------ *
 * §6.3.i: "The telemetry data shall be transmitted with ASCII comma-separated
 * fields followed by a carriage return in the following format:
 *   <TEAM ID>,<TIME STAMPING>,<PACKET COUNT>,<ALTITUDE>,<PRESSURE>,<TEMP>,
 *   <VOLTAGE>,<GNSS TIME>,<GNSS LATITUDE>,<GNSS LONGITUDE>,<GNSS ALTITUDE>,
 *   <GNSS SATS>,<ACCELEROMETER DATA>,<GYRO SPIN RATE>,<FLIGHT SOFTWARE STATE>,
 *   <ANY OPTIONAL DATA>"
 *
 * The link satisfies all four literal requirements — ASCII, comma-separated,
 * carriage-return terminated, mandated field order — with each numeric field
 * carried as a base-36 integer of (value x scale). A packed-binary downlink
 * would NOT satisfy them and has been removed.
 *
 * WHERE THE INTERPRETATION RISK LIVES, stated openly: §6.3.iii's table gives a
 * "display format with resolution" per field (0.1 m, 1 Pa, 0.01 V, 0.0001 deg).
 * Read as describing the DISPLAY — which is what the column is headed — the
 * gateway and GCS satisfy it exactly, because they render precisely those
 * resolutions in the graded CSV. Read as describing the on-air bytes, base-36
 * would not. This is a deliberate team decision, not an oversight; see
 * COMPLIANCE_AUDIT.md §2 for the full argument and the fallback.
 * The fallback is cheap: the codec is isolated in Base36.h, so reverting to
 * plain decimal touches two functions and costs ~200 ms of airtime per packet.
 *
 * Transmitted layout (\r terminated), wire layout 3 — 21 fields:
 *   <TEAM>,<TIME_S>,<PKT>,<ALT_M>,<PRES_PA>,<TEMP_C>,<BUS_V>,<GNSS_TIME>,
 *   <LAT>,<LON>,<GPS_ALT>,<SATS>,<ACC_X>,<ACC_Y>,<ACC_Z>,<SPIN>,<STATE>,
 *   <FLAGS>,<ATT_ROLL>,<ATT_PITCH>,<rotating optional value>
 * Fields 0..15 are the mandated set in the mandated order. Everything from
 * <FLAGS> onward is "<ANY OPTIONAL DATA>", which §6.3.iii.B explicitly permits
 * teams to append. There is NO ",F<n>" tag any more — layout 2 removed it and
 * the gateway derives the rotation slot as (PKT - 1) % N_FRAMES. That is why
 * the two attitude fields keep their separators even when empty: position 20
 * is identified by POSITION alone. See B36_WIRE_LAYOUT in Base36.h.
 *
 * LAYOUT 3 DROPPED ATT_YAW AND QUAT_X/Y/Z — 12 B per packet, 86 B -> 74 B and
 * 903 ms -> 780 ms of airtime, for attitude the ground station displayed
 * nowhere. ATT_ROLL/ATT_PITCH stayed: guideline table row 13 defines
 * <ACCELEROMETER DATA> as "acceleration and roll & pitch parameters", so those
 * two are mandated content. All four removed values are still written to the
 * SD card at 20 Hz by DataLogger.cpp, where bytes are free.
 *
 * Resolutions meet or beat §6.3.iii: ALT 0.1 m, PRES 1 Pa, TEMP 0.1 C,
 * VOLTAGE 0.01 V, GNSS LAT/LON 1e-6 deg (spec asks 1e-4), GNSS ALT 0.1 m,
 * GNSS TIME whole seconds (hhmmss), SATS integer, ACCEL 0.01 m/s^2,
 * SPIN 0.1 deg/s.
 *
 * TEAM ID ON AIR — a compliance judgement call, stated plainly.
 * The full string "2026-INSPACe-CAN-7USAT-025" is 26 of the 99 available
 * bytes: a quarter of the entire 1 Hz budget spent re-sending a constant. The
 * short team number "025" is transmitted instead and the gateway expands it to
 * the full identifier before the CSV the judges examine (§6.3.ii), so the
 * graded artefact is byte-identical either way. If the jury wants the full
 * string over the air, set TEAM_ID_AIR to TEAM_ID_FULL below — the packet then
 * costs 1108 ms and the downlink drops to 0.90 Hz. That is the whole trade.
 * ------------------------------------------------------------------------ */
#define TEAM_ID_AIR      "025"
/* One optional field per packet is all the 99-byte wall permits, so a field in
 * the rotation refreshes once every N_FRAMES packets — 16 s at the 1 Hz flight
 * cadence, 32 s at the 0.5 Hz pad cadence. Everything is on the SD card at
 * 20 Hz regardless; this rotation exists so the GCS panels populate, not
 * because the link is the system of record.
 *
 * WAS 19. AX/AY/AZ were removed because they duplicated the mandated
 * ACC_X/ACC_Y/ACC_Z that ride in every packet — the gateway derives them by
 * dividing by 9.80665 instead. That both makes those three live at full rate
 * and shortens the wait for GX/GY/GZ and MX/MY/MZ, which genuinely cannot be
 * derived. MUST equal B36_OPT_N in Base36.h; b36SchemaSum() now folds the
 * frame count in, so a mismatch between the two boards is caught at boot
 * instead of silently decoding every optional value into the wrong field. */
#define N_FRAMES         16
#define OPT_PER_FRAME    1

/* FLAGS is a small integer decoded by the gateway into GCS fields 67/68/71.
 * Status bits ride in EVERY packet rather than rotating, so the operator never
 * looks at a five-second-stale beacon or watchdog state.
 * The three indices below moved down by 3 when AX/AY/AZ left SCHEMA (90 -> 87
 * fields); they are the ones vritra_gs.ino writes as rec[67]/rec[68]/rec[71]
 * and verify_schema.py checks by name. */
#define FLAG_ARMED       0x01
#define FLAG_TX_ENABLED  0x02
#define FLAG_BEACON      0x04          /* -> GCS 71 BEACON_ON  */
#define FLAG_WDOG_OK     0x08          /* -> GCS 68 WDOG_OK    */
#define FLAG_MCU_BACKUP  0x10          /* -> GCS 67 MCU_ACTIVE */
#define FLAG_BARO_OK     0x20
#define FLAG_IMU_OK      0x40
#define FLAG_SD_OK       0x80

/* ---------------------------------------------------------------------------
 * 7. UPLINK — also ASCII, "<TEAM>,C,<id>,<arg>,<seq>" + \n
 * ------------------------------------------------------------------------ *
 * IDS 4-8 ARE RETIRED, NOT DELETED, AND THE GAP IS DELIBERATE.
 *
 * ARM / DISARM / FORCE_EJECT / FORCE_SEC_EJ / BRAKE_DEPLOY were removed from
 * the ground station's command list and from execCmd(): this vehicle carries no
 * pyrotechnic ejection charge, no secondary charge and no drag brake, so three
 * of them commanded actuators that do not exist, and the arming pair is
 * superseded by the auto-arm below.
 *
 * THE NUMBERS STAY RESERVED. The uplink carries the command as an INTEGER, so
 * renumbering 9-18 down into the gap would silently change the meaning of every
 * id on the wire. A gateway or flight computer flashed from a different commit
 * would then execute the WRONG command rather than failing — CMD_BEACON_ON (9)
 * would arrive as CMD_FORCE_SEC_EJ. Leaving the holes costs one byte of enum
 * space and makes that class of mistake unrepresentable. Never reuse 4-8 for a
 * new command; take 19 and upward.
 *
 * ARMING IS NOW IMPLICIT IN CMD_TX_ON. Guideline 6.1.v already requires the
 * vehicle to be commanded into transmit before flight, so that command is
 * necessarily the last deliberate operator action on the pad — which makes it
 * exactly the right place to arm the flight state machine. One action, no
 * separate step to forget, and no silent auto-arm at power-on. armed and
 * FLAG_ARMED are unchanged in telemetry; only what sets them moved.
 * CMD_TX_OFF does NOT disarm: silencing the downlink mid-flight must never
 * reset the state machine that is tracking the descent. */
enum CmdId : uint8_t {
    CMD_NONE          = 0,
    CMD_TX_ON         = 1,   /* also arms the FSM — see note above */
    CMD_TX_OFF        = 2,
    CMD_CALIBRATE     = 3,
    /* 4..8 RETIRED — see note above. Do not reuse. */
    CMD_BEACON_ON     = 9,
    CMD_BEACON_OFF    = 10,
    CMD_FAILOVER      = 11,
    CMD_RESET         = 12,
    CMD_GYRO_ON       = 13,
    CMD_GYRO_OFF      = 14,
    CMD_GYRO_SPD      = 15,   /* arg = percent */
    CMD_MAG_CAL       = 16,
    CMD_GPS_RESET     = 17,
    CMD_NEW_MISSION   = 18,
};

/* ---------------------------------------------------------------------------
 * 8. FLIGHT STATE MACHINE
 * ------------------------------------------------------------------------ */
/* Values are the GCS STATE enum (vritra_gcs.py STATES). Do not renumber. */
enum FlightState : uint8_t {
    FS_BOOT      = 0,
    FS_LAUNCH    = 1,
    FS_EJECT     = 2,
    FS_SEC_EJECT = 3,
    FS_TOUCHDOWN = 4,
};

/* Mission profile, from the guidelines rather than guesswork:
 *   §3.1  launch to 1000 (+/-100) m, deployed near peak altitude
 *   §5.4.v   primary descent rate < 20 m/s (variation within +/-5 m/s)
 *   §5.4.vi  secondary descent control activates at 600 m (+/-10 m),
 *            reducing descent to 1-3 m/s
 * SEC_EJECT_AGL_M is therefore 600, NOT a guessed value — this is a graded
 * requirement with a +/-10 m tolerance. */
#define FSM_SEC_EJECT_AGL_M   600.0f
#define FSM_LAUNCH_AGL_M       30.0f   /* UNVERIFIED — needs a real pad profile */
#define FSM_LAUNCH_VS_MS        5.0f   /* UNVERIFIED */
#define FSM_APOGEE_VS_MS       -3.0f   /* UNVERIFIED */
#define FSM_LANDED_AGL_M       30.0f   /* UNVERIFIED */
#define FSM_LANDED_VS_MS        1.0f   /* UNVERIFIED */
#define FSM_PERSIST_N           4      /* consecutive samples to transition   */
#define FSM_LANDED_PERSIST_N   20      /* landing needs a longer, calmer proof */

/* ---------------------------------------------------------------------------
 * 8b. TIMEKEEPING — the DS3231 is the only clock that survives a power cycle
 * ------------------------------------------------------------------------ *
 * WHAT EACH CLOCK IS GOOD FOR, because they are not interchangeable:
 *
 *   millis()   monotonic, resets to 0 every boot. Fine for cadence, useless
 *              for anything that has to mean something after a reset.
 *   GNSS UTC   authoritative to well under a second, but ONLY once there is a
 *              fix — which is exactly the thing we are trying to get faster.
 *   DS3231     battery-backed on its own coin cell, ±2 ppm (about ±1 minute
 *              per year). This is the one clock on the vehicle that still
 *              knows the time at the instant of a cold boot.
 *
 * THAT LAST POINT IS THE WHOLE FAST-RELOCK STRATEGY. The L89's own backup
 * supply is assumed absent, so the receiver loses its ephemeris AND its
 * internal clock every time the vehicle is powered down — every boot is a full
 * cold start with no time, no position and no orbits. But the DS3231 does not
 * lose anything, and a GNSS receiver given an accurate time and a rough
 * position can skip most of the search: it knows which satellites should be
 * overhead and roughly what Doppler to expect, instead of sweeping the whole
 * code/frequency space blindly.
 *
 * So the chain is: GNSS fix -> write UTC into the DS3231 (it becomes accurate
 * and STAYS accurate on its coin cell) and write position to SD -> next boot,
 * read both back and hand them to the receiver before it starts searching.
 * The first lock is slow exactly once; every lock after that is aided.
 *
 * THE RTC IS KEPT IN UTC, NOT LOCAL TIME. GNSS time is UTC, the graded
 * telemetry is UTC, and the aiding commands take UTC — storing local time
 * would mean converting in three places and getting it wrong in one. Local
 * time is derived for display only, via TZ_OFFSET_MIN. */
#define TZ_OFFSET_MIN        330       /* IST = UTC+05:30 (Mumbai). Display
                                          and the SD CSV's LOCAL_TIME column
                                          only — never what is stored.       */
#define TZ_NAME              "IST"

/* BUILD-TIME SEED. __DATE__ / __TIME__ are substituted by the compiler with
 * the build machine's LOCAL wall clock, so on a laptop set to India these are
 * IST and are converted to UTC with TZ_OFFSET_MIN below.
 *
 * This exists for one specific case: a DS3231 that has never been set, or
 * whose coin cell has died, reads something absurd (2000-01-01, or 2165). A
 * receiver aided with an absurd time is WORSE OFF than one given no time at
 * all, because it will confidently search the wrong satellites. Seeding from
 * the build stamp puts it within a few minutes of correct — good enough to
 * make aiding help rather than hurt, and it is replaced by real GNSS UTC the
 * moment the first fix arrives.
 *
 * ACCURACY IS MINUTES, NOT SECONDS: the stamp is when the sketch was
 * COMPILED, not when it was uploaded or when the vehicle was switched on. It
 * is a floor under the clock's sanity, not a time source. */
#define BUILD_YEAR   ((__DATE__[7]-'0')*1000 + (__DATE__[8]-'0')*100 + \
                      (__DATE__[9]-'0')*10   + (__DATE__[10]-'0'))
#define BUILD_MONTH  (__DATE__[0]=='J' ? (__DATE__[1]=='a' ? 1 : (__DATE__[2]=='n' ? 6 : 7)) : \
                      __DATE__[0]=='F' ? 2  : \
                      __DATE__[0]=='M' ? (__DATE__[2]=='r' ? 3 : 5) : \
                      __DATE__[0]=='A' ? (__DATE__[1]=='p' ? 4 : 8) : \
                      __DATE__[0]=='S' ? 9  : \
                      __DATE__[0]=='O' ? 10 : \
                      __DATE__[0]=='N' ? 11 : 12)
#define BUILD_DAY    ((__DATE__[4]==' ' ? 0 : __DATE__[4]-'0')*10 + (__DATE__[5]-'0'))
#define BUILD_HOUR   ((__TIME__[0]-'0')*10 + (__TIME__[1]-'0'))
#define BUILD_MIN    ((__TIME__[3]-'0')*10 + (__TIME__[4]-'0'))
#define BUILD_SEC    ((__TIME__[6]-'0')*10 + (__TIME__[7]-'0'))

/* An RTC reading outside this window is not believed and is re-seeded. The
 * lower bound is "before this firmware existed"; the upper is a loose sanity
 * ceiling that still catches the classic 2165 / 0 / 0xFF garbage a dead cell
 * or a torn I2C read produces. */
#define RTC_MIN_YEAR         2025
#define RTC_MAX_YEAR         2099

/* ---------------------------------------------------------------------------
 * 8c. GNSS AIDING — turning a cold start into a warm one
 * ------------------------------------------------------------------------ *
 * Save the last known position to SD and the time to the DS3231, then feed
 * both back on the next boot. Typical time-to-first-fix, all with a clear sky:
 *   cold  (nothing known)                      30-60 s, sometimes minutes
 *   warm  (time + position known, no orbits)   ~15-30 s
 *   hot   (time + position + valid ephemeris)  1-5 s
 * Ephemeris is only ~4 hours valid and needs receiver backup power to survive
 * a power cycle, which we assume we do not have — so warm is the honest target
 * and what this implements. It is still the difference between "start it and
 * wait" and "start it and go".
 *
 * AID_SAVE_MS: how often the position on SD is refreshed once a fix is held.
 * Frequent enough that the stored position is never far from where the vehicle
 * actually is, rare enough that the card is not being rewritten constantly.
 * AID_MAX_AGE_S: refuse to use a stored position older than this. A position
 * from last month is not wrong in a useful way — it is a different place, and
 * a receiver aided with the wrong place searches the wrong Dopplers.
 * AID_MAX_HDOP: only save a position we actually trust. */
#define AID_FILE             "GPSAID.TXT"
#define AID_SAVE_MS          (60UL * 1000UL)
#define AID_MAX_AGE_S        (14UL * 24UL * 3600UL)   /* two weeks */
#define AID_MAX_HDOP         5.0f
#define AID_MIN_SATS         4

/* ---------------------------------------------------------------------------
 * 9. DATA LOGGING  (guideline §5.1.xi — record to SD against telemetry loss)
 * ------------------------------------------------------------------------ */
#define LOG_RECORD_BYTES     128       /* exactly 4 records per 512 B block */
#define LOG_BLOCK_BYTES      512
#define LOG_RECS_PER_BLOCK   (LOG_BLOCK_BYTES / LOG_RECORD_BYTES)
#define LOG_RATE_HZ          20
#define LOG_SYNC_MS          2000UL
#define LOG_PREALLOC_BYTES   (32UL * 1024UL * 1024UL)   /* ~3.6 h at 20 Hz */
/* CSV MIRROR NOW RUNS AT THE FULL SAMPLE RATE, AND CARRIES EVERY SCHEMA FIELD.
 *
 * Was 1 Hz and 32 columns. Now 20 Hz and the complete field set — including
 * sensors that are not fitted on the prototype, which are written as "-" so
 * the column layout never changes between builds and a not-fitted sensor stays
 * distinguishable from one that genuinely read zero.
 *
 * COST, since this is a 20x increase: ~700 B per line at 20 Hz is ~14 kB/s,
 * about 50 MB per hour. On the fitted 32 GB card that is 0.16% per hour, so
 * capacity is a non-issue — which is why this is written as plain text rather
 * than compressed. The binary log remains the primary record: it is CRC-16
 * protected per record and block-aligned, so it survives a torn write that
 * would cost this file the rest of a line. */
#define LOG_CSV_RATE_HZ      20
#define SD_CAPACITY_BYTES    (32ULL * 1024ULL * 1024ULL * 1024ULL)

/* ---------------------------------------------------------------------------
 * 10. SYSTEM
 * ------------------------------------------------------------------------ */
#define WDT_TIMEOUT_S        6.0f
#define EEPROM_ADDR          0
/* 'VRI' + layout number. BUMP THIS whenever struct Persist changes shape.
 * Layout 8 adds padTempC (the T0 of the hypsometric formula, which has to be
 * restored alongside padPresPa or the temperature compensation is lost across a
 * reset). An old layout-7 record now fails the magic check and is treated as a
 * fresh mission, which is the safe outcome: EEPROM.get() into a struct that
 * grew by four bytes would otherwise read a garbage float straight into the
 * altitude reference. */
#define EEPROM_MAGIC         0x56524908UL   /* 'VRI' + layout 8 */
/* Guideline §6.1.ix: "In case of processor reset, the mission clock/time stamp
 * & system state shall be maintained." Mission elapsed time is therefore
 * checkpointed to EEPROM and resumed on boot, not restarted from millis(). */
#define MISSION_CLOCK_SAVE_MS  5000UL

/* MANDATORY RF SAFETY TIMER, from the frequency allocation plan:
 * "All CAN-7USATs should automatically switch off transmission 90 min after
 * launch detection, to avoid interference with other teams."
 * The clock starts at the FS_BOOT -> FS_LAUNCH transition, is persisted with
 * the rest of the mission state so a reset cannot restart it, and shuts the
 * downlink off unconditionally when it expires. The receiver stays armed, so
 * Mission Control can still command the vehicle after the cut-off. */
#define TX_AUTO_OFF_MS       (90UL * 60UL * 1000UL)

/* BACKUP-FREQUENCY DEAD-MAN TIMER — the safety net CMD_FAILOVER was missing.
 *
 * THE TRAP. CMD_FAILOVER moves the vehicle from 867.00 to 865.75 MHz. The
 * ground station does NOT follow automatically; the operator has to type
 * FAILOVER_LOCAL on the gateway to move with it. If that second step is
 * forgotten, mistyped, or the gateway reboots before it happens, the two ends
 * are on different channels — and every command that could fix it, including
 * CMD_FAILOVER itself, has to travel over the link that no longer exists. The
 * vehicle is then unreachable for the rest of the flight with nothing wrong
 * with either radio. A one-way command with no return path is a design fault,
 * not an operator-training problem.
 *
 * THE FIX. After switching, the vehicle requires proof that the ground station
 * arrived too: it must hear at least one valid uplink on the backup channel
 * within FAILOVER_PROOF_MS. If it does not, it reverts to the main frequency on
 * its own. The operator loses nothing — a real failover is confirmed by any
 * subsequent command — and a mistaken or half-completed one self-heals.
 *
 * 120 s is comfortably longer than the ~3 s worst-case command latency in
 * flight plus a full gateway reboot and re-init, so a working failover is never
 * undone by it. Both channels are ours under allocation row 025, so nothing
 * here can transmit outside the assignment. */
#define FAILOVER_PROOF_MS    (120UL * 1000UL)

#define SENSOR_SERVICE_HZ    50
#define GYRO_WHEEL_PCT       70        /* UNVERIFIED — component test only, no
                                          angular-momentum authority calc */
