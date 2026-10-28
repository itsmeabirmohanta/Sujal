---
name: project-cansat-final-build
description: "VRITRA CanSat FINAL flight build (2026-10-27) — 3 etched PCBs fly, drone drop from 1 km + guided glide, petri open 10 s at 950 m; read VRITRA_ZeroBoard_Memory/05 first"
metadata:
  node_type: memory
  type: project
  originSessionId: c609f89e-9d70-467c-b852-69f64ee21810
  modified: 2026-10-26T22:26:29.196Z
---

From 2026-10-27 the ordered final PCB will not arrive; the 3 copper-etched boards (1 MAIN T4.1, 2 BACKUP+SENSOR T4.1, 4 PARACHUTE T4.0) are the flight electronics, already soldered. Boards 1/2 were built from `kicad_single\BOARD1_MAIN_1L` / `BOARD2_BACKUP_SENSOR_1L` (120x150), NOT the compact etch_pcb copies (same nets, different positions); Board 4 from etch_pcb\BOARD4_PARACHUTE_PCB. Source of truth: `D:\linux\Software\Claude\VRITRA_ZeroBoard_Memory\05_FINAL_BUILD_2026-10-27.md` (pin maps dumped from the etched .kicad_pcb, hardware list, code lineage).

**Why:** user changed mission: drone lifts CanSat to ~1 km, releases it, CanSat glides with 2× 8 V steering servos to 26.717535 N 84.301543 E (target must be adjustable for wind); petri servo opens 10 s at 950 m above boot point. No 7Semi mux (camera via 2 wires on Board 2 U10 pads, picture in repo firmware/vritra_cam), backup LSM not used (IMU from MAIN ICM-20948), only 1 INA260 (battery).

**How to apply:** last working firmware = vritra_fc 6.1.0 + vritra_gs 6.2.0 (Aug 10, "Cansat Ground station v2"), GitHub copy `Downloads\vritra-gcs-master` adds the 19-10 radio (866.50/865.25 SF9 0x99, verified against the PDF). Ground station unchanged. Flight code must be upgraded from vritra_fc for the etched pins (see 05 "Firmware deltas"). User will calibrate GNSS/INA/BMP with their own reference — never assume standard calibration. ALL code now lives in the GCS repo `Downloads\vritra-gcs-master\vritra-gcs-master\firmware\` (user choice 2026-10-27): board_tests\, vritra_cam\ (XIAO ESP32S3 Sense camera, needs PSRAM=OPI), later the flight code; memory-folder firmware_tests\final\ copies are stale. Compile check: Teensy 1.62 g++ -fsyntax-only (script recreated per session).

Related: [[project-cansat-zeroboard]]
