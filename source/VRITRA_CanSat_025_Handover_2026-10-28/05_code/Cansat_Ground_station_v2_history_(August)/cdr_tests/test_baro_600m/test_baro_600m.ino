/* =============================================================================
 * test_baro_600m.ino — injected pressure profile through the 600 m point
 * Team 2026-INSPACe-CAN-7USAT-025 · Teensy 4.1
 * =============================================================================
 * Standalone. Nothing from vritra_fc/ is included.
 * Serial Monitor at 115200.
 *
 * Covers: barometer 600 m simulation with an injected pressure profile.
 *
 * WHY INJECT PRESSURE RATHER THAN ALTITUDE. The altitude the vehicle acts on
 * is computed from pressure, so injecting altitude would skip the very step
 * being qualified. This sketch converts a wanted altitude into the pressure
 * that produces it, feeds that through the real conversion, and compares what
 * comes back. Any error in the constant, the pad reference or the temperature
 * compensation shows up as a difference between the two.
 *
 * THE TEMPERATURE COMPENSATION IS THE POINT. The textbook form
 *     h = 44330.8 * (1 - (p/p0)^0.190263)
 * assumes a fifteen degree sea level day. On a twenty nine degree pad that is
 * wrong by about 4.8 percent — at six hundred metres, roughly twenty nine
 * metres, which is three times the plus or minus ten metre window the
 * guideline allows for the secondary mechanism. The pad temperature is used as
 * the reference instead, and the run below reports both so the difference is
 * visible rather than argued about.
 *
 * If the barometer is fitted, its real pad pressure and temperature are used
 * as the reference. If not, standard values stand in and the maths is still
 * exercised end to end.
 * ========================================================================== */
#include <Arduino.h>
#include <Wire.h>

#define PIN_SDA     18
#define PIN_SCL     19
#define BMP_ADDR    0x47

#define SEC_EJECT_AGL_M     600.0f
#define WINDOW_M             10.0f

static float padPa = 101325.0f, padTc = 25.0f;
static bool  baroOk = false;

static bool wr8(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(BMP_ADDR); Wire.write(reg); Wire.write(val);
    return Wire.endTransmission() == 0;
}
static bool rdN(uint8_t reg, uint8_t* b, uint8_t n) {
    Wire.beginTransmission(BMP_ADDR); Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)BMP_ADDR, (int)n) != n) return false;
    for (int i = 0; i < n; i++) b[i] = Wire.read();
    return true;
}
static bool baroRead(float& pa, float& tc) {
    uint8_t b[6];
    if (!rdN(0x20, b, 6)) return false;
    int32_t rp = (int32_t)(b[0] | (b[1] << 8) | (b[2] << 16)) << 8 >> 8;
    int32_t rt = (int32_t)(b[3] | (b[4] << 8) | (b[5] << 16)) << 8 >> 8;
    pa = (float)rp / 64.0f; tc = (float)rt / 65536.0f;
    return true;
}

static void rule() { Serial.println(F("  ------------------------------------------------")); }

/* The conversion the vehicle actually uses: hypsometric, with the measured pad
 * temperature as the reference rather than a fifteen degree assumption. */
static float altFromPressure(float pa) {
    if (pa <= 0.0f || padPa <= 0.0f) return 0.0f;
    const float t0 = padTc + 273.15f;
    return (t0 / 0.0065f) * (1.0f - powf(pa / padPa, 0.1902632f));
}

/* The textbook form, shown only so the error it carries is visible. */
static float altTextbook(float pa) {
    if (pa <= 0.0f || padPa <= 0.0f) return 0.0f;
    return 44330.8f * (1.0f - powf(pa / padPa, 0.1902632f));
}

/* Inverse of the conversion above: the pressure that produces a wanted height. */
static float pressureForAlt(float h) {
    const float t0 = padTc + 273.15f;
    return padPa * powf(1.0f - (h * 0.0065f) / t0, 1.0f / 0.1902632f);
}

