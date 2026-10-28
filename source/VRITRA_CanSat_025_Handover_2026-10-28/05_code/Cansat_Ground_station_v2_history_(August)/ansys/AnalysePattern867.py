"""Turn the raw 867 MHz pattern into the handful of numbers the CDR quotes.

Works on results/Pattern867.csv, which is AEDT's own far-field export, so every
number below traces to the solve rather than to a fitted curve.

One correction is applied and shown explicitly.  AEDT's RealizedGainTotal folds
in mismatch loss at the port.  The model carries NO matching network, while the
real RadioMaster part contains internal loading (that is how a 100 mm element
presents VSWR 1.25 at 867 MHz).  So realized gain understates the flight
antenna by exactly the model's mismatch loss.  Both figures are printed; the
CDR wants the mismatch-removed one, and should say so.
"""

import csv
import math
import os

HERE = os.path.dirname(os.path.abspath(__file__))
PATTERN = os.path.join(HERE, "results", "Pattern867.csv")
SUMMARY = os.path.join(HERE, "results", "Results867.csv")

S11_DB = None
with open(SUMMARY, newline="") as fh:
    for row in csv.DictReader(fh):
        if row["quantity"].startswith("S11 comm"):
            S11_DB = float(row["value"])
if S11_DB is None:
    raise SystemExit("could not read S11 from %s" % SUMMARY)

gamma = 10.0 ** (S11_DB / 20.0)
mismatch_db = -10.0 * math.log10(1.0 - gamma ** 2)
print("S11              %8.3f dB" % S11_DB)
print("|reflection|     %8.4f" % gamma)
print("power reflected  %8.1f %%" % (100.0 * gamma ** 2))
print("mismatch loss    %8.2f dB   <- removed below to get antenna gain"
      % mismatch_db)

rows = []
with open(PATTERN, newline="") as fh:
    for r in csv.DictReader(fh):
        rows.append((float(r["Phi_deg"]), float(r["Theta_deg"]),
                     float(r["RealizedGainTotal_dBi"])))
print("\npattern points   %d" % len(rows))

thetas = sorted({t for _, t, _ in rows})
phis = sorted({p for p, _, _ in rows})
print("theta            %d points, %g..%g deg" % (len(thetas), thetas[0],
                                                  thetas[-1]))
print("phi              %d points, %g..%g deg" % (len(phis), phis[0], phis[-1]))

lut = {(p, t): g for p, t, g in rows}


def Cut(phi):
    """Realized gain vs theta at one phi, nearest available cut."""
    p = min(phis, key=lambda q: abs(q - phi))
    return p, [(t, lut[(p, t)]) for t in thetas if (p, t) in lut]


def NearestTheta(cut, target):
    t, g = min(cut, key=lambda kv: abs(kv[0] - target))
    return t, g


peak_p, peak_t, peak_g = max(rows, key=lambda r: r[2])
print("\n%s\nPEAK\n%s" % ("=" * 70, "=" * 70))
print("  realized gain  %8.2f dBi  at theta=%g  phi=%g"
      % (peak_g, peak_t, peak_p))
print("  antenna gain   %8.2f dBi  (mismatch removed)" % (peak_g + mismatch_db))

print("\n%s\nDIRECTIONS THAT DECIDE THE LINK\n%s" % ("=" * 70, "=" * 70))
print("  theta is measured from +Z (up). The vehicle descends with the")
print("  antenna pointing down, so the ground station sits near theta=180.")
print()
print("  %-26s %12s %12s" % ("direction", "realized", "antenna"))
out_rows = []
for phi_want in (0.0, 90.0):
    p, cut = Cut(phi_want)
    print("  --- phi = %g deg ---" % p)
    for name, target in (("zenith    theta=0", 0.0),
                         ("theta=45", 45.0),
                         ("horizon   theta=90", 90.0),
                         ("theta=135", 135.0),
                         ("nadir     theta=180", 180.0)):
        t, g = NearestTheta(cut, target)
        print("  %-26s %9.2f dBi %9.2f dBi" % (name, g, g + mismatch_db))
        out_rows.append(["phi=%g" % p, name, "%.2f" % g,
                         "%.2f" % (g + mismatch_db)])

# Pattern statistics over the whole sphere.
gains = [g for _, _, g in rows]
print("\n%s\nSPHERE STATISTICS\n%s" % ("=" * 70, "=" * 70))
print("  max realized   %8.2f dBi" % max(gains))
print("  min realized   %8.2f dBi" % min(gains))
print("  peak-to-null   %8.2f dB" % (max(gains) - min(gains)))

# The deepest null and where it points - this is the operational risk.
null_p, null_t, null_g = min(rows, key=lambda r: r[2])
print("  deepest null   %8.2f dBi at theta=%g phi=%g"
      % (null_g, null_t, null_p))

# Coverage: what fraction of the lower hemisphere beats a useful threshold.
lower = [(p, t, g) for p, t, g in rows if t >= 90.0]
for thr in (-10.0, -6.0, -3.0, 0.0):
    n = len([1 for _, _, g in lower if g + mismatch_db >= thr])
    print("  lower hemisphere above %+5.1f dBi (antenna gain): %5.1f %%"
          % (thr, 100.0 * n / len(lower)))

path = os.path.join(HERE, "results", "Directional867.csv")
with open(path, "w", newline="") as fh:
    w = csv.writer(fh)
    w.writerow(["cut", "direction", "realized_gain_dBi", "antenna_gain_dBi"])
    w.writerows(out_rows)
print("\nwrote %s" % path)
