"""Board 4 PARACHUTE with the ESP32-P4-WIFI6, laid out like the user's photo (2026-10-27) -> KiCad 10 project.

  "C:\\Program Files\\KiCad\\10.0\\bin\\python.exe" kicad_board4_p4.py

Rectangular zero board, 2.54 mm grid, component side, USB-C at the TOP. P4 upright, LSM6DSO32 (top) and BMP585
(below it) on its LEFT, wired to the board's own SCL/SDA (GPIO8/GPIO7). Resistors sit right of the P4 on the row of
the pin they serve; plugs in one column at the right edge. NO trails are drawn: every connection is a KiCad airwire
(ratsnest), so the file shows exactly WHAT connects to WHAT and the user lays the solder trails / wires.
Nets and pad names come from board4_p4.py (same circuit as Board 4, checked against its hand-written expectation).
Output: ../kicad_single/BOARD4_PARACHUTE_P4/
"""
from __future__ import annotations

import json
import os
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import board4_p4 as m             # noqa: E402

OUT = os.path.join(os.path.dirname(HERE), "kicad_single", "BOARD4_PARACHUTE_P4")
NAME = "BOARD4_PARACHUTE_P4"
mm = pcbnew.FromMM
G = 2.54
BOARD_W, BOARD_H = 90.0, 100.0
PX_L, PY0 = 30.0, 20.0                  # P4 left column x, pin 1 / pin 40 y
PX_R = PX_L + 7 * G                     # right column (17.78 mm)
RX0, RX1 = PX_R + 2 * G, PX_R + 6 * G   # resistor holes right of the P4 (4 holes = 10.16 mm body)
JX = 70.0                               # plug column


def p4_y(pin: int) -> float:
    return PY0 + ((pin - 1) if pin <= 20 else (40 - pin)) * G


def row(x, y, n, dx=G, dy=0.0):
    return [(x + i * dx, y + i * dy) for i in range(n)]


def layout() -> dict:
    """ref -> list of pad (x, y) in pad-number order (mm, y down)."""
    P = {"U1": [(PX_L if p <= 20 else PX_R, p4_y(p)) for p in range(1, 41)]}
    lsm_x = PX_L - 3 * G
    # LSM6DSO32: front row top INT2 .. bottom VIN (photo), back row 12.7 mm further left (not soldered)
    P["U6"] = [(lsm_x, PY0 + (9 - i) * G) for i in range(1, 10)] + row(lsm_x - 5 * G, PY0 + 2 * G, 5, 0, G)
    # BMP585 (7Semi): top INT .. bottom VIN, SDI level with the P4 SCL pin (photo)
    P["U5"] = [(PX_L - 2 * G, p4_y(16) - 2 * G + (8 - i) * G) for i in range(1, 9)]
    P["C5"] = [(PX_L - G, PY0 - 3 * G), (PX_L - G, PY0 - 2 * G)]                 # 100 nF at the LSM supply
    # left bottom: GNSS + Hall plugs, Hall pull-up, logic power plug + its capacitors
    P["J5"] = row(6.0, 80.0, 2)
    P["J15"] = row(14.0, 80.0, 3)
    P["R6"] = [(24.0, 77.0), (24.0, 77.0 + 4 * G)]
    P["J8"] = row(6.0, 90.0, 4)
    P["C3"] = [(18.0, 90.0), (18.0 + 2 * G, 90.0)]
    P["C4"] = [(26.0, 90.0), (26.0 + G, 90.0)]
    # series resistors right of the P4, each on the row of the pin it serves
    for ref, sig in (("R1", "STEER_L"), ("R2", "STEER_R"), ("R3", "BAY_LID"), ("R4", "PETRI"), ("R5", "ESC"),
                     ("R9", "SERVO_X"), ("R7", "TX2")):
        y = p4_y(int(m.GPIO_PIN[m.ASSIGN[sig]]))
        P[ref] = [(RX0, y), (RX1, y)]
    y7 = P["R7"][0][1]
    P["R8"] = [(RX0 + G, y7 + 2 * G), (RX1 + G, y7 + 2 * G)]                 # TX2 to BACKUP, below R7
    # plug column (servo headers: − + S left to right, like the old board's − / + / S order)
    servo = [("J10", 12.0), ("J11", 17.08), ("J12", 22.16), ("J13", 27.24), ("J14", 32.32), ("J16", 37.4),
             ("J2", 44.0), ("J3", 52.0)]
    for ref, y in servo:
        P[ref] = row(JX, y, 2 if ref == "J14" else 3)
    P["J1"] = row(JX, 64.0, 4)
    P["C1"] = [(JX, 70.0), (JX + 2 * G, 70.0)]
    P["J6"] = row(JX, 78.0, 2)
    P["C2"] = [(JX, 84.0), (JX + 2 * G, 84.0)]
    return P


def check_layout(b, P) -> list[str]:
    msgs = []
    for ref, part in b.parts.items():
        if ref not in P:
            msgs.append(f"{ref} not placed")
        elif len(P[ref]) != len(part.pins):
            msgs.append(f"{ref}: {len(P[ref])} positions for {len(part.pins)} pads")
    seen = {}
    for ref, pts in P.items():
        for i, (x, y) in enumerate(pts):
            k = (round(x / G * 2), round(y / G * 2))
            if k in seen:
                msgs.append(f"{ref}.{i + 1} on the same hole as {seen[k]}")
            seen[k] = f"{ref}.{i + 1}"
            if not (2 <= x <= BOARD_W - 2 and 2 <= y <= BOARD_H - 2):
                msgs.append(f"{ref}.{i + 1} off the board")
    return msgs


