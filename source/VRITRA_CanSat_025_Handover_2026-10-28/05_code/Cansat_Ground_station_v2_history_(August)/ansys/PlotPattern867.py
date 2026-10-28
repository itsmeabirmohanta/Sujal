"""Draw the 867 MHz radiation pattern from the solved far field.

Plots straight from results/Pattern867.csv - AEDT's own export - so the figures
carry the solve, not a redrawn impression of it.

Two conventions worth stating on the figure itself, because both are easy to
misread:

  * theta is measured from +Z (up).  The comm dipole hangs downward, so the
    ground station during a near-overhead descent sits at theta near 180.
  * radius is ANTENNA gain, i.e. AEDT's realized gain with the model's mismatch
    loss added back.  The model has no matching network; the real RadioMaster
    part is internally loaded to VSWR 1.25.  Plotting realized gain would show
    a 4.3 dB hole that the flight hardware does not have.
"""

import csv
import math
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib import cm

HERE = os.path.dirname(os.path.abspath(__file__))
RES = os.path.join(HERE, "results")
PATTERN = os.path.join(RES, "Pattern867.csv")
SUMMARY = os.path.join(RES, "Results867.csv")

S11 = None
for r in csv.DictReader(open(SUMMARY, newline="")):
    if r["quantity"].startswith("S11 comm"):
        S11 = float(r["value"])
gamma = 10.0 ** (S11 / 20.0)
MISMATCH = -10.0 * math.log10(1.0 - gamma ** 2)

phis, thetas, vals = [], [], []
for r in csv.DictReader(open(PATTERN, newline="")):
    phis.append(float(r["Phi_deg"]))
    thetas.append(float(r["Theta_deg"]))
    vals.append(float(r["RealizedGainTotal_dBi"]) + MISMATCH)

PH = np.array(sorted(set(phis)))
TH = np.array(sorted(set(thetas)))
G = np.full((len(TH), len(PH)), np.nan)
pi = {v: i for i, v in enumerate(PH)}
ti = {v: i for i, v in enumerate(TH)}
for p, t, g in zip(phis, thetas, vals):
    G[ti[t], pi[p]] = g

peak = np.nanmax(G)
# 25 dB is the conventional display range for an antenna pattern.  At the full
# 48 dB peak-to-null the two axial nulls shrink to pinpoints and every other
# direction saturates to one colour, which hides the shape the plot exists to
# show.  The nulls themselves are quantified in LinkMargin867.csv, not read off
# the picture.
DYN = 25.0
FLOOR = peak - DYN
Gc = np.clip(G, FLOOR, None)
print("867 MHz antenna gain: peak %.2f dBi, display floor %.2f dBi (%.0f dB range)"
      % (peak, FLOOR, DYN))

TT, PP = np.meshgrid(np.radians(TH), np.radians(PH), indexing="ij")
R = Gc - FLOOR                                   # radius >= 0
X = R * np.sin(TT) * np.cos(PP)
Y = R * np.sin(TT) * np.sin(PP)
Z = R * np.cos(TT)

norm = plt.Normalize(vmin=float(np.nanmin(Gc)), vmax=float(peak))
colors = cm.jet(norm(Gc))

# ----------------------------------------------------------- 3D pattern
fig = plt.figure(figsize=(9.5, 8.2), facecolor="white")
ax = fig.add_subplot(111, projection="3d")
ax.plot_surface(X, Y, Z, facecolors=colors, rstride=1, cstride=1,
                linewidth=0, antialiased=True, shade=False)
lim = float(np.nanmax(R)) * 0.78
ax.set_xlim(-lim, lim)
ax.set_ylim(-lim, lim)
ax.set_zlim(-lim, lim)
ax.set_box_aspect((1, 1, 1))
ax.set_xlabel("X", labelpad=2)
ax.set_ylabel("Y", labelpad=2)
ax.set_zlabel("Z  (up)", labelpad=2)
ax.tick_params(labelsize=7)
# Slightly below the equator so the nadir null is visible, not hidden underneath.
ax.view_init(elev=12, azim=-128)
ax.set_title("VRITRA CanSat - comm antenna radiation pattern, 867 MHz\n"
             "antenna gain dBi, %.0f dB display range   "
             "(peak %.2f dBi at theta = 66 deg)" % (DYN, peak),
             fontsize=12, pad=14)
m = cm.ScalarMappable(cmap=cm.jet, norm=norm)
m.set_array([])
cb = fig.colorbar(m, ax=ax, shrink=0.62, pad=0.10)
cb.set_label("gain (dBi)")

# Mark the axis the dipole lies along - both nulls sit on it.
ax.plot([0, 0], [0, 0], [-lim * 1.02, lim * 1.02], color="k", lw=1.0, ls=":")
ax.text(0, 0, lim * 1.06, "zenith\n%.1f dBi" % G[0, 0], fontsize=7.5,
        ha="center", color="#222")
ax.text(0, 0, -lim * 1.20, "nadir  %.1f dBi\n(ground station on a\n"
        "near-overhead pass)" % G[-1, 0], fontsize=7.5, ha="center",
        color="#222")
p3 = os.path.join(RES, "Pattern867_3D.png")
fig.savefig(p3, dpi=170, bbox_inches="tight")
plt.close(fig)
print("wrote %s" % p3)

# ------------------------------------------------------ polar cuts
fig = plt.figure(figsize=(13.5, 6.2), facecolor="white")

axp = fig.add_subplot(121, projection="polar")
for phi_want, style, lab in ((0.0, "-", "phi = 0 deg"),
                             (90.0, "--", "phi = 90 deg")):
    j = int(np.argmin(np.abs(PH - phi_want)))
    cut = Gc[:, j]
    ang = np.radians(TH)
    axp.plot(ang, cut, style, lw=2, label=lab)
    axp.plot(-ang, cut, style, lw=2, color=axp.lines[-1].get_color())
axp.set_theta_zero_location("N")
axp.set_theta_direction(-1)
axp.set_rlabel_position(135)
axp.set_ylim(FLOOR, peak + 2)
axp.set_title("Elevation cut - theta from +Z (up)\n"
              "0 deg = zenith, 180 deg = straight down", fontsize=11, pad=14)
axp.legend(loc="lower left", bbox_to_anchor=(-0.12, -0.06), fontsize=9)
axp.grid(True, alpha=0.4)

axh = fig.add_subplot(122)
im = axh.pcolormesh(PH, TH, Gc, cmap="jet", shading="auto",
                    vmin=float(np.nanmin(Gc)), vmax=float(peak))
axh.set_xlabel("phi (deg)")
axh.set_ylabel("theta (deg)   0 = up, 180 = down")
axh.invert_yaxis()
axh.set_title("Gain over the full sphere, 867 MHz", fontsize=11)
axh.axhline(90, color="w", lw=0.9, ls=":")
axh.axhline(180, color="w", lw=0.9, ls=":")
axh.text(6, 178, "nadir - deep null", color="w", fontsize=8, va="bottom")
axh.text(6, 88, "horizon", color="w", fontsize=8, va="bottom")
cb = fig.colorbar(im, ax=axh)
cb.set_label("gain (dBi)")

fig.suptitle("VRITRA CanSat comm antenna, 867 MHz - solved in Ansys HFSS "
             "2026 R1, converged Delta S 0.0034", fontsize=12)
fig.tight_layout(rect=(0, 0, 1, 0.94))
p2 = os.path.join(RES, "Pattern867_Cuts.png")
fig.savefig(p2, dpi=170, bbox_inches="tight")
plt.close(fig)
print("wrote %s" % p2)
