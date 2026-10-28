# Verified Competition Requirements
Source: `IN-SPACe.CAN.MR.CG.01`, Issue 01, dated 24-03-2026 (40 pp)
Extracted from `00_source/originals/competition_guidelines.pdf` -> `00_source/guidelines_text.txt`

| ID | Requirement (verbatim / near-verbatim) | Section |
|---|---|---|
| R-01 | "Total mass of the CAN-7USAT shall be under 1.0 Kg (+/- 0.050 Kg)" | 5.1 i |
| R-02 | "CAN-7USAT shall fit in a cylindrical body of 0.15 m diameter x 0.40 m height" | 5.1 ii |
| R-03 | "shall contain a total of 2 descent control mechanisms, to be used at different stages" | 5.4 i |
| R-04 | "Primary descent control mechanism shall be a parachute while the secondary ... shall be decided by the teams" | 5.4 ii |
| R-05 | "shall immediately deploy the primary parachute after ejection from the rocket" | 5.4 iii |
| R-06 | **"The primary parachute shall be connected to the outer body of the CAN-7USAT and no ejection mechanism shall be attached to it"** | 5.4 iv |
| R-07 | "descent rate of the primary parachute shall be less than 20 m/s (variation within +/-5 m/s)" | 5.4 v |
| R-08 | "secondary descent control mechanism shall activate at an altitude of 600 m (+/-10 m) to further decrease the descent rate ... to 1 to 3 m/s" | 5.4 vi |
| R-09 | "shall not use any hazardous chemical-based explosive or pyrotechnic devices. However, green propulsion such as pressurized gas etc is allowed" | 5.4 vii |
| R-10 | "shall stabilize itself during the descent using the Mechanical Gyroscopic wheels" | 5.4 viii |
| R-11 | Launch to 1000 (+/-100) m above launch site, deployed near peak altitude; "orientation of the CAN-7USAT is not controlled" during ejection | 3.1 |
| R-12 | Telemetry field 14 = `<GYRO SPIN RATE>` "Spin rate of Mechanical Gyro wrt. CAN-7USAT", deg/s | 6.3 |

## Precision landing is an OPTIONAL objective, with no numeric tolerance
> "Desired additional objective: Precision, Controlled flight and Landing Systems that shall
> demonstrate controlled flight and targeted landing **near to the launch pad**." (3.1)

**No landing-accuracy figure (CEP, radius, scoring band) is specified anywhere in the rules.**
Therefore predicted dispersion is reported as-is; no pass/fail threshold is invented.

## The target IS the launch pad
Rules say "near to the launch pad"; the organiser note gives "**launch coordinates** for the
targeted landing site ... 26.717535 N, 84.301543 E"; CDR flight-readiness Table 16 confirms
"Target waypoint: **Launch pad GPS coordinates** set". Launch point and target are the same
point, so the guidance problem is a return-to-launch-site, not a cross-country transit.

## Compliance risk to raise with organisers (R-06)
The CDR's sequence releases/cuts away the primary parachute at 600 m
(FSM state `AEROBRAKE_RELEASE` "triggers ... releasing primary parachute"). R-06 requires the
primary parachute to be "connected to the outer body" with "no ejection mechanism attached to
it". Whether a 600 m cut-away violates R-06 is genuinely ambiguous in the written rule.
**This is an organiser question, not an engineering one — ask before FRR.**
