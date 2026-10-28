# Board A PRIMARY (Teensy 4.1 #1) — wiring list

Hole = C(column from left on the component side) R(row from bottom).
Bottom runs are listed as straight segments; solder-bridge every hole in each segment.

## GND  — THICK INSULATED WIRE, point to point
- pins: U1.GND (C27 R40), U1.GND (C21 R41), U1.GND (C27 R27), U2.GND (C14 R12), U2.GND (C14 R18), U2.GND (C9 R18), U3.GND (C14 R56), U4.GND (C35 R56), U5.GND (C33 R36), U6.GND (C42 R45), U6.GND (C42 R50), U6.FSYNC (C45 R50), U7.GND (C9 R9), J1.GND (C22 R52), J1.GND (C24 R52), J2.GND (C28 R52), J3.GND (C44 R57), J6.GND (C45 R33), J9.GND (C45 R38), J4.GND (C45 R26), J8.GND (C45 R30), J5.GND (C3 R20), U9.GND (C37 R10), R12.2 (C43 R4), C6.- (C42 R8), J7.GND (C4 R47), R8.2 (C43 R52), D1.K (C19 R11), D2.K (C18 R10), C1.- (C23 R55), C2.2 (C27 R55), C3.2 (C6 R20), C4.2 (C33 R33), C5.2 (C42 R42)
- wire: C27R40 ↔ C21R41
- wire: C27R40 ↔ C33R36
- wire: C33R36 ↔ C33R33
- wire: C33R33 ↔ C45R33
- wire: C45R33 ↔ C45R30
- wire: C45R30 ↔ C45R26
- wire: C45R33 ↔ C45R38
- wire: C45R38 ↔ C42R42
- wire: C42R42 ↔ C42R45
- wire: C42R45 ↔ C42R50
- wire: C42R50 ↔ C43R52
- wire: C42R50 ↔ C45R50
- wire: C43R52 ↔ C44R57
- wire: C44R57 ↔ C35R56
- wire: C35R56 ↔ C27R55
- wire: C27R55 ↔ C28R52
- wire: C28R52 ↔ C24R52
- wire: C24R52 ↔ C22R52
- wire: C27R55 ↔ C23R55
- wire: C23R55 ↔ C14R56
- wire: C33R33 ↔ C27R27
- wire: C14R56 ↔ C4R47
- wire: C45R26 ↔ C42R8
- wire: C42R8 ↔ C43R4
- wire: C42R8 ↔ C37R10
- wire: C37R10 ↔ C18R10
- wire: C18R10 ↔ C19R11
- wire: C18R10 ↔ C14R12
- wire: C14R12 ↔ C14R18
- wire: C14R18 ↔ C9R18
- wire: C9R18 ↔ C6R20
- wire: C6R20 ↔ C3R20
- wire: C14R12 ↔ C9R9

## +5V_LOGIC  — THICK INSULATED WIRE, point to point
- pins: J1.+5V (C21 R52), U1.VIN (C27 R41), C1.+ (C21 R55), C2.1 (C25 R55), U9.Vin (C38 R10), C6.+ (C40 R8)
- wire: C21R52 ↔ C21R55
- wire: C21R55 ↔ C25R55
- wire: C25R55 ↔ C27R41
- wire: C27R41 ↔ C38R10
- wire: C38R10 ↔ C40R8

## +3V3_A  — THICK INSULATED WIRE, point to point
- pins: J1.+3V3 (C23 R52), U2.3.3V (C8 R18), U3.3V3 (C13 R56), U3.WAKE (C10 R55), U4.VCC (C34 R56), U5.VIN (C31 R36), U5.SDO (C35 R36), U5.CS (C37 R36), U6.VIN (C40 R45), U7.3V3 (C14 R9), R1.2 (C17 R13), R2.2 (C17 R7), J6.3V3 (C45 R34), C3.1 (C6 R18), C4.1 (C31 R33), C5.1 (C40 R42), D3.A (C4 R51)
- wire: C23R52 ↔ C13R56
- wire: C13R56 ↔ C10R55
- wire: C10R55 ↔ C4R51
- wire: C23R52 ↔ C34R56
- wire: C34R56 ↔ C40R45
- wire: C40R45 ↔ C40R42
- wire: C40R42 ↔ C37R36
- wire: C37R36 ↔ C35R36
- wire: C35R36 ↔ C31R36
- wire: C31R36 ↔ C31R33
- wire: C37R36 ↔ C45R34
- wire: C31R33 ↔ C17R13
- wire: C17R13 ↔ C17R7
- wire: C17R7 ↔ C14R9
- wire: C17R13 ↔ C8R18
- wire: C8R18 ↔ C6R18

