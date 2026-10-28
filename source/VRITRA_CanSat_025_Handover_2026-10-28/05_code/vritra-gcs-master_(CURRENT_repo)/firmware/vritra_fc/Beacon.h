/* =============================================================================
 * Beacon.h — recovery audio beacon, guideline §5.2.v
 * =============================================================================
 * HARDWARE: DB Unlimited SM400204-1 (40 mm, 4 ohm, 3 W rated / 4 W max,
 * 99 dB SPL at 100 mm, 220 Hz resonance, 220 Hz - 20 kHz) driven by a
 * MAX98357A Class-D I2S amplifier off the 5 V rail.
 *
 * WHY THIS IS A MODULE AND NOT TWO LINES OF digitalWrite()
 *   The MAX98357A has no analogue input and no internal oscillator. It decodes
 *   an I2S stream, and it mutes itself if BCLK/LRCLK stop for more than a few
 *   milliseconds. There is no pin on this board that can be toggled to make a
 *   sound. Producing tone therefore means running a real audio pipeline:
 *   a waveform generator feeding SAI2 through DMA. That is what the Teensy
 *   Audio library is for, and it is entirely interrupt/DMA driven — service()
 *   below only ever changes a frequency or an amplitude, so nothing here
 *   blocks, in keeping with the no-delay() policy the rest of the flight code
 *   follows.
 *
 * WHAT IT SOUNDS LIKE
 *   A two-tone warble (BEACON_TONE_A_HZ / BEACON_TONE_B_HZ, swapping every
 *   BEACON_SWAP_MS) pulsed BEACON_ON_MS on, BEACON_OFF_MS off. Both the warble
 *   and the pulse exist to make the source easy for a human to LOCATE, which is
 *   a different problem from making it loud: a steady tone of the same SPL is
 *   markedly harder to walk towards.
 *
 * POWER
 *   Shut down (SD low) the amplifier draws under 1 uA, so the beacon costs the
 *   vehicle nothing in flight. Sounding, it pulls roughly 0.6-0.8 A from the
 *   5 V rail at full output — which is why it is gated to post-landing and why
 *   the duty cycle is not 100 %. Fit at least 100 uF of bulk capacitance at the
 *   amplifier's supply pins; this is the same lesson the LR1121's PA taught
 *   (Config.h §5), and a Class-D amp is a worse offender than a radio because
 *   its draw is continuous rather than bursty.
 *
 * SPL — READ THIS BEFORE CLAIMING COMPLIANCE
 *   The datasheet's 99 dB is quoted at 100 mm. Free-field spreading is 20 dB
 *   per decade of distance, so at the 1 m the requirement is measured at that
 *   is 99 - 20 = 79 dB, and even the most generous reading of the rating
 *   (99 dB at 1 W, driven to the amplifier's full 3.2 W) only reaches about
 *   84 dB(A) at 1 m. The requirement is >= 92 dB(A) at 1 m. THIS DRIVER CANNOT
 *   MEET IT — closing a 13 dB gap needs about twenty times the acoustic power,
 *   which is neither in this speaker's 4 W rating nor in this amplifier's
 *   3.2 W. That is a hardware result, and no amount of tone selection or
 *   firmware tuning changes it. The firmware's job is to extract everything
 *   this hardware can give (hence the A-weighting-aware tone choice in
 *   Config.h) and to report the shortfall honestly rather than tune around it.
 * ========================================================================== */
#pragma once
#include <Arduino.h>
#include "Config.h"

class RecoveryBeacon {
public:
    void begin();
    void service();                    /* call every loop — never blocks */

    void set(bool on);
    void on()  { set(true);  }
    void off() { set(false); }

    bool active() const { return on_; }

    /* Sound for `ms` regardless of the flight state, then return to whatever
     * set() last commanded. For the bench and for a pad self-test — this is the
     * one way to prove the whole chain works before it is needed for real. */
    void chirp(uint32_t ms = 600);

private:
    void applyAmp(bool enabled);

    bool     on_        = false;       /* commanded state                    */
    bool     sounding_  = false;       /* inside the ON half of the pulse    */
    bool     toneB_     = false;       /* which of the two tones is playing  */
    uint32_t phaseAt_   = 0;           /* when the current pulse phase began */
    uint32_t swapAt_    = 0;           /* when the current tone began        */
    uint32_t chirpUntil_ = 0;          /* 0 = no chirp in progress           */
};

extern RecoveryBeacon Beacon;
