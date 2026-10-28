---
name: cansat-hfss-867-solved
description: "The 867 MHz comm-antenna HFSS study is solved, converged, and validated both shell ways"
metadata: 
  node_type: memory
  type: project
  originSessionId: c0f739fb-b77f-4e8b-bbda-2ee8de993701
  modified: 2026-08-10T00:59:25.154Z
---

As of 2026-08-10 the VRITRA 867 MHz comm-antenna HFSS study is DONE and
converged, for the as-built vehicle (RadioMaster dual-band dipole on the centre
axis, hung from the top electronics plate pointing down, ABS shell fitted).

Results, all read back from the solve:
- Peak antenna gain **+2.57 dBi** (θ=66°), horizon **+1.2 dBi**, efficiency **99.65%**.
- Isolation comm→GNSS **S21 = −49.3 dB** → passes −45 dB, so NO firmware GNSS-blanking rule needed.
- Nadir null −45.6 dBi but only ~2° wide; link survives it (+10.6 dB overhead, +57 dB horizon at 1 km).
- **Do NOT quote modelled S11/VSWR** — model has the bare 100 mm element, no matching network. Gain/efficiency/isolation/pattern are trustworthy; match is not.

Shell **Test 3 done**: solved BOTH the homogenised εr 1.83 tube and the real
4787-face perforated print — they agree to **0.04 dB** peak, ±0.02 dB across the
pattern. Homogenisation validated; the fast tube can be used for any future study.

Projects (name collision is why these names matter — see [[cansat-hfss-project-collision]]):
- `VritraCansatRf.aedt` — homogenised shell, converged ΔS 0.0034.
- `VritraRealShell.aedt` — real perforated shell, converged ΔS 0.0028.
Both backed up to `D:\CansatRfBackup`.

**All HFSS docs consolidated into ONE file 2026-08-10: `HFSS_SIMULATION.md`** at
the repo root (the fragmented HFSS_RF_STUDY / HFSS_HANDOFF / AnsysHfssGuideV2 /
CdrRf* / Rf_CodeParameters were folded in and deleted). Deliverables:
`ansys/results/VRITRA_RF_867_Report.html` (judge-ready), `Pattern867_3D.png`,
`Pattern867_Cuts.png`, `ShellCompare867.png`.

Not done (all deliberately, per user's 867-only scope): GNSS patch at 1575 MHz,
2465 MHz fallback, slotted-plate study, ground Yagi as a modelled antenna
(carried as catalogue 11 dBi in the link budget only).
