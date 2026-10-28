---
name: cansat-handoff-doc
description: HANDOFF.md at the CanSat repo root is the entry point — read it before touching firmware.
metadata: 
  node_type: memory
  type: reference
  originSessionId: 4056e5f4-a73b-47d5-9736-1f86ee611ece
  modified: 2026-08-07T01:30:50.966Z
---

`C:\Users\kalbh\Downloads\Cansat Ground station v2\HANDOFF.md`

Written 2026-08-07. The single best starting point for this project. Covers the
system map, the ten defects fixed that day (with root causes), a "load-bearing
facts — do not clean these up" list, what is verified vs unverified, and the
open items.

**Why it matters:** this codebase contains several things that look like bugs and
are deliberate — the double `setRfSwitchTable()` call, MISO remapped to pin 39,
I2C pinned at 100 kHz, a single IRQ action on the gateway's DIO9, no `standby()`
before `startTransmit()`. Each cost real debugging time. Changing one without
reading the rationale re-introduces a solved problem.

**How to apply:** read HANDOFF.md §3 before proposing any firmware change.
`PROJECT_HISTORY.md` links to it at the top. Raw transcript of the session that
produced it is at
`C:\Users\kalbh\.claude\projects\C--Users-kalbh-Downloads-Cansat-Ground-station-v2\996d7d21-2f46-4907-a344-a1c880145e6c.jsonl`.

Related: [[cansat-rf-plan-is-assigned]], [[cansat-no-local-toolchain]].
