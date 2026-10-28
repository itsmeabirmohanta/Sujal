# VRITRA CanSat — Complete Project Brief
## For AI assistants with no prior knowledge of this project

---

## 1. Competition & Team

| Field | Value |
|-------|-------|
| Competition | IN-SPACe CAN-7USAT 2026 (India Student CanSat) |
| Team ID | 2026-INSPACe-CAN-7USAT-025 |
| Institution | Lovely Professional University |
| CanSat name | VRITRA |
| Mission profile | Launched to ~1000 m AGL, ejected, descends under parachute at <20 m/s, deploys secondary descent control at 600 m AGL, must transmit 1 Hz telemetry throughout |

---

## 2. Hardware Architecture

### 2.1 Flight Computer — Teensy 4.1 (PJRC)
- ARM Cortex-M7 at 600 MHz
- 1 MB RAM, 8 MB Flash
- Bare-metal Arduino C++ (no RTOS)
- Custom firmware: `vritra_fc/` (`.ino` + multiple `.cpp`/`.h` files)
- **Firmware version**: 6.1.0

### 2.2 Ground Station — ESP32 (generic dev board)
- Custom gateway firmware: `vritra_gs/vritra_gs.ino`
- Receives LoRa downlink, forwards to PC via USB serial
- Also sends uplink commands (CMD_TX_ON, CMD_ARM, etc.) to CanSat

### 2.3 GCS Software — Python
- `vritra_gcs.py` — runs on the PC, receives CSV from gateway, displays live telemetry in a GUI with a 3D attitude model

---

## 3. Sensors & Modules — All 7Semi Breakout Boards

> **Important**: Every sensor and radio module listed below is a breakout board
> purchased from **7Semiconductor (7Semi)**. The 7Semi breakout just exposes
> the chip on a small PCB with decoupling and level shifting. The underlying
> chip manufacturer is noted separately.

### 3.1 Barometer — 7Semi BMP585 Breakout
| Field | Value |
|-------|-------|
| Chip | Bosch BMP585 |
| Interface | I2C |
| I2C address | 0x47 (SDO pin pulled HIGH → hardcodes address) |
| Teensy pins | SDA=18, SCL=19 (shared I2C0 bus) |
| CS pin | 3.3V (forces I2C mode, disables SPI) |
| INT pin | Teensy pin 0 (wired but polled, not used as interrupt) |
| CHIP_ID | 0x51 (verified by bring-up test) |
| Measurement | Pressure (Pa), Temperature (°C), derived altitude (AGL m) |
| Sample rate | 25 Hz in firmware (50 Hz service tick, baro alternates) |

### 3.2 IMU — 7Semi ICM-20948 Breakout
| Field | Value |
|-------|-------|
| Chip | TDK InvenSense ICM-20948 (9-DOF: accel + gyro + mag) |
| Interface | I2C |
| I2C address | 0x69 (AD0/SDO pin pulled HIGH) |
| Teensy pins | SDA=18, SCL=19 (shared I2C0 bus) |
| CS pin | 3.3V (forces I2C mode) |
| FS pin | GND (disables frame sync injection) |
| INT pin | Teensy pin 41 (wired but polled, not used as interrupt) |
| WHO_AM_I | 0xEA (verified by bring-up test) |
| Accel config | ±16 g, DLPF enabled |
| Gyro config | ±2000 dps, DLPF 11.6 Hz (GYRO_DLPFCFG=5), GYRO_CONFIG_1=0x2F |
| Sample rate | 50 Hz service tick |
| **Important** | DS3231 RTC is at 0x68 — the ICM-20948 is NOT at 0x68 on this board (AD0 is HIGH) |

### 3.3 Magnetometer — AK09916 (inside ICM-20948 package)
| Field | Value |
|-------|-------|
| Chip | Asahi Kasei AK09916 (3-axis magnetic sensor) |
| Access | Via ICM-20948 I2C bypass (INT_PIN_CFG bit 1 set) |
| I2C address | 0x0C (when bypass enabled) |
| Mode | Continuous 100 Hz (CNTL2=0x08) |
| **Frame note** | AK09916 Y and Z axes are negated relative to the ICM accel/gyro frame — firmware applies the negation in Sensors.cpp |
| Status | **Not currently fitted** (no physical magnetometer module connected in current prototype) |

