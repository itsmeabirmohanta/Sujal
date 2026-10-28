"""Board 4 PARACHUTE single-layer PCB -> KiCad 10 project (run with KiCad's python):

  "C:\\Program Files\\KiCad\\10.0\\bin\\python.exe" kicad_board4_pcb.py

Ø110 mm round board, 4 rod holes Ø8.3, component holes 1.4 mm, pads 2.2 mm (pin 1 square),
tracks 1.0 mm signal / 1.5 mm power / 2.0 mm servo power, all copper on the bottom, GND fill,
wire jumpers W# (pads + silkscreen line) only where a crossing is unavoidable.
Output: ../etch_pcb/BOARD4_PARACHUTE_PCB/
"""
from __future__ import annotations

import json
import os
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import board4_pcb as b4           # noqa: E402
import zb_engine as z             # noqa: E402
import zb_router1 as r1           # noqa: E402

OUT = os.path.join(os.path.dirname(HERE), "etch_pcb", "BOARD4_PARACHUTE_PCB")
NAME = "BOARD4_PARACHUTE_PCB"
mm = pcbnew.FromMM
OX, OY = 20.0, 20.0
PAD, DRILL = 2.2, 1.4
W_SIG, W_PWR, W_HEAVY = 1.0, 1.5, 2.0
POWER = {"GND", "+3V3_B", "+5V_LOGIC"}
HEAVY = {"+8V_STEER", "+5V_SERVO"}
DRU = """(version 1)
# Board 4 single-layer PCB: 1.4 mm holes, 2.2 mm pads (0.34 mm between pads on a 2.54 mm header)
(rule "clearance"   (constraint clearance (min 0.3mm)))
(rule "track"       (constraint track_width (min 1.0mm)))
(rule "hole"        (constraint hole_size (min 1.4mm)))
(rule "annular"     (constraint annular_width (min 0.4mm)))
(rule "edge"        (constraint edge_clearance (min 1.0mm)))
(rule "gnd spokes"  (constraint min_resolved_spokes 1))
"""
# no printed silkscreen on a home-made board; LK/W jumpers may end under the LSM (on its header)
IGNORE = ("silk_over_copper", "silk_overlap", "silk_edge_clearance", "text_height", "text_thickness",
          "lib_footprint_issues", "lib_footprint_mismatch", "pth_inside_courtyard")


def K(x, y):
    return pcbnew.VECTOR2I(mm(OX + x), mm(OY + (z.BOARD_H - y)))


def H(h):
    return K(z.hx(h[0]), z.hy(h[1]))


def seg(parent, layer, a, c, w):
    s = pcbnew.PCB_SHAPE(parent)
    s.SetShape(pcbnew.SHAPE_T_SEGMENT)
    s.SetStart(a)
    s.SetEnd(c)
    s.SetLayer(layer)
    s.SetWidth(mm(w))
    parent.Add(s)


def rect(parent, layer, x0, y0, x1, y1, w=0.15):
    for a, c in (((x0, y0), (x1, y0)), ((x1, y0), (x1, y1)), ((x1, y1), (x0, y1)), ((x0, y1), (x0, y0))):
        seg(parent, layer, K(*a), K(*c), w)


def text(parent, layer, x, y, msg, size=1.0, thick=0.15):
    t = pcbnew.PCB_TEXT(parent)
    t.SetText(msg)
    t.SetPosition(K(x, y))
    t.SetLayer(layer)
    t.SetTextSize(pcbnew.VECTOR2I(mm(size), mm(size)))
    t.SetTextThickness(mm(thick))
    parent.Add(t)


def width(net):
    return W_HEAVY if net in HEAVY else W_PWR if net in POWER else W_SIG


def add_pad(fp, number, pos, net, square=False):
    pad = pcbnew.PAD(fp)
    pad.SetNumber(number)
    pad.SetAttribute(pcbnew.PAD_ATTRIB_PTH)
    pad.SetShape(pcbnew.PAD_SHAPE_RECT if square else pcbnew.PAD_SHAPE_CIRCLE)
    pad.SetSize(pcbnew.VECTOR2I(mm(PAD), mm(PAD)))
    pad.SetDrillSize(pcbnew.VECTOR2I(mm(DRILL), mm(DRILL)))
    pad.SetLayerSet(pcbnew.PAD.PTHMask())
    pad.SetPosition(pos)
    if net is not None:
        pad.SetNet(net)
    fp.Add(pad)
    return pad


