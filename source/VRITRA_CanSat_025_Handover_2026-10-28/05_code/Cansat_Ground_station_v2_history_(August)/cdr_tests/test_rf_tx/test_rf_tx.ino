/*
 * test_rf_tx.ino
 * Team 2026 INSPACe CAN 7USAT 025
 *
 * Range test transmitter. Runs on the Teensy 4.1, the CanSat side.
 * Works with test_rf_rx.ino on the ESP32 ground station.
 *
 * This is the sketch to flash on the assembled CanSat for a real range test.
 * Unlike the old bench version it does not send made up numbers. It reads the
 * real barometer, the real inertial unit, the real satellite receiver and the
 * real clock, builds a telemetry packet the same size and shape as the flight
 * packet, transmits it, and writes every reading to the SD card with a real
 * timestamp from the battery backed clock. So after a range test you have both
 * the link numbers from the ground station and a full record on the card.
 *
 * The radio settings are handed to us by the organisers, frequency allocation
 * plan row 025. Do not change the frequency, spreading factor, bandwidth,
 * coding rate, sync word or power.
 *
 * WHAT USES WHICH BUS, so nothing steps on anything else:
 *   radio            SPI1        pins 26 27 39, chip select 29
 *   SD card          SDIO        the Teensy built in card slot
 *   sensors + clock  I2C         pins 18 19
 *   satellites       Serial2     pins 7 8, reset on 6
 *   serial flash     SPI0        pins 10 11 12 13   <-- LEFT ALONE ON PURPOSE
 * The 7Semi serial flash is wired but this sketch never opens SPI0 and never
 * touches pins 10 to 13, so nothing on the flash can be disturbed by a range
 * test. If you want to test the flash use test_flash instead.
 *
 * THE CLOCK IS READ, NEVER WRITTEN. You said the real time is already set and
 * held by the coin cell. This sketch only reads it. Re flashing the Teensy does
 * not touch the clock either, it is a separate chip on its own battery, so you
 * can upload this as many times as you like without losing the time.
 *
 * ANTENNA. Use the RadioMaster dual band whip on the 900 MHz sub gigahertz
 * port, which is ANT2 on the Core1121 XF, not ANT1. It is tuned for this band
 * and has more gain and a cleaner match than the little rubber duck that ships
 * with the module. Keep the whip on both ends pointing the same way, upright,
 * so the two are polarised alike. Full reasoning is printed at the bottom of
 * this comment block in the ground sketch.
 */

#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>
#include <Wire.h>
#include <SdFat.h>

/* team id sent on air */
#define TeamId      "025"
#define TeamIdFull  "2026-INSPACe-CAN-7USAT-025"

/* radio settings from the frequency allocation plan, row 025 */
#define FreqMhz       867.00f
#define BwKhz         125.0f
#define Sf            10
#define Cr            5
#define SyncWord      0x98
#define PreambleLen   8
#define TcxoVolts     1.6f
#define UseHpPa       true
#define PowerPad      10
#define PowerFlight   14
#define PacketBytes   99
#define BufMax        160

/* radio pins, the flight wiring */
#define PinCs     29
#define PinIrq    30
#define PinRst    25
#define PinBusy   24
#define PinMosi   26
#define PinSck    27
#define PinMiso   39

/* sensors and clock on I2C */
#define PinSda      18
#define PinScl      19
#define BmpAddr     0x47
#define IcmAddr     0x69
#define RtcAddr     0x68

/* satellite receiver */
#define PinGnssRst  6

/* power sense divider on A6 */
#define PinBusV     A6
#define BusVRatio   (122.0f / 22.0f)

/*
 * Timing for the range test. A 99 byte packet is about 985 ms in the air.
 *
 * The flight firmware runs a full 1 Hz and only opens a listen window every
 * third packet, because at 1 Hz there is no room to both send and listen every
 * second. That makes commands take up to three seconds to land, which is fine
 * in flight but annoying on a range test when you are standing in a field
 * waiting for the calibrate command to take.
 *
 * So this test uses a slightly slower slot, 1500 ms, and listens on EVERY slot.
 * That means once telemetry is running a command lands within about a second
 * and a half every time, not one try in three. It is a deliberate test choice.
 * The graded 1 Hz link is the one in vritra_fc, this is a bench and field tool.
 */
#define TxPeriodMs   1500UL
#define RxWindowMs   450UL
#define TxTimeoutMs  3000UL

/* how often to write a line to the card, once a second */
#define LogPeriodMs  1000UL

/* command numbers, same as the flight computer uses */
#define CmdNone       0
#define CmdTxOn       1
#define CmdTxOff      2
#define CmdCalibrate  3

/* The RF switch table uses pins inside the LR1121 chip, not pins on the
 * Teensy. Copied from Radio.cpp. Do not change these. */
static const uint32_t swPins[] = {
    RADIOLIB_LR11X0_DIO5, RADIOLIB_LR11X0_DIO6,
    RADIOLIB_NC, RADIOLIB_NC, RADIOLIB_NC
};
static const Module::RfSwitchMode_t swTable[] = {
    { LR11x0::MODE_STBY,  { LOW,  LOW  } },
    { LR11x0::MODE_RX,    { HIGH, LOW  } },
    { LR11x0::MODE_TX,    { HIGH, HIGH } },
    { LR11x0::MODE_TX_HP, { LOW,  HIGH } },
    { LR11x0::MODE_TX_HF, { LOW,  LOW  } },
    { LR11x0::MODE_GNSS,  { LOW,  LOW  } },
    { LR11x0::MODE_WIFI,  { LOW,  LOW  } },
    END_OF_MODE_TABLE,
};

