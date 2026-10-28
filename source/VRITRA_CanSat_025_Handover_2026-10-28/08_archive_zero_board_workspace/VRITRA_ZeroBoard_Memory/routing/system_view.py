"""Whole-CanSat interconnect: every cable between boards and to external parts.

Every board-to-board cable is STRAIGHT-THROUGH (pin n to pin n). The check
below reads the net on each pin from the board definitions and fails if the
two ends of any wire carry different signals, so a connector re-order on one
board can never silently break a cable.
Run:  python system_view.py   -> SYSTEM_interconnect.png + CABLES.md
"""
from __future__ import annotations

import os
import sys

import boards

OUT = os.path.dirname(os.path.abspath(__file__))

# signals that legitimately change name across a cable (series resistor or rail alias)
SAME = [{"VBAT_SENSE", "VBAT"}, {"+5V_MQ", "+5V_LOGIC"}, {"HB_J5", "HB_IN"}, {"GNSS_B", "GNSS_IN"},
        {"GNSS_D", "GNSS_IN"}, {"RX3", "TX2_A"}, {"TX3_J4", "RX2"}, {"RX3", "TX2_B"}, {"TX3_J", "RX3"}]

# (id, end1 board, end1 connector, end2 board, end2 connector, wire, what it carries)
CABLES = [
    ("W1", "E", "J5", "A", "J1", "22 AWG ×4", "power: +5V_LOGIC, GND, +3V3_A, GND"),
    ("W2", "E", "J6", "B", "J1", "22 AWG ×4", "power: +5V_LOGIC, GND, +3V3_B, GND"),
    ("W3", "E", "J7", "D", "J8", "22 AWG ×4", "power: +5V_LOGIC, GND, +3V3_B, GND"),
    ("W4", "E", "J8", "D", "J1", "18 AWG ×2", "+8V_STEER for the A20CLM servos"),
    ("W5", "E", "J9", "D", "J6", "20 AWG ×2", "+5V_SERVO for the MG90S servos"),
    ("W6", "E", "J2", "A", "J2", "26 AWG ×3", "I²C to the three INA260"),
    ("W7", "E", "J10", "A", "J3", "26 AWG ×2", "VBAT to the A pin-20 divider"),
    ("W9", "A", "J5", "B", "J2", "26 AWG ×5", "heartbeat + GNSS copy + camera command UART (MAIN → mux ch1)"),
    ("W10", "A", "J4", "D", "J2", "26 AWG ×3", "Serial3 (A) ↔ Serial2 (D)"),
    ("W11", "A", "J8", "D", "J5", "26 AWG ×2", "GNSS copy to parafoil"),
    ("W12", "B", "J3", "D", "J3", "26 AWG ×3", "Serial3 (B) ↔ D telemetry / commands"),
]

EXTERNAL = [
    ("X1", "E", "J1", "3S Molicel P50B pack (XT60 lead)", "16 AWG", "BAT+ / BAT−"),
    ("X2", "E", "J3", "KCD1 rocker switch on the shell (COM + one ON throw)", "18 AWG", "SW_A / SW_B"),
    ("X3", "E", "F1", "10 A fuse (wire link until fitted)", "18 AWG", "IN / OUT"),
    ("X4", "E", "J4", "Cyclone 45A ESC thick + / − wires (after switch, fuse, INA260 #1) → EMAX 2205 (3 motor wires)", "16-18 AWG", "VBAT / GND"),
    ("X5", "D", "J14", "Cyclone 45A BLHeli_S ESC: its 2 thin wires (ground, signal)", "2-wire", "− / S"),
    ("X6", "D", "J10", "A20CLM LEFT steering servo, 2.54 servo header", "servo lead", "− + S"),
    ("X7", "D", "J11", "A20CLM RIGHT steering servo, 2.54 servo header", "servo lead", "− + S"),
    ("X8", "D", "J12", "MG90S #1 — parachute bay (opens the ram-air bay at 600 m), 2.54 servo header", "servo lead", "− + S"),
    ("X9", "D", "J13", "MG90S #2 — petri plate lid, 2.54 servo header", "servo lead", "− + S"),
    ("X16", "D", "J16", "MG90S #3 — extra (Teensy 4.0 pin 10), 2.54 servo header", "servo lead", "− + S"),
    ("X10", "D", "J15", "Hall sensor on the gyro-wheel mount (magnet on wheel)", "3-wire", "− +5V OUT"),
    ("X11", "B", "J4", "XIAO ESP32-S3 camera via mux: J4.3→XIAO D7(RX), XIAO D6(TX)→J4.4", "4-wire", "5V GND RX TX"),
    ("X15", "A", "U9", "beacon speaker on the MAX98357A screw terminal (+ / −, neither is GND)", "2-wire", "SPK+ SPK−"),
    ("X12", "B", "M1L", "HDC3020 JST-PH cable (+ − D C) soldered to Board 2", "module cable", "3V3 GND SDA SCL"),
    ("X13", "A", "J7", "CR2032/CR1220 holder for GNSS backup", "2-wire", "CELL+ / GND"),
    ("X14", "A", "U2", "RadioMaster T-antenna on the Core1121 u.FL (sub-GHz port)", "u.FL→RP-SMA", "RF"),
]


