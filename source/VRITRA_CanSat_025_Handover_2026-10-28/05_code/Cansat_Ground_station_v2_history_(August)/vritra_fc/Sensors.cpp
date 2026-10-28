/* =============================================================================
 * Sensors.cpp
 * =============================================================================
 * WHY THERE IS NO VENDOR LIBRARY IN HERE
 *   The previous firmware called 7Semi_BMP585::beginI2C() and the barometer
 *   silently stopped producing data — which is exactly the "ALT_M and PRES_PA
 *   render as em-dash on the GCS" symptom. Three separate faults live in that
 *   path and all three are removed here:
 *     1. the library's initSensor() leaves the on-chip IIR at coefficient 127
 *        (~12 s time constant), so altitude looks frozen;
 *     2. it calls Wire.begin() again internally, re-entering a bus this code
 *        has already configured and recovered;
 *     3. readTemperatureC() and readPressurePa() each issue their own burst,
 *        unsynchronised to the conversion cycle. A burst straddling a register
 *        update returns a torn 24-bit word — one corrupted MSB is ~1024 Pa,
 *        about 90 m of apparent altitude. Those are the -300 m spikes in the
 *        early flight logs.
 *   Every register below is written explicitly, named against its datasheet
 *   field, and the result is validated by reading back a physically plausible
 *   value before the device is accepted.
 *
 * ON delay()
 *   There is exactly one blocking construct in this file: nine
 *   delayMicroseconds(5) pairs inside busRecover(), 90 us total, which is the
 *   I2C bus-unwedge sequence and cannot be expressed as a millis() state
 *   machine because it must produce contiguous clock edges. Every other timing
 *   requirement — chip settle times, config pacing, sample cadence — runs off
 *   millis() in the staged init machine below. delay() appears nowhere.
 * ========================================================================== */
#include "Sensors.h"
#include <Wire.h>

SensorHub Sensors;

/* ---- BMP585 registers ---------------------------------------------------- */
#define BMP_CHIP_ID_REG   0x01
#define BMP_TEMP_XLSB     0x1D      /* T[3] then P[3] in one 6-byte burst     */
#define BMP_DSP_IIR       0x31
#define BMP_OSR_CONFIG    0x36
#define BMP_ODR_CONFIG    0x37
#define BMP_CMD           0x7E
/* DSP_CONFIG (0x30) IS NEVER WRITTEN. It carries the pressure/temperature
 * compensation enables, not a filter setting. Zeroing it takes the output out
 * of the physical pressure range while every register write still ACKs. */
#define BMP_OSR_BYTE      (0x40 | (0x04 << 3) | 0x01)   /* press_en, P x16, T x2 */
#define BMP_ODR_CODE      0x14                          /* 25.005 Hz            */
#define BMP_ODR_STANDBY   (0x80 | (BMP_ODR_CODE << 2) | 0x00)
#define BMP_ODR_NORMAL    (0x80 | (BMP_ODR_CODE << 2) | 0x01)

/* ---- ICM-20948 registers ------------------------------------------------- */
#define ICM_BANK_SEL      0x7F
#define ICM_WHO_AM_I      0x00
#define ICM_USER_CTRL     0x03
#define ICM_LP_CONFIG     0x05
#define ICM_PWR_MGMT_1    0x06
#define ICM_PWR_MGMT_2    0x07
#define ICM_INT_PIN_CFG   0x0F
#define ICM_ACCEL_XOUT_H  0x2D
#define ICM_GYRO_XOUT_H   0x33
#define ICM_B2_GYRO_SMPLRT 0x00
#define ICM_B2_GYRO_CFG1   0x01
#define ICM_B2_ACC_SMPLRT1 0x10
#define ICM_B2_ACC_SMPLRT2 0x11
#define ICM_B2_ACCEL_CFG   0x14

#define ACC_LSB_PER_G     (16384.0f / 8.0f)    /* FS_SEL=3 -> +-16 g          */
#define GYR_LSB_PER_DPS   (32768.0f / 2000.0f) /* FS_SEL=3 -> +-2000 dps      */

/* ---- AK09916 registers (own die, own address, behind ICM bypass) --------- */
#define AK_WIA2_REG       0x01      /* expected value AK_WIA2 (0x09)          */
#define AK_ST1            0x10      /* ST1..ST2 is a 9-byte burst             */
#define AK_CNTL2          0x31
#define AK_CNTL3          0x32
#define AK_UT_PER_LSB     0.15f

/* ---- DS3231 -------------------------------------------------------------- */
#define RTC_SECONDS       0x00
#define RTC_STATUS        0x0F
#define RTC_TEMP_MSB      0x11

/* Calibration gating constants moved to Config.h — CAL_ARM_MS, CAL_STILL_MS,
 * CAL_STILL_DPS, CAL_TIMEOUT_MS, BARO_CAL_MS, BARO_CAL_MIN_N. CAL_STILL_DPS
 * was 3.0 here, below this part's own measured 4.3-11.2 dps noise floor, so
 * the stillness gate could never be satisfied and every CMD_CALIBRATE timed
 * out in silence. Full write-up in Config.h. */

/* ============================================================================
 * I2C PRIMITIVES
 * ========================================================================= */
void SensorHub::noteFault() {
    d_.i2cFaults++;
    if (++faultRun_ >= I2C_FAULTS_TO_RECOVER) {
        faultRun_ = 0;
        d_.i2cRecovers++;
        /* Software bus reset. Wire.end() releases the peripheral, the manual
         * clock pulses free a slave that is holding SDA low, then the bus is
         * brought back up. This is the recovery path for the electrical-noise
         * lockups this harness exhibits. */
        Wire.end();
        busRecover();
        Wire.begin();
        Wire.setClock(I2C_CLOCK_HZ);
    }
}
void SensorHub::noteOk() { faultRun_ = 0; }

bool SensorHub::wr8(uint8_t addr, uint8_t reg, uint8_t val) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(val);
    if (Wire.endTransmission() != 0) { noteFault(); return false; }
    noteOk();
    return true;
}

bool SensorHub::rdN(uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t len) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) { noteFault(); return false; }
    if (Wire.requestFrom(addr, len) != len)  { noteFault(); return false; }
    for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
    noteOk();
    return true;
}

bool SensorHub::rd8(uint8_t addr, uint8_t reg, uint8_t& val) {
    return rdN(addr, reg, &val, 1);
}

/* ============================================================================
 * ALTITUDE ABOVE THE PAD  —  temperature-compensated hypsometric formula
 * ============================================================================
 * h = (T0 / L) * (1 - (p / p0) ^ (R*L / (g*M)))
 *
 * with L = 0.0065 K/m and the exponent R*L/(g*M) = 0.1902632. The familiar
 * "44330.8" is not a universal constant — it is T0/L evaluated at the ISA
 * sea-level temperature of 288.15 K, and it is only right when the air at the
 * reference level is at 15 C.
 *
 * WHY THAT MATTERED HERE. The logged pad temperature is 28.9 C = 302.05 K, so
 * the true scale height is 302.05 / 0.0065 = 46469 m, 4.8% above the ISA value.
 * Every altitude was therefore reported 4.8% LOW: about 48 m at the 1000 m
 * apogee this mission is graded on, and about 29 m at the 600 m secondary
 * deployment trigger — which guideline §5.4.vi allows only +-10 m on. One
 * hard-coded constant was putting a graded event three times outside its
 * tolerance, in the same direction, every flight.
 *
 * T0 comes from padTempC, captured at the same moment as padPresPa. If it is
 * missing or implausible — a wedged I2C read, or the BMP585 die still
 * self-heating right after power-up — ISA is used instead. Falling back to a
 * known-imperfect constant is correct; propagating a nonsense temperature into
 * every altitude in the flight is not.
 *
 * The exponent is 0.1902632, not the 0.190295 used before. That is a small
 * correction (~0.02% of the altitude) but it is free and it is the right
 * number: R*L/(g*M) = 8.31432*0.0065 / (9.80665*0.0289644). */
float SensorHub::altitudeAgl(float presPa) const {
    const float p0 = d_.padPresPa;
    /* No survey yet, or a corrupt one — report 0 rather than an altitude
     * computed against a reference that does not mean anything. */
    if (p0 < BARO_MIN_PA || p0 > BARO_MAX_PA) return 0.0f;
    if (presPa < BARO_MIN_PA || presPa > BARO_MAX_PA) return 0.0f;

    const bool t0Ok = (d_.padTempC > BARO_T0_MIN_C && d_.padTempC < BARO_T0_MAX_C);
    const float t0K = t0Ok ? (d_.padTempC + 273.15f) : BARO_ISA_T0_K;

    return (t0K / BARO_LAPSE_K_M) * (1.0f - powf(presPa / p0, 0.1902632f));
}

/* ============================================================================
 * BAROMETRIC PAD SURVEY — non-blocking, runs on the 25 Hz barometer tick
 * ============================================================================
 * Accumulates the CONDITIONED pressure (median-of-5, glitch-gated) rather than
 * raw register reads, so a torn I2C burst — worth ~1024 Pa, about 90 m — is
 * deleted before it can reach the reference instead of being averaged into it
 * at 1/16 weight. Runs for BARO_CAL_MS on the sensor's own cadence, so every
 * sample is an independent conversion; the old 20 ms polling loop against a
 * 40 ms ODR was averaging each conversion roughly twice and getting none of the
 * sqrt(N) noise reduction it appeared to. */
void SensorHub::baroCalService(uint32_t now) {
    if (!baroCalActive_) return;

    if (now - baroCalStartMs_ < BARO_CAL_MS) return;

    baroCalActive_ = false;
    if (baroCalN_ < BARO_CAL_MIN_N) {
        Serial.printf("// [CAL] baro ABANDONED — only %lu samples in %lu ms "
                      "(need %d). Pad reference UNCHANGED at %.1f Pa.\n",
                      (unsigned long)baroCalN_, (unsigned long)BARO_CAL_MS,
                      BARO_CAL_MIN_N, d_.padPresPa);
        return;
    }

    const float pa = (float)(baroCalPSum_ / (double)baroCalN_);
    const float tc = (float)(baroCalTSum_ / (double)baroCalN_);
    const float was = d_.padPresPa;

    /* Range-check here rather than inferring it from whether setPadReference()
     * changed anything — the caller should not have to reverse-engineer the
     * setter's validation by comparing floats for equality. */
    if (pa <= BARO_MIN_PA || pa >= BARO_MAX_PA) {
        Serial.printf("// [CAL] baro REJECTED %.1f Pa — outside %.0f..%.0f Pa. "
                      "Pad reference UNCHANGED at %.1f Pa.\n",
                      pa, (double)BARO_MIN_PA, (double)BARO_MAX_PA, was);
        return;
    }
    setPadReference(pa, tc);

    /* Zeroing the reference invalidates the vertical-speed differentiator's
     * previous sample — without this the first post-calibration vspeed is the
     * entire altitude step divided by one sample interval, which on the old
     * code showed up as a spurious hundreds-of-m/s spike. */
    lastAltMs_ = 0;
    d_.vspeedMs = 0;

    Serial.printf("// [CAL] baro pad reference = %.1f Pa @ %.1f C "
                  "(%lu samples, was %.1f Pa) -> AGL now reads 0.0 m here\n",
                  pa, tc, (unsigned long)baroCalN_, was);
}

/* Nine SCL pulses with SDA released, run BEFORE Wire.begin() on every boot and
 * on every recovery. All sensors share one physical bus: if any prior session
 * — including a different sketch, or a brownout mid-transaction — left a slave
 * holding SDA low, the next boot fails on its very first transaction even
 * though the wiring is perfect. Software isolation does not isolate a wire. */
void SensorHub::busRecover() {
    pinMode(PIN_SDA, INPUT_PULLUP);
    pinMode(PIN_SCL, OUTPUT);
    for (int i = 0; i < 9; i++) {
        digitalWrite(PIN_SCL, HIGH); delayMicroseconds(5);
        digitalWrite(PIN_SCL, LOW);  delayMicroseconds(5);
    }
    digitalWrite(PIN_SCL, HIGH);
    pinMode(PIN_SCL, INPUT);
}

