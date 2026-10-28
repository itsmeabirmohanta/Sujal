---
name: cansat-plate-resonance
description: "VRITRA CanSat plates are Ø110mm — exactly one wavelength circumference at the assigned 867 MHz, so making them aluminium creates 5 resonant parasitic loops"
metadata: 
  node_type: memory
  type: project
  originSessionId: 85dae6ee-1e5c-4280-b6aa-0d5247c9f08a
  modified: 2026-08-08T21:40:15.236Z
---

The VRITRA CanSat (team 025) structural plates measure **Ø110.00 mm outside
diameter**. Circumference = 345.58 mm. λ at the assigned 867.00 MHz = 345.78 mm.
Ratio **0.9994** — a 0.06% match to a full-wave resonant loop. There are five
plates stacked 18–28 mm apart (0.05–0.08λ, i.e. strongly coupled).

**Independently confirmed inside HFSS on 2026-08-09.** After importing the STEP
assembly, AEDT's own Measure tool reported the middle plate rim as
`Arc Diameter = 110 mm`, `Length = 345.5751919 mm`. λ at 867.00 MHz = 345.78 mm,
so the ratio 0.9994 is now a tool-measured number, not a hand calculation. The
Ø130 mm rims measure 408.407 mm = 1.181 λ at 867 MHz and 3.36 λ at 2465 MHz.

Measured from the Fusion model "Cansat Final almost>>" on 2026-08-04. In that
model the plates are still EPX 86FR resin (a dielectric, harmless). The team's
stated plan is to fabricate them in **aluminium**, which would turn all five
into resonant parasitic elements surrounding the LoRa antenna.

Two other near-resonant conductors in the same vehicle: PCB ground planes
90×90 mm (perimeter 1.041λ) and the gyro wheel Ø104 mm (0.945λ).

**Why:** this is not visible from the firmware or the CAD tree — it only appears
when you compare the mechanical dimensions against the assigned RF channel.
It was the finding that justified the HFSS study.

**How to apply:** if the aluminium plate decision is revisited, the cheap fix is
a 5 mm radial slot in each rim (breaks the closed loop) or a 10 mm diameter
change. Both are single Fusion edits pre-fabrication and near-impossible after.
Confirm with the HFSS comparison run described in
`ANSYS_HFSS_GUIDE.md` §4.8 before committing.

Related: [[cansat-fusion-model-messy]]
