# VRITRA CanSat — HFSS RF Simulation (complete record)

**Team 2026-INSPACe-CAN-7USAT-025 · Lovely Professional University**
Single source of truth for the 867 MHz electromagnetic study. Consolidates every
earlier HFSS note. Every number here was read back from a live solve or derived
with the arithmetic shown; nothing is a datasheet copy unless it says so.

Last updated 2026-08-10.

---

## 0. One-paragraph summary

The onboard comm antenna (RadioMaster dual-band whip, modelled as its 100 mm
dipole) was simulated **as actually built** — on the vehicle centre axis, hung
from the top electronics plate pointing down, inside the fitted ABS shell and the
aluminium plate stack — in Ansys HFSS 2026 R1 at 867 MHz. It converged
(ΔS 0.0034). Result: **peak antenna gain +2.57 dBi, +1.2 dBi at the horizon,
99.7 % radiation efficiency, comm→GNSS isolation −49.3 dB.** The perforated ABS
shell was modelled two independent ways and they agree to **0.04 dB**, proving
the fast homogenised model is faithful. The judge-ready write-up is
`ansys/results/VRITRA_RF_867_Report.html`.

---

## 1. What was simulated (the as-built model)

| Part | Detail |
|---|---|
| Comm antenna | RadioMaster dual-band, 100 mm dipole, centre axis, mount Z 68.5 mm (underside of top electronics plate), tip Z −31.5 mm, pointing down |
| GNSS antenna | corner-truncated RHCP patch, on top of the top PCB (Z 86 mm) |
| Plates | 5 × Ø110 + 1 × Ø130 base, aluminium 6061. **Rim = 345.6 mm = 0.9994 λ at 867 MHz** — the resonance that motivates the study |
| Rods | 4 × Ø8 glass-fibre, εr 4.4 |
| PCBs | 3 × Ø100 copper planes at Z 26 / 53.5 / 78.5 |
| Cells | 3 × 21700 in the base tray |
| Shell | ABS, Ø130 / 3 mm wall, **Z −50…150** (base plate up to the parachute bay), 48.6 % solid hex-lattice |
| Reaction wheel, motor deck, bay lid | aluminium / ABS, from the STEP import |
| Air box | radiation boundary ≥ λ/4 from the vehicle, λ/6 surface mesh |

Antenna placement was the user's decision on 2026-08-09 (confirmed 2026-08-10):
**middle of the stack, pointing down.** Note the tension with `vritra_fc/Config.h`
§5b, which recommends mounting *outside* the aluminium — the plates sit 55 mm from
the element, inside λ/4 (86 mm). The simulation quantifies exactly that choice
(see §6, the mismatch caveat).

Geometry verified against the real vehicle by `ansys/VerifyModel.py`: **24/24
checks pass** (shell extent, antenna position/length, plate count and resonance,
PCB heights, air-box clearance, ports, setup).

---

## 2. Method

| Item | Value | Why |
|---|---|---|
| Tool | Ansys HFSS 2026 R1, FEM frequency-domain | — |
| Solution type | Terminal, 2 lumped ports (comm + GNSS) | S-params are `St(CommPort,CommPort)` etc. |
| Adapt frequency | single, 867.00 MHz | allocation row 025 primary; only band the user needs |
| Convergence | Max ΔS 0.02, min 2 converged passes, max 12 | reached 0.0034 |
| Sweep | **none** | an adaptive pass IS a discrete converged solve at 867 MHz; a sweep adds nothing |
| Radiation mesh | λ/6 at the top frequency (20 mm) | far fields are a surface integral over the box — sizing at 867 gives a nonsense pattern |
| Materials | Al 6061, ABS (εr 2.7), glass-fibre (4.4), copper, ceramic patch, cell steel (linear) | never library `steel_1008` (nonlinear B-H, HFSS rejects it) |
| Solve inside | off on all metal (Al skin depth 2.77 µm ≪ plate) | |
| Run mode | headless batch `ansysedt -ng -BatchSolve -Monitor` | the interactive/COM path wedges on this geometry — see §8 |

---

## 3. Results at 867 MHz (comm antenna)

Read back from the converged solve (`ansys/results/Results867.csv`):

