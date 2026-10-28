# Verification actually performed

Review date: 28 October 2026, Asia/Calcutta. These are host checks, not physical acceptance results. Rerun `python research/run_checks.py` from the repository root; machine-readable command/output records are in `research/check_results.json`.

## Passed

- `test_core.cpp`: 41 grouped assertions covering live GNSS origin/coherence/quality/expiry, invalid/replayed epochs, target commit, brake limits, event deadlines and irreversible radio cutoff.
- `test_modules.cpp`: all reference headers compiled with C++17 and `-Wall -Wextra -Werror -pedantic`; runtime checks cover frame corruption/overflow/recovery, journal corruption/uncommitted writes/selection, queue saturation, log CRC, same-epoch position plus velocity, covariance stability, stale wind windows, planner inhibition, PI limits, flight-state dwells/restore, stale CSV blanks, driver CRC/final settle delay and progress watchdog.
- `test_gcs_protocol.py`: four test cases cover physical-live transport labels, invalid values, marker expiry, blank CSV preservation and target transactions within the32-byte uplink cap.
- `engineering_checks.py`: independently checked NMEA checksum examples, LoRa payload boundaries and illustrative CSV length.130-byte downlink plus32-byte uplink plus40 ms guards totals984.128 ms under the declared SF9 assumptions; this is calculated, not measured.
- Coverage and package validation checks include45 prompt rows,20 traced mission requirements,32 numeric evidence families, report sections, local Markdown/HTML links, hashes and ZIP integrity. The validator runs after packaging and exits on failure; results are in `research/deliverable_validation.json`.
- Publication audit separately verified all812 supplied source files against their original SHA256 manifest and checked original ZIP integrity. A limited recognizable-credential-pattern scan found no hits; scope/limitations are recorded in `research/publication_audit.json`.
- This audit visually rechecked the supplied frequency PDF page2 row18 (team025:866.50/865.25 MHz,SF9,0x99) and guidelines page17 (state examples and telemetry order). Existing extracted page images remain available for review.

## Not performed

- Complete Teensyduino/Arduino-ESP32/ESP-IDF application builds, firmware flashing or GCS GUI integration.
- Physical sensors, sky reception, RF cadence/range/power, servo/ESC, acoustic, endurance, deployment or drop tests.
- Measured loop jitter, GNSS-to-PWM latency, current/pulse endpoints, tuned PID gains or achieved landing accuracy.
- Independent full rebuild/inspection of historical Gerbers/STEP/CFD/FEA or every chat statement. Source coverage limits remain in SOURCE_REGISTER.md.

Reference codecs and algorithms are implemented; device adapters, typed payload dispatch, clock synchronization, storage writes, complete planner constraints and board applications remain integration work listed in IMPLEMENTATION_STATUS.md. All hardware acceptance rows remain PENDING until actual evidence and reviewer signoff are supplied.
