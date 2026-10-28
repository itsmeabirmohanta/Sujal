#include <Arduino.h>
#include <Wire.h>

#define TeamIdFull  "2026-INSPACe-CAN-7USAT-025"

#define PinSda      18
#define PinScl      19
#define BmpAddr     0x47
#define IcmAddr     0x69
#define RtcAddr     0x68
#define PinGnssRst  6
#define PinBusV     A6
#define BusVRatio   (122.0f / 22.0f)

/* BMP585 registers — copied verbatim from Sensors.cpp */
#define BmpTempXlsb   0x1D   /* 6-byte burst: T[0..2] then P[3..5], little-endian */
#define BmpDspIir     0x31   /* on-chip IIR filter — must be forced OFF (0x00).
                               * NOT the same register as 0x30 DSP_CONFIG, which
                               * carries the pressure/temperature compensation
                               * enables and must NEVER be written — zeroing
                               * that one takes the output out of physical
                               * range while every write still ACKs. */
#define BmpOsrConfig  0x36   /* press_en | osr_p | osr_t  */
#define BmpOdrConfig  0x37   /* deep-standby | odr | power-mode  */
#define BmpCmd        0x7E
#define BmpOsrByte    (0x40 | (0x04 << 3) | 0x01)    /* press_en, P x16, T x2 */
#define BmpOdrStandby (0x80 | (0x14 << 2) | 0x00)    /* 25 Hz code, standby     */
#define BmpOdrNormal  (0x80 | (0x14 << 2) | 0x01)    /* 25 Hz, normal/continuous */

/* The CSV header MUST retain underscores — it is parsed by software */
static const char* CsvHeader =
    "TEAM_ID,TIME_S,PKT,ALT_M,PRES_PA,TEMP_C,BUS_V,GNSS_TIME,LAT,LON,"
    "GPS_ALT_M,SATS,ACC_X,ACC_Y,ACC_Z,SPIN_RATE,STATE";

struct FieldSpec { const char* col; const char* resolution; const char* meaning; };
static const FieldSpec fieldSpec[] = {
    { "Team ID",   "Text",        "Team number" },
    { "Time S",    "Seconds",     "Time since power up" },
    { "Packet",    "Integer",     "Packets transmitted" },
    { "Alt M",     "0.1 m",       "Height above the ground" },
    { "Pres Pa",   "1 Pa",        "Atmospheric pressure" },
    { "Temp C",    "0.1 C",       "Temperature" },
    { "Bus V",     "0.01 V",      "Power bus voltage" },
    { "GNSS Time", "HHMMSS",      "Time from the satellite receiver" },
    { "Lat",       "0.0001 deg",  "Latitude from the receiver" },
    { "Lon",       "0.0001 deg",  "Longitude from the receiver" },
    { "GPS Alt M", "0.1 m",       "Altitude from the receiver" },
    { "Sats",      "Integer",     "Satellites connected" },
    { "Acc X",     "m/s squared", "Acceleration first axis" },
    { "Acc Y",     "m/s squared", "Acceleration second axis" },
    { "Acc Z",     "m/s squared", "Acceleration third axis" },
    { "Spin Rate", "deg/s",       "Mechanical gyro spin rate" },
    { "State",     "Integer",     "Flight software state" },
};
static const int specN = sizeof(fieldSpec) / sizeof(fieldSpec[0]);

/* state */
static bool baroOk = false, imuOk = false, rtcOk = false;
static uint32_t pkt = 0;
static float padPa = 101325.0f, padTc = 25.0f;

/* GNSS state */
static uint32_t gnssTime = 0;
static float    gnssLat  = 0.0f, gnssLon = 0.0f, gnssAlt = 0.0f;
static uint8_t  gnssSats = 0;
static bool     gnssFix  = false;
static uint8_t  gnssBuf[1024];
static char     nmeaLine[128];
static uint8_t  nmeaPos = 0;

