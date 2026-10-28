# VRITRA · CAN-7USAT Ground Control Station — v5.0

**Team 2026-INSPACe-CAN-7USAT-025 · Lovely Professional University**
IN-SPACe CAN-7USAT India Student Competition 2026 (3rd Edition)
Mission Requirement & Competition Guidelines — Doc. IN-SPACe.CAN.MR.CG.01

[![CI](https://github.com/itsmeabirmohanta/vritra-gcs/actions/workflows/ci.yml/badge.svg)](https://github.com/itsmeabirmohanta/vritra-gcs/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Release](https://img.shields.io/github/v/release/itsmeabirmohanta/vritra-gcs)](https://github.com/itsmeabirmohanta/vritra-gcs/releases/latest)

A native Windows desktop ground station. Single source file, frozen into one
standalone `VRITRA_GCS.exe` via PyInstaller. No browser, no web server, no GPU,
no internet, and no Python needed to run the built executable.

---

## 1. Build and run

**To run from source**

```bash
python -m pip install PyQt5 pyqtgraph pyserial numpy
```

```bash
python gcs/vritra_gcs.py
```

**To build the standalone executable** — double-click `gcs\build_exe.bat`. It
installs the build dependencies and freezes the app (~2–4 minutes, once).
Result: `gcs\dist\VRITRA_GCS.exe`, which runs on any Windows PC with no Python.

Python 3.10+ is required only to *build*. Verified on Python 3.13.14 with
PyQt5 5.15.11, pyqtgraph 0.14.0, numpy 2.5.1, pyserial 3.5.

> **No OpenGL.** `PyOpenGL` and `pyqtgraph.opengl` are deliberately excluded.
> The 3-D CanSat is drawn with a pure-QPainter software renderer, so the app
> cannot raise the `GLError 1282 / glClearColor invalid operation` crash seen
> on machines without a dedicated GPU. It runs on any laptop, including VMs.

---

## 2. What changed in v5, and three corrections you should know about

### 2.1 Removed
Image and video downlink are gone: the CAMERAS tab, the SD-card video player,
the MJPEG viewers and the LoRa JPEG chunk reassembler. `CMD_CAM_REC_ON` /
`CMD_CAM_REC_OFF` are removed from the uplink command set.

**`CAM1_OK` and `CAM2_OK` remain in the telemetry schema.** They are now plain
health flags shown in ENGINEERING. They were *not* deleted, because field order
is a wire contract with the Teensy — removing two columns would shift every
field after them and the parser would silently read wrong values for the rest
of the packet. Retire a field by hiding it in the UI, never by deleting it
from `SCHEMA`.

### 2.2 Correction — the old schema had 79 fields, not 84

The previous README documented 84 telemetry fields including `CELL1_V`,
`CELL2_V`, `CELL3_V`, `PACK_TEMP_C` and `WHEEL_RPM`. **Those five fields did
not exist in the parser and never did.** This is consistent with the v4.1 note
that no BMS is fitted to the flight build, so per-cell data is not telemetered.

v5 then **appended five genuinely new GNSS-quality fields** (§5b) at positions
79–83. Appending is safe — the parser is positional and trailing fields are
optional, so every pre-existing column keeps its index and firmware that does
not yet send them simply leaves them absent. Two tests pin this: one asserts
the first 79 names in exact order, another parses an old 79-field packet and
checks the new fields come back *absent* rather than zero.

The real numbers, asserted by `test_vritra.py`:

| | Count |
|---|---|
| Original downlink fields | 79 |
| Appended GNSS-quality fields | 5 |
| **Total fields in `SCHEMA`** | **84** |
| **CSV columns (`TEAM_ID` + fields)** | **85** |
| Derived parameters | 45 |

### 2.3 Correction — the research document's Time-on-Air table is wrong above SF7

`Aerospace Ground Station Design Research` tabulates airtime per spreading
factor. Its SF7 row (~102.6 ms) is reproduced exactly by this implementation
when CRC is disabled, and SF10 matches too. **SF11 and SF12 in that table are
incorrect.**

| SF | Research doc | This implementation | Reference |
|---|---|---|---|
| 7 | 102.6 ms | 102.656 ms (CRC off) | matches |
| 10 | 657.4 ms | 657.41 ms | matches |
| 11 | 1482.7 ms | **1396.74 ms** | doc is wrong |
| 12 | 2719.7 ms | **2465.79 ms** | doc is wrong |

This implementation follows Semtech AN1200.13 exactly and is pinned by a test
to the canonical published LoRaWAN DR0 figure — SF12 / BW125 / CR4-5 / 51 B /
explicit header / CRC on = **2465.79 ms**. The error in the doc's table comes
from mishandling the low-data-rate term: the divisor is `4 × (SF − 2·DE)`, not
`4 × (SF − 1)`.

**Do not quote the research document's SF11/SF12 rows in the PDR.**

### 2.4 Finding — a 1 Hz downlink at SF9 is not 1 %-duty-cycle compliant

At SF9 / BW125 / CR4-5 with a 55-byte payload, one packet is 349.18 ms of air
time. Transmitting at the mandated ≥1 Hz means **1257 s of air time per hour =
34.9 % duty cycle**, against a 1 % budget of 36 s.

The 1 % figure is the ETSI EN 300 220 convention cited in the research
document, not necessarily an Indian requirement — India's WPC SRD rules for
865–868 MHz are written primarily around power, ERP and bandwidth. **Confirm
whether a duty-cycle limit binds your sub-band before flight.** The RADIO
TOOLBOX shows this live so the trade is visible rather than assumed.

---

## 3. Architecture

```
RF modem ──serial──> SerialLink (worker thread)
                          │  raw CSV lines
                          ▼
                    queue.Queue (bounded, drop-oldest)
                          │
                          ▼
                 MainWindow._tick()  (QTimer, UI thread, 10 Hz)
                          │
          parse ─> physics ─> CSV log ─> limits/staleness ─> paint
```

The reception layer never touches a widget and the render loop never blocks on
I/O. A stalled port, a flood of packets, or a broken panel cannot freeze the
operator's display or interrupt the graded CSV.

Three properties this buys:

- **Flood governor.** At most 40 packets are drained per tick; the queue drops
  oldest when full. A misbehaving transmitter cannot starve the UI.
- **Log before display.** Parsing and CSV writing happen before any renderer
  runs, and each panel updates inside its own `_safe()` sandbox. A panel that
  throws degrades exactly one panel and is reported on the HEALTH chip.
- **Honest absence.** `parse_packet` returns a `present` set alongside the
  values. Display code checks it before rendering, so the GCS shows `—` for a
  sensor that is not reporting instead of a fabricated `0`.

---

## 4. Telemetry contract

ASCII, comma-separated, one line per packet, CR-terminated, ≥1 Hz, SI units.
The device sends the cheapest possible bytes; **all** structuring happens on
the host. The flight hardware never emits JSON.

The first 16 columns are the competition-mandated order from guideline §6.3 /
PDR Table 10 and must not be reordered. `TEAM_ID` is CSV column 0 and is
stripped by the parser before positional mapping.

```
 0  TEAM_ID       2026-INSPACe-CAN-7USAT-025
 1  TIME_S        time since power-on            s
 2  PKT           packet count                   integer
 3  ALT_M         altitude AGL                   m
 4  PRES_PA       atmospheric pressure           Pa
 5  TEMP_C        temperature                    °C
 6  BUS_V         power-bus voltage              V
 7  GNSS_TIME     GNSS receiver time             s
 8  LAT           GNSS latitude                  deg
 9  LON           GNSS longitude                 deg
10  GPS_ALT_M     GNSS altitude                  m
11  SATS          GNSS satellites                integer
12  ACC_X/Y/Z     accelerometer                  m/s²
15  SPIN_RATE     mechanical-gyro spin rate      °/s
16  STATE         flight-software state 0..4     integer
```

Then the team payload per §6.3.B: power (4), IMU 9-DoF + high-g (12),
environment (15), spectral AS7343 (14), RF (3), servo rail (2), mechanisms (4),
systems (9).

**Parser behaviour** — a trailing-truncated packet parses fine and the missing
fields are marked absent, not zero. An empty cell is absent. An unparsable cell
is absent. `NaN`, `Inf` and overflow values are rejected rather than coerced.
All of this is asserted in the test suite.

### Flight states (§6.2.i)

```
0  BOOT        Power-on self-test, pad idle
1  LAUNCH      Detected liftoff
2  EJECT       Canister ejected, primary chute deployed
3  SEC_EJECT   Secondary ejection / aerobrake release
4  TOUCHDOWN   Landed — telemetry continues until GSE command
```

---

## 5. Limits and staleness engine

Two independent failure modes, one verdict per parameter.

**Value** — is the number outside its safe envelope? A four-threshold model
(`RED_LOW`, `YELLOW_LOW`, `YELLOW_HIGH`, `RED_HIGH`) with a **persistence
counter**, so one noisy packet cannot raise an alarm. Alarms are hard to raise
and easy to clear: entering an off-nominal state requires N consecutive
samples, returning to nominal takes one.

**Time** — is the number still trustworthy? Per-subsystem ageing through
`NOMINAL → WARNING → STALE → LOST`, evaluated every tick *including ticks where
no packet arrived*. This is what catches a sensor that silently stops
transmitting.

Age outranks value: a stale reading is reported as stale no matter how nominal
it looks — but staleness never masks a live red alarm.

| Subsystem | Warning | Stale | Lost |
|---|---|---|---|
| Inertial | 2 s | 4 s | 8 s |
| Flight core | 3 s | 6 s | 12 s |
| Radio link | 3 s | 6 s | 15 s |
| Power | 5 s | 10 s | 30 s |
| Navigation | 5 s | 15 s | 30 s |
| Mechanisms | 5 s | 15 s | 30 s |
| Environment / spectral / systems | 10 s | 30 s | 60 s |

The research document tabulates these for an orbital vehicle sampling far
faster than we do. Our downlink is 1 Hz, so its "< 1.0 s nominal" band is
physically unreachable; the values above preserve its ratios and intent while
staying achievable on a 1 Hz LoRa link.

Key limits are grounded in the flight hardware — notably `BUS_V` for the **3S
Molicel Li-Ion pack with no BMS fitted**, which makes this readout the primary
battery-health indicator: red below 9.6 V (3.2 V/cell), amber below 10.5 V
(3.5 V/cell), amber above 12.6 V, red above 12.9 V.

Breaches drive the tile ink, a master alarm banner, a rate-limited audible
alert, and a severity-tagged event log that fires **exactly once per
transition** — never spammed, never missed.

---

## 5b. Precise positioning — NavIC / GNSS

The flight receiver is a **Quectel L89**: a dual-band, IRNSS-enabled module
with an integrated patch antenna for **L1** plus a separate 10.0 × 3.2 × 4.0 mm
chip antenna for **NavIC / IRNSS L5**, tracking GPS + Galileo + QZSS + NavIC
concurrently and applying **GAGAN** (India's SBAS, broadcast from GSAT-8/10/15).

### Accuracy ladder

| Method | Typical horizontal accuracy |
|---|---|
| Raw SPS, single frequency | 5–10 m |
| Dual-band L1+L5 (ionosphere measured, not modelled) | 3–5 m |
| GAGAN / SBAS corrected | 1–3 m |
| Local differential (DGNSS) | 1–2 m |

**The flight configuration is a single L89 with no ground-station receiver.**
That is a deliberate, correct choice: 5–10 m is a sub-minute walk in an open
field, and DGNSS pays off in survey and precision-agriculture work, not CanSat
recovery. The DGNSS engine is implemented and tested anyway, exposed as one
optional line — attach any NMEA receiver and it starts correcting immediately.

### What the GCS does with it

- **Honest uncertainty.** Horizontal 1σ = `HDOP × UERE`, with UERE selected by
  reported fix type (5.0 m SPS, 1.5 m SBAS, 2.8 m when dual-band removes the
  ionospheric term). Reported as 1σ, CEP50 (1.1774σ) and 95 % containment
  (2.4477σ), and drawn as a **circle on the ground track** — a bare dot claims
  precision the receiver does not have.
- **Constellation readout.** Which systems are actually in the solution, from
  the `GNSS_SYS` bitmask, so the NavIC claim is evidenced rather than asserted.
- **Pad self-survey.** While `STATE == BOOT` the receiver is stationary, so its
  fixes are averaged. Random error falls as 1/√N; the reported accuracy is
  deliberately **floored at 1.2 m** because averaging cannot remove the
  slowly-varying correlated bias, and claiming centimetres from a long average
  of an uncorrected SPS receiver would be a lie.
- **Four coordinate formats** — decimal, DMS, degrees/decimal-minutes and UTM
  grid — copyable in one click. Metres on a local grid are far easier to search
  on foot than decimal degrees.

### Recovery guidance

- **Wind, measured from the vehicle's own drift.** Under canopy the CanSat has
  no lateral propulsion, so its horizontal ground track *is* the air mass.
  Averaging recent fixes yields a wind estimate with no anemometer and no
  forecast.
- **Projected touchdown** = current fix + wind × time-remaining, drawn on the
  map with a dashed line from the vehicle to the projection.
- **Search radius**, combining three uncorrelated terms in quadrature (not a
  raw sum, which would be wildly pessimistic): the fix's 95 % uncertainty,
  drift during any telemetry gap, and descent still to come.
- **Walk-to navigation** — paste your position from a phone, get distance and
  a 16-point compass heading to the target.

### Simulator launch site

`SIM_SITE_LAT` / `SIM_SITE_LON` near the top of the file set where the
simulator flies (currently Kushinagar, UP). **Only the simulator uses it** — in
real operation the pad reference comes from the first live GNSS fix, so the GCS
never has to be told where it is.

---

## 6. Radio toolbox (ENGINEERING view)

Live and entirely offline:

- **Airtime** per Semtech AN1200.13 — symbol time, automatic LDRO above the
  16 ms threshold, time on air, bit rate.
- **Duty-cycle budget** — max packets/hour at 1 %, and what a 1 Hz downlink
  actually consumes (see §2.4).
- **Link budget** — FSPL, predicted RX power and margin at the live slant
  range, plus the free-space range ceiling.
- **Radio horizon** — `4.12 × (√h_tx + √h_rx)` km with the 4/3-earth
  allowance. This is normally what actually limits the link, so the free-space
  ceiling is labelled as a theoretical bound rather than an expected range.
- **WPC India compliance** — TX power ≤ 1 W, ERP ≤ 4 W (referenced to a
  half-wave dipole, so `ERP = P + G − 2.15`), carrier bandwidth ≤ 200 kHz, and
  occupied band against 865–868 MHz with the 865–867 MHz ETA sub-band called
  out separately. **BW125 is chosen precisely because BW250 would breach the
  200 kHz limit.**

---

## 7. The six views

| View | Owner | Contents |
|---|---|---|
| **FLIGHT** | Flight director | Subsystem health, primary flight, power, live 3-D attitude, ADI + compass, GNSS, downlink, flight sequence, ground track |
| **SCIENCE** | Payload scientist | Atmosphere, HDC3020 reference cross-check, air quality, AS7343 spectral bars, lightning log, **measurand-vs-altitude vertical profiles** |
| **ENGINEERING** | Systems engineer | Power with limit envelopes, raw IMU, mechanisms, systems health, active-limits table, radio toolbox |
| **ANALYSIS** | Either | Eight retargetable plots over any parameter, full flight statistics, X-vs-Y cross-plot, CSV export |
| **RECOVERY** | Recovery team | Large ground track, last known fix, range/bearing from pad, descent, KML + map + clipboard, beacon uplink |
| **CONSOLE** | Either | Grouped packet stream, severity-tagged event log, command history |

`Ctrl+1` … `Ctrl+6` switch views.

### Design language — "austere base, science colour"

Flight and engineering surfaces are monochrome on near-black. Amber and red are
reserved **exclusively** for limit breaches and staleness, so a genuine alarm
has no competition for the operator's eye. Colour is permitted inside
scientific plots only where it encodes a real data dimension — the AS7343 bars
map hue to wavelength, because there the hue *is* the data.

Typography is split for a measured reason: **Bahnschrift SemiCondensed**
(Windows' DIN 1451 face) for labels, and **Cascadia Mono** for every numeric
readout. Bahnschrift's digits are not tabular — its "1" is 16 px against "4" at
27 px at display size — so values would visibly jitter every packet. Cascadia
Mono is measured tabular and holds still.

---

## 8. Compliance matrix

| Requirement | Clause | Implementation |
|---|---|---|
| Own ground station, near-real-time telemetry | 6.1.i | PyQt5 desktop app, 6 views, updates every packet |
| All telemetry in SI units | 6.1.i | m, Pa, °C, V, m/s², °/s throughout |
| Plot data in real time | 6.1.i, x | Live sparkline on every tile; ANALYSIS plots any parameter |
| Command CanSat to start TM before launch | 6.1.ii, v | `CMD_TX_ON` / `CMD_TX_OFF` |
| Calibrate gyro / baro / accel on pad | 6.1.vi | `CMD_CALIBRATE` |
| Generate .csv of all sensor data | 6.1.vii | `Flight_<TEAM_ID>.csv`, flushed every packet |
| Mission time ≥ 1 s resolution | 6.1.viii | `TIME_S`, MET clock in header |
| Clock & state survive processor reset | 6.1.ix | `STATE` + `TIME_S` in every packet |
| Plot each telemetry field in real time | 6.1.x | ANALYSIS view covers all 124 parameters |
| Portable | 6.1.xi-xii | Single .exe, LR1121 via USB-serial, editable COM port |
| Maintain & telemeter FSM state | 6.2.i-ii | 5-state stepper; `STATE` in every packet |
| 1 Hz ASCII CSV, CR-terminated, SI units | 6.3.i | Parser accepts ≥1 Hz CSV |
| Save received TM as .csv with headers | 6.3.ii | Auto CSV with full header row |
| File named `Flight_<TEAM_ID>.csv` | 6.3.iii.A | Exactly that filename |
| GS power, antenna, console, last command | 9 | CONSOLE shows stream, events and full command history |

---

## 9. Testing

```bash
python test_vritra.py
```

**104 unit tests**, covering the parser (TEAM_ID stripping, absent-vs-zero,
NaN/Inf rejection, mandated column order), the LoRa formulas (pinned to the
canonical LoRaWAN DR0 reference), link budget and radio horizon, WPC
compliance verdicts, and the limits/staleness state machine (persistence
gating, immediate recovery, per-subsystem clocks, order-independence).

The design research treats a ground station as safety-critical software and
calls for exactly this validation.

Beyond the unit suite, this build was verified by driving the running
application:

- **695 UI interactions** — every button on every view, every checkbox on and
  off, every dropdown stepped through all items, every spin box swept
  min/mid/max, every tab visited. 695 passed, 0 failed, 0 degraded panels.
- **22 end-to-end alarm assertions** — persistence gating, red escalation,
  banner visibility, the full staleness ladder on both a simulated and a real
  wall clock, event-log severity tagging, and clean recovery.
- **Full-mission soak** — the complete BOOT → TOUCHDOWN profile with CSV
  integrity checks on every row.

### Sensor fault-injection matrix

`test_faults.py` and `test_faults_ui.py` drive **five cases for every sensor**
through the real pipeline, and then through a live `MainWindow` to check what
the operator actually sees:

| Case | Meaning | Screen must show |
|---|---|---|
| **BEST** | ideal nominal reading | a number, monochrome |
| **GOOD** | valid but near a limit | a number, amber if it crosses caution |
| **SHORT** | shorted to a rail, saturated, or pinned at a sensor error code | **a number** — it is really reporting, and reporting wrongly |
| **HAYWIRE** | `nan`, `inf`, `1e309`, `NULL`, `ERR`, `0x7FFF`, `12.3.4`, `--5` | **an em-dash** — rejected at the parser, never rendered |
| **DISCONN** | field never arrives | **an em-dash** |

The distinction this exists to enforce: **a shorted sensor reading 0.0 and a
disconnected sensor must never look the same.** "The tank reads empty" and
"there is no tank sensor" demand opposite responses from an operator.

Two further cases are tested because they behave differently and both matter:

- **Never connected** → em-dash forever.
- **Connection lost mid-flight** → the last known value is **retained** and
  marked `WARNING → STALE → LOST`, because an operator needs the last thing
  that sensor said, not a blank.

Also covered: every shorted sensor with limits, verified to actually alarm
rather than pass silently (13 limit-bearing sensors); and total payload-bus
failure, where every payload sensor goes haywire at once and the mandated core
must keep displaying, keep logging, and degrade no panel.

### A sixth failure mode this work uncovered

Writing the matrix exposed a fault **neither existing engine could catch**: a
sensor that locks up but **keeps transmitting the same value**. Staleness
cannot see it — packets *are* arriving. Limits cannot see it — the frozen
reading is usually in range. An operator would watch a dead sensor look
perfectly healthy for the whole flight.

`ParamState.is_frozen()` now flags any measured sensor reporting a
bit-identical value for 20 consecutive samples (~20 s at 1 Hz — no real noisy
sensor does this). It raises a `LIVE → FROZEN` event and lifts the master alarm
to caution.

Two deliberate exclusions stop it crying wolf:

- **Legitimately constant fields** — `FREQ_MHZ` is commanded, `GEOID_SEP` is a
  property of the location, servo angles hold a commanded position.
- **Derived values**, because several are constant *by design*: `APOGEE_M`,
  `PEAK_V` and `G_HI_PK` are latched running maxima, and `TTL_S` / `GLIDE_LD`
  sit at zero whenever the vehicle is not descending. A freeze in a real sensor
  is caught at the sensor.

Verified across 590 packets of a full simulated flight with no false positives.

### Bugs this testing actually caught

Each of these is now pinned by a regression test.

**Coordinates were being written to the graded CSV at 3 decimal places.**
Both `serialize_packet` and `Logger.write` formatted every float as `%.3f`.
For latitude and longitude that is roughly **111 m of resolution** — the
judged track and the recovery fix would have been quantised to the size of a
football field. The ground-track panel rendered it as a visible staircase of
right angles. Fixed with a per-field precision policy (`CSV_DECIMALS`);
coordinates now carry 7 decimals.

**The staleness machine could only escalate.** `ParamState.evaluate_age()`
never restored a parameter once its age came back inside the nominal band, so
it silently depended on `update_value()` running first. In production the
ordering hid it, but it was order-dependent and would have left stale ink on a
recovered parameter. It now reverts to the value classification — and
correctly falls back to the *value* verdict, not blindly to nominal, so a real
limit breach is not erased.

**The event log buried real transitions.** Every parameter logged its initial
`NO_DATA -> NOMINAL`, flooding the console with 100+ lines on the first packet.
A sensor beginning to report is not an event; that transition is now skipped.

**The 3-D CanSat rendered as a cone.** The perspective divide used the
vertical axis instead of the axis pointing into the screen, so the far end of
the body shrank along its own length.

**Toolbar buttons clipped their first character** ("EXPORT XML" → "XPORT XML")
because uppercase plus letter-spacing makes the rendered string wider than a
fixed width estimate. Buttons now use a minimum width.

**Limit lines dragged the plot autorange.** An `ALT_M` envelope at 1500 m
forced the y axis to 1500 and flattened the actual 0–40 m data into an
unreadable line. Limit lines are now added with `ignoreBounds=True`.

---

## 10. Output files

Written to `~\VRITRA_GCS_logs\`:

```
Flight_<TEAM_ID>.csv        graded log — TEAM_ID + 84 fields, headered
Flight_<TEAM_ID>.xml        XML export with the embedded schema
Track_<TEAM_ID>.kml         WGS-84 track for Google Earth
Statistics_<TEAM_ID>.csv    per-parameter min/max/mean/N
Dictionary_<TEAM_ID>.json   machine-readable telemetry dictionary
vritra_gcs.log              diagnostic log
```

The dictionary export is the F´ pattern: a ground system should be generated
from a single machine-readable definition rather than a hand-maintained table.
`SCHEMA` already plays that role internally; exporting it gives the PDR/FRR
appendix one authoritative artifact.

KML and the browser map are deliberately file- and link-based. Nothing in the
field flow depends on a live tile server, so recovery still works with no
network at the launch site.

---

## 11. Repository layout

```
cansat/
├── firmware/
│   ├── vritra_fc/           flight computer (Teensy) — main mission firmware
│   ├── vritra_gs/           ground-station radio node (LoRa gateway)
│   ├── vritra_sensor_test/  sensor bring-up sketch
│   └── cdr_tests/           19 CDR hardware test sketches
├── gcs/
│   ├── vritra_gcs.py        complete GCS application (~4500 lines)
│   ├── fetch_map.py         offline XYZ tile pre-cacher
│   ├── verify_schema.py     telemetry schema self-test (runs in CI)
│   ├── VRITRA_GCS.spec      PyInstaller spec (reproducible build)
│   └── build_exe.bat        one-click Windows builder → dist\VRITRA_GCS.exe
├── docs/
│   ├── VRITRA_PROJECT_BRIEF.md
│   ├── COMPLIANCE_AUDIT.md
│   ├── HANDOFF.md
│   ├── IMU_RESEARCH_BRIEF.md
│   ├── IMU_AXIS_CALIBRATION_2026-08-06.md
│   ├── PROJECT_HISTORY.md
│   └── SENSOR_TESTING_FINDINGS.md
├── .github/
│   └── workflows/ci.yml     CI: schema verify + syntax check on every push
├── LICENSE
└── README.md
```

### Running GCS tests

```bash
cd gcs
python test_vritra.py && python test_faults.py && python test_faults_ui.py && python interact.py && python alarm_test.py && python soak.py
```

`soak.py` takes about 2.5 minutes — it flies the whole profile in real time.
The three GUI harnesses write screenshots to `test_screenshots/`.
