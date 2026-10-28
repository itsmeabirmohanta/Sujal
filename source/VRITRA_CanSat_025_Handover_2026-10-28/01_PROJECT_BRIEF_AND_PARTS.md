# VRITRA CanSat — Team 2026-INSPACe-CAN-7USAT-025 — Project Brief (state on 28-10-2026)

Written for a deep-research assistant. Everything here comes from the team's files and chats in this zip.
Where this file and an older document disagree, **this file and `07_memory/VRITRA_ZeroBoard_Memory/05_FINAL_BUILD_2026-10-27.md` win**
(the CDR and the older KiCad guide still show the earlier radio plan and the earlier board set).

## 1. Who, what, when
- Team VRITRA, Lovely Professional University (Punjab, India). Hardware lead: Sujal Kalbhor.
- Competition: **IN-SPACe CAN-7USAT India Student Competition 2026 (3rd edition)**. Rules: `02_competition_documents/IN-SPACe_CAN-7USAT_Mission_Requirements_and_Guidelines_IN-SPACe.CAN.MR.CG.01.pdf`.
- Launch campaign: **31-10-2026 to 03-11-2026, Kushinagar, Uttar Pradesh**. FRR was 26-10-2026. Time is short: days, not weeks.
- **Mission as flown this year (organiser change):** a **drone lifts the CanSat to about 1 km and releases it** (no rocket ejection).
  The CanSat descends under a **7-cell ram-air parafoil (about 0.40 m²)** steered by **two brake-line servos**, and must
  **land as close as possible to the target 26.717535 N, 84.301543 E**. The target must be **adjustable from the ground station**
  (wind correction). A **petri-dish lid servo opens for 10 s when altitude reaches 950 m above the boot point**; a second servo opens the parachute bay.
- Rules that still apply (see the guidelines PDF): mass < 1000 g ±50 g, fits Ø150 × 400 mm, telemetry ≥ 1 Hz ASCII CSV in a fixed
  field order, onboard SD logging, mission clock and flight state survive a processor reset, audio beacon, external power switch with indicator,
  no LiPo, survive 15 g / 30 g shock, only transmitted telemetry is graded, ground station must plot every field live and write
  `Flight_2026-INSPACe-CAN-7USAT-025.csv`.
- Mechanical gyroscopic stabilisation wheel (BLDC + ESC + Hall sensor) gives the mandatory spin-rate field.

## 2. Radio assignment (official, `CAN7USAT Team_Frequency_Assignments_19_10.pdf`, row for team 025)
| Item | Value |
|---|---|
| LoRa main / backup | **866.50 MHz / 865.25 MHz** |
| Spreading factor | **SF9** |
| Bandwidth / coding rate | 125 kHz / 4/5 |
| Sync word | **0x99** |
| TX power | +10 dBm on pad, about +14 dBm in flight |
| XBee (not used) | Ch19 / Ch14, PAN 0x1019 |
Reserved and forbidden: 433, 446, 915 MHz, 2405 MHz. Telecommand ON/OFF per link. Auto TX-off 90 min after launch detect.
The camera's Wi-Fi/Bluetooth must stay off.

## 3. Flight electronics that fly (four boards)
Board files (pin truth): `03_kicad_current_boards/`. Holes, wires and nets are in each board's `*_BUILD_LIST.md`.