static LR1121 radio = new Module(PinCs, PinIrq, PinRst, PinBusy, SPI1);

/* the radio raises DIO9 both when a packet is sent and when one arrives,
 * so the state below decides which it was */
static volatile bool irqFlag = false;
static void onIrq() { irqFlag = true; }

#define StateIdle    0
#define StateSending 1
#define StateListen  2
#define StateWaiting 3
#define StateFault   4

static uint8_t  state    = StateIdle;
static uint32_t stateAt  = 0;
static uint32_t nextTxMs = 0;
static uint32_t retryAt  = 0;

static bool    txOn       = false;   /* stays off until we are told to start */
static int8_t  txPower    = PowerPad;
static bool    powerDirty = true;

static uint32_t sentCount = 0;
static uint32_t failCount = 0;
static uint32_t cmdCount  = 0;

static float   lastRssi = 0, lastSnr = 0;
static bool    heardGround = false;
static uint8_t lastSeq   = 255;
static uint8_t lastCmdId = CmdNone;   /* echoed in every packet so the ground
                                         can confirm its command landed */

static char     packet[BufMax];
static uint16_t packetLen = 0;
static String   inputLine;

/* ---- sensor state ---- */
static bool     baroOk = false, imuOk = false, rtcOk = false;
static float    padPa  = 101325.0f, padTc = 25.0f;

static uint32_t gnssTime = 0;
static float    gnssLat  = 0.0f, gnssLon = 0.0f, gnssAlt = 0.0f;
static uint8_t  gnssSats = 0;
static bool     gnssFix  = false;
/* 3072, not 1024. logFile.sync() on the SD card is the only blocking call in
 * this whole sketch, and on an ordinary microSD card a sync can occasionally
 * stall for the better part of a second doing its own internal housekeeping.
 * At 9600 baud a 1024 byte buffer only covers about 1.07 seconds of GNSS
 * traffic, so a slow sync landing at the wrong moment silently overflows it,
 * and the same sync runs every five seconds — this is why the standalone
 * GNSS test could get a clean fix in the same spot where this sketch, sharing
 * a loop with the card, could not. 3072 bytes covers roughly 3.2 seconds. */
static uint8_t         gnssBuf[3072];
static uint8_t         gnssBuf2[1024];
static HardwareSerial* gnssPort = &Serial2;   /* locked in gnssAutodetect() */
static char     nmea[128];
static uint8_t  nmeaPos = 0;

/* Raw traffic counters, kept only so STATUS can say whether the module is
 * talking at all. Sats staying at zero looks the same on screen whether the
 * receiver has never said a word or has been saying plenty and simply has no
 * fix, and those two cases need completely different next steps. */
static uint32_t gnssBytesSeen      = 0;
static uint32_t gnssSentencesSeen  = 0;
static uint32_t gnssChecksumFail   = 0;
static char     gnssLastSentence[96] = "";

/* live snapshot, refreshed every read, used by both the packet and the log */
static float    curAlt = 0, curPres = 0, curTemp = 0, curVolt = 0;
static float    curAx = 0, curAy = 0, curAz = 1.0f, curSpin = 0;

/* ---- SD ---- */
static SdFs   sd;
static FsFile logFile;
static bool   cardOk = false;
static char   logName[16] = "RNG000.CSV";
static uint32_t nextLogMs = 0;

/* The card header keeps underscores. That line is graded style data meant to be
 * opened in a spreadsheet, so it must match what a parser expects. Everything a
 * person reads on the serial monitor is written in plain words instead. */
static const char* CsvHeader =
    "TEAM_ID,RTC_TIME,TIME_S,PKT,ALT_M,PRES_PA,TEMP_C,BUS_V,GNSS_TIME,LAT,LON,"
    "GPS_ALT_M,SATS,ACC_X,ACC_Y,ACC_Z,SPIN_RATE,STATE,TX_ON,TX_POWER_DBM,"
    "LAST_CMD,LAST_UP_RSSI";

/* =====================================================================
 * small helpers
 * ===================================================================== */

/* turn a radio error number into words */
static const char* errorText(int16_t code) {
    switch (code) {
    case RADIOLIB_ERR_NONE:                     return "no error";
    case RADIOLIB_ERR_CHIP_NOT_FOUND:           return "chip not found, check the wiring";
    case RADIOLIB_ERR_SPI_CMD_TIMEOUT:          return "the chip never released busy";
    case RADIOLIB_ERR_SPI_CMD_INVALID:          return "the chip rejected that command";
    case RADIOLIB_ERR_SPI_CMD_FAILED:           return "the chip did not report success";
    case RADIOLIB_ERR_INVALID_FREQUENCY:        return "bad frequency";
    case RADIOLIB_ERR_INVALID_OUTPUT_POWER:     return "bad output power";
    case RADIOLIB_ERR_INVALID_BANDWIDTH:        return "bad bandwidth";
    case RADIOLIB_ERR_INVALID_SPREADING_FACTOR: return "bad spreading factor";
    default:                                    return "see the RadioLib error list";
    }
}

static const char* commandName(uint8_t id) {
    switch (id) {
    case CmdTxOn:      return "start telemetry";
    case CmdTxOff:     return "stop telemetry";
    case CmdCalibrate: return "calibrate";
    default:           return "unknown";
    }
}