/* ----------------------------------------------------------------- I2C ----- */
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

/* ---------------------------------------------------------------- BMP585 ---- */
static inline int32_t signExtend24(uint32_t x) {
    x &= 0xFFFFFFu;
    return (x & 0x800000u) ? (int32_t)(x | 0xFF000000u) : (int32_t)x;
}

static bool baroRead(float& tC, float& pPa) {
    uint8_t b[6];
    if (!rdN(BmpAddr, BmpTempXlsb, b, 6)) return false;
    /* b[0..2] = temperature, b[3..5] = pressure, all little-endian */
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

/* ---------------------------------------------------------------- ICM-20948 - */
static bool imuRead(float& ax, float& ay, float& az, float& gz) {
    uint8_t b[12];
    wr8(IcmAddr, 0x7F, 0x00);                /* select bank 0 */
    if (!rdN(IcmAddr, 0x2D, b, 12)) return false;
    ax = (int16_t)((b[0] << 8) | b[1])  / 2048.0f;
    ay = (int16_t)((b[2] << 8) | b[3])  / 2048.0f;
    az = (int16_t)((b[4] << 8) | b[5])  / 2048.0f;
    gz = (int16_t)((b[10] << 8) | b[11]) / 16.4f;
    return true;
}

/* ---------------------------------------------------------------- DS3231 ---- */
static uint8_t bcd(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }

static bool rtcRead(uint8_t& hh, uint8_t& mm, uint8_t& ss,
                    uint8_t& date, uint8_t& mon, uint8_t& yr) {
    uint8_t b[7];
    if (!rdN(RtcAddr, 0x00, b, 7)) return false;
    ss   = bcd(b[0] & 0x7F);
    mm   = bcd(b[1] & 0x7F);
    hh   = bcd(b[2] & 0x3F);
    date = bcd(b[4] & 0x3F);
    mon  = bcd(b[5] & 0x1F);
    yr   = bcd(b[6]);
    return true;
}

static const char* monthName(uint8_t m) {
    static const char* n[] = { "?","Jan","Feb","Mar","Apr","May","Jun",
                                    "Jul","Aug","Sep","Oct","Nov","Dec" };
    return (m >= 1 && m <= 12) ? n[m] : n[0];
}

/* ---------------------------------------------------------------- GNSS ------- */
static float nmeaToDecimal(const char* s, char hemi) {
    if (!s || *s == '\0') return 0.0f;
    float raw = atof(s);
    int deg = (int)(raw / 100);
    float dec = deg + (raw - deg * 100.0f) / 60.0f;
    if (hemi == 'S' || hemi == 'W') dec = -dec;
    return dec;
}

/* Checksum-validated, field-split, fix-gated — same rules as nmeaGGA() in
 * Sensors.cpp. A torn read fails the checksum and is dropped rather than
 * shown as a value; time and position are only believed once fix quality
 * (field 6) is greater than zero, never guessed from a stale field. */
static void parseNmea(char* s) {
    if (strlen(s) < 9) return;
    char* star = strchr(s, '*');
    if (!star || strlen(star) < 3) return;
    uint8_t want = (uint8_t)strtoul(star + 1, nullptr, 16);
    uint8_t got = 0;
    for (char* p = s + 1; p < star; p++) got ^= (uint8_t)*p;
    if (got != want) return;
    *star = '\0';

    char* f[20];
    int n = 0; f[n++] = s;
    for (char* p = s; *p && n < 20; p++)
        if (*p == ',') { *p = '\0'; f[n++] = p + 1; }
    if (n < 2) return;

    if (strcmp(f[0] + 3, "GGA") != 0) return;   /* GGA carries lat/lon/alt/time */
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

/* Byte-at-a-time capture, same shape as gnssPump() — a sentence starts at
 * the '$' seen on the wire, not at whatever happened to be in the buffer. */
static void pollGnss() {
    while (Serial2.available()) {
        char c = (char)Serial2.read();
        if (c == '$') { nmeaPos = 0; nmeaLine[nmeaPos++] = c; continue; }
        if (nmeaPos == 0) continue;
        if (c == '\r' || c == '\n') {
            nmeaLine[nmeaPos] = '\0';
            parseNmea(nmeaLine);
            nmeaPos = 0;
            continue;
        }
        if (nmeaPos < sizeof(nmeaLine) - 1) nmeaLine[nmeaPos++] = c;
        else nmeaPos = 0;                       /* overlong line — discard */
    }
}

/* ----------------------------------------------------------------- ADC ------- */
static float busVolts() {
    uint32_t acc = 0;
    for (int i = 0; i < 16; i++) acc += (uint32_t)analogRead(PinBusV);
    return ((float)acc / 16.0f) * (3.3f / 4095.0f) * BusVRatio;
}

/* ----------------------------------------------------------------- CSV ------- */
static int buildLine(char* out, int cap) {
    float pPa = padPa, tC = padTc, ax = 0, ay = 0, az = 1.0f, gz = 0;
    if (baroOk) baroRead(tC, pPa);
    if (imuOk)  imuRead(ax, ay, az, gz);
    pollGnss();

    float agl = baroOk ? aglFromPressure(pPa) : 0.0f;

    int n = snprintf(out, cap, "%s,%lu,%lu,%.1f,%.0f,%.1f,%.2f,",
                     TeamIdFull, (unsigned long)(millis() / 1000),
                     (unsigned long)++pkt, agl, pPa, tC, busVolts());

    /* Matches nmeaGGA() in Sensors.cpp — TIME, LAT, LON and ALT are one unit,
     * all written together only when fix quality is real, never separately. */
    if (gnssFix)
        n += snprintf(out + n, cap - n, "%06lu,%.7f,%.7f,%.1f,%u,",
                      (unsigned long)gnssTime, gnssLat, gnssLon, gnssAlt, gnssSats);
    else
        n += snprintf(out + n, cap - n, ",,,,%u,", gnssSats);

    n += snprintf(out + n, cap - n, "%.2f,%.2f,%.2f,%.1f,%u",
                  ax * 9.80665f, ay * 9.80665f, az * 9.80665f,
                  imuOk ? fabsf(gz) : 0.0f, 0);
    return n;
}

/* ----------------------------------------------------------------- audit ----- */
static void auditLine(const char* line) {
    char work[260];
    strncpy(work, line, sizeof(work) - 1); work[sizeof(work) - 1] = '\0';
    char* col[24]; int n = 0; col[n++] = work;
    for (char* p = work; *p && n < 24; p++)
        if (*p == ',') { *p = '\0'; col[n++] = p + 1; }

    Serial.println();
    Serial.println("Column Evaluation Table");
    Serial.println("Num  Field Name      Current Value     Resolution      Description");
    for (int i = 0; i < specN; i++) {
        const char* v = (i < n) ? col[i] : "Missing";
        Serial.printf("%-4d %-15s %-17s %-15s %s\n",
                      i + 1, fieldSpec[i].col,
                      (*v == '\0') ? "Empty" : v,
                      fieldSpec[i].resolution, fieldSpec[i].meaning);
    }
    Serial.println();
    Serial.printf("Columns produced: %d. Expected: %d\n", n, specN);
    Serial.println(n == specN ? "PASS Every mandated parameter is present"
                              : "FAIL Column count does not match");
    Serial.println(strcmp(col[0], TeamIdFull) == 0
        ? "PASS Team identifier is correct and first"
        : "FAIL Team identifier is wrong or not first");

    struct { int idx; int dp; const char* why; } dpCheck[] = {
        { 3,  1, "Altitude at a tenth of a metre" },
        { 4,  0, "Pressure at one pascal" },
        { 5,  1, "Temperature at a tenth of a degree" },
        { 6,  2, "Voltage at a hundredth of a volt" },
        { 10, 1, "Satellite altitude at a tenth of a metre" },
    };
    for (unsigned i = 0; i < sizeof(dpCheck) / sizeof(dpCheck[0]); i++) {
        const char* v = (dpCheck[i].idx < n) ? col[dpCheck[i].idx] : "";
        if (*v == '\0') continue;
        const char* dot = strchr(v, '.');
        const int places = dot ? (int)strlen(dot + 1) : 0;
        Serial.printf("%-46s %s\n", dpCheck[i].why,
                      places == dpCheck[i].dp ? "PASS" : "FAIL");
    }
    for (int i = 8; i <= 9; i++) {
        if (i >= n || *col[i] == '\0') {
            Serial.printf("Coordinate column %d empty, no fix yet\n", i - 7);
            continue;
        }
        const char* dot = strchr(col[i], '.');
        const int places = dot ? (int)strlen(dot + 1) : 0;
        Serial.printf("Coordinate at %d decimal places, four required     %s\n",
                      places, places >= 4 ? "PASS" : "FAIL");
    }
}

/* ================================================================= setup ===== */
void setup() {
    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 3000) { }

    /* 100 kHz, not 400 kHz — measured on this exact harness in Config.h:
     * 400 kHz makes the ICM-20948 ACK its address and then return 0xFF on
     * every register read (insufficient edge rate on the 3.3 kohm pull-ups). */
    Wire.begin(); Wire.setClock(100000);
    analogReadResolution(12);

    /* GNSS — a deterministic reset pulse every boot, same as the flight
     * computer: drive RESET_N low, hold past the minimum pulse width, then
     * release high, rather than trusting whatever state the pin was left in. */
    pinMode(PinGnssRst, OUTPUT);
    digitalWrite(PinGnssRst, LOW);
    delay(60);
    digitalWrite(PinGnssRst, HIGH);
    Serial2.addMemoryForRead(gnssBuf, sizeof(gnssBuf));
    Serial2.begin(9600);      /* L89 stays at factory 9600, never changed */

    /* BMP585 — same reset -> standby -> IIR-off -> OSR -> normal sequence as
     * the flight computer's staged init. Writing OSR while the device is
     * still in NORMAL mode (skipping the STANDBY step) and leaving the
     * on-chip IIR filter at its undefined power-on state is what produced
     * the earlier impossible readings (130558 Pa, 127.5 C). */
    uint8_t id = 0;
    baroOk = rdN(BmpAddr, 0x01, &id, 1) && id == 0x51;
    if (baroOk) {
        wr8(BmpAddr, BmpCmd, 0xB6);                delay(10);  /* soft reset settle */
        wr8(BmpAddr, BmpOdrConfig, BmpOdrStandby); delay(5);   /* force standby first */
        wr8(BmpAddr, BmpDspIir, 0x00);                          /* on-chip IIR off */
        wr8(BmpAddr, BmpOsrConfig, BmpOsrByte);
        wr8(BmpAddr, BmpOdrConfig, BmpOdrNormal);  delay(30);  /* first conversion */
        float tC, pPa;
        if (baroRead(tC, pPa)) { padPa = pPa; padTc = tC; }
    }

    /* ICM-20948 */
    wr8(IcmAddr, 0x06, 0x80); delay(100);  /* DEVICE_RESET */
    wr8(IcmAddr, 0x06, 0x01); delay(20);   /* wake, auto clock */
    wr8(IcmAddr, 0x7F, 0x00);              /* bank 0 */
    uint8_t who = 0;
    imuOk = rdN(IcmAddr, 0x00, &who, 1) && who == 0xEA;
    if (imuOk) {
        wr8(IcmAddr, 0x7F, 0x20);          /* bank 2 */
        wr8(IcmAddr, 0x14, 0x1F);          /* accel config: 16g, 4500 Hz DLPF */
        wr8(IcmAddr, 0x01, 0x2F);          /* gyro config: 2000 dps, 361 Hz */
        wr8(IcmAddr, 0x7F, 0x00);          /* back to bank 0 */
        delay(30);
    }

    /* DS3231 — force 24-hour mode before the first real read. If the clock
     * was set through a tool that defaults to 12-hour format, bit 6 of the
     * hour register is set and bit 5 is an AM/PM flag rather than a BCD
     * digit; reading it with the 24-hour mask then adds 20 to the hour on
     * every PM reading (e.g. 8 PM reads back as hour 28). Matches the
     * one-time fix in Sensors.cpp's IS_RTC init stage. */
    uint8_t h12 = 0;
    if (rdN(RtcAddr, 0x02, &h12, 1) && (h12 & 0x40))
        wr8(RtcAddr, 0x02, (uint8_t)(h12 & 0x3F));

    uint8_t rh = 0, rm = 0, rs = 0, rd = 0, rmon = 0, ryr = 0;
    rtcOk = rtcRead(rh, rm, rs, rd, rmon, ryr);

    /* Wait up to 5 s for a GNSS GGA sentence */
    uint32_t gw = millis();
    while (millis() - gw < 5000 && !gnssFix) pollGnss();

    Serial.println();
    Serial.println("Telemetry line format test");
    Serial.println("------------------------------------------------");
    Serial.printf("Barometer      %s\n", baroOk ? "Ready" : "Not responding");
    Serial.printf("Inertial unit  %s\n", imuOk  ? "Ready" : "Not responding");
    if (rtcOk)
        Serial.printf("RTC date and time  %02u %s 20%02u  %02u:%02u:%02u\n",
                      rd, monthName(rmon), ryr, rh, rm, rs);
    else
        Serial.println("RTC  Not responding");
    if (gnssFix)
        Serial.printf("Satellite fix  Yes  %06lu  %.6f  %.6f  %.1f m  %u sats\n",
                      (unsigned long)gnssTime, gnssLat, gnssLon, gnssAlt, gnssSats);
    else
        Serial.printf("Satellite fix  No fix yet  %u sats visible\n", gnssSats);
    Serial.printf("Pad reference  %.1f Pa  %.2f C\n", padPa, padTc);
    Serial.println();
    Serial.println("Header line");
    Serial.println(CsvHeader);

    char line[260];
    int len = buildLine(line, sizeof(line));
    Serial.println();
    Serial.println("One line with real sensor data");
    Serial.println(line);
    Serial.printf("Length %d bytes\n", len);
    auditLine(line);

    Serial.println();
    Serial.println("Send  f  to build a new line");
    Serial.println("Send  s  to stream one line per second");
    Serial.println("Send  r  to recapture the pad pressure reference");
    Serial.println("------------------------------------------------");
}

/* ================================================================= loop ====== */
void loop() {
    pollGnss();
    if (!Serial.available()) return;
    const char c = (char)Serial.read();
    while (Serial.available()) Serial.read();

    char line[260];
    if (c == 'f' || c == 'F') {
        buildLine(line, sizeof(line));
        Serial.println(); Serial.println(line);
        auditLine(line);
    } else if (c == 'r' || c == 'R') {
        float tC, pPa;
        if (baroOk && baroRead(tC, pPa)) {
            padPa = pPa; padTc = tC;
            Serial.printf("\nPad reference recaptured  %.1f Pa  %.2f C\n", padPa, padTc);
        } else {
            Serial.println("\nBarometer not responding");
        }
    } else if (c == 's' || c == 'S') {
        Serial.println("\nStreaming at one per second, send anything to stop\n");
        while (!Serial.available()) {
            pollGnss();
            buildLine(line, sizeof(line));
            Serial.println(line);
            delay(1000);
        }
        while (Serial.available()) Serial.read();
        Serial.println("\nStopped.");
    }
}