### Board 1 — MAIN flight computer (primary)
| Part | Maker (full name) | Bus / address / pins on Teensy 4.1 | Link |
|---|---|---|---|
| Teensy 4.1 (NXP i.MX RT1062, 600 MHz Cortex-M7) | PJRC (Paul Stoffregen) | — | https://www.pjrc.com/store/teensy41.html |
| **Quectel L89 HA** GNSS (chip **Airoha AG3335M**, firmware reported `AG3335M_V2.5.0.AG3335_20221130`), on **7Semi L89HA breakout** | Quectel Wireless Solutions (module), Airoha Technology (chip), 7Semi (breakout, India) | **Serial2: Teensy RX 7 ← L89 TX, Teensy TX 8 → L89 RX, RESET_N pin 6**, default 9600 baud. The same L89 TX line is also copied through 1 kΩ to Board 2 (pin 0) and Board 4 (GPIO14), so three MCUs hear the same NMEA; **only Board 1 can send commands to the L89**. Breakout pins: 3D_FIX, WAKE, SDA, SCL, GEO, JAM, RST, BAT, TX, RX, 3V3, GND — check the KiCad file for which are wired. | Quectel: https://www.quectel.com/product/gnss-l89/ · 7Semi: https://7semi.com/7semi-l89ha-breakout-multi-gnss-engines-for-gps-irnss-glonass-beidou-galileo-and-qzss/ · Quectel forum thread: https://forums.quectel.com/t/problem-with-pstm-commands-for-l89ha-gnss-module/48222 · 7Semi Arduino library copy: `05_code/7Semi-L89HA-GNSS-Module-Arduino-Library-main/` |
| **ICM-20948** 9-axis IMU (accel + gyro + AK09916 magnetometer) on **7Semi breakout** | TDK InvenSense (chip), Asahi Kasei Microdevices (AK09916 inside), 7Semi | I²C Wire (SDA 18 / SCL 19), **0x69** (SDO/CS tied to the module's 1V8 pin) | https://invensense.tdk.com/products/motion-tracking/9-axis/icm-20948/ · https://7semi.com/icm-20948-9dof-imu-breakout-mpu-9250-upgrade/ |
| **BMP585** barometer on **7Semi breakout** | Bosch Sensortec (chip), 7Semi | I²C Wire, **0x47** (SDO → 3V3) | https://www.bosch-sensortec.com/products/environmental-sensors/pressure-sensors/bmp585/ · datasheet https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp585-ds003.pdf · Bosch API https://github.com/boschsensortec/BMP5_SensorAPI · 7Semi listing https://evelta.com/7semi-bmp585-pressure-sensor-breakout |
| **DS3231** RTC on ZS-042 module (+ AT24C32 EEPROM) | Analog Devices (formerly Maxim Integrated) | I²C Wire, 0x68 (EEPROM 0x57) | https://www.analog.com/en/products/ds3231.html |
| **Core1121-HF/XF** LoRa module (**Semtech LR1121**), whip antenna on ANT2 | Waveshare (module), Semtech (chip) | SPI1 MOSI 26 / SCK 27 / **MISO 39 (call `SPI1.setMISO(39)` before begin)**, CS 29, BUSY 24, RST 25, IRQ (DIO9) 30, TCXO 1.6 V, RF switch on DIO5/DIO6 | https://www.waveshare.com/wiki/Core1121-XF · https://www.semtech.com/products/wireless-rf/lora-connect/lr1121 · RadioLib https://github.com/jgromes/RadioLib |
| **W25Q64** 64 Mbit SPI NOR flash (black box) on 7Semi breakout | Winbond Electronics | SPI1, CS 9 (10 k pull-up) | https://www.winbond.com/hq/product/code-storage-flash-memory/serial-nor-flash/ |
| **Grove Buzzer v1.3** (active, about 2.3 kHz, ≥ 85 dB) — replaced the burnt MAX98357A amplifier on 28-10 | Seeed Studio | **pin 2 HIGH = sound** (digitalWrite, no I2S); VCC +5 V | https://wiki.seeedstudio.com/Grove-Buzzer/ |
| **XIAO ESP32S3 Sense** camera (own microSD), Wi-Fi/BT off | Seeed Studio (Espressif ESP32-S3) | Teensy Serial1 TX 1 / RX 0 at 115200, commands SNAP / REC / STAT / PING | https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/ |
| Battery voltage divider 100 k / 22 k | — | analog pin 20 | — |
| LEDs green / red | — | pins 31 / 32 | — |
| Heartbeat to Board 2 | — | pin 5 → Board 2 pin 4 | — |
| Link to Board 4 | — | **Serial3 TX 14 → Board 4 GPIO19**, Serial3 RX 15 ← Board 4 GPIO54 | — |
| Power monitor (on Board 3) **INA260** | Texas Instruments (chip), 7Semi (breakout) | I²C Wire via J2, 0x40 | https://www.ti.com/product/INA260 |

### Board 2 — BACKUP + SENSOR (Teensy 4.1)
| Part | Maker | Bus / pins | Link |
|---|---|---|---|
| Teensy 4.1 | PJRC | — | https://www.pjrc.com/store/teensy41.html |
| BMP585 (7Semi) | Bosch Sensortec | I²C Wire 0x47 | as above |
| **HDC3020** temperature + humidity (7Semi; firmware also auto-detects SHT4x) | Texas Instruments | I²C 0x44 | https://www.ti.com/product/HDC3020 |
| LSM6DSO32 (fitted, **user reports it misbehaves → not used**) | STMicroelectronics | I²C 0x6A | see Board 4 |
| microSD module (5 V, 74xx125 buffer) | generic | SPI CS 10, MOSI 11, MISO 12, SCK 13 | — |
| GNSS copy (listen only) | — | Serial1 RX pin 0 | — |
| Heartbeat in | — | pin 4 | — |
| Link to Board 4 | — | Serial3 TX 14 → Board 4 GPIO16; RX 15 ← Board 4 GPIO54 | — |
| LEDs | — | pins 36 / 37 | — |

### Board 4 — PARACHUTE / GUIDANCE (rebuilt 27-10 on a zero board after the Teensy 4.0 board shorted)
| Part | Maker | Pin (GPIO / Pico-header pin) | Link |
|---|---|---|---|
| **ESP32-P4-Pico** (Espressif **ESP32-P4**, dual-core RISC-V 400 MHz, no radio on this board) | Waveshare (board), Espressif Systems (chip) | +5 V into **VSYS (pin 39)**, GND pins 3 + 38 | https://www.waveshare.com/wiki/ESP32-P4-Pico · Espressif ESP32-P4: https://www.espressif.com/en/products/socs/esp32-p4 · Waveshare examples https://github.com/waveshareteam/esp32-p4-platform · Arduino core https://github.com/espressif/arduino-esp32 (core 3.3.x, board "ESP32P4 Dev Module") |
| **Adafruit LSM6DSO32** 6-DoF IMU (±4…±32 g, ±125…±2000 dps), product 4692 | Adafruit Industries (board), STMicroelectronics (chip) | I²C SDA GPIO7 / SCL GPIO8, **0x6A** | https://www.adafruit.com/product/4692 · ST: https://www.st.com/en/mems-and-sensors/lsm6dso32.html · Adafruit guide https://learn.adafruit.com/lsm6dsox-and-ism330dhc-6-dof-imu · Adafruit lib https://github.com/adafruit/Adafruit_LSM6DS · ST driver https://github.com/STMicroelectronics/lsm6dso32-pid |
| BMP585 (7Semi) | Bosch Sensortec | same I²C, **0x47** | as above |
| ES8311 audio codec already on the P4-Pico (not used, shares I²C at 0x18) | Everest Semiconductor | GPIO7/8 | — |
| **2 × AGFRC B26CLM** coreless digital HV servos (steering brake lines L / R): 4.8–8.4 V, stall 7.0/8.5/9.5/10.5 kg·cm at 4.8/6.0/7.4/8.4 V, 333 Hz capable, centre 1520 µs, titanium gears, 27.3 g, programmable | AGFRC | STEER_L **GPIO20** (pin 35), STEER_R **GPIO21** (pin 34), each through 330 Ω | https://www.agf-rc.com/agfrc-coreless-mini-servo-b26clm-p1669135.html |
| 2 × MG90S metal-gear micro servo (parachute bay, petri lid), 4.8–6 V | Tower Pro | BAY **GPIO22** (32), PETRI **GPIO23** (31) | https://www.towerpro.com.tw/product/mg90s-3/ |
| Spare servo output | — | SERVO_X GPIO32 (26) | — |
| 40 A ESC + EMAX 2205 2300 KV BLDC (gyro wheel, Ø70 mm, target 10 000 RPM) | EMAX (motor); ESC model unconfirmed | ESC **GPIO33** (25), 1000 µs = stop | https://emaxmodel.com/ |
| Hall sensor + magnet (wheel RPM; model unconfirmed, A3144-type) | e.g. Allegro MicroSystems | HALL **GPIO5** (11), 10 k pull-up | — |
| GNSS copy (listen only) | — | **GPIO14** (Serial1 RX) | — |
| Link from Board 1 / Board 2, link out | — | RX GPIO19 (from MAIN), RX GPIO16 (from BACKUP), TX **GPIO54** → both (1 kΩ each) | — |
Avoided pins: GPIO24–27 (USB), GPIO34–38 (strapping), GPIO39–48 (microSD LDO domain), GPIO2 (pull-up after reset).
Test sketch: `05_code/vritra-gcs-master/firmware/board_tests/board4_parachute_test/` (LEDC 50 Hz 14-bit servo pulses, Serial1/2/3, Wire 7/8, Hall ISR).

### Board 3 — POWER (zero board)
| Part | Maker | Notes | Link |
|---|---|---|---|
| 3S pack of **Molicel P50B** 21700 Li-ion (12.6 V full, 5 Ah) | Molicel (E-One Moli Energy) | no LiPo allowed | https://www.molicel.com/ |
| 2 × **AP64500** 5 V 5 A buck (7Semi) | Diodes Incorporated | +5V_LOGIC, +5V_SERVO | https://www.diodes.com/part/view/AP64500 |
| 2 × **AP62301** 3.3 V buck (7Semi) | Diodes Incorporated | +3V3 rails | https://www.diodes.com/part/view/AP62301 |
| WeAct adjustable buck | WeAct Studio | for the steering rail; it was set to 9 V, which is **above the B26CLM 8.4 V maximum** → set 7.4–8.0 V | — |
| INA260 (one owned, battery position) | Texas Instruments | 0x40 | https://www.ti.com/product/INA260 |
| External switch KCD1 rocker + LED, XT30/XT60 | — | — | — |

### Ground station (hardware unchanged)
| Part | Maker | Notes | Link |
|---|---|---|---|
| ESP32 Dev Module + Waveshare Core1121 (LR1121) gateway, firmware `vritra_gs` 6.3.0 | Espressif / Waveshare / Semtech | VSPI CS 5, SCK 18, MOSI 23, MISO 19, RST 14, BUSY 27, IRQ 26 | — |
| RadioMaster Bandit 868/915 T-antenna (2.7 dBi) | RadioMaster | | https://radiomasterrc.com/products/bandit-t-antenna |
| GCS GUI `gcs/vritra_gcs.py` (Python, PyQt5) | team | public repo https://github.com/itsmeabirmohanta/vritra-gcs | |

### Spare (owned, not flying)
u-blox **NEO-M9N** GNSS (https://www.u-blox.com/en/product/neo-m9n-module), active NavIC/GPS SMA antenna, second ICM-20948.

## 4. Data paths today (what is wired)
```
                  L89 HA (GNSS) ──TX──┬──> Board 1 Teensy Serial2 RX7  (Board 1 also TX8 -> L89 RX: the only commander)
                                      ├─1k─> Board 2 Teensy Serial1 RX0 (listen only)
                                      └─1k─> Board 4 ESP32-P4 GPIO14    (listen only)
Board 1 I2C: BMP585 0x47, ICM-20948 0x69, DS3231 0x68 (+EEPROM 0x57), INA260 0x40 (on POWER board)
Board 1 SPI1: LR1121 radio (866.50/865.25 MHz SF9), W25Q64 flash
Board 1 Serial1 <-> XIAO ESP32-S3 camera          Board 1 pin 5 heartbeat -> Board 2 pin 4
Board 1 Serial3 TX14 ----------------------> Board 4 GPIO19
Board 2 Serial3 TX14 ----------------------> Board 4 GPIO16
Board 4 GPIO54 TX ---1k--> Board 1 RX15  and ---1k--> Board 2 RX15
Board 4 I2C: LSM6DSO32 0x6A, BMP585 0x47 ; Board 4 outputs: 2 steering servos, bay, petri, ESC ; Hall in
Ground: ESP32+LR1121 gateway <-USB-> PC GCS (PyQt5)  ; commands CMD_xxx -> uplink "<TEAM_ID>,C,<id>,<arg>,<seq>"
```

## 5. Software that exists (all in `05_code/`)
- `vritra-gcs-master/firmware/vritra_fc/` — flight software 6.1.0 for the **older** single-board design (Teensy 4.1, modular, no `delay()`,
  direct-register drivers, base-36 radio packet, SD binary + CSV, EEPROM state, FSM). **Not yet ported** to the four boards above:
  it still assumes GNSS auto-detect on Serial1, a TB6612 wheel driver, no INA260, no Board-4 link, I2S beacon.
- `.../firmware/board_tests/` — bench tests for Board 1, 2, 4 (compile clean, **not yet run on hardware** at time of writing).
- `.../firmware/vritra_gs/` — ground gateway 6.3.0 (new radio plan). `.../firmware/vritra_cam/` — camera.
- `.../firmware/cdr_tests/` — August single-function tests (GNSS, IMU, baro, 600 m trigger, RF…).
- `.../gcs/vritra_gcs.py` — ground station GUI. `.../docs/` — handoff, compliance audit, IMU calibration notes, connector list.
- `Cansat_Ground_station_v2_history/` — the August working copy (older flight/gateway pairs, ANSYS RF study results).

## 6. Lessons already learned the hard way (do not repeat)
1. The L89 HA is an **Airoha AG3335M**: it speaks `$PAIR` (plus a `$PMTK` subset and `$PQTM`), **not** ST `$PSTM`.
   `$PMTK353` has **no NavIC field** (it enabled BeiDou by mistake). Constellations are set with **`$PAIR066,<GPS>,<GLONASS>,<Galileo>,<BeiDou>,<QZSS>,<NavIC>`**.
   NavIC civil signal is on **L5**: it needs an L1+L5 antenna path.
2. Teensy's default 64-byte UART RX buffer drops NMEA at 9600 baud whenever `loop()` stalls → use `addMemoryForRead()` (1 KB+).
3. ICM-20948 can wedge (ACKs address, returns 0xFF): send `PWR_MGMT_1 = 0x80` blind before probing — **at 0x69 only** (0x68 is the DS3231; writing there corrupts the year).
4. Teensy 4.1 SPI1 MISO defaults to pin 1 → `SPI1.setMISO(39)` before `SPI1.begin()` (LR1121 error -707). Also re-apply the LR1121 RF-switch table after `begin()`.
5. 9 V on the B26CLM steering servos is over its 8.4 V maximum.
6. Calibration: the team will calibrate GNSS, INA260 and BMP585 against its own reference; do not trust datasheet constants blindly.
