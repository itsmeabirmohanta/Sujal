/* =============================================================================
 * Sensors.h — BMP585 + ICM-20948 + AK09916 + DS3231 + L89 GNSS + bus sense
 * =============================================================================
 * One owner for everything on the I2C bus and everything on Serial2.
 * (The GNSS moved Serial1 -> Serial2 at harness rev 2026-08; the port is
 * reached only through GNSS_PORT in Config.h, never named directly here.)
 *
 * CONTRACT
 *   begin()    arms a non-blocking init state machine and returns immediately.
 *   service()  must be called every loop pass. It advances init, pumps NMEA,
 *              samples whatever sensor is due, and runs bus recovery. It never
 *              blocks and never calls delay().
 *   ready()    true once every fitted device has been probed (fitted or not).
 *   data()     the current conditioned snapshot.
 *
 * Nothing here uses a vendor sensor library. Every register is written
 * explicitly and validated by reading back a physically plausible value —
 * a register write ACKing proves the bus transaction worked, not that the
 * resulting device behaviour is correct (SENSOR_TESTING_FINDINGS §8.1).
 * ========================================================================== */
#pragma once
#include <Arduino.h>
#include "Config.h"

struct SensorData {
    /* ---- barometer ---------------------------------------------------- */
    bool     baroOk      = false;
    float    presPa      = 0;      /* conditioned: median-5 then single pole */
    float    tempC       = 0;
    float    altM        = 0;      /* AGL, relative to the pad reference     */
    float    altAslM     = 0;      /* ISA altitude above mean sea level      */
    float    vspeedMs    = 0;      /* +up, from baro differentiation         */

    /* THE PAD REFERENCE. ONE VARIABLE. THIS ONE.
     *
     * There used to be two, and they were never connected to each other:
     * padPresPa here (persisted to EEPROM, written by the calibration path)
     * and a file-scope float groundPressurePa in Sensors.cpp (read by
     * getAltitudeAGL(), which is what actually produces altM).
     *
     * restoreCalibration() on boot wrote padPresPa and nothing else. So after
     * EVERY reboot the altitude pipeline silently fell back to
     * groundPressurePa's initialiser of 101325.0 Pa — standard sea level —
     * while the correct, surveyed pad pressure sat in the struct unused. At the
     * logged 97.3 kPa pad pressure that is a constant ~340 m of error, and it
     * is the direct cause of the ground station reporting ALTITUDE AGL as
     * -10.4 m on a vehicle sitting on the ground and BARO vs GNSS dALT as
     * 263.7 m.
     *
     * Two variables holding one fact will always eventually disagree. There is
     * now exactly one, it is this, and setPadReference() is its only writer. */
    float    padPresPa   = 0;      /* pad reference, persisted in EEPROM     */
    /* Air temperature captured at the same instant as padPresPa. Used as T0 in
     * the hypsometric formula instead of the ISA 15 C assumption baked into the
     * usual 44330.8 constant — see the BAROMETER block in Config.h for why that
     * constant was costing ~4.8% of every altitude. */
    float    padTempC    = 0;

    /* ---- inertial ------------------------------------------------------ */
    bool     imuOk       = false;
    float    ax = 0, ay = 0, az = 0;     /* g,  bias/scale corrected         */
    float    gx = 0, gy = 0, gz = 0;     /* deg/s, bias corrected + deadband */
    bool     magOk       = false;
    float    mx = 0, my = 0, mz = 0;     /* uT, hard-iron corrected          */
    float    spinDps     = 0;            /* |gz| — long-axis spin rate       */
    float    accMagG     = 0;            /* |a|, for jerk/launch detection   */

    /* FUSED ATTITUDE, degrees. Computed on the flight computer at the 50 Hz
     * sensor tick, NOT derived on the ground from 1 Hz samples — see the
     * complementary filter in Sensors.cpp for why that matters. */
    /* Published attitude, datum-subtracted and sign-corrected.
     * ROLL / PITCH come from the accelerometer via atan2 — anchored to
     * gravity, cannot drift. YAW is gyro-Z integrated from zero and drifts
     * at whatever the gyro bias is; CMD_CALIBRATE resets it to zero. */
    float    rollDeg    = 0;             /* -180..+180 */
    float    pitchDeg   = 0;             /*  -90..+90  */
    float    yawDeg     = 0;             /*    0..360  */

