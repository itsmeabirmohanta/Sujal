/* =============================================================================
 * test_motor.ino — TB6612FNG driver and the mechanical gyro wheel (7Semi)
 * Team 2026-INSPACe-CAN-7USAT-025 · Teensy 4.1
 * =============================================================================
 * Standalone. Nothing from vritra_fc/ is included.
 * Serial Monitor at 115200. Prints this subsystem's own parameters only.
 *
 * Covers: gyro wheel bring-up, and the spin rate the guideline asks to be
 * telemetered as GYRO SPIN RATE.
 *
 * TWENTY KILOHERTZ, NOT THE DEFAULT. The Teensy default is around 4.5 kHz,
 * squarely inside hearing and squarely inside the barometer's own sampling.
 * At 20 kHz the whine is above hearing and, more importantly, well away from
 * anything the sensors integrate.
 *
 * NEVER REVERSE DIRECTLY. Going from full forward to full reverse in one step
 * puts the full supply across a motor that is still spinning the other way.
 * The current spike browns out the 3.3 V rail and resets the processor — which
 * looks like a firmware crash and is not. Always ramp to zero, drop the
 * standby line, then set the new direction.
 *
 * THE SOFT START EXISTS FOR THE SAME REASON. Stalled rotor current on a small
 * motor is several amps for the first few milliseconds.
 *
 * SAFETY. Secure the wheel before running this. It is a flywheel, and the
 * point of it is to store angular momentum.
 * ========================================================================== */
#include <Arduino.h>
#include <Wire.h>

/* ---------- pins, must match Config.h -------------------------------------- */
#define PIN_MOT_PWMA    33
#define PIN_MOT_AIN2    34
#define PIN_MOT_AIN1    35
#define PIN_MOT_STBY    36

#define PIN_SDA         18
#define PIN_SCL         19
#define ICM_ADDR        0x69
#define PIN_BUS_V       A6
#define BUS_V_RATIO     (122.0f / 22.0f)

#define PWM_HZ          20000
#define PWM_BITS        10
#define PWM_MAX         1023
#define RAMP_STEP_PCT   5
#define RAMP_STEP_MS    20

static int  currentPct = 0;
static bool forward = true;
static bool imuOk = false;

/* ---------- IMU, only to read back the spin the wheel produces ------------- */
static bool wr8(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(ICM_ADDR);
    Wire.write(reg); Wire.write(val);
    return Wire.endTransmission() == 0;
}
static bool rdN(uint8_t reg, uint8_t* buf, uint8_t n) {
    Wire.beginTransmission(ICM_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)ICM_ADDR, (int)n) != n) return false;
    for (int i = 0; i < n; i++) buf[i] = Wire.read();
    return true;
}
static bool imuBegin() {
    wr8(0x06, 0x80); delay(100);
    wr8(0x06, 0x01); delay(20);
    wr8(0x7F, 0x00);
    uint8_t who = 0;
    if (!rdN(0x00, &who, 1) || who != 0xEA) return false;
    wr8(0x7F, 0x20);
    wr8(0x14, 0x1F);          /* accel +-16 g       */
    wr8(0x01, 0x2F);          /* gyro +-2000 dps    */
    wr8(0x7F, 0x00);
    delay(30);
    return true;
}
static float spinZ() {
    uint8_t r[6];
    if (!rdN(0x33, r, 6)) return 0.0f;         /* GYRO_XOUT_H .. ZOUT_L */
    return (int16_t)((r[4] << 8) | r[5]) / 16.4f;
}

static float busVolts() {
    uint32_t acc = 0;
    for (int i = 0; i < 16; i++) acc += (uint32_t)analogRead(PIN_BUS_V);
    return ((float)acc / 16.0f) * (3.3f / 4095.0f) * BUS_V_RATIO;
}

static void rule() { Serial.println(F("  ------------------------------------------------")); }

/* ---------- driver --------------------------------------------------------- */
static void applyDirection(bool fwd) {
    digitalWrite(PIN_MOT_AIN1, fwd ? HIGH : LOW);
    digitalWrite(PIN_MOT_AIN2, fwd ? LOW  : HIGH);
}
static void applyPct(int pct) {
    pct = constrain(pct, 0, 100);
    analogWrite(PIN_MOT_PWMA, (int)((long)pct * PWM_MAX / 100));
    currentPct = pct;
}

static void rampTo(int targetPct) {
    targetPct = constrain(targetPct, 0, 100);
    if (targetPct > 0) digitalWrite(PIN_MOT_STBY, HIGH);
    const int dir = (targetPct > currentPct) ? RAMP_STEP_PCT : -RAMP_STEP_PCT;
    while (currentPct != targetPct) {
        int next = currentPct + dir;
        if ((dir > 0 && next > targetPct) || (dir < 0 && next < targetPct))
            next = targetPct;
        applyPct(next);
        delay(RAMP_STEP_MS);
    }
    if (targetPct == 0) digitalWrite(PIN_MOT_STBY, LOW);
}

