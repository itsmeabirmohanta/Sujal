# useless/ — quarantined regenerable clutter

Moved here 2026-08-10 to declutter the project. **Nothing here is source.**
Every item regenerates from the real project, so this whole folder is safe to
delete — it's only kept in case you want something back.

| Item | What it is | How to get it back |
|---|---|---|
| `dist/` | The **built `VRITRA_GCS.exe`** (ground-station app). A product, but regenerable. | Move back here, or rebuild: run `build_exe.bat` (uses `vritra_gcs.py` + `VRITRA_GCS.spec`). |
| `build/` | PyInstaller **intermediate** build files. Pure junk. | Regenerates automatically on the next `build_exe.bat`. |
| `__pycache__/`, `.pytest_cache/`, `pycache_*` | Python bytecode / test caches. | Regenerate automatically when you run the code/tests. |
| `realshell_solve.log` | Batch-solve log from the perforated-shell HFSS run. | Historical only; the result is in `HFSS_SIMULATION.md` §4. |

**If you need the GCS executable for a demo, pull `dist/` back out** (or rebuild
it). Otherwise, deleting this entire `useless/` folder frees ~109 MB and loses
nothing you can't regenerate.
