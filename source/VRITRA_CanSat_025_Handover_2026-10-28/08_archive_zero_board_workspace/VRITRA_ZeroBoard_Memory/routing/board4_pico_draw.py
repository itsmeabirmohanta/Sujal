"""Pictures of the routed P4-Pico Board 4: component side and solder side, user hole numbers on the edges.
Run:  python board4_pico_draw.py [routed.json]  -> ../kicad_single/BOARD4_PARACHUTE_PICO/BOARD4_PICO_{TOP,BOTTOM}.png
"""
from __future__ import annotations

import json
import os
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.patches as mp     # noqa: E402
import matplotlib.pyplot as plt     # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import board4_pico as q             # noqa: E402
import zb_engine as z               # noqa: E402

OUT = os.path.join(os.path.dirname(HERE), "kicad_single", "BOARD4_PARACHUTE_PICO")
NET_COL = {"GND": "#212121", "+5V_LOGIC": "#e53935", "+3V3_B": "#d81b60", "+8V_STEER": "#bf360c",
           "+5V_SERVO": "#ff7043", "SDA": "#f9a825", "SCL": "#ef6c00"}
PALETTE = ["#1e88e5", "#43a047", "#8e24aa", "#00897b", "#3949ab", "#6d4c41", "#c0ca33", "#00acc1", "#5e35b1",
           "#7cb342", "#039be5", "#546e7a", "#fb8c00", "#26a69a", "#ab47bc", "#8d6e63", "#0277bd", "#558b2f"]


def colour(name, i):
    return NET_COL.get(name, PALETTE[i % len(PALETTE)])


def draw(routed, path, solder_side: bool):
    b = q.board4_pico()
    fig, ax = plt.subplots(figsize=(15, 14), dpi=110)
    X = (lambda x: z.BOARD_W - x) if solder_side else (lambda x: x)
    ax.add_patch(mp.Rectangle((0, 0), z.BOARD_W, z.BOARD_H, fc="#c8e6c9" if not solder_side else "#d7ccc8", ec="k"))
    for c in range(1, z.NC + 1):
        for r in range(1, z.NR + 1):
            ax.add_patch(mp.Circle((X(z.hx(c)), z.hy(r)), 0.45, fc="white", ec="#9e9e9e", lw=0.4))
    for c in range(1, z.NC + 1):
        ax.text(X(z.hx(c)), z.BOARD_H + 1.3, str(c), ha="center", fontsize=7, color="#1a237e")
        ax.text(X(z.hx(c)), -1.8, str(c), ha="center", fontsize=7, color="#1a237e")
    for r in range(1, z.NR + 1):
        ru = int(q.er(r))
        ax.text(-1.3 if not solder_side else z.BOARD_W + 1.3, z.hy(r), f"R{ru}", ha="right" if not solder_side else "left",
                va="center", fontsize=7, color="#1a237e")
    net_of = {(r, p): n.name for n in b.nets for r, p in n.pins}
    names = [n.name for n in b.nets]
    if not solder_side:
        for p in b.parts.values():
            x0, y0, x1, y1 = p.body
            ax.add_patch(mp.Rectangle((x0, y0), x1 - x0, y1 - y0, fc=p.color, ec="k", alpha=0.35, lw=0.8))
            ax.text((x0 + x1) / 2, y1 + 0.4, p.ref, ha="center", va="bottom", fontsize=8, fontweight="bold")
    for i, (name, rr) in enumerate(routed.items()):
        col = colour(name, names.index(name))
        for seg in rr["trails"]:
            xs = [X(z.hx(h[0])) for h in seg]
            ys = [z.hy(h[1]) for h in seg]
            ax.plot(xs, ys, "-", color=col, lw=4.0 if solder_side else 2.0, alpha=0.95 if solder_side else 0.45,
                    solid_capstyle="round")
        for a, c in rr["links"]:
            ax.plot([X(z.hx(a[0])), X(z.hx(c[0]))], [z.hy(a[1]), z.hy(c[1])], "--" if solder_side else "-",
                    color=col, lw=1.3 if solder_side else 3.0)
    for p in b.parts.values():
        for pid, h in p.pins.items():
            nm = net_of.get((p.ref, pid))
            ax.add_patch(mp.Circle((X(z.hx(h[0])), z.hy(h[1])), 0.75, fc="#ffd54f" if nm else "#9e9e9e", ec="k", lw=0.6,
                                   zorder=5))
            if not solder_side or nm:
                ax.text(X(z.hx(h[0])) + 0.9, z.hy(h[1]) + 0.55, p.names.get(pid, pid), fontsize=4.6, zorder=6,
                        color="#000" if not solder_side else "#1a237e")
    n = 0
    for rr in routed.values():
        for a, c in rr["links"]:
            n += 1
            ax.text((X(z.hx(a[0])) + X(z.hx(c[0]))) / 2 + 0.4, (z.hy(a[1]) + z.hy(c[1])) / 2 + 0.4, f"LK{n}", fontsize=7,
                    fontweight="bold", color="#b71c1c", zorder=7)
    side = "SOLDER SIDE (board flipped left-right): solder these trails" if solder_side else \
        "COMPONENT SIDE: parts + wire links (thick lines = LK wire links, faint = trails underneath)"
    ax.set_title(f"Board 4 PARACHUTE · ESP32-P4-Pico zero board — {side}\nC = column from the left edge (seen from the "
                 f"component side), R = row down from the top (R1 = P4 pin 1 row)", fontsize=11)
    ax.set_xlim(-4, z.BOARD_W + 4)
    ax.set_ylim(-4, z.BOARD_H + 4)
    ax.set_aspect("equal")
    ax.axis("off")
    fig.savefig(path, bbox_inches="tight")
    plt.close(fig)
    return path


def main(src=None):
    data = json.load(open(src or q.OUT_JSON, encoding="utf-8"))
    routed = {k: {"trails": [[tuple(h) for h in t] for t in v["trails"]],
                  "links": [(tuple(a), tuple(c)) for a, c in v["links"]]} for k, v in data["routes"].items()}
    os.makedirs(OUT, exist_ok=True)
    print(draw(routed, os.path.join(OUT, "BOARD4_PICO_TOP.png"), False))
    print(draw(routed, os.path.join(OUT, "BOARD4_PICO_BOTTOM.png"), True))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else None)
