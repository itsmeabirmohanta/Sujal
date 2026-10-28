"""Board 4 PARACHUTE — rectangular zero board around the user's ESP32-P4-Pico build (photo 2026-10-27).

Hole numbering for the USER: C = column counted from the LEFT edge (C1 = first hole column, beside the LSM6DSO32),
R = row counted DOWN from the TOP (R1 = top hole row on the sensor side = the row of P4 pin 1 / GPIO54).
P4-Pico already soldered, upright, USB-C up: left pins (1..20) in C13 R1..R20, right pins (40..21) in C20 R1..R20.
The user removes and re-fits the sensors where this file puts them (shortest runs):
  * BMP585 (7Semi) pins in C6 R13 (VIN) .. R20 (INT), body toward the P4.
  * LSM6DSO32 9-pin row in R23, INT2 C8 .. VIN C16, body hanging down (back row not fitted).
Everything else (plugs, servo headers, resistors, capacitors) is placed here, then every net is routed on the
solder side (trails) with insulated wire LINKS on the component side only where a crossing is unavoidable.
Same circuit and nets as board4_p4.py (Board 4, checked against its hand-written expectation).
Run:  python board4_pico.py [first_seed last_seed]  -> BOARD4_PICO_ROUTED.json
"""
from __future__ import annotations

import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import board4_p4 as m              # noqa: E402
import zb_engine as z              # noqa: E402
import zb_router1 as r1            # noqa: E402

OUT_JSON = os.path.join(HERE, "BOARD4_PICO_ROUTED.json")
NCOL, NROW = 32, 30                # design area: C1..C32 x R1..R30 (81 x 76 mm)
W, H = 2.0 + (NCOL - 1) * 2.54 + 0.54, 2.0 + (NROW - 1) * 2.54 + 0.54


def er(r_user: float) -> float:
    """user row (R1 = top) -> engine row (1 = bottom)"""
    return NROW + 1 - r_user


def hole(c: int, r_user: int) -> tuple[int, int]:
    return c, int(er(r_user))


def box(b, ref, c0, r0, c1, r1, label, color, height, pins, names, note=""):
    """body from hole (c0, r0) to (c1, r1) in user coords, +/- margin in mm"""
    x0, x1 = z.hx(c0) - 1.3, z.hx(c1) + 1.3
    y0, y1 = z.hy(int(er(r1))) - 1.3, z.hy(int(er(r0))) + 1.3
    part = z.Part(ref, label, pins, names, (x0, y0, x1, y1), height, color, note)
    b.parts[ref] = part
    return part


def hdr(b, ref, c, r_user, names, label, direction, jst=False):
    """single-row plug; direction in user terms (down = increasing R)"""
    d = {"right": "right", "left": "left", "down": "down", "up": "up"}[direction]
    return z.place_row(b, ref, c, int(er(r_user)), names, label, direction=d, jst=jst)


def two(b, ref, c1, r1u, c2, r2u, label, kind="R"):
    return z.place_2pin(b, ref, c1, int(er(r1u)), c2, int(er(r2u)), label, kind=kind)


def cap(b, ref, c1, r1u, c2, r2u, label, dia=8.0):
    p = two(b, ref, c1, r1u, c2, r2u, label, kind="CP")
    xc = (z.hx(c1) + z.hx(c2)) / 2
    yc = (z.hy(int(er(r1u))) + z.hy(int(er(r2u)))) / 2
    p.body = (xc - dia / 2, yc - dia / 2, xc + dia / 2, yc + dia / 2)
    return p