bool SensorHub::icmBank(uint8_t bank) {
    return wr8(icmAddr_, ICM_BANK_SEL, (uint8_t)((bank & 3) << 4));
}

/* ============================================================================
 * DEVICE READS
 * ========================================================================= */
static inline int32_t signExtend24(uint32_t x) {
    x &= 0xFFFFFFu;
    return (x & 0x800000u) ? (int32_t)(x | 0xFF000000u) : (int32_t)x;
}

bool SensorHub::bmpRead(float& tC, float& pPa) {
    uint8_t b[6];
    if (!rdN(bmpAddr_, BMP_TEMP_XLSB, b, 6)) return false;
    uint32_t tu = ((uint32_t)b[2] << 16) | ((uint32_t)b[1] << 8) | b[0];
    uint32_t pu = ((uint32_t)b[5] << 16) | ((uint32_t)b[4] << 8) | b[3];
    tC  = (float)signExtend24(tu) / 65536.0f;
    pPa = (float)signExtend24(pu) / 64.0f;
    return true;
}

bool SensorHub::icmRead() {
    uint8_t b[12];
    if (!icmBank(0)) return false;
    /* ACCEL_XOUT_H(0x2D) .. GYRO_ZOUT_L(0x38) is contiguous — one 12-byte
     * burst keeps accel and gyro on the same sample instant. */
    if (!rdN(icmAddr_, ICM_ACCEL_XOUT_H, b, 12)) return false;

    float rax = (int16_t)((b[0] << 8) | b[1]) / ACC_LSB_PER_G;
    float ray = (int16_t)((b[2] << 8) | b[3]) / ACC_LSB_PER_G;
    float raz = (int16_t)((b[4] << 8) | b[5]) / ACC_LSB_PER_G;
    float rgx = (int16_t)((b[6] << 8) | b[7]) / GYR_LSB_PER_DPS;
    float rgy = (int16_t)((b[8] << 8) | b[9]) / GYR_LSB_PER_DPS;
    float rgz = (int16_t)((b[10] << 8) | b[11]) / GYR_LSB_PER_DPS;

    d_.ax = rax * accScale_;
    d_.ay = ray * accScale_;
    d_.az = raz * accScale_;
    d_.accMagG = sqrtf(d_.ax * d_.ax + d_.ay * d_.ay + d_.az * d_.az);

    float cgx = rgx - gBias_[0];
    float cgy = rgy - gBias_[1];
    float cgz = rgz - gBias_[2];
    /* RAW GYRO GOES OUT RAW. No deadband here — only bias correction.
     *
     * The deadband used to be applied to these three telemetry axes, and at
     * 12 dps it was swallowing almost everything a human can do to the vehicle
     * by hand: a deliberate demonstration rotation is roughly 10-40 dps, so
     * the link carried a hard 0.0 and the ground station — correctly —
     * reported GX/GY/GZ as FROZEN and the attitude display as STALE. The IMU
     * was working the whole time; the filter was erasing it before it reached
     * the packet.
     *
     * The reason the deadband exists is real, but it belongs downstream: noise
     * leaking into SPIN_RATE is what corrupted the earlier flight logs, and
     * SPIN_RATE is what the flight state machine acts on. A false non-zero
     * there can trigger a state transition. A false non-zero in raw telemetry
     * costs nothing — the ground station can filter for display, and the
     * unfiltered stream is what you actually want on the SD card for the
     * post-flight review.
     *
     * Rule: filter where a wrong value causes a wrong DECISION, never where it
     * only causes a noisy DISPLAY. */
    d_.gx = cgx;
    d_.gy = cgy;
    d_.gz = cgz;

    /* SPIN_RATE keeps the deadband — this is the FSM's input. */
    d_.spinDps = (fabsf(cgz) < GYRO_DEADBAND_DPS) ? 0.0f : fabsf(cgz);
    /* ---- SIMPLE ATTITUDE — accel tilt + gyro yaw ------------------------
     * See ATTITUDE ESTIMATOR block in Config.h for why THIS ESTIMATOR — the
     * one feeding the numeric panels and the SD log — is not Mahony, not a
     * quaternion, and not clever. Short version: for this vehicle on a bench
     * and for 99% of a normal flight, atan2 on the accelerometer is exact and
     * cannot drift; a fancier filter added yaw drift and sign-ambiguity
     * without paying for either.
     *
     * A SEPARATE quaternion filter runs below (quatUpdate()) purely to feed
     * the ground station's 3-D model, which — unlike a numeric panel — needs
     * an orientation representation with no +-90 deg pitch ceiling to show a
     * real tumble. It does not replace this estimator; see "QUATERNION
     * ATTITUDE" further down in this file for why the bench failures that
     * got the old Mahony filter removed do not apply to that one.
     *
     *   roll  = atan2( -ax, az )                      -- rotates about Y_sensor
     *   pitch = atan2(  ay, sqrt(ax^2 + az^2) )       -- rotates about X_sensor
     *   yaw   = integral of gz                        -- rotates about Z_sensor
     *
     * THE X AND Y AXES ARE SWAPPED RELATIVE TO THE OBVIOUS FORM, AND THAT IS
     * MEASURED, NOT GUESSED. Six-position static test, 2026-08-06, plateau
     * means from sensor_tests/test_imu_axis_calibration (full data in
     * docs/IMU_AXIS_CALIBRATION_2026-08-06.md):
     *
     *   pose        ax      ay      az     dominant axis
     *   LEVEL     -0.029  -0.052  +0.997   +Z
     *   INVERTED  -0.029  -0.064  -1.047   -Z
     *   LEFT 90   +1.005  -0.161  +0.029   +X   <- rolling moves X, not Y
     *   RIGHT 90  -1.002  -0.216  -0.019   -X
     *   FRONT 90  -0.192  -1.012  -0.021   -Y   <- pitching moves Y, not X
     *   BACK 90   +0.135  +0.984  -0.028   +Y
     *
     * So the board is mounted rotated 90 deg about its own Z:
     *   sensor +X points along the vehicle's RIGHT
     *   sensor +Y points along the vehicle's FRONT
     *   sensor +Z points UP
     * which is a proper rotation (determinant +1 against the right-handed
     * (forward, right, down) body frame), not a reflection.
     *
     * This is why months of flipping ATT_SIGN_* never converged: a swap is not
     * a negation, so no combination of three signs can express it. Fixing roll
     * always broke pitch. Verified against the measured plateaus above:
     *   RIGHT 90 -> roll +91.1   LEFT 90 -> roll -88.4   LEVEL -> roll +1.7
     *   BACK 90  -> pitch +82.0  FRONT 90 -> pitch -79.2  LEVEL -> pitch -3.0
     * (the residual few degrees is real mounting tilt, removed by the datum.)
     *
     * DATUM: after ATT_ZERO_DELAY_MS the current accel tilt is captured as
     * the reference and yaw is reset to zero. Everything published after that
     * is movement RELATIVE to that pose. CMD_CALIBRATE re-captures the datum,
     * so the operator can zero the model in the vehicle's real launch pose.
     *
     * ACCEL GATE: when |a| is well outside 1 g the accelerometer is measuring
     * thrust or shock, not gravity, so roll and pitch temporarily lose their
     * meaning. We publish the raw atan2 anyway — an honest wobble during
     * acceleration is more useful than a filtered value hiding the event. */
    const uint32_t nowUs = micros();
    float dt = (attLastUs_ == 0) ? 0.0f : (nowUs - attLastUs_) * 1e-6f;
    attLastUs_ = nowUs;
    if (dt > 0.0f && dt < 0.5f) {
        const float ax = d_.ax, ay = d_.ay, az = d_.az;

        /* Raw tilt from accelerometer. In radians -> degrees.
         * ax and ay are exchanged versus the textbook form because the board
         * is mounted rotated 90 deg about Z — see the measured six-position
         * table in the block comment above. Do not "simplify" this back. */
        const float rollRaw  = atan2f(-ax, az) * 57.29577951f;
        const float pitchRaw = atan2f(ay, sqrtf(ax * ax + az * az))
                               * 57.29577951f;

        /* Integrate gyro Z into yaw. cgz is already bias-corrected. */
        attYaw_ += cgz * dt;
        while (attYaw_ >= 360.0f) attYaw_ -= 360.0f;
        while (attYaw_ <   0.0f)  attYaw_ += 360.0f;

        /* QUATERNION ATTITUDE (3-D model feed only) — advanced every pass, on
         * the SAME accel/gyro sample, so it can never fall out of step with
         * the atan2 estimator above. See "QUATERNION ATTITUDE" below for what
         * this is for and why it does not replace rollDeg/pitchDeg/yawDeg.
         *
         * FED IN VEHICLE AXES, NOT SENSOR AXES. The same 90 deg mounting
         * rotation documented above applies here, but a quaternion filter
         * cannot absorb it the way a pair of ATT_SIGN_* constants might: the
         * datum subtraction conj(qRef)*qFree leaves the rotation ANGLE right
         * while rotating its AXIS by the mounting offset, so the model would
         * tilt correctly in magnitude but about the wrong axis.
         *
         * The remap below is (forward, right, down) = (Y, X, -Z), which is a
         * PROPER rotation — determinant +1. The naive (Y, X, +Z) is a
         * reflection, determinant -1, and would make the filter integrate
         * rotations backwards about the vertical. Both accel and gyro must be
         * remapped identically or the accel correction fights the gyro. */
        quatUpdate( ay,  ax, -az,
                   cgy, cgx, -cgz, dt);

        /* Datum capture. Wait for the accel to settle onto gravity before
         * locking the reference in — one first-pass sample is too noisy to
         * anchor "0 forever" against. The delay is ATT_ZERO_DELAY_MS in
         * Config.h: long enough for the filter to converge on a bench, short
         * enough that the operator sees "0,0,0" almost immediately at boot.
         * This used to be a hard-coded 2000UL that silently ignored the
         * Config.h value — the two had drifted apart. */
        const uint32_t nowMs = millis();
        if (attFirstMs_ == 0) attFirstMs_ = nowMs;

        /* TWO DATUM PATHS, ONE CAPTURE. The settle delay exists to stop a
         * single noisy first-pass sample being frozen in as "0 forever" at
         * boot. It is NOT a correctness requirement of the capture itself, and
         * applying it to a commanded re-zero is what made CMD_CALIBRATE look
         * like it was being ignored: the operator put the vehicle where they
         * wanted it, sent the command, and the display did nothing for three
         * full seconds. By then they had usually moved it again.
         *
         * On CMD_CALIBRATE the filter has already been converged for as long as
         * the vehicle has been powered, so there is nothing to settle and the
         * datum is taken on this very sample. See Config.h ATTITUDE DATUM. */
        const bool settled  = (nowMs - attFirstMs_) >= ATT_ZERO_DELAY_MS;
        const bool takeNow  = attZeroNowReq_ || settled;

        if (!attZeroed_ && takeNow) {
            attRefRoll_  = rollRaw;
            attRefPitch_ = pitchRaw;
            attYaw_      = 0.0f;
            attZeroed_   = true;
            /* Datum for the quaternion path: whatever pose the filter has
             * converged to at the SAME instant the Euler datum locks, so
             * both paths agree on what "0,0,0 / identity" means. */
            qRefW_ = qFreeW_; qRefX_ = qFreeX_; qRefY_ = qFreeY_; qRefZ_ = qFreeZ_;
            Serial.printf("// [ATT] datum %s: R%.1f P%.1f -> 0,0,0 "
                          "(this pose is now HOME)\n",
                          attZeroNowReq_ ? "CAPTURED ON COMMAND" : "captured at boot",
                          rollRaw, pitchRaw);
            attZeroNowReq_ = false;
        }

        /* Publish relative to the datum, with the mounting signs applied last.
         * Signs come last so a flipped axis can never subtract its reference
         * in the wrong direction. */
        float rOut = ATT_SIGN_ROLL  * (rollRaw  - attRefRoll_);
        float pOut = ATT_SIGN_PITCH * (pitchRaw - attRefPitch_);
        float yOut = ATT_SIGN_YAW   * attYaw_;
        /* Wrap yaw in case sign flipped it negative. */
        while (yOut <   0.0f) yOut += 360.0f;
        while (yOut >= 360.0f) yOut -= 360.0f;

        d_.rollDeg  = rOut;
        d_.pitchDeg = pOut;
        d_.yawDeg   = yOut;

        /* Datum-relative quaternion = conjugate(qRef) * qFree. Unlike Euler
         * subtraction this composes correctly through any orientation,
         * including past the +-90 deg pitch the atan2 path above cannot
         * represent. */
        const float qcw =  qRefW_, qcx = -qRefX_, qcy = -qRefY_, qcz = -qRefZ_;
        float ow = qcw*qFreeW_ - qcx*qFreeX_ - qcy*qFreeY_ - qcz*qFreeZ_;
        float ox = qcw*qFreeX_ + qcx*qFreeW_ + qcy*qFreeZ_ - qcz*qFreeY_;
        float oy = qcw*qFreeY_ - qcx*qFreeZ_ + qcy*qFreeW_ + qcz*qFreeX_;
        float oz = qcw*qFreeZ_ + qcx*qFreeY_ - qcy*qFreeX_ + qcz*qFreeW_;
        /* Ship the half with W >= 0 — q and -q are the same rotation, and
         * this is what lets the wire drop W entirely (see B36_QUAT). */
        if (ow < 0.0f) { ow = -ow; ox = -ox; oy = -oy; oz = -oz; }
        d_.qw = ow; d_.qx = ox; d_.qy = oy; d_.qz = oz;
        d_.attQuatOk = attZeroed_;
    }

    /* Raw (pre-deadband) gyro feeds the stillness gate — the deadband would
     * otherwise make every reading look perfectly still. */
    if (calState_ != CAL_IDLE) {
        float v[3] = { rgx, rgy, rgz };
        for (int i = 0; i < 3; i++) {
            if (v[i] < calGMin_[i]) calGMin_[i] = v[i];
            if (v[i] > calGMax_[i]) calGMax_[i] = v[i];
        }
        if (calState_ == CAL_ACCUM) {
            calGSum_[0] += rgx; calGSum_[1] += rgy; calGSum_[2] += rgz;
            calN_++;
        }
    }
    return true;
}

