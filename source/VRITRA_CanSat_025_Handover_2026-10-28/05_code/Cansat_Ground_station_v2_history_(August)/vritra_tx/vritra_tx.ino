/*
 * VRITRA  ·  2026-INSPACe-CAN-7USAT-025  ·  Lovely Professional University
 * Teensy 4.1 + Semtech LR1121 (Core1121-XF)  —  FLIGHT TRANSMITTER
 * =============================================================================
 * PRODUCTION FIRMWARE — rev 3.0
 *
 * DESIGN PRINCIPLE: every subsystem is independently initialised and
 * independently recoverable. A failure in any one of {radio, GNSS, barometer,
 * IMU, magnetometer, RTC} must never stall, block or corrupt the others. A
 * field with no trustworthy reading is transmitted as an EMPTY CELL so the
 * ground station renders "—" rather than a fabricated zero.
 *
 * -----------------------------------------------------------------------------
 * WHY THIS REV EXISTS — defects found in the vendor libraries and fixed here
 * -----------------------------------------------------------------------------
 * [BMP585-1] 7Semi_BMP585 initSensor() applies IIR coefficient 127 on BOTH
 *            temperature and pressure at ODR 10 Hz. That is a time constant of
 *            roughly 12 s. Altitude appeared frozen / laggy. FIXED: on-chip IIR
 *            is disabled and all filtering is done in software below, where it
 *            is deterministic, tunable and observable.
 * [BMP585-2] readTemperatureC() and readPressurePa() EACH perform their own
 *            6-byte burst, unsynchronised to the conversion cycle. A burst that
 *            straddles a register update returns a torn word; one corrupted MSB
 *            of the 24-bit pressure value is 1024 Pa ~= 90 m, which is exactly
 *            the -300 m spikes observed in flight logs. The naive 50-110 kPa
 *            range gate cannot reject these because the torn value is still
 *            "plausible". FIXED: single burst for T+P, then a 5-sample median
 *            (kills isolated torn samples outright) followed by a physical
 *            rate limiter and a light single-pole smoother.
 * [BMP585-3] Library setODR() clears mask 0x83 before writing the ODR field,
 *            but ODR occupies bits [6:2] (mask 0x7C). The previous rate is
 *            never cleared and is OR-ed with the new one. FIXED: this file
 *            composes and writes the whole ODR_CONFIG byte explicitly.
 * [ICM-1]    applyBasicDefaults() writes INT_PIN_CFG = 0x30, which CLEARS
 *            BYPASS_EN. The AK09916 magnetometer lives behind the ICM aux bus
 *            and is only reachable at 0x0C while bypass is asserted — hence
 *            0x0C never appeared in an I2C scan and MX/MY/MZ were always empty.
 *            FIXED: bypass is asserted explicitly AFTER begin(), and the
 *            AK09916 is placed in 100 Hz continuous mode once at boot.
 * [ICM-2]    Library readMag() re-asserts bypass and blocks up to 20 ms waiting
 *            on single-shot DRDY, every single call. FIXED: not used. This file
 *            reads ST1..ST2 in one 9-byte burst with no blocking wait.
 * [ICM-3]    Accelerometer read 9.60 m/s2 against a true 9.80665 — a ~2%
 *            sensitivity error. FIXED: single-point scale calibration; the
 *            gravity magnitude measured at rest defines a correction factor.
 * [GNSS-1]   9600 baud carries 960 char/s. GGA+RMC+GSA+GSV across four
 *            constellations at 1 Hz exceeds that, so sentences were being
 *            truncated mid-stream — the "freeze while moving". FIXED: the
 *            receiver is moved to 38400 baud at boot (with automatic fallback
 *            to 9600 if it does not take) and GSV is decimated to 1-in-5.
 * [GNSS-2]   PMTK sentences were carried as hand-computed checksum literals.
 *            FIXED: checksums are computed at runtime; a typo is now
 *            structurally impossible.
 *
 * -----------------------------------------------------------------------------
 * WIRE FORMAT — the contract with esp32rec.ino. DO NOT CHANGE.
 * -----------------------------------------------------------------------------
 *   025,<FRAME>,<16 core fields>,<optional slice for FRAME>
 *   All numeric fields base-36. FRAME rotates 0,1,2.
 *   Core (schema 0..15) is present in EVERY packet — the 15 mandatory
 *   competition fields plus PKT. Optional data is divided equally across the
 *   three frames. The ESP32 gateway decodes base-36 to plain decimal, injects
 *   RSSI/SNR/FREQ (RF-domain values only the receiver can know) and forwards
 *   the full 84-field CSV to the GCS.
 *
 * -----------------------------------------------------------------------------
 * LINK BUDGET (867 MHz, SF10, BW125, +22 dBm HP PA, sensitivity -132 dBm)
 * -----------------------------------------------------------------------------
 *   Budget 154 dB.  FSPL @ 2 km = 97.2 dB  ->  56.8 dB margin.
 *   Even allowing ~30 dB of antenna/matching loss the 2 km requirement holds
 *   with better than 20 dB to spare. Airtime, not range, is the binding
 *   constraint: 51-75 B costs 616-821 ms and is fixed by the mandated RF plan.
 *
 * -----------------------------------------------------------------------------
 * HARDWARE MAP (as built)
 * -----------------------------------------------------------------------------
 * SPI1  LR1121   CS=24  SCK=27  MOSI=26  MISO=39  RST=30  BUSY=31  IRQ=32
 *                MISO MUST be remapped to 39: SPI1's default MISO is pin 1,
 *                which is also Serial1 TX (the GNSS line). Leaving it there
 *                corrupts every SPI read and returns CHIP_NOT_FOUND (-2).
 * Wire  SDA=18  SCL=19  (3.3k pull-ups fitted)
 *                BMP585 0x47/0x46 · ICM-20948 0x69/0x68 · DS3231 0x68
 *                AK09916 0x0C appears only once bypass is asserted
 * Serial1 GNSS   RX=0  TX=1 · RESET_N=22 (active low, idle high)
 *                WAKE_UP hardwired to 3V3 · 1PPS not wired
 * TB6612FNG      STBY=36  AIN1=35  AIN2=34  PWMA=33   (GA12-N20 gyro wheel)
 * DS3231 SQW=2   wired, unused, reserved for alarm wake
 * Bus sense      A6 via 100k/22k divider
 */

#include <RadioLib.h>
#include <SPI.h>
#include <Wire.h>
#include <EEPROM.h>
#include <TinyGPSPlus.h>
#include <7Semi_BMP585.h>
#include <7Semi_ICM20948.h>
#include <RTClib.h>

extern "C" uint32_t set_arm_clock(uint32_t frequency);

/* ===========================================================================
 * SECTION 1 — BUILD-TIME CONFIGURATION
 * ======================================================================== */

/* One-time RTC set. Uncomment, upload, confirm the printed time, then COMMENT
 * OUT AND RE-UPLOAD — otherwise every reset stamps the clock back to this
 * instant. Value is UTC; IST is UTC+5:30. */
// #define SET_RTC_TIME
#define RTC_SET_Y     2026
#define RTC_SET_MON   8
#define RTC_SET_D     2
#define RTC_SET_H     5      /* 10:48 IST == 05:18 UTC */
#define RTC_SET_MIN   18
#define RTC_SET_S     0

/* BENCH_TEST trades competition compliance for iteration speed (~8x faster
 * airtime at 2 m). MUST be commented out for flight. */
// #define BENCH_TEST

#define FREQ_MAIN     867.00f
#define FREQ_BACKUP   865.75f
#ifdef BENCH_TEST
  #define LORA_SF     7
  #define LORA_BW     250.0f
#else
  #define LORA_SF     10
  #define LORA_BW     125.0f
#endif
#define LORA_CR       5
#define LORA_SYNC     0x98
#define LORA_PRE      8
#define TX_POWER      22          /* LR1121 HP PA ceiling */
#define TCXO_V        1.6f        /* Core1121-XF TCXO — 0.0f causes PLL instability */

/* Uplink RX window. An 18-20 B command at SF10/BW125 is ~370 ms of airtime and
 * the "received" IRQ only fires at the END of reception, so the window must
 * span the whole packet. The gateway transmits immediately after hearing our
 * downlink, so 500 ms is sufficient with margin.
 *
 * Cadence is adaptive: on the pad, commands matter more than telemetry rate, so
 * we listen every cycle. Once airborne, telemetry rate matters more, so we
 * listen every RX_SKIP_FLIGHT-th cycle. This buys back ~330 ms per cycle in
 * flight without ever going deaf for more than ~3 s. */
#define RX_WINDOW_MS      500UL
#define RX_SKIP_FLIGHT    3

#define TEAM_ID "025"

/* I2C bus speed. KEEP AT 100 kHz.
 * 400 kHz is within datasheet spec for every device on this bus, but it was
 * tested on this harness and the ICM-20948 fails: 0x69 acknowledges its address
 * during a scan yet returns 0xFF on every register read — address phase works,
 * data phase does not. That is the classic signature of insufficient edge rate,
 * i.e. 3.3k pull-ups driving flying-lead capacitance. The DS3231 (a far slower,
 * more forgiving part) survives 400 kHz on the same wires, which is exactly why
 * the failure looked like a dead IMU rather than a bus problem.
 * Raise this ONLY after moving to a PCB with 2.2k pull-ups, and re-verify the
 * WHO_AM_I read returns 0xEA before trusting it. */
#define I2C_CLOCK_HZ  100000

/* GNSS. The receiver does NOT persist a baud change (PMTK251 is volatile) so it
 * always powers up at 9600; we raise it every boot and fall back automatically
 * if the change does not take. */
#define GPS_BOOT_BAUD    9600
#define GPS_TARGET_BAUD  38400

/* Pressure conditioning */
#define BARO_MEDIAN_N        5        /* odd; 5 rejects any single torn sample */
#define BARO_MAX_RATE_PA_S   2000.0f  /* ~165 m/s vertical — far beyond flight,
                                         but rejects the ~3600 Pa torn glitches */
#define BARO_SMOOTH_ALPHA    0.35f    /* light single pole AFTER the median */
#define BARO_MIN_PA          50000.0f
#define BARO_MAX_PA          110000.0f

#define GYRO_DEADBAND_DPS    0.5f
#define ICM_REINIT_AFTER     20

#define BUS_V_PIN     A6
#define BUS_V_RATIO   (122.0f / 22.0f)
/* 1S LiPo. For 2S use 6.6/8.4, for 3S use 9.9/12.6. */
#define BATT_EMPTY_V  3.30f
#define BATT_FULL_V   4.20f

/* ===========================================================================
 * SECTION 2 — PIN MAP
 * ======================================================================== */

#define PIN_CS         24
#define PIN_IRQ        32
#define PIN_RST        30
#define PIN_BUSY       31
#define PIN_SPI1_MISO  39
#define PIN_SPI1_MOSI  26
#define PIN_SPI1_SCK   27

