# BOARD1_MAIN — etched single-layer PCB, build notes

Board 118 × 150 mm, copper on the BOTTOM only, parts on the top (through-hole).
Tracks never cross: where two connections must cross, one goes over the top as a WIRE JUMPER.
Copper rules: pads Ø1.8 mm, tracks 0.7 mm (servo power 1.8 mm), every gap ≥ 0.5 mm, GND copper fill.

## Wire jumpers (insulated wire on the component side, fit them FIRST — some run under modules)
| jumper | net | from (x, y mm) | to (x, y mm) | length |
|---|---|---|---|---|
| W1 | GNSS_VBCKP | (24.7, 143.6) | (9.5, 143.6) | 15.2 mm |
| W2 | LORA_IRQ | (32.3, 47.1) | (50.1, 47.1) | 17.8 mm |
| W3 | CAM_TX | (40.0, 97.9) | (40.0, 80.1) | 17.8 mm |
| W4 | LORA_RST | (19.6, 47.1) | (37.4, 47.1) | 17.8 mm |
| W5 | LORA_BUSY | (17.1, 29.3) | (17.1, 54.7) | 25.4 mm |
| W6 | MISO1 | (73.0, 57.2) | (73.0, 26.7) | 30.5 mm |
| W7 | MISO1 | (73.0, 19.1) | (45.0, 19.1) | 27.9 mm |
| W8 | MISO1 | (42.5, 19.1) | (19.6, 19.1) | 22.9 mm |
| W9 | GNSS_RX | (45.0, 146.1) | (45.0, 115.6) | 30.5 mm |
| W10 | GNSS_RX | (45.0, 110.6) | (45.0, 80.1) | 30.5 mm |
| W11 | GNSS_TX | (50.1, 90.2) | (50.1, 82.6) | 7.6 mm |
| W12 | GNSS_RST | (45.0, 108.0) | (45.0, 85.2) | 22.9 mm |
| W13 | BUS_V | (85.7, 130.9) | (85.7, 115.6) | 15.2 mm |
| W14 | BUS_V | (73.0, 115.6) | (73.0, 87.7) | 27.9 mm |
| W15 | GNSS_D | (78.1, 105.5) | (108.5, 105.5) | 30.5 mm |
| W16 | CAM_RX | (37.4, 62.3) | (37.4, 92.8) | 30.5 mm |
| W17 | SDA | (98.4, 95.3) | (98.4, 125.8) | 30.5 mm |
| W18 | +5V_LOGIC | (75.5, 100.4) | (75.5, 69.9) | 30.5 mm |
| W19 | +3V3_A | (47.6, 136.0) | (42.5, 136.0) | 5.1 mm |
| W20 | +3V3_A | (24.7, 136.0) | (14.6, 136.0) | 10.2 mm |
| W21 | +3V3_A | (62.8, 143.6) | (83.1, 143.6) | 20.3 mm |
| W22 | +3V3_A | (88.2, 136.0) | (88.2, 105.5) | 30.5 mm |
| W23 | +3V3_A | (88.2, 95.3) | (78.1, 95.3) | 10.2 mm |
| W24 | +3V3_A | (42.5, 133.4) | (42.5, 102.9) | 30.5 mm |
| W25 | +3V3_A | (45.0, 102.9) | (45.0, 72.5) | 30.5 mm |
| W26 | +3V3_A | (45.0, 67.4) | (45.0, 36.9) | 30.5 mm |
| W27 | +3V3_A | (47.6, 54.7) | (47.6, 24.2) | 30.5 mm |
| W28 | +3V3_A | (42.5, 34.4) | (24.7, 34.4) | 17.8 mm |
| W29 | GND | (70.4, 100.4) | (70.4, 120.7) | 20.3 mm |
| W30 | GND | (73.0, 123.3) | (73.0, 138.5) | 15.2 mm |
| W31 | GND | (47.6, 141.0) | (40.0, 141.0) | 7.6 mm |
| W32 | GND | (111.1, 130.9) | (111.1, 113.1) | 17.8 mm |
| W33 | GND | (108.5, 113.1) | (108.5, 100.4) | 12.7 mm |
| W34 | GND | (106.0, 77.5) | (106.0, 64.8) | 12.7 mm |
| W35 | GND | (93.3, 16.6) | (78.1, 16.6) | 15.2 mm |
| W36 | GND | (37.4, 3.9) | (37.4, 26.7) | 22.9 mm |
| W37 | GND | (34.9, 31.8) | (34.9, 42.0) | 10.2 mm |
| W38 | GND | (34.9, 24.2) | (22.2, 24.2) | 12.7 mm |
| W39 | GND | (50.1, 102.9) | (19.6, 102.9) | 30.5 mm |

x from the LEFT edge, y from the BOTTOM edge, looking at the component side.

## Parts
| ref | part | notes |
|---|---|---|
| U1 | Teensy 4.1 MAIN | female headers · USB up · microSD down |
| KO1 | USB plug keep clear |  |
| KO2 | microSD pull-out |  |
| U2 | Core1121-HF | label side DOWN · u.FL → antenna |
| U3 | L89HA GNSS | 7Semi board · antennas on it · 9600 Bd |
| U4 | DS3231 (flat on 4-pin end) | REMOVE charge resistor · 0x68 |
| U5 | BMP585 #1 | 0x47 (SDO→3V3) |
| U6 | ICM-20948 #1 | CS+SDO → 1V8 · 0x69 |
| U7 | W25Q64 flash |  |
| U9 | MAX98357A amp | speaker on its screw terminal |
| J8 | GNSS to PARACHUTE J5 |  |
| J4 | UART to PARACHUTE J2 |  |
| J5 | LINK to BACKUP J2 |  |
| J2 | I²C to POWER J2 |  |
| J3 | VBAT from POWER J10 |  |
| J1 | PWR from POWER J5 |  |
| J7 | GNSS backup cell |  |
| R1 | 10k LORA_CS↑ |  |
| R2 | 10k FLASH_CS↑ |  |
| R3 | 1k HB |  |
| R4 | 1k TX3 |  |
| R5 | 1k GNSS→D |  |
| R6 | 1k GNSS→B |  |
| R7 | 100k VBAT |  |
| R8 | 22k VBAT |  |
| R9 | 470 LED OK |  |
| R10 | 470 LED FLT |  |
| R11 | 1k CAM TX |  |
| R12 | 100k GAIN 15dB |  |
| C6 | 100µF amp |  |
| D1 | LED green |  |
| D2 | LED red |  |
| D3 | 1N5817 3V3→BAT |  |
| D4 | 1N5817 cell→BAT |  |
| C1 | 470µF |  |
| C2 | 100nF |  |
| C3 | 100nF |  |
| C4 | 100nF |  |
| C5 | 100nF |  |
