"""Board 4 PARACHUTE on a Ø110 zero board with the Waveshare ESP32-P4-WIFI6 instead of the Teensy 4.0
(user, 2026-10-27: the etched Board 4 shorted and killed its Teensy).

UNCHANGED from the Board 4 zero-board design (boards.board_d, LSM6DSO32 fitted): every plug, servo header,
sensor, capacitor and series resistor stays in the SAME hole, so every cable and servo still plugs in where it did.
CHANGED:
  * U1 = ESP32-P4-WIFI6 (Pico-style 2x20, rows 17.78 mm = 7 holes apart) on female headers, USB-C to the left edge.
    Pin 1 (GPIO52) at C8 R22, pins 1..20 along R22, pins 40..21 along R29 (pin 40 VBUS above pin 1).
  * +5V_LOGIC feeds VSYS (pin 39). The board's own AO3401 ideal diode then blocks back-feed into a USB host,
    so USB may stay plugged in while the CanSat is powered. Board 3V3 (pin 36) is NOT tied to +3V3_B.
  * The P4 is longer than the Teensy, so three parts that sat under it moved: C3 (470 µF logic) to the
    top-left, J15 (Hall plug) + R6 (10k pull-up) above J8. R5 / R7 are 1 hole shorter (same columns).
GPIO choice (ESP32-P4 datasheet v0.7 §2.3.4/§2.3.5 + Waveshare schematic):
  never GPIO24/25 (USB-Serial-JTAG), GPIO26/27 (USB pins), GPIO34-38 (strapping), GPIO7/8 (board I²C to the
  ES8311 codec), GPIO2 only as an INPUT (weak pull-up after reset); every servo/ESC output is a plain pin.
Run:  python board4_p4.py [first_seed last_seed]  -> BOARD4_P4_ROUTED.json
"""
from __future__ import annotations

import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import boards                      # noqa: E402
import zb_engine as z              # noqa: E402
import zb_router1 as r1            # noqa: E402

OUT_JSON = os.path.join(HERE, "BOARD4_P4_ROUTED.json")
P4_C, P4_R, P4_ROWS = 8, 22, 7               # pin 1 hole, rows 7 holes apart (17.78 mm)
P4_PIN1_TO_USB_END, P4_PIN20_TO_ANT_END, P4_EDGE = 4.52, 4.52, 1.61       # P4-Pico 57.30 x 21.00     # Waveshare outline drawing, mm

# Waveshare ESP32-P4-Pico header (schematic U10 "Pico no debug", ESP32-P4-Pico-datasheet.pdf): pin -> name.
# (The user's board is the P4-Pico: photo silk 54/19/GND/18/17/16/15 on the left column, no ESP32-C6 module.)
P4_NAMES = {
    "1": "GPIO54", "2": "GPIO19", "3": "GND", "4": "GPIO18", "5": "GPIO17", "6": "GPIO16", "7": "GPIO15",
    "8": "GND", "9": "GPIO14", "10": "GPIO6", "11": "GPIO5", "12": "GPIO4", "13": "GND", "14": "GPIO3",
    "15": "GPIO2", "16": "GPIO8/SCL", "17": "GPIO7/SDA", "18": "GND", "19": "GPIO24/DM", "20": "GPIO25/DP",
    "21": "GPIO48", "22": "GPIO47", "23": "GND", "24": "GPIO46", "25": "GPIO33", "26": "GPIO32", "27": "GPIO27",
    "28": "GND", "29": "GPIO26", "30": "RUN", "31": "GPIO23", "32": "GPIO22", "33": "GND", "34": "GPIO21",
    "35": "GPIO20", "36": "3V3_OUT", "37": "3V3_EN", "38": "GND", "39": "VSYS", "40": "VBUS",
}
GPIO_PIN = {v.split("/")[0]: k for k, v in P4_NAMES.items() if v.startswith("GPIO")}
# GPIO39..48 run on VDDPST_5 = the chip's LDO channel 4 (microSD supply, set by firmware): never used here.
OUT_OK = {f"GPIO{n}" for n in (54, 19, 18, 17, 16, 15, 14, 6, 5, 4, 3, 20, 21, 22, 23, 32, 33)}
IN_OK = OUT_OK | {"GPIO2"}
I2C_OK = {"GPIO7", "GPIO8"}       # the board's own I²C (ES8311 codec at 0x18 on it): fine for 0x47 / 0x6A sensors
OUTPUTS = {"STEER_L", "STEER_R", "BAY_LID", "PETRI", "ESC", "SERVO_X", "TX2", "SCL", "SDA"}

