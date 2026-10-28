"""SINGLE-LAYER zero-board KiCad projects (run with KiCad's python):

  "C:\\Program Files\\KiCad\\10.0\\bin\\python.exe" kicad_export1.py [A B C D E]

All copper on B.Cu (solder side): hole-to-hole trails, straight runs and
90-degree turns only. Crossings use WIRE LINKS (footprints LK1, LK2 ...):
two pads with the same number, flagged as jumper pads, drawn as a line on the
component side — exactly a wire link on a real single-sided board.
Output: ../kicad_single/ZB_<X>_<NAME>_1L/  (.kicad_pro, .kicad_pcb,
_BUILD_LIST.md). KiCad DRC must then report 0 errors / 0 unconnected.
"""
from __future__ import annotations

import json
import os
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import boards                       # noqa: E402
import zb_engine as z               # noqa: E402
import zb_router1 as r1             # noqa: E402
from kicad_export import H, NAMES, NUMBER, P, mm, rect, text, width, _runs   # noqa: E402

OUT = os.path.join(os.path.dirname(HERE), "kicad_single")


def link_footprint(board, ref, a, c, net):
    fp = pcbnew.FOOTPRINT(board)
    fp.SetReference(ref)
    fp.SetValue("wire link")
    fp.SetPosition(H(a))
    fp.SetDuplicatePadNumbersAreJumpers(True)
    board.Add(fp)
    for h in (a, c):
        pad = pcbnew.PAD(fp)
        pad.SetNumber("1")
        pad.SetAttribute(pcbnew.PAD_ATTRIB_PTH)
        pad.SetShape(pcbnew.PAD_SHAPE_CIRCLE)
        pad.SetSize(pcbnew.VECTOR2I(mm(1.8), mm(1.8)))
        pad.SetDrillSize(pcbnew.VECTOR2I(mm(1.0), mm(1.0)))
        pad.SetLayerSet(pcbnew.PAD.PTHMask())
        pad.SetPosition(H(h))
        pad.SetNet(net)
        fp.Add(pad)
    for layer, w in ((pcbnew.F_Fab, 0.6),):
        s = pcbnew.PCB_SHAPE(fp)
        s.SetShape(pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(H(a))
        s.SetEnd(H(c))
        s.SetLayer(layer)
        s.SetWidth(mm(w))
        fp.Add(s)
    mx, my = (z.hx(a[0]) + z.hx(c[0])) / 2, (z.hy(a[1]) + z.hy(c[1])) / 2
    fp.Reference().SetPosition(P(mx + (0.9 if a[0] == c[0] else 0), my + (0.9 if a[1] == c[1] else 0)))
    fp.Reference().SetLayer(pcbnew.F_Fab)
    fp.Reference().SetTextSize(pcbnew.VECTOR2I(mm(0.8), mm(0.8)))
    fp.Value().SetVisible(False)


def build(bk: str) -> str:
    b = boards.BOARDS[bk][0]()
    frozen = getattr(boards, "ROUTE_FROZEN", {}).get(bk)
    if frozen and os.path.exists(frozen):          # copper already built on the real board: never re-route it
        data = json.load(open(frozen, encoding="utf-8"))["routes"]
        routed = {n: {"trails": [[tuple(h) for h in t] for t in v["trails"]],
                      "links": [(tuple(a), tuple(c)) for a, c in v["links"]]} for n, v in data.items()}
    else:
        results, _ = r1.route1(b, seed=getattr(boards, "ROUTE_SEED", {}).get(bk))   # best routing order found
        routed = r1.split1(results)
    problems = r1.verify1(b, routed) + z.fit_check(b)
    if problems:
        raise SystemExit(f"{bk}: refusing to export, verification failed: {problems[:5]}")

    board = pcbnew.BOARD()
    board.SetCopperLayerCount(2)
    ds = board.GetDesignSettings()
    ds.m_CopperEdgeClearance = mm(0.5)
    ds.m_MinClearance = mm(0.2)
    rnd = getattr(b, "round", None)
    if rnd:                                   # round disk + rod holes
        cx, cy, rad = rnd
        circ = pcbnew.PCB_SHAPE(board)
        circ.SetShape(pcbnew.SHAPE_T_CIRCLE)
        circ.SetCenter(P(cx, cy))
        circ.SetEnd(P(cx + rad, cy))
        circ.SetLayer(pcbnew.Edge_Cuts)
        circ.SetWidth(mm(0.15))
        board.Add(circ)
        for i, (rx, ry, d) in enumerate(getattr(b, "rods", ())):
            fp = pcbnew.FOOTPRINT(board)
            fp.SetReference(f"ROD{i + 1}")
            fp.SetValue(f"rod hole Ø{d}")
            fp.SetPosition(P(rx, ry))
            board.Add(fp)
            pad = pcbnew.PAD(fp)
            pad.SetAttribute(pcbnew.PAD_ATTRIB_NPTH)
            pad.SetShape(pcbnew.PAD_SHAPE_CIRCLE)
            pad.SetSize(pcbnew.VECTOR2I(mm(d), mm(d)))
            pad.SetDrillSize(pcbnew.VECTOR2I(mm(d), mm(d)))
            pad.SetLayerSet(pcbnew.PAD.UnplatedHoleMask())
            pad.SetPosition(P(rx, ry))
            fp.Add(pad)
            fp.Reference().SetLayer(pcbnew.F_Fab)
            fp.Value().SetLayer(pcbnew.F_Fab)
    else:
        rect(board, pcbnew.Edge_Cuts, 0, 0, z.BOARD_W, z.BOARD_H, 0.15)
    for c in range(1, z.NC + 1):
        for r in range(1, z.NR + 1):
            if rnd and not z.usable(b, c, r) and ((z.hx(c) - rnd[0]) ** 2 + (z.hy(r) - rnd[1]) ** 2) ** 0.5 > rnd[2] - 1.0:
                continue
            s = pcbnew.PCB_SHAPE(board)
            s.SetShape(pcbnew.SHAPE_T_CIRCLE)
            s.SetCenter(H((c, r)))
            s.SetEnd(P(z.hx(c) + 0.5, z.hy(r)))
            s.SetLayer(pcbnew.Dwgs_User)
            s.SetWidth(mm(0.1))
            board.Add(s)
    for c in range(1, z.NC + 1, 2):
        text(board, pcbnew.Dwgs_User, z.hx(c), -1.6, str(c), 0.8, 0.1)
    for r in range(1, z.NR + 1, 2):
        text(board, pcbnew.Dwgs_User, -2.0, z.hy(r), str(r), 0.8, 0.1)
    text(board, pcbnew.Dwgs_User, z.BOARD_W / 2, z.BOARD_H + 4,
         f"VRITRA BOARD {NUMBER[bk]} {NAMES[bk]} — SINGLE LAYER zero PCB {z.BOARD_W:.0f} x {z.BOARD_H:.0f} — blue = solder trail (bottom), "
         f"black LKn = wire link on the component side", 1.4, 0.2)

    nets = {}
    for n in b.nets:
        ni = pcbnew.NETINFO_ITEM(board, n.name)
        board.Add(ni)
        nets[n.name] = ni
    net_of = {(r, p): n.name for n in b.nets for r, p in n.pins}

    for p in b.parts.values():
        x0, y0, x1, y1 = p.body
        if p.dashed or not p.pins:
            rect(board, pcbnew.Dwgs_User, x0, y0, x1, y1, 0.15)
            text(board, pcbnew.Dwgs_User, (x0 + x1) / 2, (y0 + y1) / 2,
                 p.label + (" (future)" if p.dashed else ""), 1.0, 0.12)
            continue
        fp = pcbnew.FOOTPRINT(board)
        fp.SetReference(p.ref)
        fp.SetValue(p.label)
        fp.SetPosition(H(next(iter(p.pins.values()))))
        board.Add(fp)
        fp.Reference().SetPosition(P((x0 + x1) / 2, y1 + 1.2))
        fp.Reference().SetLayer(pcbnew.F_Fab)
        fp.Reference().SetTextSize(pcbnew.VECTOR2I(mm(1.2), mm(1.2)))
        fp.Value().SetPosition(P((x0 + x1) / 2, (y0 + y1) / 2))
        fp.Value().SetLayer(pcbnew.F_Fab)
        fp.Value().SetTextSize(pcbnew.VECTOR2I(mm(1.0), mm(1.0)))
        rect(fp, pcbnew.F_CrtYd, x0, y0, x1, y1, 0.05)
        rect(fp, pcbnew.F_Fab, x0, y0, x1, y1, 0.1)
        for pad_id, hole in p.pins.items():
            pad = pcbnew.PAD(fp)
            pad.SetNumber(pad_id)
            pad.SetAttribute(pcbnew.PAD_ATTRIB_PTH)
            pad.SetShape(pcbnew.PAD_SHAPE_RECT if pad_id == "1" else pcbnew.PAD_SHAPE_CIRCLE)
            pad.SetSize(pcbnew.VECTOR2I(mm(1.8), mm(1.8)))
            pad.SetDrillSize(pcbnew.VECTOR2I(mm(1.0), mm(1.0)))
            pad.SetLayerSet(pcbnew.PAD.PTHMask())
            pad.SetPosition(H(hole))
            pad.SetPinFunction(p.names.get(pad_id, pad_id))
            net = net_of.get((p.ref, pad_id))
            if net:
                pad.SetNet(nets[net])
            fp.Add(pad)
            text(fp, pcbnew.F_Fab, z.hx(hole[0]) + 0.3, z.hy(hole[1]) + 1.25, p.names.get(pad_id, pad_id), 0.55, 0.08)

    ntr = nlk = 0
    for name, rr in routed.items():
        for seg in rr["trails"]:
            for a, c in zip(seg, seg[1:]):
                t = pcbnew.PCB_TRACK(board)
                t.SetStart(H(a))
                t.SetEnd(H(c))
                t.SetWidth(width(name))
                t.SetLayer(pcbnew.B_Cu)
                t.SetNet(nets[name])
                board.Add(t)
                ntr += 1
        for a, c in rr["links"]:
            nlk += 1
            link_footprint(board, f"LK{nlk}", a, c, nets[name])

    d = os.path.join(OUT, f"BOARD{NUMBER[bk]}_{NAMES[bk]}_1L")
    os.makedirs(d, exist_ok=True)
    stem = os.path.join(d, f"BOARD{NUMBER[bk]}_{NAMES[bk]}_1L")
    pcbnew.SaveBoard(stem + ".kicad_pcb", board)
    pro = stem + ".kicad_pro"
    data = {"meta": {"filename": os.path.basename(pro), "version": 1}}
    if os.path.exists(pro):
        with open(pro, encoding="utf-8") as f:
            data = json.load(f)
    sev = data.setdefault("board", {}).setdefault("design_settings", {}).setdefault("rule_severities", {})
    for k in getattr(b, "drc_ignore", []):        # documented, physically checked exceptions only
        sev[k] = "ignore"
    with open(pro, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
    write_build_list(stem + "_BUILD_LIST.md", bk, b, routed)
    print(f"[{bk}] wrote {stem}.kicad_pcb nets={len(b.nets)} trail-segments={ntr} wire-links={nlk}", flush=True)
    return stem + ".kicad_pcb"


def write_build_list(path, bk, b, routed):
    L = [f"# VRITRA {NAMES[bk]} board — SINGLE-LAYER build list (same routes as the KiCad file)", "",
         "Hole = C(column, 1 = left on the component side) R(row, 1 = bottom).",
         "TRAIL = solder trail / bare tinned wire on the solder side, every hole in the run soldered, 90° turns only.",
         "LINK = straight insulated wire link on the component side between the two holes (it jumps over other trails).", ""]
    n_links = 0
    for n in b.nets:
        rr = routed[n.name]
        pins = ", ".join(f"{ref}.{b.parts[ref].names.get(pad, pad)} (C{b.parts[ref].pins[pad][0]}R{b.parts[ref].pins[pad][1]})"
                         for ref, pad in n.pins)
        L.append(f"## {n.name}")
        L.append(f"- pins: {pins}")
        for seg in rr["trails"]:
            L.append("- TRAIL: " + " · ".join(_runs(seg)))
        for a, c in rr["links"]:
            n_links += 1
            L.append(f"- LINK LK{n_links}: C{a[0]}R{a[1]} ↔ C{c[0]}R{c[1]}")
        L.append("")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(L))


if __name__ == "__main__":
    for k in sys.argv[1:] or list(boards.BOARDS):
        build(k)