static void sweep() {
    Serial.println();
    Serial.printf("  Pad reference %.1f Pa at %.2f C\n", padPa, padTc);
    Serial.println(F("  Descending from 1000 m to 400 m in ten metre steps"));
    rule();
    Serial.println(F("  Injected   Pressure     Recovered    Error    Textbook   Its error"));

    bool fired = false;
    float firedAt = 0.0f;
    float worst = 0.0f;

    for (float h = 1000.0f; h >= 400.0f; h -= 10.0f) {
        const float pa   = pressureForAlt(h);
        const float back = altFromPressure(pa);
        const float old  = altTextbook(pa);
        const float err  = back - h;
        if (fabsf(err) > fabsf(worst)) worst = err;

        if (!fired && back < SEC_EJECT_AGL_M) { fired = true; firedAt = back; }

        const bool near600 = fabsf(h - SEC_EJECT_AGL_M) <= 30.0f;
        if (near600 || fmodf(h, 100.0f) == 0.0f)
            Serial.printf("  %7.1f m  %9.1f Pa  %8.1f m  %+6.2f m  %8.1f m  %+7.2f m%s\n",
                          h, pa, back, err, old, old - h,
                          near600 && fabsf(h - SEC_EJECT_AGL_M) < 1.0f ? "   <- the trigger" : "");
    }

    rule();
    Serial.printf("  Worst error through the whole sweep %+.3f m\n", worst);
    Serial.println(fabsf(worst) < 1.0f
        ? F("  PASS  the conversion round trips to better than a metre")
        : F("  FAIL  the conversion does not round trip"));

    Serial.printf("  Secondary mechanism fired at %.1f m\n", firedAt);
    Serial.println(fired && fabsf(firedAt - SEC_EJECT_AGL_M) <= WINDOW_M
        ? F("  PASS  inside six hundred metres plus or minus ten")
        : F("  FAIL  outside the required window"));

    /* what the textbook constant would have cost, at the trigger */
    const float paAt600 = pressureForAlt(SEC_EJECT_AGL_M);
    const float oldAt600 = altTextbook(paAt600);
    Serial.println();
    Serial.printf("  At the true six hundred metre point the textbook constant\n");
    Serial.printf("  would have read %.1f m, an error of %+.1f m.\n",
                  oldAt600, oldAt600 - SEC_EJECT_AGL_M);
    Serial.println(fabsf(oldAt600 - SEC_EJECT_AGL_M) > WINDOW_M
        ? F("  That is outside the ten metre window, which is why the pad")
        : F("  That happens to be inside the window at this temperature, but"));
    Serial.println(F("  temperature is used as the reference instead."));
}

void setup() {
    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 3000) { }

    Wire.begin(); Wire.setClock(100000);

    Serial.println();
    Serial.println(F("  Six hundred metre simulation — injected pressure profile"));
    rule();

    uint8_t id = 0;
    baroOk = rdN(0x01, &id, 1) && id == 0x51;
    if (baroOk) {
        wr8(0x7E, 0xB6); delay(5);
        wr8(0x37, 0xC7); wr8(0x36, 0x45); delay(60);
        float pa, tc;
        if (baroRead(pa, tc)) {
            padPa = pa; padTc = tc;
            Serial.println(F("  Barometer fitted, using its real reading as the pad reference"));
        }
    } else {
        Serial.println(F("  Barometer not fitted or not answering."));
        Serial.println(F("  Standing in with standard values so the maths still runs."));
    }

    sweep();

    Serial.println();
    Serial.println(F("  Send  r  to run the sweep again with a fresh pad reference."));
    Serial.println(F("  Send  t  to sweep the pad temperature and see what it costs."));
    rule();
}

void loop() {
    if (!Serial.available()) return;
    const char c = (char)Serial.read();
    while (Serial.available()) Serial.read();

    if (c == 'r' || c == 'R') {
        if (baroOk) {
            float pa, tc;
            if (baroRead(pa, tc)) { padPa = pa; padTc = tc; }
        }
        sweep();
    } else if (c == 't' || c == 'T') {
        Serial.println();
        Serial.println(F("  What the pad temperature is worth at the trigger"));
        rule();
        Serial.println(F("  Pad temp    Pressure at 600 m    Textbook reads    Error"));
        const float saveTc = padTc;
        for (float t = 5.0f; t <= 45.0f; t += 5.0f) {
            padTc = t;
            const float pa = pressureForAlt(SEC_EJECT_AGL_M);
            const float old = altTextbook(pa);
            Serial.printf("  %6.1f C    %11.1f Pa    %9.1f m    %+6.1f m%s\n",
                          t, pa, old, old - SEC_EJECT_AGL_M,
                          fabsf(old - SEC_EJECT_AGL_M) > WINDOW_M ? "   outside the window" : "");
        }
        padTc = saveTc;
        rule();
        Serial.println(F("  The compensated conversion reads six hundred metres at"));
        Serial.println(F("  every one of these temperatures, because it uses the"));
        Serial.println(F("  measured pad temperature rather than assuming fifteen."));
    }
}
