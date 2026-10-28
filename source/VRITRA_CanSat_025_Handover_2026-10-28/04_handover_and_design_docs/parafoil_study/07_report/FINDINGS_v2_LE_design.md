# CAN-7USAT Parafoil — Findings v2: LE design locked in
2026-10-20. Supersedes v1 where noted. Design decision (user, 2026-10-20):
**use the LEparagliding-generated design; the CDR canopy was a rough hand estimate.**

## Locked configuration
| Parameter | Value | Source |
|---|---|---|
| Model | PF072-CANSAT, 11 cells, 12 ribs | `lep-out.txt` |
| Flat / projected area | **0.71 / 0.67 m2** | LE, independently confirmed (0.6 %) |
| Flat / projected span | 1.42 / 1.32 m | LE |
| Aspect ratio | 2.82 flat / **2.62 projected** | LE |
| Rib chords | 55.00, 54.45, 52.80, 50.05, 46.20, 41.25 cm | **measured = LE exactly, 6/6** |
| Airfoil t/c | **19.1 % at x/c 0.308** (17.2 % at tip) | measured from LE seam lines |
| Airfoil camber | **~1.1 % at x/c 0.152** | measured from LE seam lines |
| TE thickness | 0.89 % chord | measured |
| Trim AoA / calage | 8.61 deg, pilot centring 5.0 % | LE |
| Line height incl. risers | 1.10 m (risers 3 x 10 cm) | LE |
| Lines | 24, 18.30 m (21.66 m with loops), UHMWPE | LE `lines.txt` |
| Fabric | Ripstop Nylon 40D, **1.568 m2** needed | CDR + measured area |
| Fabric mass | **63 g** at 40 g/m2 (~78 g with seams) | computed |

### LEparagliding is self-consistent — the design tool checks out
Two independent validations, both passing:
1. All **six** rib chords measured off the 2D cutting data match `lep-out.txt` to the
   millimetre.
2. Lower-surface area from the 3D mesh = 0.7057 m2 vs LE's stated flat area 0.71 m2 (0.6 %);
   projected areas 0.627-0.687 m2 vs stated 0.67 m2. The 2D parts file independently states
   flatArea 0.7146 / projectedArea 0.6694 m2.

**Note:** LEparagliding is GPL Fortran from laboratoridenvol.com (Pere Casellas) — it is *not*
on GitHub. Parameters were taken from its actual output files for this wing, which is the
better source than any generic template.

**Unit warning:** the STL files are in **centimetres**, the 2D parts JSON is in **millimetres**.
Two different units in one package. Read as mm the canopy is 100x wrong in area.

## Corrected aerodynamics — and all three sources now agree
Extracting the real profile changed the answer. Camber is only ~1.1 %, not the 9 % the CDR
assumed, so zero-alpha lift is far lower: CL0 ~ 0.13, not the 0.40 used in v1.

| Source | Glide ratio |
|---|---|
| LEparagliding "finesse" | 2.70 |
| CDR Table 9 | 2.50 |
| **This model, measured profile** | **2.37** (2.05-2.69 over CD0 0.15-0.22) |

Three independent routes converge on **GR ~2.4-2.7**. v1's figure of 3.54 was optimistic
because it assumed 0.40 camber-driven lift before the profile was measured.

**Best estimate at trim (0.860 kg suspended, 600 m, design 0.6 mm lines):**
- CL 0.597, CD 0.252, **GR 2.37**
- **descent rate 2.20 m/s** — comfortably mid-band in the 1-3 m/s rule
- **forward airspeed 5.21 m/s (19 km/h)**
- descent time 273 s, still-air range ~1300 m

The corrected profile *improves* the design: airspeed is **+16 %** vs v1 (5.21 vs 4.48 m/s),
and descent rate moves off the 1 m/s floor to mid-band.

## Precision landing verdict (2500-run Monte Carlo, `run07_final_dispersion.m`)
| Metric | Value |
|---|---|
| **CEP50** | **29.5 m** |
| CEP90 | 132 m |
| CEP95 | 280 m |
| within 25 m | 41 % |
| within 50 m | 67 % |
| **within 100 m** | **88 %** |
| **R-08 (1-3 m/s) satisfied** | **99.3 % of runs** |
| P(wind aloft > airspeed, unrecoverable) | **12.3 %** (was 19.8 % in v1) |

Uncertainty swept: wind (right-skewed, mean ~2 m/s at 10 m, power-law shear), CL0 0.10-0.20,
CD0 0.15-0.23, mass +/-40 g, trim +/-1 deg.

**Median accuracy ~30 m is a good result for a student system.** The residual risk is the
**12.3 % of cases where wind at altitude exceeds the 5.2 m/s airspeed** — in those the CanSat
cannot make headway upwind under any guidance law and is carried downwind. That is inherent to
a lightly loaded canopy (W/S 11.9 N/m2) and is the price of the larger area; it is now
quantified rather than hidden. October/November are the calmest months at the nearest station
(Gorakhpur, mean surface wind ~1.3-2.0 m/s), which is why the median is good.

