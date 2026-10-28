"""One picture per board: parts placed + every line, perfboard style, seen from
the COMPONENT side (traces on the solder side are drawn as if you could see
through the board, so left/right match the parts — no mirror confusion).

Built from boards.py + the same router as the verified sheets, so the
picture, the pin tables and EASY_CONNECTIONS.md always agree.
Run:  python combined_view.py [A B C]   -> <X>_COMBINED.png
"""
from __future__ import annotations

import os
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
from matplotlib.patches import Circle, FancyArrowPatch, Rectangle

import boards
import zb_engine as z

OUT = os.path.dirname(os.path.abspath(__file__))
TITLE = {"A": "MAIN board (Teensy 4.1 #1 + LoRa + GPS + sensors)", "B": "BACKUP board (Teensy 4.1 #2)",
         "C": "SENSOR board (environment)", "D": "PARACHUTE board (Teensy 4.0)", "E": "POWER board"}
EASY = {
    "GND": "GND (ground)", "+3V3_A": "3.3 V", "+3V3_B": "3.3 V", "+5V_LOGIC": "5 V", "+5V_MQ": "5 V (ozone heater)",
    "SDA": "I2C data (SDA)", "SCL": "I2C clock (SCL)", "SDA1": "sensor I2C data", "SCL1": "sensor I2C clock",
    "GNSS_TX": "GPS TX → Teensy 7", "GNSS_RX": "Teensy 8 → GPS RX", "GNSS_RST": "GPS reset (Teensy 6)",
    "GNSS_VBCKP": "GPS backup supply", "GNSS_CELL": "coin cell +", "GNSS_D": "GPS copy → Parachute",
    "GNSS_B": "GPS copy → Backup", "GNSS_IN": "GPS copy in", "LORA_CS": "LoRa CS (29)", "LORA_BUSY": "LoRa BUSY (24)",
    "LORA_RST": "LoRa RESET (25)", "LORA_IRQ": "LoRa IRQ DIO9 (30)", "MOSI1": "SPI MOSI (26)", "SCK1": "SPI clock (27)",
    "MISO1": "SPI MISO (39)", "FLASH_CS": "Flash CS (9)", "ICM_1V8": "IMU 1V8–CS–SDO tie",
    "HB_OUT": "heartbeat (5) → 1k", "HB_J5": "heartbeat → Backup plug", "HB_IN": "heartbeat in (4)",
    "TX3": "UART TX (14) → 1k", "TX3_J4": "UART TX → Parachute plug", "TX3_J": "UART TX → Parachute plug",
    "RX3": "UART RX (15)", "VBAT_SENSE": "battery sense in", "BUS_V": "battery sense → pin 20",
    "ENV_UV": "UV analog", "ENV_O3": "ozone analog", "MQ_AO": "ozone sensor out",
    "LED_OK": "green LED drive", "LED_OK_A": "→ green LED", "LED_FLT": "red LED drive", "LED_FLT_A": "→ red LED",
    "TX8": "Serial8 TX (35) → 1k", "TX8_J": "→ mux ch2 TX", "RX8": "Serial8 RX (34)", "RX8_J": "mux ch2 RX → 1k",
    "CAM_TX": "MAIN camera command (pin 1) → 1k", "CAM_TX_J": "camera command in LINK cable → mux ch1",
    "CAM_RX": "camera reply in LINK cable → MAIN pin 0", "MUX_A": "mux select A (5)", "MUX_B": "mux select B (6)",
    "MUX_C": "mux select C / off (9)", "CAM_COM_TX": "mux common TX → 1k", "CAM_TO_XIAO": "→ XIAO D7 (RX)",
    "CAM_FROM_XIAO": "XIAO D6 (TX) → 1k", "CAM_COM_RX": "→ mux common RX",
    "AMP_DIN": "amp DIN (2)", "AMP_LRC": "amp LRC (3)", "AMP_BCLK": "amp BCLK (4)", "AMP_SD": "amp on/off SD (37)",
    "AMP_GAIN": "amp GAIN → 100k → GND (15 dB)",
    "+8V_STEER": "8 V servo power", "+5V_SERVO": "5 V servo power", "GNSS_IN": "GPS copy in (pin 0)",
    "STEER_L": "left steer (2) → 330", "STEER_L_S": "→ left servo S", "STEER_R": "right steer (3) → 330",
    "STEER_R_S": "→ right servo S", "BAY_LID": "bay-lid servo (4) → 330", "BAY_LID_S": "→ MG90S #1 S",
    "PETRI": "petri servo (5) → 330", "PETRI_S": "→ MG90S #2 S", "ESC": "ESC throttle (6) → 330",
    "ESC_S": "→ ESC signal wire", "RX2": "data from MAIN (7)", "TX2": "data out (8) → 1k",
    "TX2_A": "→ MAIN", "TX2_B": "→ BACKUP", "HALL": "Hall spin sensor (9)", "SD_CS": "SD CS (10)",
    "SD_SCK": "SD SCK (13)", "SD_MOSI": "SD MOSI (11)", "SD_MISO": "SD MISO (12)",
}
PART_NAME = {("A", "U1"): "Teensy 4.1 MAIN", ("A", "U2"): "LoRa Core1121", ("A", "U3"): "GPS L89HA",
             ("A", "U4"): "RTC DS3231", ("A", "U5"): "Pressure BMP585", ("A", "U6"): "IMU ICM-20948",
             ("A", "U7"): "Flash W25Q64", ("A", "U9"): "Beacon amp MAX98357A", ("B", "U1"): "Teensy 4.1 BACKUP", ("B", "U5"): "Pressure BMP585 #2",
             ("B", "U8"): "IMU LSM6DSO32", ("B", "U10"): "Camera mux 7Semi", ("D", "U1"): "Teensy 4.0 PARACHUTE", ("D", "U5"): "Pressure BMP585 #3", ("D", "U6"): "IMU ICM-20948 #2", ("D", "U7"): "SD card module", ("C", "M1"): "HDC3020 temp/humidity"}


