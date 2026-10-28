"""Placement helpers for the ETCHED single-layer PCBs (real KiCad footprints).

Every pad that sits on the 2.54 mm grid becomes a routing point; JST-XH pads
(2.50 mm pitch) are within 0.08 mm of the grid and are centred by the exporter.
Pads that are clearly off the grid (7Semi mux bottom row) get a landing grid
point next to them plus a short stub track, and the grid points around them
are blocked so no trace comes too close.
"""
from __future__ import annotations

import math
import os
import re

import zb_engine as z

KLIB = r"C:\Program Files\KiCad\10.0\share\kicad\footprints"
LIBS = {"VRITRA": z.FP_LIB}
OFFGRID_TOL = 0.12            # pitches; a 5-pin JST-XH drifts 0.063
STUB_CLEAR = 2.3              # mm from an off-grid pad centre to the nearest trace centre (1.9 pad + 0.8 trace + 0.55 gap)


def fp_path(fpid: str) -> str:
    lib, name = fpid.split(":", 1)
    d = LIBS.get(lib) or os.path.join(KLIB, lib + ".pretty")
    return os.path.join(d, name + ".kicad_mod")


def load_any(fpid: str):
    """-> pads [(num, x, y)], courtyard points [(x, y)] in footprint mm (KiCad y down)."""
    s = open(fp_path(fpid), encoding="utf-8").read()
    pads = [(m.group(1), float(m.group(2)), float(m.group(3)))
            for m in re.finditer(r'\(pad\s+"([^"]*)"\s+(?:thru_hole|np_thru_hole)\s+\w+\s*\(at\s+([-\d.]+)\s+([-\d.]+)', s)
            if m.group(1)]
    crt = []
    for m in re.finditer(r'\((fp_rect|fp_line|fp_poly|fp_circle)(.*?)\(layer\s+"?F\.CrtYd"?\)', s, re.S):
        body = m.group(2)
        if m.group(1) == "fp_circle":
            cx, cy = map(float, re.search(r'\(center\s+([-\d.]+)\s+([-\d.]+)\)', body).groups())
            ex, ey = map(float, re.search(r'\(end\s+([-\d.]+)\s+([-\d.]+)\)', body).groups())
            rad = math.hypot(ex - cx, ey - cy)
            crt += [(cx - rad, cy - rad), (cx + rad, cy + rad)]
        else:
            crt += [(float(a), float(b)) for a, b in re.findall(r'\((?:start|end|xy)\s+([-\d.]+)\s+([-\d.]+)\)', body)]
    return pads, crt


def place_lib(b: z.Board, ref, fpid, anchor, c, r, rot, label, names, height, color, note="",
              pins_used=None, padmap=None, raised=False):
    """Footprint `fpid` rotated `rot` (CCW) with our pad `anchor` on hole (c, r).
    padmap renames footprint pads to our pad ids (LED/diode: fp 2 = anode = our 1)."""
    padmap = padmap or {}
    pads, crt = load_any(fpid)
    pads = [(padmap.get(p, p), x, y) for p, x, y in pads]
    ax_, ay_ = next((x, y) for p, x, y in pads if p == anchor)
    pins, offgrid = {}, {}
    cx, cy = z.hx(c), z.hy(r)
    for p, x, y in pads:
        if pins_used is not None and p not in pins_used:
            continue
        u, v = (x - ax_) / z.PITCH, -(y - ay_) / z.PITCH
        du, dv = z._rot(u, v, rot)
        if abs(du - round(du)) <= OFFGRID_TOL and abs(dv - round(dv)) <= OFFGRID_TOL:
            pins[p] = (c + int(round(du)), r + int(round(dv)))
        else:
            offgrid[p] = (cx + du * z.PITCH, cy + dv * z.PITCH)
    pts = [z._rot(x - ax_, -(y - ay_), rot) for x, y in crt]
    body = (cx + min(p[0] for p in pts), cy + min(p[1] for p in pts),
            cx + max(p[0] for p in pts), cy + max(p[1] for p in pts))
    part = z.Part(ref, label, pins, {k: names.get(k, k) for k in list(pins) + list(offgrid)}, body, height, color, note)
    part.fp, part.rot, part.padmap, part.offgrid, part.raised = fpid, rot, padmap, offgrid, raised
    part.keep_pads = set(pins) | set(offgrid)
    b.parts[ref] = part
    return part


ROT_OF = {"right": 0, "up": 90, "left": 180, "down": 270}


def place_jst(b, ref, c, r, names, label, direction="right", color="#d9b3ff", note=""):
    """JST-XH vertical (B#B-XH-A), pin 1 on hole (c, r), pins running `direction`."""
    n = len(names)
    if not 2 <= n <= 5:
        raise ValueError(f"{ref}: JST-XH here is 2..5 pins")
    fpid = f"Connector_JST:JST_XH_B{n}B-XH-A_1x{n:02d}_P2.50mm_Vertical"
    return place_lib(b, ref, fpid, "1", c, r, ROT_OF[direction], label,
                     {str(i + 1): nm for i, nm in enumerate(names)}, 7.0, color, note)


