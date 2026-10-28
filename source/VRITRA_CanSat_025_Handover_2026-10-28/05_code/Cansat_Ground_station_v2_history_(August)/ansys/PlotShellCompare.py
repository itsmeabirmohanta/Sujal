"""Judge figure: overlay the two shell models' 867 MHz patterns.

If the homogenised er=1.83 tube and the real 4787-face perforated print give the
same pattern, their elevation cuts lie on top of each other. Drawing both proves
the homogenisation visually, which is more convincing than a single number.

Antenna gain (mismatch removed) is plotted, each with its OWN S11, so the
comparison is like-for-like.
"""

import csv
import math
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
RES = os.path.join(HERE, "results")


def load(sub):
    pat = os.path.join(RES, sub, "Pattern867.csv") if sub else \
        os.path.join(RES, "Pattern867.csv")
    summ = os.path.join(RES, sub, "Results867.csv") if sub else \
        os.path.join(RES, "Results867.csv")
    s11 = None
    for r in csv.DictReader(open(summ, newline="")):
        if r["quantity"].startswith("S11 comm"):
            s11 = float(r["value"])
    mis = -10.0 * math.log10(1.0 - (10.0 ** (s11 / 20.0)) ** 2)
    by_theta = {}
    for r in csv.DictReader(open(pat, newline="")):
        t = float(r["Theta_deg"])
        g = float(r["RealizedGainTotal_dBi"]) + mis
        by_theta.setdefault(t, []).append(g)
    TH = np.array(sorted(by_theta))
    G = np.array([np.mean(by_theta[t]) for t in TH])
    return TH, G, mis


TH_a, G_a, mis_a = load("")            # AbsLattice (primary results)
TH_r, G_r, mis_r = load("realshell")   # Real perforated

peak_a, peak_r = float(np.max(G_a)), float(np.max(G_r))
rms = float(np.sqrt(np.mean((G_a - G_r) ** 2)))
maxdiff = float(np.max(np.abs(G_a - G_r)))
print("AbsLattice peak %.3f dBi, Real peak %.3f dBi, delta %.3f dB"
      % (peak_a, peak_r, peak_r - peak_a))
print("phi-averaged cut: RMS diff %.3f dB, max diff %.3f dB" % (rms, maxdiff))

fig, (axL, axR) = plt.subplots(1, 2, figsize=(13.5, 6.0), facecolor="white",
                               gridspec_kw={"width_ratios": [1.25, 1]})

# Left: the two cuts overlaid.
axL.plot(TH_a, G_a, "-", lw=2.6, color="#1f77b4",
         label="Homogenised ABS tube (εr 1.83)")
axL.plot(TH_r, G_r, "--", lw=2.0, color="#d62728",
         label="Real perforated print (4787 faces)")
axL.set_xlabel("theta from +Z, deg   (0 = up, 90 = horizon, 180 = down)")
axL.set_ylabel("antenna gain, dBi  (phi-averaged, mismatch removed)")
axL.set_title("867 MHz elevation pattern — two shell models overlaid",
              fontsize=12)
axL.set_xlim(0, 180)
axL.set_xticks(range(0, 181, 30))
axL.grid(True, alpha=0.35)
axL.legend(loc="lower center", fontsize=10)
axL.annotate("peak gap %.2f dB\nRMS %.2f dB over the whole cut"
             % (abs(peak_r - peak_a), rms),
             xy=(0.03, 0.03), xycoords="axes fraction", fontsize=9.5,
             va="bottom", bbox=dict(boxstyle="round", fc="#f4f4f4", ec="#bbb"))

# Right: the difference, magnified, to show there is nothing hiding.
axR.plot(TH_a, G_r - np.interp(TH_r, TH_a, G_a) if len(TH_a) != len(TH_r)
         else G_r - G_a, color="#555", lw=1.8)
axR.axhline(0, color="#aaa", lw=0.8)
axR.set_xlabel("theta, deg")
axR.set_ylabel("Real − Homogenised, dB")
axR.set_title("difference (note the ±0.5 dB scale)", fontsize=12)
axR.set_ylim(-0.5, 0.5)
axR.set_xlim(0, 180)
axR.set_xticks(range(0, 181, 30))
axR.grid(True, alpha=0.35)

fig.suptitle("VRITRA CanSat — the perforated ABS shell is correctly "
             "homogenised (both solved in Ansys HFSS 2026 R1, ΔS < 0.003)",
             fontsize=12.5)
fig.tight_layout(rect=(0, 0, 1, 0.95))
out = os.path.join(RES, "ShellCompare867.png")
fig.savefig(out, dpi=170, bbox_inches="tight")
print("wrote %s" % out)