/* ============================================================================
 * QUATERNION ATTITUDE  (3-D MODEL FEED ONLY — see Config.h ATTITUDE ESTIMATOR)
 * ============================================================================
 * Config.h documents three real bench failures that got a Mahony quaternion
 * filter REMOVED from this project: unbounded yaw drift with no magnetometer
 * reference, sign ambiguity (q and -q are the same rotation) making a raw
 * numeric quaternion readout jump with no real motion, and paying for
 * gimbal-lock immunity nothing on this vehicle needed.
 *
 * None of those apply here, because this filter's ONLY consumer is the
 * ground station's 3-D render, never a numeric panel:
 *   - yaw still drifts, identically to attYaw_ above — no worse than today,
 *     because this filter also has no magnetometer reference and uses no
 *     integral (Ki) feedback that could otherwise let a biased correction
 *     accumulate silently.
 *   - the sign ambiguity is invisible to the 3-D model: rotating a point by
 *     q or by -q gives the identical screen position (every term in
 *     CanSatView._project's quaternion path is bilinear in q), so it only
 *     ever mattered for a text readout, which this quaternion never feeds —
 *     ATT_ROLL/PITCH/YAW keep doing that job, unchanged.
 *   - the one property this buys — no +-90 deg pitch ceiling — is now
 *     exactly what was asked for: a full 360 deg tumble on the model.
 *
 * Standard Mahony 6-DOF (accel+gyro, no magnetometer) attitude filter,
 * proportional-only. ax/ay/az: any consistent unit, normalised inside.
 * gxDps/gyDps/gzDps: deg/s, already bias-corrected. dt: seconds. */
#define MAHONY_KP   2.0f    /* proportional gain, accel -> gyro correction */

void SensorHub::quatUpdate(float ax, float ay, float az,
                           float gxDps, float gyDps, float gzDps, float dt) {
    float gx = gxDps * 0.017453293f;    /* deg/s -> rad/s */
    float gy = gyDps * 0.017453293f;
    float gz = gzDps * 0.017453293f;

    const float norm = sqrtf(ax * ax + ay * ay + az * az);
    if (norm > 1e-3f) {
        ax /= norm; ay /= norm; az /= norm;

        /* Gravity direction implied by the current quaternion estimate —
         * canonical Mahony/Madgwick reference form. */
        const float vx = 2.0f * (qFreeX_ * qFreeZ_ - qFreeW_ * qFreeY_);
        const float vy = 2.0f * (qFreeW_ * qFreeX_ + qFreeY_ * qFreeZ_);
        const float vz = qFreeW_ * qFreeW_ - qFreeX_ * qFreeX_
                        - qFreeY_ * qFreeY_ + qFreeZ_ * qFreeZ_;

        /* Error = cross(measured, estimated): a small-angle correction
         * torque that nudges the gyro integration back onto gravity. */
        gx += MAHONY_KP * (ay * vz - az * vy);
        gy += MAHONY_KP * (az * vx - ax * vz);
        gz += MAHONY_KP * (ax * vy - ay * vx);
    }
    /* else: accelerometer is not a usable gravity reference this sample
     * (thrust/shock) — integrate gyro alone, same gate philosophy as
     * ATT_GATE_LO_G/HI_G above: an honest uncorrected estimate for one
     * sample beats a filtered fiction. */

    const float qDotW = 0.5f * (-qFreeX_*gx - qFreeY_*gy - qFreeZ_*gz);
    const float qDotX = 0.5f * ( qFreeW_*gx + qFreeY_*gz - qFreeZ_*gy);
    const float qDotY = 0.5f * ( qFreeW_*gy - qFreeX_*gz + qFreeZ_*gx);
    const float qDotZ = 0.5f * ( qFreeW_*gz + qFreeX_*gy - qFreeY_*gx);

    qFreeW_ += qDotW * dt;
    qFreeX_ += qDotX * dt;
    qFreeY_ += qDotY * dt;
    qFreeZ_ += qDotZ * dt;

    const float n = sqrtf(qFreeW_*qFreeW_ + qFreeX_*qFreeX_
                         + qFreeY_*qFreeY_ + qFreeZ_*qFreeZ_);
    if (n > 1e-9f) {
        qFreeW_ /= n; qFreeX_ /= n; qFreeY_ /= n; qFreeZ_ /= n;
    }
}

bool SensorHub::magRead() {
    uint8_t b[9];
    /* ST1 .. ST2 in ONE burst. Reading the data registers without also reading
     * ST2 (0x18) leaves the AK09916's measurement latch closed and the data
     * registers simply stop updating. */
    if (!rdN(AK_ADDR, AK_ST1, b, 9)) return false;
    if (!(b[0] & 0x01)) return true;              /* DRDY clear — not new yet */
    if (b[8] & 0x08)    return true;              /* HOFL — magnetic overflow */

    /* ---- AK09916 -> ICM FRAME TRANSFORM: NEGATE Y AND Z --------------------
     * THE ICM-20948 IS A MULTI-CHIP MODULE. Inside the 24-pin QFN there are
     * two physically separate dies — the InvenSense accel/gyro and an Asahi
     * Kasei AK09916 magnetometer — bonded independently to the same substrate.
     * Their coordinate frames are NOT natively aligned, and the datasheet
     * (TDK DS-000189) draws them in two different figures for exactly that
     * reason. Relative to the same Pin 1 reference:
     *     magnetometer +X points right   -> aligned with accel +X
     *     magnetometer +Y points down    -> INVERTED vs accel +Y
     *     magnetometer +Z points into the package -> INVERTED vs accel +Z
     *
     * So the correct transform is a negation of Y and Z. Note this is NOT the
     * X/Y swap that the older MPU-9250 required — carrying that habit over is
     * a common way to get this wrong twice.
     *
     * WHY IT MATTERS EVEN THOUGH IT LOOKS HARMLESS. Negating two axes
     * preserves right-handedness, so the magnetometer frame is not broken in
     * isolation — it is simply rotated 180 deg about X relative to the inertial
     * sensors. But a tilt-compensated heading takes roll and pitch from the
     * correctly-oriented accelerometer and projects the magnetic vector
     * through them. With Y inverted, that projection comes out mirrored, and a
     * physical clockwise turn produces an anti-clockwise computed yaw. This is
     * a documented and frequent cause of mirrored 3-D ground station displays
     * in hand-written ICM-20948 drivers.
     *
     * Applied at the point of conversion so that everything downstream — the
     * hard-iron calibrator, the heading solution, the SD log — sees one
     * consistent frame. */
    float rmx =  (int16_t)((b[2] << 8) | b[1]) * AK_UT_PER_LSB;
    float rmy = -(int16_t)((b[4] << 8) | b[3]) * AK_UT_PER_LSB;
    float rmz = -(int16_t)((b[6] << 8) | b[5]) * AK_UT_PER_LSB;

    if (magCalActive_) {
        float v[3] = { rmx, rmy, rmz };
        for (int i = 0; i < 3; i++) {
            if (v[i] < magLo_[i]) magLo_[i] = v[i];
            if (v[i] > magHi_[i]) magHi_[i] = v[i];
        }
        magCalN_++;
    }
    d_.mx = rmx - mBias_[0];
    d_.my = rmy - mBias_[1];
    d_.mz = rmz - mBias_[2];
    return true;
}

static inline uint8_t bcd2bin(uint8_t v) { return (uint8_t)(v - 6 * (v >> 4)); }
static inline uint8_t bin2bcd(uint8_t v) { return (uint8_t)(v + 6 * (v / 10)); }

/* ============================================================================
 * THE CLOCK  —  see the TIMEKEEPING block in Config.h
 * ============================================================================
 * The DS3231 is the only clock on this vehicle that survives a power cycle. It
 * runs off its own coin cell at ±2 ppm, so once it has been set from GNSS it
 * stays accurate to about a minute a year. That is what makes an aided restart
 * possible at all: the L89 is assumed to have no backup supply, so it loses its
 * clock AND its ephemeris every time the vehicle is switched off, and the only
 * way it can start a search with a known time is if we hand it one.
 *
 * Everything stored here is UTC. Local time is derived for display only. */
bool SensorHub::rtcSetUtc(uint16_t y, uint8_t mo, uint8_t d,
                          uint8_t h, uint8_t mi, uint8_t s) {
    if (y < RTC_MIN_YEAR || y > RTC_MAX_YEAR) return false;
    if (mo < 1 || mo > 12 || d < 1 || d > 31)  return false;
    if (h > 23 || mi > 59 || s > 59)           return false;

    /* Register 0x00..0x06 is one contiguous write: sec, min, hour, dow, day,
     * month, year. Writing them in a single transaction matters — the DS3231
     * latches the whole set together, so a per-register write can be caught by
     * a seconds rollover between two of them and produce a time that never
     * existed. Day-of-week is not used anywhere in this project and is
     * written as 1 rather than computed. */
    Wire.beginTransmission(RTC_ADDR);
    Wire.write((uint8_t)RTC_SECONDS);
    Wire.write(bin2bcd(s));
    Wire.write(bin2bcd(mi));
    Wire.write(bin2bcd(h));            /* bit 6 clear = 24-hour mode */
    Wire.write((uint8_t)1);
    Wire.write(bin2bcd(d));
    Wire.write(bin2bcd(mo));
    Wire.write(bin2bcd((uint8_t)(y - 2000)));
    if (Wire.endTransmission() != 0) { noteFault(); return false; }
    noteOk();

    /* Clear the oscillator-stop flag. It latches when the part loses all power
     * (dead cell, first power-up) and is the honest signal that the time it was
     * reporting up to now was meaningless. Leaving it set after deliberately
     * setting the clock would make every later boot distrust a good time. */
    uint8_t st = 0;
    if (rd8(RTC_ADDR, RTC_STATUS, st) && (st & 0x80))
        wr8(RTC_ADDR, RTC_STATUS, (uint8_t)(st & 0x7F));

    rtcRead();                          /* read back — never assume the write */
    return true;
}

