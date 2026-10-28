# Historical one-off transformation: unsafe to rerun on the audited report.
# Retained for provenance only; use run_checks.py for supported regeneration.
raise SystemExit('Historical script retired. Use python research/run_checks.py instead.')
from pathlib import Path
p=Path('deliverables/VRITRA_ENGINEERING_REPORT.md')
t=p.read_text(encoding='utf-8')
replacements={
'|`firmware/vritra_fc/Config.h`':'|`firmware/vritra_fc/Config.h` pins/radio/states|Keep verified mission and radio constants|Replace old Serial1 GNSS, BMP INT pin0, TB6612 and I²S pins; set flash CS9; update states0–4 to0–7; remove absent hardware fields|',
'|`Sensors.cpp`I²C':'|`Sensors.cpp` I²C/barometer/ICM|Keep bounded recovery and checked conversions|Use MAIN Serial2; coherent GNSS epochs and RMC validity; remove blind PMTK fallback and stale-position refresh|',
'|`Sensors.cpp:nmeaRMC`':'|`Sensors.cpp:nmeaRMC` around1749|Keep date and knots conversion|Check RMC status A, numeric grammar, course, epoch and age before updating velocity|',
'|`Sensors.cpp:nmeaGGA`':'|`Sensors.cpp:nmeaGGA` around1704|Keep checksum front end|Validate coordinates, hemispheres, minutes and fix quality; exclude predictions from GNSS fields; never convert empty input into position0|',
'|`Radio.cpp`':'|`Radio.cpp` around200–430|Keep SPI1 MISO39, TCXO, switch table and async service|Use decimal CSV; remove autonomous backup-channel switching; check all configuration results and timeouts|',
'|`vritra_fc.ino:execCmd`':'|`vritra_fc.ino:execCmd` around429–485|Keep TX_OFF independent of disarming|Remove cutoff override; restrict destructive/calibration commands in flight; forward wheel commands to P4 with phase checks|',
'|`vritra_fc.ino:buildPacket`':'|`vritra_fc.ino:buildPacket` and `Base36.h`|Keep only for archived decoding|Use decimal core CSV in the launch path unless an exception is approved; update all endpoints together|',
'|`Motor.cpp/.h`':'|`Motor.cpp/.h`|Old TB6612 implementation is obsolete|Replace with qualified P4 ESC/Hall control; MAIN must not drive old motor pins|',
'|`Beacon.cpp/.h`':'|`Beacon.cpp/.h`|Keep portable timing logic|Remove I²S/MAX98357A; use active buzzer on GPIO2|',
'|`DataLogger.cpp`':'|`DataLogger.cpp`|Keep qualified queues and versioned records|Remap flash bus/CS; inspect blocking calls; disable unverified PMTK assistance; log raw samples and validity|',
'|`board_tests/*`':'|`board_tests/*`|Keep pin maps and isolated bring-up methods|Ground tests only; remove interactive sweeps from flight builds; verify actual hardware/core versions|',
'|`vritra_gs.ino`':'|`vritra_gs.ino` command table around100–170|Keep reserved IDs and radio hardware map|Replace int16 coordinate arguments; remove repeated toggle bursts and sticky-field reconstruction; pass decimal CSV transparently|',
'|`gcs/vritra_gcs.py`':'|`gcs/vritra_gcs.py` SCHEMA~250, parser~490, CSV writer~1920|Keep SI plots, presence flags, port detection and filename|Update states, INA260 labels, time encoding and airtime; isolate simulation classes from the flight entrypoint|',
'|`vritra_cam.ino`':'|`vritra_cam.ino`|Keep UART commands and local SD recording|Verify PSRAM, camera identity, storage errors and REC ACK; never assume recording succeeded|',
'|Teensyduino|':'|Teensyduino|Team records1.62; [PJRC](https://www.pjrc.com/teensy/td_download.html)|Freeze tested compiler/core and bundled SD/SPI/Wire; no platform build was reproduced here|',
'|Arduino-ESP32|':'|Arduino-ESP32|Required3.3.x; team records3.3.11 for camera; [releases](https://github.com/espressif/arduino-esp32/releases)|Lock actual tested P4/S3 versions and chip revision; avoid a launch-week upgrade|',
'|RadioLib|':'|RadioLib|Verified7.7.1 [tag](https://github.com/jgromes/RadioLib/tree/7.7.1)|Freeze the tested blob; the positional begin overload remains available; verify every result|',
'|BMP5 SensorAPI|':'|BMP5 SensorAPI|Downloaded header reports1.5.0; [Bosch](https://github.com/boschsensortec/BMP5_SensorAPI)|Use as configuration reference; pin the actual commit/hash if adopted|',
'|LSM6DSO32 PID|':'|LSM6DSO32 PID|[ST](https://github.com/STMicroelectronics/lsm6dso32-pid); no semver asserted|Pin reviewed source; use official register and self-test helpers|',
'|ICM-20948|':'|ICM-20948|[SparkFun](https://github.com/sparkfun/SparkFun_ICM-20948_ArduinoLibrary); release unselected|Community reference; keep a qualified direct-register driver if that reduces change|',
'|INA260/HDC302x|':'|INA260/HDC302x|[Adafruit INA260](https://github.com/adafruit/Adafruit_INA260), [HDC302x](https://github.com/adafruit/Adafruit_HDC302x)|Reference implementations; TI documentation controls commands and conversions|',
'|AHRS|':'|AHRS|Existing implementation or [x-io Fusion](https://github.com/xioTechnologies/Fusion)|A: keep qualified code. B: compare Fusion offline and pin if adopted|',
'|SemtechSDK|':'|Semtech SDK|[SWDR001](https://github.com/Lora-net/SWDR001), [SWSD003](https://github.com/Lora-net/SWSD003)|Reference material; do not add a second radio stack before launch|',
'|Gyro A|':'|Gyro A|Stationary, thermally settled samples; store mean bias in rad/s, variance, temperature and duration|Reject motion; repeat with wheel off/on; no universal bias constant|',
'|Accelerometer A|':'|Accelerometer A|Six supported orientations; fit offsets/scales and, if justified, a cross-axis matrix; store mounting transform separately|Check gravity norm and direction in every pose; preserve gravity in physical data|',
'|Magnetometer B/A':'|Magnetometer B/A if used|Rotate the assembled payload; fit `m_cal=S(m_raw-b)`; test wheel/servo states|Validate heading and residual field with an independent reference; a bench-only fit is insufficient|',
'|Baropad A|':'|Barometer pad A|Three sensors vented and stationary, away from hand heat; average at least5 s D; store p0, ambient T, variance and time|Check pad zero and interboard offsets; qualify600/950 m pressure ramps|',
'|INA260 A|':'|INA260 A|Compare several voltage/current loads against a reference meter; record offset, gain and residuals|Verify polarity, sensed branches and thermal behavior; no INA226-style calibration register|',
'|GNSS A|':'|GNSS A|Surveyed reference with known frame/quality; stationary and walking captures|Measure position distribution, epoch rate and dropouts; do not force output to the reference coordinate|',
'|Servo A|':'|Servo A|Measure pulse→angle→line travel/tension; identify signs, lid limits and current|No endstop stall; repeatable trim and adequate mechanical margin; store per-unit maps|',
'|Hall/ESC A|':'|Hall/ESC A|Guarded rig, known magnets, external tachometer, arm/stop/signal-loss tests|Correct RPM and deg/s; no automatic throttle calibration; rotor contained|',
'|T-PINS|':'|T-PINS|Power-off continuity against ICD; check logic voltages, pull-ups, backfeeding and CS idle states; scope all GNSS listeners at both bauds|',
'|T-BOOT|':'|T-BOOT|Cold/soft resets of each real board; no sweep/throttle; correct journal restore and RF silence; record P4 revision/options|',
'|T-SENSOR|':'|T-SENSOR|Read identities/configuration, sample counters and physical units; run supported self-tests; inject disconnects and stuck devices; reject0xFF|',
'|T-GNSS|':'|T-GNSS|Record commands/ACKs/readbacks; measure1/5/10 Hz epochs, UART overflows, constellation trials, reboot and backup retention|',
'|T-NAV|':'|T-NAV|Reject checksum errors, RMC V, q0/DR, malformed coordinates, NaN, stale GSA, mixed/duplicate epochs, future timestamps and replay/default origins|',
'|T-BARO|':'|T-BARO|Pressure ramps at glide and drogue rates; physical opening must meet600±10 m including every error and delay|',
'|T-EVENT|':'|T-EVENT|Descending950 m crossing, ascent inhibit, reset below threshold and reset before/during/after10 s exposure; no extra reopening|',
'|T-INHIBIT|':'|T-INHIBIT|Lift, hover and attached-drone descent without release confirmation; no bay/petri/steering; test delayed/lost confirmation|',
'|T-FSM|':'|T-FSM|Actual drone profile with transient pressure/shock; correct states0–7, launch clock, pad datum and reset recovery|',
'|T-LINK|':'|T-LINK|Unplug MAIN, BACKUP and raw GNSS separately; inject CRC/sequence/one-way failures; verify authority and local P4 operation|',
'|T-TARGET|':'|T-TARGET|Partial, repeated, old-session and out-of-area updates; int32 bounds; atomic commit and correct P4 echo|',
'|T-RF|':'|T-RF|Capture actual RF CSV/CR/cadence, byte lengths and command slots; test TX ON/OFF, authorised channel switch, power/range and90-minute cutoff|',
'|T-RESET|':'|T-RESET|Reset at each phase and journal/ACK boundary, including89:59; corrupt one/both slots; no cutoff override or unsafe rollback|',
'|T-LOG|':'|T-LOG|Disconnect RF; fill/remove/slow SD; force flash timeout; control continues and records retain CRC/source/sample time|',
'|T-POWER|':'|T-POWER|Actual combined-load endurance for2 h pad plus flight; scope rails at loads; test overshoot, wheel startup, connectors and retention|',
'|T-HALL/T-WHEEL|':'|T-HALL/T-WHEEL|Tachometer/PPR agreement, stuck/noisy input, stop response, balance and rotor containment|',
'|T-TIMING|':'|T-TIMING|Capture sample/parse/planner/PWM under full load; verify §8.5 targets, progress watchdog and deadline margins|',
'|T-GCS/T-ISOLATE|':'|T-GCS/T-ISOLATE|Real serial→plots→judges CSV; missing data stays missing; simulation/replay cannot reach outputs or the flight file|',
'|T-GLIDE/T-GUIDE|':'|T-GLIDE/T-GUIDE|Authorised repeated drops: inflation,1–3 m/s sink, brake sign/response, safe authority, wind observability and independently measured landing error|',
'|T-BEACON|':'|T-BEACON|Measure at least92 dB using the agreed setup, assembled enclosure and representative battery condition|',
'|A ifbeaconfails|':'|A if beacon fails|Beacon meeting92 dB with proper drive/power|₹200–1500 estimate; hours; repeat acoustic and power tests|',
'|A ifabsoluteautonomousreleaseproofrequired|':'|A if positive release verification is mandatory|Release interlock and wiring with debounce/failure analysis|₹50–500 estimate plus mounting; hours; revise ICD and repeat release tests|',
'|A ifexistingbuckcannotholdsafevoltage|':'|A if existing buck fails|Qualified steering supply/protection|₹300–2000 estimate; hours; load, thermal and overshoot tests|',
'|B|Servorail':'|B|Servo-rail voltage/current measurement with3.3 V-compatible interface|₹100–1500 estimate; wire and calibrate; still may miss fast dips|',
'|B|Independentbuzzer':'|B|Independent beacon/radio takeover path|₹500–5000+ estimate; days of integration; no immediate full-redundancy claim|',
'|C|Canopy-heading':'|C|Canopy heading, airspeed, rangefinder and physical deployment feedback|Part-dependent, not quoted; new identification and flight campaign|',
}
lines=[]
for line in t.splitlines():
    for prefix,new in replacements.items():
        if line.startswith(prefix):line=new;break
    lines.append(line)
t='\n'.join(lines)+'\n'
# Correct common prose joins without touching source identifiers or paths.
for a,b in {
    'targetrevision':'target revision','landingpoint':'landing point','secondarydescent':'secondary descent',
    'adjustablebuck':'adjustable buck','qualifiedworking':'qualified working','allaxes':'all axes',
    'oldbegin':'old begin','camera-recordingsuccessflag':'camera recording success flag',
}.items():t=t.replace(a,b)
p.write_text(t,encoding='utf-8')
print('Rewrote compact prose/tables for readability.')
