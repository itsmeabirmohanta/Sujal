# VRITRA — every connector and cable (as-built boards, 2026-10-27)

Read from the board files you actually built:
Board 1 MAIN = `kicad_single/BOARD1_MAIN_1L`, Board 2 BACKUP = `kicad_single/BOARD2_BACKUP_SENSOR_1L`,
Board 3 POWER = `kicad_single/BOARD3_POWER_1L`, Board 4 PARACHUTE = `kicad_single/BOARD4_PARACHUTE_PICO`
(zero board, Waveshare ESP32-P4-Pico since 2026-10-28; same plug names J1..J16 and the same pin order on every plug).
Pictures with every connector marked: `docs/connectors/BOARD1_MAIN_connectors.png` … `BOARD4_PARACHUTE_connectors.png`.

**Every board-to-board cable is straight: pin 1 → pin 1, pin 2 → pin 2 …** (pin 1 = square pad).
Board-to-board plugs are JST-XH 2.50 mm. Servo plugs on Board 4 are plain 2.54 mm pin headers.

## 1. Board-to-board cables (11)

| # | From | To | Pins (1 → n) | Wire |
|---|---|---|---|---|
| C1 | POWER J5 | MAIN J1 | 5V, GND, 3V3, GND | JST-XH 4, 22 AWG |
| C2 | POWER J6 | BACKUP J1 | 5V, GND, 3V3, GND | JST-XH 4, 22 AWG |
| C3 | POWER J7 | PARACHUTE J8 | 5V, GND, 3V3, GND | JST-XH 4, 22 AWG |
| C4 | POWER J8 | PARACHUTE J1 | STEER+, STEER+, GND, GND | JST-XH 4, 20–22 AWG (2 wires per side share the servo current). Steering rail 5.0–8.4 V (B26CLM max 8.4 V — never 9 V) |
| C5 | POWER J9 | PARACHUTE J6 | 5V servo, GND | JST-XH 2, 20–22 AWG |
| C6 | POWER J2 | MAIN J2 | SCL, SDA, GND | JST-XH 3 (INA260 I²C) |
| C7 | POWER J10 | MAIN J3 | VBAT, GND | JST-XH 2 (battery voltage sense) |
| C8 | MAIN J4 | PARACHUTE J2 | MAIN RX3 ↔ PARA TX2, MAIN TX3 ↔ PARA RX2, GND | JST-XH 3 |
| C9 | MAIN J8 | PARACHUTE J5 | GNSS data, GND | JST-XH 2 |
| C10 | MAIN J5 | BACKUP J2 | heartbeat, GNSS data, camera TX, camera RX, GND | JST-XH 5 |
| C11 | BACKUP J3 | PARACHUTE J3 | BACKUP RX3 ↔ PARA TX2, BACKUP TX3 ↔ PARA RX3, GND | JST-XH 3 |

The TX↔RX crossing is already done in the copper, so the cables themselves are straight.

## 2. Everything that plugs in from outside the boards

| Part | Goes to | Pins |
|---|---|---|
| 3S Molicel battery (XT60) | POWER J1 | 1 = BAT+, 2 = GND |
| Rocker switch on the shell | POWER J3 | 1 = battery +, 2 = switched + (the switch sits in the + line) |
| ESC thick power (XT30) | POWER J4 | 1 = VBAT, 2 = GND |
| ESC thin lead (signal + GND) | PARACHUTE J14 | 1 = GND, 2 = signal. Do NOT connect a BEC red wire anywhere |
| BLDC motor | ESC | 3 motor wires (direction = BLHeli setting) |
| AGFRC B26CLM steering LEFT | PARACHUTE J10 | 1 GND (brown/black), 2 + steering rail (red), 3 signal · centre 1520 µs |
| AGFRC B26CLM steering RIGHT | PARACHUTE J11 | same |
| MG90S parachute bay | PARACHUTE J12 | 1 GND, 2 +5 V, 3 signal |
| MG90S petri dish | PARACHUTE J13 | same |
| MG90S extra (optional) | PARACHUTE J16 | same |
| Hall sensor (wheel RPM) | PARACHUTE J15 | 1 GND, 2 +5 V, 3 OUT |
| LoRa antenna | MAIN, Core1121 **LORA_ANT u.FL (ANT2)** | never ANT1, never transmit without it |
| Speaker | MAIN, MAX98357A screw terminal | + / − |
| GNSS backup coin cell (optional) | MAIN J7 | 1 = cell +, 2 = GND |
| HDC3020 temp/humidity cable | BACKUP M1L holes | + → 1, − → 2, D → 3, C → 4 |
| XIAO ESP32S3 camera — power | BACKUP J4 | pin 1 = 5 V → XIAO 5V, pin 2 = GND → XIAO GND |
| XIAO ESP32S3 camera — data | BACKUP U10 mux holes | hole 2 → XIAO **D7** (RX), hole 3 ← XIAO **D6** (TX) — soldered wires |
| Buck modules | POWER M1…M5 landing holes | 1 = IN, 2 = GND, 3 = OUT (M1 5 V logic, M2 5 V servo, M3 8 V, M4 3.3 V A, M5 3.3 V B) |
| INA260 (only one owned) | POWER "INA260 #1 BATTERY" | 1 VCC 2 GND 3 SCL 4 SDA 5 ALERT 6 VBUS 7 IN+ 8 IN− (check the silk first) |

## 3. Leave these EMPTY
- MAIN **J6**, **J9** (the sensor board was removed).
- BACKUP **J4 pins 3/4** and the whole **U10B** row (no mux; the camera uses the 2 soldered wires).
- POWER INA260 #2 and #3 positions get **no module** — instead bridge **pad 7 → pad 8 with thick wire** on each, or the 8 V / 5 V logic rails get no power.
- POWER **F1**: a 10 A fuse, or a thick wire link until you have one.

## 4. Plugs to make
| Housing | How many cables | Housings |
|---|---|---|
| JST-XH 2-pin | C5, C7, C9 + ESC lead + coin cell (+ one each end of the cables) | 8 |
| JST-XH 3-pin | C6, C8, C11 + Hall sensor | 7 |
| JST-XH 4-pin | C1, C2, C3, C4 + camera power | 9 |
| JST-XH 5-pin | C10 | 2 |

Before the first power-up: meter every cable pin 1 to pin 1, check no 5 V / 8 V / VBAT pin beeps to any signal pin,
and power the system from a current-limited bench supply (~1 A) with servos and ESC unplugged.