def build():
    out = {}
    for k, (make, _) in boards.BOARDS.items():
        b = make()
        net_of = {(ref, pad): n.name for n in b.nets for ref, pad in n.pins}
        out[k] = (b, net_of)
    return out


def check(bs) -> list[str]:
    msgs = []
    for cid, b1, j1, b2, j2, _, _ in CABLES:
        p1, p2 = bs[b1][0].parts[j1], bs[b2][0].parts[j2]
        if len(p1.pins) != len(p2.pins):
            msgs.append(f"{cid}: {b1}.{j1} has {len(p1.pins)} pins but {b2}.{j2} has {len(p2.pins)}")
            continue
        for pad in p1.pins:
            n1, n2 = bs[b1][1].get((j1, pad)), bs[b2][1].get((j2, pad))
            if n1 != n2 and {n1, n2} not in SAME:
                msgs.append(f"{cid} pin {pad}: {b1}.{j1}={n1} but {b2}.{j2}={n2}")
    return msgs


def cable_md(bs) -> str:
    L = ["# VRITRA zero-board harness — every cable, pin by pin", "",
         "All board-to-board cables are STRAIGHT-THROUGH: pin 1 to pin 1, pin 2 to pin 2 … "
         "Pin 1 is the square / first pin listed on each board sheet. Verified automatically by `system_view.py`.", ""]
    for cid, b1, j1, b2, j2, wire, what in CABLES:
        p1, p2 = bs[b1][0].parts[j1], bs[b2][0].parts[j2]
        L.append(f"## {cid}  {b1}.{j1} ↔ {b2}.{j2}  ({wire}) — {what}")
        L.append("| pin | " + f"{b1}.{j1} label | net on {b1} | {b2}.{j2} label | net on {b2} |")
        L.append("|---|---|---|---|---|")
        for pad in p1.pins:
            L.append(f"| {pad} | {p1.names[pad]} | {bs[b1][1].get((j1, pad))} | {p2.names[pad]} | {bs[b2][1].get((j2, pad))} |")
        L.append("")
    L.append("## External connections")
    L.append("| id | board.connector | device | wire | pins |")
    L.append("|---|---|---|---|---|")
    for x in EXTERNAL:
        L.append(f"| {x[0]} | {x[1]}.{x[2]} | {x[3]} | {x[4]} | {x[5]} |")
    return "\n".join(L) + "\n"


