# Simulation status — what ran, what did not, and why
2026-10-20. Written so no result here is mistaken for more than it is.

## Octave analytical model — COMPLETE, trustworthy
Eight scripts in `02_octave`, all re-runnable from a clean session. Atmosphere, drag build-up,
R-08 compliance, CD0 sensitivity, area trade-off, wind profile, phase-1 drift, two guidance
laws, Monte Carlo dispersion (2500 runs), structural loads. These are the basis of every
conclusion in `FINDINGS_v2_LE_design.md`.

## Geometry validation — COMPLETE, trustworthy
Four independent checks, all passing (see `GEOMETRY_VALIDATION.md`). The strongest rebuilds the
full 3D arch from only the per-rib beta angles and recovers LE's projected span, vault arrow,
flat span, flattening and area.

## ANSYS licensing — VERIFIED by execution, not inspection
- Fluent: `cfd_base` + `cfd_solve_level1` + `cfd_solve_level2` (Shared Web)
- MAPDL: feature `ansys` = **Ansys Mechanical Enterprise**, plus `HPC_PARALLEL` (4 cores)
- Confirmed by a completed MAPDL batch solve and Fluent checkout logs.

## Fluent 3D CFD — BLOCKED. No CFD result exists.
The headless chain is proven up to volume meshing: Python workflow API drives Fluent Meshing,
geometry imports correctly with `MeshUnit="m"` (15584 faces, bounding box 1.368 x 0.550 x
0.324 m matching the unit gate), and the workflow runs through Describe Geometry, Capping,
Update Boundaries, Create Regions and Update Regions. Volume meshing fails two different ways:

1. **With surface remeshing:** host process fails a fixed allocation of
   **1,936,024,426 bytes (~1.94 GB)**. Critically, *the requested size is identical for a
   300 m3 full model and a 22.5 m3 half model*, so this is **not** mesh-size dependent — it is
   an environment/configuration limit, not a modelling choice. 13.5 GB RAM was free at the time, but only
   **8.1 GB of commit** was available.
2. **Skipping surface remeshing:** `Apply Share Topology` fails its Intersect operation, so no
   closed fluid region is formed. The volume mesh task then reports success but writes a
   **surface-only mesh with zero cells** — the solver rejects it as "invalid grid". Fluent's own
   message recommends doing the boolean in SpaceClaim or DesignModeler instead.

Six attempts were made (full/half model, poly-hexcore/tetrahedral, with/without surface
remeshing, 300 m3 down to 22.5 m3, MinSize 6-30 mm, 1-4 processes). **No CL or CD from CFD is
reported anywhere**, and none should be inferred.

**To unblock:** do the fluid-domain boolean in SpaceClaim (installed) and export a closed fluid
body, or raise the Windows page-file/commit limit, or run the case on a larger machine. A 2D
RANS of the rib section is the cheap alternative and would pin the profile drag directly.

**Consequence:** `CD0_canopy` remains an *assumption* (0.18 used, swept 0.15-0.23). It is
bounded independently: LE's stated GR 2.70 implies ~0.20 and the CDR's 2.50 implies ~0.23, and
the measured-profile model gives GR 2.37 at 0.18. The three agree to within ~12 %, which is why
the mission conclusions survive without CFD — but the aerodynamic verdict stays
**CONDITIONAL**, not PASS.

## MAPDL canopy FEA — RAN, but the result is REJECTED
A membrane model was built directly from the watertight triangulation (7816 nodes, 15624
SHELL181 elements, 0.05 mm fabric, E 2.5 GPa, 186 Pa internal ram pressure at 18 m/s).

- Membrane-only + large deflection: failed, "excessive thickness change".
- Full shell + linear: converged with 0 errors, returning max von Mises **842 MPa** and max
  displacement **0.531 m**.

