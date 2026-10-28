# Board D PARAFOIL (Teensy 4.0) — wiring list

Hole = C(column from left on the component side) R(row from bottom).
Bottom runs are listed as straight segments; solder-bridge every hole in each segment.

## GND  — THICK INSULATED WIRE, point to point
- pins: U1.GND (C21 R40), U1.GND (C27 R39), U5.GND (C33 R36), U6.GND (C33 R44), U6.GND (C33 R49), U6.FSYNC (C36 R49), U7.GND (C23 R20), J10.− (C2 R37), J11.− (C2 R36), J12.− (C2 R35), J13.− (C2 R34), J14.− (C3 R33), J15.− (C2 R30), J2.GND (C3 R22), J5.GND (C3 R44), J3.GND (C45 R29), J1.GND (C3 R55), J6.GND (C7 R55), J8.GND (C22 R52), J8.GND (C24 R52), C1.- (C4 R48), C2.- (C8 R48), C3.- (C23 R55), C4.2 (C27 R55), C5.2 (C33 R33)
- wire: C21R40 ↔ C27R39
- wire: C27R39 ↔ C33R36
- wire: C33R36 ↔ C33R33
- wire: C33R36 ↔ C33R44
- wire: C33R44 ↔ C33R49
- wire: C33R49 ↔ C36R49
- wire: C33R49 ↔ C27R55
- wire: C27R55 ↔ C23R55
- wire: C23R55 ↔ C22R52
- wire: C22R52 ↔ C24R52
- wire: C23R55 ↔ C7R55
- wire: C7R55 ↔ C3R55
- wire: C7R55 ↔ C8R48
- wire: C8R48 ↔ C4R48
- wire: C4R48 ↔ C3R44
- wire: C3R44 ↔ C2R37
- wire: C2R37 ↔ C2R36
- wire: C2R36 ↔ C2R35
- wire: C2R35 ↔ C2R34
- wire: C2R34 ↔ C3R33
- wire: C3R33 ↔ C2R30
- wire: C2R30 ↔ C3R22
- wire: C33R33 ↔ C45R29
- wire: C21R40 ↔ C23R20

## +8V_STEER  — THICK INSULATED WIRE, point to point
- pins: J1.+8V (C2 R55), C1.+ (C2 R48), J10.+ (C3 R37), J11.+ (C3 R36)
- wire: C2R55 ↔ C2R48
- wire: C2R48 ↔ C3R37
- wire: C3R37 ↔ C3R36

## +5V_SERVO  — THICK INSULATED WIRE, point to point
- pins: J6.+5VS (C6 R55), C2.+ (C6 R48), J12.+ (C3 R35), J13.+ (C3 R34)
- wire: C6R55 ↔ C6R48
- wire: C6R48 ↔ C3R35
- wire: C3R35 ↔ C3R34

## +5V_LOGIC  — THICK INSULATED WIRE, point to point
- pins: J8.+5V (C21 R52), U1.VIN (C27 R40), C3.+ (C21 R55), C4.1 (C25 R55), U7.VCC (C22 R20), J15.+5V (C3 R30)
- wire: C21R52 ↔ C21R55
- wire: C21R55 ↔ C25R55
- wire: C25R55 ↔ C27R40
- wire: C27R40 ↔ C22R20
- wire: C22R20 ↔ C3R30

## +3V3_B  — THICK INSULATED WIRE, point to point
- pins: J8.+3V3 (C23 R52), U5.VIN (C31 R36), U5.SDO (C35 R36), U5.CS (C37 R36), U6.VIN (C31 R44), R6.2 (C12 R29), C5.1 (C31 R33)
- wire: C23R52 ↔ C31R44
- wire: C31R44 ↔ C31R36
- wire: C31R36 ↔ C31R33
- wire: C31R36 ↔ C35R36
- wire: C35R36 ↔ C37R36
- wire: C31R33 ↔ C12R29

## ICM_1V8
- pins: U6.1V8 (C32 R44), U6.CS (C31 R49), U6.SDO (C32 R49)
- bottom: C32R44→C32R49
- bottom: C32R49→C31R49

## SDA
- pins: U1.18 (C27 R32), U5.SDI (C36 R36), U6.SDA (C35 R44)
- bottom: C27R32→C36R32 · C36R32→C36R36
- bottom: C36R36→C36R43 · C36R43→C35R43 · C35R43→C35R44

