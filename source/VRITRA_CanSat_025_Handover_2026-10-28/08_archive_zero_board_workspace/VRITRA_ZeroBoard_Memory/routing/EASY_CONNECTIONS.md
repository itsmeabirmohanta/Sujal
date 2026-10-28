# VRITRA — every connection in plain words

## BOARD 1 MAIN (etched)

**Teensy 4.1 (main)**
- 0 → JST-5 LINK cable to BACKUP pin 4 (MUX_RX)
- 1 → 1k resistor (R11) → JST-5 LINK cable to BACKUP pin 3 (MUX_TX)
- 2 → Beacon amp (Adafruit MAX98357A) DIN
- 3 → Beacon amp (Adafruit MAX98357A) LRC
- 4 → Beacon amp (Adafruit MAX98357A) BCLK
- 5 → 1k resistor (R3) → JST-5 LINK cable to BACKUP pin 1 (HB)
- 6 → GPS (L89HA) RST
- 7 → GPS (L89HA) TX + 1k resistor (R5) → JST-2 GPS copy to PARACHUTE pin 1 (GNSS) + 1k resistor (R6) → JST-5 LINK cable to BACKUP pin 2 (GNSS)
- 8 → GPS (L89HA) RX
- 9 → Flash memory (W25Q64) CS + 10k resistor (R2) → 3.3V wire
- 14 → 1k resistor (R4) → JST-3 UART to PARACHUTE pin 2 (TX3)
- 15 → JST-3 UART to PARACHUTE pin 1 (RX3)
- 16 → JST-4 I2C to SENSOR pin 2 (SCL1)
- 17 → JST-4 I2C to SENSOR pin 1 (SDA1)
- 18 → RTC clock (DS3231) SDA + Pressure sensor (BMP585) SDI + IMU (ICM-20948) SDA + JST-3 I2C to POWER pin 2 (SDA)
- 19 → RTC clock (DS3231) SCL + Pressure sensor (BMP585) SCK + IMU (ICM-20948) SCL + JST-3 I2C to POWER pin 1 (SCL)
- 20 → 100k resistor (R7) → JST-2 BATTERY SENSE pin 1 (VBAT) + 22k resistor (R8) → GND wire
- 22 → JST-3 ANALOG to SENSOR pin 2 (UV)
- 23 → JST-3 ANALOG to SENSOR pin 1 (O3)
- 24 → LoRa radio (Core1121) BUSY
- 25 → LoRa radio (Core1121) RESET
- 26 → LoRa radio (Core1121) MOSI + Flash memory (W25Q64) MOSI
- 27 → LoRa radio (Core1121) CLK + Flash memory (W25Q64) SCK
- 29 → LoRa radio (Core1121) CS + 10k resistor (R1) → 3.3V wire
- 30 → LoRa radio (Core1121) DIO9
- 31 → 470Ω resistor (R9) → green LED (D1) [anode leg here] → GND wire
- 32 → 470Ω resistor (R10) → red LED (D2) [anode leg here] → GND wire
- 37 → Beacon amp (Adafruit MAX98357A) SD
- 39 → LoRa radio (Core1121) MISO + Flash memory (W25Q64) MISO
- GND → GND wire  (all 3 GND pins)
- VIN → 5V wire
- not connected: 10, 11, 12, 13, 21, 28, 33, 34, 35, 36, 38, 40, 41, 3V3 (leave EMPTY (never join to the 3.3V wire))

**LoRa radio (Core1121)**
- GND → GND wire  (all 3 GND pins)
- DIO9 → Teensy 4.1 (main) pin 30
- 3.3V → 3.3V wire
- CS → Teensy 4.1 (main) pin 29 + 10k resistor (R1) → 3.3V wire
- CLK → Teensy 4.1 (main) pin 27 + Flash memory (W25Q64) SCK
- MOSI → Teensy 4.1 (main) pin 26 + Flash memory (W25Q64) MOSI
- MISO → Teensy 4.1 (main) pin 39 + Flash memory (W25Q64) MISO
- RESET → Teensy 4.1 (main) pin 25
- BUSY → Teensy 4.1 (main) pin 24
- not connected: 2.4G, DIO8, DIO7, NC, LORA_ANT