/* how long a packet of this many bytes stays in the air, in milliseconds */
static float airtime(uint16_t bytes) {
    float symbolMs = ((float)(1UL << Sf) / (BwKhz * 1000.0f)) * 1000.0f;
    float top      = (float)(8 * bytes) - (float)(4 * Sf) + 44.0f;
    float bottom   = (float)(4 * Sf);
    float extra    = ceilf(top / bottom) * (float)Cr;
    if (extra < 0) extra = 0;
    return ((float)PreambleLen + 4.25f + 8.0f + extra) * symbolMs;
}

/* base 36, with a leading minus for negative values so accel and altitude can
 * go below zero. The ground sketch reverses exactly this. */
static void toBase36(char* out, long value) {
    const char* digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char* p = out;
    unsigned long mag;
    if (value < 0) { *p++ = '-'; mag = (unsigned long)(-value); }
    else           {             mag = (unsigned long)value;     }
    char temp[10];
    int  n = 0;
    if (mag == 0) temp[n++] = '0';
    while (mag > 0 && n < 9) { temp[n++] = digits[mag % 36]; mag /= 36; }
    while (n > 0) *p++ = temp[--n];
    *p = '\0';
}

/* =====================================================================
 * I2C and sensors, register sequences lifted from the passing standalone
 * tests test_telemetry_csv and test_barometer so they behave identically
 * ===================================================================== */
static bool wr8(uint8_t dev, uint8_t reg, uint8_t val) {
    Wire.beginTransmission(dev); Wire.write(reg); Wire.write(val);
    return Wire.endTransmission() == 0;
}
static bool rdN(uint8_t dev, uint8_t reg, uint8_t* b, uint8_t n) {
    Wire.beginTransmission(dev); Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)dev, (int)n) != n) return false;
    for (int i = 0; i < n; i++) b[i] = Wire.read();
    return true;
}

static inline int32_t signExtend24(uint32_t x) {
    x &= 0xFFFFFFu;
    return (x & 0x800000u) ? (int32_t)(x | 0xFF000000u) : (int32_t)x;
}

/* --- barometer --- */
static bool baroRead(float& tC, float& pPa) {
    uint8_t b[6];
    if (!rdN(BmpAddr, 0x1D, b, 6)) return false;
    uint32_t tu = ((uint32_t)b[2] << 16) | ((uint32_t)b[1] << 8) | b[0];
    uint32_t pu = ((uint32_t)b[5] << 16) | ((uint32_t)b[4] << 8) | b[3];
    tC  = (float)signExtend24(tu) / 65536.0f;
    pPa = (float)signExtend24(pu) / 64.0f;
    return true;
}
static float aglFromPressure(float pPa) {
    if (pPa <= 0.0f || padPa <= 0.0f) return 0.0f;
    const float t0 = padTc + 273.15f;
    return (t0 / 0.0065f) * (1.0f - powf(pPa / padPa, 0.1902632f));
}
static void baroBegin() {
    uint8_t id = 0;
    baroOk = rdN(BmpAddr, 0x01, &id, 1) && id == 0x51;
    if (!baroOk) return;
    wr8(BmpAddr, 0x7E, 0xB6);                                    delay(10);
    wr8(BmpAddr, 0x37, (0x80 | (0x14 << 2) | 0x00));            delay(5);
    wr8(BmpAddr, 0x31, 0x00);                                    /* IIR off */
    wr8(BmpAddr, 0x36, (0x40 | (0x04 << 3) | 0x01));            /* P x16 T x2 */
    wr8(BmpAddr, 0x37, (0x80 | (0x14 << 2) | 0x01));           delay(30);
    float tC, pPa;
    if (baroRead(tC, pPa)) { padPa = pPa; padTc = tC; }
}

/* --- inertial unit --- */
static bool imuRead(float& ax, float& ay, float& az, float& gz) {
    uint8_t b[12];
    wr8(IcmAddr, 0x7F, 0x00);                /* bank 0 */
    if (!rdN(IcmAddr, 0x2D, b, 12)) return false;
    ax = (int16_t)((b[0] << 8) | b[1])   / 2048.0f;
    ay = (int16_t)((b[2] << 8) | b[3])   / 2048.0f;
    az = (int16_t)((b[4] << 8) | b[5])   / 2048.0f;
    gz = (int16_t)((b[10] << 8) | b[11]) / 16.4f;
    return true;
}
static void imuBegin() {
    wr8(IcmAddr, 0x06, 0x80); delay(100);    /* device reset */
    wr8(IcmAddr, 0x06, 0x01); delay(20);     /* wake, auto clock */
    wr8(IcmAddr, 0x7F, 0x00);
    uint8_t who = 0;
    imuOk = rdN(IcmAddr, 0x00, &who, 1) && who == 0xEA;
    if (!imuOk) return;
    wr8(IcmAddr, 0x7F, 0x20);                /* bank 2 */
    wr8(IcmAddr, 0x14, 0x1F);                /* accel 16 g */
    wr8(IcmAddr, 0x01, 0x2F);                /* gyro 2000 dps */
    wr8(IcmAddr, 0x7F, 0x00);
    delay(30);
}

/* --- clock, READ ONLY, both 12 and 24 hour formats handled without ever
 *     writing a single register --- */
static uint8_t bcd(uint8_t v) { return (uint8_t)((v >> 4) * 10 + (v & 0x0F)); }

