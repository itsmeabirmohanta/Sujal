/* =============================================================================
 * test_gnss.ino — Quectel L89 satellite receiver (Airoha AG3335M inside)
 * Team 2026-INSPACe-CAN-7USAT-025 · Teensy 4.1
 * =============================================================================
 * Standalone. Nothing from vritra_fc/ is included.
 * Serial Monitor at 115200. Line ending "Newline" or "Both NL & CR" both work.
 *
 * PURPOSE. This is a ground up bring up tool, not a fix test. It assumes
 * nothing about the wiring, the baud rate, or which port the module is on.
 * Every one of those is measured, not guessed:
 *
 *   - PORT AND BAUD are found by scanning, not assumed to be Serial2 at 9600.
 *   - RAW BYTES can be watched directly in hex, before any NMEA parsing, so a
 *     wiring fault (dead line, inverted line, wrong voltage) looks different
 *     from a live line carrying garbage.
 *   - EVERY SATELLITE THE ANTENNA CAN SEE is listed with its own signal
 *     strength, from GSV, which is not the same question as "does the
 *     receiver have a fix" from GGA. A receiver can see satellites and still
 *     report no fix (bad geometry, too few for a solution); those are two
 *     different faults and this sketch tells them apart.
 *   - EVERY COMMAND SENT IS CHECKED FOR AN ACKNOWLEDGEMENT. A command that is
 *     never acknowledged either did not arrive or was rejected, and that is
 *     not the same as a command that was accepted but has no visible effect.
 *
 * THE MODULE IS AN AIROHA AG3335M, NOT A TESEO. It speaks the PAIR command
 * set natively, plus a subset of PMTK. $PAIR066 is the command that actually
 * turns on the Indian constellation; the $PMTK353 that gets copied around
 * forums enables BeiDou on this part, not NavIC.
 *
 * THE RECEIVE BUFFER MUST BE ENLARGED. The default is 64 bytes. A single GSV
 * burst is longer than that, so characters are dropped mid sentence and the
 * parser sees corruption that looks like a bad antenna. 1024 bytes is enough.
 *
 * NO delay() IN THE READ PATH. At 9600 baud a byte arrives roughly every
 * millisecond; a delay of any length in the wrong place loses sentences.
 * ========================================================================== */
#include <Arduino.h>

#define PIN_GNSS_RST    6       /* active low, idle high */
/* Serial2 = RX pin 7, TX pin 8 — the rev 2026-08 harness.
 * Serial1 = RX pin 0, TX pin 1 — the older harness, kept as a fallback. */

static const uint32_t BAUD_LIST[] = { 9600, 4800, 19200, 38400, 57600, 115200 };
static const uint8_t  BAUD_COUNT  = sizeof(BAUD_LIST) / sizeof(BAUD_LIST[0]);

static uint8_t rxBufA[1024];
static uint8_t rxBufB[1024];

static HardwareSerial* gnssPort = &Serial2;
static uint32_t        gnssBaud = 9600;
static bool             rawMode = false;

/* ---------- raw traffic counters -------------------------------------------
 * These answer "is anything arriving at all" before a single NMEA field is
 * trusted. A wiring fault and a sky with no satellites look identical once
 * you only watch parsed fix data; they do not look identical here. */
static uint32_t bytesSeen      = 0;
static uint32_t sentencesOk    = 0;
static uint32_t sentencesBad   = 0;
static char     lastSentence[100] = "";

/* ---------- acknowledgement log ---------------------------------------------
 * $PAIR001,<cmd>,<result> and $PMTKnnn,<cmd>,<flag> answer every command sent.
 * Keep the last few so a SEND that silently fails is still visible later. */
#define ACK_LOG_N 8
static char    ackLog[ACK_LOG_N][48];
static uint8_t ackLogCount = 0;
static void logAck(const char* text) {
    strncpy(ackLog[ackLogCount % ACK_LOG_N], text, sizeof(ackLog[0]) - 1);
    ackLog[ackLogCount % ACK_LOG_N][sizeof(ackLog[0]) - 1] = '\0';
    ackLogCount++;
    Serial.print("  Acknowledged  ");
    Serial.println(text);
}