## GNSS_VBCKP
- pins: U3.BAT (C10 R56), D3.K (C4 R55), D4.K (C2 R55)
- bottom: C10R58→C4R58 · C4R58→C4R55
- bottom: C4R55→C2R55
- insulated HOP wire (solder side, over the other trace): C10R56 ↔ C10R58

## GNSS_CELL
- pins: J7.CELL+ (C3 R47), D4.A (C2 R51)
- bottom: C3R47→C3R51 · C3R51→C2R51

## ICM_1V8
- pins: U6.1V8 (C41 R45), U6.CS (C40 R50), U6.SDO (C41 R50)
- bottom: C41R45→C41R50
- bottom: C41R50→C40R50

## SDA
- pins: U1.18 (C27 R33), U4.SDA (C33 R56), U5.SDI (C36 R36), U6.SDA (C44 R45), J2.SDA (C27 R52)
- bottom: C27R33→C30R33 · C30R33→C30R34 · C30R34→C36R34 · C36R34→C36R36
- bottom: C36R35→C40R35 · C40R35→C40R41 · C40R41→C44R41 · C44R41→C44R45
- bottom: C27R33→C26R33 · C26R33→C26R47 · C26R47→C27R47 · C27R47→C27R52
- bottom: C27R52→C27R54 · C27R54→C33R54 · C33R54→C33R56

## SCL
- pins: U1.19 (C27 R34), U4.SCL (C32 R56), U5.SCK (C34 R36), U6.SCL (C43 R45), J2.SCL (C26 R52)
- bottom: C27R34→C29R34 · C29R34→C29R35 · C29R35→C34R35 · C34R35→C34R36
- bottom: C34R36→C34R37 · C34R37→C37R37 · C37R37→C37R44 · C37R44→C43R44 · C43R44→C43R45
- bottom: C37R44→C37R57 · C37R57→C32R57 · C32R57→C32R56
- bottom: C32R56→C26R56 · C26R56→C26R52

## GNSS_TX
- pins: U3.TX (C11 R56), U1.7 (C21 R33), R5.1 (C10 R37), R6.1 (C10 R35)
- bottom: C11R56→C11R57 · C11R57→C7R57 · C7R57→C7R41
- bottom: C7R39→C7R37 · C7R37→C10R37
- bottom: C10R37→C10R35
- bottom: C10R36→C18R36 · C18R36→C18R33
- bottom: C20R33→C21R33
- insulated HOP wire (solder side, over the other trace): C7R41 ↔ C7R39
- insulated HOP wire (solder side, over the other trace): C18R33 ↔ C20R33

## GNSS_RX
- pins: U3.RX (C12 R56), U1.8 (C21 R32)
- bottom: C12R56→C12R57 · C12R57→C15R57 · C15R57→C15R55
- bottom: C15R53→C15R47
- bottom: C15R45→C15R42
- bottom: C15R40→C15R37
- bottom: C15R35→C15R32 · C15R32→C21R32
- insulated HOP wire (solder side, over the other trace): C15R55 ↔ C15R53
- insulated HOP wire (solder side, over the other trace): C15R47 ↔ C15R45
- insulated HOP wire (solder side, over the other trace): C15R42 ↔ C15R40
- insulated HOP wire (solder side, over the other trace): C15R37 ↔ C15R35

## GNSS_RST
- pins: U3.RST (C9 R56), U1.6 (C21 R34)
- bottom: C9R56→C8R56 · C8R56→C8R46 · C8R46→C13R46
- bottom: C15R46→C18R46 · C18R46→C18R42
- bottom: C18R40→C18R38 · C18R38→C20R38 · C20R38→C20R34 · C20R34→C21R34
- insulated HOP wire (solder side, over the other trace): C13R46 ↔ C15R46
- insulated HOP wire (solder side, over the other trace): C18R42 ↔ C18R40

## LORA_CS
- pins: U2.CS (C13 R12), U1.29 (C21 R21), R1.1 (C17 R9)
- bottom: C13R12→C13R10 · C13R10→C17R10 · C17R10→C17R9
- bottom: C17R9→C21R9 · C21R9→C21R17 · C21R17→C22R17 · C22R17→C22R21 · C22R21→C21R21

## LORA_BUSY
- pins: U2.BUSY (C8 R12), U1.24 (C21 R26)
- bottom: C8R12→C7R12 · C7R12→C7R20 · C7R20→C15R20 · C15R20→C15R26 · C15R26→C21R26

## LORA_RST
- pins: U2.RESET (C9 R12), U1.25 (C21 R25)
- bottom: C9R12→C9R17 · C9R17→C8R17
- bottom: C6R17→C5R17 · C5R17→C5R21 · C5R21→C14R21 · C14R21→C14R25
- bottom: C16R25→C21R25
- insulated HOP wire (solder side, over the other trace): C8R17 ↔ C6R17
- insulated HOP wire (solder side, over the other trace): C14R25 ↔ C16R25

