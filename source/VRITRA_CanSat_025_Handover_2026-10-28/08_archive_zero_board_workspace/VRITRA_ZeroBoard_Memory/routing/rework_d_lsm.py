"""Board 4 rework: ICM-20948 -> Adafruit LSM6DSO32, EVERYTHING ELSE KEPT AS SOLDERED.

1. Re-create the routing the user built from (boards.board_d with the ICM, seed ROUTE_SEED["D"]).
2. Drop the ICM: prune only trail branches that served ICM pins (dead ends after the ICM is gone);
   the ICM_1V8 net disappears. Every other trail, link and part stays exactly where it is.
3. Try LSM6DSO32 positions in the free space (body clear of every part / rod / disk edge, no pin on a
   foreign trail). Front row soldered (VIN 3Vo GND SCL SDA DO CS I1 I2); the 5-pin back row is NOT
   fitted, its holes are kept free of copper.
4. Route only the new LSM connections to the kept copper of the same net; wire links only if needed.
Writes BOARD4_ROUTED.json (the frozen routing of the reworked board) and REWORK_BOARD4.md (what to
remove / add on the real board).
Run:  python rework_d_lsm.py
"""
from __future__ import annotations

import heapq
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import boards                     # noqa: E402
import zb_engine as z             # noqa: E402
import zb_router1 as r1           # noqa: E402

LSM_FP = "LSM6DSO32_hdr"
LSM_NAMES = {"1": "VIN", "2": "3Vo", "3": "GND", "4": "SCL", "5": "SDA", "6": "DO", "7": "CS", "8": "INT1",
             "9": "INT2", "10": "SCX", "11": "SDX", "12": "CS(back)", "13": "DO(back)", "14": "GND(back)"}
LSM_NETS_GND = {"1": "+3V3_B", "7": "+3V3_B", "3": "GND", "6": "GND", "4": "SCL", "5": "SDA"}   # DO→GND = 0x6A, CS→3V3 = I²C
LSM_NETS_3V3 = {"1": "+3V3_B", "7": "+3V3_B", "3": "GND", "6": "+3V3_B", "4": "SCL", "5": "SDA"}  # DO→3V3 = 0x6B
LSM_NETS = dict(LSM_NETS_GND)
STEP, TURN, LINK = 1.0, 0.6, 60.0
ORDERS = [("4", "5", "1", "7", "3", "6"), ("5", "4", "1", "7", "3", "6"), ("3", "6", "4", "5", "1", "7"),
          ("4", "5", "3", "6", "1", "7"), ("5", "4", "3", "6", "7", "1"), ("1", "7", "3", "6", "5", "4")]
OUT_JSON = os.path.join(HERE, "BOARD4_ROUTED.json")
OUT_MD = os.path.join(HERE, "REWORK_BOARD4.md")


def old_board_and_routes():
    b = boards.board_d(imu="ICM")          # the board exactly as first built
    res, _ = r1.route1(b, seed=boards.ROUTE_SEED["D"])
    routed = r1.split1(res)
    assert not r1.verify1(b, routed)
    return b, routed


def edges_of(rr):
    e = []
    for seg in rr["trails"]:
        e += [(a, c, "t") for a, c in zip(seg, seg[1:])]
    e += [(a, c, "l") for a, c in rr["links"]]
    return e


def prune(edges, terminals):
    """Remove dead-end branches whose end is not a terminal (repeat until stable)."""
    edges = list(edges)
    while True:
        deg = {}
        for a, c, _ in edges:
            deg[a] = deg.get(a, 0) + 1
            deg[c] = deg.get(c, 0) + 1
        leaves = {h for h, d in deg.items() if d == 1 and h not in terminals}
        if not leaves:
            return edges
        edges = [(a, c, k) for a, c, k in edges if a not in leaves and c not in leaves]


def connected(edges, pts):
    parent = {}

    def f(x):
        parent.setdefault(x, x)
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x
    for a, c, _ in edges:
        parent[f(a)] = f(c)
    return len({f(p) for p in pts}) <= 1