/* ---------- satellites in view, per constellation ---------------------------
 * GSV arrives as several sentences per constellation per second (message 1 of
 * N, 2 of N, and so on), four satellites per sentence. Table is cleared for a
 * system the moment its message 1 arrives, then refilled as the rest of that
 * system's messages come in, so what you read is always the latest complete
 * round, never a mix of two different seconds. */
struct SatEntry { uint8_t prn; uint8_t elev; uint16_t azim; uint8_t snr; bool tracked; };
#define SATS_PER_SYS 16
struct SysTable { char sysChar; const char* name; SatEntry sat[SATS_PER_SYS]; uint8_t inView; };
static SysTable sys[6] = {
    { 'P', "GPS    ", {}, 0 },
    { 'L', "GLONASS", {}, 0 },
    { 'A', "Galileo", {}, 0 },
    { 'B', "BeiDou ", {}, 0 },
    { 'Q', "QZSS   ", {}, 0 },
    { 'I', "NavIC  ", {}, 0 },
};
/* ---------- GGA / GSA / RMC state -------------------------------------------
 * These carry the receiver's OWN VERDICT: does it consider itself fixed, and
 * with what quality. GSV above is what the antenna sees; this is what the
 * receiver's solver decided to do with it. */
static double   lat = 0, lon = 0;
static float    altM = 0, geoid = 0, speedMs = 0;
static uint32_t utc = 0;
static uint8_t  satsUsed = 0, fixQuality = 0;
static uint8_t  fixType = 1;              /* 1 no fix, 2 2D, 3 3D, from GSA   */
static float    pdop = 99, hdop = 99, vdop = 99;
static bool     haveFix = false;
static char     usedPrns[64] = "";        /* satellite ids the solver used   */

/* ---------- bring up bookkeeping --------------------------------------------
 * time to first fix and position stability, same acceptance numbers as the
 * competition guideline: six or more satellites, fix inside sixty seconds,
 * position steady to a ten thousandth of a degree once fixed. */
static uint32_t bootMs = 0, ttffMs = 0;
static bool     firstFix = false;
static double   lat0 = 0, lon0 = 0, worstDev = 0;
static bool     wasFixed = false;
static uint32_t dropouts = 0;

/* ---------- NMEA line assembly ---------------------------------------------- */
static char    line[128];
static uint8_t linePos = 0;

static double toDecimalDegrees(const char* t, char hemi) {
    const double v = atof(t);
    const int    deg = (int)(v / 100.0);
    const double mins = v - deg * 100.0;
    double out = deg + mins / 60.0;
    if (hemi == 'S' || hemi == 'W') out = -out;
    return out;
}

static void parseGSV(char* talker, char** f, int n) {
    /* talker looks like "$GPGSV" / "$GLGSV" / "$GIGSV" and so on. Character
     * after the $ and before "GSV" tells the constellation. */
    char sysChar = talker[1] == 'G' && talker[2] == 'I' ? 'I' : talker[2];
    SysTable* t = nullptr;
    for (uint8_t i = 0; i < 6; i++) if (sys[i].sysChar == sysChar) { t = &sys[i]; break; }
    if (!t || n < 4) return;

    uint8_t totalMsgs = (uint8_t)atoi(f[1]);
    uint8_t msgNum     = (uint8_t)atoi(f[2]);
    (void)totalMsgs;
    t->inView = (uint8_t)atoi(f[3]);

    if (msgNum == 1) {
        for (uint8_t i = 0; i < SATS_PER_SYS; i++) t->sat[i].tracked = false;
    }

    /* four satellites per sentence starting at field 4, each PRN/elev/azim/snr */
    uint8_t slot = (uint8_t)((msgNum - 1) * 4);
    for (int i = 4; i + 3 < n && slot < SATS_PER_SYS; i += 4, slot++) {
        if (!f[i][0]) continue;
        t->sat[slot].prn     = (uint8_t)atoi(f[i]);
        t->sat[slot].elev    = (uint8_t)atoi(f[i + 1]);
        t->sat[slot].azim    = (uint16_t)atoi(f[i + 2]);
        t->sat[slot].snr     = f[i + 3][0] ? (uint8_t)atoi(f[i + 3]) : 0;
        t->sat[slot].tracked = true;
    }
}

