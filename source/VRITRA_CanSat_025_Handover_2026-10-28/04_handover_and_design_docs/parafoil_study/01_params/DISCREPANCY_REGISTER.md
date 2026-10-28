# Discrepancy Register — CDR vs as-designed canopy
Generated 2026-10-20. Sources: CDR Tables 6/7/9 (`00_source/CDR_text.txt`),
LEparagliding `lep-out.txt` + `lines.txt`, and geometry measured from the STL meshes.

## D-01 (CRITICAL) The CDR and the zip describe TWO DIFFERENT CANOPIES

| Parameter | CDR Table 9 | LEparagliding PF072-CANSAT | Delta |
|---|---|---|---|
| Planform | rectangular | **tapered** (chord 550 -> 412.5 mm) | different shape |
| Cells | **7** | **11** | +4 |
| Ribs | 8 | 12 | +4 |
| Plan area | **0.400 m2** | **0.71 m2** flat / 0.67 m2 proj | **+78 %** |
| Span | **960 mm** | **1420 mm** flat / 1320 mm proj | **+48 %** |
| Mean chord | 420 mm | ~500 mm | +19 % |
| Aspect ratio | **2.30** | 2.82 flat / **2.62** proj | +14 % |
| Cell width | 137 mm | 142 mm | +4 % |
| Glide ratio | 2.50 | 2.70 | +8 % |
| Canopy loading | 24.5 N/m2 | **13.8 N/m2** | **-44 %** |
| Total suspension length | 960 mm | 1100 mm | +15 % |
| Riser | 90 mm | 100 mm | +11 % |
| Line count | **16** (14 main + 2 brake) | **24** (18 main + 6 brake) | +8 |
| Line material | "Nylon Thread" | UHMWPE (Dyneema) 0.4-1.2 mm | changed |

User confirmed (2026-10-19) the **11-cell LE design is the flight article**.
=> **The CDR's entire secondary-descent section is superseded and must be rewritten before FRR.**
All CDR performance figures (sink 2.55 m/s, forward 6.37 m/s, GR 2.50, W/S 24.5 N/m2) belong to
the obsolete 0.40 m2 canopy and must not be quoted as the current design.

## D-02 (HIGH) Servo part number appears three different ways
- Current brief: AGFRC **B26CLM V2**, 10.5 kgf-cm
- Project research doc: AGFRC **A20CLM**, 10.2 kgf-cm, 8.0 V rail
- CDR Table 33 + BOM: AGFRC **A20BHM** x2, Rs 9000
No torque margin can be stated until the actual part is fixed. **Open question to user.**

## D-03 (HIGH) No structural data exists in the design package
`lep-out.txt` section 17 reads *"Don't try to solve equilibrium equacions"*, so the line-load
column (0.01-0.09 kg) is **unsolved, not computed**. Line/seam/fabric margins must be derived
independently. Known line ratings (LE types table): Riser12 100 daN, Main40 40 daN,
Upper20 20 daN, BrkMid30 30 daN, BrkUp20 20 daN, BrkMain40 40 daN.

## D-04 (MEDIUM) Line diameter: bought cord is thicker than every design line
Bought: IMAGE throw line, **1.8 mm UHMWPE, ~300 daN**, 60 m.
Design: 1.2 mm risers / 0.6 mm mains / **0.4 mm uppers**.
Strength is irrelevant (300 daN where 20 daN is required); **drag is the penalty** — see
`02_octave` drag build-up. Required length 21.66 m incl. loops, so 60 m is ample either way.

## D-05 (MEDIUM) CDR internal inconsistency on primary descent rate
CDR Table 6 design target "18-22 m/s"; CDR Table 7 equilibrium sink "**14.4 m/s** at 1000 m AGL";
the sizing calculation itself substitutes V = 15 m/s. R-07 band is 15-25 m/s.
**14.4 m/s sits below the band.** Also Table 7 lists plan area 0.1134 m2 while the derivation
yields 0.1045 m2. Needs reconciling — it sets the parafoil's deployment dynamic pressure.

## D-06 (MEDIUM) Flywheel motor KV inconsistent
CDR title/BOM: EMAX 2205 **2300 KV**; the RPM calculation uses **1590 KV** (giving 17,649 RPM at
11.1 V). Readiness threshold is 5000 +/-500 RPM. Three different numbers.

## D-07 (LOW, resolved) Fabric spec does exist
CDR names **Ripstop Nylon 40D** (~38-45 g/m2 typical). Previously logged as missing.
Still unverified by measurement, and **permeability remains unspecified** — permeability governs
inflation, so inflation verdicts stay NOT VERIFIED until a sample is tested.

## D-08 (INFO) STL unit trap — verified
The LE STL files are in **centimetres**. Measured lower-surface area = 0.7057 m2 vs LE's stated
flat area 0.71 m2 (0.6 % agreement); projected 0.627-0.687 m2 vs stated 0.67 m2. Read as
millimetres the canopy is 10x small and **100x off in area**. Every geometry import must assert
area in 0.65-0.72 m2 before meshing.
This agreement also **independently validates LEparagliding's geometry output**.
Total fabric surface = 1.568 m2 (upper 0.836 + lower 0.706 + vents 0.026).