static void stopWheel() {
    rampTo(0);
    digitalWrite(PIN_MOT_STBY, LOW);
    applyDirection(true);
    Serial.println(F("  Wheel stopped, driver in standby"));
}

static void reverseWheel() {
    const int was = currentPct;
    Serial.println(F("  Reversing — ramping to zero first, see the note in the header"));
    rampTo(0);
    digitalWrite(PIN_MOT_STBY, LOW);
    delay(150);
    forward = !forward;
    applyDirection(forward);
    delay(50);
    rampTo(was);
    Serial.printf("  Now running %s at %d percent\n",
                  forward ? "forward" : "reverse", currentPct);
}

static void spinUpProfile() {
    Serial.println();
    Serial.println(F("  Spin up profile, twenty percent at a time"));
    Serial.println(F("  Power    Bus       Rate about the long axis"));
    for (int p = 0; p <= 100; p += 20) {
        rampTo(p);
        delay(1200);                       /* let it settle */
        float sum = 0;
        for (int i = 0; i < 20; i++) { sum += spinZ(); delay(10); }
        const float rate = imuOk ? sum / 20.0f : 0.0f;
        Serial.printf("  %3d %%    %5.2f V   %+8.1f deg/s\n", p, busVolts(), rate);
        if (busVolts() < 9.0f) {
            Serial.println(F("  WARN  the bus sagged below nine volts, stopping"));
            break;
        }
    }
    Serial.println(F("  Ramping back down"));
    rampTo(0);
    digitalWrite(PIN_MOT_STBY, LOW);
}

static void help() {
    Serial.println();
    Serial.println(F("  0 to 9   set power, 0 is stop and 9 is ninety percent"));
    Serial.println(F("  f        full power"));
    Serial.println(F("  r        reverse direction, safely"));
    Serial.println(F("  p        spin up profile with the rate measured at each step"));
    Serial.println(F("  s        stop"));
    Serial.println(F("  Secure the wheel before spinning it."));
    rule();
}

void setup() {
    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 3000) { }

    pinMode(PIN_MOT_AIN1, OUTPUT);
    pinMode(PIN_MOT_AIN2, OUTPUT);
    pinMode(PIN_MOT_STBY, OUTPUT);
    pinMode(PIN_MOT_PWMA, OUTPUT);
    digitalWrite(PIN_MOT_STBY, LOW);        /* standby until asked */
    applyDirection(true);
    analogWriteFrequency(PIN_MOT_PWMA, PWM_HZ);
    analogWriteResolution(PWM_BITS);
    analogWrite(PIN_MOT_PWMA, 0);
    analogReadResolution(12);

    Wire.begin();
    Wire.setClock(100000);
    imuOk = imuBegin();

    Serial.println();
    Serial.println(F("  Gyro wheel test — TB6612FNG driver"));
    rule();
    Serial.printf("  Standby %d, direction %d and %d, speed %d\n",
                  PIN_MOT_STBY, PIN_MOT_AIN1, PIN_MOT_AIN2, PIN_MOT_PWMA);
    Serial.printf("  Switching at %d Hz, above hearing and clear of the sensors\n", PWM_HZ);
    Serial.printf("  Soft start %d percent every %d ms\n", RAMP_STEP_PCT, RAMP_STEP_MS);
    Serial.printf("  Inertial unit %s\n",
                  imuOk ? "ready, spin rate will be measured"
                        : "NOT RESPONDING, spin rate will read zero");
    Serial.printf("  Bus voltage %.2f V\n", busVolts());
    Serial.println();
    Serial.println(F("  The wheel stores angular momentum. Secure it."));
    help();
}

void loop() {
    if (Serial.available()) {
        const char c = (char)Serial.read();
        while (Serial.available()) Serial.read();
        if (c >= '0' && c <= '9') {
            const int pct = (c - '0') * 10;
            Serial.printf("\n  Setting %d percent, %s\n", pct,
                          forward ? "forward" : "reverse");
            rampTo(pct);
        } else switch (c) {
            case 'f': case 'F':
                Serial.println(F("\n  Full power"));
                rampTo(100);
                break;
            case 'r': case 'R': Serial.println(); reverseWheel(); break;
            case 'p': case 'P': spinUpProfile();                  break;
            case 's': case 'S': Serial.println(); stopWheel();    break;
            default:            help();                           break;
        }
    }

    static uint32_t last = 0;
    if (millis() - last < 1000) return;
    last = millis();

    const float v = busVolts();
    Serial.printf("  Power %3d %%   %s   bus %5.2f V   rate %+7.1f deg/s\n",
                  currentPct, forward ? "forward" : "reverse", v,
                  imuOk ? spinZ() : 0.0f);

    if (currentPct > 0 && v < 9.0f) {
        Serial.println(F("  WARN  bus below nine volts under load, stopping the wheel"));
        stopWheel();
    }
}
