"""ETCHED single-layer PCBs -> KiCad 10 projects + Gerbers (run with KiCad's python):

  "C:\\Program Files\\KiCad\\10.0\\bin\\python.exe" kicad_etch.py [1 2 4]

All copper on B.Cu (bottom). Components on the top, through-hole. Crossings are
WIRE JUMPERS W1, W2 ... (two pads + a line on the top silkscreen). GND copper
fill on the bottom with thermal spokes (less copper to etch, solid ground).
Rules: pad Ø1.8 mm, track 0.7 mm (servo power 1.8 mm), gap ≥ 0.5 mm.
Output: ../etch_pcb/BOARD<n>_<NAME>/ (.kicad_pro, .kicad_pcb, BUILD_NOTES.md)
"""
from __future__ import annotations

import json
import os
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import etch_boards as eb            # noqa: E402  (sets the 118 mm grid)
import etch_place as ep             # noqa: E402
import etch_router as er            # noqa: E402
import zb_engine as z               # noqa: E402

OUT = os.path.join(os.path.dirname(HERE), "etch_pcb")
mm = pcbnew.FromMM
PAD = 1.8
DRILL = 1.0
DRU = """(version 1)
# home etching (laser print -> iron -> etch), 1 oz copper, single sided, copper on B.Cu only
(rule "etch clearance"  (constraint clearance (min 0.45mm)))
(rule "etch track"      (constraint track_width (min 0.7mm)))
(rule "etch hole"       (constraint hole_size (min 1.0mm)))
(rule "etch annular"    (constraint annular_width (min 0.4mm)))
(rule "etch edge"       (constraint edge_clearance (min 1.0mm)))
(rule "gnd spokes"      (constraint min_resolved_spokes 1))
"""
# no silkscreen is printed on a home-etched board (the top-side guide is a paper print), so silk
# cosmetics are not checked; every copper, hole and connection rule stays an error
IGNORE = ("silk_over_copper", "silk_overlap", "silk_edge_clearance", "text_height", "text_thickness",
          "lib_footprint_issues", "lib_footprint_mismatch",
          "pth_inside_courtyard")   # wire jumpers deliberately sit under modules on headers (fit them first);
                                    # real part overlaps are still caught by courtyards_overlap
TRACK, HEAVY_W = 0.7, 1.8
CLEAR = 0.5
OX, OY = 20.0, 20.0                 # sheet offset of the board's bottom-left corner


def K(x, y):
    """board mm (origin bottom-left, y up) -> KiCad IU (y down)"""
    return pcbnew.VECTOR2I(mm(OX + x), mm(OY + (z.BOARD_H - y)))


def seg(parent, layer, a, b, w):
    s = pcbnew.PCB_SHAPE(parent)
    s.SetShape(pcbnew.SHAPE_T_SEGMENT)
    s.SetStart(K(*a))
    s.SetEnd(K(*b))
    s.SetLayer(layer)
    s.SetWidth(mm(w))
    parent.Add(s)


def rect(parent, layer, x0, y0, x1, y1, w=0.15):
    for a, b in (((x0, y0), (x1, y0)), ((x1, y0), (x1, y1)), ((x1, y1), (x0, y1)), ((x0, y1), (x0, y0))):
        seg(parent, layer, a, b, w)


def text(parent, layer, x, y, msg, size=1.0, thick=0.15, mirror=False):
    t = pcbnew.PCB_TEXT(parent)
    t.SetText(msg)
    t.SetPosition(K(x, y))
    t.SetLayer(layer)
    t.SetTextSize(pcbnew.VECTOR2I(mm(size), mm(size)))
    t.SetTextThickness(mm(thick))
    if mirror:
        t.SetMirrored(True)
    parent.Add(t)


def load(fpid):
    lib, name = fpid.split(":", 1)
    path = ep.fp_path(fpid)
    fp = pcbnew.FootprintLoad(os.path.dirname(path), name)
    if fp is None:
        raise SystemExit(f"footprint {fpid} not found")
    return fp


