# VRITRA zero boards — pin-by-pin connection tables

Generated from `boards.py` (the same data the routing sheets and checks use). Hole = column C (from the left on the component side) and row R (from the bottom). A resistor in the path is shown with BOTH legs: which hole each leg goes in and what that leg connects to. GND / 3V3 / 5V rails are insulated wires on the component side (see `<board>_top.png`); signals are solder-side traces (see `<board>_bottom.png`).

## Board 1 — MAIN (Teensy 4.1 #1)

### U1 — Teensy 4.1 PRIMARY  
*on female headers · USB up · microSD down*

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| 0 | C21 R40 | CAM_RX | J5 LINK cable to BACKUP J2 → pin **MUX_RX** (C3 R21) |
| 1 | C21 R39 | CAM_TX | through **R11 1k CAM TX**: leg 1 in C10 R31 (joined to this signal), leg 2 in C14 R31 → J5 LINK cable to BACKUP J2 → pin **MUX_TX** (C3 R22) |
| 2 | C21 R38 | AMP_DIN | U9 MAX98357A beacon amp → pin **DIN** (C34 R10) |
| 3 | C21 R37 | AMP_LRC | U9 MAX98357A beacon amp → pin **LRC** (C32 R10) |
| 4 | C21 R36 | AMP_BCLK | U9 MAX98357A beacon amp → pin **BCLK** (C33 R10) |
| 5 | C21 R35 | HB_OUT | through **R3 1k HB**: leg 1 in C10 R33 (joined to this signal), leg 2 in C14 R33 → J5 LINK cable to BACKUP J2 → pin **HB** (C3 R24) |
| 6 | C21 R34 | GNSS_RST | U3 L89HA GNSS → pin **RST** (C9 R56) |
| 7 | C21 R33 | GNSS_TX | U3 L89HA GNSS → pin **TX** (C11 R56) ; through **R5 1k GNSS→D**: leg 1 in C10 R37 (joined to this signal), leg 2 in C14 R37 → J8 GNSS to D J5 → pin **GNSS** (C45 R31) ; through **R6 1k GNSS→B**: leg 1 in C10 R35 (joined to this signal), leg 2 in C14 R35 → J5 LINK cable to BACKUP J2 → pin **GNSS** (C3 R23) |
| 8 | C21 R32 | GNSS_RX | U3 L89HA GNSS → pin **RX** (C12 R56) |
| 9 | C21 R31 | FLASH_CS | U7 W25Q64 flash → pin **CS** (C13 R9) ; through **R2 10k FLASH_CS↑**: leg 1 in C17 R3 (joined to this signal), leg 2 in C17 R7 → **+3V3_A rail** (insulated rail wire on the component side; rail enters at J1 pin 3) |
| 10 | C21 R30 | — | **not connected — leave open** |
| 11 | C21 R29 | — | **not connected — leave open** |
| 12 | C21 R28 | — | **not connected — leave open** |
| 13 | C27 R28 | — | **not connected — leave open** (on-board LED (firmware status LED)) |
| 14 | C27 R29 | TX3 | through **R4 1k TX3**: leg 1 in C39 R29 (joined to this signal), leg 2 in C43 R29 → J4 UART to D J2 → pin **TX3** (C45 R27) |
| 15 | C27 R30 | RX3 | J4 UART to D J2 → pin **RX3** (C45 R28) |
| 16 | C27 R31 | SCL1 | J6 I2C to C J1 → pin **SCL1** (C45 R35) |
| 17 | C27 R32 | SDA1 | J6 I2C to C J1 → pin **SDA1** (C45 R36) |
| 18 | C27 R33 | SDA | U4 DS3231 (MH ZS-042) → pin **SDA** (C33 R56) ; U5 BMP585 #1 → pin **SDI** (C36 R36) ; U6 ICM-20948 #1 → pin **SDA** (C44 R45) ; J2 I2C to E J2 → pin **SDA** (C27 R52) |
| 19 | C27 R34 | SCL | U4 DS3231 (MH ZS-042) → pin **SCL** (C32 R56) ; U5 BMP585 #1 → pin **SCK** (C34 R36) ; U6 ICM-20948 #1 → pin **SCL** (C43 R45) ; J2 I2C to E J2 → pin **SCL** (C26 R52) |
| 20 | C27 R35 | BUS_V | through **R7 100k VBAT**: leg 2 in C43 R54 (joined to this signal), leg 1 in C39 R54 → J3 VBAT from E J10 → pin **VBAT** (C43 R57) ; through **R8 22k VBAT**: leg 1 in C39 R52 (joined to this signal), leg 2 in C43 R52 → **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |
| 21 | C27 R36 | — | **not connected — leave open** |
| 22 | C27 R37 | ENV_UV | J9 analog to C J7 → pin **UV** (C45 R39) |
| 23 | C27 R38 | ENV_O3 | J9 analog to C J7 → pin **O3** (C45 R40) |
| 24 | C21 R26 | LORA_BUSY | U2 Core1121-HF → pin **BUSY** (C8 R12) |
| 25 | C21 R25 | LORA_RST | U2 Core1121-HF → pin **RESET** (C9 R12) |
| 26 | C21 R24 | MOSI1 | U2 Core1121-HF → pin **MOSI** (C11 R12) ; U7 W25Q64 flash → pin **MOSI** (C10 R9) |
| 27 | C21 R23 | SCK1 | U2 Core1121-HF → pin **CLK** (C12 R12) ; U7 W25Q64 flash → pin **SCK** (C11 R9) |
| 28 | C21 R22 | — | **not connected — leave open** |
| 29 | C21 R21 | LORA_CS | U2 Core1121-HF → pin **CS** (C13 R12) ; through **R1 10k LORA_CS↑**: leg 1 in C17 R9 (joined to this signal), leg 2 in C17 R13 → **+3V3_A rail** (insulated rail wire on the component side; rail enters at J1 pin 3) |
| 30 | C21 R20 | LORA_IRQ | U2 Core1121-HF → pin **DIO9** (C13 R18) |
| 31 | C21 R19 | LED_OK | through **R9 470 LED OK**: leg 1 in C19 R19 (joined to this signal), leg 2 in C19 R15 → through **D1 LED green**: leg A in C19 R12 (joined to this signal), leg K in C19 R11 → **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |
| 32 | C21 R18 | LED_FLT | through **R10 470 LED FLT**: leg 1 in C18 R18 (joined to this signal), leg 2 in C18 R14 → through **D2 LED red**: leg A in C18 R11 (joined to this signal), leg K in C18 R10 → **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |
| 33 | C27 R18 | — | **not connected — leave open** (firmware drives 33–36 (old motor driver) — keep free) |
| 34 | C27 R19 | — | **not connected — leave open** (firmware drives 33–36 — keep free) |
| 35 | C27 R20 | — | **not connected — leave open** (firmware drives 33–36 — keep free) |
| 36 | C27 R21 | — | **not connected — leave open** (firmware drives 33–36 — keep free) |
| 37 | C27 R22 | AMP_SD | U9 MAX98357A beacon amp → pin **SD** (C36 R10) |
| 38 | C27 R23 | — | **not connected — leave open** |
| 39 | C27 R24 | MISO1 | U2 Core1121-HF → pin **MISO** (C10 R12) ; U7 W25Q64 flash → pin **MISO** (C12 R9) |
| 40 | C27 R25 | — | **not connected — leave open** |
| 41 | C27 R26 | — | **not connected — leave open** |
| 3V3 | C27 R39 | — | **not connected — leave open** (Teensy's own 3.3 V output — do NOT join it to the 3V3_A rail) |
| 3V3 | C21 R27 | — | **not connected — leave open** (Teensy's own 3.3 V output — do NOT join it to the 3V3_A rail) |
| GND | C27 R40 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |
| GND | C21 R41 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |
| GND | C27 R27 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |
| VIN | C27 R41 | +5V_LOGIC | **+5V_LOGIC rail** (insulated rail wire on the component side; rail enters at J1 pin 1) |

### U2 — Core1121-HF  
*label side DOWN · u.FL → T-antenna*

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| 2.4G | C15 R12 | — | **not connected — leave open** (2.4 GHz not used (reserved band)) |
| GND | C14 R12 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |
| CS | C13 R12 | LORA_CS | U1 Teensy 4.1 PRIMARY → pin **29** (C21 R21) ; through **R1 10k LORA_CS↑**: leg 1 in C17 R9 (joined to this signal), leg 2 in C17 R13 → **+3V3_A rail** (insulated rail wire on the component side; rail enters at J1 pin 3) |
| CLK | C12 R12 | SCK1 | U1 Teensy 4.1 PRIMARY → pin **27** (C21 R23) ; U7 W25Q64 flash → pin **SCK** (C11 R9) |
| MOSI | C11 R12 | MOSI1 | U1 Teensy 4.1 PRIMARY → pin **26** (C21 R24) ; U7 W25Q64 flash → pin **MOSI** (C10 R9) |
| MISO | C10 R12 | MISO1 | U1 Teensy 4.1 PRIMARY → pin **39** (C27 R24) ; U7 W25Q64 flash → pin **MISO** (C12 R9) |
| RESET | C9 R12 | LORA_RST | U1 Teensy 4.1 PRIMARY → pin **25** (C21 R25) |
| BUSY | C8 R12 | LORA_BUSY | U1 Teensy 4.1 PRIMARY → pin **24** (C21 R26) |
| LORA_ANT | C15 R18 | — | **not connected — leave open** (antenna goes on the module's u.FL socket, not this pad) |
| GND | C14 R18 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |
| DIO9 | C13 R18 | LORA_IRQ | U1 Teensy 4.1 PRIMARY → pin **30** (C21 R20) |
| DIO8 | C12 R18 | — | **not connected — leave open** |
| DIO7 | C11 R18 | — | **not connected — leave open** |
| NC | C10 R18 | — | **not connected — leave open** |
| GND | C9 R18 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |
| 3.3V | C8 R18 | +3V3_A | **+3V3_A rail** (insulated rail wire on the component side; rail enters at J1 pin 3) |

### U3 — L89HA GNSS  
*7Semi board · built-in antennas · 9600 baud*

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| 3D_FIX | C9 R55 | — | **not connected — leave open** (would need pin 33, which the firmware drives) |
| WAKE | C10 R55 | +3V3_A | **+3V3_A rail** (insulated rail wire on the component side; rail enters at J1 pin 3) |
| SDA | C11 R55 | — | **not connected — leave open** (GNSS uses UART only) |
| SCL | C12 R55 | — | **not connected — leave open** (GNSS uses UART only) |
| GEO | C13 R55 | — | **not connected — leave open** |
| JAM | C14 R55 | — | **not connected — leave open** |
| RST | C9 R56 | GNSS_RST | U1 Teensy 4.1 PRIMARY → pin **6** (C21 R34) |
| BAT | C10 R56 | GNSS_VBCKP | through **D3 Schottky 3V3→BAT**: leg K in C4 R55 (joined to this signal), leg A in C4 R51 → **+3V3_A rail** (insulated rail wire on the component side; rail enters at J1 pin 3) ; through **D4 Schottky cell→BAT**: leg K in C2 R55 (joined to this signal), leg A in C2 R51 → J7 GNSS backup cell → pin **CELL+** (C3 R47) |
| TX | C11 R56 | GNSS_TX | U1 Teensy 4.1 PRIMARY → pin **7** (C21 R33) ; through **R5 1k GNSS→D**: leg 1 in C10 R37 (joined to this signal), leg 2 in C14 R37 → J8 GNSS to D J5 → pin **GNSS** (C45 R31) ; through **R6 1k GNSS→B**: leg 1 in C10 R35 (joined to this signal), leg 2 in C14 R35 → J5 LINK cable to BACKUP J2 → pin **GNSS** (C3 R23) |
| RX | C12 R56 | GNSS_RX | U1 Teensy 4.1 PRIMARY → pin **8** (C21 R32) |
| 3V3 | C13 R56 | +3V3_A | **+3V3_A rail** (insulated rail wire on the component side; rail enters at J1 pin 3) |
| GND | C14 R56 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |

### U4 — DS3231 (MH ZS-042)  
*flat on its 4-pin end · CR2032 up · REMOVE charge resistor · 0x68 + EEPROM 0x57*

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| SCL | C32 R56 | SCL | U1 Teensy 4.1 PRIMARY → pin **19** (C27 R34) ; U5 BMP585 #1 → pin **SCK** (C34 R36) ; U6 ICM-20948 #1 → pin **SCL** (C43 R45) ; J2 I2C to E J2 → pin **SCL** (C26 R52) |
| SDA | C33 R56 | SDA | U1 Teensy 4.1 PRIMARY → pin **18** (C27 R33) ; U5 BMP585 #1 → pin **SDI** (C36 R36) ; U6 ICM-20948 #1 → pin **SDA** (C44 R45) ; J2 I2C to E J2 → pin **SDA** (C27 R52) |
| VCC | C34 R56 | +3V3_A | **+3V3_A rail** (insulated rail wire on the component side; rail enters at J1 pin 3) |
| GND | C35 R56 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |

### U5 — BMP585 #1  
*0x47 (SDO→3V3)*

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| VIN | C31 R36 | +3V3_A | **+3V3_A rail** (insulated rail wire on the component side; rail enters at J1 pin 3) |
| 3V3 | C32 R36 | — | **not connected — leave open** (module's 3.3 V output — leave open) |
| GND | C33 R36 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |
| SCK | C34 R36 | SCL | U1 Teensy 4.1 PRIMARY → pin **19** (C27 R34) ; U4 DS3231 (MH ZS-042) → pin **SCL** (C32 R56) ; U6 ICM-20948 #1 → pin **SCL** (C43 R45) ; J2 I2C to E J2 → pin **SCL** (C26 R52) |
| SDO | C35 R36 | +3V3_A | **+3V3_A rail** (insulated rail wire on the component side; rail enters at J1 pin 3) |
| SDI | C36 R36 | SDA | U1 Teensy 4.1 PRIMARY → pin **18** (C27 R33) ; U4 DS3231 (MH ZS-042) → pin **SDA** (C33 R56) ; U6 ICM-20948 #1 → pin **SDA** (C44 R45) ; J2 I2C to E J2 → pin **SDA** (C27 R52) |
| CS | C37 R36 | +3V3_A | **+3V3_A rail** (insulated rail wire on the component side; rail enters at J1 pin 3) |
| INT | C38 R36 | — | **not connected — leave open** (firmware polls the baro) |

### U6 — ICM-20948 #1  
*CS+SDO → 1V8 · 0x69*

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| VIN | C40 R45 | +3V3_A | **+3V3_A rail** (insulated rail wire on the component side; rail enters at J1 pin 3) |
| 1V8 | C41 R45 | ICM_1V8 | join to this module's own pin(s) **CS** (C40 R50), **SDO** (C41 R50) |
| GND | C42 R45 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |
| SCL | C43 R45 | SCL | U1 Teensy 4.1 PRIMARY → pin **19** (C27 R34) ; U4 DS3231 (MH ZS-042) → pin **SCL** (C32 R56) ; U5 BMP585 #1 → pin **SCK** (C34 R36) ; J2 I2C to E J2 → pin **SCL** (C26 R52) |
| SDA | C44 R45 | SDA | U1 Teensy 4.1 PRIMARY → pin **18** (C27 R33) ; U4 DS3231 (MH ZS-042) → pin **SDA** (C33 R56) ; U5 BMP585 #1 → pin **SDI** (C36 R36) ; J2 I2C to E J2 → pin **SDA** (C27 R52) |
| INT | C45 R45 | — | **not connected — leave open** (firmware polls the IMU) |
| CS | C40 R50 | ICM_1V8 | join to this module's own pin(s) **1V8** (C41 R45), **SDO** (C41 R50) |
| SDO | C41 R50 | ICM_1V8 | join to this module's own pin(s) **1V8** (C41 R45), **CS** (C40 R50) |
| GND | C42 R50 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |
| ACL | C43 R50 | — | **not connected — leave open** |
| ADA | C44 R50 | — | **not connected — leave open** |
| FSYNC | C45 R50 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |

### U7 — W25Q64 flash  
*CS → pin 9*

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| 3V3 | C14 R9 | +3V3_A | **+3V3_A rail** (insulated rail wire on the component side; rail enters at J1 pin 3) |
| CS | C13 R9 | FLASH_CS | U1 Teensy 4.1 PRIMARY → pin **9** (C21 R31) ; through **R2 10k FLASH_CS↑**: leg 1 in C17 R3 (joined to this signal), leg 2 in C17 R7 → **+3V3_A rail** (insulated rail wire on the component side; rail enters at J1 pin 3) |
| MISO | C12 R9 | MISO1 | U1 Teensy 4.1 PRIMARY → pin **39** (C27 R24) ; U2 Core1121-HF → pin **MISO** (C10 R12) |
| SCK | C11 R9 | SCK1 | U1 Teensy 4.1 PRIMARY → pin **27** (C21 R23) ; U2 Core1121-HF → pin **CLK** (C12 R12) |
| MOSI | C10 R9 | MOSI1 | U1 Teensy 4.1 PRIMARY → pin **26** (C21 R24) ; U2 Core1121-HF → pin **MOSI** (C11 R12) |
| GND | C9 R9 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |

### U9 — MAX98357A beacon amp  
*speaker on the amp screw terminal · (+ and - only, neither is GND)*

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| LRC | C32 R10 | AMP_LRC | U1 Teensy 4.1 PRIMARY → pin **3** (C21 R37) |
| BCLK | C33 R10 | AMP_BCLK | U1 Teensy 4.1 PRIMARY → pin **4** (C21 R36) |
| DIN | C34 R10 | AMP_DIN | U1 Teensy 4.1 PRIMARY → pin **2** (C21 R38) |
| GAIN | C35 R10 | AMP_GAIN | through **R12 100k GAIN↓ 15dB**: leg 1 in C39 R4 (joined to this signal), leg 2 in C43 R4 → **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |
| SD | C36 R10 | AMP_SD | U1 Teensy 4.1 PRIMARY → pin **37** (C27 R22) |
| GND | C37 R10 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |
| Vin | C38 R10 | +5V_LOGIC | **+5V_LOGIC rail** (insulated rail wire on the component side; rail enters at J1 pin 1) |

### J9 — analog to C J7

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| O3 | C45 R40 | ENV_O3 | U1 Teensy 4.1 PRIMARY → pin **23** (C27 R38) |
| UV | C45 R39 | ENV_UV | U1 Teensy 4.1 PRIMARY → pin **22** (C27 R37) |
| GND | C45 R38 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |

### J6 — I2C to C J1

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| SDA1 | C45 R36 | SDA1 | U1 Teensy 4.1 PRIMARY → pin **17** (C27 R32) |
| SCL1 | C45 R35 | SCL1 | U1 Teensy 4.1 PRIMARY → pin **16** (C27 R31) |
| 3V3 | C45 R34 | +3V3_A | **+3V3_A rail** (insulated rail wire on the component side; rail enters at J1 pin 3) |
| GND | C45 R33 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |

### J8 — GNSS to D J5

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| GNSS | C45 R31 | GNSS_D | through **R5 1k GNSS→D**: leg 2 in C14 R37 (joined to this signal), leg 1 in C10 R37 → U3 L89HA GNSS → pin **TX** (C11 R56) ; U1 Teensy 4.1 PRIMARY → pin **7** (C21 R33) ; through **R6 1k GNSS→B**: leg 1 in C10 R35 (joined to this signal), leg 2 in C14 R35 → J5 LINK cable to BACKUP J2 → pin **GNSS** (C3 R23) |
| GND | C45 R30 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |

### J4 — UART to D J2

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| RX3 | C45 R28 | RX3 | U1 Teensy 4.1 PRIMARY → pin **15** (C27 R30) |
| TX3 | C45 R27 | TX3_J4 | through **R4 1k TX3**: leg 2 in C43 R29 (joined to this signal), leg 1 in C39 R29 → U1 Teensy 4.1 PRIMARY → pin **14** (C27 R29) |
| GND | C45 R26 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |

### J5 — LINK cable to BACKUP J2

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| HB | C3 R24 | HB_J5 | through **R3 1k HB**: leg 2 in C14 R33 (joined to this signal), leg 1 in C10 R33 → U1 Teensy 4.1 PRIMARY → pin **5** (C21 R35) |
| GNSS | C3 R23 | GNSS_B | through **R6 1k GNSS→B**: leg 2 in C14 R35 (joined to this signal), leg 1 in C10 R35 → U3 L89HA GNSS → pin **TX** (C11 R56) ; U1 Teensy 4.1 PRIMARY → pin **7** (C21 R33) ; through **R5 1k GNSS→D**: leg 1 in C10 R37 (joined to this signal), leg 2 in C14 R37 → J8 GNSS to D J5 → pin **GNSS** (C45 R31) |
| MUX_TX | C3 R22 | CAM_TX_J | through **R11 1k CAM TX**: leg 2 in C14 R31 (joined to this signal), leg 1 in C10 R31 → U1 Teensy 4.1 PRIMARY → pin **1** (C21 R39) |
| MUX_RX | C3 R21 | CAM_RX | U1 Teensy 4.1 PRIMARY → pin **0** (C21 R40) |
| GND | C3 R20 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |

### J2 — I2C to E J2

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| SCL | C26 R52 | SCL | U1 Teensy 4.1 PRIMARY → pin **19** (C27 R34) ; U4 DS3231 (MH ZS-042) → pin **SCL** (C32 R56) ; U5 BMP585 #1 → pin **SCK** (C34 R36) ; U6 ICM-20948 #1 → pin **SCL** (C43 R45) |
| SDA | C27 R52 | SDA | U1 Teensy 4.1 PRIMARY → pin **18** (C27 R33) ; U4 DS3231 (MH ZS-042) → pin **SDA** (C33 R56) ; U5 BMP585 #1 → pin **SDI** (C36 R36) ; U6 ICM-20948 #1 → pin **SDA** (C44 R45) |
| GND | C28 R52 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |

### J3 — VBAT from E J10

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| VBAT | C43 R57 | VBAT_SENSE | through **R7 100k VBAT**: leg 1 in C39 R54 (joined to this signal), leg 2 in C43 R54 → through **R8 22k VBAT**: leg 1 in C39 R52 (joined to this signal), leg 2 in C43 R52 → **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) ; U1 Teensy 4.1 PRIMARY → pin **20** (C27 R35) |
| GND | C44 R57 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |

### J1 — PWR from E J5

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| +5V | C21 R52 | +5V_LOGIC | **+5V_LOGIC rail** (insulated rail wire on the component side; rail enters at the power-module landing holes) |
| GND | C22 R52 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J3 pin 2) |
| +3V3 | C23 R52 | +3V3_A | **+3V3_A rail** (insulated rail wire on the component side; rail enters at the power-module landing holes) |
| GND | C24 R52 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J3 pin 2) |

### J7 — GNSS backup cell

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| CELL+ | C3 R47 | GNSS_CELL | through **D4 Schottky cell→BAT**: leg A in C2 R51 (joined to this signal), leg K in C2 R55 → U3 L89HA GNSS → pin **BAT** (C10 R56) ; through **D3 Schottky 3V3→BAT**: leg K in C4 R55 (joined to this signal), leg A in C4 R51 → **+3V3_A rail** (insulated rail wire on the component side; rail enters at J1 pin 3) |
| GND | C4 R47 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |

### Resistors, diodes, LEDs and capacitors on this board

| part | value / role | leg 1 → hole → signal | leg 2 → hole → signal |
|---|---|---|---|
| C1 | 470µF | + → C21 R55 → +5V_LOGIC | - → C23 R55 → GND |
| C2 | 100nF | 1 → C25 R55 → +5V_LOGIC | 2 → C27 R55 → GND |
| C3 | 100nF | 1 → C6 R18 → +3V3_A | 2 → C6 R20 → GND |
| C4 | 100nF | 1 → C31 R33 → +3V3_A | 2 → C33 R33 → GND |
| C5 | 100nF | 1 → C40 R42 → +3V3_A | 2 → C42 R42 → GND |
| C6 | 100µF amp | + → C40 R8 → +5V_LOGIC | - → C42 R8 → GND |
| D1 | LED green | A → C19 R12 → LED_OK_A | K → C19 R11 → GND |
| D2 | LED red | A → C18 R11 → LED_FLT_A | K → C18 R10 → GND |
| D3 | Schottky 3V3→BAT | A → C4 R51 → +3V3_A | K → C4 R55 → GNSS_VBCKP |
| D4 | Schottky cell→BAT | A → C2 R51 → GNSS_CELL | K → C2 R55 → GNSS_VBCKP |
| R1 | 10k LORA_CS↑ | 1 → C17 R9 → LORA_CS | 2 → C17 R13 → +3V3_A |
| R2 | 10k FLASH_CS↑ | 1 → C17 R3 → FLASH_CS | 2 → C17 R7 → +3V3_A |
| R3 | 1k HB | 1 → C10 R33 → HB_OUT | 2 → C14 R33 → HB_J5 |
| R4 | 1k TX3 | 1 → C39 R29 → TX3 | 2 → C43 R29 → TX3_J4 |
| R5 | 1k GNSS→D | 1 → C10 R37 → GNSS_TX | 2 → C14 R37 → GNSS_D |
| R6 | 1k GNSS→B | 1 → C10 R35 → GNSS_TX | 2 → C14 R35 → GNSS_B |
| R7 | 100k VBAT | 1 → C39 R54 → VBAT_SENSE | 2 → C43 R54 → BUS_V |
| R8 | 22k VBAT | 1 → C39 R52 → BUS_V | 2 → C43 R52 → GND |
| R9 | 470 LED OK | 1 → C19 R19 → LED_OK | 2 → C19 R15 → LED_OK_A |
| R10 | 470 LED FLT | 1 → C18 R18 → LED_FLT | 2 → C18 R14 → LED_FLT_A |
| R11 | 1k CAM TX | 1 → C10 R31 → CAM_TX | 2 → C14 R31 → CAM_TX_J |
| R12 | 100k GAIN↓ 15dB | 1 → C39 R4 → AMP_GAIN | 2 → C43 R4 → GND |

Polarity: LED / diode leg **A** = anode (long leg / no band), **K** = cathode (short leg / band). Electrolytic **+** = long leg, **−** = stripe side.

## Board 2 — BACKUP + SENSOR (Teensy 4.1 #2)

### U1 — Teensy 4.1 BACKUP  
*on female headers · USB up · microSD down*

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| 0 | C21 R40 | GNSS_IN | J2 LINK cable from MAIN J5 → pin **GNSS** (C3 R40) |
| 1 | C21 R39 | — | **not connected — leave open** |
| 2 | C21 R38 | — | **not connected — leave open** |
| 3 | C21 R37 | — | **not connected — leave open** |
| 4 | C21 R36 | HB_IN | J2 LINK cable from MAIN J5 → pin **HB** (C3 R41) |
| 5 | C21 R35 | MUX_A | U10 7Semi UART mux → pin **A** (C29 R7) ; through **R6 10k MUX_A↓**: leg 1 in C14 R30 (joined to this signal), leg 2 in C18 R30 → **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |
| 6 | C21 R34 | MUX_B | U10 7Semi UART mux → pin **B** (C29 R6) ; through **R7 10k MUX_B↓**: leg 1 in C14 R28 (joined to this signal), leg 2 in C18 R28 → **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |
| 7 | C21 R33 | — | **not connected — leave open** |
| 8 | C21 R32 | — | **not connected — leave open** |
| 9 | C21 R31 | MUX_C | U10 7Semi UART mux → pin **C** (C36 R7) ; through **R8 10k MUX_C↓**: leg 1 in C14 R26 (joined to this signal), leg 2 in C18 R26 → **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |
| 10 | C21 R30 | SD_CS | U11 microSD card module → pin **CS** (C7 R41) |
| 11 | C21 R29 | SD_MOSI | U11 microSD card module → pin **MOSI** (C9 R41) |
| 12 | C21 R28 | SD_MISO | U11 microSD card module → pin **MISO** (C10 R41) |
| 13 | C27 R28 | SD_SCK | U11 microSD card module → pin **SCK** (C8 R41) |
| 14 | C27 R29 | TX3 | through **R1 1k TX3**: leg 1 in C38 R29 (joined to this signal), leg 2 in C42 R29 → J3 to D J3 → pin **TX3** (C45 R30) |
| 15 | C27 R30 | RX3 | J3 to D J3 → pin **RX3** (C45 R31) |
| 16 | C27 R31 | — | **not connected — leave open** |
| 17 | C27 R32 | — | **not connected — leave open** |
| 18 | C27 R33 | SDA | U5 BMP585 #2 → pin **SDI** (C36 R36) ; U8 LSM6DSO32 (Adafruit) → pin **SDA** (C35 R44) ; M1L HDC3020 cable wires → pin **SDA** (C43 R50) |
| 19 | C27 R34 | SCL | U5 BMP585 #2 → pin **SCK** (C34 R36) ; U8 LSM6DSO32 (Adafruit) → pin **SCL** (C34 R44) ; M1L HDC3020 cable wires → pin **SCL** (C44 R50) |
| 20 | C27 R35 | — | **not connected — leave open** |
| 21 | C27 R36 | — | **not connected — leave open** |
| 22 | C27 R37 | — | **not connected — leave open** |
| 23 | C27 R38 | — | **not connected — leave open** |
| 24 | C21 R26 | — | **not connected — leave open** |
| 25 | C21 R25 | — | **not connected — leave open** |
| 26 | C21 R24 | — | **not connected — leave open** |
| 27 | C21 R23 | — | **not connected — leave open** |
| 28 | C21 R22 | — | **not connected — leave open** |
| 29 | C21 R21 | — | **not connected — leave open** |
| 30 | C21 R20 | — | **not connected — leave open** |
| 31 | C21 R19 | — | **not connected — leave open** |
| 32 | C21 R18 | — | **not connected — leave open** |
| 33 | C27 R18 | — | **not connected — leave open** |
| 34 | C27 R19 | RX8 | through **R3 1k RX8**: leg 1 in C29 R18 (joined to this signal), leg 2 in C33 R18 → U10 7Semi UART mux → pin **RX2** (C29 R9) |
| 35 | C27 R20 | TX8 | through **R2 1k TX8**: leg 1 in C29 R20 (joined to this signal), leg 2 in C33 R20 → U10 7Semi UART mux → pin **TX2** (C29 R10) |
| 36 | C27 R21 | LED_OK | through **R4 470 LED OK**: leg 1 in C29 R23 (joined to this signal), leg 2 in C33 R23 → through **D1 LED green**: leg A in C35 R23 (joined to this signal), leg K in C35 R22 → **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |
| 37 | C27 R22 | LED_FLT | through **R5 470 LED FLT**: leg 1 in C29 R25 (joined to this signal), leg 2 in C33 R25 → through **D2 LED red**: leg A in C35 R26 (joined to this signal), leg K in C35 R25 → **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |
| 38 | C27 R23 | — | **not connected — leave open** |
| 39 | C27 R24 | — | **not connected — leave open** |
| 40 | C27 R25 | — | **not connected — leave open** |
| 41 | C27 R26 | — | **not connected — leave open** |
| 3V3 | C27 R39 | — | **not connected — leave open** (Teensy's own 3.3 V output — do NOT join it to the 3V3_B rail) |
| 3V3 | C21 R27 | — | **not connected — leave open** (Teensy's own 3.3 V output — do NOT join it to the 3V3_B rail) |
| GND | C27 R40 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |
| GND | C21 R41 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |
| GND | C27 R27 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |
| VIN | C27 R41 | +5V_LOGIC | **+5V_LOGIC rail** (insulated rail wire on the component side; rail enters at J1 pin 1) |

### U5 — BMP585 #2  
*0x47 (SDO→3V3)*

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| VIN | C31 R36 | +3V3_B | **+3V3_B rail** (insulated rail wire on the component side; rail enters at J1 pin 3) |
| 3V3 | C32 R36 | — | **not connected — leave open** (module's 3.3 V output — leave open) |
| GND | C33 R36 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |
| SCK | C34 R36 | SCL | U1 Teensy 4.1 BACKUP → pin **19** (C27 R34) ; U8 LSM6DSO32 (Adafruit) → pin **SCL** (C34 R44) ; M1L HDC3020 cable wires → pin **SCL** (C44 R50) |
| SDO | C35 R36 | +3V3_B | **+3V3_B rail** (insulated rail wire on the component side; rail enters at J1 pin 3) |
| SDI | C36 R36 | SDA | U1 Teensy 4.1 BACKUP → pin **18** (C27 R33) ; U8 LSM6DSO32 (Adafruit) → pin **SDA** (C35 R44) ; M1L HDC3020 cable wires → pin **SDA** (C43 R50) |
| CS | C37 R36 | +3V3_B | **+3V3_B rail** (insulated rail wire on the component side; rail enters at J1 pin 3) |
| INT | C38 R36 | — | **not connected — leave open** |

### U8 — LSM6DSO32 (Adafruit)  
*I²C mode: CS→3V3, DO→GND = 0x6A · ±32 g · solder only the 9-pin FRONT row*

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| VIN | C31 R44 | +3V3_B | **+3V3_B rail** (insulated rail wire on the component side; rail enters at J1 pin 3) |
| 3Vo | C32 R44 | — | **not connected — leave open** (module's 3.3 V output — leave open) |
| GND | C33 R44 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |
| SCL | C34 R44 | SCL | U1 Teensy 4.1 BACKUP → pin **19** (C27 R34) ; U5 BMP585 #2 → pin **SCK** (C34 R36) ; M1L HDC3020 cable wires → pin **SCL** (C44 R50) |
| SDA | C35 R44 | SDA | U1 Teensy 4.1 BACKUP → pin **18** (C27 R33) ; U5 BMP585 #2 → pin **SDI** (C36 R36) ; M1L HDC3020 cable wires → pin **SDA** (C43 R50) |
| DO | C36 R44 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |
| CS | C37 R44 | +3V3_B | **+3V3_B rail** (insulated rail wire on the component side; rail enters at J1 pin 3) |
| INT1 | C38 R44 | — | **not connected — leave open** (not needed for polling) |
| INT2 | C39 R44 | — | **not connected — leave open** (not needed for polling) |
| SCX | C33 R49 | — | **not connected — leave open** |
| SDX | C34 R49 | — | **not connected — leave open** |
| CS(back) | C35 R49 | — | **not connected — leave open** |
| DO(back) | C36 R49 | — | **not connected — leave open** |
| GND(back) | C37 R49 | — | **not connected — leave open** |

### U10 — 7Semi UART mux  
*ch1 = MAIN · ch2 = BACKUP · DIP OFF · VCC jumper 3V3*

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| VCC_L1 | C29 R15 | — | **not connected — leave open** |
| TX1 | C29 R14 | CAM_TX_J | J2 LINK cable from MAIN J5 → pin **MUX_TX** (C3 R39) |
| RX1 | C29 R13 | CAM_RX | J2 LINK cable from MAIN J5 → pin **MUX_RX** (C3 R38) |
| GND | C29 R12 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |
| VCC_L2 | C29 R11 | — | **not connected — leave open** |
| TX2 | C29 R10 | TX8_J | through **R2 1k TX8**: leg 2 in C33 R20 (joined to this signal), leg 1 in C29 R20 → U1 Teensy 4.1 BACKUP → pin **35** (C27 R20) |
| RX2 | C29 R9 | RX8_J | through **R3 1k RX8**: leg 2 in C33 R18 (joined to this signal), leg 1 in C29 R18 → U1 Teensy 4.1 BACKUP → pin **34** (C27 R19) |
| GND | C29 R8 | — | **not connected — leave open** |
| A | C29 R7 | MUX_A | U1 Teensy 4.1 BACKUP → pin **5** (C21 R35) ; through **R6 10k MUX_A↓**: leg 1 in C14 R30 (joined to this signal), leg 2 in C18 R30 → **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |
| B | C29 R6 | MUX_B | U1 Teensy 4.1 BACKUP → pin **6** (C21 R34) ; through **R7 10k MUX_B↓**: leg 1 in C14 R28 (joined to this signal), leg 2 in C18 R28 → **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |
| VCC_R1 | C36 R15 | — | **not connected — leave open** |
| TX3 | C36 R14 | — | **not connected — leave open** |
| RX3 | C36 R13 | — | **not connected — leave open** |
| GND | C36 R12 | — | **not connected — leave open** |
| VCC_R2 | C36 R11 | — | **not connected — leave open** |
| TX4 | C36 R10 | — | **not connected — leave open** |
| RX4 | C36 R9 | — | **not connected — leave open** |
| GND | C36 R8 | — | **not connected — leave open** |
| C | C36 R7 | MUX_C | U1 Teensy 4.1 BACKUP → pin **9** (C21 R31) ; through **R8 10k MUX_C↓**: leg 1 in C14 R26 (joined to this signal), leg 2 in C18 R26 → **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |
| GND | C36 R6 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |

### U11 — microSD card module  
*straight header, module FLAT · card slot → top edge · VCC = 5 V*

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| CS | C7 R41 | SD_CS | U1 Teensy 4.1 BACKUP → pin **10** (C21 R30) |
| SCK | C8 R41 | SD_SCK | U1 Teensy 4.1 BACKUP → pin **13** (C27 R28) |
| MOSI | C9 R41 | SD_MOSI | U1 Teensy 4.1 BACKUP → pin **11** (C21 R29) |
| MISO | C10 R41 | SD_MISO | U1 Teensy 4.1 BACKUP → pin **12** (C21 R28) |
| VCC | C11 R41 | +5V_LOGIC | **+5V_LOGIC rail** (insulated rail wire on the component side; rail enters at J1 pin 1) |
| GND | C12 R41 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |

### J1 — PWR from E J6

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| +5V | C21 R52 | +5V_LOGIC | **+5V_LOGIC rail** (insulated rail wire on the component side; rail enters at the power-module landing holes) |
| GND | C22 R52 | GND | **GND rail** (insulated rail wire on the component side; rail enters at the power-module landing holes) |
| +3V3 | C23 R52 | +3V3_B | **+3V3_B rail** (insulated rail wire on the component side; rail enters at the power-module landing holes) |
| GND | C24 R52 | GND | **GND rail** (insulated rail wire on the component side; rail enters at the power-module landing holes) |

### J2 — LINK cable from MAIN J5

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| HB | C3 R41 | HB_IN | U1 Teensy 4.1 BACKUP → pin **4** (C21 R36) |
| GNSS | C3 R40 | GNSS_IN | U1 Teensy 4.1 BACKUP → pin **0** (C21 R40) |
| MUX_TX | C3 R39 | CAM_TX_J | U10 7Semi UART mux → pin **TX1** (C29 R14) |
| MUX_RX | C3 R38 | CAM_RX | U10 7Semi UART mux → pin **RX1** (C29 R13) |
| GND | C3 R37 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |

### J3 — to D J3

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| RX3 | C45 R31 | RX3 | U1 Teensy 4.1 BACKUP → pin **15** (C27 R30) |
| TX3 | C45 R30 | TX3_J | through **R1 1k TX3**: leg 2 in C42 R29 (joined to this signal), leg 1 in C38 R29 → U1 Teensy 4.1 BACKUP → pin **14** (C27 R29) |
| GND | C45 R29 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |

### J4 — CAMERA plug (the only one)

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| +5V | C45 R22 | +5V_LOGIC | **+5V_LOGIC rail** (insulated rail wire on the component side; rail enters at J1 pin 1) |
| GND | C45 R21 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |
| to D7 | C45 R20 | CAM_TO_XIAO | through **R9 1k cam RX**: leg 2 in C42 R17 (joined to this signal), leg 1 in C38 R17 → U10B mux bottom-row wires → pin **TX_COM** (C34 R3) |
| from D6 | C45 R19 | CAM_FROM_XIAO | through **R10 1k cam TX**: leg 2 in C42 R15 (joined to this signal), leg 1 in C38 R15 → U10B mux bottom-row wires → pin **RX_COM** (C33 R3) |

### M1L — HDC3020 cable wires

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| 3V3 | C41 R50 | +3V3_B | **+3V3_B rail** (insulated rail wire on the component side; rail enters at J1 pin 3) |
| GND | C42 R50 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J1 pin 4) |
| SDA | C43 R50 | SDA | U1 Teensy 4.1 BACKUP → pin **18** (C27 R33) ; U5 BMP585 #2 → pin **SDI** (C36 R36) ; U8 LSM6DSO32 (Adafruit) → pin **SDA** (C35 R44) |
| SCL | C44 R50 | SCL | U1 Teensy 4.1 BACKUP → pin **19** (C27 R34) ; U5 BMP585 #2 → pin **SCK** (C34 R36) ; U8 LSM6DSO32 (Adafruit) → pin **SCL** (C34 R44) |

### Resistors, diodes, LEDs and capacitors on this board

| part | value / role | leg 1 → hole → signal | leg 2 → hole → signal |
|---|---|---|---|
| C1 | 470µF | + → C21 R55 → +5V_LOGIC | - → C23 R55 → GND |
| C2 | 100nF | 1 → C25 R55 → +5V_LOGIC | 2 → C27 R55 → GND |
| C3 | 100nF | 1 → C31 R33 → +3V3_B | 2 → C33 R33 → GND |
| C4 | 100nF | 1 → C42 R42 → +3V3_B | 2 → C44 R42 → GND |
| D1 | LED green | A → C35 R23 → LED_OK_A | K → C35 R22 → GND |
| D2 | LED red | A → C35 R26 → LED_FLT_A | K → C35 R25 → GND |
| R1 | 1k TX3 | 1 → C38 R29 → TX3 | 2 → C42 R29 → TX3_J |
| R2 | 1k TX8 | 1 → C29 R20 → TX8 | 2 → C33 R20 → TX8_J |
| R3 | 1k RX8 | 1 → C29 R18 → RX8 | 2 → C33 R18 → RX8_J |
| R4 | 470 LED OK | 1 → C29 R23 → LED_OK | 2 → C33 R23 → LED_OK_A |
| R5 | 470 LED FLT | 1 → C29 R25 → LED_FLT | 2 → C33 R25 → LED_FLT_A |
| R6 | 10k MUX_A↓ | 1 → C14 R30 → MUX_A | 2 → C18 R30 → GND |
| R7 | 10k MUX_B↓ | 1 → C14 R28 → MUX_B | 2 → C18 R28 → GND |
| R8 | 10k MUX_C↓ | 1 → C14 R26 → MUX_C | 2 → C18 R26 → GND |
| R9 | 1k cam RX | 1 → C38 R17 → CAM_COM_TX | 2 → C42 R17 → CAM_TO_XIAO |
| R10 | 1k cam TX | 1 → C38 R15 → CAM_COM_RX | 2 → C42 R15 → CAM_FROM_XIAO |

Polarity: LED / diode leg **A** = anode (long leg / no band), **K** = cathode (short leg / band). Electrolytic **+** = long leg, **−** = stripe side.

## Board 4 — PARACHUTE (Teensy 4.0, etched)

### U1 — Teensy 4.0 PARACHUTE  
*female headers · USB to the left edge*

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| 0 | C16 R23 | GNSS_IN | J5 GNSS from MAIN J8 → pin **GNSS** (C8 R19) |
| 1 | C17 R23 | — | **not connected — leave open** |
| 2 | C18 R23 | STEER_L | through **R1 330**: leg 1 in C13 R19 (joined to this signal), leg 2 in C13 R15 → J10 A20CLM LEFT steer (8 V) → pin **S** (C13 R14) |
| 3 | C19 R23 | STEER_R | through **R2 330**: leg 1 in C16 R19 (joined to this signal), leg 2 in C16 R15 → J11 A20CLM RIGHT steer (8 V) → pin **S** (C16 R14) |
| 4 | C20 R23 | BAY_LID | through **R3 330**: leg 1 in C19 R19 (joined to this signal), leg 2 in C19 R15 → J12 MG90S #1 PARACHUTE BAY → pin **S** (C19 R14) |
| 5 | C21 R23 | PETRI | through **R4 330**: leg 1 in C22 R19 (joined to this signal), leg 2 in C22 R15 → J13 MG90S #2 PETRI PLATE → pin **S** (C22 R14) |
| 6 | C22 R23 | ESC | through **R5 330**: leg 1 in C28 R21 (joined to this signal), leg 2 in C28 R17 → J14 ESC signal (Cyclone 45A BLHeli_S) → pin **S** (C28 R15) |
| 7 | C23 R23 | RX2 | J2 UART from MAIN J4 → pin **RX2←A** (C31 R15) |
| 8 | C24 R23 | TX2 | through **R7 1k TX2→A**: leg 1 in C31 R22 (joined to this signal), leg 2 in C31 R18 → J2 UART from MAIN J4 → pin **TX2→A** (C31 R16) ; through **R8 1k TX→B**: leg 1 in C36 R16 (joined to this signal), leg 2 in C36 R20 → J3 UART to BACKUP J3 → pin **TX→B** (C36 R22) |
| 9 | C25 R23 | HALL | J15 HALL sensor plug (sensor on the body) → pin **OUT** (C31 R24) ; through **R6 10k HALL↑3V3**: leg 1 in C33 R26 (joined to this signal), leg 2 in C33 R22 → **+3V3_B rail** (insulated rail wire on the component side; rail enters at J8 pin 3) |
| 10 | C26 R23 | SERVO_X | through **R9 330**: leg 1 in C34 R20 (joined to this signal), leg 2 in C34 R16 → J16 MG90S #3 EXTRA → pin **S** (C34 R14) |
| 11 | C27 R23 | — | **not connected — leave open** |
| 12 | C28 R23 | — | **not connected — leave open** |
| 13 | C28 R29 | — | **not connected — leave open** |
| 14 | C27 R29 | — | **not connected — leave open** |
| 15 | C26 R29 | RX3 | J3 UART to BACKUP J3 → pin **RX3←B** (C36 R23) |
| 16 | C25 R29 | — | **not connected — leave open** |
| 17 | C24 R29 | — | **not connected — leave open** |
| 18 | C23 R29 | SDA | U5 BMP585 #3 → pin **SDI** (C23 R32) ; U6 LSM6DSO32 (Adafruit) → pin **SDA** (C32 R31) |
| 19 | C22 R29 | SCL | U5 BMP585 #3 → pin **SCK** (C21 R32) ; U6 LSM6DSO32 (Adafruit) → pin **SCL** (C31 R31) |
| 20 | C21 R29 | — | **not connected — leave open** |
| 21 | C20 R29 | — | **not connected — leave open** |
| 22 | C19 R29 | — | **not connected — leave open** |
| 23 | C18 R29 | — | **not connected — leave open** |
| GND | C15 R23 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J8 pin 2, J8 pin 4) |
| 3V3 | C17 R29 | — | **not connected — leave open** (Teensy's own 3.3 V output — do NOT join it to the 3V3_B rail) |
| GND | C16 R29 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J8 pin 2, J8 pin 4) |
| VIN | C15 R29 | +5V_LOGIC | J8 logic PWR (POWER J7) → pin **+5V** (C12 R33) ; through **C3 470µF logic**: leg + in C11 R29 (joined to this signal), leg - in C11 R27 → **GND rail** (insulated rail wire on the component side; rail enters at J8 pin 2, J8 pin 4) ; through **C4 100nF**: leg 1 in C14 R31 (joined to this signal), leg 2 in C16 R31 → **GND rail** (insulated rail wire on the component side; rail enters at J8 pin 2, J8 pin 4) ; J15 HALL sensor plug (sensor on the body) → pin **+5V** (C31 R25) |

### U5 — BMP585 #3  
*0x47 · own 600 m baro*

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| VIN | C18 R32 | +3V3_B | **+3V3_B rail** (insulated rail wire on the component side; rail enters at J8 pin 3) |
| 3V3 | C19 R32 | — | **not connected — leave open** (module's 3.3 V output — leave open) |
| GND | C20 R32 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J8 pin 2, J8 pin 4) |
| SCK | C21 R32 | SCL | U1 Teensy 4.0 PARACHUTE → pin **19** (C22 R29) ; U6 LSM6DSO32 (Adafruit) → pin **SCL** (C31 R31) |
| SDO | C22 R32 | +3V3_B | **+3V3_B rail** (insulated rail wire on the component side; rail enters at J8 pin 3) |
| SDI | C23 R32 | SDA | U1 Teensy 4.0 PARACHUTE → pin **18** (C23 R29) ; U6 LSM6DSO32 (Adafruit) → pin **SDA** (C32 R31) |
| CS | C24 R32 | +3V3_B | **+3V3_B rail** (insulated rail wire on the component side; rail enters at J8 pin 3) |
| INT | C25 R32 | — | **not connected — leave open** |

### U6 — LSM6DSO32 (Adafruit)  
*I²C: CS→3V3, DO→GND = 0x6A · solder the FRONT row only (back 5 holes empty)*

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| VIN | C28 R31 | +3V3_B | **+3V3_B rail** (insulated rail wire on the component side; rail enters at J8 pin 3) |
| 3Vo | C29 R31 | — | **not connected — leave open** |
| GND | C30 R31 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J8 pin 2, J8 pin 4) |
| SCL | C31 R31 | SCL | U1 Teensy 4.0 PARACHUTE → pin **19** (C22 R29) ; U5 BMP585 #3 → pin **SCK** (C21 R32) |
| SDA | C32 R31 | SDA | U1 Teensy 4.0 PARACHUTE → pin **18** (C23 R29) ; U5 BMP585 #3 → pin **SDI** (C23 R32) |
| DO | C33 R31 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J8 pin 2, J8 pin 4) |
| CS | C34 R31 | +3V3_B | **+3V3_B rail** (insulated rail wire on the component side; rail enters at J8 pin 3) |
| INT1 | C35 R31 | — | **not connected — leave open** |
| INT2 | C36 R31 | — | **not connected — leave open** |
| SCX | C30 R36 | — | **not connected — leave open** |
| SDX | C31 R36 | — | **not connected — leave open** |
| CS(back) | C32 R36 | — | **not connected — leave open** |
| DO(back) | C33 R36 | — | **not connected — leave open** |
| GND(back) | C34 R36 | — | **not connected — leave open** |

### J5 — GNSS from MAIN J8

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| GNSS | C8 R19 | GNSS_IN | U1 Teensy 4.0 PARACHUTE → pin **0** (C16 R23) |
| GND | C8 R18 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J8 pin 2, J8 pin 4) |