static bool rtcReadStamp(char* out, int cap) {
    uint8_t b[7];
    if (!rdN(RtcAddr, 0x00, b, 7)) { snprintf(out, cap, "-"); return false; }
    uint8_t ss = bcd(b[0] & 0x7F);
    uint8_t mm = bcd(b[1] & 0x7F);
    uint8_t hh;
    if (b[2] & 0x40) {                       /* 12 hour mode */
        uint8_t h = bcd(b[2] & 0x1F);
        bool pm = (b[2] & 0x20);
        if (pm && h != 12) h = (uint8_t)(h + 12);
        if (!pm && h == 12) h = 0;
        hh = h;
    } else {
        hh = bcd(b[2] & 0x3F);               /* 24 hour mode */
    }
    uint8_t dd  = bcd(b[4] & 0x3F);
    uint8_t mon = bcd(b[5] & 0x1F);
    uint16_t yr = (uint16_t)(2000 + bcd(b[6]));
    snprintf(out, cap, "%04u-%02u-%02u %02u:%02u:%02u", yr, mon, dd, hh, mm, ss);
    return true;
}

/* --- satellites --- */
static float nmeaToDecimal(const char* s, char hemi) {
    if (!s || *s == '\0') return 0.0f;
    float raw = atof(s);
    int deg = (int)(raw / 100);
    float dec = deg + (raw - deg * 100.0f) / 60.0f;
    if (hemi == 'S' || hemi == 'W') dec = -dec;
    return dec;
}
static void parseNmea(char* s) {
    if (strlen(s) < 9) return;
    char* star = strchr(s, '*');
    if (!star || strlen(star) < 3) return;
    uint8_t want = (uint8_t)strtoul(star + 1, nullptr, 16);
    uint8_t got = 0;
    for (char* p = s + 1; p < star; p++) got ^= (uint8_t)*p;
    if (got != want) { gnssChecksumFail++; return; }

    /* This sentence is real and intact, whatever it turns out to say. Keep a
     * copy before anything below chops it up, so STATUS can show the last
     * thing the module actually said even when it never mentions a fix. */
    gnssSentencesSeen++;
    strncpy(gnssLastSentence, s, sizeof(gnssLastSentence) - 1);
    gnssLastSentence[sizeof(gnssLastSentence) - 1] = '\0';
    *star = '\0';

    char* f[20];
    int n = 0; f[n++] = s;
    for (char* p = s; *p && n < 20; p++)
        if (*p == ',') { *p = '\0'; f[n++] = p + 1; }
    if (n < 2) return;
    if (strcmp(f[0] + 3, "GGA") != 0) return;
    if (n < 12) return;

    uint8_t q = (uint8_t)atoi(f[6]);
    gnssSats = (uint8_t)atoi(f[7]);
    if (q == 0 || !f[2][0] || !f[4][0]) { gnssFix = false; return; }
    gnssTime = (uint32_t)atof(f[1]);
    gnssLat  = nmeaToDecimal(f[2], f[3][0]);
    gnssLon  = nmeaToDecimal(f[4], f[5][0]);
    gnssAlt  = atof(f[9]);
    gnssFix  = true;
}
static void pollGnss() {
    while (gnssPort->available()) {
        char c = (char)gnssPort->read();
        gnssBytesSeen++;
        if (c == '$') { nmeaPos = 0; nmea[nmeaPos++] = c; continue; }
        if (nmeaPos == 0) continue;
        if (c == '\r' || c == '\n') {
            nmea[nmeaPos] = '\0';
            parseNmea(nmea);
            nmeaPos = 0;
            continue;
        }
        if (nmeaPos < sizeof(nmea) - 1) nmea[nmeaPos++] = c;
        else nmeaPos = 0;
    }
}

static void gnssSendCmd(const char* body) {
    uint8_t ck = 0;
    for (const char* p = body; *p; p++) ck ^= (uint8_t)*p;
    char buf[96];
    snprintf(buf, sizeof(buf), "$%s*%02X\r\n", body, ck);
    gnssPort->print(buf);
}

/* Try Serial2 then Serial1. Lock to whichever speaks first.
 * vritra_fc does the same — without this, a board still wired to the old
 * Serial1 pins (0/1) shows zero bytes on Serial2 forever. */
static void gnssAutodetect() {
    Serial2.begin(9600);
    Serial2.addMemoryForRead(gnssBuf,  sizeof(gnssBuf));
    Serial1.begin(9600);
    Serial1.addMemoryForRead(gnssBuf2, sizeof(gnssBuf2));

    uint32_t deadline = millis() + 3000UL;
    bool locked = false;
    while (millis() < deadline && !locked) {
        if (Serial2.available() && (char)Serial2.peek() == '$') {
            gnssPort = &Serial2;
            locked = true;
            Serial.println("Satellites  talking on Serial2 pins 7 and 8");
        } else if (Serial1.available() && (char)Serial1.peek() == '$') {
            gnssPort = &Serial1;
            locked = true;
            Serial.println("Satellites  talking on Serial1 pins 0 and 1");
        } else {
            /* drain noise bytes that are not sentence starts */
            if (Serial2.available()) Serial2.read();
            if (Serial1.available()) Serial1.read();
        }
    }
    if (!locked) {
        gnssPort = &Serial2;
        Serial.println("Satellites  no data on either port after 3 seconds, "
                       "check wiring on pins 6 7 8 and 0 1");
    }
    if (gnssPort != &Serial2) { Serial2.end(); }
    if (gnssPort != &Serial1) { Serial1.end(); }

    /* 300 ms for the module UART to come up, then send constellation config.
     * Sending too fast causes the L89 to drop commands silently. */
    delay(300);
    gnssSendCmd("PAIR066,1,1,1,0,0,1");   /* GPS GLONASS Galileo NavIC      */
    delay(60);
    gnssSendCmd("PMTK353,1,1,1,0,0");     /* MTK fallback, no NavIC param   */
    delay(60);
    gnssSendCmd("PMTK869,1,1");            /* EASY: orbit predictions cached */
}