    /* FULL-ORIENTATION QUATERNION, for the ground station's 3-D model only.
     * See "QUATERNION ATTITUDE" in Sensors.cpp for why this runs ALONGSIDE
     * the atan2 estimator above rather than replacing it — the two feed
     * different consumers (numeric panels vs. the tumbling 3-D render) and
     * Config.h's ATTITUDE ESTIMATOR section documents real bench failures
     * from using a quaternion for the numeric display. This one never is.
     * Datum-subtracted the same way as rollDeg/pitchDeg/yawDeg: 0,0,0,1 at
     * whatever pose the vehicle booted in (or was last CMD_CALIBRATE'd to).
     * W is reconstructed on the ground, never transmitted — see B36_QUAT. */
    bool     attQuatOk  = false;
    float    qw = 1, qx = 0, qy = 0, qz = 0;

    /* ---- GNSS ---------------------------------------------------------- */
    bool     gnssFix     = false;
    double   lat         = 0;
    double   lon         = 0;
    float    gpsAltM     = 0;
    float    gndSpeedMs  = 0;
    float    hdop        = 0;
    float    vdop        = 0;
    float    geoidSepM   = 0;
    uint32_t gnssTime    = 0;      /* hhmmsscc as an integer                 */
    uint8_t  sats        = 0;      /* used in the solution (GGA)             */
    uint8_t  navicSats   = 0;      /* in view, from $GIGSV                   */
    uint8_t  fixQuality  = 0;      /* GGA field 6                            */
    uint8_t  gnssSysMask = 0;      /* bit0 GPS 1 GLO 2 GAL 3 BDS 4 NavIC     */
    uint8_t  bestSnr     = 0;      /* dB-Hz, best satellite this cycle       */
    uint32_t lastFixMs   = 0;
    uint32_t fixDropouts = 0;
    uint32_t worstGapMs  = 0;
    uint8_t  gnssDay     = 0;      /* from $GPRMC field 9, valid when > 0    */
    uint8_t  gnssMon     = 0;
    uint16_t gnssYear    = 0;

    /* TIME TO FIRST FIX, milliseconds from boot. The single number that says
     * whether the aiding chain is working: unaided cold starts land at
     * 30-60 s, an aided warm start at 15-30 s. Printed on the status line and
     * logged, so an improvement is measured rather than believed. 0 = no fix
     * yet this boot. */
    uint32_t ttffMs      = 0;
    bool     aidInjected = false;  /* stored position was fed to the receiver */
    bool     rtcSynced   = false;  /* DS3231 has been set from GNSS this boot */

    /* ---- RTC ----------------------------------------------------------- */
    bool     rtcOk       = false;
    uint16_t utcYear = 0;
    uint8_t  utcMon = 0, utcDay = 0, utcHour = 0, utcMin = 0, utcSec = 0;
    uint32_t utcEpoch    = 0;      /* seconds since 2000-01-01T00:00:00Z     */
    float    rtcTempC    = 0;

    /* ---- power --------------------------------------------------------- */
    float    busV        = 0;
    float    socPct      = 0;

    /* ---- health -------------------------------------------------------- */
    uint16_t i2cFaults   = 0;      /* cumulative failed transactions         */
    uint16_t i2cRecovers = 0;      /* cumulative bus-recovery events         */
};

class SensorHub {
public:
    void begin();
    void service();                       /* call every loop — never blocks  */
    bool ready() const { return initDone_; }
    const SensorData& data() const { return d_; }

