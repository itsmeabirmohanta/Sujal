# Board E POWER — wiring list

Hole = C(column from left on the component side) R(row from bottom).
Bottom runs are listed as straight segments; solder-bridge every hole in each segment.

## SCL
- pins: J2.SCL (C30 R57), U1.SCL (C5 R48), U2.SCL (C5 R12), U3.SCL (C5 R28)
- bottom: C30R57→C12R57 · C12R57→C12R48 · C12R48→C5R48
- bottom: C7R48→C7R28 · C7R28→C5R28
- bottom: C6R28→C6R12 · C6R12→C5R12

## SDA
- pins: J2.SDA (C31 R57), U1.SDA (C5 R47), U2.SDA (C5 R11), U3.SDA (C5 R27)
- bottom: C31R57→C31R58 · C31R58→C4R58 · C4R58→C4R47 · C4R47→C5R47
- bottom: C4R47→C4R27 · C4R27→C5R27
- bottom: C4R27→C4R11 · C4R11→C5R11

## GND  — THICK INSULATED WIRE, point to point
- pins: J1.BAT− (C3 R57), U1.GND (C5 R49), U2.GND (C5 R13), U3.GND (C5 R29), M1L.GND (C19 R49), M2L.GND (C34 R49), M3L.GND (C19 R39), M4L.GND (C19 R21), M5L.GND (C30 R21), D1.K (C20 R53), J4.GND (C45 R57), J2.GND (C32 R57), J5.GND (C44 R15), J5.GND (C44 R13), J6.GND (C41 R15), J6.GND (C41 R13), J7.GND (C38 R15), J7.GND (C38 R13), J8.GND (C35 R15), J9.GND (C35 R11), J10.GND (C44 R8), J11.GND (C41 R8), C1.- (C24 R44)
- wire: C3R57 ↔ C5R49
- wire: C5R49 ↔ C19R49
- wire: C19R49 ↔ C20R53
- wire: C19R49 ↔ C19R39
- wire: C19R49 ↔ C24R44
- wire: C19R49 ↔ C34R49
- wire: C34R49 ↔ C32R57
- wire: C32R57 ↔ C45R57
- wire: C19R39 ↔ C19R21
- wire: C19R21 ↔ C30R21
- wire: C30R21 ↔ C35R15
- wire: C35R15 ↔ C38R15
- wire: C38R15 ↔ C38R13
- wire: C38R13 ↔ C41R13
- wire: C41R13 ↔ C41R15
- wire: C41R13 ↔ C44R13
- wire: C44R13 ↔ C44R15
- wire: C35R15 ↔ C35R11
- wire: C44R13 ↔ C44R8
- wire: C44R8 ↔ C41R8
- wire: C5R49 ↔ C5R29
- wire: C5R29 ↔ C5R13

## BAT+  — THICK INSULATED WIRE, point to point
- pins: J1.BAT+ (C2 R57), J3.SW_A (C6 R57)
- wire: C2R57 ↔ C6R57

## VBAT_SW  — THICK INSULATED WIRE, point to point
- pins: J3.SW_B (C7 R57), F1.IN (C10 R57), R1.1 (C14 R54)
- wire: C7R57 ↔ C10R57
- wire: C10R57 ↔ C14R54

## VBAT_F  — THICK INSULATED WIRE, point to point
- pins: F1.OUT (C11 R57), U1.IN+ (C5 R44)
- wire: C11R57 ↔ C5R44

## VBAT  — THICK INSULATED WIRE, point to point
- pins: U1.IN- (C5 R43), U1.VBUS (C5 R45), M1L.IN (C19 R50), M2L.IN (C34 R50), M3L.IN (C19 R40), J4.VBAT (C44 R57), J10.VBAT (C44 R9), C1.+ (C22 R44)
- wire: C5R43 ↔ C5R45
- wire: C5R43 ↔ C19R40
- wire: C19R40 ↔ C22R44
- wire: C22R44 ↔ C19R50
- wire: C19R50 ↔ C34R50
- wire: C34R50 ↔ C44R57
- wire: C44R57 ↔ C44R9

## LED_A
- pins: R1.2 (C18 R54), D1.A (C20 R54)
- bottom: C18R54→C20R54

## 8V_RAW  — THICK INSULATED WIRE, point to point
- pins: M3L.OUT (C19 R38), U2.IN+ (C5 R8)
- wire: C19R38 ↔ C5R8

## +8V_STEER  — THICK INSULATED WIRE, point to point
- pins: U2.IN- (C5 R7), U2.VBUS (C5 R9), J8.+8V (C35 R16)
- wire: C5R7 ↔ C5R9
- wire: C5R9 ↔ C35R16

## 5V_RAW  — THICK INSULATED WIRE, point to point
- pins: M1L.OUT (C19 R48), U3.IN+ (C5 R24)
- wire: C19R48 ↔ C5R24

## +5V_LOGIC  — THICK INSULATED WIRE, point to point
- pins: U3.IN- (C5 R23), U3.VBUS (C5 R25), J5.+5V (C44 R16), J6.+5V (C41 R16), J7.+5V (C38 R16), J11.+5V (C41 R9), M4L.IN (C19 R22), M5L.IN (C30 R22)
- wire: C5R23 ↔ C5R25
- wire: C5R23 ↔ C19R22
- wire: C19R22 ↔ C30R22
- wire: C30R22 ↔ C38R16
- wire: C38R16 ↔ C41R16
- wire: C41R16 ↔ C44R16
- wire: C41R16 ↔ C41R9

## +5V_SERVO  — THICK INSULATED WIRE, point to point
- pins: M2L.OUT (C34 R48), J9.+5VS (C35 R12)
- wire: C34R48 ↔ C35R12

## +3V3_A  — THICK INSULATED WIRE, point to point
- pins: M4L.OUT (C19 R20), J5.+3V3_A (C44 R14), U1.VCC (C5 R50), U2.VCC (C5 R14), U3.VCC (C5 R30)
- wire: C19R20 ↔ C5R14
- wire: C5R14 ↔ C5R30
- wire: C5R30 ↔ C5R50
- wire: C19R20 ↔ C44R14

## +3V3_B  — THICK INSULATED WIRE, point to point
- pins: M5L.OUT (C30 R20), J6.+3V3_B (C41 R14), J7.+3V3_B (C38 R14)
- wire: C30R20 ↔ C38R14
- wire: C38R14 ↔ C41R14
