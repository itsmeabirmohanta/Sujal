"""One download zip per board, build files only.

  Board 1, 2 (etched):  Gerbers (B.Cu, B.Mask, F.Silkscreen, Edge.Cuts) + Excellon drill
  Board 3, 4 (zero):    hole-by-hole build list + picture + KiCad board/project
Run:  python make_zips.py   -> ../downloads/BOARD<n>_<NAME>_*.zip
"""
from __future__ import annotations

import os
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "downloads")
GERBER_KEEP = ("-B_Cu.gbl", "-B_Mask.gbs", "-Edge_Cuts.gm1", "-F_Silkscreen.gto", ".drl")
ZERO_KEEP = (".kicad_pcb", ".kicad_pro", ".png", "_BUILD_LIST.md")
EXTRA = {"BOARD4_PARACHUTE_zero_board.zip": ("REWORK_BOARD4.md",)}     # ICM → LSM6DSO32 change list

JOBS = [
    ("BOARD1_MAIN_etched_GERBER_DRILL.zip", os.path.join(ROOT, "etch_pcb", "BOARD1_MAIN", "GERBER"), GERBER_KEEP),
    ("BOARD2_BACKUP_SENSOR_etched_GERBER_DRILL.zip", os.path.join(ROOT, "etch_pcb", "BOARD2_BACKUP_SENSOR", "GERBER"),
     GERBER_KEEP),
    ("BOARD3_POWER_zero_board.zip", os.path.join(ROOT, "kicad_single", "BOARD3_POWER_1L"), ZERO_KEEP),
    ("BOARD4_PARACHUTE_zero_board.zip", os.path.join(ROOT, "kicad_single", "BOARD4_PARACHUTE_1L"), ZERO_KEEP),
]


def main():
    os.makedirs(OUT, exist_ok=True)
    for name, src, keep in JOBS:
        files = sorted(f for f in os.listdir(src) if f.endswith(keep) or f in EXTRA.get(name, ()))
        if len(files) < len(keep) + len(EXTRA.get(name, ())):
            raise SystemExit(f"{name}: expected {len(keep)} files in {src}, found {files}")
        path = os.path.join(OUT, name)
        with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as zf:
            for f in files:
                zf.write(os.path.join(src, f), f)
        print(f"{path}  {os.path.getsize(path) // 1024} KB  {files}")


if __name__ == "__main__":
    main()
