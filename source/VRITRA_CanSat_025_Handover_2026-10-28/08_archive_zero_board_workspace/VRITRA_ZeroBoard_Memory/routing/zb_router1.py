"""Single-layer hole-grid router (zero-board style) — EVERY net, power included.

All copper is on the BOTTOM: solder trails hole to hole, straight runs and
90-degree turns only. Where two lines must cross, the router puts a straight
WIRE LINK on the component side: it starts in a free hole, jumps straight over
2..JMAX holes (never over any component pin) and lands in a free hole.
Negotiated congestion, then a hard-blocked pass; raises if anything is left.
"""
from __future__ import annotations

import heapq

import zb_engine as z

STEP = 1.0
TURN = 0.6
LINK_COST = 80.0          # one wire link costs about as much as 80 holes of trail (tuned on MAIN: fewest links that verify)
LINK_PER_HOLE = 0.6
JMAX = 10
ITERATIONS = 70
DIRS = ((1, 0), (-1, 0), (0, 1), (0, -1))


def link_blocks(b: z.Board):
    """Holes a wire link may not END in (pins + anything under a part body) and
    may not PASS over (pins + bodies of parts sitting flat; only a Teensy on
    female headers leaves room for a link underneath)."""
    pins = {h for p in b.parts.values() for h in p.pins.values()} | set(getattr(b, "blocked", ()))
    no_end, no_span = set(pins), set(pins)
    for p in b.parts.values():
        if p.dashed:
            continue
        x0, y0, x1, y1 = p.body
        raised = getattr(p, "raised", False) or p.label.lower().startswith("teensy")   # module on headers: wire fits under
        for c in range(1, z.NC + 1):
            for r in range(1, z.NR + 1):
                inside = x0 - 0.2 <= z.hx(c) <= x1 + 0.2 and y0 - 0.2 <= z.hy(r) <= y1 + 0.2
                near = x0 - 1.2 <= z.hx(c) <= x1 + 1.2 and y0 - 1.2 <= z.hy(r) <= y1 + 1.2   # 1.8 mm pad
                if near and not getattr(p, "link_end_ok", False):
                    no_end.add((c, r))
                if inside and not raised:
                    no_span.add((c, r))
    return no_end, no_span


def route1(b: z.Board, strict: bool = True, seed=None):
    pin_owner = {}
    for n in b.nets:
        for ref, pad in n.pins:
            h = b.parts[ref].pins[pad]
            if h in pin_owner and pin_owner[h] != n.name:
                raise ValueError(f"pin {ref}.{pad} in two nets {pin_owner[h]} / {n.name}")
            pin_owner[h] = n.name
    for p in b.parts.values():
        for pad, h in p.pins.items():
            pin_owner.setdefault(h, f"NC:{p.ref}.{pad}")
    for h in getattr(b, "blocked", ()):           # grid points too close to an off-grid pad
        pin_owner.setdefault(h, "BLOCKED")
    nets = sorted(b.nets, key=lambda n: z._hpwl(b, n))
    if seed is not None:                 # multi-start: a different routing order, same rules
        import random
        random.Random(seed).shuffle(nets)
    usage, hist = {}, {}
    no_end, no_span = link_blocks(b)

    def cost_at(q, net, pres, hard):
        o = pin_owner.get(q)
        if o is not None and o != net:
            return None
        users = usage.get(q, set()) - {net}
        if users and hard:
            return None
        return (1.0 + hist.get(q, 0.0)) * (1.0 + pres * len(users))

    def dijkstra(net, sources, target, pres, hard, link_ends):
        pq, best, prev = [], {}, {}
        for s in sources:
            best[(s, -1)] = 0.0
            heapq.heappush(pq, (0.0, s, -1))
        while pq:
            cost, cur, d = heapq.heappop(pq)
            if cur == target:
                path, st = [(cur, False)], (cur, d)
                while st in prev:
                    st, link = prev[st]
                    path.append((st[0], link))
                path.reverse()
                # path[i] = (hole, link_used_to_reach_next)
                holes = [h for h, _ in path]
                links = [path[i][1] for i in range(len(path) - 1)]
                return holes, links
            if cost > best.get((cur, d), 1e18):
                continue
            c, r = cur
            came_by_link = d >= 4
            dd = d % 4 if d >= 0 else -1
            for nd, (dc, dr) in enumerate(DIRS):
                if dd != -1 and nd == (dd ^ 1):           # no U-turn (0<->1, 2<->3)
                    continue
                turn = TURN if dd not in (-1, nd) else 0.0
                q = (c + dc, r + dr)
                if 1 <= q[0] <= z.NC and 1 <= q[1] <= z.NR and z.usable(b, *q):
                    fc = cost_at(q, net, pres, hard)
                    if fc is not None:
                        nc = cost + (STEP + turn) * fc
                        if nc < best.get((q, nd), 1e18):
                            best[(q, nd)] = nc
                            prev[(q, nd)] = ((cur, d), False)
                            heapq.heappush(pq, (nc, q, nd))
                # wire link: straight jump; both ends in EMPTY holes outside every part,
                # never over a pin or over a part that sits flat on the board
                if cur in no_end or came_by_link or cur in link_ends:   # one link end per hole
                    continue
                for k in range(2, JMAX + 1):
                    mid = (c + dc * (k - 1), r + dr * (k - 1))
                    if mid in no_span or not (1 <= mid[0] <= z.NC and 1 <= mid[1] <= z.NR):
                        break
                    q = (c + dc * k, r + dr * k)
                    if not (1 <= q[0] <= z.NC and 1 <= q[1] <= z.NR) or not z.usable(b, *q):
                        break
                    if q in no_end or q in link_ends:
                        continue
                    fc = cost_at(q, net, pres, hard)
                    if fc is None:
                        continue
                    nc = cost + (LINK_COST + LINK_PER_HOLE * k) * fc + turn
                    if nc < best.get((q, nd + 4), 1e18):
                        best[(q, nd + 4)] = nc
                        prev[(q, nd + 4)] = ((cur, d), True)
                        heapq.heappush(pq, (nc, q, nd + 4))
        return None

    def route_net(n, pres, hard):
        holes = list(dict.fromkeys(b.parts[ref].pins[pad] for ref, pad in n.pins))
        tree, remaining, segs, link_ends = {holes[0]}, holes[1:], [], set()
        while remaining:
            remaining.sort(key=lambda h: min(abs(h[0] - t[0]) + abs(h[1] - t[1]) for t in tree))
            tgt = remaining.pop(0)
            if tgt in tree:
                continue
            res = dijkstra(n.name, tree, tgt, pres, hard, link_ends)
            if res is None:
                return None
            hs, links = res
            for i, is_link in enumerate(links):
                if is_link:
                    link_ends.update((hs[i], hs[i + 1]))
            for h in hs:
                if h not in pin_owner:
                    usage.setdefault(h, set()).add(n.name)
            segs.append((hs, links))
            tree.update(hs)
        return segs

    def rip(name):
        for s in usage.values():
            s.discard(name)

    results, pres = {}, 0.6
    for _ in range(ITERATIONS):
        for n in nets:
            rip(n.name)
            res = route_net(n, pres, hard=False)
            if res is None:
                raise RuntimeError(f"{b.key}: {n.name} unroutable even with sharing")
            results[n.name] = res
        shared = [q for q, s in usage.items() if len(s) > 1]
        if not shared:
            break
        for q in shared:
            hist[q] = hist.get(q, 0.0) + 0.8
        pres *= 1.6
    for n in nets:
        if any(len(usage.get(h, set())) > 1 for hs, _ in results[n.name] for h in hs):
            rip(n.name)
            res = route_net(n, pres, hard=True)
            if res is None:
                if strict:
                    raise RuntimeError(f"{b.key}: {n.name} cannot be routed without sharing a hole")
                res = route_net(n, pres, hard=False)      # debug: keep the shared route so it can be drawn
                b.failed = getattr(b, "failed", []) + [n.name]
            results[n.name] = res
    return results, pin_owner


