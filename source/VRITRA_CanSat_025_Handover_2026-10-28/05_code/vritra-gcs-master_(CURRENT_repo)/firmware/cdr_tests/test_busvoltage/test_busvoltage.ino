/* =============================================================================
 * test_busvoltage.ino — main bus voltage sense, and the power endurance run
 * Team 2026-INSPACe-CAN-7USAT-025 · Teensy 4.1
 * =============================================================================
 * Standalone. Nothing from vritra_fc/ is included.
 * Serial Monitor at 115200. Prints this measurement only.
 *
 * Covers: power endurance test, and the multimeter cross check.
 *
 * WHAT THIS CAN AND CANNOT MEASURE. It reads the battery bus through the
 * 100k/22k divider on analogue pin A6. It CANNOT read the 3.3 V rail, because
 * that rail is also the reference the converter compares against — it is the
 * ruler, so it always reads full scale and can never measure itself. Put a
 * multimeter on the 3.3 V rail if you need that number for the review. It
 * should sit flat until the regulator drops out, and the moment it sags the
 * processor resets anyway, which ends the test for you.
 *
 * The endurance criterion is therefore the battery: three cells in series,
 * stopping at 9.00 V, which is 3.00 V per cell. Target is two hours or more.
 * ========================================================================== */
#include <Arduino.h>

#define PIN_BUS_V       A6          /* = digital pin 20 */
#define BUS_V_RATIO     (122.0f / 22.0f)   /* 100k over 22k */
#define ADC_BITS        12
#define ADC_AVERAGING   16

#define BATT_CELLS      3
#define BATT_EMPTY_V    9.00f       /* 3.00 V per cell */
#define BATT_FULL_V     12.60f      /* 4.20 V per cell */

static float   vMin = 99.0f, vMax = 0.0f;
static uint32_t startMs = 0;
static bool    enduring = false;
static uint32_t lastLog = 0;

static float busVolts() {
    uint32_t acc = 0;
    for (int i = 0; i < ADC_AVERAGING; i++) acc += (uint32_t)analogRead(PIN_BUS_V);
    const float counts = (float)acc / (float)ADC_AVERAGING;
    return counts * (3.3f / 4095.0f) * BUS_V_RATIO;
}

static uint16_t rawCounts() {
    uint32_t acc = 0;
    for (int i = 0; i < ADC_AVERAGING; i++) acc += (uint32_t)analogRead(PIN_BUS_V);
    return (uint16_t)(acc / ADC_AVERAGING);
}

/* Rough state of charge from resting voltage. Lithium ion is very flat through
 * the middle of its range, so this is indicative, not a fuel gauge. */
static float stateOfCharge(float v) {
    const float pc = (v - BATT_EMPTY_V) / (BATT_FULL_V - BATT_EMPTY_V) * 100.0f;
    return constrain(pc, 0.0f, 100.0f);
}

void setup() {
    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 3000) { }

    analogReadResolution(ADC_BITS);

    Serial.println();
    Serial.println(F("  Bus voltage test"));
    Serial.println(F("  ------------------------------------------------"));
    Serial.printf("  Divider ratio %.4f, from 100k over 22k\n", BUS_V_RATIO);
    Serial.printf("  Converter %d bits against a 3.3 V reference\n", ADC_BITS);
    Serial.printf("  Averaging %d readings\n", ADC_AVERAGING);
    Serial.printf("  Pack is %d cells, empty at %.2f V, full at %.2f V\n",
                  BATT_CELLS, BATT_EMPTY_V, BATT_FULL_V);
    Serial.println();
    Serial.println(F("  Cross check the reading below against a multimeter on the"));
    Serial.println(F("  battery terminals. If they disagree by more than about two"));
    Serial.println(F("  percent the divider resistors are not the values assumed."));
    Serial.println();
    Serial.println(F("  Send  e  to start the endurance run, which prints every"));
    Serial.println(F("  thirty seconds until the pack reaches 9.00 V."));
    Serial.println(F("  ------------------------------------------------"));
}

void loop() {
    if (Serial.available()) {
        const char c = (char)Serial.read();
        while (Serial.available()) Serial.read();
        if (c == 'e' || c == 'E') {
            enduring = true;
            startMs = millis();
            lastLog = 0;
            Serial.println(F("\n  Endurance run started. Leave everything powered."));
            Serial.println(F("  Elapsed        Voltage    Per cell   Charge"));
        }
    }

    const float v = busVolts();
    if (v < vMin) vMin = v;
    if (v > vMax) vMax = v;

    if (enduring) {
        if (millis() - lastLog < 30000UL && lastLog != 0) { delay(200); return; }
        lastLog = millis();
        const float mins = (millis() - startMs) / 60000.0f;
        Serial.printf("  %7.1f min    %6.2f V   %5.3f V    %5.1f %%\n",
                      mins, v, v / BATT_CELLS, stateOfCharge(v));
        if (v <= BATT_EMPTY_V) {
            const float hours = (millis() - startMs) / 3600000.0f;
            Serial.println();
            Serial.printf("  Pack reached %.2f V after %.2f hours\n", v, hours);
            Serial.println(hours >= 2.0f
                ? F("  PASS  two hour endurance requirement met")
                : F("  FAIL  shorter than the two hour requirement"));
            enduring = false;
        }
        return;
    }

    Serial.printf("  Raw counts   %8u of 4095\n", rawCounts());
    Serial.printf("  Bus voltage  %8.3f V\n", v);
    Serial.printf("  Per cell     %8.3f V\n", v / BATT_CELLS);
    Serial.printf("  Charge       %8.1f %%   indicative only\n", stateOfCharge(v));
    Serial.printf("  Seen range   %8.3f to %.3f V\n", vMin, vMax);

    if (v > BATT_FULL_V + 0.3f)
        Serial.println(F("  WARN  higher than a full three cell pack, check the divider"));
    else if (v < 6.0f)
        Serial.println(F("  WARN  far below a three cell pack — running on USB only?"));
    else if (v < BATT_EMPTY_V)
        Serial.println(F("  WARN  below three volts per cell, stop and charge"));
    else
        Serial.println(F("  PASS  plausible for a three cell lithium ion pack"));

    Serial.println();
    delay(1000);
}
