# Deep-research prompt: guided ram-air parafoil for VRITRA CAN-7USAT 2026

> Paste everything below the line into the deep-research tool.
> Before pasting, replace every `[FILL: ...]` with your real value. If you don't know a value, delete the brackets and write "unknown — estimate it".

---

## Role and goal

You are a senior guidance, navigation and control (GNC) engineer. You have flown guided ram-air parafoils at small scale: ARLISS "Come-Back" CanSats, rocketry recovery, and precision airdrop research such as JPADS and Snowflake ADS.

Research deeply and produce a complete, build-ready engineering plan. The plan must let our student CanSat steer a 7-cell ram-air parafoil, using two servos, from 600 m altitude to a landing as close as possible to a target coordinate.

The plan must state:
- exactly what hardware we still need (with Indian sources, prices and links where possible);
- how to size and rig the parafoil and steering lines;
- which guidance, navigation and control algorithms to use, with pseudocode and gains;
- how to simulate and test before the competition.

Cite sources (papers, theses, datasheets, open-source repositories, competition reports) for every key number. Clearly separate **verified facts**, **engineering estimates** and **assumptions**.

## 1. The competition

- **Competition:** IN-SPACe CAN-7USAT India Student Competition 2026. Team VRITRA, Team ID 2026-INSPACe-CAN-7USAT-025.
- **Flight profile** (from our mission summary of the rules):
  1. A rocket carries the CanSat to about 1000 m (±100 m) and ejects it.
  2. A primary drag parachute deploys immediately on ejection. Descent rate must be under 20 m/s (±5 m/s).
  3. At **600 m ±10 m**, a secondary mechanism deploys: a **GPS-guided 7-cell ram-air parafoil**. It must reduce descent to **1–3 m/s** and steer toward the target.
  4. A mechanical gyroscopic flywheel stabilises the CanSat. Its spin rate is mandatory telemetry field 14.
  5. LoRa telemetry goes to our ground station throughout the flight. A buzzer/beacon (≥92 dB) sounds after touchdown.
- **Target landing coordinate, verbatim from the organisers:**
  "The launch coordinates for the targeted landing site of the CAN-7USAT for the competitions are 26.717535 deg North and 84.301543 deg East. Depending on local wind vectors, the actual landing site may vary in proximity to these coordinates; therefore, provisions should be incorporated within the software to allow for adjustments if required."
- **Competition dates and time of day:** [FILL: dates / month]
- **How landing accuracy is scored, and any steering rules** (e.g., is steering allowed only after 600 m, are there distance bands): [FILL or "unknown — find in the rules"]
- **CanSat envelope:** [FILL: diameter × height] (our PCBs are Ø110 mm)
- **Total CanSat mass under the parafoil:** [FILL: kg]
- **Ejection orientation and how the drag chute → parafoil transition happens mechanically:** [FILL: e.g., MG90S servo releases a door, drag chute pulls the parafoil out / drag chute is cut away / stays attached]
- **Parafoil:** [FILL: bought or home-made? area m², span, chord, cell count = 7, line lengths, material]

## 2. Our electronics (already designed; PCBs ordered)

The CanSat has five stacked Ø110 mm boards plus off-board modules. All logic is 3.3 V.

| Board | Processor | Role relevant to gliding |
|---|---|---|
| 1 — Primary telemetry (4-layer) | Teensy 4.1 (600 MHz Cortex-M7, FPU) | Owns the **one GNSS receiver**: Quectel **L89HA** (7Semi breakout), GPS/GLONASS/Galileo/BeiDou/QZSS + **NavIC L1/L5**, with an active NavIC L1/L5 antenna. Its NMEA TX is broadcast through isolated buffers to all three processors. Also: LSM6DSO32 IMU (SPI), BMP585 barometer, Core1121-XF LoRa 868 MHz radio, W25Q64 flash, microSD, DS3231 RTC, MAX98357A amp + speaker. |
| 2 — Environmental (perfboard) | — | Science sensors (SCD41, AS3935 lightning, ML8511 UV, MQ-131 ozone, BMV080 particulates, AS7343 spectral, HDC3020). Not relevant to steering. |
| 3 — Backup telemetry (2-layer) | Teensy 4.1 | Hot backup of Board 1, with its own ICM-20948 9-axis IMU, BMP585 and Core1121 LoRa radio. Drives TAKEOVER, which switches the GNSS command line and the speaker from Board 1 to Board 3. Also: INA260 5 V monitor, UART mux to two XIAO ESP32-S3 Sense cameras. |
| 4 — Power (perfboard) | — | 3S Molicel P50B 21700 Li-ion + 3S 40 A BMS. AP64500 bucks: 5 V logic, 5 V servo rail, **8.0 V steering rail (one 5 A buck shared by both steering servos)**, 5 V logger. AP62300 3.3 V bucks. DFR1015 5 V camera buck. INA226 monitors. |
| 5 — **Parachute controller (2-layer)** | **Teensy 4.0** | **Runs the parafoil.** It has its own BMP585 barometer (I²C 0x46) and its own **ICM-20948 9-axis IMU** (accel + gyro + AK09916 magnetometer, I²C 0x69). It receives GNSS NMEA only (Serial1 RX0; no GNSS configuration from this board). It also has a DS3231 RTC and a microSD black-box logger. |

