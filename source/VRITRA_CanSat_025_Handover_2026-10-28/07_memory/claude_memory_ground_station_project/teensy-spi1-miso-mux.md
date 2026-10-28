---
name: teensy-spi1-miso-mux
description: "On Teensy 4.1, SPI1 defaults its MISO to pin 1 regardless of wiring — the LR1121 -707 was a floating input, not a bad joint"
metadata: 
  node_type: memory
  type: project
  originSessionId: 85dae6ee-1e5c-4280-b6aa-0d5247c9f08a
  modified: 2026-08-05T05:16:54.657Z
---

The VRITRA LR1121 `-707` (`RADIOLIB_ERR_SPI_CMD_FAILED`) at bring-up was **not**
a hardware fault. MISO was correctly soldered to **pin 39**, but Teensy 4.1's
SPI1 peripheral defaults its receive channel to **pin 1**. The MCU was
listening to an unconnected pad, a floating CMOS input idles high, so every
read returned `0xFF` — which is not a valid LR1121 status byte, so RadioLib
reported a failed command.

Fix: `SPI1.setMISO(39)` **before** `SPI1.begin()` — it rewires the internal
multiplexer to the physical pad. All of `setMISO`/`setMOSI`/`setSCK` must
precede `begin()`; afterwards the peripheral is already latched.

**Why this is worth keeping:** the symptom points at the radio, but nothing on
the board is broken — chip alive, joints good, MCU simply listening elsewhere.
The generic error code says only "status byte was not OK". On this project
`-707` has had **two** unrelated causes: this one, and the RF switch table
being cleared by `begin()` (fixed by calling `setRfSwitchTable()` both before
and after). Chasing the wrong one costs a day.

**How to apply:** `Radio.cpp` now decodes error codes to text and calls
`diagnoseSpi()` on any `begin()` failure, which classifies the raw MISO bytes —
all `0xFF` = floating/wrong pin, all `0x00` = shorted low or unpowered, mixed =
chip is talking so look at the RF switch table / `TCXO_V` / frequency args.
Read that output before touching anything. Note the ESP32 gateway cannot hit
this: `SPI.begin()` there takes the pins as explicit arguments.

Related: [[cansat-plate-resonance]]