PLACE = {   # new parts (user coords). Servo headers: − + S, S nearest the resistor.
    # right of the P4: servo resistors on the row of their pin, headers, 8 V / 5 V servo power at the right edge
    "R1": ("R", (22, 6, 26, 6, "330")), "R2": ("R", (22, 7, 26, 7, "330")),
    "R3": ("R", (22, 9, 26, 9, "330")), "R4": ("R", (22, 10, 26, 10, "330")),
    "R9": ("R", (22, 15, 26, 15, "330")), "R5": ("R", (22, 16, 26, 16, "330")),
    "J10": ("servo", (29, 6, "A20CLM LEFT steer (8 V)")), "J11": ("servo", (29, 7, "A20CLM RIGHT steer (8 V)")),
    "J12": ("servo", (29, 9, "MG90S #1 PARACHUTE BAY")), "J13": ("servo", (29, 10, "MG90S #2 PETRI PLATE")),
    "J16": ("servo", (29, 15, "MG90S #3 EXTRA")), "J14": ("esc", (28, 16, "ESC signal")),
    "J1": ("jst", (31, 4, ["+8V", "+8V", "GND", "GND"], "8 V steer in (POWER J8)", "down")),
    "C1": ("CP", (27, 2, 29, 2, "470µF 8V rail", 6.3)),
    "J6": ("jst", (31, 11, ["+5VS", "GND"], "5 V servo in (POWER J9)", "down")),
    "C2": ("CP", (25, 12, 25, 14, "470µF 5VS rail", 6.3)),
    # left of the P4: MAIN / BACKUP UART and GNSS plugs straight across from their pins
    "R7": ("R", (12, 1, 8, 1, "1k TX2→A")),
    "J2": ("jst", (5, 1, ["TX2→A", "RX2←A", "GND"], "UART from MAIN J4", "down")),
    "R8": ("R", (12, 5, 8, 5, "1k TX→B")),
    "J3": ("jst", (5, 5, ["TX→B", "RX3←B", "GND"], "UART to BACKUP J3", "down")),
    "J5": ("jst", (5, 9, ["GNSS", "GND"], "GNSS from MAIN J8", "down")),
    # under the P4 / bottom left: Hall plug, logic power plug and its capacitors
    "J15": ("jst", (4, 23, ["−", "+5V", "OUT"], "HALL sensor plug", "down")),
    "R6": ("R", (6, 22, 6, 25, "10k HALL↑3V3")),
    "J8": ("jst", (2, 28, ["+5V", "GND", "+3V3", "GND"], "logic PWR (POWER J7)", "right")),
    "C3": ("CP", (19, 26, 19, 28, "470µF logic", 6.3)),
    "C4": ("C", (21, 1, 21, 3, "100nF at VSYS")),
    "C5": ("C", (18, 23, 19, 23, "100nF")),
}