def build():
    b = b4.board4_pcb()
    data = json.load(open(b4.OUT_JSON, encoding="utf-8"))["routes"]
    routed = {n: {"trails": [[tuple(h) for h in t] for t in v["trails"]],
                  "links": [(tuple(a), tuple(c)) for a, c in v["links"]]} for n, v in data.items()}
    problems = r1.verify1(b, routed) + z.fit_check(b) + b4.check_netlist(b)
    if problems:
        raise SystemExit(f"refusing to export: {problems[:5]}")

    board = pcbnew.BOARD()
    board.SetCopperLayerCount(2)
    ds = board.GetDesignSettings()
    ds.m_CopperEdgeClearance = mm(1.0)
    ds.m_MinClearance = mm(0.3)
    ds.m_TrackMinWidth = mm(1.0)
    ds.m_MinThroughDrill = mm(1.4)
    nc = ds.m_NetSettings.GetDefaultNetclass()
    nc.SetClearance(mm(0.3))
    nc.SetTrackWidth(mm(W_SIG))

    # outline + rod holes
    cx, cy, rad = b.round
    c = pcbnew.PCB_SHAPE(board)
    c.SetShape(pcbnew.SHAPE_T_CIRCLE)
    c.SetCenter(K(cx, cy))
    c.SetEnd(K(cx + rad, cy))
    c.SetLayer(pcbnew.Edge_Cuts)
    c.SetWidth(mm(0.15))
    board.Add(c)
    for i, (rx, ry, d) in enumerate(b.rods):
        fp = pcbnew.FOOTPRINT(board)
        fp.SetReference(f"ROD{i + 1}")
        fp.SetValue(f"rod Ø{d}")
        fp.SetPosition(K(rx, ry))
        board.Add(fp)
        pad = pcbnew.PAD(fp)
        pad.SetAttribute(pcbnew.PAD_ATTRIB_NPTH)
        pad.SetShape(pcbnew.PAD_SHAPE_CIRCLE)
        pad.SetSize(pcbnew.VECTOR2I(mm(d), mm(d)))
        pad.SetDrillSize(pcbnew.VECTOR2I(mm(d), mm(d)))
        pad.SetLayerSet(pcbnew.PAD.UnplatedHoleMask())
        pad.SetPosition(K(rx, ry))
        fp.Add(pad)
        fp.Reference().SetLayer(pcbnew.F_Fab)
        fp.Value().SetVisible(False)
    text(board, pcbnew.F_SilkS, cx - 22, cy - rad + 33, "VRITRA BOARD 4", 1.6, 0.25)   # clear of rods and parts

    nets = {}
    for n in b.nets:
        ni = pcbnew.NETINFO_ITEM(board, n.name)
        board.Add(ni)
        nets[n.name] = ni
    net_of = {(r, p): n.name for n in b.nets for r, p in n.pins}

    for p in b.parts.values():
        x0, y0, x1, y1 = p.body
        fp = pcbnew.FOOTPRINT(board)
        fp.SetReference(p.ref)
        fp.SetValue(p.label)
        fp.SetPosition(H(next(iter(p.pins.values()))))
        board.Add(fp)
        rect(fp, pcbnew.F_SilkS, x0, y0, x1, y1, 0.15)          # part outline on the silkscreen
        rect(fp, pcbnew.F_CrtYd, x0, y0, x1, y1, 0.05)
        fp.Reference().SetPosition(K((x0 + x1) / 2, y1 + 1.3))
        fp.Reference().SetLayer(pcbnew.F_SilkS)
        fp.Reference().SetTextSize(pcbnew.VECTOR2I(mm(1.2), mm(1.2)))
        fp.Reference().SetTextThickness(mm(0.18))
        fp.Value().SetLayer(pcbnew.F_Fab)
        fp.Value().SetPosition(K((x0 + x1) / 2, (y0 + y1) / 2))
        for pad_id, hole in p.pins.items():
            net = net_of.get((p.ref, pad_id))
            add_pad(fp, pad_id, H(hole), nets[net] if net else None, square=(pad_id == "1"))
            pt = p.names.get(pad_id, pad_id)
            text(fp, pcbnew.F_Fab, z.hx(hole[0]) + 0.4, z.hy(hole[1]) + 1.5, pt, 0.6, 0.08)

    ntr = 0
    for name, rr in routed.items():
        for t in rr["trails"]:
            for a, c2 in zip(t, t[1:]):
                tr = pcbnew.PCB_TRACK(board)
                tr.SetStart(H(a))
                tr.SetEnd(H(c2))
                tr.SetWidth(mm(width(name)))
                tr.SetLayer(pcbnew.B_Cu)
                tr.SetNet(nets[name])
                board.Add(tr)
                ntr += 1
    nj, jl = 0, []
    for name, rr in routed.items():
        for a, c2 in rr["links"]:
            nj += 1
            fp = pcbnew.FOOTPRINT(board)
            fp.SetReference(f"W{nj}")
            fp.SetValue("wire jumper")
            fp.SetPosition(H(a))
            fp.SetDuplicatePadNumbersAreJumpers(True)
            board.Add(fp)
            for h in (a, c2):
                add_pad(fp, "1", H(h), nets[name])
            seg(fp, pcbnew.F_SilkS, H(a), H(c2), 0.4)
            fp.Reference().SetLayer(pcbnew.F_SilkS)
            fp.Reference().SetPosition(K((z.hx(a[0]) + z.hx(c2[0])) / 2 + 1.2, (z.hy(a[1]) + z.hy(c2[1])) / 2 + 1.2))
            fp.Value().SetVisible(False)
            jl.append((f"W{nj}", name, a, c2))

    # GND copper fill on the bottom, inside the disk
    zone = pcbnew.ZONE(board)
    zone.SetLayer(pcbnew.B_Cu)
    zone.SetNet(nets["GND"])
    ol = zone.Outline()
    ol.NewOutline()
    import math
    for k in range(72):
        ang = 2 * math.pi * k / 72
        v = K(cx + (rad - 1.5) * math.cos(ang), cy + (rad - 1.5) * math.sin(ang))
        ol.Append(v.x, v.y)
    zone.SetLocalClearance(mm(0.6))
    zone.SetMinThickness(mm(0.8))
    zone.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
    zone.SetThermalReliefGap(mm(0.6))
    zone.SetThermalReliefSpokeWidth(mm(1.0))
    zone.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
    board.Add(zone)
    board.BuildConnectivity()                       # needed so the filler can drop islands
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())

    # Gerber settings stored IN the board: File > Fabrication Outputs > Gerbers opens ready to Plot
    po = board.GetPlotOptions()
    po.SetFormat(pcbnew.PLOT_FORMAT_GERBER)
    po.SetOutputDirectory("GERBER/")
    ls = pcbnew.LSET()
    for layer in (pcbnew.B_Cu, pcbnew.B_Mask, pcbnew.F_SilkS, pcbnew.Edge_Cuts):
        ls.AddLayer(layer)
    po.SetLayerSelection(ls)
    po.SetUseGerberProtelExtensions(True)        # .gbl .gbs .gto .gm1
    po.SetCreateGerberJobFile(True)
    po.SetUseGerberX2format(True)
    po.SetIncludeGerberNetlistInfo(True)
    po.SetGerberPrecision(6)
    po.SetSubtractMaskFromSilk(False)
    po.SetPlotFrameRef(False)
    po.SetUseAuxOrigin(False)
    po.SetMirror(False)
    po.SetNegative(False)
    po.SetSkipPlotNPTH_Pads(False)
    po.SetPlotReference(True)
    po.SetPlotValue(False)
    board.SetPlotOptions(po)

    os.makedirs(OUT, exist_ok=True)
    stem = os.path.join(OUT, NAME)
    pcbnew.SaveBoard(stem + ".kicad_pcb", board)
    pro = stem + ".kicad_pro"
    data = {"meta": {"filename": os.path.basename(pro), "version": 1}}
    if os.path.exists(pro):
        data = json.load(open(pro, encoding="utf-8"))
    sev = data.setdefault("board", {}).setdefault("design_settings", {}).setdefault("rule_severities", {})
    for k in IGNORE:
        sev[k] = "ignore"
    json.dump(data, open(pro, "w", encoding="utf-8"), indent=2)
    open(stem + ".kicad_dru", "w", encoding="utf-8").write(DRU)
    notes(stem + "_BUILD_NOTES.md", b, jl, ntr)
    print(f"wrote {stem}.kicad_pcb  tracks={ntr} jumpers={nj}")