static void parseGSA(char** f, int n) {
    if (n < 18) return;
    fixType = (uint8_t)atoi(f[2]);            /* 1 no fix, 2 2D, 3 3D          */
    uint8_t p = 0;
    usedPrns[0] = '\0';
    for (int i = 3; i <= 14 && i < n; i++) {
        if (f[i][0]) {
            char tmp[6];
            snprintf(tmp, sizeof(tmp), "%s%s", p ? "," : "", f[i]);
            strncat(usedPrns, tmp, sizeof(usedPrns) - strlen(usedPrns) - 1);
            p++;
        }
    }
    pdop = atof(f[15]);
    hdop = atof(f[16]);
    vdop = atof(f[17]);
}

static void parseGGA(char** f, int n) {
    if (n < 12) return;
    utc        = (uint32_t)atof(f[1]);
    fixQuality = (uint8_t)atoi(f[6]);
    satsUsed   = (uint8_t)atoi(f[7]);
    altM       = atof(f[9]);
    if (n > 11) geoid = atof(f[11]);
    if (fixQuality > 0 && f[2][0]) {
        lat = toDecimalDegrees(f[2], f[3][0]);
        lon = toDecimalDegrees(f[4], f[5][0]);
        haveFix = true;
    } else {
        haveFix = false;
    }
}

static void parseRMC(char** f, int n) {
    if (n < 8) return;
    speedMs = atof(f[7]) * 0.514444f;    /* knots to metres per second        */
}

static void dispatch(char* s) {
    /* keep an unmodified copy before the string gets chopped up by strtok */
    strncpy(lastSentence, s, sizeof(lastSentence) - 1);
    lastSentence[sizeof(lastSentence) - 1] = '\0';

    char* f[24];
    int n = 0;
    f[n++] = s;
    for (char* p = s; *p && n < 24; p++)
        if (*p == ',' || *p == '*') { *p = '\0'; f[n++] = p + 1; }

    char* tag = f[0];
    if      (n > 9  && strstr(tag, "GGA")) parseGGA(f, n);
    else if (n > 17 && strstr(tag, "GSA")) parseGSA(f, n);
    else if (n > 7  && strstr(tag, "RMC")) parseRMC(f, n);
    else if (n > 3  && strstr(tag, "GSV")) parseGSV(tag, f, n);
    else if (!strncmp(tag, "$PAIR001", 8) || !strncmp(tag, "$PAIR", 5)) {
        char buf[48]; snprintf(buf, sizeof(buf), "%s,%s,%s", tag, n > 1 ? f[1] : "", n > 2 ? f[2] : "");
        logAck(buf);
    }
    else if (!strncmp(tag, "$PMTK001", 8)) {
        char buf[48]; snprintf(buf, sizeof(buf), "%s,%s,%s", tag, n > 1 ? f[1] : "", n > 2 ? f[2] : "");
        logAck(buf);
    }
}

