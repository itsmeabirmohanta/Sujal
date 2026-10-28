---
name: cansat-beacon-buzzer-wiring
description: "The recovery beacon is an I2S speaker+amp, not a buzzer pin — it must use SAI2, and the chosen driver cannot reach the mandated 92 dB(A) at 1 m."
metadata:
  node_type: memory
  type: project
  originSessionId: 305a8c13-c13b-4cd9-a065-26cf9855a60d
  modified: 2026-08-10T03:00:13.287Z
---

**SUPERSEDED 2026-08-10 — the beacon is no longer a GPIO buzzer.** The earlier
version of this memory specified a MOSFET stage on pin 37 with a 100 kΩ gate
pull-down. The user's actual part is a **DB Unlimited SM400204-1** speaker
(40 mm, 4 Ω, 3 W rated / 4 W max, 99 dB SPL @ 100 mm, 220 Hz resonance,
220 Hz–20 kHz) driven by a **MAX98357A** Class-D **I2S** amplifier.

**`digitalWrite()` and `tone()` both produce silence on this hardware.** The
MAX98357A has no analogue input and no oscillator; it decodes an I2S stream and
mutes itself if BCLK/LRCLK stop for a few ms. Driving it means a real audio
pipeline — Teensy Audio library, `AudioSynthWaveform` → `AudioOutputI2S2`.

**SAI1 is unusable on this vehicle, and that is the non-obvious part.** The
Audio library's default `AudioOutputI2S` is fixed at OUT1A = pin 7,
LRCLK1 = 20, BCLK1 = 21, MCLK1 = 23. Pin 7 is **Serial2 RX, the GNSS receiver**
(hardware-fixed) and pin 20 is **PIN_BUS_V / A6, the only battery measurement**.
Use `AudioOutputI2S2` (SAI2): DIN 2, LRCLK 3, BCLK 4 — all free. Watch that
MCLK2 is pin 33 = `PIN_MOT_PWMA`; the MAX98357A needs no MCLK, so if a
Teensyduino version ever claims it, move the wheel PWM, not the amp.

**SD_MODE is a voltage-coded channel select, not an enable.** <0.16 V shutdown ·
0.16–0.77 V (L+R)/2 · 0.77–1.4 V right · >1.4 V left. A 3.3 V GPIO HIGH selects
**left**, but a breakout's own divider may land elsewhere — so feed the tone to
**both** channels. A mono amp fed only the unselected channel is silent, and
that looks identical to a dead part. Drive SD low in `begin()` before anything
else or the beacon shrieks through every power-up.

**THE PART CANNOT MEET THE REQUIREMENT. Say so, do not tune around it.**
Requirement is **≥ 92 dB(A) at 1 m** (`cdr_tests/README.md`, guideline §5.2.v).
99 dB at 100 mm is **79 dB at 1 m** (20 dB per decade, free field); even reading
the rating generously as 99 dB @ 1 W and driving the amp's full 3.2 W gives
~84 dB(A) at 1 m. Closing 8–13 dB needs ~20× the acoustic power — not available
in a 4 W speaker or a 3.2 W amp. This needs a louder transducer (piezo siren /
horn), not firmware.

**Tone choice is an A-weighting decision.** 220 Hz resonance is where the driver
is most *efficient* but A-weighting docks it ~11 dB; 3 kHz gets +1 dB and is the
ear's peak sensitivity — ~12 dB better on the meter that grades us. Firmware
uses a 2.8/3.5 kHz warble, pulsed, because a *changing* tone is far easier to
walk toward than a steady one of the same SPL.

Implementation: `vritra_fc/Beacon.h` + `Beacon.cpp` (`RecoveryBeacon Beacon`),
`BEACON_ENABLED` in Config.h compiles it out cleanly, and `setup()` fires a
250 ms `Beacon.chirp()` so every power-up proves the whole chain audibly.

Pin 13 doubles as the Teensy's onboard LED, so it flickers during SPI flash
access on the separate 7Semi flash chip (SPI0, pins 10–13) — harmless, and the
reason `test_switch_led` uses an external LED on pin 14 instead.

Related: [[cansat-no-local-toolchain]] — none of this is compiled.
