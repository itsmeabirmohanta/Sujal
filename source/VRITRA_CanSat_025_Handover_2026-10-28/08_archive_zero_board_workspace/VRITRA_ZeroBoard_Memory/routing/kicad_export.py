"""Write each zero board as a real KiCad 10 project (run with KiCad's python):

  "C:\\Program Files\\KiCad\\10.0\\bin\\python.exe" kicad_export.py [A B C D E]

Output: ../kicad/ZB_<X>_<NAME>/ZB_<X>_<NAME>.kicad_pro + .kicad_pcb
  * 120 x 150 mm rectangle (Edge.Cuts), 2.54 mm hole grid drawn on User.Drawings
  * every module / connector / resistor as a footprint whose pads sit on the
    exact holes, pad numbers + pin names (pin function), nets assigned
  * B.Cu tracks = solder trails on the bottom, F.Cu tracks = insulated wires on
    the component side, vias = a wire going through an empty hole
  * all nets routed, power included (zb_router2), independently verified
    before writing; then KiCad's own DRC must report 0 errors / 0 unconnected.
"""
from __future__ import annotations

import json
import os
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import boards                  # noqa: E402
import zb_engine as z          # noqa: E402
import zb_router2 as r2        # noqa: E402

OUT = os.path.join(os.path.dirname(HERE), "kicad")
OX, OY = 50.0, 50.0
NAMES = {"A": "MAIN", "B": "BACKUP_SENSOR", "D": "PARACHUTE", "E": "POWER"}
NUMBER = {"A": 1, "B": 2, "E": 3, "D": 4}          # the team's board numbers (1, 2, 4 are etched now)
HEAVY = {"VBAT", "BAT+", "VBAT_SW", "VBAT_F", "+8V_STEER", "8V_RAW", "+5V_SERVO", "5V_RAW"}
POWER = {"GND", "+3V3_A", "+3V3_B", "+5V_LOGIC", "+5V_MQ"}
mm = pcbnew.FromMM


def P(x, y):
    """our mm (origin bottom-left, y up) -> KiCad IU (y down)"""
    return pcbnew.VECTOR2I(mm(OX + x), mm(OY + (z.BOARD_H - y)))


def H(hole):
    return P(z.hx(hole[0]), z.hy(hole[1]))


def width(net):
    return mm(1.5) if net in HEAVY else mm(1.1) if net in POWER else mm(0.7)


def rect(parent, layer, x0, y0, x1, y1, w=0.12):
    s = pcbnew.PCB_SHAPE(parent)
    s.SetShape(pcbnew.SHAPE_T_RECT)
    s.SetStart(P(x0, y1))
    s.SetEnd(P(x1, y0))
    s.SetLayer(layer)
    s.SetWidth(mm(w))
    parent.Add(s)
    return s


def text(parent, layer, x, y, msg, size=1.0, thick=0.15):
    t = pcbnew.PCB_TEXT(parent)
    t.SetText(msg)
    t.SetPosition(P(x, y))
    t.SetLayer(layer)
    t.SetTextSize(pcbnew.VECTOR2I(mm(size), mm(size)))
    t.SetTextThickness(mm(thick))
    parent.Add(t)
    return t


