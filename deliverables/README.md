# VRITRA flight software and precision-landing review

Team 2026-INSPACe-CAN-7USAT-025. Engineering review dated **28 October 2026, India time**.

Start with the [deliverable audit](DELIVERABLE_AUDIT.md) and [45-row prompt coverage matrix](PROMPT_COVERAGE.csv). The [engineering report](VRITRA_ENGINEERING_REPORT.md) is the canonical design. The [repository role index](../README.md) directs each team to its work.

Operational extracts: [action plan](ACTION_PLAN.md), [launch checklist](LAUNCH_DAY_CHECKLIST.md), [team questions](TEAM_OPEN_QUESTIONS.md), [test evidence template](TEST_EVIDENCE_TEMPLATE.csv). Integration: [wire ICD](ICD_WIRE_FORMATS.md), [module status](IMPLEMENTATION_STATUS.md), [GCS handoff](GCS_HANDOFF.md), [sensor evidence](SENSOR_COMPLETENESS.md), [numeric traceability](numeric_traceability.csv).

The compact ZIP contains this deliverables folder. For links to the original source, use the GitHub checkout or extract the compact ZIP into `deliverables/` and the original archive into `source/` under the same parent. No source file has been rewritten.

Supporting deliverables:

- [Reference C++ and executable checks](reference_code/README.md): isolated engineering examples; **not an integrated flight firmware release**.
- [Requirements and verification matrix](requirements.csv): proposed acceptance tests, all hardware tests pending until the team records results.
- [Source and coverage register](SOURCE_REGISTER.md): source authority, retrieval limitations, and archive coverage.
- [Calculated radio/latency budgets](calculations.json): reproducible calculations, not flight measurements.
- [Verification results](VERIFICATION.md): what was actually run here.

**Controlling clarification from the team during this review:** passive drogue immediately after the drone releases the CanSat near 1 km; ram-air secondary opens at **600 ±10 m**. The petri event remains a separate **950 m, 10 s** event. Canopy cell count/area has not yet been reconfirmed; use the brief's 7-cell/0.40 m² only as an assumption, not a measured flight model.

The original ZIP and its extracted source have not been edited. No firmware was flashed, radio transmitted, mechanism operated, or message sent to organisers.
