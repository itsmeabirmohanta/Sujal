"""ETCHED single-layer PCBs: Board 1 MAIN, Board 2 BACKUP + SENSOR, Board 4 PARACHUTE.

Max outline 118 x 150 mm (the user's limit); width stays 118 so the boards
still fit the slots, height is cut down to what the parts need.
Nets come from boards.py (the same verified netlists), with these changes:
  * MAIN: J6 (Wire1) and J9 (analog) dropped — the sensor board no longer exists
  * BACKUP: mux bottom row = real pads 21..25 (stub tracks), HDC3020 on JST J5
  * PARACHUTE: every plug is JST-XH (8 V input doubled: 4 pins)
Run:  python etch_boards.py [1 2 4]   -> placement check + preview PNGs
"""
from __future__ import annotations

import os
import sys

import zb_engine as z

z.set_board(118.0, 150.0)          # grid first; the height is cut after placement
FULL_X0, FULL_Y0 = z.X0, z.Y0

import boards                                                       # noqa: E402
from boards import (AMP, BMP, CORE, ICM, L89, LSM, MUX, T40_NAMES, T41_NAMES, W25, C_MCU, C_PWR, C_RF,   # noqa: E402
                    C_SENS)
from etch_place import land_offgrid, place2, place_area, place_hdr, place_jst, place_lib   # noqa: E402

OUT = os.path.dirname(os.path.abspath(__file__))
TOP_MARGIN = 3.0           # mm from the highest body to the cut top edge
KEEP_X = (1.5, 116.5)      # module bodies may overhang the copper margin, not the board
NUMBER = {"A": 1, "B": 2, "E": 3, "D": 4}
NAMES = {"A": "MAIN", "B": "BACKUP_SENSOR", "E": "POWER", "D": "PARACHUTE"}


def _nets(b0, drop_refs=(), drop_nets=(), rename=None):
    """Copy the netlist of a zero-board definition, dropping / renaming pins."""
    rename = rename or {}
    out = []
    for n in b0.nets:
        if n.name in drop_nets:
            continue
        pins = [rename.get(p, p) for p in n.pins if p[0] not in drop_refs]
        pins = [p for p in pins if p is not None]
        out.append(z.Net(n.name, pins, n.kind))
    return out