**GPS (L89HA)**
- RX → Teensy 4.1 (main) pin 8
- 3V3 → 3.3V wire
- GND → GND wire
- WAKE → 3.3V wire
- RST → Teensy 4.1 (main) pin 6
- BAT → Schottky diode (D3) [cathode leg here] → 3.3V wire + Schottky diode (D4) [cathode leg here] → JST-2 GPS COIN CELL pin 1 (CELL+)
- TX → Teensy 4.1 (main) pin 7 + 1k resistor (R5) → JST-2 GPS copy to PARACHUTE pin 1 (GNSS) + 1k resistor (R6) → JST-5 LINK cable to BACKUP pin 2 (GNSS)
- not connected: 3D_FIX, SDA, SCL, GEO, JAM

**RTC clock (DS3231)**
- SCL → Teensy 4.1 (main) pin 19 + Pressure sensor (BMP585) SCK + IMU (ICM-20948) SCL + JST-3 I2C to POWER pin 1 (SCL)
- SDA → Teensy 4.1 (main) pin 18 + Pressure sensor (BMP585) SDI + IMU (ICM-20948) SDA + JST-3 I2C to POWER pin 2 (SDA)
- VCC → 3.3V wire
- GND → GND wire

**Pressure sensor (BMP585)**
- VIN → 3.3V wire
- GND → GND wire
- SCK → Teensy 4.1 (main) pin 19 + RTC clock (DS3231) SCL + IMU (ICM-20948) SCL + JST-3 I2C to POWER pin 1 (SCL)
- SDO → 3.3V wire
- SDI → Teensy 4.1 (main) pin 18 + RTC clock (DS3231) SDA + IMU (ICM-20948) SDA + JST-3 I2C to POWER pin 2 (SDA)
- CS → 3.3V wire
- not connected: 3V3, INT

**IMU (ICM-20948)**
- VIN → 3.3V wire
- FSYNC → GND wire
- 1V8 → its own CS and SDO pins
- GND → GND wire  (all 2 GND pins)
- SCL → Teensy 4.1 (main) pin 19 + RTC clock (DS3231) SCL + Pressure sensor (BMP585) SCK + JST-3 I2C to POWER pin 1 (SCL)
- SDA → Teensy 4.1 (main) pin 18 + RTC clock (DS3231) SDA + Pressure sensor (BMP585) SDI + JST-3 I2C to POWER pin 2 (SDA)
- CS → its own 1V8 and SDO pins
- SDO → its own 1V8 and CS pins
- not connected: ACL, ADA, INT

**Flash memory (W25Q64)**
- 3V3 → 3.3V wire
- CS → Teensy 4.1 (main) pin 9 + 10k resistor (R2) → 3.3V wire
- MISO → Teensy 4.1 (main) pin 39 + LoRa radio (Core1121) MISO
- SCK → Teensy 4.1 (main) pin 27 + LoRa radio (Core1121) CLK
- MOSI → Teensy 4.1 (main) pin 26 + LoRa radio (Core1121) MOSI
- GND → GND wire

**Beacon amp (Adafruit MAX98357A)**
- LRC → Teensy 4.1 (main) pin 3
- BCLK → Teensy 4.1 (main) pin 4
- DIN → Teensy 4.1 (main) pin 2
- GAIN → 100k resistor (R12) → GND wire
- SD → Teensy 4.1 (main) pin 37
- GND → GND wire
- Vin → 5V wire

