"""How wide is the nadir null, and does the link survive crossing it?

The comm dipole hangs vertically and radiates a null along its own axis, so the
deepest null in the whole pattern points straight down - at a ground station
directly beneath the vehicle.  Whether that matters depends on two things this
script computes from the solved pattern rather than from intuition:

  1. how many degrees wide the null actually is;
  2. whether the link budget still closes inside it.

Link-budget terms other than antenna gain are the team's own hand-computed
figures from HFSS_RF_STUDY.md section 7, reproduced here only so the margin can
be evaluated per-angle.  They are NOT re-derived.
"""

import csv
import math
import os

HERE = os.path.dirname(os.path.abspath(__file__))
PATTERN = os.path.join(HERE, "results", "Pattern867.csv")
SUMMARY = os.path.join(HERE, "results", "Results867.csv")

# ---- hand-computed link terms, from HFSS_RF_STUDY.md section 7 -------------
TX_DBM = 14.0             # flight power, allocation row 025
FEEDLINE_DB = 2.7         # 0.6 coax + 2.1 connector/mismatch
SENS_DBM = -132.53        # -174 + 10log10(125e3) + NF 5.5 + SNRmin -15
FREQ_MHZ = 867.0
GRX_WHIP = 3.58           # RadioMaster whip on the ground
GRX_YAGI = 11.0           # the allocation plan's recommended ground Yagi


def Fspl(km):
    return 20 * math.log10(km) + 20 * math.log10(FREQ_MHZ) + 32.44


S11 = None
with open(SUMMARY, newline="") as fh:
    for r in csv.DictReader(fh):
        if r["quantity"].startswith("S11 comm"):
            S11 = float(r["value"])
gamma = 10.0 ** (S11 / 20.0)
MISMATCH = -10.0 * math.log10(1.0 - gamma ** 2)

rows = []
with open(PATTERN, newline="") as fh:
    for r in csv.DictReader(fh):
        rows.append((float(r["Phi_deg"]), float(r["Theta_deg"]),
                     float(r["RealizedGainTotal_dBi"])))

# Average over phi at each theta: the vehicle spins under its parachute, so the
# phi-averaged gain is what a ground station actually experiences over a second.
by_theta = {}
for _, t, g in rows:
    by_theta.setdefault(t, []).append(g + MISMATCH)     # antenna gain
avg = {t: sum(v) / len(v) for t, v in by_theta.items()}
thetas = sorted(avg)

print("mismatch removed: %.2f dB   (model has no matching network)" % MISMATCH)
print("phi-averaged antenna gain vs theta, 867 MHz\n")
print("  %8s %10s   %s" % ("theta", "gain dBi", "off-nadir"))
for t in thetas:
    if t >= 140.0:
        print("  %8.0f %10.2f   %6.0f deg" % (t, avg[t], 180.0 - t))

peak = max(avg.values())
print("\npeak phi-averaged gain %.2f dBi" % peak)

print("\n%s\nHOW WIDE IS THE NADIR NULL\n%s" % ("=" * 72, "=" * 72))
for drop in (3.0, 6.0, 10.0, 20.0):
    edge = None
    for t in sorted(thetas, reverse=True):
        if avg[t] >= peak - drop:
            edge = t
            break
    if edge is not None:
        print("  gain recovers to peak-%.0f dB (%.2f dBi) by theta=%.0f, "
              "i.e. %.0f deg off nadir" % (drop, peak - drop, edge,
                                           180.0 - edge))

print("\n%s\nLINK MARGIN THROUGH THE NULL\n%s" % ("=" * 72, "=" * 72))
print("  Slant range fixed at 1.0 km. Margin = P_RX - sensitivity.")
print("  TX %+.0f dBm, feedline %.1f dB, FSPL(1 km) %.2f dB, sens %.2f dBm\n"
      % (TX_DBM, FEEDLINE_DB, Fspl(1.0), SENS_DBM))
print("  %-10s %10s %14s %14s" % ("off-nadir", "G_tx dBi",
                                  "margin, whip", "margin, Yagi"))
out = []
for off in (0, 2, 5, 10, 15, 20, 30, 45, 60, 90):
    t = 180.0 - off
    tt = min(thetas, key=lambda q: abs(q - t))
    g = avg[tt]
    prx_w = TX_DBM + g + GRX_WHIP - FEEDLINE_DB - Fspl(1.0)
    prx_y = TX_DBM + g + GRX_YAGI - FEEDLINE_DB - Fspl(1.0)
    m_w, m_y = prx_w - SENS_DBM, prx_y - SENS_DBM
    print("  %7.0f deg %10.2f %11.1f dB %11.1f dB" % (off, g, m_w, m_y))
    out.append([off, "%.2f" % g, "%.1f" % m_w, "%.1f" % m_y])

print("\n  Worst case is directly overhead. Even there the budget is dominated")
print("  by the 59.8 dB of nominal margin, so the question is whether the null")
print("  eats all of it - the table above answers that from the solved pattern.")

path = os.path.join(HERE, "results", "LinkMargin867.csv")
with open(path, "w", newline="") as fh:
    w = csv.writer(fh)
    w.writerow(["off_nadir_deg", "antenna_gain_dBi",
                "margin_whip_dB", "margin_yagi_dB"])
    w.writerows(out)
print("\nwrote %s" % path)
