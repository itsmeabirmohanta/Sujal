# 01 — PROJECT CONTEXT (read this first, every session)

> **2026-10-27: read `05_FINAL_BUILD_2026-10-27.md` FIRST** — the 3 etched PCBs are the flight boards, mission is now a drone drop from ~1 km with a guided glide.

Team VRITRA · Team ID **2026-INSPACe-CAN-7USAT-025** · Lovely Professional University
Lead / hardware integration: Sujal Kalbhor · Competition: IN-SPACe CAN-7USAT India 2026 (3rd ed.)
Launch window 16-10-2026 → 30-11-2026, Kushinagar UP. This file last rebuilt 2026-10-22.

## Memory folder rules
This folder is the single source of truth for the zero-board (perfboard) build.
- `01_PROJECT_CONTEXT.md` — mission, rules, radio assignment, architecture, open questions
- `02_HARDWARE_INVENTORY.md` — every module the team OWNS, with verified electrical facts
- `03_PIN_MAP_AND_WIRING.md` — authoritative pin allocation per MCU + wiring rules
- `04_DECISIONS_CHANGELOG.md` — user answers, decisions, change log, AI working rules
Before changing any pin: read 02 + 03 in full, apply the checks in 04 §AI rules, then log it in 04.

## Mission (from guidelines IN-SPACe.CAN.MR.CG.01 + our CDR)
- Mass < 1000 g ±50 g; fits Ø150 × 400 mm. Our body: Ø130 OD, Ø124 inner (parachute bay drawing), 281 mm tall, 4 glass-fibre rods Ø8.2 on a 94 mm circle (cardinal), Al mid/base plates.
- Launched to 1000 ±100 m, ejected. Primary parachute opens by drag immediately, must be attached to the body, **no ejection mechanism on it** (5.4.iv). Descent < 20 m/s ±5.
- Secondary mechanism opens at **600 ±10 m** → 1–3 m/s. Ours: 7-cell ram-air parafoil, 0.40 m², steered by 2 brake-line servos, GPS-guided (target 26.717535 N, 84.301543 E, adjustable for wind).
- Mechanical gyroscopic wheel stabilisation (EMAX 2205 2300 KV + 40 A ESC, Ø70 wheel, 70 g, target 10 000 RPM). Spin rate = mandatory field 14.
- Audio beacon ≥ 92 dB. External power switch + light/sound indicator. ≥ 2 h pad wait. No LiPo (Molicel P50B 21700 3S Li-ion, metal can). No spring battery contacts. Battery removable < 1 min.
- Boards hard-mounted (standoffs/screws). Survive 15 g launch, 30 g shock.
- Only TRANSMITTED telemetry is graded. Onboard SD logging mandatory.
- Mission clock + flight state must survive a processor reset (6.1.ix, 6.2.ii).
- Telemetry ≥ 1 Hz, ASCII CSV + CR, field order:
  TEAM ID, TIME(s), PACKET COUNT, ALT(0.1 m AGL), PRESSURE(1 Pa), TEMP(0.1 °C), VOLTAGE(0.01 V),
  GNSS TIME, LAT(1e-4°), LON(1e-4°), GNSS ALT(0.1 m), SATS, ACCEL/roll/pitch (m/s²), GYRO SPIN RATE (deg/s),
  FSW STATE, optional…
- States: 0 BOOT, 1 TEST_MODE, 2 LAUNCH_PAD, 3 ASCENT, 4 ROCKET_DEPLOY, 5 DESCENT, 6 AEROBREAK_RELEASE, 7 IMPACT.
- GS must: command TX on/off, calibrate (gyro/baro/accel zero), write `Flight_2026-INSPACe-CAN-7USAT-025.csv` with headers, plot every field live, SI units.
- Antenna guidance (App. A): GNSS patch toward zenith with clear FOV; TM/TC whip along can axis.

## RADIO — OFFICIAL ASSIGNMENT (Team_Frequency_Assignments_19_10.pdf) — SUPERSEDES CDR
| Item | Value for team 025 |
|---|---|
| 868 MAIN | **866.50 MHz** |
| 868 BACKUP | **865.25 MHz** |
| SF | **SF9** (→ launch pad 3) |
| Sync word | **0x99** (0x80 + 25) |
| BW / CR | 125 kHz (mandatory) / 4/5, LoRa not FSK |
| TX power | +10 dBm on pad, ~+14 dBm in flight |
| XBee (if used) | main Ch19 (2445), backup Ch14 (2420), PAN 0x1019 |
Rules: never transmit outside these; reserved 433, 446, 915 MHz, 2405 MHz; telecommand ON/OFF per link;
auto TX-off 90 min after launch detect; any video TX must avoid 2.4 GHz XBee channels → **XIAO Wi-Fi/BT OFF**.
GCS and all firmware constants were UPDATED to this on 2026-10-22 (user approved). CDR + KiCad guide still show the old 867.00/865.75 SF10 0x98.

## FRR — 26-10-2026, 10:34 AM, virtual (Webex)
Demo videos + test results: Mission GUI, Telemetry, Battery status & provisions; physical CanSat present. Launch 31-10 → 03-11-2026 Kushinagar.

## Architecture (CDR / KiCad rev E, adapted for the zero-board build)
- Primary FC: Teensy 4.1 #1 — owns telemetry, L89HA GNSS (NavIC), LR1121 #1, BMP585 #1, LSM6DSO32, W25Q64, DS3231, INA226.
- Backup FC: Teensy 4.1 #2 — BMP585 #2, ICM-20948 #1, (LR1121 #2 if owned), shares GNSS, takes over if primary heartbeat stops.
- Recovery/parafoil: Teensy 4.0 — independent; M9N GNSS, ICM-20948 #2, 2× A20CLM steering, 2× MG90S (drogue release + petri lid), ESC, Hall, SD logger. Decides 600 m itself.
- Power: 3S Molicel → BMS → external switch + LED → AP64500 5 V bucks / AP62301 3.3 V bucks; ESC straight from battery.
- KiCad rev E (5 Ø110 boards, 4-layer B1) lives in `C:\Users\kalbh\Documents\KiCad\cansat final pcb\` — reference only for the zero-board build; it contains parts the team does not currently own (TPL5010, SN74 logic, MAX98357A, INA260, DFR1015, UART mux, env sensors).

## Zero-board build (user, 2026-10-22)
- 4 × double-sided perfboard, 12 cm × 15 cm. This build is intended as the FINAL flight electronics (used for structural testing in a different structure, same connections).
- 3 boards "placed vertically in the middle of the CanSat"; 1 board dedicated to parafoil steering.
- Primary Teensy 4.1 on the board that "faces outward", with the GNSS.
- GEOMETRY CONFLICT (open): a 120 mm-wide board needs a 120 mm chord; Ø124 inner leaves 2 mm/side and only at ≤15 mm from centre; the 4 rods at r = 47 mm cross any board through the centre line. See open question Q1.

## Open questions (blocking) — answers go to 04
See `04_DECISIONS_CHANGELOG.md` §Open questions.