### J1 — 8 V steer in (POWER J8)

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| +8V | C6 R14 | +8V_STEER | join to this module's own pin(s) **+8V** (C6 R13) ; through **C1 470µF 8V rail**: leg + in C9 R13 (joined to this signal), leg - in C11 R12 → **GND rail** (insulated rail wire on the component side; rail enters at J8 pin 2, J8 pin 4) ; J10 A20CLM LEFT steer (8 V) → pin **+** (C13 R13) ; J11 A20CLM RIGHT steer (8 V) → pin **+** (C16 R13) |
| +8V | C6 R13 | +8V_STEER | join to this module's own pin(s) **+8V** (C6 R14) ; through **C1 470µF 8V rail**: leg + in C9 R13 (joined to this signal), leg - in C11 R12 → **GND rail** (insulated rail wire on the component side; rail enters at J8 pin 2, J8 pin 4) ; J10 A20CLM LEFT steer (8 V) → pin **+** (C13 R13) ; J11 A20CLM RIGHT steer (8 V) → pin **+** (C16 R13) |
| GND | C6 R12 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J8 pin 2, J8 pin 4) |
| GND | C6 R11 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J8 pin 2, J8 pin 4) |

### J10 — A20CLM LEFT steer (8 V)  
*2.54 mm servo header*

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| − | C13 R12 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J8 pin 2, J8 pin 4) |
| + | C13 R13 | +8V_STEER | J1 8 V steer in (POWER J8) → pin **+8V** (C6 R14) ; J1 8 V steer in (POWER J8) → pin **+8V** (C6 R13) ; through **C1 470µF 8V rail**: leg + in C9 R13 (joined to this signal), leg - in C11 R12 → **GND rail** (insulated rail wire on the component side; rail enters at J8 pin 2, J8 pin 4) ; J11 A20CLM RIGHT steer (8 V) → pin **+** (C16 R13) |
| S | C13 R14 | STEER_L_S | through **R1 330**: leg 2 in C13 R15 (joined to this signal), leg 1 in C13 R19 → U1 Teensy 4.0 PARACHUTE → pin **2** (C18 R23) |

