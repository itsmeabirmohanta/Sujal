# BOARD4_PARACHUTE — etched single-layer PCB, build notes

Board 118 × 83 mm, copper on the BOTTOM only, parts on the top (through-hole).
Tracks never cross: where two connections must cross, one goes over the top as a WIRE JUMPER.
Copper rules: pads Ø1.8 mm, tracks 0.7 mm (servo power 1.8 mm), every gap ≥ 0.5 mm, GND copper fill.

## Wire jumpers (insulated wire on the component side, fit them FIRST — some run under modules)
| jumper | net | from (x, y mm) | to (x, y mm) | length |
|---|---|---|---|---|
| W1 | SCL | (78.1, 44.5) | (108.5, 44.5) | 30.5 mm |
| W2 | SDA | (62.8, 44.5) | (83.1, 44.5) | 20.3 mm |
| W3 | +3V3_B | (62.8, 62.3) | (62.8, 39.4) | 22.9 mm |
| W4 | +3V3_B | (62.8, 31.8) | (37.4, 31.8) | 25.4 mm |
| W5 | GND | (55.2, 75.0) | (73.0, 75.0) | 17.8 mm |
| W6 | GND | (75.5, 75.0) | (75.5, 69.9) | 5.1 mm |
| W7 | GND | (90.8, 64.8) | (90.8, 42.0) | 22.9 mm |
| W8 | GND | (75.5, 39.4) | (75.5, 26.7) | 12.7 mm |

x from the LEFT edge, y from the BOTTOM edge, looking at the component side.

## Parts
| ref | part | notes |
|---|---|---|
| U1 | Teensy 4.0 PARACHUTE | female headers · USB up |
| J1 | 8 V STEER in (POWER J8) |  |
| J10 | A20CLM LEFT |  |
| J11 | A20CLM RIGHT |  |
| J6 | 5 V SERVO in (POWER J9) |  |
| J12 | MG90S #1 BAY LID |  |
| J13 | MG90S #2 PETRI |  |
| J14 | ESC signal (2 thin wires) |  |
| J15 | HALL sensor |  |
| R1 | 330 |  |
| R2 | 330 |  |
| R3 | 330 |  |
| R4 | 330 |  |
| R5 | 330 |  |
| R6 | 10k HALL↑ |  |
| C1 | 470µF 8V |  |
| C2 | 470µF 5VS |  |
| J8 | logic PWR (POWER J7) |  |
| C3 | 470µF |  |
| C4 | 100nF |  |
| U5 | BMP585 #3 | 0x47 · own 600 m baro |
| U6 | ICM-20948 #2 | CS+SDO → 1V8 · 0x69 |
| C5 | 100nF |  |
| J5 | GNSS from MAIN J8 |  |
| J2 | UART from MAIN J4 |  |
| J3 | UART to BACKUP J3 |  |
| R7 | 1k TX2→A |  |
| R8 | 1k TX→B |  |
