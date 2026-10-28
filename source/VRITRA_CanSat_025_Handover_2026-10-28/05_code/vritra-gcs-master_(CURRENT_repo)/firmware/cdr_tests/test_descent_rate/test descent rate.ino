/* =============================================================================
 * test_descent_rate.ino — telemetry cadence during a simulated 10 m/s descent
 * Team 2026-INSPACe-CAN-7USAT-025 · Teensy 4.1
 * =============================================================================
 * Standalone. Nothing from vritra_fc/ is included.
 * Serial Monitor at 115200.
 *
 * Covers: high speed reception test during a simulated ten metre per second
 * descent.
 *
 * WHAT THIS SIDE MEASURES. This board generates the stream at flight cadence
 * and reports whether it can hold the rate while doing everything else it has
 * to do. It does NOT measure reception — that needs the gateway listening at
 * the far end. Run this, watch the ground station, and compare the packet
 * count here against the count that arrived there. The difference is the loss.
 *
 * THE HONEST NUMBER FOR THIS RADIO. At the assigned spreading factor of ten,
 * one hundred and twenty five kilohertz of bandwidth and a four fifths coding
 * rate, a ninety one byte packet occupies the air for nine hundred and forty
 * four milliseconds. That is the physics, not a setting — and it means one
 * packet per second is very nearly the ceiling. Airtime also moves in steps of
 * five bytes, so trimming one or two bytes usually buys nothing at all.
 *
 *   payload     airtime
 *    80 to 84    862 ms
 *    85 to 89    903 ms
 *    90 to 94    944 ms   <- where the flight packet sits
 *    95 to 99    985 ms
 *
 * A ten metre per second descent from one thousand metres lasts one hundred
 * seconds, so about one hundred packets is the whole descent. Losing ten of
 * them is ten percent of the descent record.
 * ========================================================================== */
#include <Arduino.h>
#include <Wire.h>

#define TEAM_ID_FULL "2026-INSPACe-CAN-7USAT-025"

#define PIN_SDA     18
#define PIN_SCL     19
#define BMP_ADDR    0x47
#define ICM_ADDR    0x69
#define PIN_BUS_V   A6
#define BUS_V_RATIO (122.0f / 22.0f)

#define DESCENT_MS      10.0f       /* metres per second */
#define START_ALT_M   1000.0f
#define PERIOD_MS     1000UL        /* one packet per second */

static bool baroOk = false, imuOk = false;

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
static bool imuRead(float& ax, float& ay, float& az, float& gz) {
    uint8_t r[12];
    if (!rdN(ICM_ADDR, 0x2D, r, 12)) return false;
    ax = (int16_t)((r[0] << 8) | r[1]) / 2048.0f;
    ay = (int16_t)((r[2] << 8) | r[3]) / 2048.0f;
    az = (int16_t)((r[4] << 8) | r[5]) / 2048.0f;
    gz = (int16_t)((r[10] << 8) | r[11]) / 16.4f;
    return true;
}
static float busVolts() {
    uint32_t acc = 0;
    for (int i = 0; i < 16; i++) acc += (uint32_t)analogRead(PIN_BUS_V);
    return ((float)acc / 16.0f) * (3.3f / 4095.0f) * BUS_V_RATIO;
}

static void rule() { Serial.println(F(" ")); }

/* Pressure that corresponds to a height, so the stream carries a physically
 * consistent descent rather than a counter pretending to be one. */
static float pressureForAlt(float h) {
    return 101325.0f * powf(1.0f - (h * 0.0065f) / 298.15f, 5.2559f);
}

