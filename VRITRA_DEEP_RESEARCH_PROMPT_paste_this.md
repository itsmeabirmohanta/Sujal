# DEEP RESEARCH PROMPT — VRITRA CanSat (Team 2026-INSPACe-CAN-7USAT-025)

> Paste this whole file as the prompt and attach the zip. Read `01_PROJECT_BRIEF_AND_PARTS.md` first; it is the current truth.

## Your role
Act as the **lead avionics and GNC (guidance, navigation and control) engineer** of a flight-computer team that designs
the way a national space agency such as ISRO would: requirements first, interfaces controlled, every number traced to a datasheet
or a test, every failure mode handled, every decision justified.
You are designing the **complete flight software architecture and the precision-landing guidance** for our CanSat, using
**only the hardware we already have and the pins already wired** (listed in the brief). If something truly needs a hardware change,
put it in a separate list with cost and effort; do not assume it in the main design.

## What is in the zip (read all of it; nothing is hidden)
| Folder | Contents |
|---|---|
| `01_PROJECT_BRIEF_AND_PARTS.md` | Mission, every part with its **maker and link**, every pin, every data path, lessons learned |
| `02_competition_documents/` | Official **Mission Requirements & Guidelines (IN-SPACe.CAN.MR.CG.01)**, the July **frequency allocation plan** (superseded for our values), our **official team frequency assignment of 19-10-2026** (use this one; read the PDF, the text copies have shifted columns), our **CDR** (PDF + Word) |
| `03_kicad_current_boards/` | KiCad files of the 4 boards that fly (Board 1 MAIN, Board 2 BACKUP, Board 3 POWER, Board 4 PARACHUTE on ESP32-P4-Pico) with hole-by-hole build lists, pictures and DRC reports |
| `04_handover_and_design_docs/` | PCB handover, earlier AI coding prompt, earlier parafoil guidance research prompt, KiCad guide, Board 4 wiring page, attitude-estimation diagnostics, parafoil engineering study (CFD/FEA reports, parameters, Octave scripts) |
| `05_code/` | The GitHub repo (flight software `vritra_fc`, board tests, ground-station gateway `vritra_gs`, camera `vritra_cam`, **GCS GUI `vritra_gcs.py`**, docs) + the August history copy + the 7Semi L89HA Arduino library |
| `06_chats/` | Every chat we had with Claude about this CanSat (our messages and the answers), August to 28-10-2026 |
| `07_memory/` | The engineering memory files (decisions, changelog, pin maps, final build state) |
| `08_archive_zero_board_workspace/` | Full working folder: older board versions, routing generators, Gerbers |
| `09_mechanical/` | CanSat STEP model |

## Constraints you must respect
- Launch campaign **31-10-2026 → 03-11-2026** in Kushinagar (UP, India). Today is 28-10-2026. Rank every recommendation as
  **(A) must do before launch**, **(B) should do if time allows**, **(C) future**.
- The **pins and wiring are fixed** (brief §3–4). The L89 HA GNSS TX line is shared by three MCUs; **only Board 1 can command it**.
- Radio: 866.50 / 865.25 MHz, SF9, BW 125 kHz, CR 4/5, sync 0x99, ≥ 1 Hz telemetry in the exact field order of the guidelines.
- Board 4 is an **ESP32-P4** (Arduino-ESP32 core 3.3.x or ESP-IDF), Boards 1 and 2 are **Teensy 4.1** (Teensyduino).
- The guidelines PDF is the authority for telemetry fields, flight states, GCS behaviour and safety rules. Where our mission change
  (drone drop from about 1 km, guided parafoil to 26.717535 N, 84.301543 E, adjustable target, petri lid open 10 s at 950 m above boot)
  interacts with the guideline rules (for example the 600 m secondary-mechanism requirement), say exactly how to satisfy both.
- **No fake data anywhere.** Steering must use the live measured position and velocity. No simulated, replayed or default coordinates
  may ever reach the guidance loop or the telemetry; show how the software guarantees this (validity flags, age checks, fix-quality gates).

## Research tasks

### 1. Every sensor and module — datasheet-level configuration
For **each** part below, find the **official datasheet, reference manual / register map, application notes, and the maker's
official driver or SDK**, then give a configuration table: interface, address, every register or command we should write at boot with its
value and the reason, output data rate, range, filter / oversampling settings, FIFO and interrupt use, power-on timing, self-test,
error detection, recovery from a hung device, and the exact read sequence (burst reads, data-ready, timestamps).
Give links to the documents and to good **GitHub repositories** (official and community) you used.
- **Quectel L89 HA** (Airoha AG3335M) — see task 2 (the most important).
- **Bosch Sensortec BMP585** (7Semi breakout) on Board 1, Board 2 and Board 4: best OSR / IIR / ODR for a 1 km descent at 1–8 m/s,
  pressure-to-altitude formula, pad reference, temperature compensation, noise in metres, how fast it can detect 950 m and 600 m reliably.