def V(x, y):
    return pcbnew.VECTOR2I(mm(x), mm(y))


def main() -> str:
    b = m.board4_p4()
    errs = m.check(b)
    errs = [e for e in errs if "body" not in e and "disk" not in e and "outside" not in e]   # round-board fit only
    P = layout()
    errs += check_layout(b, P)
    if errs:
        raise SystemExit("refusing to export: " + "; ".join(errs))
    board = pcbnew.BOARD()
    board.SetCopperLayerCount(2)
    for a, c in (((0, 0), (BOARD_W, 0)), ((BOARD_W, 0), (BOARD_W, BOARD_H)), ((BOARD_W, BOARD_H), (0, BOARD_H)),
                 ((0, BOARD_H), (0, 0))):
        s = pcbnew.PCB_SHAPE(board)
        s.SetShape(pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(V(*a))
        s.SetEnd(V(*c))
        s.SetLayer(pcbnew.Edge_Cuts)
        s.SetWidth(mm(0.15))
        board.Add(s)
    nets = {}
    for n in b.nets:
        ni = pcbnew.NETINFO_ITEM(board, n.name)
        board.Add(ni)
        nets[n.name] = ni
    net_of = {(r, p): n.name for n in b.nets for r, p in n.pins}
    for ref, part in b.parts.items():
        pts = P[ref]
        fp = pcbnew.FOOTPRINT(board)
        fp.SetReference(ref)
        fp.SetValue(part.label)
        fp.SetPosition(V(*pts[0]))
        board.Add(fp)
        xs, ys = [p[0] for p in pts], [p[1] for p in pts]
        fp.Reference().SetPosition(V(min(xs), min(ys) - 2.0))
        fp.Reference().SetLayer(pcbnew.F_SilkS)
        fp.Reference().SetTextSize(V(1.1, 1.1))
        fp.Value().SetLayer(pcbnew.F_Fab)
        fp.Value().SetPosition(V(min(xs), max(ys) + 2.0))
        fp.Value().SetTextSize(V(0.8, 0.8))
        for pad_id, (x, y) in zip(part.pins, pts):
            pad = pcbnew.PAD(fp)
            pad.SetNumber(pad_id)
            pad.SetAttribute(pcbnew.PAD_ATTRIB_PTH)
            pad.SetShape(pcbnew.PAD_SHAPE_RECT if pad_id == "1" else pcbnew.PAD_SHAPE_CIRCLE)
            pad.SetSize(V(1.8, 1.8))
            pad.SetDrillSize(V(1.0, 1.0))
            pad.SetLayerSet(pcbnew.PAD.PTHMask())
            pad.SetPosition(V(x, y))
            pad.SetPinFunction(part.names.get(pad_id, pad_id))
            net = net_of.get((ref, pad_id))
            if net:
                pad.SetNet(nets[net])
            fp.Add(pad)
            t = pcbnew.PCB_TEXT(fp)
            t.SetText(part.names.get(pad_id, pad_id))
            t.SetPosition(V(x + 1.3, y - 1.1))
            t.SetLayer(pcbnew.F_Fab)
            t.SetTextSize(V(0.55, 0.55))
            t.SetTextThickness(mm(0.08))
            fp.Add(t)
    os.makedirs(OUT, exist_ok=True)
    stem = os.path.join(OUT, NAME)
    pcbnew.SaveBoard(stem + ".kicad_pcb", board)
    with open(stem + ".kicad_pro", "w", encoding="utf-8") as f:
        json.dump({"meta": {"filename": NAME + ".kicad_pro", "version": 1}}, f, indent=2)
    write_list(stem + "_CONNECTIONS.md", b, P)
    print("wrote", stem + ".kicad_pcb", "nets", len(b.nets), "parts", len(b.parts))
    return stem + ".kicad_pcb"


def write_list(path, b, P):
    L = ["# Board 4 PARACHUTE — ESP32-P4-WIFI6 — every connection (same nets as the KiCad file)", "",
         "Each line = one net: solder a trail or wire joining ALL the pads listed. Pin names as printed on the modules.",
         "P4 = Waveshare ESP32-P4-WIFI6, header pin numbers as in its pinout (pin 1 = GPIO52, top-left, USB-C up).", ""]
    for n in b.nets:
        pads = ", ".join(f"{r}.{b.parts[r].names.get(p, p)}" + (f" (P4 pin {p})" if r == "U1" else "")
                         for r, p in n.pins)
        L.append(f"- **{n.name}**: {pads}")
    L += ["", "Leave open: P4 pins 36 (3V3 OUT), 37 (3V3_EN), 40 (VBUS), 30 (RUN), 19/20 (USB D−/D+), and every "
          "other unlisted P4 pin. LSM back row (SCX, SDX, CS, DO, GND) not soldered."]
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(L) + "\n")


if __name__ == "__main__":
    main()