/* Seed the clock from the compile-time stamp, but ONLY if what it currently
 * holds is not believable. A DS3231 with a live cell and a real time must never
 * be dragged backwards to whenever this firmware happened to be built.
 *
 * __DATE__/__TIME__ are the BUILD MACHINE'S LOCAL wall clock, so on a laptop
 * set to India they are IST and are converted to UTC here. Accuracy is minutes
 * (compile time, not upload time, not power-on time) — enough to make GNSS
 * aiding help rather than hurt, and replaced by real GNSS UTC on first fix. */
void SensorHub::seedRtcFromBuildTime() {
    if (rtcValid()) {
        Serial.printf("// [RTC] %04u-%02u-%02u %02u:%02u:%02u UTC — believable, "
                      "left alone\n",
                      d_.utcYear, d_.utcMon, d_.utcDay,
                      d_.utcHour, d_.utcMin, d_.utcSec);
        return;
    }

    /* Build stamp is local; subtract the zone offset to get UTC. Done on the
     * seconds-since-epoch value so a midnight or month boundary cannot be
     * mishandled by subtracting from the fields directly. */
    static const uint16_t cum[12] = {0,31,59,90,120,151,181,212,243,273,304,334};
    const uint16_t by = BUILD_YEAR, bmo = BUILD_MONTH, bd = BUILD_DAY;
    const uint16_t yy = (uint16_t)(by - 2000);
    uint32_t days = (uint32_t)yy * 365 + (yy + 3) / 4 + cum[bmo - 1] + (bd - 1);
    if (bmo <= 2 && (yy % 4) == 0) days--;
    int32_t secs = (int32_t)(days * 86400UL) + BUILD_HOUR * 3600
                 + BUILD_MIN * 60 + BUILD_SEC;
    secs -= (int32_t)TZ_OFFSET_MIN * 60;           /* local -> UTC */
    if (secs < 0) secs = 0;

    /* Back to civil fields. Same no-leap-second civil calendar rtcRead() uses. */
    uint32_t D = (uint32_t)secs / 86400UL;
    uint32_t rem = (uint32_t)secs % 86400UL;
    uint8_t  uh = (uint8_t)(rem / 3600), um = (uint8_t)((rem / 60) % 60),
             us = (uint8_t)(rem % 60);
    uint16_t uy = 2000;
    for (;;) {
        uint16_t len = (uint16_t)(((uy % 4) == 0) ? 366 : 365);
        if (D < len) break;
        D -= len; uy++;
    }
    uint8_t  umo = 1;
    for (; umo <= 12; umo++) {
        uint16_t ml = (uint16_t)((umo == 2) ? (((uy % 4) == 0) ? 29 : 28)
                     : ((umo == 4 || umo == 6 || umo == 9 || umo == 11) ? 30 : 31));
        if (D < ml) break;
        D -= ml;
    }
    uint8_t ud = (uint8_t)(D + 1);

    if (rtcSetUtc(uy, umo, ud, uh, um, us))
        Serial.printf("// [RTC] was unset/implausible — seeded from build stamp "
                      "(" __DATE__ " " __TIME__ " " TZ_NAME ") "
                      "-> %04u-%02u-%02u %02u:%02u:%02u UTC\n",
                      uy, umo, ud, uh, um, us);
    else
        Serial.println(F("// [RTC] seed FAILED — no usable clock. GNSS aiding "
                         "will fall back to position-only."));
}

/* UTC + TZ_OFFSET_MIN. Display and the SD CSV only. */
void SensorHub::localTime(uint16_t& y, uint8_t& mo, uint8_t& d,
                          uint8_t& h, uint8_t& mi, uint8_t& s) const {
    int32_t t = (int32_t)d_.utcHour * 3600 + (int32_t)d_.utcMin * 60 + d_.utcSec
              + (int32_t)TZ_OFFSET_MIN * 60;
    int32_t dayRoll = 0;
    while (t >= 86400) { t -= 86400; dayRoll++; }
    while (t < 0)      { t += 86400; dayRoll--; }
    h  = (uint8_t)(t / 3600);
    mi = (uint8_t)((t / 60) % 60);
    s  = (uint8_t)(t % 60);

    y = d_.utcYear; mo = d_.utcMon; d = d_.utcDay;
    if (!dayRoll) return;
    /* Only ever ±1 day for any real zone offset. */
    uint8_t ml = (uint8_t)((mo == 2) ? (((y % 4) == 0) ? 29 : 28)
                : ((mo == 4 || mo == 6 || mo == 9 || mo == 11) ? 30 : 31));
    if (dayRoll > 0) {
        if (++d > ml) { d = 1; if (++mo > 12) { mo = 1; y++; } }
    } else {
        if (d > 1) d--;
        else {
            if (--mo < 1) { mo = 12; y--; }
            d = (uint8_t)((mo == 2) ? (((y % 4) == 0) ? 29 : 28)
               : ((mo == 4 || mo == 6 || mo == 9 || mo == 11) ? 30 : 31));
        }
    }
}

void SensorHub::rtcRead() {
    uint8_t b[7];
    if (!rdN(RTC_ADDR, RTC_SECONDS, b, 7)) { d_.rtcOk = false; return; }
    d_.utcSec  = bcd2bin(b[0] & 0x7F);
    d_.utcMin  = bcd2bin(b[1] & 0x7F);
    d_.utcHour = bcd2bin(b[2] & 0x3F);          /* forced 24 h at init        */
    d_.utcDay  = bcd2bin(b[4] & 0x3F);
    d_.utcMon  = bcd2bin(b[5] & 0x1F);
    d_.utcYear = (uint16_t)(2000 + bcd2bin(b[6]));

    /* Seconds since 2000-01-01. Civil calendar, no leap seconds — this is a
     * log timestamp, not a time standard. */
    static const uint16_t cum[12] = {0,31,59,90,120,151,181,212,243,273,304,334};
    uint16_t y = (uint16_t)(d_.utcYear - 2000);
    uint8_t  m = (d_.utcMon >= 1 && d_.utcMon <= 12) ? d_.utcMon : 1;
    uint32_t days = (uint32_t)y * 365 + (y + 3) / 4 + cum[m - 1] +
                    (uint32_t)(d_.utcDay ? d_.utcDay - 1 : 0);
    if (m <= 2 && (y % 4) == 0) days--;          /* not yet past this Feb 29  */
    d_.utcEpoch = days * 86400UL + (uint32_t)d_.utcHour * 3600UL +
                  (uint32_t)d_.utcMin * 60UL + d_.utcSec;

    uint8_t t[2];
    if (rdN(RTC_ADDR, RTC_TEMP_MSB, t, 2))
        d_.rtcTempC = (float)(int8_t)t[0] + ((t[1] >> 6) * 0.25f);
    d_.rtcOk = true;
}

/* Li-ion open-circuit-voltage curve, per cell. A linear voltage-to-percent map
 * reads badly wrong across the 3.6-4.0 V plateau where most of the usable
 * capacity actually sits. */
static float cellSoc(float v) {
    static const float V[]   = {3.00f,3.30f,3.40f,3.50f,3.60f,3.70f,3.80f,
                                3.90f,4.00f,4.10f,4.20f};
    static const float S[]   = {0.0f, 2.0f, 6.0f, 12.0f,22.0f,35.0f,50.0f,
                                65.0f,80.0f,90.0f,100.0f};
    const int N = sizeof(V) / sizeof(V[0]);
    if (v <= V[0])     return 0.0f;
    if (v >= V[N - 1]) return 100.0f;
    for (int i = 1; i < N; i++)
        if (v < V[i])
            return S[i - 1] + (S[i] - S[i - 1]) * (v - V[i - 1]) / (V[i] - V[i - 1]);
    return 100.0f;
}

void SensorHub::readBus() {
    float raw = (float)analogRead(PIN_BUS_V);
    float v = raw * (3.3f / 4095.0f) * BUS_V_RATIO;
    /* Light smoothing — the ADC already averages 16 samples in hardware. */
    d_.busV = (d_.busV > 0.5f) ? (d_.busV * 0.8f + v * 0.2f) : v;
    d_.socPct = cellSoc(d_.busV / (float)BATT_CELLS);
}

/* ============================================================================
 * STAGED, NON-BLOCKING INIT
 * ========================================================================= */
void SensorHub::begin() {
    stage_    = IS_RECOVER;
    stageAt_  = millis();
    cfgLine_  = 0;
    initDone_ = false;
    pinMode(PIN_GNSS_RST, OUTPUT);
    digitalWrite(PIN_GNSS_RST, HIGH);       /* active low, idle high          */
    /* Sensor INT lines, wired at harness rev 2026-08. Claimed as inputs so the
     * pins are reserved and cannot float into a driven state, but NOT attached
     * to interrupts — acquisition stays polled on the 50 Hz service tick. See
     * the PIN_BMP_INT note in Config.h for why. Both sensors drive these
     * push-pull, so no pull-up is configured. */
    pinMode(PIN_BMP_INT, INPUT);
    pinMode(PIN_ICM_INT, INPUT);
    /* DS3231 SQW is OPEN DRAIN — it can only pull low, never drive high. With
     * no pull-up the line sits floating and an edge counter sees nothing at
     * all, which is exactly the "zero transitions on pin 2" result in
     * SENSOR_TESTING_FINDINGS §4 that was recorded as an unexplained fault.
     * A missing pull-up fits that evidence better than a dead RTC does, since
     * the same chip answers normally over I2C. INPUT_PULLUP costs nothing, is
     * harmless if the breakout already fits its own resistor, and removes the
     * most likely cause. Teensy's internal pull-up is ~22 k, weak but ample
     * for a 1 Hz square wave. Unconfirmed until someone scopes the pin. */
    pinMode(PIN_RTC_SQW, INPUT_PULLUP);
    analogReadResolution(ADC_BITS);
    analogReadAveraging(ADC_AVERAGING);
    for (int i = 0; i < 3; i++) { magLo_[i] = 1e9f; magHi_[i] = -1e9f; }
}

/* ============================================================================
 * GNSS CONFIGURATION — MODULE IS QUECTEL L89 / AIROHA AG3335M, NOT ST TESEO.
 * ============================================================================
 * CONFIRMED BY BRING-UP TEST: the module outputs "$PAIR021,AG3335M_V2.5.0..."
 * on first boot. PAIR is the Airoha/AG3335M proprietary NMEA talker — this is
 * a Quectel L89 wrapping an Airoha AG3335M chip, not an STMicro Teseo III.
 *
 * The previous $PSTM commands were silently ignored by this chip. Every one of
 * them was a no-op. The correct protocol is $PMTK (MediaTek/Airoha native),
 * and Quectel extends it with $PQTM proprietary sentences.
 *
 * FACTORY DEFAULT (confirmed from GSV output):
 *   GPS + Galileo + NavIC/IRNSS already enabled. GLONASS is NOT in the
 *   default. $PMTK353 adds it; the command takes effect immediately without a
 *   restart on the AG3335M.
 *
 * PMTK353 parameter order: GPS, GLONASS, Galileo, BeiDou, IRNSS
 *   1 = enabled, 0 = disabled.
 *   GPS+GLONASS+Galileo+NavIC (no BDS): "PMTK353,1,1,1,0,1"
 *   BeiDou is omitted because running 5 constellations simultaneously may
 *   exceed the AG3335M channel budget; 4 is reliable.
 *
 * NO FIX RATE CHANGE. Default 1 Hz matches the downlink cadence and the
 * 9600 baud UART. Higher rates are not needed and waste UART bandwidth. */
