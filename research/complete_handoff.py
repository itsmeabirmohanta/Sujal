"""One-time audit amendments and generated navigation. Does not modify supplied source."""
from pathlib import Path
import csv, json

out=Path('deliverables')
report=out/'VRITRA_ENGINEERING_REPORT.md'
s=report.read_text(encoding='utf-8')
s=s.replace('maximum decoded payload96 bytes','maximum payload64 bytes (version2; decoded frame90 bytes, COBS wire92 bytes including delimiter)')
s=s.replace('Each96-byte frame takes at most8.34 ms at115200 8N1 (D:96×10/115200). At20 Hz that is16.7% of one link.', 'The version2 maximum92-byte wire frame takes7.99 ms at115200 8N1 (D:92×10/115200), or16.0% occupancy at20 Hz. The electrical inventory reserves96 bytes/8.34 ms per frame, leaving framing margin. See [the byte-level ICD](ICD_WIRE_FORMATS.md) for the exact schema and version boundary.')
old='|INA260 A|Compare several voltage/current loads against a reference meter; record offset, gain and residuals|Verify polarity, sensed branches and thermal behavior; no INA226-style calibration register|'
new='|INA260 A|I²C0x40; verify0xFE=0x5449 and0xFF device bits=0x2270 (mask low revision nibble). Reset by0x00←0x8000; poll reset completion with deadline. Write0x00←0x6527:16 averages,1.1 ms voltage/current conversions,continuous both; read back. Write0x06←0x0000 (no wired ALERT). Poll0x06 CVRF bit3; individually read0x01 signed current,0x02 unsigned voltage,0x03 unsigned power,MSB first. Scale1.25 mA,1.25 mV,10 mW per LSB.|Nominal conversion cycle35.2 ms D=16×(1.1+1.1),poll50 Hz,publish latest valid sample20 Hz with original timestamp; no FIFO. Check math-overflow bit2; bracket the register reads with status and discard/retry if another conversion completed mid-read. Finite bus timeout and shared recovery; identity/config/reference-load test rather than invented self-test. No calibration register; calibrate externally. Reset/startup deadline10 ms D to qualify.|'
assert old in s or new in s
s=s.replace(old,new)
s=s.replace('keep the current AHRS', 'keep the current AHRS')
s=s.replace('The linked [reference code](reference_code/README.md) contains compiled guards,parser,controller and event examples.', 'The linked [reference code](reference_code/README.md) contains host-checked guards, parser, controller, events, frame/log codecs, journal selection, estimator, planner and scheduling examples. [Integration boundaries](IMPLEMENTATION_STATUS.md) distinguish these from unimplemented platform adapters.')
s=s.replace('Use one shared protocol/types library and three board applications. Keep the supplied repository read-only during this design handoff;apply the implementation in a versioned branch after the team reviews the exact interfaces.', 'Use one shared protocol/types library and three board applications. The supplied repository remains a provenance snapshot; the reference modules and integration contract live in this handoff. See [module status](IMPLEMENTATION_STATUS.md) before adopting any code. The research prompt requests architecture and key blocks, not proof of a flashed/flight-qualified image.')
report.write_text(s,encoding='utf-8')

rows=[]
def row(i,request,section,artifact,status='DELIVERED_DESIGN',remaining='Hardware qualification remains pending'):
    rows.append(dict(ID=i,Request=request,Report_section=section,Artifact=artifact,Status=status,Remaining=remaining))