def new_board(c, r, rot):
    """board_d placement with the LSM6DSO32 as U6 and the nets rewritten (ICM gone)."""
    b = boards.board_d(imu="ICM")
    del b.parts["U6"]
    z.place_fp(b, "U6", LSM_FP, "1", c, r, rot, "LSM6DSO32 (Adafruit)", LSM_NAMES, 6.0, boards.C_SENS,
               note="I²C: CS→3V3 · solder the FRONT row only")
    b.parts["U6"].raised = True          # stands on a pin header: a wire link may run / end under it (fit it first)
    nets = []
    for n in b.nets:
        if n.name == "ICM_1V8":
            continue
        pins = [p for p in n.pins if p[0] != "U6"]
        pins += [("U6", pad) for pad, net in LSM_NETS.items() if net == n.name]
        nets.append(z.Net(n.name, pins, n.kind))
    b.nets = nets
    return b


def try_candidate(b, kept, old_links_ends, reuse=None, reuse_links=None, lsm_nets=None,
                  order=("4", "5", "1", "7", "3", "6")):
    """-> (problems or None, new routes {net: (trails, links)}, cost).
    reuse: {net: set(old trail edges no longer kept)} — stepping along them is cheap (already soldered);
    reuse_links: {net: set(old link pairs)} — an old wire link may be used again at almost no cost."""
    reuse = reuse or {}
    reuse_links = reuse_links or {}
    lsm_nets = lsm_nets or LSM_NETS
    own = {}                                   # hole -> net (kept copper)
    for name, edges in kept.items():
        for a, c, k in edges:
            own.setdefault(a, name)
            own.setdefault(c, name)
    pin_net = {}
    for n in b.nets:
        for ref, pad in n.pins:
            pin_net[b.parts[ref].pins[pad]] = n.name
    all_pins = {}
    for p in b.parts.values():
        for pad, h in p.pins.items():
            all_pins[h] = pin_net.get(h, f"NC:{p.ref}.{pad}")
    for pad, h in b.parts["U6"].pins.items():
        net = lsm_nets.get(pad)
        if h in old_links_ends:
            return "LSM pin on a wire-link hole", None, None
        if h in own and own[h] != net:
            return f"LSM {LSM_NAMES[pad]} on {own[h]} copper", None, None
        if not z.usable(b, *h):
            return "LSM pin outside copper area", None, None
    fit = z.fit_check(b)
    if fit:
        return "fit: " + fit[0], None, None
    no_end, no_span = r1.link_blocks(b)
    new_routes, cost = {}, 0.0
    occupied = dict(own)
    used_link_ends = set(old_links_ends)
    for pad in order:                                     # SCL, SDA, VIN, CS, GND, DO by default
        net = lsm_nets[pad]
        src = b.parts["U6"].pins[pad]
        lsm_holes = set(b.parts["U6"].pins.values())
        # only copper already joined to the net (kept tree + LSM paths routed so far) or a non-LSM pin
        targets = {h for h, n in occupied.items() if n == net} |                   {h for h, n in all_pins.items() if n == net and h not in lsm_holes}
        targets.discard(src)
        if occupied.get(src) == net:
            continue
        old_e = reuse.get(net, set())
        old_l = {}
        for a, c in reuse_links.get(net, set()):
            old_l.setdefault(a, []).append(c)
            old_l.setdefault(c, []).append(a)
        pq, best, prev = [(0.0, src, -1)], {(src, -1): 0.0}, {}
        found = None
        while pq:
            cst, cur, d = heapq.heappop(pq)
            if cur in targets and cur != src:
                found = (cur, d)
                break
            if cst > best.get((cur, d), 1e18):
                continue
            for nd, (dc, dr) in enumerate(r1.DIRS):
                if d >= 0 and nd == ((d % 4) ^ 1):
                    continue
                turn = TURN if d % 4 != nd and d >= 0 else 0.0
                q = (cur[0] + dc, cur[1] + dr)
                if not (1 <= q[0] <= z.NC and 1 <= q[1] <= z.NR) or not z.usable(b, *q):
                    continue
                if q in all_pins and all_pins[q] != net:
                    continue
                if q in occupied and occupied[q] != net:
                    continue
                step = 0.2 if (min(cur, q), max(cur, q)) in old_e else STEP
                nc = cst + step + turn
                if nc < best.get((q, nd), 1e18):
                    best[(q, nd)] = nc
                    prev[(q, nd)] = ((cur, d), False)
                    heapq.heappush(pq, (nc, q, nd))
            # an old wire link of this net, already fitted: reuse it
            for q in old_l.get(cur, []):
                if q in occupied and occupied[q] != net:
                    continue
                ld = r1.DIRS.index(((q[0] > cur[0]) - (q[0] < cur[0]), (q[1] > cur[1]) - (q[1] < cur[1])))
                nc = cst + 1.0
                key = (q, ld + 4)                      # arriving by a link, heading the link's way
                if nc < best.get(key, 1e18):
                    best[key] = nc
                    prev[key] = ((cur, d), True)
                    heapq.heappush(pq, (nc, q, ld + 4))
            # a new wire link straight over anything except pins / flat bodies
            if cur in no_end or cur in all_pins or d >= 4 or cur in used_link_ends:
                continue
            for nd, (dc, dr) in enumerate(r1.DIRS):
                for k in range(2, r1.JMAX + 1):
                    mid = (cur[0] + dc * (k - 1), cur[1] + dr * (k - 1))
                    if mid in no_span:
                        break
                    q = (cur[0] + dc * k, cur[1] + dr * k)
                    if not (1 <= q[0] <= z.NC and 1 <= q[1] <= z.NR) or not z.usable(b, *q):
                        break
                    if q in no_end or q in all_pins or q in used_link_ends:
                        continue
                    if q in occupied and occupied[q] != net:
                        continue
                    nc = cst + LINK + 0.5 * k
                    if nc < best.get((q, nd + 4), 1e18):
                        best[(q, nd + 4)] = nc
                        prev[(q, nd + 4)] = ((cur, d), True)
                        heapq.heappush(pq, (nc, q, nd + 4))
        if not found:
            try_candidate.last = (occupied, new_routes)
            return f"no path for LSM {LSM_NAMES[pad]}", None, None
        path, st = [(found[0], False)], found
        while st in prev:
            st, lk = prev[st]
            path.append((st[0], lk))
        path.reverse()
        hs = [h for h, _ in path]
        lks = [path[i][1] for i in range(len(path) - 1)]
        trails, links, curt = [], [], [hs[0]]
        for i, is_link in enumerate(lks):
            if is_link:
                if len(curt) > 1:
                    trails.append(curt)
                links.append((hs[i], hs[i + 1]))
                used_link_ends.update((hs[i], hs[i + 1]))
                curt = [hs[i + 1]]
            else:
                curt.append(hs[i + 1])
        if len(curt) > 1:
            trails.append(curt)
        new_routes.setdefault(net, ([], []))
        new_routes[net][0].extend(trails)
        new_routes[net][1].extend(links)
        for h in hs:
            occupied[h] = net
        cost += best[found]
    return None, new_routes, cost


