# 02 — HARDWARE INVENTORY (owned parts + verified electrical facts)

Status key: **V** = verified from datasheet/vendor page/KiCad netlist · **U** = unverified, must be checked on the real module · **Q** = quantity or identity unconfirmed (ask user).
The user's list on 2026-10-22 was the CDR MASS table: the number after each name is MASS in grams, not quantity, unless "(Quantity-n)" is written.

| # | Part | Qty | Role | Key facts / hazards | Status |
|---|---|---|---|---|---|
| 1 | Teensy 4.1 | 2 | Primary + backup FC | 3.3 V I/O, **NOT 5 V tolerant**. VIN 3.6–5.5 V. 3V3 out ≈250 mA spare. Built-in SDIO microSD. Never wire On/Off pin (held LOW 5 s = regulator OFF) or Program pin. Cut VIN–VUSB pad before USB + external 5 V together. No reset pin. | V (PJRC) |
| 2 | Teensy 4.0 | 1 | Parafoil / recovery MCU | Same rules. Pins 24–33 are BOTTOM SMD pads (avoid on perfboard); use 0–23 only. | V (PJRC) |
| 3 | 7Semi BMP585 | 2 | Baro (B-primary, B-backup) | 8-pin hdr VIN,3V3,GND,SCK,SDO,SDI,CS,INT. I²C: CS→3V3, SDO→GND = **0x46** (SDO high = 0x47). Only 2 owned → Teensy 4.0 has NO baro (see Q). | V (KiCad netlist) / Q qty |
| 4 | Adafruit LSM6DSO32 | 1 | ±32 g IMU (field 13) on primary | VIN 3–5 V (on-board reg + level shift). SPI: SCL=SCK, SDA=MOSI, DO=MISO, CS. I²C 0x6A/0x6B if used. | V |
| 5 | 7Semi ICM-20948 | 2 | 9-DOF (backup + parafoil) | 12-pin hdr VIN,1V8,GND,SCL,SDA,INT,CS,SDO,GND,ACL,ADA,FSYNC. Chip VDDIO = 1.8 V: **CS and SDO/AD0 go to the module's 1V8 pin, NEVER 3V3**. SDO high = **0x69** (0x68 clashes with DS3231). FSYNC→GND. Pin order vs silkscreen: U. | V/U |
| 6 | 7Semi HDC3020 | 1 | Temp/RH (optional field 16) | JST-PH 4-pin cable module, silk `+ - D C`. I²C 0x44 default. 3.3 V. | V (earlier photo) |
| 7 | 7Semi W25Q64 | 1 | SPI flash black box | 6-pin 3V3,CS,MISO,SCK,MOSI,GND. 3.3 V only. Needs CS pull-up 10k. | V (KiCad) |
| 8 | **7Semi INA260** (replaces INA226) | 3 | #1 battery/main bus, #2 +8V_STEER, #3 +5V_LOGIC | Integrated 2 mΩ shunt, ±15 A, bus 0–36 V, 2.7–5.5 V supply, I²C 0x40–0x4F via A0/A1. 7Semi pad layout unknown (U) — scan. 30.5 × 31.5 mm courtyard. | V chip / U board |
| 9 | microSD card module | 1 | Logger for Teensy 4.0 (board D) | Photo: pins CS, SCK, MOSI, MISO, VCC, GND; 74xx125 buffer + regulator → VCC **5 V**; MISO not tri-stated → **alone on its SPI bus**. ~42 × 24 mm. | V (photo) |
| 10 | DS3231 "MH" ZS-042 | 1 | RTC on board A | Photo: 32K, SQW, SCL, SDA, VCC, GND (+4-pin pass-through), AT24C32 (A0–A2 pads) 0x57, DS3231 0x68, CR2032 fitted → **remove the charging resistor** next to the diode. VCC = 3.3 V. ~38 × 22 mm. | V (photo) |
| 11 | XIAO ESP32-S3 Sense | 1 | Camera, own microSD | 5 V pin in. UART D6=GPIO43 TX, D7=GPIO44 RX (boot ROM log appears on D6 at boot). Sense board SD uses GPIO21 + D8/D9/D10 → **do not use D8–D10**. GPIO3 (D2) is a strapping pin → leave free. **Wi-Fi/BT must be disabled in firmware.** | V (Seeed wiki) |
| 12 | Waveshare Core1121 (LR1121) | 2 (1 CanSat on A, 1 ground gateway) | Telemetry | 19 × 22 mm, 2.54 castellated, 3.3 V (max 3.8), 22 dBm max sub-GHz, TX 115 mA @22 dBm, RX 9.2 mA. Pins (photo): top LORA_ANT GND DIO9 DIO8 DIO7 NC GND 3.3V; bottom 2.4G_ANT GND CS CLK MOSI MISO RESET BUSY. **Must be the HF variant (850–930 MHz)** — confirm print. Firmware: TCXO 1.6 V, RF switch DIO5/6 (proven). | V pins / U variant |
| 13 | RadioMaster Bandit 868/915 T-antenna | 2 (+2 LoRa whips) | Onboard + GS | 860–930 MHz, 2.7 dBi, RP-SMA, 14 g. | V (CDR) |
| 14 | Quectel L89HA (7Semi) | 1 (board A) | GNSS w/ NavIC L1/L5 | 12-pin: 3D_FIX,WAKE,SDA,SCL,GEO,JAM,RST,BAT,TX,RX,3V3,GND. Module 26.4 × 18.4 × 6.8, 7Semi board ~50 × 30. **Built-in patch + chip antennas**; external antenna needs external bias. Default **9600 baud** (datasheet; firmware matches). Chip AG3335M → PAIR/PMTK commands. | V (datasheet) |
| 15 | u-blox NEO-M9N module | 1 | spare unless it gets the NavIC antenna | No antenna of its own. Default UART 38400. | U |
| 16 | AGFRC **A20CLM** | 2 | Parafoil brake lines | 4.8–8.4 V. Torque 6.8 kg·cm @4.8 V, 7.5 @6 V, 8.8 @7.4 V, 10.2 @8.4 V. 0.093 s/60° @8.4 V. 23×12×27.5 mm, 21 g, 1520 µs centre, ≤333 Hz, coreless, potentiometer, 180°. **Stall current NOT published → measure.** (CDR text says A20BHM brushless — the owned part is A20CLM.) | V (agf-rc.com) |
| 17 | MG90S | 2 | Drogue release + petri lid | 4.8–6 V, 1.8–2.2 kg·cm, stall ≈ 0.7 A each. 50 Hz, ~500–2400 µs. | V (generic) |
| 18 | EMAX 2205 2300 KV BLDC | 1 | Gyro wheel | 3–4S, up to ~32 A. | V (CDR) |
| 19 | 40 A ESC | 1 | Wheel driver | Powered from battery (XT30). Signal 3.3 V OK for most ESCs. If it has a BEC, **do not connect the BEC red wire** to any rail. Model U (CDR says Hobbywing XRotor 40A / 7Semi bidirectional). | U |
| 20 | AP64500 5 V 5 A buck (7Semi) | 2 | +5V_LOGIC, +5V_SERVO | 7–40 V in, FIXED 5 V (user). ~21 × 18. | V (user) |
| 21 | AP62301 3.3 V 2 A buck (7Semi) | 2 | +3V3_A, +3V3_B | Fed from +5V_LOGIC (input-range margin). ~18 × 16. | V (user) |
| 22 | 8 V multi-output buck | 1 | +8V_STEER (A20CLM only) | Model and size TBD (DFR1015?). 1.8 V regulator dropped (not needed). | Q |
| 23 | XT30 connectors | 5 (CDR) | Battery / ESC / servo power | 30 A class. | V |
| 24 | Capacitors 470 µF + 0.1 µF, resistors 0.1 Ω, 4.7 k, 330 Ω, 10 k, diodes | set | Filtering, pull-ups, series protection | Need also 1 k (inter-board series) — check stock. | Q |
| 25 | Molicel P50B 21700 | 3 (6 bought) | 3S pack | 12.6 V full, 10.8 V nominal, 5 Ah. | V |