/* --- power --- */
static float busVolts() {
    uint32_t acc = 0;
    for (int i = 0; i < 16; i++) acc += (uint32_t)analogRead(PinBusV);
    return ((float)acc / 16.0f) * (3.3f / 4095.0f) * BusVRatio;
}

/* refresh the whole live snapshot */
static void readSensors() {
    float tC, pPa, ax, ay, az, gz;
    if (baroOk && baroRead(tC, pPa)) {
        curPres = pPa; curTemp = tC; curAlt = aglFromPressure(pPa);
    }
    if (imuOk && imuRead(ax, ay, az, gz)) {
        curAx = ax; curAy = ay; curAz = az; curSpin = fabsf(gz);
    }
    curVolt = busVolts();
    pollGnss();
}

/* =====================================================================
 * SD card
 * ===================================================================== */
static void sdBegin() {
    /* SDIO, the built in slot. This does not open SPI0, so the serial flash on
     * pins 10 to 13 is never touched. */
    if (!sd.begin(SdioConfig(FIFO_SDIO))) {
        Serial.println("No SD card found, the range test will still run but");
        Serial.println("nothing will be recorded. Check the card and reset.");
        cardOk = false;
        return;
    }
    /* pick the next free file so a reset never writes over the last run */
    for (uint16_t i = 0; i < 1000; i++) {
        snprintf(logName, sizeof(logName), "RNG%03u.CSV", i);
        if (!sd.exists(logName)) break;
    }
    if (!logFile.open(logName, O_WRITE | O_CREAT | O_TRUNC)) {
        Serial.println("Could not create the log file on the card");
        cardOk = false;
        return;
    }
    logFile.println(CsvHeader);
    logFile.sync();
    cardOk = true;
    Serial.print("Recording to ");
    Serial.print(logName);
    Serial.println(" on the card");
}

static void logRow() {
    if (!cardOk) return;
    pollGnss();                       /* drain before the RTC's I2C read too */
    char stamp[24];
    rtcReadStamp(stamp, sizeof(stamp));

    char row[240];
    int n = snprintf(row, sizeof(row),
        "%s,%s,%lu,%lu,%.1f,%.0f,%.1f,%.2f,",
        TeamIdFull, stamp,
        (unsigned long)(millis() / 1000UL), (unsigned long)sentCount,
        curAlt, curPres, curTemp, curVolt);

    if (gnssFix)
        n += snprintf(row + n, sizeof(row) - n, "%06lu,%.7f,%.7f,%.1f,%u,",
                      (unsigned long)gnssTime, gnssLat, gnssLon, gnssAlt, gnssSats);
    else
        n += snprintf(row + n, sizeof(row) - n, "-,-,-,-,%u,", gnssSats);

    n += snprintf(row + n, sizeof(row) - n, "%.2f,%.2f,%.2f,%.1f,%d,%d,%d,%u,%.1f",
                  curAx * 9.80665f, curAy * 9.80665f, curAz * 9.80665f, curSpin,
                  txOn ? 3 : 0,               /* state, 3 is a stand in for on air */
                  txOn ? 1 : 0, (int)txPower, lastCmdId,
                  heardGround ? lastRssi : 0.0f);

    logFile.println(row);
    pollGnss();                       /* drain right up to the edge of sync() */
    static uint8_t sinceSync = 0;
    if (++sinceSync >= 5) {
        sinceSync = 0;
        logFile.sync();               /* the one call in this sketch that can
                                        * stall for the better part of a second */
        pollGnss();                   /* drain again the instant it returns  */
    }
}

/* =====================================================================
 * radio
 * ===================================================================== */
static void showHelp() {
    Serial.println();
    Serial.println("Type one of these:");
    Serial.println("  ON             start sending telemetry");
    Serial.println("  OFF            stop sending");
    Serial.println("  POWER PAD      use 10 dBm, the on pad level");
    Serial.println("  POWER FLIGHT   use 14 dBm, the in flight level");
    Serial.println("  CAL            recapture the ground pressure reference");
    Serial.println("  STATUS         show the totals");
    Serial.println("  HELP           show this list");
    Serial.println();
    Serial.println("You can also drive it from the ground station over the air.");
    Serial.println();
}