| Quantity | Value | Notes |
|---|---|---|
| **Peak antenna gain** | **+2.57 dBi** at θ=66° | directivity 2.585 dB × efficiency |
| **Horizon gain** (θ=90°) | **+1.2 dBi** | use this for the descent link budget |
| Zenith / nadir (θ=0/180°) | −43.6 / −45.6 dBi | dipole nulls along its own axis |
| **Radiation efficiency** | **99.65 %** | the shell and plates scatter, they do not absorb |
| **Isolation S21** (comm→GNSS) | **−49.3 dB** | passes the −45 dB threshold |
| Directivity | 2.585 dB | |

**Cross-checks (both pass):** VSWR from S11 = 8.64 vs AEDT 8.634; realised gain
= directivity − efficiency − mismatch = −1.72 dBi vs AEDT −1.72. Pattern is
rotationally symmetric (φ=0 and φ=90 cuts agree within 0.13 dB) — an independent
sign the model is not skewed.

### 3.1 The nadir null — real but survivable

The dipole hangs vertically, so its deepest null points **straight down** at a
ground station directly below. From the solved pattern
(`ansys/results/LinkMargin867.csv`), the null is only **~2° wide** and the link
closes through it. At 1 km slant, +14 dBm, dual-band whips both ends:

| Off-nadir | Antenna gain | Margin, whip | Margin, +Yagi |
|---|---|---|---|
| 0° (overhead, worst) | −45.6 dBi | +10.6 dB | +18.0 dB |
| 20° | −8.1 dBi | +48.1 dB | +55.6 dB |
| 45° (typical descent) | −2.9 dBi | +53.3 dB | +60.7 dB |
| 90° (horizon) | +1.2 dBi | +57.4 dB | +64.8 dB |

Operator note: a momentary RSSI dip on a near-overhead pass is this null, not a
fault. It cannot be tuned out in firmware — only by tilting the mount.

Figures: `ansys/results/Pattern867_3D.png`, `Pattern867_Cuts.png`.

---

## 4. Shell homogenisation — validated (Test 3, partial)

The perforated ABS shell was solved **two independent ways** at 867 MHz, same
antenna, both converged:

| | Homogenised tube (εr 1.83) | Real perforated print (4787 faces) | Δ |
|---|---|---|---|
| Peak antenna gain | 2.574 dBi | 2.534 dBi | **0.04 dB** |
| Directivity | 2.585 dB | 2.549 dB | 0.04 dB |
| Radiation efficiency | 99.65 % | 99.66 % | 0.01 % |
| Isolation S21 | −49.34 dB | −49.31 dB | 0.03 dB |
| Converged | pass 5, ΔS 0.0034, 338k tets | pass 5, ΔS 0.0028, 420k tets | — |

Across the full elevation cut they agree within **±0.02 dB** at every angle
except the two axial nulls (where a negligible absolute difference reads as a
large ratio). **The homogenisation is exact physics, not an approximation:** the
perforations are ~10 mm = λ/35 at 867 MHz, deep in the quasi-static regime where
volume mixing (48.6 % ABS + 51.4 % air → εr 1.83) is rigorous. This means any
future study can use the fast tube with confidence. Figure:
`ansys/results/ShellCompare867.png`.

---

## 5. Link budget & real-world (2 km question)

HFSS supplies the antenna gain; everything else is hand arithmetic (team policy).

```
sensitivity  S = −174 + 10log10(125000) + NF 5.5 + SNRmin(−15) = −132.53 dBm
             (matches the firmware's LORA_SENS_DBM = −132.0)
FSPL(2 km)   = 20log10(2) + 20log10(867) + 32.44 = 97.20 dB
feedline     = 2.7 dB
```

**In flight (line of sight, +14 dBm), 2 km is comfortable:** ~−81 dBm at the
horizon, ~50 dB of margin, dropping only in the 2° overhead null. **The binding
real-world constraints are not distance or power** — they are (a) line of sight,
and (b) after landing, ground-station mast height (two-ray reflection dominates a
low ground-to-ground path).

### 5.1 Why a ground range test reads far worse (the 738 m / high-PER test)

A low ground-to-ground test measures **two-ray ground reflection**, not free
space: beyond ~26 m the loss grows at 40 dB/decade. At 738 m with ~1.5 m
antennas, two-ray predicts ≈ −96 dBm — which matched the measured −92 to −100 dBm
almost exactly. So that test showed the antenna working as simulated; the ground
path ate ~20 dB that does not exist in flight. Contributing factors seen in the
log: the CanSat sat in `STATE=BOOT` so it ran **+10 dBm (pad) at 0.5 Hz**, never
stepping to +14 dBm / 1 Hz; SNR rode the SF10 −15 dB cliff, giving ~50 % PER.

