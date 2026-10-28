"""Check + picture every single-layer board written by kicad_export1.py.

  python check_single.py            (system python with PyMuPDF; calls kicad-cli + KiCad python)

For each ../kicad_single/BOARD*_1L/*.kicad_pcb:
  * KiCad DRC (all severities) -> must be 0 violations / 0 unconnected
  * reload in pcbnew: every pad's net == boards.py netlist, 0 tracks on F.Cu
  * SVG of the solder side + footprints -> PNG next to the board
"""
from __future__ import annotations

import glob
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(os.path.dirname(HERE), "kicad_single")
KBIN = r"C:\Program Files\KiCad\10.0\bin"
CLI = os.path.join(KBIN, "kicad-cli.exe")
KPY = os.path.join(KBIN, "python.exe")
LAYERS = "B.Cu,Edge.Cuts,F.Fab,F.Silkscreen,User.Drawings"

RELOAD = r'''
import sys, json
sys.path.insert(0, sys.argv[2])
import pcbnew, boards
b = boards.BOARDS[sys.argv[3]][0]()
want = {(r, p): n.name for n in b.nets for r, p in n.pins}
pcb = pcbnew.LoadBoard(sys.argv[1])
bad, seen = [], set()
for fp in pcb.GetFootprints():
    ref = fp.GetReference()
    if ref not in b.parts:
        continue
    for pad in fp.Pads():
        key = (ref, pad.GetNumber())
        got = pad.GetNetname() or None
        if key in want or got:
            seen.add(key)
            if want.get(key) != got:
                bad.append(f"{key}: board={got} netlist={want.get(key)}")
missing = [str(k) for k in want if k not in seen]
fcu = sum(1 for t in pcb.GetTracks() if t.GetLayer() == pcbnew.F_Cu and t.GetClass() == "PCB_TRACK")
print(json.dumps({"mismatch": bad, "missing": missing, "fcu_tracks": fcu}))
'''


def drc(pcb: str) -> tuple[int, int]:
    rep = pcb[:-10] + "_drc.json"
    subprocess.run([CLI, "pcb", "drc", "--format", "json", "--severity-all", "-o", rep, pcb],
                   check=True, capture_output=True)
    with open(rep, encoding="utf-8") as f:
        d = json.load(f)
    return len(d.get("violations", [])), len(d.get("unconnected_items", []))


def reload_check(pcb: str, bk: str) -> dict:
    out = subprocess.run([KPY, "-c", RELOAD, pcb, HERE, bk], check=True, capture_output=True, text=True)
    return json.loads(out.stdout.strip().splitlines()[-1])


def picture(pcb: str) -> str:
    import fitz
    svg = pcb[:-10] + ".svg"
    subprocess.run([CLI, "pcb", "export", "svg", "--layers", LAYERS, "--mode-single", "--page-size-mode", "2",
                    "--exclude-drawing-sheet", "-o", svg, pcb], check=True, capture_output=True)
    png = pcb[:-10] + ".png"
    doc = fitz.open(svg)
    doc[0].get_pixmap(dpi=200).save(png)
    return png


def main() -> int:
    import boards
    rev = {f"BOARD{n}": k for k, n in {"A": 1, "B": 2, "E": 3, "D": 4}.items() if k in boards.BOARDS}
    failed = 0
    for pcb in sorted(glob.glob(os.path.join(ROOT, "BOARD*_1L", "*.kicad_pcb"))):
        bk = rev[os.path.basename(pcb).split("_")[0]]
        v, u = drc(pcb)
        r = reload_check(pcb, bk)
        png = picture(pcb)
        ok = v == 0 and u == 0 and not r["mismatch"] and not r["missing"] and r["fcu_tracks"] == 0
        failed += not ok
        print(f"{os.path.basename(pcb)}: DRC violations={v} unconnected={u} pad-net mismatches={len(r['mismatch'])}"
              f" missing pads={len(r['missing'])} F.Cu tracks={r['fcu_tracks']}  {'OK' if ok else 'FAIL'}")
        for m in (r["mismatch"] + r["missing"])[:8]:
            print("   !", m)
        print("   ", png)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
