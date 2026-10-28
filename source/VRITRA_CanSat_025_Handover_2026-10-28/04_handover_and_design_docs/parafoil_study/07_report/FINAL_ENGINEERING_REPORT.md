# CAN-7USAT Ram-Air Parafoil — Final Engineering Report
Team VRITRA · ID `2026-INSPACe-CAN-7USAT-025` · issued 2026-10-20

Every number here traces to a script in `02_octave`, a solver log in `04_cfd`/`05_fea`, or a
named source document. Where something is assumed rather than computed, it says so.

---

## 1. Executive summary and overall assessment

The LEparagliding-generated canopy (`PF072-CANSAT`, 11 cells, 0.71 m²) is **aerodynamically
sound and structurally over-strong**, and is predicted to land with a median error of about
**30 m**, with **91.5 %** of cases inside 100 m. The design tool's geometry output has been
validated four independent ways and can be trusted.

**Overall: NOT FLIGHT-READY**, on four specific items, none of them aerodynamic:

1. **Opening shock exceeds the CDR's own structural design point** (11–40 g computed against
   11.1 G stated) — the CanSat body and riser attachment, not the canopy, are the concern.
2. **Fabric permeability is unspecified**, so inflation cannot be assessed at all.
3. **The steering servo is named three different ways** across documents, so no torque margin
   can be stated.
4. **The CDR describes a different canopy** from the one being built and must be rewritten.

A fifth item is a competition-compliance question for the organisers, not an engineering one.

---

## 2. Mission requirements and competition constraints
Source: `IN-SPACe.CAN.MR.CG.01` Issue 01, 24-03-2026 (40 pp). Register:
`01_params/requirements_verbatim.md`.

| ID | Requirement | § |
|---|---|---|
| R-01 | Total mass under 1.0 kg (±0.050 kg) | 5.1 i |
| R-02 | Fits 0.15 m dia × 0.40 m height | 5.1 ii |
| R-06 | Primary parachute "connected to the outer body … no ejection mechanism attached to it" | 5.4 iv |
| R-07 | Primary descent rate < 20 m/s (±5) | 5.4 v |
| **R-08** | **Secondary activates at 600 m (±10 m), descent to 1–3 m/s** | 5.4 vi |
| R-10 | Stabilise using mechanical gyroscopic wheels | 5.4 viii |
| R-11 | Launch to 1000 m (±100); ejection orientation uncontrolled | 3.1 |

**Precision landing is an *optional* objective with no numeric tolerance anywhere in the rules.**
Dispersion is reported as-is; no pass/fail threshold has been invented.

**The target is the launch pad.** The rules say "targeted landing near to the launch pad", the
organiser note gives the "launch coordinates for the targeted landing site", and CDR Table 16
confirms "Target waypoint: Launch pad GPS coordinates". This is a return-to-launch-site problem,
which is why the offset to overcome is only 47–187 m.

**Compliance question for the organisers:** R-06 requires the primary parachute to stay connected
to the outer body, yet the CDR's state machine releases it at 600 m (`AEROBRAKE_RELEASE`).
Whether a cut-away violates R-06 is genuinely ambiguous in the written rule. **Ask before FRR.**

---

## 3. Verified computer, software and hardware inventory
| Item | Status |
|---|---|
| CPU / RAM | Ryzen 7 4800H, 8C/16T, 23.4 GB usable |
| Disk | D: 312 GB free |
| **Octave 11.3.0** | working (tested) |
| **ANSYS Fluent** | **licensed** — `cfd_base`, `cfd_solve_level1/2` (Shared Web), 2 seats |
| **ANSYS Mechanical** | **licensed** — feature `ansys` = Mechanical Enterprise, plus `HPC_PARALLEL` (4 cores) |
| LS-DYNA | installed, entitlement untested |
| MATLAB | **unusable** — licence file is `.deact`, no `matlab.exe` |
| Python | miniconda 3.12.9 + numpy/scipy/matplotlib/pypdf (installed during this work) |

