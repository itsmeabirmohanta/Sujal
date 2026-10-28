"""Two-layer hole-grid router for the zero boards — EVERY net, power included.

Layer 0 = BOTTOM: solder trail / bare tinned wire laid hole to hole.
Layer 1 = TOP:    insulated wire laid along the holes on the component side.
A layer change happens only in an empty hole (the wire goes through it) =
a "via" in KiCad. Pin holes belong to their net on both layers.

Negotiated congestion (PathFinder): nets may share cells while costs rise,
then a hard-blocked pass legalises. If any net still cannot be routed the
function raises — nothing is ever silently left unconnected.
"""
from __future__ import annotations

import heapq

import zb_engine as z

BOTTOM, TOP = 0, 1
STEP_COST = (1.0, 1.7)        # prefer solder trails on the bottom
TURN_COST = 0.6
VIA_COST = 6.0
ITERATIONS = 60
DIRS = ((1, 0), (-1, 0), (0, 1), (0, -1))


def route2(b: z.Board):
    pin_owner = {}
    for n in b.nets:
        for ref, pad in n.pins:
            h = b.parts[ref].pins[pad]
            if h in pin_owner and pin_owner[h] != n.name:
                raise ValueError(f"pin {ref}.{pad} is in two nets: {pin_owner[h]} and {n.name}")
            pin_owner[h] = n.name
    for p in b.parts.values():
        for pad, h in p.pins.items():
            pin_owner.setdefault(h, f"NC:{p.ref}.{pad}")

    nets = sorted(b.nets, key=lambda n: z._hpwl(b, n))
    usage = {}          # (c, r, L) -> set(net)
    hist = {}

    def free_cost(node, net, pres, hard):
        c, r, L = node
        o = pin_owner.get((c, r))
        if o is not None and o != net:
            return None
        users = usage.get(node, set()) - {net}
        if users and hard:
            return None
        return (1.0 + hist.get(node, 0.0)) * (1.0 + pres * len(users))

    def dijkstra(net, sources, targets, pres, hard):
        pq, best, prev = [], {}, {}
        for s in sources:
            best[(s, -1)] = 0.0
            heapq.heappush(pq, (0.0, s, -1))
        while pq:
            cost, node, d = heapq.heappop(pq)
            if node in targets:
                path, st = [node], (node, d)
                while st in prev:
                    st = prev[st]
                    path.append(st[0])
                return path[::-1]
            if cost > best.get((node, d), 1e18):
                continue
            c, r, L = node
            moves = []
            for nd, (dc, dr) in enumerate(DIRS):
                moves.append(((c + dc, r + dr, L), nd, STEP_COST[L] + (TURN_COST if d not in (-1, nd) else 0)))
            if (c, r) not in pin_owner:                    # layer change through an empty hole
                moves.append(((c, r, 1 - L), d, VIA_COST))
            for q, nd, step in moves:
                qc, qr, qL = q
                if not (1 <= qc <= z.NC and 1 <= qr <= z.NR) or not z.usable(b, qc, qr):
                    continue
                fc = free_cost(q, net, pres, hard)
                if fc is None:
                    continue
                if qL != L:                                 # a via uses the hole on both layers
                    fc2 = free_cost((qc, qr, L), net, pres, hard)
                    if fc2 is None:
                        continue
                    fc = max(fc, fc2)
                ncost = cost + step * fc
                if ncost < best.get((q, nd), 1e18):
                    best[(q, nd)] = ncost
                    prev[(q, nd)] = (node, d)
                    heapq.heappush(pq, (ncost, q, nd))
        return None

    def route_net(n, pres, hard):
        holes = list(dict.fromkeys(b.parts[ref].pins[pad] for ref, pad in n.pins))
        tree = {(holes[0][0], holes[0][1], BOTTOM), (holes[0][0], holes[0][1], TOP)}
        remaining = holes[1:]
        paths = []
        while remaining:
            remaining.sort(key=lambda h: min(abs(h[0] - t[0]) + abs(h[1] - t[1]) for t in tree))
            tgt = remaining.pop(0)
            targets = {(tgt[0], tgt[1], BOTTOM), (tgt[0], tgt[1], TOP)}
            if targets & tree:
                continue
            path = dijkstra(n.name, tree, targets, pres, hard)
            if path is None:
                return None
            for node in path:
                if (node[0], node[1]) not in pin_owner:
                    usage.setdefault(node, set()).add(n.name)
            for i in range(1, len(path)):              # vias: mark both layers
                a, c = path[i - 1], path[i]
                if a[2] != c[2]:
                    usage.setdefault((a[0], a[1], 1 - a[2]), set()).add(n.name)
            paths.append(path)
            tree.update(path)
            tree.update({(tgt[0], tgt[1], BOTTOM), (tgt[0], tgt[1], TOP)})
        return paths

    def rip(name):
        for s in usage.values():
            s.discard(name)

    results, pres = {}, 0.6
    for _ in range(ITERATIONS):
        for n in nets:
            rip(n.name)
            res = route_net(n, pres, hard=False)
            if res is None:
                raise RuntimeError(f"{b.key}: net {n.name} has no path even with sharing allowed")
            results[n.name] = res
        shared = [q for q, s in usage.items() if len(s) > 1]
        if not shared:
            break
        for q in shared:
            hist[q] = hist.get(q, 0.0) + 0.8
        pres *= 1.6
    bad = [n for n in nets if any(len(usage.get(node, set())) > 1 for p in results[n.name] for node in p)]
    for n in bad:                                       # legalise the leftovers
        rip(n.name)
        res = route_net(n, pres, hard=True)
        if res is None:
            raise RuntimeError(f"{b.key}: net {n.name} cannot be routed without sharing a hole")
        results[n.name] = res
    return results, pin_owner