# ===========================================================================
# BOARD 1 — MAIN (Teensy 4.1 #1)
# ===========================================================================
def board_1() -> z.Board:
    """MAIN on the proven planar layout of the zero board (same holes), etched rules. Full 150 mm height."""
    b = z.Board("A", "BOARD 1 MAIN (etched)", keep_x=KEEP_X, plane_y=None)
    place_lib(b, "U1", "VRITRA:Teensy 4.1_hdr", "0", 21, 40, 270, "Teensy 4.1 MAIN", T41_NAMES, 13.0, C_MCU,
              note="female headers · USB up · microSD down", raised=True)
    place_area(b, "KO1", 50.5, 104.5, 69.5, 118.0, "USB plug keep clear")
    place_area(b, "KO2", 50.5, 30.0, 69.5, 38.5, "microSD pull-out")
    place_lib(b, "U2", "VRITRA:Core1121xf Lora", "8", 8, 12, 90, "Core1121-HF", CORE, 6.0, C_RF,
              note="label side DOWN · u.FL → antenna", raised=True)
    place_lib(b, "U3", "VRITRA:L89_hdr", "1", 9, 55, 90, "L89HA GNSS", L89, 10.0, C_RF, raised=True,
              note="7Semi board · antennas on it · 9600 Bd")
    xc = (z.hx(32) + z.hx(35)) / 2
    place_hdr(b, "U4", 32, 56, ["SCL", "SDA", "VCC", "GND"], "DS3231 (flat on 4-pin end)", direction="right",
              color=C_SENS, height=9.5, body=(xc - 11.0, z.hy(56) - 36.0, xc + 11.0, z.hy(56) + 2.0),
              note="REMOVE charge resistor · 0x68")
    place_lib(b, "U5", "VRITRA:BMP585_hdr8", "1", 31, 36, 0, "BMP585 #1", BMP, 6.0, C_SENS, raised=True,
              note="0x47 (SDO→3V3)")
    place_lib(b, "U6", "VRITRA:ICM20948_7Semi_hdr12", "1", 45, 43, 90, "ICM-20948 #1", ICM, 6.0, C_SENS, raised=True,
              note="CS+SDO → 1V8 · 0x69")
    place_lib(b, "U7", "VRITRA:W25Q64_hdr6", "1", 14, 9, 180, "W25Q64 flash", W25, 6.0, C_SENS, raised=True)
    place_lib(b, "U9", "VRITRA:MAX98357A_hdr7", "1", 32, 10, 0, "MAX98357A amp", AMP, 9.0, C_PWR, raised=True,
              note="speaker on its screw terminal")
    place_jst(b, "J8", 45, 32, ["GNSS", "GND"], "GNSS to PARACHUTE J5", direction="down")
    place_jst(b, "J4", 45, 28, ["RX3", "TX3", "GND"], "UART to PARACHUTE J2", direction="down")
    place_jst(b, "J5", 3, 24, ["HB", "GNSS", "MUX_TX", "MUX_RX", "GND"], "LINK to BACKUP J2", direction="down")
    place_jst(b, "J2", 27, 52, ["SCL", "SDA", "GND"], "I²C to POWER J2", direction="down")
    place_jst(b, "J3", 43, 57, ["VBAT", "GND"], "VBAT from POWER J10")
    place_jst(b, "J1", 20, 52, ["+5V", "GND", "+3V3", "GND"], "PWR from POWER J5")
    place_jst(b, "J7", 3, 47, ["CELL+", "GND"], "GNSS backup cell", direction="down")
    place2(b, "R1", "R", 17, 9, "up", "10k LORA_CS↑")
    place2(b, "R2", "R", 17, 3, "up", "10k FLASH_CS↑")
    place2(b, "R3", "R", 10, 33, "right", "1k HB")
    place2(b, "R4", "R", 37, 29, "right", "1k TX3")
    place2(b, "R5", "R", 10, 37, "right", "1k GNSS→D")
    place2(b, "R6", "R", 10, 35, "right", "1k GNSS→B")
    place2(b, "R7", "R", 39, 54, "right", "100k VBAT")
    place2(b, "R8", "R", 39, 52, "right", "22k VBAT")
    place2(b, "R9", "R", 22, 7, "right", "470 LED OK")
    place2(b, "R10", "R", 22, 5, "right", "470 LED FLT")
    place2(b, "R11", "R", 10, 31, "right", "1k CAM TX")
    place2(b, "R12", "R", 39, 4, "right", "100k GAIN 15dB")
    place2(b, "C6", "CP6", 40, 8, "right", "100µF amp")
    place2(b, "D1", "LED_G", 27, 7, "right", "LED green")
    place2(b, "D2", "LED_R", 27, 5, "right", "LED red")
    place2(b, "D3", "D", 4, 51, "up", "1N5817 3V3→BAT")
    place2(b, "D4", "D", 2, 51, "up", "1N5817 cell→BAT")
    place2(b, "C1", "CP8", 21, 56, "right", "470µF")
    place2(b, "C2", "C", 25, 55, "right", "100nF")
    place2(b, "C3", "C", 6, 18, "up", "100nF")
    place2(b, "C4", "C", 31, 33, "right", "100nF")
    place2(b, "C5", "C", 40, 40, "right", "100nF")
    b.nets = _nets(boards.board_a(), drop_refs=("J6", "J9"), drop_nets=("SDA1", "SCL1", "ENV_UV", "ENV_O3"))
    return b


EXPECT_1 = {k: v for k, v in boards.EXPECT_A.items() if k not in ("16", "17", "22", "23")}