### J11 — A20CLM RIGHT steer (8 V)  
*2.54 mm servo header*

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| − | C16 R12 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J8 pin 2, J8 pin 4) |
| + | C16 R13 | +8V_STEER | J1 8 V steer in (POWER J8) → pin **+8V** (C6 R14) ; J1 8 V steer in (POWER J8) → pin **+8V** (C6 R13) ; through **C1 470µF 8V rail**: leg + in C9 R13 (joined to this signal), leg - in C11 R12 → **GND rail** (insulated rail wire on the component side; rail enters at J8 pin 2, J8 pin 4) ; J10 A20CLM LEFT steer (8 V) → pin **+** (C13 R13) |
| S | C16 R14 | STEER_R_S | through **R2 330**: leg 2 in C16 R15 (joined to this signal), leg 1 in C16 R19 → U1 Teensy 4.0 PARACHUTE → pin **3** (C19 R23) |

### J12 — MG90S #1 PARACHUTE BAY  
*2.54 mm servo header*

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| − | C19 R12 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J8 pin 2, J8 pin 4) |
| + | C19 R13 | +5V_SERVO | J6 5 V servo in (POWER J9) → pin **+5VS** (C35 R9) ; through **C2 470µF 5VS rail**: leg + in C31 R7 (joined to this signal), leg - in C33 R7 → **GND rail** (insulated rail wire on the component side; rail enters at J8 pin 2, J8 pin 4) ; J13 MG90S #2 PETRI PLATE → pin **+** (C22 R13) ; J16 MG90S #3 EXTRA → pin **+** (C34 R13) |
| S | C19 R14 | BAY_LID_S | through **R3 330**: leg 2 in C19 R15 (joined to this signal), leg 1 in C19 R19 → U1 Teensy 4.0 PARACHUTE → pin **4** (C20 R23) |