def render(bs, path):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import FancyBboxPatch
    fig, ax = plt.subplots(figsize=(17, 12.5), dpi=170)
    ax.axis("off")
    ax.set_xlim(0, 170)
    ax.set_ylim(0, 125)
    pos = {"D": (60, 92), "A": (60, 50), "B": (112, 50), "E": (60, 6)}
    W, H = 44, 28
    colors = {"A": "#f6c85f", "B": "#ffe9a8", "C": "#b8e0a6", "D": "#9fd3f5", "E": "#f4a582"}
    titles = {"A": "BOARD 1  MAIN — Teensy 4.1 #1\nCore1121 · L89HA · BMP585 · ICM-20948 · DS3231 · W25Q64 · amp",
              "B": "BOARD 2  BACKUP + SENSOR — Teensy 4.1 #2\nBMP585 · LSM6DSO32 · HDC3020 · mux · microSD logger",
              "D": "BOARD 4  PARACHUTE — Teensy 4.0\nICM-20948 · BMP585 #3 · servos · ESC · Hall",
              "E": "BOARD 3  POWER\nINA260 ×3 · AP64500 ×2 · AP62301 ×2 · 8 V buck"}
    anchor = {}
    for k, (x, y) in pos.items():
        ax.add_patch(FancyBboxPatch((x, y), W, H, boxstyle="round,pad=0.6", fc=colors[k], ec="k", lw=1.4))
        ax.text(x + W / 2, y + H - 2.5, titles[k], ha="center", va="top", fontsize=7.6, fontweight="bold")
        b = bs[k][0]
        js = [p for p in b.parts.values() if p.ref.startswith(("J", "F")) and not p.ref.endswith("L")]
        js.sort(key=lambda p: (len(p.ref), p.ref))
        for i, p in enumerate(js):
            col, row = i % 2, i // 2
            jx, jy = x + 2 + col * (W / 2), y + H - 9.5 - row * 3.1
            ax.text(jx, jy, f"{p.ref}: " + " ".join(p.names[pad] for pad in p.pins), fontsize=4.6, va="center",
                    family="monospace")
            anchor[(k, p.ref)] = (jx + (0 if col == 0 else W / 2 - 2), jy)
    for cid, b1, j1, b2, j2, wire, what in CABLES:
        (x1, y1), (x2, y2) = anchor[(b1, j1)], anchor[(b2, j2)]
        red = "power" in what or "+8V" in what or "+5V" in what
        ax.annotate("", xy=(x2, y2), xytext=(x1, y1),
                    arrowprops=dict(arrowstyle="-", lw=1.6 if red else 1.0, color="#b03a2e" if red else "#1f3b73",
                                    connectionstyle="arc3,rad=0.12", alpha=0.85))
        ax.text((x1 + x2) / 2, (y1 + y2) / 2, cid, fontsize=6, ha="center", va="center", fontweight="bold",
                bbox=dict(fc="white", ec="#888", pad=0.4))
    ext_xy = {"X1": (5, 8), "X2": (5, 16), "X3": (5, 24), "X4": (120, 8), "X5": (120, 100), "X6": (8, 118),
              "X7": (8, 112), "X8": (8, 106), "X9": (8, 100), "X10": (120, 118), "X11": (160, 70),
              "X12": (2, 36), "X16": (8, 94), "X13": (40, 42), "X14": (110, 40), "X15": (110, 30)}
    for xid, bk, jref, dev, wire, pins in EXTERNAL:
        ex, ey = ext_xy[xid]
        ax.text(ex, ey, f"{xid}  {dev}\n({wire}: {pins})", fontsize=5.6, va="center",
                bbox=dict(fc="#f7f7f7", ec="#555", pad=0.5))
        tgt = anchor.get((bk, jref), (pos[bk][0] + W / 2, pos[bk][1] + H / 2))
        ax.annotate("", xy=tgt, xytext=(ex + (14 if ex < 60 else 0), ey),
                    arrowprops=dict(arrowstyle="->", lw=0.8, color="#555", ls="--"))
    ax.text(1, 123.5, "VRITRA CAN-7USAT — complete zero-board interconnect (every cable straight-through, pin n ↔ pin n)",
            fontsize=11, fontweight="bold")
    ax.text(1, 120.5, "Red = power cables (wire gauge in CABLES.md), blue = signals. W-numbers match CABLES.md. "
            "Mechanical: D sits by the parachute bay, Boards 1/2 stand in the middle, E sits by the battery.", fontsize=7)
    fig.savefig(path, bbox_inches="tight")
    plt.close(fig)


if __name__ == "__main__":
    bs = build()
    problems = check(bs)
    for m in problems:
        print("  !", m)
    print(f"cables checked: {len(CABLES)}  problems: {len(problems)}")
    with open(os.path.join(OUT, "CABLES.md"), "w", encoding="utf-8") as f:
        f.write(cable_md(bs))
    render(bs, os.path.join(OUT, "SYSTEM_interconnect.png"))
    sys.exit(1 if problems else 0)
