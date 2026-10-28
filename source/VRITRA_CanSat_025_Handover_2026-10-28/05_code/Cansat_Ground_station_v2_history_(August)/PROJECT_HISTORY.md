# VRITRA / CAN-7USAT — Project History & Consolidated Knowledge

> **Newer than this file:** [`HANDOFF.md`](HANDOFF.md) records the 2026-08-07
> final-prototype session — ten defects found and fixed across the flight
> computer, gateway and GCS (calibration that could never converge, an altitude
> datum that reset to sea level on every reboot, a TX-power step that was
> silently lost, NavIC that was never actually enabled, and more), plus what is
> verified vs unverified and the open items. **Read it before changing
> firmware.**

Team 2026-INSPACe-CAN-7USAT-025, Lovely Professional University.
Compiled 2026-08-01 from 5 shared Claude conversations (a 6th, a Claude Code
cloud session, could not be read — it requires sign-in and only the owner's
browser can open it).

Source conversations:
1. "Maximizing LR1121 range at 867 MHz" — RF bring-up/debug saga
2. "Cansat CDR preparation and component review" — BOM, PDR baseline, CDR plan
3. "CanSat ground station software redesign with scientific UI" — VRITRA_GCS build history
4. "CanSat electronics system design and radio mitigation plan" — power/servo/radio architecture
5. "CAN-7USAT PDR critical technical analysis and compliance review" — red-team PDR audit

---

## 1. What the project is

CAN-7USAT — a CanSat (soda-can-sized satellite) built for the IN-SPACe
CAN-7USAT India Student Competition 2026 (3rd edition), governed by
Mission Requirement & Competition Guidelines Doc. IN-SPACe.CAN.MR.CG.01.
Constraints: ≤1000 g ±50 g, ≤150×400 mm, survives 15G launch / 30G shock,
must self-eject, descend under two independent mechanisms, telemeter live
over radio, and log everything to CSV.

VRITRA's design: dual-stage descent (drag parachute, then a GPS-guided
ram-air parafoil released at 600 m), Teensy 4.1 flight computer with a
backup Teensy 4.1, ESP32-S3 sensor MCU, dual LR1121 LoRa radios (uplink +
downlink), 3S Molicel 21700 Li-ion pack (no BMS, direct to buck converters),
NavIC-capable Quectel L89 GNSS, and an optional biochemical payload.

The ground station (this repo) is `vritra_gcs.py` — a single-file PyQt5
desktop app, no browser/GPU/internet dependency, built specifically to look
like real aerospace ground-segment software rather than an "AI-generated"
dashboard. See [README.md](README.md) for its current technical spec (84
telemetry fields, limits/staleness engine, radio toolbox, etc.) — this file
is the *history and decisions* behind it, not a restatement of it.

---

## 2. PDR baseline (as submitted, before CDR fixes)

- Mass budget: 995.4 g total — compliant against the 1000 g ±50 g cap (an
  earlier "500 g limit" note floating around was a stale error).
- Primary chute: flat circular vented, Cd 0.75, D 380 mm, S=0.1134 m² →
  14.4 m/s sink rate — **below the 15 m/s floor** of the mandated 20±5 m/s
  band. Flagged, unresolved at PDR time.
- Secondary parafoil: 7-cell ram-air, AR 2.30, S=0.4 m², 2.55 m/s sink at
  600 m AGL, 6.37 m/s forward speed — compliant (1–3 m/s band).
- Structure assumed in PDR: 1.5 mm CFRP lattice shell — **this was a hard
  rule violation** (see §5 red-team findings) because carbon fiber is
  RF-conductive and blocks the LoRa/GNSS antennas, and a lattice doesn't
  "enclose and shield" electronics as required.

## 3. CDR-era design changes (post-PDR, confirmed real changes not doc typos)