/* ---------------------------------------------------------------------------
 * NAVIC — WHY THE OLD CONFIGURATION NEVER ENABLED IT
 * ------------------------------------------------------------------------ *
 * The line here used to be:
 *     "PMTK353,1,1,1,0,1"   with the comment "GPS + GLONASS + Galileo + NavIC"
 * and NAVIC_SATS has read 0 on every flight since.
 *
 * $PMTK353 IS A MEDIATEK COMMAND AND ITS FIFTH PARAMETER IS NOT NAVIC. The
 * MediaTek definition, unchanged since the MT3333, is:
 *     $PMTK353,<GPS>,<GLONASS>,<GALILEO>,<GALILEO_FULL>,<BEIDOU>
 * So that command asked for GPS + GLONASS + Galileo + BeiDou, and said nothing
 * whatsoever about NavIC. The comment described intent, not behaviour. Worse,
 * it is the command the code then relied on, so nothing ever reported that
 * NavIC had not been enabled — $PMTK353 was ACKed, just for something else.
 *
 * THE CORRECT COMMAND FOR THIS MODULE IS $PAIR066. The bring-up test recorded
 * the receiver announcing itself as "$PAIR021,AG3335M_V2.5.0,..." — PAIR is the
 * Airoha proprietary talker, which is what an AG3335M speaks natively. Airoha's
 * constellation selector is:
 *     $PAIR066,<GPS>,<GLONASS>,<Galileo>,<BeiDou>,<QZSS>,<NavIC>
 * NavIC has its own parameter, in sixth position, and nothing else reaches it.
 *
 * FOUR CONSTELLATIONS, NOT SIX. The previous note in this file — that running
 * five or more simultaneously can exceed the AG3335M's channel budget and that
 * four is reliable — still stands and is respected here. NavIC displaces
 * BeiDou rather than being added on top of it, because NavIC is the one this
 * mission actually cares about. QZSS stays off for the same reason.
 *
 * $PMTK353 IS KEPT, DELIBERATELY, AND SENT SECOND. If this receiver ever turns
 * out to be a variant that answers MTK and not PAIR, the PMTK line still brings
 * up GPS + GLONASS + Galileo and the vehicle degrades to a working 3-system fix
 * instead of no configuration at all. Sending both costs 60 ms.
 *
 * THE ACKS ARE NOW PARSED. Every PAIR command is answered with
 * "$PAIR001,<cmd>,<result>" where result 0 means accepted. nmeaLine() decodes
 * it and prints the outcome, so "did the receiver take the NavIC command" is a
 * question the boot log answers rather than something to be inferred from a
 * satellite count three minutes later.
 *
 * HARDWARE CAVEAT, WHICH FIRMWARE CANNOT FIX. NavIC's civil signal is primarily
 * L5 (1176.45 MHz), not L1 (1575.42 MHz). If the patch antenna fitted to this
 * vehicle is a plain GPS L1 element — which most small ceramic patches are —
 * then NavIC satellites cannot be received no matter what this receiver is
 * configured to search for, and NAVIC_SATS will stay 0 with every command here
 * ACKed successfully. Confirm the antenna is an L1+L5 dual-band type before
 * concluding anything from a zero count. */
static const char* const GNSS_CFG[] = {
    /* GPS, GLONASS, Galileo, BeiDou=0, QZSS=0, NavIC=1 */
    "PAIR066,1,1,1,0,0,1",
    /* MTK fallback for a non-PAIR variant: GPS, GLONASS, Galileo, GalFull, BDS.
     * There is no NavIC parameter in this command — see the block above. */
    "PMTK353,1,1,1,0,0",
    "PMTK869,1,1",         /* Enable EASY: saves orbit predictions to internal flash     */
};
static const uint8_t GNSS_CFG_N = sizeof(GNSS_CFG) / sizeof(GNSS_CFG[0]);

void SensorHub::sendPMTKCommand(String payload) {
    byte checksum = 0;
    for (unsigned int i = 0; i < payload.length(); i++) {
        checksum ^= payload.charAt(i);
    }
    String hexSum = String(checksum, HEX);
    if (hexSum.length() < 2) hexSum = "0" + hexSum;
    hexSum.toUpperCase();
    if (gnss_) {
        gnss_->print("$" + payload + "*" + hexSum + "\r\n");
    }
}

void SensorHub::gnssSend(const char* body) {
    uint8_t ck = 0;
    for (const char* p = body; *p; p++) ck ^= (uint8_t)*p;
    gnss_->print('$');
    gnss_->print(body);
    gnss_->print('*');
    if (ck < 16) gnss_->print('0');
    gnss_->println(ck, HEX);
}

/* Backing store for the GNSS UARTs — one per candidate port, because both are
 * open simultaneously during auto-detection. Must be file-scope and must
 * outlive the ports: addMemoryForRead() keeps the pointer, it does not copy.
 * 2 KB total on a 1 MB part. */
static uint8_t gnssBufA_[1024];
static uint8_t gnssBufB_[1024];

void SensorHub::gnssHardReset() {
    digitalWrite(PIN_GNSS_RST, LOW);        /* released by IS_GNSS_PORT       */
    stage_   = IS_GNSS_PORT;                /* IS_GNSS_PORT now owns the reset
                                               release; the old code jumped
                                               past it and wedged the L89 in
                                               reset until a power cycle      */
    gnssLocked_ = false;                    /* re-detect: the harness may have
                                               changed since the last boot    */
    stageAt_ = millis();
    cfgLine_ = 0;
    initDone_ = false;
}