def split1(results):
    """-> per net: trails (lists of adjacent holes) and links ((a, b) hole pairs)."""
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


def verify1(b: z.Board, routed) -> list[str]:
    msgs, owner, pin_holes = [], {}, {}
    for n in b.nets:
        for ref, pad in n.pins:
            pin_holes[b.parts[ref].pins[pad]] = n.name
    all_pins = {h for p in b.parts.values() for h in p.pins.values()}
    no_end, no_span = link_blocks(b)
    for n in b.nets:
        rr = routed[n.name]
        parent = {}

        def find(x):
            parent.setdefault(x, x)
            while parent[x] != x:
                parent[x] = parent[parent[x]]
                x = parent[x]
            return x

        ends = [h for lk in rr["links"] for h in lk]
        if len(ends) != len(set(ends)):
            msgs.append(f"{n.name}: two link ends share one hole")
        for seg in rr["trails"]:
            if len(seg) != len(set(seg)):
                msgs.append(f"{n.name}: trail doubles back on itself {seg[:4]}...")
            for a, c in zip(seg, seg[1:]):
                if abs(a[0] - c[0]) + abs(a[1] - c[1]) != 1:
                    msgs.append(f"{n.name}: trail step not to a neighbour {a}->{c}")
                parent[find(a)] = find(c)
            for h in seg:
                if h in getattr(b, "blocked", ()):
                    msgs.append(f"{n.name}: trail through blocked point {h}")
                if h in pin_holes and pin_holes[h] != n.name:
                    msgs.append(f"{n.name}: trail through pin of {pin_holes[h]} at {h}")
                if owner.get(h, n.name) != n.name:
                    msgs.append(f"hole {h} used by {owner[h]} and {n.name}")
                owner[h] = n.name
        for a, c in rr["links"]:
            if a[0] != c[0] and a[1] != c[1]:
                msgs.append(f"{n.name}: link not straight {a}->{c}")
            dc = (c[0] > a[0]) - (c[0] < a[0])
            dr = (c[1] > a[1]) - (c[1] < a[1])
            k = max(abs(c[0] - a[0]), abs(c[1] - a[1]))
            for i in range(1, k):
                if (a[0] + dc * i, a[1] + dr * i) in no_span:
                    msgs.append(f"{n.name}: link {a}->{c} passes over a pin or a flat part")
            for h in (a, c):
                if h in no_end:
                    msgs.append(f"{n.name}: link end {h} is on a pin or under a part")
                if h in pin_holes and pin_holes[h] != n.name:
                    msgs.append(f"{n.name}: link lands on pin of {pin_holes[h]}")
                if owner.get(h, n.name) != n.name:
                    msgs.append(f"hole {h} used by {owner[h]} and {n.name}")
                owner[h] = n.name
            parent[find(a)] = find(c)
        holes = [b.parts[ref].pins[pad] for ref, pad in n.pins]
        if len({find(h) for h in holes}) != 1:
            msgs.append(f"{n.name}: NOT connected")
    return msgs
