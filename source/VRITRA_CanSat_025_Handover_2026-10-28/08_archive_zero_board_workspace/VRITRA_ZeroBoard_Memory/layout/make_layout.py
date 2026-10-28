"""VRITRA zero-board placement sheets (to scale, 1 unit = 1 mm).

Run:  python make_layout.py      -> writes PNGs next to this file.
Every board is drawn from its COMPONENT side, top of the sheet = top of the
CanSat (parachute-bay end). Grey dots = 2.54 mm perfboard holes.
Solid boxes = parts you own. Dashed boxes = reserved space for parts not owned
yet. Module sizes come from the KiCad courtyards in VRITRA_footprints.pretty.
"""
import math
import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Circle

OUT = os.path.dirname(os.path.abspath(__file__))
P = 2.54

C_MCU, C_RF, C_SENS, C_PWR, C_CONN, C_PASS, C_RES = (
    "#f6c85f", "#9fd3f5", "#b8e0a6", "#f4a582", "#d9b3ff", "#e6e6e6", "#ffffff")

# Teensy outer pins, viewed from the component side, USB at the TOP.
T41_LEFT = ["GND", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11",
            "12", "3V3", "24", "25", "26", "27", "28", "29", "30", "31", "32"]
T41_RIGHT = ["VIN", "GND", "3V3", "23", "22", "21", "20", "19", "18", "17", "16",
             "15", "14", "13", "GND", "41", "40", "39", "38", "37", "36", "35",
             "34", "33"]