### J13 — MG90S #2 PETRI PLATE  
*2.54 mm servo header*

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| − | C22 R12 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J8 pin 2, J8 pin 4) |
| + | C22 R13 | +5V_SERVO | J6 5 V servo in (POWER J9) → pin **+5VS** (C35 R9) ; through **C2 470µF 5VS rail**: leg + in C31 R7 (joined to this signal), leg - in C33 R7 → **GND rail** (insulated rail wire on the component side; rail enters at J8 pin 2, J8 pin 4) ; J12 MG90S #1 PARACHUTE BAY → pin **+** (C19 R13) ; J16 MG90S #3 EXTRA → pin **+** (C34 R13) |
| S | C22 R14 | PETRI_S | through **R4 330**: leg 2 in C22 R15 (joined to this signal), leg 1 in C22 R19 → U1 Teensy 4.0 PARACHUTE → pin **5** (C21 R23) |

### J14 — ESC signal (Cyclone 45A BLHeli_S)

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| − | C28 R14 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J8 pin 2, J8 pin 4) |
| S | C28 R15 | ESC_S | through **R5 330**: leg 2 in C28 R17 (joined to this signal), leg 1 in C28 R21 → U1 Teensy 4.0 PARACHUTE → pin **6** (C22 R23) |

### J2 — UART from MAIN J4

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| TX2→A | C31 R16 | TX2_A | through **R7 1k TX2→A**: leg 2 in C31 R18 (joined to this signal), leg 1 in C31 R22 → U1 Teensy 4.0 PARACHUTE → pin **8** (C24 R23) ; through **R8 1k TX→B**: leg 1 in C36 R16 (joined to this signal), leg 2 in C36 R20 → J3 UART to BACKUP J3 → pin **TX→B** (C36 R22) |
| RX2←A | C31 R15 | RX2 | U1 Teensy 4.0 PARACHUTE → pin **7** (C23 R23) |
| GND | C31 R14 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J8 pin 2, J8 pin 4) |