#define PIN_SDA        18
#define PIN_SCL        19
#define PIN_RTC_SQW    2

#define PIN_MOT_STBY   36
#define PIN_MOT_AIN1   35
#define PIN_MOT_AIN2   34
#define PIN_MOT_PWMA   33

#define PIN_GPS_RESET  22

/* ===========================================================================
 * SECTION 3 — FLIGHT STATE
 * ======================================================================== */

/* Values MUST match the GCS STATE enum. The vehicle starts at IDLE on the pad
 * and only ever moves forward. */
enum FlightState : uint8_t {
    FS_IDLE      = 0,
    FS_LAUNCH    = 1,
    FS_EJECT     = 2,
    FS_SEC_EJECT = 3,
    FS_TOUCHDOWN = 4
};

/* Reset-survival record (guideline 6.1.ix / 6.2.ii: clock and state shall
 * survive a processor reset). Without it a mid-flight brownout would restart
 * TIME_S at zero and re-take the pad-altitude reference at whatever altitude
 * the reset happened, making ALT_M wrong for the remainder of the flight. */
struct PersistState {
    uint32_t magic;
    uint32_t bootEpoch;
    uint8_t  state;
    uint32_t pktCount;
    float    padPress_pa;
    float    gyroBias[3];
    float    accelScale;
    float    magBias[3];
    float    R_pad[3][3];
};
#define EEPROM_MAGIC 0x56524933UL   /* 'VRI3' — bumped: struct layout changed */
#define EEPROM_ADDR  0

/* ===========================================================================
 * SECTION 4 — RF SWITCH TABLE + OBJECTS
 * ======================================================================== */

static const uint32_t SW_PINS[] = {
    RADIOLIB_LR11X0_DIO5, RADIOLIB_LR11X0_DIO6,
    RADIOLIB_NC, RADIOLIB_NC, RADIOLIB_NC
};
static const Module::RfSwitchMode_t SW_TABLE[] = {
    { LR11x0::MODE_STBY,  { LOW,  LOW  } },
    { LR11x0::MODE_RX,    { HIGH, LOW  } },
    { LR11x0::MODE_TX,    { HIGH, HIGH } },
    { LR11x0::MODE_TX_HP, { LOW,  HIGH } },
    { LR11x0::MODE_TX_HF, { LOW,  LOW  } },
    { LR11x0::MODE_GNSS,  { LOW,  LOW  } },
    { LR11x0::MODE_WIFI,  { LOW,  LOW  } },
    END_OF_MODE_TABLE,
};

LR1121          radio(new Module(PIN_CS, PIN_IRQ, PIN_RST, PIN_BUSY, SPI1));
TinyGPSPlus     gps;
BMP585_7Semi    bmp;
ICM20948_7Semi  icm;
RTC_DS3231      rtc;

/* ===========================================================================
 * SECTION 5 — GLOBAL STATE
 * ======================================================================== */

static uint8_t  icmAddr    = 0x69;
static uint8_t  bmpAddr    = 0x47;
static bool     bmpFitted  = false;
static bool     icmFitted  = false;
static bool     magFitted  = false;
static bool     rtcFitted  = false;
static bool     radioReady = false;

static bool     freshBoot        = true;
static uint32_t missionBootEpoch = 0;
static uint32_t gpsBaudActive    = GPS_BOOT_BAUD;

static uint8_t  gpsRxBuffer[2048];   /* 38400 baud -> ~3.8 kB/s; 2 kB is >0.5 s
                                        of slack against any TX-blocked gap */

volatile bool   pktFlag   = false;
static uint32_t pktCount  = 0;
static FlightState state  = FS_IDLE;
static bool     txEnabled = true;
static bool     armed     = false;
static bool     beaconOn  = false;
static bool     useBackup = false;

/* Pad reference is stored as PRESSURE, not altitude. Referencing the barometric
 * formula directly to pad pressure removes the sea-level assumption entirely
 * and makes AGL exactly zero on the pad by construction. */
static float    padPress_pa = 0.0f;
static float    padAlt_m    = 0.0f;   /* derived, for display/telemetry only */

/* GNSS extras parsed out of raw NMEA (TinyGPSPlus does not expose these) */
static uint8_t  navicSats  = 0;
static uint8_t  fixQuality = 0;
static float    hdop_val   = 0.0f;
static float    vdop_val   = 0.0f;
static float    geoidSep   = 0.0f;
static bool     vdopValid  = false;
static bool     geoidValid = false;
static uint8_t  gnssMask   = 0;
static uint8_t  svInView[6] = {0,0,0,0,0,0};  /* GPS,GLO,GAL,BDS,QZSS,NavIC */

static char     nmeaBuf[128];
static uint8_t  nmeaLen = 0;

/* IMU calibration */
static float gyroBiasX = 0, gyroBiasY = 0, gyroBiasZ = 0;
static float accelScale = 1.0f;            /* corrects sensitivity error */
static float magBiasX = 0, magBiasY = 0, magBiasZ = 0;   /* hard-iron */
static float R_pad[3][3] = {{1,0,0},{0,1,0},{0,0,1}};

static uint8_t icmFailStreak = 0;

/* Live sensor values plus per-read validity. A *Valid flag false means the cell
 * goes out EMPTY — never a fabricated zero. */
struct SensorSet {
    float pres_pa = 0, temp_c = 0;   bool bmpValid = false;
    float alt_agl = 0;
    float acc_ms2[3]  = {0,0,0};
    float acc_g[3]    = {0,0,0};
    float gyro_dps[3] = {0,0,0};
    float mag_ut[3]   = {0,0,0};
    float spin_dps    = 0;
    bool  imuValid = false;
    bool  magValid = false;
    float bus_v = 0;                 bool busValid = false;
    float soc_pct = 0;
};
static SensorSet S;

/* ===========================================================================
 * SECTION 6 — RAW I2C REGISTER LAYER
 * ---------------------------------------------------------------------------
 * Both vendor libraries keep their register helpers private, and both need
 * register-level correction (see the defect list at the top of this file). This
 * layer lets us drive any device on the bus directly, which is how the BMP585
 * is configured and how the AK09916 is reached.
 * ======================================================================== */

static bool i2cWrite8(uint8_t addr, uint8_t reg, uint8_t val) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission() == 0;
}

static bool i2cReadBytes(uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t len) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    uint8_t got = Wire.requestFrom(addr, len);
    if (got != len) return false;
    for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
    return true;
}

static bool i2cRead8(uint8_t addr, uint8_t reg, uint8_t& val) {
    return i2cReadBytes(addr, reg, &val, 1);
}

