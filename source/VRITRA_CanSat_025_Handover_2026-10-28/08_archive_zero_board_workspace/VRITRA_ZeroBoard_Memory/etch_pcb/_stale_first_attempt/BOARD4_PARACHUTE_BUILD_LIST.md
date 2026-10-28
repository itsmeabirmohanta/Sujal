# VRITRA BOARD 4 PARACHUTE — etched single-sided PCB, 150 x 118 mm

Copper on the BOTTOM only. Parts on the top, soldered on the copper side. All holes 1.0 mm.
Grid hole C(column, 1 = left seen from the component side) R(row, 1 = bottom).

## Wire jumpers (component side, bare tinned wire or 0 Ω, fit them FIRST)

| jumper | net | end 1 | end 2 | length |
|---|---|---|---|---|
| JP1 | RX3 | C17 R38 | C28 R38 | 27.9 mm |
| JP2 | RX3 | C27 R39 | C27 R29 | 25.4 mm |
| JP3 | RX3 | C27 R27 | C19 R27 | 20.3 mm |
| JP4 | TX2 | C23 R15 | C23 R26 | 27.9 mm |
| JP5 | TX2 | C27 R37 | C15 R37 | 30.5 mm |
| JP6 | RX2 | C10 R42 | C10 R36 | 15.2 mm |
| JP7 | RX2 | C9 R36 | C2 R36 | 17.8 mm |
| JP8 | SDA | C24 R38 | C21 R38 | 7.6 mm |
| JP9 | SCL | C10 R28 | C22 R28 | 30.5 mm |
| JP10 | SCL | C25 R30 | C25 R38 | 20.3 mm |
| JP11 | GNSS_IN | C12 R42 | C12 R37 | 12.7 mm |
| JP12 | GNSS_IN | C11 R36 | C3 R36 | 20.3 mm |
| JP13 | GNSS_IN | C3 R30 | C3 R18 | 30.5 mm |
| JP14 | SDA1 | C13 R28 | C21 R28 | 20.3 mm |
| JP15 | SCL1 | C27 R31 | C27 R41 | 25.4 mm |
| JP16 | EXP_TX5 | C9 R28 | C17 R28 | 20.3 mm |
| JP17 | EXP_TX5 | C18 R28 | C30 R28 | 30.5 mm |
| JP18 | EXP_RX5 | C30 R27 | C26 R27 | 10.2 mm |
| JP19 | EXP_SCK | C23 R28 | C34 R28 | 27.9 mm |
| JP20 | EXP_A8 | C24 R24 | C29 R24 | 12.7 mm |
| JP21 | EXP_A9 | C19 R21 | C27 R21 | 20.3 mm |
| JP22 | EXP_CS | C38 R27 | C38 R39 | 30.5 mm |
| JP23 | EXP_MOSI | C19 R19 | C22 R19 | 7.6 mm |
| JP24 | +5V_SERVO | C20 R5 | C25 R5 | 12.7 mm |
| JP25 | +3V3_B | C2 R37 | C7 R37 | 12.7 mm |
| JP26 | +3V3_B | C28 R42 | C28 R30 | 30.5 mm |
| JP27 | GND | C26 R12 | C26 R24 | 30.5 mm |
| JP28 | GND | C25 R24 | C25 R35 | 27.9 mm |
| JP29 | GND | C25 R37 | C20 R37 | 12.7 mm |
| JP30 | GND | C25 R36 | C25 R40 | 10.2 mm |
| JP31 | GND | C25 R39 | C30 R39 | 12.7 mm |
| JP32 | GND | C31 R42 | C43 R42 | 30.5 mm |
| JP33 | +5V_LOGIC | C4 R36 | C13 R36 | 22.9 mm |
| JP34 | +5V_LOGIC | C13 R37 | C16 R37 | 7.6 mm |
| JP35 | +5V_LOGIC | C17 R37 | C28 R37 | 27.9 mm |
| JP36 | +5V_LOGIC | C31 R38 | C43 R38 | 30.5 mm |

## Parts