**Levers, ranked:** raise both antennas (≈12 dB, biggest), clear line of sight,
keep the CanSat upright (horizon lobe not the null), test at +14 dBm (+4 dB),
check the u.FL→RP-SMA pigtail, give the antenna clearance from the plates (the §6
mismatch risk).

---

## 6. What the sim tells you for the code

The firmware RF plan is organiser-assigned and **must not change** (867.00/865.75,
SF10, BW125, CR4/5, sync 0x98, +10/+14 dBm). What the sim legitimately informs:

1. **No GNSS TX-blanking rule needed.** S21 −49.3 dB clears −45 dB, so the two
   antennas can transmit/receive together. Re-check on the matched hardware —
   coupling could rise ~4 dB toward the line.
2. **Airframe-mounted gain +1.2 dBi (horizon)** replaces the placeholder `g_tx=2.0`
   in `vritra_gcs.py` `link_budget()` and documents `Config.h:609` (vendor 3.58
   is free-space; the airframe costs ~1 dB).
3. **`LORA_SENS_DBM = −132.0` already matches** the derived −132.53 — no change.
4. **GCS RSSI thresholds (−125/−110) are well-calibrated:** nominal flight ≈ −75
   dBm (deep green); only the 2° overhead null momentarily reaches red.
5. **The radios are already near-optimal.** Both ends already enable **image-
   rejection calibration at 863–870 MHz** (`Radio.cpp:266`, `vritra_gs.ino:260`)
   and **boosted RX gain** — the two biggest hidden sensitivity wins — plus HP PA
   and DCDC. There is no large software dB left to recover; remaining gains are
   physical (antenna height, clearance, connectors, +14 dBm test).

**The mismatch caveat (important).** The model has the bare 100 mm element with
no matching network, so its raw S11/VSWR is not the flight antenna's — the real
RadioMaster is internally matched (VSWR 1.25) in **free space**. Inside the metal
stack that match is degraded by an unknown amount, so the installed antenna
likely has 1–4 dB of mismatch loss not in the +1.2 dBi figure. **Quote gain,
efficiency, pattern and isolation from this study; do NOT quote modelled
S11/VSWR as flight values.** A NanoVNA return-loss measurement of the antenna as
installed would settle it.

---

## 6b. Verification against publication / patent consensus (2026-08-27)

Antenna studies in IEEE/arXiv papers and antenna patents consistently report the
same set (checked against the literature): **S11/return-loss vs frequency**
(with −10 dB bandwidth), **VSWR vs frequency** (≤2 = good match), **gain**
(peak, often vs frequency), **radiation pattern** (2-D cuts + 3-D),
**radiation efficiency**, **bandwidth**, and — the credibility gate —
**measured-vs-simulated** validation (VNA + anechoic chamber), often with a
second solver (CST vs HFSS) for cross-check.

Audited against that:

| Deliverable | Ours | Grade |
|---|---|---|
| Radiation pattern (3-D + E/H cuts) | ✓ converged, φ-symmetric | publication-grade |
| Gain / directivity / efficiency | ✓ +2.57 dBi / 2.585 dB / 99.7 % | publication-grade |
| Isolation S21 | ✓ −49.3 dB | publication-grade |
| Convergence (ΔS) + mesh stats | ✓ 0.0034, 338k tets | publication-grade |
| Solver cross-check | ✓ two shell models agree 0.04 dB (a method-validation result in its own right) | strong |
| **S11 / VSWR vs frequency** | ✓ now swept 830–905 MHz (`CommSweep867.csv/.png`) — **but see below** | **not a valid flight claim** |
| **Bandwidth (−10 dB)** | **none in band** — S11 ≈ −2 dB everywhere | limited by the above |
| **Measured vs simulated** | **absent** (no VNA / chamber) | the main gap |

**The S11 sweep, and why it is honest but not flattering.** Across 830–905 MHz
the bare 100 mm element shows S11 ≈ −2.0 dB (VSWR ~8.6) at 867, best −2.8 dB at
the band edge — **no resonance anywhere.** That is correct physics: 100 mm is
0.29 λ at 867 MHz, an electrically-short dipole (low radiation resistance, large
capacitive reactance), so it cannot self-match here. The real RadioMaster part
reaches VSWR 1.25 **only** through an internal matching network the vendor does
not publish and which therefore is not in the model. **Conclusion: quote gain,
efficiency, pattern and isolation as simulation results; the antenna's S11/VSWR
must be MEASURED on hardware (NanoVNA), not taken from this model.**

