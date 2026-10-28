/* =============================================================================
 * Motor.h — TB6612FNG driver for the GA12-N20 gyro-stabilisation wheel
 * =============================================================================
 * The TB6612 has no feedback pin, so there is nothing to close a loop around:
 * this is open-loop speed command only, and the firmware must never pretend
 * otherwise. What it CAN do safely is bound the electrical behaviour.
 *
 * SOFT START — this matters more than it looks on this vehicle.
 *   The wheel, the LR1121's +22 dBm PA and the Teensy all sit on one 2S pack
 *   with no BMS. Slamming a stalled N20 from 0 to full duty pulls its stall
 *   current through the pack's series resistance and drops the rail; if that
 *   coincides with a transmit burst the radio browns out mid-packet and the
 *   downlink drops for no reason visible anywhere in the telemetry.
 *   set() therefore only ever sets a TARGET, and service() walks the actual
 *   duty toward it at RAMP_PCT_PER_STEP. Nothing blocks.
 *
 * SAFE DEFAULT — STBY is driven LOW (driver disabled) in begin(), on stop(),
 * and on every direction change. A direction reversal at speed would otherwise
 * put both half-bridges through a shoot-through transient.
 * ========================================================================== */
#pragma once
#include <Arduino.h>
#include "Config.h"

class GyroWheel {
public:
    void begin();
    void service();                     /* call every loop — never blocks   */

    /* pct: -100..+100. Sign selects direction, 0 coasts and disables STBY. */
    void set(int16_t pct);
    void stop() { set(0); }

    int16_t target()  const { return targetPct_; }
    int16_t current() const { return curPct_; }
    bool    running() const { return curPct_ != 0; }

private:
    void applyOutputs(int16_t pct);

    int16_t  targetPct_ = 0;
    int16_t  curPct_    = 0;
    int8_t   curDir_    = 0;            /* -1, 0, +1                        */
    uint32_t lastStepMs_ = 0;

    static constexpr uint32_t RAMP_STEP_MS      = 20;   /* 0->100 % in 0.4 s */
    static constexpr int16_t  RAMP_PCT_PER_STEP = 5;
    static constexpr uint32_t PWM_HZ            = 20000; /* above audible    */
};

extern GyroWheel Wheel;
