# 2D RANS of the PF072 rib section — results
2026-10-20. Pins down the aerodynamic assumption that the whole analysis rested on.

## Why 2D
3D volume meshing is blocked on this machine (see `SIMULATION_STATUS.md`: a size-independent
~1.94 GB allocation failure, plus a Share Topology intersect failure). A 2D section RANS uses a
completely different code path, fits easily in memory, and directly resolves the dominant
unknown — the canopy's profile drag. Its limitation is stated plainly at the end.

## Method
- **Geometry:** rib-1 `seam` polyline from `lep-2d-parts.json` — the *as-designed* sewn profile,
  chord 550 mm, t/c 19.1 % at x/c 0.308, camber ~1.1 %, TE thickness 0.75 % chord.
- **Mesh:** structured O-grid written directly to Fluent `.msh` by `04_cfd/gen_ogrid.py`
  (no mesher involved). 240 x 112 = 26 640 quads, first cell 6x10^-5 m (**y+ ~ 1**, resolved to
  the wall), geometric growth r = 1.094, far field at **25 chords**.
  Verified by Fluent: all volumes positive, total 524 m3, 5 of 53 520 faces left-handed.
- **Physics:** incompressible, rho = 1.147 kg/m3 (600 m AGL), mu = 1.78x10^-5,
  V = 4.75 m/s -> **Re = 1.68x10^5**. k-omega **SST**, 2500 iterations per angle.
- Each angle run in its **own Fluent process** — a shared-session loop silently reused one
  converged field and gave identical forces at every angle (caught and discarded).
- Coefficients computed from raw wall forces normalised by hand, not read from Fluent's own
  coefficient reports.

## Polar
| alpha (deg) | CL | CD | L/D |
|---|---|---|---|
| 0.00 | 0.0334 | 0.0290 | 1.16 |
| 2.00 | 0.2596 | 0.0312 | 8.33 |
| 4.00 | 0.4702 | 0.0340 | 13.83 |
| 11.00 | 1.1153 | 0.0635 | 17.56 |
| 14.00 | 1.3453 | 0.0927 | 14.51 |

- **Lift slope 0.109 /deg = 6.26 /rad**, i.e. essentially 2*pi — textbook thin-airfoil behaviour.
- **CL0 = 0.0334**, consistent with the measured 1.1 % camber. The CDR's assumed 9 % camber
  would have given roughly ten times this.
- **CD at zero incidence = 0.0290**; at the 8.61 deg design incidence ~0.053.
- Max sectional L/D ~17.6 near 11 deg; by 14 deg drag is rising sharply (approaching stall).

## Two independent confirmations
**1. The 3D lift slope I had assumed is correct.** Reducing the CFD 2D slope of 6.26 /rad by the
Helmbold low-aspect-ratio relation at AR 2.62 gives **3.11 /rad**. The analytical model had been
using **3.108 /rad** from the same relation, derived without any CFD. Independent agreement to
three figures.

**2. Both published glide ratios are physically reachable.** Canopy CD0 = 2D profile drag (which
CFD gives) + ram-air inlet + fabric/rigging roughness (which it does not). Solving for the
increment needed to reproduce each published figure:

| Target GR | Required inlet+roughness dCD | Total CD0 | Verdict |
|---|---|---|---|
| LE's 2.70 | **0.075** | 0.128 | inside Lingard's 0.05-0.13 band for ram-air canopies |
| CDR's 2.50 | **0.090** | 0.143 | inside the same band |

Neither figure requires anything unphysical, so **LE's GR 2.70 and the CDR's 2.50 are both
credible**, and the earlier concern that LE was optimistic is withdrawn. My own first-pass
estimate of 3.54 was the outlier, caused by assuming CL0 = 0.40 before the profile was measured.

## Revised performance (CFD-calibrated, dCD 0.075-0.090)
| Quantity | Value |
|---|---|
| CL at trim | 0.500 |
| Glide ratio | 2.50 - 2.70 |
| **Descent rate** | **2.17 - 2.31 m/s** (mid-band in the 1-3 m/s rule) |
| **Forward airspeed** | **5.75 - 5.84 m/s (21 km/h)** |
| Descent time from 600 m | 260 - 277 s |

This is **better than the pre-CFD estimate**: airspeed rises from 4.48 to ~5.8 m/s (+30 %), which
directly reduces the risk of being blown downwind, and descent rate sits comfortably mid-band
rather than near the 1 m/s floor.

## New finding: trim angle has little margin on the fast side
The CFD-calibrated trim sweep fails the 1-3 m/s rule **below about 7 deg incidence** (at 6 deg the
descent rate is 3.3 m/s). The design trim is 8.61 deg, so there is only ~1.6 deg of margin before
the canopy descends too fast. **Rigging length and riser trim must be built accurately** — a
canopy rigged flatter than designed breaches R-08. There is no corresponding risk on the slow
side; more incidence only reduces descent rate.

## Limitations — read before quoting these numbers
1. **2D only.** No tip vortices, no arch (anhedral) effect, no spanwise flow. 3D effects are
   applied analytically via Helmbold, not resolved.
2. **Rigid, closed section.** The real canopy has an **open inlet** and a flexible, inflated,
   slightly distorted surface. The inlet and roughness drag are *not* computed here — they are
   the swept increment, bounded by published data rather than by this simulation.
3. **Continuity residual plateaued at ~3x10^-3** (momentum and turbulence residuals reached
   ~10^-6). Adequate for force coefficients, not tight convergence.
4. **Mesh independence not yet confirmed** — coarse (13 280) and fine (53 640) cell grids are
   generated and queued; until those complete, discretisation error is unquantified.