# ===========================================================================
# BOARD 2 — BACKUP + SENSOR (Teensy 4.1 #2, mux, microSD, BMP585 #2, LSM6DSO32, HDC3020)
# ===========================================================================
def board_2() -> z.Board:
    b = z.Board("B", "BOARD 2 BACKUP + SENSOR (etched)", keep_x=KEEP_X, plane_y=None)
    # Teensy: left col 23 = 0 GNSS, 4 HB, 5/6/9 mux A/B/C, 10/11/12 SD; right col 29 = I²C, UART3, SD SCK, LEDs, Serial8
    place_lib(b, "U1", "VRITRA:Teensy 4.1_hdr", "0", 23, 33, 270, "Teensy 4.1 BACKUP", T41_NAMES, 13.0, C_MCU,
              note="female headers · USB up", raised=True)
    place_area(b, "KO1", 58.0, 19.5, 69.0, 24.7, "microSD card access")
    # LEFT: link plug, 7Semi mux (lying sideways, COM pins toward the Teensy), microSD module, camera plug
    place_jst(b, "J2", 3, 33, ["HB", "GNSS", "MUX_TX", "MUX_RX", "GND"], "LINK from MAIN J5", direction="down")
    place_lib(b, "U10", "VRITRA:UART_MUX_7SEMI_25", "1", 8, 27, 90, "7Semi UART mux", MUX, 8.0, C_PWR, raised=True,
              note="DIP OFF · VCC jumper 3V3 · ch1 MAIN, ch2 BACKUP")
    sd_c, sd_r = 5, 20
    xc = z.hx(sd_c) + 6.35
    place_hdr(b, "U11", sd_c, sd_r, ["CS", "SCK", "MOSI", "MISO", "VCC", "GND"], "microSD card module", direction="right",
              color=C_SENS, height=8.0, body=(xc - 12.0, z.hy(sd_r) - 42.0, xc + 12.0, z.hy(sd_r) + 2.0),
              note="flat on its header · card slot DOWN · VCC 5 V")
    place_jst(b, "J4", 31, 4, ["+5V", "GND", "to D7", "from D6"], "CAMERA plug (XIAO)")
    place2(b, "R9", "R", 37, 8, "right", "1k cam RX")
    place2(b, "R10", "R", 37, 6, "right", "1k cam TX")
    place2(b, "R2", "R", 14, 24, "right", "1k TX8")
    place2(b, "R3", "R", 14, 22, "right", "1k RX8")
    place2(b, "R6", "R", 19, 18, "down", "10k MUX_A↓")
    place2(b, "R7", "R", 21, 18, "down", "10k MUX_B↓")
    place2(b, "R8", "R", 17, 18, "down", "10k MUX_C↓")
    # RIGHT: power, sensors, HDC3020 plug + its spot, UART to PARACHUTE, LEDs
    place_jst(b, "J1", 32, 33, ["+5V", "GND", "+3V3", "GND"], "PWR from POWER J6")
    place2(b, "C1", "CP8", 41, 21, "right", "470µF")
    place2(b, "C2", "C", 38, 33, "right", "100nF")
    place_lib(b, "U5", "VRITRA:BMP585_hdr8", "1", 31, 24, 0, "BMP585 #2", BMP, 6.0, C_SENS, raised=True,
              note="0x47 (SDO→3V3)")
    place_lib(b, "U8", "VRITRA:LSM6DSO32_hdr", "1", 31, 15, 0, "LSM6DSO32", LSM, 6.0, C_SENS, raised=True,
              note="CS→3V3 · DO→GND = 0x6A · solder FRONT row only")
    place_jst(b, "J5", 44, 34, ["3V3", "GND", "SDA", "SCL"], "HDC3020 plug (+ − D C)", direction="down")
    place_area(b, "M1", 99.5, 59.5, 116.0, 73.0, "HDC3020 module spot", C_SENS, note="double-sided tape · 0x44")
    place2(b, "C3", "C", 38, 31, "right", "100nF")
    place2(b, "C4", "C", 39, 12, "right", "100nF")
    place_jst(b, "J3", 43, 17, ["RX3", "TX3", "GND"], "UART to PARACHUTE J3", direction="down")
    place2(b, "R1", "R", 39, 10, "right", "1k TX3")
    place2(b, "R4", "R", 31, 10, "right", "470 LED OK")
    place2(b, "R5", "R", 31, 8, "right", "470 LED FLT")
    place2(b, "D1", "LED_G", 42, 6, "right", "LED green")
    place2(b, "D2", "LED_R", 45, 4, "left", "LED red")
    ren = {("U10B", "1"): ("U10", "21"), ("U10B", "2"): ("U10", "22"), ("U10B", "3"): ("U10", "23"),
           ("U10B", "4"): ("U10", "24"), ("U10B", "5"): ("U10", "25")}
    ren.update({("M1L", k): ("J5", k) for k in "1234"})
    b.nets = _nets(boards.board_b(), rename=ren)
    return b