My first pass wrongly reported ANSYS as unlicensed. `ansysli_util -printavail` cannot query Ansys
Common/Elastic licensing and says so in its own error text; the checkout logs are the evidence.
**The CDR cites a MATLAB wind-drift simulation that cannot be reproduced on this machine**, and
the CDR states neither a wind speed nor a dispersion radius for it.

---

## 4. Reconstructed design and deployment sequence
Rocket to ~1000 m → CanSat ejected (orientation uncontrolled) → flat-circular vented drogue
deploys on drag alone, Ø380 mm, 8 shroud lines → descent at 14.4–22 m/s (the CDR states this
three different ways) → at 600 m AGL a servo releases the bay lid and the ram-air parafoil
deploys → glide back to the launch pad under two-servo brake-line steering, flywheel-stabilised
→ touchdown at ~2.2 m/s, beacon ≥92 dB.

---

## 5. Design parameter register
Full register: `01_params/`. Discrepancies: `01_params/DISCREPANCY_REGISTER.md`.

| Parameter | Value | Source / confidence |
|---|---|---|
| Flat / projected area | 0.71 / 0.67 m² | LE, **validated 4 ways** |
| Flat / projected span | 1.42 / 1.32 m | LE, validated |
| Aspect ratio | 2.82 / 2.62 | LE, validated |
| Cells / ribs | 11 / 12 | LE (user-confirmed as flight article) |
| Rib chords | 55.00 → 41.25 cm | **measured = LE exactly, 6 of 6** |
| Airfoil t/c | **19.1 % at x/c 0.308** | measured from LE seam lines |
| Airfoil camber | **~1.1 %** | measured (the CDR's 9 % is wrong) |
| Vault arrow | 0.221 m | **rebuilt from rib angles**; LE states 0.22 |
| Line height / risers | 1.10 m / 3 × 10 cm | LE |
| Lines | **36 load-bearing + 12 brake; 36.6 m (≈43 m with loops)** | LE one-side listing, doubled |
| Fabric | Ripstop Nylon 40D, **1.568 m² → 63 g** | CDR + measured area |
| Suspended mass | 0.86 kg (920 g total − 60 g released) | **assumption** |
| Trim AoA | 8.61° | LE calage |
| Site elevation | 80 m AMSL | **assumption, unverified** |

**Unresolved:** servo part number; drogue + plate mass; fabric areal density and permeability;
site elevation; whether 0.4–0.8 mm line can be sourced.

---

## 6. Analytical validation (GNU Octave)
Ten scripts, all re-runnable from a clean session: ISA atmosphere, drag build-up, R-08
compliance, CD0 sensitivity, canopy-area trade, wind profile, phase-1 drift, two guidance laws,
Monte Carlo dispersion (3000 runs), structural loads, cascade statics.

The analytical lift slope (Helmbold, 3.108 /rad at AR 2.62) was **later confirmed by CFD** to
three significant figures — see §9.

---

## 7. Solver selection and setup
- **Fluent 2D RANS** on the rib section: k-ω SST, incompressible, ρ 1.147 kg/m³, μ 1.78×10⁻⁵,
  V 4.75 m/s, **Re 1.68×10⁵**, 2500–3000 iterations, **each angle in its own process** (a shared
  session silently reused one converged field and gave identical forces at every angle).
- **Mesh written directly to Fluent `.msh`** by `04_cfd/gen_ogrid.py` — no mesher involved,
  because 3D volume meshing is blocked on this machine (below).
- **Mechanical/MAPDL** for the structural attempt; superseded by a determinate statics solve (§10).

**3D CFD is blocked and no 3D result is claimed.** Two independent failures: the meshing host
process fails a fixed ~1.94 GB allocation *identically* for a 300 m³ and a 22.5 m³ model (so not
mesh-size dependent), and the skip-remeshing path fails Share Topology's intersect, producing a
zero-cell surface-only mesh the solver rejects as "invalid grid". Six attempts. Fluent itself
recommends performing the boolean in SpaceClaim.

---

## 8. Mesh quality, convergence and numerical verification
Three systematically refined O-grids at the 8.61° design point:

| Grid | Cells | first cell | CL | CD | L/D |
|---|---|---|---|---|---|
| coarse | 13 280 | 1.2e-4 | 0.9076 | 0.0454 | 19.99 |
| medium | 26 640 | 6.0e-5 | 0.9147 | 0.0495 | 18.48 |
| fine | 53 640 | 3.0e-5 | 0.9015 | 0.0520 | 17.33 |

- y⁺ ≈ 1 (wall-resolved), far field at 25 chords, all cell volumes positive.
- **CL converged:** changes under 1.5 %, oscillatory → **CL = 0.90 ± 0.015**.
- **CD not fully converged:** monotonic rise; observed order p = 1.41, Richardson-extrapolated
  **CD = 0.0559 with GCI 9.4 %** → **CD = 0.056 ± 0.005**. Even the fine grid under-predicts
  drag by ~7 %.
- Continuity residual plateaued at ~3×10⁻³ (momentum and turbulence ~10⁻⁶).

**My own generator had a defect**: the far-field blend produced 64 negative-area cells that
segfaulted Fluent with zero iterations. Fixed with a radius-anchored radial fan outboard of 2
chords, plus a hard gate that refuses to write any mesh containing a non-positive cell.

---

## 9. Aerodynamic performance
2D polar (α → CL, CD): 0° → 0.033, 0.029 · 4° → 0.470, 0.034 · 8.61° → 0.863, 0.053 ·
11° → 1.115, 0.064 · 14° → 1.345, 0.093. Max sectional L/D ≈ 17.6 near 11°.

**Two independent confirmations:**
1. The CFD 2D lift slope of 6.26 /rad, reduced by Helmbold at AR 2.62, gives **3.11 /rad** — the
   analytical model had used **3.108 /rad** with no CFD input.
2. Canopy CD0 = 2D profile drag (computed) + inlet/roughness (not computed). The increment needed
   to reproduce LE's GR 2.70 is **0.075**; the CDR's 2.50 needs **0.090**. Both sit inside
   Lingard's published 0.05–0.13 band for ram-air canopies, so **both published figures are
   credible.** My own first-pass 3.54 was the outlier, from assuming CL0 = 0.40 before the
   profile was measured.

**Best estimate:** CL 0.500, **GR 2.50–2.70, descent rate 2.17–2.31 m/s, airspeed 5.75–5.84 m/s
(21 km/h)**, descent time 260–277 s, still-air range ~1500 m.

---

## 10. Structural integrity
Full detail: `STRUCTURAL_LINE_LOADS.md`. LE never solved its line loads (its §17 reads *"Don't
try to solve equilibrium equacions"*), so these are derived independently.

The cascade geometry was reconstructed from LE's per-line midpoints (`node = 2 × midpoint −
parent`) and **validated to 0.08 %** across all 18 load-bearing lines of plans A/B/C.

**Load sharing is 4.76 : 1, not equal.** With cascade inclinations of 9.2–39.6° from vertical,
the worst line carries **2.07× the equal-share tension** — my earlier equal-sharing margins were
2× optimistic.

| Case | worst element | tension | rating | margin |
|---|---|---|---|---|
| Steady flight (8.43 N) | upper 0.4 mm | 0.97 N | 200 N | 206× |
| **Opening shock (187 N)** | **upper 0.4 mm, C row** | **21.5 N** | 200 N | **9.3×** |

**Minimum margin anywhere in the line system: 9.3×.** Fabric hoop tension gives >200× by
closed-form calculation. The solve is validated twice: force closure to 2.6 %, and the
resultant's 12.4° chordwise tilt matches LE's independently-stated assiette of 11.71°.

**The membrane FEA was attempted three times and every result rejected** — including one that
converged with zero errors but returned 842 MPa and 0.531 m deflection (≈10× nylon's strength,
97 % of chord). Quasi-static membrane inflation is the wrong solver class; it needs explicit
dynamics (LS-DYNA) or coupled FSI.

---

## 11. Deployment, inflation and opening loads
| Deploy speed | Cx 1.2 | Cx 1.5 | Cx 1.8 |
|---|---|---|---|
| 14.4 m/s (CDR Table 7) | 11.3 g | 14.2 g | 17.0 g |
| 18 m/s (CDR Table 6) | 17.7 g | **22.1 g** | 26.6 g |
| 22 m/s (R-07 limit) | 26.5 g | 33.1 g | **39.7 g** |

**Every combination exceeds the CDR's stated 11.1 G structural design point**, by 1.0× to 3.6×.
The canopy and lines survive comfortably; the CanSat body, riser anchor and PCB stack are the
concern and have not been re-analysed at these levels.

**Inflation itself is NOT VERIFIED and cannot be**: fabric permeability is unspecified and it
governs whether and how fast the canopy inflates.

---

## 12. Servo and steering authority — NOT VERIFIED
The servo is named **three different ways**: AGFRC **B26CLM V2** (10.5 kgf·cm) in the current
brief, **A20CLM** (10.2 kgf·cm) in the project research doc, and **A20BHM** in CDR Table 33 and
the BOM. No torque margin can be stated until this is resolved.

Order-of-magnitude estimate only: at a brake force of 5–15 N and an arm or spool radius of
15–25 mm, demand is 0.8–3.8 kgf·cm against ~10 kgf·cm rated → 2.6–13× margin, **probably
adequate**. The CDR's own brake drag figure is 0.661 N per side. Both servos share one 8 V 5 A
buck, so simultaneous stall current must be checked against that rail.

**Controller constraint:** the payload pendulum period on 1.10 m lines is **2.10 s**. The
steering loop must stay well below ~0.5 Hz or it will excite the swing.

---

## 13. Wind sensitivity and landing dispersion
October and November are the **calmest months** at the nearest station (Gorakhpur, ~50 km):
mean surface wind ~1.3–2.0 m/s across several sources. Monte Carlo, 3000 runs, sweeping wind
(right-skewed, mean ~2 m/s at 10 m, power-law shear α 0.17), CL0 ±0.02, inlet drag 0.05–0.13,
mass ±40 g and trim ±1°:

| Metric | Value |
|---|---|
| **CEP50** | **30.6 m** |
| CEP90 | 92.5 m |
| CEP95 | 182.5 m |
| within 50 m | 65.3 % |
| **within 100 m** | **91.5 %** |
| **P(blown downwind, unrecoverable)** | **7.7 %** |
| R-08 satisfied | **95.0 %** of runs |

The canopy has **7–28× more glide range than it needs** (≈1500 m available against 47–187 m
required), so altitude must be deliberately burned off — loiter over an initial point, then an
into-wind final leg. Guidance law is a second-order effect next to wind penetration.

---

## 14. Limitations, uncertainties and failure modes
1. **No 3D CFD** — blocked. Tip, arch and spanwise effects are applied analytically only.
2. **Inlet and roughness drag bounded by literature, not computed** — roughly a third of total drag.
3. **CD carries 9.4 % GCI** and is still rising with refinement.
4. **Inflation NOT VERIFIED** — permeability unknown.
5. **CanSat structure not analysed at 20–40 g.**
6. **Uniform chordwise lift assumed** in the line-load solve (probably conservative, but reasoned
   rather than computed).
7. **No experimental validation of anything** — no wind tunnel, no drop test.
8. Unanalysed risks: line twist on deployment, asymmetric inflation, spiral dive, gyroscopic
   coupling between the stabilisation flywheel and steering, GNSS loss, wrong ground elevation
   at the flare.

---

## 15. Design modifications by priority
1. **Re-verify the CanSat load path at 20–30 g**, or add a slider/reefing stage to cap inflation
   Cx — the cheapest way to cut peak load.
2. **Fix the servo part number**, then compute the real torque and stall-current margin.
3. **Measure the fabric**: weigh a 10×10 cm sample for areal density; obtain or test permeability.
4. **Build the rigging accurately.** R-08 is violated on the *fast* side below ~7° incidence and
   trim is 8.61°, leaving only ~1.6° of margin. Rigging flatter than designed breaches the rule.
   Err nose-up; keep suspended mass at or below ~0.9 kg.
5. **Source 0.4–0.8 mm UHMWPE** for the upper cascade. Using 1.8 mm throughout costs ~7 % glide
   ratio in added line drag; strength is irrelevant (300 daN against a 21.5 N peak demand).
6. **Implement energy-managed guidance**, not naive homing.
7. **Rewrite the CDR's secondary-descent section** (see §17).

---

## 16. Physical testing and flight readiness
1. Bench: servo travel and force at 8 V, simultaneous stall current, brake-line routing,
   line-twist prevention. Pass: full deflection without stall, rail holds ≥7.5 V.
2. Fabric coupon: areal density, tensile strip test, permeability.
3. Pack-and-deploy drill ×10: no line twists, clean bag extraction.
4. **Drop test from 100–400 m** (check DGCA rules): verify inflation, descent rate against the
   1–3 m/s rule, and steering response. This is the single highest-value test — it validates
   inflation, which no simulation here can.
5. Car-top or kite tow: trim angle and steering authority at representative airspeed.
6. Full-altitude rehearsal if possible, logging the Board 5 black box for replay.

---

## 17. Conclusions and outstanding work

| Area | Verdict | Basis |
|---|---|---|
| Design-tool geometry correctness | **PASS** | 4 independent validations |
| Descent rate vs R-08 | **CONDITIONAL** | 95.0 % of MC runs; fails on the fast side below ~7° trim |
| Steady aerodynamic performance | **CONDITIONAL** | CFD-anchored; inlet drag from literature; CD ±9.4 % |
| Glide performance / descent time | **CONDITIONAL** | same dependency |
| Line and riser strength | **PASS** | 9.3× minimum with realistic 2.07× load concentration |
| Fabric and seam strength | **PASS** | >200× closed-form |
| CanSat structure at opening shock | **FAIL / NOT VERIFIED** | 11–40 g against an 11.1 G design point |
| Deployment and inflation | **NOT VERIFIED** | permeability unspecified |
| Servo and steering authority | **NOT VERIFIED** | part number ambiguous three ways |
| Precision landing | **CONDITIONAL** | CEP50 30.6 m, 91.5 % within 100 m, 7.7 % blow-away |
| **Overall mission readiness** | **NOT FLIGHT-READY** | the four items above must close |

### Answer to the question that framed this work
> *Given the actual geometry, 1 kg mass, deployment at 600 m, available hardware and expected
> winds — is the system aerodynamically feasible, structurally adequate, capable of controlled
> gliding, and capable of the required landing accuracy?*

**Aerodynamically feasible: yes**, with CFD-anchored evidence. Descent rate and glide ratio are
consistent across three independent routes.

**Structurally adequate — in the canopy: yes**, by a wide and now properly quantified margin.
**In the CanSat: unproven and probably marginal**, because opening shock exceeds the structure's
own stated design point in every deployment case.

**Capable of controlled gliding: yes**, with ample control time (260–277 s) and 7–28× surplus
range — provided energy-managed guidance is implemented and the steering loop stays clear of the
2.10 s pendulum.

**Capable of the required accuracy: there is no required accuracy.** Precision landing is
optional and the rules set no tolerance. Predicted performance is ~30 m median with 91.5 %
inside 100 m, and a 7.7 % chance of being carried away by wind exceeding the canopy's 5.8 m/s
airspeed — a risk inherent to a lightly loaded canopy, now quantified rather than hidden.

### Outstanding work, in order
1. Re-analyse the CanSat load path at 20–30 g (Mechanical is licensed and ready).
2. Measure the fabric; inflation can then be assessed.
3. Resolve the servo part number.
4. Drop test.
5. Unblock 3D CFD via SpaceClaim's boolean, if 3D coefficients are wanted.
6. Rewrite the CDR: canopy area 0.40 → 0.71 m², 7 → 11 cells, rectangular → tapered,
   t/c 14 → 19.1 %, camber 9 → 1.1 %; reconcile the primary descent rate (stated as 14.4 / 15 /
   18–22 m/s) and the flywheel KV (2300 against 1590).