### 3.4 GNSS — 7Semi L89HA Breakout
| Field | Value |
|-------|-------|
| Module | 7Semi L89HA (their GNSS breakout board) |
| Internal chip | **Airoha AG3335M** (confirmed: module outputs `$PAIR021,AG3335M_V2.5.0.AG3335_20221130`) |
| Interface | UART at 9600 baud |
| Teensy port | Serial2 — RX=7 (← L89HA TX), TX=8 (→ L89HA RX) |
| RST pin | Teensy pin 6 (active LOW, idle HIGH) |
| WAKE_UP pin | Strapped to 3.3V on harness (no Teensy pin needed) |
| Protocol | **$PMTK** (Airoha/MediaTek native) + **$PQTM** (Quectel extensions). NOT $PSTM. |
| Factory default | GPS + Galileo + NavIC/IRNSS already enabled |
| Config sent | `$PMTK353,1,1,1,0,1` — adds GLONASS (GPS+GLONASS+Galileo+NavIC, no BDS) |
| NMEA outputs | $GNGGA, $GNRMC, $GNVTG, $GNGSA, $GPGSV, $GAGSV, $GIGSV, $PQTMANTENNASTATUS |
| **Critical** | $PSTM commands (ST Teseo III protocol) are silently ignored by this chip |

### 3.5 LoRa Radio — 7Semi LR1121 Breakout
| Field | Value |
|-------|-------|
| Chip | Semtech LR1121 (LoRa + FSK + GFSK, sub-GHz and 2.4 GHz) |
| Interface | SPI (hardware SPI1 on Teensy 4.1) |
| SPI pins | MOSI=26, SCK=27, MISO=39 |
| Control pins | CS=29, RESET=25, BUSY=24, DIO9/IRQ=30 |
| DIO7, DIO8 | Left unconnected |
| Library | RadioLib |
| **MISO trap** | SPI1 default MISO is pin 1. Firmware calls `SPI1.setMISO(39)` BEFORE `SPI1.begin()` — if omitted, the chip returns error -707 even with perfect soldering |
| RF parameters | 867.00 MHz, SF10, BW125 kHz, CR 4/5, sync 0x98, +14 dBm HP PA |
| Backup channel | 865.75 MHz |
| TCXO | Fitted — requires host to re-run image calibration (`calibrateImageRejection(863, 870)`) since POR calibrations are skipped with TCXO |

---

## 4. Actuator & Power

### 4.1 Gyro-Stabilisation Wheel — TB6612FNG + N20 Motor
| Field | Value |
|-------|-------|
| Driver chip | Toshiba TB6612FNG dual H-bridge motor driver |
| Logic supply | 3.3V |
| Motor supply | Direct from battery positive |
| Teensy pins | PWMA=33, AIN1=35, AIN2=34, STBY=36 |
| Motor | GA12-N20 brushed DC motor (gyroscopic stabilisation wheel) |

### 4.2 RTC — DS3231
| Field | Value |
|-------|-------|
| Chip | Maxim DS3231 (precision RTC with TCXO) |
| Interface | I2C |
| I2C address | 0x68 (shares bus with BMP585 and ICM-20948) |
| Teensy pins | SDA=18, SCL=19 |
| SQW/INT pin | Teensy pin 22 (wired, currently unused in firmware) |
| Note | RTC time was 2000-01-01 in bring-up test — coin cell may be flat or never set |

### 4.3 Bus Voltage Sense
| Field | Value |
|-------|-------|
| Teensy pin | A6 = digital pin 20 |
| Purpose | Monitor battery voltage, report as BUS_V in telemetry |

### 4.4 Battery
| Field | Value |
|-------|-------|
| Type | LiPo (cell count under clarification — PDR says 3S, bench reads ~7.87V suggesting 2S) |

---

## 5. Physical Form Factor

| Dimension | Value |
|-----------|-------|
| Outer diameter | 130 mm |
| Height | 152 mm |
| Aluminium separation plates | Ø110 mm — resonant at 867 MHz; must NOT be aluminium (use non-conductive material near the LoRa antenna) |

---

## 6. Firmware Architecture (`vritra_fc/`)

