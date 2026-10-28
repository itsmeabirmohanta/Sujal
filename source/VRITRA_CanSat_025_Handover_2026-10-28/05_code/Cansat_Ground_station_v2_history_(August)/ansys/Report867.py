"""Pull the 867 MHz numbers the CDR needs out of the solved design.

Reads the ADAPTIVE solution, not a sweep.  An adaptive pass is a discrete solve
at the adapt frequency, so "SolveAllBands : LastAdaptive" holds a real, fully
converged 867 MHz result and no interpolation is involved.

Everything printed here is read back out of AEDT.  Where a quantity cannot be
read, the script says so rather than substituting a plausible number - a results
file that quietly invents a value is worse than one with a gap in it.
"""

import csv
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import AedtSession

HERE = os.path.dirname(os.path.abspath(__file__))
# VRITRA_OUTSUB lets a second design write its CSVs into their own subfolder so
# they never overwrite the primary AbsLattice results.
OutDir = os.path.join(HERE, "results", os.environ.get("VRITRA_OUTSUB", ""))
os.makedirs(OutDir, exist_ok=True)

# VRITRA_PROJECT (bare stem) targets a specific project by full path.
_stem = os.environ.get("VRITRA_PROJECT")
_projPath = (os.path.join(r"C:\Users\kalbh\Documents\Ansoft", _stem + ".aedt")
             if _stem else None)
app = AedtSession.Connect(create=False, project_path=_projPath)

setups = list(app.oanalysis.GetSetups() or [])
if not setups:
    AedtSession.Release(app)
    raise SystemExit("no solution setup in the design")
setupName = setups[0]

solved = None
for st in app.setups:
    if st.name == setupName:
        solved = st.is_solved
print("setup %s   is_solved=%s" % (setupName, solved))
if not solved:
    AedtSession.Release(app)
    raise SystemExit("setup is NOT solved - nothing to report")

CTX = "%s : LastAdaptive" % setupName
terminal = str(app.solution_type).lower().startswith("terminal")
S = "St" if terminal else "S"
SCAT = "Terminal Solution Data" if terminal else "Modal Solution Data"
print("context %s   solution type %s   using %s() under %r"
      % (CTX, app.solution_type, S, SCAT))

Results = {}


def Grab(label, expression, category, context=None, families=None):
    """Read one expression; record the value or the reason there is none."""
    try:
        data = app.post.get_solution_data(
            expressions=expression, setup_sweep_name=CTX,
            report_category=category, context=context,
            variations=families or {"Freq": ["All"]})
    except Exception as exc:                                   # noqa: BLE001
        Results[label] = ("UNAVAILABLE", str(exc)[:150])
        print("  %-34s UNAVAILABLE  %s" % (label, str(exc)[:90]))
        return None
    if data is None:
        Results[label] = ("UNAVAILABLE", "get_solution_data returned None")
        print("  %-34s UNAVAILABLE  (None)" % label)
        return None
    # PyAEDT 1.3.0 has no SolutionData.data_real - that was an earlier API.
    # get_expression_data returns (sweep_values, data_values) as ndarrays.
    try:
        got = data.get_expression_data(expression=expression, formula="real")
        vals = list(got[1]) if isinstance(got, tuple) else list(got)
    except Exception as exc:                                   # noqa: BLE001
        Results[label] = ("UNAVAILABLE", str(exc)[:150])
        print("  %-34s UNAVAILABLE  %s" % (label, str(exc)[:90]))
        return None
    vals = [v for v in vals if v is not None]
    if not vals:
        Results[label] = ("UNAVAILABLE", "no data points")
        print("  %-34s UNAVAILABLE  (empty)" % label)
        return None
    return data, vals


print("\n" + "=" * 78)
print("MATCH AND ISOLATION AT 867 MHz")
print("=" * 78)

for label, expr in (
    ("S11 comm  dB(%s(CommPort,CommPort))" % S,
     "dB(%s(CommPort,CommPort))" % S),
    ("S22 gnss  dB(%s(GnssPort,GnssPort))" % S,
     "dB(%s(GnssPort,GnssPort))" % S),
    ("S21 isolation dB(%s(GnssPort,CommPort))" % S,
     "dB(%s(GnssPort,CommPort))" % S),
    ("VSWR comm", "VSWR(CommPort)"),
    ("VSWR gnss", "VSWR(GnssPort)"),
):
    got = Grab(label, expr, SCAT)
    if got:
        _, vals = got
        v = vals[0]
        Results[label] = ("%.3f" % v, expr)
        print("  %-34s %10.3f" % (label, v))