def build(bk: str) -> str:
    b = boards.BOARDS[bk][0]()
    results, _ = r2.route2(b)
    routed = r2.split(results)
    problems = r2.verify2(b, routed) + z.fit_check(b)
    if problems:
        raise SystemExit(f"{bk}: refusing to export, verification failed: {problems[:5]}")

    board = pcbnew.BOARD()
    board.SetCopperLayerCount(2)
    ds = board.GetDesignSettings()
    ds.m_CopperEdgeClearance = mm(0.5)
    try:
        nc = ds.m_NetSettings.GetDefaultNetclass()
        nc.SetClearance(mm(0.3))
        nc.SetTrackWidth(mm(0.7))
        nc.SetViaDiameter(mm(1.8))
        nc.SetViaDrill(mm(1.0))
    except Exception:
        pass
    ds.m_MinClearance = mm(0.2)

    # outline + hole grid + title
    rect(board, pcbnew.Edge_Cuts, 0, 0, z.BOARD_W, z.BOARD_H, 0.15)
    for c in range(1, z.NC + 1):
        for r in range(1, z.NR + 1):
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
         f"VRITRA {NAMES[bk]} board — 120 x 150 zero PCB — red F.Cu = insulated top wire, blue B.Cu = bottom solder trail",
         1.5, 0.2)

    nets = {}
    for n in b.nets:
        ni = pcbnew.NETINFO_ITEM(board, n.name)
        board.Add(ni)
        nets[n.name] = ni
    net_of = {(r, p): n.name for n in b.nets for r, p in n.pins}

    # footprints
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
        first = next(iter(p.pins.values()))
        fp.SetPosition(H(first))
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

    # tracks: one segment per hole-to-hole step (every junction is a real endpoint)
    ntracks = nvias = 0
    for name, rr in routed.items():
        for layer, key in ((pcbnew.B_Cu, "bottom"), (pcbnew.F_Cu, "top")):
            for seg in rr[key]:
                for a, c in zip(seg, seg[1:]):
                    t = pcbnew.PCB_TRACK(board)
                    t.SetStart(H(a))
                    t.SetEnd(H(c))
                    t.SetWidth(width(name))
                    t.SetLayer(layer)
                    t.SetNet(nets[name])
                    board.Add(t)
                    ntracks += 1
        for h in rr["vias"]:
            v = pcbnew.PCB_VIA(board)
            v.SetPosition(H(h))
            v.SetViaType(pcbnew.VIATYPE_THROUGH)
            v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
            v.SetWidth(mm(1.8))
            v.SetDrill(mm(1.0))
            v.SetNet(nets[name])
            board.Add(v)
            nvias += 1

    d = os.path.join(OUT, f"ZB_{bk}_{NAMES[bk]}")
    os.makedirs(d, exist_ok=True)
    stem = os.path.join(d, f"ZB_{bk}_{NAMES[bk]}")
    pcbnew.SaveBoard(stem + ".kicad_pcb", board)
    if not os.path.exists(stem + ".kicad_pro"):
        with open(stem + ".kicad_pro", "w", encoding="utf-8") as f:
            json.dump({"meta": {"filename": os.path.basename(stem) + ".kicad_pro", "version": 1}}, f)
    write_build_list(stem + "_BUILD_LIST.md", bk, b, routed)
    print(f"[{bk}] wrote {stem}.kicad_pcb  footprints={len([p for p in b.parts.values() if p.pins and not p.dashed])}"
          f" nets={len(b.nets)} tracks={ntracks} vias={nvias}")
    return stem + ".kicad_pcb"


def _runs(seg):
    """hole path -> straight runs 'C21R33→C21R40'"""
    runs, start = [], seg[0]
    for i in range(1, len(seg)):
        last = i == len(seg) - 1
        if last or (seg[i + 1][0] - seg[i][0], seg[i + 1][1] - seg[i][1]) != (seg[i][0] - seg[i - 1][0], seg[i][1] - seg[i - 1][1]):
            runs.append(f"C{start[0]}R{start[1]}→C{seg[i][0]}R{seg[i][1]}")
            start = seg[i]
    return runs


def write_build_list(path, bk, b, routed):
    L = [f"# VRITRA {NAMES[bk]} board — build list (same routes as the KiCad file)", "",
         "Hole = C(column, 1 = left on the component side) R(row, 1 = bottom).",
         "BOTTOM = solder trail / bare tinned wire on the solder side, every hole in the run soldered.",
         "TOP = insulated wire on the component side lying along the listed holes (only its ends are soldered).",
         "THROUGH = the wire passes through that empty hole from one side to the other.", ""]
    for n in b.nets:
        rr = routed[n.name]
        pins = ", ".join(f"{ref}.{b.parts[ref].names.get(pad, pad)} (C{b.parts[ref].pins[pad][0]}R{b.parts[ref].pins[pad][1]})"
                         for ref, pad in n.pins)
        L.append(f"## {n.name}")
        L.append(f"- pins: {pins}")
        for seg in rr["bottom"]:
            L.append("- BOTTOM: " + " · ".join(_runs(seg)))
        for seg in rr["top"]:
            L.append("- TOP wire: " + " · ".join(_runs(seg)))
        for h in rr["vias"]:
            L.append(f"- THROUGH hole C{h[0]}R{h[1]}")
        L.append("")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(L))


if __name__ == "__main__":
    for k in sys.argv[1:] or list(boards.BOARDS):
        build(k)