# signal -> GPIO (the firmware pin map). Chosen so the servo plugs sit straight below their pins.
# Pin map for the user's build (photo 2026-10-27): P4 upright, USB-C at the top, LSM6DSO32 + BMP585 to its LEFT and
# already wired to the board's own SCL/SDA pins (GPIO8 / GPIO7). Every plug signal therefore comes from the RIGHT
# column (pins 21..35), the two inputs GNSS / HALL from the free left pins between the two sensors (pins 11/12).
ASSIGN = {
    "SCL": "GPIO8", "SDA": "GPIO7",
    "STEER_L": "GPIO20", "STEER_R": "GPIO21", "BAY_LID": "GPIO22", "PETRI": "GPIO23",
    "SERVO_X": "GPIO32", "ESC": "GPIO33",
    "TX2": "GPIO54", "RX2": "GPIO19", "RX3": "GPIO16", "GNSS_IN": "GPIO14", "HALL": "GPIO5",
}
POWER_PINS = {"VSYS": "+5V_LOGIC", "GND_A": "GND", "GND_B": "GND"}
P4_GND_USED = ("3", "38")


def p4_hole(pin: int) -> tuple[int, int]:
    if 1 <= pin <= 20:
        return P4_C + pin - 1, P4_R
    return P4_C + 40 - pin, P4_R + P4_ROWS


def place_p4(b: z.Board) -> z.Part:
    pins = {str(p): p4_hole(p) for p in range(1, 41)}
    x0 = z.hx(P4_C) - P4_PIN1_TO_USB_END
    x1 = z.hx(P4_C + 19) + P4_PIN20_TO_ANT_END
    y0, y1 = z.hy(P4_R) - P4_EDGE, z.hy(P4_R + P4_ROWS) + P4_EDGE
    part = z.Part("U1", "ESP32-P4-Pico PARACHUTE", pins, dict(P4_NAMES), (x0, y0, x1, y1), 11.0, boards.C_MCU,
                  "Waveshare ESP32-P4-Pico on 2x20 female headers (8.5 mm) · USB-C to the left edge")
    part.raised = True            # on female headers: low parts and wire links fit underneath
    b.parts["U1"] = part
    return part


MOVED = {   # ref -> new placement (everything else keeps its hole)
    "C3": ("CP", (8, 34, 8, 32, "470µF logic")),
    "J15": ("jst", (12, 36, ["−", "+5V", "OUT"], "HALL sensor plug (sensor on the body)", "right")),
    "R6": ("R", (16, 38, 16, 35, "10k HALL↑3V3")),
    "R5": ("R", (28, 20, 28, 17, "330")),
    "R7": ("R", (31, 21, 31, 18, "1k TX2→A")),
}
UNDER_P4_OK = {"R7"}     # 2.5 mm resistor, its body edge 0.3 mm under the P4 raised 8.5 mm on female headers
# The P4's USB end is 6.92 mm from the left rod centre (rule: 4.1 + 2.9 = 7.0 mm). It sits 8.5 mm up on the
# female headers, above the rod nut, and the nearest header pin is 11.4 mm from the rod: accepted, documented.
ROD_OK = "U1: body within 2.9 mm of the rod hole at (8.0, 55.0)"


def board4_p4(assign: dict | None = None) -> z.Board:
    a = dict(ASSIGN if assign is None else assign)
    b = boards.board_d(imu="LSM")
    b.title = "Board 4 PARACHUTE — Ø110 zero board, ESP32-P4-WIFI6"
    for ref in ("U1", *MOVED):
        del b.parts[ref]
    place_p4(b)
    for ref, (kind, args) in MOVED.items():
        if kind == "CP":
            boards._cp(b, ref, *args)
        elif kind == "jst":
            c, r, names, label, direction = args
            z.place_row(b, ref, c, r, names, label, direction=direction, jst=True)
        else:
            z.place_2pin(b, ref, *args)
    sig_pin = {s: ("U1", GPIO_PIN[g]) for s, g in a.items()}
    nets = []
    for n in b.nets:
        pins = [p for p in n.pins if p[0] != "U1"]
        if n.name in sig_pin:
            pins.append(sig_pin[n.name])
        elif n.name == "GND":
            pins += [("U1", g) for g in P4_GND_USED]
        elif n.name == "+5V_LOGIC":
            pins.append(("U1", "39"))
        nets.append(z.Net(n.name, pins, n.kind))
    b.nets = nets
    b.p4_assign = a
    return b


