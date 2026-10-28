# Line load distribution — replaces LE's unsolved load column
2026-10-20. `lep-out.txt` section 17 reads *"Don't try to solve equilibrium equacions"*, so LE's
own load column (0.01-0.09 kg) is **unsolved, not computed**. These are derived independently.

## Geometry reconstruction — validated to 0.08 %
LE lists each line's **midpoint** coordinates and exact length. Given the karabiner at
(4.00, 2.75, 110.00) cm, each cascade node follows from `node = 2 x midpoint - parent`, applied
down the tree. Reconstructed lengths against LE's stated lengths:

| Plan | lines | worst error |
|---|---|---|
| A | riser + 2 mains + 3 uppers | **0.04 %** |
| B | riser + 2 mains + 3 uppers | **0.06 %** |
| C | riser + 2 mains + 3 uppers | **0.08 %** |
| 4 (brakes) | — | **44-63 % — excluded** |

The brake plan fails because brake lines run to the steering servo, not the karabiner. LE's own
load column shows 0.00 for every brake line, so omitting them from a flight/opening load case is
physically correct — brakes only carry load when actively pulled.

**Cascade topology** (from the `ap` column): riser carries 3 attachment points -> splits into
main1 (carries 2) and main2 (carries 1) -> main1 feeds the rib-2 and rib-4 uppers, main2 feeds
the rib-6 upper. Three such plans (A/B/C) per side, mirrored: **36 load-bearing lines total.**

The anchor points land at 9 %, 35 % and 67 % chord (rows A/B/C) and their vertical positions
span 4.2-25.0 cm, reproducing the **22.1 cm vault arrow** — an independent confirmation that the
reconstruction is geometrically consistent with the arch.

### Correction to an earlier statement
LE's 24-line listing and its 18.30 m total are for **one side**. The full canopy needs
**36.6 m of line, ~43 m with loops** — not the 21.66 m I quoted earlier. Against 60 m of cord
purchased that is still sufficient, but the margin is much tighter than I implied.

## Load sharing is NOT equal — 4.76 : 1
Lift was distributed across the 18 anchors by nearest-anchor (Voronoi) tributary projected area:

| Row | chord position | share each | vs equal share |
|---|---|---|---|
| A (fwd) | 9 % | 2.18 - 4.78 % | 0.39 - 0.86x |
| B (mid) | 35 % | 2.98 - 6.46 % | 0.54 - 1.16x |
| **C (aft)** | **67 %** | **4.41 - 10.39 %** | **0.79 - 1.87x** |

Equal sharing would be 5.56 % each. Combined with cascade inclination (lines sit at 9.2-39.6
degrees from vertical, so tension is amplified by 1/cos), the worst line carries
**2.07x the equal-share tension**. My earlier margin figures, which assumed equal sharing, were
therefore **2x optimistic**.

## Line tensions and margins
Statically determinate solve on the exact reconstructed geometry. Ratings from LE's own types
table (Riser12 100 daN, Main40 40 daN, Upper20 20 daN).

### Steady flight (total lift 8.43 N, 0.860 kg)
| Element | max tension | rating | **margin** |
|---|---|---|---|
| upper (0.4 mm) | 0.97 N | 200 N | **206x** |
| main (0.6 mm) | 1.78 N | 400 N | **225x** |
| riser (1.2 mm) | 2.23 N | 1000 N | **449x** |

### Opening shock (total 187 N, 18 m/s deployment, Cx 1.5)
| Element | max tension | rating | **margin** |
|---|---|---|---|
| **upper (0.4 mm)** | **21.54 N** | 200 N | **9.3x** |
| main (0.6 mm) | 39.42 N | 400 N | **10.1x** |
| riser (1.2 mm) | 49.40 N | 1000 N | **20.2x** |

**Minimum margin anywhere in the line system is 9.3x**, on the aft-row (C) upper lines at
opening shock. The purchased 1.8 mm UHMWPE cord (~300 daN) would give ~139x on that same line.

## Two independent validations of the solve
1. **Force closure:** the vector sum of all six riser forces is 191.9 N against 187.0 N applied
   — a **2.6 % closure error**, with the spanwise component exactly zero (perfect symmetry).
2. **Attitude check:** the resultant is tilted **12.4 degrees** from vertical in the chordwise
   plane. LE independently states the canopy *assiette* (pitch attitude) as **11.71 degrees**.
   Agreement to 0.7 degrees, from a completely separate part of the output.

## Limitations
- **Uniform chordwise lift assumed.** Real airfoil loading is front-loaded, which would shift
  load toward the A row. Since A-row lines sit at shallower inclination (9-12 deg vs 37-40 deg)
  they amplify tension less, so the uniform assumption is **probably conservative** for the
  worst-case line — but this is reasoning, not a computed result. The 2D CFD pressure
  distribution could settle it.
- **Brake lines excluded** (see above). A separate case is needed for the braked/flare condition,
  where brake-line tension is the load of interest.
- **Fabric stress is not from this model**; it comes from the closed-form pressurised-cell
  calculation in `02_octave/run08_structural.m` (>200x margin). The membrane FEA that would give
  it directly did not converge — see below.
- Line elastic stretch is not modelled; this is a rigid-geometry equilibrium solve. For load
  distribution in a determinate cascade that is appropriate.

## Why the membrane FEA was abandoned
Three attempts, all rejected:
1. **Membrane-only SHELL181 + large deflection:** failed with "excessive thickness change".
2. **Full shell, linear:** converged but returned 842 MPa and 0.531 m deflection — ~10x nylon's
   strength and 97 % of the chord. Physically invalid, caused by restraining 24 nodes chosen by a
   crude heuristic rather than modelling lines.
3. **Full shell + 36 LINK180 tension-only lines at the validated anchors, geometric
   nonlinearity:** "DOF LIMIT EXCEEDED" — rigid-body motion. The cause was my own physics error:
   **uniform internal pressure on a closed surface produces zero net force**, so the lines never
   tensioned. Re-running with the net aerodynamic load distributed as nodal lift still diverged,
   because a 0.05 mm membrane held by tension-only lines has no stiffness until it tensions.

A pure truss model cannot substitute: each anchor connects to exactly **one** line, leaving two
unrestrained DOF per anchor that in reality the fabric carries. That is why the determinate
statics solve above is the correct formulation for this question.

**Quasi-static membrane inflation is the wrong solver class for this problem.** Doing it properly
needs explicit dynamics (LS-DYNA is installed and licensed) with fabric prestress and a follower
pressure load, or a coupled FSI run. That is a substantial separate task and is **not** required
for the line-load question answered here.

**Status: line and riser strength PASS** (minimum 9.3x at opening shock, with realistic load
concentration). **Fabric and seam strength PASS by closed-form calculation.** Canopy deformation
and inflation dynamics remain **NOT VERIFIED**.
