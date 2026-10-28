"""Single-layer router for the ETCHED boards, on a 1.27 mm (half-pitch) grid.

Copper rules (home etching, toner transfer):
  pad Ø1.8 mm · track 0.7 mm · heavy track 1.8 mm · every gap ≥ 0.5 mm
A normal track point must be ≥ 1.75 mm from a foreign pad centre, so it may
sit diagonally next to a pin but never straight beside it. Wire-link pads and
heavy tracks are "big": ≥ 2.3 mm from foreign pads and they claim their 8
neighbours. Tracks are straight with 90° turns. Where two nets must cross,
a straight WIRE LINK (jumper on the component side) is used; it never passes
over a pad or over a part that sits flat on the board.
Negotiated congestion (PathFinder), then a strict pass; raises if not clean.
"""
from __future__ import annotations

import heapq
import math

import zb_engine as z

HALF = z.PITCH / 2
PAD_R, TRACK_R, BIG_R, GAP = 0.9, 0.35, 0.9, 0.5
NORM_CLEAR = PAD_R + TRACK_R + GAP          # 1.75 mm: track point to foreign pad centre
BIG_CLEAR = PAD_R + BIG_R + GAP             # 2.30 mm: link pad / heavy track to foreign pad
STEP, TURN = 0.5, 0.3
LINK_COST, LINK_PER = 30.0, 0.25
LINK_COST_NET = {"GND": 10.0, "+3V3_A": 18.0, "+3V3_B": 18.0, "+5V_LOGIC": 18.0}   # rails may jump more readily
LMIN, LMAX = 4, 24                          # link length in half steps (5.08 .. 30.5 mm), ends on the 2.54 hole grid
ITERATIONS = 80
DIRS = ((1, 0), (-1, 0), (0, 1), (0, -1))
N8 = [(dx, dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1) if (dx, dy) != (0, 0)]
HEAVY = {"+8V_STEER", "+5V_SERVO"}
INF = float("inf")
PRES_MAX = 400.0


def P(c, r):
    """hole (c, r) -> half-grid point"""
    return (2 * c - 1, 2 * r - 1)


def xy(q):
    return z.X0 + (q[0] - 1) * HALF, z.Y0 + (q[1] - 1) * HALF


class Grid:
    def __init__(self, b: z.Board):
        self.b = b
        self.imax = int((z.BOARD_W - z.X0) / HALF) + 1
        self.jmax = int((z.BOARD_H - z.Y0) / HALF) + 1
        self.pad_net = {}          # half point -> net of the pad there
        pads = []                  # (x, y, net)
        net_of = {(r, p): n.name for n in b.nets for r, p in n.pins}
        for part in b.parts.values():
            off = getattr(part, "offgrid", {})
            for pad, h in part.pins.items():
                net = net_of.get((part.ref, pad), f"NC:{part.ref}.{pad}")
                if pad in off:                  # landing point of an off-grid pad
                    continue
                q = P(*h)
                if q in self.pad_net and self.pad_net[q] != net:
                    raise ValueError(f"two pads on {q}")
                self.pad_net[q] = net
                pads.append((*xy(q), net))
            for pad, (x, y) in off.items():
                pads.append((x, y, net_of.get((part.ref, pad), f"NC:{part.ref}.{pad}")))
        self.pads = pads
        self.near_norm, self.near_big = {}, {}
        for (x, y, net) in pads:
            i0, j0 = round((x - z.X0) / HALF) + 1, round((y - z.Y0) / HALF) + 1
            for i in range(i0 - 2, i0 + 3):
                for j in range(j0 - 2, j0 + 3):
                    px, py = xy((i, j))
                    d = math.hypot(px - x, py - y)
                    if d < NORM_CLEAR - 1e-6:
                        self.near_norm.setdefault((i, j), set()).add(net)
                    if d < BIG_CLEAR - 1e-6:
                        self.near_big.setdefault((i, j), set()).add(net)
        # link rules: ends outside every body (+1.2 mm), span never over a pad or a flat body
        self.no_end, self.no_span = set(self.pad_net), set(self.pad_net)
        for part in b.parts.values():
            x0, y0, x1, y1 = part.body
            raised = getattr(part, "raised", False)
            for i in range(1, self.imax + 1):
                px = xy((i, 1))[0]
                if not x0 - 1.2 <= px <= x1 + 1.2:
                    continue
                for j in range(1, self.jmax + 1):
                    py = xy((1, j))[1]
                    if raised:          # module on headers sits ≥2.5 mm up: a jumper fitted first may run/land under it
                        continue
                    if y0 - 1.2 <= py <= y1 + 1.2:
                        self.no_end.add((i, j))
                        if x0 - 0.2 <= px <= x1 + 0.2 and y0 - 0.2 <= py <= y1 + 0.2:
                            self.no_span.add((i, j))

    def usable(self, q):
        if not (1 <= q[0] <= self.imax and 1 <= q[1] <= self.jmax):
            return False
        x, y = xy(q)
        return z.EDGE <= x <= z.BOARD_W - z.EDGE and z.EDGE <= y <= z.BOARD_H - z.EDGE

    def ok(self, q, net, big):
        near = (self.near_big if big else self.near_norm).get(q)
        return not near or near <= {net}


