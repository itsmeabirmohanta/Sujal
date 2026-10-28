"""Independent electrical audit of the zero-board design (does not use the
router). Checks, per board:
  1. every module supply pin sits on the rail its datasheet allows
  2. no Teensy I/O pin (3.3 V only) shares a net with 5 V / 8 V / battery
  3. every net has >= 2 pins; every GND-named pin is on GND
  4. no two driving outputs on one net (Teensy TX/output pins listed below)
  5. I2C addresses unique on each bus (addresses from 02_HARDWARE_INVENTORY)
  6. MAIN Teensy pins match the flight firmware Config.h #defines
Run: python audit.py   (exit 1 on any finding)
"""
from __future__ import annotations

import os
import re
import sys

import boards

FW = r"C:\Users\kalbh\Downloads\vritra-gcs-master\vritra-gcs-master\firmware\vritra_fc\Config.h"
V33 = {"+3V3_A", "+3V3_B"}
V5 = {"+5V_LOGIC", "+5V_MQ"}
HIGH = {"+5V_LOGIC", "+5V_MQ", "+5V_SERVO", "+8V_STEER", "VBAT", "BAT+", "VBAT_SW", "VBAT_F", "8V_RAW", "5V_RAW"}

# (board, ref, pin name) -> allowed rails
SUPPLY = {
    ("A", "U1", "VIN"): V5, ("A", "U2", "3.3V"): V33, ("A", "U3", "3V3"): V33, ("A", "U4", "VCC"): V33,
    ("A", "U5", "VIN"): V33, ("A", "U6", "VIN"): V33, ("A", "U7", "3V3"): V33, ("A", "U9", "Vin"): V5,
    ("B", "U1", "VIN"): V5, ("B", "U5", "VIN"): V33, ("B", "U8", "VIN"): V33, ("B", "U10B", "3V3"): V33,
    ("D", "U1", "VIN"): V5, ("D", "U5", "VIN"): V33, ("D", "U6", "VIN"): V33, ("B", "U11", "VCC"): V5, ("B", "M1L", "3V3"): V33,
}
# pins that actively drive their net (two of these on one net = fight)
DRIVERS = {
    ("A", "U1"): {"1", "2", "3", "4", "5", "6", "8", "9", "14", "24", "25", "26", "27", "29", "31", "32", "37"},
    ("A", "U3"): {"9"},                     # L89 TX
    ("B", "U1"): {"5", "6", "9", "10", "11", "13", "14", "35", "36", "37"},
    ("D", "U1"): {str(n + 2) for n in (2, 3, 4, 5, 6, 8)},
}
I2C = {  # bus net -> [(device, address)]
    ("A", "SDA"): [("BMP585", 0x47), ("ICM-20948", 0x69), ("DS3231", 0x68), ("AT24C32 EEPROM", 0x57),
                   ("INA260 #1", 0x40), ("INA260 #2", 0x41), ("INA260 #3", 0x45)],
    ("B", "SDA"): [("BMP585 #2", 0x47), ("LSM6DSO32", 0x6A), ("HDC3020", 0x44)],
    ("D", "SDA"): [("BMP585 #3", 0x47), ("LSM6DSO32", 0x6A)],
}
FW_PINS = {"PIN_LORA_CS": "LORA_CS", "PIN_LORA_IRQ": "LORA_IRQ", "PIN_LORA_RST": "LORA_RST",
           "PIN_LORA_BUSY": "LORA_BUSY", "PIN_SPI1_MOSI": "MOSI1", "PIN_SPI1_SCK": "SCK1",
           "PIN_SPI1_MISO": "MISO1", "PIN_GNSS_RX": "GNSS_TX", "PIN_GNSS_TX": "GNSS_RX",
           "PIN_GNSS_RST": "GNSS_RST", "PIN_SDA": "SDA", "PIN_SCL": "SCL", "PIN_BEACON_DIN": "AMP_DIN",
           "PIN_BEACON_LRCLK": "AMP_LRC", "PIN_BEACON_BCLK": "AMP_BCLK", "PIN_BEACON_SD": "AMP_SD"}


def audit() -> list[str]:
    out = []
    for bk in boards.BOARDS:
        b = boards.BOARDS[bk][0]()
        net_of = {(r, p): n.name for n in b.nets for r, p in n.pins}
        for (bb, ref, pin), ok in SUPPLY.items():
            if bb != bk:
                continue
            pad = next((k for k, v in b.parts[ref].names.items() if v == pin), None)
            net = net_of.get((ref, pad))
            if net not in ok:
                out.append(f"{bk}: {ref} {pin} is on {net}, must be one of {sorted(ok)}")
        for n in b.nets:
            refs = {r for r, _ in n.pins}
            if len(n.pins) < 2:
                out.append(f"{bk}: net {n.name} has only one pin")
            if n.name in HIGH:
                for r, p in n.pins:
                    if bk in "ABD" and r == "U1" and b.parts[r].names[p] not in ("VIN", "GND"):
                        out.append(f"{bk}: Teensy pin {b.parts[r].names[p]} is on {n.name} (3.3 V-only pin!)")
            drivers = [(r, p) for r, p in n.pins if p in DRIVERS.get((bk, r), set())]
            if len(drivers) > 1:
                out.append(f"{bk}: net {n.name} has {len(drivers)} drivers {drivers}")
        for p in b.parts.values():
            for pad, nm in p.names.items():
                if nm.startswith("GND") and nm != "GNSS" and (p.ref, pad) in net_of and net_of[(p.ref, pad)] != "GND":
                    out.append(f"{bk}: {p.ref} {nm} is on {net_of[(p.ref, pad)]}, not GND")
    for (bk, bus), devs in I2C.items():
        addrs = [a for _, a in devs]
        if len(addrs) != len(set(addrs)):
            out.append(f"{bk}: I2C address clash on {bus}: {devs}")
    # firmware cross-check (MAIN)
    src = open(FW, encoding="utf-8", errors="ignore").read()
    a = boards.BOARDS["A"][0]()
    net_of = {(r, p): n.name for n in a.nets for r, p in n.pins}
    for define, net in FW_PINS.items():
        m = re.search(rf"#define\s+{define}\s+(\w+)", src)
        if not m:
            out.append(f"firmware: {define} not found in Config.h")
            continue
        val = m.group(1)
        pin = str(int(val)) if val.isdigit() else {"A6": "20"}.get(val, val)
        if net_of.get(("U1", pin)) != net:
            out.append(f"firmware {define}={val}: board has Teensy pin {pin} on {net_of.get(('U1', pin))}, expected {net}")
    return out


if __name__ == "__main__":
    f = audit()
    for m in f:
        print("  !", m)
    print(f"audit findings: {len(f)}")
    sys.exit(1 if f else 0)
