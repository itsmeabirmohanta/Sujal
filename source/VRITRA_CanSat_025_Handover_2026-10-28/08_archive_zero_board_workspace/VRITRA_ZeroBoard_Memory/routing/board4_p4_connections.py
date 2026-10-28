"""Connection picture for Board 4 PARACHUTE with the ESP32-P4-Pico (reads the pin map from board4_p4.py).
Run:  python board4_p4_connections.py  -> ../kicad_single/BOARD4_PARACHUTE_P4/BOARD4_P4_CONNECTIONS.png
"""
from __future__ import annotations

import os
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.patches as mp       # noqa: E402
import matplotlib.pyplot as plt       # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import board4_p4 as m                 # noqa: E402

OUT = os.path.join(os.path.dirname(HERE), "kicad_single", "BOARD4_PARACHUTE_PICO")
TEENSY_OLD = {"GNSS_IN": "0", "STEER_L": "2", "STEER_R": "3", "BAY_LID": "4", "PETRI": "5", "ESC": "6",
              "RX2": "7", "TX2": "8", "HALL": "9", "SERVO_X": "10", "RX3": "15", "SDA": "18", "SCL": "19"}
# signal -> (what it reaches on Board 4, colour)
DEST = {
    "GNSS_IN": ("J5 pin 1  (GNSS from MAIN J8)", "#2e7d32"),
    "STEER_L": ("330 Ω R1 → J10 S  (A20CLM LEFT, 8 V)", "#c62828"),
    "STEER_R": ("330 Ω R2 → J11 S  (A20CLM RIGHT, 8 V)", "#d84315"),
    "BAY_LID": ("330 Ω R3 → J12 S  (MG90S parachute bay)", "#6a1b9a"),
    "PETRI": ("330 Ω R4 → J13 S  (MG90S petri dish)", "#ad1457"),
    "ESC": ("330 Ω R5 → J14 S  (ESC signal)", "#4e342e"),
    "TX2": ("1k R7 → J2 pin 1 (to MAIN)  +  1k R8 → J3 pin 1 (to BACKUP)", "#1565c0"),
    "RX2": ("J2 pin 2  (from MAIN TX3)", "#0277bd"),
    "SERVO_X": ("330 Ω R9 → J16 S  (MG90S extra)", "#5d4037"),
    "RX3": ("J3 pin 2  (from BACKUP TX)", "#00838f"),
    "SCL": ("BMP585 SCK + LSM6DSO32 SCL  (board I²C, codec 0x18 shares it)", "#ef6c00"),
    "SDA": ("BMP585 SDI + LSM6DSO32 SDA  (sensors 0x47 / 0x6A)", "#f9a825"),
    "HALL": ("J15 OUT  + 10k R6 to +3V3_B", "#558b2f"),
}
POWER = {"39": ("+5V_LOGIC  ← J8 pin 1 (POWER J7 5 V)", "#d50000"),
         "38": ("GND", "#000000"), "3": ("GND", "#000000")}
NEVER = {"40": "VBUS — leave open (USB only)", "36": "3V3 OUT — NOT to +3V3_B", "37": "3V3_EN — open",
         "30": "RUN — open", "19": "USB D− — open", "20": "USB D+ — open"}


def pin_xy(pin: int) -> tuple[float, float]:
    """Drawing coordinates, P4 seen from the component side, USB-C at the TOP (as in the user's photo)."""
    if pin <= 20:
        return 40.0, 100.0 - (pin - 1) * 4.0
    return 62.0, 100.0 - (40 - pin) * 4.0


