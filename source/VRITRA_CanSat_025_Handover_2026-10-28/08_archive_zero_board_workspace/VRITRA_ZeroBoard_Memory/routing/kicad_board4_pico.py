"""Board 4 PARACHUTE (ESP32-P4-Pico zero board) -> KiCad 10 project with every trail and wire link.

  "C:\\Program Files\\KiCad\\10.0\\bin\\python.exe" kicad_board4_pico.py

Reads BOARD4_PICO_ROUTED.json (board4_pico.py), refuses to export unless the routing verifies.
Trails on B.Cu (solder side), wire links LKn on the component side. Silkscreen/Fab shows the USER hole numbers:
C from the left edge, R counted DOWN from the top row (P4 pin 1 row).
Output: ../kicad_single/BOARD4_PARACHUTE_PICO/  (.kicad_pro, .kicad_pcb, _BUILD_LIST.md)
"""
from __future__ import annotations

import json
import os
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import board4_pico as q            # noqa: E402
import zb_engine as z              # noqa: E402
import zb_router1 as r1            # noqa: E402
from kicad_export import H, P, mm, rect, text, width   # noqa: E402
from kicad_export1 import link_footprint               # noqa: E402

OUT = os.path.join(os.path.dirname(HERE), "kicad_single", "BOARD4_PARACHUTE_PICO")
NAME = "BOARD4_PARACHUTE_PICO"


def U(h) -> str:
    """engine hole -> user label"""
    return f"C{h[0]}R{int(q.er(h[1]))}"


def runs(seg) -> list[str]:
    out, start = [], seg[0]
    for i in range(1, len(seg)):
        last = i == len(seg) - 1
        turn = not last and (seg[i + 1][0] - seg[i][0], seg[i + 1][1] - seg[i][1]) != (seg[i][0] - seg[i - 1][0],
                                                                                         seg[i][1] - seg[i - 1][1])
        if last or turn:
            out.append(f"{U(start)}→{U(seg[i])}")
            start = seg[i]
    return out


def load_routed():
    data = json.load(open(q.OUT_JSON, encoding="utf-8"))
    return {n: {"trails": [[tuple(h) for h in t] for t in v["trails"]],
                "links": [(tuple(a), tuple(c)) for a, c in v["links"]]} for n, v in data["routes"].items()}


def main() -> str:
    b = q.board4_pico()
    routed = load_routed()
    problems = r1.verify1(b, routed) + q.check(b)
    if problems:
        raise SystemExit(f"refusing to export: {problems[:6]}")
    board = pcbnew.BOARD()
    board.SetCopperLayerCount(2)
    ds = board.GetDesignSettings()
    ds.m_CopperEdgeClearance = mm(0.5)
    ds.m_MinClearance = mm(0.2)
    rect(board, pcbnew.Edge_Cuts, -1.27, -1.27, z.BOARD_W + 1.27, z.BOARD_H + 1.27, 0.15)   # perfboard is larger
    for c in range(1, z.NC + 1):
        for r in range(1, z.NR + 1):
            s = pcbnew.PCB_SHAPE(board)
            s.SetShape(pcbnew.SHAPE_T_CIRCLE)
            s.SetCenter(H((c, r)))
            s.SetEnd(P(z.hx(c) + 0.5, z.hy(r)))
            s.SetLayer(pcbnew.Dwgs_User)
            s.SetWidth(mm(0.1))
            board.Add(s)
    for c in range(1, z.NC + 1):
        text(board, pcbnew.Dwgs_User, z.hx(c), z.BOARD_H + 1.6, str(c), 0.8, 0.1)
    for r in range(1, z.NR + 1):
        text(board, pcbnew.Dwgs_User, -2.2, z.hy(r), f"R{int(q.er(r))}", 0.8, 0.1)
    text(board, pcbnew.Dwgs_User, z.BOARD_W / 2, -4,
         "VRITRA BOARD 4 PARACHUTE — ESP32-P4-Pico zero board — C from the left edge, R down from the top row — "
         "blue = solder trail (bottom), LKn = wire link on the component side", 1.2, 0.18)
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
        fp.Reference().SetPosition(P((x0 + x1) / 2, y1 + 1.0))
        fp.Reference().SetLayer(pcbnew.F_SilkS)
        fp.Reference().SetTextSize(pcbnew.VECTOR2I(mm(1.1), mm(1.1)))
        fp.Value().SetPosition(P((x0 + x1) / 2, (y0 + y1) / 2))
        fp.Value().SetLayer(pcbnew.F_Fab)
        fp.Value().SetTextSize(pcbnew.VECTOR2I(mm(0.9), mm(0.9)))
        rect(fp, pcbnew.F_Fab, x0, y0, x1, y1, 0.1)
        for pad_id, h in p.pins.items():
            pad = pcbnew.PAD(fp)
            pad.SetNumber(pad_id)
            pad.SetAttribute(pcbnew.PAD_ATTRIB_PTH)
            pad.SetShape(pcbnew.PAD_SHAPE_RECT if pad_id == "1" else pcbnew.PAD_SHAPE_CIRCLE)
            pad.SetSize(pcbnew.VECTOR2I(mm(1.8), mm(1.8)))
            pad.SetDrillSize(pcbnew.VECTOR2I(mm(1.0), mm(1.0)))
            pad.SetLayerSet(pcbnew.PAD.PTHMask())
            pad.SetPosition(H(h))
            pad.SetPinFunction(p.names.get(pad_id, pad_id))
            net = net_of.get((p.ref, pad_id))
            if net:
                pad.SetNet(nets[net])
            fp.Add(pad)
            text(fp, pcbnew.F_Fab, z.hx(h[0]) + 0.3, z.hy(h[1]) + 1.2, p.names.get(pad_id, pad_id), 0.5, 0.07)
    nlk = 0
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
        for a, c in rr["links"]:
            nlk += 1
            link_footprint(board, f"LK{nlk}", a, c, nets[name])
    os.makedirs(OUT, exist_ok=True)
    stem = os.path.join(OUT, NAME)
    pcbnew.SaveBoard(stem + ".kicad_pcb", board)
    with open(stem + ".kicad_pro", "w", encoding="utf-8") as f:
        json.dump({"meta": {"filename": NAME + ".kicad_pro", "version": 1},
                   "board": {"design_settings": {"rule_severities": {
                       "silk_over_copper": "ignore", "silk_overlap": "ignore", "silk_edge_clearance": "ignore",
                       "lib_footprint_issues": "ignore", "lib_footprint_mismatch": "ignore",
                       "courtyards_overlap": "ignore", "pth_inside_courtyard": "ignore"}}}}, f, indent=2)
    write_build_list(stem + "_BUILD_LIST.md", b, routed)
    print("wrote", stem + ".kicad_pcb", "links", nlk)
    return stem + ".kicad_pcb"