def main():
    old_b, old = old_board_and_routes()
    old_icm = {}                                    # net -> holes of the old ICM pins on that net
    for n in old_b.nets:
        for ref, pad in n.pins:
            if ref == "U6":
                old_icm.setdefault(n.name, set()).add(old_b.parts["U6"].pins[pad])
    best = None
    tried = 0
    for variant, nets_v in (("DO→GND (0x6A)", LSM_NETS_GND), ("DO→3V3 (0x6B)", LSM_NETS_3V3)):
      LSM_NETS.clear()
      LSM_NETS.update(nets_v)
      for rot in (0, 180, 90, 270):
          for r in range(10, 42):
              for c in range(5, 42):
                  b = new_board(c, r, rot)
                  if z.fit_check(b):
                      continue
                  lsm_pin_net = {h: LSM_NETS.get(pad, "NC") for pad, h in b.parts["U6"].pins.items()}
                  terms = {}
                  for n in b.nets:
                      keep = {b.parts[ref].pins[pad] for ref, pad in n.pins if ref != "U6"}
                      # keep the old copper that ran to an ICM pin unless an LSM pin of another net sits there now
                      keep |= {h for h in old_icm.get(n.name, ()) if lsm_pin_net.get(h, n.name) == n.name}
                      terms[n.name] = keep
                  kept = {}
                  ok = True
                  for name, rr in old.items():
                      if name == "ICM_1V8":
                          continue
                      e = prune(edges_of(rr), terms[name])
                      if not connected(e, terms[name]):
                          ok = False
                          break
                      kept[name] = e
                  if not ok:
                      continue
                  kept_link_ends = {h for e in kept.values() for a, c, k in e if k == "l" for h in (a, c)}
                  reuse, reuse_l = {}, {}
                  for name, rr in old.items():
                      ke = {(min(a, c), max(a, c)) for a, c, k in kept.get(name, []) if k == "t"}
                      kl = {(min(a, c), max(a, c)) for a, c, k in kept.get(name, []) if k == "l"}
                      reuse[name] = {(min(a, c), max(a, c)) for a, c, k in edges_of(rr) if k == "t"} - ke
                      reuse_l[name] = {(min(a, c), max(a, c)) for a, c in rr["links"]} - kl
                  tried += 1
                  res = None
                  for order in ORDERS:
                      why, new_o, cost_o = try_candidate(b, kept, kept_link_ends, reuse, reuse_l, order=order)
                      if why:
                          continue
                      nl = sum(1 for net_, v in new_o.items() for lk in v[1]
                               if (min(lk), max(lk)) not in reuse_l.get(net_, set()))
                      if res is None or (nl, cost_o) < res[0]:
                          res = ((nl, cost_o), new_o, cost_o)
                  if res is None:
                      continue
                  _, new, cost = res
                  new = {k: v for k, v in new.items()}
                  nlinks = sum(1 for net_, v in new.items() for lk in v[1]
                               if (min(lk), max(lk)) not in reuse_l.get(net_, set()))
                  nholes = sum(len(t) for v in new.values() for t in v[0])
                  # prefer: no new links, short new trails, sensor near the board centre
                  cx = (b.parts["U6"].body[0] + b.parts["U6"].body[2]) / 2
                  cy = (b.parts["U6"].body[1] + b.parts["U6"].body[3]) / 2
                  key = (nlinks, (0 if nets_v is LSM_NETS_GND else 1), nholes + 0.02 * math.hypot(cx - 55, cy - 55))
                  print(f"  {variant} pos {(c, r, rot)}: new links {nlinks}, new/reused trail holes {nholes}", flush=True)
                  if best is None or key < best[0]:
                      best = (key, (c, r, rot), b, kept, new, dict(nets_v), variant)
    print("candidates checked:", tried)
    if best is None:
        raise SystemExit("no LSM6DSO32 position works without moving other parts")
    key, pos, b, kept, new, nets_v, variant = best
    LSM_NETS.clear()
    LSM_NETS.update(nets_v)
    print("best position", pos, variant, "new links", key[0])
    write_outputs(old_b, old, b, kept, new, pos)