**Both numbers are physically invalid and are not used.** 0.531 m is 97 % of the chord, and
842 MPa is ~10x the strength of nylon. The cause is the boundary conditions: I restrained 24
nodes chosen by a crude geometric heuristic, which does not represent 24 slender lines carrying
tension to a confluence point. A valid model needs LINK180 line elements at LE's **actual**
anchor coordinates — which LE does supply, in the "Additional detailed lines properties" table
of `lep-out.txt` (x, y, z for all 24 lines) — plus geometric nonlinearity and fabric prestress.

**The analytical structural results are unaffected** and remain the basis for the structural
findings: fabric and lines are 200-700x overstrength, and opening shock of 11-40 g exceeds the
CDR's stated 11.1 G design point in every deployment-speed/Cx combination. That conclusion
comes from closed-form dynamic pressure, not from this FEA.

## Honest verdict table
| Area | Status | Basis |
|---|---|---|
| Geometry / design-tool correctness | **PASS** | 4 independent checks |
| Descent rate vs R-08 (1-3 m/s) | **PASS** | analytical, 99.3 % of 2500 MC runs |
| Steady aerodynamic performance | **CONDITIONAL** | 3-way agreement, but CD0 assumed; no CFD |
| Glide performance / descent time | **CONDITIONAL** | same dependency |
| Structural strength of canopy & lines | **PASS (analytical)** | 200-700x margins, closed-form |
| CanSat structure at opening shock | **FAIL / NOT VERIFIED** | 11-40 g vs 11.1 G design point |
| Deployment & inflation | **NOT VERIFIED** | fabric permeability unspecified |
| Servo & steering authority | **NOT VERIFIED** | servo part ambiguous 3 ways |
| Precision landing | **CONDITIONAL** | CEP50 ~30 m, 88 % within 100 m; 12.3 % blow-away risk |
| Overall mission readiness | **NOT FLIGHT-READY** | the four items above must close first |

---

# 3D CFD: addendum (2026-10-20, later)

## The original 3D blocker was found and fixed
LEparagliding leaves a **0.2 mm slot at the centreline** (its "zero-thickness central cell"):
the two half-wings end at x = +0.0001 m and x = -0.0001 m, with identical (y,z) point sets.
My capping sealed each half separately, producing **two lobes separated by a 0.2 mm fluid slot**
— which is what generated the sliver cells and the trapped interior pockets.

Fix: snap all nodes with |x| < 0.5 mm to x = 0 and weld. The two root loops then merge and
vanish (264 -> 136 boundary edges), giving **one watertight, single-connected body** (volume
8.73 L, surface 1.608 m2, bbox unchanged). Capping then needs only the 2 wingtips and 2 small
holes.

Result: **minimum orthogonal quality improved from 3.46e-08 to 0.1005** — a factor of 3 million.

## 3D solve WITHOUT prism layers: numerically valid, but cannot predict lift
912,651 cells (746k fluid after deleting the canopy interior). Pseudo-transient, k-omega SST.

| | before fix | after fix |
|---|---|---|
| continuity residual | 3 x 10^7 (diverged) | **2.6 x 10^-4** |
| velocity residuals | - | **2.2 x 10^-8** |
| max inlet pressure | 54 MPa (nonsense) | **0.94 Pa** |
| canopy Cp range | - | **-2.9 to +1.03** (textbook) |

**Cross-validation at zero incidence:** 3D gives CD **0.0290**; the independent 2D RANS gives
CD **0.0290**. Identical. |CL| 0.033 both ways.

At the 8.61 deg trim the converged result is **CL 0.0351, CD 0.0326, lift 0.606 N = 0.062 kg**.
**Those lift figures are not usable.** The lift slope is 0.008/deg against an expected
0.054/deg — **7x too low**. Cause, measured directly: **y+ on the canopy averages 100
(range 0.45-260) with no prism layers**, the first cell sitting up to 13 mm off a 0.55 m chord.
That cannot resolve circulation. Pressures and the flow pattern are trustworthy; the integrated
lift is not.

## Adding prism layers to reach y+ ~ 1: layers added, mesh unusable
Target first-layer height computed from Re 2.37e5: **9.84 x 10^-5 m** (cell centre at y+ ~ 1),
20 layers at growth 1.20 giving an 18.4 mm stack — a good match to the physical turbulent BL
thickness of 17.1 mm (3.1 % chord) at the trailing edge.