def split(results):
    """paths -> per net: bottom segments, top segments, via holes."""
    out = {}
    for name, paths in results.items():
        segs = {BOTTOM: [], TOP: []}
        vias = []
        for path in paths:
            cur = [path[0]]
            for a, c in zip(path, path[1:]):
                if a[2] != c[2]:
                    if len(cur) > 1:
                        segs[cur[0][2]].append([(n[0], n[1]) for n in cur])
                    vias.append((a[0], a[1]))
                    cur = [c]
                else:
                    cur.append(c)
            if len(cur) > 1:
                segs[cur[0][2]].append([(n[0], n[1]) for n in cur])
        out[name] = {"bottom": segs[BOTTOM], "top": segs[TOP], "vias": vias}
    return out


def verify2(b: z.Board, routed) -> list[str]:
    """Independent check: union-find per net over its own segments/vias/pins,
    and no (hole, layer) cell or via used by two nets."""
    msgs, owner = [], {}
    pin_holes = {}
    for n in b.nets:
        for ref, pad in n.pins:
            pin_holes[b.parts[ref].pins[pad]] = n.name

    def claim(cell, net):
        o = owner.get(cell)
        if o is not None and o != net:
            msgs.append(f"cell {cell} used by {o} and {net}")
        owner[cell] = net

    for n in b.nets:
        r = routed[n.name]
        parent = {}

        def find(x):
            parent.setdefault(x, x)
            while parent[x] != x:
                parent[x] = parent[parent[x]]
                x = parent[x]
            return x

        for L, key in ((BOTTOM, "bottom"), (TOP, "top")):
            for seg in r[key]:
                for a, c in zip(seg, seg[1:]):
                    if abs(a[0] - c[0]) + abs(a[1] - c[1]) != 1:
                        msgs.append(f"{n.name}: non-adjacent step {a}->{c}")
                    parent[find((a, L))] = find((c, L))
                for h in seg:
                    if h in pin_holes and pin_holes[h] != n.name:
                        msgs.append(f"{n.name}: runs through pin of {pin_holes[h]} at {h}")
                    claim((h, L), n.name)
        for h in r["vias"]:
            if h in pin_holes:
                msgs.append(f"{n.name}: via on a pin hole {h}")
            parent[find((h, BOTTOM))] = find((h, TOP))
            claim((h, BOTTOM), n.name)
            claim((h, TOP), n.name)
        holes = [b.parts[ref].pins[pad] for ref, pad in n.pins]
        for h in holes:
            parent[find((h, BOTTOM))] = find((h, TOP))
        roots = {find((h, BOTTOM)) for h in holes}
        if len(roots) != 1:
            msgs.append(f"{n.name}: NOT connected ({len(roots)} pieces)")
    return msgs
