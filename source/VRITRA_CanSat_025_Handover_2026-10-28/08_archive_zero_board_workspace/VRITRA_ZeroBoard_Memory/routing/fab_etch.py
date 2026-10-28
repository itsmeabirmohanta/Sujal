"""Check + fabrication outputs for the etched boards (system python, calls kicad-cli):

  python fab_etch.py [1 2 4]

For each ../etch_pcb/BOARD<n>_*/BOARD<n>_*.kicad_pcb:
  * KiCad DRC, all severities -> must be 0 violations / 0 unconnected
  * reload in pcbnew: every pad carries the netlist's net, nothing on F.Cu
  * Gerbers (B.Cu, B.Mask, F.Silkscreen, Edge.Cuts) + Excellon drill -> GERBER/ and a .zip
  * PDFs at 1:1 for home etching: *_TONER_bottom.pdf (B.Cu, print NOT mirrored),
    *_PLACEMENT_top.pdf (silkscreen guide), *_DRILL_map.pdf
  * PNG preview of the bottom copper with the top silkscreen
"""
from __future__ import annotations

import glob
import json
import os
import subprocess
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(os.path.dirname(HERE), "etch_pcb")
KBIN = r"C:\Program Files\KiCad\10.0\bin"
CLI = os.path.join(KBIN, "kicad-cli.exe")
KPY = os.path.join(KBIN, "python.exe")

RELOAD = r'''
import sys, json
sys.path.insert(0, sys.argv[2])
import pcbnew, etch_boards as eb
b, problems = eb.build(sys.argv[3])
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
        seen.add(key)
        if want.get(key) != got:
            bad.append(f"{key}: board={got} netlist={want.get(key)}")
missing = [str(k) for k in want if k not in seen]
fcu = sum(1 for t in pcb.GetTracks() if t.GetLayer() == pcbnew.F_Cu)
print(json.dumps({"mismatch": bad, "missing": missing, "fcu_tracks": fcu, "problems": problems}))
'''


def run(args):
    r = subprocess.run(args, capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit(f"failed: {' '.join(args)}\n{r.stdout}\n{r.stderr}")
    return r.stdout


def drc(pcb, out):
    rep = os.path.join(out, "DRC_report.json")
    run([CLI, "pcb", "drc", "--format", "json", "--severity-all", "-o", rep, pcb])
    with open(rep, encoding="utf-8") as f:
        d = json.load(f)
    return d.get("violations", []), d.get("unconnected_items", [])


def fab(pcb, stem, out):
    gdir = os.path.join(out, "GERBER")
    os.makedirs(gdir, exist_ok=True)
    run([CLI, "pcb", "export", "gerbers", "--layers", "B.Cu,B.Mask,F.Silkscreen,Edge.Cuts", "-o", gdir + os.sep, pcb])
    run([CLI, "pcb", "export", "drill", "--format", "excellon", "--excellon-units", "mm", "--generate-map",
         "--map-format", "pdf", "-o", gdir + os.sep, pcb])
    z = os.path.join(out, stem + "_GERBER.zip")
    with zipfile.ZipFile(z, "w", zipfile.ZIP_DEFLATED) as zf:
        for f in sorted(os.listdir(gdir)):
            zf.write(os.path.join(gdir, f), f)
    # 1:1 PDFs for toner transfer / photo resist (bottom copper is printed as KiCad shows it = NOT mirrored)
    run([CLI, "pcb", "export", "pdf", "--layers", "B.Cu,Edge.Cuts", "--mode-single", "--black-and-white",
         "--drill-shape-opt", "1", "--scale", "1", "-o", os.path.join(out, stem + "_TONER_bottom.pdf"), pcb])
    run([CLI, "pcb", "export", "pdf", "--layers", "F.Silkscreen,F.Fab,Edge.Cuts", "--mode-single", "--black-and-white",
         "--scale", "1", "-o", os.path.join(out, stem + "_PLACEMENT_top.pdf"), pcb])
    svg = os.path.join(out, stem + "_preview.svg")
    run([CLI, "pcb", "export", "svg", "--layers", "B.Cu,F.Silkscreen,Edge.Cuts", "--mode-single", "--page-size-mode", "2",
         "--exclude-drawing-sheet", "--drill-shape-opt", "2", "-o", svg, pcb])
    import fitz
    png = os.path.join(out, stem + "_preview.png")
    fitz.open(svg)[0].get_pixmap(dpi=220).save(png)
    return z, png


def main():
    nums = sys.argv[1:] or ["1", "2"]
    bad = 0
    for num in nums:
        hits = glob.glob(os.path.join(ROOT, f"BOARD{num}_*", f"BOARD{num}_*.kicad_pcb"))
        if not hits:
            print(f"board {num}: no .kicad_pcb yet")
            bad += 1
            continue
        pcb = hits[0]
        out = os.path.dirname(pcb)
        stem = os.path.basename(pcb)[:-10]
        v, u = drc(pcb, out)
        r = json.loads(subprocess.run([KPY, "-c", RELOAD, pcb, HERE, num], capture_output=True, text=True,
                                      check=True).stdout.strip().splitlines()[-1])
        ok = not v and not u and not r["mismatch"] and not r["missing"] and r["fcu_tracks"] == 0 and not r["problems"]
        bad += not ok
        print(f"{stem}: DRC violations={len(v)} unconnected={len(u)} pad-net mismatches={len(r['mismatch'])} "
              f"missing pads={len(r['missing'])} F.Cu tracks={r['fcu_tracks']}  {'OK' if ok else 'FAIL'}")
        for item in (v + u)[:12]:
            print("   !", item.get("type"), "-", item.get("description"),
                  [i.get("description") for i in item.get("items", [])][:2])
        for m in (r["mismatch"] + r["missing"] + r["problems"])[:8]:
            print("   !", m)
        if ok:
            zp, png = fab(pcb, stem, out)
            print("   ", zp)
            print("   ", png)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