def route(b: z.Board, strict=True):
    g = Grid(b)
    rails = set(LINK_COST_NET)
    nets = sorted(b.nets, key=lambda n: (-1 if n.name in HEAVY else (1 if n.name in rails else 0), z._hpwl(b, n)))
    usage, hist = {}, {}
    pin_pts = {}
    for n in b.nets:
        pts = []
        for ref, pad in n.pins:
            part = b.parts[ref]
            pts.append(P(*part.pins[pad]))
        pin_pts[n.name] = list(dict.fromkeys(pts))

    def users(q, net):
        s = usage.get(q)
        return len(s - {net}) if s else 0

    def cost_at(q, net, pres, hard, big):
        if not g.usable(q) or not g.ok(q, net, big):
            return None
        u = users(q, net)
        if big:
            u += sum(users((q[0] + dx, q[1] + dy), net) for dx, dy in N8)
        if u and hard:
            return None
        return (1.0 + hist.get(q, 0.0)) * (1.0 + pres * u)

    def dijkstra(net, sources, target, pres, hard, link_ends):
        heavy = net in HEAVY
        lcost = LINK_COST_NET.get(net, LINK_COST)
        tx, ty = target

        def hh(q):                                   # A*: every step costs at least STEP
            return (abs(q[0] - tx) + abs(q[1] - ty)) * STEP

        pq, best, prev = [], {}, {}
        for s in sources:
            best[(s, -1)] = 0.0
            heapq.heappush(pq, (hh(s), 0.0, s, -1))
        while pq:
            _, cost, cur, d = heapq.heappop(pq)
            if cur == target:
                path, st = [(cur, False)], (cur, d)
                while st in prev:
                    st, link = prev[st]
                    path.append((st[0], link))
                path.reverse()
                return [h for h, _ in path], [path[i][1] for i in range(len(path) - 1)]
            if cost > best.get((cur, d), INF):
                continue
            by_link = d >= 4
            dd = d % 4 if d >= 0 else -1
            for nd, (dx, dy) in enumerate(DIRS):
                if dd != -1 and nd == (dd ^ 1):
                    continue
                turn = TURN if dd not in (-1, nd) else 0.0
                q = (cur[0] + dx, cur[1] + dy)
                fc = cost_at(q, net, pres, hard, heavy)
                if fc is not None:
                    nc = cost + (STEP + turn) * fc
                    if nc < best.get((q, nd), INF):
                        best[(q, nd)] = nc
                        prev[(q, nd)] = ((cur, d), False)
                        heapq.heappush(pq, (nc + hh(q), nc, q, nd))
                # wire link from here: this end and the far end are big pads
                if heavy or by_link or cur in g.no_end or cur in link_ends or cur in g.pad_net                         or not (cur[0] & cur[1] & 1):           # jumper holes only on the 0.1" grid
                    continue
                if cost_at(cur, net, pres, hard, True) is None:
                    continue
                for k in range(1, LMAX + 1):
                    mid = (cur[0] + dx * k, cur[1] + dy * k)
                    if k < LMIN or k & 1:
                        if mid in g.no_span:
                            break
                        continue
                    q = mid
                    if not g.usable(q):
                        break
                    if q in g.no_span:
                        break
                    if q in g.no_end or q in link_ends:
                        continue
                    fc = cost_at(q, net, pres, hard, True)
                    if fc is None:
                        continue
                    nc = cost + (lcost + LINK_PER * k) * fc + turn
                    if nc < best.get((q, nd + 4), INF):
                        best[(q, nd + 4)] = nc
                        prev[(q, nd + 4)] = ((cur, d), True)
                        heapq.heappush(pq, (nc + hh(q), nc, q, nd + 4))
        return None

    def claim(name, pts, big_pts):
        for q in pts:
            usage.setdefault(q, set()).add(name)
        for q in big_pts:
            for dx, dy in N8:
                usage.setdefault((q[0] + dx, q[1] + dy), set()).add(name)

    def route_net(n, pres, hard):
        holes = pin_pts[n.name]
        tree, remaining, segs, link_ends = {holes[0]}, holes[1:], [], set()
        claim(n.name, [holes[0]], [])
        while remaining:
            remaining.sort(key=lambda h: min(abs(h[0] - t[0]) + abs(h[1] - t[1]) for t in tree))
            tgt = remaining.pop(0)
            if tgt in tree:
                continue
            avoid = set(link_ends)
            for _ in range(8):                  # one jumper per hole: retry if a path reuses a jumper hole
                res = dijkstra(n.name, tree, tgt, pres, hard, avoid)
                if res is None:
                    return None
                hs, links = res
                ends, dup = set(), set()
                for i, is_link in enumerate(links):
                    if is_link:
                        for q in (hs[i], hs[i + 1]):
                            if q in ends or q in link_ends:
                                dup.add(q)
                            ends.add(q)
                if not dup:
                    break
                avoid |= dup
            else:
                return None
            link_ends |= ends
            claim(n.name, hs, (hs if n.name in HEAVY else []) + list(ends))
            segs.append((hs, links))
            tree.update(hs)
        return segs

    def rip(name):
        for s in usage.values():
            s.discard(name)

    results, pres = {}, 0.6
    for it in range(ITERATIONS):
        for n in nets:
            rip(n.name)
            res = route_net(n, pres, hard=False)
            if res is None:
                raise RuntimeError(f"{b.key}: {n.name} cannot be routed at all (pin boxed in?)")
            results[n.name] = res
        shared = [q for q, s in usage.items() if len(s) > 1]
        b.route_log = getattr(b, "route_log", []) + [len(shared)]
        if not shared:
            break
        for q in shared:
            hist[q] = hist.get(q, 0.0) + 0.8
        pres = min(pres * 1.5, PRES_MAX)
    b.failed = []
    for n in nets:
        if any(len(usage.get(h, set())) > 1 for hs, _ in results[n.name] for h in hs):
            rip(n.name)
            res = route_net(n, pres, hard=True)
            if res is None:
                if strict:
                    raise RuntimeError(f"{b.key}: {n.name} cannot be routed without sharing")
                res = route_net(n, pres, hard=False)
                b.failed.append(n.name)
            results[n.name] = res
    return results, g