static void handleByte(char c) {
    bytesSeen++;
    if (rawMode) {
        if (c >= 32 && c < 127) Serial.write(c);
        else { Serial.print('['); Serial.print((uint8_t)c, HEX); Serial.print(']'); }
    }
    if (c == '$') { linePos = 0; line[linePos++] = c; return; }
    if (linePos == 0) return;
    if (c == '\r' || c == '\n') {
        line[linePos] = '\0';
        /* checksum: XOR of every byte between $ and *, compared to the two
         * hex digits after *. A sentence that fails this is corrupted in
         * transit, not merely unparsed — say so, do not just drop it. */
        char* star = strchr(line, '*');
        if (star && strlen(star) >= 3) {
            uint8_t want = (uint8_t)strtoul(star + 1, nullptr, 16);
            uint8_t got = 0;
            for (char* p = line + 1; p < star; p++) got ^= (uint8_t)*p;
            if (got == want) { sentencesOk++; *star = '\0'; dispatch(line); }
            else sentencesBad++;
        }
        linePos = 0;
        return;
    }
    if (linePos < sizeof(line) - 1) line[linePos++] = c;
    else linePos = 0;
}

static void pump() {
    while (gnssPort->available()) handleByte((char)gnssPort->read());
}

static void sendCmd(const char* body) {
    uint8_t ck = 0;
    for (const char* p = body; *p; p++) ck ^= (uint8_t)*p;
    Serial.print("  Sending  $"); Serial.print(body); Serial.print('*');
    if (ck < 16) Serial.print('0');
    Serial.println(ck, HEX);
    gnssPort->printf("$%s*%02X\r\n", body, ck);
}

static void resetPulse() {
    Serial.println("  Resetting: pin 6 low for 100 ms, then released high");
    digitalWrite(PIN_GNSS_RST, LOW);
    delay(100);
    digitalWrite(PIN_GNSS_RST, HIGH);
    bootMs = millis();
    firstFix = false; ttffMs = 0; worstDev = 0; haveFix = false;
    satsUsed = 0; fixType = 1;
    bytesSeen = 0; sentencesOk = 0; sentencesBad = 0;
}

static void openPort(HardwareSerial* port, uint32_t baud) {
    if (gnssPort != port) gnssPort->end();
    gnssPort = port;
    gnssBaud = baud;
    /* addMemoryForRead() is not part of the HardwareSerial base interface,
     * only the concrete Serial1 / Serial2 objects have it, so it has to be
     * called on the named object directly, never through the base pointer. */
    if (port == &Serial2) { Serial2.begin(baud); Serial2.addMemoryForRead(rxBufA, sizeof(rxBufA)); }
    else                  { Serial1.begin(baud); Serial1.addMemoryForRead(rxBufB, sizeof(rxBufB)); }
    Serial.print("  Port now ");
    Serial.print(port == &Serial2 ? "Serial2, pins 7 and 8" : "Serial1, pins 0 and 1");
    Serial.print(" at ");
    Serial.print(baud);
    Serial.println(" baud");
}

/* Try every port and baud combination, 700 ms each, watching for a raw '$'.
 * This is the answer to "which wire and which speed" with no guessing. */
static void scanAll() {
    Serial.println();
    Serial.println("  Scanning every port and baud rate, about half a minute total");
    Serial.println("  Port      Baud      Result");
    HardwareSerial* ports[2] = { &Serial2, &Serial1 };
    const char* names[2] = { "Serial2 ", "Serial1 " };
    HardwareSerial* foundPort = nullptr;
    uint32_t foundBaud = 0;

    for (uint8_t pIdx = 0; pIdx < 2 && !foundPort; pIdx++) {
        for (uint8_t bIdx = 0; bIdx < BAUD_COUNT; bIdx++) {
            ports[pIdx]->begin(BAUD_LIST[bIdx]);
            uint32_t until = millis() + 700;
            uint32_t seen = 0;
            bool sawDollar = false;
            while (millis() < until) {
                if (ports[pIdx]->available()) {
                    char c = (char)ports[pIdx]->read();
                    seen++;
                    if (c == '$') sawDollar = true;
                }
            }
            Serial.print("  "); Serial.print(names[pIdx]);
            Serial.print("  "); Serial.print(BAUD_LIST[bIdx]);
            for (int pad = 8 - (int)String(BAUD_LIST[bIdx]).length(); pad > 0; pad--) Serial.print(' ');
            Serial.print("  ");
            if (sawDollar) { Serial.print("sentence start seen, "); Serial.print(seen); Serial.println(" bytes in the window"); }
            else if (seen)  { Serial.print(seen); Serial.println(" bytes, but never a $ — wrong baud, this is noise"); }
            else             Serial.println("nothing");
            if (sawDollar && !foundPort) { foundPort = ports[pIdx]; foundBaud = BAUD_LIST[bIdx]; }
            ports[pIdx]->end();
        }
    }
    Serial.println();
    if (foundPort) {
        Serial.println("  Found it. Switching to that port and baud now.");
        openPort(foundPort, foundBaud);
    } else {
        Serial.println("  Nothing answered on any port or baud. That points at power,");
        Serial.println("  the reset line, or the physical wire, not software.");
        openPort(&Serial2, 9600);
    }
}