    /* FULL PAD CALIBRATION — guideline §6.1.vi (zero gyro, barometric altitude
     * and accelerometer on command from the ground station).
     *
     * Arms THREE things at once and returns immediately; none of them block:
     *   - gyro bias + accel scale, gated on the platform actually being still
     *     (a calibration that averages a fixed sample count without checking
     *     stillness bakes motion straight into the bias constant, findings §8.5);
     *   - the barometric pad reference, averaged over BARO_CAL_MS of CONDITIONED
     *     pressure on the sensor's own 25 Hz tick;
     *   - the attitude datum, captured immediately (see zeroAttitude).
     *
     * Progress and the final result are printed. The previous implementation
     * could fail to converge and said nothing at all — see the CMD_CALIBRATE
     * GATING block in Config.h for the CAL_STILL_DPS bug that caused it. */
    void  startCalibration();
    /* Barometric pad survey alone, without the gyro stillness gate. Used by the
     * boot path when no reference was restored from EEPROM. */
    void  startBaroCal();
    bool  calibrating() const { return calState_ != CAL_IDLE || baroCalActive_; }

    /* Hard-iron magnetometer calibration: run, rotate the CanSat through two
     * full planes, then call finishMagCal(). Also non-blocking. */
    void  startMagCal();
    void  finishMagCal();

    /* THE ONLY WRITER OF THE PAD REFERENCE. Used to restore it from EEPROM on
     * reboot and to commit a fresh survey at the end of a calibration. Passing
     * an out-of-range pressure is ignored, so a corrupt EEPROM record cannot
     * poison the altitude pipeline. */
    void  setPadReference(float pa, float tC);
    float padReference() const { return d_.padPresPa; }
    /* AGL from a conditioned pressure, referenced to the pad survey and
     * temperature-compensated. Replaces the free function that read a second,
     * disconnected copy of the pad pressure — see SensorData::padPresPa. */
    float altitudeAgl(float presPa) const;

    void  gnssHardReset();
    void  sendPMTKCommand(String payload);

    /* ---- clock ---------------------------------------------------------- *
     * The DS3231 is the only clock that survives a power cycle (own coin
     * cell, ±2 ppm), which makes it the anchor of the fast-relock chain — see
     * the TIMEKEEPING block in Config.h. Everything here is UTC. */

    /* Write the DS3231. Returns false if the bus transaction failed or the
     * arguments are not a plausible date. */
    bool  rtcSetUtc(uint16_t y, uint8_t mo, uint8_t d,
                    uint8_t h, uint8_t mi, uint8_t s);

    /* Is the RTC reading a date we are prepared to believe? A dead coin cell
     * reads 2000-01-01 or 2165; a torn I2C read can produce anything. Aiding a
     * receiver with a wrong time is worse than aiding it with none. */
    bool  rtcValid() const {
        return d_.rtcOk && d_.utcYear >= RTC_MIN_YEAR && d_.utcYear <= RTC_MAX_YEAR
            && d_.utcMon >= 1 && d_.utcMon <= 12 && d_.utcDay >= 1 && d_.utcDay <= 31;
    }

    /* If the RTC is not believable, set it from the compile-time stamp so it
     * is at least in the right minute. Called once at boot. Does nothing when
     * the clock is already sane, so a good RTC is never dragged backwards to
     * whenever the firmware happened to be built. */
    void  seedRtcFromBuildTime();

    /* Local wall-clock components derived from the stored UTC. Display and the
     * SD CSV only — nothing is ever STORED in local time. */
    void  localTime(uint16_t& y, uint8_t& mo, uint8_t& d,
                    uint8_t& h, uint8_t& mi, uint8_t& s) const;

    /* DataLogger tells us whether it managed to feed the receiver a stored
     * position, so the FIRST FIX line can report whether the TTFF it just
     * measured was an aided one. Without this the headline number is
     * uninterpretable — a fast fix under clear sky and a fast fix from good
     * aiding look identical. */
    void  markAidInjected(bool yes) { d_.aidInjected = yes; }