# ===========================================================================
# BOARD 4 — PARACHUTE (Teensy 4.0): every plug JST-XH
# ===========================================================================
def board_4() -> z.Board:
    b = z.Board("D", "BOARD 4 PARACHUTE (etched)", keep_x=KEEP_X, plane_y=None)
    place_lib(b, "U1", "VRITRA:Teensy_4.0_hdr_Robu", "1", 17, 27, 270, "Teensy 4.0 PARACHUTE", T40_NAMES, 13.0, C_MCU,
              note="female headers · USB up", pins_used={str(i) for i in range(1, 29)}, raised=True)
    # LEFT edge: one column of plugs, − on column 3, + on column 4, signal on column 5
    place_jst(b, "J1", 6, 30, ["+8V", "+8V", "GND", "GND"], "8 V STEER in (POWER J8)", direction="left")
    place_jst(b, "J10", 3, 27, ["−", "+", "S"], "A20CLM LEFT")
    place_jst(b, "J11", 3, 24, ["−", "+", "S"], "A20CLM RIGHT")
    place_jst(b, "J6", 4, 20, ["+5VS", "GND"], "5 V SERVO in (POWER J9)", direction="left")
    place_jst(b, "J12", 3, 16, ["−", "+", "S"], "MG90S #1 BAY LID")
    place_jst(b, "J13", 3, 13, ["−", "+", "S"], "MG90S #2 PETRI")
    place_jst(b, "J14", 3, 10, ["−", "S"], "ESC signal (2 thin wires)")
    place_jst(b, "J15", 3, 7, ["−", "+5V", "OUT"], "HALL sensor")
    for i, row in enumerate((27, 24, 16, 13, 10)):
        place2(b, f"R{i + 1}", "R", 11, row, "left", "330")
    place2(b, "R6", "R", 7, 7, "right", "10k HALL↑")
    place2(b, "C1", "CP8", 9, 30, "right", "470µF 8V")
    place2(b, "C2", "CP8", 7, 20, "right", "470µF 5VS")
    # RIGHT: logic power, sensors
    place_jst(b, "J8", 25, 28, ["+5V", "GND", "+3V3", "GND"], "logic PWR (POWER J7)")
    place2(b, "C3", "CP", 32, 28, "right", "470µF")
    place2(b, "C4", "C", 36, 28, "right", "100nF")
    place_lib(b, "U5", "VRITRA:BMP585_hdr8", "1", 27, 20, 0, "BMP585 #3", BMP, 6.0, C_SENS, raised=True,
              note="0x47 · own 600 m baro")
    place_lib(b, "U6", "VRITRA:ICM20948_7Semi_hdr12", "1", 39, 20, 0, "ICM-20948 #2", ICM, 6.0, C_SENS, raised=True,
              note="CS+SDO → 1V8 · 0x69")
    place2(b, "C5", "C", 27, 16, "right", "100nF")
    # BOTTOM: links to MAIN and BACKUP
    place_jst(b, "J5", 22, 9, ["GNSS", "GND"], "GNSS from MAIN J8")
    place_jst(b, "J2", 27, 9, ["TX2→A", "RX2←A", "GND"], "UART from MAIN J4")
    place_jst(b, "J3", 34, 9, ["TX→B", "RX3←B", "GND"], "UART to BACKUP J3")
    place2(b, "R7", "R", 27, 4, "right", "1k TX2→A")
    place2(b, "R8", "R", 34, 4, "right", "1k TX→B")
    b.nets = _nets(boards.board_d())
    return b