def place_hdr(b, ref, c, r, names, label, direction="down", color="#d9b3ff", height=8.5, note="", body=None,
              raised=True):
    """Straight 2.54 pin header (a module standing / lying on it). body overrides the outline."""
    n = len(names)
    fpid = f"Connector_PinHeader_2.54mm:PinHeader_1x{n:02d}_P2.54mm_Vertical"
    rot = {"down": 0, "right": 90, "up": 180, "left": 270}[direction]
    p = place_lib(b, ref, fpid, "1", c, r, rot, label, {str(i + 1): nm for i, nm in enumerate(names)}, height,
                  color, note, raised=raised)
    if body is not None:
        p.body = body
        p.outline = body
    return p


TWO = {   # kind: (footprint, padmap, names, pitch-span, height, colour)
    "R": ("Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal", {}, {"1": "1", "2": "2"}, 4, 2.5, "#f2e6c9"),
    "C": ("Capacitor_THT:C_Disc_D5.0mm_W2.5mm_P5.00mm", {}, {"1": "1", "2": "2"}, 2, 6.0, "#cfe2f3"),
    "CP": ("Capacitor_THT:CP_Radial_D10.0mm_P5.00mm", {}, {"1": "+", "2": "-"}, 2, 13.0, "#9fc5e8"),
    "CP8": ("Capacitor_THT:CP_Radial_D8.0mm_P5.00mm", {}, {"1": "+", "2": "-"}, 2, 12.0, "#9fc5e8"),
    "CP6": ("Capacitor_THT:CP_Radial_D6.3mm_P2.50mm", {}, {"1": "+", "2": "-"}, 1, 11.0, "#9fc5e8"),
    "LED_G": ("LED_THT:LED_D3.0mm", {"2": "1", "1": "2"}, {"1": "A", "2": "K"}, 1, 5.0, "#2ecc40"),
    "LED_R": ("LED_THT:LED_D3.0mm", {"2": "1", "1": "2"}, {"1": "A", "2": "K"}, 1, 5.0, "#ff4136"),
    "D": ("Diode_THT:D_DO-41_SOD81_P10.16mm_Horizontal", {"2": "1", "1": "2"}, {"1": "A", "2": "K"}, 4, 3.0, "#cccccc"),
}


def place2(b, ref, kind, c, r, direction, label):
    """2-lead part, our pin 1 on hole (c, r), pin 2 `span` holes toward `direction`.
    Our pin 1 = A for LEDs/diodes, + for electrolytics."""
    fpid, padmap, names, span, h, color = TWO[kind]
    dc, dr = {"right": (1, 0), "left": (-1, 0), "up": (0, 1), "down": (0, -1)}[direction]
    for rot in (0, 90, 180, 270):
        p = place_lib(b, ref, fpid, "1", c, r, rot, label, names, h, color, padmap=padmap)
        if p.pins.get("2") == (c + dc * span, r + dr * span):
            return p
    raise ValueError(f"{ref}: no rotation puts pin 2 {span} holes {direction}")


def place_area(b, ref, x0, y0, x1, y1, label, color="#ffffff", height=6.0, note=""):
    """Free-standing item with no pads (cable-mounted sensor, keep-clear area)."""
    p = z.place_box(b, ref, x0, y0, x1, y1, label, color, height=height, note=note)
    p.fp, p.area = None, True
    return p


def land_offgrid(b: z.Board) -> list[str]:
    """Give every off-grid pad a landing grid point (stub track) and block the
    grid points that are too close to it. Returns problems."""
    msgs = []
    used = {h for p in b.parts.values() for h in p.pins.values()}
    exact = []        # every pad position in mm (on- and off-grid)
    for p in b.parts.values():
        exact += [(z.hx(h[0]), z.hy(h[1])) for h in p.pins.values()]
        exact += list(getattr(p, "offgrid", {}).values())
    blocked = set(getattr(b, "blocked", set()))
    for p in b.parts.values():
        for pad, (x, y) in getattr(p, "offgrid", {}).items():
            c0, r0 = round((x - z.X0) / z.PITCH) + 1, round((y - z.Y0) / z.PITCH) + 1
            near = [(c, r) for c in range(c0 - 3, c0 + 4) for r in range(r0 - 3, r0 + 4)]
            for g in near:
                if math.hypot(z.hx(g[0]) - x, z.hy(g[1]) - y) < STUB_CLEAR and g not in used:
                    blocked.add(g)
            best = None
            for g in near:
                gx, gy = z.hx(g[0]), z.hy(g[1])
                d = math.hypot(gx - x, gy - y)
                if g in used or d < STUB_CLEAR:
                    continue
                # the landing (a trace end) must keep STUB_CLEAR from every OTHER pad
                if any(math.hypot(gx - ex, gy - ey) < STUB_CLEAR for ex, ey in exact if (ex, ey) != (x, y)):
                    continue
                # prefer straight below / above (short stub, no lean toward neighbours)
                key = (round(d, 2), abs(gx - x))
                if best is None or key < best[0]:
                    best = (key, g)
            if best is None:
                msgs.append(f"{p.ref}.{pad}: no landing point")
                continue
            g = best[1]
            p.pins[pad] = g
            used.add(g)
            blocked.discard(g)
    # a landing point of one pad must not be blocked by another pad's halo
    b.blocked = blocked - used
    return msgs