def write_build_list(path, b, routed):
    a = q.m.ASSIGN
    L = ["# VRITRA Board 4 PARACHUTE — ESP32-P4-Pico zero board — BUILD LIST", "",
         "**Hole numbers:** C = column counted from the LEFT edge of the board (C1 = the first hole column, beside the "
         "LSM6DSO32). R = row counted DOWN from the TOP (R1 = the top hole row = the row of P4 pin 1 / GPIO54).",
         "TRAIL = solder trail / bare tinned wire on the SOLDER side through every hole of the run, 90° turns only.",
         "LINK = insulated wire on the COMPONENT side between the two holes (it jumps over other trails).", "",
         "## 0. Before you start",
         "- The ESP32-P4-Pico STAYS where it is: left pins in **C13 R1..R20** (pin 1 GPIO54 = C13 R1, the top row), "
         "right pins in **C20 R1..R20** (pin 40 VBUS = C20 R1). Count every other hole from these.",
         "- REMOVE the LSM6DSO32, the BMP585 and all temporary header pins / Dupont wires (grey, black, yellow, green).",
         "- Refit the **BMP585** with its 8 pins in **C6 R13..R20**: VIN at the TOP (C6R13), INT at the bottom (C6R20), "
         "its board lying to the RIGHT (toward the P4). Chip side up.",
         "- Refit the **LSM6DSO32** with its 9-pin row in **R23 C8..C16**: INT2 at C8, VIN at C16, its board hanging "
         "DOWN (toward R29). Chip side up. Do NOT fit the 5-pin back row.",
         "- Both are rotations of how they sat before (no flipping): labels stay readable from above.", "",
         "## 1. Firmware pin map (ESP32-P4-Pico)", "", "| signal | P4 pin | GPIO |", "|---|---|---|"]
    for s, g in a.items():
        L.append(f"| {s} | {q.m.GPIO_PIN[g]} | {g} |")
    L += ["| +5V_LOGIC in | 39 | VSYS |", "| GND | 3, 38 | GND |", "",
          "Never connect P4 pins 36 (3V3 OUT), 37 (3V3_EN), 40 (VBUS), 30 (RUN), 19/20 (USB). "
          "GPIO46/47/48 (pins 24/22/21) stay free: they run on the microSD LDO supply.", "",
          "## 2. Parts to fit (hole of every pin)", ""]
    for p in b.parts.values():
        pins = ", ".join(f"{p.names.get(k, k)} {U(h)}" for k, h in p.pins.items())
        L.append(f"- **{p.ref}** {p.label}: {pins}")
    L += ["", "## 3. Every net — trails and links", ""]
    nl = 0
    for n in b.nets:
        rr = routed[n.name]
        pins = ", ".join(f"{ref}.{b.parts[ref].names.get(pad, pad)} ({U(b.parts[ref].pins[pad])})" for ref, pad in n.pins)
        L.append(f"### {n.name}")
        L.append(f"- pins: {pins}")
        for seg in rr["trails"]:
            L.append("- TRAIL: " + " · ".join(runs(seg)))
        for x, y in rr["links"]:
            nl += 1
            L.append(f"- LINK LK{nl}: {U(x)} ↔ {U(y)}")
        L.append("")
    L += ["## 4. Test before plugging anything in",
          "1. Meter on beep, NO power: +5V_LOGIC, +3V3_B, +8V_STEER, +5V_SERVO each to GND → must NOT beep. "
          "+8V_STEER to +5V_SERVO → must NOT beep.",
          "2. Each signal trail to its neighbours → must NOT beep.",
          "3. POWER board only (no P4 / servos): J8 shows 5 V and 3.3 V on the right pins.",
          "4. Then plug the P4-Pico power, then servos one at a time."]
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(L) + "\n")


if __name__ == "__main__":
    main()
