"""Per-module pin connection tables, generated from boards.py (the verified
placement + netlist), so the tables can never disagree with the routing sheets.

For every module / connector pin: the signal, where it goes, and any resistor,
diode or capacitor in the path with BOTH of its legs spelled out.
Run:  python pin_tables.py   -> MODULE_PIN_TABLES.md
"""
from __future__ import annotations

import os

import boards

OUT = os.path.dirname(os.path.abspath(__file__))
PASSIVE = ("R", "C", "D", "F")
RAILS = {"GND", "+3V3_A", "+3V3_B", "+5V_LOGIC", "+5V_SERVO", "+8V_STEER", "VBAT", "+5V_MQ"}

BOARD_TITLE = {"A": "Board 1 — MAIN (Teensy 4.1 #1)", "B": "Board 2 — BACKUP + SENSOR (Teensy 4.1 #2)",
               "D": "Board 4 — PARACHUTE (Teensy 4.0, etched)", "E": "Board 3 — POWER (zero board)"}

# extra words for pins that need a reason, keyed (board, ref, pin name)
WHY = {
    ("A", "U1", "3V3"): "Teensy's own 3.3 V output — do NOT join it to the 3V3_A rail",
    ("B", "U1", "3V3"): "Teensy's own 3.3 V output — do NOT join it to the 3V3_B rail",
    ("D", "U1", "3V3"): "Teensy's own 3.3 V output — do NOT join it to the 3V3_B rail",
    ("A", "U1", "33"): "firmware drives 33–36 (old motor driver) — keep free",
    ("A", "U1", "34"): "firmware drives 33–36 — keep free", ("A", "U1", "35"): "firmware drives 33–36 — keep free",
    ("A", "U1", "36"): "firmware drives 33–36 — keep free",
    ("A", "U1", "13"): "on-board LED (firmware status LED)",
    ("A", "U2", "LORA_ANT"): "antenna goes on the module's u.FL socket, not this pad",
    ("A", "U2", "2.4G"): "2.4 GHz not used (reserved band)",
    ("A", "U3", "3D_FIX"): "would need pin 33, which the firmware drives",
    ("A", "U3", "SDA"): "GNSS uses UART only", ("A", "U3", "SCL"): "GNSS uses UART only",
    ("A", "U6", "INT"): "firmware polls the IMU", ("A", "U5", "INT"): "firmware polls the baro",
    ("B", "U8", "INT1"): "not needed for polling", ("B", "U8", "INT2"): "not needed for polling",
    ("B", "U8", "3Vo"): "module's 3.3 V output — leave open", ("A", "U5", "3V3"): "module's 3.3 V output — leave open",
    ("B", "U5", "3V3"): "module's 3.3 V output — leave open", ("D", "U5", "3V3"): "module's 3.3 V output — leave open",
    ("E", "U1", "ALERT"): "not used", ("E", "U2", "ALERT"): "not used", ("E", "U3", "ALERT"): "not used",
}


def hole(p, pad):
    c, r = p.pins[pad]
    return f"C{c} R{r}"


def ptype(ref):
    return ref.rstrip("0123456789")


def rail_text(b, pins_of, net, exclude_ref):
    """Name the power-INPUT connector(s) of a rail (cable from board E / A),
    not whichever connector happens to share the rail."""
    entry = ("PWR", "from E", "thick", "from A", "BATTERY")
    src = sorted({f"{r} pin {p}" for r, p in pins_of[net]
                  if r.startswith("J") and r != exclude_ref and any(k in b.parts[r].label for k in entry)})
    where = ", ".join(src[:2]) if src else "the power-module landing holes"
    return f"**{net} rail** (insulated rail wire on the component side; rail enters at {where})"