| 26 | KCD1-5-103W rocker, ON-OFF-ON, 3-pin, waterproof | 4 | External power switch | Current rating at 12 V DC not known — the whole battery current flows through it (U). | U |
| 27 | XT60PW-F (PCB right-angle female) | 5 | Battery input on E | | V |
| 28 | LEDs red / green, Hall sensor | some | Indicator, spin rate | Hall model U (A3144?). | U |
| 29 | NavIC/GPS active antenna, SMA, 3–5 V | 1 | spare / M9N | | V (photo) |

## NOT in the owned list but required by rules or by the design (ask user)
- Audio beacon ≥ 92 dB (CDR: SM400204-1 + MAX98357A, or a buzzer + transistor) — **mandatory**.
- Hall sensor (A3144 or similar) + magnet for gyro-wheel RPM — **mandatory field 14**.
- External power switch + indicator LED — **mandatory**.
- 3S BMS 40 A (CDR) — strongly recommended.
- Active GNSS antenna(s) for L89HA (and M9N if it has no patch).
- Third BMP585 for the Teensy 4.0 (design rule: recovery decides 600 m from its OWN baro).
- Env sensors (SCD41, SEN0290, BMV080, AS7343, ML8511, MQ-131) — user will add later; I²C/analog spare pins are reserved in 03.