- **BMS removed.** Going 3S Molicel Li-ion → buck converters directly, no
  BMS board. (Guidelines do **not** mandate a BMS — see §5 Q&A. This raises
  the importance of software/EPS-level overcurrent & undervoltage handling
  since there's no hardware cutoff.)
- **Structure changed** from CFRP lattice to carbon-fiber rods (structural
  members only, non-continuous so less of an RF shield) + transparent
  fiberglass shell + PETG internal plates. This directly fixes the PDR's
  RF-transparency violation and the "enclosure" violation, but invalidated
  the old CFRP-based Abaqus FEA (needs to move to a mixed beam/shell/solid
  model) and the mass budget (needs recomputing against real rod/fiberglass/
  PETG densities).
- **13-tool CDR simulation stack locked in** (two-tier core + extended):
  Ansys Workbench, Abaqus/Explicit, SimScale CFD, OpenFOAM, CST Studio, STK,
  COMSOL Acoustics, LS-DYNA, Ansys Motion, MATLAB, LTspice, Renode, Ansys
  Medini (FTA/FMEA).
- **Radio config locked** for the field range test: 867.000 MHz, SF10, BW
  125 kHz, CR 4/5, sync word 0x98, target 22 dBm — though early bench
  firmware used 14 dBm / sync 0x12 (deliberately weaker, safe-for-desk
  settings). **Important distinction: 0x98 is the real Team-025 sync word;
  0x12 was only ever a bench-test placeholder** — don't confuse the two in
  future firmware.

---

## 4. Red-team PDR audit — findings & fixes (session 5)

A full adversarial "lead judge" review was run against the actual PDR PDF.
Overall verdict at the time: **HIGH risk as submitted, MEDIUM-LOW after
fixes** — concept was sound, but ~2 dozen internal contradictions and a few
hard rule violations would have hurt badly in front of a jury.

### Hard rule violations found
1. **RF-transparency claim was false.** PDR claimed the CFRP body was
   "RF transparent" — physically wrong; carbon fiber is conductive. Also
   self-contradicted by calling the same part "fiberglass" elsewhere.
   → Fixed by the CDR material change above.
2. **30G shock survival argument was physically incoherent.** PDR reported
   a 420 N / 43.7G peak on the chassis (its own Fig. 15, titled "chassis
   reaction forces") then claimed the chassis doesn't see it because "it's
   in the lines" — violates Newton's second law; if lines transmit 420N to
   the can, the can decelerates at ~43G. Needs either a softer opening
   (slider/reefing/larger initial reefed area) or hardware genuinely
   qualified >44G with honest reporting, not a hand-wave.
3. **Open lattice shell ≠ "enclosed and shielded" electronics** (guideline
   requirement). Same fix as the RF-transparency issue — solid
   RF-transparent skin over the avionics bay.
4. **Primary chute sink rate (14.4 m/s) sat below the legal 15 m/s floor**
   of the 15–25 m/s band, while the PDR text elsewhere claimed "18–22 m/s"
   (self-contradictory). Fix: shrink canopy to ~0.073 m² (D≈305mm) for
   ~18 m/s.

### Other serious internal contradictions flagged
- Mass: "~920 g" (prose) vs 995.4 g (actual table) — needed unit×qty
  explicit columns, one authoritative total.
- GNSS: "both computers share one L89" (single point of failure) vs BOM
  listing qty 2 — needed to commit to two independent GNSS receivers.
- TX power: link budget used +22 dBm/+24 dBm EIRP vs the team's *own*
  communication test plan capping EIRP at 25 mW (+14 dBm) — a 10× internal
  contradiction. Also SF9 airtime (~0.4–0.7s/packet) at 1Hz ≈ 50% duty
  cycle, risking the 1% duty-cycle budget — recommended SF7/SF8 for
  downlink instead.
- Link margin (74 dB) was boresight-only; a tumbling whip antenna has an
  axial null and polarization mismatch vs a Yagi ground antenna — worst
  case erodes to ~25–30 dB, still positive but should be reported honestly.
- 5V/5A rail was shared across Teensy + camera + gyro-motor driver + two
  35 kg·cm brake servos with no accounting for stall current — servo rail
  needed its own budget/supply.
- Endurance claim ("750 mA avg → 6h") didn't reconcile with the power
  budget's implied ~190 mA (~26h) — needed re-derivation from the real
  budget.
- FSM state names were inconsistent across 3 different naming schemes in
  the same document — needed to standardize on the guideline's 8 names:
  BOOT, TEST_MODE, LAUNCH_PAD, ASCENT, ROCKET_DEPLOY, DESCENT,
  AEROBREAK_RELEASE, IMPACT.
- 600m detection (baro-trigger for parafoil release) had ~10m of
  lag/smear from its 0.5s+0.3s filtering windows — recommended a static
  port and drop-test characterization plus hysteresis.
- Magnetometer (heading source) sits near the spinning N20 gyro-wheel
  motor — will get corrupted by motor EMI. Recommended GPS course-over-
  ground as primary heading, magnetometer as backup only.
- **Gyro wheel vs. parafoil steering conflict**: a spinning momentum wheel
  resists yaw; parafoil steering *needs* yaw. Recommended running the gyro
  wheel only pre-600m (drag-chute phase) and disabling it during parafoil
  steering.
- I2C bus-hang risk: BMP585 (the critical 600m trigger sensor) shared an
  I2C bus with other devices — one latched device could kill the bus.
  Recommended moving BMP585 to SPI or adding bus-recovery + watchdog.
- Servo torque calc (0.101 kg·cm) was steering-only, understated the full
  flare/brake load — servos (35 kg·cm) are over-margined so not a real
  risk, just needed honest reporting.
- Spring-contact risk: guidelines forbid spring contacts (§5.3.v); typical
  21700 holders use springs — needed spot-welded/soldered tabs instead.
- Naming chaos across the doc: servos alternately called OT5330M /
  AGFRC A20BHM / MG90S / MG996R; beacon as SFM20 / 2104-SM400204; MCU as
  ESP32-S3 / ESP32-P4 / ESP32-WROOM; BMS as 20A vs 40A; body diameter
  130mm vs 150mm; battery 2S vs 3S. Needed one canonical name per part,
  applied everywhere.
- Bio payload (petri plate w/ live cultures under 43.7G shock): spill/
  contamination risk to electronics, unclear in-flight function, and a
  possible biosafety/restricted-material question. Recommended converting
  to a sealed passive air-sampler analyzed post-flight, with a BSL-1
  sign-off.

### Direct rule-interpretation Q&A (from the audit)
1. **Is a BMS mandatory?** No — the guidelines only require an external
   switch + indicator, 2-hour capacity, allowed chemistry (no LiPo, metal
   package), accessible compartment, no spring contacts. "BMS" doesn't
   appear in the rules at all. (The team later removed the BMS anyway —
   see §3 CDR changes — which is rules-compliant.)
2. **Is onboard SD-card storage mandatory, and was VRITRA's approach
   valid?** Yes, mandatory (§5.1.xi: "shall have provision to record...
   onboard SD card"). VRITRA's design was valid and exceeded the
   requirement — microSD blackbox on SPI1 *plus* W25Q64 flash *plus*
   Teensy EEPROM as extra redundancy.
3. **Were all guidelines followed?** Mostly — mass, dimensions, fluorescent
   body+flag markings, Made-in-India+NavIC bonus, mandatory dataset,
   SI units, SD logging, dual-descent architecture, immediate primary
   deploy, no pyrotechnics, parafoil sink rate, battery chemistry, LoRa
   radio choice, 15-field packet format, CSV filename convention,
   calibrate command, reset-survival, portable GS with elevated Yagi all
   passed as designed. The failing/borderline items are the ones listed
   above (RF transparency, 30G shock argument, enclosure, chute sizing,
   TX power self-contradiction, 3-language exterior marking, beacon dB
   verification, spring contacts, gyro-authority calc, bio-payload risk,
   FSM naming).

### Bonus-point strategy identified
- NavIC (Quectel L89) — explicitly weighted extra by the guidelines.
- ENS160 VOC/CO2 sensor — counts as an "additional innovative sensor."
- GPS-guided parafoil controlled landing — hits the "innovative controlled
  flight/targeted landing" bonus objective directly.
- Video capture (dual XIAO ESP32-S3 Sense cameras) — bonus points for
  video from separation to touchdown.

---

## 5. Electronics / power system architecture decisions

### MCU split (confirmed correct approach)
- **ESP32-S3** — "satellite bus" brain: all sensing, radio, data logging.
- **Teensy 4.1 (600MHz Cortex-M7)** — real-time servo control / parafoil
  flight maneuvers (needs µs-precision PWM that ESP32 can't guarantee).
- Cross-talk over UART: Teensy gets altitude/orientation from ESP32; ESP32
  gets gyro-wheel RPM status back from Teensy.
- Backup Teensy 4.1 planned for failover — recommended pairing: a
  **7Semi SN74LV4052A mux breakout** (routes the UART line) + a
  **TPL5010 watchdog IC**. If the primary Teensy stops kicking the
  watchdog, the mux auto-switches the UART to the backup within ~3s.
  ~₹150 total for both parts.

### Sensor/voltage gotchas found
- **ICM-20948 VDDIO is 1.71–1.95V** — cannot go directly on ESP32-S3's
  3.3V logic; needs a dedicated 1.8V LDO (e.g. AMS1117-1.8 / MIC5219-1.8),
  or better, treat **LSM6DSO32 as the primary IMU** (natively 3.3V-
  compatible, 1.71–3.6V range) and ICM-20948 as secondary only.
- Battery rule: **no LiPo** — must be metal-can Li-ion (18650/21700 style).
  This was already satisfied by the Molicel 21700 choice.
- Servo current is high: two OT5330M @ 7.4V stall at ~3.8A each (7.6A
  combined worst case); cells/rails must handle ≥10A continuous. Route
  servo power straight from battery through a fuse, not through a
  regulator, was the early recommendation — later refined (see below).

### Power rail / buck converter saga (final recommendation)
Three rails needed: 3.3V (sensors), 5V (Teensy/cameras), 7.4V (servos).
- **3.3V — 7Semi AP62301 breakout.** Kept as-is; 2.2MHz switching with
  EMI-reducing gate driver, good next to the LR1121 radio.
- **5V — 7Semi AP64500 (5A).** Kept; add a 100µF ceramic cap across the
  output to absorb the surge when both XIAO cameras boot simultaneously.
- **7.4V servo rail — went through several wrong answers before landing
  on the right one:**
  - ❌ XL4015 adjustable buck — rejected: minimum input voltage is 8V,
    but a 2S pack at 50% discharge sits at 7.4V, *below* the XL4015's own
    input floor. Servos would die mid-descent as the battery sags.
  - ❌ Fixed-5V AP64500 — can't be trimmed to 7.4V, wrong part for this rail.
  - ❌ UBEC-8A / Hobbywing ESC-style BEC — technically works (jumper-
    selectable 7.4V, 8A continuous) but the user specifically wanted a
    proper buck IC, not an RC-hobby ESC/BEC module.
  - ✅ **Final answer: power the OT5330M servos directly off the battery
    rail** (they're rated for 7.4V nominal and tolerate 6–8.4V), through a
    4A blade fuse and a 470µF/16V cap across the servo power rail to
    absorb current spikes — no buck converter at all. This is standard
    practice in RC aircraft/competition robotics for high-torque digital
    servos and avoids buck dropout risk entirely.
  - If a regulated 7.4V rail is still wanted for isolation: 7Semi
    TPS54560 breakout (4.5–60V in, 5A out) with the feedback resistor
    swapped (R1≈82k�ORe, R2=10kΩ) to retarget it from its stock 5V output
    to 7.4V.

### Radio interference mitigation plan (as written for the PDR)
Two independent LR1121 radios: **downlink** (866.5 MHz, SF9, telemetry,
TX-dominant) and **uplink** (865.5 MHz, SF7, commands, RX-dominant),
avoiding self-interference. Packet addressing uses sync word + NETID
(team ID) + 16-bit sequence number + CRC-16 for 4-layer cross-team
rejection. Pre-launch RSSI channel scan 30 min before launch, with a
software-switchable backup frequency if the primary channel reads
>-100dBm. Kill-switch command puts the radio to sleep (0.9µA) within 1ms.
Recommended simulations for the PDR appendix: (1) Monte Carlo link-budget
across TX power/orientation/atmospheric variance, (2) parafoil descent
trajectory, (3) gyro-wheel angular momentum calc, (4) battery discharge
curve vs subsystem brownout thresholds.

**Note the discrepancy vs the CDR-locked radio config in §3**: this
electronics-chat plan used 865.5/866.5 MHz dual-radio SF9/SF7 — the later
CDR session locked a single test config of 867.000 MHz / SF10 / sync 0x98.
Reconcile which is the actual final RF plan before flight.

---

## 6. Bills / components ordered (as of the CDR session, ~July 23)

| Vendor | Invoice | Date | Items | Total |
|---|---|---|---|---|
| Robu (Macfos) | INV2627/139906 | 13/07 | Teensy 4.1 ×2 (incl. backup), resistors, solder consumables, Dupont wire, XH2.54 terminal kit | ₹8,596.73 |
| Robu (Macfos) | INV2627/154939 | 23/07 | Universal PCB prototype board ×4, 1×40 female header strip ×20 | ₹1,485.00 |
| Evelta Electronics | PI27-001770 | 08/07 | BMP585 ×3, ICM-20948 ×2, L89 GNSS ×2, AP64500 buck ×3, AP62301 buck ×3, TB6612FNG ×2, XT60PW connectors ×5, W25Q64 flash, KCD1 switches ×4 | ₹9,085.00 |
| Atlantis Robotics | TR/2026-27/3864 | 08/07 | DS3231 RTC ×1, Core1121-HF (LR1121) ×3 | ₹3,979.96 |
| Amazon | 404-0425240-9563565 | 08/07 | SanDisk Ultra microSD 32GB 120MB/s | ₹2,503.00 |
| Amazon | 404-5715787-1097951 | 08/07 | 0.1µF ceramic cap ×40 pack | ₹125.75 |
| Amazon | 404-8365376-8276307 | 18/07 (delivered 19/07) | Themisto TH-M100 digital multimeter, 6000 count, 10A | ₹1,401.00 |

**Running total ordered (as of late July): ≈ ₹27,176.44**, roughly half of
the ₹54,284 full BOM estimate.

**Still outstanding to order at that point** (per BOM, not yet invoiced):
AGFRC A20BHM servos, MG90S, OT5330M, GA12-N20, Molicel 21700 cells ×3,
7.4V servo supply (see §5 — resolved to direct-battery-feed, may not need
a dedicated buck), INA226 ×2, SN74LV4052A, TPL5010, AMS1117-1.8, SCD41,
HDC3020, ML8511, MQ131, BMV080, AS7343, SEN0290, LSM6DSO32, XIAO
ESP32-S3 Sense ×2, ESP32-S3 WROOM, ESP32-P4, MAX98357A, buzzer, passives
kit.

**Cost context**: total project cost cap is ₹1,00,000 per competition
rules; non-refundable ₹15,000 CDR fee applies separately.

---

## 7. LoRa/radio bring-up saga — full failure & fix history

This was the single longest-running technical struggle in the whole
project ("stuck on this problem for too long," "worst i can even get to
previous range"). Chronological log of root causes found, in order:

1. **Illegal RF parameters used for a "max range" attempt.** An early
   answer suggested SF12/BW62.5/CR4-8/22dBm/866.3MHz for max theoretical
   range — but this violates the team's mandated frequency plan
   (SF10/BW125/CR4-5/sync 0x98/867.00 main+865.75 backup/+14dBm flight).
   **Lesson recorded: never deviate from the jury-locked frequency plan,
   even for bench range tests, without clearly labeling it as an
   illegal/bench-only config.**
2. **PA (power amplifier) path bug.** The LR1121 has two separate TX pins:
   RFO_HP_LF (high-power, up to +22dBm) and RFO_LP_LF (low-power, up to
   +15dBm). RadioLib silently routes to LP for any `setOutputPower()` call
   ≤14dBm. Since the Core1121-HF board's antenna is matched to the HP pin
   only, setting the legal +14dBm output silently sent RF out an
   unmatched/dead pin — collapsing range from ~2km to ~30-40m. **Fix:
   `setOutputPower(14, true)` — second arg forces the HP PA path while
   keeping the legal power level.**
3. **`setCurrentLimit()` doesn't exist on LR1121** in RadioLib (that's an
   SX127x/SX126x-family method) — simple compile fix, delete the call.
4. **CRC_ERROR vs RX_DONE IRQ ordering bug** (recurred across multiple
   library rewrites/attempts, including a from-scratch rewrite using the
   raw Semtech `lr11xx_driver` API instead of RadioLib): on the LR1121, a
   corrupt packet sets **both** RX_DONE and CRC_ERROR simultaneously.
   Code that checked RX_DONE first would try to parse garbage every time.
   **Fix: always check CRC_ERROR (and HEADER_ERROR) before RX_DONE, and
   bail out immediately if set.**
5. **RSSI stuck at exactly -127.5 dBm / -127 dBm repeating** — this is the
   chip's fixed noise-floor read, meaning the receiver's RF front-end was
   not seeing the transmitter's signal *at all* (not a weak-signal
   problem, a *no-signal* problem). Root cause chased through several
   hypotheses (IRQ pin wiring, blocking-receive bypass test) before
   landing on hardware: **antenna plugged into the wrong IPEX4 port.** The
   Core1121 module has two IPEX connectors — one sub-GHz, one 2.4GHz —
   and it's easy to seat the "868 MHz" antenna on the wrong one. Also
   flagged: IPEX connectors need a firm flush click, a half-seated
   connector radiates almost nothing, and TX must never key with zero
   antenna connected (can damage the PA).
6. **After the antenna fix**, real reception started (RSSI -30 to -46dBm
   at bench range, clean CRC-passing packets) — confirming the whole chain
   (SPI wiring, TCXO config, IRQ handling) was correct all along; the
   entire "no signal" period was purely the antenna port.
7. Multiple **compile errors from guessed/hallucinated RadioLib and raw
   `lr11xx_driver` API function names** while iterating with another AI
   (Gemini) in parallel — e.g. `set_lfclk` vs `cfg_lfclk`,
   `invert_iq` vs `iq`, `rx_boost_mode` vs `rx_boosted`,
   `set_buffer_base_address` (doesn't exist). **Lesson: when the exact
   header/API isn't visible, ask the user to paste the real function
   prototypes from their installed library rather than guessing repeatedly.**
8. A late CRC-error debugging round hit a **plain C++ brace-mismatch bug**
   introduced while hand-editing the receiver's `handleRxDone()` — an
   extra `}` pushed the RSSI/SNR print and payload-read code outside the
   `if (RX_DONE)` block, causing `'pkt_status' was not declared in this
   scope`. Straightforward fix, but shows the hand-patching process was
   getting fragile — a sign the code had been through too many manual
   edits without a clean rewrite.

**State at the end of the visible transcript**: TX confirmed working (65+
packets sent, HP PA path confirmed active), RX confirmed receiving real
packets after the antenna-port fix (RSSI -30 to -46dBm, SNR ~13-15dB at
bench range, CRCerr=0 on good packets). The chat was cut off mid-session
during a further CRC-error debugging round after that point — **check with
the team whether the actual 2km field range test was ever completed**,
since the last visible messages were still bench-range (~1-2m) validation,
not the full-range walk test.

**Two Arduino sketch families exist and should not be confused:**
- The **RadioLib-based** Transmitter.ino/Receiver.ino (session 1) — uses
  the Arduino RadioLib library, higher-level API.
- The **raw Semtech `lr11xx_driver`-based** Transmitter.ino/Receiver.ino +
  lr1121_config.h/.cpp (sessions 1 & 2) — uses Waveshare's
  `waveshare_lora_1121` demo library directly (note: the include name
  is literally misspelled `wavesahre_lora_1121.h` in the vendor's own
  package — not a typo introduced by the team, don't "fix" it).
Confirm which family is the one actually flashed on the current hardware
before doing further firmware work — the debugging history jumps between
both.

---

## 8. Ground station software (VRITRA_GCS) — build history

The current `vritra_gcs.py` in this repo (v5.0, see [README.md](README.md)
for full technical spec) went through several major redesigns:

1. **v1 (June 13)** — first pass, adapted from a friend's reference
   software + the guideline PDF + the project's BOM. Used a Three.js/
   ECharts-style 3D model concept early on, later replaced.
2. **Critical complaint (June 13): "looks too AI."** The user explicitly
   wanted a "less AI-looking," scientific/control-room aesthetic — no
   neon, no glow, no emoji, no gradients — closer to how NASA/ISRO/SpaceX/
   real aerospace ground segments look. This became a recurring design
   constraint through every subsequent revision.
3. **First real build**: removed a synthetic/fake data generator entirely
   (the earlier version incremented time/packets on its own — exactly the
   "dummy data" the user rejected). Real serial-only pyserial reader on a
   background thread; parser tolerates malformed/short packets; GCS-side
   mission clock survives a CanSat reset; auto-CSV logging with headers;
   derived values (vertical speed, accel magnitude, apogee, packet loss %,
   RX rate, GPS drift). Bug found & fixed: vertical-speed calc mixed CanSat
   MET and wall-clock time when MET=0, breaking the least-squares slope —
   fixed by making the time axis consistent; also two uninitialized state
   vars (`_met_valid`, `_t0_wall`) were referenced before being set in
   `__init__`.
4. **First structured design critique (June 13)**, run against 7 UX
   lenses (hierarchy/clarity/consistency/contrast/alignment/whitespace/
   CTA). Key findings: the 3D model looked like a rocket (fin+nosecone),
   not a can; sensors from the BOM (lightning, magnetometer, spectral,
   cameras, servos) weren't displayed at all; inconsistent tile/spacing
   components made it look "AI-assembled"; low value/label contrast;
   ragged column alignment; wasted whitespace around an undersized model;
   undifferentiated button styling (CONNECT/START/KILL all looked the
   same). All were addressed in the following rebuild.
5. **"Control-room grade" rebuild**: true can-shaped 3D model (cylinder,
   anodized body, roll-reference stripe, panel rings, whip antennas, NavIC
   patch) over a compass+ground-grid reference, ADI, subsystem health-lamp
   matrix, one consistent Tile component/grid everywhere, a 4-page layout
   (FLIGHT / PAYLOAD & ANALYSIS / CHARTS / CONSOLE), CSV+XML+per-parameter
   graph PNG export. Verified visually by rendering actual pages to PNG
   and inspecting them (not just "doesn't crash"). Two cosmetic bugs found
   and fixed during that pass: (a) a `self.b_rec` naming collision between
   the banner's REC indicator and the command-bar REC button silently
   orphaned the banner one; (b) Qt was eating the `&` in a "PAYLOAD &
   ANALYSIS" tab label as a keyboard mnemonic, rendering as "AYLOAD
   _ANALYSI" — fixed by escaping/avoiding the literal `&`.
6. Deliberate engineering choices carried into the current build (both
   still true per README.md): **pure QPainter 3D rendering with zero
   OpenGL dependency** (won't fail on a competition laptop without a
   dedicated GPU), and **stale telemetry is dimmed with an age-stamp
   rather than hard-zeroed** on a brief dropout, because snapping a real
   altitude to zero mid-gap would itself be dishonest data representation
   — only a genuine "never connected"/link-lost state shows as absent.

The telemetry schema referenced across these chats evolved (17 mandated +
~38 appended fields in the GCS-redesign chat, vs. the 84-field/45-derived
schema documented in the current README) — **the README.md in this repo is
the authoritative, current schema**; treat any field list in the older
chat summaries above as historical, not current.

---

## 9a. `lap1-adaptive-tiger` Claude Code session (2026-08-01 update)

This is a *Claude Code cloud session* (not a claude.ai chat), found at
`https://claude.ai/code/session_01PFdvfXJSuLxg5cSYHioUne`. It operates
directly on this repo's files, not just conversation. State as of this
update:

- It wrote the **first-ever Teensy 4.1 flight transmitter** —
  [vritra_tx/vritra_tx.ino](vritra_tx/vritra_tx.ino) — plus updated
  [esp32rec/esp32rec.ino](esp32rec/esp32rec.ino) (the ESP32 ground-side
  gateway that decodes the compact base-36 air frames back to the 84-field
  CSV the GCS expects). Before this, only an ESP32 range-test transmitter
  existed — there was no real flight firmware.
- Fixed several bugs carried over from the §7 LoRa saga: wrong TCXO voltage
  (`0.0f` → `1.6f`), RF switch table not re-applied after `radio.begin()`
  resets it, `setOutputPower()` missing the HP-PA-forcing flag (the same
  PA-routing bug from earlier), inconsistent preamble length, and no
  queued/timed uplink command delivery.
- The session was then asked to wire in a **DS3231 RTC** and a **7Semi
  TB6612FNG motor driver** (for the GA12-N20 gyro-stabilization wheel) plus
  full GNSS pin connections including the L89's wake pin — but the session
  had **no visible response to that request** (it shows the same message
  repeated, then nothing). That work was picked up and completed directly
  in this conversation instead — see the pin table and functional changes
  below.

### RTC + motor driver + GNSS-aux wiring (completed 2026-08-01, revised to match actual as-built wiring)

All on the Teensy 4.1 TX board ([vritra_tx.ino](vritra_tx/vritra_tx.ino)). The
first pass used a placeholder pin assignment; it was then revised to match
the physical build exactly, including a change from the default SPI0 bus to
SPI1 for the radio:

| Function | Pin(s) | Notes |
|---|---|---|
| DS3231 RTC | shares I2C bus (SDA=18, SCL=19), addr 0x68 | SQW/INT wired to D2 but unused/reserved; 3.3kΩ pull-ups fitted on SDA/SCL |
| TB6612FNG (GA12-N20 gyro wheel, channel A only) | STBY=D36, AIN1=D35, AIN2=D34, PWMA=D33 | VM from battery rail directly (not a Teensy pin), VCC from Teensy 3V3 |
| Quectel L89 GNSS aux | RESET_N=D22 only | WAKE_UP is hardwired directly to 3V3 (always-active, no GPIO); 1PPS is not wired on this build |
| **LR1121 radio** | **SPI1**: CS=D24, SCK=D27, MOSI=D26, MISO=D39, RST=D30, BUSY=D31, IRQ=D32 | Moved off the default SPI0 bus (10/11/12/13) onto Teensy 4.1's SPI1 peripheral — `Module()` constructor and `radioInit()` both updated to use the `SPI1` object instead of `SPI` |

Functional additions that came out of wiring the RTC in for its actual
purpose (not just connecting a pin):

- **Fixed a real reset-survival gap.** `state`, `pktCount`, and `padAlt_m`
  previously lived only in RAM — a mid-flight Teensy reset (brownout,
  watchdog, `CMD_RESET`) would have silently dropped back to `FS_BOOT` and
  **re-taken the pad-altitude reference from whatever pressure existed at
  reset time**, corrupting `ALT_M` for the rest of the flight, despite the
  PDR audit (§4) claiming reset-survival was satisfied. Now persisted to
  the Teensy's onboard EEPROM (state, packet count, pad altitude, gyro
  bias, attitude reference matrix) and restored on a "warm" boot; the
  pad-altitude and attitude self-surveys only re-run on a genuinely
  **fresh** boot (no valid EEPROM record).
- **`TIME_S` now survives a reset** once the RTC's wall-clock epoch is
  known: computed as `rtc.now() - missionBootEpoch` instead of
  `millis()/1000`, which always resets to 0. The very first time the board
  is ever powered (before the DS3231's coin cell has ever been set), the
  epoch is established from the GPS's first valid UTC fix; every boot
  after that the RTC already has correct time on its own.
- **Gyro wheel is now auto-staged by the FSM**, directly implementing a
  fix the red-team PDR audit (§4) called for: it spins up at `FS_LAUNCH`
  and shuts off at `FS_SEC_EJECT`, because a spinning momentum wheel
  resists yaw while parafoil steering needs yaw — running both at once
  makes them fight each other.
- New ground commands: `GYRO_ON` / `GYRO_OFF` / `GYRO_SPD,<-100..100>`,
  `GPS_RESET` (hardware pulse via `PIN_GPS_RESET`), and `NEW_MISSION`
  (ground-crew command to force a genuinely fresh mission — clears the
  EEPROM record and reboots — for use before each new launch attempt so a
  previous test's state doesn't leak into a real flight).

**Not yet done / needs verification**: gyro-wheel telemetry (RPM/speed) is
not on the wire — there's no schema slot for it in the current 84-field
`SCHEMA` (see [README.md](README.md) §4), so it's controllable from the
ground but not currently observable in the GCS. The `RTClib` (Adafruit)
Arduino library needs to be installed for this to compile; `EEPROM.h` is
built into Teensyduino already. The `GYRO_ON` speed of 70% is a placeholder
— needs tuning against the actual angular-momentum authority calculation
the PDR audit asked for (§4, inquisition question 4's neighbor finding).

## 9. Known open items / things to verify going forward

- Confirm which RF plan is actually final: the electronics-chat's dual-
  radio 865.5/866.5 MHz SF9/SF7 plan, or the CDR-locked single 867.000 MHz
  SF10 sync-0x98 plan (§3 vs §5 above disagree).
- Confirm whether the 2 km field range test was ever completed — the LoRa
  saga's visible transcript ends at successful *bench*-range reception
  (1-2m), not a full walked-out range test.
- Confirm which LR1121 firmware family (RadioLib vs raw lr11xx_driver) is
  actually on the current hardware.
- Re-verify the mass budget against the real carbon-rod + fiberglass +
  PETG structure (the 995.4 g PDR figure was computed against the old,
  since-abandoned CFRP lattice design).
- Confirm the FSM state names in the actual firmware match the guideline's
  8 canonical names (BOOT, TEST_MODE, LAUNCH_PAD, ASCENT, ROCKET_DEPLOY,
  DESCENT, AEROBREAK_RELEASE, IMPACT) — 3 different naming schemes existed
  across PDR documents.
- Confirm dual/independent GNSS wiring was actually implemented (vs. the
  flagged single-shared-L89 SPOF).
- Confirm the bio payload's final form (sealed passive sampler vs. open
  petri plate) and whether a biosafety sign-off was obtained.
- Confirm primary chute was actually resized (~D305mm target) to bring
  sink rate into the legal 15-25 m/s band.