BUILDERS = {"1": ("A", board_1, [("U1", EXPECT_1, boards.FORBIDDEN_A)]),
            "2": ("B", board_2, [("U1", boards.EXPECT_B, boards.FORBIDDEN_B)]),
            "4": ("D", board_4, [("U1", boards.EXPECT_D, boards.FORBIDDEN_D)])}


def finish(b: z.Board) -> list[str]:
    """Land off-grid pads, cut the height to the parts, re-grid. -> problems"""
    b.copper_x = (0.0, 118.0)
    msgs = land_offgrid(b)
    top = max(p.body[3] for p in b.parts.values())
    h = min(150.0, round(top + TOP_MARGIN + 0.49))
    z.set_board(118.0, float(h), FULL_X0, FULL_Y0)
    b.height = h
    return msgs


def preview(b: z.Board, path: str):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import Rectangle
    net_of = {(r, p): n.name for n in b.nets for r, p in n.pins}
    fig, ax = plt.subplots(figsize=(11, 11 * z.BOARD_H / 118 + 0.5), dpi=110)
    ax.add_patch(Rectangle((0, 0), z.BOARD_W, z.BOARD_H, fc="#c8e6c9", ec="k"))
    ax.add_patch(Rectangle((3, 3), z.BOARD_W - 6, z.BOARD_H - 6, fc="none", ec="#888", ls=":"))
    for c in range(1, z.NC + 1):
        if c % 5 == 0:
            ax.text(z.hx(c), -2, str(c), ha="center", fontsize=6)
    for r in range(1, z.NR + 1):
        if r % 5 == 0:
            ax.text(-2.5, z.hy(r), str(r), va="center", fontsize=6)
    for p in b.parts.values():
        x0, y0, x1, y1 = p.body
        ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, fc=p.color, ec="k", alpha=0.55, lw=0.8))
        ax.text((x0 + x1) / 2, (y0 + y1) / 2, f"{p.ref}\n{p.label}", ha="center", va="center", fontsize=6)
        for pad, (c, r) in p.pins.items():
            net = net_of.get((p.ref, pad))
            ax.plot(z.hx(c), z.hy(r), "o", ms=4, color="#b8860b" if net else "#999")
            ax.text(z.hx(c) + 0.5, z.hy(r) + 0.5, f"{p.names.get(pad, pad)}" + (f"\n{net}" if net else ""), fontsize=3.6)
        for pad, (x, y) in getattr(p, "offgrid", {}).items():
            ax.plot(x, y, "s", ms=4, color="red")
    for g in getattr(b, "blocked", ()):
        ax.plot(z.hx(g[0]), z.hy(g[1]), "x", ms=3, color="red")
    ax.set_xlim(-5, 123)
    ax.set_ylim(-5, z.BOARD_H + 3)
    ax.set_aspect("equal")
    ax.set_title(f"{b.title}  {z.BOARD_W:.0f} x {z.BOARD_H:.0f} mm")
    fig.savefig(path, bbox_inches="tight")
    plt.close(fig)