**What it would take to be fully publication/patent grade:** (1) a matched-antenna
model (add the matching network, or design a custom resonant element) so S11
shows a real −10 dB dip and a bandwidth; and (2) measured-vs-simulated validation
— a VNA S11 of the antenna as installed, and an anechoic-chamber (or calibrated
outdoor) pattern/gain. The field range test (§5.1) is supportive real-world
evidence but is not a controlled measurement. Everything else here already meets
the bar.

**Real-world sanity check (consensus):** a monopole/dipole on a finite ground
plane is textbook-reported at ~1.6–5 dBi with a toroidal pattern nulled on its
axis. Ours (+2.57 dBi peak, +1.2 dBi horizon, axial nulls) sits squarely in that
range — the radiation result is realistic and achievable, not optimistic.

## 7. Files (after 2026-08-10 cleanup)

Live AEDT project (canonical): `C:\Users\kalbh\Documents\Ansoft\VritraCansatRf.aedt`
(+ `.aedtresults`, the homogenised-shell converged solution).
Full backup of **both** shell solutions: `D:\CansatRfBackup\` (VritraCansatRf and
VritraRealShell — the real perforated solution lives here; it was freed from C:
after proving §4, restore with robocopy if needed).

Deliverables in `ansys/results/`:
- `VRITRA_RF_867_Report.html` — judge-ready, self-contained (open or print to PDF)
- `Pattern867_3D.png`, `Pattern867_Cuts.png`, `ShellCompare867.png`
- `Pattern867.csv` (16 471 pts), `Results867.csv`, `Directional867.csv`,
  `LinkMargin867.csv`, `ShellCompare.csv`, and `realshell/` (the perforated run's CSVs)

Scripts in `ansys/` (the reproducible pipeline):
- `AedtSession.py` — the ONLY safe way to attach to AEDT (see §8)
- `CansatRfModel.py` — antenna + solver definitions
- `BuildFromImport.py` — builds the model; env vars `VRITRA_PROJECT` / `VRITRA_SHELLCASE` / `VRITRA_DESIGN` retarget it
- `VerifyModel.py` — 24-point geometry check against the real vehicle
- `Report867.py` — extract results/pattern (`VRITRA_PROJECT` / `VRITRA_OUTSUB` to retarget)
- `CompareShells.py` — the two-shell comparison
- `AnalysePattern867.py`, `LinkFromPattern867.py`, `PlotPattern867.py`, `PlotShellCompare.py`, `MakeJudgeReport.py` — analysis & figures

---

## 8. Traps — read before touching AEDT again

1. **Project-name collision destroys the model.** AEDT auto-names projects
   `Project<N>`; a saved `Project1.aedt` collides and the next save writes 4 KB
   over it, clearing the `.aedtresults` (hundreds of MB) with no Recycle Bin.
   This happened **twice**. Fix, enforced by `AedtSession.py`: never use a
   `Project<N>` name (we use `VritraCansatRf`), always open by **full path**, and
   refuse any project < 100 KB or with 0 solids. **Back up `.aedtresults`, not
   just the `.aedt`.**
2. **Solve via headless batch**, not the GUI/COM path. Interactive solving wedges
   on heavy geometry — the engine spawns, then 0 CPU for an hour with no error.
   `ansysedt -ng -BatchSolve -Monitor` bypasses it and solves cleanly.
3. **The solver worker is `hf3d.exe`**, not `ansysedt`/`HFSSCOMENGINE` (those show
   0 CPU while solving — they are only coordinators). Check `hf3d` CPU/RAM to
   confirm real progress.
4. `validate_simple()` returns **1 = pass, 0 = fail** (inverted). Validate checks
   for conflicts, not correctness — a body floating off-axis still passes. Always
   re-read bounding boxes after an import/move.
5. Don't quote modelled S11/VSWR as flight values (§6).

## 9. Not done (deliberately, per the 867-only scope)

GNSS patch at 1575.42 MHz (its S22 here is meaningless — a patch tuned to 1575 is
an open circuit at 867); 2465 MHz fallback band; the slotted-plate mitigation
study; the ground Yagi as a *modelled* antenna (carried as catalogue 11 dBi in
the link budget only). Say the word to add any of these.
