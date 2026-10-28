# Should you stitch it? — YES. Go ahead.
Decision sheet, 2026-10-20. Canopy exactly as dimensioned in `parachute_LE.zip`, CanSat 1.000 kg.

## The answer

**Stitch it.** The canopy as dimensioned will survive the fall with a very large margin. The
minimum safety factor anywhere in the whole canopy — fabric, seams, lines, risers — is **6.2×**,
and that is at the absolute worst case the competition rules permit. Recovery systems are
normally designed to 1.5–2×. You have three to four times more margin than you need.

**And it meets the descent-rate rule.** At your 1.000 kg it descends at **2.41 m/s**, against a
required 1–3 m/s. That sits in the middle of the band, with room on both sides.

## Will it survive? — the numbers

Loads from the exact reconstructed line geometry, line ratings from LEparagliding's own table
(upper 0.4 mm = 200 N, main 0.6 mm = 400 N, riser 1.2 mm = 1000 N).

| Load case | total load | g on 1 kg | worst line tension | **safety factor** | verdict |
|---|---|---|---|---|---|
| Steady glide | 10 N | 1.0 | 1.13 N | **177×** | survives |
| Opening at 14.4 m/s | 120 N | 12.2 | 13.8 N | **14.5×** | survives |
| Opening at 18 m/s | 187 N | 19.1 | 21.5 N | **9.3×** | survives |
| Opening at 22 m/s *(worst the rules allow)* | 279 N | 28.4 | 32.1 N | **6.2×** | survives |

The weakest element in every case is the **0.4 mm upper cascade line**, and even that is 6.2×
over-strength at the worst opening shock.

### Fabric and seams are nowhere near their limits
| Deploy speed | ram pressure | fabric load | fabric SF | seam SF |
|---|---|---|---|---|
| 6.7 m/s (steady) | 26 Pa | 0.09 N / 50 mm | 2188× | 1313× |
| 18 m/s | 186 Pa | 0.66 N / 50 mm | 303× | 182× |
| 22 m/s | 278 Pa | 0.99 N / 50 mm | 203× | 122× |

Ripstop 40D is taken conservatively at 200 N per 50 mm strip, and seams at 60 % of that. **The
fabric will not tear and the seams will not pull.** They are loaded to under half a percent of
capacity. This canopy is, if anything, far stronger than it needs to be for a 1 kg payload.

## Are the zip's dimensions correct? — Yes, and here is the proof

This was checked two independent ways, and both pass.

**1. LEparagliding's own verification** (`lep-out.txt` section 6) compares every panel edge
against the rib it attaches to and against the neighbouring panel. Worst disagreement across the
whole canopy: **0.009 mm**.

**2. My own measurement** straight out of the 2D cutting file, not relying on LE's claim.
Consecutive panels must share an edge length or they cannot be sewn together:

| Shared edge | panel giving it | panel receiving it | difference |
|---|---|---|---|
| 587 mm | extrados-panel-2 | extrados-panel-3 | **0.08 mm** |
| 569 mm | extrados-panel-3 | extrados-panel-4 | **0.06 mm** |
| 539.57 mm | extrados-panel-4 | extrados-panel-5 | **0.00 mm** (identical) |
| 498 mm | extrados-panel-5 | extrados-panel-6 | **0.05 mm** |

The intrados panels match the same way (within 0.04 mm). My measured edge lengths also reproduce
LE's section-6 numbers (592.90 / 586.97 / 569.18 / 539.53 / 498.03 / 440.54 mm) to within
0.04 mm — from a completely separate file.

**Conclusion: the pattern is geometrically self-consistent to under a tenth of a millimetre.**
Your tailor can make the exact parachute from it. There is nothing wrong with the dimensions.

Earlier I printed a check showing 16–36 mm mismatches. **That was my error, not the pattern's** —
I was comparing panel edges against the wrong path around the rib outline. Corrected above.

## What stitching will NOT settle
Three things are still open, and none of them is a strength or dimension problem:

1. **Will it inflate properly?** Unknown — it depends on the fabric's air permeability, which
   nobody has specified or measured. This is a *function* question, not a survival one: the
   canopy will not break, but it might open slowly or unevenly. **Only a drop test answers this.**
2. **Will the CanSat survive?** Opening shock is **12–28 g**, and your CDR says the structure was
   sized for 11.1 G. The parachute is fine; the **CanSat body and the riser anchor point** are the
   concern. This needs checking separately.
3. **Descent rate depends on rigging accuracy.** The canopy is designed to hang at 8.61° angle of
   attack. Below about 7° the descent rate exceeds the 3 m/s limit. Cut the line lengths carefully
   and err slightly nose-up.

## So, practically
- **Cut and stitch the canopy.** The dimensions are right and it will survive.
- Use `PF072_cutting_layered.dxf` and `TAILOR_INSTRUCTIONS.md`, not the raw zip — the raw DXF has
  cut/seam/mark lines on one layer and already includes 8 mm seam allowance.
- Don't forget the **4 vent holes per rib** and cut **everything twice, mirrored** (12 ribs,
  20 panels, 4 tabs).
- **Then drop test it** from 100–400 m before you trust it on the rocket. That is the one thing
  simulation cannot replace here.
