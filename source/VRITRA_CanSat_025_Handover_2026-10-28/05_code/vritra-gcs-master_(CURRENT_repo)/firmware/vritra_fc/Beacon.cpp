/* =============================================================================
 * Beacon.cpp — MAX98357A I2S amplifier + SM400204-1 speaker
 * =============================================================================
 * Signal chain:
 *
 *     AudioSynthWaveform  --+--> ch0 (left)  --.
 *          (square)         |                  }-- AudioOutputI2S2 --> SAI2
 *                           +--> ch1 (right) --'      pins 2/3/4
 *
 * BOTH CHANNELS CARRY THE SAME TONE, DELIBERATELY.
 * The MAX98357A picks its input channel from the VOLTAGE on SD_MODE (see
 * PIN_BEACON_SD in Config.h): driven straight from a 3.3 V pin it selects LEFT,
 * but a breakout board with its own pull-up divider may land in the RIGHT or
 * (L+R)/2 window instead. Feeding one channel would make the beacon silent on
 * some boards and fine on others, and the symptom — powered amplifier, valid
 * clocks, no sound — is indistinguishable from a dead part. Two connections
 * cost one AudioConnection object and remove the entire failure mode.
 *
 * WHY A SQUARE WAVE.
 * For the same peak voltage a square wave carries about 2.1 dB more RMS than a
 * sine, and its harmonics land in the 3-10 kHz region where the ear is most
 * sensitive and where A-weighting is generous. On a beacon, "harsh" is the
 * desired quality — it is what makes the sound carry through wind and stand out
 * from natural background. The speaker cannot be damaged by the choice: the
 * MAX98357A tops out at 3.2 W into 4 ohms from 5 V and the driver is rated 3 W
 * continuous / 4 W maximum, so the amplifier clips before the voice coil is at
 * risk.
 * ========================================================================== */
#include "Beacon.h"

RecoveryBeacon Beacon;

#if BEACON_ENABLED

#include <Audio.h>

static AudioSynthWaveform   beaconTone;
static AudioOutputI2S2      beaconOut;
static AudioConnection      beaconPatchL(beaconTone, 0, beaconOut, 0);
static AudioConnection      beaconPatchR(beaconTone, 0, beaconOut, 1);

void RecoveryBeacon::begin() {
    /* Eight blocks is generous for one waveform feeding one output — the
     * pipeline is two connections deep. Undersizing this does not fail loudly,
     * it drops blocks and the tone develops gaps, so the margin is cheap
     * insurance on a part we only get one chance to hear. */
    AudioMemory(8);

    beaconTone.begin(WAVEFORM_SQUARE);
    beaconTone.frequency(BEACON_TONE_A_HZ);
    beaconTone.amplitude(0.0f);              /* silent until commanded */

    /* SAFE DEFAULT, AND IT MATTERS MORE HERE THAN ON MOST PINS.
     * The amplifier must come up shut down. Without an explicit LOW the pin
     * floats through reset, and SD_MODE floating high enough puts a 3 W
     * amplifier into an undefined channel mode the moment power is applied —
     * i.e. the recovery beacon shrieks on the pad, during integration, every
     * time the vehicle is powered. Same reasoning as the gate pull-down a
     * MOSFET buzzer stage would have needed. */
    pinMode(PIN_BEACON_SD, OUTPUT);
    digitalWrite(PIN_BEACON_SD, LOW);

    on_ = sounding_ = toneB_ = false;
    phaseAt_ = swapAt_ = millis();
    chirpUntil_ = 0;

    Serial.printf("// [BEACON] MAX98357A on SAI2 (DIN %d, LRCLK %d, BCLK %d), "
                  "SD %d — %.0f/%.0f Hz warble, amp shut down\n",
                  PIN_BEACON_DIN, PIN_BEACON_LRCLK, PIN_BEACON_BCLK,
                  PIN_BEACON_SD, (double)BEACON_TONE_A_HZ,
                  (double)BEACON_TONE_B_HZ);
}

void RecoveryBeacon::applyAmp(bool enabled) {
    digitalWrite(PIN_BEACON_SD, enabled ? HIGH : LOW);
    beaconTone.amplitude(enabled ? BEACON_AMPLITUDE : 0.0f);
}

void RecoveryBeacon::set(bool on) {
    if (on == on_) return;
    on_ = on;
    /* Restart the pattern from the top so a commanded beacon always begins with
     * sound rather than landing mid-gap and looking like it did not take. */
    phaseAt_ = swapAt_ = millis();
    sounding_ = on;
    toneB_    = false;
    if (on) beaconTone.frequency(BEACON_TONE_A_HZ);
    if (!chirpUntil_) applyAmp(on);
}

void RecoveryBeacon::chirp(uint32_t ms) {
    chirpUntil_ = millis() + ms;
    /* 0 is the "no chirp in progress" sentinel, so the one millisecond in every
     * 49.7 days where the deadline lands exactly on the millis() rollover would
     * cancel the chirp instead of running it. Nudging it by 1 ms costs nothing
     * and removes a bug that would be unreproducible if it ever bit. */
    if (!chirpUntil_) chirpUntil_ = 1;
    beaconTone.frequency(BEACON_TONE_A_HZ);
    applyAmp(true);
}

void RecoveryBeacon::service() {
    const uint32_t now = millis();

    /* A chirp overrides the pattern for its duration, then hands back. */
    if (chirpUntil_) {
        if ((int32_t)(now - chirpUntil_) < 0) return;
        chirpUntil_ = 0;
        phaseAt_ = swapAt_ = now;
        sounding_ = on_;
        applyAmp(on_);
        return;
    }

    if (!on_) return;

    /* Pulse: ON for BEACON_ON_MS, silent for BEACON_OFF_MS. The amplifier is
     * shut down through the gap rather than merely fed silence, so the gap
     * genuinely saves the 5 V rail instead of just sounding like it does. */
    const uint32_t phaseLen = sounding_ ? BEACON_ON_MS : BEACON_OFF_MS;
    if (now - phaseAt_ >= phaseLen) {
        phaseAt_  = now;
        sounding_ = !sounding_;
        applyAmp(sounding_);
        if (sounding_) {
            swapAt_ = now;
            toneB_  = false;
            beaconTone.frequency(BEACON_TONE_A_HZ);
        }
        return;
    }

    /* Warble within the ON phase. */
    if (sounding_ && (now - swapAt_ >= BEACON_SWAP_MS)) {
        swapAt_ = now;
        toneB_  = !toneB_;
        beaconTone.frequency(toneB_ ? BEACON_TONE_B_HZ : BEACON_TONE_A_HZ);
    }
}

#else   /* BEACON_ENABLED == 0 — amplifier not fitted ---------------------- */

/* No Audio library dependency, no SAI2 claim, no pins driven. CMD_BEACON_ON
 * still sets the telemetry flag and still blinks the status LED, exactly as the
 * firmware behaved before the amplifier existed — so the ground station's
 * BEACON_ON indication remains truthful about what was COMMANDED, and the build
 * stays flyable on a vehicle with no sounder fitted. */
void RecoveryBeacon::begin() {
    Serial.println(F("// [BEACON] BEACON_ENABLED=0 — no sounder driven, "
                     "LED only (does NOT satisfy guideline 5.2.v)"));
}
void RecoveryBeacon::applyAmp(bool) {}
void RecoveryBeacon::set(bool on) { on_ = on; }
void RecoveryBeacon::chirp(uint32_t) {}
void RecoveryBeacon::service() {}

#endif