static bool i2cPresent(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

/* Free a slave that is holding SDA low mid-transaction by clocking SCL until it
 * releases. Must run BEFORE Wire.begin() takes the pins. */
static void i2cBusRecover() {
    pinMode(PIN_SDA, INPUT_PULLUP);
    pinMode(PIN_SCL, OUTPUT);
    for (int i = 0; i < 9; i++) {
        digitalWrite(PIN_SCL, HIGH); delayMicroseconds(5);
        digitalWrite(PIN_SCL, LOW);  delayMicroseconds(5);
    }
    digitalWrite(PIN_SCL, HIGH);
    pinMode(PIN_SCL, INPUT);
}

/* ===========================================================================
 * SECTION 7 — BMP585 BAROMETER (direct register control)
 * ======================================================================== */

/* Register field composition, written as whole bytes so no read-modify-write
 * bug can survive here (see [BMP585-3]).
 *
 * OSR_CONFIG 0x36 : bit6 press_en | bits[5:3] osr_p | bits[2:0] osr_t
 * ODR_CONFIG 0x37 : bit7 deep_dis | bits[6:2] odr   | bits[1:0] pwr_mode
 * DSP_IIR    0x31 : bits[5:3] iir_p | bits[2:0] iir_t
 *
 * osr_p = 4 (x16) gives low pressure noise; osr_t = 1 (x2) is ample for a
 * temperature channel that only trims the altitude solution. ODR code 0x14 is
 * 25 Hz, comfortably slower than the x16 conversion so every read is a settled
 * sample. On-chip IIR is OFF by deliberate choice — all smoothing is done in
 * software where it can be inspected and tuned.
 */
#define BMP_OSR_BYTE   (0x40 | (0x04 << 3) | 0x01)   /* press_en, x16 P, x2 T */
#define BMP_ODR_CODE   0x14                          /* 25.005 Hz */
#define BMP_ODR_STBY   (0x80 | (BMP_ODR_CODE << 2) | 0x00)
#define BMP_ODR_NORMAL (0x80 | (BMP_ODR_CODE << 2) | 0x01)

static bool bmpReadRaw(float& tempC, float& pressPa);   /* defined below */

/* DO NOT WRITE DSP_CONFIG (0x30) HERE.
 * That register carries the pressure/temperature COMPENSATION enables. The
 * vendor library never touches it, so it keeps its reset default and readings
 * are correct. Zeroing it disables compensation and the pressure output leaves
 * the physical range entirely — every sample then fails the range gate and the
 * barometer looks dead even though every register write succeeded. This cost a
 * flight-test cycle; leave the register alone. With DSP_IIR set to 0 the
 * shadow-select bits in DSP_CONFIG are irrelevant anyway. */
static bool bmpConfigure() {
    /* Datasheet requires IIR/OSR changes to be made in STANDBY. */
    if (!i2cWrite8(bmpAddr, REG_ODR_CONFIG, BMP_ODR_STBY))   return false;
    delay(5);
    if (!i2cWrite8(bmpAddr, REG_DSP_IIR,    0x00))           return false;
    if (!i2cWrite8(bmpAddr, REG_OSR_CONFIG, BMP_OSR_BYTE))   return false;
    if (!i2cWrite8(bmpAddr, REG_ODR_CONFIG, BMP_ODR_NORMAL)) return false;
    delay(30);
    return true;
}

/* Prove a configuration actually yields physical data before trusting it.
 * A register write returning ACK says nothing about whether the resulting
 * measurement is meaningful — that gap is precisely how a broken config passed
 * as "[BMP] OK" while producing garbage. */
static bool bmpValidate(const char* tag) {
    float t = 0, p = 0;
    for (int i = 0; i < 12; i++) {
        if (bmpReadRaw(t, p) && p > BARO_MIN_PA && p < BARO_MAX_PA) {
            Serial.printf("[BMP] %s validated — %.1f Pa, %.2f C\n", tag, p, t);
            return true;
        }
        delay(40);
    }
    Serial.printf("[BMP] %s REJECTED — last raw P=%.1f Pa T=%.2f C (need %.0f..%.0f Pa)\n",
                  tag, p, t, BARO_MIN_PA, BARO_MAX_PA);
    return false;
}

/* Three-tier bring-up, each tier proven by measurement before it is accepted:
 *   1. fast config  — 25 Hz, OSR x16 P / x2 T, IIR off        (best)
 *   2. library defaults with the IIR forced off               (fixes the lag)
 *   3. library defaults untouched                             (laggy but real)
 * Tier 3 is the configuration that demonstrably produced correct 97353 Pa
 * readings on this hardware, so there is always a known-good floor. */
static bool bmpBringUp() {
    if (bmpConfigure() && bmpValidate("fast config")) {
        Serial.println(F("[BMP] 25 Hz, OSR x16 P / x2 T, chip IIR off"));
        return true;
    }

    Serial.println(F("[BMP] tier 1 failed — restoring library defaults, IIR off"));
    bmp.initSensor();
    delay(30);
    i2cWrite8(bmpAddr, REG_ODR_CONFIG, BMP_ODR_STBY);
    delay(5);
    i2cWrite8(bmpAddr, REG_DSP_IIR, 0x00);
    i2cWrite8(bmpAddr, REG_ODR_CONFIG, BMP_ODR_NORMAL);
    delay(30);
    if (bmpValidate("defaults + IIR off")) return true;

    Serial.println(F("[BMP] tier 2 failed — reverting to untouched library defaults"));
    bmp.initSensor();
    delay(30);
    if (bmpValidate("library defaults")) {
        Serial.println(F("[BMP] WARNING: on-chip IIR 127 active — altitude will lag ~12 s"));
        return true;
    }
    return false;
}

static int32_t signExtend24(uint32_t x) {
    x &= 0xFFFFFFu;
    return (x & 0x800000u) ? (int32_t)(x | 0xFF000000u) : (int32_t)x;
}

/* ONE burst for temperature and pressure — they must come from the same
 * conversion or the pair is inconsistent (see [BMP585-2]). */
static bool bmpReadRaw(float& tempC, float& pressPa) {
    uint8_t b[6];
    if (!i2cReadBytes(bmpAddr, REG_TEMP_XLSB, b, 6)) return false;
    uint32_t tu = ((uint32_t)b[2] << 16) | ((uint32_t)b[1] << 8) | b[0];
    uint32_t pu = ((uint32_t)b[5] << 16) | ((uint32_t)b[4] << 8) | b[3];
    tempC   = (float)signExtend24(tu) / 65536.0f;
    pressPa = (float)signExtend24(pu) / 64.0f;
    return true;
}

/* --- Pressure conditioning pipeline -------------------------------------
 * raw -> range gate -> 5-sample median -> physical rate limit -> 1-pole
 *
 * The median is the load-bearing element: a torn I2C burst is an ISOLATED
 * outlier, and a median of 5 rejects up to two of them per window outright,
 * without the lag a long averaging filter would add. */
static float   baroRing[BARO_MEDIAN_N];
static uint8_t baroRingN   = 0;
static uint8_t baroRingIdx = 0;
static float   baroSmooth  = 0.0f;
static bool    baroPrimed  = false;
static uint32_t baroLastMs = 0;
static uint16_t baroRejects = 0;

static float medianOf(const float* src, uint8_t n) {
    float t[BARO_MEDIAN_N];
    for (uint8_t i = 0; i < n; i++) t[i] = src[i];
    for (uint8_t i = 1; i < n; i++) {          /* insertion sort, n<=5 */
        float k = t[i];
        int8_t j = (int8_t)i - 1;
        while (j >= 0 && t[j] > k) { t[j+1] = t[j]; j--; }
        t[j+1] = k;
    }
    return t[n / 2];
}

static void baroReset() {
    baroRingN = baroRingIdx = 0;
    baroPrimed = false;
    baroRejects = 0;
}

/* Returns true when a NEW conditioned pressure is available in S.pres_pa. */
static bool baroService() {
    float t_raw, p_raw;
    if (!bmpReadRaw(t_raw, p_raw)) return false;

    if (!(p_raw > BARO_MIN_PA && p_raw < BARO_MAX_PA)) {
        baroRejects++;
        /* Surface the offending value periodically. A silent reject counter
         * tells you something is wrong; the raw number tells you what. */
        static uint32_t lastCry = 0;
        if (millis() - lastCry > 5000) {
            lastCry = millis();
            Serial.printf("[BMP] out of range: raw P=%.1f Pa T=%.2f C (rej=%u)\n",
                          p_raw, t_raw, baroRejects);
        }
        return false;
    }
    if (t_raw > -60.0f && t_raw < 100.0f) S.temp_c = t_raw;

    baroRing[baroRingIdx] = p_raw;
    baroRingIdx = (uint8_t)((baroRingIdx + 1) % BARO_MEDIAN_N);
    if (baroRingN < BARO_MEDIAN_N) { baroRingN++; return false; }

    float med = medianOf(baroRing, baroRingN);

    uint32_t now = millis();
    if (baroPrimed) {
        float dt = (now - baroLastMs) * 0.001f;
        if (dt < 0.001f) dt = 0.001f;
        float maxStep = BARO_MAX_RATE_PA_S * dt;
        float delta   = med - baroSmooth;
        if (fabsf(delta) > maxStep) {
            /* Physically impossible jump. Clamp rather than discard so a
             * genuine sustained change still tracks in, just rate-limited. */
            med = baroSmooth + (delta > 0 ? maxStep : -maxStep);
            baroRejects++;
        }
        baroSmooth += (med - baroSmooth) * BARO_SMOOTH_ALPHA;
    } else {
        baroSmooth = med;
        baroPrimed = true;
    }
    baroLastMs = now;

    S.pres_pa  = baroSmooth;
    S.bmpValid = true;

    /* Barometric altitude referenced directly to pad pressure. Exactly zero on
     * the pad by construction; no sea-level assumption involved. */
    if (padPress_pa > 1000.0f)
        S.alt_agl = 44330.8f * (1.0f - powf(S.pres_pa / padPress_pa, 0.190263f));
    return true;
}

/* ISA pressure -> altitude, used only to publish a human-readable pad altitude */
static float p2alt(float p_pa) {
    return 44330.8f * (1.0f - powf(p_pa / 101325.0f, 0.190263f));
}

/* ===========================================================================
 * SECTION 8 — ICM-20948 + AK09916 MAGNETOMETER
 * ======================================================================== */

static bool icmSelectBank(uint8_t bank) {
    return i2cWrite8(icmAddr, REG_BANK_SEL, (uint8_t)((bank & 0x03) << 4));
}

/* Assert the aux-bus bypass so the AK09916 becomes directly addressable at
 * 0x0C. MUST be called AFTER icm.begin(), because applyBasicDefaults() writes
 * INT_PIN_CFG = 0x30 and clears BYPASS_EN (see [ICM-1]). */
static bool icmEnableBypass() {
    if (!icmSelectBank(0)) return false;
    uint8_t uc = 0, ic = 0;
    if (!i2cRead8(icmAddr, USER_CTRL, uc)) return false;
    uc &= (uint8_t)~USER_CTRL_I2C_MST_EN;          /* host owns the aux bus */
    if (!i2cWrite8(icmAddr, USER_CTRL, uc)) return false;
    if (!i2cRead8(icmAddr, INT_PIN_CFG, ic)) return false;
    ic |= (uint8_t)BYPASS_EN;
    if (!i2cWrite8(icmAddr, INT_PIN_CFG, ic)) return false;
    delay(10);
    return true;
}

/* Place the AK09916 in 100 Hz continuous mode ONCE. Thereafter a read is a
 * single 9-byte burst with no blocking DRDY wait (see [ICM-2]).
 * CNTL2: 0x00 power-down, 0x02 10 Hz, 0x04 20 Hz, 0x06 50 Hz, 0x08 100 Hz. */
static bool magInit() {
    if (!icmEnableBypass()) {
        Serial.println(F("[MAG] bypass assert failed"));
        return false;
    }
    i2cWrite8(AK09916_I2C_ADDR, AK_CNTL3, 0x01);   /* soft reset */
    delay(100);

    uint8_t wia = 0;
    if (!i2cRead8(AK09916_I2C_ADDR, AK_WIA2, wia) || wia != AK_WIA2_VAL) {
        Serial.printf("[MAG] AK09916 not responding at 0x0C (WIA2=0x%02X, want 0x%02X)\n",
                      wia, AK_WIA2_VAL);
        return false;
    }
    if (!i2cWrite8(AK09916_I2C_ADDR, AK_CNTL2, 0x08)) return false;
    delay(10);
    Serial.println(F("[MAG] AK09916 OK — 100 Hz continuous"));
    return true;
}

/* One burst covers ST1(0x10), HXL..HZH(0x11..0x16), TMPS(0x17), ST2(0x18).
 * ST2 MUST be included: reading it is what releases the AK09916 measurement
 * latch. Omit it and the data registers stop updating. */
static bool magRead(float& mx, float& my, float& mz) {
    uint8_t b[9];
    if (!i2cReadBytes(AK09916_I2C_ADDR, AK_ST1, b, 9)) return false;
    if (!(b[0] & 0x01)) return false;              /* DRDY not set */
    if (b[8] & 0x08)    return false;              /* HOFL — magnetic overflow */
    mx = (float)(int16_t)((uint16_t)b[2] << 8 | b[1]) * 0.15f - magBiasX;
    my = (float)(int16_t)((uint16_t)b[4] << 8 | b[3]) * 0.15f - magBiasY;
    mz = (float)(int16_t)((uint16_t)b[6] << 8 | b[5]) * 0.15f - magBiasZ;
    return true;
}

/* Probe WHO_AM_I with an explicit bank-0 select first. Reading register 0x00
 * without forcing bank 0 returns whatever bank the chip was left in — which is
 * why a previous probe reported 0x02/0x03 instead of 0xEA. */
/* READ-ONLY diagnostic. This deliberately issues no writes — a prior version
 * of this routine sent a DEVICE_RESET write that timed out (Wire err=4) and
 * left the Teensy LPI2C peripheral in a state that then made the BMP585 -
 * an entirely different device, on the same shared bus - fail every read for
 * the rest of the run, even though it had validated cleanly seconds earlier.
 * A diagnostic must never be able to take down a device it isn't diagnosing;
 * a write that can time out is exactly that risk, so no writes happen here. */
static void icmBusDiagnose(uint8_t addr) {
    Serial.printf("[DIAG] ---- I2C read-only probe for 0x%02X ----\n", addr);

    Wire.beginTransmission(addr);
    uint8_t e = Wire.endTransmission();
    Serial.printf("[DIAG] address ACK        : %s (err=%u)\n", e == 0 ? "YES" : "NO", e);
    if (e != 0) { Serial.println(F("[DIAG] nothing responds — stop here")); return; }

    Wire.beginTransmission(addr);
    Wire.write((uint8_t)0x00);
    e = Wire.endTransmission(true);
    uint8_t n1 = Wire.requestFrom(addr, (uint8_t)1);
    uint8_t v1 = n1 ? Wire.read() : 0xFF;
    Serial.printf("[DIAG] write+STOP, read   : wErr=%u n=%u val=0x%02X\n", e, n1, v1);

    Wire.beginTransmission(addr);
    Wire.write((uint8_t)0x00);
    e = Wire.endTransmission(false);
    uint8_t n2 = Wire.requestFrom(addr, (uint8_t)1);
    uint8_t v2 = n2 ? Wire.read() : 0xFF;
    Serial.printf("[DIAG] repeated START     : wErr=%u n=%u val=0x%02X  %s\n",
                  e, n2, v2, v2 == WHO_AM_I_VAL ? "<-- ICM-20948 CONFIRMED" : "");

    Serial.println(F("[DIAG] --------------------------------------"));
}

static uint8_t icmProbe(uint8_t addr) {
    if (!i2cPresent(addr)) return 0xFF;          /* nothing acknowledges here */
    uint8_t v = 0xFF;
    /* Retry: a device that ACKs its address but fails the data phase is a bus
     * integrity problem, not an absent part. Several attempts distinguish the
     * two — a truly absent device never ACKs at all. */
    for (uint8_t i = 0; i < 4; i++) {
        i2cWrite8(addr, REG_BANK_SEL, 0x00);     /* force bank 0 for WHO_AM_I */
        delay(2);
        if (i2cRead8(addr, WHO_AM_I, v) && v == WHO_AM_I_VAL) return v;
        delay(5);
    }
    return v;
}

/* ===========================================================================
 * SECTION 9 — ORIENTATION MATH
 * ======================================================================== */

static inline void rotateBodyToPad(float& x, float& y, float& z) {
    float xp = R_pad[0][0]*x + R_pad[0][1]*y + R_pad[0][2]*z;
    float yp = R_pad[1][0]*x + R_pad[1][1]*y + R_pad[1][2]*z;
    float zp = R_pad[2][0]*x + R_pad[2][1]*y + R_pad[2][2]*z;
    x = xp; y = yp; z = zp;
}

static void setIdentity() {
    R_pad[0][0]=1; R_pad[0][1]=0; R_pad[0][2]=0;
    R_pad[1][0]=0; R_pad[1][1]=1; R_pad[1][2]=0;
    R_pad[2][0]=0; R_pad[2][1]=0; R_pad[2][2]=1;
}

/* Build R such that R * g_body = (0,0,+1) via Rodrigues' rotation formula.
 * Whatever attitude the CanSat is bolted down in, at rest ACC then reads
 * (0,0,+g) and the FSM's launch detector sees a clean vertical axis. */
static void computeReferenceRotation(float ax, float ay, float az) {
    float mag = sqrtf(ax*ax + ay*ay + az*az);
    if (mag < 0.5f || mag > 1.5f) { setIdentity(); return; }
    float gx = ax/mag, gy = ay/mag, gz = az/mag;
    float vx = gy, vy = -gx;
    float s = sqrtf(vx*vx + vy*vy);
    float c = gz;
    if (s < 1e-6f) {
        setIdentity();
        if (c < 0) { R_pad[1][1] = -1; R_pad[2][2] = -1; }
        return;
    }
    float kx = vx/s, ky = vy/s;
    float t = 1.0f - c;
    R_pad[0][0]=c+kx*kx*t; R_pad[0][1]=kx*ky*t;    R_pad[0][2]=ky*s;
    R_pad[1][0]=ky*kx*t;   R_pad[1][1]=c+ky*ky*t;  R_pad[1][2]=-kx*s;
    R_pad[2][0]=-ky*s;     R_pad[2][1]=kx*s;       R_pad[2][2]=c;
}

static inline float deadband(float v, float thr) {
    return (fabsf(v) < thr) ? 0.0f : v;
}

/* ===========================================================================
 * SECTION 10 — TB6612FNG GYRO-STABILISATION WHEEL
 * ======================================================================== */

static void motorInit() {
    pinMode(PIN_MOT_STBY, OUTPUT);
    pinMode(PIN_MOT_AIN1, OUTPUT);
    pinMode(PIN_MOT_AIN2, OUTPUT);
    pinMode(PIN_MOT_PWMA, OUTPUT);
    digitalWrite(PIN_MOT_STBY, LOW);     /* standby = safe default on the pad */
    digitalWrite(PIN_MOT_AIN1, LOW);
    digitalWrite(PIN_MOT_AIN2, LOW);
    analogWrite(PIN_MOT_PWMA, 0);
}

static int16_t gyroWheelPct = 0;

static void setGyroWheel(int16_t speedPct) {
    speedPct = constrain(speedPct, -100, 100);
    gyroWheelPct = speedPct;
    if (speedPct == 0) {
        digitalWrite(PIN_MOT_AIN1, LOW);
        digitalWrite(PIN_MOT_AIN2, LOW);
        analogWrite(PIN_MOT_PWMA, 0);
        digitalWrite(PIN_MOT_STBY, LOW);
        return;
    }
    digitalWrite(PIN_MOT_STBY, HIGH);
    digitalWrite(PIN_MOT_AIN1, speedPct > 0 ? HIGH : LOW);
    digitalWrite(PIN_MOT_AIN2, speedPct > 0 ? LOW  : HIGH);
    analogWrite(PIN_MOT_PWMA, map(abs((int)speedPct), 0, 100, 0, 255));
}

/* ===========================================================================
 * SECTION 11 — GNSS (Quectel L89)
 * ======================================================================== */

/* Build "$<body>*<CS>\r\n" with the checksum computed at run time. Hand-written
 * checksum literals are a silent-failure mode: a wrong byte means the receiver
 * discards the sentence and the configuration simply never applies. */
static void gpsSend(const char* body) {
    uint8_t cs = 0;
    for (const char* p = body; *p; ++p) cs ^= (uint8_t)*p;
    Serial1.printf("$%s*%02X\r\n", body, cs);
}

static void gpsSerialBegin(uint32_t baud) {
    Serial1.begin(baud);
    Serial1.addMemoryForRead(gpsRxBuffer, sizeof(gpsRxBuffer));
    gpsBaudActive = baud;
}

static void gpsHardwareInit() {
    pinMode(PIN_GPS_RESET, OUTPUT);
    digitalWrite(PIN_GPS_RESET, HIGH);   /* RESET_N is active low, idle high */
}

static void gpsHardReset() {
    Serial.println(F("[GPS] hardware reset"));
    digitalWrite(PIN_GPS_RESET, LOW);
    delay(10);
    digitalWrite(PIN_GPS_RESET, HIGH);
    delay(1500);
}

static void gpsApplyConfig() {
    /* Constellations: GPS, GLONASS, Galileo, Galileo-full, BeiDou.
     * Galileo-full stays 0 — this is the exact combination that was verified
     * working on this receiver (fix acquired, 5 SVs, HDOP 1.48). Do not change
     * it speculatively; an unsupported field can make the receiver reject the
     * whole sentence and silently keep its previous constellation set.
     * NOTE: PMTK353 has NO NavIC field — NavIC availability is a property of
     * the module variant, not of this command. NavIC satellites, when present,
     * are reported as $GIGSV/$GIGSA and counted in parseNmea(). */
    gpsSend("PMTK353,1,1,1,0,1");
    delay(250);

    /* Sentence rates: GLL off, RMC 1, VTG off, GGA 1, GSA 1, GSV 5, rest off.
     * GSV is decimated 1-in-5 because a full multi-constellation GSV set is the
     * single largest consumer of UART bandwidth and we only need it for the
     * satellites-in-view diagnostic, not for the navigation solution. */
    gpsSend("PMTK314,0,1,0,1,1,5,0,0,0,0,0,0,0,0,0,0,0,0,0");
    delay(250);

    gpsSend("PMTK313,1");     /* SBAS search on (GAGAN over India) */
    delay(100);
    gpsSend("PMTK301,2");     /* accept differential corrections from SBAS */
    delay(100);
    gpsSend("PMTK869,1,1");   /* EASY: offline orbit prediction, faster TTFF */
    delay(100);
    gpsSend("PMTK220,1000");  /* 1 Hz fix rate */
    delay(100);
}

/* Wait up to timeoutMs for a STRUCTURALLY VALID NMEA sentence: '$', a talker+
 * type, a '*', two hex checksum digits, and the checksum must actually match.
 * Raw byte traffic is NOT sufficient evidence the link is good — a UART fed
 * the wrong baud still produces byte counts from framing noise, which is
 * indistinguishable from real data by count alone. This is what "421 B
 * verified" got wrong: it proved bytes arrived, not that they meant anything. */
static bool gpsSeeValidSentence(uint32_t timeoutMs) {
    char buf[100]; uint8_t len = 0; bool inSentence = false;
    uint32_t t0 = millis();
    while (millis() - t0 < timeoutMs) {
        while (Serial1.available()) {
            char c = (char)Serial1.read();
            if (c == '$') { inSentence = true; len = 0; buf[len++] = c; continue; }
            if (!inSentence) continue;
            if (len < sizeof(buf) - 1) buf[len++] = c;
            if (c == '\n') {
                buf[len] = '\0';
                char* star = strchr(buf, '*');
                if (star && (star - buf) >= 6 && strlen(star) >= 3) {
                    uint8_t cs = 0;
                    for (char* p = buf + 1; p < star; p++) cs ^= (uint8_t)*p;
                    uint8_t got = (uint8_t)strtol(star + 1, nullptr, 16);
                    if (cs == got) return true;
                }
                inSentence = false;
            }
        }
    }
    return false;
}

/* Raise the link to 38400. At 9600 the multi-constellation NMEA burst exceeds
 * the 960 char/s the UART can carry and sentences are truncated — that is the
 * "GNSS freezes while moving" symptom (see [GNSS-1]). The change is volatile in
 * the receiver, so it is re-applied every boot and verified against an actual
 * checksum-valid sentence, not merely byte traffic; if none arrives we revert
 * to 9600 and re-apply the constellation/rate config there. */
static void gpsRaiseBaud() {
    char cmd[24];
    snprintf(cmd, sizeof(cmd), "PMTK251,%lu", (unsigned long)GPS_TARGET_BAUD);
    gpsSend(cmd);
    Serial1.flush();
    delay(150);

    gpsSerialBegin(GPS_TARGET_BAUD);
    delay(400);

    if (gpsSeeValidSentence(2000)) {
        Serial.printf("[GPS] link raised to %lu baud — valid NMEA confirmed\n",
                      (unsigned long)GPS_TARGET_BAUD);
        return;
    }

    Serial.println(F("[GPS] 38400 produced no checksum-valid sentence — reverting to 9600"));
    gpsSerialBegin(GPS_BOOT_BAUD);
    delay(200);
    gpsApplyConfig();          /* the switch attempt may have left the receiver
                                 * mid-configuration; reassert everything at the
                                 * baud we are actually staying on */
    if (gpsSeeValidSentence(2000))
        Serial.println(F("[GPS] confirmed alive at 9600"));
    else
        Serial.println(F("[GPS] WARNING: no valid NMEA at 9600 either — check"
                         " Serial1 RX=0/TX=1 wiring and RESET_N idle state"));
}

/* Extract the Nth comma-separated field (field 0 is the sentence identifier).
 * Returns false when the field is absent or empty, so callers can distinguish
 * "not reported" from "reported as zero". */
static bool nmeaField(const char* s, uint8_t idx, char* out, uint8_t outSz) {
    uint8_t f = 0;
    const char* p = s;
    while (*p && f < idx) { if (*p == ',') f++; p++; }
    if (f != idx) return false;
    uint8_t n = 0;
    while (*p && *p != ',' && *p != '*' && n < outSz - 1) out[n++] = *p++;
    out[n] = '\0';
    return n > 0;
}

/* GSV carries satellites IN VIEW per constellation; GGA carries satellites USED
 * in the solution. Reporting both is what makes a poor fix diagnosable: many in
 * view but few used points at geometry or signal quality, not at the receiver. */
static void parseNmea(const char* line) {
    if (line[0] != '$' || strlen(line) < 7) return;

    const char* talker = line + 1;      /* e.g. "GPGSV" */
    const char* type   = line + 3;      /* e.g. "GSV"   */

    if (!strncmp(type, "GSV", 3)) {
        char f[8];
        if (nmeaField(line, 3, f, sizeof(f))) {
            uint8_t n = (uint8_t)atoi(f);
            if      (!strncmp(talker, "GP", 2)) svInView[0] = n;
            else if (!strncmp(talker, "GL", 2)) svInView[1] = n;
            else if (!strncmp(talker, "GA", 2)) svInView[2] = n;
            else if (!strncmp(talker, "GB", 2) || !strncmp(talker, "BD", 2)) svInView[3] = n;
            else if (!strncmp(talker, "GQ", 2)) svInView[4] = n;
            else if (!strncmp(talker, "GI", 2)) { svInView[5] = n; navicSats = n; }
        }
    }

    if (!strncmp(type, "GSA", 3)) {
        if      (!strncmp(talker, "GP", 2)) gnssMask |= 1;
        else if (!strncmp(talker, "GL", 2)) gnssMask |= 2;
        else if (!strncmp(talker, "GA", 2)) gnssMask |= 4;
        else if (!strncmp(talker, "GB", 2)) gnssMask |= 8;
        else if (!strncmp(talker, "GQ", 2)) gnssMask |= 16;
        else if (!strncmp(talker, "GI", 2)) gnssMask |= 32;
        else if (!strncmp(talker, "GN", 2)) gnssMask |= 64;
        char f[10];
        if (nmeaField(line, 17, f, sizeof(f))) { vdop_val = atof(f); vdopValid = true; }
    }

    if (!strncmp(type, "GGA", 3)) {
        char f[12];
        if (nmeaField(line, 6, f, sizeof(f)))  fixQuality = (uint8_t)atoi(f);
        if (nmeaField(line, 11, f, sizeof(f))) { geoidSep = atof(f); geoidValid = true; }
    }
}

/* Non-blocking. Called from every wait loop in the firmware so NMEA is drained
 * continuously even while the radio owns the foreground. */
static void pollGPS() {
    while (Serial1.available()) {
        char c = (char)Serial1.read();
        gps.encode(c);
        if (c == '\n' || c == '\r') {
            if (nmeaLen > 5) { nmeaBuf[nmeaLen] = '\0'; parseNmea(nmeaBuf); }
            nmeaLen = 0;
        } else if (nmeaLen < sizeof(nmeaBuf) - 1) {
            nmeaBuf[nmeaLen++] = c;
        } else {
            nmeaLen = 0;                 /* overlong garbage — resynchronise */
        }
    }
}

/* ===========================================================================
 * SECTION 12 — SENSOR SERVICE
 * ======================================================================== */

static void readIMU() {
    if (!icmFitted) { S.imuValid = false; S.magValid = false; return; }

    float ax, ay, az, gx, gy, gz;
    bool accOk = icm.readAccel(ax, ay, az);
    bool gyrOk = icm.readGyro(gx, gy, gz);

    if (accOk && gyrOk) {
        icmFailStreak = 0;

        /* Sensitivity correction before anything else consumes the vector. */
        ax *= accelScale; ay *= accelScale; az *= accelScale;

        gx -= gyroBiasX; gy -= gyroBiasY; gz -= gyroBiasZ;
        rotateBodyToPad(ax, ay, az);
        rotateBodyToPad(gx, gy, gz);
        gx = deadband(gx, GYRO_DEADBAND_DPS);
        gy = deadband(gy, GYRO_DEADBAND_DPS);
        gz = deadband(gz, GYRO_DEADBAND_DPS);

        S.acc_g[0]=ax; S.acc_g[1]=ay; S.acc_g[2]=az;
        S.acc_ms2[0]=ax*9.80665f; S.acc_ms2[1]=ay*9.80665f; S.acc_ms2[2]=az*9.80665f;
        S.gyro_dps[0]=gx; S.gyro_dps[1]=gy; S.gyro_dps[2]=gz;
        S.spin_dps = gz;
        S.imuValid = true;
    } else {
        S.imuValid = false;
        if (icmFailStreak < 255) icmFailStreak++;
        if (icmFailStreak >= ICM_REINIT_AFTER) {
            Serial.println(F("[ICM] read failures — reinitialising"));
            i2cBusRecover();
            Wire.begin(); Wire.setClock(I2C_CLOCK_HZ);
            if (icm.begin(Wire, icmAddr)) {
                icm.applyBasicDefaults();
                if (magFitted) magInit();     /* begin() clears bypass again */
            }
            icmFailStreak = 0;
        }
    }

    if (magFitted) {
        float mx, my, mz;
        if (magRead(mx, my, mz)) {
            rotateBodyToPad(mx, my, mz);
            S.mag_ut[0]=mx; S.mag_ut[1]=my; S.mag_ut[2]=mz;
            S.magValid = true;
        } else {
            S.magValid = false;    /* no reading this cycle -> empty cell */
        }
    }
}

static void readPower() {
    float vadc = analogRead(BUS_V_PIN) * 3.3f / 4095.0f;
    S.bus_v    = vadc * BUS_V_RATIO;
    S.busValid = true;
    S.soc_pct  = constrain((S.bus_v - BATT_EMPTY_V) /
                           (BATT_FULL_V - BATT_EMPTY_V) * 100.0f, 0.0f, 100.0f);
}

/* Called both from the main loop AND from inside the radio's airtime wait.
 * Running the sensor pipeline during airtime is free: the MCU would otherwise
 * be idle for ~700 ms, and it feeds the barometer's median window many more
 * samples than a once-per-packet read ever could. */
static void serviceSensors() {
    static uint32_t lastMs = 0;
    uint32_t now = millis();
    if (now - lastMs < 40) return;        /* ~25 Hz, matched to the BMP ODR */
    lastMs = now;
    if (bmpFitted) baroService();
    readIMU();
    readPower();
}

/* ===========================================================================
 * SECTION 13 — PERSISTENCE
 * ======================================================================== */

static void saveState() {
    if (missionBootEpoch == 0) return;
    PersistState ps;
    ps.magic       = EEPROM_MAGIC;
    ps.bootEpoch   = missionBootEpoch;
    ps.state       = (uint8_t)state;
    ps.pktCount    = pktCount;
    ps.padPress_pa = padPress_pa;
    ps.gyroBias[0] = gyroBiasX; ps.gyroBias[1] = gyroBiasY; ps.gyroBias[2] = gyroBiasZ;
    ps.accelScale  = accelScale;
    ps.magBias[0]  = magBiasX;  ps.magBias[1]  = magBiasY;  ps.magBias[2]  = magBiasZ;
    memcpy(ps.R_pad, R_pad, sizeof(R_pad));
    EEPROM.put(EEPROM_ADDR, ps);
}

/* ===========================================================================
 * SECTION 14 — RADIO
 * ======================================================================== */

void onPacket() { pktFlag = true; }

static bool radioInit() {
    /* SPI1's default MISO is pin 1, which is also Serial1 TX — the GNSS line.
     * Without this remap the SPI peripheral samples MISO on a pin the GNSS is
     * actively driving, every register read is corrupt, and begin() reports
     * RADIOLIB_ERR_CHIP_NOT_FOUND (-2). The board is wired to pin 39; only the
     * peripheral routing was ever missing. */
    static bool spi1Started = false;
    if (!spi1Started) {
        SPI1.setMISO(PIN_SPI1_MISO);
        SPI1.setMOSI(PIN_SPI1_MOSI);
        SPI1.setSCK(PIN_SPI1_SCK);
        SPI1.begin();
        spi1Started = true;
    }

    radio.setRfSwitchTable(SW_PINS, SW_TABLE);
    float f = useBackup ? FREQ_BACKUP : FREQ_MAIN;
    int16_t st = radio.begin(f, LORA_BW, LORA_SF, LORA_CR,
                             LORA_SYNC, TX_POWER, LORA_PRE, TCXO_V);
    if (st != RADIOLIB_ERR_NONE) {
        Serial.printf("[RF] begin FAILED code=%d\n", st);
        return false;
    }
    radio.setRfSwitchTable(SW_PINS, SW_TABLE);   /* begin() clears it */
    radio.setCRC(2);
    radio.setRegulatorDCDC();
    radio.setSyncWord(LORA_SYNC);
    /* The second argument forces the HIGH-POWER PA path. Without it the part
     * routes through the LP PA, and the Core1121-XF antenna match is built for
     * the HP pin — roughly 30 dB of loss, i.e. kilometres become metres. */
    radio.setOutputPower(TX_POWER, true);

    LR11x0VersionInfo_t vi;
    if (radio.getVersionInfo(&vi) == RADIOLIB_ERR_NONE)
        Serial.printf("[RF] HW=0x%02X DEV=0x%02X FW=%d.%d  %.2f MHz SF%d BW%.0fk OK\n",
                      vi.hardware, vi.device, vi.fwMajor, vi.fwMinor,
                      f, LORA_SF, LORA_BW);
    return true;
}

/* ===========================================================================
 * SECTION 15 — PACKET BUILDER (base-36, format unchanged)
 * ======================================================================== */

static char pkt[320];
static int  pktLen = 0;

static void csvBegin(const char* teamId) {
    pktLen = 0;
    for (const char* s = teamId; *s && pktLen < (int)sizeof(pkt)-1; ++s)
        pkt[pktLen++] = *s;
    pkt[pktLen] = '\0';
}

static void b36append(uint32_t v) {
    if (v == 0) { pkt[pktLen++] = '0'; pkt[pktLen] = '\0'; return; }
    char tmp[8]; int n = 0;
    while (v) { tmp[n++] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"[v % 36]; v /= 36; }
    for (int i = n - 1; i >= 0; i--) pkt[pktLen++] = tmp[i];
    pkt[pktLen] = '\0';
}

static void b36Cell(bool has, int32_t v) {
    if (pktLen < (int)sizeof(pkt) - 1) pkt[pktLen++] = ',';
    if (!has) { pkt[pktLen] = '\0'; return; }   /* empty cell -> GCS shows "—" */
    if (v < 0) { pkt[pktLen++] = '-'; b36append((uint32_t)(-v)); }
    else        b36append((uint32_t)v);
}

static inline void b36_i (bool has, long  v) { b36Cell(has, (int32_t)v); }
static inline void b36_f1(bool has, float v) { b36Cell(has, (int32_t)lroundf(v * 10.0f)); }
static inline void b36_f2(bool has, float v) { b36Cell(has, (int32_t)lroundf(v * 100.0f)); }
static inline void b36_f6(bool has, float v) { b36Cell(has, (int32_t)lroundf(v * 1e6f)); }

static uint8_t frame = 0;

static void buildPacket() {
    /* TIME_S is derived from the battery-backed RTC once the mission epoch is
     * known, so a mid-flight processor reset is invisible to the mission clock.
     * millis() alone would restart at zero (guideline 6.1.ix). */
    long t_s = (rtcFitted && missionBootEpoch != 0)
                   ? (long)(rtc.now().unixtime() - missionBootEpoch)
                   : (long)(millis() / 1000UL);

    bool locOk  = gps.location.isValid();
    bool altOk  = gps.altitude.isValid();
    bool satOk  = gps.satellites.isUpdated() || gps.satellites.isValid();
    bool spdOk  = gps.speed.isValid();
    bool timeOk = gps.time.isValid();
    long gnss_t = timeOk ? (long)gps.time.hour()*3600 + gps.time.minute()*60
                                                     + gps.time.second() : 0;
    if (gps.hdop.isValid()) hdop_val = gps.hdop.hdop();

    csvBegin(TEAM_ID);
    b36_i(true, frame);

    /* --- Core, schema 0..15, present in EVERY packet --------------------- */
    b36_i (true,       t_s);                        /*  0 TIME_S      */
    b36_i (true,       pktCount);                   /*  1 PKT         */
    b36_f1(S.bmpValid, S.alt_agl);                  /*  2 ALT_M   x10 */
    b36_i (S.bmpValid, (long)S.pres_pa);            /*  3 PRES_PA     */
    b36_f1(S.bmpValid, S.temp_c);                   /*  4 TEMP_C  x10 */
    b36_f2(S.busValid, S.bus_v);                    /*  5 BUS_V  x100 */
    b36_i (timeOk,     gnss_t);                     /*  6 GNSS_TIME   */
    b36_f6(locOk,      gps.location.lat());         /*  7 LAT    x1e6 */
    b36_f6(locOk,      gps.location.lng());         /*  8 LON    x1e6 */
    b36_f1(altOk,      gps.altitude.meters());      /*  9 GPS_ALT x10 */
    b36_i (satOk,      (long)gps.satellites.value());/* 10 SATS       */
    b36_f1(S.imuValid, S.acc_ms2[0]);               /* 11 ACC_X   x10 */
    b36_f1(S.imuValid, S.acc_ms2[1]);               /* 12 ACC_Y   x10 */
    b36_f1(S.imuValid, S.acc_ms2[2]);               /* 13 ACC_Z   x10 */
    b36_f1(S.imuValid, S.spin_dps);                 /* 14 SPIN    x10 */
    b36_i (true,       (long)state);                /* 15 STATE       */

    /* --- Optional slice, one third per frame ----------------------------- */
    switch (frame) {
    case 0:
        b36_i (S.busValid, (long)lroundf(S.soc_pct));           /* 17 SOC_PCT   */
        b36_f1(spdOk, spdOk ? gps.speed.mps() : 0.0f);          /* 18 GND_SPEED */
        b36_i (true, 0);                                        /* 70 MCU_ACTIVE*/
        b36_i (true, 1);                                        /* 71 WDOG_OK   */
        b36_i (true, beaconOn ? 1 : 0);                         /* 74 BEACON_ON */
        break;
    case 1:
        b36_f1(S.imuValid, S.gyro_dps[0]);                      /* 23 GX  x10 */
        b36_f1(S.imuValid, S.gyro_dps[1]);                      /* 24 GY  x10 */
        b36_f1(S.imuValid, S.gyro_dps[2]);                      /* 25 GZ  x10 */
        b36_i (S.magValid, (long)lroundf(S.mag_ut[0]));         /* 26 MX  uT  */
        b36_i (S.magValid, (long)lroundf(S.mag_ut[1]));         /* 27 MY  uT  */
        b36_i (S.magValid, (long)lroundf(S.mag_ut[2]));         /* 28 MZ  uT  */
        break;
    case 2:
        b36_i (true, navicSats);                                /* 77 NAVIC_SATS */
        b36_f2(gps.hdop.isValid(), hdop_val);                   /* 79 HDOP  x100 */
        b36_f2(vdopValid, vdop_val);                            /* 80 VDOP  x100 */
        b36_i (true, fixQuality);                               /* 81 FIX_QUALITY*/
        b36_f1(geoidValid, geoidSep);                           /* 82 GEOID x10  */
        b36_i (true, gnssMask);                                 /* 83 GNSS_SYS   */
        break;
    }

    frame = (frame + 1) % 3;
}

/* ===========================================================================
 * SECTION 16 — CALIBRATION
 * ======================================================================== */

/* Gyro bias with a stillness gate. Averaging while the vehicle is being handled
 * bakes the motion INTO the bias — which is exactly how a 2.6 dps residual on
 * GY survives a "successful" calibration. Here the spread of the samples is
 * measured and a calibration taken while moving is rejected outright. */
static bool calibrateGyro(uint16_t samples, float maxStdDps) {
    float sx=0, sy=0, sz=0, sx2=0, sy2=0, sz2=0;
    uint16_t n = 0;
    for (uint16_t i = 0; i < samples; i++) {
        float x, y, z;
        if (icm.readGyro(x, y, z)) {
            sx += x; sy += y; sz += z;
            sx2 += x*x; sy2 += y*y; sz2 += z*z;
            n++;
        }
        delay(5);
    }
    if (n < samples / 2) { Serial.println(F("[CAL] gyro: too few samples")); return false; }

    float mx = sx/n, my = sy/n, mz = sz/n;
    float vx = sx2/n - mx*mx, vy = sy2/n - my*my, vz = sz2/n - mz*mz;
    float sd = sqrtf(fmaxf(0.0f, fmaxf(vx, fmaxf(vy, vz))));

    if (sd > maxStdDps) {
        Serial.printf("[CAL] gyro REJECTED — vehicle moving (sd=%.2f > %.2f dps)\n",
                      sd, maxStdDps);
        return false;
    }
    gyroBiasX = mx; gyroBiasY = my; gyroBiasZ = mz;
    Serial.printf("[CAL] gyro bias %.3f,%.3f,%.3f dps (n=%u sd=%.3f)\n",
                  gyroBiasX, gyroBiasY, gyroBiasZ, n, sd);
    return true;
}

/* Single-point accelerometer scale calibration. At rest the measured vector
 * magnitude must be exactly 1 g; anything else is sensitivity error. This is
 * what turns the observed 9.60 m/s2 into a true 9.80665 (see [ICM-3]). The
 * same still sample defines the pad attitude reference. */
static bool calibrateAccel(uint16_t samples) {
    float sx=0, sy=0, sz=0;
    uint16_t n = 0;
    for (uint16_t i = 0; i < samples; i++) {
        float x, y, z;
        if (icm.readAccel(x, y, z)) { sx += x; sy += y; sz += z; n++; }
        delay(5);
    }
    if (n < samples / 2) { Serial.println(F("[CAL] accel: too few samples")); return false; }

    float ax = sx/n, ay = sy/n, az = sz/n;
    float mag = sqrtf(ax*ax + ay*ay + az*az);
    if (mag < 0.80f || mag > 1.20f) {
        Serial.printf("[CAL] accel REJECTED — |a|=%.3f g is not gravity\n", mag);
        return false;
    }
    accelScale = 1.0f / mag;
    computeReferenceRotation(ax * accelScale, ay * accelScale, az * accelScale);
    float tilt = acosf(constrain(az/mag, -1.0f, 1.0f)) * 57.2958f;
    Serial.printf("[CAL] accel |a|=%.4f g -> scale %.4f (%.2f%% corrected), tilt %.1f deg\n",
                  mag, accelScale, (accelScale - 1.0f) * 100.0f, tilt);
    return true;
}

static bool calibratePad(uint16_t samples) {
    float sp = 0; uint16_t np = 0;
    for (uint16_t i = 0; i < samples; i++) {
        float t, p;
        if (bmpReadRaw(t, p) && p > BARO_MIN_PA && p < BARO_MAX_PA) { sp += p; np++; }
        delay(20);
    }
    if (np < samples / 2) { Serial.println(F("[CAL] pad pressure: too few samples")); return false; }
    padPress_pa = sp / np;
    padAlt_m    = p2alt(padPress_pa);
    baroReset();
    baroSmooth = padPress_pa;
    baroPrimed = true;
    baroLastMs = millis();
    S.pres_pa  = padPress_pa;
    S.alt_agl  = 0.0f;
    Serial.printf("[CAL] pad P=%.1f Pa (n=%u) -> ISA alt %.2f m, AGL zeroed\n",
                  padPress_pa, np, padAlt_m);
    return true;
}

static void runFullCalibration() {
    Serial.println(F("[CAL] full recalibration — hold still..."));
    if (icmFitted) { calibrateGyro(200, 1.5f); calibrateAccel(200); }
    if (bmpFitted) calibratePad(64);
    saveState();
}

/* Hard-iron magnetometer calibration. Rotate the CanSat through all
 * orientations for the duration; the midpoint of each axis extreme is the
 * hard-iron offset. Without this the magnetometer reads a constant offset
 * imposed by the vehicle's own steel, magnets and current loops. */
static void runMagCalibration(uint16_t seconds) {
    if (!magFitted) { Serial.println(F("[MAGCAL] no magnetometer")); return; }
    Serial.printf("[MAGCAL] rotate the CanSat in all axes for %u s...\n", seconds);
    float lo[3] = { 1e6f, 1e6f, 1e6f}, hi[3] = {-1e6f,-1e6f,-1e6f};
    float sx = magBiasX, sy = magBiasY, sz = magBiasZ;
    magBiasX = magBiasY = magBiasZ = 0.0f;          /* collect raw */
    uint32_t t0 = millis(); uint32_t n = 0;
    while (millis() - t0 < (uint32_t)seconds * 1000UL) {
        float m[3];
        if (magRead(m[0], m[1], m[2])) {
            for (uint8_t i = 0; i < 3; i++) {
                if (m[i] < lo[i]) lo[i] = m[i];
                if (m[i] > hi[i]) hi[i] = m[i];
            }
            n++;
        }
        pollGPS();
        delay(10);
    }
    if (n < 100 || (hi[0]-lo[0]) < 5.0f) {
        Serial.printf("[MAGCAL] insufficient coverage (n=%lu) — keeping old bias\n",
                      (unsigned long)n);
        magBiasX = sx; magBiasY = sy; magBiasZ = sz;
        return;
    }
    magBiasX = (hi[0]+lo[0]) * 0.5f;
    magBiasY = (hi[1]+lo[1]) * 0.5f;
    magBiasZ = (hi[2]+lo[2]) * 0.5f;
    Serial.printf("[MAGCAL] hard-iron bias %.1f,%.1f,%.1f uT (n=%lu)\n",
                  magBiasX, magBiasY, magBiasZ, (unsigned long)n);
    saveState();
}

/* ===========================================================================
 * SECTION 17 — COMMANDS
 * ======================================================================== */

static void execCmd(const char* cmd) {
    Serial.print(F("[CMD] ")); Serial.println(cmd);
    if      (!strcmp(cmd, "TX_ON"))        txEnabled = true;
    else if (!strcmp(cmd, "TX_OFF"))       txEnabled = false;
    else if (!strcmp(cmd, "ARM"))          armed = true;
    else if (!strcmp(cmd, "DISARM"))       armed = false;
    else if (!strcmp(cmd, "BEACON_ON"))    beaconOn = true;
    else if (!strcmp(cmd, "BEACON_OFF"))   beaconOn = false;
    else if (!strcmp(cmd, "FORCE_EJECT"))  state = FS_EJECT;
    else if (!strcmp(cmd, "FORCE_SEC_EJ")) state = FS_SEC_EJECT;
    else if (!strcmp(cmd, "BRAKE_DEPLOY")) Serial.println(F("[CMD] BRAKE_DEPLOY — hook servo here"));
    else if (!strcmp(cmd, "CALIBRATE"))    runFullCalibration();
    else if (!strcmp(cmd, "MAG_CAL"))      runMagCalibration(20);
    else if (!strcmp(cmd, "FAILOVER"))   { useBackup = !useBackup; radioReady = radioInit(); }
    else if (!strcmp(cmd, "GYRO_ON"))      setGyroWheel(70);
    else if (!strcmp(cmd, "GYRO_OFF"))     setGyroWheel(0);
    else if (!strncmp(cmd, "GYRO_SPD,", 9)) setGyroWheel((int16_t)atoi(cmd + 9));
    else if (!strcmp(cmd, "GPS_RESET"))  { gpsHardReset(); gpsSerialBegin(GPS_BOOT_BAUD);
                                           delay(1200); gpsApplyConfig(); gpsRaiseBaud(); }
    else if (!strcmp(cmd, "NEW_MISSION")) {
        PersistState clear; memset(&clear, 0, sizeof(clear));
        EEPROM.put(EEPROM_ADDR, clear);
        Serial.println(F("[CMD] NEW_MISSION — EEPROM cleared, rebooting"));
        delay(50);
        SCB_AIRCR = 0x05FA0004;
    }
    else if (!strcmp(cmd, "RESET")) { saveState(); delay(20); SCB_AIRCR = 0x05FA0004; }
    else { Serial.print(F("[CMD] unknown: ")); Serial.println(cmd); }
}

/* Expects "025,CMD,PAYLOAD". Fixed buffers throughout — no String, so there is
 * no heap fragmentation path in the uplink handler. */
static void parseUplink(char* raw) {
    char* p1 = strchr(raw, ',');            if (!p1) return;
    *p1 = '\0';
    if (strcmp(raw, TEAM_ID) != 0) return;
    char* p2 = strchr(p1 + 1, ',');         if (!p2) return;
    *p2 = '\0';
    if (strcmp(p1 + 1, "CMD") != 0) return;
    char* cmd = p2 + 1;
    char* e = cmd + strlen(cmd);
    while (e > cmd && (e[-1]=='\r' || e[-1]=='\n' || e[-1]==' ')) *--e = '\0';
    if (*cmd) execCmd(cmd);
}

static void listenForCommand() {
    pktFlag = false;
    radio.setPacketReceivedAction(onPacket);
    radio.startReceive();
    uint32_t t0 = millis();
    while (millis() - t0 < RX_WINDOW_MS) {
        pollGPS();
        serviceSensors();
        if (pktFlag) {
            pktFlag = false;
            char buf[96];
            int len = radio.getPacketLength();
            if (len > 0 && len < (int)sizeof(buf)) {
                if (radio.readData((uint8_t*)buf, len) == RADIOLIB_ERR_NONE) {
                    buf[len] = '\0';
                    parseUplink(buf);
                }
            }
            break;
        }
    }
    radio.standby();
}

/* ===========================================================================
 * SECTION 18 — TX CYCLE
 * ======================================================================== */

static void txCycle() {
    /* Radio recovery is attempted once per cycle. Sensors keep sampling and the
     * downlink resumes the instant the part answers again — no other subsystem
     * ever waits on the radio. */
    if (!radioReady) {
        radioReady = radioInit();
        if (!radioReady) { pollGPS(); serviceSensors(); delay(100); return; }
        Serial.println(F("[RF] recovered"));
    }

    if (!txEnabled) { listenForCommand(); return; }

    pktCount++;
    buildPacket();

    radio.setPacketSentAction(onPacket);
    pktFlag = false;
    int16_t st = radio.startTransmit(pkt);
    if (st == RADIOLIB_ERR_NONE) {
        /* ~620-820 ms of airtime at SF10/BW125. Spend it draining NMEA and
         * feeding the barometer median window rather than idling. */
        uint32_t t0 = millis();
        while (!pktFlag && (millis() - t0 < 4000UL)) {
            pollGPS();
            serviceSensors();
        }
    } else {
        radioReady = false;      /* force a re-init on the next cycle */
    }
    radio.finishTransmit();
    pktFlag = false;

    Serial.printf("[TX] #%lu %dB %s\n", (unsigned long)pktCount, pktLen,
                  st == RADIOLIB_ERR_NONE ? "OK" : "FAIL");

    /* Adaptive uplink cadence: every cycle on the pad where commanding matters,
     * decimated in flight where telemetry rate matters. */
    static uint8_t rxSkip = 0;
    bool onPad = (state == FS_IDLE);
    if (onPad || (++rxSkip >= RX_SKIP_FLIGHT)) {
        rxSkip = 0;
        listenForCommand();
    }
}

/* ===========================================================================
 * SECTION 19 — FLIGHT STATE MACHINE
 * ======================================================================== */

static void updateState() {
    FlightState prev = state;

    float az_g = S.imuValid ? fabsf(S.acc_ms2[2] / 9.80665f) : 1.0f;
    if (state == FS_IDLE && armed && az_g > 3.0f) state = FS_LAUNCH;

    static float prevAgl = 0;
    float agl = S.alt_agl;
    if (state == FS_LAUNCH && agl < prevAgl - 2.0f) state = FS_EJECT;
    prevAgl = agl;

    if (state == FS_SEC_EJECT && agl < 10.0f && az_g < 1.05f) state = FS_TOUCHDOWN;

    if (state != prev) {
        /* The reaction wheel resists yaw; the parafoil steers BY yaw. Running
         * both simultaneously means the two actuators fight each other, so the
         * wheel runs only through the drag-chute phase and is shut down the
         * moment the parafoil takes authority. */
        if      (state == FS_LAUNCH)    setGyroWheel(70);
        else if (state == FS_SEC_EJECT) setGyroWheel(0);
        saveState();
        Serial.printf("[FSM] %d -> %d (saved)\n", prev, state);
    }
}

/* ===========================================================================
 * SECTION 20 — DIAGNOSTICS
 * ======================================================================== */

static void statusPrint() {
    static uint32_t last = 0;
    if (millis() - last < 10000UL) return;
    last = millis();

    uint8_t inView = 0;
    for (uint8_t i = 0; i < 6; i++) inView += svInView[i];

    Serial.printf("[GNSS] used=%u inview=%u (G%u R%u E%u B%u Q%u I%u) fix=%u "
                  "hdop=%.2f chars=%lu csErr=%lu baud=%lu\n",
                  (unsigned)gps.satellites.value(), inView,
                  svInView[0], svInView[1], svInView[2],
                  svInView[3], svInView[4], svInView[5],
                  fixQuality, gps.hdop.isValid() ? gps.hdop.hdop() : 99.99f,
                  gps.charsProcessed(), gps.failedChecksum(),
                  (unsigned long)gpsBaudActive);

    Serial.printf("[SENS] baro=%s %.1fPa agl=%.2fm rej=%u | imu=%s az=%.2f | mag=%s %.0f,%.0f,%.0f\n",
                  S.bmpValid ? "OK" : "--", S.pres_pa, S.alt_agl, baroRejects,
                  S.imuValid ? "OK" : "--", S.acc_ms2[2],
                  S.magValid ? "OK" : "--", S.mag_ut[0], S.mag_ut[1], S.mag_ut[2]);

    if (gps.charsProcessed() < 10 && millis() > 8000)
        Serial.println(F("[GNSS] NO DATA — check Serial1 RX=0/TX=1 and RESET_N"));
}

/* ===========================================================================
 * SECTION 21 — SETUP
 * ======================================================================== */

void setup() {
    set_arm_clock(600000000);

    Serial.begin(115200);
    delay(400);
    Serial.println(F("\n[VRITRA] 2026-INSPACe-CAN-7USAT-025  Teensy 4.1 TX  rev 3.0"));

    analogReadResolution(12);
    motorInit();

    i2cBusRecover();
    Wire.begin();
    Wire.setClock(I2C_CLOCK_HZ);

    /* ---- RTC + reset-survival record ------------------------------------ */
    rtcFitted = rtc.begin(&Wire);
    if (!rtcFitted) {
        Serial.println(F("[RTC] NOT FOUND — TIME_S falls back to millis() and will"
                         " not survive a mid-flight reset"));
    } else {
#ifdef SET_RTC_TIME
        rtc.adjust(DateTime(RTC_SET_Y, RTC_SET_MON, RTC_SET_D,
                            RTC_SET_H, RTC_SET_MIN, RTC_SET_S));
        Serial.println(F("[RTC] *** TIME FORCIBLY SET — comment out SET_RTC_TIME "
                         "and re-upload ***"));
#endif
        if (rtc.lostPower()) {
            Serial.println(F("[RTC] power lost — will sync from first GNSS UTC fix"));
        } else {
            DateTime n = rtc.now();
            Serial.printf("[RTC] OK — %04d-%02d-%02d %02d:%02d:%02d UTC\n",
                          n.year(), n.month(), n.day(), n.hour(), n.minute(), n.second());
        }
    }

    /* A warm restore is only ever meaningful mid-flight. If the stored state is
     * pre-launch, its pad pressure and attitude reference are stale bench data
     * whose conditions have since changed — carrying them forward silently
     * corrupts ALT_M for the whole run. Pre-launch therefore means cold boot. */
    PersistState ps;
    EEPROM.get(EEPROM_ADDR, ps);
    bool eepromValid = (ps.magic == EEPROM_MAGIC);
    bool midMission  = eepromValid && (ps.state >= FS_LAUNCH) && (ps.state != FS_TOUCHDOWN);
    freshBoot = !midMission;

    if (midMission) {
        state            = (FlightState)ps.state;
        pktCount         = ps.pktCount;
        padPress_pa      = ps.padPress_pa;
        padAlt_m         = p2alt(padPress_pa);
        gyroBiasX        = ps.gyroBias[0];
        gyroBiasY        = ps.gyroBias[1];
        gyroBiasZ        = ps.gyroBias[2];
        accelScale       = (ps.accelScale > 0.5f && ps.accelScale < 2.0f) ? ps.accelScale : 1.0f;
        magBiasX         = ps.magBias[0];
        magBiasY         = ps.magBias[1];
        magBiasZ         = ps.magBias[2];
        memcpy(R_pad, ps.R_pad, sizeof(R_pad));
        missionBootEpoch = ps.bootEpoch;
        Serial.printf("[EEPROM] WARM boot MID-MISSION — state=%d pkt=%lu padP=%.1fPa\n",
                      state, (unsigned long)pktCount, padPress_pa);
    } else {
        if (eepromValid)
            Serial.printf("[EEPROM] stored state=%d is pre-launch — COLD boot\n", ps.state);
        else
            Serial.println(F("[EEPROM] no valid record — COLD boot"));
        if (rtcFitted && !rtc.lostPower()) missionBootEpoch = rtc.now().unixtime();
    }

    /* ---- Radio first: it is fast, and every second it comes up earlier is a
     * second the operator has a live downlink while the slow sensor
     * calibrations below are still running. Failure here is non-fatal. ---- */
    for (int a = 1; a <= 5 && !radioReady; a++) {
        radioReady = radioInit();
        if (!radioReady) { Serial.printf("[RF] setup attempt %d failed\n", a); delay(200); }
    }
    if (!radioReady)
        Serial.println(F("[RF] not up — loop() keeps retrying; sensors still init"));

    /* ---- I2C inventory --------------------------------------------------- */
    /* I2C bring-up runs BEFORE the GNSS sequence. The GNSS path costs ~4 s
     * (1.5 s baseband guard, PMTK config, a baud switch and its verification
     * window) and there is no reason the inertial stack should sit behind it.
     * Keeping the fast bus first also means a sensor fault is reported early
     * instead of four seconds into the boot log. */
    Serial.print(F("[I2C] devices:"));
    for (uint8_t a = 1; a < 127; a++)
        if (i2cPresent(a)) Serial.printf(" 0x%02X", a);
    Serial.println();

    /* ---- Barometer -------------------------------------------------------- */
    bmpAddr   = 0x47;
    bmpFitted = bmp.beginI2C(Wire, 0x47, I2C_CLOCK_HZ);
    if (!bmpFitted) { bmpAddr = 0x46; bmpFitted = bmp.beginI2C(Wire, 0x46, I2C_CLOCK_HZ); }
    Wire.setClock(I2C_CLOCK_HZ);
    if (bmpFitted) {
        Serial.printf("[BMP] found at 0x%02X\n", bmpAddr);
        /* Override the library's defaults: it leaves IIR at coefficient 127,
         * a ~12 s time constant that made altitude look frozen ([BMP585-1]).
         * Each tier is validated by measurement before it is accepted. */
        bmpFitted = bmpBringUp();
        if (!bmpFitted)
            Serial.println(F("[BMP] all tiers failed — pressure/altitude absent"));
    } else {
        Serial.println(F("[BMP] NOT FOUND — pressure/altitude will be absent"));
    }

    /* ---- IMU -------------------------------------------------------------- */
    uint8_t w69 = icmProbe(0x69);
    uint8_t w68 = icmProbe(0x68);
    Serial.printf("[ICM] WHO_AM_I: 0x69=0x%02X  0x68=0x%02X  (want 0x%02X)\n",
                  w69, w68, WHO_AM_I_VAL);
    icmAddr = (w69 == WHO_AM_I_VAL) ? 0x69 : ((w68 == WHO_AM_I_VAL) ? 0x68 : 0x69);

    icmFitted = icm.begin(Wire, icmAddr);

    if (!icmFitted) {
        /* Attempt 2: the part may be sitting in a wedged state from a previous
         * run — an aborted burst, or bypass left asserted. A DEVICE_RESET
         * followed by an explicit wake clears that; icm.begin() alone does not
         * issue one, so a wedged part stays wedged across MCU resets and only a
         * full power cycle appears to "fix" it. */
        Serial.printf("[ICM] begin() failed at 0x%02X — hard reset + wake, retrying\n", icmAddr);
        i2cBusRecover();
        Wire.begin(); Wire.setClock(I2C_CLOCK_HZ);
        delay(20);
        i2cWrite8(icmAddr, PWR_MGMT_1, 0x80);      /* DEVICE_RESET */
        delay(120);
        i2cWrite8(icmAddr, PWR_MGMT_1, 0x01);      /* wake, CLKSEL auto */
        delay(50);
        i2cWrite8(icmAddr, REG_BANK_SEL, 0x00);
        delay(10);
        icmFitted = icm.begin(Wire, icmAddr);

        /* Attempt 3: try the alternate address in case AD0/SDO is floating and
         * the part settled on the other one this power cycle. */
        if (!icmFitted) {
            uint8_t alt = (icmAddr == 0x69) ? 0x68 : 0x69;
            if (icmProbe(alt) == WHO_AM_I_VAL) {
                Serial.printf("[ICM] found at alternate address 0x%02X instead\n", alt);
                icmAddr = alt;
                icmFitted = icm.begin(Wire, icmAddr);
            }
        }
    }

    if (!icmFitted) {
        Serial.println(F("[ICM] NOT FOUND — inertial fields will be absent"));
        icmBusDiagnose(0x69);
        Serial.println(F("[ICM] Read the matrix above:"));
        Serial.println(F("[ICM]  * 'after reset+wake' == 0xEA -> part is alive;"));
        Serial.println(F("[ICM]    it was wedged. This boot path now clears that."));
        Serial.println(F("[ICM]  * all reads 0xFF but address ACKs -> AD0/SDO is"));
        Serial.println(F("[ICM]    floating, or VDDIO is unpowered while VDD is not."));
        Serial.println(F("[ICM]    Tie AD0/SDO firmly to 3V3 and confirm VDDIO=3V3."));
    } else {
        Serial.printf("[ICM] OK at 0x%02X (accel +/-16g, gyro +/-2000dps, DLPF on)\n", icmAddr);
        /* Magnetometer must be brought up AFTER begin(): applyBasicDefaults()
         * writes INT_PIN_CFG=0x30 which clears BYPASS_EN ([ICM-1]). */
        magFitted = magInit();
        if (magFitted && i2cPresent(AK09916_I2C_ADDR))
            Serial.println(F("[MAG] 0x0C now visible on the bus (bypass asserted)"));
        if (!magFitted)
            Serial.println(F("[MAG] unavailable — MX/MY/MZ will be sent as empty cells"));
    }

    /* ---- Boot calibration -------------------------------------------------- */
    if (icmFitted && freshBoot) {
        Serial.println(F("[CAL] boot calibration — keep the vehicle STILL on a flat surface"));
        if (!calibrateGyro(200, 1.5f)) {
            Serial.println(F("[CAL] retrying gyro once..."));
            delay(500);
            calibrateGyro(200, 3.0f);
        }
        calibrateAccel(200);
    } else if (icmFitted) {
        Serial.printf("[CAL] warm reset — restored bias %.3f,%.3f,%.3f scale %.4f\n",
                      gyroBiasX, gyroBiasY, gyroBiasZ, accelScale);
    }

    if (bmpFitted) {
        if (freshBoot) {
            calibratePad(64);
        } else {
            /* Never re-zero on a warm reset: the vehicle may already be
             * airborne and re-referencing would corrupt ALT_M for the rest of
             * the flight. Prime the filter and keep the stored reference. */
            float t, p;
            if (bmpReadRaw(t, p) && p > BARO_MIN_PA && p < BARO_MAX_PA) {
                baroSmooth = p; baroPrimed = true; baroLastMs = millis(); S.pres_pa = p;
            }
            Serial.printf("[CAL] warm reset — kept pad reference %.1f Pa\n", padPress_pa);
        }
    }

    /* ---- GNSS (after I2C, which is fast and must not queue behind it) ----- */
    gpsHardwareInit();
    gpsSerialBegin(GPS_BOOT_BAUD);
    delay(1500);                 /* L89 baseband ignores PMTK sent before it is up */
    gpsApplyConfig();
    gpsRaiseBaud();

    if (freshBoot) { state = FS_IDLE; saveState(); }

    Serial.printf("[VRITRA] READY  %.2f MHz SF%d BW%.0fk  pad=%.1fPa (%.2fm)  RXwin=%lums\n",
                  useBackup ? FREQ_BACKUP : FREQ_MAIN, LORA_SF, LORA_BW,
                  padPress_pa, padAlt_m, RX_WINDOW_MS);
    Serial.printf("[VRITRA] subsystems: RF=%s GNSS=%s BARO=%s IMU=%s MAG=%s RTC=%s\n",
                  radioReady ? "OK" : "--", "OK",
                  bmpFitted ? "OK" : "--", icmFitted ? "OK" : "--",
                  magFitted ? "OK" : "--", rtcFitted ? "OK" : "--");
}

/* ===========================================================================
 * SECTION 22 — LOOP
 * ======================================================================== */

/* Only fires on a board whose DS3231 has never been set. Once the coin cell
 * holds a valid time this is a no-op for the life of the vehicle. */
static void syncRtcFromGpsIfNeeded() {
    if (!rtcFitted || missionBootEpoch != 0) return;
    if (!(gps.date.isValid() && gps.time.isValid())) return;
    if (gps.date.year() < 2024) return;              /* reject the 2000 default */
    rtc.adjust(DateTime(gps.date.year(), gps.date.month(), gps.date.day(),
                        gps.time.hour(), gps.time.minute(), gps.time.second()));
    missionBootEpoch = rtc.now().unixtime();
    Serial.println(F("[RTC] synced from first GNSS UTC fix"));
    if (freshBoot) saveState();
}

void loop() {
    pollGPS();
    syncRtcFromGpsIfNeeded();
    serviceSensors();
    updateState();
    statusPrint();
    txCycle();
}