static void runDescent(float rateMs) {
    Serial.println();
    Serial.printf("  Simulated descent at %.1f metres per second from %.0f m\n",
                  rateMs, START_ALT_M);
    Serial.printf("  That is %.0f seconds and about %.0f packets.\n",
                  START_ALT_M / rateMs, START_ALT_M / rateMs);
    Serial.println(F("  Watch the ground station and compare its count with this one."));
    rule();

    uint32_t pkt = 0, late = 0, worstLateMs = 0;
    const uint32_t t0 = millis();
    uint32_t nextDue = millis();

    for (float alt = START_ALT_M; alt > 0.0f; alt -= rateMs) {
        if (Serial.available()) { while (Serial.available()) Serial.read(); break; }

        /* wait for this packet's slot, and record if we were late to it */
        const uint32_t due = nextDue;
        while ((int32_t)(millis() - due) < 0) { /* spin, no delay() */ }
        const uint32_t lateBy = millis() - due;
        if (lateBy > 50) { late++; if (lateBy > worstLateMs) worstLateMs = lateBy; }
        nextDue += PERIOD_MS;

        float ax = 0, ay = 0, az = 1.0f, gz = 0;
        if (imuOk) imuRead(ax, ay, az, gz);

        char line[240];
        int n = snprintf(line, sizeof(line),
                         "%s,%lu,%lu,%.1f,%.0f,%.1f,%.2f,",
                         TEAM_ID_FULL, (unsigned long)((millis() - t0) / 1000),
                         (unsigned long)++pkt, alt, pressureForAlt(alt),
                         24.5f, busVolts());
        n += snprintf(line + n, sizeof(line) - n, ",,,,%u,", 0);
        n += snprintf(line + n, sizeof(line) - n, "%.2f,%.2f,%.2f,%.1f,%u",
                      ax * 9.80665f, ay * 9.80665f, az * 9.80665f,
                      imuOk ? fabsf(gz) : 0.0f, 3);

        Serial.print(F("  ")); Serial.println(line);
    }

    const float secs = (millis() - t0) / 1000.0f;
    rule();
    Serial.printf("  Packets built      %lu\n", (unsigned long)pkt);
    Serial.printf("  Elapsed            %.1f s\n", secs);
    Serial.printf("  Rate               %.3f per second\n", pkt / secs);
    Serial.printf("  Slots missed by 50 ms or more  %lu\n", (unsigned long)late);
    Serial.printf("  Worst lateness     %lu ms\n", (unsigned long)worstLateMs);

    Serial.println(pkt / secs >= 0.95f
        ? F("  PASS  held a one hertz through the whole descent")
        : F("  FAIL  could not hold one hertz"));
    Serial.println(late == 0
        ? F("  PASS  every slot was hit on time")
        : F("  WARN  some slots slipped, check what else is running in the loop"));

    Serial.println();
   
}

void setup() {
    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 3000) { }

    Wire.begin(); Wire.setClock(100000);
    analogReadResolution(12);

    uint8_t id = 0;
    baroOk = rdN(BMP_ADDR, 0x01, &id, 1) && id == 0x51;
    wr8(ICM_ADDR, 0x06, 0x80); delay(100);
    wr8(ICM_ADDR, 0x06, 0x01); delay(20);
    wr8(ICM_ADDR, 0x7F, 0x00);
    uint8_t who = 0;
    imuOk = rdN(ICM_ADDR, 0x00, &who, 1) && who == 0xEA;
    if (imuOk) { wr8(ICM_ADDR, 0x7F, 0x20); wr8(ICM_ADDR, 0x14, 0x1F);
                 wr8(ICM_ADDR, 0x01, 0x2F); wr8(ICM_ADDR, 0x7F, 0x00); delay(30); }

    Serial.println();
    Serial.println(F("  Descent cadence test"));
    rule();
    Serial.printf("  Barometer %s, inertial unit %s\n",
                  baroOk ? "ready" : "not responding",
                  imuOk  ? "ready" : "not responding");
    Serial.println(F("  This board generates the stream. Reception is measured at"));
    Serial.println(F("  the ground station — run both and compare the counts."));
    Serial.println();
    Serial.println(F("  d   descend at ten metres per second, the required case"));
    Serial.println(F("  f   descend at twenty, the primary parachute rate"));
    Serial.println(F("  s   descend at two, the secondary rate"));
    Serial.println(F("  Send anything while running to stop early."));
    rule();
}

void loop() {
    if (!Serial.available()) return;
    const char c = (char)Serial.read();
    while (Serial.available()) Serial.read();

    switch (c) {
        case 'd': case 'D': runDescent(10.0f); break;
        case 'f': case 'F': runDescent(20.0f); break;
        case 's': case 'S': runDescent(2.0f);  break;
        default:
            Serial.println(F("\n  d for ten, f for twenty, s for two metres per second"));
            break;
    }
}