def draw(bk: str) -> str:
    b = boards.BOARDS[bk][0]()
    results, occ = z.route(b)
    problems = z.verify(b, results, occ) + z.fit_check(b)
    if problems:
        raise SystemExit(f"{bk}: verification failed: {problems}")
    cols = z.net_colors(b)
    net_of = {(r, p): n.name for n in b.nets for r, p in n.pins}

    fig, ax = plt.subplots(figsize=(17, 15), dpi=200)
    ax.set_aspect("equal")
    ax.axis("off")
    ax.set_xlim(-6, z.BOARD_W + 62)
    ax.set_ylim(-9, z.BOARD_H + 9)
    # board + holes
    ax.add_patch(Rectangle((0, 0), z.BOARD_W, z.BOARD_H, fc="#d8c99b", ec="#5a4a1e", lw=2, zorder=0))
    xs = [z.hx(c) for c in range(1, z.NC + 1)]
    ys = [z.hy(r) for r in range(1, z.NR + 1)]
    ax.scatter([x for x in xs for _ in ys], [y for _ in xs for y in ys], s=4, c="#8c7a45", zorder=1)
    for c in range(1, z.NC + 1, 2):
        ax.text(z.hx(c), -2.0, str(c), fontsize=4.5, ha="center", va="top")
        ax.text(z.hx(c), z.BOARD_H + 1.0, str(c), fontsize=4.5, ha="center", va="bottom")
    for r in range(1, z.NR + 1, 2):
        ax.text(-1.0, z.hy(r), str(r), fontsize=4.5, ha="right", va="center")
    ax.text(-1.0, -2.0, "row\\col", fontsize=4.5, ha="right", va="top")

    # parts (outline only, so the lines underneath stay visible)
    for p in b.parts.values():
        x0, y0, x1, y1 = p.body
        ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, fc=p.color if not p.dashed else "none",
                               ec="#333" if not p.dashed else "#999", lw=1.0, ls="--" if p.dashed else "-",
                               alpha=0.22 if not p.dashed else 1.0, zorder=2))
        name = PART_NAME.get((bk, p.ref), p.label)
        big = (x1 - x0) * (y1 - y0) > 150
        ax.text((x0 + x1) / 2, y1 - 0.8, f"{p.ref}  {name}", fontsize=6.2 if big else 4.4, ha="center",
                va="top", fontweight="bold", zorder=12, color="#111",
                bbox=dict(fc="white", ec="none", alpha=0.7, pad=0.3))

    # power wires (insulated, component side): drawn as right-angled thick lines
    for name, res in results.items():
        if not res.get("wire"):
            continue
        for a, c in res["jumpers"]:
            px = [z.hx(a[0]), z.hx(c[0]), z.hx(c[0])]
            py = [z.hy(a[1]), z.hy(a[1]), z.hy(c[1])]
            ax.plot(px, py, color=cols[name], lw=3.2, alpha=0.45, solid_capstyle="round", zorder=4)
    # signal traces (solder side, seen through the board)
    for name, res in results.items():
        if res.get("wire"):
            continue
        col = cols[name]
        for path in res["segs"]:
            ax.plot([z.hx(c) for c, r in path], [z.hy(r) for c, r in path], color=col, lw=2.0,
                    solid_capstyle="round", solid_joinstyle="round", zorder=6)
            ax.scatter([z.hx(c) for c, r in path], [z.hy(r) for c, r in path], s=5, color=col, zorder=7)
        for a, c in res.get("hops", []):
            arc = FancyArrowPatch((z.hx(a[0]), z.hy(a[1])), (z.hx(c[0]), z.hy(c[1])), arrowstyle="-",
                                  connectionstyle="arc3,rad=0.6", color=col, lw=1.6, zorder=8)
            ax.add_patch(arc)
    # pins on top
    for p in b.parts.values():
        for pad, (c, r) in p.pins.items():
            net = net_of.get((p.ref, pad))
            ax.add_patch(Circle((z.hx(c), z.hy(r)), 0.95, fc=cols.get(net, "white") if net else "white",
                                ec="black", lw=0.6, zorder=9))
            ax.text(z.hx(c) + 1.05, z.hy(r) + 0.35, p.names.get(pad, pad), fontsize=3.4, zorder=10,
                    fontweight="bold", bbox=dict(fc="white", ec="none", alpha=0.6, pad=0.1))
    for x0, y0, x1, y1, lab in b.keepout:
        ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, fc="none", ec="red", ls=":", lw=1.0, zorder=3))
        ax.text((x0 + x1) / 2, (y0 + y1) / 2, lab, fontsize=5, color="red", ha="center", va="center", zorder=3)

    # legend
    lx, ly = z.BOARD_W + 4, z.BOARD_H - 2
    ax.text(lx, ly, "LINES (colour = signal)", fontsize=8, fontweight="bold", va="top")
    ly -= 4
    for n in b.nets:
        if ly < 8:
            break
        kind = "wire" if n.kind == "wire" else "trace"
        ax.add_line(Line2D([lx, lx + 5], [ly, ly], color=cols[n.name], lw=3.2 if kind == "wire" else 2.0,
                           alpha=0.6 if kind == "wire" else 1.0))
        ax.text(lx + 6.5, ly, f"{EASY.get(n.name, n.name)}" + ("  [power wire]" if kind == "wire" else ""),
                fontsize=5.6, va="center")
        ly -= 2.9
    ax.text(lx, 6, "How to build it\n"
            "• thin line along holes = solder trail / bare tinned wire on the BOTTOM (shown through the board)\n"
            "• small arch = short insulated wire jumping over another line (don't solder the hole under it)\n"
            "• thick pale line = insulated power wire (GND / 3.3 V / 5 V) on the TOP\n"
            "• every hole a line passes belongs to that line only",
            fontsize=5.4, va="bottom", linespacing=1.4)
    hops = sum(len(r.get("hops", [])) for r in results.values())
    ax.set_title(f"{TITLE[bk]} — parts + all lines, 120 × 150 mm zero board, seen from the COMPONENT side   "
                 f"(verified: 0 errors, {hops} hop wires)", fontsize=11, fontweight="bold", loc="left")
    path = os.path.join(OUT, f"{bk}_COMBINED.png")
    fig.savefig(path, bbox_inches="tight")
    plt.close(fig)
    return path


if __name__ == "__main__":
    for k in sys.argv[1:] or ["A", "B", "C"]:
        print("wrote", draw(k))