## LORA_IRQ
- pins: U2.DIO9 (C13 R18), U1.30 (C21 R20)
- bottom: C13R18→C13R19 · C13R19→C15R19
- bottom: C17R19→C18R19 · C18R19→C18R20 · C18R20→C19R20
- insulated HOP wire (solder side, over the other trace): C15R19 ↔ C17R19
- insulated HOP wire (solder side, over the other trace): C19R20 ↔ C21R20

## MOSI1
- pins: U1.26 (C21 R24), U2.MOSI (C11 R12), U7.MOSI (C10 R9)
- bottom: C21R24→C16R24 · C16R24→C16R17 · C16R17→C11R17 · C11R17→C11R12
- bottom: C11R10→C10R10 · C10R10→C10R9
- insulated HOP wire (solder side, over the other trace): C11R12 ↔ C11R10

## SCK1
- pins: U1.27 (C21 R23), U2.CLK (C12 R12), U7.SCK (C11 R9)
- bottom: C21R23→C20R23 · C20R23→C20R16 · C20R16→C12R16 · C12R16→C12R12
- bottom: C12R12→C12R11 · C12R11→C11R11
- insulated HOP wire (solder side, over the other trace): C11R11 ↔ C11R9

## MISO1
- pins: U1.39 (C27 R24), U2.MISO (C10 R12), U7.MISO (C12 R9)
- bottom: C27R24→C39R24 · C39R24→C39R5 · C39R5→C40R5 · C40R5→C40R2 · C40R2→C8R2 · C8R2→C8R11 · C8R11→C10R11 · C10R11→C10R12
- bottom: C8R8→C12R8 · C12R8→C12R9

## FLASH_CS
- pins: U1.9 (C21 R31), U7.CS (C13 R9), R2.1 (C17 R3)
- bottom: C21R31→C22R31 · C22R31→C22R22 · C22R22→C23R22 · C23R22→C23R8 · C23R8→C13R8 · C13R8→C13R9
- bottom: C18R8→C18R3 · C18R3→C17R3

## HB_OUT
- pins: U1.5 (C21 R35), R3.1 (C10 R33)
- bottom: C19R35→C19R33
- bottom: C19R31→C19R27 · C19R27→C15R27
- bottom: C13R27→C9R27
- bottom: C9R29→C9R33 · C9R33→C10R33
- insulated HOP wire (solder side, over the other trace): C21R35 ↔ C19R35
- insulated HOP wire (solder side, over the other trace): C19R33 ↔ C19R31
- insulated HOP wire (solder side, over the other trace): C15R27 ↔ C13R27
- insulated HOP wire (solder side, over the other trace): C9R27 ↔ C9R29

## HB_J5
- pins: R3.2 (C14 R33), J5.HB (C3 R24)
- bottom: C14R33→C13R33 · C13R33→C13R28 · C13R28→C5R28
- bottom: C3R28→C3R24
- insulated HOP wire (solder side, over the other trace): C5R28 ↔ C3R28

## CAM_TX
- pins: U1.1 (C21 R39), R11.1 (C10 R31)
- bottom: C21R39→C19R39
- bottom: C17R39→C16R39 · C16R39→C16R37
- bottom: C16R35→C16R34
- bottom: C14R34→C12R34 · C12R34→C12R31 · C12R31→C10R31
- insulated HOP wire (solder side, over the other trace): C19R39 ↔ C17R39
- insulated HOP wire (solder side, over the other trace): C16R37 ↔ C16R35
- insulated HOP wire (solder side, over the other trace): C16R34 ↔ C14R34

## CAM_TX_J
- pins: R11.2 (C14 R31), J5.MUX_TX (C3 R22)
- bottom: C14R31→C14R26 · C14R26→C5R26 · C5R26→C5R22 · C5R22→C3R22

## CAM_RX
- pins: J5.MUX_RX (C3 R21), U1.0 (C21 R40)
- bottom: C3R21→C2R21 · C2R21→C2R40 · C2R40→C13R40 · C13R40→C13R41 · C13R41→C20R41 · C20R41→C20R40 · C20R40→C21R40

## AMP_DIN
- pins: U1.2 (C21 R38), U9.DIN (C34 R10)
- bottom: C21R38→C24R38 · C24R38→C24R24
- bottom: C24R22→C24R9 · C24R9→C34R9 · C34R9→C34R10
- insulated HOP wire (solder side, over the other trace): C24R24 ↔ C24R22

## AMP_LRC
- pins: U1.3 (C21 R37), U9.LRC (C32 R10)
- bottom: C21R37→C23R37 · C23R37→C23R35
- bottom: C23R33→C23R23 · C23R23→C24R23
- bottom: C26R23→C26R18
- bottom: C26R16→C26R10 · C26R10→C32R10
- insulated HOP wire (solder side, over the other trace): C23R35 ↔ C23R33
- insulated HOP wire (solder side, over the other trace): C24R23 ↔ C26R23
- insulated HOP wire (solder side, over the other trace): C26R18 ↔ C26R16

