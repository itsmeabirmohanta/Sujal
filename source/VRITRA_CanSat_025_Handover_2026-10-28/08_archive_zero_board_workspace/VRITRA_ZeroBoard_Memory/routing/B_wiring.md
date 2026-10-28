# Board 2 BACKUP + SENSOR (Teensy 4.1 #2) — wiring list

Hole = C(column from left on the component side) R(row from bottom).
Bottom runs are listed as straight segments; solder-bridge every hole in each segment.

## GND  — THICK INSULATED WIRE, point to point
- pins: U1.GND (C27 R40), U1.GND (C21 R41), U1.GND (C27 R27), U5.GND (C33 R36), U8.GND (C33 R44), U8.DO (C36 R44), J1.GND (C22 R52), J1.GND (C24 R52), J2.GND (C3 R37), J3.GND (C45 R29), J4.GND (C45 R21), D1.K (C35 R22), D2.K (C35 R25), C1.- (C23 R55), C2.2 (C27 R55), C3.2 (C33 R33), C4.2 (C44 R42), U10.GND (C29 R12), U10.GND (C36 R6), U10B.GND (C32 R3), R6.2 (C18 R30), R7.2 (C18 R28), R8.2 (C18 R26), U11.GND (C12 R41), M1L.GND (C42 R50)
- wire: C27R40 ↔ C21R41
- wire: C21R41 ↔ C12R41
- wire: C27R40 ↔ C33R36
- wire: C33R36 ↔ C33R33
- wire: C33R36 ↔ C33R44
- wire: C33R44 ↔ C36R44
- wire: C36R44 ↔ C44R42
- wire: C44R42 ↔ C42R50
- wire: C33R33 ↔ C35R25
- wire: C35R25 ↔ C35R22
- wire: C35R25 ↔ C27R27
- wire: C27R27 ↔ C18R26
- wire: C18R26 ↔ C18R28
- wire: C18R28 ↔ C18R30
- wire: C35R22 ↔ C45R21
- wire: C45R21 ↔ C45R29
- wire: C21R41 ↔ C22R52
- wire: C22R52 ↔ C24R52
- wire: C22R52 ↔ C23R55
- wire: C23R55 ↔ C27R55
- wire: C12R41 ↔ C3R37
- wire: C35R22 ↔ C29R12
- wire: C29R12 ↔ C32R3
- wire: C32R3 ↔ C36R6

## +5V_LOGIC  — THICK INSULATED WIRE, point to point
- pins: J1.+5V (C21 R52), U1.VIN (C27 R41), C1.+ (C21 R55), C2.1 (C25 R55), J4.+5V (C45 R22), U11.VCC (C11 R41)
- wire: C21R52 ↔ C21R55
- wire: C21R55 ↔ C25R55
- wire: C25R55 ↔ C27R41
- wire: C27R41 ↔ C11R41
- wire: C27R41 ↔ C45R22

## +3V3_B  — THICK INSULATED WIRE, point to point
- pins: J1.+3V3 (C23 R52), U5.VIN (C31 R36), U5.SDO (C35 R36), U5.CS (C37 R36), U8.VIN (C31 R44), U8.CS (C37 R44), C3.1 (C31 R33), C4.1 (C42 R42), U10B.3V3 (C31 R3), M1L.3V3 (C41 R50)
- wire: C23R52 ↔ C31R44
- wire: C31R44 ↔ C37R44
- wire: C37R44 ↔ C42R42
- wire: C31R44 ↔ C31R36
- wire: C31R36 ↔ C31R33
- wire: C31R36 ↔ C35R36
- wire: C35R36 ↔ C37R36
- wire: C42R42 ↔ C41R50
- wire: C31R33 ↔ C31R3

## SD_CS
- pins: U1.10 (C21 R30), U11.CS (C7 R41)
- bottom: C19R30→C19R31 · C19R31→C17R31
- bottom: C13R31→C7R31 · C7R31→C7R35
- bottom: C7R37→C7R41
- insulated HOP wire (solder side, over the other trace): C21R30 ↔ C19R30
- insulated HOP wire (solder side, over the other trace): C17R31 ↔ C15R31
- insulated HOP wire (solder side, over the other trace): C15R31 ↔ C13R31
- insulated HOP wire (solder side, over the other trace): C7R35 ↔ C7R37

## SD_SCK
- pins: U1.13 (C27 R28), U11.SCK (C8 R41)
- bottom: C27R28→C25R28 · C25R28→C25R44 · C25R44→C8R44 · C8R44→C8R41

## SD_MOSI
- pins: U1.11 (C21 R29), U11.MOSI (C9 R41)
- bottom: C21R29→C22R29 · C22R29→C22R42 · C22R42→C11R42
- bottom: C9R42→C9R41
- insulated HOP wire (solder side, over the other trace): C11R42 ↔ C9R42

## SD_MISO
- pins: U1.12 (C21 R28), U11.MISO (C10 R41)
- bottom: C21R28→C23R28 · C23R28→C23R43 · C23R43→C10R43 · C10R43→C10R41

## CAM_TX_J
- pins: J2.MUX_TX (C3 R39), U10.TX1 (C29 R14)
- bottom: C3R39→C5R39 · C5R39→C5R16 · C5R16→C10R16
- bottom: C14R16→C22R16 · C22R16→C22R14 · C22R14→C29R14
- insulated HOP wire (solder side, over the other trace): C10R16 ↔ C12R16
- insulated HOP wire (solder side, over the other trace): C12R16 ↔ C14R16

## CAM_RX
- pins: J2.MUX_RX (C3 R38), U10.RX1 (C29 R13)
- bottom: C3R38→C4R38 · C4R38→C4R13 · C4R13→C29R13

