"""Extract the real-perforated-shell 867 MHz result and compare to AbsLattice.

Run AFTER the VritraRealShell batch solve finishes, against a gRPC AEDT that has
VritraRealShell.aedt open. Reads the homogenised-shell numbers back from
results/Results867.csv (written by Report867.py) and prints the side-by-side.

The whole point: if peak gain and efficiency agree within a fraction of a dB,
the homogenised ABS tube (er 1.83) is proven to reproduce the perforated print,
and the fast model is trustworthy. That agreement IS the judge-ready result.
"""

import csv
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import AedtSession

HERE = os.path.dirname(os.path.abspath(__file__))
RES = os.path.join(HERE, "results")
ABS_SUMMARY = os.path.join(RES, "Results867.csv")

REAL_PROJECT = r"C:\Users\kalbh\Documents\Ansoft\VritraRealShell.aedt"


def ReadAbs():
    d = {}
    with open(ABS_SUMMARY, newline="") as fh:
        for r in csv.DictReader(fh):
            d[r["quantity"]] = r["value"]
    return d


def Val(data, expr, formula="real"):
    got = data.get_expression_data(expression=expr, formula=formula)
    vals = list(got[1]) if isinstance(got, tuple) else list(got)
    vals = [v for v in vals if v is not None]
    return vals


app = AedtSession.Connect(create=False, project_path=REAL_PROJECT)

setups = list(app.oanalysis.GetSetups() or [])
setupName = setups[0]
solved = any(st.name == setupName and st.is_solved for st in app.setups)
print("real-shell setup %s  is_solved=%s" % (setupName, solved))
if not solved:
    AedtSession.Release(app)
    raise SystemExit("real shell not solved yet")

CTX = "%s : LastAdaptive" % setupName
SCAT = "Terminal Solution Data"

# S-parameters at 867 MHz.
sp = app.post.get_solution_data(
    expressions=["dB(St(CommPort,CommPort))", "dB(St(GnssPort,CommPort))"],
    setup_sweep_name=CTX, report_category=SCAT, variations={"Freq": ["All"]})
s11 = Val(sp, "dB(St(CommPort,CommPort))")[0]
s21 = Val(sp, "dB(St(GnssPort,CommPort))")[0]

# Far field: peak realized gain, efficiency, directivity over the full sphere.
ff = app.post.get_solution_data(
    expressions="dB(RealizedGainTotal)", setup_sweep_name=CTX,
    report_category="Far Fields", context="FullSphere",
    variations={"Theta": ["All"], "Phi": ["All"], "Freq": ["867MHz"]})
gains = Val(ff, "dB(RealizedGainTotal)")
peak_realized = max(gains)

eff = app.post.get_solution_data(
    expressions="RadiationEfficiency", setup_sweep_name=CTX,
    report_category="Far Fields", context="FullSphere",
    variations={"Theta": ["All"], "Phi": ["All"], "Freq": ["867MHz"]})
efficiency = max(Val(eff, "RadiationEfficiency"))

dr = app.post.get_solution_data(
    expressions="dB(DirTotal)", setup_sweep_name=CTX,
    report_category="Far Fields", context="FullSphere",
    variations={"Theta": ["All"], "Phi": ["All"], "Freq": ["867MHz"]})
directivity = max(Val(dr, "dB(DirTotal)"))

gamma = 10.0 ** (s11 / 20.0)
mismatch = -10.0 * math.log10(1.0 - gamma ** 2)
peak_antenna = peak_realized + mismatch

AedtSession.Release(app)

# ---- compare ----
a = ReadAbs()
absPeakRealized = float(a.get("peak realized gain dBi", "nan"))
absS11 = float(a.get("S11 comm  dB(St(CommPort,CommPort))", "nan"))
absS21 = float(a.get("S21 isolation dB(St(GnssPort,CommPort))", "nan"))
absEff = float(a.get("radiation efficiency", "nan"))
absDir = float(a.get("peak directivity dB", "nan"))
absGammaMis = -10.0 * math.log10(1.0 - (10.0 ** (absS11 / 20.0)) ** 2)
absPeakAntenna = absPeakRealized + absGammaMis

rows = [
    ("peak antenna gain (dBi)", absPeakAntenna, peak_antenna),
    ("peak realized gain (dBi)", absPeakRealized, peak_realized),
    ("directivity (dB)", absDir, directivity),
    ("radiation efficiency", absEff, efficiency),
    ("isolation S21 (dB)", absS21, s21),
]

print("\n" + "=" * 74)
print("SHELL MODEL COMPARISON, 867 MHz")
print("=" * 74)
print("  %-30s %14s %14s %10s" %
      ("quantity", "AbsLattice", "Real perf.", "delta"))
out = []
for name, av, rv in rows:
    d = rv - av
    print("  %-30s %14.3f %14.3f %+10.3f" % (name, av, rv, d))
    out.append([name, "%.3f" % av, "%.3f" % rv, "%+.3f" % d])

gain_delta = abs((peak_antenna) - (absPeakAntenna))
print("\n  peak antenna-gain difference: %.3f dB" % gain_delta)
if gain_delta < 0.5:
    print("  -> AGREE within %.2f dB. Homogenisation validated: the fast "
          "er=1.83 tube reproduces the perforated print." % gain_delta)
else:
    print("  -> difference %.2f dB is larger than expected; investigate before "
          "quoting the homogenised model." % gain_delta)

path = os.path.join(RES, "ShellCompare.csv")
with open(path, "w", newline="") as fh:
    w = csv.writer(fh)
    w.writerow(["quantity", "AbsLattice", "RealPerforated", "delta"])
    w.writerows(out)
print("\nwrote %s" % path)