def board4_pico() -> z.Board:
    b = m.board4_p4()                         # nets + pad ids (Board 4 circuit, P4 pin map)
    z.set_board(W, H)
    z.EDGE = 1.0                              # perfboard: holes run right to the edge (no slot-mount margin)
    assert (z.NC, z.NR) == (NCOL, NROW), (z.NC, z.NR)
    b.round = None
    b.rods = []
    b.keep_x = (0.0, W)
    b.title = "Board 4 PARACHUTE — ESP32-P4-Pico zero board (user's build)"
    old = b.parts
    b.parts = {}
    # --- already on the board (photo) ---
    p4 = {str(k): hole(13, k) for k in range(1, 21)} | {str(k): hole(20, 41 - k) for k in range(21, 41)}
    u1 = box(b, "U1", 13, 1, 20, 20, "ESP32-P4-Pico", "#f6c85f", 11.0, p4, dict(m.P4_NAMES),
             "already soldered · USB-C up")
    u1.raised = True
    u1.body = (u1.body[0], u1.body[1] - 1.3 - 0.95, u1.body[2], min(u1.body[3] + 0.5, H))   # 57.3 mm long
    # sensors from the team's own footprints (top view), ROTATED only, never mirrored:
    # LSM6DSO32 rot 180: 9-pin row along R23, VIN at C16 .. INT2 at C8, board body hangs DOWN (back row R28, empty)
    z.place_fp(b, "U6", "LSM6DSO32_hdr", "1", 16, int(er(23)), 180, "LSM6DSO32 (Adafruit)", old["U6"].names, 6.0,
               "#b8e0a6", note="9-pin row in R23, VIN right · back row not fitted")
    b.parts["U6"].raised = True
    # BMP585 (7Semi) rot 270: pin column C6, VIN R13 (top) .. SCK R16 .. SDI R18 .. INT R20, board body to the RIGHT
    z.place_fp(b, "U5", "BMP585_hdr8", "1", 6, int(er(13)), 270, "BMP585 (7Semi)", old["U5"].names, 6.0, "#b8e0a6",
               note="pins in C6, body toward the P4")
    # --- new parts ---
    for ref, (kind, a) in PLACE.items():
        if kind == "R":
            two(b, ref, *a)
        elif kind == "C":
            two(b, ref, *a, kind="C")
        elif kind == "CP":
            c1, r1u, c2, r2u, label, dia = a
            cap(b, ref, c1, r1u, c2, r2u, label, dia)
        elif kind == "servo":
            c, r, label = a
            hdr(b, ref, c, r, ["−", "+", "S"], label, "left")
        elif kind == "esc":
            c, r, label = a
            hdr(b, ref, c, r, ["−", "S"], label, "left")
        else:
            c, r, names, label, direction = a
            hdr(b, ref, c, r, names, label, direction, jst=True)
    missing = set(old) - set(b.parts)
    if missing:
        raise SystemExit(f"not placed: {missing}")
    for ref, p in b.parts.items():
        if set(p.pins) != set(old[ref].pins):
            raise SystemExit(f"{ref}: pads {sorted(p.pins)} != {sorted(old[ref].pins)}")
    return b


TOP_EDGE_OK = {"J2"}   # its JST housing overhangs the top edge by 1.2 mm (pin 1 must sit in the top row R1)


def check(b) -> list[str]:
    msgs = [x for x in m.check(b) if not x.startswith(("bodies overlap", "U1: body", "U6: body", "U5: body", "J"))
            and "disk" not in x and "keep zone" not in x and "rod" not in x]
    fit = z.fit_check(b)
    pre = {"U1"}                 # already soldered
    for f in fit:
        if f.startswith("bodies overlap"):
            a, c = f[len("bodies overlap: "):].split(" and ")
            if {a, c} <= pre:
                continue
        if "leaves the keep zone" in f and f.split(":")[0] in pre | TOP_EDGE_OK:
            continue
        msgs.append(f)
    return msgs


def search(seeds):
    best = None
    for sd in seeds:
        b = board4_pico()
        try:
            res, _ = r1.route1(b, strict=True, seed=sd)
        except RuntimeError as e:
            print(sd, "fail", str(e)[:80], flush=True)
            continue
        routed = r1.split1(res)
        p = r1.verify1(b, routed)
        nl = sum(len(r["links"]) for r in routed.values())
        holes = sum(len(seg) for r in routed.values() for seg in r["trails"])
        print(sd, "links", nl, "holes", holes, "problems", len(p), flush=True)
        if not p and (best is None or (nl, holes) < best[0]):
            best = ((nl, holes), sd, routed)
    return best


if __name__ == "__main__":
    b = board4_pico()
    f = check(b)
    print("fit/netlist:", f or "OK")
    if f:
        raise SystemExit(1)
    a0, a1 = (int(sys.argv[1]), int(sys.argv[2])) if len(sys.argv) > 2 else (0, 4)
    best = search(([None] if a0 == 0 else []) + list(range(a0, a1)))
    if best is None:
        raise SystemExit("no clean routing found")
    (nl, holes), sd, routed = best
    out = OUT_JSON if len(sys.argv) <= 2 else OUT_JSON.replace(".json", f"_{a0}_{a1}.json")
    json.dump({"seed": sd, "links": nl, "holes": holes,
               "routes": {k: {"trails": v["trails"], "links": v["links"]} for k, v in routed.items()}}, open(out, "w"))
    print("BEST", sd, nl, holes, "->", out)
