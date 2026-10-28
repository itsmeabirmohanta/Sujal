"""VRITRA offline map fetcher — download the launch area ONCE, at home, on wifi.

    python fetch_map.py "Phagwara, Punjab" --radius 10
    python fetch_map.py 26.7400 83.8900 --radius 8 --zoom 12-16
    python fetch_map.py --list

The ground station is a field instrument with no internet (see the module
docstring in vritra_gcs.py). It therefore cannot fetch map tiles while you are
using it, which is exactly when you need them. This script builds a local tile
cache beforehand; vritra_gcs.py's GroundTrack renders straight out of it and
never touches the network.

    map_tiles/<z>/<x>/<y>.png        standard slippy-map XYZ layout
    map_tiles/index.json             what was fetched, when, and from where

WHY THE RADIUS IS CAPPED, AND WHY THAT IS NOT A LIMITATION
    OpenStreetMap's tile usage policy forbids bulk downloading from
    tile.openstreetmap.org. That is not fine print to route around — it is a
    volunteer-funded service, and a team that scrapes it gets its IP blocked,
    usually on launch week.

    The arithmetic also says you do not need it. A 50 km radius at street zoom
    is ~30 000 tiles (about 1 GB and several hours). A CanSat released at
    1000 m under canopy, drifting in a 10 m/s wind for a 4 minute descent,
    travels about 2.4 km. A 10 km radius is already four times the worst
    credible landing dispersion, and at zoom 12-16 it is roughly 1 400 tiles —
    a few minutes, well inside anything anyone would call reasonable use.

    MAX_TILES below is the guard. Raise it only if you genuinely need to and
    understand you are the one answering for the traffic.

ZOOM LEVELS, AND WHY YOU WANT A RANGE RATHER THAN ONE
    Each level is a separate copy of the world at a different scale, and the
    renderer picks whichever suits the current view. Fetching only z16 means a
    zoomed-out view has nothing to draw; fetching only z12 means the recovery
    walk-in is a blur. 12-16 spans "whole flight line" to "which field is it in".
        z12  ~38 m/pixel   the whole launch range
        z14  ~9.5 m/pixel  roads and field boundaries
        z16  ~2.4 m/pixel  individual buildings, tree lines, tracks
"""
import argparse, json, math, os, sys, time, urllib.parse, urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
CACHE = os.path.join(HERE, "map_tiles")

# Identify the client honestly. A generic urllib User-Agent is refused outright
# by both Nominatim and the OSM tile servers, and rightly so.
UA = ("VRITRA-CanSat-GCS/1.0 (IN-SPACe CAN-7USAT team 025, Lovely Professional "
      "University; offline field map cache; contact via team submission)")

MAX_TILES = 4000           # see the docstring before raising this
SLEEP_S = 0.12             # ~8 tiles/s, polite and still quick

SOURCES = {
    # name: (url template, attribution, max sane zoom)
    "osm": ("https://tile.openstreetmap.org/{z}/{x}/{y}.png",
            "(c) OpenStreetMap contributors", 19),
    "topo": ("https://a.tile.opentopomap.org/{z}/{x}/{y}.png",
             "(c) OpenStreetMap contributors, SRTM | OpenTopoMap (CC-BY-SA)", 17),
}


# ---- Web Mercator, the projection every XYZ tile scheme uses --------------
def deg2tile(lat, lon, z):
    n = 2.0 ** z
    x = (lon + 180.0) / 360.0 * n
    la = math.radians(max(-85.05112878, min(85.05112878, lat)))
    y = (1.0 - math.asinh(math.tan(la)) / math.pi) / 2.0 * n
    return int(x), int(y)


def tile2deg(x, y, z):
    """North-west corner of tile (x, y) at zoom z."""
    n = 2.0 ** z
    lon = x / n * 360.0 - 180.0
    lat = math.degrees(math.atan(math.sinh(math.pi * (1.0 - 2.0 * y / n))))
    return lat, lon


def bbox_from_radius(lat, lon, km):
    """Square bounding box, radius in km. 1 deg latitude is ~110.574 km; the
    longitude degree shrinks with cos(lat), which at 31 N is a 14 % difference —
    ignoring it would make the box noticeably narrower than asked for."""
    dlat = km / 110.574
    dlon = km / (111.320 * max(math.cos(math.radians(lat)), 1e-6))
    return lat - dlat, lon - dlon, lat + dlat, lon + dlon


def geocode(place):
    """Place name -> (lat, lon, display name) via Nominatim."""
    url = ("https://nominatim.openstreetmap.org/search?"
           + urllib.parse.urlencode({"q": place, "format": "json", "limit": 1}))
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(req, timeout=20) as r:
        data = json.loads(r.read().decode("utf-8"))
    if not data:
        raise SystemExit("could not find a place called %r" % place)
    d = data[0]
    return float(d["lat"]), float(d["lon"]), d.get("display_name", place)


def plan(lat, lon, km, zooms):
    """-> [(z, x, y), ...] for every tile covering the box."""
    s, w, n, e = bbox_from_radius(lat, lon, km)
    out = []
    for z in zooms:
        x0, y0 = deg2tile(n, w, z)          # NW corner -> min x, min y
        x1, y1 = deg2tile(s, e, z)          # SE corner -> max x, max y
        for x in range(min(x0, x1), max(x0, x1) + 1):
            for y in range(min(y0, y1), max(y0, y1) + 1):
                out.append((z, x, y))
    return out


