---
name: cansat-gnss-module
description: GNSS module is Quectel L89 / Airoha AG3335M — NOT ST Teseo III. It speaks PAIR; $PMTK353 does NOT enable NavIC.
metadata:
  node_type: memory
  type: project
  originSessionId: 742f9cf8-7508-4dfc-92dc-2481a80b85f2
  modified: 2026-08-07T01:31:41.055Z
---

GNSS module is confirmed **Quectel L89 / Airoha AG3335M**, NOT STMicro Teseo III.

**Why:** bring-up produced `$PAIR021,AG3335M_V2.5.0.AG3335_20221130,...` — PAIR is
the Airoha proprietary talker prefix.

**Consequences:**
- `$PSTM` commands are silently ignored by this chip.
- It speaks **PAIR** natively, plus a `$PMTK` subset and `$PQTM` (Quectel).
- PAIR commands are acknowledged with `$PAIR001,<cmd>,<result>` (0 = accepted).
  `Sensors.cpp` `nmeaLine()` parses these — use them, don't assume.

## CORRECTION 2026-08-07 — $PMTK353 does NOT enable NavIC

An earlier version of this memory said "$PMTK353,1,1,1,0,1 = GPS+GLONASS+
Galileo+NavIC". **That was wrong**, and it is why `NAVIC_SATS` read 0 on every
flight.

MediaTek's actual parameter order is:
`$PMTK353,<GPS>,<GLONASS>,<GALILEO>,<GALILEO_FULL>,<BEIDOU>` — there is **no
IRNSS/NavIC parameter at all**. That command was enabling BeiDou. The comment in
the firmware described intent, not behaviour, and the command ACKed — just for
something else.

**The correct command is `$PAIR066`:**
`$PAIR066,<GPS>,<GLONASS>,<Galileo>,<BeiDou>,<QZSS>,<NavIC>` — NavIC is the 6th
parameter. Firmware now sends `PAIR066,1,1,1,0,0,1` (4 constellations; a
documented note warns 5+ may exceed the AG3335M channel budget), with PMTK353 kept
as a fallback for a non-PAIR variant.

**Hardware caveat firmware cannot fix:** NavIC's civil signal is primarily **L5
(1176 MHz)**, not L1. With a plain L1 ceramic patch, NavIC is unreceivable no
matter what the receiver is configured to search for — `NAVIC_SATS` will stay 0
with every command ACKed successfully. **Confirm the antenna is L1+L5 before
concluding anything from a zero count.**

**How to apply:** `GNSS_SEND_CONFIG=true` in `Config.h` sends the config block on
every boot. Watch the console for `// [GNSS] PAIR066 accepted`. See
[[cansat-handoff-doc]] §2.5.
