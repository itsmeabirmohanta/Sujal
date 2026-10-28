---
name: cansat-antenna-and-link-margin
description: "Which antenna to fly, why the 30 dB \"bad antenna\" reading wasn't the antenna, and how link margin gets calculated for this project."
metadata: 
  node_type: memory
  type: project
  originSessionId: 305a8c13-c13b-4cd9-a065-26cf9855a60d
  modified: 2026-08-08T21:40:32.968Z
---

Fly the **RadioMaster dual-band whip** (3.58 dBi @ 900 MHz, VSWR ~1.25) on the
**`LORA_ANT` port (ANT2, sub-GHz)** at both ends, vertical and parallel at both
ends so polarisation matches. Keep the Waveshare rubber duck that ships with
the Core1121-XF as spare only — it's a generic, lower-gain part.

**Why this took a correction.** A first pass of RF A/B testing (through a wall
vs apparently open, antenna swapped at the same time) showed the RadioMaster
~30 dB worse and it was reported as an inferior antenna. That was wrong. The
project's own `COMPLIANCE_AUDIT.md` §3.1 already specs the RadioMaster with a
good VSWR — a well-matched antenna cannot lose 30 dB to mismatch alone (VSWR
10:1 is only ~4.8 dB mismatch loss). `vritra_fc/Radio.cpp`'s load-bearing
fact #1 documents that **~30 dB is this exact module's signature for using the
wrong RF path** — LP PA instead of HP PA, or the `2.4G_ANT` port instead of
`LORA_ANT`. Once port and connector were confirmed correct by the user, a 1 m
bench test showed a clean, healthy link (0/22 packets lost, SNR 12–13.5 dB).

**Why:** the user cares about getting this right specifically because a wrong
antenna call would be a flight decision, not a bench curiosity — see
[[cansat-rf-plan-is-assigned]] for why RF parameters here are treated as
non-negotiable and organiser-assigned.

**How to apply:** if a future antenna A/B test again shows tens of dB of
unexplained loss, check RF port and connector gender *before* concluding the
antenna itself is bad. A single confounded test (two variables changed at
once, e.g. wall + antenna) is not enough to blame the antenna.

**Link margin methodology for this project:**
`margin (dB) = received power (dBm) − receiver sensitivity (dBm)`.
Sensitivity for SF10/BW125 with boosted RX gain is derived as
`-174 + 10·log10(125000) + NF(5.5) + SNRmin(-15) = -132.5 dBm`, not just quoted
— which lands exactly on Semtech's published SF10/BW125 figure. Use -132.5
everywhere; the CDR's own -135.4 dBm is ~3 dB optimistic because it interpolates
at 2.8 dB/SF from an SF7 anchor that is really the SF8 number.
Two path-loss models matter and must not be conflated: free space (20 dB/decade,
applies in flight) vs two-ray ground reflection (40 dB/decade beyond breakpoint
`d_b = 4π·h_t·h_r/λ`, applies to ground testing and post-landing recovery).
Using free-space loss for a ground test is the most common way this kind of
budget comes out wrong by tens of dB.

**The user explicitly wants to do link-margin arithmetic by hand.** A version
of `test_rf_rx.ino` that auto-computed predicted RSSI and margin was built,
then deliberately stripped back out on request — the test sketches
(`cdr_tests/test_rf_tx/`, `cdr_tests/test_rf_rx/`) print only raw packet
content, RSSI, SNR, byte count and TX power, nothing derived. Don't
re-introduce budget math into these sketches without being asked again.

Full detail: `HANDOFF.md` §8 in the repo root, session 2026-08-08.
