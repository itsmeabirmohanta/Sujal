# What the code actually implements

The original prompt asks for architecture, per-board code structure and key blocks. Those are delivered in the report and reference modules. They are not complete platform firmware or a migrated GUI. Hardware validation, achieved landing accuracy and inaccessible vendor documentation cannot be completed by inventing results.

| Module | Delivered behavior | Remaining platform work |
|---|---|---|
| `pins.hpp` | Fixed MAIN/BACKUP/P4 constants | Continuity and platform pin ownership checks |
| `flight_core.hpp`, `nmea_epoch.hpp` | Coherent checked GNSS epochs, validity/age/provenance, atomic target staging, bounded direct-course P, pulses, event crossings, TX cutoff | Physical-only UART adapter, five-epoch reacquisition, area/innovation gates, session checks, reset reconciliation |
| `protocol.hpp` | Bounded COBS/CRC frame encode/decode, overflow resync | Typed payload codecs from ICD, sequence/session checks, clock mapping, leases |
| `drivers.hpp` | BMP/LSM boot-write tables, bounded stepping interface, BMP/HDC conversion/CRC helpers | HAL timeouts, identity/readbacks/status/settling, full ICM/INA/RTC/radio/camera drivers from report blocks |
| `scheduler.hpp` | No catch-up job scheduler and progress mask | Actual task priorities/timers/watchdog peripheral and WCET measurement |
| `estimators.hpp` | Scalar Joseph-form position/velocity filter; geometry/residual-gated wind circle | Attitude adapter, calibrated sensor-to-body transform, covariance/noise qualification, window management |
| `planner.hpp` | Measured origin, live-only homing, modeled reachability, circular energy phase, downwind approach entry, final latch, no flare | Approved-area/geofence/path collision check, reachable safe-point selection, turn limits, reacquisition gate, tested canopy parameters |
| `control_pi.hpp` | PI with applied-command antiwindup, saturation and slew | Measured inverse steering map, qualified rate measurement and gains, actuator adapter |
| `fsm.hpp` | States0–7, lift/descent/landing dwell, explicit release, restore consistency | Ground TEST mode UI, persistence/time/reboot recovery, physical release evidence |
| `journal.hpp` | Explicit bytes/CRC/commit marker, dual-slot selection and ambiguity rejection | NVS/EEPROM/NOR storage, power-loss timing and wear testing, mission clock restore |
| `logging.hpp` | SPSC bounded queue, CRC log codec | SD/flash writer, preallocation, drop metrics, export |
| `telemetry.hpp` | Guideline-order decimal core, blanks for stale/invalid fields, CR, packet byte count | RF scheduler, overflow policy, judge schema approval |
| `gcs_protocol.py` | Strict live-labelled parser, current marker expiry, CSV writer and target commands | Serial adapter, gateway migration, GUI/map/plots, ACK/revision display |

All reference headers compile in the host test suite. This verifies C++ logic and selected failure cases, not peripheral APIs or flight integration. String/enum provenance is a guard within trusted software, not a way to detect someone feeding prerecorded NMEA into a physical UART. Do not claim hardware-authenticated position or spoof resistance.

## Board assembly contracts

MAIN owns GNSS configuration, radio, camera, mission clock, BMP/ICM/INA/RTC, heartbeat and flash. BACKUP owns its BMP/environment sensors, independent raw GNSS parser, SD log and backup support. P4 owns local BMP/LSM, raw GNSS, all servo/ESC/Hall outputs, event latches and guidance. Board3 has no software. Camera and ground gateway keep their physical wiring and receive coordinated protocol changes.

```cpp
// Per-board wiring sketch; HAL methods below are contracts, not compiled adapters.
// MAIN loop: consume physical bytes -> coherent epoch -> immutable snapshot.
gnss.drainPhysicalUart(Serial2);  // fixed RX7/TX8; never open test input in flight
sensorOwner.step(now); radio.serviceIrqAndDeadline(now);
mission.step(now); link.publishOriginalSampleAge(snapshot);
heartbeat.updateOnlyIfCriticalTasksProgressed(now); logger.tryDrainBudget(now);
// BACKUP: RX0 is input only. SD cannot block its UART drain without detection.
backupGnss.drainPhysicalUart(Serial1); backupSensors.step(now);
backupLease.observeHeartbeatAndMainStatus(now); sd.tryDrainBudget(now);
// P4: one actuator owner; final permit checks after every guidance calculation.
localGnss.drainPhysicalUart(UART1); eventSafety.step(now);
auto desired = planner.update(liveNav, activeTarget, now, gates, permit,
                              agl, sink, baroValid, wind, qualifiedPlanner);
if (!desired.valid || !areaGuard.accepts(desired) || !reacquisition.ready())
    actuatorOwner.returnToMeasuredTrim();
else actuatorOwner.applyBoundedCourse(desired.course_rad, liveNav, permit);
```

`CourseController` directly homes to a target and does not consume planner course demands. To integrate energy/final phases, implement a desired-course adapter using report §8.3; do not compute a planner demand and then ignore it by calling direct homing. `RatePi` is optional until a measured canopy turn-rate signal is qualified. The reference planner's reachability flag is advisory and it has no safe-area polygon; an external path guard is mandatory before enabling it on hardware.

## Integration order and acceptance

1. Freeze board/core/library identities; compile a separate MAIN/BACKUP/P4 application with fixed pin constants and grounded actuator inhibits.
2. Implement bounded device/HAL readback and timeout state machines. Qualify samples and clock monotonicity before fusion.
3. Implement typed version2 payloads, leases, journals and reset recovery. Corrupt/disconnect each source and test original sample age preservation.
4. Migrate MAIN radio, gateway and GCS together; verify real RF core bytes/cadence and target commit/ACK across all endpoints.
5. Qualify passive descent/events before enabling any steering. Connect calibrated pulse maps only after measured sign, current and travel checks.
6. Run full-load timing, endurance and authorized drops. Record actual evidence in `TEST_EVIDENCE_TEMPLATE.csv`; update requirement status only from that evidence.