The layers **were** added successfully (scoped to the canopy via `BLZoneList`), giving
1,650,664 cells with 1,484,156 mixed wedge/tet cells in the fluid. But:

| mesh | cells | min orthogonal quality | solve outcome |
|---|---|---|---|
| no prism layers | 912,651 | 0.1005 | **converged** |
| 20 prism layers | 1,650,664 | **3.68 x 10^-5** | **diverged** (continuity 1 -> 2 x 10^18) |

The worst cell sits at x = 0.653 m — **95 % of the half-span, i.e. the wingtip**. My wingtip
caps are flat centroid fans; extruding 20 prism layers off a sharp capped tip edge makes the
layers self-intersect and collapse. The solve diverged exponentially within 70 iterations and
was stopped.

**No valid 3D lift coefficient exists.** None is quoted anywhere.

## What would actually fix it
Rebuild the **wingtip geometry** rather than capping it with a flat fan — either close each tip
with a smoothly curved surface, or exclude the tip caps from the bound-layer scope so prisms are
grown only on the upper and lower skins. Either is a bounded geometry task, not a solver setting.

**Meanwhile the 2D RANS remains the trustworthy aerodynamic source**: y+ ~ 1, 112 graded layers,
three systematically refined grids, Richardson extrapolation with GCI 9.4 %, and agreement with
both LE's and the CDR's published glide ratios. The 3D run would refine those numbers, not
replace them, and nothing in the mission conclusions depends on it.

## Prism layers: the blocker is the trailing edge, not the wingtips

My first diagnosis (flat wingtip caps) was **wrong**. I rebuilt both tips as smooth ellipsoidal
domes (7 rings, 20 mm, watertight manifold, span +9.1 mm) and the worst cell did not move:

| tip treatment | prism layers | stack | min orthogonal quality | worst cell location |
|---|---|---|---|---|
| none | 0 | - | **0.1005** | - |
| flat centroid fan | 20 | 18.4 mm | 3.68e-05 | (0.653, 0.452, -0.227) |
| **smooth dome** | 20 | 18.4 mm | **4.01e-05** | (0.656, 0.451, -0.227) |

Both worst cells sit at **y ~ 0.451 m = 82 % of chord**, i.e. at the **trailing edge** — and the
mesher reports duplicate nodes at y = 0.528 m (96 % chord) as well.

**Root cause:** the canopy's trailing edge is only **0.75 % of chord thick — about 3 mm** at the
tip. A prism stack must stay under roughly half of that (~1.5 mm) or the layers growing off the
upper and lower skins interpenetrate. But y+ ~ 1 needs a first layer of 9.84e-5 m, and at growth
1.2 the stack reaches:

| layers | stack | fits a 3 mm TE? |
|---|---|---|
| 4 | 0.53 mm | yes |
| 6 | 0.98 mm | yes |
| 8 | 1.62 mm | no |
| 10 | 2.55 mm | no |
| 20 | 18.4 mm | no (by 12x) |

So only **4-6 layers** fit — which resolves about 1 mm of a **17 mm** turbulent boundary layer.
That gives y+ ~ 1 at the wall but still cannot resolve the circulation, so it would not fix the
lift either. **Reducing the layer count is not a route to a trustworthy 3D lift.**

**What would work:** local trailing-edge mesh treatment — a dedicated TE size control with
refined surface mesh and a locally thinned prism stack, or deliberately blunting the TE to a
meshable thickness and accepting that modelling change. That is a genuine meshing project, not a
parameter tweak, and it is where this stops.

**Final position on 3D: no valid lift coefficient, and none is quoted.** The 2D RANS remains the
validated aerodynamic source (y+ ~ 1, 112 graded layers, three-grid study, GCI 9.4 %, agreement
with both published glide ratios). Nothing in the mission conclusions, the stitching decision or
the structural margins depends on the 3D result.