### J16 — MG90S #3 EXTRA  
*2.54 mm servo header*

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| − | C34 R12 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J8 pin 2, J8 pin 4) |
| + | C34 R13 | +5V_SERVO | J6 5 V servo in (POWER J9) → pin **+5VS** (C35 R9) ; through **C2 470µF 5VS rail**: leg + in C31 R7 (joined to this signal), leg - in C33 R7 → **GND rail** (insulated rail wire on the component side; rail enters at J8 pin 2, J8 pin 4) ; J12 MG90S #1 PARACHUTE BAY → pin **+** (C19 R13) ; J13 MG90S #2 PETRI PLATE → pin **+** (C22 R13) |
| S | C34 R14 | SERVO_X_S | through **R9 330**: leg 2 in C34 R16 (joined to this signal), leg 1 in C34 R20 → U1 Teensy 4.0 PARACHUTE → pin **10** (C26 R23) |

### J6 — 5 V servo in (POWER J9)

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| +5VS | C35 R9 | +5V_SERVO | through **C2 470µF 5VS rail**: leg + in C31 R7 (joined to this signal), leg - in C33 R7 → **GND rail** (insulated rail wire on the component side; rail enters at J8 pin 2, J8 pin 4) ; J12 MG90S #1 PARACHUTE BAY → pin **+** (C19 R13) ; J13 MG90S #2 PETRI PLATE → pin **+** (C22 R13) ; J16 MG90S #3 EXTRA → pin **+** (C34 R13) |
| GND | C36 R9 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J8 pin 2, J8 pin 4) |

