# PF072-CANSAT canopy — cutting and stitching instructions
For the tailor. Generated 2026-10-20 from the LEparagliding design files.

## Short answer to "can I just give him the zip file?"

**No — not as it is.** The pattern inside the zip is good and full-scale, but three things in it
will cause the canopy to come out wrong:

1. **Everything is on one drawing layer.** The DXF in the zip (`leparagliding.dxf`) puts the
   cut lines, the stitch lines and the alignment marks all on layer "0". Your tailor cannot tell
   which line to cut and which line to sew.
2. **Seam allowance is already included — 8 mm.** If he adds his own allowance on top, every
   panel comes out 8 mm oversize and the canopy will not assemble.
3. **Only half the wing is in the file.** There are 6 ribs and 10 panels; the canopy needs
   **12 ribs and 20 panels**. Everything must be cut twice, mirrored.

Use **`PF072_cutting_layered.dxf`** in this folder instead. It fixes all three: separate CUT /
SEAM / MARK layers, all 36 parts already mirrored and labelled, and millimetres declared in the
file header.

---

## 1. Print / plot check — do this first
- The file is **1 : 1 in millimetres**. Plot at **100 %**, no "fit to page".
- **Verify before cutting anything:** measure part `rib-1-R`. The red CUT outline must be
  **559.8 mm** long; the green SEAM line inside it must be **550.0 mm**. If those two numbers
  are wrong, the scale is wrong — stop and re-plot.

## 2. Layers — what each colour means
| Layer | Colour | Meaning |
|---|---|---|
| **CUT** | red | **Cut on this line.** 8 mm seam allowance is already in it. |
| **SEAM** | green | The stitch line. Sew here. Do **not** cut. |
| **MARK** | blue | Alignment marks — match these between rib and panel. Also the line-attachment tab positions. |
| TEXT | white | Part name |

## 3. Parts list — 36 pieces total
| Part | Qty | Size (w × h mm) | Notes |
|---|---|---|---|
| rib-1 … rib-6 | 2 each = **12** | 577×129 down to 440×102 | internal ribs; see §4 for vent holes |
| extrados-panel-2 … 6 | 2 each = **10** | 178×616 down to 276×506 | **upper** surface |
| intrados-panel-2 … 5, 6-3 | 2 each = **10** | 163×557 down to 196×472 | **lower** surface |
| intrados-panel-6-1, 6-2 | 2 each = **4** | 60×60 | small reinforcement patches, labelled `3A2` / `3B2` — these are **line-attachment tabs** |

There is no "panel-1" and that is correct — the centre cell has zero width by design
(LEparagliding calls it a "zero-thickness central cell"). Numbering starts at 2.

## 4. Ribs have vent holes that MUST be cut out
Each rib contains **4 closed interior openings** (roughly 33×81, 33×80, 33×64 and 21×42 mm).
They appear as closed red loops inside the rib outline. These are cross-vents that let the cells
equalise pressure. **If they are not cut, the canopy will not inflate properly.**

## 5. Fabric
- **Ripstop nylon 40D** (specified in the team's CDR).
- Finished canopy surface area is **1.57 m²**. Allowing for seams and nesting waste, buy
  **about 3.5 m** of 1.5 m-wide cloth. Buy extra — a spare set of ribs is cheap insurance.
- Expected finished fabric weight ≈ **63 g** bare, ≈ 78 g with seams and reinforcement.

### Grain direction — ask before cutting
The design file lists the fabric grain at 90° but tags it
`grainSource: "assumed-from-plan-layout"` — i.e. **the software guessed it; the designer never
specified it.** For a parachute this matters, because ripstop stretches far more on the bias than
along the weave. Decide deliberately and keep it consistent across all panels. Conventional
practice is to run the **warp (least-stretch) direction spanwise** on the panels and chordwise on
the ribs.

## 6. Stitching notes
- Sew on the **green** line, 8 mm in from the cut edge.
- Allowance is **not uniform**: it is 8 mm on edges that get sewn and **0 mm on free edges**
  (the cell mouths at the leading edge, and the trailing edge). On the layered DXF you can see
  this directly — where the red and green lines touch, that edge is finished, not sewn.
- Match the **blue** alignment marks between each rib and its neighbouring panels. There are
  66–70 marks per rib; they are what keeps the aerofoil shape true.
- Reinforce every line-attachment tab (the `3A2` / `3B2` patches and the marked points on the
  lower surface). These carry the whole load.

## 7. Lines — not the tailor's job, but get the lengths right
There are **36 load-bearing lines plus 12 brake lines**, needing **36.6 m of cord (≈43 m with
loops)**. Design diameters are 1.2 mm risers, 0.6 mm mains, 0.4 mm uppers in UHMWPE.

**Line length accuracy is critical and is the single easiest way to ruin a good canopy.** The
canopy is designed to hang at 8.61° angle of attack. Analysis shows that below about **7°** the
descent rate exceeds the competition's 3 m/s limit — so there is only ~1.6° of margin. Lines
rigged too short at the front (or too long at the back) will flatten the wing and break the rule.
Measure under light tension, and err slightly nose-up rather than nose-down.

## 8. Will the finished canopy work?
The pattern will produce the canopy that was analysed, and that canopy is sound: predicted
descent **2.2 m/s** (inside the required 1–3 m/s), glide ratio **2.5–2.7**, forward speed
**5.8 m/s**. Line strength margin is at least **9×** even at opening shock.

Two honest caveats that stitching cannot fix:
- **Inflation has not been verified.** It depends on the fabric's air permeability, which nobody
  has specified or measured. A drop test is the only way to settle it.
- **Trim accuracy**, as in §7, is what decides whether the descent-rate rule is met.

Full engineering basis: `../07_report/FINAL_ENGINEERING_REPORT.md`.
