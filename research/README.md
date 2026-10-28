# Reproduction and evidence

Run `run_checks.py` from the repository root for C++/Python checks, calculations, navigation extracts, HTML, hashes and compact ZIP. `check_publication.py` separately verifies the original source manifest and archive and scans recognizable credential patterns; download the original release ZIP first when running it from a fresh clone.

| File | Purpose |
|---|---|
| `complete_handoff.py` | Idempotent report corrections and generated coverage/traceability/checklist/test templates; run via check runner |
| `package_deliverables.py` | Requirements extraction, source register, HTML, handoff hashes and ZIP |
| `validate_deliverables.py` | Coverage, file links, package/manifest and consistency checks |
| `check_results.json` | Actual local compiler/test output from the final run |
| `deliverable_validation.json` | Final structural/package validation results |
| `bundle_manifest.json` | All812 original supplied paths, sizes and SHA256 |
| `source_manifest.json`, `additional_manifest.json` | Public-source URLs, retrieval results and hashes |
| `publication_audit.json` | Original source integrity and limited credential scan result |
| `extracted/*.png` | Retained official-PDF page evidence for visual comparison |
| `fetch_sources.py`, `fetch_additional.py` | Optional source-retrieval scripts; network and document availability may change |
| `inspect_bundle.py` | Historical extraction script; requires PyMuPDF and the original source tree |
| `polish_report.py` | Retired one-off transformation, guarded against accidental report regression |
| `start_audit_revision.py` | Historical local backup helper, not needed for regeneration |

Do not use a historical report-regeneration script to overwrite team test results. The generated test evidence CSV is a blank template: record completed tests in separately named evidence files and link them from a reviewed requirement update. Downloaded third-party caches and duplicate text extraction are excluded; originals and citations remain available.
