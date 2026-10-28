"""Board 4 PARACHUTE as a real single-layer PCB (user, 2026-10-26).

Same circuit as the zero board EXCEPT:
  * no series resistors on signal pins (R1-R5, R9 servo/ESC, R7/R8 UART): pins wired straight
  * R6 10k Hall pull-up kept (it is not in the signal path; the Hall output needs it)
  * IMU = Adafruit LSM6DSO32 (0x6A)
PCB rules: Ø110 mm round, rod holes Ø8.3 (94 mm circle), component holes 1.4 mm, pads 2.2 mm,
tracks 1.0 mm signal / 1.5 mm power / 2.0 mm servo power, all copper on the bottom.
Routing on the 2.54 mm grid (fewest wire links over a multi-start search).
Run:  python board4_pcb.py [first_seed last_seed]   -> BOARD4_PCB_ROUTED.json
"""
from __future__ import annotations

import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import boards                      # noqa: E402
import zb_engine as z              # noqa: E402
import zb_router1 as r1            # noqa: E402

ROD_D = 8.3
OUT_JSON = os.path.join(HERE, "BOARD4_PCB_ROUTED.json")
SERIES_R = ("R1", "R2", "R3", "R4", "R5", "R9", "R7", "R8")
# signal nets merged once the series resistor is gone: new name <- old names
MERGE = {
    "STEER_L": ("STEER_L", "STEER_L_S"), "STEER_R": ("STEER_R", "STEER_R_S"),
    "BAY_LID": ("BAY_LID", "BAY_LID_S"), "PETRI": ("PETRI", "PETRI_S"),
    "SERVO_X": ("SERVO_X", "SERVO_X_S"), "ESC": ("ESC", "ESC_S"),
    "TX2": ("TX2", "TX2_A", "TX2_B"),
}


def board4_pcb() -> z.Board:
    b = boards.board_d(imu="LSM")                 # placement of the built board, LSM6DSO32 fitted
    b.title = "Board 4 PARACHUTE — Ø110 single-layer PCB (no signal resistors)"
    b.rods = [(x, y, ROD_D) for x, y, _ in b.rods]
    for ref in SERIES_R:
        del b.parts[ref]
    old = {n.name: n for n in b.nets}
    absorbed = {o for olds in MERGE.values() for o in olds}
    nets = []
    for name, olds in MERGE.items():
        pins = [p for o in olds for p in old[o].pins if p[0] not in SERIES_R]
        nets.append(z.Net(name, pins))
    for n in b.nets:
        if n.name in absorbed:
            continue
        nets.append(z.Net(n.name, [p for p in n.pins if p[0] not in SERIES_R], n.kind))
    b.nets = nets
    return b


# independent expectation: every Teensy 4.0 signal pin straight to its plug pin
T40 = lambda n: ("U1", str(n + 2))
EXPECT = {
    "STEER_L": {T40(2), ("J10", "3")}, "STEER_R": {T40(3), ("J11", "3")},
    "BAY_LID": {T40(4), ("J12", "3")}, "PETRI": {T40(5), ("J13", "3")},
    "ESC": {T40(6), ("J14", "2")}, "SERVO_X": {T40(10), ("J16", "3")},
    "RX2": {T40(7), ("J2", "2")}, "TX2": {T40(8), ("J2", "1"), ("J3", "1")},
    "RX3": {T40(15), ("J3", "2")}, "GNSS_IN": {T40(0), ("J5", "1")},
    "HALL": {T40(9), ("J15", "3"), ("R6", "1")},
    "SDA": {T40(18), ("U5", "6"), ("U6", "5")}, "SCL": {T40(19), ("U5", "4"), ("U6", "4")},
}


def check_netlist(b) -> list[str]:
    got = {n.name: set(n.pins) for n in b.nets}
    msgs = [f"{n}: has {sorted(got.get(n, set()))}, expected {sorted(e)}" for n, e in EXPECT.items() if got.get(n) != e]
    seen = {}
    for n in b.nets:
        for p in n.pins:
            if p in seen:
                msgs.append(f"{p} in {seen[p]} and {n.name}")
            seen[p] = n.name
            if p[0] not in b.parts or p[1] not in b.parts[p[0]].pins:
                msgs.append(f"{n.name}: {p} has no pad")
    msgs += boards.check_pins(b, "U1", {k: ({"STEER_L_S": "STEER_L"}.get(v, v)) for k, v in boards.EXPECT_D.items()} |
                              {T40(n)[1]: v for n, v in ((2, "STEER_L"), (3, "STEER_R"), (4, "BAY_LID"), (5, "PETRI"),
                                                          (6, "ESC"), (10, "SERVO_X"), (8, "TX2"))}, boards.FORBIDDEN_D)
    return msgs


def search(seeds):
    best = None
    for sd in seeds:
        b = board4_pcb()
        try:
            res, _ = r1.route1(b, strict=True, seed=sd)
        except RuntimeError as e:
            print(sd, "fail", e, flush=True)
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
    b = board4_pcb()
    f = z.fit_check(b) + check_netlist(b)
    print("fit/netlist:", f or "OK")
    if f:
        raise SystemExit(1)
    a, c = (int(sys.argv[1]), int(sys.argv[2])) if len(sys.argv) > 2 else (0, 40)
    best = search([None] + list(range(a, c)))
    if best is None:
        raise SystemExit("no clean routing found")
    (nl, holes), sd, routed = best
    print("BEST seed", sd, "links", nl, "holes", holes)
    json.dump({"seed": sd, "routes": {k: {"trails": v["trails"], "links": v["links"]} for k, v in routed.items()}},
              open(OUT_JSON, "w"))
    print("wrote", OUT_JSON)