def describe(b, net_of, pins_of, ref, pad, seen_parts):
    """Where pin (ref,pad) goes. A passive in the path is written out leg by
    leg: which hole each leg sits in and what is on the far leg."""
    net = net_of.get((ref, pad))
    if net is None:
        return None, None
    if net in RAILS and len(pins_of[net]) > 5:
        return net, rail_text(b, pins_of, net, ref)
    others = [(r, p) for r, p in pins_of[net] if (r, p) != (ref, pad) and r not in seen_parts]
    parts_txt = []
    same = [p for r, p in pins_of[net] if r == ref and p != pad]
    if same:
        me = b.parts[ref]
        parts_txt.append("join to this module's own pin(s) " +
                         ", ".join(f"**{me.names[p]}** ({hole(me, p)})" for p in same))
    for r, p in others:
        part = b.parts[r]
        if ptype(r) in PASSIVE:
            far = "2" if p == "1" else "1"
            _, nxt = describe(b, net_of, pins_of, r, far, seen_parts | {ref, r})
            parts_txt.append(f"through **{r} {part.label}**: leg {part.names[p]} in {hole(part, p)} (joined to this "
                             f"signal), leg {part.names[far]} in {hole(part, far)} → {nxt or 'nothing (check)'}")
        else:
            parts_txt.append(f"{r} {part.label} → pin **{part.names[p]}** ({hole(part, p)})")
    return net, " ; ".join(parts_txt) if parts_txt else "only this pin (check)"


def table_for(bk, b, net_of, pins_of, ref) -> list[str]:
    p = b.parts[ref]
    head = f"### {ref} — {p.label}" + (f"  \n*{p.note.replace(chr(10), ' · ')}*" if p.note else "")
    rows = [head, "", "| pin | hole | signal | connect to (resistor / diode legs spelled out) |", "|---|---|---|---|"]

    def order(pad):
        name = p.names[pad]
        if ref == "U1" and name.isdigit():
            return (0, int(name))
        return (1, int(pad)) if pad.isdigit() else (2, name)

    for pad in sorted(p.pins, key=order):
        name = p.names[pad]
        net, where = describe(b, net_of, pins_of, ref, pad, frozenset({ref}))
        why = WHY.get((bk, ref, name))
        if net is None:
            where = "**not connected — leave open**" + (f" ({why})" if why else "")
            net = "—"
        elif why:
            where += f" — {why}"
        rows.append(f"| {name} | {hole(p, pad)} | {net} | {where} |")
    return rows + [""]


def passive_list(b) -> list[str]:
    rows = ["### Resistors, diodes, LEDs and capacitors on this board", "",
            "| part | value / role | leg 1 → hole → signal | leg 2 → hole → signal |", "|---|---|---|---|"]
    net_of = {(r, p): n.name for n in b.nets for r, p in n.pins}
    for ref, p in sorted(b.parts.items(), key=lambda kv: (ptype(kv[0]), int(kv[0][len(ptype(kv[0])):] or 0))):
        if ptype(ref) not in PASSIVE or ref.endswith("L"):
            continue
        legs = []
        for pad in ("1", "2"):
            legs.append(f"{p.names[pad]} → {hole(p, pad)} → {net_of.get((ref, pad), 'open')}")
        rows.append(f"| {ref} | {p.label} | {legs[0]} | {legs[1]} |")
    return rows + ["", "Polarity: LED / diode leg **A** = anode (long leg / no band), **K** = cathode (short leg / band). "
                       "Electrolytic **+** = long leg, **−** = stripe side.", ""]


def main():
    L = ["# VRITRA zero boards — pin-by-pin connection tables", "",
         "Generated from `boards.py` (the same data the routing sheets and checks use). Hole = column C (from the left on "
         "the component side) and row R (from the bottom). A resistor in the path is shown with BOTH legs: which hole "
         "each leg goes in and what that leg connects to. GND / 3V3 / 5V rails are insulated wires on the component side "
         "(see `<board>_top.png`); signals are solder-side traces (see `<board>_bottom.png`).", ""]
    for bk in boards.BOARDS:
        b = boards.BOARDS[bk][0]()
        net_of = {(r, p): n.name for n in b.nets for r, p in n.pins}
        pins_of = {}
        for n in b.nets:
            pins_of[n.name] = n.pins
        L.append(f"## {BOARD_TITLE[bk]}")
        L.append("")
        refs = [r for r in b.parts if ptype(r) in ("U", "M") and not b.parts[r].dashed and b.parts[r].pins]
        refs += [r for r in b.parts if (ptype(r) == "J" or r.endswith("L")) and b.parts[r].pins and r not in refs]
        for ref in refs:
            L += table_for(bk, b, net_of, pins_of, ref)
        L += passive_list(b)
    path = os.path.join(OUT, "MODULE_PIN_TABLES.md")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(L))
    print("wrote", path, len(L), "lines")


if __name__ == "__main__":
    main()