static void printSatTable() {
    Serial.println();
    Serial.println("  Satellites the antenna can see right now, from GSV");
    Serial.println("  System    PRN   Elevation  Azimuth  Signal");
    uint16_t totalTracked = 0;
    for (uint8_t s = 0; s < 6; s++) {
        if (sys[s].inView == 0) continue;
        Serial.print("  "); Serial.print(sys[s].name);
        Serial.print("  in view total reported  "); Serial.println(sys[s].inView);
        for (uint8_t i = 0; i < SATS_PER_SYS; i++) {
            if (!sys[s].sat[i].tracked) continue;
            totalTracked++;
            Serial.print("             ");
            Serial.print(sys[s].sat[i].prn);
            Serial.print("     ");
            Serial.print(sys[s].sat[i].elev);
            Serial.print(" deg     ");
            Serial.print(sys[s].sat[i].azim);
            Serial.print(" deg    ");
            if (sys[s].sat[i].snr) { Serial.print(sys[s].sat[i].snr); Serial.println(" dB-Hz"); }
            else Serial.println("not tracked yet");
        }
    }
    if (totalTracked == 0) {
        Serial.println("  None. Either no GSV sentences have arrived yet, or the antenna");
        Serial.println("  genuinely has no satellites in view from here.");
    }
    Serial.print("  Satellites the solver is actually using  ");
    Serial.println(usedPrns[0] ? usedPrns : "none");
}

static void printStatus() {
    Serial.println();
    Serial.println("  ---- status --------------------------------------------------");
    Serial.print("  Port             "); Serial.println(gnssPort == &Serial2 ? "Serial2, pins 7 and 8" : "Serial1, pins 0 and 1");
    Serial.print("  Baud             "); Serial.println(gnssBaud);
    Serial.print("  Raw byte view    "); Serial.println(rawMode ? "on" : "off");
    Serial.print("  Bytes seen       "); Serial.println(bytesSeen);
    Serial.print("  Sentences good   "); Serial.println(sentencesOk);
    Serial.print("  Sentences bad checksum  "); Serial.println(sentencesBad);
    Serial.print("  Last sentence    "); Serial.println(lastSentence[0] ? lastSentence : "none yet");
    Serial.print("  Fix type from GSA  ");
    Serial.println(fixType == 3 ? "3D" : fixType == 2 ? "2D" : "none");
    Serial.print("  PDOP / HDOP / VDOP  ");
    Serial.print(pdop, 1); Serial.print(" / "); Serial.print(hdop, 1); Serial.print(" / "); Serial.println(vdop, 1);
    Serial.print("  Fix from GGA     "); Serial.println(haveFix ? "yes" : "no");
    Serial.print("  Satellites used  "); Serial.println(satsUsed);

    Serial.println("  Acknowledgements received:");
    if (ackLogCount == 0) Serial.println("    none yet");
    else {
        uint8_t start = ackLogCount > ACK_LOG_N ? ackLogCount - ACK_LOG_N : 0;
        for (uint8_t i = start; i < ackLogCount; i++) {
            Serial.print("    "); Serial.println(ackLog[i % ACK_LOG_N]);
        }
    }

    if (bytesSeen == 0) {
        Serial.println("  Nothing has arrived at all. Run SCAN. If SCAN also finds");
        Serial.println("  nothing, the fault is power, the reset line, or the wire —");
        Serial.println("  not a setting in this sketch.");
    } else if (sentencesOk == 0 && sentencesBad > 0) {
        Serial.println("  Bytes are arriving and framing as sentences but every");
        Serial.println("  checksum fails. That is bit errors in transit, usually a");
        Serial.println("  wrong baud rate or a marginal voltage level, not a dead link.");
    } else if (sentencesOk == 0) {
        Serial.println("  Bytes are arriving but nothing has framed into a full");
        Serial.println("  sentence yet. Give it a moment, or turn on RAW to look at");
        Serial.println("  the bytes directly.");
    }
    Serial.println("  -----------------------------------------------------------------");
}

