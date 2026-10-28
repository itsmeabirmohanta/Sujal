# VRITRA CanSat — team engineering handoff

**Team 2026-INSPACe-CAN-7USAT-025 · review 28 October 2026 · campaign 31 October–3 November 2026**

Start with the [deliverable audit](deliverables/DELIVERABLE_AUDIT.md). The package contains the flight-software architecture, GNSS and sensor configuration, precision-landing design, checked reference code, and the complete supplied source history. **This is a research/design handoff, not flight-qualified firmware.** Hardware results and unavailable vendor facts remain explicitly open.

## Research desk

Open [the Editorial Grid research desk](website/index.html) for a visual, unified way to navigate the handoff. It links directly to the canonical report, operational documents, verification evidence, reference code, source register, and the original source snapshot. The site is a dependency-free static app; the only remote resources are optional display fonts, and all research links resolve to files in this repository.

| Your role | Read first | Then use |
|---|---|---|
| Team lead / cross-team coordinator | [Audit and coverage](deliverables/DELIVERABLE_AUDIT.md) | [Action plan](deliverables/ACTION_PLAN.md), [open questions](deliverables/TEAM_OPEN_QUESTIONS.md) |
| Avionics / sensors | [Engineering report](deliverables/VRITRA_ENGINEERING_REPORT.md) §§3–6 | [Sensor evidence](deliverables/SENSOR_COMPLETENESS.md), [numeric traceability](deliverables/numeric_traceability.csv) |
| Firmware | [Implementation status](deliverables/IMPLEMENTATION_STATUS.md) | [Wire ICD](deliverables/ICD_WIRE_FORMATS.md), [reference code](deliverables/reference_code/README.md) |
| GNC / mechanisms | [Engineering report](deliverables/VRITRA_ENGINEERING_REPORT.md) §§7–9 | [Requirements](deliverables/requirements.csv), [test evidence template](deliverables/TEST_EVIDENCE_TEMPLATE.csv) |
| Ground station | [GCS handoff](deliverables/GCS_HANDOFF.md) | [Protocol helper](deliverables/reference_code/gcs_protocol.py), report §10 |
| Launch operations | [Launch checklist](deliverables/LAUNCH_DAY_CHECKLIST.md) | [Verification boundaries](deliverables/VERIFICATION.md) |

## Downloads and provenance

- [Release downloads](https://github.com/itsmeabirmohanta/Sujal/releases/tag/research-handoff-2026-10-28): compact deliverables ZIP and original 118.6 MB supplied archive. The original archive exceeds GitHub's single-file Git limit, so it is a release asset.
- [Complete extracted source](source/VRITRA_CanSat_025_Handover_2026-10-28): all 812 supplied files, including chats and notes, retained with original bytes. Public inclusion was explicitly approved by the owner.
- [Original deep-research prompt](VRITRA_DEEP_RESEARCH_PROMPT_paste_this.md), [source manifest](research/bundle_manifest.json), [publication/integrity audit](research/publication_audit.json).
- [Readable report HTML](deliverables/VRITRA_ENGINEERING_REPORT.html): download/open locally; GitHub displays its source. Markdown is the canonical review format.

`source/` is historical evidence, including older firmware and superseded board maps. Use the current brief and boards as directed by the report. Do not flash the August firmware or assume historical chat answers are manufacturer facts.

## Reproduce the checks

Install Python3.10+ and a C++17 `g++` compiler, then from the repository root:

```text
python -m pip install -r research/requirements.txt
python research/run_checks.py
python research/check_publication.py
```

These are offline checks; they do not connect to boards, operate actuators or transmit RF. The runner builds tests in `build/`, regenerates calculations/HTML/ZIP/hash manifests and validates coverage, source integrity and relative links. `[PASS]` means the named host check passed, never that a hardware requirement was qualified.

Local Python dependencies, executable test outputs, downloaded third-party document caches, duplicate extracted text and old audit backups are excluded from Git. Source URLs/retrieval manifests are retained; `research/fetch_sources.py` and `research/fetch_additional.py` document retrieval. No credentials were detected by the limited recognizable-pattern scan; that scan is not a guarantee of absence of every possible secret.