parts=[('L89 HA','4','reference_code/gnss_commands.txt','Firmware-matched AGNSS/dynamic-mode documents and sky tests remain unavailable'),('BMP585','5.2','reference_code/drivers.hpp','Pressure ramp/port and actual noise tests'),('ICM-20948 / AK09916','5.3','VRITRA_ENGINEERING_REPORT.md','Installed firmware-independent self-test limits require full TDK document/driver validation'),('LSM6DSO32','5.4','reference_code/drivers.hpp','Board2 fault isolation and optional FIFO qualification'),('LR1121','6.1–6.2','calculations.json','Actual waveform length, duty cycle, RX guards and conducted RF tests'),('INA260','5.5','SENSOR_COMPLETENESS.md','Restored boot register/read sequence; reference load test'),('DS3231','5.5','SENSOR_COMPLETENESS.md','OSF/time trust and reset endurance'),('HDC3020','5.5','reference_code/drivers.hpp','Physical identity/CRC/reference humidity test'),('W25Q64','5.5','reference_code/logging.hpp','Identify actual suffix before maximum program/erase timing'),('XIAO camera','5.6','VRITRA_ENGINEERING_REPORT.md','Installed sensor, recording ACK/SD and Wi-Fi-off validation'),('Grove buzzer','5.6/9','VRITRA_ENGINEERING_REPORT.md','92 dB compliance measurement'),('ESP32-P4','7.1/5.6','reference_code/pins.hpp','Core lock, peripheral build and timing'),('Teensy4.1','7.1/5.6','reference_code/scheduler.hpp','Core lock, platform build and WCET'),('B26CLM','8.4','VRITRA_ENGINEERING_REPORT.md','Measured endpoints/current/holding/signal loss'),('MG90S','8.4','VRITRA_ENGINEERING_REPORT.md','Actual mechanism limits and current'),('ESC / EMAX / Hall','8.4','VRITRA_ENGINEERING_REPORT.md','Identify exact ESC/Hall; containment and tachometer tests')]
for i,(name,section,artifact,remaining) in enumerate(parts,1):row(f'1.{i:02}',name+' documents/configuration/read/recovery',section,artifact,'EVIDENCE_LIMITED' if name in ('L89 HA','ICM-20948 / AK09916','W25Q64','ESC / EMAX / Hall') else 'DELIVERED_DESIGN',remaining)
gnss=[('Hardware/protocol/SDK and compatible family evidence','4.1/4.4','SOURCE_REGISTER.md','EVIDENCE_LIMITED','No verified public AG3335M SDK or firmware-matched assistance transaction'),('Constellations/NavIC/L5/channel budget','4.1–4.3','reference_code/gnss_commands.txt','EVIDENCE_LIMITED','Exact per-constellation channel allocation unavailable; best India profile requires comparative test'),('Baud change and three listeners','4.2–4.3','ICD_WIRE_FORMATS.md','DELIVERED_DESIGN','ACK/save/reboot and resync require physical receiver'),('Rates/sentences/line budget','4.2–4.3','calculations.json','DELIVERED_DESIGN','Measure actual 1/5/10 Hz and GSV bursts'),('Dynamics/SBAS/mask/static hold/PPS/jam/geofence','4.4','SOURCE_REGISTER.md','EVIDENCE_LIMITED','Unsupported firmware-specific commands not invented; unwired pins documented'),('Save/restart/checksums/ACKs','4.2–4.4','reference_code/gnss_commands.txt','DELIVERED_DESIGN','Ground transaction only; installed firmware validation'),('TTFF/backup/EPO/RTC/real fix','4.4','reference_code/nmea_epoch.hpp','EVIDENCE_LIMITED','Matching AGNSS document and assistance file unavailable'),('Raw plus MAIN state sharing/age/checksums','3.3/4.4','ICD_WIRE_FORMATS.md','DELIVERED_DESIGN','Clock/age adapter and link fault tests'),('Antenna L1/L5','4.1','SOURCE_REGISTER.md','DELIVERED_DESIGN','Inspect any fitted external antenna')]
for i,r in enumerate(gnss,1):row(f'2.{i:02}',*r)
arch=[('Requirements traced to tests','2/11','requirements.csv'),('All links/commands/byte budgets/timeouts','3/10.3','ICD_WIRE_FORMATS.md'),('Per-board tasks/rates/priority/CPU/watchdog','7.1','reference_code/scheduler.hpp'),('Typed pipeline blocks at every stage','7.2','IMPLEMENTATION_STATUS.md'),('Attitude/position/velocity/wind equations/tuning','7.3–7.4','reference_code/estimators.hpp'),('State0–7/release/events/reset recovery','7.5–7.6','reference_code/fsm.hpp'),('Takeover/local P4/FMEA/safe defaults','7.6/9','IMPLEMENTATION_STATUS.md'),('Exact CSV/SD/flash/1Hz','6.2–6.4','reference_code/telemetry.hpp'),('Field calibration/storage','11.1','LAUNCH_DAY_CHECKLIST.md')]
for i,r in enumerate(arch,1):row(f'3.{i}',*r)
guidance=[('Parafoil papers/code comparison','7.4/8','SOURCE_REGISTER.md'),('Homing/energy/wind/final/GNSS loss','8.1–8.2','reference_code/planner.hpp'),('PID/sign/gains/tuning/limits/antiwindup','8.3','reference_code/control_pi.hpp'),('Loop/latency/jitter/timestamps','7.1/8.5','calculations.json'),('Servo/pulse/travel/power/interlocks','8.4/9','reference_code/flight_core.hpp'),('HIL/walk/drop accuracy campaign','11.2','TEST_EVIDENCE_TEMPLATE.csv')]
for i,r in enumerate(guidance,1):row(f'4.{i}',*r)
row('5.1','Per-board production-oriented structure/key modules/keep-change-remove','7.2/10.1','IMPLEMENTATION_STATUS.md',remaining='Reference blocks delivered; full board applications/builds are not claimed')
row('5.2','Library versions and GitHub links','10.2','SOURCE_REGISTER.md','EVIDENCE_LIMITED','Team core versions unverified; hash-lock adopted driver sources before launch')
row('6.1','Read existing GCS/gateway and specify mission changes','10.3–10.4','GCS_HANDOFF.md',remaining='GUI/firmware protocol migration remains an implementation/qualification task')
row('F.1','Numeric citations and explicitly unverified values','Throughout','numeric_traceability.csv',remaining='D entries are proposals; U entries remain unknown')
row('F.2','Finish with A/B/C plan, launch checklist, team questions','13–15','ACTION_PLAN.md',remaining='Team supplies named owners and closes acceptance evidence')
with (out/'PROMPT_COVERAGE.csv').open('w',newline='',encoding='utf-8') as f:
    w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)