static void showStatus() {
    Serial.println();
    Serial.print("Sending is ");
    Serial.println(txOn ? "on" : "off");
    Serial.print("Power ");
    Serial.print(txPower);
    Serial.println(" dBm");
    Serial.print("Packets sent ");
    Serial.println(sentCount);
    Serial.print("Failed ");
    Serial.println(failCount);
    Serial.print("Commands received ");
    Serial.println(cmdCount);
    Serial.print("Last command ");
    Serial.println(commandName(lastCmdId));
    Serial.print("Barometer ");
    Serial.println(baroOk ? "ready" : "not responding");
    Serial.print("Inertial unit ");
    Serial.println(imuOk ? "ready" : "not responding");
    Serial.print("Clock ");
    if (rtcOk) { char s[24]; rtcReadStamp(s, sizeof(s)); Serial.println(s); }
    else       Serial.println("not responding");
    Serial.print("Satellite fix ");
    if (gnssFix) { Serial.print("yes, "); Serial.print(gnssSats); Serial.println(" sats"); }
    else         { Serial.print("no, "); Serial.print(gnssSats); Serial.println(" visible"); }

    /* This is the part that actually answers "why zero sats" instead of just
     * restating it. Bytes but no sentences means the wiring or the baud rate
     * is wrong. Sentences but no fix means the module is fine and it is
     * simply waiting on the sky, which needs time and a clear view, not a
     * firmware change. */
    Serial.print("Satellite receiver bytes seen ");
    Serial.println(gnssBytesSeen);
    Serial.print("Satellite sentences read cleanly ");
    Serial.println(gnssSentencesSeen);
    Serial.print("Satellite sentences with a bad checksum ");
    Serial.println(gnssChecksumFail);
    if (gnssBytesSeen == 0) {
        Serial.println("The receiver has not sent a single byte. At startup this "
                       "sketch tried both Serial2 (pins 7 and 8) and Serial1 "
                       "(pins 0 and 1) and the boot log says which one it locked "
                       "to. Check the reset line on pin 6, then check the TX wire "
                       "from the module into whichever port the boot log named.");
    } else if (gnssSentencesSeen == 0) {
        Serial.println("Bytes are arriving but nothing has read cleanly yet. Give "
                       "it a few more seconds, or check the baud rate and the "
                       "reset pulse if this does not change.");
    } else if (!gnssFix) {
        Serial.print("Last sentence heard  ");
        Serial.println(gnssLastSentence);
        Serial.println("The receiver is talking and every sentence checks out, it "
                       "just has no fix yet. That is the sky, not the code. Give it "
                       "a clear view upward and more time, away from any roof, "
                       "wall or the aluminium plates.");
    }
    Serial.print("Card ");
    Serial.println(cardOk ? logName : "not recording");
    if (heardGround) {
        Serial.print("Last uplink RSSI ");
        Serial.print(lastRssi, 1);
        Serial.print(" dBm, SNR ");
        Serial.print(lastSnr, 1);
        Serial.println(" dB");
    } else {
        Serial.println("The ground station has not been heard yet");
    }
    Serial.println();
}

static bool startRadio() {
    /* These three lines must come before SPI1.begin(). The Teensy uses a
     * different pin for SPI1 receive by default and our board is wired to 39.
     * Getting this wrong gives error 707, which looks like a dead chip. */
    SPI1.setMISO(PinMiso);
    SPI1.setMOSI(PinMosi);
    SPI1.setSCK(PinSck);
    SPI1.begin();

    radio.setRfSwitchTable(swPins, swTable);
    int16_t st = radio.begin(FreqMhz, BwKhz, Sf, Cr, SyncWord,
                             txPower, PreambleLen, TcxoVolts);
    if (st != RADIOLIB_ERR_NONE) {
        Serial.print("The radio did not start. ");
        Serial.println(errorText(st));
        return false;
    }
    radio.setRfSwitchTable(swPins, swTable);   /* begin() wipes it */
    radio.setCRC(2);
    radio.setRegulatorDCDC();
    radio.setSyncWord(SyncWord);
    if (radio.setOutputPower(txPower, UseHpPa) == RADIOLIB_ERR_NONE)
        powerDirty = false;
    if (radio.calibrateImageRejection(863.0f, 870.0f) != RADIOLIB_ERR_NONE)
        Serial.println("Image calibration did not finish cleanly");
    if (radio.setRxBoostedGainMode(true) != RADIOLIB_ERR_NONE)
        Serial.println("Could not turn on boosted receive gain");

    Serial.print("Radio ready on ");
    Serial.print(FreqMhz, 2);
    Serial.print(" MHz, SF");
    Serial.print(Sf);
    Serial.print(", ");
    Serial.print(BwKhz, 0);
    Serial.print(" kHz, CR 4 of ");
    Serial.print(Cr);
    Serial.print(", ");
    Serial.print(txPower);
    Serial.println(" dBm");

    radio.setIrqAction(onIrq);
    irqFlag = false;
    return true;
}

/* write the power to the chip only when the radio is free, otherwise the write
 * is lost and the chip stays at the old level while we think it changed */
static void applyPower() {
    if (!powerDirty) return;
    if (state != StateIdle && state != StateWaiting) return;
    if (radio.setOutputPower(txPower, UseHpPa) == RADIOLIB_ERR_NONE) {
        powerDirty = false;
        Serial.print("Power is now ");
        Serial.print(txPower);
        Serial.println(" dBm");
    }
}

static void listenForever() {
    irqFlag = false;
    if (radio.startReceive() == RADIOLIB_ERR_NONE) state = StateWaiting;
    else { state = StateFault; retryAt = millis() + 1000; }
    stateAt = millis();
}

/*
 * Build the packet from the live snapshot. Same twenty fields and same order as
 * the flight telemetry packet, so the airtime matches. Two extra fields ride at
 * the end, the transmit power and the id of the last command we obeyed, so the
 * ground station can see its command was received. Then it is padded to 99
 * bytes so the air time is representative of the real link.
 */