    /* Re-capture the attitude datum: the vehicle's current pose becomes the
     * new 0,0,0.
     *
     * immediate = false  (boot path)  wait ATT_ZERO_DELAY_MS for the filter to
     *   settle onto gravity before locking the reference. At t=0 roll and pitch
     *   come from essentially one accelerometer sample and zeroing then bakes
     *   that noise in permanently.
     *
     * immediate = true   (CMD_CALIBRATE)  capture on the very next IMU sample.
     *   The operator has deliberately placed the vehicle in this pose and the
     *   filter has already been converged for as long as the vehicle has been
     *   powered, so there is nothing left to settle. Waiting three seconds here
     *   is pure latency and is what made the command look like it was being
     *   ignored. See the ATTITUDE DATUM block in Config.h. */
    void  zeroAttitude(bool immediate = false) {
        attZeroed_    = false;
        attFirstMs_   = 0;
        attYaw_       = 0.0f;
        attZeroNowReq_ = immediate;
    }

    /* Which UART the receiver was actually found on, for the boot banner.
     * "none" means neither port produced a byte — see IS_GNSS_DETECT. */
    bool bmpRead(float& tC, float& pPa);
    const char* gnssPortName() const {
        if (!gnssLocked_) return "none";
        return (gnss_ == &GNSS_PORT_A) ? "Serial2" : "Serial1";
    }

    /* Values the flight computer persists across a brownout. padTc travels with
     * padPa because the two are only meaningful together: the pad temperature
     * is the T0 of the hypsometric formula that turns padPa into an altitude,
     * so restoring one without the other reintroduces the ISA-15 C error the
     * compensation exists to remove. */
    void  restoreCalibration(float padPa, float padTc,
                             float gbx, float gby, float gbz,
                             float mbx, float mby, float mbz);
    void  exportCalibration(float& padPa, float& padTc,
                             float& gbx, float& gby, float& gbz,
                             float& mbx, float& mby, float& mbz) const;

private:
    /* ---- I2C primitives with fault accounting ------------------------- */
    bool wr8(uint8_t addr, uint8_t reg, uint8_t val);
    bool rdN(uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t len);
    bool rd8(uint8_t addr, uint8_t reg, uint8_t& val);
    void noteFault();
    void noteOk();
    static void busRecover();             /* 9 SCL pulses, SDA released      */

    /* ---- device helpers ------------------------------------------------ */
    bool icmBank(uint8_t bank);
    bool icmRead();
    bool magRead();
    void rtcRead();
    void readBus();

    /* ---- staged, non-blocking init ------------------------------------- */
    enum InitStage : uint8_t {
        IS_RECOVER = 0, IS_WIRE, IS_BMP_PROBE, IS_BMP_RESET, IS_BMP_STANDBY,
        IS_BMP_CONFIG, IS_BMP_VERIFY, IS_ICM_PROBE, IS_ICM_RESET, IS_ICM_WAKE,
        IS_ICM_CONFIG, IS_ICM_VERIFY, IS_MAG_BYPASS, IS_MAG_RESET, IS_MAG_MODE,
        IS_MAG_VERIFY, IS_RTC, IS_GNSS_PORT, IS_GNSS_DETECT, IS_GNSS_CFG,
        IS_DONE
    };
    InitStage stage_    = IS_RECOVER;
    uint32_t  stageAt_  = 0;
    uint8_t   cfgLine_  = 0;
    bool      initDone_ = false;

    /* ---- attitude state ------------------------------------------------- */
    uint32_t attLastUs_    = 0;           /* micros() of the last pass */
    float    attYaw_       = 0;           /* gyro-integrated, deg, 0..360 */
    float    attRefRoll_   = 0;           /* accel roll captured at datum time */
    float    attRefPitch_  = 0;           /* accel pitch captured at datum time */
    bool     attZeroed_    = false;
    uint32_t attFirstMs_   = 0;           /* when the first sample arrived */
    bool     attZeroNowReq_ = false;      /* CMD_CALIBRATE: skip the settle wait */

    /* ---- quaternion attitude state (3-D model feed only) ---------------- */
    float    qFreeW_ = 1, qFreeX_ = 0, qFreeY_ = 0, qFreeZ_ = 0; /* filter's
                       own running estimate, undatumed */
    float    qRefW_  = 1, qRefX_  = 0, qRefY_  = 0, qRefZ_  = 0; /* datum,
                       captured the same moment attZeroed_ latches */
    void     quatUpdate(float ax, float ay, float az,
                        float gxDps, float gyDps, float gzDps, float dt);

