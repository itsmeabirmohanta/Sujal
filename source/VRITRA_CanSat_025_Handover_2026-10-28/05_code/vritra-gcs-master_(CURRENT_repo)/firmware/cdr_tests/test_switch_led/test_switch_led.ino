/* =============================================================================
 * test_switch_led.ino — external power switch and status indicator
 * Team 2026-INSPACe-CAN-7USAT-025 · Teensy 4.1
 * =============================================================================
 * Standalone. Nothing from vritra_fc/ is included.
 * Serial Monitor at 115200.
 *
 * Covers: switch and led test using battery pack and multimeter.
 *
 * WIRING
 *     switch    one side to pin 15, the other to ground
 *               the internal pull up does the rest, so closed reads LOW
 *     LED       pin 14 through a 330 ohm resistor to the anode,
 *               cathode to ground
 *
 * WHY PIN 14 AND NOT THE ON-BOARD LED. The on-board LED is pin 13, which is
 * also the clock line of the SPI0 bus the serial flash sits on. Sharing them
 * makes the indicator flicker during flash access and makes this test
 * ambiguous. The on-board LED is still driven here, as a second opinion.
 *
 * WHY THE SWITCH IS DEBOUNCED. A mechanical contact bounces for a few
 * milliseconds. Without debouncing a single flick prints ten state changes and
 * the count is meaningless, which matters because the guideline wants the
 * external switch demonstrated as a real, single, deliberate action.
 * ========================================================================== */
#include <Arduino.h>

#define PIN_SWITCH      15
#define PIN_STATUS_LED  14
#define PIN_ONBOARD_LED LED_BUILTIN     /* pin 13 */
#define PIN_BUS_V       A6
#define BUS_V_RATIO     (122.0f / 22.0f)

#define DEBOUNCE_MS     25

static bool     stable = false;         /* debounced, true when closed */
static bool     lastRaw = false;
static uint32_t lastEdgeMs = 0;
static uint32_t changes = 0;
static uint32_t closedSinceMs = 0;

static float busVolts() {
    uint32_t acc = 0;
    for (int i = 0; i < 16; i++) acc += (uint32_t)analogRead(PIN_BUS_V);
    return ((float)acc / 16.0f) * (3.3f / 4095.0f) * BUS_V_RATIO;
}

static void rule() { Serial.println(F("  ------------------------------------------------")); }

static void lamps(bool on) {
    digitalWrite(PIN_STATUS_LED,  on ? HIGH : LOW);
    digitalWrite(PIN_ONBOARD_LED, on ? HIGH : LOW);
}

static void selfTest() {
    Serial.println(F("  Indicator self test, three flashes"));
    for (int i = 0; i < 3; i++) {
        lamps(true);  delay(200);
        lamps(false); delay(200);
    }
    Serial.println(F("  If you saw three flashes on both the external LED and the"));
    Serial.println(F("  on-board one, the indicator path is good."));
}

void setup() {
    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 3000) { }

    pinMode(PIN_SWITCH, INPUT_PULLUP);
    pinMode(PIN_STATUS_LED, OUTPUT);
    pinMode(PIN_ONBOARD_LED, OUTPUT);
    lamps(false);
    analogReadResolution(12);

    Serial.println();
    Serial.println(F("  External switch and indicator test"));
    rule();
    Serial.println(F("  Switch on pin 15, closed pulls it to ground."));
    Serial.println(F("  Status LED on pin 14, on-board LED on pin 13."));
    Serial.println();
    selfTest();
    Serial.println();
    Serial.println(F("  Now operate the switch. Every debounced change is reported."));
    Serial.println(F("  Put a multimeter across the switch to confirm continuity,"));
    Serial.println(F("  and across the battery to confirm the pack is doing the work."));
    Serial.println();
    Serial.println(F("  Send  t  for the indicator self test again."));
    rule();

    stable = (digitalRead(PIN_SWITCH) == LOW);
    lastRaw = stable;
    lamps(stable);
    Serial.printf("  Switch starts %s, bus %.2f V\n",
                  stable ? "closed" : "open", busVolts());
    if (stable) closedSinceMs = millis();
}

void loop() {
    if (Serial.available()) {
        const char c = (char)Serial.read();
        while (Serial.available()) Serial.read();
        if (c == 't' || c == 'T') {
            Serial.println();
            selfTest();
            lamps(stable);
            Serial.println();
        }
    }

    const bool raw = (digitalRead(PIN_SWITCH) == LOW);
    if (raw != lastRaw) { lastRaw = raw; lastEdgeMs = millis(); }

    if (raw != stable && (millis() - lastEdgeMs) >= DEBOUNCE_MS) {
        stable = raw;
        changes++;
        lamps(stable);

        Serial.println();
        Serial.printf("  Switch is now %s\n", stable ? "CLOSED, system powered"
                                                     : "OPEN, system isolated");
        Serial.printf("  Indicator     %s\n", stable ? "lit" : "dark");
        Serial.printf("  Bus voltage   %.2f V\n", busVolts());
        Serial.printf("  Changes seen  %lu\n", (unsigned long)changes);

        if (stable) {
            closedSinceMs = millis();
        } else if (closedSinceMs) {
            Serial.printf("  It was closed for %.1f s\n",
                          (millis() - closedSinceMs) / 1000.0f);
        }

        if (stable && busVolts() < 6.0f)
            Serial.println(F("  WARN  switch closed but the bus is low — running on USB?"));
        else if (stable)
            Serial.println(F("  PASS  the switch energises the bus from the pack"));
    }

    /* slow heartbeat so an idle bench still shows the sketch is alive */
    static uint32_t lastBeat = 0;
    if (millis() - lastBeat >= 5000) {
        lastBeat = millis();
        Serial.printf("  Idle — switch %s, bus %.2f V, %lu changes so far\n",
                      stable ? "closed" : "open", busVolts(),
                      (unsigned long)changes);
    }

    delay(5);
}