static void printHelp() {
    Serial.println();
    Serial.println("  Commands, type one and press enter:");
    Serial.println("    SCAN           try every port and baud, lock to whichever answers");
    Serial.println("    RAW            toggle raw byte view, printable as text, else [hex]");
    Serial.println("    RESET          pulse the reset line and start a fresh cold start timer");
    Serial.println("    PORT1          force Serial1, pins 0 and 1, current baud");
    Serial.println("    PORT2          force Serial2, pins 7 and 8, current baud");
    Serial.println("    BAUD <number>  force a specific baud rate on the current port");
    Serial.println("    SEND <text>    send a raw command body, checksum added automatically");
    Serial.println("                   example  SEND PAIR066,1,1,1,0,0,1");
    Serial.println("    SATS           list every satellite currently in view with signal");
    Serial.println("    STATUS         full state dump");
    Serial.println("    HELP           this list");
    Serial.println();
}

static void handleCommand(String cmd) {
    cmd.trim();
    String upper = cmd; upper.toUpperCase();

    if (upper == "SCAN")   { scanAll(); return; }
    if (upper == "RAW")    { rawMode = !rawMode; Serial.print("  Raw byte view now "); Serial.println(rawMode ? "on" : "off"); return; }
    if (upper == "RESET")  { resetPulse(); return; }
    if (upper == "PORT1")  { openPort(&Serial1, gnssBaud); return; }
    if (upper == "PORT2")  { openPort(&Serial2, gnssBaud); return; }
    if (upper == "SATS")   { printSatTable(); return; }
    if (upper == "STATUS") { printStatus(); return; }
    if (upper == "HELP")   { printHelp(); return; }
    if (upper.startsWith("BAUD ")) {
        uint32_t b = (uint32_t)upper.substring(5).toInt();
        if (b) openPort(gnssPort, b);
        else Serial.println("  That is not a number.");
        return;
    }
    if (upper.startsWith("SEND ")) {
        sendCmd(cmd.substring(5).c_str());
        return;
    }
    Serial.println("  Not a recognised command. Type HELP.");
}

void setup() {
    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 3000) { }

    pinMode(PIN_GNSS_RST, OUTPUT);
    digitalWrite(PIN_GNSS_RST, HIGH);          /* release reset, idle state    */

    Serial.println();
    Serial.println("  Satellite receiver bring up — Quectel L89, Airoha AG3335M inside");
    Serial.println("  -----------------------------------------------------------------");
    Serial.println("  No assumptions. Port, baud and sentence set are all measured.");
    Serial.println();

    openPort(&Serial2, 9600);
    delay(300);
    Serial.println("  If nothing below ever shows a sentence, type SCAN.");
    Serial.println();

    sendCmd("PAIR066,1,1,1,0,0,1");    /* GPS GLONASS Galileo NavIC, no BeiDou/QZSS */
    delay(80);
    sendCmd("PAIR050,1000");           /* one position report per second   */
    delay(80);

    bootMs = millis();
    printHelp();
}

