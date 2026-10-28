"""Plain-language connection list, component by component, generated from
boards.py + system_view.py (verified data). Run: python easy_list.py -> EASY_CONNECTIONS.md"""
from __future__ import annotations

import os

import boards
from system_view import CABLES

OUT = os.path.dirname(os.path.abspath(__file__))
BOARD = {"A": "BOARD 1 MAIN (etched)", "B": "BOARD 2 BACKUP + SENSOR (etched)", "D": "BOARD 4 PARACHUTE (etched)", "E": "BOARD 3 POWER (zero board)"}
PART = {
    ("A", "U1"): "Teensy 4.1 (main)", ("A", "U2"): "LoRa radio (Core1121)", ("A", "U3"): "GPS (L89HA)",
    ("A", "U4"): "RTC clock (DS3231)", ("A", "U5"): "Pressure sensor (BMP585)", ("A", "U6"): "IMU (ICM-20948)",
    ("A", "U7"): "Flash memory (W25Q64)", ("A", "U9"): "Beacon amp (Adafruit MAX98357A)",
    ("B", "U1"): "Teensy 4.1 (backup)", ("B", "U5"): "Pressure sensor (BMP585 #2)", ("B", "U8"): "IMU (LSM6DSO32)", ("B", "U10"): "Camera mux (7Semi 4-ch UART)",
    ("B", "U10B"): "Camera mux bottom-row wires",
    ("D", "U1"): "Teensy 4.0 (parachute)", ("D", "U5"): "Pressure sensor (BMP585 #3, to buy)",
    ("D", "U6"): "IMU (ICM-20948 #2)", ("D", "U7"): "SD card module",
    ("E", "U1"): "INA260 #1 (battery)", ("E", "U2"): "INA260 #2 (8V servo line)", ("E", "U3"): "INA260 #3 (5V line)",
    ("E", "M1L"): "5V buck #1 AP64500 (logic 5V)", ("E", "M2L"): "5V buck #2 AP64500 (servo 5V)",
    ("E", "M3L"): "8V buck (steering servos)", ("E", "M4L"): "3.3V buck #1 AP62301", ("E", "M5L"): "3.3V buck #2 AP62301",
}
PLUG = {
    ("A", "J1"): "JST-4 POWER IN", ("A", "J2"): "JST-3 I2C to POWER", ("A", "J3"): "JST-2 BATTERY SENSE",
    ("A", "J4"): "JST-3 UART to PARACHUTE", ("A", "J5"): "JST-5 LINK cable to BACKUP", ("A", "J6"): "JST-4 I2C to SENSOR",
    ("A", "J7"): "JST-2 GPS COIN CELL", ("A", "J8"): "JST-2 GPS copy to PARACHUTE", ("A", "J9"): "JST-3 ANALOG to SENSOR",
    ("B", "J1"): "JST-4 POWER IN", ("B", "J2"): "JST-5 LINK cable from MAIN", ("B", "J3"): "JST-3 link to PARACHUTE",
    ("B", "J4"): "JST-4 CAMERA plug (the only one)",
    ("B", "M1L"): "HDC3020 cable wires (solder)",
    ("D", "J8"): "JST-4 POWER IN", ("D", "J2"): "JST-3 UART from MAIN", ("D", "J5"): "JST-2 GPS copy from MAIN",
    ("D", "J3"): "JST-3 link to BACKUP", ("D", "J1"): "8V thick-wire pads", ("D", "J6"): "5V-servo thick-wire pads",
    ("D", "J10"): "LEFT steering servo plug", ("D", "J11"): "RIGHT steering servo plug",
    ("D", "J12"): "parachute-bay servo plug (MG90S #1)", ("D", "J13"): "petri-lid servo plug (MG90S #2)",
    ("D", "J14"): "2-pin ESC signal plug (Cyclone 45A)", ("D", "J15"): "Hall sensor plug",
    ("E", "J1"): "battery thick-wire pads", ("E", "J3"): "ON/OFF switch pads", ("E", "F1"): "fuse pads",
    ("E", "J4"): "ESC power thick-wire pads", ("E", "J2"): "JST-3 I2C to MAIN", ("E", "J5"): "JST-4 POWER to MAIN",
    ("E", "J6"): "JST-4 POWER to BACKUP", ("E", "J7"): "JST-4 POWER to PARACHUTE", ("E", "J8"): "8V thick-wire pads",
    ("E", "J9"): "5V-servo thick-wire pads", ("E", "J10"): "JST-2 BATTERY SENSE to MAIN", ("E", "J11"): "JST-2 5V to SENSOR",
}
RAIL = {"GND": "GND wire", "+3V3_A": "3.3V wire", "+3V3_B": "3.3V wire", "+5V_LOGIC": "5V wire",
        "+5V_SERVO": "5V-servo wire", "+8V_STEER": "8V-servo wire", "VBAT": "battery+ wire", "+5V_MQ": "5V wire"}
PASSIVE = ("R", "C", "D", "F")
NOTE = {("A", "U1", "3V3"): "leave EMPTY (never join to the 3.3V wire)", ("B", "U1", "3V3"): "leave EMPTY",
        ("D", "U1", "3V3"): "leave EMPTY", ("A", "U4", "VCC"): None}


def ptype(r):
    return r.rstrip("0123456789")


