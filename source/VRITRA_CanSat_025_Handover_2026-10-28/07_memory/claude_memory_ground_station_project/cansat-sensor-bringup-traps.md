---
name: cansat-sensor-bringup-traps
description: "Two VRITRA sensor faults that look like dead hardware but are not — the 64-byte Teensy UART buffer starving GNSS, and the ICM-20948 wedging until DEVICE_RESET"
metadata: 
  node_type: memory
  type: project
  originSessionId: 742f9cf8-7508-4dfc-92dc-2481a80b85f2
  modified: 2026-08-05T06:14:00.374Z
---

Both of these present as "the part is dead" and neither is.

**1. GNSS gets no fix — Teensy's default 64-byte UART RX buffer.**
At 9600 baud that is only 66 ms of data, but one 1 Hz NMEA burst
(GGA+RMC+GSA+GSV+GIGSV) is 400–700 bytes and takes ~600 ms to arrive. Any
`loop()` stall longer than 66 ms — an SD `sync()`, a 944 ms SF10 transmit slot —
overruns it. Bytes vanish mid-sentence, the checksum fails, the line is
discarded, and the receiver looks broken while working perfectly.
Fix: `GNSS_PORT.addMemoryForRead(buf, 1024)` right after `begin()`. The older
monolithic firmware had this and got 7–11 satellites; the modular rewrite
dropped it, which is the likeliest cause of the fix disappearing.

**2. ICM-20948 answers its address but returns 0xFF to every read.**
An I2C transaction cut off mid-byte (wire wiggle, or the Teensy resetting while
the ICM clocks out data) stalls the sensor's internal state machine waiting for
clock edges that never come. The address phase still ACKs, so a scan shows the
part present. `busRecover()`'s nine SCL pulses free the *bus* but cannot clear
the *chip*. Only `PWR_MGMT_1 = 0x80` (DEVICE_RESET) does.
Must be issued **blind and before the probe** — a wedged part fails WHO_AM_I, so
a reset placed after a successful probe never runs when it is actually needed.

**Why:** neither is inferable from the datasheets or the code; both cost real
bench time and both re-present as hardware faults.

**How to apply:** never blind-reset ICM_ADDR_B (0x68) — that is the DS3231 on
this harness, and its register 0x06 is the YEAR register, so 0x80 there writes
an invalid BCD year and corrupts the date. The ICM straps AD0 high (0x69). If a
future board straps it low, the reset must move, not gain a second address.

Related: [[teensy-spi1-miso-mux]]