- **TDK InvenSense ICM-20948** (7Semi breakout, address 0x69) on Board 1: accel and gyro ranges, DLPF, sample-rate dividers,
  AK09916 magnetometer access (I²C master / bypass), DMP or not, bank switching, the wedge-recovery procedure, hard/soft-iron calibration.
- **STMicroelectronics LSM6DSO32** (Adafruit 4692, 0x6A) on Board 4 (and Board 2, where it misbehaves — tell us how to diagnose it):
  ODR, full scale, filters (LPF1/LPF2, HPF), FIFO with timestamps, data-ready interrupt (we have no INT pin wired — say how to work without it).
- **Semtech LR1121** (Waveshare Core1121) with **RadioLib**: exact init for our plan (TCXO 1.6 V, RF switch DIO5/DIO6), packet time-on-air at SF9,
  how many bytes fit in our ≥ 1 Hz budget, uplink command windows, CRC, power settings.
- **Texas Instruments INA260**, **Analog Devices DS3231**, **TI HDC3020**, **Winbond W25Q64**, **Seeed XIAO ESP32-S3 Sense** camera control,
  **Seeed Grove Buzzer** beacon, **Espressif ESP32-P4** (LEDC/MCPWM for servos, UARTs, I²C, timers, dual core, FreeRTOS), **PJRC Teensy 4.1**
  (IntervalTimer, DMA UART, I²C at 400 kHz/1 MHz, EEPROM emulation, SD on SDIO).
- Actuators: **AGFRC B26CLM** steering servos, **Tower Pro MG90S**, the ESC + **EMAX 2205** wheel, Hall sensor.