def build(num: str):
    key, make, checks = BUILDERS[num]
    z.set_board(118.0, 150.0, FULL_X0, FULL_Y0)
    b = make()
    problems = finish(b) + z.fit_check(b)
    for ref, exp, forb in checks:
        problems += boards.check_pins(b, ref, exp, forb)
    for n in b.nets:
        for ref, pad in n.pins:
            if ref not in b.parts or pad not in b.parts[ref].pins:
                problems.append(f"net {n.name}: {ref}.{pad} has no pad")
    problems += jst_room(b)
    return b, problems


JST_ROOM = 1.5     # mm of free board around every JST-XH header so the cable plug goes in and out


def jst_room(b: z.Board) -> list[str]:
    msgs = []
    for p in b.parts.values():
        if not str(getattr(p, "fp", "")).startswith("Connector_JST"):
            continue
        x0, y0, x1, y1 = p.body
        for q in b.parts.values():
            if q is p or getattr(q, "area", False) and q.ref.startswith("KO") is False:
                continue
            a0, b0, a1, b1 = q.body
            if x0 - JST_ROOM < a1 and a0 < x1 + JST_ROOM and y0 - JST_ROOM < b1 and b0 < y1 + JST_ROOM:
                msgs.append(f"{p.ref}: less than {JST_ROOM} mm free space to {q.ref}")
    return msgs


if __name__ == "__main__":
    for num in sys.argv[1:] or list(BUILDERS):
        b, problems = build(num)
        print(f"[board {num}] {z.BOARD_W:.0f} x {z.BOARD_H:.0f} mm  parts={len(b.parts)} problems={len(problems)}")
        for m in problems:
            print("   !", m)
        preview(b, os.path.join(OUT, f"etch_preview_{num}.png"))



def route_board(num: str, strict=True):
    """Place + route (1.27 mm grid) + verify one board. -> (board, grid, routed, problems)"""
    import etch_router as er
    b, problems = build(num)
    if problems:
        return b, None, None, problems
    results, g = er.route(b, strict=strict)
    routed = er.split(results)
    return b, g, routed, er.verify(b, g, routed)


def draw_route(b, routed, path):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    import etch_router as er
    fig, ax = plt.subplots(figsize=(12, 12 * z.BOARD_H / 118 + 0.5), dpi=120)
    ax.add_patch(plt.Rectangle((0, 0), z.BOARD_W, z.BOARD_H, fc="#f4f4f4", ec="k"))
    for p in b.parts.values():
        x0, y0, x1, y1 = p.body
        ax.add_patch(plt.Rectangle((x0, y0), x1 - x0, y1 - y0, fc=p.color, ec="k", alpha=0.25, lw=0.6))
        ax.text((x0 + x1) / 2, (y0 + y1) / 2, p.ref, ha="center", va="center", fontsize=6)
        for pad, (c, r) in p.pins.items():
            if pad not in getattr(p, "offgrid", {}):
                ax.plot(z.hx(c), z.hy(r), "o", ms=4, color="#b8860b")
        for (x, y) in getattr(p, "offgrid", {}).values():
            ax.plot(x, y, "s", ms=4, color="#b8860b")
    failed = set(getattr(b, "failed", []))
    for name, rr in routed.items():
        col = "red" if name in failed else ("#0b3d91" if name not in ("GND",) else "#333")
        lw = 2.6 if name in er.HEAVY else 1.1
        for seg in rr["trails"]:
            ax.plot([er.xy(q)[0] for q in seg], [er.xy(q)[1] for q in seg], "-", color=col, lw=lw)
        for a, c in rr["links"]:
            ax.plot([er.xy(a)[0], er.xy(c)[0]], [er.xy(a)[1], er.xy(c)[1]], "-", color="#e67e22", lw=2.2)
    ax.set_aspect("equal")
    ax.set_xlim(-1, 119)
    ax.set_ylim(-1, z.BOARD_H + 1)
    nl = sum(len(r["links"]) for r in routed.values())
    ax.set_title(f"{b.title}  links={nl}  failed={sorted(failed)}", fontsize=8)
    fig.savefig(path, bbox_inches="tight")
    plt.close(fig)