**Plugs and pads on the BOARD 1 MAIN (etched)** (pin 1 = first pin)
- JST-3 ANALOG to SENSOR: pin 1 O3 → Teensy 4.1 (main) pin 23 | pin 2 UV → Teensy 4.1 (main) pin 22 | pin 3 GND → GND wire
- JST-4 I2C to SENSOR: pin 1 SDA1 → Teensy 4.1 (main) pin 17 | pin 2 SCL1 → Teensy 4.1 (main) pin 16 | pin 3 3V3 → 3.3V wire | pin 4 GND → GND wire
- JST-2 GPS copy to PARACHUTE: pin 1 GNSS → 1k resistor (R5) → GPS (L89HA) TX + Teensy 4.1 (main) pin 7 + 1k resistor (R6) → JST-5 LINK cable to BACKUP pin 2 (GNSS) | pin 2 GND → GND wire
- JST-3 UART to PARACHUTE: pin 1 RX3 → Teensy 4.1 (main) pin 15 | pin 2 TX3 → 1k resistor (R4) → Teensy 4.1 (main) pin 14 | pin 3 GND → GND wire
- JST-5 LINK cable to BACKUP: pin 1 HB → 1k resistor (R3) → Teensy 4.1 (main) pin 5 | pin 2 GNSS → 1k resistor (R6) → GPS (L89HA) TX + Teensy 4.1 (main) pin 7 + 1k resistor (R5) → JST-2 GPS copy to PARACHUTE pin 1 (GNSS) | pin 3 MUX_TX → 1k resistor (R11) → Teensy 4.1 (main) pin 1 | pin 4 MUX_RX → Teensy 4.1 (main) pin 0 | pin 5 GND → GND wire
- JST-3 I2C to POWER: pin 1 SCL → Teensy 4.1 (main) pin 19 + RTC clock (DS3231) SCL + Pressure sensor (BMP585) SCK + IMU (ICM-20948) SCL | pin 2 SDA → Teensy 4.1 (main) pin 18 + RTC clock (DS3231) SDA + Pressure sensor (BMP585) SDI + IMU (ICM-20948) SDA | pin 3 GND → GND wire
- JST-2 BATTERY SENSE: pin 1 VBAT → 100k resistor (R7) → 22k resistor (R8) → GND wire + Teensy 4.1 (main) pin 20 | pin 2 GND → GND wire
- JST-4 POWER IN: pin 1 +5V → 5V wire | pin 2 GND → GND wire | pin 3 +3V3 → 3.3V wire | pin 4 GND → GND wire
- JST-2 GPS COIN CELL: pin 1 CELL+ → Schottky diode (D4) [anode leg here] → GPS (L89HA) BAT + Schottky diode (D3) [cathode leg here] → 3.3V wire | pin 2 GND → GND wire

## BOARD 2 BACKUP + SENSOR (etched)

