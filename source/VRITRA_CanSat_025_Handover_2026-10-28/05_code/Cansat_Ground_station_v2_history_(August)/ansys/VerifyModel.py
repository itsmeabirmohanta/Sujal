"""Check the built model against the real vehicle, value by value.  Read-only.

Every check states the expected value, where that expectation comes from, and
what the live model actually contains.  A check that cannot be evaluated says
so rather than passing quietly - a verification script that reports PASS when it
did not measure anything is worse than no script.

Sources, as used in the study notes:
  [F] measured in Fusion through its API
  [S] measured in rf structure.step
  [A] frequency allocation plan, team row 025
  [H] Ansys HFSS 2026 R1 documentation
  [D] datasheet / vendor page
"""

import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import AedtSession

C = 299792458.0
F867 = 867.00e6
LAMBDA = C / F867 * 1000.0                      # mm

app = AedtSession.Connect(create=False)

Checks = []


def Check(name, ok, actual, expected, source, note=""):
    Checks.append((name, ok, actual, expected, source, note))


def Box(n):
    """(xmin,xmax,ymin,ymax,zmin,zmax) in mm, or None."""
    try:
        b = app.modeler[n].bounding_box
        return (b[0], b[3], b[1], b[4], b[2], b[5])
    except Exception:                                          # noqa: BLE001
        return None


names = set(app.modeler.object_names)
solids = sorted(app.modeler.solid_names)

print("\n%s\nALL SOLIDS\n%s" % ("=" * 92, "=" * 92))
print("  %-16s %8s %8s %8s %8s %8s %8s  %-20s %s"
      % ("body", "xmin", "xmax", "ymin", "ymax", "zmin", "zmax",
         "material", "solve_in"))
for n in solids:
    b = Box(n)
    if not b:
        print("  %-16s <no bounding box>" % n)
        continue
    o = app.modeler[n]
    print("  %-16s %8.2f %8.2f %8.2f %8.2f %8.2f %8.2f  %-20s %s"
          % (n, b[0], b[1], b[2], b[3], b[4], b[5],
             str(o.material_name), o.solve_inside))

print("\n%s\nWAVELENGTH\n%s" % ("=" * 92, "=" * 92))
print("  f  = %.2f MHz            [A] allocation row 025, main channel"
      % (F867 / 1e6))
print("  c  = %.0f m/s" % C)
print("  lambda = c/f = %.3f mm" % LAMBDA)
print("  lambda/2 = %.2f mm, lambda/4 = %.2f mm, lambda/6 = %.2f mm"
      % (LAMBDA / 2, LAMBDA / 4, LAMBDA / 6))

# ------------------------------------------------------------------ shell
print("\n%s\n1. SHELL - does it run from the parachute bay downward?\n%s"
      % ("=" * 92, "=" * 92))
shell = None
for cand in ("CanSatShell", "ShellOuter", "Shell"):
    if cand in names:
        shell = cand
        break
if shell is None:
    shell = next((n for n in solids if "shell" in n.lower()), None)

if shell is None:
    Check("shell exists", False, "ABSENT", "one shell body", "[F]",
          "no body with 'shell' in its name")
    print("  NO SHELL BODY FOUND. Bodies present: %s" % solids)
else:
    b = Box(shell)
    od = max(b[1] - b[0], b[3] - b[2])
    z0, z1 = b[4], b[5]
    print("  body            %s" % shell)
    print("  outer diameter  %.2f mm" % od)
    print("  Z extent        %.2f .. %.2f  (%.1f mm tall)" % (z0, z1, z1 - z0))
    print("  material        %s   solve_inside=%s"
          % (app.modeler[shell].material_name, app.modeler[shell].solve_inside))
    Check("shell OD 130 mm", abs(od - 130.0) < 0.6, "%.2f mm" % od,
          "130.0 mm", "[F] cylindrical face radius 65.0")
    Check("shell bottom at base plate top (Z -50)", abs(z0 + 50.0) < 0.6,
          "%.2f" % z0, "-50.0", "[F] AEDT Measure, arc centre [0,0,-50]")
    Check("shell top where parachute bay starts (Z 150)", abs(z1 - 150.0) < 0.6,
          "%.2f" % z1, "150.0", "[F] AEDT Measure, arc centre [0,0,150]")
    Check("shell height 200 mm", abs((z1 - z0) - 200.0) < 1.0,
          "%.1f mm" % (z1 - z0), "200.0 mm", "[F]")
    # centred on the axis: the 2026-08-09 trap where an imported shell landed
    # 500 mm to the side and Validate passed anyway.
    cx, cy = (b[0] + b[1]) / 2.0, (b[2] + b[3]) / 2.0
    Check("shell centred on the vehicle axis", abs(cx) < 0.6 and abs(cy) < 0.6,
          "centre (%.2f, %.2f)" % (cx, cy), "(0, 0)", "[F]",
          "a shell floating off-axis still passes Validate")

