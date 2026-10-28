# Three-day action plan

Generated from the [canonical engineering report](VRITRA_ENGINEERING_REPORT.md). All hardware results remain pending.

## 13. Priority list for the next three days

### A — must do before launch

**28 October:** freeze the pins, radio protocol and two-stage descent sequence. Identify the actual canopy, ESC, Hall sensor and buck. Set the steering rail to7.4–8.0 V. Run board bring-up, identity/readbacks and pulse checks. Resolve release confirmation, petri crossing direction and beacon compliance. Start endurance testing early.

**29 October:** implement the smallest multi-board port: live GNSS validity, local P4 events,600 m bay,950 m petri deadline, progress watchdogs, hard TX cutoff, truthful CSV and atomic target update. Flash matching MAIN/GS/GCS versions. Test pressure ramps, resets, link loss, invalid commands and SD failures before connecting packed mechanisms.

**30 October:** rehearse with the actual battery, camera, radio, servo loads and deployment mechanics. Use authorised drops to measure inflation, sink, steering sign and latency. If steering is unqualified, retain proven stable trim and report precision landing as undemonstrated. Close mandatory descent/beacon failures before clearance. Freeze the tested builds, hashes, cards and CSV schema.

### B — should do if time allows after A passes

Qualify tagged IMU FIFO, wind circle fitting, limited energy holding, turn-rate PI, better landing prediction, ambient temperature profiling, motor-on compass calibration and matching-protocol AGNSS assistance. Add them only when measured benefit justifies the complexity.

### C — future

Develop an observable augmented EKF/UKF, optimized terminal trajectories, airspeed/canopy heading measurements, range-assisted flare, independent backup telemetry, release feedback and a statistically supported landing campaign.

