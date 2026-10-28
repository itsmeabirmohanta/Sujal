---
name: cansat-hfss-project-collision
description: AEDT auto-names projects Project<N>; never use that name or it destroys the model
metadata: 
  node_type: memory
  type: feedback
  originSessionId: c0f739fb-b77f-4e8b-bbda-2ee8de993701
  modified: 2026-08-10T00:19:33.808Z
---

AEDT names every new empty project `Project1`, `Project2`, … A saved project
ALSO called `Project1.aedt` collides: `pyaedt.Hfss(project="Project1")` binds to
the empty in-memory default, and the next save writes 4 KB over the real file
while AEDT clears the matching `.aedtresults` folder. This destroyed a 1577 MB
converged solution TWICE (2026-08-09, once by me).

**Why:** the collision is silent — PyAEDT still reports the real path, and
Validate passes on the empty project.

**How to apply:**
- Projects are now named `VritraCansatRf` / `VritraRealShell` — names AEDT can
  never auto-generate. Never rename them back to `Project<N>`.
- Always open by FULL PATH, never bare name. `ansys/AedtSession.py` enforces this
  and refuses any project under 100 KB or with 0 solids.
- **Back up `.aedtresults` (the field solution, hundreds of MB), not just the
  `.aedt`** before killing/relaunching AEDT — I lost the solution once because I
  backed up only the 2 MB project. Now mirrored to `D:\CansatRfBackup`.
- Solve via headless batch (`ansysedt -ng -BatchSolve -Monitor`), not the GUI/COM
  path — the interactive path wedges on heavy geometry (engine spawns, zero CPU,
  no error). See [[cansat-hfss-867-solved]].
