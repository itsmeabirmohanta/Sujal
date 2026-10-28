---
name: cansat-no-local-toolchain
description: "There is no Arduino/C++ toolchain on this machine — firmware changes cannot be compile-checked, only structurally verified."
metadata: 
  node_type: memory
  type: project
  originSessionId: 4056e5f4-a73b-47d5-9736-1f86ee611ece
  modified: 2026-08-10T02:13:44.562Z
---

`arduino-cli` and `g++` are both absent on this machine (checked 2026-08-07).
Firmware edits to `vritra_fc/` and `vritra_gs/` **cannot be compiled** here. The
user builds and flashes from the Arduino IDE separately.

**Why:** it is dishonest to report a firmware change as "done" or "working" when
it has only been written, not built. Several sessions have been burned by
premature "it's fixed" claims that the user then disproved with a fresh crash
log.

**How to apply:** after firmware edits, say plainly that they are unverified and
ask the user to build. Substitute the structural checks that *are* possible —
brace/paren balance, cross-file constant agreement, wire-format field-count
cross-checks between `Base36.h`, `Config.h`, the gateway and `vritra_gcs.py`'s
SCHEMA. Write those as small Python scripts in the scratchpad.

**The strongest substitute is an END-TO-END WIRE SIMULATION, not index checking.**
`verify_schema.py` (in the repo root) proves the *indices* agree by name. It does
not prove a real packet decodes. Port `buildPacket()` (vritra_fc.ino) and
`ingest()`/`emitCsv()` (vritra_gs.ino) into Python — parsing the scale tables and
maps *out of* `Base36.h`/`vritra_gs.ino` with regex rather than retyping them —
then push a known vehicle state through all 16 rotation frames plus the degraded
cases (no fix / IMU dead / baro dead / quaternion unsettled / everything dead)
and assert the value that lands in `parse_packet()` is the value that was sent.
On 2026-08-10 that caught two real defects nothing else did: the gateway's
`len < 40` guard rejecting a legal 39-byte all-sensors-dead packet, and
GNSS_TIME being logged to SD as hhmmsscc while the downlink carries hhmmss.

`import vritra_gcs` works directly — PyQt5, pyqtgraph, numpy and pyserial are all
installed, so the real `SCHEMA`, `ALL_FIELDS` and `parse_packet()` can be used in
a checker instead of being re-derived by regex.

Two gotchas found the hard way:
- A brace-balance checker MUST strip string literals **before** `//` line
  comments — almost every log string in this project begins with `"// "`, so the
  wrong order eats the rest of the line and every logging-heavy file looks
  unbalanced.
- Bash heredocs mangle backslashes in regexes here; write checker scripts with
  the Write tool instead of piping them through `python - <<EOF`.

Related: [[cansat-handoff-doc]].