void SensorHub::service() {
    const uint32_t now = millis();

    /* ---- staged init ---------------------------------------------------- */
    if (!initDone_) {
        switch (stage_) {
        case IS_RECOVER:
            busRecover();
            stage_ = IS_WIRE;
            break;

        case IS_WIRE:
            Wire.begin();
            Wire.setClock(I2C_CLOCK_HZ);
            stage_ = IS_BMP_PROBE; stageAt_ = now;
            break;

        case IS_BMP_PROBE: {
            uint8_t id = 0;
            bmpAddr_ = BMP_ADDR_A;
            bool ok = rd8(bmpAddr_, BMP_CHIP_ID_REG, id) && id == BMP_CHIP_ID;
            if (!ok) {
                bmpAddr_ = BMP_ADDR_B;
                ok = rd8(bmpAddr_, BMP_CHIP_ID_REG, id) && id == BMP_CHIP_ID;
            }
            d_.baroOk = ok;
            if (ok) { wr8(bmpAddr_, BMP_CMD, 0xB6); stage_ = IS_BMP_RESET; }
            else      stage_ = IS_ICM_PROBE;
            stageAt_ = now;
            break; }

        case IS_BMP_RESET:
            if (now - stageAt_ < 10) break;            /* soft-reset settle   */
            wr8(bmpAddr_, BMP_ODR_CONFIG, BMP_ODR_STANDBY);
            stage_ = IS_BMP_STANDBY; stageAt_ = now;
            break;

        case IS_BMP_STANDBY:
            if (now - stageAt_ < 5) break;
            wr8(bmpAddr_, BMP_DSP_IIR,    0x00);       /* on-chip IIR OFF     */
            wr8(bmpAddr_, BMP_OSR_CONFIG, BMP_OSR_BYTE);
            wr8(bmpAddr_, BMP_ODR_CONFIG, BMP_ODR_NORMAL);
            stage_ = IS_BMP_CONFIG; stageAt_ = now;
            break;

        case IS_BMP_CONFIG:
            if (now - stageAt_ < 30) break;            /* first conversion    */
            stage_ = IS_BMP_VERIFY;
            break;

        case IS_BMP_VERIFY: {
            float t, p;
            /* Accept the barometer only on a physically plausible reading.
             * A configuration that ACKs but produces out-of-range pressure is
             * the exact failure that previously printed "[BMP] OK" while
             * feeding garbage into the altitude pipeline. */
            d_.baroOk = bmpRead(t, p) && p > BARO_MIN_PA && p < BARO_MAX_PA;
            /* Reset the IMU BEFORE probing it — see IS_ICM_RESET. */
            wr8(ICM_ADDR_A, ICM_PWR_MGMT_1, 0x80);     /* DEVICE_RESET        */
            stage_ = IS_ICM_RESET; stageAt_ = now;
            break; }

        /* UNCONDITIONAL DEVICE_RESET, ISSUED BLIND BEFORE THE PROBE.
         *
         * When an I2C transaction is cut off mid-byte — a wire wiggles, or the
         * Teensy is reset while the ICM is clocking out data — the sensor's
         * internal state machine stalls waiting for clock edges that never
         * arrive. It then refuses new requests: the address phase still ACKs,
         * but every register read returns 0xFF. That is indistinguishable from
         * a dead part, and it survives a Teensy reset because only the Teensy
         * rebooted, not the IMU.
         *
         * The nine SCL pulses in busRecover() free the BUS, but they cannot
         * clear the ICM's own internal state. Only PWR_MGMT_1 = 0x80 does that:
         * it wipes the state machine and reboots the die.
         *
         * WHY BEFORE THE PROBE, NOT AFTER. A wedged ICM fails WHO_AM_I, so a
         * reset placed after a successful probe never runs in exactly the case
         * that needs it. Issued blind and unconditionally, it costs one ignored
         * write when the part is healthy and rescues it when it is not.
         *
         * WHY ONLY 0x69. ICM_ADDR_B is 0x68, which on this harness is the
         * DS3231 — and DS3231 register 0x06 is the YEAR register. A blind 0x80
         * there would write an invalid BCD year and corrupt the RTC date. The
         * ICM on this board straps AD0 high (I2C scan shows 0x68 and 0x69 both
         * present, and WHO_AM_I answers 0xEA only at 0x69), so resetting 0x69
         * alone is both sufficient and safe. If a future board straps AD0 low,
         * this reset must move — do not simply add 0x68 here.
         *
         * 100 ms is the datasheet-safe settling time; the part typically
         * returns in ~35 ms. PWR_MGMT_1 self-clears the reset bit and the bank
         * selector returns to 0, which is why IS_ICM_PROBE re-selects it. */
        case IS_ICM_RESET:
            if (now - stageAt_ < 100) break;
            stage_ = IS_ICM_PROBE; stageAt_ = now;
            break;

        case IS_ICM_PROBE: {
            uint8_t who = 0;
            icmAddr_ = ICM_ADDR_A;
            icmBank(0);
            bool ok = rd8(icmAddr_, ICM_WHO_AM_I, who) && who == ICM_WHOAMI;
            if (!ok) {
                /* 0x68 is also the DS3231. Only WHO_AM_I == 0xEA proves this
                 * is the IMU — never assume from the address alone. */
                icmAddr_ = ICM_ADDR_B;
                icmBank(0);
                ok = rd8(icmAddr_, ICM_WHO_AM_I, who) && who == ICM_WHOAMI;
            }
            d_.imuOk = ok;
            if (ok) {
                icmBank(0);
                wr8(icmAddr_, ICM_PWR_MGMT_1, 0x01);   /* wake, CLKSEL auto   */
                stage_ = IS_ICM_WAKE;
            } else {
                icmAddr_ = ICM_ADDR_A;
                stage_ = IS_RTC;
            }
            stageAt_ = now;
            break; }

        case IS_ICM_WAKE:
            if (now - stageAt_ < 10) break;
            wr8(icmAddr_, ICM_PWR_MGMT_2, 0x00);       /* accel+gyro enabled  */
            wr8(icmAddr_, ICM_LP_CONFIG,  0x00);       /* duty cycling off    */
            icmBank(2);
            /* 0x2F = GYRO_DLPFCFG 5 (11.6 Hz) | FS_SEL 3 (+-2000 dps) | FCHOICE 1.
             *
             * ANTI-ALIASING, NOT JUST NOISE. This was 0x27 (DLPFCFG 4, 23.9 Hz)
             * and that was wrong for a reason more serious than noise: we poll
             * at 50 Hz, so Nyquist is exactly 25 Hz, and a 23.9 Hz corner gives
             * essentially ZERO attenuation at the fold-over frequency. Every
             * component above 25 Hz — N20 gear whine, motor commutation, chute
             * snatch, aero-acoustic buffet — aliases down into the baseband and
             * becomes indistinguishable from real vehicle rotation. No filter
             * downstream can remove it once folded.
             *
             * DLPFCFG 5 puts the corner at 11.6 Hz, a guard band of just over
             * 2x below Nyquist, which is the standard margin. It also cuts the
             * measured 4.3-11.2 dps pp noise floor further, since gyro noise
             * scales with the square root of bandwidth.
             *
             * The alternative fix is to raise the poll rate to 100 Hz and keep
             * 23.9 Hz. That is better if the vehicle ever needs bandwidth above
             * 11 Hz, but rigid-body rates on a parachute descent are far below
             * that, and this change costs nothing in scheduler complexity.
             *
             * FS_SEL stays +-2000 dps: 16.4 LSB/dps discards resolution against
             * a 300 dps sustained spin, but ejection can exceed 1000 dps and
             * clipping the one measurement the mission exists to capture is not
             * a trade worth making. Re-evaluate once the wheel's real angular
             * momentum authority has been calculated. */
            wr8(icmAddr_, ICM_B2_GYRO_CFG1,   0x2F);   /* DLPF 11.6Hz, +-2000 dps */
            wr8(icmAddr_, ICM_B2_GYRO_SMPLRT, 0x00);
            wr8(icmAddr_, ICM_B2_ACCEL_CFG,   0x1F);   /* DLPF on, +-16 g     */
            wr8(icmAddr_, ICM_B2_ACC_SMPLRT1, 0x00);
            wr8(icmAddr_, ICM_B2_ACC_SMPLRT2, 0x00);
            icmBank(0);
            stage_ = IS_ICM_CONFIG; stageAt_ = now;
            break;

        case IS_ICM_CONFIG:
            if (now - stageAt_ < 20) break;
            stage_ = IS_ICM_VERIFY;
            break;

        case IS_ICM_VERIFY: {
            d_.imuOk = icmRead();
            /* At rest |a| must be near 1 g. A chip that ACKs but returns
             * garbage on every read is a wedged bus, not a missing part. */
            if (d_.imuOk && (d_.accMagG < 0.2f || d_.accMagG > 4.0f))
                d_.imuOk = false;
            stage_ = d_.imuOk ? IS_MAG_BYPASS : IS_RTC;
            stageAt_ = now;
            break; }

        case IS_MAG_BYPASS: {
            /* The magnetometer is a separate die reachable only once the ICM's
             * bypass bit is asserted. Any library call that writes
             * INT_PIN_CFG (applyBasicDefaults() writes 0x30) CLEARS bypass as
             * a side effect — which is the entire reason 0x0C "never shows up
             * in an I2C scan". Assert it explicitly, after the ICM's own
             * configuration, or it silently reverts. */
            uint8_t uc = 0;
            icmBank(0);
            rd8(icmAddr_, ICM_USER_CTRL, uc);
            wr8(icmAddr_, ICM_USER_CTRL, (uint8_t)(uc & ~0x20)); /* I2C_MST off */
            wr8(icmAddr_, ICM_INT_PIN_CFG, 0x02);                /* BYPASS_EN   */
            stage_ = IS_MAG_RESET; stageAt_ = now;
            break; }

        case IS_MAG_RESET:
            if (now - stageAt_ < 5) break;
            wr8(AK_ADDR, AK_CNTL3, 0x01);              /* soft reset          */
            stage_ = IS_MAG_MODE; stageAt_ = now;
            break;

        case IS_MAG_MODE:
            if (now - stageAt_ < 10) break;
            wr8(AK_ADDR, AK_CNTL2, 0x08);              /* continuous, 100 Hz  */
            stage_ = IS_MAG_VERIFY; stageAt_ = now;
            break;

        case IS_MAG_VERIFY: {
            if (now - stageAt_ < 15) break;
            uint8_t id = 0;
            d_.magOk = rd8(AK_ADDR, AK_WIA2_REG, id) && id == AK_WIA2;
            stage_ = IS_RTC; stageAt_ = now;
            break; }

        case IS_RTC: {
            uint8_t st = 0;
            if (rd8(RTC_ADDR, RTC_STATUS, st)) {
                uint8_t h = 0;
                rd8(RTC_ADDR, 0x02, h);
                if (h & 0x40)                          /* force 24-hour mode  */
                    wr8(RTC_ADDR, 0x02, (uint8_t)(h & 0x3F));
                rtcRead();
                /* Do this HERE, not in setup(): the RTC has just been probed
                 * and read, so rtcValid() has something real to judge. It only
                 * writes when the clock is not believable, so a good clock is
                 * never dragged back to the build stamp. The GNSS aiding that
                 * runs a few stages later depends on this having happened —
                 * a receiver aided with a nonsense time is slower than one
                 * given no time at all. */
                seedRtcFromBuildTime();
            } else d_.rtcOk = false;
            /* Drive RESET_N low and let IS_GNSS_DETECT release it. Every boot
             * therefore gives the L89 a deterministic reset pulse instead of
             * relying on whatever state it was left in by the previous run. */
            digitalWrite(PIN_GNSS_RST, LOW);
            stage_ = IS_GNSS_PORT; stageAt_ = now;
            break; }

        /* THE ONLY PLACE RESET_N IS RELEASED.
         * Previously the release lived in IS_RTC, which meant gnssHardReset()
         * — the CMD_GPS_RESET path — pulled RESET_N low, jumped straight to
         * IS_GNSS_PORT, and NEVER RAISED IT AGAIN. The receiver was held in
         * reset permanently and the only recovery was a power cycle. Routing
         * both the boot path and the command path through this one stage makes
         * that unrepresentable.
         * 50 ms is comfortably above the L89's minimum reset pulse width. */
        case IS_GNSS_PORT:
            if (now - stageAt_ < 50) break;        /* reset pulse width       */
            digitalWrite(PIN_GNSS_RST, HIGH);
            GNSS_PORT_A.begin(GNSS_BAUD);
            /* ENLARGE THE UART RX BUFFER — do not remove this line.
             *
             * Teensy's default hardware-serial RX buffer is 64 bytes. At 9600
             * baud that is 66 ms of data. One 1 Hz NMEA burst carrying
             * GGA+RMC+GSA+GSV+GIGSV is 400-700 bytes and takes ~600 ms to
             * arrive, so the buffer must survive every pause in loop() across
             * that whole window. It does not: an SD sync(), a 944 ms SF10
             * transmit slot, or any other stall longer than 66 ms silently
             * overruns it. The bytes are dropped mid-sentence, the checksum
             * then fails, nmeaLine() discards the line, and the symptom is
             * "the GNSS never gets a fix" while the receiver is in fact working
             * perfectly and the antenna is fine.
             *
             * This is a REGRESSION FIX. The older monolithic firmware called
             * addMemoryForRead() and got 7-11 satellites on the bench; the
             * modular rewrite dropped the call, and that is the most likely
             * single cause of the fix disappearing.
             *
             * 1024 bytes buys ~1.06 s of slack at 9600 baud, which covers the
             * worst TX slot with margin. */
            GNSS_PORT_A.addMemoryForRead(gnssBufA_, sizeof(gnssBufA_));
            GNSS_PORT_B.begin(GNSS_BAUD);
            GNSS_PORT_B.addMemoryForRead(gnssBufB_, sizeof(gnssBufB_));
            gnssLocked_ = false;
            stage_ = IS_GNSS_DETECT; stageAt_ = now;
            break;

        /* PORT AUTO-DETECTION.
         *
         * Both candidate UARTs are open. Whichever one first delivers a '$'
         * — the start of any NMEA sentence — is the one the receiver is
         * physically wired to, and we lock to it and close the other.
         *
         * WHY THIS EXISTS. The rev 2026-08 rewire moved the L89 from Serial1
         * (pins 0/1) to Serial2 (pins 7/8). A firmware/harness mismatch there
         * produces no GNSS output whatsoever, which is indistinguishable from
         * a dead module, a broken antenna, or an indoor test with no sky view.
         * Those are four different problems and guessing between them wastes
         * hours. After this stage the boot log states which port answered, or
         * says plainly that neither did — which narrows it to the module or
         * the antenna and rules the wiring out.
         *
         * The read budget is bounded per pass so a chattering port cannot
         * stall the 25 Hz barometer cadence. Bytes consumed here are discarded;
         * NMEA repeats every second, so nothing is lost.
         *
         * CLOSING THE LOSER MATTERS. Serial1's RX is pin 0, which this harness
         * also uses as BMP585 INT. Leaving Serial1 open would hold pin 0 as a
         * UART input forever. end() releases it and the pinMode() restores it
         * to the plain input the pin map promises. */
        case IS_GNSS_DETECT: {
            for (uint8_t n = 0; n < 64 && !gnssLocked_ && GNSS_PORT_A.available(); n++)
                if ((char)GNSS_PORT_A.read() == '$') { gnss_ = &GNSS_PORT_A; gnssLocked_ = true; }
            for (uint8_t n = 0; n < 64 && !gnssLocked_ && GNSS_PORT_B.available(); n++)
                if ((char)GNSS_PORT_B.read() == '$') { gnss_ = &GNSS_PORT_B; gnssLocked_ = true; }

            if (!gnssLocked_ && (now - stageAt_) < GNSS_AUTODETECT_MS) break;

            if (!gnssLocked_) {
                /* Neither port spoke. Fall back to the documented harness so
                 * the rest of init proceeds normally, but say so loudly. */
                gnss_ = &GNSS_PORT_A;
                Serial.println(F("// [GNSS] NO DATA ON EITHER PORT after autodetect.\n"
                                 "// [GNSS]   Not a wiring-port problem — both were listened to.\n"
                                 "// [GNSS]   Check 3V3 at the module, RESET_N (pin 6) idle HIGH,\n"
                                 "// [GNSS]   and that TX/RX are not swapped. Defaulting to Serial2."));
            } else {
                Serial.printf("// [GNSS] locked to %s\n", gnssPortName());
            }

            if (gnss_ != &GNSS_PORT_B) {
                GNSS_PORT_B.end();
                pinMode(PIN_BMP_INT, INPUT);   /* Serial1 RX == pin 0 == BMP INT */
            } else {
                GNSS_PORT_A.end();
            }
            stage_ = IS_GNSS_CFG; stageAt_ = now; cfgLine_ = 0;
            break; }

        case IS_GNSS_CFG:
            /* Module is AG3335M. Factory default = GPS+Galileo+NavIC.
             * GNSS_SEND_CONFIG adds GLONASS via $PMTK353. Disable to use the
             * factory constellation set unchanged. */
            if (!GNSS_SEND_CONFIG) {
                Serial.println(F("// [GNSS] factory defaults (config block disabled)"));
                stage_ = IS_DONE;
                break;
            }
            /* 300 ms for the receiver's UART to come up, then one config line
             * every 60 ms. Pacing matters: the L89 drops commands that arrive
             * faster than it can consume them. */
            if (cfgLine_ == 0 && now - stageAt_ < 300) break;
            if (cfgLine_ > 0  && now - stageAt_ < 60)  break;
            if (cfgLine_ < GNSS_CFG_N) {
                gnssSend(GNSS_CFG[cfgLine_++]);
                stageAt_ = now;
            } else {
                stage_ = IS_DONE;
            }
            break;

        case IS_DONE:
            initDone_ = true;
            /* AUTO-SURVEY ONLY IF WE DO NOT ALREADY HAVE A REFERENCE.
             *
             * The old code called the blocking calibrateBarometer() here
             * unconditionally, which is actively dangerous: guideline §6.1.ix
             * requires system state to survive a processor reset, and a
             * brownout at 900 m would have re-zeroed the altimeter to the
             * pressure AT 900 m. Altitude would read 0, the FSM would see no
             * descent, and the secondary-deployment trigger at 600 m could
             * never fire. A reset mid-flight must inherit the pad reference,
             * never invent a new one.
             *
             * loadState() -> restoreCalibration() has already run by the time
             * this stage is reached (setup() calls it before the first
             * service() pass completes init), so a valid persisted reference is
             * present here if one exists. A fresh mission has padPresPa == 0,
             * fails the range check, and surveys normally. */
            if (d_.baroOk) {
                if (d_.padPresPa > BARO_MIN_PA && d_.padPresPa < BARO_MAX_PA) {
                    Serial.printf("// [CAL] pad reference RESUMED from EEPROM: "
                                  "%.1f Pa @ %.1f C — not re-surveying\n",
                                  d_.padPresPa, d_.padTempC);
                } else {
                    Serial.println(F("// [CAL] no stored pad reference — surveying"));
                    startBaroCal();
                }
            }
            nextBaroMs_ = nextImuMs_ = nextRtcMs_ = nextBusMs_ = now;
            break;
        }
        /* NMEA is pumped even during init so the receiver's output buffer
         * never overflows while the config lines are still going out. */
        gnssPump();
        return;
    }

    /* ---- steady state --------------------------------------------------- */
    gnssPump();

    if (d_.baroOk && (int32_t)(now - nextBaroMs_) >= 0) {
        nextBaroMs_ = now + (1000UL / BARO_SAMPLE_HZ);
        float t, p;
        if (bmpRead(t, p) && p > BARO_MIN_PA && p < BARO_MAX_PA) {
            /* median-of-5 first: it deletes a single torn burst outright,
             * which no linear filter can do. */
            medBuf_[medIdx_] = p;
            medIdx_ = (uint8_t)((medIdx_ + 1) % BARO_MEDIAN_N);
            if (medN_ < BARO_MEDIAN_N) medN_++;

            float s[BARO_MEDIAN_N];
            for (uint8_t i = 0; i < medN_; i++) s[i] = medBuf_[i];
            for (uint8_t i = 1; i < medN_; i++) {        /* insertion sort    */
                float k = s[i]; int8_t j = (int8_t)i - 1;
                while (j >= 0 && s[j] > k) { s[j + 1] = s[j]; j--; }
                s[j + 1] = k;
            }
            float med = s[medN_ / 2];

            if (!presInit_) { presFilt_ = med; presInit_ = true; }
            else {
                /* Glitch gate at ~13x the measured 3.8 Pa noise floor. A
                 * single-sample jump past this is a torn read, not pressure. */
                if (fabsf(med - presFilt_) < BARO_GLITCH_PA)
                    presFilt_ += BARO_SMOOTH_ALPHA * (med - presFilt_);
                else
                    presFilt_ += 0.05f * (med - presFilt_);  /* slow re-acquire */
            }

            d_.presPa = presFilt_;
            d_.tempC  = t;
            /* ASL stays referenced to the ISA standard atmosphere on purpose:
             * "altitude above mean sea level" is only comparable between
             * vehicles if everyone uses the same reference. AGL below is the
             * one that is compensated, because it is referenced to OUR pad. */
            d_.altAslM = 44330.8f * (1.0f - powf(presFilt_ / 101325.0f, 0.1902632f));

            d_.altM = altitudeAgl(d_.presPa);

            if (lastAltMs_ && now > lastAltMs_) {
                float dt = (now - lastAltMs_) * 0.001f;
                float v  = (d_.altM - lastAlt_) / dt;
                d_.vspeedMs = d_.vspeedMs * 0.7f + v * 0.3f;
            }
            lastAlt_ = d_.altM; lastAltMs_ = now;

            /* Pad survey accumulates the CONDITIONED value, on the barometer's
             * own cadence — one independent conversion per sample. */
            if (baroCalActive_) {
                baroCalPSum_ += presFilt_;
                baroCalTSum_ += t;
                baroCalN_++;
            }
        }
    }
    baroCalService(now);

    if (d_.imuOk && (int32_t)(now - nextImuMs_) >= 0) {
        nextImuMs_ = now + (1000UL / SENSOR_SERVICE_HZ);
        if (!icmRead()) {
            /* The IMU is the part most sensitive to bus noise on this harness.
             * Re-run its configuration after a run of failures rather than
             * declaring it dead — a wedged bus recovers, a dead chip does not
             * and will simply fail again. */
            if (++icmFailRun_ >= 10) {
                icmFailRun_ = 0;
                stage_ = IS_ICM_PROBE; stageAt_ = now; initDone_ = false;
            }
        } else icmFailRun_ = 0;
        if (d_.magOk) magRead();
    }

    if ((int32_t)(now - nextRtcMs_) >= 0) { nextRtcMs_ = now + 1000; rtcRead(); }
    if ((int32_t)(now - nextBusMs_) >= 0) { nextBusMs_ = now + 200;  readBus(); }

    /* ---- GNSS -> DS3231, once per boot ---------------------------------- *
     * THIS IS THE STEP THAT MAKES THE NEXT BOOT FAST. The DS3231 keeps time on
     * its own coin cell through a power cycle; the L89 does not. So the moment
     * GNSS delivers real UTC, it is written into the RTC, and from then on the
     * vehicle can hand an accurate time to the receiver at the START of the
     * next cold boot instead of waiting for the receiver to decode one off the
     * air — which is a large part of what makes a cold start slow.
     *
     * Gated on a fix with a decoded DATE (RMC field 9), not merely a time:
     * GGA gives hh:mm:ss with no day, and setting the RTC to the right time on
     * the wrong date is worse than not setting it.
     *
     * Done ONCE per boot. Rewriting the RTC every second would burn I2C
     * bandwidth for no gain — the part is ±2 ppm and cannot drift measurably
     * inside one flight. */
    if (!d_.rtcSynced && d_.gnssFix && d_.gnssYear >= RTC_MIN_YEAR
        && d_.gnssMon >= 1 && d_.gnssDay >= 1 && d_.gnssTime > 0) {
        const uint8_t gh = (uint8_t)(d_.gnssTime / 1000000UL);
        const uint8_t gm = (uint8_t)((d_.gnssTime / 10000UL) % 100UL);
        const uint8_t gs = (uint8_t)((d_.gnssTime / 100UL) % 100UL);
        if (rtcSetUtc(d_.gnssYear, d_.gnssMon, d_.gnssDay, gh, gm, gs)) {
            d_.rtcSynced = true;
            Serial.printf("// [RTC] synced from GNSS -> %04u-%02u-%02u "
                          "%02u:%02u:%02u UTC (next boot starts warm)\n",
                          d_.gnssYear, d_.gnssMon, d_.gnssDay, gh, gm, gs);
        }
    }

    /* ---- stillness-gated gyro/accel calibration -------------------------- *
     * EVERY EXIT FROM THIS MACHINE NOW PRINTS. It previously had a silent
     * failure path — the CAL_TIMEOUT_MS branch dropped straight back to
     * CAL_IDLE keeping the old constants and saying nothing — and because
     * CAL_STILL_DPS was set below this part's own noise floor, that silent path
     * was the one taken EVERY TIME. From the operator's seat the command simply
     * had no effect and no explanation. A calibration that cannot converge is
     * useful information; swallowing it is not. */
    if (calState_ != CAL_IDLE) {
        if (now - calStartMs_ > CAL_TIMEOUT_MS) {
            float spread = 0;
            for (int i = 0; i < 3; i++) {
                float s = calGMax_[i] - calGMin_[i];
                if (s > spread) spread = s;
            }
            calState_ = CAL_IDLE;                 /* never still — keep old   */
            Serial.printf("// [CAL] gyro TIMED OUT after %lu s — never still enough "
                          "(worst spread %.1f dps vs %.1f limit). Bias UNCHANGED "
                          "at %.2f/%.2f/%.2f dps.\n",
                          (unsigned long)(CAL_TIMEOUT_MS / 1000UL),
                          spread, (double)CAL_STILL_DPS,
                          gBias_[0], gBias_[1], gBias_[2]);
        } else if (calState_ == CAL_WAIT_STILL) {
            if (now - calStartMs_ < CAL_ARM_MS) {
                for (int i = 0; i < 3; i++) { calGMin_[i] = 1e9f; calGMax_[i] = -1e9f; }
            } else {
                float spread = 0;
                for (int i = 0; i < 3; i++) {
                    float s = calGMax_[i] - calGMin_[i];
                    if (s > spread) spread = s;
                }
                if (spread > CAL_STILL_DPS) {
                    calStillMs_ = now;
                    for (int i = 0; i < 3; i++) { calGMin_[i] = 1e9f; calGMax_[i] = -1e9f; }
                } else if (now - calStillMs_ > 400) {
                    calState_ = CAL_ACCUM;
                    calStillMs_ = now;
                    calGSum_[0] = calGSum_[1] = calGSum_[2] = 0;
                    calN_ = 0;
                    Serial.printf("// [CAL] gyro still (spread %.1f dps) — averaging "
                                  "for %lu ms\n", spread,
                                  (unsigned long)CAL_STILL_MS);
                }
            }
        } else if (calState_ == CAL_ACCUM) {
            if (now - calStillMs_ >= CAL_STILL_MS && calN_ > 50) {
                gBias_[0] = (float)(calGSum_[0] / calN_);
                gBias_[1] = (float)(calGSum_[1] / calN_);
                gBias_[2] = (float)(calGSum_[2] / calN_);
                /* THE PAD REFERENCE IS NOT WRITTEN HERE ANY MORE. This branch
                 * used to do d_.padPresPa = presFilt_ — one instantaneous
                 * sample, not an average, from a third code path that also
                 * believed it owned the reference. The survey in
                 * baroCalService() owns it now and averages properly. */
                const float scaleWas = accScale_;
                if (d_.accMagG > 0.5f && d_.accMagG < 1.5f)
                    accScale_ = 1.0f / d_.accMagG;
                calState_ = CAL_IDLE;
                Serial.printf("// [CAL] gyro bias = %.3f/%.3f/%.3f dps  "
                              "accel scale %.5f -> %.5f  (%lu samples)\n",
                              gBias_[0], gBias_[1], gBias_[2],
                              scaleWas, accScale_, (unsigned long)calN_);
            }
        }
    }
}

