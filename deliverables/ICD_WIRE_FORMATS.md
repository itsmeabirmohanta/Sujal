# Interboard wire contract, version 2

This supplements report §3.3. It resolves the earlier ambiguous 96-byte **payload** versus **whole-frame** budget. All numbers here are design allocations (D), verified by host codec tests where implemented. Physical endpoints and fixed pins remain in report §3.1 and `reference_code/pins.hpp`.

Each UART uses 115200 8N1. A frame is COBS encoded and ends with zero. Decoded bytes: version u8=2, type u8, source u8 (1 MAIN, 2 BACKUP, 4 P4), flags u8, session u32, sequence u32, sender monotonic microseconds u64, payload length u16, payload 0–64 bytes, CRC32 u32. Integer fields are little-endian. CRC-32/ISO-HDLC covers header and payload, excluding CRC. Header 22 + payload 64 + CRC 4 = 90 decoded bytes; maximum wire frame 92 bytes. Golden CRC check: ASCII `123456789` → `CBF43926`.

No C++ struct is copied to the wire. Float values are IEEE754 binary32, little-endian. Nonfinite fields require their validity bit clear; a receiver does not use invalid numeric fields. Unknown version/type/source, invalid size, failed CRC and expired session are rejected. The generic codec does not enforce semantic payloads or leases; board adapters must implement the table below. Sequence numbers wrap using half-range comparison. A session change requires mission resynchronization, not clearing flight latches.

| Type | Exact payload in order | Bytes | Period / expiry |
|---|---|---:|---|
| 1 NAV_RAW | epoch_counter u32; latitude_e7 i32; longitude_e7 i32; MSL_mm i32; vN_mmps i32; vE_mmps i32; fix_quality u8; fix_dimension u8; sats u8; origin u8; HDOP_x100 u16; GGA_age_ms u16; RMC_age_ms u16; GSA_age_ms u16; UTC_day_ms u32; UTC_date_yyyymmdd u32 | 44 | 20 Hz envelope; original epoch/age retained; report §4.4 gates |
| 2 BARO | sample_counter u32; pressure_Pa f32; temperature_C f32; pad_pressure_Pa f32; pad_temperature_K f32; AGL_m f32; v_up_mps f32; sample_age_ms u16; validity u16; calibration_id u32 | 36 | 20 Hz; 100 ms sample age |
| 3 MISSION | mission_id u32; elapsed_us u64; launch_elapsed_us u64; state u8; event_flags u8; owner u8; time_trusted u8; target_revision u32; lease_ms u16; reserved u16=0 | 32 | 10 Hz plus event; MAIN lease 500 ms |
| 4 ACT_STATUS | mission_id u32; elapsed_us u64; state u8; owner u8; guidance_mode u8; validity u8; left/right/bay/petri_us u16 each; wheel_degps f32; target_revision u32; nav_epoch u32; nav_age_ms u16; main_age_ms u16; backup_age_ms u16; reserved u16=0; faults u32 | 48 | 20 Hz; mark stale after 250 ms |
| 5 COMMAND | transaction_id u32; command_id u8; argument_length u8; argument bytes (0–58) | 6–64 | On demand; session, command, phase and argument validation |
| 6 ACK | transaction_id u32; command_id u8; result u8; event_flags u8; reserved u8=0; applied_revision u32; applied_lat_e7 i32; applied_lon_e7 i32 | 20 | On demand; committed P4 result required |
| 7 TIME_SYNC | challenge_id u32; stage u8 (0 request/1 reply); reserved u8=0; uncertainty_ms u16; requester_send_us u64; responder_receive_us u64; responder_send_us u64; mission_elapsed_us u64 | 40 | 1 Hz; match challenge; bounded round-trip uncertainty |

Origin values match `Origin`: None=0, LiveUart=1, Replay=2, Synthetic=3, Default=4. Only LiveUart may supply flight navigation. NAV_RAW is the same receiver as the P4 raw UART, never a second independent fix. MISSION event bits follow `JournalFlag` in `journal.hpp`. ACK results are D allocations: 0 applied, 1 staged, 2 phase denied, 3 bad argument, 4 stale session/sequence, 5 unavailable, 6 persistence failed. Never interpret ACK0 as physical mechanism movement.

The receiver computes effective age as reported sample age + measured transport/queue delay + clock uncertainty. Reject saturation of a u16 millisecond age rather than wrapping it. A retransmitted envelope cannot refresh the epoch. Do not subtract independently booted MCU clocks; TIME_SYNC maps clocks with uncertainty. If that mapping is unavailable, use a conservative upper bound or invalidate the forwarded navigation. Local P4 raw GNSS remains the lowest-latency option.

One P4 writer owns GPIO54, which physically fans to both Teensys; do not create two competing TX drivers. MAIN and BACKUP receive each other's status only through P4 traffic. MAIN alone owns GNSS TX8/reset6. Heartbeat is progress-qualified at 10 Hz, suspect after 500 ms; backup support requires MAIN lease and heartbeat loss for 1 s. P4 retains all actuator/event ownership. Commands cannot roll back release or deployment flags after ownership changes.

## Ground radio and camera

Radio downlink: report §6.3 / `telemetry.hpp` defines the 17 core scalar CSV fields, in guideline order with accelerometer expanded to X/Y/Z. ASCII decimal, final CR. Blank means unavailable; the judge's treatment of blanks/acceleration expansion remains an acceptance item. Proposed 130-byte downlink + 32-byte uplink + 40 ms guards = 984.128 ms under the stated SF9 assumptions. Extra data or ACKs need a separately calculated schedule; they must not silently displace 1 Hz core telemetry.

Uplink: `025,C,<id>,<signed-int32>,<txn-2hex>,<session-8hex>` with no terminator, PHY CRC on; `gcs_protocol.py` checks the 32-byte cap. Command19 latitude,20 longitude,21 commit use one transaction. Session/sequence/area/phase checks happen before the target helper. A short alias or CRC is not authentication. Legacy IDs4–8 stay retired.

Camera: preserve the actual `vritra_cam.ino` command/response parser at 115200; report §5.6 lists PING/STAT/SNAP/REC. Ground integration must capture exact newline and response strings from that source instead of accepting a substring as proof of recording. Bound commands to 64 bytes, response deadline 500 ms D, and limited retries. Camera failure never stalls mechanisms.

## Persistence and log bytes

`journal.hpp` specifies an 88-byte version2 journal, CRC over bytes0–79, CRC at80–83, commit marker at84–87. Two slots must occupy independently erasable storage when using NOR. Validate semantic fields after CRC, preserve old slot until new slot is committed/read back. The header implements encoding/selection, **not storage writes, RTC recovery or atomic physical movement**. The reference schema is a core subset; calibration IDs and instrument records are separate versioned records and must be linked by the platform integration.

`logging.hpp` defines `VLR2`, version2, type/source, session/sequence, sample and mission microseconds, payload length/flags, payload≤64 bytes and CRC. Total 40–104 bytes. The SPSC ring holds Capacity−1 elements; exactly one producer and one consumer. Verify atomic lock-free behavior on the target. Record drop counts and preserve sample time when SD is slow. SD/NOR writers and offline export adapters remain integration work.
