from pathlib import Path
import csv
import html
import json
import re

import markdown

ROOT = Path(__file__).resolve().parent.parent
OUT = Path(__file__).resolve().parent / "pages"

DOCUMENTS = [
    ("Engineering report", "VRITRA_ENGINEERING_REPORT.md", "Canonical design", "read"),
    ("Deliverable audit", "DELIVERABLE_AUDIT.md", "Start here", "read"),
    ("Action plan", "ACTION_PLAN.md", "Next", "build"),
    ("Implementation status", "IMPLEMENTATION_STATUS.md", "Open work", "build"),
    ("GCS handoff", "GCS_HANDOFF.md", "Ground", "build"),
    ("Wire formats / ICD", "ICD_WIRE_FORMATS.md", "Interface", "build"),
    ("Verification results", "VERIFICATION.md", "Host pass", "verify"),
    ("Requirements matrix", "requirements.csv", "20 rows", "verify"),
    ("Prompt coverage", "PROMPT_COVERAGE.csv", "45 rows", "verify"),
    ("Numeric traceability", "numeric_traceability.csv", "32 families", "verify"),
    ("Test evidence template", "TEST_EVIDENCE_TEMPLATE.csv", "Fill next", "verify"),
    ("Launch-day checklist", "LAUNCH_DAY_CHECKLIST.md", "Operations", "build"),
    ("Open questions", "TEAM_OPEN_QUESTIONS.md", "10 questions", "build"),
    ("Sensor completeness", "SENSOR_COMPLETENESS.md", "Evidence", "read"),
    ("Source register", "SOURCE_REGISTER.md", "Provenance", "source"),
    ("Reference code index", "reference_code/README.md", "C++ / Python", "build"),
    ("Research validation", "../research/validate_deliverables.py", "Runner", "verify"),
    ("Host check runner", "../research/run_checks.py", "Runner", "verify"),
]


def source_path(relative_path):
    return ROOT / "deliverables" / relative_path if not relative_path.startswith("../") else ROOT / relative_path[3:]


def csv_to_html(path):
    rows = list(csv.reader(path.read_text(encoding="utf-8-sig").splitlines()))
    if not rows:
        return "<p>No rows.</p>"
    head, body = rows[0], rows[1:]
    table = ["<div class=\"table-scroll\"><table><thead><tr>"]
    table.extend(f"<th>{html.escape(cell)}</th>" for cell in head)
    table.append("</tr></thead><tbody>")
    for row in body:
        table.append("<tr>")
        table.extend(f"<td>{html.escape(cell)}</td>" for cell in row)
        table.append("</tr>")
    table.append("</tbody></table></div>")
    return "".join(table)


def render_content(path):
    if path.suffix.lower() == ".csv":
        return csv_to_html(path), path.read_text(encoding="utf-8-sig")
    if path.suffix.lower() in {".py", ".cpp", ".hpp", ".txt"}:
        raw = path.read_text(encoding="utf-8", errors="replace")
        return f"<pre><code>{html.escape(raw)}</code></pre>", raw
    raw = path.read_text(encoding="utf-8")
    return markdown.markdown(raw, extensions=["extra", "sane_lists", "toc"]), raw


def page(title, kind, body, original):
    return f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>{html.escape(title)} / VRITRA Research desk</title><link rel="stylesheet" href="../reader.css"></head>
<body><header class="reader-head"><a class="brand" href="../index.html#library"><span class="brand-mark">V</span><span><strong>VRITRA</strong><small>Research desk · 025</small></span></a><a class="back-link" href="../index.html#library">← Back to library</a></header>
<main class="reader-shell"><div class="reader-kicker">VRITRA RESEARCH DESK / {kind.upper()}</div><div class="reader-title"><h1>{html.escape(title)}</h1><a class="source-link" href="{html.escape(original)}">Open source file ↗</a></div><article>{body}</article></main>
<footer><span>CAN-7USAT-025 · REVIEW 01</span><a href="../index.html#library">Research library ↑</a></footer></body></html>"""


def main():
    OUT.mkdir(exist_ok=True)
    index = []
    for title, relative, status, category in DOCUMENTS:
        source = source_path(relative)
        body, raw = render_content(source)
        slug = re.sub(r"[^a-z0-9]+", "-", title.lower()).strip("-")
        output = OUT / f"{slug}.html"
        original = f"../../deliverables/{relative}" if not relative.startswith("../") else f"../../{relative[3:]}"
        output.write_text(page(title, status, body, original), encoding="utf-8")
        plain = re.sub(r"\s+", " ", raw).strip()
        index.append({"title": title, "page": f"pages/{slug}.html", "category": category, "status": status, "text": plain})
    (Path(__file__).resolve().parent / "search-index.js").write_text(
        "window.VRITRA_SEARCH_INDEX = " + json.dumps(index, ensure_ascii=False) + ";\n",
        encoding="utf-8",
    )
    print(f"Generated {len(index)} reader pages and search index.")


if __name__ == "__main__":
    main()
