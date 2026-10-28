# VRITRA: avionics, flight software and precision-landing design

**Team:** 2026-INSPACe-CAN-7USAT-025. **Review date:** 28 October 2026 (Asia/Calcutta). **Campaign:** 31 October–3 November 2026, as supplied by the team.

## 1. Engineering decision and evidence boundaries

**Build a conservative four-board port with autonomous deployment and bounded steering. Do not attempt a new full inertial-navigation UKF, untested flare, or wholesale library replacement in the remaining three days.** The supplied flight software is for an older single-board system. Syntax checks of board tests are not evidence that the flight article works.

The team clarified during this review that the **drogue deploys immediately after drone release near 1 km, and the ram-air secondary deploys at 600 ±10 m**. This resolves the main mission-sequence conflict with guideline §5.4. The ram-air bay servo belongs to the **600 m event**; the **950 m petri-lid event is independent**. Do not relabel petri opening as a second descent mechanism.

Evidence labels used throughout:

- **R:** requirement in supplied official PDF or current user instruction.
- **H:** fixed hardware/pin fact in current brief and current board files; continuity on the built article is still a team test.
- **V:** value checked against a linked manufacturer document or driver.
- **D:** proposed design setting or derived calculation. These are **not measured performance**. A test ID identifies how to qualify them.
- **U:** unresolved, unavailable or unverified. No flight-ready claim attaches to a U item.

All thresholds and timing budgets below that are not expressly R/H/V are **D starting values**. Hardware test results are not available in this session. This is an architecture and reference-code handoff, not a certified or fully compiled firmware image.

Controlling local sources: [current brief](../source/VRITRA_CanSat_025_Handover_2026-10-28/01_PROJECT_BRIEF_AND_PARTS.md), [official guidelines](../source/VRITRA_CanSat_025_Handover_2026-10-28/02_competition_documents/IN-SPACe_CAN-7USAT_Mission_Requirements_and_Guidelines_IN-SPACe.CAN.MR.CG.01.pdf), [19 October radio assignment](../source/VRITRA_CanSat_025_Handover_2026-10-28/02_competition_documents/CAN7USAT_Team_Frequency_Assignments_19-10-2026_(OFFICIAL_for_team_025).pdf), [current boards](../source/VRITRA_CanSat_025_Handover_2026-10-28/03_kicad_current_boards), and [supplied repository](../source/VRITRA_CanSat_025_Handover_2026-10-28/05_code/vritra-gcs-master_(CURRENT_repo)). Historical prompts and previous AI answers are evidence of decisions, not datasheets.

### 1.1 Launch-critical findings

| Priority | Finding | Consequence / action |
|---|---|---|
| A | Board 4 is now ESP32-P4; old Teensy 4.0 Board 4 and earlier P4 pin maps conflict with the current files | Freeze the current GPIO map below; compile and run on the actual P4 revision (T-PINS, T-BOOT) |
| A | Board 2 has no wired radio, GNSS command path, beacon or camera takeover | Call it backup sensing and deployment-authority support; it cannot restore telemetry after MAIN dies |
| A | Existing base-36 downlink is not decimal engineering-unit CSV over the air; PAD cadence is documented as 0.5 Hz | Replace the flight/gateway/GCS wire path together, or obtain an explicit format exception. The strict design here uses ≥1 Hz CSV (T-RF) |
| A | Existing TX_ON handler clears the 90-minute latch and launch epoch | Remove that override. The deadline must survive reboot and repeated TX_ON (T-RESET) |
| A | Existing GNSS RMC parser updates speed without checking RMC validity; older numeric state persists | Commit coherent, fresh GGA/RMC epochs; missing values stay invalid, never zero-filled (T-NAV) |
| A | Grove specification is ≥85 dB, whereas guideline §5.2 / compliance item 13 requires ≥92 dB unobstructed | Measure using an organiser-agreed distance/weighting; change beacon hardware if it fails. A new drive pattern does not prove compliance |
| A | No wired release switch, canopy-open sensor, servo-position feedback, steering-rail monitor or rangefinder | Software can infer events and monitor commands, but cannot prove physical release, deployment, servo position, rail voltage or flare height |
| A | Brief says 0.40 m²/7 cells; study says 0.71 m²/11 cells | Do not use its predicted dispersion, turn rate or polar as measurements of this flight article (T-GLIDE) |
| A | Steering rail was reportedly 9 V; B26CLM maximum is 8.4 V | Set the existing adjustable supply to 7.4–8.0 V and measure transient overshoot under representative load before connecting servos (T-POWER) |

The [Seeed buzzer specification](https://wiki.seeedstudio.com/Grove-Buzzer/) supports the rating discrepancy; the [AGFRC B26CLM specification](https://www.agf-rc.com/agfrc-coreless-mini-servo-b26clm-p1669135.html) supports its supply limit. The other findings are from the supplied files, with exact code pointers in §10.

## 2. Requirements and verification

The complete working matrix is [requirements.csv](requirements.csv). These requirements allocate functions to the actual hardware, not an imagined redundant computer.

| ID / priority | Requirement / source | Owner and acceptance evidence |
|---|---|---|
| R01 A | Mass under 1 kg with stated ±50 g tolerance; envelope Ø150 ×400 mm (§5.1) | Team: weigh complete flying system; gauge with rigging and antenna included; clarify interpretation of mass tolerance |
| R02 A | Two descent mechanisms; passive primary immediately on release; secondary at 600 ±10 m; secondary sink 1–3 m/s (§5.4 and current clarification) | Mechanics + P4: witnessed deployment and measured drop profile, T-DEPLOY/T-GLIDE |
| R03 A | Petri opens once at 950 m above boot/pad reference for 10 s (brief) | P4: pressure-chamber sweep, direction gate, once-only latch and reset during open, T-EVENT |
| R04 A | No mechanism motion before release (user interlock) | P4: explicit release-confirmed latch plus inhibits; see §7 for the fixed-hardware limitation, T-INHIBIT |
| R05 A | Collect and transmit ≥1 Hz ASCII CSV after command, fixed order, CR terminator (§6.3) | MAIN + GS: RF capture and timestamps, not USB-only reconstruction, T-RF |
| R06 A | Boot RF-silent; RF ON/OFF commands (§6.1); 90-minute hard cutoff (19 October assignment p1) | MAIN: all reset/command permutations; receive remains available after TX_OFF; T-RESET |
| R07 A | State and mission clock survive processor reset (§6.1/6.2) | All FCs: dual-slot CRC journal, preserved baro datum/event latches, T-RESET |
| R08 A | All required fields live in SI units, plotted and saved as Flight_2026-INSPACe-CAN-7USAT-025.csv (§6.1/6.3) | GCS: hardware link -> raw receive log -> CSV -> plots; stale values visibly invalid, T-GCS |
| R09 A | Onboard SD record despite RF loss (§5.1/requirement 8) | Board 2 SD, optionally MAIN native SD: disconnect radio and inspect independently decoded records, T-LOG |
| R10 A | Mechanical gyro stabilization and measured wheel spin in deg/s (§5.4/6.3) | P4 Hall sensor; wheel command is not spin measurement, T-HALL/T-WHEEL |
| R11 A | No LiPo; metal-can Li-ion allowed; external switch/indicator; 2 h pad plus flight endurance (§5.3) | Power board + team: physical inspection and actual endurance test, T-POWER |
| R12 A | Survive 15 g acceleration /30 g shock, enclosed boards, mounts, battery retention (§5.2) | Team: existing qualification evidence or controlled qualification; software is not structural proof |
| R13 A | Recovery beacon ≥92 dB (§5.2); identifying labels | Team: acoustic measurement + label inspection, T-BEACON |
| R14 A | MAIN 866.50 / BACKUP 865.25 MHz, SF9,125 kHz,4/5,0x99; +10 dBm pad/~+14 flight; backup only on Net Control instruction | MAIN + gateway: RF configuration/readback, controlled conducted/range test during authorised slot, T-RF |
| R15 A | Only MAIN commands GNSS; raw TX heard by three processors (current brief/current KiCad) | T-PINS, scope all listeners during baud changes; never attach listener TX to GNSS |
| R16 A | No simulated/replayed/default position or velocity in flight guidance or LIVE telemetry (user) | Parser validity, provenance, coherence, expiry and build isolation, T-NAV/T-ISOLATE |
| R17 A | Adjustable target, atomic and acknowledged (brief) | MAIN -> P4 command transaction -> active target echo; stale/partial changes rejected, T-TARGET |
| R18 A | Camera Wi-Fi/BT off; autonomous control survives lost GCS (brief) | Camera build inspection; cut links during hardware test, T-LINK |
| R19 A | GNSS and sensor data represent physical samples with timestamps | Sensor-ready flags, bounded read time, saturation flags, unit tests + bench reference, T-SENSOR |
| R20 B | Predicted landing point, wind and mode display | Estimate validity and uncertainty shown; never label prediction as a GNSS measurement, T-GCS |

The state names in guideline §6.2 are explicitly an **example**, not an assertion that every team must use precisely those names. Preserve the requested numbering 0–7 for this port to avoid needless ambiguity. Guideline §6.3's accelerometer field is underspecified about scalar subfields and mentions roll/pitch; retain the existing expansion ACC_X/Y/Z, append measured roll/pitch as optional, and confirm the judge's parser before freezing the header. The official table also has a longitude-description typo; longitude still occupies its own stated position.

## 3. Fixed-pin interface control document

### 3.1 Electrical inventory

**H source:** current brief §3–4 plus current board BUILD_LIST files. GPIO numbers on the P4 are not physical header-pin numbers. Shared ground is required. None of these assignments is changed by this design.

| Board | Link / pins | Devices / traffic | D rate / budget / expiry |
|---|---|---|---|
| MAIN Teensy4.1 | Wire SDA18/SCL19,400 kHz after electrical test;100 kHz fallback | BMP585 0x47,ICM 0x69,DS3231 0x68,INA260 0x40,AT24C32 0x57 if fitted | IMU225/220 Hz,baro50 Hz,power20 Hz,RTC1 Hz; ~10–15% nominal bus occupation including register pointers; measure retries |
| MAIN | Serial2 RX7/TX8;RESET_N6 | L89: only bidirectional GNSS connection | 9600 acquisition ->115200 after qualified change;2 KiB extra RX ring; ≥1 ms drain cadence |
| MAIN | SPI1 MOSI26/SCK27/MISO39;CS29,BUSY24,RST25,IRQ30 | LR1121 | Start at tested SPI clock; IRQ flag only; radio deadline as §6 |
| MAIN | Same SPI1,CS9 | W25Q64 | Independent transactions, both CS HIGH idle; ≤256-byte program chunks; WIP polled outside critical path |
| MAIN | Serial1 TX1/RX0,115200 | XIAO camera commands | REC/SNAP/STAT/PING lines;≤64-byte command;500 ms response deadline, bounded retries; camera delay never blocks deployment |
| MAIN | Serial3 TX14 ->P4 RX19;RX15 <-P4 TX54 | MAIN_NAV + authoritative mission state/commands;P4 status back |115200,20 Hz;≤96 bytes/frame incl framing; MAIN state expires250 ms; nav age checked independently |
| MAIN | GPIO5 ->BACKUP GPIO4 | Progress-qualified heartbeat |10 Hz toggles;missing edges500 ms ->suspect; lease expires1 s |
| MAIN | GPIO2,LED31/32,ADC20 | Digital active buzzer;status;100k/22k divider | Nonblocking pattern;ADC diagnostic until calibrated |
| BACKUP Teensy4.1 | Wire18/19,400 kHz after check | BMP585 0x47,HDC3020 0x44;LSM6DSO32 0x6A quarantined initially | Baro50 Hz,RH/T1 Hz;optional IMU208 Hz only after repair test |
| BACKUP | Serial1 RX0 only | Shared L89 TX through1k |115200 after resync;no GNSS TX commands |
| BACKUP | Serial3 TX14 ->P4 RX16;RX15 <-P4 TX54 | BACKUP_NAV +status/backup authority;P4 status |115200,20 Hz;≤96-byte frames;250 ms expiry |
| BACKUP | SPI CS10,MOSI11,MISO12,SCK13 | External SD module,alone on bus | Binary queues +CSV export;card stalls measured,not in event task |
| BACKUP | LEDs36/37 | Status | No new actuator or RF connection assumed |
| P4 | Wire SDA7/SCL8 | LSM6DSO32 0x6A,BMP585 0x47;ES8311 0x18 ignored |IMU208 Hz,baro50 Hz;no IMU/BMP interrupt wire assumed |
| P4 | UART1 RX14 only | Raw L89 NMEA |115200,4 KiB RX ring;independent parse |
| P4 | UART2 RX19/TX54;UART3 RX16 only | MAIN input,BACKUP input;one TX54 fans to MAIN/BACKUP through separate1k |20 Hz normal status;extra ACKs share the same TX owner queue |
| P4 | GPIO20/21,22/23,32,33 | Left/right brakes,bay,petri,spare,ESC |Start all servo frames50 Hz;spare detached;ESC stop1000 µs subject to exact ESC qualification |
| P4 | GPIO5 Hall input,10k pull-up | Wheel pulses |Edge timestamp /counter ISR;calibrated magnets-per-revolution;zero-speed timeout |
| POWER | I²C INA2600x40 accessed by MAIN | Battery-side V/I/P only |No independent steering/logic rail readings exist |
| GS ESP32 | SPI CS5,SCK18,MOSI23,MISO19,RST14,BUSY27,IRQ26 | LR1121;USB115200 to GCS |Same RF plan;transparent decimal CSV;no stale-value reconstruction |

**Do not use 1 MHz I²C on MAIN.** Its DS3231 and ICM interfaces constrain the common bus to400 kHz; P4 also gains little from changing this close to launch. PJRC's [UART](https://www.pjrc.com/teensy/td_uart.html) and [Wire](https://www.pjrc.com/teensy/td_libs_Wire.html) documentation describe buffering and controller APIs. Standard Teensy HardwareSerial is interrupt buffered; do not call it DMA without installing and testing a DMA implementation. DMA is unnecessary for these small UART budgets.

### 3.2 GNSS breakout pins checked against board nets

Current MAIN BUILD_LIST connects L89 **TX/RX/RST**, **WAKE to +3V3_A**, and **BAT to GNSS_VBCKP**. VBCKP is diode-ORed from logic3V3 and the J7 cell input. **3D_FIX,JAM,GEO,SDA,SCL and 1PPS are not routed to an MCU in the current build**. They cannot be polled by software. Test the backup cell and diode voltage with main power removed; a drawn backup circuit does not prove a battery is installed. Do not confuse the DS3231 backup cell with the L89's J7 supply.

### 3.3 Interboard binary frame (D, T-LINK)

Use COBS framing with a zero delimiter, maximum payload64 bytes (version2; decoded frame90 bytes, COBS wire92 bytes including delimiter) and **CRC-32/ISO-HDLC** over the explicit serialized bytes. Header, little endian: `version:u8,type:u8,source:u8,flags:u8,session:u32,sequence:u32,send_mono_us:u64,payload_len:u16`. CRC:u32 follows payload. Do not send raw C++ structs; padding, enum width and float layout are not an ICD. Explicitly encode IEEE754 float32 only where declared; coordinate fields are signed degrees×10^7 integers.

Message allocation:

| Type | Payload / cadence | Validity |
|---|---|---|
| NAV_RAW 1 |epoch_id:u32,lat_e7:i32,lon_e7:i32,MSL_mm:i32,vN/vE_mmps:i32 each,fix_quality:u8,fix_dimension:u8,sats:u8,source_kind:u8,HDOP×100:u16,GGA_age_ms:u16,RMC_age_ms:u16,UTC_day_ms:u32;20 Hz envelope, only new GNSS epochs change sample |Never mark a repeated envelope as a new fix; receiver adds local queue age |
| BARO 2 |sample counter,pressure:f32,temperature:f32,pad_pressure:f32,pad_temperature:f32,AGL:f32,v_up:f32,age:u16,validity:u16;20 Hz |Different board barometers have separate calibration IDs and sample clocks |
| MISSION 3 |mission ID,elapsed_ms:u64,launch_elapsed_ms:u64,state:u8,armed/release/bay/petri bits,target revision,owner lease;10 Hz +events |Monotonic phase;cannot clear completed latches via normal command |
| ACT_STATUS 4 |P4 reset/session,state,owner,mode,pulses:u16×4,Hall deg/s:f32,target revision,nav epoch,ages,fault mask;20 Hz |Commanded position is not measured position |
| COMMAND 5 |transaction ID:u32,command:u8,typed payload,length;on demand |Strict source/phase/range validation;idempotent execution |
| ACK 6 |transaction ID,result code,applied revision,event flags;on demand |ACK means software committed,not mechanism moved |
| TIME_SYNC 7 |challenge ID,MAIN elapsed_ms and local roundtrip exchange;1 Hz |Monotonic offset with uncertainty;never subtract unrelated MCU micros clocks directly |

The version2 maximum92-byte wire frame takes7.99 ms at115200 8N1 (D:92×10/115200), or16.0% occupancy at20 Hz. The electrical inventory reserves96 bytes/8.34 ms per frame, leaving framing margin. See [the byte-level ICD](ICD_WIRE_FORMATS.md) for the exact schema and version boundary. P4's single outward link has **one producer queue**, because two independent Serial objects must not drive GPIO54. Use a TX-disabled RX-only port for BACKUP and another for GNSS. The P4 has enough hardware UART controllers; see [Espressif's P4 UART reference](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-reference/peripherals/uart.html).

