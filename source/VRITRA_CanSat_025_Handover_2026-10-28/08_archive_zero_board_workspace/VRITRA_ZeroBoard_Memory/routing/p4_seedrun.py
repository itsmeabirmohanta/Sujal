"""Worker: route board4_p4 for a seed range, write the best result to p4_best_<a>_<b>.json."""
import json, sys
import board4_p4 as m
a, c = int(sys.argv[1]), int(sys.argv[2])
seeds = ([None] if a == 0 else []) + list(range(a, c))
best = m.search(seeds)
if best:
    (nl, holes), sd, routed = best
    json.dump({"links": nl, "holes": holes, "seed": sd,
               "routes": {k: {"trails": v["trails"], "links": v["links"]} for k, v in routed.items()}},
              open(f"p4_best_{a}_{c}.json", "w"))
    print("BEST", sd, nl, holes)
