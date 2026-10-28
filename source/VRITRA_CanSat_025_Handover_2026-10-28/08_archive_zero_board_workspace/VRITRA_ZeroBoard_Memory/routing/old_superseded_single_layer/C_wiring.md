# Board C ENVIRONMENT — wiring list

Hole = C(column from left on the component side) R(row from bottom).
Bottom runs are listed as straight segments; solder-bridge every hole in each segment.

## GND  — THICK INSULATED WIRE, point to point
- pins: J1.GND (C3 R39), J7.GND (C3 R33), J3.GND (C3 R27), J2.GND (C21 R44), J4.GND (C21 R34), J8.GND (C21 R26), J5.GND (C33 R34), J6.GND (C33 R26), R5.2 (C42 R20), C1.2 (C28 R44)
- wire: C3R39 ↔ C3R33
- wire: C3R33 ↔ C3R27
- wire: C3R33 ↔ C21R34
- wire: C21R34 ↔ C21R26
- wire: C21R34 ↔ C21R44
- wire: C21R44 ↔ C28R44
- wire: C21R26 ↔ C33R26
- wire: C33R26 ↔ C33R34
- wire: C33R26 ↔ C42R20

## +3V3_A  — THICK INSULATED WIRE, point to point
- pins: J1.3V3 (C3 R40), J2.3V3 (C20 R44), J4.3V3 (C20 R34), J8.3V3 (C20 R26), J5.3V3 (C32 R34), R1.2 (C14 R44), R2.2 (C14 R46), C1.1 (C26 R44)
- wire: C3R40 ↔ C14R44
- wire: C14R44 ↔ C14R46
- wire: C14R44 ↔ C20R44
- wire: C20R44 ↔ C26R44
- wire: C20R44 ↔ C20R34
- wire: C20R34 ↔ C20R26
- wire: C20R34 ↔ C32R34

## +5V_MQ  — THICK INSULATED WIRE, point to point
- pins: J3.+5V (C3 R28), J6.+5V (C32 R26)
- wire: C3R28 ↔ C32R26

## SDA1
- pins: J1.SDA1 (C3 R42), J2.SDA (C22 R44), J4.SDA (C22 R34), J8.SDA (C22 R26), R1.1 (C10 R44)
- bottom: C3R42→C3R43 · C3R43→C10R43 · C10R43→C10R44
- bottom: C10R43→C22R43 · C22R43→C22R44
- bottom: C22R43→C22R34
- bottom: C22R34→C22R26

## SCL1
- pins: J1.SCL1 (C3 R41), J2.SCL (C23 R44), J4.SCL (C23 R34), J8.SCL (C23 R26), R2.1 (C10 R46)
- bottom: C3R41→C2R41 · C2R41→C2R46 · C2R46→C10R46
- bottom: C10R46→C10R45 · C10R45→C23R45 · C23R45→C23R44
- bottom: C23R44→C23R34
- bottom: C23R34→C23R26

## ENV_UV
- pins: J7.UV (C3 R34), J5.UV (C34 R34)
- bottom: C3R34→C4R34 · C4R34→C4R25 · C4R25→C31R25 · C31R25→C31R33 · C31R33→C34R33 · C34R33→C34R34

## MQ_AO
- pins: J6.AO (C34 R26), R4.1 (C38 R24)
- bottom: C34R26→C34R24 · C34R24→C38R24

## ENV_O3
- pins: R4.2 (C42 R24), R5.1 (C38 R20), J7.O3 (C3 R35)
- bottom: C42R24→C39R24 · C39R24→C39R20 · C39R20→C38R20
- bottom: C39R23→C2R23 · C2R23→C2R35 · C2R35→C3R35