static uint16_t makePacket(char* out) {
    char t[10], p[10], alt[12], pres[10], temp[10], volt[10], gt[10];
    char lat[12], lon[12], galt[10], sats[10];
    char ax[10], ay[10], az[10], spin[10], st[10], flags[10], pwr[10], lc[10];

    toBase36(t,     (long)(millis() / 1000UL));
    toBase36(p,     (long)(sentCount + 1));
    toBase36(alt,   (long)lroundf(curAlt * 10.0f));
    toBase36(pres,  (long)lroundf(curPres));
    toBase36(temp,  (long)lroundf(curTemp * 10.0f));
    toBase36(volt,  (long)lroundf(curVolt * 100.0f));
    toBase36(gt,    (long)gnssTime);
    toBase36(lat,   (long)lroundf(gnssLat * 100000.0f));
    toBase36(lon,   (long)lroundf(gnssLon * 100000.0f));
    toBase36(galt,  (long)lroundf(gnssAlt * 10.0f));
    toBase36(sats,  (long)gnssSats);
    toBase36(ax,    (long)lroundf(curAx * 1000.0f));
    toBase36(ay,    (long)lroundf(curAy * 1000.0f));
    toBase36(az,    (long)lroundf(curAz * 1000.0f));
    toBase36(spin,  (long)lroundf(curSpin * 10.0f));
    toBase36(st,    (long)(txOn ? 3 : 0));
    toBase36(flags, (long)(imuOk ? 0x40 : 0) | (baroOk ? 0x20 : 0) | (gnssFix ? 1 : 0));
    toBase36(pwr,   (long)txPower);
    toBase36(lc,    (long)lastCmdId);

    int n = snprintf(out, BufMax,
        "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,F0,%s,%s",
        TeamId, t, p, alt, pres, temp, volt, gt, lat, lon, galt, sats,
        ax, ay, az, spin, st, flags, pwr, lc);
    if (n <= 0 || n >= BufMax) return 0;

    /* Pad the optional part so the packet reaches the real size. The padding
     * goes after a comma so it is its own field. Without the comma the X
     * letters would join onto the last value, and X is a real base 36 digit,
     * so the ground station would read that value wrong. */
    if (n < PacketBytes - 3 && n < BufMax - 4) {
        out[n++] = ',';
        while (n < PacketBytes - 2 && n < BufMax - 3) out[n++] = 'X';
    }
    out[n++] = '\r';
    out[n++] = '\n';
    out[n]   = '\0';
    return (uint16_t)n;
}

static void readUplink() {
    char buf[64];
    int  len = radio.getPacketLength();
    if (len <= 0 || len >= (int)sizeof(buf)) { radio.startReceive(); return; }
    if (radio.readData((uint8_t*)buf, (size_t)len) != RADIOLIB_ERR_NONE) {
        radio.startReceive();
        return;
    }
    buf[len] = '\0';
    lastRssi = radio.getRSSI();
    lastSnr  = radio.getSNR();
    heardGround = true;

    while (len > 0 && (buf[len-1] == '\r' || buf[len-1] == '\n')) buf[--len] = '\0';

    /* the message looks like  025,C,<id>,<arg>,<seq>  */
    int idLen = strlen(TeamId);
    bool mine = (len > idLen + 4 && strncmp(buf, TeamId, idLen) == 0 &&
                 buf[idLen] == ',' && buf[idLen+1] == 'C');

    Serial.println();
    Serial.println("Heard the ground station");
    Serial.print("  Message  ");
    Serial.println(buf);
    Serial.print("  RSSI     ");
    Serial.print(lastRssi, 1);
    Serial.println(" dBm");
    Serial.print("  SNR      ");
    Serial.print(lastSnr, 1);
    Serial.println(" dB");

    if (mine) {
        int id = 0, arg = 0, seq = -1;
        if (sscanf(buf + idLen + 3, "%d,%d,%d", &id, &arg, &seq) == 3 &&
            (uint8_t)seq != lastSeq) {
            lastSeq   = (uint8_t)seq;
            lastCmdId = (uint8_t)id;         /* echoed back so the ground knows */
            cmdCount++;
            if (id == CmdTxOn) {
                txOn = true;
                Serial.println("  Command  start telemetry");
            } else if (id == CmdTxOff) {
                txOn = false;
                Serial.println("  Command  stop telemetry");
            } else if (id == CmdCalibrate) {
                float tC, pPa;
                if (baroOk && baroRead(tC, pPa)) { padPa = pPa; padTc = tC; }
                Serial.println("  Command  calibrate, ground pressure recaptured");
            } else {
                Serial.print("  Command  number ");
                Serial.println(id);
            }
        }
    } else {
        Serial.println("  Not addressed to us, ignored");
    }
    Serial.println();
    radio.startReceive();
}

static void runRadio() {
    uint32_t now = millis();

    if (state == StateFault) {
        if ((int32_t)(now - retryAt) >= 0) {
            if (startRadio()) { state = StateIdle; nextTxMs = now; stateAt = now; }
            else retryAt = now + 1000;
        }
        return;
    }

    if (state == StateIdle) {
        applyPower();
        if (!txOn) { listenForever(); return; }   /* silent, so listen always */
        if ((int32_t)(now - nextTxMs) < 0) return;

        readSensors();                            /* fresh values for this packet */
        packetLen = makePacket(packet);
        if (packetLen == 0) return;
        irqFlag = false;
        if (radio.startTransmit((uint8_t*)packet, packetLen) != RADIOLIB_ERR_NONE) {
            failCount++;
            state = StateFault;
            retryAt = now + 500;
            return;
        }
        state    = StateSending;
        stateAt  = now;
        nextTxMs = now + TxPeriodMs;
        return;
    }

    if (state == StateSending) {
        if (irqFlag) {
            irqFlag = false;
            radio.finishTransmit();
            sentCount++;

            char shown[BufMax];
            strncpy(shown, packet, BufMax - 1);
            shown[BufMax - 1] = '\0';
            int k = strlen(shown);
            while (k > 0 && (shown[k-1] == '\r' || shown[k-1] == '\n')) shown[--k] = '\0';

            Serial.println();
            Serial.print("Packet ");
            Serial.println(sentCount);
            Serial.print("  Content  ");
            Serial.println(shown);
            Serial.print("  Bytes    ");
            Serial.println(packetLen);
            Serial.print("  Height   ");
            Serial.print(curAlt, 1);
            Serial.println(" m above the ground reference");
            Serial.print("  Sats     ");
            Serial.println(gnssSats);
            Serial.print("  Power    ");
            Serial.print(txPower);
            Serial.println(" dBm");

            /* listen on EVERY slot, that is the whole point of the test cadence */
            state = (radio.startReceive() == RADIOLIB_ERR_NONE)
                  ? StateListen : StateIdle;
            stateAt = now;
        } else if (now - stateAt > TxTimeoutMs) {
            failCount++;
            state = StateFault;
            retryAt = now + 200;
        }
        return;
    }

    if (state == StateListen) {
        if (irqFlag) {
            irqFlag = false;
            readUplink();
            radio.standby();
            state = StateIdle;
            stateAt = now;
        } else if (now - stateAt > RxWindowMs) {
            radio.standby();
            state = StateIdle;
            stateAt = now;
        }
        return;
    }

    if (state == StateWaiting) {
        applyPower();
        if (txOn) {
            radio.standby();
            state = StateIdle;
            nextTxMs = now;
            stateAt = now;
            return;
        }
        if (irqFlag) { irqFlag = false; readUplink(); stateAt = now; }
        else if (now - stateAt > 10000UL) listenForever();
        return;
    }
}

