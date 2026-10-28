# 03 — PIN MAP AND WIRING (zero-board build)

> **2026-10-24 — 4 BOARDS NOW (see 04 change log "BOARD MERGE").** Board 1 MAIN (old A) · Board 2 BACKUP + SENSOR
> (old B + C: BMP585 #2, LSM6DSO32, HDC3020, mux, camera plug, **microSD on backup Teensy CS10 SCK13 MOSI11 MISO12, 5 V**)
> · Board 4 POWER (old E) · Board 5 PARACHUTE (old D, no SD). Build from `kicad_single/BOARD<n>_<NAME>_1L/`
> (PNG + KiCad + BUILD_LIST). Anything below about board C, the D microSD or MAIN J6/J9 wiring is superseded.

STATUS: **v0.3 (2026-10-22) — hole-level routed build, waiting for user sign-off.**
AUTHORITATIVE SOURCE = `routing/boards.py` (placement + nets per board, every Teensy pin cross-checked in code)
and `routing/system_view.py` (13 cables, all straight-through, checked end to end). Generated outputs per board:
`routing/<A..E>_top.png` (component side: module outline + every pin hole), `<A..E>_bottom.png` (solder side, mirrored:
traces hole by hole, hops), `<A..E>_wiring.md` (hole list per net), `CABLES.md`, `SYSTEM_interconnect.png`.
Regenerate: `cd routing && python boards.py && python system_view.py` (exit code 0 = all checks pass).
Boards are the full 120 × 150 mm (user cut them), 47 × 59 holes, hole C1 R1 = bottom-left on the component side,
3 mm edge margin for the slot mount. Signals = solder-side traces; GND/3V3/5V/servo power = insulated wires on the
component side (minimum-spanning-tree, pin to pin); high current (battery, ESC, 8 V) = thick wire, never traces.

### v0.3 changes vs v0.2 (the tables below are v0.2 — where they differ, boards.py wins)
| Board | Change | Why |
|---|---|---|
| A | Teensy upright, USB top / microSD bottom; radio + GNSS + flash on the left, I²C sensors on the right | planar routing: 47 → 20 hops |
| A | **FLASH_CS 38 → 9** | keeps all SPI1 flash wiring on the left side |
| A | **ENV_UV 40 → 22** (O3 stays 23) | sits next to O3 on the right column |
| A | BMP585 INT (pin 0) and ICM INT (41) **not wired** | firmware only polls them |
| A | L89 3D_FIX not wired | pins 33–36 are firmware outputs (old motor driver) |
| A | DS3231 mounted FLAT on its 4-pin end (SCL SDA VCC GND); SQW (22) not wired | 6-pin end is right-angle → module would stand 38 mm |
| A | **J7 GNSS_BAT** + 2 Schottky diodes OR-ing +3V3_A and the cell into L89 BAT | GNSS backup supply, cell never charged |
| A | D link split: **J4 = RX3, TX3, GND** and **J8 = GNSS, GND** | straight-through cables, no crossover |
| B | J2 = HB, GNSS, GND (same order as A J5); LSM6DSO32 CS→3V3, DO→GND (0x6A) | straight-through |
| D | J2 = TX2→A, RX2←A, GND; J5 = GNSS, GND; 330 Ω on every servo/ESC signal; SD module flat on a straight header | straight-through + pin protection |
| E | 5 buck modules as boxes + IN/GND/OUT landing holes (no module pin order assumed); INA260 VCC = +3V3_A | 7Semi pad layout unverified; I²C pull-ups must be 3.3 V |

Key rule of v0.2: **board A uses exactly the pins of the working flight firmware**
`vritra-gcs-master/firmware/vritra_fc/Config.h` (harness rev 2026-08) so the FRR demo needs no
pin changes. Pins marked `*` are NEW and need firmware work. KiCad rev E pins are NOT used for A.

## §0 Teensy capability table (PJRC pinout cards) — check every pin against this
| Function | Teensy 4.1 | Teensy 4.0 (top pins only) |
|---|---|---|
| Serial1 / 2 / 3 RX,TX | 0,1 / 7,8 / 15,14 | 0,1 / 7,8 / 15,14 |
| Serial4 / 5 RX,TX | 16,17 / 21,20 | 16,17 / 21,20 |
| Serial6 / 7 / 8 | 25,24 / 28,29 / 34,35 | bottom pads — avoid |
| SPI (CS,MOSI,MISO,SCK) | 10,11,12,13 (13 = LED) | 10,11,12,13 |
| SPI1 | MOSI 26, SCK 27, MISO 1 or **39**, CS 0 or 38 | bottom pads |
| Wire / Wire1 / Wire2 SDA,SCL | 18,19 / 17,16 / 25,24 | 18,19 / 17,16 / bottom |
| SAI1 I²S | OUT1A 7, LRCLK 20, BCLK 21 | same |
| SAI2 I²S | OUT2 2, LRCLK2 3, BCLK2 4, MCLK2 33 | same |
| PWM | 0–15, 18, 19, 22–25, 28, 29, 33, 36, 37, 42–47 | 0–15, 18, 19, 22, 23 |
| Same PWM submodule (same freq) | 2&3, 6&9, 7&8, 28&29 | 2&3, 6&9, 7&8 |
| Analog | A0–A17 = 14–27, 38–41 | A0–A9 = 14–23 |
| NEVER wire | On/Off, Program | On/Off, Program |
All Teensy I/O is 3.3 V and NOT 5 V tolerant. Teensy 3V3 pin ≈250 mA — do not power radios/GNSS from it.

## §1 Board A — PRIMARY Teensy 4.1 (firmware vritra_fc)
| Pin | Net | To | Note |
|---|---|---|---|
| 0 | BMP_INT | BMP585 #1 INT | firmware: input, polled |
| 2 / 3 / 4 | I2S DIN / LRCLK2 / BCLK2 | MAX98357A (NOT OWNED) | reserved — beacon is MANDATORY (5.2.v) |
| 37 | AMP_SD | MAX98357A SD | reserved |
| *5 | HB_OUT | →1 k→ B pin 4 | heartbeat |
| 6 | GNSS_RST | L89 RST | active low |
| 7 RX2 | GNSS_RX | L89 TX | 9600 baud (datasheet default) |
| 8 TX2 | GNSS_TX | L89 RX | PAIR config |
| 13 | LED_BUILTIN | — | firmware status LED; keep SPI0 unused |
| *14 TX3 | CMD_TO_D | →1 k→ D pin 7 | |
| *15 RX3 | TLM_FROM_D | ←1 k← D pin 8 | spin rate (field 14), recovery state |
| *16 SCL1 / *17 SDA1 | ENV bus | board C | HDC3020 0x44 + future env |
| 18 SDA / 19 SCL | I²C0 100 kHz | BMP585 #1, ICM-20948 #1, DS3231, INA260 ×3 (on E, via J2) | firmware bus; addresses below |
| 20 (A6) | BUS_V | VBAT via 100 k / 22 k divider | firmware battery voltage (ratio 122/22) — keep as backup to INA260 |
| 22 | RTC_SQW | DS3231 SQW | |
| *23 (A9) | ENV_O3 | board C | future |
| 24 | LORA_BUSY | Core1121 BUSY | |
| 25 | LORA_RST | Core1121 RESET | |
| 26 | MOSI1 | Core1121 MOSI + W25Q64 MOSI | SPI1 |
| 27 | SCK1 | Core1121 CLK + W25Q64 SCK | SPI1 |
| 29 | LORA_CS | Core1121 CS | 10 k pull-up to 3V3_A |
| 30 | LORA_IRQ | Core1121 DIO9 | |
| *31 / *32 | LED_OK / LED_FAULT | green / red LED + 470 Ω | |
| 33–36 | (old TB6612) | — | free; firmware still sets them as outputs — leave unconnected |
| *38 | FLASH_CS | W25Q64 CS | 10 k pull-up |
| 39 | MISO1 | Core1121 MISO + W25Q64 MISO | |
| *40 (A16) | ENV_UV | board C | future |
| 41 | ICM_INT | ICM-20948 #1 INT | |
| VIN | +5V_LOGIC | E | |
| 1, 9, 10, 11, 12, 21, 28 | spare | | 1 = Serial1 TX: firmware also opens Serial1 for GNSS auto-detect — keep 0/1 free of outputs |

I²C0 (pins 18/19) address table: BMP585 #1 **0x47** (SDO→3V3, firmware tries 0x47 then 0x46),
ICM-20948 #1 **0x69** (+AK09916 0x0C), DS3231 **0x68**, AT24C32 EEPROM **0x57**,
INA260 #1/#2/#3 **0x40 / 0x41 / 0x45** (PROPOSED — confirm 7Semi A0/A1 pads, then scan). All unique.
Pull-ups: modules already carry them; add none until rise time is measured (firmware runs 100 kHz because
400 kHz failed on flying leads).

Core1121-HF (photo-verified pin labels): top row LORA_ANT, GND, DIO9, DIO8, DIO7, NC, GND, 3.3V;
bottom row 2.4G_ANT, GND, CS, CLK, MOSI, MISO, RESET, BUSY. 19 × 22 mm, 2.54 mm castellated.
3.3 V (max 3.8 V). T-antenna on the sub-GHz u.FL / LORA_ANT. Firmware: TCXO 1.6 V, RF switch DIO5/DIO6
(proven). Never transmit without an antenna.

L89HA: 3D_FIX, WAKE(→3V3), SDA, SCL, GEO, JAM, RST(→pin 6), BAT(→3V3), TX(→pin 7, and via 1 k to B pin 0 and D pin 0),
RX(←pin 8), 3V3(+3V3_A), GND. Built-in patch + chip antenna (L1+L5). An external active antenna needs EXTERNAL bias
(datasheet "Antenna Power Supply: External").

## §2 Board B — BACKUP Teensy 4.1 (needs new firmware)
| Pin | Net | To |
|---|---|---|
| 0 RX1 | GNSS copy | ←1 k← L89 TX |
| 4 | HB_IN | ← A pin 5 (INPUT_PULLDOWN) |
| 14 TX3 / 15 RX3 | link D | →1 k→ D pin 15 / ←1 k← D pin 8 |
| 18 / 19 | I²C0 | BMP585 #2 (0x47), LSM6DSO32 (I²C, SDO/SA0→GND = 0x6A) |
| 34 RX8 / 35 TX8 | XIAO | ←1 k← XIAO D6 (TX) / →1 k→ XIAO D7 (RX) |
| 36 / 37 | LEDs | green / red + 470 Ω |
| 10–13 | reserved | 2nd Core1121 if bought |
Only ONE Core1121 flies (the other is the ground gateway), so B cannot transmit — independent logger only.

## §3 Board D — PARAFOIL Teensy 4.0 (needs new firmware — none exists yet)
| Pin | Net | To |
|---|---|---|
| 0 RX1 | GNSS | ←1 k← L89 TX (or M9N TX if it gets an antenna) |
| 1 TX1 | M9N RX | only if M9N used |
| 2 / 3 | STEER_L / STEER_R | A20CLM signal (8 V servos; 3.3 V signal OK) |
| 4 | BAY_LID | MG90S #1 — opens the ram-air bay at 600 m |
| 5 | PETRI_LID | MG90S #2 |
| 6 | ESC | ESC signal (servo-type PWM) |
| 7 RX2 / 8 TX2 | link A | ←1 k← A pin 14 / →1 k→ A pin 15 **and** B pin 15 |
| 9 | HALL | Hall sensor out, 10 k pull-up to **3.3 V** (sensor powered 5 V, open-collector out) |
| 10–13 | SD CS/MOSI/MISO/SCK | microSD module ONLY (its 74xx125 MISO is never tri-stated) |
| 15 RX3 | link B | ←1 k← B pin 14 |
| 18 / 19 | I²C0 | ICM-20948 #2 (0x69), BMP585 #3 (to buy) |
microSD module: pins CS, SCK, MOSI, MISO, VCC, GND (photo). VCC must be **5 V** (on-board AMS1117).

## §4 Board C — ENV / PAYLOAD (no MCU)
J1 from A: SDA1(17), SCL1(16), UV(40), O3(23), +3V3_A, GND, +5V (MQ-131 heater, from E), GND.
HDC3020 0x44 (JST-PH silk `+ - D C`). Future: SCD41 0x62, SEN0290 0x03, AS7343 0x39, BMV080 0x57
(**clashes with DS3231 EEPROM 0x57 if ever moved to I²C0 — keep it on Wire1**), ML8511 / MQ-131 analog.

## §5 Board E — POWER
```
3S P50B ─XT60PW─► rocker switch (KCD1, on shell) ─► FUSE 10–15 A (not owned) ─► INA260 #1 IN+→IN− ─► VBAT bus
                                                                                   └► red LED + 1 k (power indicator)
VBAT ─XT30─► ESC ─► EMAX 2205                         (ESC BEC red wire NOT connected)
VBAT ─► AP64500 #1 ─► INA260 #3 ─► +5V_LOGIC ─► Teensy A, B, D VIN · XIAO 5V · SD module · Hall · MQ-131
VBAT ─► AP64500 #2 ─────────────► +5V_SERVO ─► MG90S ×2
VBAT ─► 8 V multi-output buck ─► INA260 #2 ─► +8V_STEER ─► A20CLM ×2 only
+5V_LOGIC ─► AP62301 #1 ─► +3V3_A ─► A (Core1121, L89, BMP, ICM, DS3231, W25Q64) + C
+5V_LOGIC ─► AP62301 #2 ─► +3V3_B ─► B and D sensors
VBAT ─► 100 k / 22 k divider ─► A pin 20 (firmware battery voltage)
```
Star ground on E. Thick wire (≥20 AWG) on +8V_STEER and ESC. 470 µF at each servo rail on D.

## §6 Harness (every cross-board signal: 1 kΩ at the driving end)
| From | To | Signals |
|---|---|---|
| E | A J1 / B J1 / D J1 | power rails above |
| A J2 | E | SDA, SCL, GND (INA260 ×3) |
| E | A J3 | VBAT_SENSE, GND |
| A J4 | D J2 | A14→D7, D8→A15, L89 TX copy→D0, GND |
| A J5 | B J2 | A5→B4 heartbeat, L89 TX copy→B0, GND |
| B J3 | D J2 | B14→D15, D8→B15, GND |
| A J6 | C J1 | Wire1, UV, O3, +3V3_A, GND |
| B J4 | XIAO | +5V, GND, D6→B34, B35→D7 |

## §7 Pre-power checks
1. No 5 V net touches any Teensy I/O (continuity). 2. I²C scan matches the address table.
3. ICM-20948 CS/SDO ≈1.8 V. 4. DS3231 charge resistor removed (CR2032). 5. Bench supply current-limited ~1 A, servos unplugged, ESC unplugged.