def fetch(source, lat, lon, km, zooms, label):
    url_tpl, attribution, zmax = SOURCES[source]
    bad_z = [z for z in zooms if z > zmax]
    if bad_z:
        raise SystemExit("source %r does not serve zoom %s (max %d)"
                         % (source, bad_z, zmax))

    tiles = plan(lat, lon, km, zooms)
    have = sum(1 for z, x, y in tiles
               if os.path.exists(os.path.join(CACHE, str(z), str(x), "%d.png" % y)))
    todo = len(tiles) - have

    print("centre    %.6f, %.6f  (%s)" % (lat, lon, label))
    print("radius    %g km   zooms %s   source %s" % (km, list(zooms), source))
    print("tiles     %d total, %d already cached, %d to download"
          % (len(tiles), have, todo))
    est = todo * SLEEP_S + todo * 0.15
    print("estimate  ~%.0f s, ~%.0f MB" % (est, todo * 0.018))

    if len(tiles) > MAX_TILES:
        raise SystemExit(
            "\nREFUSING: %d tiles is past the MAX_TILES=%d guard.\n"
            "Bulk downloading is against the OSM tile usage policy and will get\n"
            "you blocked. Reduce --radius or drop the top zoom level: one zoom\n"
            "level down is 4x fewer tiles for the same area."
            % (len(tiles), MAX_TILES))
    if not todo:
        print("\nnothing to do — the cache already covers this area.")
        return

    print()
    ok = failed = 0
    t0 = time.time()
    for i, (z, x, y) in enumerate(tiles, 1):
        d = os.path.join(CACHE, str(z), str(x))
        path = os.path.join(d, "%d.png" % y)
        if os.path.exists(path):
            continue
        os.makedirs(d, exist_ok=True)
        url = url_tpl.format(z=z, x=x, y=y)
        try:
            req = urllib.request.Request(url, headers={"User-Agent": UA})
            with urllib.request.urlopen(req, timeout=20) as r:
                blob = r.read()
            # Write via a temp name then rename: a Ctrl-C mid-write would
            # otherwise leave a truncated PNG that the renderer silently skips
            # forever, and re-running would not repair it because the file
            # exists. Same atomic-write reasoning as the GNSS aid file.
            tmp = path + ".part"
            with open(tmp, "wb") as f:
                f.write(blob)
            os.replace(tmp, path)
            ok += 1
        except Exception as e:
            failed += 1
            if failed <= 5:
                print("  failed z%d/%d/%d: %s" % (z, x, y, e))
        if ok and ok % 25 == 0:
            done = time.time() - t0
            print("  %d/%d  %.0f%%  %.0fs elapsed" % (i, len(tiles),
                                                      100.0 * i / len(tiles), done))
        time.sleep(SLEEP_S)

    s, w, n, e = bbox_from_radius(lat, lon, km)
    idx = {
        "centre": [lat, lon], "label": label, "radius_km": km,
        "zooms": list(zooms), "bounds": {"south": s, "west": w,
                                         "north": n, "east": e},
        "source": source, "attribution": attribution,
        "fetched": time.strftime("%Y-%m-%dT%H:%M:%S"),
        "tiles": len(tiles),
    }
    os.makedirs(CACHE, exist_ok=True)
    with open(os.path.join(CACHE, "index.json"), "w", encoding="utf-8") as f:
        json.dump(idx, f, indent=2)

    print("\ndownloaded %d, failed %d, in %.0f s" % (ok, failed, time.time() - t0))
    print("cache: %s" % CACHE)
    print("attribution (keep this visible if you publish a screenshot): %s"
          % attribution)
    if failed:
        print("\nRe-run the same command to retry the failures — tiles already "
              "on disk are skipped.")


def show_index():
    p = os.path.join(CACHE, "index.json")
    if not os.path.exists(p):
        print("no cache yet at %s" % CACHE)
        return
    idx = json.load(open(p, encoding="utf-8"))
    print(json.dumps(idx, indent=2))
    n = sum(len(files) for _, _, files in os.walk(CACHE))
    size = sum(os.path.getsize(os.path.join(r, f))
               for r, _, fs in os.walk(CACHE) for f in fs)
    print("\non disk: %d files, %.1f MB" % (n, size / 1e6))


def zoom_range(text):
    if "-" in text:
        a, b = text.split("-", 1)
        return list(range(int(a), int(b) + 1))
    return [int(text)]


def main():
    ap = argparse.ArgumentParser(
        description="Download an offline map tile cache for the VRITRA GCS.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='examples:\n'
               '  python fetch_map.py "Phagwara, Punjab" --radius 10\n'
               '  python fetch_map.py 26.74 83.89 --radius 8 --zoom 12-16\n'
               '  python fetch_map.py --list\n')
    ap.add_argument("place", nargs="*",
                    help='place name in quotes, or "LAT LON"')
    ap.add_argument("--radius", type=float, default=10.0,
                    help="half-width of the square area, km (default 10)")
    ap.add_argument("--zoom", default="12-16",
                    help="zoom level or range, e.g. 14 or 12-16 (default 12-16)")
    ap.add_argument("--source", default="osm", choices=sorted(SOURCES),
                    help="tile source (default osm)")
    ap.add_argument("--list", action="store_true",
                    help="show what is already cached and exit")
    a = ap.parse_args()

    if a.list or not a.place:
        show_index()
        if not a.place and not a.list:
            ap.print_help()
        return

    if len(a.place) == 2:
        try:
            lat, lon = float(a.place[0]), float(a.place[1])
            label = "%.5f, %.5f" % (lat, lon)
        except ValueError:
            lat, lon, label = geocode(" ".join(a.place))
    else:
        lat, lon, label = geocode(" ".join(a.place))

    fetch(a.source, lat, lon, a.radius, zoom_range(a.zoom), label)


if __name__ == "__main__":
    main()