/* Arm the barometric pad survey. Separate from startCalibration() so the boot
 * path can survey pressure without also demanding the stillness gate pass —
 * see the note on baroCalActive_ in Sensors.h. */
void SensorHub::startBaroCal() {
    baroCalActive_  = true;
    baroCalStartMs_ = millis();
    baroCalPSum_    = 0;
    baroCalTSum_    = 0;
    baroCalN_       = 0;
}

/* §6.1.vi — "the ground station shall be able to command the CanSat to
 * calibrate the gyroscope, barometric altitude and accelerometer to zero".
 * All three, plus the attitude datum, from this one entry point.
 *
 * The three sub-calibrations are deliberately INDEPENDENT of one another:
 *   attitude datum  — immediate, cannot fail;
 *   barometric pad  — BARO_CAL_MS of conditioned pressure, needs no stillness;
 *   gyro/accel bias — stillness-gated, may legitimately refuse to converge.
 * Coupling them meant a gyro gate that could not pass also silently discarded a
 * perfectly good pressure survey and left the attitude datum untouched, so one
 * marginal condition took out all three. Now a windy pad still gets a correct
 * altitude zero and a correct home pose, and says clearly which part failed. */
void SensorHub::startCalibration() {
    calState_   = CAL_WAIT_STILL;
    calStartMs_ = millis();
    calStillMs_ = calStartMs_;
    calN_ = 0;
    calGSum_[0] = calGSum_[1] = calGSum_[2] = 0;
    for (int i = 0; i < 3; i++) { calGMin_[i] = 1e9f; calGMax_[i] = -1e9f; }

    startBaroCal();
    zeroAttitude(true);          /* immediate — this pose is home, now */

    Serial.printf("// [CAL] started: attitude datum NOW, baro %lu ms, "
                  "gyro when still (<%.1f dps spread, %lu ms timeout)\n",
                  (unsigned long)BARO_CAL_MS, (double)CAL_STILL_DPS,
                  (unsigned long)CAL_TIMEOUT_MS);
}

void SensorHub::startMagCal() {
    magCalActive_ = true; magCalN_ = 0;
    for (int i = 0; i < 3; i++) { magLo_[i] = 1e9f; magHi_[i] = -1e9f; }
}

void SensorHub::finishMagCal() {
    magCalActive_ = false;
    /* Hard-iron bias is the midpoint of min/max on each axis. Reject a capture
     * that never covered enough of the sphere — a partial rotation produces a
     * bias that is worse than none at all. */
    if (magCalN_ < 200 || (magHi_[0] - magLo_[0]) < 5.0f) return;
    for (int i = 0; i < 3; i++) mBias_[i] = (magHi_[i] + magLo_[i]) * 0.5f;
}

/* THE ONE WRITER OF THE PAD REFERENCE. Every path that wants to change the
 * altitude datum — EEPROM restore, boot survey, CMD_CALIBRATE — comes through
 * here, so there is no way for a second copy to exist and drift. The range
 * check means a corrupt EEPROM word leaves the previous reference standing
 * rather than silently becoming the altitude datum for the whole flight. */