### Board 5 pin map (fixed in hardware)

- **Steering servos:** two **AGFRC A20CLM** coreless digital servos on 8.0 V. Rated 4.8–8.4 V, about 10.2 kg·cm, about 2.2 A loading at 8.4 V. Each has its own fuse but they share one 8 V buck.
  - Left: Teensy pin 2 (connector J6).
  - Right: pin 3 (J7).
- **Other actuators:** two MG90S deployment servos (pins 4, 5), on 5 V; flywheel ESC (pin 6), driving an EMAX RS2205 2300 KV motor on battery voltage; Hall sensor for flywheel RPM (pin 9).
- **Command links in:** UART from primary Board 1 (pin 7) and from backup Board 3 (pin 15).
  - **Ground-station uplink path:** LoRa → Board 1 or Board 3 → UART → Board 5.
  - This is how a new target coordinate or a wind offset can be sent in flight.
- **Telemetry out:** pins 1 and 8.
- **SD logger:** SPI 10–13, card-detect pin 16. **RTC and sensors:** I²C on pins 18/19.
- **Firmware:** Teensyduino / Arduino C++. We write all firmware ourselves.

### Hardware questions to answer

Answer each with a recommendation:
1. Is one L89 enough for guidance? Find:
   - its maximum NMEA update rate (can it do 5–10 Hz?);
   - its velocity and course-over-ground accuracy at 3–8 m/s;
   - its time to first fix and how it behaves under the rocket's high g;
   - whether NavIC improves accuracy at this location (northern Bihar);
   - which NMEA/PQTM sentences to enable;
   - how to configure it from Board 1/Board 3 through the shared command line.
2. Heading source. Compare:
   - GNSS course-over-ground;
   - the AK09916 magnetometer;
   - gyro-integrated heading fused with GNSS.

   Account for magnetic interference from the servos, the 3S wiring, the brushless flywheel motor and the battery currents. Give a calibration procedure (hard/soft iron) and the magnetic declination at the target.
3. **Gyroscopic flywheel versus steering (critical).** A spinning flywheel stores angular momentum. Analyse:
   - how it affects the yaw turn rate the parafoil can achieve, and whether it causes precession, coning or a rolling moment when the canopy turns;
   - whether the flywheel's axis orientation matters;
   - whether we should reduce or stop the wheel after parafoil deployment, or mount the payload with a swivel or riser so the canopy can yaw freely.

   Quantify with our masses: [FILL: flywheel mass, radius, rpm].
4. Is the A20CLM (10.2 kg·cm) strong and fast enough for brake-line pull at our wing loading? Specify:
   - servo arm versus spool/winch, drum diameter, line travel needed (typically a percentage of chord), and the force needed;
   - the 8 V current budget when both servos start or stall together.

   If it is not enough, recommend a replacement available in India.
5. Is the Teensy 4.0 sufficient for EKF + guidance at 50–100 Hz while also running deployment and the flywheel? (Expected: yes.) Give a recommended task schedule.
6. List any extra sensor that would materially improve landing accuracy, with part numbers. Candidates: a second GNSS for heading, a pitot/airspeed sensor, a downward rangefinder or LiDAR for flare. Label each **must-have** or **nice-to-have**, and remember we cannot easily change the PCBs now. Spare connector pins exist on Board 1 J8 / Board 5 J13 pin 10 and on the Board 3 UART mux channels.

## 3. Parafoil and rigging research

- Size the 7-cell ram-air canopy for our mass so steady descent is 1–3 m/s. Give:
  - area, span, aspect ratio, airfoil (e.g., Clark-Y), cell geometry, anhedral/arc, line lengths, cascade layout and trim (rigging) angle;
  - expected glide ratio (L/D), forward airspeed, sink rate and turn rate versus brake deflection.
- Brake/steering geometry: one servo per side pulling left/right trailing-edge brakes, plus symmetric brake for glide-path control and a landing flare. Specify brake line routing, attachment to the CanSat body, line-twist prevention and how to avoid servo fouling during deployment.
- Deployment and opening: transition from the drag chute at 600 m, opening shock loads, slider/reefing for small canopies, and failure modes (line twists, cell collapse, spiral). Explain how firmware detects a bad canopy, e.g., abnormal descent rate or spin.
- Materials and buying: ripstop nylon (zero-porosity), line type and strength. Name ready-made small parafoils (RC or rocketry, ~0.3–1 m²) available in India or internationally, with links and prices.

## 4. Guidance, navigation and control