def place_footprint(board, part, nets, net_of):
    fp = load(part.fp)
    padmap = getattr(part, "padmap", {}) or {}
    for pad in list(fp.Pads()):
        num = padmap.get(pad.GetNumber(), pad.GetNumber())
        if num not in part.keep_pads or pad.GetAttribute() == pcbnew.PAD_ATTRIB_NPTH:
            fp.Remove(pad)
            continue
        pad.SetNumber(num)
    fp.SetReference(part.ref)
    fp.SetValue(part.label)
    fp.SetOrientationDegrees(part.rot)
    fp.SetPosition(pcbnew.VECTOR2I(0, 0))
    board.Add(fp)
    # targets: grid holes for on-grid pads, exact points for off-grid ones
    off = getattr(part, "offgrid", {})
    dx = dy = 0.0
    tg = {}
    for pad in fp.Pads():
        num = pad.GetNumber()
        tx, ty = off[num] if num in off else (z.hx(part.pins[num][0]), z.hy(part.pins[num][1]))
        t = K(tx, ty)
        p = pad.GetPosition()
        tg[num] = t
        dx += t.x - p.x
        dy += t.y - p.y
    n = max(1, len(tg))
    fp.SetPosition(pcbnew.VECTOR2I(int(dx / n), int(dy / n)))
    worst = 0.0
    for pad in fp.Pads():
        num = pad.GetNumber()
        p, t = pad.GetPosition(), tg[num]
        worst = max(worst, pcbnew.ToMM(((p.x - t.x) ** 2 + (p.y - t.y) ** 2) ** 0.5))
        # home etching: every pad round Ø1.8, every hole 1.0 mm (one drill bit); pin 1 is marked on the silkscreen
        pad.SetShape(pcbnew.PAD_SHAPE_CIRCLE)
        pad.SetSize(pcbnew.VECTOR2I(mm(PAD), mm(PAD)))
        pad.SetDrillShape(pcbnew.PAD_DRILL_SHAPE_CIRCLE)
        pad.SetDrillSize(pcbnew.VECTOR2I(mm(DRILL), mm(DRILL)))
        net = net_of.get((part.ref, num))
        if net:
            pad.SetNet(nets[net])
    if worst > 0.2:
        raise SystemExit(f"{part.ref}: pad lands {worst:.2f} mm off its routing point")
    fp.Reference().SetLayer(pcbnew.F_SilkS)
    fp.Reference().SetTextSize(pcbnew.VECTOR2I(mm(1.0), mm(1.0)))
    fp.Reference().SetTextThickness(mm(0.15))
    fp.Value().SetLayer(pcbnew.F_Fab)
    outline = getattr(part, "outline", None)
    if outline:
        rect(fp, pcbnew.F_SilkS, *outline, 0.15)
        x0, y0, x1, y1 = outline
        crt = (x0, y0, x1, y1)
        rect(fp, pcbnew.F_CrtYd, *crt, 0.05)
    return fp


def jumper(board, ref, a, c, net):
    fp = pcbnew.FOOTPRINT(board)
    fp.SetReference(ref)
    fp.SetValue("wire jumper")
    fp.SetPosition(K(*a))
    fp.SetDuplicatePadNumbersAreJumpers(True)
    board.Add(fp)
    for q in (a, c):
        pad = pcbnew.PAD(fp)
        pad.SetNumber("1")
        pad.SetAttribute(pcbnew.PAD_ATTRIB_PTH)
        pad.SetShape(pcbnew.PAD_SHAPE_CIRCLE)
        pad.SetSize(pcbnew.VECTOR2I(mm(PAD), mm(PAD)))
        pad.SetDrillSize(pcbnew.VECTOR2I(mm(DRILL), mm(DRILL)))
        pad.SetLayerSet(pcbnew.PAD.PTHMask())
        pad.SetPosition(K(*q))
        pad.SetNet(net)
        fp.Add(pad)
    seg(fp, pcbnew.F_SilkS, a, c, 0.3)
    horiz = abs(a[1] - c[1]) < 0.01
    mx, my = (a[0] + c[0]) / 2, (a[1] + c[1]) / 2
    fp.Reference().SetPosition(K(mx + (0 if horiz else 1.2), my + (1.1 if horiz else 0)))
    fp.Reference().SetLayer(pcbnew.F_SilkS)
    fp.Reference().SetTextSize(pcbnew.VECTOR2I(mm(0.9), mm(0.9)))
    fp.Reference().SetTextThickness(mm(0.13))
    fp.Value().SetVisible(False)