# --------------------------------------------------------------- antenna
print("\n%s\n2. COMM ANTENNA - centre axis, hung from the top plate\n%s"
      % ("=" * 92, "=" * 92))
arms = sorted(n for n in solids if n.startswith("CommArm"))
print("  arm bodies: %s" % (arms or "NONE"))
if arms:
    xs, ys, zs = [], [], []
    for n in arms:
        b = Box(n)
        xs += [b[0], b[1]]
        ys += [b[2], b[3]]
        zs += [b[4], b[5]]
    z0, z1 = min(zs), max(zs)
    cx = (min(xs) + max(xs)) / 2.0
    cy = (min(ys) + max(ys)) / 2.0
    dia = max(max(xs) - min(xs), max(ys) - min(ys))
    total = z1 - z0
    print("  Z extent        %.2f .. %.2f   total %.1f mm" % (z0, z1, total))
    print("  axis centre     (%.3f, %.3f)   element diameter %.2f mm"
          % (cx, cy, dia))
    print("  electrical      %.4f lambda at 867 MHz" % (total / LAMBDA))
    Check("antenna on the vehicle centre axis", abs(cx) < 0.3 and abs(cy) < 0.3,
          "(%.3f, %.3f)" % (cx, cy), "(0, 0)",
          "user 2026-08-09: 'anteena in the middle of all electronnics plate'")
    Check("antenna total length 100 mm", abs(total - 100.0) < 2.5,
          "%.1f mm" % total, "100.0 mm", "[D] RadioMaster dual-band, 100 mm")
    Check("antenna hangs from the top plate underside Z 68.5",
          abs(z1 - 68.5) < 1.0, "top at Z %.2f" % z1, "68.5",
          "[S] Body1_11 underside")
    Check("antenna points DOWN (-Z)", z0 < z1 and z0 < 0,
          "tip at Z %.2f" % z0, "tip below Z 0",
          "user 2026-08-09: 'it will be mounted downward'")

# battery clearance
cells = sorted(n for n in solids if n.startswith("Cell"))
if arms and cells:
    cellTop = max(Box(n)[5] for n in cells)
    gap = min(min(Box(n)[4] for n in arms) - cellTop, 1e9)
    print("  battery cell top Z %.2f -> clearance %.1f mm" % (cellTop, gap))
    Check("clearance antenna tip to battery", gap > 2.0, "%.1f mm" % gap,
          "> 2 mm", "[S] measured", "conductive mass near the feed detunes it")

# --------------------------------------------------------------- plates
print("\n%s\n3. PLATES - the resonance that motivates the whole study\n%s"
      % ("=" * 92, "=" * 92))
plateNames = ["Body1_10", "Body1_4", "Body1_2", "Body1_1", "Body1_11", "Body1_5"]
present = [n for n in plateNames if n in names]
print("  plates present: %d of %d  %s" % (len(present), len(plateNames), present))
Check("all six plates present", len(present) == len(plateNames),
      "%d" % len(present), "6", "[S] rf structure.step")
for n in present:
    b = Box(n)
    dia = max(b[1] - b[0], b[3] - b[2])
    circ = math.pi * dia
    print("     %-10s Ø%-7.2f Z %7.2f..%7.2f   circumference %7.2f mm "
          "= %.4f lambda" % (n, dia, b[4], b[5], circ, circ / LAMBDA))
if "Body1_2" in names:
    b = Box("Body1_2")
    dia = max(b[1] - b[0], b[3] - b[2])
    ratio = math.pi * dia / LAMBDA
    Check("Ø110 plate rim is ~1 lambda at 867 MHz", 0.95 < ratio < 1.05,
          "%.4f lambda" % ratio, "~1.0",
          "[S]+[A] pi x 110 = 345.58 mm vs lambda 345.78 mm",
          "this is WHY the study exists - resonant parasitic loops")

# ------------------------------------------------------------------ pcbs
print("\n%s\n4. AVIONICS BOARDS\n%s" % ("=" * 92, "=" * 92))
pcbs = sorted(n for n in solids if n.startswith("PcbPlane"))
for n in pcbs:
    b = Box(n)
    dia = max(b[1] - b[0], b[3] - b[2])
    print("  %-12s Ø%-7.2f Z %7.2f..%7.2f  t=%.2f  %s"
          % (n, dia, b[4], b[5], b[5] - b[4], app.modeler[n].material_name))
Check("three avionics boards", len(pcbs) == 3, "%d" % len(pcbs), "3",
      "[S]+user: three stacked boards")
if pcbs:
    zs = sorted(Box(n)[4] for n in pcbs)
    Check("boards bonded flat to plate tops (26.0/53.5/78.5)",
          all(abs(a - b) < 0.6 for a, b in zip(zs, [26.0, 53.5, 78.5])),
          "%s" % [round(z, 1) for z in zs], "[26.0, 53.5, 78.5]",
          "user 2026-08-09: boards laminated directly onto the plates")