### J8 — logic PWR (POWER J7)

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| +5V | C12 R33 | +5V_LOGIC | U1 Teensy 4.0 PARACHUTE → pin **VIN** (C15 R29) ; through **C3 470µF logic**: leg + in C11 R29 (joined to this signal), leg - in C11 R27 → **GND rail** (insulated rail wire on the component side; rail enters at J8 pin 2, J8 pin 4) ; through **C4 100nF**: leg 1 in C14 R31 (joined to this signal), leg 2 in C16 R31 → **GND rail** (insulated rail wire on the component side; rail enters at J8 pin 2, J8 pin 4) ; J15 HALL sensor plug (sensor on the body) → pin **+5V** (C31 R25) |
| GND | C13 R33 | GND | **GND rail** (insulated rail wire on the component side; rail enters at the power-module landing holes) |
| +3V3 | C14 R33 | +3V3_B | **+3V3_B rail** (insulated rail wire on the component side; rail enters at the power-module landing holes) |
| GND | C15 R33 | GND | **GND rail** (insulated rail wire on the component side; rail enters at the power-module landing holes) |

### J15 — HALL sensor plug (sensor on the body)

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| − | C31 R26 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J8 pin 2, J8 pin 4) |
| +5V | C31 R25 | +5V_LOGIC | J8 logic PWR (POWER J7) → pin **+5V** (C12 R33) ; U1 Teensy 4.0 PARACHUTE → pin **VIN** (C15 R29) ; through **C3 470µF logic**: leg + in C11 R29 (joined to this signal), leg - in C11 R27 → **GND rail** (insulated rail wire on the component side; rail enters at J8 pin 2, J8 pin 4) ; through **C4 100nF**: leg 1 in C14 R31 (joined to this signal), leg 2 in C16 R31 → **GND rail** (insulated rail wire on the component side; rail enters at J8 pin 2, J8 pin 4) |
| OUT | C31 R24 | HALL | U1 Teensy 4.0 PARACHUTE → pin **9** (C25 R23) ; through **R6 10k HALL↑3V3**: leg 1 in C33 R26 (joined to this signal), leg 2 in C33 R22 → **+3V3_B rail** (insulated rail wire on the component side; rail enters at J8 pin 3) |

