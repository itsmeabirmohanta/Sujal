# Board 4 rework — ICM-20948 out, Adafruit LSM6DSO32 in

Every other part, trail and wire link stays exactly as you built it (same 5 wire links LK1–LK5).
Hole = C(column) R(row), same numbering as the build list. A step `C30R32–C31R32` = the solder bridge / trail between those two holes.

## 1. LSM6DSO32 position — solder the FRONT row only
| LSM pin | hole | goes to |
|---|---|---|
| VIN | C28R31 | +3V3_B |
| 3Vo | C29R31 | leave open |
| GND | C30R31 | GND |
| SCL | C31R31 | SCL |
| SDA | C32R31 | SDA |
| DO | C33R31 | GND |
| CS | C34R31 | +3V3_B |
| INT1 | C35R31 | leave open |
| INT2 | C36R31 | leave open |

Back row (SCX, SDX, CS, DO, GND): no header pins — holes C30R36, C31R36, C32R36, C33R36, C34R36 stay empty.
Result: I²C address **0x6A** (DO→GND), I²C mode (CS→3V3). 3Vo is the board's own 3.3 V output — leave it open.

## 2. CUT — copper that only served the ICM (desolder / cut these steps if you already made them)
- C26R34–C26R35  (was ICM_1V8)
- C26R35–C26R36  (was ICM_1V8)
- C26R36–C27R36  (was ICM_1V8)
- C27R36–C28R36  (was ICM_1V8)
- C28R31–C29R31  (was +3V3_B)
- C28R36–C29R36  (was ICM_1V8)
- C29R31–C30R31  (was +3V3_B)
- C29R36–C30R36  (was ICM_1V8)
- C30R36–C31R36  (was ICM_1V8)
- C31R31–C31R32  (was ICM_1V8)
- C32R31–C32R32  (was GND)
- C32R35–C32R36  (was GND)
- C32R35–C33R35  (was GND)
- C33R29–C33R30  (was SCL)
- C33R30–C33R31  (was SCL)
- C33R32–C34R32  (was GND)
- C33R35–C34R35  (was GND)
- C34R30–C34R31  (was SDA)
- C34R32–C35R32  (was GND)
- C34R34–C34R35  (was GND)
- C35R32–C36R32  (was GND)
- C35R34–C35R35  (was GND)
- C35R35–C35R36  (was GND)
- C36R27–C36R28  (was GND)
- C36R27–C37R27  (was GND)
- C36R28–C36R29  (was GND)
- C36R29–C36R30  (was GND)
- C36R30–C36R31  (was GND)
- C36R31–C36R32  (was GND)

## 3. ADD — new solder trail steps for the LSM6DSO32
- C26R34–C27R34  (GND)
- C27R34–C28R34  (GND)
- C28R34–C29R34  (GND)
- C29R34–C30R34  (GND)
- C30R31–C30R32  (GND)
- C30R34–C31R34  (GND)
- C31R29–C31R30  (SCL)
- C31R29–C32R29  (SCL)
- C31R30–C31R31  (SCL)
- C31R32–C32R32  (GND)
- C31R34–C32R34  (GND)
- C32R29–C33R29  (SCL)
- C32R30–C32R31  (SDA)
- C32R30–C33R30  (SDA)
- C32R34–C33R34  (GND)
- C33R30–C34R30  (SDA)
- C33R31–C33R32  (GND)
- C33R34–C34R34  (GND)
- C34R31–C34R32  (+3V3_B)
- C34R32–C34R33  (+3V3_B)

## 4. Same copper, now a different signal (only valid after the cuts above)
- C26R32–C26R33: was ICM_1V8, now GND
- C26R32–C27R32: was ICM_1V8, now GND
- C26R33–C26R34: was ICM_1V8, now GND
- C27R32–C28R32: was ICM_1V8, now GND
- C28R32–C29R32: was ICM_1V8, now GND
- C29R32–C30R32: was ICM_1V8, now GND
- C30R32–C31R32: was ICM_1V8, now GND

## Wire links
- unchanged: 5 links, none added, none removed
- LK1 (C27R31 ↔ C27R33) now ends right at the edge under the LSM board — fine, the LSM stands on its header.