### Files
| File | Purpose |
|------|---------|
| `vritra_fc.ino` | Main loop, flight state machine, telemetry packet builder |
| `Config.h` | All pins, RF parameters, wire format constants — single source of truth |
| `Sensors.cpp / .h` | Non-blocking staged init + 50 Hz service for BMP585, ICM-20948, AK09916, DS3231, L89HA, bus sense |
| `Radio.cpp / .h` | Non-blocking LoRa state machine: RS_INIT → RS_IDLE ↔ RS_TXING ↔ RS_RX_WINDOW, RS_RX_CONT |
| `Logger.cpp / .h` | SD card telemetry logging |
| `Wheel.cpp / .h` | Soft-start ramp for gyro wheel |
| `Base36.h` | Base-36 encoding for wire format |

### Boot sequence (§6.1.v compliance)
1. Boots **SILENT** — radio in continuous receive (RS_RX_CONT), no TX
2. Sends `$PMTK353,1,1,1,0,1` to GNSS during staged init
3. Ground station sends `CMD_TX_ON` via LoRa uplink (blind timer, every 1.2 s)
4. FC receives command → `Radio.setTxEnabled(true)` → starts 1 Hz downlink

### Attitude estimation
- **Roll/Pitch**: `atan2` on accelerometer — gravity-anchored, no drift
- **Yaw**: Gyro Z integration from zero datum — drifts slowly
- **Datum**: Captured at ATT_ZERO_DELAY_MS (2000 ms) after boot; `CMD_CALIBRATE` re-zeros
- **Gate**: Attitude only updated when 0.75 g < |a| < 1.25 g (ignores thrust and shock)
- **No quaternion** — simple Euler angles transmitted and displayed

---

## 7. Wire Format (Telemetry)

- **Encoding**: Base-36 ASCII CSV
- **Schema**: N_FIELDS = 87 fields
- **Key positions**: 0=TEAM_ID, 1=TIME_S, 2=PKT, 3=ALT_M, 4=PRES_PA, 5=TEMP_C, 6=BUS_V, 7=GNSS_TIME, 8=LAT, 9=LON, 10=GPS_ALT, 11=SATS, 12=ACC_X, 13=ACC_Y, 14=ACC_Z, 15=SPIN, 16=STATE, 17=FLAGS, 18=ATT_ROLL, 19=ATT_PITCH, 20=ATT_YAW ...
- **Schema sum**: 0x007C (must match between FC and gateway or all fields silently misalign)
- **Uplink format**: `025,C,<cmdId>,<arg>,<seq>` (short team ID, 12 bytes, 288 ms at SF10)

---

## 8. Known Issues & Traps (Read Before Touching Code)

| Issue | Detail |
|-------|--------|
| SPI1 MISO mux | `SPI1.setMISO(39)` MUST precede `SPI1.begin()`. Omitting it = error -707 with perfect hardware. |
| GNSS protocol | Module is AG3335M. Use `$PMTK`, NOT `$PSTM`. $PSTM is silently ignored. |
| ICM-20948 address | Always 0x69. DS3231 is at 0x68. Don't swap them. |
| AK09916 frame | Y and Z axes negated in firmware to align with accel/gyro frame. |
| LR1121 image cal | Must call `calibrateImageRejection(863, 870)` after `begin()` because TCXO skips POR calibrations. |
| CanSat boots SILENT | Radio is in RX-only until `CMD_TX_ON` received. Ground station sends this on blind timer. |
| Schema sum | Both FC and gateway must compile the same `Base36.h`. Mismatch = silent field misalignment. |
| I2C speed | Bus runs at 100 kHz (400 kHz caused ICM-20948 failures on this harness). |
| 0x57 on I2C | Unknown device at 0x57 found during bring-up scan — unidentified, investigate. |

---

## 9. Development Environment

| Tool | Version/Detail |
|------|----------------|
| Arduino IDE | With Teensyduino for FC; ESP32 board support for gateway |
| RadioLib | Used for LR1121 on both FC and gateway |
| Language | Arduino C++ (bare metal, no RTOS) |
| OS | Windows 10 |
| Working directory | `C:\Users\kalbh\Downloads\Cansat Ground station v2\` |
| Python | `vritra_gcs.py` — GCS display, 3D attitude model, command panel |
