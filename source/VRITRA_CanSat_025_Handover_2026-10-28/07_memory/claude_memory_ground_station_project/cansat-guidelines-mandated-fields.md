---
name: cansat-guidelines-mandated-fields
description: "Where the CAN-7USAT guidelines PDF is, and exactly which telemetry fields it mandates — the test for whether anything may be cut from the wire."
metadata: 
  node_type: memory
  type: reference
  originSessionId: a0b2f73f-b0a4-4f0b-a15d-7abd5ddd45e9
  modified: 2026-08-10T18:38:08.431Z
---

`D:\Download\Cansat\cansat_competition_guidelines_3 (1).pdf` — 40 pages. The
telemetry contract is **§6.3 on pages 17-18**; the field table is page 18.

Read it with `pypdf` (installed): `PdfReader(path).pages[17].extract_text()`.

**The mandated format (§6.3.i), rows 1-16 of the page-18 table.** Row 1 is
`<TEAM ID>`, row 16 is `<OPTIONAL DATA>`, so **14 mandated data fields**:

TIME STAMPING · PACKET COUNT · ALTITUDE (0.1 m, relative to ground) · PRESSURE
(1 Pa) · TEMP (0.1 °C) · VOLTAGE (0.01 V) · GNSS TIME (s) · GNSS LATITUDE
(0.0001°) · GNSS LONGITUDE (0.0001°) · GNSS ALTITUDE (0.1 m) · GNSS SATS (int) ·
ACCELEROMETER DATA (m/s²) · GYRO SPIN RATE (deg/s) · FLIGHT SOFTWARE STATE

Row 16.B: *"Additional data fields may be appended after the required fields."*

**THE TRAP, and it decided a real design choice on 2026-08-10.** Row 13
`<ACCELEROMETER DATA>` is described as *"Data received from the gyroscopic sensor
i.e acceleration and **roll & pitch** parameters"*. So **roll and pitch are
mandated content**, even though the resolution column says only m/s². **Yaw and
quaternions are named nowhere** and fall under row 16.

That is why the wire-layout-3 cut removed `ATT_YAW` + `QUAT_X/Y/Z` but kept
`ATT_ROLL`/`ATT_PITCH` — the latter two ride the wire and sit in the graded CSV
purely for compliance; no GCS panel reads them (the displays use the GCS's own
DERIVED ROLL/PITCH/YAW).

**How to apply:** before proposing that any field leave the downlink, check it
against the 14 above *and* row 13's prose. The user's standing instruction
(2026-08-10) is: *"don't remove any data that is mandatory in those tables,
other data we can remove"*.

Other requirements worth knowing from the same section: §6.3.i telemetry at
**1 Hz or better**; §6.3.ii the graded file is `Flight_<TEAM_ID>.csv` **with
headers**, handed to judges on a USB drive; §6.1.viii mission time to 1 s or
better; §6.1.ix mission clock and state survive a processor reset;
§6.1.v the CanSat must not transmit until commanded.

Related: [[cansat-rf-plan-is-assigned]], [[cansat-no-local-toolchain]].