def split(results):
    out = {}
    for name, segs in results.items():
        trails, links = [], []
        for hs, ls in segs:
            cur = [hs[0]]
            for i, is_link in enumerate(ls):
                a, c = hs[i], hs[i + 1]
                if is_link:
                    if len(cur) > 1:
                        trails.append(cur)
                    links.append((a, c))
                    cur = [c]
                else:
                    cur.append(c)
            if len(cur) > 1:
                trails.append(cur)
        out[name] = {"trails": trails, "links": links}
    return out


def verify(b: z.Board, g: Grid, routed) -> list[str]:
    """Independent check of the routed copper on the half grid."""
    msgs = []
    pts, halo = {}, {}
    for name, rr in routed.items():
        heavy = name in HEAVY
        for seg in rr["trails"]:
            for a, c in zip(seg, seg[1:]):
                if abs(a[0] - c[0]) + abs(a[1] - c[1]) != 1:
                    msgs.append(f"{name}: step {a}->{c} is not to a neighbour")
            for q in seg:
                if not g.usable(q):
                    msgs.append(f"{name}: {q} outside the copper area")
                if not g.ok(q, name, heavy):
                    msgs.append(f"{name}: track at {xy(q)} too close to a foreign pad")
                pts.setdefault(q, set()).add(name)
                if heavy:
                    for dx, dy in N8:
                        halo.setdefault((q[0] + dx, q[1] + dy), set()).add(name)
        ends = [q for lk in rr["links"] for q in lk]
        if len(ends) != len(set(ends)):
            msgs.append(f"{name}: two link ends on one point")
        for a, c in rr["links"]:
            if a[0] != c[0] and a[1] != c[1]:
                msgs.append(f"{name}: link {a}->{c} not straight")
            k = max(abs(c[0] - a[0]), abs(c[1] - a[1]))
            dx, dy = (c[0] > a[0]) - (c[0] < a[0]), (c[1] > a[1]) - (c[1] < a[1])
            for i in range(1, k):
                if (a[0] + dx * i, a[1] + dy * i) in g.no_span:
                    msgs.append(f"{name}: link {a}->{c} passes over a pad or a flat part")
                    break
            for q in (a, c):
                if q in g.no_end or q in g.pad_net:
                    msgs.append(f"{name}: link end {q} on a pad / under a part")
                if not g.ok(q, name, True):
                    msgs.append(f"{name}: link pad {q} too close to a foreign pad")
                pts.setdefault(q, set()).add(name)
                for ddx, ddy in N8:
                    halo.setdefault((q[0] + ddx, q[1] + ddy), set()).add(name)
    for q, s in pts.items():
        if len(s) > 1:
            msgs.append(f"point {xy(q)} used by {sorted(s)}")
        h = halo.get(q, set()) - s
        if h:
            msgs.append(f"point {xy(q)} of {sorted(s)} inside the clearance of {sorted(h)}")
    # connectivity
    for n in b.nets:
        parent = {}

        def find(x):
            parent.setdefault(x, x)
            while parent[x] != x:
                parent[x] = parent[parent[x]]
                x = parent[x]
            return x
        rr = routed[n.name]
        for seg in rr["trails"]:
            for a, c in zip(seg, seg[1:]):
                parent[find(a)] = find(c)
        for a, c in rr["links"]:
            parent[find(a)] = find(c)
        pins = [P(*b.parts[r].pins[p]) for r, p in n.pins]
        if len({find(q) for q in pins}) != 1:
            msgs.append(f"{n.name}: NOT connected")
    return msgs
