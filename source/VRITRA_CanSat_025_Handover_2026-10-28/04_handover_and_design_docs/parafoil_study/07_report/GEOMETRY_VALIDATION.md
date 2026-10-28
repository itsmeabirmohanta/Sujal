# Does LEparagliding's output actually describe the wing it claims?
Four independent checks. **All four pass.** This answers the first of the two driving
questions: the design tool's geometry output is sound and can be trusted as the design basis.

## Check 1 — 3D mesh areas vs stated areas
From the STL surfaces (converted cm -> m, `03_geom`):

| Quantity | Measured | LE states | Agreement |
|---|---|---|---|
| Lower-surface area | 0.7057 m2 | 0.71 m2 (flat) | 0.6 % |
| Upper-surface XY projection | 0.6866 m2 | 0.67 m2 (projected) | 2.5 % |
| Lower-surface XY projection | 0.6265 m2 | 0.67 m2 | 6.5 % |
| Total fabric surface | 1.5680 m2 | — | (used for cloth order) |

## Check 2 — rib chords from the 2D cutting data
Measured off the `seam` polylines in `lep-2d-parts.json`, independent of the 3D mesh:

| rib | 1 | 2 | 3 | 4 | 5 | 6 |
|---|---|---|---|---|---|---|
| measured (cm) | 55.00 | 54.45 | 52.80 | 50.05 | 46.20 | 41.25 |
| `lep-out.txt` (cm) | 55.00 | 54.45 | 52.80 | 50.05 | 46.20 | 41.25 |

**Exact to the millimetre, 6 of 6.** The 2D parts file also independently states
flatArea 0.7146 m2 and projectedArea 0.6694 m2.

## Check 3 — rebuild the arch from the rib beta angles
Taking only the per-rib arch angles beta = 0, 7.40, 14.79, 22.19, 29.59, 36.98 deg and the
14.2 cm cell width, and integrating along the arc:

| Quantity | Rebuilt | LE states |
|---|---|---|
| Flat span | **1.4200 m** | 1.42 m |
| Projected span | **1.3243 m** | 1.32 m |
| Vault arrow | **0.2215 m** | 0.22 m |
| Flattening | **0.0674** | 0.06 |
| Flat area (trapezoid over ribs) | **0.7146 m2** | 0.71 m2 |

The arch, projected span and vault arrow all fall out of the rib angles correctly. This is the
strongest of the four checks because it reconstructs the 3D shape from scratch.

## Check 4 — airfoil profile is consistent across all ribs
t/c = **19.09 %** at x/c = **0.308** for ribs 1-5 (17.18 % at the tip rib), camber ~1.1 % at
x/c 0.152, TE thickness 0.89 % chord. Constant to 2 decimal places across ribs, i.e. one
profile scaled by chord, as intended.

**This also corrects the CDR**, which claims t/c 14 % and camber 9 %. The thickness *position*
(CDR 32 % vs measured 30.8 %) was right; the thickness and camber values were not.

---

## Units trap (documented, gated)
The package mixes units: **STL files are centimetres, the 2D parts JSON is millimetres.**
Read as millimetres the canopy is 10x small and 100x wrong in area. `03_geom` therefore asserts
four geometric quantities before exporting, and the Fluent import is pinned to `MeshUnit="m"`.

## Method rejected, for the record
I implemented a hand-rolled vortex-lattice solver over the real arched planform to get an
independent 3D lift slope. **It is wrong and its numbers are discarded**: it returned negative
CL and a lift slope roughly 8x too high (~25/rad against the Helmbold expectation of 3.1/rad),
indicating sign and normalisation errors in the horseshoe influence terms. Rather than debug a
bespoke solver, the 3D aerodynamics is left to Fluent, with the Helmbold low-aspect-ratio
estimate as the analytical bound. No VLM figure appears anywhere in the results.