    /* ---- GNSS port auto-detection --------------------------------------- */
    /* Defaults to the rev 2026-08 port so that if detection is ever skipped
     * the behaviour matches the documented harness rather than nothing. */
    HardwareSerial* gnss_       = &GNSS_PORT_A;
    bool            gnssLocked_ = false;

    /* ---- runtime ------------------------------------------------------- */
    SensorData d_;
    uint8_t  bmpAddr_ = BMP_ADDR_A;
    uint8_t  icmAddr_ = ICM_ADDR_A;
    uint8_t  faultRun_ = 0;

    uint32_t nextBaroMs_ = 0, nextImuMs_ = 0, nextRtcMs_ = 0, nextBusMs_ = 0;

    /* barometer conditioning */
    float    medBuf_[BARO_MEDIAN_N];
    uint8_t  medN_ = 0, medIdx_ = 0;
    float    presFilt_ = 0;
    bool     presInit_ = false;
    float    lastAlt_ = 0;
    uint32_t lastAltMs_ = 0;

    /* imu correction state */
    float    gBias_[3] = { GYRO_BIAS_X_DPS, GYRO_BIAS_Y_DPS, GYRO_BIAS_Z_DPS };
    float    mBias_[3] = { 0, 0, 0 };
    float    accScale_ = ACCEL_SCALE_CORR;
    uint8_t  icmFailRun_ = 0;

    /* stillness-gated calibration */
    enum CalState : uint8_t { CAL_IDLE = 0, CAL_WAIT_STILL, CAL_ACCUM };
    CalState calState_ = CAL_IDLE;
    uint32_t calStartMs_ = 0, calStillMs_ = 0;
    double   calGSum_[3] = { 0, 0, 0 };
    uint32_t calN_ = 0;
    float    calGMin_[3], calGMax_[3];

    /* Barometric pad survey — runs independently of the stillness gate.
     * Deliberately NOT gated on stillness: pressure does not care whether the
     * vehicle is rotating, and coupling the two meant a failed gyro gate also
     * silently threw away a perfectly good pressure average. Accumulates the
     * CONDITIONED (median + glitch-gated) pressure on the barometer's own 25 Hz
     * tick, so a torn I2C read can never reach the reference. */
    bool     baroCalActive_ = false;
    uint32_t baroCalStartMs_ = 0;
    double   baroCalPSum_ = 0, baroCalTSum_ = 0;
    uint32_t baroCalN_ = 0;
    void     baroCalService(uint32_t now);

    /* magnetometer hard-iron capture */
    bool     magCalActive_ = false;
    float    magLo_[3], magHi_[3];
    uint32_t magCalN_ = 0;

    /* ---- NMEA ---------------------------------------------------------- */
    void gnssPump();
    void gnssSend(const char* body);
    void nmeaLine(char* s);
    void nmeaGGA(char** f, int n);
    void nmeaRMC(char** f, int n);
    void nmeaGSA(char** f, int n);
    void nmeaGSV(char** f, int n, const char* talker);

    char     nmea_[100];
    uint8_t  nmeaLen_ = 0;
    uint8_t  gsvMaskAccum_ = 0;
    uint8_t  navicAccum_ = 0;
    uint8_t  bestSnrAccum_ = 0;
    uint32_t gsvCycleMs_ = 0;      /* start of the current GSV burst, ms */
};

extern SensorHub Sensors;

/* REMOVED, DELIBERATELY — do not add these back:
 *     extern float groundPressurePa;
 *     void  calibrateBarometer();
 *     float getAltitudeAGL(float currentPressurePa);
 *
 * They were a second, file-scope copy of the pad reference that the persisted
 * one was never written into, so every reboot silently reverted the altitude
 * datum to 101325 Pa. Their replacements are members of SensorHub —
 * startCalibration(), setPadReference() and altitudeAgl() — which means the
 * reference has exactly one owner and cannot drift out of step with the copy
 * that goes to EEPROM. See SensorData::padPresPa for the full failure. */