void SensorHub::setPadReference(float pa, float tC) {
    if (pa > BARO_MIN_PA && pa < BARO_MAX_PA) {
        d_.padPresPa = pa;
        /* Temperature is validated separately: a good pressure with a bad
         * temperature is still worth having — altitudeAgl() just falls back to
         * ISA for T0 rather than discarding the pressure too. */
        if (tC > BARO_T0_MIN_C && tC < BARO_T0_MAX_C) d_.padTempC = tC;
    }
}

void SensorHub::restoreCalibration(float padPa, float padTc,
                                   float gbx, float gby, float gbz,
                                   float mbx, float mby, float mbz) {
    setPadReference(padPa, padTc);
    if (fabsf(gbx) < 50) gBias_[0] = gbx;
    if (fabsf(gby) < 50) gBias_[1] = gby;
    if (fabsf(gbz) < 50) gBias_[2] = gbz;
    if (fabsf(mbx) < 2000) mBias_[0] = mbx;
    if (fabsf(mby) < 2000) mBias_[1] = mby;
    if (fabsf(mbz) < 2000) mBias_[2] = mbz;
}

void SensorHub::exportCalibration(float& padPa, float& padTc,
                                  float& gbx, float& gby, float& gbz,
                                  float& mbx, float& mby, float& mbz) const {
    padPa = d_.padPresPa;
    padTc = d_.padTempC;
    gbx = gBias_[0]; gby = gBias_[1]; gbz = gBias_[2];
    mbx = mBias_[0]; mby = mBias_[1]; mbz = mBias_[2];
}

/* ============================================================================
 * NMEA — self-contained, byte-at-a-time, no external parser library
 * ============================================================================
 * Written in-house rather than using TinyGPSPlus for two reasons that matter
 * here: TinyGPSPlus does not parse $GSV at all, so per-satellite SNR and the
 * NavIC ($GIGSV) count are simply unavailable through it; and a flight parser
 * should have no third-party behaviour between the receiver and the log.
 * SNR is the measurement that distinguishes "few satellites because the RF
 * path is weak" from "few satellites because the sky is obstructed" — without
 * it, a low satellite count is uninterpretable.
 * ========================================================================= */
void SensorHub::gnssPump() {
    /* Hard 2 ms budget per call. The barometer must keep its 25 Hz cadence and
     * the radio must hit its TX slot; draining a full NMEA burst inline is
     * what previously starved both. Leftover bytes stay in the UART FIFO and
     * are picked up on the next pass. */
    uint32_t t0 = millis();
    while (gnss_->available() && (millis() - t0 < 2)) {
        char c = (char)gnss_->read();
        if (c == '$') { nmeaLen_ = 0; nmea_[nmeaLen_++] = c; continue; }
        if (nmeaLen_ == 0) continue;
        if (c == '\r' || c == '\n') {
            nmea_[nmeaLen_] = '\0';
            nmeaLine(nmea_);
            nmeaLen_ = 0;
            continue;
        }
        if (nmeaLen_ < sizeof(nmea_) - 1) nmea_[nmeaLen_++] = c;
        else nmeaLen_ = 0;                      /* overlong line — discard    */
    }
}

/* NMEA ddmm.mmmm -> decimal degrees. */
static double nmeaCoord(const char* v, const char* hemi) {
    if (!v || !*v) return 0;
    double raw = atof(v);
    double deg = floor(raw / 100.0);
    double dd  = deg + (raw - deg * 100.0) / 60.0;
    if (hemi && (*hemi == 'S' || *hemi == 'W')) dd = -dd;
    return dd;
}

void SensorHub::nmeaLine(char* s) {
    /* "$" + 2-char talker + 3-char type + "*" + 2 hex is the shortest legal
     * sentence header; anything shorter cannot be indexed safely below. */
    if (strlen(s) < 9) return;
    /* Validate the checksum before anything is believed. */
    char* star = strchr(s, '*');
    if (!star || strlen(star) < 3) return;
    uint8_t want = (uint8_t)strtoul(star + 1, nullptr, 16);
    uint8_t got = 0;
    for (char* p = s + 1; p < star; p++) got ^= (uint8_t)*p;
    if (got != want) return;
    *star = '\0';

    char* f[24];
    int n = 0;
    f[n++] = s;
    for (char* p = s; *p && n < 24; p++)
        if (*p == ',') { *p = '\0'; f[n++] = p + 1; }
    if (n < 2) return;

    /* ---- Airoha PAIR acknowledgements ------------------------------------
     * "$PAIR001,<cmd>,<result>" — result 0 = accepted. These are NOT standard
     * NMEA: the "talker" is four characters (PAIR) followed by a three-digit
     * message number, so the 2-character-talker indexing below would slice them
     * wrongly. Handled first and returned from.
     *
     * This is the only direct evidence that the receiver accepted the NavIC
     * command. Without it, a rejected $PAIR066 is indistinguishable from an
     * accepted one until someone notices NAVIC_SATS sitting at 0 — which is
     * precisely how the $PMTK353 parameter-order bug survived so long. */
    if (!strncmp(f[0], "$PAIR", 5)) {
        if (!strcmp(f[0] + 5, "001") && n >= 3) {
            const int res = atoi(f[2]);
            Serial.printf("// [GNSS] PAIR%s %s%s\n", f[1],
                          res == 0 ? "accepted" : "REJECTED code=",
                          res == 0 ? "" : f[2]);
            if (res == 0 && !strcmp(f[1], "066"))
                Serial.println(F("// [GNSS]   -> NavIC search enabled "
                                 "(needs an L1+L5 antenna to actually see it)"));
        }
        return;
    }

    const char* tag = f[0] + 3;          /* skip '$' + 2-char talker          */
    const char  t0  = f[0][1], t1 = f[0][2];
    char talker[3]  = { t0, t1, '\0' };

    if      (!strcmp(tag, "GGA")) nmeaGGA(f, n);
    else if (!strcmp(tag, "RMC")) nmeaRMC(f, n);
    else if (!strcmp(tag, "GSA")) nmeaGSA(f, n);
    else if (!strcmp(tag, "GSV")) nmeaGSV(f, n, talker);
}

void SensorHub::nmeaGGA(char** f, int n) {
    if (n < 12) return;
    uint8_t q = (uint8_t)atoi(f[6]);
    d_.fixQuality = q;
    d_.sats = (uint8_t)atoi(f[7]);
    if (f[8][0]) d_.hdop = atof(f[8]);
    if (f[11][0]) d_.geoidSepM = atof(f[11]);

    if (q > 0 && f[2][0] && f[4][0]) {
        d_.lat = nmeaCoord(f[2], f[3]);
        d_.lon = nmeaCoord(f[4], f[5]);
        if (f[9][0]) d_.gpsAltM = atof(f[9]);
        /* hhmmss.ss -> hhmmsscc as an integer, so the wire format needs no
         * float and the GCS reconstructs the exact same value. */
        d_.gnssTime = (uint32_t)(atof(f[1]) * 100.0 + 0.5);

        uint32_t now = millis();
        if (d_.gnssFix && d_.lastFixMs) {
            uint32_t gap = now - d_.lastFixMs;
            /* 1.5x the configured 1 Hz interval. Anything longer is a real
             * loss of continuity, not jitter — this is the number that turns
             * "does it hold lock while moving" into something measurable. */
            if (gap > 1500) {
                d_.fixDropouts++;
                if (gap > d_.worstGapMs) d_.worstGapMs = gap;
            }
        }
        /* TIME TO FIRST FIX. Latched once per boot, from power-on to the first
         * GGA carrying a real solution. This is the number that says whether
         * the aiding chain in DataLogger is actually earning its keep — an
         * unaided cold start is 30-60 s, an aided warm start 15-30 s. Without
         * measuring it, "it feels faster" is all anyone can say. */
        if (d_.ttffMs == 0) {
            d_.ttffMs = now;
            Serial.printf("// [GNSS] FIRST FIX at %lu.%lu s  (%s, %u sats, "
                          "HDOP %.1f)\n",
                          (unsigned long)(now / 1000), (unsigned long)((now % 1000) / 100),
                          d_.aidInjected ? "AIDED warm start" : "unaided cold start",
                          d_.sats, d_.hdop);
        }
        d_.lastFixMs = now;
        d_.gnssFix = true;
    } else if (q == 0) {
        d_.gnssFix = false;
    }
}

void SensorHub::nmeaRMC(char** f, int n) {
    if (n < 8) return;
    if (f[7][0]) d_.gndSpeedMs = (float)(atof(f[7]) * 0.514444);   /* kn->m/s */
    /* Field 9 is DDMMYY — the only source of UTC date available from NMEA
     * without a battery-backed RTC. Parse it so saveGPSStateToSD can record a
     * valid year even on prototype hardware that has no RTC battery. */
    if (n >= 10 && f[9] && strlen(f[9]) >= 6) {
        d_.gnssDay  = (uint8_t)((f[9][0]-'0')*10 + (f[9][1]-'0'));
        d_.gnssMon  = (uint8_t)((f[9][2]-'0')*10 + (f[9][3]-'0'));
        d_.gnssYear = (uint16_t)(2000 + (f[9][4]-'0')*10 + (f[9][5]-'0'));
    }
}

void SensorHub::nmeaGSA(char** f, int n) {
    if (n < 18) return;
    if (f[17][0]) d_.vdop = atof(f[17]);
}

void SensorHub::nmeaGSV(char** f, int n, const char* talker) {
    if (n < 4) return;
    int msgNum = atoi(f[2]);
    int inView = atoi(f[3]);

    uint8_t bit = 0;
    if      (!strcmp(talker, "GP")) bit = 0x01;
    else if (!strcmp(talker, "GL")) bit = 0x02;
    else if (!strcmp(talker, "GA")) bit = 0x04;
    else if (!strcmp(talker, "GB") || !strcmp(talker, "BD")) bit = 0x08;
    else if (!strcmp(talker, "GI")) bit = 0x10;      /* NavIC / IRNSS         */

    /* CYCLE COMMIT IS ANCHORED ON TIME, NOT ON $GPGSV BEING PRESENT.
     *
     * This used to commit only when a GPS GSV group started (bit == 0x01).
     * That silently made the GPS constellation a prerequisite for reporting any
     * other one: with GPS GSV absent — indoors, under partial sky, or simply
     * with no GPS satellites in view while NavIC geostationaries are — the
     * accumulators were never committed and NAVIC_SATS, GNSS_SYS and BEST_SNR
     * froze at whatever they last held, or at 0 forever from boot. The one
     * constellation this mission most wants to measure was the one that could
     * not be reported without the constellation it was meant to supplement.
     *
     * A burst of NMEA arrives once per second and takes 400-700 ms to clock out
     * at 9600 baud, so the first "message 1 of a group" seen more than 500 ms
     * after the last commit is the start of a genuinely new cycle, whichever
     * talker it belongs to. Groups within the same burst arrive well inside
     * that and correctly do not re-commit. */
    const uint32_t nowMs = millis();
    if (msgNum == 1 && (nowMs - gsvCycleMs_) > 500) {
        d_.gnssSysMask = gsvMaskAccum_;
        d_.navicSats   = navicAccum_;
        d_.bestSnr     = bestSnrAccum_;
        gsvMaskAccum_ = 0; navicAccum_ = 0; bestSnrAccum_ = 0;
        gsvCycleMs_ = nowMs;
    }
    /* <in view> is repeated in every sentence of a group, so this does not need
     * to be gated on msgNum == 1 — and gating it there meant a NavIC group
     * whose first sentence was dropped by a UART overrun reported zero
     * satellites for that whole cycle. */
    if (bit == 0x10 && inView > 0) navicAccum_ = (uint8_t)inView;
    if (inView > 0) gsvMaskAccum_ |= bit;

    /* Per-satellite SNR, 4 satellites per sentence: id, elev, azim, snr. */
    for (int i = 4; i + 3 < n; i += 4) {
        if (!f[i + 3][0]) continue;
        int snr = atoi(f[i + 3]);
        if (snr > 0 && snr < 100 && snr > bestSnrAccum_)
            bestSnrAccum_ = (uint8_t)snr;
    }
}