Give equations, block diagrams, **Teensy-ready pseudocode / C++ structure**, starting gains and tuning procedures for:

1. **Navigation filter:**
   - fuse GNSS position/velocity, BMP585 altitude, gyro, accelerometer and (optionally) magnetometer;
   - say whether to use an EKF or a complementary filter for this compute;
   - handle GNSS dropouts and the rocket-phase acceleration.
2. **Coordinates:**
   - convert latitude/longitude to a local North-East-Down frame centred on the target (26.717535° N, 84.301543° E);
   - give the WGS-84 maths and the precision needed;
   - use double versus float carefully (the Teensy 4.0 FPU is single precision — explain what to do).
3. **In-flight wind estimation:**
   - from GNSS ground velocity versus heading (circling or "wind-circle" method, least-squares fit), updated continuously;
   - how long it takes and what accuracy to expect.
4. **Guidance strategy:**
   - compare homing (point at target), proportional navigation, the "T-approach", energy management (holding/spiral or figure-8 over the target) and the final approach **into the wind**;
   - recommend one for 600 m altitude, about 300 s flight time and likely Bihar winds;
   - show how to compute reachable footprint/range from altitude, glide ratio and wind, and what to do when the target is unreachable (best-effort closest approach).
5. **Low-level control:**
   - heading/turn-rate controller (PID or similar) that maps to differential brake;
   - symmetric brake for sink control;
   - servo limits, rate limits and deadband;
   - anti-oscillation and protection against turn-induced spiral dive;
   - when to flare, and when to stop steering near the ground. Use BMP585 altitude above ground: we must know ground elevation at the target, so find the terrain elevation at the coordinate.
6. **Software provision for adjustment** (required by the rules):
   - **Before launch:** target lat/lon and ground elevation/pressure stored in EEPROM, settable over USB and over the LoRa uplink.
   - **In flight:** commands to move the target, apply a north/east offset in metres, set a wind override, or abort steering (neutral glide / "spiral down").
   - Packet format, acknowledgement, CRC, range checks and sanity limits so a corrupted packet cannot send the CanSat away.
   - **Failsafes:** what Board 5 does if GNSS is lost, if both Board 1 and Board 3 die (it must still steer on its own), or if the magnetometer is bad.
7. **Logging and telemetry:**
   - which guidance variables to log on the Board 5 SD card and send to the ground station;
   - how to replay flights for tuning.

## 5. Simulation and testing plan

- A simulator we can run on a laptop (Python preferred): 4-DOF or 6-DOF parafoil dynamics, wind profiles and gusts, GNSS noise, servo lag. Name and link open-source parafoil models or repositories to start from, e.g., ARLISS team code, ArduPilot/PX4 parafoil or parachute work, academic 6-DOF/9-DOF models.
- Use **software-in-the-loop**, then **hardware-in-the-loop** with the real Teensy 4.0.
- Field tests, in order:
  1. ground tests (servo travel, GNSS rate, magnetometer calibration);
  2. car-top / kite tow tests;
  3. drone drops from 100–400 m (check Indian drone/DGCA rules);
  4. full-altitude test if possible.

  For each test give pass/fail criteria (e.g., circular error probable, CEP, at the target).
- Expected realistic landing accuracy (CEP) for a student system like ours, with evidence from published results.

## 6. Site research for the target coordinate

For 26.717535° N, 84.301543° E (research location, likely West Champaran / northern Bihar):
- terrain elevation and obstacles (trees, buildings, power lines, water bodies);
- typical surface and 0–1000 m winds for [FILL: competition month] (ERA5 / IMD / radiosonde data), and typical gusts and thermals at midday;
- magnetic declination and inclination at the site (WMM/IGRF);
- GNSS/NavIC visibility at that latitude.

## 7. Required output format

1. **One-page summary:** what to buy, what to change, the chosen algorithm, and expected accuracy.
2. **Bill of materials** for everything still missing, with quantity, part number, Indian supplier link, approximate ₹ price, and must/nice-to-have.
3. **Parafoil and rigging specification** with dimensions and a drawing description.
4. **Power and servo budget** for the 8 V steering rail.
5. **GNC design:** equations, state machine (ejection → drag chute → 600 m deployment → canopy check → wind estimation → homing → energy management → final approach → flare → touchdown → beacon), and pseudocode for the Teensy 4.0, using the pin map above.
6. **Uplink command set** for target adjustment, with a packet definition.
7. **Simulation and test plan** with pass/fail criteria.
8. **Risk register:** top 10 failure modes with mitigations. Include the flywheel interaction, servo stall, GNSS loss, line twist, magnetometer interference, and wrong ground elevation.
9. **Sources:** links for every key number.

Do not give generic advice. Tie every recommendation to our specific hardware, masses, altitudes, pins and the target coordinate above. Where a number depends on a value marked [FILL], show the formula and a worked example with a stated assumption.
