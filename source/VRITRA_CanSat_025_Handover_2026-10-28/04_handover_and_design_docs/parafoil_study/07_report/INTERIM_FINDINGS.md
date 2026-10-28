# CAN-7USAT Parafoil — Interim Findings
2026-10-20. Evidence-backed; every number traces to a script in `02_octave` or a log in `04_cfd`.

## THE ACTION ITEM: the canopy is oversized. Do not cut 0.71 m2.

Monte Carlo landing dispersion, 1200 runs per area (`02_octave/run05_area_tradeoff.m`),
mass 0.860 kg, AR held at 2.62, wind drawn from a right-skewed distribution with mean
~2.0 m/s at 10 m, power-law shear to altitude:

| Area m2 | Vz m/s | Vx m/s | GR | R-08 | CEP50 m | CEP90 m | CEP95 m | P(blown downwind) |
|---|---|---|---|---|---|---|---|---|
| 0.30 | 2.59 | 6.79 | 2.62 | PASS | 32 | **95** | **105** | **1.7 %** |
| 0.35 | 2.36 | 6.31 | 2.67 | PASS | 30 | 88 | 102 | 3.1 % |
| **0.40** | **2.18** | **5.92** | **2.71** | **PASS** | **29** | **86** | **115** | **5.2 %** |
| 0.50 | 1.92 | 5.31 | 2.77 | PASS | 28 | 94 | 238 | 9.5 % |
| 0.60 | 1.73 | 4.86 | 2.81 | PASS | 27 | 192 | 367 | 13.9 % |
| **0.71 (as designed)** | 1.58 | **4.48** | 2.84 | PASS | 28 | **299** | **527** | **19.8 %** |

**Median accuracy is the same (~28-32 m) for every area.** The difference is entirely in the
tail. The as-designed 0.71 m2 canopy has a **~20 % chance of being blown downwind and missing by
hundreds of metres**; at 0.40 m2 that falls to 5 %, at 0.30 m2 to 1.7 %. Every area satisfies the
1-3 m/s descent rule, so **nothing is gained by the larger canopy and a great deal is lost.**

**Cause:** forward airspeed is the binding constraint, not descent rate. At 0.71 m2 the canopy
flies at only 4.48 m/s. Wind at 300 m exceeds that **20.8 %** of the time, and when it does the
CanSat cannot make headway upwind by any guidance law — it is simply carried away. At 0.40 m2
airspeed rises to 5.92 m/s and the exceedance falls to 5.4 %.

**Recommendation: cut the canopy at ~0.40 m2** — which is what the CDR originally specified.
Keep the LE 11-cell tapered construction if preferred, scaled down. Cloth is still uncut, so
this costs nothing now and is extremely expensive to fix later.

## Second finding: the system has 12-36x more glide range than it needs
GR 2.84 from 600 m gives ~1700 m of still-air range; the offset to fly back is only 47-140 m
(`run04_guidance.m`). Altitude **must** be deliberately burned off. Naive homing happens to work
in calm air (CEP50 35 m) because the ~18 m turn radius makes it spiral tightly onto the target,
but it has no margin strategy. A loiter-then-into-wind-final law is still the right design; my
first implementation scored worse (CEP50 59 m) purely because its final phase dropped target
tracking — a bug in my guidance, not a verdict on the method. **Guidance law is a second-order
effect compared with wind penetration.** Fix the area first.

## Third finding: the target is the launch pad
Rules 3.1 ("targeted landing **near to the launch pad**"), the organiser's note ("the **launch
coordinates** for the targeted landing site"), and CDR Table 16 ("Target waypoint: Launch pad GPS
coordinates") all agree. This is a **return-to-launch-site** problem, which simplifies guidance:
the offset to overcome is only the drift accumulated during rocket ascent plus the 22 s
drag-chute descent from 1000 to 600 m (47-187 m for wind 1-4 m/s).

**No landing-accuracy tolerance exists anywhere in the rules**, so no pass/fail threshold has
been invented. Precision landing is an *optional* objective.

## Fourth finding: wind season is favourable
October/November are the **calmest months** at the nearest station (Gorakhpur, ~50 km): mean
surface wind ~1.3-2.0 m/s across several sources. That is why median accuracy is good. The risk
sits in the upper tail of the wind distribution, which is exactly what the area change protects
against.

## Verified environment (corrects my own earlier error)
- **Fluent licensed and working**: `cfd_base` + `cfd_solve_level1` + `cfd_solve_level2` (Shared Web)
- **Mechanical licensed**: feature `ansys` = **Ansys Mechanical Enterprise**, plus `HPC_PARALLEL`
  (4 cores). Verified by a completed MAPDL batch solve, not by inspection.
- My first pass wrongly reported ANSYS as unlicensed: `ansysli_util -printavail` cannot query
  Ansys Common/Elastic licensing and says so in its own error. The checkout logs are the evidence.
- **Octave 11.3.0** working. **MATLAB is deactivated** (`.deact` licence, no `matlab.exe`), so the
  CDR's claimed MATLAB wind-drift simulation cannot be reproduced on this machine — and the CDR
  states no wind speed and no dispersion radius for it.
- Installed: numpy, scipy, matplotlib, pandas, pypdf, pdfplumber. Initial conda/pip failures were
  **network truncation** (`IncompleteRead`), not package tampering; retries fixed it.

## Geometry: unit trap found and closed
The LE STL files are in **centimetres**. All four assertion checks pass only on that reading
(`03_geom`): lower-surface area 0.7057 m2 vs LE's stated 0.71 m2, total fabric 1.5680 m2,
projected span 1.368 m, root chord 0.550 m. Read as millimetres the canopy is 10x small and
**100x wrong in area**. This agreement also **independently validates LEparagliding's geometry
output**. The canopy was closed headlessly to a watertight shell (264 boundary edges in 6 loops
-> 0 boundary edges, 0 non-manifold) and the Fluent import is proven with `LengthUnit [m]`.

Fabric mass at 40 g/m2 ripstop (CDR specifies Ripstop Nylon 40D): **63 g**, ~78 g with seams.

## Still open / not verified
- **CL and CD0 are assumed, not computed.** The whole analysis hinges on CD0_canopy, assumed
  0.18. LE's GR 2.70 implies ~0.20; the CDR's GR 2.50 implies ~0.23. Fluent must settle it.
  My first-principles build-up gave GR 3.54 at CD0 0.12 — **more optimistic than both documents**,
  which is why 0.18 was used and why the CFD matters.
- **Structural margins: NOT VERIFIED.** LE never solved the line loads (its section 17 says
  *"Don't try to solve equilibrium equacions"*), so the load column in its output is meaningless.
- **Fabric permeability unspecified** — it governs inflation, so inflation stays NOT VERIFIED.
- **Airfoil profile not yet validated.** Slicing the 3D arched surface in global z measures the
  vault slope, not the local profile (apparent t/c wrongly grows toward the tip), so it must be
  read from `lep-2d-parts.json` / the rib DXFs instead.
- **Servo part number is still ambiguous three ways** (B26CLM V2 / A20CLM / CDR's A20BHM).
- **Compliance question for the organisers:** rule 5.4 iv requires the primary parachute to stay
  "connected to the outer body ... no ejection mechanism attached to it", yet the CDR releases it
  at 600 m. Ask before FRR.
- Site elevation assumed 80 m AMSL; affects flare AGL, not materially the density.
