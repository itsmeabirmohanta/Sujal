---
name: cansat-step-files-not-simulatable
description: "The three exported STEP files are dimensional references only — no antenna in the assemblies, everything tagged default Steel, and antenna.step has no radiating element"
metadata: 
  node_type: memory
  type: project
  originSessionId: 9bd653b2-326b-4639-b793-c952b28b745a
  modified: 2026-08-09T01:03:54.996Z
---

Parsed 2026-08-08 (entity counts, unit contexts, body inventories, surface radii):

| File | Units | Bodies | Faces | Contents |
|---|---|---|---|---|
| `cansat new structure 2.step` | **cm** | 104 | 3792 | XIAO-ESP32S3-Sense with USB-C, microSD, camera, u.FL, DF40C connectors, shield can; base, 4 rods, hollow plate, PCB1 |
| `rf structure.step` | **cm** | 47 | 2439 | Reaction wheel, N20 motor + gearbox + shaft, holder, parachute bay, base, 4 rods, hollow plate, battery tray, PCB1 |
| `antenna.step` | **mm** | 3 | 16 | One antenna on a 125 × 10 × 2 mm bar |

Key facts:

- **Neither assembly contains an antenna.** Nothing to excite; not simulatable.
- **40 of 42 material tags are `Steel`/`Stahl` (ρ 7850)**; the other two are
  `EPX 86FR` (ρ 1600). Fusion defaults, not decisions. Imported unchanged, HFSS
  would treat the whole CanSat as solid metal.
- **Units are centimetres** in both assemblies (millimetres in `antenna.step`).
  Max cylindrical radius 6.5 cm confirms the **Ø130 mm** envelope independently
  of the PDR — that cross-check is the main value of these files.
- **`antenna.step` is a shell mock-up, not a radiator**: axis along Y, total
  96 mm, radii 6.0/5.0/4.5/3.5/2.108 mm, and a **31.5 mm gap between Body2
  (Y −16…−2) and Body1 (Y −47.5…−96) that is simply not modelled**. No internal
  element, no feed point.
- Its **96.0 mm extent matches the CDR antenna table's 98 mm**, so this is
  almost certainly the RadioMaster Bandit T. That envelope agreement is the one
  genuinely useful thing in the file — the whip model is sized against it.
- The 31.5 mm gap reads plausibly as a 2.4 GHz sleeve junction (λ/4 = 30.6 mm).
  This was withdrawn on 2026-08-08 when the antenna was believed sub-GHz, then
  **reinstated 2026-08-09 when the user reconfirmed the dual-band RadioMaster**.
  Suggestive, not proof; the file is still not something to model from.

**UPDATE 2026-08-09 — the "never import" advice is now qualified.** The user
chose to import for a full-fidelity model, and the import *works*: AEDT 2026 R1
reads the `SI_UNIT(.CENTI.,.METRE.)` header and converts correctly. Verified in
HFSS with the Measure tool: shell rim **Ø130 mm**, plate rim **Ø110 mm**
(circumference printed by HFSS as **345.5752 mm** = 0.9994 λ at 867 MHz — see
[[cansat-plate-resonance]]), plates spanning **Z −59 to +222 mm**. So no rescale
is needed on import. What still holds: strip the material names on import (all
default Steel), defeature the sub-millimetre features, and graft the antennas +
air box on afterwards, because the assemblies contain no radiator.
Use `rf structure.step` (48 solids), never `cansat new structure 2.step`
(105 solids, XIAO board down to 0.05 mm features).

**CRITICAL, found 2026-08-09 via the Fusion MCP: `rf structure.step` is STALE.**
Measured world-space against the live Fusion assembly, only the reaction drum
(Z 140..218) matches. The base plate differs in thickness and Z, the Ø110 plates
are 8 mm and 2 mm off, the tie rods are **150 mm in Fusion vs 210 mm in the
STEP**, and the outer shell is absent from the STEP entirely. Any HFSS result
from this import describes a previous revision of the airframe. Re-export before
quoting numbers — and tidy the assembly first (see [[cansat-fusion-model-messy]]).

**The real shell, measured in Fusion (`New Shell:1 / Body1`):** Ø130 OD, wall
**exactly 3.0 mm** (cylindrical faces at r = 62.0 and 65.0), 140 mm tall,
volume 81 454 mm³ against 167 573 mm³ for a solid tube — so it is a perforated
lattice, **48.6 % solid / 51.4 % open**, 3280 faces. Perforations ~10 mm = λ/35
at 867 MHz, far below the λ/2 = 173 mm an aperture needs, so volume mixing is
rigorously valid: **εr_eff = 1.83, tanδ_eff = 0.0057** — not 2.7/0.008. The
parachute bay wall is a separate plain tube, Ø128, **2.00 mm** wall (team stated
3 mm — discrepancy).

**Why:** same trap as [[cansat-fusion-model-messy]] — importing produces either
an outright failure or a mesh that never solves, and silently makes every
dielectric a conductor.

**How to apply:** read dimensions out of them, never geometry or materials.
Build the EM model from primitives — `ansys/CansatRfModel.py` does this. Model
the comm antenna as an equivalent sleeve monopole sized from first principles
(0.24λ), constrained to fit the 96 mm STEP envelope.

Related: [[cansat-fusion-model-messy]], [[cansat-antenna-and-link-margin]],
[[cansat-carbon-shell-blocks-rf]]