def runs(seg_pts):
    """half-grid point list -> straight runs [(start, end), ...]"""
    out, start = [], seg_pts[0]
    for i in range(1, len(seg_pts)):
        last = i == len(seg_pts) - 1
        if last or (seg_pts[i + 1][0] - seg_pts[i][0], seg_pts[i + 1][1] - seg_pts[i][1]) != \
                (seg_pts[i][0] - seg_pts[i - 1][0], seg_pts[i][1] - seg_pts[i - 1][1]):
            out.append((start, seg_pts[i]))
            start = seg_pts[i]
    return out


def gnd_fill(board, net):
    zone = pcbnew.ZONE(board)
    zone.SetLayer(pcbnew.B_Cu)
    zone.SetNet(net)
    m = 1.5
    pts = [(m, m), (z.BOARD_W - m, m), (z.BOARD_W - m, z.BOARD_H - m), (m, z.BOARD_H - m)]
    ol = zone.Outline()
    ol.NewOutline()
    for x, y in pts:
        v = K(x, y)
        ol.Append(v.x, v.y)
    zone.SetLocalClearance(mm(CLEAR + 0.1))
    zone.SetMinThickness(mm(0.6))
    zone.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
    zone.SetThermalReliefGap(mm(0.5))
    zone.SetThermalReliefSpokeWidth(mm(0.7))
    zone.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
    zone.SetIsFilled(False)
    board.Add(zone)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())


