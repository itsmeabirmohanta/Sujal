---
name: cansat-fusion-model-messy
description: "The VRITRA Fusion assembly has duplicate overlapping plates, stray rods, and all-default Steel materials — never import it into a simulator"
metadata: 
  node_type: memory
  type: project
  originSessionId: 85dae6ee-1e5c-4280-b6aa-0d5247c9f08a
  modified: 2026-08-09T01:04:16.149Z
---

Fusion document **"Cansat Final almost>>"** (Default Project) is a working model,
not a clean one. As of 2026-08-04:

- **269 components, 750+ bodies.** Shell alone has 3280 faces (hex lattice).
- **Duplicate overlapping plates**: "hollow plate for canset" copies at Z 27.6
  and 103.1 overlap "hollow plate for cansat" at Z 24 and 96. Also a stray copy
  at Z −13.1, below the base.
- **The 4 rods are not assembled** — they sit in a row off to the side at
  X = 180/220/260/300 mm. That is why the root bounding box is 762 mm wide.
- **Materials are almost all defaults**: 537 bodies "Steel", 109 "Stahl". The
  reported 2.949 kg total mass is therefore meaningless.
- Real envelope of the assembled stack: **Ø130 mm × 152 mm tall**; plates are
  Ø110 × 10 mm and only 12–15% fill (open frames, not discs); shell is 4% fill.

**Why:** feeding this into HFSS/Icepak/any mesher either fails outright or
produces a mesh that never solves. Every "Ansys gave me errors" report from a
model like this traces back to importing it.

**Re-measured 2026-08-09 through the Fusion MCP** (33 top-level occurrences).
Still messy, and this is now the blocker on a real-world-valid HFSS result. The
exact offenders, in world coordinates:

| Occurrence | World position | Problem |
|---|---|---|
| `newperachutebay:1 / Body284` (bay outer wall, Ø128 × 67, 2 mm) | X −224…−96 | **160 mm off axis** |
| `newperachutebay:1 / Body124` | X −222…−98 | 160 mm off axis |
| `hollow plate for canset:1/2/3` | X −78…31, X 135…245, X −110…0 | three loose duplicate plates |

Assembled and correct: `Base optimised` (Ø130, Z 0…10), `hollow plate for
cansat:1/2` (Ø110, Z 24…34 and 96…106), `Rod 1..4` (Ø8, Z 2…152 — so the rods
ARE assembled now, unlike 2026-08-04), `New Shell:1` (Ø130 × 140, Z 10…150),
`newperachutebay:1 / Body1` (drum Ø124, Z 140…218).

A re-export today would pull the off-axis bay wall and the three loose plates
into HFSS as stray metal sitting 160–250 mm from the vehicle, inside the air
box, wrecking the radiation pattern. **Fix the assembly before re-exporting.**

**How to apply:** build simulation models from primitives instead — the scripts
in `ansys/` do exactly that, using dimensions extracted from this model rather
than its geometry. Only trust dimensions read out of it, never its materials or
mass properties.

Related: [[cansat-plate-resonance]]