static void readSerial() {
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c != '\r' && c != '\n') {
            if (inputLine.length() < 32) inputLine += c;
            continue;
        }
        if (inputLine.length() == 0) continue;
        String cmd = inputLine;
        inputLine = "";
        cmd.trim();
        cmd.toUpperCase();

        if (cmd == "ON") {
            txOn = true;  lastCmdId = CmdTxOn;
            Serial.println("Sending started");
        } else if (cmd == "OFF") {
            txOn = false; lastCmdId = CmdTxOff;
            Serial.println("Sending stopped");
        } else if (cmd == "POWER PAD") {
            txPower = PowerPad;    powerDirty = true;
            Serial.println("Switching to pad power");
        } else if (cmd == "POWER FLIGHT") {
            txPower = PowerFlight; powerDirty = true;
            Serial.println("Switching to flight power");
        } else if (cmd == "CAL" || cmd == "CALIBRATE") {
            float tC, pPa;
            if (baroOk && baroRead(tC, pPa)) {
                padPa = pPa; padTc = tC;
                Serial.print("Ground pressure recaptured, ");
                Serial.print(padPa, 1);
                Serial.println(" Pa, height now reads zero here");
            } else {
                Serial.println("Barometer not responding, nothing changed");
            }
        } else if (cmd == "STATUS") {
            showStatus();
        } else if (cmd == "HELP") {
            showHelp();
        } else {
            Serial.println("I did not understand that. Type HELP for the list.");
        }
    }
}

void setup() {
    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 3000) { }

    Serial.println();
    Serial.println("Range test transmitter, team 2026 INSPACe CAN 7USAT 025");
    Serial.println("This board stays silent until it is told to start,");
    Serial.println("either by typing ON here or by a command from the ground.");

    /* 100 kHz, not 400 kHz. Measured on this harness, 400 kHz makes the
     * inertial unit answer its address and then return all ones on every read. */
    Wire.begin();
    Wire.setClock(100000);
    analogReadResolution(12);

    /* satellites: reset pulse, then auto-detect which serial port the module
     * actually talks on (Serial2 = rev 2026-08 harness, Serial1 = older wiring)
     * and send the constellation config the same way vritra_fc does */
    pinMode(PinGnssRst, OUTPUT);
    digitalWrite(PinGnssRst, LOW);
    delay(60);
    digitalWrite(PinGnssRst, HIGH);
    gnssAutodetect();

    baroBegin();
    imuBegin();

    /* clock, read only. If the hour comes back in 12 hour format it is decoded
     * in software, nothing is ever written, so the time you set stays exactly
     * as it is. */
    { char s[24]; rtcOk = rtcReadStamp(s, sizeof(s)); }

    Serial.print("Barometer      "); Serial.println(baroOk ? "ready" : "not responding");
    Serial.print("Inertial unit  "); Serial.println(imuOk  ? "ready" : "not responding");
    if (rtcOk) { char s[24]; rtcReadStamp(s, sizeof(s));
                 Serial.print("Clock          "); Serial.println(s); }
    else       Serial.println("Clock          not responding");
    Serial.print("Pad pressure   "); Serial.print(padPa, 1); Serial.println(" Pa");

    sdBegin();

    while (!startRadio()) {
        Serial.println("Trying the radio again in two seconds");
        delay(2000);
    }

    Serial.print("Packet size ");
    Serial.print(PacketBytes);
    Serial.print(" bytes, airtime ");
    Serial.print(airtime(PacketBytes), 0);
    Serial.print(" ms, slot ");
    Serial.print(TxPeriodMs);
    Serial.println(" ms with a listen window every slot");

    showHelp();

    state     = StateIdle;
    stateAt   = millis();
    nextTxMs  = millis();
    nextLogMs = millis();
}

void loop() {
    readSerial();
    pollGnss();
    runRadio();

    /* record to the card once a second, whatever the radio is doing, so the
     * whole session is on the card with a real timestamp */
    uint32_t now = millis();
    if (cardOk && (int32_t)(now - nextLogMs) >= 0) {
        nextLogMs = now + LogPeriodMs;
        if (state != StateSending) readSensors();   /* fresh unless mid packet */
        logRow();
    }
}
