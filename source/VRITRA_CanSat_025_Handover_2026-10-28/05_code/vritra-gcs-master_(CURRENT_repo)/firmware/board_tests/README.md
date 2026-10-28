# VRITRA board tests (final flight boards, 2026-10-28)

Bench tests for the boards that fly. Each prints a PASS / FAIL table at power-up, then takes single-key commands in
the Serial Monitor at **115200 baud** (press `h` for the list).

| Sketch | Board | Arduino board setting |
|---|---|---|
| `board1_main_test` | 1 MAIN, Teensy 4.1 | Teensy 4.1, USB Type: Serial |
| `board2_backup_test` | 2 BACKUP + SENSOR, Teensy 4.1 | Teensy 4.1, USB Type: Serial |
| `board4_parachute_test` | 4 PARACHUTE, Waveshare **ESP32-P4-Pico** | ESP32P4 Dev Module (esp32 core 3.3.x), USB CDC On Boot: Disabled, Chip Variant = the revision esptool prints (v1.x → "Before v3.00") |
| `../vritra_cam` | XIAO ESP32S3 Sense camera | XIAO_ESP32S3, **PSRAM: OPI PSRAM** |
| `../vritra_gs` | Ground station gateway, ESP32 + Core1121 | ESP32 Dev Module — pins unchanged (NSS 5, SCK 18, MOSI 23, MISO 19, RST 14, BUSY 27, IRQ 26) |

Radio everywhere (IN-SPACe 19-10-2026, team 025): **866.50 MHz main / 865.25 MHz backup, SF9, BW 125 kHz,
CR 4/5, sync 0x99**, antenna on ANT2. Board 1 only transmits when you press `r`.

## Board 4 pin map (ESP32-P4-Pico header pin → GPIO)
| Signal | P4 pin | GPIO | Goes to |
|---|---|---|---|
| SDA / SCL | 17 / 16 | 7 / 8 | BMP585 #3 0x47, LSM6DSO32 0x6A (ES8311 codec 0x18 also answers) |
| STEER_L / STEER_R | 35 / 34 | 20 / 21 | 330 Ω → J10 / J11 (B26CLM, centre 1520 µs) |
| BAY / PETRI | 32 / 31 | 22 / 23 | 330 Ω → J12 / J13 (MG90S) |
| SERVO_X / ESC | 26 / 25 | 32 / 33 | 330 Ω → J16 / J14 |
| TX2 | 1 | 54 | 1 kΩ → J2 (MAIN) and 1 kΩ → J3 (BACKUP) |
| RX2 / RX3 | 2 / 6 | 19 / 16 | J2 (from MAIN) / J3 (from BACKUP) |
| GNSS_IN / HALL | 9 / 11 | 14 / 5 | J5 / J15 |
| +5 V in | 39 | VSYS | J8 pin 1 |

## Link test between boards
Run all three tests. On Board 1 press `p` → Board 4 answers `B4 PONG B1 PING ...` (also seen by Board 2).
On Board 2 press `p` → Board 4 answers `B4 PONG B2 PING ...`. On Board 4 press `u` → both other boards print `B4 PING n`.

## Checked
Every sketch compiles with 0 errors / 0 warnings (Teensy 1.62 g++, esp32 core 3.3.11 compilers, RadioLib 7.x).
Not yet run on the hardware.