# Short operational extracts stay tied to the canonical report; do not hand-edit duplicates.
for filename,start,end,title in [('ACTION_PLAN.md','## 13.','## 14.','Three-day action plan'),('LAUNCH_DAY_CHECKLIST.md','## 14.','## 15.','Launch-day checklist'),('TEAM_OPEN_QUESTIONS.md','## 15.',None,'Open questions and evidence required')]:
    chunk=s[s.index(start):];chunk=chunk[:chunk.index(end)] if end else chunk
    (out/filename).write_text(f'# {title}\n\nGenerated from the [canonical engineering report](VRITRA_ENGINEERING_REPORT.md). All hardware results remain pending.\n\n'+chunk,encoding='utf-8')
print(f'Wrote {len(rows)} individually mapped prompt deliverables and operational extracts.')

tests=[]
for line in s.splitlines():
    if line.startswith('|T-'):
        test,criteria=[x.strip() for x in line.strip('|').split('|')][:2]
        tests.append([test,criteria,'PENDING','','','','','',''])
with (out/'TEST_EVIDENCE_TEMPLATE.csv').open('w',newline='',encoding='utf-8') as f:
    w=csv.writer(f);w.writerow(['Test_ID','Method_and_acceptance','Status','Named_owner','Build_and_calibration_IDs','Date_UTC','Observed_result','Evidence_path_or_URL','Reviewer']);w.writerows(tests)