# independent expectation of the circuit (written out by hand, not derived from the code above)
def expected(a: dict) -> dict:
    U = lambda s: ("U1", GPIO_PIN[a[s]])
    return {
        "GNSS_IN": {U("GNSS_IN"), ("J5", "1")},
        "STEER_L": {U("STEER_L"), ("R1", "1")}, "STEER_L_S": {("R1", "2"), ("J10", "3")},
        "STEER_R": {U("STEER_R"), ("R2", "1")}, "STEER_R_S": {("R2", "2"), ("J11", "3")},
        "BAY_LID": {U("BAY_LID"), ("R3", "1")}, "BAY_LID_S": {("R3", "2"), ("J12", "3")},
        "PETRI": {U("PETRI"), ("R4", "1")}, "PETRI_S": {("R4", "2"), ("J13", "3")},
        "ESC": {U("ESC"), ("R5", "1")}, "ESC_S": {("R5", "2"), ("J14", "2")},
        "SERVO_X": {U("SERVO_X"), ("R9", "1")}, "SERVO_X_S": {("R9", "2"), ("J16", "3")},
        "TX2": {U("TX2"), ("R7", "1"), ("R8", "1")}, "TX2_A": {("R7", "2"), ("J2", "1")},
        "TX2_B": {("R8", "2"), ("J3", "1")},
        "RX2": {U("RX2"), ("J2", "2")}, "RX3": {U("RX3"), ("J3", "2")},
        "HALL": {U("HALL"), ("J15", "3"), ("R6", "1")},
        "SDA": {U("SDA"), ("U5", "6"), ("U6", "5")}, "SCL": {U("SCL"), ("U5", "4"), ("U6", "4")},
        "+5V_LOGIC": {("J8", "1"), ("U1", "39"), ("C3", "1"), ("C4", "1"), ("J15", "2")},
        "+3V3_B": {("J8", "3"), ("U5", "1"), ("U5", "5"), ("U5", "7"), ("U6", "1"), ("U6", "7"), ("R6", "2"),
                   ("C5", "1")},
        "+8V_STEER": {("J1", "1"), ("J1", "2"), ("C1", "1"), ("J10", "2"), ("J11", "2")},
        "+5V_SERVO": {("J6", "1"), ("C2", "1"), ("J12", "2"), ("J13", "2"), ("J16", "2")},
    }


def check(b: z.Board) -> list[str]:
    a = b.p4_assign
    msgs = []
    for s, g in a.items():
        ok = (OUT_OK | I2C_OK) if s in ("SCL", "SDA") else OUT_OK if s in OUTPUTS else IN_OK
        if g not in ok:
            msgs.append(f"{s} on {g}: pin not allowed for this use")
    if len(set(a.values())) != len(a):
        msgs.append("one GPIO used twice")
    got = {n.name: set(n.pins) for n in b.nets}
    for name, e in expected(a).items():
        if got.get(name) != e:
            msgs.append(f"{name}: has {sorted(got.get(name, set()))}, expected {sorted(e)}")
    u1 = {p for n in b.nets for r, p in n.pins if r == "U1"}
    for forbidden in ("36", "37", "30", "40", "19", "20", "27", "29"):
        if forbidden in u1:
            msgs.append(f"U1 pin {forbidden} ({P4_NAMES[forbidden]}) must stay unconnected")
    seen = {}
    for n in b.nets:
        for p in n.pins:
            if p in seen:
                msgs.append(f"{p} in {seen[p]} and {n.name}")
            seen[p] = n.name
            if p[0] not in b.parts or p[1] not in b.parts[p[0]].pins:
                msgs.append(f"{n.name}: {p} has no pad")
    fit = [m for m in z.fit_check(b)
           if not any(m == f"bodies overlap: {x} and {y}" or m == f"bodies overlap: {y} and {x}"
                      for x in ("U1",) for y in UNDER_P4_OK) and m != ROD_OK]
    return msgs + fit


def search(seeds, assign=None):
    best = None
    for sd in seeds:
        b = board4_p4(assign)
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
    b = board4_p4()
    f = check(b)
    print("fit/netlist:", f or "OK")
    if f:
        raise SystemExit(1)
    a0, a1 = (int(sys.argv[1]), int(sys.argv[2])) if len(sys.argv) > 2 else (0, 30)
    best = search([None] + list(range(a0, a1)))
    if best is None:
        raise SystemExit("no clean routing found")
    (nl, holes), sd, routed = best
    print("BEST seed", sd, "links", nl, "holes", holes)
    json.dump({"seed": sd, "assign": b.p4_assign,
               "routes": {k: {"trails": v["trails"], "links": v["links"]} for k, v in routed.items()}},
              open(OUT_JSON, "w"))
    print("wrote", OUT_JSON)