def write_outputs(old_b, old, b, kept, new, pos):
    # final routed dict (kept + new) in split1 format; stubs that now lead nowhere are removed
    final = {}
    for n in b.nets:
        e = list(kept.get(n.name, []))
        if n.name in new:
            for t in new[n.name][0]:
                e += [(a, c, "t") for a, c in zip(t, t[1:])]
            e += [(a, c, "l") for a, c in new[n.name][1]]
        e = prune(e, {b.parts[ref].pins[pad] for ref, pad in n.pins})
        final[n.name] = {"trails": [[a, c] for a, c, k in e if k == "t"], "links": [(a, c) for a, c, k in e if k == "l"]}
    write_outputs.dbg = (b, final, kept, new)
    problems = r1.verify1(b, final)
    # a wire link that was already fitted and now ends under the LSM board (raised on its header) is fine
    raised_zone = set()
    x0, y0, x1, y1 = b.parts["U6"].body
    for cc in range(1, z.NC + 1):
        for rr_ in range(1, z.NR + 1):
            if x0 - 1.2 <= z.hx(cc) <= x1 + 1.2 and y0 - 1.2 <= z.hy(rr_) <= y1 + 1.2:
                raised_zone.add((cc, rr_))
    pins_all = {h for p in b.parts.values() for h in p.pins.values()}
    def ok_under_lsm(m):
        if "is on a pin or under a part" not in m:
            return False
        h = eval(m.split("link end ")[1].split(" is on")[0])
        return h in raised_zone and h not in pins_all
    problems = [m for m in problems if not ok_under_lsm(m)]
    print("verify reworked board:", problems[:5] if problems else "OK")
    if problems:
        raise SystemExit("reworked routing failed verification")
    json.dump({"lsm_pos": pos, "routes": {k: {"trails": v["trails"], "links": v["links"]} for k, v in final.items()}},
              open(OUT_JSON, "w"), indent=0)
    # what changes on the real board
    old_e = {n: {(min(a, c), max(a, c)) for a, c, k in edges_of(rr) if k == "t"} for n, rr in old.items()}
    old_l = {n: {(min(a, c), max(a, c)) for a, c in rr["links"]} for n, rr in old.items()}
    new_e = {n: {(min(t[0], t[1]), max(t[0], t[1])) for t in v["trails"]} for n, v in final.items()}
    new_l = {n: {(min(a, c), max(a, c)) for a, c in v["links"]} for n, v in final.items()}
    H = lambda h: f"C{h[0]}R{h[1]}"
    L = ["# Board 4 rework — ICM-20948 out, Adafruit LSM6DSO32 in", "",
         "Everything else stays exactly as you soldered it. Hole = C(column) R(row) as in the build list.", "",
         f"## LSM6DSO32 position (front row only, back 5-pin row NOT fitted)",
         "| pin | hole | connect to |", "|---|---|---|"]
    for pad in [str(i) for i in range(1, 10)]:
        h = b.parts["U6"].pins[pad]
        L.append(f"| {LSM_NAMES[pad]} | {H(h)} | {LSM_NETS.get(pad, 'leave open')} |")
    L += ["", "Back-row holes (SCX SDX CS DO GND) — keep empty: " +
          ", ".join(H(b.parts['U6'].pins[p]) for p in ('10', '11', '12', '13', '14')), ""]
    L += ["## REMOVE (copper that only fed the ICM — cut / desolder if already made)"]
    for n in sorted(old_e):
        gone = sorted(old_e[n] - new_e.get(n, set()))
        if gone:
            L.append(f"- {n}: " + ", ".join(f"{H(a)}–{H(c)}" for a, c in gone))
        goneL = sorted(old_l[n] - new_l.get(n, set()))
        for a, c in goneL:
            L.append(f"- {n}: WIRE LINK {H(a)} ↔ {H(c)}")
    L += ["", "## ADD (new solder trail steps / wire links for the LSM6DSO32)"]
    for n in sorted(new_e):
        add = sorted(new_e[n] - old_e.get(n, set()))
        if add:
            L.append(f"- {n}: " + ", ".join(f"{H(a)}–{H(c)}" for a, c in add))
        for a, c in sorted(new_l[n] - old_l.get(n, set())):
            L.append(f"- {n}: WIRE LINK {H(a)} ↔ {H(c)}")
    open(OUT_MD, "w", encoding="utf-8").write("\n".join(L) + "\n")
    print("wrote", OUT_JSON, "and", OUT_MD)


if __name__ == "__main__":
    main()