### 2. The GNSS — Quectel L89 HA (AG3335M): get the best possible position, fast
We want **everything** about this module and how to use it with our existing pins (UART RX7/TX8 at 9600 by default, RESET_N on pin 6,
and the breakout's 3D_FIX, WAKE, JAM, GEO, SDA/SCL, BAT pins — check the KiCad files for which are wired).
1. Find the **L89 (HA) hardware design guide, protocol specification (PAIR / PMTK / PQTM command manuals), and the Airoha AG3335 documents**,
   plus Quectel forum threads and GitHub code that actually configures an AG3335/L89/LC29H/LC76G-family receiver.
2. Give the **complete boot configuration command block**, each command with its checksum, expected ACK (`$PAIR001`), and why:
   - **Multi-constellation**: GPS + Galileo + BeiDou + GLONASS + QZSS + **NavIC** (`$PAIR066` field order), what the channel budget allows,
     whether L5 is usable for anything other than NavIC on this module, and which combination gives the best accuracy over India.
   - **Baud rate** change (e.g. 115200) and how Boards 2 and 4, which only listen, follow the change safely (detect and re-sync).
   - **Update rate** (1 / 5 / 10 Hz) and which NMEA sentences to keep (GGA, RMC, GSA, GSV, VTG, GST…) so the line is not overloaded;
     or a binary / proprietary output if one exists and is better.
   - **Navigation mode / dynamic model** for a slow descending glider, SBAS / **GAGAN** use, elevation mask, static-hold / velocity thresholds,
     1PPS (if reachable), jamming detection (JAM pin), geofence (GEO pin).
   - **Saving the configuration** to the module's flash / backup so it survives power cycles.
3. **Time-to-first-fix**: everything that shortens a cold start — hot/warm/cold restart commands, keeping the **BAT / V_BCKP** pin powered,
   **EPO / AGNSS / reference position + time injection** (we have the DS3231 RTC and a known launch site), how to load assistance data
   from Board 1 or from the ground station before launch, and how to prove a fix is real (fix type, HDOP/PDOP, satellites, accuracy estimate,
   age of data).
4. **Sharing GNSS data**: the L89 is the **primary** position source on Board 1, and the same NMEA stream also reaches Board 4 (guidance)
   and Board 2 (backup). Design the best scheme: should Board 4 parse the raw NMEA itself (lowest latency) and also receive Board 1's
   fused navigation state over Serial3, or only one of them? Define the message format, rate, timestamps, latency, checksums and what
   happens when either source is lost.
5. Antenna: our breakout has an on-board patch; check whether it is L1-only or L1+L5 and what that means for NavIC.

### 3. System architecture — like a real flight computer
Deliver:
1. **Requirements list** (derived from the guidelines PDF and our mission), each traced to a test.
2. **Interface Control Document (ICD)**: every UART/I²C/SPI link, message format, rate, byte budget, checksum, and timeout, for Boards 1, 2, 4,
   the camera and the ground station (including the existing GCS command set in `vritra_gcs.py` / `vritra_gs.ino`).
3. **Software architecture per board**: tasks, rates and priorities (for example 200–1000 Hz IMU, 50–100 Hz baro, GNSS at its rate,
   guidance at 10–50 Hz, servo output, telemetry 1 Hz, logging), scheduler type (super-loop with timers vs FreeRTOS on the P4),
   interrupt use, DMA, **no blocking calls**, a CPU and timing budget, watchdogs.
4. **Data pipelines, stage by stage**: sensor read → timestamp → unit conversion → calibration → validity/plausibility checks →
   filtering → sensor fusion → state estimate → guidance → control → actuator command → logging → telemetry. For each stage give the code
   block, data types, units and update rate.
5. **State estimation**: attitude (Madgwick / Mahony / EKF) and a **position–velocity–wind estimator** (e.g. EKF/UKF fusing GNSS, baro, IMU,
   magnetometer) suitable for these CPUs; equations, tuning values, and how to run it in real time.
6. **Flight state machine** that meets the guidelines (states 0–7 and their numbering) and survives resets (EEPROM / RTC / flash), with
   the exact detection logic and thresholds for drone lift, release, 950 m petri event, bay opening, parafoil phase, landing.
7. **Redundancy and fault management**: Board 1 ↔ Board 2 takeover (heartbeat on pin 5 → pin 4), Board 4 running alone if both links die,
   FMEA table, safe defaults for every actuator.
8. **Telemetry and logging**: exact CSV packet per the guidelines, SD and flash formats, ground-station CSV, how to keep ≥ 1 Hz on SF9.
9. **Calibration procedures** we can do in the field before launch: gyro bias, accelerometer 6-position, magnetometer hard/soft-iron,
   baro pad zero, INA260 against a meter, GNSS position check against a surveyed point; what to store and where.

### 4. Precision landing — guidance, PID and servos (the core of the mission)
1. Research **guided parafoil / ram-air precision landing** (papers, theses, open-source code: e.g. ArduPilot / PX4 parafoil or glider modes,
   academic GNC for small parafoils, "energy management" and "terminal guidance" schemes, wind estimation from GNSS ground track).
2. Design the **guidance law** for our parafoil (0.40 m², about 1 kg, from about 1 km): homing / heading-to-target, energy management
   (spiral or figure-eight above the target to lose height), wind-compensated final approach into the wind, flare decision if any,
   and what to do when GNSS is degraded. Use the **live** GNSS position and velocity only.
3. **Control**: map a heading-rate or turn-rate command to differential brake deflection; inner/outer loops; **PID structure and gains**
   with a step-by-step tuning method we can do in a few drops or on the ground (system identification from the first glide,
   Ziegler–Nichols alternatives, gain scheduling with airspeed / brake setting), anti-windup, rate limits, dead-band, saturation.
4. **Timing**: show that the decision loop runs in milliseconds on the ESP32-P4: loop period, latency from GNSS fix to servo pulse,
   jitter, and how to time-stamp everything.
5. **Servos**: B26CLM (steering) and MG90S (bay, petri): pulse width ranges, 50 Hz vs 333 Hz frames, centre 1520 µs, how much brake
   line travel per degree, holding torque and current, behaviour when the signal stops or power dips, whether gears can be back-driven,
   how to "lock" a position safely, **software interlocks** (no actuation before release, petri only once at 950 m, bay logic, ESC armed only when
   allowed), and how to avoid brown-outs on the 7.4–8.0 V steering rail.
6. **Test plan** to prove landing accuracy: hardware-in-the-loop with recorded data, ground tests (walk/ car test with GNSS), drop tests,
   what to log and how to tune from the logs.

### 5. Code
For each board give production-quality code structure and the key code blocks (C++ for Teensyduino and Arduino-ESP32 / ESP-IDF),
module by module (drivers, scheduler, estimator, guidance, control, actuators, links, telemetry, logging, FSM, fault handling),
following the pins in the brief. Point out what in our existing `vritra_fc` and board tests should be **kept, changed or removed**.
List the libraries (with versions and GitHub links) you recommend and why.

### 6. Ground station
Read `vritra_gcs.py` and `vritra_gs.ino`. Say what must change for the new mission: adjustable target command, live map of position and
predicted landing point, wind estimate display, guidance mode, link health; keep the guideline-mandated plots and CSV.

## How to answer
- Cite every number with a source link (datasheet page or section, GitHub file, paper). Mark anything unverified.
- Give tables for configurations, and code blocks for every pipeline stage.
- Finish with: (1) a **priority list A/B/C** for the next 3 days; (2) a **checklist** to run on launch day; (3) open questions only our team can answer.
