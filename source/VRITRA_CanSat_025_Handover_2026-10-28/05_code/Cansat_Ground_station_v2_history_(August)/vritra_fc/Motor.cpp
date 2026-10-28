/* =============================================================================
 * Motor.cpp — TB6612FNG
 * =============================================================================
 * Truth table used here (TB6612FNG datasheet, channel A):
 *   STBY  AIN1  AIN2   result
 *    L     x     x     standby, outputs high-Z
 *    H     H     L     CW,  speed = PWMA duty
 *    H     L     H     CCW, speed = PWMA duty
 *    H     L     L     short brake
 * Coast is produced by dropping STBY rather than by short brake — a braked
 * flywheel dumps its energy back into the driver, and there is no reason to do
 * that when the wheel is simply being switched off.
 * ========================================================================== */
#include "Motor.h"

GyroWheel Wheel;

void GyroWheel::begin() {
    pinMode(PIN_MOT_STBY, OUTPUT);
    pinMode(PIN_MOT_AIN1, OUTPUT);
    pinMode(PIN_MOT_AIN2, OUTPUT);
    pinMode(PIN_MOT_PWMA, OUTPUT);

    digitalWrite(PIN_MOT_STBY, LOW);          /* safe default: disabled     */
    digitalWrite(PIN_MOT_AIN1, LOW);
    digitalWrite(PIN_MOT_AIN2, LOW);

    /* 20 kHz keeps the wheel out of the audible band and away from the
     * barometer's 25 Hz sample cadence; 8-bit resolution is plenty for an
     * open-loop percentage command. */
    analogWriteFrequency(PIN_MOT_PWMA, PWM_HZ);
    analogWriteResolution(8);
    analogWrite(PIN_MOT_PWMA, 0);

    targetPct_ = curPct_ = 0;
    curDir_ = 0;
    lastStepMs_ = millis();
}

void GyroWheel::set(int16_t pct) {
    if (pct >  100) pct =  100;
    if (pct < -100) pct = -100;
    targetPct_ = pct;
}

void GyroWheel::applyOutputs(int16_t pct) {
    int8_t dir = (pct > 0) ? 1 : (pct < 0 ? -1 : 0);

    if (dir == 0) {
        analogWrite(PIN_MOT_PWMA, 0);
        digitalWrite(PIN_MOT_STBY, LOW);
        digitalWrite(PIN_MOT_AIN1, LOW);
        digitalWrite(PIN_MOT_AIN2, LOW);
        curDir_ = 0;
        return;
    }

    if (dir != curDir_) {
        /* Never flip direction with the bridge live. */
        analogWrite(PIN_MOT_PWMA, 0);
        digitalWrite(PIN_MOT_STBY, LOW);
        digitalWrite(PIN_MOT_AIN1, dir > 0 ? HIGH : LOW);
        digitalWrite(PIN_MOT_AIN2, dir > 0 ? LOW  : HIGH);
        curDir_ = dir;
    }

    digitalWrite(PIN_MOT_STBY, HIGH);
    uint16_t duty = (uint16_t)((abs(pct) * 255) / 100);
    analogWrite(PIN_MOT_PWMA, duty);
}

void GyroWheel::service() {
    uint32_t now = millis();
    if (now - lastStepMs_ < RAMP_STEP_MS) return;
    lastStepMs_ = now;

    if (curPct_ == targetPct_) return;

    /* A sign change ramps down through zero first — going straight from +70 to
     * -70 would be a direction reversal at speed.
     * Written with explicit comparisons rather than min()/max(): those are
     * preprocessor macros in the Arduino core, so the templated form does not
     * compile and the macro form would double-evaluate its arguments. */
    int16_t next;
    if ((curPct_ > 0 && targetPct_ < 0) || (curPct_ < 0 && targetPct_ > 0)) {
        if (curPct_ > 0) {
            next = (int16_t)(curPct_ - RAMP_PCT_PER_STEP);
            if (next < 0) next = 0;
        } else {
            next = (int16_t)(curPct_ + RAMP_PCT_PER_STEP);
            if (next > 0) next = 0;
        }
    } else if (curPct_ < targetPct_) {
        next = (int16_t)(curPct_ + RAMP_PCT_PER_STEP);
        if (next > targetPct_) next = targetPct_;
    } else {
        next = (int16_t)(curPct_ - RAMP_PCT_PER_STEP);
        if (next < targetPct_) next = targetPct_;
    }

    curPct_ = next;
    applyOutputs(curPct_);
}