In ~0.7 % of runs descent rate reached 3.4 m/s (low-CL0 + high-CD0 + high-mass corner),
marginally breaching the 3 m/s ceiling. Keep suspended mass at or below ~0.9 kg.

## Guidance: a second-order effect, but fix the energy problem
GR 2.37 from 600 m gives ~1300 m of range against an offset of only 47-187 m — a **7-28x
surplus**. Altitude must be deliberately burned off (loiter over an initial point, then an
into-wind final leg). Naive homing scores well in calm air only because the ~18 m turn radius
makes it spiral tightly onto the target; it has no margin strategy. Payload pendulum period on
1.10 m lines is **2.10 s**, so the steering loop must stay well below ~0.5 Hz or it will excite
the swing.

## Not yet verified
- **CFD in progress.** Geometry is closed watertight (264 boundary edges -> 0, 0 non-manifold),
  the farfield domain is built (7 named zones, 300 m3), and the headless Fluent path is proven
  (Python workflow API, `LengthUnit [m]`, 15624 facets). The mesh is building now. CFD will
  settle CD0, which is still the dominant assumption.
- **Structural: NOT VERIFIED.** LE never solved the line loads (its section 17 says *"Don't try
  to solve equilibrium equacions"*). Mechanical Enterprise is licensed and ready.
- **Fabric permeability unspecified** — governs inflation, so inflation stays NOT VERIFIED.
- **Servo part still ambiguous three ways**: B26CLM V2 / A20CLM / CDR BOM's A20BHM.
- **Compliance question for organisers:** rule 5.4 iv requires the primary parachute to remain
  "connected to the outer body ... no ejection mechanism attached to it", but the CDR releases
  it at 600 m. Ask before FRR.

## CDR corrections required before FRR
Beyond the canopy swap (0.40 -> 0.71 m2, 7 -> 11 cells, rectangular -> tapered), the CDR's
airfoil table is wrong: it claims **t/c 14 %** and **camber 9 %**; the actual design is
**19.1 %** and **~1.1 %**. Its thickness *position* (32 % vs measured 30.8 %) was right.
Also fix: primary descent rate stated three ways (14.4 / 15 / 18-22 m/s) and flywheel KV
stated as both 2300 and 1590.

---

# Structural assessment (closes D-03 analytically)
`02_octave/run08_structural.m`. LE never solved the line loads, so these are derived
independently. Line ratings from LE's own types table.

## Lines and fabric are not the risk — they are 2-3 orders of magnitude overstrength
Steady flight at 0.860 kg (W = 8.43 N), load shared over the mirrored LE line plan:

| Element | count | load each | rating | margin |
|---|---|---|---|---|
| riser | 6 | 1.41 N | 1000 N | **711x** |
| main | 12 | 0.70 N | 400 N | 569x |
| upper | 18 | 0.47 N | 200 N | 427x |

Even at peak opening load (187 N) the margins are 19-32x. The bought 1.8 mm UHMWPE cord
(~300 daN) gives **96x** on a riser — it is ~15x stronger than the 1.2 mm riser the design
already specifies. Fabric hoop tension at 22 m/s deployment is 19.7 N/m (0.99 N per 50 mm
strip) against ~200 N per 50 mm for ripstop 40D: **margin >200x**.

## THE REAL STRUCTURAL CONCERN: opening shock exceeds the CDR's own design point
The CDR states the load path is *"sized to handle the recovery system's peak opening shock
(up to 11.1 G)"*. Computed opening load on the 0.67 m2 projected canopy:

| Deploy speed | Cx 1.2 | Cx 1.5 | Cx 1.8 |
|---|---|---|---|
| 14.4 m/s (CDR Table 7) | **11.3 g** | 14.2 g | 17.0 g |
| 18 m/s (CDR Table 6 design) | 17.7 g | **22.1 g** | 26.6 g |
| 22 m/s (R-07 upper bound) | 26.5 g | 33.1 g | **39.7 g** |

**Every combination exceeds 11.1 G** — by 1.0x at the most favourable corner and up to 3.6x at
the worst. The canopy will survive; the question is the **CanSat body, the riser attachment and
the PCB stack**. Note R-07 permits the primary descent rate up to 25 m/s, which would be worse
still.

**Actions:** (a) decide the real deployment speed and stop quoting three different values;
(b) consider a slider/reefing stage to cap the inflation Cx — the cheapest way to cut peak load;
(c) re-verify the fiberglass-rod load path and riser anchor at 20-30 G, not 11.1 G. Mechanical
Enterprise is licensed, so this is a runnable FEA case rather than a hand-waved margin.

**Status: CONDITIONAL / NOT VERIFIED.** Loads are now quantified, but the CanSat structure has
not been re-analysed at the higher g-levels and fabric permeability is still unknown.