## AMP_BCLK
- pins: U1.4 (C21 R36), U9.BCLK (C33 R10)
- bottom: C21R36→C22R36 · C22R36→C22R34 · C22R34→C23R34
- bottom: C25R34→C25R17 · C25R17→C33R17 · C33R17→C33R10
- insulated HOP wire (solder side, over the other trace): C23R34 ↔ C25R34

## AMP_SD
- pins: U1.37 (C27 R22), U9.SD (C36 R10)
- bottom: C27R22→C36R22 · C36R22→C36R10

## AMP_GAIN
- pins: U9.GAIN (C35 R10), R12.1 (C39 R4)
- bottom: C35R10→C35R4 · C35R4→C39R4

## TX3
- pins: U1.14 (C27 R29), R4.1 (C39 R29)
- bottom: C27R29→C39R29

## TX3_J4
- pins: R4.2 (C43 R29), J4.TX3 (C45 R27)
- bottom: C43R29→C43R27 · C43R27→C45R27

## RX3
- pins: U1.15 (C27 R30), J4.RX3 (C45 R28)
- bottom: C27R30→C44R30 · C44R30→C44R28 · C44R28→C45R28

## GNSS_D
- pins: R5.2 (C14 R37), J8.GNSS (C45 R31)
- bottom: C14R37→C14R40
- bottom: C14R42→C14R54 · C14R54→C24R54 · C24R54→C24R58 · C24R58→C46R58 · C46R58→C46R31 · C46R31→C45R31
- insulated HOP wire (solder side, over the other trace): C14R40 ↔ C14R42

## GNSS_B
- pins: R6.2 (C14 R35), J5.GNSS (C3 R23)
- bottom: C14R35→C11R35 · C11R35→C11R34 · C11R34→C4R34 · C4R34→C4R23 · C4R23→C3R23

## VBAT_SENSE
- pins: J3.VBAT (C43 R57), R7.1 (C39 R54)
- bottom: C43R57→C39R57 · C39R57→C39R54

## BUS_V
- pins: R7.2 (C43 R54), R8.1 (C39 R52), U1.20 (C27 R35)
- bottom: C43R54→C40R54 · C40R54→C40R52 · C40R52→C39R52
- bottom: C39R52→C39R49 · C39R49→C38R49
- bottom: C36R49→C30R49 · C30R49→C30R36 · C30R36→C28R36 · C28R36→C28R35 · C28R35→C27R35
- insulated HOP wire (solder side, over the other trace): C38R49 ↔ C36R49

## SDA1
- pins: U1.17 (C27 R32), J6.SDA1 (C45 R36)
- bottom: C27R32→C42R32 · C42R32→C42R36 · C42R36→C45R36

## SCL1
- pins: U1.16 (C27 R31), J6.SCL1 (C45 R35)
- bottom: C27R31→C44R31 · C44R31→C44R35 · C44R35→C45R35

## ENV_UV
- pins: U1.22 (C27 R37), J9.UV (C45 R39)
- bottom: C27R37→C29R37
- bottom: C31R37→C33R37 · C33R37→C33R39 · C33R39→C36R39
- bottom: C38R39→C39R39
- bottom: C41R39→C45R39
- insulated HOP wire (solder side, over the other trace): C29R37 ↔ C31R37
- insulated HOP wire (solder side, over the other trace): C36R39 ↔ C38R39
- insulated HOP wire (solder side, over the other trace): C39R39 ↔ C41R39

## ENV_O3
- pins: U1.23 (C27 R38), J9.O3 (C45 R40)
- bottom: C27R38→C28R38 · C28R38→C28R40 · C28R40→C29R40
- bottom: C31R40→C36R40
- bottom: C38R40→C39R40
- bottom: C41R40→C45R40
- insulated HOP wire (solder side, over the other trace): C29R40 ↔ C31R40
- insulated HOP wire (solder side, over the other trace): C36R40 ↔ C38R40
- insulated HOP wire (solder side, over the other trace): C39R40 ↔ C41R40

## LED_OK
- pins: U1.31 (C21 R19), R9.1 (C19 R19)
- insulated HOP wire (solder side, over the other trace): C21R19 ↔ C19R19

## LED_OK_A
- pins: R9.2 (C19 R15), D1.A (C19 R12)
- bottom: C19R15→C19R12

## LED_FLT
- pins: U1.32 (C21 R18), R10.1 (C18 R18)
- bottom: C19R18→C18R18
- insulated HOP wire (solder side, over the other trace): C21R18 ↔ C19R18

## LED_FLT_A
- pins: R10.2 (C18 R14), D2.A (C18 R11)
- bottom: C18R14→C18R11