# --------------------------------------------------------------- air box
print("\n%s\n5. AIR BOX AND RADIATION BOUNDARY\n%s" % ("=" * 92, "=" * 92))
air = next((n for n in names if n.lower().startswith("air")), None)
if air:
    b = Box(air)
    print("  %s  X %.1f..%.1f  Y %.1f..%.1f  Z %.1f..%.1f"
          % (air, b[0], b[1], b[2], b[3], b[4], b[5]))
    # smallest gap from any model body to the box wall
    modelBodies = [n for n in solids if n != air]
    mx = [Box(n) for n in modelBodies]
    mx = [q for q in mx if q]
    pad = min(
        min(q[0] for q in mx) - b[0], b[1] - max(q[1] for q in mx),
        min(q[2] for q in mx) - b[2], b[3] - max(q[3] for q in mx),
        min(q[4] for q in mx) - b[4], b[5] - max(q[5] for q in mx),
    )
    print("  smallest pad from the model to the boundary: %.1f mm" % pad)
    print("  lambda/4 at 867 MHz = %.1f mm" % (LAMBDA / 4))
    Check("air pad >= lambda/4 at 867 MHz", pad >= LAMBDA / 4 - 1.0,
          "%.1f mm" % pad, ">= %.1f mm" % (LAMBDA / 4),
          "[H] radiation boundary must sit at least lambda/4 from the radiator")
    Check("air box centred on the model",
          abs((b[0] + b[1]) / 2) < 15 and abs((b[2] + b[3]) / 2) < 15,
          "centre (%.1f, %.1f)" % ((b[0] + b[1]) / 2, (b[2] + b[3]) / 2),
          "near (0, 0)", "[H]",
          "a stray off-axis body shows up here first, as a stretched box")
else:
    Check("air box exists", False, "ABSENT", "one air region", "[H]")

# ------------------------------------------------------------ excitations
print("\n%s\n6. PORTS, BOUNDARIES, SETUP\n%s" % ("=" * 92, "=" * 92))
bnames = [b.name for b in app.boundaries]
print("  boundaries: %s" % bnames)
Check("comm port present", any("CommPort" == b for b in bnames),
      str([b for b in bnames if "Comm" in b]), "CommPort", "[H]")
Check("gnss port present", any("GnssPort" == b for b in bnames),
      str([b for b in bnames if "Gnss" in b]), "GnssPort", "[H]")
Check("radiation boundary present", any("OpenAir" in b for b in bnames),
      str([b for b in bnames if "Open" in b]), "OpenAir", "[H]")
Check("no duplicate suffixed ports",
      not any(b.startswith(("CommPort_", "GnssPort_")) for b in bnames),
      str(bnames), "no *_XXXXXX names", "[H]",
      "deleting a port sheet does not delete the port")

setups = list(app.oanalysis.GetSetups() or [])
print("  setups: %s" % setups)
Check("exactly one solution setup", len(setups) == 1, str(setups), "one",
      "[H]", "create_setup auto-suffixes; duplicates all solve identically")
for st in app.setups:
    sweeps = list(app.oanalysis.GetSweeps(st.name) or [])
    print("  setup %s" % st.name)
    for k in ("Frequency", "MaxDeltaS", "MaximumPasses",
              "MinimumConvergedPasses", "UseMultiFrequency"):
        if k in st.props:
            print("     %-24s %s" % (k, st.props[k]))
    print("     sweeps                   %s" % (sweeps or "none"))
    Check("adapt frequency is 867 MHz",
          str(st.props.get("Frequency", "")).replace(" ", "").lower()
          in ("867mhz", "0.867ghz"),
          str(st.props.get("Frequency")), "867MHz",
          "[A] allocation row 025 main channel")
    Check("max delta S <= 0.02",
          float(st.props.get("MaxDeltaS", 1)) <= 0.02 + 1e-9,
          str(st.props.get("MaxDeltaS")), "<= 0.02",
          "[H] 0.01-0.02 typical for antennas")
    Check("no frequency sweep on this run", not sweeps, str(sweeps or "none"),
          "none", "team instruction: 867 MHz only",
          "the sweep is what killed the previous run")

# ---------------------------------------------------------------- summary
print("\n%s\nVERIFICATION SUMMARY\n%s" % ("=" * 92, "=" * 92))
bad = 0
for name, ok, actual, expected, source, note in Checks:
    print("  [%s] %-46s actual %-24s expect %-22s %s"
          % ("PASS" if ok else "FAIL", name, str(actual)[:24],
             str(expected)[:22], source))
    if note:
        print("         %s" % note)
    if not ok:
        bad += 1
print("\n  %d checks, %d passed, %d FAILED" % (len(Checks), len(Checks) - bad, bad))

AedtSession.Release(app)
sys.exit(1 if bad else 0)