### J3 — UART to BACKUP J3

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| TX→B | C36 R22 | TX2_B | through **R8 1k TX→B**: leg 2 in C36 R20 (joined to this signal), leg 1 in C36 R16 → U1 Teensy 4.0 PARACHUTE → pin **8** (C24 R23) ; through **R7 1k TX2→A**: leg 1 in C31 R22 (joined to this signal), leg 2 in C31 R18 → J2 UART from MAIN J4 → pin **TX2→A** (C31 R16) |
| RX3←B | C36 R23 | RX3 | U1 Teensy 4.0 PARACHUTE → pin **15** (C26 R29) |
| GND | C36 R24 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J8 pin 2, J8 pin 4) |

### Resistors, diodes, LEDs and capacitors on this board

| part | value / role | leg 1 → hole → signal | leg 2 → hole → signal |
|---|---|---|---|
| C1 | 470µF 8V rail | + → C9 R13 → +8V_STEER | - → C11 R12 → GND |
| C2 | 470µF 5VS rail | + → C31 R7 → +5V_SERVO | - → C33 R7 → GND |
| C3 | 470µF logic | + → C11 R29 → +5V_LOGIC | - → C11 R27 → GND |
| C4 | 100nF | 1 → C14 R31 → +5V_LOGIC | 2 → C16 R31 → GND |
| C5 | 100nF | 1 → C37 R29 → +3V3_B | 2 → C37 R27 → GND |
| R1 | 330 | 1 → C13 R19 → STEER_L | 2 → C13 R15 → STEER_L_S |
| R2 | 330 | 1 → C16 R19 → STEER_R | 2 → C16 R15 → STEER_R_S |
| R3 | 330 | 1 → C19 R19 → BAY_LID | 2 → C19 R15 → BAY_LID_S |
| R4 | 330 | 1 → C22 R19 → PETRI | 2 → C22 R15 → PETRI_S |
| R5 | 330 | 1 → C28 R21 → ESC | 2 → C28 R17 → ESC_S |
| R6 | 10k HALL↑3V3 | 1 → C33 R26 → HALL | 2 → C33 R22 → +3V3_B |
| R7 | 1k TX2→A | 1 → C31 R22 → TX2 | 2 → C31 R18 → TX2_A |
| R8 | 1k TX→B | 1 → C36 R16 → TX2 | 2 → C36 R20 → TX2_B |
| R9 | 330 | 1 → C34 R20 → SERVO_X | 2 → C34 R16 → SERVO_X_S |

Polarity: LED / diode leg **A** = anode (long leg / no band), **K** = cathode (short leg / band). Electrolytic **+** = long leg, **−** = stripe side.

## Board 3 — POWER (zero board)

### U1 — INA260 #1 BATTERY  
*0x40*

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| VCC | C5 R50 | +3V3_A | M4L M4 landing → pin **OUT** (C19 R20) ; J5 PWR to A J1 → pin **+3V3_A** (C44 R14) ; U2 INA260 #2 8V STEER → pin **VCC** (C5 R14) ; U3 INA260 #3 5V LOGIC → pin **VCC** (C5 R30) |
| GND | C5 R49 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J5 pin 2) |
| SCL | C5 R48 | SCL | J2 I²C to A J2 → pin **SCL** (C30 R57) ; U2 INA260 #2 8V STEER → pin **SCL** (C5 R12) ; U3 INA260 #3 5V LOGIC → pin **SCL** (C5 R28) |
| SDA | C5 R47 | SDA | J2 I²C to A J2 → pin **SDA** (C31 R57) ; U2 INA260 #2 8V STEER → pin **SDA** (C5 R11) ; U3 INA260 #3 5V LOGIC → pin **SDA** (C5 R27) |
| ALERT | C5 R46 | — | **not connected — leave open** (not used) |
| VBUS | C5 R45 | VBAT | **VBAT rail** (insulated rail wire on the component side; rail enters at the power-module landing holes) |
| IN+ | C5 R44 | VBAT_F | through **F1 FUSE 10 A (link until fitted)**: leg OUT in C11 R57 (joined to this signal), leg IN in C10 R57 → J3 rocker switch (shell) → pin **SW_B** (C7 R57) ; through **R1 1k LED**: leg 1 in C14 R54 (joined to this signal), leg 2 in C18 R54 → through **D1 PWR LED red**: leg A in C20 R54 (joined to this signal), leg K in C20 R53 → **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J5 pin 2) |
| IN- | C5 R43 | VBAT | **VBAT rail** (insulated rail wire on the component side; rail enters at the power-module landing holes) |

### U3 — INA260 #3 5V LOGIC  
*0x45*

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| VCC | C5 R30 | +3V3_A | M4L M4 landing → pin **OUT** (C19 R20) ; J5 PWR to A J1 → pin **+3V3_A** (C44 R14) ; U1 INA260 #1 BATTERY → pin **VCC** (C5 R50) ; U2 INA260 #2 8V STEER → pin **VCC** (C5 R14) |
| GND | C5 R29 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J5 pin 2) |
| SCL | C5 R28 | SCL | J2 I²C to A J2 → pin **SCL** (C30 R57) ; U1 INA260 #1 BATTERY → pin **SCL** (C5 R48) ; U2 INA260 #2 8V STEER → pin **SCL** (C5 R12) |
| SDA | C5 R27 | SDA | J2 I²C to A J2 → pin **SDA** (C31 R57) ; U1 INA260 #1 BATTERY → pin **SDA** (C5 R47) ; U2 INA260 #2 8V STEER → pin **SDA** (C5 R11) |
| ALERT | C5 R26 | — | **not connected — leave open** (not used) |
| VBUS | C5 R25 | +5V_LOGIC | **+5V_LOGIC rail** (insulated rail wire on the component side; rail enters at J5 pin 1, J6 pin 1) |
| IN+ | C5 R24 | 5V_RAW | M1L M1 landing → pin **OUT** (C19 R48) |
| IN- | C5 R23 | +5V_LOGIC | **+5V_LOGIC rail** (insulated rail wire on the component side; rail enters at J5 pin 1, J6 pin 1) |

