# BOARD2_BACKUP_SENSOR — etched single-layer PCB, build notes

Board 118 × 92 mm, copper on the BOTTOM only, parts on the top (through-hole).
Tracks never cross: where two connections must cross, one goes over the top as a WIRE JUMPER.
Copper rules: pads Ø1.8 mm, tracks 0.7 mm (servo power 1.8 mm), every gap ≥ 0.5 mm, GND copper fill.

## Wire jumpers (insulated wire on the component side, fit them FIRST — some run under modules)
| jumper | net | from (x, y mm) | to (x, y mm) | length |
|---|---|---|---|---|
| W1 | LED_OK | (70.4, 34.4) | (70.4, 3.9) | 30.5 mm |
| W2 | LED_OK | (65.3, 3.9) | (65.3, 11.5) | 7.6 mm |
| W3 | LED_OK | (75.5, 14.0) | (75.5, 24.2) | 10.2 mm |
| W4 | TX8_J | (37.4, 64.8) | (32.3, 64.8) | 5.1 mm |
| W5 | LED_FLT | (70.4, 36.9) | (70.4, 19.1) | 17.8 mm |
| W6 | LED_FLT_A | (90.8, 19.1) | (90.8, 3.9) | 15.2 mm |
| W7 | SD_MISO | (50.1, 47.1) | (19.6, 47.1) | 30.5 mm |
| W8 | MUX_A | (55.2, 69.9) | (40.0, 69.9) | 15.2 mm |
| W9 | SD_MOSI | (52.6, 52.1) | (22.2, 52.1) | 30.5 mm |
| W10 | RX3 | (111.1, 26.7) | (90.8, 26.7) | 20.3 mm |
| W11 | RX3 | (70.4, 26.7) | (70.4, 57.2) | 30.5 mm |
| W12 | SD_CS | (55.2, 57.2) | (24.7, 57.2) | 30.5 mm |
| W13 | GNSS_IN | (24.7, 80.1) | (55.2, 80.1) | 30.5 mm |
| W14 | TX3 | (75.5, 34.4) | (98.4, 34.4) | 22.9 mm |
| W15 | MUX_C | (55.2, 59.8) | (50.1, 59.8) | 5.1 mm |
| W16 | MUX_C | (50.1, 57.2) | (32.3, 57.2) | 17.8 mm |
| W17 | SD_SCK | (70.4, 52.1) | (70.4, 47.1) | 5.1 mm |
| W18 | SD_SCK | (70.4, 44.5) | (70.4, 14.0) | 30.5 mm |
| W19 | SD_SCK | (55.2, 14.0) | (24.7, 14.0) | 30.5 mm |
| W20 | HB_IN | (34.9, 82.6) | (34.9, 72.5) | 10.2 mm |
| W21 | SCL | (85.7, 69.9) | (85.7, 62.3) | 7.6 mm |
| W22 | SDA | (75.5, 57.2) | (95.8, 57.2) | 20.3 mm |
| W23 | CAM_COM_TX | (113.6, 19.1) | (106.0, 19.1) | 7.6 mm |
| W24 | CAM_COM_RX | (17.1, 54.7) | (17.1, 77.5) | 22.9 mm |
| W25 | +3V3_B | (95.8, 72.5) | (95.8, 62.3) | 10.2 mm |
| W26 | +3V3_B | (103.4, 75.0) | (103.4, 85.2) | 10.2 mm |
| W27 | +5V_LOGIC | (93.3, 82.6) | (93.3, 67.4) | 15.2 mm |
| W28 | +5V_LOGIC | (65.3, 85.2) | (65.3, 54.7) | 30.5 mm |
| W29 | +5V_LOGIC | (70.4, 39.4) | (70.4, 9.0) | 30.5 mm |
| W30 | GND | (55.2, 85.2) | (45.0, 85.2) | 10.2 mm |
| W31 | GND | (42.5, 82.6) | (42.5, 75.0) | 7.6 mm |
| W32 | GND | (80.6, 75.0) | (80.6, 62.3) | 12.7 mm |
| W33 | GND | (27.2, 72.5) | (12.0, 72.5) | 15.2 mm |
| W34 | GND | (27.2, 64.8) | (27.2, 49.6) | 15.2 mm |
| W35 | GND | (27.2, 31.8) | (52.6, 31.8) | 25.4 mm |

x from the LEFT edge, y from the BOTTOM edge, looking at the component side.

## Parts
| ref | part | notes |
|---|---|---|
| U1 | Teensy 4.1 BACKUP | female headers · USB up |
| KO1 | microSD card access |  |
| J2 | LINK from MAIN J5 |  |
| U10 | 7Semi UART mux | DIP OFF · VCC jumper 3V3 · ch1 MAIN, ch2 BACKUP |
| U11 | microSD card module | flat on its header · card slot DOWN · VCC 5 V |
| J4 | CAMERA plug (XIAO) |  |
| R9 | 1k cam RX |  |
| R10 | 1k cam TX |  |
| R2 | 1k TX8 |  |
| R3 | 1k RX8 |  |
| R6 | 10k MUX_A↓ |  |
| R7 | 10k MUX_B↓ |  |
| R8 | 10k MUX_C↓ |  |
| J1 | PWR from POWER J6 |  |
| C1 | 470µF |  |
| C2 | 100nF |  |
| U5 | BMP585 #2 | 0x47 (SDO→3V3) |
| U8 | LSM6DSO32 | CS→3V3 · DO→GND = 0x6A · solder FRONT row only |
| J5 | HDC3020 plug (+ − D C) |  |
| M1 | HDC3020 module spot | double-sided tape · 0x44 |
| C3 | 100nF |  |
| C4 | 100nF |  |
| J3 | UART to PARACHUTE J3 |  |
| R1 | 1k TX3 |  |
| R4 | 470 LED OK |  |
| R5 | 470 LED FLT |  |
| D1 | LED green |  |
| D2 | LED red |  |