def notes(path, b, jl, ntr):
    H_ = lambda h: f"C{h[0]}R{h[1]}"
    net_of = {(r, p): n.name for n in b.nets for r, p in n.pins}
    L = ["# Board 4 PARACHUTE — single-layer PCB", "",
         "Ø110 mm, rod holes Ø8.3, component holes 1.4 mm, pads 2.2 mm, copper on the BOTTOM only.",
         "Tracks 1.0 mm signal, 1.5 mm GND/5 V/3.3 V, 2.0 mm servo power (8 V, 5 V servo). GND copper fill.",
         "No series resistors: every servo / ESC / UART pin is wired straight to the Teensy. R6 = Hall pull-up only.", "",
         "## Wire jumpers (top side, fit them first)"]
    L += [f"- {ref} ({net}): {H_(a)} ↔ {H_(c)}" for ref, net, a, c in jl] or ["- none"]
    L += ["", "## Every pad and its signal", "| part | pin | hole | signal |", "|---|---|---|---|"]
    for p in b.parts.values():
        for pad, h in p.pins.items():
            L.append(f"| {p.ref} {p.label} | {p.names.get(pad, pad)} | {H_(h)} | {net_of.get((p.ref, pad), 'not connected')} |")
    open(path, "w", encoding="utf-8").write("\n".join(L) + "\n")


if __name__ == "__main__":
    build()