void loop() {
    pump();

    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        handleCommand(cmd);
    }

    static uint32_t lastPrint = 0;
    if (millis() - lastPrint < 1000) return;
    lastPrint = millis();

    if (wasFixed && !haveFix) dropouts++;
    wasFixed = haveFix;

    if (haveFix && !firstFix) {
        firstFix = true;
        ttffMs = millis() - bootMs;
        lat0 = lat; lon0 = lon;
        Serial.println();
        Serial.print("  FIRST FIX after "); Serial.print(ttffMs / 1000.0f, 1); Serial.println(" seconds");
        Serial.println(ttffMs < 60000UL ? "  PASS  inside the sixty second target"
                                         : "  FAIL  slower than sixty seconds");
        Serial.println();
    }

    Serial.println();
    if (haveFix) {
        const double dev = max(fabs(lat - lat0), fabs(lon - lon0));
        if (dev > worstDev) worstDev = dev;

        Serial.print("  Latitude       "); Serial.println(lat, 7);
        Serial.print("  Longitude      "); Serial.println(lon, 7);
        Serial.print("  Altitude       "); Serial.print(altM, 1); Serial.println(" m above mean sea level");
        Serial.print("  Geoid offset   "); Serial.print(geoid, 1); Serial.println(" m");
        Serial.print("  Ground speed   "); Serial.print(speedMs, 2); Serial.println(" m/s");
        Serial.print("  Satellites used "); Serial.println(satsUsed);
        Serial.print("  Fix type        "); Serial.println(fixType == 3 ? "3D" : "2D");
        Serial.print("  HDOP / VDOP     "); Serial.print(hdop, 1); Serial.print(" / "); Serial.println(vdop, 1);
        Serial.print("  Receiver time   "); Serial.print(utc); Serial.println(" UTC as hhmmss");
        Serial.print("  Worst drift     "); Serial.print(worstDev, 7); Serial.print(" deg, about ");
        Serial.print(worstDev * 111320.0, 1); Serial.println(" m");
        Serial.print("  Dropouts        "); Serial.println(dropouts);

        Serial.println(satsUsed >= 6 ? "  PASS  six or more satellites in the solution"
                                      : "  WARN  fewer than six satellites");
        Serial.println(worstDev <= 0.0001 ? "  PASS  position steady to 0.0001 degrees"
                                           : "  WARN  position wandering beyond 0.0001 degrees");
        if (hdop > 5.0f) Serial.println("  WARN  geometry is poor, the fix is weak");
    } else {
        Serial.print("  Searching for "); Serial.print((millis() - bootMs) / 1000.0f, 0);
        Serial.print(" s, port "); Serial.print(gnssPort == &Serial2 ? "Serial2" : "Serial1");
        Serial.print(" at "); Serial.print(gnssBaud); Serial.println(" baud");
        Serial.print("  Bytes seen "); Serial.print(bytesSeen);
        Serial.print(", sentences good "); Serial.print(sentencesOk);
        Serial.print(", sentences bad "); Serial.println(sentencesBad);

        uint16_t inView = 0;
        for (uint8_t s = 0; s < 6; s++) inView += sys[s].inView;
        Serial.print("  Satellites reported in view (any system)  "); Serial.println(inView);

        if (bytesSeen == 0) {
            Serial.println("  Nothing has arrived. Type SCAN to try every port and baud.");
        } else if (sentencesOk == 0 && sentencesBad > 0) {
            Serial.println("  Sentences are arriving but every checksum fails — likely the");
            Serial.println("  wrong baud rate. Type SCAN.");
        } else if (inView == 0) {
            Serial.println("  The receiver is talking correctly and sees zero satellites.");
            Serial.println("  That is the sky or the antenna, not the wiring or the code.");
            Serial.println("  Take it outside, flat, with nothing overhead.");
        } else {
            Serial.println("  The antenna sees satellites (type SATS to list them) but the");
            Serial.println("  solver has not produced a fix yet. Normal for the first");
            Serial.println("  30-60 seconds; if it stays this way, check HDOP with STATUS.");
        }
    }
}