## MUX_A
- pins: U1.5 (C21 R35), U10.A (C29 R7), R6.1 (C14 R30)
- bottom: C21R35→C14R35 · C14R35→C14R30
- bottom: C14R30→C13R30 · C13R30→C13R29
- bottom: C13R27→C13R14
- bottom: C13R12→C13R7 · C13R7→C29R7
- insulated HOP wire (solder side, over the other trace): C13R29 ↔ C13R27
- insulated HOP wire (solder side, over the other trace): C13R14 ↔ C13R12

## MUX_B
- pins: U1.6 (C21 R34), U10.B (C29 R6), R7.1 (C14 R28)
- bottom: C21R34→C20R34 · C20R34→C20R32 · C20R32→C16R32 · C16R32→C16R28 · C16R28→C14R28
- bottom: C14R28→C11R28 · C11R28→C11R14
- bottom: C11R12→C11R6 · C11R6→C29R6
- insulated HOP wire (solder side, over the other trace): C11R14 ↔ C11R12

## MUX_C
- pins: U1.9 (C21 R31), U10.C (C36 R7), R8.1 (C14 R26)
- bottom: C21R31→C20R31 · C20R31→C20R27 · C20R27→C17R27 · C17R27→C17R26 · C17R26→C14R26
- bottom: C20R27→C20R17 · C20R17→C35R17 · C35R17→C35R7 · C35R7→C36R7

## CAM_COM_TX
- pins: U10B.TX_COM (C34 R3), R9.1 (C38 R17)
- bottom: C34R3→C37R3 · C37R3→C37R17 · C37R17→C38R17

## CAM_TO_XIAO
- pins: R9.2 (C42 R17), J4.to D7 (C45 R20)
- bottom: C42R17→C42R20 · C42R20→C45R20

## CAM_FROM_XIAO
- pins: J4.from D6 (C45 R19), R10.2 (C42 R15)
- bottom: C45R19→C45R15 · C45R15→C42R15

## CAM_COM_RX
- pins: R10.1 (C38 R15), U10B.RX_COM (C33 R3)
- bottom: C38R15→C38R2 · C38R2→C33R2 · C33R2→C33R3

## SDA
- pins: U1.18 (C27 R33), U5.SDI (C36 R36), U8.SDA (C35 R44), M1L.SDA (C43 R50)
- bottom: C27R33→C28R33 · C28R33→C28R32 · C28R32→C36R32 · C36R32→C36R36
- bottom: C36R36→C36R43 · C36R43→C35R43 · C35R43→C35R44
- bottom: C36R43→C41R43 · C41R43→C41R49 · C41R49→C43R49 · C43R49→C43R50

## SCL
- pins: U1.19 (C27 R34), U5.SCK (C34 R36), U8.SCL (C34 R44), M1L.SCL (C44 R50)
- bottom: C27R34→C34R34 · C34R34→C34R36
- bottom: C34R36→C34R44
- bottom: C34R44→C34R48 · C34R48→C40R48 · C40R48→C40R51 · C40R51→C44R51 · C44R51→C44R50

## GNSS_IN
- pins: J2.GNSS (C3 R40), U1.0 (C21 R40)
- bottom: C3R40→C6R40
- bottom: C8R40→C21R40
- insulated HOP wire (solder side, over the other trace): C6R40 ↔ C8R40

## HB_IN
- pins: J2.HB (C3 R41), U1.4 (C21 R36)
- bottom: C3R41→C6R41
- bottom: C6R39→C6R36 · C6R36→C21R36
- insulated HOP wire (solder side, over the other trace): C6R41 ↔ C6R39

## TX3
- pins: U1.14 (C27 R29), R1.1 (C38 R29)
- bottom: C27R29→C38R29

## TX3_J
- pins: R1.2 (C42 R29), J3.TX3 (C45 R30)
- bottom: C42R29→C42R30 · C42R30→C45R30

## RX3
- pins: J3.RX3 (C45 R31), U1.15 (C27 R30)
- bottom: C45R31→C28R31 · C28R31→C28R30 · C28R30→C27R30

## TX8
- pins: U1.35 (C27 R20), R2.1 (C29 R20)
- bottom: C27R20→C29R20

## TX8_J
- pins: R2.2 (C33 R20), U10.TX2 (C29 R10)
- bottom: C33R20→C32R20 · C32R20→C32R18
- bottom: C32R16→C32R10 · C32R10→C29R10
- insulated HOP wire (solder side, over the other trace): C32R18 ↔ C32R16

## RX8_J
- pins: U10.RX2 (C29 R9), R3.2 (C33 R18)
- bottom: C29R9→C33R9 · C33R9→C33R16
- insulated HOP wire (solder side, over the other trace): C33R16 ↔ C33R18

## RX8
- pins: R3.1 (C29 R18), U1.34 (C27 R19)
- bottom: C29R18→C29R19 · C29R19→C27R19

## LED_OK
- pins: U1.36 (C27 R21), R4.1 (C29 R23)
- bottom: C27R21→C29R21 · C29R21→C29R23

## LED_OK_A
- pins: R4.2 (C33 R23), D1.A (C35 R23)
- bottom: C33R23→C35R23

## LED_FLT
- pins: U1.37 (C27 R22), R5.1 (C29 R25)
- bottom: C27R22→C28R22 · C28R22→C28R25 · C28R25→C29R25

## LED_FLT_A
- pins: R5.2 (C33 R25), D2.A (C35 R26)
- bottom: C33R25→C33R26 · C33R26→C35R26