def main() -> str:
    a = m.ASSIGN
    used = {m.GPIO_PIN[g]: s for s, g in a.items()}
    fig = plt.figure(figsize=(16.5, 23.4), dpi=110)
    ax = fig.add_axes([0.0, 0.25, 1.0, 0.70])
    ax.set_xlim(-28, 132)
    ax.set_ylim(14, 122)
    ax.set_aspect("equal")
    ax.axis("off")
    fig.suptitle("VRITRA Board 4 PARACHUTE — ESP32-P4-Pico connections\n(replaces the Teensy 4.0)",
                 fontsize=22, fontweight="bold", y=0.995)
    fig.text(0.5, 0.938, "Top view (component side): USB-C at the top, BMP585 + plugs on the left, servos on the right, LSM6DSO32 "
             "below.\nSame circuit as before — only the MCU pin numbers change.", ha="center", fontsize=14)

    k = 4.0 / 2.54
    ytop, ybot = 100 + 4.52 * k, 100 - 19 * 4 - 4.52 * k
    ax.add_patch(mp.FancyBboxPatch((40 - 1.61 * k, ybot), 22 + 3.22 * k, ytop - ybot, boxstyle="round,pad=0.3",
                                   fc="#1b1b1b", ec="k", lw=2))
    ax.add_patch(mp.Rectangle((46.5, ytop - 4), 9, 7, fc="#9e9e9e", ec="k"))
    ax.text(51, ytop - 0.5, "USB-C", ha="center", va="center", fontsize=10)
    ax.text(51, 62, "Waveshare\nESP32-P4-\nPico", ha="center", va="center", color="w", fontsize=14,
            fontweight="bold")

    for pin in range(1, 41):
        x, y = pin_xy(pin)
        p = str(pin)
        name = m.P4_NAMES[p]
        on = p in used or p in POWER
        col = DEST[used[p]][1] if p in used else POWER[p][1] if p in POWER else "#9e9e9e"
        ax.add_patch(mp.Circle((x, y), 1.3, fc="#ffd54f" if on else "#757575", ec=col, lw=2.5, zorder=3))
        ax.text(x, y, p, ha="center", va="center", fontsize=7, fontweight="bold", zorder=4)
        ax.text(x + (2.2 if pin <= 20 else -2.2), y, name.replace("/SCL", " SCL").replace("/SDA", " SDA")
                .replace("/DM", "").replace("/DP", ""), ha="left" if pin <= 20 else "right", va="center",
                fontsize=7.5, color="w", zorder=4)
        if p in NEVER:
            ax.text(x, y, "✕", ha="center", va="center", fontsize=17, color="#ff1744", alpha=0.9, zorder=5)

    def lead(pin, label, sub, col):
        x, y = pin_xy(pin)
        if pin <= 20:
            ax.plot([x - 1.4, 26], [y, y], color=col, lw=3)
            ax.text(25, y + 0.3, label, ha="right", va="bottom", fontsize=12, fontweight="bold", color=col)
            ax.text(25, y - 0.3, sub, ha="right", va="top", fontsize=8.5, color="#212121")
        else:
            ax.plot([x + 1.4, 76], [y, y], color=col, lw=3)
            ax.text(77, y + 0.3, label, ha="left", va="bottom", fontsize=12, fontweight="bold", color=col)
            ax.text(77, y - 0.3, sub, ha="left", va="top", fontsize=8.5, color="#212121")

    for p, s in used.items():
        lead(int(p), f"{s}  ({m.ASSIGN[s]})", DEST[s][0], DEST[s][1])
    lead(39, "+5V_LOGIC → VSYS", "J8 pin 1 (+5 V from POWER J7)", POWER["39"][1])
    lead(38, "GND", "J8 pin 2/4, all plug GNDs", "#000000")
    lead(3, "GND", "sensor GND", "#000000")

    ax.text(-26, 118, "Pin colours", fontsize=12, fontweight="bold")
    ax.add_patch(mp.Circle((-24, 115), 1.2, fc="#ffd54f", ec="k"))
    ax.text(-21.5, 115, "solder to a trail (used)", va="center", fontsize=11)
    ax.add_patch(mp.Circle((-24, 111.5), 1.2, fc="#757575", ec="k"))
    ax.text(-21.5, 111.5, "not connected", va="center", fontsize=11)
    ax.text(-24, 108, "✕", ha="center", va="center", fontsize=17, color="#ff1744")
    ax.text(-21.5, 108, "must stay open", va="center", fontsize=11)

    tx = fig.add_axes([0.03, 0.005, 0.94, 0.25])
    tx.axis("off")
    order = ["SCL", "SDA", "STEER_L", "STEER_R", "BAY_LID", "PETRI", "ESC", "SERVO_X", "TX2", "RX2", "RX3",
             "GNSS_IN", "HALL"]
    rows = [[s, f"pin {m.GPIO_PIN[a[s]]}", a[s], f"pin {TEENSY_OLD[s]}", DEST[s][0]] for s in order]
    rows.append(["+5V_LOGIC", "pin 39", "VSYS", "VIN", "J8 pin 1 — also C3 470 µF, C4 100 nF, J15 +5V"])
    rows.append(["GND", "pins 3, 38", "GND", "GND", "J8 pins 2/4 and the GND trail"])
    rows.append(["never", "36 / 37 / 40 / 30", "3V3 OUT / EN / VBUS / RUN", "3V3",
                 "leave open — sensors stay on +3V3_B from POWER J7"])
    t = tx.table(cellText=rows, colLabels=["signal", "P4 header", "GPIO", "old Teensy", "goes to on Board 4"],
                 loc="upper center", cellLoc="left", colWidths=[0.11, 0.11, 0.16, 0.09, 0.53])
    t.auto_set_font_size(False)
    t.set_fontsize(11)
    t.scale(1, 1.6)
    for (r, c), cell in t.get_celld().items():
        if r == 0:
            cell.set_text_props(fontweight="bold")
            cell.set_facecolor("#e0e0e0")
        elif r - 1 < len(order) and c == 0:
            cell.set_text_props(color=DEST[order[r - 1]][1], fontweight="bold")
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, "BOARD4_P4_CONNECTIONS.png")
    fig.savefig(path, dpi=110)
    return path


if __name__ == "__main__":
    print(main())
