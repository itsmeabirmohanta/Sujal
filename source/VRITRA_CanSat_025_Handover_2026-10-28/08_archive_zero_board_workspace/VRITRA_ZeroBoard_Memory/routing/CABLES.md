# VRITRA zero-board harness — every cable, pin by pin

All board-to-board cables are STRAIGHT-THROUGH: pin 1 to pin 1, pin 2 to pin 2 … Pin 1 is the square / first pin listed on each board sheet. Verified automatically by `system_view.py`.

## W1  E.J5 ↔ A.J1  (22 AWG ×4) — power: +5V_LOGIC, GND, +3V3_A, GND
| pin | E.J5 label | net on E | A.J1 label | net on A |
|---|---|---|---|---|
| 1 | +5V | +5V_LOGIC | +5V | +5V_LOGIC |
| 2 | GND | GND | GND | GND |
| 3 | +3V3_A | +3V3_A | +3V3 | +3V3_A |
| 4 | GND | GND | GND | GND |

## W2  E.J6 ↔ B.J1  (22 AWG ×4) — power: +5V_LOGIC, GND, +3V3_B, GND
| pin | E.J6 label | net on E | B.J1 label | net on B |
|---|---|---|---|---|
| 1 | +5V | +5V_LOGIC | +5V | +5V_LOGIC |
| 2 | GND | GND | GND | GND |
| 3 | +3V3_B | +3V3_B | +3V3 | +3V3_B |
| 4 | GND | GND | GND | GND |

## W3  E.J7 ↔ D.J8  (22 AWG ×4) — power: +5V_LOGIC, GND, +3V3_B, GND
| pin | E.J7 label | net on E | D.J8 label | net on D |
|---|---|---|---|---|
| 1 | +5V | +5V_LOGIC | +5V | +5V_LOGIC |
| 2 | GND | GND | GND | GND |
| 3 | +3V3_B | +3V3_B | +3V3 | +3V3_B |
| 4 | GND | GND | GND | GND |

## W4  E.J8 ↔ D.J1  (18 AWG ×2) — +8V_STEER for the A20CLM servos
| pin | E.J8 label | net on E | D.J1 label | net on D |
|---|---|---|---|---|
| 1 | +8V | +8V_STEER | +8V | +8V_STEER |
| 2 | +8V | +8V_STEER | +8V | +8V_STEER |
| 3 | GND | GND | GND | GND |
| 4 | GND | GND | GND | GND |

## W5  E.J9 ↔ D.J6  (20 AWG ×2) — +5V_SERVO for the MG90S servos
| pin | E.J9 label | net on E | D.J6 label | net on D |
|---|---|---|---|---|
| 1 | +5VS | +5V_SERVO | +5VS | +5V_SERVO |
| 2 | GND | GND | GND | GND |

## W6  E.J2 ↔ A.J2  (26 AWG ×3) — I²C to the three INA260
| pin | E.J2 label | net on E | A.J2 label | net on A |
|---|---|---|---|---|
| 1 | SCL | SCL | SCL | SCL |
| 2 | SDA | SDA | SDA | SDA |
| 3 | GND | GND | GND | GND |

## W7  E.J10 ↔ A.J3  (26 AWG ×2) — VBAT to the A pin-20 divider
| pin | E.J10 label | net on E | A.J3 label | net on A |
|---|---|---|---|---|
| 1 | VBAT | VBAT | VBAT | VBAT_SENSE |
| 2 | GND | GND | GND | GND |

## W9  A.J5 ↔ B.J2  (26 AWG ×5) — heartbeat + GNSS copy + camera command UART (MAIN → mux ch1)
| pin | A.J5 label | net on A | B.J2 label | net on B |
|---|---|---|---|---|
| 1 | HB | HB_J5 | HB | HB_IN |
| 2 | GNSS | GNSS_B | GNSS | GNSS_IN |
| 3 | MUX_TX | CAM_TX_J | MUX_TX | CAM_TX_J |
| 4 | MUX_RX | CAM_RX | MUX_RX | CAM_RX |
| 5 | GND | GND | GND | GND |

## W10  A.J4 ↔ D.J2  (26 AWG ×3) — Serial3 (A) ↔ Serial2 (D)
| pin | A.J4 label | net on A | D.J2 label | net on D |
|---|---|---|---|---|
| 1 | RX3 | RX3 | TX2→A | TX2_A |
| 2 | TX3 | TX3_J4 | RX2←A | RX2 |
| 3 | GND | GND | GND | GND |

## W11  A.J8 ↔ D.J5  (26 AWG ×2) — GNSS copy to parafoil
| pin | A.J8 label | net on A | D.J5 label | net on D |
|---|---|---|---|---|
| 1 | GNSS | GNSS_D | GNSS | GNSS_IN |
| 2 | GND | GND | GND | GND |

## W12  B.J3 ↔ D.J3  (26 AWG ×3) — Serial3 (B) ↔ D telemetry / commands
| pin | B.J3 label | net on B | D.J3 label | net on D |
|---|---|---|---|---|
| 1 | RX3 | RX3 | TX→B | TX2_B |
| 2 | TX3 | TX3_J | RX3←B | RX3 |
| 3 | GND | GND | GND | GND |

## External connections
| id | board.connector | device | wire | pins |
|---|---|---|---|---|
| X1 | E.J1 | 3S Molicel P50B pack (XT60 lead) | 16 AWG | BAT+ / BAT− |
| X2 | E.J3 | KCD1 rocker switch on the shell (COM + one ON throw) | 18 AWG | SW_A / SW_B |
| X3 | E.F1 | 10 A fuse (wire link until fitted) | 18 AWG | IN / OUT |
| X4 | E.J4 | Cyclone 45A ESC thick + / − wires (after switch, fuse, INA260 #1) → EMAX 2205 (3 motor wires) | 16-18 AWG | VBAT / GND |
| X5 | D.J14 | Cyclone 45A BLHeli_S ESC: its 2 thin wires (ground, signal) | 2-wire | − / S |
| X6 | D.J10 | A20CLM LEFT steering servo, 2.54 servo header | servo lead | − + S |
| X7 | D.J11 | A20CLM RIGHT steering servo, 2.54 servo header | servo lead | − + S |
| X8 | D.J12 | MG90S #1 — parachute bay (opens the ram-air bay at 600 m), 2.54 servo header | servo lead | − + S |
| X9 | D.J13 | MG90S #2 — petri plate lid, 2.54 servo header | servo lead | − + S |
| X16 | D.J16 | MG90S #3 — extra (Teensy 4.0 pin 10), 2.54 servo header | servo lead | − + S |
| X10 | D.J15 | Hall sensor on the gyro-wheel mount (magnet on wheel) | 3-wire | − +5V OUT |
| X11 | B.J4 | XIAO ESP32-S3 camera via mux: J4.3→XIAO D7(RX), XIAO D6(TX)→J4.4 | 4-wire | 5V GND RX TX |
| X15 | A.U9 | beacon speaker on the MAX98357A screw terminal (+ / −, neither is GND) | 2-wire | SPK+ SPK− |
| X12 | B.M1L | HDC3020 JST-PH cable (+ − D C) soldered to Board 2 | module cable | 3V3 GND SDA SCL |
| X13 | A.J7 | CR2032/CR1220 holder for GNSS backup | 2-wire | CELL+ / GND |
| X14 | A.U2 | RadioMaster T-antenna on the Core1121 u.FL (sub-GHz port) | u.FL→RP-SMA | RF |
