# Deep-research prompt audit

**Audit date: 28 October 2026, Asia/Calcutta.** Compared the supplied prompt with the current report, source snapshot, supporting artifacts and reference code. Read the [45-row coverage matrix](PROMPT_COVERAGE.csv) for each requested output, its location and remaining evidence.

**Result:** the existing package was incomplete and inconsistent as a handoff. Missing audit/navigation/supporting documents are now provided, identified omissions are corrected, reference modules are covered by host checks, and the shareable package is rebuilt. The design deliverables are available. This does **not** close unavailable vendor documentation, unidentified components, platform integration or physical acceptance tests.

| Finding at audit start | Resolution |
|---|---|
| No prompt-by-prompt completion register | 45 mapped rows, explicit DELIVERED_DESIGN / EVIDENCE_LIMITED status; neither status means flight verified |
| INA260 boot/read configuration replaced by calibration text | Restored identity/reset/config/read/status/scaling using TI register map |
| 96-byte payload versus whole-frame ambiguity | Version2 capped at64-byte payload/90 decoded/92 wire; exact message allocations in separate ICD |
| New reference modules absent from verification scope and README | All headers compile together; module behavioral tests and GCS tests added; status table documents adapters still needed |
| GNSS position update suppressed velocity from the same epoch | Independent position/velocity epoch tracking in scalar filter; regression check |
| FSM omitted stated descent dwell and accepted inconsistent restored flags | Dwell and restore validation fixed and tested |
| Driver boot helper skipped final settling delay | Completion now waits for last write's deadline; regression check |
| A fresh wind sample could hide an old fitting window | Fit requires both oldest/newest samples within the configured window; regression check |
| Shareable ZIP absent/stale verification and hashes | Deterministic handoff regenerated with current file list, verification and hashes |
| Long report difficult to assign across teams | Role index, action plan, checklist, questions, GCS brief, implementation status, sensor evidence and test sheet |
| Source/history might be mistaken for current firmware | Original files retained separately and hash checked; current constraints and qualification boundary made prominent |

## What remains genuinely open

- Quectel HA firmware-matched AGNSS/dynamic-model documentation, AG3335M SDK/channel allocation and some full-source self-test evidence are unavailable. Unsupported command blocks are intentionally not invented.
- Exact ESC/Hall/buck/flash variant, canopy geometry, servo limits/current/signal-loss behavior and physical release/deployment confirmation need team evidence.
- The report's two-stage sequence (drogue on release, secondary at600±10 m) is an inherited design decision recorded in the prior report. This audit visually checked the official state/telemetry pages and team025 frequency row. The team must still sign off the actual packed canopy, release-confirmation procedure and950 m event interpretation.
- The provided code comprises key modules and board integration contracts. Complete Teensy/P4 applications, HAL drivers, GCS GUI migration, platform builds and hardware qualification have not been produced or passed by implication.
- Per the prompt, no fabricated position/velocity belongs in the flight pipeline. Offline tests necessarily use synthetic inputs in separate test executables. Physical adapters, build isolation and hardware fault tests are still needed to enforce the policy in a complete firmware image.

The original task asks for architecture and key code blocks plus a ground-station change plan; these are supplied. It also asks for fully sourced facts that are not all obtainable. Those rows remain EVIDENCE_LIMITED rather than falsely marked complete. See [verification](VERIFICATION.md) for exactly what ran and [team questions](TEAM_OPEN_QUESTIONS.md) for how to close the remaining gaps.
