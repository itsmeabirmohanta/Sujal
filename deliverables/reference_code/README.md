# Reference code: integration and qualification boundary

These are engineering examples for the architecture in the report. They are **not complete MAIN/BACKUP/P4 firmware**, have not been compiled with Teensyduino or Arduino-ESP32 here, and have not operated physical hardware.

The [module status table](../IMPLEMENTATION_STATUS.md) describes every header, implemented behavior and remaining platform adapter. The [wire ICD](../ICD_WIRE_FORMATS.md) controls version2 frames. New modules include `protocol.hpp`, `journal.hpp`, `logging.hpp`, `drivers.hpp`, `estimators.hpp`, `planner.hpp`, `control_pi.hpp`, `fsm.hpp`, `scheduler.hpp`, `pins.hpp`, `telemetry.hpp` and `gcs_protocol.py`. All C++ headers are included in the host module tests.

- `flight_core.hpp`: checked types, current-live navigation gate, atomic target staging, conservative course P controller, calibrated pulse interpolation, event/deadline helpers and CRC.
- `nmea_epoch.hpp`: bounded checksummed GGA/RMC/GSA parser, coherent epochs, validity, duplicate suppression and correct speed/course conversion. Its input in flight must be a physical L89 UART. It cannot authenticate satellite signals.
- `test_core.cpp`: **offline-only** synthetic tests. It is never linked into a flight binary. No test coordinate is a flight navigation default.
- `engineering_checks.py`: reproducible airtime, pressure/noise, timing/capacity calculations and GNSS command checksums. It does not open serial ports or send commands.
- `gnss_commands.txt`: generated transaction checklist; send one command at a time with ACK/readback handling, never blindly stream it.

Run all offline checks from the repository root (Python dependencies are in `research/requirements.txt`):

```powershell
python research/run_checks.py
```

Important boundaries:

1. Frame/log/journal codecs, wind fitting, planner and scheduling helpers are implemented and host-checked. Physical drivers, clock synchronization, session/lease handling, actual persistent writes, complete safe-area planner constraints, SD/flash writers, PWM adapters and GCS UI still require integration. Generic framing is not a complete typed command receiver.
2. GNSS quality gates are starting engineering thresholds. The example parser bounds the GGA/RMC pairing interval to500 ms;the final gate controls expiry. It has no RTC/GNSS clock-discipline or spoof detection. Leap second60 is rejected until a time policy is implemented.
3. UTC/date keys order epochs;they are not Unix timestamps. A backward receiver date after reset requires explicit disarmed recovery rather than silently clearing replay protection in flight.
4. Events require a seen crossing in the normal path. Reset below a threshold needs the separate recovery rules in the report;the example does not infer an unobserved deployment.
5. State mutation precedes returned actuator actions. Persist/reconcile intent at the board layer;the helper cannot atomically commit flash and move a servo. No exact-once physical-action claim is made.
6. Controller output is normalized differential brake. The board must apply the measured sign,common trim,pulse table,final pulse slew and electrical/mission permits. An invalid command returns a request for safe trim;the hardware adapter must perform the tested transition.
7. Target transactions require external mission/session/sequence/area checks. A CRC and a short team alias are not authentication.
8. Controller gains,mechanism latency,pulse endpoints and safe target area have no flight-qualified defaults. Callers must provide measured/qualified values.
