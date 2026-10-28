"""Extract and plot S11 / VSWR vs frequency from the comm-band sweep.

Produces the frequency-domain deliverable antenna papers report. Reads the
solved 'SolveAllBands : CommBandSweep' from the live AEDT session.

Honest note baked into the plot: this is the BARE 100 mm element with no
matching network, so it shows the radiator's own (poor) match. The flight
RadioMaster part is internally matched; its real S11 must be measured, not
taken from this curve.
"""

import csv
import os
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import AedtSession

HERE = os.path.dirname(os.path.abspath(__file__))
RES = os.path.join(HERE, "results")
os.makedirs(RES, exist_ok=True)

app = AedtSession.Connect(create=False)
setup = list(app.oanalysis.GetSetups() or [])[0]
sweeps = list(app.oanalysis.GetSweeps(setup) or [])
sweep = next((s for s in sweeps if s.startswith("CommBandSweep")), sweeps[0])
ctx = "%s : %s" % (setup, sweep)
print("reading %s" % ctx)

data = app.post.get_solution_data(
    expressions=["dB(St(CommPort,CommPort))", "VSWR(CommPort)"],
    setup_sweep_name=ctx, domain="Sweep",
    report_category="Terminal Solution Data")

freqs = list(data.primary_sweep_values)          # GHz
s11 = list(data.get_expression_data("dB(St(CommPort,CommPort))")[1])
vswr = list(data.get_expression_data("VSWR(CommPort)")[1])
fmhz = [f * 1000.0 for f in freqs]

# write CSV
path = os.path.join(RES, "CommSweep867.csv")
with open(path, "w", newline="") as fh:
    w = csv.writer(fh)
    w.writerow(["Freq_MHz", "S11_dB", "VSWR"])
    for f, s, v in zip(fmhz, s11, vswr):
        w.writerow(["%.2f" % f, "%.3f" % s, "%.3f" % v])
print("wrote %s (%d points)" % (path, len(fmhz)))


def at(target: float) -> tuple:
    i = min(range(len(fmhz)), key=lambda k: abs(fmhz[k] - target))
    return fmhz[i], s11[i], vswr[i]


for band, mhz in (("main 867.00", 867.0), ("backup 865.75", 865.75)):
    f, s, v = at(mhz)
    print("  %-14s @ %.2f MHz: S11 %.2f dB, VSWR %.2f" % (band, f, s, v))
best = min(range(len(s11)), key=lambda k: s11[k])
print("  best match in band: %.2f dB (VSWR %.2f) at %.1f MHz"
      % (s11[best], vswr[best], fmhz[best]))

# plot
fig, ax1 = plt.subplots(figsize=(10, 5.2), facecolor="white")
ax1.plot(fmhz, s11, "-", color="#1f77b4", lw=2.2, label="S11 (dB)")
ax1.axhline(-10, color="#888", ls=":", lw=1, label="-10 dB (good-match line)")
ax1.axvline(867.0, color="#d62728", ls="--", lw=1, label="867.00 main")
ax1.axvline(865.75, color="#e08", ls="--", lw=1, label="865.75 backup")
ax1.set_xlabel("Frequency (MHz)")
ax1.set_ylabel("S11 (dB)", color="#1f77b4")
ax1.tick_params(axis="y", labelcolor="#1f77b4")
ax1.set_xlim(min(fmhz), max(fmhz))
ax1.grid(True, alpha=0.3)

ax2 = ax1.twinx()
ax2.plot(fmhz, vswr, "-", color="#2ca02c", lw=1.6, alpha=0.8, label="VSWR")
ax2.set_ylabel("VSWR", color="#2ca02c")
ax2.tick_params(axis="y", labelcolor="#2ca02c")

fig.suptitle("VRITRA comm antenna — S11 / VSWR vs frequency, 867 MHz band\n"
             "BARE 100 mm element, NO matching network — radiator's own match, "
             "not the flight antenna's", fontsize=11)
lines1, lab1 = ax1.get_legend_handles_labels()
lines2, lab2 = ax2.get_legend_handles_labels()
ax1.legend(lines1 + lines2, lab1 + lab2, loc="upper right", fontsize=8)
fig.tight_layout(rect=(0, 0, 1, 0.94))
out = os.path.join(RES, "CommSweep867.png")
fig.savefig(out, dpi=170, bbox_inches="tight")
print("wrote %s" % out)
AedtSession.Release(app)