def build(num: str) -> str:
    b, g, routed, problems = eb.route_board(num)
    if problems:
        raise SystemExit(f"board {num}: refusing to export, {len(problems)} problems: {problems[:5]}")
    key = eb.BUILDERS[num][0]
    name = f"BOARD{num}_{eb.NAMES[key]}"
    board = pcbnew.BOARD()
    board.SetCopperLayerCount(2)
    ds = board.GetDesignSettings()
    ds.m_CopperEdgeClearance = mm(1.0)
    ds.m_MinClearance = mm(0.4)
    ds.m_TrackMinWidth = mm(0.6)
    ds.m_MinThroughDrill = mm(0.8)
    ds.m_HoleClearance = mm(0.4)
    ds.m_HoleToHoleMin = mm(0.5)
    ds.m_ViasMinAnnularWidth = mm(0.3)
    nc = ds.m_NetSettings.GetDefaultNetclass()
    nc.SetClearance(mm(0.45))
    nc.SetTrackWidth(mm(TRACK))

    for a, c in (((0, 0), (z.BOARD_W, 0)), ((z.BOARD_W, 0), (z.BOARD_W, z.BOARD_H)),
                 ((z.BOARD_W, z.BOARD_H), (0, z.BOARD_H)), ((0, z.BOARD_H), (0, 0))):
        seg(board, pcbnew.Edge_Cuts, a, c, 0.15)
    text(board, pcbnew.F_SilkS, z.BOARD_W / 2, z.BOARD_H - 0.9, f"VRITRA {name} — {z.BOARD_W:.0f}x{z.BOARD_H:.0f} — jumpers W# on this side",
         0.9, 0.14)

    nets = {}
    for n in b.nets:
        ni = pcbnew.NETINFO_ITEM(board, n.name)
        board.Add(ni)
        nets[n.name] = ni
    net_of = {(r, p): n.name for n in b.nets for r, p in n.pins}

    for p in b.parts.values():
        if getattr(p, "fp", None):
            place_footprint(board, p, nets, net_of)
        else:                                            # area: cable sensor / keep-clear zone
            rect(board, pcbnew.F_SilkS, *p.body, 0.12)
            text(board, pcbnew.F_SilkS, (p.body[0] + p.body[2]) / 2, (p.body[1] + p.body[3]) / 2, p.label, 0.9, 0.13)

    ntr = 0
    for name_, rr in routed.items():
        w = HEAVY_W if name_ in er.HEAVY else TRACK
        for pts in rr["trails"]:
            for a, c in runs(pts):
                t = pcbnew.PCB_TRACK(board)
                t.SetStart(K(*er.xy(a)))
                t.SetEnd(K(*er.xy(c)))
                t.SetWidth(mm(w))
                t.SetLayer(pcbnew.B_Cu)
                t.SetNet(nets[name_])
                board.Add(t)
                ntr += 1
    # stubs from off-grid pads to their landing point
    for p in b.parts.values():
        for pad, (x, y) in getattr(p, "offgrid", {}).items():
            net = net_of.get((p.ref, pad))
            if not net:
                continue
            t = pcbnew.PCB_TRACK(board)
            t.SetStart(K(x, y))
            t.SetEnd(K(*er.xy(er.P(*p.pins[pad]))))
            t.SetWidth(mm(TRACK))
            t.SetLayer(pcbnew.B_Cu)
            t.SetNet(nets[net])
            board.Add(t)
    nj = 0
    jlist = []
    for name_, rr in routed.items():
        for a, c in rr["links"]:
            nj += 1
            jumper(board, f"W{nj}", er.xy(a), er.xy(c), nets[name_])
            jlist.append((f"W{nj}", name_, er.xy(a), er.xy(c)))
    if "GND" in nets:
        gnd_fill(board, nets["GND"])

    d = os.path.join(OUT, name)
    os.makedirs(d, exist_ok=True)
    stem = os.path.join(d, name)
    pcbnew.SaveBoard(stem + ".kicad_pcb", board)
    pro = stem + ".kicad_pro"
    data = {"meta": {"filename": os.path.basename(pro), "version": 1}}
    if os.path.exists(pro):
        with open(pro, encoding="utf-8") as f:
            data = json.load(f)
    sev = data.setdefault("board", {}).setdefault("design_settings", {}).setdefault("rule_severities", {})
    for k in IGNORE:
        sev[k] = "ignore"
    with open(pro, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
    with open(stem + ".kicad_dru", "w", encoding="utf-8") as f:
        f.write(DRU)
    notes(stem + "_BUILD_NOTES.md", name, b, jlist)
    print(f"[{num}] {stem}.kicad_pcb  {z.BOARD_W:.0f}x{z.BOARD_H:.0f} mm  nets={len(b.nets)} track-runs={ntr} jumpers={nj}",
          flush=True)
    return stem + ".kicad_pcb"


def notes(path, name, b, jlist):
    L = [f"# {name} — etched single-layer PCB, build notes", "",
         f"Board {z.BOARD_W:.0f} × {z.BOARD_H:.0f} mm, copper on the BOTTOM only, parts on the top (through-hole).",
         "Tracks never cross: where two connections must cross, one goes over the top as a WIRE JUMPER.",
         "Copper rules: pads Ø1.8 mm, tracks 0.7 mm (servo power 1.8 mm), every gap ≥ 0.5 mm, GND copper fill.", "",
         "## Wire jumpers (insulated wire on the component side, fit them FIRST — some run under modules)",
         "| jumper | net | from (x, y mm) | to (x, y mm) | length |", "|---|---|---|---|---|"]
    for ref, net, a, c in jlist:
        L.append(f"| {ref} | {net} | ({a[0]:.1f}, {a[1]:.1f}) | ({c[0]:.1f}, {c[1]:.1f}) | "
                 f"{abs(a[0] - c[0]) + abs(a[1] - c[1]):.1f} mm |")
    L += ["", "x from the LEFT edge, y from the BOTTOM edge, looking at the component side.", "",
          "## Parts", "| ref | part | notes |", "|---|---|---|"]
    for p in b.parts.values():
        L.append(f"| {p.ref} | {p.label} | {p.note.replace(chr(10), ' · ')} |")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(L) + "\n")


if __name__ == "__main__":
    for k in sys.argv[1:] or ["1", "2"]:
        build(k)
