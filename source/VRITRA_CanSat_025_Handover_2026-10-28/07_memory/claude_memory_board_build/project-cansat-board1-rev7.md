---
name: project-cansat-board1-rev7
description: VRITRA CanSat Board 1 PCB — rev7 restructure decisions and the real root cause of the DRC failures
metadata:
  node_type: memory
  type: project
  originSessionId: bffd6522-8706-4cd6-b12f-4b4b69b0ff7d
  modified: 2026-10-02T20:14:11.158Z
---

Team VRITRA CAN-7USAT 2026 (Team ID 2026-INSPACe-CAN-7USAT-025), Board 1
primary flight computer. Ø110 mm 4-layer circular PCB, KiCad 10.
Project lives at `C:\Users\kalbh\Documents\KiCad\cansat final pcb`.
Rev7 design docs written 2026-10-02 to
`D:\linux\Software\Claude\VRITRA_Board1_rev7\`.

**Root cause of the months of unresolvable DRC errors: not capacity.**
All 12 footprints in `VRITRA_footprints.pretty` and all 47 footprint
instances in `Board1.kicad_pcb` had **zero** `F.Courtyard` geometry.
KiCad then falls back to the bounding box of existing geometry, which
for most of these was the pad row only (1.7 mm tall) instead of the
15–38 mm module body. DRC was simultaneously inventing collisions and
hiding real ones. The earlier conclusion that "Board 1 cannot fit its
parts" was drawn from that bad data.

**Why:** a footprint with no courtyard gives DRC nothing real to check,
so every manual placement fix was chasing a measurement artefact.

**How to apply:** before trusting any KiCad DRC result on this project,
confirm every footprint has an F.Courtyard. Patched footprints with
courtyards, fab outlines and pin-1 marks are in
`VRITRA_Board1_rev7/VRITRA_footprints_fixed.pretty/`.

Rev7.1 architecture decisions, confirmed by user 2026-10-02:
- **SN74LV4052A UART mux deleted entirely**, replaced by a broadcast
  GNSS net (`GPS_BCAST`): L89 TX feeds Boards 1, 3 and 5 permanently
  since UART RX pins are inputs; only Board 1 drives L89 RX, via 1k.
  Each receiving board buffers with an SN74LVC1G125 (Ioff) on its own
  rail.
- **DS3231 RTC: user overrode the delete-it recommendation.** Kept, but
  moved off Board 1 onto its own small carrier, wired back to Primary
  Teensy's I2C (Wire, 0x68) through a new 4-pin connector J7
  (SDA/SCL/+3V3/GND). Not required by any of the 16 mandatory fields
  (field 2 = millis(), field 8 GNSS time covers wall-clock) but useful
  for SD log filename timestamps before GNSS fix — told user this
  plainly, they want it anyway for competition-requirement peace of
  mind.
- **W25Q64 flash: user overrode the move-it-off-board idea.** Kept
  soldered on Board 1 next to the Teensy — moving it to a connector
  would add a cable/connector/buffer between the Teensy and the exact
  data the user called critical ("must log in all cases even if Teensy
  hangs"). Correct system-level answer to that requirement is Board 3's
  independent microSD + watchdog failover, not flash placement.
- **New J8 expansion header** (1x10, 2.54mm, not JST) breaks out 8 spare
  Teensy GPIO (6,8,9,27,29,30,31,32) for future sensors without a board
  respin.
- 7 modules stay soldered on Board 1: Teensy 4.1, Core1121/LR1121, L89,
  BMP585, LSM6DSO32, MAX98357A, W25Q64.
- Rods **cardinal** on a 94 mm bolt circle, not diagonal — cardinal
  gives the Teensy 14.7 mm of placement freedom vs 0.9 mm diagonal.
- Recommended stackup F.Cu / **In1=GND** / In2=+3V3 / B.Cu, which is a
  deliberate swap of what the user asked for (they said 3V3 on layer 2);
  reason is the ~0.2 mm L1–L2 gap vs ~1.065 mm L2–L3.

Verified-clean placement coordinates (rev7.1, re-solved after U8/J7/J8
added) are in `02_NETS.md` section 7 and `layout_v2.json`; 53% fill,
worst clearance 1.00mm on the rod envelope, re-checked with
`verify_layout.py`.

**Unconfirmed and blocking fabrication:** the 94 mm bolt circle has
never been checked against the Fusion 360 model, and the rod hardware
envelope (Ø13 spacers vs Ø17 M8 nuts) decides whether the layout holds —
three parts sit at exactly 1.00 mm rod clearance.

Also: old Board 1 used **JST EH** footprints while the build guide's
harness table specifies **JST XH**. Both 2.5 mm pitch, housings do not
mate. Needs picking once for all five boards.

Related: [[project-solid-fuel-rocketry-2026]]

**Board setup applied 2026-10-02 (live project, KiCad closed):** 4-layer stackup (F.Cu / In1=GND / In2=3V3 / B.Cu, 1.6mm, red mask, white silk, ENIG), rules and Power net class written; `missing_courtyard` was *ignored* and `min_clearance` was 0.0 before — both fixed. Silk text raised to 1.0mm/0.15mm stroke in the VRITRA footprint library. Schematic ERC 0, schematic parity 0. Backup of the pre-setup files: `Board 1/_backup_before_setup_20261002_143158`. Spec in `VRITRA_Board1_rev7/06_BOARD_SETUP.md`. Next: outline, 5 holes, place parts at rev7.1 coordinates, zones.

**Placement lessons (2026-10-02):** the earlier "rev7.1" coordinate table was WRONG for the real project — KiCad's Position sets the footprint *origin* (pad 1 area), not the courtyard centre; the solver also omitted the 9 resistors, 3 caps and TPL5010 and used wrong connector sizes (real parts are JST **EH**, J8 = EH 10-pin, 28.6mm long). Re-solved from real courtyards (grid packer + ruin-and-recreate search; random annealing failed at 64% fill) and verified with kicad-cli DRC = 0 violations. Authoritative guide: `VRITRA_Board1_rev7/07_PLACEMENT_GUIDE.md` (28 origin coords). Real refs: TPL5010=U8, flash=U7, amp=U9. **C2 (100uF amp bulk cap) was found MISSING from schematic+PCB** — user must re-add; C3 should be a ceramic disc footprint. Holes are board-only footprints (`RodHole_8.5mm_NPTH`, `AntennaHole_12mm_NPTH`), antenna hole is O12 not O13. Useful tooling facts: KiCad's bundled python has pcbnew but the stackup API is not exposed (insert stackup text); FootprintLoad works once per process (use PCB_IO_MGR.FindPlugin); ZONE keepout method is SetDoNotAllowZoneFills. Backups of live files are in `Board 1/_backup_*`.

**APPLIED to live project 2026-10-02 ~15:10 (KiCad was closed):** outline O110 (Edge.Cuts circle, centre 100,100), 4x RodHole_8.5mm_NPTH on 94mm bolt circle + **AntennaHole_13mm_NPTH** (user said 13mm in the end; earlier said 12), board-only footprints, 5 keep-out rule areas (rods 13x13, antenna 16x16: no tracks/vias/pour; pads+footprints allowed), 28 parts at verified origins, GND (In1) + 3V3 (In2) planes filled, 4-layer red/ENIG stackup. Verified on LIVE files with kicad-cli: ERC 0, DRC 0, parity 0, 60 unrouted signal items (expected), gerber+drill export OK (NPTH 4x8.5 + 1x13). Fixed: **C2 was wired reversed (pin1 on GND; CP_Radial pad 1 = + lead)** -> rotated symbol 180 so pin1=+5V; C3 footprint -> C_Disc_D5.0mm_W2.5mm_P5.00mm. Remaining: U8 (TPL5010) SMD pads GND/+3V3 need a via to planes; route signals. Backups: `Board 1/_backup_before_apply_20261002_150351` (+ earlier ones). Still unconfirmed: 94mm rod circle and spacer-vs-nut envelope.

**C4 added + routing-ready 2026-10-02 ~15:45:** C4 100nF (Capacitor_SMD:C_0805_2012Metric) added to schematic (at 203.2,127.0, labels +3V3/GND) and PCB at (86.97,143.80) rot 0, 3.5mm from U8 — decoupling for the bare TPL5010, which is the only non-module IC so nothing else supplies its bypass cap. Live files verified: ERC 0, DRC 0 (all severities), parity 0, 62 unrouted, gerber+drill OK. Backup `_backup_before_C4_20261002_153908`. **Lesson: do NOT auto-route by straight line pad-to-via — it shorted across U8 pad 3 (SOT-23-6 pads are close); all tracks were stripped, board has zero tracks/vias, user routes by hand.** Routing guide = `08_ROUTING.md` (04_ROUTING.md superseded). Only 4 pads need manual power routing (U8 and C4 SMD pads to plane vias); all THT GND/+3V3 pads already connect via the filled planes.

**Placement optimised for decoupling + RF isolation 2026-10-02 ~17:00.** Before: C1->U2 51.1mm, C2->U9 42.6mm, C3->U7 20.9mm, U3 GNSS only 10.8mm from U7 flash. After: C1 14.2, C2 7.1, C3 4.3, C4 2.4mm; U3 nearest noisy 35.8mm, U2 22.0mm, U2<->U3 39.9mm, both RF at rim. **C2 footprint changed Ø10mm -> `Capacitor_THT:CP_Radial_D6.3mm_P2.50mm`** (schematic+PCB) — a Ø10.6mm part physically could not reach U9; board is at **77% effective fill** (courtyards + 0.6mm gaps vs disc minus keepouts), so only 4 legal positions existed for it. **User must buy 100uF in a Ø6.3mm radial can (e.g. 100uF/16V 6.3x11mm).** Live verified ERC 0 / DRC 0 / parity 0, 62 unrouted. Backup `_backup_before_optplace_20261002_165853`.

**Solver lessons (cost me 5 failed searches):** my hard "keep Teensy USB/SD corridor clear" rule is NOT a DRC rule and made the real, DRC-clean board test as illegal — corridors must be a weighted preference, not a hard gate. Also: snapping IC+cap relative offsets to the grid with round() pushed them into overlap so OFF[] came out empty — snap outward with ceil. Robust approach that worked: start from the existing legal layout and do feasibility-gated annealing with translate/rotate/**swap** moves (`improve.py`), not construct-from-scratch (`pack*.py`).

**ROUTING COMPLETE, merged to live 2026-10-02 ~20:45.** A prior ChatGPT/Codex session routed on a COPY at `C:\Users\kalbh\Downloads\CHAT\pcb-routing` (freerouting 2.1.0 + KiCad python; its scripts prepare.py/ground.py/repair.py/merge_audio.py live there) and never merged back; it stopped after producing `repair.ses` without importing it. I imported that SES (fixed GPIO29), then routed the 5 nets freerouting could not finish (GPS_BCAST had zero tracks; gaps were 10-35mm) with my own grid maze router `scratchpad/maze.py` — 0.1mm grid, clearance baked into the obstacle mask so paths are DRC-legal by construction, Dijkstra with via cost, F.Cu/B.Cu only.
Final live state: 535 tracks, 69 vias, F.Cu 315 / B.Cu 220, **nothing on the GND/3V3 plane layers**, widths 0.25 signal / 0.6 power, 3342mm copper, closest copper-to-edge 0.64mm. **ERC 0, DRC 0, schematic parity 0, KiCad connectivity 0 unconnected, all 38 multi-pad nets verified reachable pad-to-pad via KiCad's own engine.** Netclass clearance raised to **0.33mm** (Robu capability). Fab package in `Board 1/fab_rev7_routed` (gerbers, PTH+NPTH drill w/ maps, placement csv, BOM, DRC+ERC reports). Backup `_backup_before_routing_20261002_204259`.
**Decisions:** outer F.Cu/B.Cu GND pours deliberately OMITTED — they caused 7 `starved_thermal` warnings and the shield-fill+stitching alternative segfaulted pcbnew; the solid inner GND plane 0.2mm under F.Cu already provides return path/shielding.
**Tooling gotchas:** `b.FindNet(x).GetNetCode()` sometimes returns SwigPyObject (get netcode from a pad instead); iterating `b.Zones()` twice in one process breaks; `GetConnectedItems(item)` takes no flags arg; pad obstacle rasterisation MUST use `pad.GetBoundingBox()` (rotation-aware) not GetSizeX/Y — using sizes caused a real clearance violation against a rotated JST pad.
**Still unverified before fabrication:** physical module header geometry (U2 Core1121 2x8 @2.54mm pitch / 15.24mm row spacing; U3 L89 2x6 with **2.54mm** row separation — the earlier "12.7mm" was wrong; U5 9-pin + 5-pin rows 12.7mm apart) and the 94mm rod bolt circle / spacer-vs-nut envelope.

**FINAL STATE 2026-10-03 (fab-ready):** a Codex session did "rev9" while I was away (C1/C2 -> Device:C_Polarized, D3 = Vishay BAT54W-E3-08 SOD-123 in series J9->U3 BAT, 351 signals widened to 0.30, +5V to 0.80, silk >=0.16). User's 3S->3.3V buck feeds J9. Then I converted H1-H5 (8.5/13mm) from drilled NPTH pads to **Edge.Cuts circles** because **Robu max drill = 6.30mm** (also edited the library .kicad_mod so updates can't revert it). Strict DRC (all 62 rules as error) + parity + ERC = 0. Package: `Board 1/manufacturing/` + `Board1_FAB_NOTES.md` (single doc). All `_backup_*`, `fab_rev7/8/9`, REV9_AUDIT.md were sent to the **Recycle Bin** at user request — none of those backup paths exist any more. Robu 4-layer limits (read from their site via browser; WebFetch gets 403): track/space 0.09, via ring 0.13, PTH ring 0.25, hole-hole 0.5, PTH-track 0.33, mask dam 0.20 green / 0.254 other colours. U8 SOT-23-6 web is 0.250 -> recommended green (user had wanted red "for heat"; colour has no thermal effect).
**Lessons:** never blanket-snap track endpoints to via centres — my snap script moved 36 endpoints and created 11 clearance/crossing errors + zero-length tracks; recovered the user's last save from KiCad 10's git-based `.history/` folder (`git log` there shows "PCB Save" commits). Always check `.history` and file mtimes before assuming the live file is my last version — the user and other AI sessions edit between my turns.