def pname(bk, ref):
    if ref in ("U1",) and (bk, ref) in PART:
        return PART[(bk, ref)]
    return PART.get((bk, ref)) or PLUG.get((bk, ref)) or ref


def passive_name(p):
    t = ptype(p.ref)
    if t == "F":
        return f"fuse ({p.ref})"
    if t == "D":
        if "LED" in p.label:
            return f"{'green' if 'green' in p.label else 'red'} LED ({p.ref})"
        return f"Schottky diode ({p.ref})"
    v = p.label.split()[0]
    if t == "R" and v[-1].isdigit():
        v += "Ω"
    return f"{v} {'resistor' if t == 'R' else 'capacitor'} ({p.ref})"


def pin_word(bk, b, ref, pad):
    p = b.parts[ref]
    n = p.names[pad]
    if ref == "U1":
        return f"{pname(bk, ref)} pin {n}"
    if ptype(ref) == "J" or ref == "F1":
        return f"{PLUG.get((bk, ref), ref)} pin {pad} ({n})"
    return f"{pname(bk, ref)} {n}"


def where(bk, b, pins_of, net_of, ref, pad, seen):
    net = net_of.get((ref, pad))
    if net is None:
        return None
    if net in RAIL and len(pins_of[net]) > 3:
        return RAIL[net]
    out = []
    own = [p for r, p in pins_of[net] if r == ref and p != pad]
    if own:
        me = b.parts[ref]
        out.append("its own " + " and ".join(me.names[p] for p in own) + " pin" + ("s" if len(own) > 1 else ""))
    for r, p in pins_of[net]:
        if (r, p) == (ref, pad) or r in seen:
            continue
        part = b.parts[r]
        if ptype(r) in PASSIVE:
            far = "2" if p == "1" else "1"
            nxt = where(bk, b, pins_of, net_of, r, far, seen | {ref, r})
            legs = ""
            if part.names[p] in ("A", "K", "+", "-"):
                legs = f" [{ 'anode' if part.names[p]=='A' else 'cathode' if part.names[p]=='K' else part.names[p]} leg here]"
            out.append(f"{passive_name(part)}{legs} → {nxt}")
        elif r == ref:
            continue
        else:
            out.append(pin_word(bk, b, r, p))
    return " + ".join(out) if out else "nothing (check)"


def board_section(bk) -> list[str]:
    b = boards.BOARDS[bk][0]()
    net_of = {(r, p): n.name for n in b.nets for r, p in n.pins}
    pins_of = {n.name: n.pins for n in b.nets}
    L = [f"## {BOARD[bk]}", ""]
    refs = [r for r in b.parts if (bk, r) in PART]
    for ref in refs:
        p = b.parts[ref]
        L.append(f"**{PART[(bk, ref)]}**")
        used, empty, mod_lines = [], [], {}

        def key(pad):
            n = p.names[pad]
            return (0, int(n)) if ref == "U1" and n.isdigit() else (1, pad)

        for pad in sorted(p.pins, key=key):
            n = p.names[pad]
            w = where(bk, b, pins_of, net_of, ref, pad, frozenset({ref}))
            note = NOTE.get((bk, ref, n))
            if w is None:
                empty.append(n + (f" ({note})" if note else ""))
            else:
                mod_lines.setdefault(f"{n} → {w}", 0)
                mod_lines[f"{n} → {w}"] += 1
        for line, k in mod_lines.items():
            L.append(f"- {line}" + (f"  (all {k} {line.split()[0]} pins)" if k > 1 else ""))
        if empty:
            L.append(f"- not connected: {', '.join(dict.fromkeys(empty))}")
        L.append("")
    plugs = [r for r in b.parts if (bk, r) in PLUG]
    if plugs:
        L.append(f"**Plugs and pads on the {BOARD[bk]}** (pin 1 = first pin)")
        for ref in plugs:
            p = b.parts[ref]
            items = []
            for pad in p.pins:
                w = where(bk, b, pins_of, net_of, ref, pad, frozenset({ref}))
                items.append(f"pin {pad} {p.names[pad]} → {w or 'empty'}")
            L.append(f"- {PLUG[(bk, ref)]}: " + " | ".join(items))
        L.append("")
    return L


def cables() -> list[str]:
    bs = {k: boards.BOARDS[k][0]() for k in boards.BOARDS}
    L = ["## CABLES between boards (all straight: pin 1 ↔ pin 1, pin 2 ↔ pin 2 …)", ""]
    for cid, b1, j1, b2, j2, wire, what in CABLES:
        p1 = bs[b1].parts[j1]
        pins = ", ".join(f"{pad}={p1.names[pad]}" for pad in p1.pins)
        L.append(f"- **{cid}** {BOARD[b1]} {PLUG[(b1, j1)]} ↔ {BOARD[b2]} {PLUG[(b2, j2)]} — {wire} — pins: {pins}")
    return L + [""]


def main():
    L = ["# VRITRA — every connection in plain words", ""]
    for bk in boards.BOARDS:
        L += board_section(bk)
    L += cables()
    path = os.path.join(OUT, "EASY_CONNECTIONS.md")
    open(path, "w", encoding="utf-8").write("\n".join(L))
    print("\n".join(L))


if __name__ == "__main__":
    main()
