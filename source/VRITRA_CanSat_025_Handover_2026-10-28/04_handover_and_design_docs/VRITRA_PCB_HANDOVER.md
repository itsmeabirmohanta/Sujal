# VRITRA CAN-7USAT 2026 — PCB release handover, rev C (6 October 2026)

Team VRITRA · 2026-INSPACe-CAN-7USAT-025 · KiCad 10.0.6 · fabrication at Robu.in
Project folder: `C:\Users\kalbh\Documents\KiCad\cansat final pcb\` — only the five projects, their libraries and this file.

Rev C answers your component list (32 items) and photos: every part is now on a PCB or drawn in a schematic, the UART mux, INA260 and a third data logger are added, the beacon amplifier can be driven by either Teensy 4.1, every module footprint was checked against the real board, and STEP models are in the library.

---

## 1. What to order / build

| Board | Build | Upload to Robu | Verified on the released files |
|---|---|---|---|
| 1 Primary telemetry | **4-layer PCB** | `Board 1\manufacturing\Board1_Robu_gerbers_revC.zip` | ERC 0 · DRC 0 · unconnected 0 · parity 0 · mask PASS · 0 vias in pads · no tracks on the inner planes |
| 2 Environmental | **perfboard** | — wire from `Board 2\Board 2\Board 2.kicad_sch` | ERC 0 |
| 3 Backup telemetry | **2-layer PCB** | `Board 3\manufacturing\Board3_Robu_gerbers_revC.zip` | ERC 0 · DRC 0 · unconnected 0 · parity 0 · mask PASS · 0 vias in pads |
| 4 Power (EPS) | **perfboard** | — wire from `Board 4\Board 4.kicad_sch` | ERC 0 (7 expected "module has no footprint" notes) |
| 5 Parachute + power hub | **2-layer PCB** | `Board 5\manufacturing\Board5_Robu_gerbers_revC.zip` | ERC 0 · DRC 0 · unconnected 0 · parity 0 · mask PASS · 0 vias in pads |

DRC = every rule at *error* severity, zones refilled, schematic parity on. Mask audit = every opening in the exported F/B mask Gerbers equals a pad; all tracks, vias and pours are covered, all vias tented.

Robu order form (all three): FR-4 1.6 mm, ENIG, **green** mask (keeps Robu's 0.20 mm web), white silk, tented vias, electrical test. Board 1: 4 layers, 1 oz outer / 0.5 oz inner, layer order F.Cu → In1 = GND → In2 = +3V3 → B.Cu. Boards 3/5: 2 layers, 1 oz. Note to Robu: "4 × Ø8.5 mm + 1 × Ø13 mm openings are routed from the outline layer". Boards 3 and 5 also carry 4 × Ø2.2 mm NPTH (ICM-20948 mounting holes).
Smallest drill 0.30 mm, smallest track 0.25 mm (Board 1/5), 0.30 mm (Board 3); SD-card nets on Board 5 use 0.20 mm clearance (Robu 2-layer minimum 0.127).

Each `manufacturing\assembly\` holds BOM, pick-and-place, schematic PDF, 1:1 fit-check PDF, top copper and **bottom assembly (mirrored)** PDFs — Boards 1 and 5 now have parts on the underside.

## 2. Your 32 components — where each one is

| # | Part | Where | Notes |
|---|---|---|---|
| 1 | 7Semi BMP585 ×3 | B1 U4, B3 U3, B5 U2 | I²C 0x46 (SDO low, CS high) |
| 2 | Adafruit LSM6DSO32 | B1 U5 | SPI, CS = Teensy pin 2 |
| 3 | 7Semi ICM-20948 ×2 | B3 U2, B5 U3 | I²C 0x69, CS/AD0 to the module's 1V8 pin |
| 4 | 7Semi SCD41 | B2 U1 (perfboard) | ENV bus |
| 5 | DFRobot AS3935 (SEN0290) | B2 U2 | IRQ → B1 pin 26 |
| 6 | GY-ML8511 | B2 U6 | VIN from +5V, OUT → B1 A3 |
| 7 | MQ-131 | B2 U7 | AO via divider → B1 A8 |
| 8 | 7Semi BMV080 | B2 U3 | ENV bus |
| 9 | TPL5010 | B1 U8 | watchdog / heartbeat to Board 3 |
| 10 | 7Semi W25Q64 flash | B1 U7 | SPI, CS = pin 3 |
| 11 | **7Semi INA260** | **B3 U7 (new)** | in series with the whole avionics +5 V; read by Board 3 (Wire2) **and** Board 1 (Wire2, via Board 2 J4) |
| 12 | **7Semi UART mux** | **B3 U10 (new)** | Board 3 Serial8 → XIAO camera 1 / camera 2 (+2 spare channels) |
| 13 | DS3231 (ZS-042) ×2 | cabled to B1 J7 and B3 J4 | drawn as MOD1 in both schematics |
| 14 | Teensy 4.1 ×2 | B1 U1, B3 U1 | |
| 15 | ESP32-S3 DevKit | **ground station** | not part of the flight stack (CDR: GS processor) |
| 16 | XIAO ESP32S3 Sense ×2 | 5 V from B4 J12/J13, UART from B3 J10/J11 | drawn as CAM1/CAM2 on Board 3 |
| 17 | Waveshare Core1121-XF ×2 | B1 U2, B3 U4 | |
| 18 | Bandit 868 T-antenna | B1 U2 LoRa u.FL (AE1) | **the backup radio (B3 U4) needs its own 868 MHz antenna** |
| 19 | 7Semi L89HA | B1 U3 | TX broadcast to B3/B5, RX from B1 **or B3** (U10) |
| 20 | NavIC L1/L5 antenna | B1 U3 u.FL (AE2) | |
| 21 | MG90S 360° (dish) | B5 J11 (+ PTC F1) | new lid end-stop switches on B5 J16 |
| 22 | EMAX RS2205 | on the ESC | |
| 23 | Bidirectional 40 A ESC | power B4 J10 (XT30), signal B5 J10 | B5 J10 "+" pin is unconnected: the ESC's 5 V BEC is never tied to a rail |
| 24 | 3S 40 A BMS | B4 BMS1 (cabled, drawn) | pack → BMS → fuse → XT60 J1 |
| 25 | DFRobot DFR1015 | **B4 U8 (new)** | dedicated 5 V camera buck |
| 26 | 7Semi AP64500 ×3 | B4 U3 (+5V), U4 (+5V_SERVO), U5 (V_SERVO_A) | |
| 27 | 7Semi AP62300 ×2 | B4 U6 (+3V3_A), U7 (+3V3_B) | value text corrected from AP62301 |
| 28 | Teensy 4.0 | B5 U1 | |
| 29 | SM400204-1 speaker | on U9's screw terminal (LS1) | neither terminal is GND |
| 30 | MAX98357A | B1 U9 | fed by Board 1 **or** Board 3 through U10 |
| 31 | LEDs | B1 D1 (OK), D2 (fault); B4 D1 (power) | |
| 32 | Connectors | JST EH (harness), JST VH (5 V loop), 2×3 header (takeover), XT30/XT60, screw terminals | |

## 3. What changed in rev C

**Board 1**
* **U10 SN74LVC157AD** (SOIC-16, underside, below the amplifier) + **J10 2×3 "TAKEOVER"** header + R13 100k + C5. The amplifier's I²S (LRC/BCLK/DIN) and the L89 RX line come from this Teensy (S = 0, default) or from Board 3 (S = 1 after Board 3 drives TAKEOVER high). The touchdown beacon and GNSS configuration therefore survive a dead Board 1 Teensy. SOIC, not TSSOP: the TSSOP's 0.2 mm pad gaps break the board's 0.33 mm clearance rule.
* Re-routed around U10 with **no via inside any pad and no track on the inner GND/+3V3 planes**.
* Schematic sheet A3, with the cabled parts (DS3231, speaker, both antennas) drawn as "not on board" symbols.

**Board 3** (re-placed and re-routed)
* **U7 INA260** with **J6 JST VH 4-pin "5V LOOP"** right next to its IN+/IN−: 1 5V_RAW in, 2-3 GND, 4 5V_MON out. VBUS = IN− side. 1.5 mm traces on the trunk. ALERT → pin 22.
* **U8/U9 SN74CBTLV1G125** bus switches + **J8 "PWR_I2C"**: Board 1 reaches the INA260 over its Wire2 (through Board 2 J4); the switches open when this board takes over, so a hung Board 1 can never block the bus. They run from +3V3 rail B, so they stay closed when this Teensy is dead.
* **U10 7Semi UART mux** on Serial8 (TX8 35 / RX8 34), select A = pin 5, B = pin 6, INH = pin 9; **J10/J11** camera UART cables with 1k on every line.
* **J9 2×3 "TAKEOVER"** → Board 1 J10: TAKEOVER (pin 2 via 1k, 100k pull-down), GNSS commands (TX7 pin 29), I²S 20/21/7 (100 Ω series, 100k pull-downs R22-R25).
* Net classes: Default 0.3 mm, Power (+5V, +3V3, T3_3V3, GND) 0.6 mm, Trunk5V 1.5 mm.

**Board 5**
* **ICM-20948 footprint corrected** (see section 4) and re-routed.
* **J15 microSD (Hirose DM3AT-SF-PEJM5, underside)** on Teensy 4.0 SPI: CS 10, MOSI 11, MISO 12, SCK 13, card-detect 14. 10k/47k pull-ups, 10 µF + 100 nF. This is the **black-box logger** (section 6).
* **J16 3-pin "DISH_LID"**: open / closed end-stop switches to GND → pins 22 / 23 through 1k. The MG90S 360° has no position feedback.
* Whole board re-routed, 2.0 mm servo rails kept, GND pours on both sides tied by stitching vias.

**Board 4** (perfboard): U3 output now leaves on **J14 (JST VH 4)** to Board 3's INA260 and returns as +5V; **U8 DFR1015** powers only the cameras (F2/F3); battery pack + BMS drawn; AP62300 naming.

**Board 2** (perfboard): **J4 "PWR_I2C"** pass-through of the ENV bus to Board 3 J8 (wire in parallel with J1 pins 1/2/4).

## 4. Footprint verification (against your photos, the 7Semi / Waveshare / PJRC drawings and the STEP models)

| Footprint | Checked against | Result |
|---|---|---|
| Teensy 4.1 | PJRC pinout card + PJRC STEP (pins detected in 3D) | all 48 pads correct in position and function |
| Teensy 4.0 | PJRC front/back cards + STEP | 28 edge + 5 end pads correct (end column VBat / 3.3V / GND / Program / On-Off). Pad 34 "VUSB" is an unused hole — the real VUSB is an underside pad; leave it empty |
| Core1121-XF | Waveshare dimension drawing (22 × 19 mm, 2 × 8 at 2.54, rows 15.24) | correct; your pin photo is the module's underside. The `core2021-xf` STEP you saved is a different module (16.5 mm rows), so its 3D view is approximate only |
| L89HA | 7Semi 50 × 30 mm drawing | pin columns, pitch and outline correct |
| BMP585 | 7Semi top + bottom photos | pin order correct; outline enlarged to the real 20.85 × 15.8 mm |
| LSM6DSO32 | Adafruit layout + Adafruit 6-DoF STEP | correct |
| **ICM-20948** | 7Semi BO-10167 photo | **was mirrored (VIN row and CS row swapped) — fixed in the library; Boards 3 and 5 re-routed** |
| W25Q64 | 7Semi photo + 7Semi STEP | correct |
| MAX98357A | Adafruit pinout + STEP | correct; courtyard extended over the terminal block |
| UART mux | 7Semi DXF (hole centres) + pinout guide | correct |
| INA260 | 7Semi dimension photo (30 × 31 mm, 1 × 8 header) | new footprint + symbol |
| microSD | KiCad Hirose DM3AT | pads 8–9 are 0.25 mm apart → SD nets in a 0.20 mm net class, socket mask margin 0 (0.26 mm web) |

**3D models**: copied to `Board 1\VRITRA_3D.3dshapes\` and linked in the footprints with measured offsets (Teensy 4.1/4.0, Core1121, ICM-20948, W25Q64, UART mux, MAX98357A, LSM6DSO32-class Adafruit board). The SCD41, DS3231 (HW-84), XIAO and ESP32-S3 STEPs are stored there too. Your originals in Downloads are untouched. The 3D viewer now shows every module seated on its own pads on Boards 1, 3 and 5.

## 5. Cables (rev C) — checked pin by pin from the five netlists: **17 cables, 63 pins, all match**

| Cable | End A | End B | Pins |
|---|---|---|---|
| ENV digital | B1 J3 | B2 J1 | SDA2 · SCL2 · LTG_IRQ · GND |
| ENV analog | B1 J4 | B2 J2 | UV · O3 · GND |
| BACKUP | B1 J5 | B3 J1 | GNSS · WDT_RSTn · WDT_PET · (spare) · GND |
| PARA | B1 J6 | B5 J1 | GNSS · PARA_TLM · GND |
| EXPANSION (10-way) | B1 J8 | B5 J13 | 1 GND · 2 dish PWM · 6 commands; others spare |
| PARA_B | B5 J14 | B3 J5 | spin rate · commands · GND |
| **TAKEOVER (2×3 ribbon)** | **B1 J10** | **B3 J9** | 1 TAKEOVER · 2 GNSS cmd · 3 LRCLK · 4 GND · 5 BCLK · 6 DIN |
| **PWR_I2C** | **B2 J4** | **B3 J8** | SDA2 · SCL2 · GND |
| **5V LOOP (JST VH-4, 18 AWG)** | **B4 J14** | **B3 J6** | 5V_RAW · GND · GND · +5V return |
| PWR B1/B2/B3/B5 | B4 J2/J3/J4/J5 | B1 J1 / B2 J3 / B3 J3 / B5 J2 | +5V · +3V3 · GND |
| INA226 A / B | B4 J6 / J7 | B1 J2 / B3 J2 | SDA · SCL · GND |
| Servo steer / deploy | B4 J8 / J9 (XT30) | B5 TB4 / TB2 | rail · GND |
| Cameras | B4 J12/J13 (5 V) + B3 J10/J11 (UART) | XIAO 5V/GND + D7 RX / D6 TX | |
| RTC | B1 J7, B3 J4 | DS3231 modules | SDA · SCL · 3V3 · GND |

**Never fly without Board 3 (or a VH-4 bypass plug with pin 1 bridged to pin 4 on B4 J14)** — the avionics 5 V now runs through Board 3's INA260.

## 6. Data storage — "keep every byte even if both Teensy 4.1s fail"

1. Board 1: microSD on the Teensy 4.1 + W25Q64 flash (crash-survivable copy).
2. Board 3: its own Teensy 4.1 microSD.
3. **Board 5 (new): microSD J15 on the Teensy 4.0** — log Board 5's own BMP585 / ICM-20948 / GNSS / spin data **and** every telemetry line that Board 1 (Serial2, the existing command link) and Board 3 (Serial3) send it. If both Teensy 4.1s die, this card still holds everything up to the failure plus Board 5's continuing data; pull the card to read it (the slot faces the board edge, underside).
4. Optional 4th copy: the XIAO cameras' own microSD cards via the UART mux.
A flash chip cannot record by itself — something must write it; that is why the third logger sits on the third processor.

## 7. Firmware pin maps (new pins in bold)

* **Board 1 (Teensy 4.1)**: unchanged — Serial1 GNSS (RX0 via R12, TX1 → U10 → L89 RX), Serial7 RX28 / TX29, SPI 10-13 (CS 10 radio, 2 IMU, 3 flash), Wire 18/19, Wire2 24/25 (Board 2 sensors **+ INA260 at 0x40**), I²S 20/21/7 → U10 → amplifier, WDT 4, 3D-fix 33, LEDs 34/35, dish PWM 6.
* **Board 3 (Teensy 4.1)**: Serial1 RX0 GNSS / TX1 commands to B5 · Serial7 RX28 spin rate / **TX29 L89 commands** · **pin 2 TAKEOVER (keep LOW until a qualified takeover)** · **I²S1 20/21/7 beacon** · **Wire2 24/25 INA260, pin 22 ALERT** · **Serial8 34/35 camera mux, A 5, B 6, INH 9** · watchdog 3/4 · radio 10-16 · Wire 18/19.
* **Board 5 (Teensy 4.0)**: as rev B + **SD 10/11/12/13, card-detect 14 (LOW = card in)** + **lid switches 22 (open) / 23 (closed), INPUT_PULLUP**.
* GNSS: send PAIR/PQTM commands to enable GPS + GLONASS + Galileo + BeiDou + QZSS + **NavIC (L1 + L5)** and save to the L89 flash; Board 3 can resend them after a takeover.
* Dish (MG90S 360°): drive until the "open" switch closes (with a timeout), hold 20 s, drive back until "closed".

## 8. Buy list for rev C

SN74LVC157AD (SOIC-16) ×1 · SN74CBTLV1G125DBV ×2 · Hirose DM3AT-SF-PEJM5 ×1 + microSD card · 7Semi INA260 ×1 · JST VH B4P-VH header ×2 + VHR-4N housings · 2×3 2.54 mm ribbon/DuPont cable · 2 micro-switches (lid) · **a second 868 MHz antenna with u.FL for the backup radio** · 0805 resistors 100 Ω ×3, 1 kΩ ×8, 10 kΩ ×2, 47 kΩ ×3, 100 kΩ ×7 · 0805 100 nF ×5, 1206 10 µF ×1. Everything from rev B still applies (SN74LVC1G125 ×5, PTCs, bulk caps, DS3231 ×2, JST EH housings).

## 9. Still to prove on hardware
1. Print every `*-fit-check-1to1.pdf` at 100 %: rods (94 mm circle, Ø8.5), module overhangs, stack heights, USB/SD access; check Board 1's underside U10/C5/R13 and Board 5's underside microSD against the board below.
2. Module pin 1 of each breakout vs the square pad before soldering (ICM-20948: VIN row is pad 1).
3. 5 V loop: measure the drop across Board 3 at full load (< 0.1 V expected at 2 A); INA260 address 0x40 on both buses.
4. Current, pad-wait endurance, beacon SPL, takeover sequence (amp + GNSS switching), SD logging on Board 5 — as in rev B.

## 10. Files
```
cansat final pcb\
  Board 1\  Board1.kicad_* , Board1_FAB_NOTES.md, manufacturing\, VRITRA_symbols.kicad_sym,
            VRITRA_footprints.pretty\, VRITRA_3D.3dshapes\ (STEP models), Board 2 module symbol libs
  Board 2\Board 2\  Board 2.kicad_sch (perfboard)
  Board 3\  Board 3.kicad_* , manufacturing\
  Board 4\  Board 4.kicad_sch (perfboard)
  Board 5\  Board 5.kicad_* , manufacturing\
  VRITRA_PCB_HANDOVER.md (this file)
```
Rev B packages and working copies were sent to the Recycle Bin, not deleted. Generators, routers and checkers: `D:\linux\Software\Claude\VRITRA_PCB_tools\`.