5. One degenerate cell remains at the blunt trailing edge (minimum orthogonal quality 0).
6. **No experimental validation.** Nothing here has been checked against a wind tunnel or a
   drop test.

**Status: CONDITIONAL.** The section aerodynamics are now anchored by simulation rather than
assumption, and the result is self-consistent with both design documents. The open-inlet
contribution — roughly a third of total drag — remains bounded by literature, not computed.

---

# Addendum: mesh independence and the recalibrated mission result

## Mesh independence — NOT yet demonstrated
Design point 8.61 deg, three systematically refined O-grids:

| Grid | Cells | first cell (m) | CL | CD | L/D |
|---|---|---|---|---|---|
| coarse | 13 280 | 1.2e-4 | 0.9165 | 0.0463 | 19.77 |
| medium | 26 640 | 6.0e-5 | **0.8634** | **0.0529** | **16.33** |
| fine | 53 640 | 3.0e-5 | 0.9015 | 0.0520 | 17.33 |

Coarse -> medium shifts CL by **-5.8 %**, CD by **+14.3 %** and L/D by **-17.4 %**. That is a
large change, so **the medium grid is not demonstrably in the asymptotic range** and the drag
coefficient carries meaningful discretisation uncertainty. The trend (CD rising with refinement)
suggests the medium grid may still *under*-predict drag. The fine-grid result is required before
the quoted CD of 0.0529 can be treated as converged; until then treat it as +/- ~15 %.

## Recalibrated mission result (3000-run Monte Carlo)
Nominal, CFD-calibrated: CL 0.500, CD 0.193, **GR 2.59, Vz 2.24 m/s, Vx 5.80 m/s**, 268 s descent.

| Metric | First pass | Pre-CFD | **CFD-calibrated** |
|---|---|---|---|
| CEP50 | 28.4 m | 29.5 m | **30.6 m** |
| CEP90 | 299 m | 132 m | **92.5 m** |
| CEP95 | 527 m | 280 m | **182.5 m** |
| within 100 m | 84.5 % | 88.2 % | **91.5 %** |
| **P(blown downwind)** | 19.8 % | 12.3 % | **7.7 %** |
| R-08 satisfied | - | 99.3 % | **95.0 %** |

The tail improves markedly: **CEP90 falls from 299 m to 92.5 m and the blow-away probability
from 19.8 % to 7.7 %**, because the CFD-calibrated airspeed (5.80 m/s) is ~30 % higher than the
pre-CFD estimate. Median accuracy is unchanged at ~30 m, as it always was — the area/airspeed
question only ever affected the tail.

## But one thing got worse: the fast-side descent-rate margin
R-08 compliance drops from 99.3 % to **95.0 %**, with descent rate ranging 1.48-4.26 m/s across
the uncertainty sweep. The cause is the CFD-measured CL0 of 0.033 (against the 0.13 previously
assumed), which lowers trim CL to 0.500 and therefore raises sink rate. **About 1 run in 20
breaches the 3 m/s ceiling**, in the corner where incidence is rigged low, mass is high and inlet
drag is low.

This reinforces the trim-angle finding: the rule is violated on the **fast** side, so rigging the
canopy with *less* incidence than the designed 8.61 deg is the dangerous error. Mitigations:
build the rigging to length carefully, keep suspended mass at or below ~0.9 kg, and consider
trimming slightly nose-up (the trim sweep shows more incidence only improves R-08 compliance).

---

# Mesh independence — COMPLETE (supersedes the addendum table above)

My grid generator had a defect: the normal-marching blend into the far field produced
**64 cells of negative area** on the 360x150 grid, which segfaulted Fluent during partitioning
with zero iterations. Fixed by extending outboard of 2 chords along pure radial rays *anchored
to the marched radius* (an absolute jump to `cen + u*dc[j]` pushed points inward past the
previous ring and made things worse — 106/192/318 bad cells). A hard validation gate now
refuses to write any mesh containing a non-positive cell. All three grids pass at zero.

All three were then re-run with the corrected generator for consistency:

| Grid | Cells | first cell (m) | CL | CD | L/D |
|---|---|---|---|---|---|
| coarse | 13 280 | 1.2e-4 | 0.9076 | 0.0454 | 19.99 |
| medium | 26 640 | 6.0e-5 | 0.9147 | 0.0495 | 18.48 |
| **fine** | **53 640** | **3.0e-5** | **0.9015** | **0.0520** | **17.33** |

| Step | CL | CD |
|---|---|---|
| coarse -> medium | +0.78 % | +9.03 % |
| medium -> fine | -1.44 % | +5.05 % |

**CL is converged:** changes stay under 1.5 % and are oscillatory, giving
**CL = 0.90 +/- 0.015** at the 8.61 deg design point.

**CD is not fully converged.** It rises monotonically with refinement. Richardson extrapolation
gives observed order **p = 1.41**, extrapolated **CD = 0.0559**, with **GCI = 9.4 %**. Even the
fine grid therefore under-predicts drag by roughly 7 %.

### Corrected design-point values
| Quantity | Value |
|---|---|
| CL (8.61 deg) | **0.90 +/- 0.015** |
| CD (8.61 deg) | **0.056 +/- 0.005** (Richardson-extrapolated; fine grid reads 0.052) |
| Sectional L/D | **~16.1** |

This supersedes every earlier CD figure in this document (0.0529 on the original medium grid,
then 0.0495 on the corrected medium grid). **Mission conclusions are unchanged**: the 2D profile
term rises from 0.053 to 0.056, a ~6 % increase on a quantity the inlet/roughness increment
dominates, so glide ratio, descent rate and landing dispersion all move by less than their
existing uncertainty bands.