print("\n" + "=" * 78)
print("FAR FIELD AT 867 MHz - realized gain")
print("=" * 78)

spheres = []
try:
    spheres = [s.name for s in app.field_setups]
except Exception:                                              # noqa: BLE001
    pass
print("  far-field spheres in the design: %s" % (spheres or "none found"))

pattern = None
for sphere in ("FullSphere", "ElevationCuts", "ZenithCone"):
    if spheres and sphere not in spheres:
        continue
    got = Grab("pattern via %s" % sphere, "dB(RealizedGainTotal)",
               "Far Fields", context=sphere)
    if got:
        pattern = (sphere, got[0], got[1])
        break

if pattern is None:
    print("  NO FAR-FIELD DATA READ. Radiated fields may not have been saved.")
else:
    sphere, data, vals = pattern
    print("  read %d points from %s" % (len(vals), sphere))
    peak = max(vals)
    print("  peak realized gain            %8.2f dBi" % peak)
    Results["peak realized gain dBi"] = ("%.2f" % peak, sphere)

    # Pull theta/phi so specific directions can be quoted, not just the peak.
    theta = phi = None
    for name in ("Theta", "theta"):
        try:
            theta = list(data.variation_values(name))
            break
        except Exception:                                      # noqa: BLE001
            continue
    for name in ("Phi", "phi"):
        try:
            phi = list(data.variation_values(name))
            break
        except Exception:                                      # noqa: BLE001
            continue
    print("  theta values: %s" % (("%d from %s to %s"
                                   % (len(theta), theta[0], theta[-1]))
                                  if theta else "not available"))
    print("  phi values:   %s" % (("%d from %s to %s"
                                   % (len(phi), phi[0], phi[-1]))
                                  if phi else "not available"))

    # Write the raw pattern so the CDR figure can be plotted from real data.
    path = os.path.join(OutDir, "Pattern867.csv")
    with open(path, "w", newline="") as fh:
        w = csv.writer(fh)
        if theta and phi and len(vals) == len(theta) * len(phi):
            w.writerow(["Phi_deg", "Theta_deg", "RealizedGainTotal_dBi"])
            i = 0
            for p in phi:
                for t in theta:
                    w.writerow([p, t, "%.4f" % vals[i]])
                    i += 1
        elif theta and len(vals) == len(theta):
            w.writerow(["Theta_deg", "RealizedGainTotal_dBi"])
            for t, v in zip(theta, vals):
                w.writerow([t, "%.4f" % v])
        else:
            w.writerow(["index", "RealizedGainTotal_dBi"])
            for i, v in enumerate(vals):
                w.writerow([i, "%.4f" % v])
    print("  wrote %s (%d rows)" % (path, len(vals)))

    # Gain in the directions that decide the link.
    if theta and len(vals) == len(theta):
        def AtTheta(target):
            i = min(range(len(theta)), key=lambda k: abs(theta[k] - target))
            return theta[i], vals[i]
        for name, target in (("zenith  theta=0", 0.0),
                             ("horizon theta=90", 90.0),
                             ("nadir   theta=180", 180.0)):
            t, v = AtTheta(target)
            print("  %-28s  %8.2f dBi   (theta=%g)" % (name, v, t))
            Results["gain %s dBi" % name.split()[0]] = ("%.2f" % v,
                                                        "theta=%g" % t)

for label, expr in (("radiation efficiency", "RadiationEfficiency"),
                    ("peak directivity dB", "dB(DirTotal)")):
    got = Grab(label, expr, "Far Fields",
               context=(pattern[0] if pattern else "FullSphere"))
    if got:
        _, vals = got
        v = max(vals)
        Results[label] = ("%.4f" % v, expr)
        print("  %-34s %10.4f" % (label, v))

print("\n" + "=" * 78)
print("SUMMARY - every value read back out of AEDT")
print("=" * 78)
summary = os.path.join(OutDir, "Results867.csv")
with open(summary, "w", newline="") as fh:
    w = csv.writer(fh)
    w.writerow(["quantity", "value", "source_or_reason"])
    for k in sorted(Results):
        v, src = Results[k]
        w.writerow([k, v, src])
        print("  %-40s %-14s %s" % (k, v, str(src)[:34]))
print("\nwrote %s" % summary)

AedtSession.Release(app)