### U2 — INA260 #2 8V STEER  
*0x41*

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| VCC | C5 R14 | +3V3_A | M4L M4 landing → pin **OUT** (C19 R20) ; J5 PWR to A J1 → pin **+3V3_A** (C44 R14) ; U1 INA260 #1 BATTERY → pin **VCC** (C5 R50) ; U3 INA260 #3 5V LOGIC → pin **VCC** (C5 R30) |
| GND | C5 R13 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J5 pin 2) |
| SCL | C5 R12 | SCL | J2 I²C to A J2 → pin **SCL** (C30 R57) ; U1 INA260 #1 BATTERY → pin **SCL** (C5 R48) ; U3 INA260 #3 5V LOGIC → pin **SCL** (C5 R28) |
| SDA | C5 R11 | SDA | J2 I²C to A J2 → pin **SDA** (C31 R57) ; U1 INA260 #1 BATTERY → pin **SDA** (C5 R47) ; U3 INA260 #3 5V LOGIC → pin **SDA** (C5 R27) |
| ALERT | C5 R10 | — | **not connected — leave open** (not used) |
| VBUS | C5 R9 | +8V_STEER | join to this module's own pin(s) **IN-** (C5 R7) ; J8 8 V → PARACHUTE J1 → pin **+8V** (C35 R16) ; J8 8 V → PARACHUTE J1 → pin **+8V** (C35 R15) |
| IN+ | C5 R8 | 8V_RAW | M3L M3 landing → pin **OUT** (C19 R38) |
| IN- | C5 R7 | +8V_STEER | join to this module's own pin(s) **VBUS** (C5 R9) ; J8 8 V → PARACHUTE J1 → pin **+8V** (C35 R16) ; J8 8 V → PARACHUTE J1 → pin **+8V** (C35 R15) |

### M1L — M1 landing

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| IN | C19 R50 | VBAT | **VBAT rail** (insulated rail wire on the component side; rail enters at the power-module landing holes) |
| GND | C19 R49 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J5 pin 2) |
| OUT | C19 R48 | 5V_RAW | U3 INA260 #3 5V LOGIC → pin **IN+** (C5 R24) |

### M2L — M2 landing

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| IN | C34 R50 | VBAT | **VBAT rail** (insulated rail wire on the component side; rail enters at the power-module landing holes) |
| GND | C34 R49 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J5 pin 2) |
| OUT | C34 R48 | +5V_SERVO | J9 5 V servo → PARACHUTE J6 → pin **+5VS** (C35 R9) |

### M3L — M3 landing

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| IN | C19 R40 | VBAT | **VBAT rail** (insulated rail wire on the component side; rail enters at the power-module landing holes) |
| GND | C19 R39 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J5 pin 2) |
| OUT | C19 R38 | 8V_RAW | U2 INA260 #2 8V STEER → pin **IN+** (C5 R8) |

### M4L — M4 landing

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| IN | C19 R22 | +5V_LOGIC | **+5V_LOGIC rail** (insulated rail wire on the component side; rail enters at J5 pin 1, J6 pin 1) |
| GND | C19 R21 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J5 pin 2) |
| OUT | C19 R20 | +3V3_A | J5 PWR to A J1 → pin **+3V3_A** (C44 R14) ; U1 INA260 #1 BATTERY → pin **VCC** (C5 R50) ; U2 INA260 #2 8V STEER → pin **VCC** (C5 R14) ; U3 INA260 #3 5V LOGIC → pin **VCC** (C5 R30) |

### M5L — M5 landing

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| IN | C30 R22 | +5V_LOGIC | **+5V_LOGIC rail** (insulated rail wire on the component side; rail enters at J5 pin 1, J6 pin 1) |
| GND | C30 R21 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J5 pin 2) |
| OUT | C30 R20 | +3V3_B | J6 PWR to B J1 → pin **+3V3_B** (C41 R14) ; J7 PWR to D J8 → pin **+3V3_B** (C38 R14) |

### J1 — BATTERY (XT60 lead)

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| BAT+ | C2 R57 | BAT+ | J3 rocker switch (shell) → pin **SW_A** (C6 R57) |
| BAT− | C3 R57 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J5 pin 2, J5 pin 4) |

### J3 — rocker switch (shell)

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| SW_A | C6 R57 | BAT+ | J1 BATTERY (XT60 lead) → pin **BAT+** (C2 R57) |
| SW_B | C7 R57 | VBAT_SW | through **F1 FUSE 10 A (link until fitted)**: leg IN in C10 R57 (joined to this signal), leg OUT in C11 R57 → U1 INA260 #1 BATTERY → pin **IN+** (C5 R44) ; through **R1 1k LED**: leg 1 in C14 R54 (joined to this signal), leg 2 in C18 R54 → through **D1 PWR LED red**: leg A in C20 R54 (joined to this signal), leg K in C20 R53 → **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J5 pin 2) |

### J4 — ESC (XT30 lead)

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| VBAT | C44 R57 | VBAT | **VBAT rail** (insulated rail wire on the component side; rail enters at the power-module landing holes) |
| GND | C45 R57 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J5 pin 2) |

### J2 — I²C to A J2

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| SCL | C30 R57 | SCL | U1 INA260 #1 BATTERY → pin **SCL** (C5 R48) ; U2 INA260 #2 8V STEER → pin **SCL** (C5 R12) ; U3 INA260 #3 5V LOGIC → pin **SCL** (C5 R28) |
| SDA | C31 R57 | SDA | U1 INA260 #1 BATTERY → pin **SDA** (C5 R47) ; U2 INA260 #2 8V STEER → pin **SDA** (C5 R11) ; U3 INA260 #3 5V LOGIC → pin **SDA** (C5 R27) |
| GND | C32 R57 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J5 pin 2) |

### J5 — PWR to A J1

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| +5V | C44 R16 | +5V_LOGIC | **+5V_LOGIC rail** (insulated rail wire on the component side; rail enters at J6 pin 1, J7 pin 1) |
| GND | C44 R15 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J6 pin 2) |
| +3V3_A | C44 R14 | +3V3_A | M4L M4 landing → pin **OUT** (C19 R20) ; U1 INA260 #1 BATTERY → pin **VCC** (C5 R50) ; U2 INA260 #2 8V STEER → pin **VCC** (C5 R14) ; U3 INA260 #3 5V LOGIC → pin **VCC** (C5 R30) |
| GND | C44 R13 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J6 pin 2) |

### J6 — PWR to B J1

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| +5V | C41 R16 | +5V_LOGIC | **+5V_LOGIC rail** (insulated rail wire on the component side; rail enters at J5 pin 1, J7 pin 1) |
| GND | C41 R15 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J5 pin 2) |
| +3V3_B | C41 R14 | +3V3_B | M5L M5 landing → pin **OUT** (C30 R20) ; J7 PWR to D J8 → pin **+3V3_B** (C38 R14) |
| GND | C41 R13 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J5 pin 2) |

### J7 — PWR to D J8

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| +5V | C38 R16 | +5V_LOGIC | **+5V_LOGIC rail** (insulated rail wire on the component side; rail enters at J5 pin 1, J6 pin 1) |
| GND | C38 R15 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J5 pin 2) |
| +3V3_B | C38 R14 | +3V3_B | M5L M5 landing → pin **OUT** (C30 R20) ; J6 PWR to B J1 → pin **+3V3_B** (C41 R14) |
| GND | C38 R13 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J5 pin 2) |

### J8 — 8 V → PARACHUTE J1

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| +8V | C35 R16 | +8V_STEER | join to this module's own pin(s) **+8V** (C35 R15) ; U2 INA260 #2 8V STEER → pin **IN-** (C5 R7) ; U2 INA260 #2 8V STEER → pin **VBUS** (C5 R9) |
| +8V | C35 R15 | +8V_STEER | join to this module's own pin(s) **+8V** (C35 R16) ; U2 INA260 #2 8V STEER → pin **IN-** (C5 R7) ; U2 INA260 #2 8V STEER → pin **VBUS** (C5 R9) |
| GND | C35 R14 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J5 pin 2) |
| GND | C35 R13 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J5 pin 2) |

### J9 — 5 V servo → PARACHUTE J6

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| +5VS | C35 R9 | +5V_SERVO | M2L M2 landing → pin **OUT** (C34 R48) |
| GND | C35 R8 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J5 pin 2) |

### J10 — VBAT sense to A J3

| pin | hole | signal | connect to (resistor / diode legs spelled out) |
|---|---|---|---|
| VBAT | C44 R9 | VBAT | **VBAT rail** (insulated rail wire on the component side; rail enters at the power-module landing holes) |
| GND | C44 R8 | GND | **GND rail** (insulated rail wire on the component side; rail enters at J1 pin 2, J5 pin 2) |

### Resistors, diodes, LEDs and capacitors on this board

| part | value / role | leg 1 → hole → signal | leg 2 → hole → signal |
|---|---|---|---|
| C1 | 470µF 25V VBAT | + → C22 R44 → VBAT | - → C24 R44 → GND |
| D1 | PWR LED red | A → C20 R54 → LED_A | K → C20 R53 → GND |
| F1 | FUSE 10 A (link until fitted) | IN → C10 R57 → VBAT_SW | OUT → C11 R57 → VBAT_F |
| R1 | 1k LED | 1 → C14 R54 → VBAT_SW | 2 → C18 R54 → LED_A |

Polarity: LED / diode leg **A** = anode (long leg / no band), **K** = cathode (short leg / band). Electrolytic **+** = long leg, **−** = stripe side.