T40_LEFT = ["GND", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12"]
T40_RIGHT = ["VIN", "GND", "3V3", "23", "22", "21", "20", "19", "18", "17", "16",
             "15", "14", "13"]


FULL_W, FULL_H = 120.0, 150.0     # the real perfboards: 12 cm wide x 15 cm tall


def new_sheet(w, h, title, sub):
    """Draw the full 120 x 150 board. Parts are placed in a centred keep-zone
    w mm wide (0..w in placement coordinates); the strips outside it are
    hatched = trim off so the board fits beside the rods / inside the shell.
    Axis numbers are FULL-board millimetres measured from the board's left
    and bottom edges, so they can be counted as holes on the real board."""
    ox = (FULL_W - w) / 2
    fig, ax = plt.subplots(figsize=(FULL_W / 13.0 + 4.2, FULL_H / 13.0 + 1.6), dpi=150)
    ax.set_xlim(-ox - 30, w + ox + 30)
    ax.set_ylim(-14, FULL_H + 10)
    ax.set_aspect("equal")
    ax.axis("off")
    ax.add_patch(Rectangle((-ox, 0), FULL_W, FULL_H, fc="#2f6e3b", ec="black", lw=1.5, alpha=0.18))
    xs = [i * P - ox for i in range(1, int(FULL_W / P))]
    ys = [j * P for j in range(1, int(FULL_H / P))]
    ax.scatter([x for x in xs for _ in ys], [y for _ in xs for y in ys],
               s=0.6, c="#7a8a7a", zorder=1)
    if ox > 0:
        for x0 in (-ox, w):
            ax.add_patch(Rectangle((x0, 0), ox, FULL_H, fc="#ffdddd", ec="red", lw=0.8,
                                   hatch="///", alpha=0.55, zorder=2))
            ax.text(x0 + ox / 2, FULL_H / 2, f"TRIM {ox:.0f} mm", rotation=90, fontsize=6,
                    color="red", ha="center", va="center", fontweight="bold", zorder=7)
    if h < FULL_H:
        ax.add_patch(Rectangle((-ox, h), FULL_W, FULL_H - h, fc="#ffdddd", ec="red", lw=0.8,
                               hatch="///", alpha=0.55, zorder=2))
        ax.text(w / 2, (h + FULL_H) / 2, "optional trim (unused)", fontsize=6, color="red",
                ha="center", va="center", fontweight="bold", zorder=7)
    ax.set_title(title, fontsize=11, fontweight="bold", loc="left")
    ax.text(-ox, -6, sub, fontsize=6.3, va="top", wrap=True)
    for y in range(0, int(FULL_H) + 1, 10):
        ax.plot([-ox - 2, -ox], [y, y], c="k", lw=0.5)
        ax.text(-ox - 3, y, str(y), fontsize=4.5, ha="right", va="center")
    for x in range(0, int(FULL_W) + 1, 10):
        ax.plot([x - ox, x - ox], [-2, 0], c="k", lw=0.5)
        ax.text(x - ox, -3.2, str(x), fontsize=4.5, ha="center", va="top")
    return fig, ax


def box(ax, x, y, w, h, name, fc, note="", dashed=False, fs=5.5):
    ax.add_patch(Rectangle((x, y), w, h, fc=C_RES if dashed else fc, ec="black",
                           lw=0.9, ls="--" if dashed else "-", zorder=3,
                           alpha=0.95))
    ax.text(x + w / 2, y + h / 2 + (1.6 if note else 0), name, fontsize=fs,
            ha="center", va="center", fontweight="bold", zorder=4)
    if note:
        ax.text(x + w / 2, y + h / 2 - 2.2, note, fontsize=fs - 1.4, ha="center",
                va="center", zorder=4, linespacing=1.05)


def header(ax, x, y, labels, vertical=True, side="right", fs=4.3, color="#5b2a86"):
    """1-row header, pin 1 = first label, drawn top-down (vertical) or left-right."""
    for i, lab in enumerate(labels):
        px, py = (x, y - i * P) if vertical else (x + i * P, y)
        ax.add_patch(Rectangle((px - 1.1, py - 1.1), 2.2, 2.2,
                               fc=color if i == 0 else "#c9a8e8", ec="k", lw=0.4, zorder=5))
        if vertical:
            dx, ha = (2.0, "left") if side == "right" else (-2.0, "right")
            ax.text(px + dx, py, lab, fontsize=fs, ha=ha, va="center", zorder=6)
        else:
            ax.text(px, py + 2.0, lab, fontsize=fs, ha="center", va="bottom",
                    rotation=90, zorder=6)


def teensy(ax, x, y, model, nets, label):
    """x,y = bottom-left of the Teensy body, USB at the top."""
    left, right = (T41_LEFT, T41_RIGHT) if model == "4.1" else (T40_LEFT, T40_RIGHT)
    n = len(left)
    w, h = 17.8, (61.0 if model == "4.1" else 35.6)
    box(ax, x, y, w, h, label, C_MCU, f"Teensy {model}\nUSB at top", fs=5.2)
    ax.add_patch(Rectangle((x + w / 2 - 4, y + h - 1), 8, 3, fc="#999", ec="k", lw=0.5, zorder=4))
    if model == "4.1":
        ax.add_patch(Rectangle((x + w / 2 - 6, y - 2.5), 12, 4, fc="#bbb", ec="k", lw=0.5, zorder=4))
        ax.text(x + w / 2, y - 4.2, "microSD", fontsize=3.8, ha="center", va="top")
    y0 = y + h - 1.27 - (h - (n - 1) * P - 2.54) / 2
    for i, pin in enumerate(left):
        py = y0 - i * P
        ax.add_patch(Circle((x + 1.27, py), 0.55, fc="k", zorder=5))
        net = nets.get(pin, "")
        if net:
            ax.text(x - 0.8, py, f"{net}  [{pin}]", fontsize=3.7, ha="right",
                    va="center", zorder=6, color="#003a8c" if pin not in ("GND", "3V3") else "k")
    for i, pin in enumerate(right):
        py = y0 - i * P
        ax.add_patch(Circle((x + w - 1.27, py), 0.55, fc="k", zorder=5))
        net = nets.get("R" + pin if pin in ("GND", "3V3") else pin, nets.get(pin, ""))
        if net:
            ax.text(x + w + 0.8, py, f"[{pin}]  {net}", fontsize=3.7, ha="left",
                    va="center", zorder=6, color="#003a8c" if pin not in ("GND", "3V3", "VIN") else "k")


def save(fig, name):
    path = os.path.join(OUT, name)
    fig.savefig(path, bbox_inches="tight")
    plt.close(fig)
    print("wrote", path)


# --------------------------------------------------------------------------
# 0. CROSS-SECTION (top view)
# --------------------------------------------------------------------------
def cross_section():
    fig, ax = plt.subplots(figsize=(8.6, 8.6), dpi=150)
    ax.set_aspect("equal"); ax.axis("off")
    ax.set_xlim(-72, 72); ax.set_ylim(-78, 72)
    ax.add_patch(Circle((0, 0), 65, fc="#ffd6ef", ec="#c2187a", lw=1.5))
    ax.add_patch(Circle((0, 0), 62, fc="white", ec="#c2187a", lw=1))
    ax.add_patch(Circle((0, 0), 60, fc="none", ec="grey", lw=0.8, ls="--"))
    for (rx, ry) in ((47, 0), (-47, 0), (0, 47), (0, -47)):
        ax.add_patch(Circle((rx, ry), 4, fc="#888", ec="k", zorder=5))
        ax.text(rx, ry - 6.5, "rod Ø8", fontsize=6, ha="center")
    def slab(y_plane, comp_dir, width, name, fc, tall=15):
        ax.add_patch(Rectangle((-width / 2, y_plane - 0.8), width, 1.6, fc="#2f6e3b", ec="k", zorder=4))
        if comp_dir > 0:
            ax.add_patch(Rectangle((-width / 2 + 3, y_plane + 0.8), width - 6, tall, fc=fc, ec="k", lw=0.6, alpha=0.6, zorder=3))
            ax.add_patch(Rectangle((-width / 2 + 3, y_plane - 3.8), width - 6, 3, fc="#ddd", ec="k", lw=0.4, ls=":", zorder=3))
            ty = y_plane + 0.8 + tall / 2
        else:
            ax.add_patch(Rectangle((-width / 2 + 3, y_plane - 0.8 - tall), width - 6, tall, fc=fc, ec="k", lw=0.6, alpha=0.6, zorder=3))
            ax.add_patch(Rectangle((-width / 2 + 3, y_plane + 0.8), width - 6, 3, fc="#ddd", ec="k", lw=0.4, ls=":", zorder=3))
            ty = y_plane - 0.8 - tall / 2
        ax.text(0, ty, name, fontsize=7, ha="center", va="center", fontweight="bold", zorder=6)
    slab(24, +1, 100, "A  PRIMARY  (100 wide, parts face OUT)", C_MCU)
    slab(0, +1, 84, "B  BACKUP  (84 wide, between rods)", "#ffe9a8")
    slab(-12, -1, 100, "C  ENV / PAYLOAD  (100 wide, parts face OUT)", C_SENS)
    ax.annotate("", xy=(55, 24), xytext=(55, 0), arrowprops=dict(arrowstyle="<->", lw=0.6))
    ax.text(57, 12, "24", fontsize=6)
    ax.annotate("", xy=(55, 0), xytext=(55, -12), arrowprops=dict(arrowstyle="<->", lw=0.6))
    ax.text(57, -6, "12", fontsize=6)
    ax.text(0, 66.5, "shell Ø130 OD / Ø124 ID   ·   grey dashed = Ø120 usable (2 mm clearance, ASSUMED)",
            fontsize=6.5, ha="center")
    ax.text(0, -70, "TOP VIEW (looking down the can axis). Boards stand on edge, 150 mm along the axis.\n"
            "Coloured block = 15 mm tallest-part envelope; dotted grey = 3 mm wiring side.\n"
            "A: tall parts must stay inside the central 88 mm (shell curvature). B: rods at x=±47 are its side rails.\n"
            "Board D (parafoil, Teensy 4.0) sits by the parachute bay; board E (power) sits by the battery.",
            fontsize=6.2, ha="center", va="top")
    ax.set_title("VRITRA zero-board stack — cross-section (to scale, mm)", fontsize=10, fontweight="bold")
    save(fig, "00_cross_section.png")


# --------------------------------------------------------------------------
# A. PRIMARY
# --------------------------------------------------------------------------
def board_a():
    W, H = 100, 150
    fig, ax = new_sheet(W, H, "Board A — PRIMARY (Teensy 4.1 #1)  ·  120 × 150 board, parts in central 100 mm  ·  parts face OUT",
                        "Pins follow the working flight firmware vritra_fc (Config.h rev 2026-08) so no code change is needed for the FRR demo. "
                        "New pins (marked *) need firmware: Serial3 link to D, heartbeat to B, Wire1 env bus, W25Q64, LEDs. "
                        "Module pin order from KiCad symbols — confirm against each module's silkscreen before soldering.")
    nets = {"0": "BMP585 INT", "2": "I2S DIN  (beacon, reserved)", "3": "I2S LRCLK (reserved)",
            "4": "I2S BCLK (reserved)", "5": "*HB_OUT →1k→ B pin4", "6": "L89 RST",
            "7": "RX2 ← L89 TX", "8": "TX2 → L89 RX", "24": "LR1121 BUSY", "25": "LR1121 RESET",
            "26": "MOSI1 → LR1121 + W25Q64", "27": "SCK1 → LR1121 + W25Q64", "29": "LR1121 CS",
            "30": "LR1121 DIO9 (IRQ)", "31": "*LED OK (green)", "32": "*LED FAULT (red)",
            "GND": "GND", "3V3": "(do not load)", "VIN": "+5V_LOGIC in",
            "23": "*ENV O3 analog (future)", "22": "DS3231 SQW", "20": "VBAT divider 100k/22k (A6)",
            "19": "SCL  (I²C0)", "18": "SDA  (I²C0)", "17": "*SDA1 → board C", "16": "*SCL1 → board C",
            "15": "*RX3 ←1k← D pin8", "14": "*TX3 →1k→ D pin7", "13": "(LED_BUILTIN)",
            "RGND": "GND", "R3V3": "", "41": "ICM-20948 INT", "40": "*ENV UV analog (future)",
            "39": "MISO1 ← LR1121 + W25Q64", "38": "*W25Q64 CS (10k↑)", "37": "beacon amp SD (reserved)"}
    teensy(ax, 41.1, 4.0, "4.1", nets, "U1  PRIMARY")
    ax.add_patch(Rectangle((40, 66), 20, 13, fc="none", ec="red", ls=":", lw=0.8, zorder=2))
    ax.text(50, 72.5, "keep clear:\nUSB plug", fontsize=4.2, ha="center", color="red")
    box(ax, 2.5, 68.5, 22, 18, "W25Q64", C_SENS, "3V3 CS MISO SCK MOSI GND\nCS → pin 38, 10k pull-up")
    box(ax, 72, 66, 18.3, 19.9, "MAX98357A", C_PWR, "beacon amp\nNOT OWNED\n(rule 5.2.v)", dashed=True)
    box(ax, 2.5, 91, 38, 22, "DS3231 (MH ZS-042)", C_SENS,
        "32K SQW SCL SDA VCC GND\nVCC=3V3_A · SQW→22 · 0x68 + EEPROM 0x57\nREMOVE charge resistor (CR2032)")
    box(ax, 45, 93, 21, 17, "ICM-20948 #1", C_SENS, "VIN 1V8 GND SCL SDA INT\nCS SDO → 1V8 (0x69)\nINT → 41")
    box(ax, 72, 94, 21, 16, "BMP585 #1", C_SENS, "VIN 3V3 GND SCK SDO SDI CS INT\nCS→3V3 · SDO→3V3 = 0x47\nINT → 0")
    box(ax, 2.5, 122, 19, 22, "Core1121-HF", C_RF, "LR1121 868 MHz\nu.FL LORA_ANT\n→ T-antenna")
    ax.text(12, 146, "top row: LORA_ANT GND DIO9 DIO8 DIO7 NC GND 3V3\nbottom: 2.4G_ANT GND CS CLK MOSI MISO RESET BUSY",
            fontsize=3.5, ha="center")
    box(ax, 25.5, 117, 50, 30, "L89HA (7Semi)", C_RF,
        "3D_FIX WAKE SDA SCL GEO JAM RST BAT TX RX 3V3 GND\nbuilt-in patch + chip antenna, 9600 baud\nWAKE→3V3 · BAT→3V3 · TX also →1k→ B & D")
    ax.add_patch(Circle((86, 138), 1.6, fc="#2ecc40", ec="k", zorder=5)); ax.text(89, 138, "LED OK\n+470Ω", fontsize=4, va="center")
    ax.add_patch(Circle((86, 129), 1.6, fc="#ff4136", ec="k", zorder=5)); ax.text(89, 129, "LED FLT\n+470Ω", fontsize=4, va="center")
    box(ax, 80, 116, 18, 6, "470µF+0.1µF", C_PASS, "", fs=4)
    header(ax, 4, 62, ["+5V_LOGIC", "GND", "+3V3_A", "GND"], side="right")
    ax.text(3, 64.5, "J1 power from E", fontsize=4.3, fontweight="bold")
    header(ax, 4, 49, ["SDA", "SCL", "GND"], side="right")
    ax.text(3, 51.5, "J2 I²C → E (INA260×3)", fontsize=4.3, fontweight="bold")
    header(ax, 4, 38, ["VBAT_SENSE", "GND"], side="right")
    ax.text(3, 40.5, "J3 VBAT divider in", fontsize=4.3, fontweight="bold")
    header(ax, 4, 30, ["TX3→D", "RX3←D", "GNSS→D", "GND"], side="right")
    ax.text(3, 32.5, "J4 link → D (parafoil)", fontsize=4.3, fontweight="bold")
    header(ax, 4, 17, ["HB→B", "GNSS→B", "GND"], side="right")
    ax.text(3, 19.5, "J5 link → B (backup)", fontsize=4.3, fontweight="bold")
    header(ax, 96, 62, ["SDA1", "SCL1", "UV", "O3", "+3V3_A", "GND"], side="left")
    ax.text(97, 64.5, "J6 env bus → C", fontsize=4.3, fontweight="bold", ha="right")
    box(ax, 80, 4, 17, 34, "resistors", C_PASS,
        "100k+22k divider\n1k ×5 series\n10k CS pull-up\n470Ω ×2 LED", fs=4.6)
    save(fig, "01_board_A_primary.png")


# --------------------------------------------------------------------------
# B. BACKUP
# --------------------------------------------------------------------------
def board_b():
    W, H = 84, 150
    fig, ax = new_sheet(W, H, "Board B — BACKUP (Teensy 4.1 #2)  ·  120 × 150 board trimmed to 84 mm  ·  slides between the x=±47 rods",
                        "Only ONE Core1121 flies, so B cannot transmit: it is an independent logger (own baro + IMU + microSD) "
                        "that watches A's heartbeat and the parafoil telemetry. A 2nd Core1121 (dashed) makes it a real radio failover. "
                        "Everything above y≈100 is empty today — you may cut the board to ~100 mm tall to save mass.")
    nets = {"0": "RX1 ←1k← L89 TX (copy)", "4": "*HB_IN ← A pin5 (pull-down)",
            "14": "TX3 →1k→ D pin15", "15": "RX3 ←1k← D pin8 (copy)",
            "18": "SDA (I²C0)", "19": "SCL (I²C0)", "34": "RX8 ←1k← XIAO D6", "35": "TX8 →1k→ XIAO D7",
            "36": "LED OK", "37": "LED FAULT", "GND": "GND", "RGND": "GND", "VIN": "+5V_LOGIC in",
            "10": "(LoRa #2 CS – future)", "11": "(MOSI – future)", "12": "(MISO – future)", "13": "(SCK – future)"}
    teensy(ax, 33.1, 4.0, "4.1", nets, "U1  BACKUP")
    ax.add_patch(Rectangle((32, 66), 20, 12, fc="none", ec="red", ls=":", lw=0.8))
    ax.text(42, 72, "keep clear:\nUSB plug", fontsize=4.2, ha="center", color="red")
    box(ax, 4, 80, 21, 16, "BMP585 #2", C_SENS, "CS→3V3 · SDO→3V3 = 0x47")
    box(ax, 30, 79, 25.4, 17.8, "LSM6DSO32", C_SENS, "Adafruit, I²C mode\nSDO/SA0→GND = 0x6A\n±32 g")
    box(ax, 60, 80, 19, 22, "Core1121 #2", C_RF, "OPTIONAL BUY\n(real failover)", dashed=True)
    header(ax, 4, 62, ["+5V_LOGIC", "GND", "+3V3_B", "GND"], side="right")
    ax.text(3, 64.5, "J1 power from E", fontsize=4.3, fontweight="bold")
    header(ax, 4, 46, ["HB←A", "GNSS←A", "GND"], side="right")
    ax.text(3, 48.5, "J2 link ← A", fontsize=4.3, fontweight="bold")
    header(ax, 4, 34, ["TX3→D", "RX3←D", "GND"], side="right")
    ax.text(3, 36.5, "J3 link ↔ D", fontsize=4.3, fontweight="bold")
    header(ax, 80, 62, ["+5V_CAM", "GND", "XIAO D6 TX", "XIAO D7 RX"], side="left")
    ax.text(81, 64.5, "J4 XIAO camera cable", fontsize=4.3, fontweight="bold", ha="right")
    ax.add_patch(Circle((70, 40), 1.6, fc="#2ecc40", ec="k")); ax.add_patch(Circle((70, 34), 1.6, fc="#ff4136", ec="k"))
    ax.text(73, 37, "LEDs\n+470Ω", fontsize=4, va="center")
    box(ax, 62, 4, 19, 22, "resistors / caps", C_PASS, "1k ×4 series\n470Ω ×2\n470µF+0.1µF", fs=4.6)
    ax.plot([0, W], [104, 104], c="red", ls="--", lw=0.8)
    ax.text(W / 2, 106, "optional cut line (~104 mm)", fontsize=5, color="red", ha="center")
    save(fig, "02_board_B_backup.png")


# --------------------------------------------------------------------------
# C. ENVIRONMENT / PAYLOAD
# --------------------------------------------------------------------------
def board_c():
    W, H = 100, 150
    fig, ax = new_sheet(W, H, "Board C — ENVIRONMENT / PAYLOAD  ·  120 × 150 board, parts in central 100 mm  ·  parts face OUT",
                        "No processor: all sensors hang off A's Wire1 (pins 17/16) plus two analog lines. Only the HDC3020 is owned today; "
                        "dashed boxes reserve space for the CDR environmental suite. Optical/gas sensors need to see out through the shell perforations. "
                        "MQ-131 heater runs hot (~750 mW) — keep it at the top edge, away from the baro on A.")
    header(ax, 4, 140, ["SDA1", "SCL1", "UV", "O3", "+3V3_A", "GND", "+5V (MQ131)", "GND"], side="right")
    ax.text(3, 142.5, "J1 from A J6 (+5V from E)", fontsize=4.3, fontweight="bold")
    box(ax, 30, 120, 20, 15, "HDC3020", C_SENS, "JST-PH  + - D C\n0x44 · 3V3", fs=5)
    box(ax, 56, 112, 32, 22, "MQ-131", C_RES, "ozone, 5 V heater\nAO via 12k/18k", dashed=True)
    box(ax, 4, 88, 30, 22, "SCD41", C_RES, "CO₂ (I²C 0x62)", dashed=True)
    box(ax, 40, 88, 27, 22, "SEN0290", C_RES, "lightning 0x03", dashed=True)
    box(ax, 72, 88, 25, 18, "AS7343", C_RES, "spectral 0x39", dashed=True)
    box(ax, 4, 66, 31, 11, "BMV080", C_RES, "PM 0x57 ⚠ = DS3231 EEPROM", dashed=True, fs=4.8)
    box(ax, 40, 64, 19, 15, "ML8511", C_RES, "UV analog", dashed=True)
    ax.text(50, 40, "free area\n(cut the board to ~85 mm tall\nif nothing else is added)", fontsize=6, ha="center", color="#555")
    ax.plot([0, W], [60, 60], c="red", ls="--", lw=0.8)
    ax.text(W / 2, 57.5, "optional cut line", fontsize=5, color="red", ha="center")
    save(fig, "03_board_C_environment.png")


# --------------------------------------------------------------------------
# D. PARAFOIL / RECOVERY
# --------------------------------------------------------------------------
def board_d():
    W, H = 100, 150
    fig, ax = new_sheet(W, H, "Board D — PARAFOIL / RECOVERY (Teensy 4.0)  ·  120 × 150 board, parts in central 100 mm",
                        "Independent of A/B. Owns the 600 m release (MG90S #1 opens the ram-air bay), steering (2× A20CLM on 8 V), "
                        "petri lid (MG90S #2), gyro-wheel ESC and the Hall spin sensor (field 14, sent to A). Servo plugs: S = signal, + , −. "
                        "Hall output is pulled up to 3.3 V, NEVER 5 V. The SD module must stay alone on this SPI bus.")
    nets = {"0": "RX1 ← GNSS (L89 copy or M9N TX)", "1": "TX1 → M9N RX (if used)",
            "2": "A20CLM LEFT signal", "3": "A20CLM RIGHT signal", "4": "MG90S #1 BAY LID", "5": "MG90S #2 PETRI LID",
            "6": "ESC signal", "7": "RX2 ←1k← A pin14", "8": "TX2 →1k→ A pin15 & B pin15", "9": "HALL (10k↑3V3)",
            "10": "SD CS", "11": "SD MOSI", "12": "SD MISO", "GND": "GND", "RGND": "GND", "VIN": "+5V_LOGIC in",
            "13": "SD SCK", "15": "RX3 ←1k← B pin14", "18": "SDA (ICM #2, BMP585 #3)", "19": "SCL"}
    teensy(ax, 41.1, 52, "4.0", nets, "U1  PARAFOIL")
    ax.add_patch(Rectangle((40, 88), 20, 11, fc="none", ec="red", ls=":", lw=0.8))
    ax.text(50, 93.5, "USB plug", fontsize=4.2, ha="center", color="red")
    box(ax, 4, 100, 21, 17, "ICM-20948 #2", C_SENS, "CS SDO → 1V8 (0x69)")
    box(ax, 30, 101, 21, 16, "BMP585 #3", C_SENS, "TO BUY (own 600 m baro)", dashed=True)
    box(ax, 56, 101, 40, 16, "NEO-M9N", C_RF, "only if 2nd GNSS antenna", dashed=True)
    box(ax, 56, 4, 42, 24, "microSD module", C_SENS, "pins: CS SCK MOSI MISO VCC GND\nVCC = +5 V (AMS1117 on board)")
    sx = 4
    for i, (nm, rail) in enumerate((("A20CLM L", "8V"), ("A20CLM R", "8V"), ("MG90S #1 bay", "5VS"),
                                    ("MG90S #2 petri", "5VS"), ("ESC", "—"), ("HALL", "5V"))):
        y = 44 - i * 7.5
        header(ax, sx, y, ["S", "+" + rail if rail != "—" else "NC", "−"], vertical=False, fs=3.6)
        ax.text(sx + 9, y, nm, fontsize=4.6, va="center")
    header(ax, 4, 88, ["+8V_STEER", "GND", "+5V_SERVO", "GND", "+5V_LOGIC", "GND"], side="right")
    ax.text(3, 90.5, "J1 power from E (thick wire on 8V)", fontsize=4.3, fontweight="bold")
    box(ax, 4, 52, 14, 14, "470µF", C_PASS, "8 V rail", fs=4.5)
    box(ax, 20, 52, 14, 14, "470µF", C_PASS, "5VS rail", fs=4.5)
    header(ax, 96, 85, ["RX2←A", "TX2→A,B", "RX3←B", "GNSS←A", "GND"], side="left")
    ax.text(97, 87.5, "J2 links A/B", fontsize=4.3, fontweight="bold", ha="right")
    save(fig, "04_board_D_parafoil.png")


# --------------------------------------------------------------------------
# E. POWER
# --------------------------------------------------------------------------
def board_e():
    W, H = 100, 150
    fig, ax = new_sheet(W, H, "Board E — POWER (next to the battery)  ·  120 × 150 board, parts in central 100 mm",
                        "Battery → rocker switch (on the shell) → INA260 #1 → VBAT bus. ESC takes VBAT through XT30. "
                        "Bucks: AP64500 #1 = +5V_LOGIC, AP64500 #2 = +5V_SERVO (MG90S), 8 V multi-output buck = A20CLM only, "
                        "AP62301 #1/#2 = +3V3_A (board A, C) / +3V3_B (B, D sensors), both fed from +5V_LOGIC. "
                        "INA260 addresses below are a PROPOSAL — confirm 7Semi A0/A1 pads and scan. Star ground at the GND bar.")
    box(ax, 4, 4, 16, 8.5, "XT60PW", C_CONN, "BATTERY IN", fs=4.6)
    header(ax, 26, 9, ["SW_IN", "SW_OUT"], vertical=False, fs=4)
    ax.text(34, 9, "→ rocker switch\n(KCD1, shell)", fontsize=4.3, va="center")
    ax.add_patch(Circle((62, 8), 1.6, fc="#ff4136", ec="k")); ax.text(65, 8, "PWR LED + 1k\non VBAT_SW", fontsize=4.2, va="center")
    box(ax, 80, 3, 16, 10, "FUSE", C_RES, "10–15 A\nnot owned", dashed=True, fs=4.5)
    box(ax, 4, 18, 30.5, 31.5, "INA260 #1", C_PWR, "BATTERY / main bus\nIN+ ← fuse, IN− → VBAT\nI²C 0x40 (A0,A1=GND)")
    box(ax, 40, 22, 22, 26, "XT30", C_CONN, "→ ESC\n(VBAT)", fs=5)
    box(ax, 67, 18, 30, 31.5, "INA260 #3", C_PWR, "+5V_LOGIC line\n0x45 (A0,A1=VS)")
    box(ax, 4, 56, 21, 18, "AP64500 #1", C_PWR, "VBAT → +5V_LOGIC\n5 A fixed")
    box(ax, 30, 56, 21, 18, "AP64500 #2", C_PWR, "VBAT → +5V_SERVO\nMG90S ×2")
    box(ax, 56, 56, 41, 24, "8 V multi-output buck", C_PWR, "VBAT → +8V_STEER\nA20CLM ×2 ONLY\n(model / size TBD)")
    box(ax, 4, 80, 18, 16, "AP62301 #1", C_PWR, "+5V_LOGIC → +3V3_A")
    box(ax, 27, 80, 18, 16, "AP62301 #2", C_PWR, "+5V_LOGIC → +3V3_B")
    box(ax, 56, 85, 30.5, 31.5, "INA260 #2", C_PWR, "+8V_STEER line\n0x41 (A0=VS)")
    ax.add_patch(Rectangle((4, 101), 46, 5, fc="#444", ec="k")); ax.text(27, 103.5, "GND STAR BAR", fontsize=5, color="white", ha="center", va="center")
    header(ax, 4, 140, ["+5V_LOGIC", "GND", "+3V3_A", "GND"], side="right"); ax.text(3, 142.5, "→ A J1", fontsize=4.3, fontweight="bold")
    header(ax, 24, 140, ["+5V_LOGIC", "GND", "+3V3_B", "GND"], side="right"); ax.text(23, 142.5, "→ B J1", fontsize=4.3, fontweight="bold")
    header(ax, 44, 140, ["+8V_STEER", "GND", "+5V_SERVO", "GND", "+5V_LOGIC", "GND"], side="right"); ax.text(43, 142.5, "→ D J1 (thick)", fontsize=4.3, fontweight="bold")
    header(ax, 66, 140, ["SDA", "SCL", "GND"], side="right"); ax.text(65, 142.5, "I²C ← A J2", fontsize=4.3, fontweight="bold")
    header(ax, 84, 140, ["VBAT_SENSE", "GND", "+5V_CAM", "GND", "+5V MQ", "GND"], side="right"); ax.text(83, 142.5, "→ A J3 / cam / C", fontsize=4.3, fontweight="bold", ha="left")
    save(fig, "05_board_E_power.png")


# --------------------------------------------------------------------------
# F. HARNESS BLOCK DIAGRAM
# --------------------------------------------------------------------------
def harness():
    fig, ax = plt.subplots(figsize=(11, 6.4), dpi=150)
    ax.axis("off"); ax.set_xlim(0, 110); ax.set_ylim(0, 64)
    B = {"A": (40, 40, "A  PRIMARY\nTeensy 4.1 · LR1121 · L89HA\nBMP585 · ICM-20948 · DS3231 · W25Q64", C_MCU),
         "B": (75, 40, "B  BACKUP\nTeensy 4.1 · BMP585 · LSM6DSO32\nmicroSD logger", "#ffe9a8"),
         "C": (8, 40, "C  ENV / PAYLOAD\nHDC3020 (+ future env)", C_SENS),
         "D": (75, 8, "D  PARAFOIL\nTeensy 4.0 · ICM-20948 · SD\n2×A20CLM · 2×MG90S · ESC · Hall", C_RF),
         "E": (25, 8, "E  POWER\n3S P50B · switch · INA260×3\nAP64500×2 · AP62301×2 · 8 V buck", C_PWR)}
    for k, (x, y, t, c) in B.items():
        ax.add_patch(Rectangle((x, y), 26, 14, fc=c, ec="k", lw=1.2))
        ax.text(x + 13, y + 7, t, fontsize=6.3, ha="center", va="center")
    def link(p, q, label, col="k", lw=1.0):
        ax.annotate("", xy=q, xytext=p, arrowprops=dict(arrowstyle="-", color=col, lw=lw))
        ax.text((p[0] + q[0]) / 2, (p[1] + q[1]) / 2 + 0.8, label, fontsize=5.2, ha="center", color=col,
                bbox=dict(fc="white", ec="none", alpha=0.85, pad=0.4))
    link((66, 47), (75, 47), "HB (5→4) · GNSS copy (L89→B0)")
    link((34, 47), (40, 47), "Wire1 17/16 · UV · O3 · 3V3")
    link((60, 40), (82, 22), "Serial3 A14→D7, D8→A15 · GNSS copy→D0")
    link((88, 40), (88, 22), "D8→B15 · B14→D15")
    link((45, 40), (40, 22), "I²C 18/19 → INA260×3 · VBAT sense → A20", "#b03a2e")
    link((51, 15), (75, 15), "+8V_STEER (thick) · +5V_SERVO · +5V_LOGIC · +3V3_B", "#b03a2e", 1.6)
    link((30, 22), (24, 40), "+5V (MQ-131)", "#b03a2e")
    link((38, 22), (52, 40), "+5V_LOGIC · +3V3_A", "#b03a2e", 1.4)
    link((48, 22), (82, 40), "+5V_LOGIC · +3V3_B", "#b03a2e", 1.4)
    ax.text(1, 61, "Harness overview — black = signal (every cross-board signal has a 1 kΩ at the driving end), red = power. "
            "All grounds return to E's star bar.", fontsize=7, fontweight="bold")
    ax.text(1, 3, "External: T-antenna on A's Core1121 (u.FL LORA_ANT) · XIAO camera cable to B · ESC XT30 from E · "
            "rocker switch + LED wired to E · NavIC antenna: L89 uses its built-in antenna; the external antenna is spare (M9N option).",
            fontsize=6)
    save(fig, "06_harness_overview.png")


if __name__ == "__main__":
    cross_section(); board_a(); board_b(); board_c(); board_d(); board_e(); harness()
