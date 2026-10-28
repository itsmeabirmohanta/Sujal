# Board 4 PARACHUTE — single-layer PCB

Ø110 mm, rod holes Ø8.3, component holes 1.4 mm, pads 2.2 mm, copper on the BOTTOM only.
Tracks 1.0 mm signal, 1.5 mm GND/5 V/3.3 V, 2.0 mm servo power (8 V, 5 V servo). GND copper fill.
No series resistors: every servo / ESC / UART pin is wired straight to the Teensy. R6 = Hall pull-up only.

## Wire jumpers (top side, fit them first)
- W1 (RX3): C35R33 ↔ C27R33
- W2 (GND): C23R12 ↔ C23R14
- W3 (TX2): C24R21 ↔ C34R21
- W4 (SCL): C27R35 ↔ C27R32

## Every pad and its signal
| part | pin | hole | signal |
|---|---|---|---|
| U1 Teensy 4.0 PARACHUTE | GND | C15R23 | GND |
| U1 Teensy 4.0 PARACHUTE | 0 | C16R23 | GNSS_IN |
| U1 Teensy 4.0 PARACHUTE | 1 | C17R23 | not connected |
| U1 Teensy 4.0 PARACHUTE | 2 | C18R23 | STEER_L |
| U1 Teensy 4.0 PARACHUTE | 3 | C19R23 | STEER_R |
| U1 Teensy 4.0 PARACHUTE | 4 | C20R23 | BAY_LID |
| U1 Teensy 4.0 PARACHUTE | 5 | C21R23 | PETRI |
| U1 Teensy 4.0 PARACHUTE | 6 | C22R23 | ESC |
| U1 Teensy 4.0 PARACHUTE | 7 | C23R23 | RX2 |
| U1 Teensy 4.0 PARACHUTE | 8 | C24R23 | TX2 |
| U1 Teensy 4.0 PARACHUTE | 9 | C25R23 | HALL |
| U1 Teensy 4.0 PARACHUTE | 10 | C26R23 | SERVO_X |
| U1 Teensy 4.0 PARACHUTE | 11 | C27R23 | not connected |
| U1 Teensy 4.0 PARACHUTE | 12 | C28R23 | not connected |
| U1 Teensy 4.0 PARACHUTE | 13 | C28R29 | not connected |
| U1 Teensy 4.0 PARACHUTE | 14 | C27R29 | not connected |
| U1 Teensy 4.0 PARACHUTE | 15 | C26R29 | RX3 |
| U1 Teensy 4.0 PARACHUTE | 16 | C25R29 | not connected |
| U1 Teensy 4.0 PARACHUTE | 17 | C24R29 | not connected |
| U1 Teensy 4.0 PARACHUTE | 18 | C23R29 | SDA |
| U1 Teensy 4.0 PARACHUTE | 19 | C22R29 | SCL |
| U1 Teensy 4.0 PARACHUTE | 20 | C21R29 | not connected |
| U1 Teensy 4.0 PARACHUTE | 21 | C20R29 | not connected |
| U1 Teensy 4.0 PARACHUTE | 22 | C19R29 | not connected |
| U1 Teensy 4.0 PARACHUTE | 23 | C18R29 | not connected |
| U1 Teensy 4.0 PARACHUTE | 3V3 | C17R29 | not connected |
| U1 Teensy 4.0 PARACHUTE | GND | C16R29 | GND |
| U1 Teensy 4.0 PARACHUTE | VIN | C15R29 | +5V_LOGIC |
| J5 GNSS from MAIN J8 | GNSS | C8R19 | GNSS_IN |
| J5 GNSS from MAIN J8 | GND | C8R18 | GND |
| J1 8 V steer in (POWER J8) | +8V | C6R14 | +8V_STEER |
| J1 8 V steer in (POWER J8) | +8V | C6R13 | +8V_STEER |
| J1 8 V steer in (POWER J8) | GND | C6R12 | GND |
| J1 8 V steer in (POWER J8) | GND | C6R11 | GND |
| C1 470µF 8V rail | + | C9R13 | +8V_STEER |
| C1 470µF 8V rail | - | C11R12 | GND |
| J10 A20CLM LEFT steer (8 V) | − | C13R12 | GND |
| J10 A20CLM LEFT steer (8 V) | + | C13R13 | +8V_STEER |
| J10 A20CLM LEFT steer (8 V) | S | C13R14 | STEER_L |
| J11 A20CLM RIGHT steer (8 V) | − | C16R12 | GND |
| J11 A20CLM RIGHT steer (8 V) | + | C16R13 | +8V_STEER |
| J11 A20CLM RIGHT steer (8 V) | S | C16R14 | STEER_R |
| J12 MG90S #1 PARACHUTE BAY | − | C19R12 | GND |
| J12 MG90S #1 PARACHUTE BAY | + | C19R13 | +5V_SERVO |
| J12 MG90S #1 PARACHUTE BAY | S | C19R14 | BAY_LID |
| J13 MG90S #2 PETRI PLATE | − | C22R12 | GND |
| J13 MG90S #2 PETRI PLATE | + | C22R13 | +5V_SERVO |
| J13 MG90S #2 PETRI PLATE | S | C22R14 | PETRI |
| J14 ESC signal (Cyclone 45A BLHeli_S) | − | C28R14 | GND |
| J14 ESC signal (Cyclone 45A BLHeli_S) | S | C28R15 | ESC |
| J2 UART from MAIN J4 | TX2→A | C31R16 | TX2 |
| J2 UART from MAIN J4 | RX2←A | C31R15 | RX2 |
| J2 UART from MAIN J4 | GND | C31R14 | GND |
| J16 MG90S #3 EXTRA | − | C34R12 | GND |
| J16 MG90S #3 EXTRA | + | C34R13 | +5V_SERVO |
| J16 MG90S #3 EXTRA | S | C34R14 | SERVO_X |
| J6 5 V servo in (POWER J9) | +5VS | C35R9 | +5V_SERVO |
| J6 5 V servo in (POWER J9) | GND | C36R9 | GND |
| C2 470µF 5VS rail | + | C31R7 | +5V_SERVO |
| C2 470µF 5VS rail | - | C33R7 | GND |
| J8 logic PWR (POWER J7) | +5V | C12R33 | +5V_LOGIC |
| J8 logic PWR (POWER J7) | GND | C13R33 | GND |
| J8 logic PWR (POWER J7) | +3V3 | C14R33 | +3V3_B |
| J8 logic PWR (POWER J7) | GND | C15R33 | GND |
| C3 470µF logic | + | C11R29 | +5V_LOGIC |
| C3 470µF logic | - | C11R27 | GND |
| C4 100nF | 1 | C14R31 | +5V_LOGIC |
| C4 100nF | 2 | C16R31 | GND |
| U5 BMP585 #3 | VIN | C18R32 | +3V3_B |
| U5 BMP585 #3 | 3V3 | C19R32 | not connected |
| U5 BMP585 #3 | GND | C20R32 | GND |
| U5 BMP585 #3 | SCK | C21R32 | SCL |
| U5 BMP585 #3 | SDO | C22R32 | +3V3_B |
| U5 BMP585 #3 | SDI | C23R32 | SDA |
| U5 BMP585 #3 | CS | C24R32 | +3V3_B |
| U5 BMP585 #3 | INT | C25R32 | not connected |
| U6 LSM6DSO32 (Adafruit) | VIN | C28R31 | +3V3_B |
| U6 LSM6DSO32 (Adafruit) | 3Vo | C29R31 | not connected |
| U6 LSM6DSO32 (Adafruit) | GND | C30R31 | GND |
| U6 LSM6DSO32 (Adafruit) | SCL | C31R31 | SCL |
| U6 LSM6DSO32 (Adafruit) | SDA | C32R31 | SDA |
| U6 LSM6DSO32 (Adafruit) | DO | C33R31 | GND |
| U6 LSM6DSO32 (Adafruit) | CS | C34R31 | +3V3_B |
| U6 LSM6DSO32 (Adafruit) | INT1 | C35R31 | not connected |
| U6 LSM6DSO32 (Adafruit) | INT2 | C36R31 | not connected |
| U6 LSM6DSO32 (Adafruit) | SCX | C30R36 | not connected |
| U6 LSM6DSO32 (Adafruit) | SDX | C31R36 | not connected |
| U6 LSM6DSO32 (Adafruit) | CS(back) | C32R36 | not connected |
| U6 LSM6DSO32 (Adafruit) | DO(back) | C33R36 | not connected |
| U6 LSM6DSO32 (Adafruit) | GND(back) | C34R36 | not connected |
| C5 100nF | 1 | C37R29 | +3V3_B |
| C5 100nF | 2 | C37R27 | GND |
| J15 HALL sensor plug (sensor on the body) | − | C31R26 | GND |
| J15 HALL sensor plug (sensor on the body) | +5V | C31R25 | +5V_LOGIC |
| J15 HALL sensor plug (sensor on the body) | OUT | C31R24 | HALL |
| R6 10k HALL↑3V3 | 1 | C33R26 | HALL |
| R6 10k HALL↑3V3 | 2 | C33R22 | +3V3_B |
| J3 UART to BACKUP J3 | TX→B | C36R22 | TX2 |
| J3 UART to BACKUP J3 | RX3←B | C36R23 | RX3 |
| J3 UART to BACKUP J3 | GND | C36R24 | GND |
