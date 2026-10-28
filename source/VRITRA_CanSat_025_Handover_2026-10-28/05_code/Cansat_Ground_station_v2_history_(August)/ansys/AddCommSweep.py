"""Add a narrow interpolating sweep around 867 MHz to the converged model.

The full 800-1700 MHz sweep took ~3 h and killed a run. This one spans only the
assigned 868-band channels (865.75 backup, 867.00 main) with margin, so it is
cheap and gives the S11/VSWR-vs-frequency curve that antenna papers report -
rather than the single adaptive point we had.

The adaptive mesh is already converged, so the batch solve reuses it and only
solves the sweep. Read-only except for adding the sweep and saving.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import AedtSession

START_MHZ = 830.0
STOP_MHZ = 905.0
POINTS = 76                      # 1 MHz step
SWEEP_NAME = "CommBandSweep"

app = AedtSession.Connect(create=False)

setups = list(app.oanalysis.GetSetups() or [])
setup_name = setups[0]
print("setup: %s" % setup_name)

existing = list(app.oanalysis.GetSweeps(setup_name) or [])
for s in existing:
    if s.startswith(SWEEP_NAME):
        app.oanalysis.DeleteSweep(setup_name, s)
        print("  removed stale sweep %s" % s)

app.create_linear_count_sweep(
    setup=setup_name,
    unit="MHz",
    start_frequency=START_MHZ,
    stop_frequency=STOP_MHZ,
    num_of_freq_points=POINTS,
    name=SWEEP_NAME,
    sweep_type="Interpolating",
    save_fields=False,
    save_rad_fields=False,
)
live = list(app.oanalysis.GetSweeps(setup_name) or [])
print("  added sweep; setup now has: %s" % live)

app.save_project()
print("saved. Ready for batch solve of the sweep.")
AedtSession.Release(app)