**Teensy 4.1 (backup)**
- 0 → JST-5 LINK cable from MAIN pin 2 (GNSS)
- 4 → JST-5 LINK cable from MAIN pin 1 (HB)
- 5 → Camera mux (7Semi 4-ch UART) A + 10k resistor (R6) → GND wire
- 6 → Camera mux (7Semi 4-ch UART) B + 10k resistor (R7) → GND wire
- 9 → Camera mux (7Semi 4-ch UART) C + 10k resistor (R8) → GND wire
- 10 → U11 CS
- 11 → U11 MOSI
- 12 → U11 MISO
- 13 → U11 SCK
- 14 → 1k resistor (R1) → JST-3 link to PARACHUTE pin 2 (TX3)
- 15 → JST-3 link to PARACHUTE pin 1 (RX3)
- 18 → Pressure sensor (BMP585 #2) SDI + IMU (LSM6DSO32) SDA + HDC3020 cable wires (solder) SDA
- 19 → Pressure sensor (BMP585 #2) SCK + IMU (LSM6DSO32) SCL + HDC3020 cable wires (solder) SCL
- 34 → 1k resistor (R3) → Camera mux (7Semi 4-ch UART) RX2
- 35 → 1k resistor (R2) → Camera mux (7Semi 4-ch UART) TX2
- 36 → 470Ω resistor (R4) → green LED (D1) [anode leg here] → GND wire
- 37 → 470Ω resistor (R5) → red LED (D2) [anode leg here] → GND wire
- GND → GND wire  (all 3 GND pins)
- VIN → 5V wire
- not connected: 1, 2, 3, 7, 8, 16, 17, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 38, 39, 40, 41, 3V3 (leave EMPTY)

**Pressure sensor (BMP585 #2)**
- VIN → 3.3V wire
- GND → GND wire
- SCK → Teensy 4.1 (backup) pin 19 + IMU (LSM6DSO32) SCL + HDC3020 cable wires (solder) SCL
- SDO → 3.3V wire
- SDI → Teensy 4.1 (backup) pin 18 + IMU (LSM6DSO32) SDA + HDC3020 cable wires (solder) SDA
- CS → 3.3V wire
- not connected: 3V3, INT

**IMU (LSM6DSO32)**
- VIN → 3.3V wire
- GND → GND wire
- SCL → Teensy 4.1 (backup) pin 19 + Pressure sensor (BMP585 #2) SCK + HDC3020 cable wires (solder) SCL
- SDA → Teensy 4.1 (backup) pin 18 + Pressure sensor (BMP585 #2) SDI + HDC3020 cable wires (solder) SDA
- DO → GND wire
- CS → 3.3V wire
- not connected: SCX, SDX, CS(back), DO(back), GND(back), 3Vo, INT1, INT2

**Camera mux (7Semi 4-ch UART)**
- B → Teensy 4.1 (backup) pin 6 + 10k resistor (R7) → GND wire
- C → Teensy 4.1 (backup) pin 9 + 10k resistor (R8) → GND wire
- TX1 → JST-5 LINK cable from MAIN pin 3 (MUX_TX)
- GND → GND wire  (all 2 GND pins)
- RX1 → JST-5 LINK cable from MAIN pin 4 (MUX_RX)
- TX2 → 1k resistor (R2) → Teensy 4.1 (backup) pin 35
- RX2 → 1k resistor (R3) → Teensy 4.1 (backup) pin 34
- A → Teensy 4.1 (backup) pin 5 + 10k resistor (R6) → GND wire
- not connected: VCC_L1, VCC_R1, TX3, RX3, GND, VCC_R2, TX4, RX4, VCC_L2

**Camera mux bottom-row wires**
- 3V3 → 3.3V wire
- GND → GND wire
- RX_COM → 1k resistor (R10) → JST-4 CAMERA plug (the only one) pin 4 (from D6)
- TX_COM → 1k resistor (R9) → JST-4 CAMERA plug (the only one) pin 3 (to D7)
- not connected: 5V

**Plugs and pads on the BOARD 2 BACKUP + SENSOR (etched)** (pin 1 = first pin)
- JST-4 POWER IN: pin 1 +5V → 5V wire | pin 2 GND → GND wire | pin 3 +3V3 → 3.3V wire | pin 4 GND → GND wire
- JST-5 LINK cable from MAIN: pin 1 HB → Teensy 4.1 (backup) pin 4 | pin 2 GNSS → Teensy 4.1 (backup) pin 0 | pin 3 MUX_TX → Camera mux (7Semi 4-ch UART) TX1 | pin 4 MUX_RX → Camera mux (7Semi 4-ch UART) RX1 | pin 5 GND → GND wire
- JST-3 link to PARACHUTE: pin 1 RX3 → Teensy 4.1 (backup) pin 15 | pin 2 TX3 → 1k resistor (R1) → Teensy 4.1 (backup) pin 14 | pin 3 GND → GND wire
- JST-4 CAMERA plug (the only one): pin 1 +5V → 5V wire | pin 2 GND → GND wire | pin 3 to D7 → 1k resistor (R9) → Camera mux bottom-row wires TX_COM | pin 4 from D6 → 1k resistor (R10) → Camera mux bottom-row wires RX_COM
- HDC3020 cable wires (solder): pin 1 3V3 → 3.3V wire | pin 2 GND → GND wire | pin 3 SDA → Teensy 4.1 (backup) pin 18 + Pressure sensor (BMP585 #2) SDI + IMU (LSM6DSO32) SDA | pin 4 SCL → Teensy 4.1 (backup) pin 19 + Pressure sensor (BMP585 #2) SCK + IMU (LSM6DSO32) SCL

## BOARD 4 PARACHUTE (etched)

**Teensy 4.0 (parachute)**
- 0 → JST-2 GPS copy from MAIN pin 1 (GNSS)
- 2 → 330Ω resistor (R1) → LEFT steering servo plug pin 3 (S)
- 3 → 330Ω resistor (R2) → RIGHT steering servo plug pin 3 (S)
- 4 → 330Ω resistor (R3) → parachute-bay servo plug (MG90S #1) pin 3 (S)
- 5 → 330Ω resistor (R4) → petri-lid servo plug (MG90S #2) pin 3 (S)
- 6 → 330Ω resistor (R5) → 2-pin ESC signal plug (Cyclone 45A) pin 2 (S)
- 7 → JST-3 UART from MAIN pin 2 (RX2←A)
- 8 → 1k resistor (R7) → JST-3 UART from MAIN pin 1 (TX2→A) + 1k resistor (R8) → JST-3 link to BACKUP pin 1 (TX→B)
- 9 → Hall sensor plug pin 3 (OUT) + 10k resistor (R6) → 3.3V wire
- 10 → 330Ω resistor (R9) → J16 pin 3 (S)
- 15 → JST-3 link to BACKUP pin 2 (RX3←B)
- 18 → Pressure sensor (BMP585 #3, to buy) SDI + IMU (ICM-20948 #2) SDA
- 19 → Pressure sensor (BMP585 #3, to buy) SCK + IMU (ICM-20948 #2) SCL
- GND → GND wire  (all 2 GND pins)
- VIN → 5V wire
- not connected: 1, 11, 12, 13, 14, 16, 17, 20, 21, 22, 23, 3V3 (leave EMPTY)

**Pressure sensor (BMP585 #3, to buy)**
- VIN → 3.3V wire
- GND → GND wire
- SCK → Teensy 4.0 (parachute) pin 19 + IMU (ICM-20948 #2) SCL
- SDO → 3.3V wire
- SDI → Teensy 4.0 (parachute) pin 18 + IMU (ICM-20948 #2) SDA
- CS → 3.3V wire
- not connected: 3V3, INT

**IMU (ICM-20948 #2)**
- VIN → 3.3V wire
- GND → GND wire
- SCL → Teensy 4.0 (parachute) pin 19 + Pressure sensor (BMP585 #3, to buy) SCK
- SDA → Teensy 4.0 (parachute) pin 18 + Pressure sensor (BMP585 #3, to buy) SDI
- DO → GND wire
- CS → 3.3V wire
- not connected: SCX, SDX, CS(back), DO(back), GND(back), 3Vo, INT1, INT2

**Plugs and pads on the BOARD 4 PARACHUTE (etched)** (pin 1 = first pin)
- JST-2 GPS copy from MAIN: pin 1 GNSS → Teensy 4.0 (parachute) pin 0 | pin 2 GND → GND wire
- 8V thick-wire pads: pin 1 +8V → 8V-servo wire | pin 2 +8V → 8V-servo wire | pin 3 GND → GND wire | pin 4 GND → GND wire
- LEFT steering servo plug: pin 1 − → GND wire | pin 2 + → 8V-servo wire | pin 3 S → 330Ω resistor (R1) → Teensy 4.0 (parachute) pin 2
- RIGHT steering servo plug: pin 1 − → GND wire | pin 2 + → 8V-servo wire | pin 3 S → 330Ω resistor (R2) → Teensy 4.0 (parachute) pin 3
- parachute-bay servo plug (MG90S #1): pin 1 − → GND wire | pin 2 + → 5V-servo wire | pin 3 S → 330Ω resistor (R3) → Teensy 4.0 (parachute) pin 4
- petri-lid servo plug (MG90S #2): pin 1 − → GND wire | pin 2 + → 5V-servo wire | pin 3 S → 330Ω resistor (R4) → Teensy 4.0 (parachute) pin 5
- 2-pin ESC signal plug (Cyclone 45A): pin 1 − → GND wire | pin 2 S → 330Ω resistor (R5) → Teensy 4.0 (parachute) pin 6
- JST-3 UART from MAIN: pin 1 TX2→A → 1k resistor (R7) → Teensy 4.0 (parachute) pin 8 + 1k resistor (R8) → JST-3 link to BACKUP pin 1 (TX→B) | pin 2 RX2←A → Teensy 4.0 (parachute) pin 7 | pin 3 GND → GND wire
- 5V-servo thick-wire pads: pin 1 +5VS → 5V-servo wire | pin 2 GND → GND wire
- JST-4 POWER IN: pin 1 +5V → 5V wire | pin 2 GND → GND wire | pin 3 +3V3 → 3.3V wire | pin 4 GND → GND wire
- Hall sensor plug: pin 1 − → GND wire | pin 2 +5V → 5V wire | pin 3 OUT → Teensy 4.0 (parachute) pin 9 + 10k resistor (R6) → 3.3V wire
- JST-3 link to BACKUP: pin 1 TX→B → 1k resistor (R8) → Teensy 4.0 (parachute) pin 8 + 1k resistor (R7) → JST-3 UART from MAIN pin 1 (TX2→A) | pin 2 RX3←B → Teensy 4.0 (parachute) pin 15 | pin 3 GND → GND wire

## BOARD 3 POWER (zero board)

**INA260 #1 (battery)**
- VCC → 3.3V wire
- GND → GND wire
- SCL → JST-3 I2C to MAIN pin 1 (SCL) + INA260 #2 (8V servo line) SCL + INA260 #3 (5V line) SCL
- SDA → JST-3 I2C to MAIN pin 2 (SDA) + INA260 #2 (8V servo line) SDA + INA260 #3 (5V line) SDA
- VBUS → battery+ wire
- IN+ → fuse (F1) → ON/OFF switch pads pin 2 (SW_B) + 1k resistor (R1) → red LED (D1) [anode leg here] → GND wire
- IN- → battery+ wire
- not connected: ALERT

**INA260 #3 (5V line)**
- VCC → 3.3V wire
- GND → GND wire
- SCL → JST-3 I2C to MAIN pin 1 (SCL) + INA260 #1 (battery) pin SCL + INA260 #2 (8V servo line) SCL
- SDA → JST-3 I2C to MAIN pin 2 (SDA) + INA260 #1 (battery) pin SDA + INA260 #2 (8V servo line) SDA
- VBUS → 5V wire
- IN+ → 5V buck #1 AP64500 (logic 5V) OUT
- IN- → 5V wire
- not connected: ALERT

**INA260 #2 (8V servo line)**
- VCC → 3.3V wire
- GND → GND wire
- SCL → JST-3 I2C to MAIN pin 1 (SCL) + INA260 #1 (battery) pin SCL + INA260 #3 (5V line) SCL
- SDA → JST-3 I2C to MAIN pin 2 (SDA) + INA260 #1 (battery) pin SDA + INA260 #3 (5V line) SDA
- VBUS → 8V-servo wire
- IN+ → 8V buck (steering servos) OUT
- IN- → 8V-servo wire
- not connected: ALERT

**5V buck #1 AP64500 (logic 5V)**
- IN → battery+ wire
- GND → GND wire
- OUT → INA260 #3 (5V line) IN+

**5V buck #2 AP64500 (servo 5V)**
- IN → battery+ wire
- GND → GND wire
- OUT → 5V-servo thick-wire pads pin 1 (+5VS)

**8V buck (steering servos)**
- IN → battery+ wire
- GND → GND wire
- OUT → INA260 #2 (8V servo line) IN+

**3.3V buck #1 AP62301**
- IN → 5V wire
- GND → GND wire
- OUT → 3.3V wire

**3.3V buck #2 AP62301**
- IN → 5V wire
- GND → GND wire
- OUT → JST-4 POWER to BACKUP pin 3 (+3V3_B) + JST-4 POWER to PARACHUTE pin 3 (+3V3_B)

**Plugs and pads on the BOARD 3 POWER (zero board)** (pin 1 = first pin)
- battery thick-wire pads: pin 1 BAT+ → ON/OFF switch pads pin 1 (SW_A) | pin 2 BAT− → GND wire
- ON/OFF switch pads: pin 1 SW_A → battery thick-wire pads pin 1 (BAT+) | pin 2 SW_B → fuse (F1) → INA260 #1 (battery) pin IN+ + 1k resistor (R1) → red LED (D1) [anode leg here] → GND wire
- fuse pads: pin 1 IN → ON/OFF switch pads pin 2 (SW_B) + 1k resistor (R1) → red LED (D1) [anode leg here] → GND wire | pin 2 OUT → INA260 #1 (battery) pin IN+
- ESC power thick-wire pads: pin 1 VBAT → battery+ wire | pin 2 GND → GND wire
- JST-3 I2C to MAIN: pin 1 SCL → INA260 #1 (battery) pin SCL + INA260 #2 (8V servo line) SCL + INA260 #3 (5V line) SCL | pin 2 SDA → INA260 #1 (battery) pin SDA + INA260 #2 (8V servo line) SDA + INA260 #3 (5V line) SDA | pin 3 GND → GND wire
- JST-4 POWER to MAIN: pin 1 +5V → 5V wire | pin 2 GND → GND wire | pin 3 +3V3_A → 3.3V wire | pin 4 GND → GND wire
- JST-4 POWER to BACKUP: pin 1 +5V → 5V wire | pin 2 GND → GND wire | pin 3 +3V3_B → 3.3V buck #2 AP62301 OUT + JST-4 POWER to PARACHUTE pin 3 (+3V3_B) | pin 4 GND → GND wire
- JST-4 POWER to PARACHUTE: pin 1 +5V → 5V wire | pin 2 GND → GND wire | pin 3 +3V3_B → 3.3V buck #2 AP62301 OUT + JST-4 POWER to BACKUP pin 3 (+3V3_B) | pin 4 GND → GND wire
- 8V thick-wire pads: pin 1 +8V → 8V-servo wire | pin 2 +8V → 8V-servo wire | pin 3 GND → GND wire | pin 4 GND → GND wire
- 5V-servo thick-wire pads: pin 1 +5VS → 5V buck #2 AP64500 (servo 5V) OUT | pin 2 GND → GND wire
- JST-2 BATTERY SENSE to MAIN: pin 1 VBAT → battery+ wire | pin 2 GND → GND wire

## CABLES between boards (all straight: pin 1 ↔ pin 1, pin 2 ↔ pin 2 …)

- **W1** BOARD 3 POWER (zero board) JST-4 POWER to MAIN ↔ BOARD 1 MAIN (etched) JST-4 POWER IN — 22 AWG ×4 — pins: 1=+5V, 2=GND, 3=+3V3_A, 4=GND
- **W2** BOARD 3 POWER (zero board) JST-4 POWER to BACKUP ↔ BOARD 2 BACKUP + SENSOR (etched) JST-4 POWER IN — 22 AWG ×4 — pins: 1=+5V, 2=GND, 3=+3V3_B, 4=GND
- **W3** BOARD 3 POWER (zero board) JST-4 POWER to PARACHUTE ↔ BOARD 4 PARACHUTE (etched) JST-4 POWER IN — 22 AWG ×4 — pins: 1=+5V, 2=GND, 3=+3V3_B, 4=GND
- **W4** BOARD 3 POWER (zero board) 8V thick-wire pads ↔ BOARD 4 PARACHUTE (etched) 8V thick-wire pads — 18 AWG ×2 — pins: 1=+8V, 2=+8V, 3=GND, 4=GND
- **W5** BOARD 3 POWER (zero board) 5V-servo thick-wire pads ↔ BOARD 4 PARACHUTE (etched) 5V-servo thick-wire pads — 20 AWG ×2 — pins: 1=+5VS, 2=GND
- **W6** BOARD 3 POWER (zero board) JST-3 I2C to MAIN ↔ BOARD 1 MAIN (etched) JST-3 I2C to POWER — 26 AWG ×3 — pins: 1=SCL, 2=SDA, 3=GND
- **W7** BOARD 3 POWER (zero board) JST-2 BATTERY SENSE to MAIN ↔ BOARD 1 MAIN (etched) JST-2 BATTERY SENSE — 26 AWG ×2 — pins: 1=VBAT, 2=GND
- **W9** BOARD 1 MAIN (etched) JST-5 LINK cable to BACKUP ↔ BOARD 2 BACKUP + SENSOR (etched) JST-5 LINK cable from MAIN — 26 AWG ×5 — pins: 1=HB, 2=GNSS, 3=MUX_TX, 4=MUX_RX, 5=GND
- **W10** BOARD 1 MAIN (etched) JST-3 UART to PARACHUTE ↔ BOARD 4 PARACHUTE (etched) JST-3 UART from MAIN — 26 AWG ×3 — pins: 1=RX3, 2=TX3, 3=GND
- **W11** BOARD 1 MAIN (etched) JST-2 GPS copy to PARACHUTE ↔ BOARD 4 PARACHUTE (etched) JST-2 GPS copy from MAIN — 26 AWG ×2 — pins: 1=GNSS, 2=GND
- **W12** BOARD 2 BACKUP + SENSOR (etched) JST-3 link to PARACHUTE ↔ BOARD 4 PARACHUTE (etched) JST-3 link to BACKUP — 26 AWG ×3 — pins: 1=RX3, 2=TX3, 3=GND