CRC detects corruption; it does not authenticate commands. A mission nonce, transaction sequence and physical phase interlocks prevent accidental stale/repeated operation but are not cryptographic spoofing protection. Do not describe this as secure authentication.

## 4. L89HA: boot configuration, real fixes and fastest practical startup

### 4.1 What the evidence supports

The current receiver reports AG3335M firmware (H, team capture). The official [L89 R2.0 hardware guide v1.2](https://quectel.com/content/uploads/2024/02/Quectel_L89_R2.0_Hardware-Design_V1.2.pdf), antenna section, describes an **L1 patch and L5 chip antenna**. [7Semi's board listing](https://7semi.com/7semi-l89ha-breakout-multi-gnss-engines-for-gps-irnss-glonass-beidou-galileo-and-qzss/) also lists both built-in antennas. Therefore an extra antenna is **not inherently required for NavIC**. An attached external antenna may switch out the internal path; check its bands and actual antenna-status report.

This receiver's published signal list uses **NavIC L5**, not GPS L5/Galileo E5a dual-frequency observations of the same satellite. Do not promise RTK, centimetre landing, or dual-frequency ionospheric cancellation from the L1+L5 label. Channel capacity does not by itself identify the most accurate constellation combination for India. Compare the default NavIC-capable profile against all-six using repeated open-sky static/moving tests, same mounting and near-simultaneous conditions; keep the one with lower measured residuals and fewer outages.

The installed firmware string is not identical to the commercial release names in the protocol's compatibility notes. **Every setting must be ACK-checked and queried where supported.** An unsupported command leaves the last tested configuration active; do not retry indefinitely or send a second vendor's command family blindly. The [Quectel support thread](https://forums.quectel.com/t/problem-with-pstm-commands-for-l89ha-gnss-module/48222) confirms why ST PSTM commands are inappropriate to this Airoha unit. The supplied [7Semi library](https://github.com/7semi-solutions/7semi-L89HA-GNSS-Module-Arduino-Library) is useful for comparison, not sufficient to enforce epoch validity and boot transactions by itself.

### 4.2 Complete proposed command transaction

The following **D configuration**, based on [Quectel protocol v1.0 §§2.4.9–2.4.41](https://quectel.com/content/uploads/2024/02/Quectel_L89_R2.0_GNSS_Protocol_Specification_V1.0.pdf), is generated with XOR checksums in `reference_code/engineering_checks.py`; `\r\n` follows every line. `0` ACK result means accepted. Settings/behaviour must still pass T-GNSS on this firmware.

| Command body (checksum in generated file) | Expected ACK body | Purpose |
|---|---|---|
|PQTMVERNO|PQTMVERNO response|Identify|
|PAIR067|PAIR001,067,0|Query constellations|
|PAIR050,1000|PAIR001,050,0|Safe configuration rate|
|PAIR066,1,1,1,1,1,1|PAIR001,066,0|All-six trial|
|PAIR070,0|PAIR001,070,0|Disable static hold|
|PAIR072,10|PAIR001,072,0|Elevation mask trial|
|PAIR410,1|PAIR001,410,0|SBAS search|
|PAIR400,2|PAIR001,400,0|SBAS corrections|
|PAIR062,0,1|PAIR001,062,0|GGA|
|PAIR062,1,0|PAIR001,062,0|No GLL|
|PAIR062,2,1|PAIR001,062,0|GSA|
|PAIR062,3,5|PAIR001,062,0|Sparse GSV|
|PAIR062,4,1|PAIR001,062,0|RMC|
|PAIR062,5,0|PAIR001,062,0|No VTG|
|PAIR062,6,0|PAIR001,062,0|No ZDA|
|PAIR062,7,0|PAIR001,062,0|No GRS|
|PAIR062,8,1|PAIR001,062,0|GST if supported|
|PAIR864,0,0,115200|PAIR001,864,0|Baud|
|PAIR513|PAIR001,513,0|Save|
|PAIR050,200|PAIR001,050,0|5 Hz trial|
|PAIR003;PAIR513;PAIR002|003,513,002 ACKs|Persist high-rate profile|

**Wire-exact sentences and ACK checksums are in [gnss_commands.txt](reference_code/gnss_commands.txt).** This list is a transaction plan, not a paste-and-forget script. The final three commands are separate transactions. Save at high rate only while GNSS is stopped, then restart. Baud change requires receiver reboot; preserve backup supply, avoid a cold-start command. MAIN waits for final UART TX completion before changing its own baud. ACK may arrive at the previous rate; collect it before reset. Re-discover the rate and query settings after restart. Do not change baud or persist settings in flight.

The six PAIR066 fields are GPS,GLONASS,Galileo,BeiDou,QZSS,NavIC. The existing `1,1,1,0,0,1` combination is not in the published supported-mode list. All-six and the default GPS/Galileo/NavIC/QZSS profile are listed. A successful ACK is necessary but acquisition/fix testing is also required. The profile is a trial, not an assertion that more constellations always improve accuracy.

Protocol caveats: high-rate GGA/RMC can differ from other sentences' cadence; GST can be unsupported. Do not treat a requested5 Hz as a measured5 Hz. Capture UTC epoch intervals, UART load and dropped-byte counters. PAIR004/005/006/007 are hot/warm/cold/full-cold restarts; use cold commands only for deliberate ground tests. Configuration save and navigation-data save are different operations (PAIR513 versus511). Details and restart checksums are included in the generated command file.

### 4.3 Nonblocking boot state machine (D)

`LISTEN -> IDENTIFY -> QUERY -> WRITE_ONE -> WAIT_ACK -> QUERY_READBACK -> NEXT -> SAVE -> REBOOT_IF_NEEDED -> VERIFY_STREAM -> READY`.

- MAIN alone transmits commands. Start listening at the last qualified baud, then9600 and115200. UART discovery requires **two checksum-correct recognizable sentences with sensible lengths**; bytes alone are not proof of baud.
- Each command has a500 ms ACK deadline and at most2 retries (D). Identify replies may arrive outside that window; the configuration FSM has a separate startup deadline. If a command is rejected, log command/result and return to the tested profile. Never claim NavIC enabled from a sent string.
- BACKUP/P4 rotate receive rates after an idle/decode-failure timeout and independently reacquire. MAIN sends a baud-change notice over the interboard links, but raw GNSS reacquisition also works if that notice is lost. Disable guidance until a fresh coherent epoch has been accepted at the new rate.
- The launch configuration becomes immutable after arming. MAIN reset in flight listens for the running stream and does **not** reset/reconfigure the shared GNSS.

At1152008N1 the budget is11520 bytes/s (D). An assumed200 bytes per GGA+RMC epoch costs1000 bytes/s at5 Hz or2000 at10 Hz. Allow additional1 Hz quality messages and all constellation GSV bursts; measure the true maximum line length. At9600 the budget is960 bytes/s:5 Hz pairs alone can overflow it. Use5 Hz if demonstrated; retain1 Hz as the qualified fallback. Ten hertz requires measured benefit, not just ACK success.

### 4.4 Navigation mode, assistance and proof of a real fix

No exact dynamic-model, geofence configuration, reference-time/location injection or EPO transfer block is asserted for this particular firmware: the currently listed HA v1.1 / AGNSS documents are account-gated on the [Quectel product page](https://www.quectel.com/product/gnss-l89-r2/). No publicly verified AG3335M register-level SDK was obtained. LC29H/LC76G command similarity is not firmware compatibility. Debug binary output is not a documented superior PVT interface. Leave unspecified dynamic settings at tested defaults. The10° mask is a D tradeoff; compare against the original setting in T-GNSS.

For launch in three days, the highest-confidence TTFF measures are clear sky before arming, unobstructed internal antennas, quiet motor/DC-DC environment, verified V_BCKP retention, and avoiding unnecessary resets. Retain a warm receiver through the pad wait. Do not promise a cold/hot TTFF without a capture from this unit. GAGAN-capable SBAS support does not prove corrections are currently being applied.

For context only, hardware-guide Table2 lists unaided cold/warm/hot TTFF28/23/1 s, EASY cold/warm15/5 s and flash-EPO cold3 s under its stated test conditions. These are not acceptance results for the team's installed firmware. The same guide specifies V_BCKP2.2–4.3 V and RESET_N low for at least100 ms; use a scheduled reset pulse only during deliberate ground recovery. A permanent reset loop destroys acquisition progress. An exact per-constellation tracking-channel allocation was not verified; do not infer the optimal profile from a generic chipset channel count. [Quectel hardware guide, Table2 and §§3.2.2,5.2.2](https://quectel.com/content/uploads/2024/02/Quectel_L89_R2.0_Hardware-Design_V1.2.pdf).

JAM/GEO/1PPS are not wired to a processor. A B diagnostic can enable UART jamming reports with PAIR391,1 and parse PAIRSPF status, after firmware qualification; critical interference must invalidate precision navigation. PAIR074,1 is the documented AIC option, subject to ACK/readback testing. Neither supplies spoof-proof integrity. No unverified geofence command is added; enforce the approved landing area in the flight planner and target validator.

Assistance option B: obtain the **matching** HA AGNSS specification and a current assistance file through the vendor tool; verify firmware, GNSS week/time scale, validity interval, record lengths and expected response. MAIN can receive the binary file over USB before flight and stream vendor-defined assistance transactions in bounded chunks while disarmed. DS3231 provides time only if OSF is clear and it has been synchronized; convert UTC/GNSS time with the correct leap-second handling from that assistance protocol. A known pad coordinate is an **assistance input**, never a navigation output. Do not add that input to the LIVE GNSS or steering state. The current code's PMTK-based aiding path must not be called working until the receiver confirms it and repeated TTFF comparisons show improvement.

**GNSS acceptance contract (D, T-NAV):**

| Check | Launch starting gate |
|---|---|
| Origin | LIVE_UART only; no replay/simulation/default values |
| Transport | Correct NMEA XOR checksum, valid numeric grammar, proper hemisphere/minutes bounds |
| Coherence | GGA and RMC same UTC epoch; RMC status A; allowed non-DR fix quality; recent GSA3D |
| Quality | At least6 satellites used and HDOP≤2.5 initially; recent3D status; GST if available supplies uncertainty,not HDOP in metres |
| Freshness | At5 Hz:GGA/RMC ages≤500 ms; at qualified1 Hz≤1500 ms; quality messages≤2500 ms; no replayed UTC epoch refresh |
| Plausibility | Finite lat/lon/velocity,physical geographic bounds,drop-zone plausibility,innovation check against last accepted state with uncertainty |
| Reacquisition | Five consecutive new coherent accepted epochs before full-authority steering; neutral/limited authority during transition |

Do not reject a genuine coordinate solely because it equals zero: equator/Greenwich are legitimate. Invalidity is represented separately. At this site use a launch-area plausibility gate centred on a **measured pad fix**, with a documented radius appropriate to the permitted flight area. Do not hard-code the target as the measured origin. GNSS quality flags are useful but do not prove resistance to spoofing; hardware here has no independent absolute-position integrity sensor.

P4 parses raw NMEA itself for minimum delay, and also receives MAIN's navigation and quality state. Both are views of the **same receiver**, not independent position measurements. Do not fuse the two as if covariance decreases. Select the fresher validated copy of the same epoch; compare for disagreement. Raw lost but MAIN valid: use MAIN with its original age plus transport uncertainty. MAIN lost but raw valid: keep local guidance. Both stale: leave precision guidance; §8 defines the safe degraded policy.

## 5. Sensor and module configuration

### 5.1 Shared driver contract

Every driver has `beginStep(now)`, `pollReady(now)`, `startRead`, `finishRead`, `convert`, `health` and bounded `recoverStep`; no driver calibrates or sleeps the rest of the computer. A synchronous I²C call still occupies CPU/bus time even with no `delay()`. Use a bus owner, explicit controller timeout and measured worst case; do not claim zero blocking for stock Wire/RadioLib initialization. Long boot waits are deadlines across scheduler passes. Built-in self-tests run only on the ground; restore and verify flight configuration afterward.

For I²C recovery: stop scheduling the failed device, terminate the peripheral, release SDA/SCL open-drain, clock SCL up to9 times only while it can actually rise, generate STOP if possible, restart controller, reidentify, reconfigure and discard settling samples. If SCL is held low, pin toggling does not fix it. No per-sensor power switches are wired; repeated failures quarantine the device. A shared-rail power cycle would reset unrelated sensors and must not be performed automatically in flight. Use100 kHz fallback if a400 kHz waveform fails rise-time/voltage checks.

### 5.2 BMP585, all three processors

**V basis:** [Bosch datasheet §§4.3–4.6,8.27–8.36,Tables7/9](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp585-ds003.pdf); [SensorAPI definitions](https://github.com/boschsensortec/BMP5_SensorAPI/blob/master/bmp5_defs.h). **D selection:** pressureOSR16,temperatureOSR2,50 Hz,hardwareIIR bypass; short software filter separate from event path.

| Action | Address/value | Reason / verification |
|---|---|---|
|Soft reset|0x7E←0xB6|Schedule ≥2 ms;verify ID0x01=0x51 and NVM-ready/no-error0x28|
|Standby|0x37←0xBC|deep-dis;50 Hz code0x0F;wait mode transition|
|Disable FIFO|0x16←0x00;0x18←0x00|Polling baseline|
|Interrupt routes off|0x15←0x00|No INT wire|
|DSP/IIR|0x30←0x03;0x31←0x00|Preserve reserved bits;unfiltered shadow|
|OSR|0x36←0x61|Pressure enabled,P×16,T×2|
|Run|0x37←0xBD|NORMAL50 Hz|
|Verify|0x38|Effective OSRs and ODR-valid;reject silent fallback|
|Read|0x27 DRDY then burst0x1D..0x22|Timestamp actual ready/read;P=u24/64 Pa,T=s24/65536°C|

Do not apply BMP280/BMP3 compensation coefficients. BMP585 outputs compensated data. The official [Bosch driver](https://github.com/boschsensortec/BMP5_SensorAPI/blob/master/bmp5.c) supplies initialization/status/self-test support; use its result codes and verify nonconstant plausible physical samples. The pressureOSR16 intrinsic noise figure is0.21 Pa RMS at the datasheet's stated setting; the exact fitted-board noise with T×2 must be measured. Highest-resolution0.08 Pa is a different slower setting, not the guaranteed noise of this50 Hz profile.

**Derived altimetry (D, T-BARO):** with a fixed pad pressure p0 and measured ambient pad temperature T0K,

`h_AGL = T0K/L * [1 - (p/p0)^(Rd*L/g)]`, using `L=0.0065 K/m`, `Rd=287.05 J/(kg K)`, `g=9.80665 m/s²` as the declared standard-atmosphere model constants. An isothermal alternative is `h = Rd*Tmean/g*ln(p0/p)`. Both assume a temperature profile; neither sensor die temperature during descent nor boot zero removes atmospheric-model error. Retain raw pressure and the datum in logs. Match organiser ground-reference expectations using a calibrated pressure/height comparison.

Model basis: [U.S. Standard Atmosphere1976](https://ntrs.nasa.gov/api/citations/19770009539/downloads/19770009539.pdf). Using a measured pad temperature changes the reference model but does not measure the actual lapse rate or humidity profile.

At100 kPa and293.15 K, `|dh/dp|≈0.0858 m/Pa`;0.21 Pa corresponds to≈0.018 m intrinsic RMS. That is **resolution noise**, not altitude accuracy. Pressure-port flow at8 m/s withρ1.2 kg/m³ has dynamic pressure≈38.4 Pa, equivalent to≈3.3 m if fully coupled; at20 m/s≈240 Pa/20.6 m. Port design, weather drift, datum error and temperature profile dominate the centimetre noise figure. An idealized±6 Pa relative sensor error alone maps to≈0.51 m at that operating point; environmental and model errors must be added.

Event path: median-of3 valid samples plus a short causal filter/vertical estimate. Budget≤100 ms from a threshold crossing to event command (D), equivalent to≤0.8 m at8 m/s and≤2.5 m at25 m/s. Confirm with a pressure ramp and GPIO/servo trace; do not claim this from ODR alone. Guidance may use a0.2 s smoothing time constant; it must not silently become the600 m trigger path. Read DRDY at≥100 Hz, ignore duplicates, flag baro stale after100 ms. Test all three barometers together at pad and during ramps; they are independent sensors but share environmental errors.

### 5.3 ICM-20948 / AK09916 on MAIN

**V basis:** [TDK DS-000189 register map](https://product.tdk.com/system/files/dam/doc/product/sensor/mortion-inertial/imu/data_sheet/ds-000189-icm-20948-v1.5.pdf), [SparkFun register definitions/driver](https://github.com/sparkfun/SparkFun_ICM-20948_ArduinoLibrary). **D baseline:** no DMP,no FIFO,gyro≈220 Hz,accel225 Hz;poll ready at500 Hz;±2000 dps/±16 g;filter setting3. Readiness is set only after measured samples pass plausibility.

| Bank / write | Proposed value | Purpose |
|---|---|---|
|REG_BANK_SEL0x7F|0x00|Known bank|
|B0 PWR_MGMT_1 0x06|0x80,then0x01|Reset,wake/clock;wait ≥100 ms D|
|B0 PWR_MGMT_2 0x07;LP_CONFIG0x05|0x00;0x00|All axes,continuous|
|B0 USER_CTRL0x03;INT_PIN_CFG0x0F|0x00;0x02|DMP/FIFO/master off;bypass for AK|
|B0 INT_ENABLE_1 0x11|0x01|Raw-ready status;no physical IRQ assumed|
|B2 GYRO_SMPLRT_DIV0x00|0x04|Base-divider5|
|B2 GYRO_CONFIG_1 0x01|0x1F|FS2000,DLPF3,FCHOICE1|
|B2 ACCEL_SMPLRT_DIV1/2 0x10/11|0x00/0x04|Divider5|
|B2 ACCEL_CONFIG0x14|0x1F|FS16g,DLPF3,FCHOICE1|
|Return B0|0x7F←0x00|Data access|
|AK0x0C CNTL3 0x32;CNTL2 0x31|0x01;then0x08|Reset;100 Hz continuous after settling|

Bank select0x20 selects B2. WHO_AM_I0x00 must read0xEA. Poll status0x1A; burst14 bytes from0x2D for accel/gyro/temperature. Convert big-endian signed words;±16g scale2048 LSB/g,gyro16.4 LSB/(deg/s). AK WIA2 must be0x09; burstST1 at0x10 throughST2 at0x18, including the intervening byte;accept DRDY and reject overflow. Magnetometer axes need the separately verified mounting transform. Bypass is simpler than programming the I²C-master state machine; do not enable both at once. B3 master/slave registers remain untouched in this baseline.

**Wedge recovery (H plus D test):** at known0x69, selectB0 then blind-reset PWR_MGMT_1,wait,verify WHO_AM_I and reconfigure. **Never blind-reset0x68**, the wired DS3231 address. ACK plus0xFF data is a fault, not a valid IMU. If bank-select itself cannot be trusted because the device is wedged, soft reset may fail; recover the bus then quarantine if still stuck. Do not promise a software cure for every power-domain failure.

Self-test: use a driver/datasheet-defined ground test with baseline versus stimulated samples and published limits; store both results and restore the above registers. No guessed self-test thresholds are introduced here. Six-position accel calibration,stationary gyro bias,and magnetometer ellipsoid fit are in §11. Shock can exceed±16g even though structure must survive30g: flag saturation,do not integrate clipped acceleration into navigation. Mechanical-wheel magnetic disturbance can invalidate compass yaw; do not compensate it with one hard-iron offset measured motor-off.

### 5.4 LSM6DSO32 on P4; diagnosis on BACKUP

**V basis:** [ST datasheet](https://www.st.com/resource/en/datasheet/lsm6dso32.pdf), [official platform-independent driver](https://github.com/STMicroelectronics/lsm6dso32-pid), [ST examples](https://github.com/STMicroelectronics/STMems_Standard_C_drivers/tree/master/lsm6dso32_STdC). **D baseline:**208 Hz,±16g/±2000 dps,poll ready500 Hz;FIFO off until tagged timestamp parsing is tested.

| Register / write | Value | Use |
|---|---|---|
|WHO_AM_I0x0F read|expect0x6C|At0x6A|
|CTRL3_C0x12|0x01 then0x44|Wait SW_RESET clear;BDU+auto-increment|
|CTRL1_XL0x10|0x5C|208 Hz,16g,LPF2 off|
|CTRL2_G0x11|0x5C|208 Hz,2000dps|
|CTRL4_C0x13|0x02|Gyro LPF1 enabled|
|CTRL6_C0x15|0x03|Gyro filter MEDIUM;high-performance|
|CTRL7_G0x16;CTRL8_XL0x17|0x00;0x00|High-pass off;preserve gravity|
|CTRL10_C0x19|0x20|Timestamp counter enabled|
|INT1/2_CTRL0x0D/0E|0x00/0x00|No wired IRQ|
|FIFO_CTRL1..4 0x07..0x0A|0,0,0,0|Bypass baseline|
|STATUS_REG0x1E read|XLDA/GDA|Only count new samples|
|Burst0x20..0x2D|14 bytes|Temp,gyro,accel;little endian|
|Timestamp0x40..0x43 read|u32×25 µs|Wrap-aware sensor time;mapping uncertainty logged|

16g uses0.488 mg/LSB,2000dps uses70 mdps/LSB;temperature=25+raw/256°C. These are driver/datasheet conversions, not calibration results. LPF2 is deliberately off to keep the launch configuration small; add a bounded software anti-noise filter at the full sample rate, not after downsampling. Do not enable accelerometer high-pass on an attitude/gravity channel.

**B FIFO option:** ST functions `fifo_xl_batch_set(208Hz)`, `fifo_gy_batch_set(208Hz)`, `fifo_timestamp_decimation_set(DEC_1)`, `fifo_watermark_set(32)`, `fifo_mode_set(STREAM_MODE)`. Poll FIFO level/overrun every5 ms,drain complete tagged7-byte records via0x78..0x7E,decode tag parity/type and timestamp records,track sample order,flush and reinitialize after overrun. Watermark units are records, not a guaranteed32 synchronized six-axis samples. Use the official tagged-FIFO example to generate/read back register values; this optional profile is **not qualified** in the provided reference code.

**A BACKUP diagnosis:** disconnect actuator power first; confirm breakout supply and ground and DO/CS straps;scan100 kHz including0x6B;read0x0F repeatedly;run reset/status readbacks;inspect bus pull-ups,voltage and clock waveform;read all six static orientations and hand rotations;check scales for DSO32 rather than an ordinary2g LSM variant. Compare the same breakout on the good P4 bus with power off while moving it. If failures follow the breakout,quarantine it;if they follow BACKUP,inspect that bus/wiring. Do not revive it into the estimator based on an ACK alone. Full positive/negative self-test follows the exact ST example and its settling/limits; restore flight configuration and retest outputs.

The downloaded [ST self-test example](https://github.com/STMicroelectronics/STMems_Standard_C_drivers/blob/master/lsm6dso32_STdC/examples/lsm6dso32_self_test.c) uses52 Hz/4g for the acceleration test, averages5 samples after settling, and checks an absolute stimulated difference of50–1700 mg per axis; its gyro test checks150000–700000 mdps per axis. Follow its complete setup and100 ms settling stages rather than applying those limits to the flight configuration. Replace the example's unlimited wait loops with deadlines in the board integration.

### 5.5 INA260, DS3231, HDC3020 and W25Q64

| Device / priority | D boot configuration and exact read path | Detection / timing / recovery |
|---|---|---|
|INA260 A|I²C0x40; verify0xFE=0x5449 and0xFF device bits=0x2270 (mask low revision nibble). Reset by0x00←0x8000; poll reset completion with deadline. Write0x00←0x6527:16 averages,1.1 ms voltage/current conversions,continuous both; read back. Write0x06←0x0000 (no wired ALERT). Poll0x06 CVRF bit3; individually read0x01 signed current,0x02 unsigned voltage,0x03 unsigned power,MSB first. Scale1.25 mA,1.25 mV,10 mW per LSB.|Nominal conversion cycle35.2 ms D=16×(1.1+1.1),poll50 Hz,publish latest valid sample20 Hz with original timestamp; no FIFO. Check math-overflow bit2; bracket the register reads with status and discard/retry if another conversion completed mid-read. Finite bus timeout and shared recovery; identity/config/reference-load test rather than invented self-test. No calibration register; calibrate externally. Reset/startup deadline10 ms D to qualify.|
|DS3231 A|I²C0x68;read status0x0F first;read coherent0x00..0x06 calendar burst;BCD and calendar validation. RMW0x0E:EOSC=0,INTCN=1,alarm enables0;disable unused32kHz by RMW0x0F bit3;do not overwrite OSF|OSF=1 makes absolute time untrusted until explicitly set from verified UTC;readback then clearOSF. No physical1PPS link. Never set compile-time date on every boot |
|HDC3020 A|I²C0x44;command0x3781 read manufacturer0x3000+CRC;0x30A2 reset,0x3066 heater-off,0x2130 auto1Hz lowest-noise;0xE000 ->6 bytes T_MSB,T_LSB,CRC,RH_MSB,RH_LSB,CRC|T=-45+175·u16/65535;RH=100·u16/65535;CRC8 poly0x31/init0xFF per word;12.5 ms conversion;allow ground startup10 ms D before first command,one full measurement before data. No all-FF reads accepted. Reset/reprobe if repeated CRC/read errors |
|W25Q64 A|SPI1 CS9;read JEDEC0x9F first,confirm suffix/voltage;0x05 readstatus;0x06 WREN,verifyWEL;0x02+24bitaddr+page data;0x03+address readback;0x20+alignedaddress4KiB erase **only preflight**|Use256-byte page boundaries;pollWIP without holdingSPI bus. Max times depend on actual suffix;timeout from correct datasheet,not infinite while(WIP). Do not rewrite protection/QE bits or chip erase at boot. CRC-framed append records,scan after reset |

V references: [TI INA260 §§8.5–8.6](https://www.ti.com/lit/ds/symlink/ina260.pdf), [ADI DS3231 register map](https://www.analog.com/media/en/technical-documentation/data-sheets/ds3231.pdf), [TI HDC3020 Tables7-1/7-4/7-5](https://www.ti.com/lit/ds/symlink/hdc3020.pdf), [TI programming application note](https://www.ti.com/lit/an/sboa631/sboa631.pdf), [Winbond W25Q64JV instruction tables](https://www.winbond.com/resource-files/w25q64jv%20spi%20%20%20revc%2006032016%20kms.pdf). Winbond suffix is **U** until read from the actual chip;JV timings must not be assumed for every W25Q64 variant. TI does not supply a universal Arduino SDK for these parts; [Adafruit INA260](https://github.com/adafruit/Adafruit_INA260) and [Adafruit HDC302x](https://github.com/adafruit/Adafruit_HDC302x) are board-vendor/community implementations, not TI drivers.

HDC3020 and SHT4x share a possible address but use different protocols;a generic ACK cannot select the driver. Detect identity first,log the part once,and never oscillate between two command sets in flight. Environmental self-test here means CRC,identity,known-reference comparison and plausible response;these devices do not all expose a MEMS-style stimulated self-test. The INA260 sees only current physically downstream of its shunt. Any ESC wired directly to battery is excluded from coulomb counting;show that limitation rather than manufacturing total battery energy/SOC.

### 5.6 Camera, buzzer and MCU services

The camera protocol in the supplied `vritra_cam.ino` is retained:115200 UART0 GPIO44 RX/GPIO43 TX on XIAO;MAIN Serial1TX1/RX0;PING,STAT,SNAP,REC lines. Confirm actual line endings/responses from this source,then log `CAM REC OK` as a software ACK. MAIN repeats REC with a bounded timer until acknowledgment;camera initialization or SD failure must never hold the main loop. Start recording on drone lift if practical to capture release;this is earlier than the optional separation-to-touchdown interval. Sense PSRAM/camera settings must match the actual board/camera revision. Camera storage is independent and its timestamps need a documented mapping to mission time. The source's never-start-Wi-Fi/BT approach is appropriate;do not add wireless setup for launch. [Seeed Sense camera documentation](https://wiki.seeedstudio.com/xiao_esp32s3_camera_usage/) and [Espressif camera driver](https://github.com/espressif/esp32-camera) are the hardware/driver sources;exact camera sensor is U until identified.

Buzzer:MAIN GPIO2 digital HIGH/LOW drives the fitted active Grove input. Remove I²S initialization and the old amplifier pin ownership. Use a timed beacon pattern after landing and a grounded manual command. Boot LOW until state restore;restore beacon after a valid landed journal. Sound pressure requires measurement;GPIO success is not acoustic success.

Teensy:use a cooperative deadline scheduler;IntervalTimer only sets flags/timestamps,no I²C/SD/Serial prints in the ISR. Native Teensy4.1 SD usesSDIO via the bundled SD/SdFat path when a card is fitted;BACKUP's external module stays on its ownSPI bus. EEPROM emulation is flash-backed,so writing every sample is inappropriate;journal only essential phase/mission changes. Read [IntervalTimer](https://www.pjrc.com/teensy/td_timing_IntervalTimer.html), [EEPROM](https://www.pjrc.com/teensy/td_libs_EEPROM.html) and the [PJRC SD library](https://github.com/PaulStoffregen/SD).

P4:use one sensor/bus owner and one actuator owner. FreeRTOS tasks exchange immutable snapshots;no spin waiting on a mutex held by logging. `esp_timer_get_time()` supplies64-bit monotonic timestamps;hardwarePWM runs independently of tasks. Arduino core3.3.x uses pin-based `ledcAttach/ledcWrite`;the included bench sketch's14-bit50Hz method has≈1.22 µs quantization (D). [Arduino LEDC reference](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/ledc.html), [P4 MCPWM](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-reference/peripherals/mcpwm.html) and [IDF FreeRTOS](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-reference/system/freertos_idf.html) define supported APIs. MCPWM offers explicit synchronization/fault features but replacing a workingLEDC setup is B;prove pulse widths on the scope first. No register map should be hard-coded in application code for these peripherals.

## 6. Radio, truthful CSV and storage

### 6.1 LR1121 initialization (A)

The official assignment was **visually checked on PDF page2,row18,team025**. MAIN866.50 MHz,backup865.25 MHz,SF9,BW125 kHz,CR4/5,sync0x99. Use MAIN by default and change channel only on Net Control instruction;do not preserve the old autonomous toggling failover. This is a raw LoRa point-to-point link using RadioLib,not a LoRaWAN network implementation. The assignment supports this link plan;do not claim NETID compliance from a LoRa sync byte alone.

Retain the supplied proven-to-compile RF-switch table,then verify on the actual Core1121/ANT2:STBY(LOW,LOW),RX(HIGH,LOW),TX_LP(HIGH,HIGH),TX_HP(LOW,HIGH),TX_HF(LOW,LOW),unused modesLOW/LOW. These are radio-internalDIO5/6,not processorGPIOs. The fitted module's path must be confirmed;the generic RadioLib example explicitly warns that board routing can differ. The supplied code forces HP PA for ANT2. [RadioLib v7.7.1 example](https://github.com/jgromes/RadioLib/blob/7.7.1/examples/LR11x0/LR11x0_Transmit_Blocking/LR11x0_Transmit_Blocking.ino), [LR1120 API inherited by LR1121](https://github.com/jgromes/RadioLib/blob/7.7.1/src/modules/LR11x0/LR1120.h), [Semtech LR1121 datasheet](https://files.waveshare.com/wiki/Core1121/LR1121_H2_DS_v2_0.pdf) and [Semtech driver SWDR001](https://github.com/Lora-net/SWDR001) are the references.

```cpp
// Setup outline for MAIN; each returned error must enter an RF fault state.
digitalWrite(9, HIGH);  pinMode(9, OUTPUT);    // flash deselected
digitalWrite(29, HIGH); pinMode(29, OUTPUT);   // radio deselected
SPI1.setMOSI(26); SPI1.setSCK(27); SPI1.setMISO(39);
SPI1.begin();
// LR1121 radio(new Module(29, 30, 25, 24, SPI1));
radio.setRfSwitchTable(SW_PINS, SW_TABLE);
int16_t rc = radio.begin(866.50f, 125.0f, 9, 5, 0x99, 10, 8, 1.6f);
if (rc != RADIOLIB_ERR_NONE) { /* latch RF init fault; no TX */ }
radio.setRfSwitchTable(SW_PINS, SW_TABLE); // reapply after begin/reset
// check each status, do not ignore these results:
// setCRC(2), setRegulatorDCDC(), setSyncWord(0x99),
// setOutputPower(10, true), calibrateImageRejection(863,870),
// setRxBoostedGainMode(true), startReceive().
```

The positional `begin` overload is retained in the checked7.7.1 header although marked deprecated;it explicitly sets TCXO1.6 V. Freeze that actual library blob/build rather than silently compiling against current `main`. `begin()` and calibration are boot operations with internal waits;calling this outline is not evidence of a nonblocking runtime driver. Runtime TX uses `startTransmit`,IRQ status/timeout,`finishTransmit`,then RX;no blocking `transmit()` in the flight loop. One IRQ dispatch reads radio flags and handles TX_DONE/RX_DONE/timeouts;never install competing callbacks onto one hardware IRQ.

Use+10 dBm while pad-authorized,~+14 dBm only in the assigned flight condition. Check RF-path and power settings after resets. CRC failure discards the packet. A success code proves command exchange with the chip,not radiated power,antenna match or range. Keep old22 dBm tests out of launch images.

### 6.2 Airtime and 1 Hz feasibility

**D calculation:**explicit header,CRC on,preamble8 symbols,SF9,BW125000 Hz,CR denominator5,LDRO off. `Ts=2^9/125000=4.096 ms`;`Tpreamble=(8+4.25)Ts`;`Npayload=8+5 ceil((8PL-4·9+28+16)/(4·9))`. Formula is cross-checkable with [Semtech's LoRa calculator](https://www.semtech.com/design-support/lora-calculator) and RadioLib's time-on-air implementation. All lengths include application bytes actually passed to radio;no LoRaWAN overhead is assumed.

| Application bytes | Calculated TX airtime |
|---:|---:|
|24|205.824 ms|
|28|226.304 ms|
|40|287.744 ms|
|120|635.904 ms|
|130|697.344 ms|
|138|717.824 ms|
|140|738.304 ms|
|160|820.224 ms|
|180|922.624 ms|
|200|1004.544 ms|
|255|1250.304 ms|

The absolute mathematical≤1 s payload ceiling is197 bytes with **no command window or margin**. It is not a practical packet budget. For a conservative half-duplex1 Hz schedule choose **downlink≤130 bytes, uplink≤32 bytes, total turnaround/guard40 ms**, giving984.128 ms. Verify actual RF intervals on hardware (T-RF); processing, clock skew and retries consume the remaining margin. Each second starts with a downlink, followed by at most one queued uplink. RF-off mode listens continuously. Blind retries stop when scheduled reception resumes.

A representative **length calculation only**,using the declared field widths,requires130 bytes before optional fields. It is a test vector in the offline check,not injected into LIVE telemetry. Latitude/longitude keep6 decimal degrees;using4 decimals to save bytes quantizes roughly11 m latitude (D),undesirable for precision landing. Packet count and mission time must have bounded variable decimal formats and capacity for the whole pad+flight period. Never clamp an unexpected measurement into a plausible number to fit:send an empty invalid field with a fault record,or reduce optional content. Validate the serializer's maximum length over its complete declared operating envelope before launch.

Original full-team-ID uplinks may exceed32 bytes. **Before-flight target updates can use the verbose protocol while telemetry is silent.** In-flight updates under strict1 Hz require the compact protocol in §10. If mandatory CSV exceeds130 bytes, measure the length and recompute the uplink allowance; never drop mandatory samples or quietly revert to0.5 Hz. Optional fields may use measured slack or separately budgeted diagnostic packets. Their display rate can be lower than the mandatory1 Hz.

High duty occupation follows from the requested RF plan and1 Hz requirement;the assignment is the competition instruction,not a general regulatory analysis. Confirm the organiser's allowed airtime plan at launch rather than importing a generic1%LoRaWAN duty-cycle model from the old GUI.

### 6.3 Exact CSV order and missing data

Freeze this header across MAIN,GS,GCS and the judges' file:

```text
TEAM_ID,TIME_S,PKT,ALT_M,PRES_PA,TEMP_C,BUS_V,GNSS_TIME,LAT,LON,GPS_ALT_M,SATS,ACC_X,ACC_Y,ACC_Z,SPIN_RATE,STATE
```

`GNSS_TIME` is **UTC seconds since midnight**,derived from an accepted receiver time,not the legacy packedhhmmsscc value. `TIME_S` is elapsed since initial mission power-on,including pad wait. UTC date/epoch belongs in optional/log records;handle midnight roll-over. MAIN's existing prelaunch/launch epoch distinction must be made explicit so the90-minute clock starts at launch detect without resetting TIME_S.

Core units/rounding:ALT/GPS_ALT0.1 m,P1 Pa,T0.1°C,V0.01V,lat/lon6 decimals (finer than required),accelerationm/s²,Hallwheeldeg/s,stateinteger0–7. Pressure and temperature are **measured sources**,not a fabricated weather model. Prefer the identified ambient HDC temperature when its age/quality is good;otherwise use the measured BMP die temperature **with that source limitation recorded**. Do not blend two different temperature types invisibly.

Each packet is a complete current snapshot;CR terminates it. An invalid/stale sensor field is empty,not0,not a held old value with a refreshed timestamp. The **field order and delimiters remain intact**. GCS records receive time separately and marks gaps. Confirm empty-field acceptance with the organiser because the rules do not prescribe invalid-value encoding. Do not use a parser's lack of missing-value support as permission to invent a value.

The current software's3-axis expansion is retained as stated in §2. Optional named columns append after STATE in an agreed schema;for this launch do not append every old science field or fill absent hardware fields. Suggested optional compact series:validity mask,GNSS age,fix-quality,guidance mode,target revision,windN/E,predicted landingN/E/uncertainty,reset count,owner,commandACK. Either append a **fixed declared subset** within byte budget or place diagnostics on a separately typed packet that the judges' core CSV parser cannot confuse with telemetry.

### 6.4 Logging and storage loss

Keep raw receive-only NMEA,raw pressure/sensor counts,calibrated SI data,estimator state/covariance,guidance setpoint,controller error/terms,commanded pulses,Hall edges/counts,fault transitions,state/event journal and radio TX/RX results. Each record carries source board/session,sequence,local sample time and mission-time mapping. Onboard binary records useversion/type/length/CRC;generate CSV offline. The mandatory ground CSV contains **received/transmitted measurements only**,never a later onboard backfill masquerading as an RF reception.

MAIN owns the sharedSPI1 radio/flash schedule. Pre-erase flash sectors while disarmed;page-program from aRAM queue and releaseSPI duringWIP. BACKUP'sSD task consumes P4 broadcast status plus its own sensors;to log MAIN's full data,MAIN must explicitly route those fields throughP4 or provide them in existing link frames. No directMAIN↔BACKUP dataUART is wired. Do not claim full backup of data that never traverses a cable.

**D capacity examples:**W25Q64=8 MiB;96-byte records at20 Hz consume1920 B/s,giving≈72.8 minutes before headers/reserved sectors. Full200 Hz recording would last≈7.3 minutes. Reserve launch/deployment/high-rate windows and lower pad rates;calculate using actual record sizes. SD latency has an unbounded practical failure tail;preallocate where possible,use a64 KiB ring buffer(D),monitor fill,discard lower-priority diagnostics on overflow and retain fault counters. Never wait forSDflush in a600 m event handler. Storage failure must not stop steering/deployment;show it in telemetry.

## 7. Software architecture, estimation and flight states

### 7.1 Board responsibilities and timing budgets

All WCET figures are **D reservations to measure**,not benchmarks. Periodic acquisition follows sensor data-ready rather than claiming identical IMU clocks. Use deadline-based scheduling,no catch-up burst that reprocesses the same sample. MAIN watchdog is fed only after every critical activity has advanced or explicitly entered a bounded degraded state. A timerISR that togglesheartbeat while the main task is dead is not a valid heartbeat.

| Board / task | Rate / priority | D CPU reservation and deadline |
|---|---|---|
|MAIN UART drain+RF service|Every loop/highest|≤1 ms service gap;100 µs/pass excludingbounded peripheral calls|
|MAIN ICM/AK|ready checks500 Hz;IMU220/225 Hz,mag100 Hz/high|≤700 µs/sample incl bus/compute;AHRS compute target≤200 µs|
|MAIN BMP|50 Hz/high|≤400 µs/sample;event age≤100 ms|
|MAIN nav/vertical fusion|new GNSS epochs +50 Hz predict/high|≤500 µs/update;no matrix inversion of full state when scalar update suffices|
|MAIN mission+heartbeat|100 Hz+10 Hz/high|≤100 µs;phase transitions/journal handoff bounded|
|MAIN telemetry+power+RTC|1 Hz/20 Hz/1 Hz/medium|serialization≤1 ms;radio async;I²C reads scheduled|
|MAIN logging/camera|queued/lowest|≤1 ms work slice if API supports it;otherwise move storage load out ofcritical path and measure actual stall|
|BACKUP sensor+links+lease|baro50 Hz,linkscontinuous,lease100 Hz/high|No need to run the old full FC;quarantinedIMU remains excluded|
|BACKUP SD writer|queued/low|No actuator task on thisMCU;captureSDstall andUARToverflow|
|P4 UART ingestion|event driven/core0,priority20 D|No indefinite UARTreadwait;bounded queues;small frames|
|P4 sensor/AHRS|208 Hz/core1,priority21 D|≤1.5 ms incl bus per sample;recordWCET|
|P4 event safety monitor|100 Hz/core1,priority23 D|≤200 µs;never waits forlogger/GCS|
|P4 guidance|20 Hz/core1,priority18 D|≤1 ms compute target;only acceptedliveNAV|
|P4 servo control|50 Hz/core1,priority22 D|≤300 µs;PWM edge jittermeasured,separate hardwarecounter|
|P4 links/logging/NVS|20 Hz/event/core0,priority8 D|OneTXowner;NVScommit in known windows;time-critical events cannotwaitforflash|

StartP4FreeRTOS with control tasks on one core to reduce shared-state complexity;put parsing/log output on the other. Respect the configured `configMAX_PRIORITIES`;the numbers above assume a compatible configuration and need adjustment if the chosen core differs. IRQ handlers only capture timestamps/counters/set flags. Use task-watchdog registration plus a progress mask,not oneunconditionalglobalfeed. Idle task starvation indicates a scheduling fault.

Proposed acceptance:criticalCPU utilization<50%,criticaldeadline misses0 in a2 h pad+flight workload test,remainingRAM≥25%,noUARToverflows,stack high-water margin≥25%(allD). These margins do not prove bounded SD/NVS latency. Bench under simultaneouscamera,radio,flash,servos and injected device failures.

### 7.2 Typed pipeline: code at every boundary

The linked [reference code](reference_code/README.md) contains host-checked guards, parser, controller, events, frame/log codecs, journal selection, estimator, planner and scheduling examples. [Integration boundaries](IMPLEMENTATION_STATUS.md) distinguish these from unimplemented platform adapters. The following integration blocks specify the remaining stages. They are **API sketches**,not independently buildable complete board programs.

```cpp
// 1 acquisition -> timestamp; at sensor rate, no fabricated "new" sample.
if (driver.ready(now)) {
    RawImu raw;
    if (driver.readBurst(raw)) rawQueue.push({raw, monotonicUs(), ++sampleSeq});
    else faults.note(IMU_READ_FAIL);
}
// RawImu: int16_t accel[3],gyro[3],temperature; sensor-specific byte order.
```

```cpp
// 2 unit conversion -> calibration -> mounting; ~208/220 Hz.
Vec3 a_mps2 = R_body_sensor * (A_accel * (rawAccel * accelScale - accelBias));
Vec3 w_rps  = R_body_sensor * (rawGyro * gyroScale_rps - gyroBias_rps);
// Calibration matrix/bias/version/CRC comes from this actual sensor's record.
```

```cpp
// 3 validity/plausibility; never substitute 1g,0 yaw or last-good as a new read.
ImuSample s{a_mps2,w_rps,sampleUs,sequence};
s.valid = allFinite(s) && !driver.overrun() && !rawSaturated(raw);
if (s.valid) imuSnapshots.publish(s); else faults.note(IMU_INVALID);
```

```cpp
// 4 filtering -> fusion; dt comes from distinct sample timestamps.
double dt = (s.sample_us - previous.sample_us) * 1e-6;
if (s.valid && dt > 0 && dt < maxQualifiedGap)
    attitude.update(s, calibratedMagIfFresh(), dt); // q body->navigation
if (baro.newSequence()) vertical.update(baro.h_agl_m,baro.variance_m2);
if (gnss.newAcceptedEpoch()) horizontal.update(gnss.position,gnss.velocity);
// A held snapshot can be read repeatedly but cannot be fused repeatedly.
```

```cpp
// 5 state estimate -> guidance; 20 Hz. Sample/provenance gates repeated here.
if (mission.parafoilCommanded && canopyStabilizationQualified && liveNavGate(nav,now))
    desiredCourse = planner.update(nav,target,vertical,windIfObservable());
else desiredCourse.invalidate();
```

```cpp
// 6 control -> actuator; 50 Hz, finite/saturation/phase checks at final writer.
if (desiredCourse.valid && actuatorPermit(mission,health)) {
    double brakeDiff = controller.update(desiredCourse,nav,dt);
    ServoPair pulses = measuredBrakeMap.limit(brakeDiff,trimBrake,dt);
    actuatorOwner.writeChecked(pulses);
} else { controller.reset(); actuatorOwner.writeMeasuredGlideTrim(); }
```

```cpp
// 7 logging -> telemetry; independent consumers of one immutable snapshot.
logQueue.tryPush(encodeVersionedRecord(snapshot,CRC32)); // records may drop;control may not wait
if (nextRadioDeadline(now) && rfPermit && !cutoffLatched)
    radioQueue.tryPush(formatCurrentCsv(snapshot)); // blanks when invalid,CR terminator
```

Rawcounts:int16/u24;sensorseconds:uint64µs;positions:doublelocally orint32e7 onwire;velocityfloatm/s;angles/ratesrad/rad/s internally;servoµs:uint16 only at the actuator layer. Convert deg/s for telemetry and magnetic µT at the driver boundary. UseENUorNED consistently:the guidance below uses **N,E,up** with course measured clockwise fromNorth. Body+Xforward,+Yright,+Zdown is a separate convention and its transform must be measured. Do not mix AHRS yawrelative-to-pad with true course-to-target.

### 7.3 Attitude estimator

Keep the current AHRS only after axis,sample-time and motor-disturbance tests;do not switch algorithms to fix a wrong mounting matrix. A complementaryMahony/Madgwick-type quaternion update is sufficient for this launch. With bodygyroω,biasb,orientationq:

`qdot = 0.5 q⊗[0,ω-b + kp e]`, `bdot = -ki e`.

`e` is the calibratedgravity-direction cross-product plus optional trustedmagnetic error. Normalizeq and reject nonfinite/near-zero norm. **D starting gains:**kp=0.5 s^-1,ki=0 during flight until bias adaptation is qualified;estimate gyro bias while stationary on ground. Gategravity correction when |a| differs fromg by>20% D or motion is high;gate magnetic correction if norm/innovation or motor-state checks fail. Gyro-only integration temporarily preserves short-term orientation with growing uncertainty;it does not create absolute yaw. [x-io Fusion](https://github.com/xioTechnologies/Fusion) is a maintained reference with rejection/overrange support;its current algorithm should not be described as identical to the original Madgwick implementation.

P4 has no magnetometer. MAIN's compass measures **payload orientation**,which may differ from canopyheading because of swivel/line twist/swing. Do not use payloadyaw as canopyheading or equate GNSSgroundcourse to air-relativeheading. The turn controller initially uses measured GNSScourse response;IMU yawrate damping is B until a drop proves the body-to-canopy relationship.

### 7.4 Position, vertical velocity and wind

**A:**a small error-tolerant estimator with separate horizontal and vertical filters. Horizontalstate`[N,E,vN,vE]`;constant-velocity prediction at20–50 Hz,GNSSposition+velocity update only for new acceptedepochs. Eachaxis uses`F=[[1,dt],[0,1]]`,acceleration-noiseQ=`σa²[[dt⁴/4,dt³/2],[dt³/2,dt²]]`. D initialσa=1 m/s²,positionσ=5 m,velocityσ=0.5 m/s if no trustworthyreceivererror estimate. HDOP scalesgeometry but is not itselfσmetres. Adapt from innovationrecords,not optimism. UseJosephcovarianceupdate,finite/positivity checks and a documentedinnovationgate. Do not averagerawNMEA and MAINcopy from the samereceiver.

Verticalstate`[h_up,v_up]`,baroAGLmeasurement50 Hz;Dinitialσbaro=1 m andσverticalaccel=2 m/s² until pad/rampdata refine them. GNSSMSLaltitude can sanity-check drift but must first be referenced to the measuredpadMSLandnotellipsoidheight. Do not overwritepadreference in ascent. A fullaccelerometer-driven verticalINS is B because gravity/remounting/payloadswing errors can be larger thanbaronoise.

**B augmented EKF:**`x=[N,E,h,vN,vE,vh,wN,wE,Va,ψ]`,with

`Ndot=vN`, `Edot=vE`, `hdot=vh`, `vN=Va cosψ+wN`, `vE=Va sinψ+wE`, `ψdot=r`, with slow random walks for wind and airspeed. GNSS observes position and ground velocity; the barometer observes height; a validated canopy heading or turn model supplies heading information. Without heading information or varied turns, wind and Va are **unobservable**. No UKF can remove this limitation. An initial wind process sigma of0.05 m/s/√s is D only; increase it during shear or model mismatch. Do not publish an unexcited wind estimate as a measurement.

For a launch-feasible GNSS-only wind fit during a safe, steady turn, fit a circle to several diverse ground velocity vectors:

`(vN-wN)^2+(vE-wE)^2=Va²`.

Linear form: `2vN wN +2vE wE +c = vN²+vE²`, where `Va²=c+wN²+wE²`. Use weighted least squares, geometry/condition-number checks and residuals. Require at least12 accepted new epochs,120° of velocity-direction coverage, steady brakes/sink and a small residual before publishing valid wind (all D). Straight flight cannot separate wind and airspeed. A full turn may be unsafe in high wind or at low altitude; skip the fit in those cases. Gate GNSS course by speed, quality and uncertainty.

[NASA's on-the-fly parafoil aerodynamic/wind estimation research](https://ntrs.nasa.gov/citations/20200011546) explains whywind/modelestimationmatters. The [convex real-time parafoil guidance paper](https://arxiv.org/abs/2105.00715) provides a trajectory-optimization route;the launchdesignhere deliberately uses a simplerplanner because the flightpolar and steeringresponse are unmeasured. No paper'slandingaccuracy transfers to thisCanSat.

### 7.5 Flight state machine and mechanism events

| State | Meaning / entry (D unless R) | Allowed actions |
|---:|---|---|
|0 BOOT|Restore journal and identify hardware; use airborne recovery if needed|RF receive, ESC stop, conservative pulses; no automatic mechanism test|
|1 TEST_MODE|Explicit ground session, disarmed, actuator power controlled|Individual bench tests; inaccessible through in-flight commands|
|2 LAUNCH_PAD|Pad reference valid, required checks passed, mission armed|TX after command; stationary calibration; camera available|
|3 ASCENT|AGL>10 m and v_up>0.5 m/s continuously for2 s; retain peak|Camera REC; persist launch deadline; bay, petri and steering inhibited|
|4 ROCKET_DEPLOY|Number/name retained; means confirmed drone release|Passive drogue; persist release; monitor950 m crossing|
|5 DESCENT|Release confirmed; v_up<−0.5 m/s for0.5 s with valid barometer|Petri event; prepare600 m bay trigger; qualified ESC operation only|
|6 AEROBREAK_RELEASE|Secondary bay command committed at600 m crossing|Hold bay open; stabilize; then bounded qualified guidance|
|7 IMPACT|AGL<5 m, absolute v_up<0.3 m/s, GNSS speed<0.5 m/s if valid, stable IMU for10 s; or validated impact plus settling|ESC stop, beacon, tested brake trim; no automatic rearm|

These are conservative starting thresholds, **not characterized release detectors**. Test actual lift, hover and descent profiles (T-FSM). A drone can descend with the payload attached: barometric descent or an apogee does not prove release. **The fixed hardware cannot guarantee automatic physical release verification.** The strict software path requires a `RELEASE_CONFIRMED` command sent after observed physical release and latched on P4. A sensor-only heuristic relaxes that guarantee and needs an explicit team decision and separate tests; it is not assumed here.

If the confirmation link fails before P4 latches release, the strict inhibit keeps bay and petri actuation off; the passive drogue is the fallback. If both complete early link-loss autonomy and positive release verification are required, a wired release interlock is a hardware change (§12). Confirmation latency must also permit petri opening at950 m during fast drogue descent. Rehearse the command and pressure crossing together; a several-second ACK delay can miss that event.

**Petri:** on a descending950 m crossing, require release confirmation and a valid pad reference. If not already started, commit `petri_started` and the mission start time, command the measured open pulse, close at start+10 s and commit completion. A reset preserves the original deadline; it never starts another10 s exposure. With untrusted restored time, close and retain the event/fault history. Descent is the assumed crossing direction because pre-release actuation is inhibited; the team still needs to confirm that interpretation.

**Bay:** the requirement is600±10 m. Trigger on descending predicted event altitude `h_pred=h+v_up·Ldeploy` crossing600 m, using **measured mechanism latency** Ldeploy. If latency is unknown, a600 m command does not prove physical opening at600 m. At25 m/s,400 ms delay consumes the entire10 m tolerance before barometer/model error (D). Log command time and independent video or observer evidence of opening. A servo ACK does not prove canopy inflation. No automatic drogue cutaway is assumed; inspect the retained drogue for interference.

### 7.6 Persistent state and redundancy

Journal fields: magic, version, mission ID, generation, mission-start RTC, launch RTC, elapsed checkpoint, p0/T0, calibration IDs, state, armed/release flags, bay intent, petri start/deadline/completion, cutoff latch, target/revision, CRC and commit marker. Alternate two slots: write payload and CRC, verify, then commit. On boot select the highest valid generation. Never overwrite both copies first. Publish critical journal changes over the P4 links.

MAIN uses the battery-backed DS3231 and stored mission epoch; require trustworthy OSF-clear time. TIME_S never decreases, and TX_ON cannot change the launch deadline. BACKUP and P4 keep running their monotonic clocks during link loss. After reset they recover elapsed time from fresh MAIN time or a proven retained RTC source. **An isolated P4 cold restart cannot reconstruct exact elapsed wall time from an NVS checkpoint alone.** Declare time uncertain, preserve completed events and use conservative recovery. No independent battery RTC is wired to P4.

Use idempotent absolute servo positions for restoration. **Exactly-once physical actuation cannot be proven across a power failure without feedback.** Journaling before motion risks a reset before movement; journaling afterward risks repetition. Record intent first and reconcile the intended absolute position and deadline after restart.

**Explicit airborne recovery policy (D, T-RESET):** do not capture a new pad datum. Restore release/arming and validate fresh barometer samples against the persisted reference. If a bay intent exists, reassert the measured open position. If release is confirmed, bay intent is absent and valid AGL is already below the compensated600 m threshold, commit/open immediately and log a late-deployment fault; the missed altitude cannot be repaired. If still above threshold, resume crossing detection. A petri exposure already started uses its old deadline; one not started and already below950 m is marked missed and remains closed rather than inventing an on-time exposure. With uncertain time or mission identity, close petri, stop the wheel and preserve fault/event history. The reference crossing helper intentionally leaves these board-level recovery decisions to its journal adapter.

Authority policy (D): MAIN lease500 ms; BACKUP may supply navigation/mission support after MAIN lease and heartbeat fail for1 s. Require several valid frames for handoff. P4 always owns physical outputs and event latches. A restarted MAIN session must resynchronize before regaining authority. Reject conflicting commands from stale owners/revisions; primary loss never clears release or deployment history.

With both links dead, P4 uses its local BMP/LSM, raw GNSS, valid mission latches and last accepted target. MAIN attitude is not needed for the course loop. GNSS, its TX wire, shared power and antenna remain common failure points. BACKUP cannot reset the L89, drive MAIN's buzzer or start the radio. This is not full triple-modular redundancy.

## 8. Precision-landing guidance and control

### 8.1 Model and reachable area

The brief's 0.40 m² canopy and approximately 1 kg suspended mass are **H assumptions**, not a tested polar. Area and mass alone do not determine forward speed, sink, stall brake, turn rate or deployment reliability. The supplied parafoil study used a different geometry and includes evolving simulation reports. Its 30 m-class predicted dispersion is not flight evidence and must not appear as achieved accuracy in the GCS.

Use measured trim forward speed `Va`, sink `s=-v_up>0`, turn authority and brake map after deployment. A simple flight model is:

`p_dot = Va [cos ψ, sin ψ] + w`, `h_dot = -s(b_common,Va)`, `ψ_dot = r`, `τ_r r_dot + r = K_r u_diff`.

`ψ` is canopy air heading, not payload yaw. Keep measured GNSS ground velocity as the primary navigation observation. Calculate in a local tangent frame around a **measured** pad origin:

`N≈R(φ-φ0)`, `E≈R cos(φ0)(λ-λ0)` in radians, with WGS84 local curvature or a qualified geodesy routine for the final implementation. A spherical `R=6378137 m` approximation is D for the small test area; validate against a WGS84 implementation. Never initialize the current position to the target.

Nominal remaining time is `t_go=h_AGL/s`; reject invalid/nonpositive sink, and bound it using measured sink uncertainty. A first reachability check is `||target-p-w*t_go|| ≤ Va*t_go`, with extra margin for turn radius, deployment delay and wind uncertainty. If the target is unreachable, show **UNREACHABLE** and choose the best reachable point in the organiser-approved landing area. Do not increase brake to manufacture forward penetration into wind faster than the canopy can fly.

Only use a valid wind fit. With desired ground-track unit vector `t=[cos χ,sin χ]`, right normal `n=[-sin χ,cos χ]`, crosswind `w_cross=w·n`, the air-heading correction is `δ=asin(-w_cross/Va)` and `ψ_cmd=χ+δ`. Feasible along-track speed is `Va cos δ+w·t`; if `|w_cross|≥Va` or along-track speed≤0, mark that leg infeasible. This equation is a D engineering derivation from vector addition, not proof that Va/wind are observable on the current flight.

For launch A, direct control of **measured ground course** already corrects moderate wind drift without needing an air-heading sensor. Do not apply both a wind crab correction to a ground-course loop and then expect the same desired ground track; that double counts wind. Wind compensation belongs in the planner/air-heading model, while the course loop tracks the planned **ground** course. Without a validated canopy-heading estimate, retain the ground-course loop.

### 8.2 Guidance phases (D; T-GUIDE, T-GLIDE)

| Mode | Entry and behavior | Exit / inhibit |
|---|---|---|
|INHIBITED|Before confirmed release or before secondary command; brake trim only|No homing during drone carriage or drogue phase|
|DEPLOY_STABILIZE|After bay command, hold measured glide trim; wait a qualified inflation interval and stable sink/IMU response|A guessed timer does not prove canopy inflation; reject unstable samples|
|HOME|Compute course to the active target or approach-entry point from the latest live position; bounded differential brake|Reachable-area test and new GNSS epochs drive planning|
|ENERGY_MANAGE|If substantial excess height exists, follow a bounded racetrack/figure-eight inside the approved area, preferably upwind|Do not orbit solely because range is small; verify turn radius, drift margin, altitude reserve|
|APPROACH|Leave holding when remaining height approximately matches the measured path-to-final height plus reserve|Continue checking feasibility and actual sink|
|FINAL|Straight upwind ground track toward target, limited turns, no untested flare|Freeze target updates that would demand an abrupt low turn|
|NAV_DEGRADED|No acceptable current position/velocity; reset course integrator and smoothly return to tested symmetric glide trim|Require five new coherent accepted epochs before re-entry|
|LANDED|Stable landing declaration; stop wheel and start beacon|No automatic mission rearm|

“Into wind” means final **velocity opposite the wind vector**. For wind toward +North, start north/downwind of the target and fly south. Use explicit vector math: `q_entry=target+w_hat*D_final`, final track `-w_hat`. If wind is not observable, use a team-selected approach direction from measured field wind, identify it as an operator input, and retain live GNSS course feedback.

Choose final distance `D_final=(Va-|w|)*H_final/s` for a straight into-wind leg only when positive and supported by the model. This is an estimate, not a command to cross hazards. A conservative initial `H_final=50 m`, reserve `20 m`, turn-rate limit `10 deg/s`, minimum GNSS ground speed for reliable course `1.5 m/s`, and energy-mode altitude floor `100 m` are **D test values**, not calibrated settings. Below the final floor, prefer a stable straight approach over aggressive last-second target pursuit. Tune from the actual canopy, sink, terrain reference and test data.

A simple bounded holding law uses a circle or racetrack whose radius exceeds the measured minimum turn radius. For a circle with radial distance ρ, radius R_hold and selected direction, command a tangent plus an inward radial correction, e.g. `v_dir = tangent - k_radius*(ρ-R_hold)*radial`, normalize, then convert to a desired course. Do not fly near the circle center where radial direction is undefined; enter via a tangent. Radius and handedness must fit the approved area. A tight spiral can increase sink or collapse a lightly loaded wing; it is not an automatic energy-management default.

Predict landing with the same measured model and assumed current plan. The simplest diagnostic `p_pred=p+v_ground*t_go` assumes constant ground velocity and is **not a final guided landing prediction**. A better B predictor propagates the bounded commanded trajectory, wind layers and measured sink to ground; show a covariance/ensemble footprint. Never label either prediction as the measured current position.

### 8.3 Controller structure and tuning

Start with a **low-bandwidth course P controller** using fresh GNSS ground course and speed. Let `eχ=wrap(χ_des-atan2(vE,vN))`. Initial outer law `r_cmd=sat(kχ eχ, ±r_max)`, then a measured inverse steering map converts r_cmd to brake differential. Add an inner yaw/turn-rate loop only after identifying a stable canopy response. Payload gyro yaw rate is not automatically canopy turn rate; compare it against GNSS course change in actual drops.

For a validated turn model, inner PI:

`u_unsat = u_ff(r_cmd) + Kp_r(r_cmd-r_meas) + I`,
`I_dot = Ki_r(r_cmd-r_meas) + Kaw(u_applied-u_unsat)`.

Use the **applied** command after saturation and slew limiting for anti-windup. Reset/freeze I on invalid navigation, deployment, authority handoff and landing. Derivative is off initially; if later needed, differentiate a filtered measurement rather than the setpoint and account for GNSS interval jitter. Set a small measured course deadband and a neutral brake deadband only after characterizing friction; avoid on/off chatter at the target.

**No flight-qualified numeric PID gains exist in this evidence.** The included reference P controller uses D `k_course=0.25 normalized brake/radian`, `u_max=0.20`, `slew=0.25 normalized units/s`, `Ki=0` to demonstrate signs, limits and gates. It is a bench starting configuration, not permission to fly those values. Normalized brake=1 means the **measured safe maximum**, not a full servo sweep or an assumed stall setting.

Practical tuning sequence:

1. Ground rig: identify positive left/right commands, released-line trim and safe travel. Confirm which differential sign makes a right turn; do not infer it from mirrored servo pulse signs.
2. First authorised short glide: fixed trim, then small symmetric left/right brake steps at safe height. Log actual pulses, new GNSS velocity epochs, sink and gyro; return to trim between steps. Avoid sustained oscillation tests.
3. Fit `r(t)=K_r u(1-exp(-(t-Td)/τ_r))` after delay Td, using separate left/right fits and rejecting deployment/swing segments. If a clean first-order response is absent, use a lookup table plus slow outer P instead of forcing this model.
4. Choose an IMC-style PI only if justified: for `K_r/(τ_r s+1)` with delay Td, `Kp_r≈τ_r/[K_r(λ+Td)]`, `Ti≈τ_r`, `Ki_r=Kp_r/Ti`. Start λ longer than the measured lag and pendulum effects, then increase authority gradually. These are model-based starting equations, not guaranteed stable gains.
5. Adjust outer course gain to keep heading response slower than the inner rate response and avoid line/payload swing. Use rise/settle time, overshoot, sink increase, saturation fraction and landing error from independent drops; do not use Ziegler–Nichols induced oscillations in a small uncharacterized parafoil.
6. Schedule gains/common brake only against **measured** Va/response data. GNSS ground speed alone is not airspeed for gain scheduling in wind. No airspeed sensor is wired.

### 8.4 Servo mapping and physical limits

Manufacturer evidence for [B26CLM](https://www.agf-rc.com/agfrc-coreless-mini-servo-b26clm-p1669135.html): 4.8–8.4 V, center1520 µs,333 Hz capability, stall torque9.5 kg·cm at7.4 V/10.5 kg·cm at8.4 V, speed0.085 s/60° at7.4 V. These are advertised conditions, not measured loaded steering performance. Published stall current, guaranteed signal-loss position and backdrive/holding behavior were not established. Do not assume a worm gear, mechanical lock or full-range pulse endpoints from the brand/model name.

The [Tower Pro MG90S page](https://towerpro.com.tw/product/mg90s-3/) is inconsistent about voltage versus performance test points; use the existing regulated5 V rail and verify the actual units. Its listed torque1.8 kg·cm at4.8 V does not establish latch margin. Pulse endpoints, lid positions, stall current and no-signal behavior must be measured for each mechanism.

Use50 Hz initially for both steering and MG90S.333 Hz is B for B26CLM only and may require programming;keep MG90S on its independently qualified frame rate. A333 Hz frame is about3 ms, so pulse range and peripheral resolution must fit. Higher update rate does not remove GNSS/control delay or prove faster loaded motion. Do not globally change every servo timer to333 Hz.

For a spool radius r and angle change Δθ radians, line travel `Δl=rΔθ`; for a horn with attachment geometry, measure the actual nonlinear line displacement versus angle. A D example r=15 mm and60° gives15.7 mm travel. Required torque `τ=F_line*r`; compare measured line tension, transient opening loads and the working supply with a derated continuous thermal capability, not just stall torque. Without a measured radius/tension, no numerical torque margin is justified.

Map brake fractions bL,bR through separate measured pulse tables. One convention is `bL=b_common+max(-u,0)`, `bR=b_common+max(u,0)` with positive u defined as a right-brake command **only after sign qualification**. Clamp each to the tested safe pull range, rate-limit line travel, and preserve room for trim. Equal pulse widths do not mean equal brake travel on mirrored installations.

Keep a periodic pulse to hold a commanded servo position under normal power. Stopping PWM does not reliably “lock” a digital servo. Power loss cannot be solved in software;use the existing latch geometry if it is mechanically stable. Bay open should be a stable physical state;petri closure after its deadline should not depend on a blocked task. Do not detach a loaded brake servo in flight as an assumed safe fallback.

Power: test simultaneous steering transients plus both small servos, camera, wheel and radio. INA260 only measures the battery path;it does **not** verify the7.4–8.0 V rail or fast dips. Scope that rail at the servo connectors and logic rails at MCUs, including overshoot when load releases. Capacitor hold-up follows `C≥I·Δt/ΔV`;a470 µF capacitor with2 A for5 ms would droop≈21 V in an idealized calculation,so the fitted bulk capacitance cannot replace regulator current capacity. Inspect connector/return-current drops and avoid motor current in sensor ground paths. Required regulator model/current/transient data remain U.

ESC/EMAX/Hall: the exact ESC is unconfirmed, so only stop/idle pulses may be assumed after bench qualification. Keep1000 µs stop as the supplied starting convention,never perform throttle-endpoint calibration automatically at boot. Wheel target10000 RPM is a team objective,not a safe rotor rating. With one magnet per revolution this is166.7 pulses/s and60000 deg/s(D);use an optical tachometer/reference to verify Hall PPR and sign convention. KV2300 is a motor speed constant,not a closed-loop RPM measurement. The rotor attachment, balance, containment, ESC input protocol, signal-loss response and brownout restart need a guarded test. Hall stuck/no edges while throttle is commanded ->stop wheel and latch fault;never increase throttle blindly.

### 8.5 Millisecond execution versus navigation latency

**D budget at5 Hz GNSS/115200:**≤200 ms epoch period, assumed≤17.4 ms for a200-byte epoch on UART,≤1 ms parsing,≤50 ms wait for20 Hz guidance,≤20 ms next50 Hz PWM frame. Sum≈288.4 ms from a worst-phase physical observation boundary, **excluding receiver internal navigation latency**. From receipt of the complete accepted epoch to next PWM edge, the budget is≤71 ms;the actual planner computation target is≤1 ms. At1 Hz the sampling component alone is up to1 s. Do not call a1 ms calculation a1 ms end-to-end navigation loop.

Use RX byte/epoch time, measurement UTC, sample counter, planner-start/end, command-write and scope PWM-edge timestamps. Measure jitter and high-water marks with all tasks active. Hardware PWM keeps edge timing stable while a task runs,but a CPU stall still delays new pulse values. T-TIMING requires planner WCET≤1 ms,accepted-epoch-to-command-write≤55 ms under the declared schedule,and PWM output within the measured frame bound,with no control-deadline miss during the qualified workload. These are acceptance targets,not results.

## 9. Fault management and safe defaults

| Failure | Detection | Response with current wiring / residual risk |
|---|---|---|
|GNSS invalid/stale/replayed epoch|Checksum,fix/status/coherence,ages,new epoch,innovation|Invalidate navigation;reset integrator;measured symmetric glide trim;no target-vector update from held coordinates|
|GNSS RAW lost at P4 only|Raw timeout,MAIN epoch still fresh|Use MAIN copy with original age and transport uncertainty;do not combine as an independent fix|
|MAIN stops|Heartbeat edge+lease+link timeout|P4 local guidance;BACKUP support;downlink/beacon/camera control are not recoverable through BACKUP wiring|
|BACKUP stops /SD fails|Lease/CRC/storage counters|MAIN/P4 continue;onboard data redundancy reduced,reported honestly|
|P4 stalls/resets|Task progress watchdog,reset reason|Stop ESC at earliest initialization;restore mission/event journal;reassert measured safe pulses;no ground sweep. Hardware signal-loss behavior remains a residual risk|
|Barometer fails/stuck|Age,DRDY,pressure/temperature plausibility,cross-board disagreement|Use a fresh qualified remote barometer only with same persisted datum;no GNSS-altitude substitution for600±10 m unless separately qualified|
|IMU saturated/wedged|Range/status/constant0xFF,identity,stale|Reject sample;recover only affected device;course guidance may continue with reduced authority if tested|
|Magnetic interference|Norm/innovation correlated with wheel/current|Reject compass correction;do not steer from disturbed yaw|
|Corrupt/repeated command|Frame CRC,strict grammar,mission/transaction/session checks|Reject or return cached ACK;no repeated bay/petri timer restart|
|Partial target update|Staged fields/revision/commit|Keep previous complete target;echo active revision|
|Servo stall /line jam|Current spike is only indirect;no position feedback|Limit travel/duration;return to qualified trim if possible;fault detection is incomplete without feedback|
|Steering brownout|Bench rail scope;in flight battery proxy only|Software cannot guarantee detection from INA260;retain supply margin and mechanical fail behavior|
|Hall lost while wheel commanded|Pulse age/PPR limits|Stop ESC;flag measured RPM invalid rather than reporting requested RPM|
|Clock/journal corrupt|CRC/version/OSF/monotonic checks|No pad recalibration in flight;retain conservative phase;flag time uncertainty;do not clear TX cutoff|
|Radio BUSY stuck/CRC failures|Bounded transaction/IRQ deadline|Radio-specific recovery and RX re-entry;never block mechanism control on a radio error|

Safe outputs before mission restore:ESC stop;no spare PWM;bay and petri at their **measured safe closed positions only when a valid ground state exists**;steering at measured released-line trim. During a verified airborne reset,do not close a bay that was already opened or re-tension released lines without the tested recovery policy. Until state is known,there is no single universally safe servo sweep. Log RESET_UNCERTAIN and choose the mechanically qualified conservative position.

No fallback fabricates position,wind,gyro spin,voltage,current or camera success. Estimated values have explicit source/validity/uncertainty;mandatory GNSS fields remain receiver-derived and blank when invalid. An estimator prediction may support a short tested degraded policy,but must not be transmitted as a new GNSS observation or used indefinitely to chase a target.

## 10. Code organization and ground-station migration

### 10.1 Modules and existing-code disposition

Use one shared protocol/types library and three board applications. The supplied repository remains a provenance snapshot; the reference modules and integration contract live in this handoff. See [module status](IMPLEMENTATION_STATUS.md) before adopting any code. The research prompt requests architecture and key blocks, not proof of a flashed/flight-qualified image.

```text
firmware/shared/{Units,Validity,Protocol,CRC,Calibration,JournalSchema,TelemetrySchema}.h
firmware/main/{PinsMain,SensorDrivers,LiveGnss,RadioService,Camera,Clock,Journal,Telemetry,LogQueue,Main}.cpp
firmware/backup/{PinsBackup,BaroEnv,LiveGnss,BackupAuthority,SdLogger,Backup}.cpp
firmware/parafoil/{PinsP4,SensorTask,LiveGnss,Estimator,Planner,CourseControl,ActuatorOwner,Events,Journal,LinkTask,P4}.cpp
gcs/{protocol,live_transport,telemetry_schema,target_transaction,map_guidance,flight_csv}.py
tests/offline/{nmea,epochs,radio_budget,protocol_crc,fsm_resets,controllers,telemetry_schema}/
```

| Existing file/function | Keep | Change / remove before flight |
|---|---|---|
|`firmware/vritra_fc/Config.h` pins/radio/states|Keep verified mission and radio constants|Replace old Serial1 GNSS, BMP INT pin0, TB6612 and I²S pins; set flash CS9; update states0–4 to0–7; remove absent hardware fields|
|`Sensors.cpp` I²C/barometer/ICM|Keep bounded recovery and checked conversions|Use MAIN Serial2; coherent GNSS epochs and RMC validity; remove blind PMTK fallback and stale-position refresh|
|`Sensors.cpp:nmeaRMC` around1749|Keep date and knots conversion|Check RMC status A, numeric grammar, course, epoch and age before updating velocity|
|`Sensors.cpp:nmeaGGA` around1704|Keep checksum front end|Validate coordinates, hemispheres, minutes and fix quality; exclude predictions from GNSS fields; never convert empty input into position0|
|`Radio.cpp` around200–430|Keep SPI1 MISO39, TCXO, switch table and async service|Use decimal CSV; remove autonomous backup-channel switching; check all configuration results and timeouts|
|`vritra_fc.ino:execCmd` around429–485|Keep TX_OFF independent of disarming|Remove cutoff override; restrict destructive/calibration commands in flight; forward wheel commands to P4 with phase checks|
|`vritra_fc.ino:buildPacket` and `Base36.h`|Keep only for archived decoding|Use decimal core CSV in the launch path unless an exception is approved; update all endpoints together|
|`Motor.cpp/.h`|Old TB6612 implementation is obsolete|Replace with qualified P4 ESC/Hall control; MAIN must not drive old motor pins|
|`Beacon.cpp/.h`|Keep portable timing logic|Remove I²S/MAX98357A; use active buzzer on GPIO2|
|`DataLogger.cpp`|Keep qualified queues and versioned records|Remap flash bus/CS; inspect blocking calls; disable unverified PMTK assistance; log raw samples and validity|
|`board_tests/*`|Keep pin maps and isolated bring-up methods|Ground tests only; remove interactive sweeps from flight builds; verify actual hardware/core versions|
|`vritra_gs.ino` command table around100–170|Keep reserved IDs and radio hardware map|Replace int16 coordinate arguments; remove repeated toggle bursts and sticky-field reconstruction; pass decimal CSV transparently|
|`gcs/vritra_gcs.py` SCHEMA~250, parser~490, CSV writer~1920|Keep SI plots, presence flags, port detection and filename|Update states, INA260 labels, time encoding and airtime; isolate simulation classes from the flight entrypoint|
|`vritra_cam.ino`|Keep UART commands and local SD recording|Verify PSRAM, camera identity, storage errors and REC ACK; never assume recording succeeded|

These line numbers are navigation pointers into the supplied snapshot,not an immutable upstream commit. Hashes in the bundle manifest identify that snapshot. Historical August code is not an additional current flight implementation.

### 10.2 Libraries and reproducibility

| Component | Version decision / source | Launch recommendation |
|---|---|---|
|Teensyduino|Team records1.62; [PJRC](https://www.pjrc.com/teensy/td_download.html)|Freeze tested compiler/core and bundled SD/SPI/Wire; no platform build was reproduced here|
|Arduino-ESP32|Required3.3.x; team records3.3.11 for camera; [releases](https://github.com/espressif/arduino-esp32/releases)|Lock actual tested P4/S3 versions and chip revision; avoid a launch-week upgrade|
|RadioLib|Verified7.7.1 [tag](https://github.com/jgromes/RadioLib/tree/7.7.1)|Freeze the tested blob; the positional begin overload remains available; verify every result|
|BMP5 SensorAPI|Downloaded header reports1.5.0; [Bosch](https://github.com/boschsensortec/BMP5_SensorAPI)|Use as configuration reference; pin the actual commit/hash if adopted|
|LSM6DSO32 PID|[ST](https://github.com/STMicroelectronics/lsm6dso32-pid); no semver asserted|Pin reviewed source; use official register and self-test helpers|
|ICM-20948|[SparkFun](https://github.com/sparkfun/SparkFun_ICM-20948_ArduinoLibrary); release unselected|Community reference; keep a qualified direct-register driver if that reduces change|
|INA260/HDC302x|[Adafruit INA260](https://github.com/adafruit/Adafruit_INA260), [HDC302x](https://github.com/adafruit/Adafruit_HDC302x)|Reference implementations; TI documentation controls commands and conversions|
|AHRS|Existing implementation or [x-io Fusion](https://github.com/xioTechnologies/Fusion)|A: keep qualified code. B: compare Fusion offline and pin if adopted|
|Semtech SDK|[SWDR001](https://github.com/Lora-net/SWDR001), [SWSD003](https://github.com/Lora-net/SWSD003)|Reference material; do not add a second radio stack before launch|

No driver/library is required for an analog PWM servo or activebuzzer beyond the MCU peripheral API. No official ESC SDK can be selected before the actual ESC is identified. Version numbers that could not be verified are explicitly unselected;the team should freeze the working installed copies,record SHA256 and retain installers/source archives.

### 10.3 Commands, adjustable target and ACK behavior

Preserve existing IDs:1TX_ON,2TX_OFF,3CALIBRATE,9BEACON_ON,10BEACON_OFF,11FAILOVER,12RESET,13GYRO_ON,14GYRO_OFF,15GYRO_SPD,16MAG_CAL,17GPS_RESET,18NEW_MISSION. **4–8 remain retired.** FAILOVER becomes an explicit authorised channel selection with ACK/scheduled-switch recovery,not a repeated toggle. NEW_MISSION and calibration are ground-only;GPS_RESET is blocked in flight unless a specifically tested recovery policy calls it. TX_ON must not arm mechanism outputs merely because an operator wants telemetry;retain a separate mission-arming interlock or explicit readiness check.

D extensions:19 TARGET_LAT,20 TARGET_LON,21 TARGET_COMMIT,22 RELEASE_CONFIRMED,23 MISSION_ARM,24 GUIDANCE_ENABLE. Stage both signed int32e7 coordinates under one transaction. Commit requires both fields, valid ranges, an approved landing area, allowed mission phase and a valid frame. Publish atomically. A repeated commit returns the same ACK without a new revision. The GCS shows “active” only after P4 echoes the applied coordinates and revision. MAIN's queue ACK alone is insufficient.

The existing `<TEAM_ID>,C,<id>,<arg>,<seq>` uplink has an int16 argument and48-byte buffer; both need review for coordinates. Proposed compact example: `025,C,19,267175350,FF,12345678`. The coordinate is a **target**, never a navigation fix. Fields are short team alias, C, decimal command ID, signed decimal int32 argument, two-hex-digit transaction ID, and eight-hex-digit mission session. Each packet is at most32 bytes, **without a terminator**, with PHY CRC; the RF packet length defines its end. The mandatory downlink still ends in CR. A paired version handshake binds025 to the full team; a persistent monotonic session counter changes each new mission and must not be reused after storage corruption. Latitude, longitude and commit carry the same transaction ID; deduplicate by session+command+transaction, not transaction alone. Ground USB can expose `CMD_TARGET <lat> <lon>` while the gateway stages and commits across slots. This is a coordinated protocol update, not cryptographic authentication.

Use wrap-aware sequence comparison and a bounded duplicate cache tied to the mission/session. Six repeated legacy commands can otherwise toggle a state repeatedly. Prefer absolute desired states. Release confirmation is a one-way latch; packets from an earlier mission cannot release a new one. ACKs must fit the downlink budget or a separately calculated diagnostic window. A second full CSV does not automatically fit.

### 10.4 GCS changes

Keep the mandatory live plots and judges' CSV. Add target staging/ACK status, applied revision and coordinates, approach direction and feedback for rejected low-altitude updates. The map shows current valid position and age, track, target, approach and predicted landing footprint. Before a fix, show “NO CURRENT FIX.” A last-known marker must be visibly stale and excluded from current measurement fields.

Wind display includes N/E components, magnitude, direction, fit age, coverage, residual and validity. Unknown wind is not0 m/s. Show guidance mode, owner, target revision, requested/applied brake, payload attitude, ground course, AGL, GNSS age and faults. Link health includes sequence gaps, age, RSSI/SNR, board sessions, lease and heartbeat. Correct the wheel plot's old180/360 deg/s alert bounds:10000 RPM produces60000 deg/s, not a body-rate-scale value.

The current `parse_packet` initializes entries to0 and returns a `present` set. Every renderer, exporter and derived calculation must respect that set. Write empty fields for absent or stale data. Remove gateway behavior that makes old optional fields appear current. Preserve a verbatim receive log with reception timestamps.

To enforce data isolation, the flight GCS entrypoint must not import or instantiate DummyDataStudio, Simulator or Replay. Flight firmware contains no replay adapter or coordinate-default branch. HIL runs in a separate executable with actuator power isolated; TEST records use a separate output path. LIVE mode requires a real serial transport and never falls back to simulation after a connection error. Verify that unplugged serial cannot produce new flight CSV positions.

## 11. Calibration and verification campaign

### 11.1 Field calibration and storage

| Task / priority | Procedure / stored result | Acceptance evidence |
|---|---|---|
|Gyro A|Stationary, thermally settled samples; store mean bias in rad/s, variance, temperature and duration|Reject motion; repeat with wheel off/on; no universal bias constant|
|Accelerometer A|Six supported orientations; fit offsets/scales and, if justified, a cross-axis matrix; store mounting transform separately|Check gravity norm and direction in every pose; preserve gravity in physical data|
|Magnetometer B/A if used|Rotate the assembled payload; fit `m_cal=S(m_raw-b)`; test wheel/servo states|Validate heading and residual field with an independent reference; a bench-only fit is insufficient|
|Barometer pad A|Three sensors vented and stationary, away from hand heat; average at least5 s D; store p0, ambient T, variance and time|Check pad zero and interboard offsets; qualify600/950 m pressure ramps|
|INA260 A|I²C0x40; verify0xFE=0x5449 and0xFF device bits=0x2270 (mask low revision nibble). Reset by0x00←0x8000; poll reset completion with deadline. Write0x00←0x6527:16 averages,1.1 ms voltage/current conversions,continuous both; read back. Write0x06←0x0000 (no wired ALERT). Poll0x06 CVRF bit3; individually read0x01 signed current,0x02 unsigned voltage,0x03 unsigned power,MSB first. Scale1.25 mA,1.25 mV,10 mW per LSB.|Nominal conversion cycle35.2 ms D=16×(1.1+1.1),poll50 Hz,publish latest valid sample20 Hz with original timestamp; no FIFO. Check math-overflow bit2; bracket the register reads with status and discard/retry if another conversion completed mid-read. Finite bus timeout and shared recovery; identity/config/reference-load test rather than invented self-test. No calibration register; calibrate externally. Reset/startup deadline10 ms D to qualify.|
|GNSS A|Surveyed reference with known frame/quality; stationary and walking captures|Measure position distribution, epoch rate and dropouts; do not force output to the reference coordinate|
|Servo A|Measure pulse→angle→line travel/tension; identify signs, lid limits and current|No endstop stall; repeatable trim and adequate mechanical margin; store per-unit maps|
|Hall/ESC A|Guarded rig, known magnets, external tachometer, arm/stop/signal-loss tests|Correct RPM and deg/s; no automatic throttle calibration; rotor contained|

Calibration records contain board/sensor ID, version, units, coefficients, reference instrument/date, temperature, quality metrics and CRC. Copy them to PC/SD and the appropriate MCU store: qualified Teensy EEPROM/AT24C32 or P4 NVS. Keep instrument calibration, pad zero and attitude display datum distinct. Reject in-flight barometer zeroing because it would destroy the600 m reference.

### 11.2 Verification gates

**Tests actually run in this session are listed separately in [VERIFICATION.md](VERIFICATION.md).** The following hardware tests are proposed;none is marked passed without a team result.

| Test | Method and pass criteria |
|---|---|
|T-PINS|Power-off continuity against ICD; check logic voltages, pull-ups, backfeeding and CS idle states; scope all GNSS listeners at both bauds|
|T-BOOT|Cold/soft resets of each real board; no sweep/throttle; correct journal restore and RF silence; record P4 revision/options|
|T-SENSOR|Read identities/configuration, sample counters and physical units; run supported self-tests; inject disconnects and stuck devices; reject0xFF|
|T-GNSS|Record commands/ACKs/readbacks; measure1/5/10 Hz epochs, UART overflows, constellation trials, reboot and backup retention|
|T-NAV|Reject checksum errors, RMC V, q0/DR, malformed coordinates, NaN, stale GSA, mixed/duplicate epochs, future timestamps and replay/default origins|
|T-BARO|Pressure ramps at glide and drogue rates; physical opening must meet600±10 m including every error and delay|
|T-EVENT|Descending950 m crossing, ascent inhibit, reset below threshold and reset before/during/after10 s exposure; no extra reopening|
|T-INHIBIT|Lift, hover and attached-drone descent without release confirmation; no bay/petri/steering; test delayed/lost confirmation|
|T-FSM|Actual drone profile with transient pressure/shock; correct states0–7, launch clock, pad datum and reset recovery|
|T-LINK|Unplug MAIN, BACKUP and raw GNSS separately; inject CRC/sequence/one-way failures; verify authority and local P4 operation|
|T-TARGET|Partial, repeated, old-session and out-of-area updates; int32 bounds; atomic commit and correct P4 echo|
|T-RF|Capture actual RF CSV/CR/cadence, byte lengths and command slots; test TX ON/OFF, authorised channel switch, power/range and90-minute cutoff|
|T-RESET|Reset at each phase and journal/ACK boundary, including89:59; corrupt one/both slots; no cutoff override or unsafe rollback|
|T-LOG|Disconnect RF; fill/remove/slow SD; force flash timeout; control continues and records retain CRC/source/sample time|
|T-POWER|Actual combined-load endurance for2 h pad plus flight; scope rails at loads; test overshoot, wheel startup, connectors and retention|
|T-HALL/T-WHEEL|Tachometer/PPR agreement, stuck/noisy input, stop response, balance and rotor containment|
|T-TIMING|Capture sample/parse/planner/PWM under full load; verify §8.5 targets, progress watchdog and deadline margins|
|T-GCS/T-ISOLATE|Real serial→plots→judges CSV; missing data stays missing; simulation/replay cannot reach outputs or the flight file|
|T-GLIDE/T-GUIDE|Authorised repeated drops: inflation,1–3 m/s sink, brake sign/response, safe authority, wind observability and independently measured landing error|
|T-BEACON|Measure at least92 dB using the agreed setup, assembled enclosure and representative battery condition|

Ground walk/car tests qualify GNSS mapping, ages, waypoints and command flow with **actuators mechanically disconnected**. They do not identify parafoil brake response or wind penetration. HIL exercises the FSM/control in a separate bench executable. Compare expected and actual events, including failure cases.

For drops,log each command and its observed effect with sample/actuator timestamps;measure landing point with an independent reference and report radial error,crosswind/alongwind error,sink,wind,canopy identity,mass,target revision and all faults. Use several independent drops;report the actual count and empirical dispersion. Do not claim CEP95 from a few drops or tune and evaluate on the same single trajectory. A no-GNSS degradation test must prove stable canopy behavior,not precision landing without navigation.

## 12. Hardware changes kept outside the main design

No new pin,component or board is assumed in the design above. These are conditional changes if a strict requirement cannot be met. Costs below are **planning estimates (D/U),not supplier quotes**;availability in the three-day window is unknown.

| Priority / condition | Change | Estimated cost / effort / retest |
|---|---|---|
|A if beacon fails|Beacon meeting92 dB with proper drive/power|₹200–1500 estimate; hours; repeat acoustic and power tests|
|A if positive release verification is mandatory|Release interlock and wiring with debounce/failure analysis|₹50–500 estimate plus mounting; hours; revise ICD and repeat release tests|
|A if existing buck fails|Qualified steering supply/protection|₹300–2000 estimate; hours; load, thermal and overshoot tests|
|B|Servo-rail voltage/current measurement with3.3 V-compatible interface|₹100–1500 estimate; wire and calibrate; still may miss fast dips|
|B|Independent beacon/radio takeover path|₹500–5000+ estimate; days of integration; no immediate full-redundancy claim|
|C|Canopy heading, airspeed, rangefinder and physical deployment feedback|Part-dependent, not quoted; new identification and flight campaign|

Adjusting the already owned adjustable buck and removing stale software pin assignments are configuration tasks,not new hardware assumptions. Do not spend the remaining time adding an untested navigation sensor simply to make an estimator appear complete.

## 13. Priority list for the next three days

### A — must do before launch

**28 October:** freeze the pins, radio protocol and two-stage descent sequence. Identify the actual canopy, ESC, Hall sensor and buck. Set the steering rail to7.4–8.0 V. Run board bring-up, identity/readbacks and pulse checks. Resolve release confirmation, petri crossing direction and beacon compliance. Start endurance testing early.

**29 October:** implement the smallest multi-board port: live GNSS validity, local P4 events,600 m bay,950 m petri deadline, progress watchdogs, hard TX cutoff, truthful CSV and atomic target update. Flash matching MAIN/GS/GCS versions. Test pressure ramps, resets, link loss, invalid commands and SD failures before connecting packed mechanisms.

**30 October:** rehearse with the actual battery, camera, radio, servo loads and deployment mechanics. Use authorised drops to measure inflation, sink, steering sign and latency. If steering is unqualified, retain proven stable trim and report precision landing as undemonstrated. Close mandatory descent/beacon failures before clearance. Freeze the tested builds, hashes, cards and CSV schema.

### B — should do if time allows after A passes

Qualify tagged IMU FIFO, wind circle fitting, limited energy holding, turn-rate PI, better landing prediction, ambient temperature profiling, motor-on compass calibration and matching-protocol AGNSS assistance. Add them only when measured benefit justifies the complexity.

### C — future

Develop an observable augmented EKF/UKF, optimized terminal trajectories, airspeed/canopy heading measurements, range-assisted flare, independent backup telemetry, release feedback and a statistically supported landing campaign.

## 14. Launch-day checklist

- [ ] Record actual canopy/mass/rigging and secondary mechanism; confirm **drogue immediately, ram-air600±10 m**.
- [ ] Inspect fasteners,battery retention,connectors,antenna clearances and correct physical board revisions.
- [ ] Measure battery and steering/logic rails under a brief representative load;steering never exceeds8.4 V.
- [ ] Verify safe pulse positions with unloaded mechanisms,then pack/secure them;no automatic sweep on boot.
- [ ] Boot the exact frozen MAIN/BACKUP/P4/camera/GS/GCS versions;record build IDs and reset reasons.
- [ ] Confirm RF is silent before command and only authorised slot/channel/power will be used;camera Wi-Fi/BT remains off.
- [ ] Confirm L89 real coherent fix,actual update rate,quality/age and all three listener counters;no overflows.
- [ ] Capture pad p0/ambientT and stationary calibration;compare three barometers;lock the datum against in-flight changes.
- [ ] Verify DS3231 trustworthy time,mission epoch,event journal,new-mission status and non-resettable90-minute deadline.
- [ ] Set target26.717535N,84.301543E or the authorised adjustment;read **P4's applied target/revision** back.
- [ ] Confirm active release-confirmation procedure and its latency relative to950 m;do not confuse ARM with RELEASE.
- [ ] Confirm SD/flash capacity and write health;GCS judges'filename/header/plots/raw receive log ready.
- [ ] Start telemetry by command;verify actual RF CSV at≥1Hz and ACK channel;TX_OFF test completed earlier.
- [ ] Verify cameraREC acknowledgment and storage;beacon acoustic test and wheel/Hall guarded checks already passed.
- [ ] Arm mission only after readiness flags and physical mechanism checks;record operator actions.
- [ ] During lift/release:watch state,AGL,release latch,petri deadline,600 mbay event,GNSSage and supply/fault indicators.
- [ ] After landing: stop the wheel, activate the beacon, obtain recovery clearance, shut down RF and copy the received CSV to USB immediately.
- [ ] Preserve original logs and cards;record actual landing point,target revision,wind and anomalies before tuning anything.

## 15. Open questions only the team can close

1. Which **ram-air** flight article is packed:7-cell0.40 m² or11-cell0.71 m²? The clarification confirmed two parachutes,not the ram-air area/cell count.
2. Is950 m petri opening explicitly on **descent**,and how soon can release confirmation arrive? Does “above boot” mean the pad boot point even after a reset?
3. What is the measured bay-command-to-physical-opening latency,and is the drogue retained without fouling the ram-air canopy?
4. Are B26CLM the actual two servos,and what are each servo's safe pulse endpoints,line travel,working tension and loaded current?
5. What are the exact ESC,Hall sensor and adjustable buck models,ESC protocol/arm/stop behavior,magnet count and qualified rotor speed?
6. Is the GNSS J7 backup cell physically fitted and maintaining the correct voltage? Is any external antenna connected,changing the internal antenna selection?
7. Are there independent release/deployment sensors or rail measurements omitted from the current files? No such connections are assumed here.
8. Which hardware tests have actually run since the brief's “compiled,not run” status? Provide raw traces/results before marking any matrix row passed.
9. Has the organiser accepted the accelerometer CSV expansion, empty invalid fields and short command alias, and specified the92 dB measurement setup?
10. Can the team demonstrate stable1–3 m/s secondary descent and mechanical wheel stabilization with this flight article? Simulation and requested RPM do not close those requirements.