## SCL
- pins: U1.19 (C27 R33), U5.SCK (C34 R36), U6.SCL (C34 R44)
- bottom: C27R33→C30R33 · C30R33→C30R35 · C30R35→C34R35 · C34R35→C34R36
- bottom: C34R36→C34R44

## GNSS_IN
- pins: J5.GNSS (C3 R45), U1.0 (C21 R39)
- bottom: C3R45→C20R45 · C20R45→C20R39 · C20R39→C21R39

## STEER_L
- pins: U1.2 (C21 R37), R1.1 (C12 R37)
- bottom: C21R37→C12R37

## STEER_L_S
- pins: R1.2 (C8 R37), J10.S (C4 R37)
- bottom: C8R37→C4R37

## STEER_R
- pins: U1.3 (C21 R36), R2.1 (C12 R36)
- bottom: C21R36→C12R36

## STEER_R_S
- pins: R2.2 (C8 R36), J11.S (C4 R36)
- bottom: C8R36→C4R36

## BAY_LID
- pins: U1.4 (C21 R35), R3.1 (C12 R35)
- bottom: C21R35→C12R35

## BAY_LID_S
- pins: R3.2 (C8 R35), J12.S (C4 R35)
- bottom: C8R35→C4R35

## PETRI
- pins: U1.5 (C21 R34), R4.1 (C12 R34)
- bottom: C21R34→C12R34

## PETRI_S
- pins: R4.2 (C8 R34), J13.S (C4 R34)
- bottom: C8R34→C4R34

## ESC
- pins: U1.6 (C21 R33), R5.1 (C12 R33)
- bottom: C21R33→C12R33

## ESC_S
- pins: R5.2 (C8 R33), J14.S (C4 R33)
- bottom: C8R33→C4R33

## RX2
- pins: J2.RX2←A (C3 R23), U1.7 (C21 R32)
- bottom: C3R23→C6R23 · C6R23→C6R32 · C6R32→C21R32

## TX2
- pins: U1.8 (C21 R31), R7.1 (C8 R22), R8.1 (C38 R31)
- bottom: C21R31→C24R31 · C24R31→C24R25 · C24R25→C33R25 · C33R25→C33R31 · C33R31→C38R31
- bottom: C21R31→C13R31 · C13R31→C13R25 · C13R25→C8R25 · C8R25→C8R22

## TX2_A
- pins: R7.2 (C12 R22), J2.TX2→A (C3 R24)
- bottom: C12R22→C12R21 · C12R21→C2R21 · C2R21→C2R24 · C2R24→C3R24

## TX2_B
- pins: R8.2 (C42 R31), J3.TX→B (C45 R31)
- bottom: C42R31→C45R31

## RX3
- pins: J3.RX3←B (C45 R30), U1.15 (C27 R29)
- bottom: C45R30→C34R30
- bottom: C32R30→C28R30 · C28R30→C28R29 · C28R29→C27R29
- insulated HOP wire (solder side, over the other trace): C34R30 ↔ C32R30

## HALL
- pins: U1.9 (C21 R30), J15.OUT (C4 R30), R6.1 (C8 R29)
- bottom: C21R30→C14R30
- bottom: C12R30→C8R30 · C8R30→C8R29
- bottom: C8R30→C7R30
- bottom: C5R30→C4R30
- insulated HOP wire (solder side, over the other trace): C14R30 ↔ C12R30
- insulated HOP wire (solder side, over the other trace): C7R30 ↔ C5R30

## SD_CS
- pins: U1.10 (C21 R29), U7.CS (C18 R20)
- bottom: C21R29→C18R29 · C18R29→C18R20

## SD_SCK
- pins: U1.13 (C27 R27), U7.SCK (C19 R20)
- bottom: C27R27→C27R26
- bottom: C27R24→C27R19 · C27R19→C19R19 · C19R19→C19R20
- insulated HOP wire (solder side, over the other trace): C27R26 ↔ C27R24

## SD_MOSI
- pins: U1.11 (C21 R28), U7.MOSI (C20 R20)
- bottom: C21R28→C20R28 · C20R28→C20R20

## SD_MISO
- pins: U1.12 (C21 R27), U7.MISO (C21 R20)
- bottom: C21R27→C21R20
