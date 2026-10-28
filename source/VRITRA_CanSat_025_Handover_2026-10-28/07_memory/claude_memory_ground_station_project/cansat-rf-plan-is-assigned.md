---
name: cansat-rf-plan-is-assigned
description: "The CanSat RF parameters are organiser-assigned (row 025), not a team choice — and the user wants strict compliance even for bench tests."
metadata: 
  node_type: memory
  type: project
  originSessionId: 4056e5f4-a73b-47d5-9736-1f86ee611ece
  modified: 2026-08-08T06:47:44.931Z
---

The VRITRA RF plan comes from the IN-SPACe "CAN-7USAT frequency allocation plan
for teams", row 025, and is **not** a design choice: 867.00 MHz main / 865.75 MHz
backup, SF10, BW 125 kHz, CR 4/5, sync 0x98 (= 0x80 + team id), +10 dBm on pad /
+14 dBm in flight.

On 2026-08-07 the user was offered a compile-time high-power range-test profile
and explicitly declined it: *"stay compliant but try to achieve the 2km goal
without breaking anything even for bench test keep the real frequency."*

**Why:** transmitting outside the allocation risks interfering with other teams'
pads and disqualification. Both 867.00 and 865.75 are ours, so switching between
them is fine; anything else is not.

**How to apply:** never propose changing SF, bandwidth, coding rate, sync word or
TX power to improve range. Optimise the legal levers instead — packet-byte
efficiency, boosted RX gain, image-rejection calibration, antenna siting and
polarisation. 2 km is already ~53 dB inside budget with line of sight; observed
shortfalls are obstruction, not link budget. See [[cansat-handoff-doc]] and
[[cansat-antenna-and-link-margin]] for the antenna choice and how link margin
gets computed within these fixed parameters.