trace=[
('N01','Fixed pin/address assignments','report3.1; pins.hpp','H','Current brief3-4 and current board BUILD_LIST/KiCad','../source/VRITRA_CanSat_025_Handover_2026-10-28/01_PROJECT_BRIEF_AND_PARTS.md','T-PINS'),
('N02','866.50/865.25 MHz; SF9; sync0x99','report6.1','R','Official assignment PDF page2 row18; visual recheck','../source/VRITRA_CanSat_025_Handover_2026-10-28/02_competition_documents/CAN7USAT_Team_Frequency_Assignments_19-10-2026_(OFFICIAL_for_team_025).pdf','T-RF'),
('N03','600±10 m;1Hz;state0-7;92dB','report2;7.5','R','Guidelines5.2/5.4/6.2/6.3; state names are examples','../source/VRITRA_CanSat_025_Handover_2026-10-28/02_competition_documents/IN-SPACe_CAN-7USAT_Mission_Requirements_and_Guidelines_IN-SPACe.CAN.MR.CG.01.pdf','T-DEPLOY/T-RF/T-BEACON'),
('N04','950 m;10s;target26.717535/84.301543;0.40m2','report1;8','H','Current brief; canopy geometry still disputed','../source/VRITRA_CanSat_025_Handover_2026-10-28/01_PROJECT_BRIEF_AND_PARTS.md','T-EVENT/T-GLIDE'),
('N05','PAIR IDs/fields/checksums;1/5/10Hz;115200','report4;gnss_commands.txt','V/D','Quectel protocol1.0 sections2.4; chosen profile D; XOR checked','https://quectel.com/content/uploads/2024/02/Quectel_L89_R2.0_GNSS_Protocol_Specification_V1.0.pdf','T-GNSS'),
('N06','L1/L5;RESET100ms;V_BCKP2.2-4.3V;TTFF28/23/1s','report4.1/4.4','V','Quectel hardware1.2 Table2;3.2.2;5.2.2; antenna section','https://quectel.com/content/uploads/2024/02/Quectel_L89_R2.0_Hardware-Design_V1.2.pdf','T-GNSS'),
('N07','BMP register writes;50Hz;P16/T2;0.21Pa','report5.2;drivers.hpp','V/D','Bosch DS003 4.3-4.6;8.27-8.36;Tables7/9; profile selection D','https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp585-ds003.pdf','T-SENSOR/T-BARO'),
('N08','ICM banks/scales/DLPF;16g/2000dps;AK100Hz','report5.3','V/D','TDK DS-000189 register map; partial local source evidence; selected profile D','https://product.tdk.com/system/files/dam/doc/product/sensor/mortion-inertial/imu/data_sheet/ds-000189-icm-20948-v1.5.pdf','T-SENSOR'),
('N09','LSM208Hz;16g;2000dps;timestamp25us;registers','report5.4;drivers.hpp','V/D','ST datasheet/register driver; selected profile D','https://github.com/STMicroelectronics/lsm6dso32-pid','T-SENSOR'),
('N10','LSM self-test52Hz/4g;50-1700mg;150000-700000mdps','report5.4','V','Official ST self-test example, complete test setup required','https://github.com/STMicroelectronics/STMems_Standard_C_drivers/blob/master/lsm6dso32_STdC/examples/lsm6dso32_self_test.c','T-SENSOR'),
('N11','INA0x6527;16avg;1.1ms;1.25mA/mV;10mW;35.2ms','report5.5','V/D','TI8.6 register map;35.2ms derived16*(1.1+1.1); profile D','https://www.ti.com/lit/ds/symlink/ina260.pdf','T-SENSOR/T-POWER'),
('N12','RTC control0x0E/status0x0F;OSF','report5.5','V','ADI DS3231 register description','https://www.analog.com/media/en/technical-documentation/data-sheets/ds3231.pdf','T-RESET'),
('N13','HDC commands/CRC0x31;T/RH conversion','report5.5;drivers.hpp','V/D','TI Tables7-1/7-4/7-5; startup budget D','https://www.ti.com/lit/ds/symlink/hdc3020.pdf','T-SENSOR'),
('N14','Flash256-byte pages/4KiB sectors/opcodes','report5.5','V/U','W25Q64JV instruction tables; fitted suffix and maximum timing U','https://www.winbond.com/resource-files/w25q64jv%20spi%20%20%20revc%2006032016%20kms.pdf','T-LOG'),
('N15','TCXO1.6V;DIO5/6;begin signature7.7.1','report6.1','H/V','Supplied radio source and RadioLib7.7.1 API','https://github.com/jgromes/RadioLib/blob/7.7.1/src/modules/LR11x0/LR1120.h','T-RF'),
('N16','SF9 airtime;130+32bytes+40ms=984.128ms','calculations.json','D','LoRa symbol formula with explicit header/CRC/preamble8; guards D','reference_code/engineering_checks.py','T-RF'),
('N17','Payload64/decoded90/wire92;CRC32;UART7.99ms','ICD_WIRE_FORMATS.md','D','Explicit22+64+4 schema;COBS+2;92*10/115200','reference_code/protocol.hpp','T-LINK'),
('N18','L0.0065;Rd287.05;g9.80665;pressure height model','report5.2','D/model','US Standard Atmosphere1976; measured T0 applied by design','https://ntrs.nasa.gov/api/citations/19770009539/downloads/19770009539.pdf','T-BARO'),
('N19','Noise/flow pressure/spool/capacitor/wheel calculations','calculations.json','D','Declared formulas and illustrative inputs; not measured performance','reference_code/engineering_checks.py','T-BARO/T-POWER/T-HALL'),
('N20','Freshness500/2500ms;sats6;HDOP2.5;reacquire5','report4.4','D','Conservative proposed gates; tune from raw observations','reference_code/flight_core.hpp','T-NAV'),
('N21','Task rates/priorities/WCET/RAM/CPU margins','report7.1;8.5','D','Scheduling allocations; not platform benchmarks','VRITRA_ENGINEERING_REPORT.md','T-TIMING'),
('N22','Lift10m/0.5mps/2s;descent0.5s;land5m/0.3mps/10s','report7.5','D','Proposed dwell thresholds; qualification required','reference_code/fsm.hpp','T-FSM'),
('N23','Mahonykp0.5/ki0;noise1/5/0.5/2;wind0.05','report7.3-7.4','D','Starting filter tuning, not measured sensor errors','VRITRA_ENGINEERING_REPORT.md','T-SENSOR/T-GLIDE'),
('N24','Wind12samples/120degrees;residual/age caps','report7.4','D','Observability heuristic; circle fit from velocity vectors','reference_code/estimators.hpp','T-GLIDE'),
('N25','Heading/PID/height/radius/slew examples','report8.2-8.3','D','Caller-qualified limits; no flight-qualified gain set','reference_code/planner.hpp','T-GUIDE'),
('N26','B26CLM4.8-8.4V;1520us;333Hz;torque/speed','report8.4','V/U','Manufacturer advertised values; actual current/endpoints/loss behavior U','https://www.agf-rc.com/agfrc-coreless-mini-servo-b26clm-p1669135.html','T-POWER/T-GUIDE'),
('N27','MG90S1.8kgcm/4.8V;qualified5V use','report8.4','V/D','Manufacturer listing; installed unit/pulse/current testing required','https://towerpro.com.tw/product/mg90s-3/','T-EVENT/T-POWER'),
('N28','ESC1000us;motor2300KV;wheel10000RPM','report8.4','H/U','Brief convention/target only; not validated ESC/rotor safety limits','../source/VRITRA_CanSat_025_Handover_2026-10-28/01_PROJECT_BRIEF_AND_PARTS.md','T-HALL/T-WHEEL'),
('N29','Grove85dB versus required92dB','report1.1','V/R','Seeed rating and guideline5.2; measurement setup pending','https://wiki.seeedstudio.com/Grove-Buzzer/','T-BEACON'),
('N30','MCU cores/I2C400kHz/PWM50Hz14bit','report3/5.6','V/D','SDK/core references plus chosen PWM/I2C profile; actual chip/core lock pending','https://docs.espressif.com/projects/arduino-esp32/en/latest/api/ledc.html','T-PINS/T-TIMING'),
('N31','Command IDs/frame sizes/journal/log byte offsets','ICD_WIRE_FORMATS.md','D/H','Legacy IDs from supplied code; new IDs and schemas D','reference_code/journal.hpp','T-TARGET/T-RESET/T-LOG'),
('N32','Hardware-change rupee costs and effort','report12','D/U','Planning estimates only, no vendor quote or availability verification','VRITRA_ENGINEERING_REPORT.md','Team procurement'),
]
with (out/'numeric_traceability.csv').open('w',newline='',encoding='utf-8') as f:
    w=csv.writer(f);w.writerow(['ID','Values_or_family','Location','Evidence_class','Source_section_or_derivation','Link','Acceptance']);w.writerows(trace)