| ref | part | pins |
|---|---|---|
| U1 | Teensy 4.0 PARACHUTE | GND@C4R20, 0@C5R20, 1@C6R20, 2@C7R20, 3@C8R20, 4@C9R20, 5@C10R20, 6@C11R20, 7@C12R20, 8@C13R20, 9@C14R20, 10@C15R20, 11@C16R20, 12@C17R20, 13@C17R26, 14@C16R26, 15@C15R26, 16@C14R26, 17@C13R26, 18@C12R26, 19@C11R26, 20@C10R26, 21@C9R26, 22@C8R26, 23@C7R26, 3V3@C6R26, GND@C5R26, VIN@C4R26 |
| U5 | BMP585 #3 | VIN@C6R30, 3V3@C7R30, GND@C8R30, SCK@C9R30, SDO@C10R30, SDI@C11R30, CS@C12R30, INT@C13R30 |
| U6 | ICM-20948 #2 | VIN@C17R30, 1V8@C18R30, GND@C19R30, SCL@C20R30, SDA@C21R30, INT@C22R30, CS@C17R35, SDO@C18R35, GND@C19R35, ACL@C20R35, ADA@C21R35, FSYNC@C22R35 |
| J1 | 8 V STEER in (2 pins each) | +8V@C3R3, +8V@C4R3, GND@C5R3, GND@C6R3 |
| J10 | A20CLM LEFT | −@C8R3, +@C9R3, S@C10R3 |
| J11 | A20CLM RIGHT | −@C12R3, +@C13R3, S@C14R3 |
| J6 | 5 V SERVO in | +5VS@C17R3, GND@C18R3 |
| J12 | MG90S BAY LID | −@C20R3, +@C21R3, S@C22R3 |
| J13 | MG90S PETRI | −@C24R3, +@C25R3, S@C26R3 |
| J14 | ESC signal | −@C28R3, S@C29R3 |
| J15 | HALL sensor | −@C31R3, +5V@C32R3, OUT@C33R3 |
| J8 | PWR from Board 3 | +5V@C3R44, GND@C4R44, +3V3@C5R44, GND@C6R44 |
| J2 | UART to Board 1 | TX2→A@C8R44, RX2←A@C9R44, GND@C10R44 |
| J5 | GNSS from Board 1 | GNSS@C12R44, GND@C13R44 |
| J3 | UART to Board 2 | TX→B@C15R44, RX3←B@C16R44, GND@C17R44 |
| J20 | EXP I2C | 3V3@C19R44, GND@C20R44, SDA@C21R44, SCL@C22R44 |
| J21 | EXP I2C1 (2nd bus) | 3V3@C24R44, GND@C25R44, SDA1@C26R44, SCL1@C27R44 |
| J22 | EXP UART5 (3.3 V logic) | +5V@C29R44, GND@C30R44, TX5@C31R44, RX5@C32R44 |
| J23 | EXP analog | 3V3@C34R44, GND@C35R44, A8@C36R44, A9@C37R44 |
| J24 | EXP SPI | CS@C39R44, SCK@C40R44, MOSI@C41R44, MISO@C42R44, GND@C43R44 |
| J25 | EXP 5 V | +5V@C45R44, GND@C46R44 |
| C1 | 470µF 8V rail | +@C3R7, -@C5R7 |
| C2 | 470µF 5VS rail | +@C17R6, -@C17R8 |
| C3 | 470µF | +@C4R39, -@C4R41 |
| C4 | 100nF | 1@C7R40, 2@C7R42 |
| C5 | 100nF | 1@C6R28, 2@C8R28 |
| R1 | 330 STEER L | 1@C8R16, 2@C8R12 |
| R2 | 330 STEER R | 1@C13R9, 2@C13R5 |
| R3 | 330 BAY LID | 1@C11R18, 2@C11R14 |
| R4 | 330 PETRI | 1@C15R16, 2@C15R12 |
| R5 | 330 ESC | 1@C20R9, 2@C24R9 |
| R6 | 10k HALL↑3V3 | 1@C24R16, 2@C24R20 |
| R7 | 1k TX2→A | 1@C9R37, 2@C9R41 |
| R8 | 1k TX→B | 1@C15R38, 2@C15R42 |

Notes: Servo leads: crimp JST-XH plugs (− + S), or fit 2.54 male headers in the same holes. 8 V steer power comes in on 2 pins + 2 pins (6 A).
