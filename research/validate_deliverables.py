from pathlib import Path
import csv, hashlib, json, re, subprocess, zipfile
from bs4 import BeautifulSoup
import markdown
from urllib.parse import unquote

out=Path('deliverables')
report=(out/'VRITRA_ENGINEERING_REPORT.md').read_text(encoding='utf-8')
checks=[]
def check(name,ok):
    checks.append({'check':name,'passed':bool(ok)})
    if not ok:raise AssertionError(name)

check('all 15 principal report sections',all(f'## {i}.' in report for i in range(1,16)))
check('all requested priority tiers',all(x in report for x in ['### A —','### B —','### C —']))
check('explicit team clarification',all(x in report for x in ['drogue deploys immediately','600 ±10 m']))
check('flight qualification boundary','not a certified or fully compiled firmware image' in report)
with (out/'requirements.csv').open(encoding='utf-8-sig',newline='') as f: rows=list(csv.DictReader(f))
check('20 traced requirements',len(rows)==20 and len({r['Requirement_ID'] for r in rows})==20)
check('hardware results not invented',all(r['Status'].startswith('PENDING') for r in rows))
with (out/'PROMPT_COVERAGE.csv').open(encoding='utf-8',newline='') as f: coverage=list(csv.DictReader(f))
check('45 mapped prompt outputs',len(coverage)==45 and len({r['ID'] for r in coverage})==45)
check('coverage evidence limitations explicit',all(r['Status'] in {'DELIVERED_DESIGN','EVIDENCE_LIMITED'} and r['Remaining'] for r in coverage))
check('coverage artifacts exist',all((out/r['Artifact']).is_file() for r in coverage))
with (out/'numeric_traceability.csv').open(encoding='utf-8',newline='') as f: numeric=list(csv.DictReader(f))
check('32 numeric evidence families',len(numeric)==32 and all(r['Evidence_class'] and r['Source_section_or_derivation'] for r in numeric))
check('INA260 configuration restored','0x6527' in report and '1.25 mA' in report)
check('version2 wire budget reconciled','maximum payload64 bytes' in report and 'wire92 bytes' in report)
calc=json.loads((out/'calculations.json').read_text(encoding='utf-8'))
check('selected radio schedule matches report',calc['one_hz_schedule_downlink_bytes']==130 and calc['one_hz_schedule_uplink_bytes']==32 and calc['one_hz_schedule_ms']<=1000)
soup=BeautifulSoup((out/'VRITRA_ENGINEERING_REPORT.html').read_text(encoding='utf-8'),'html.parser')
check('HTML tables and code rendered',len(soup.find_all('table'))>=15 and len(soup.find_all('pre'))>=8)
missing=[]
for a in soup.find_all('a',href=True):
    h=a['href']
    if h.startswith(('http:','https:','#','mailto:')):continue
    target=(out/h.split('#')[0]).resolve()
    if not target.exists():missing.append(h)
check('report local links resolve',not missing)
for p in [Path('README.md'),*out.rglob('*.md')]:
    html=markdown.markdown(p.read_text(encoding='utf-8'),extensions=['tables','fenced_code'])
    for a in BeautifulSoup(html,'html.parser').find_all('a',href=True):
        h=a['href']
        if h.startswith(('http:','https:','#','mailto:')):continue
        if not (p.parent/unquote(h.split('#')[0])).exists():missing.append(p.as_posix()+': '+h)
check('all handoff Markdown links resolve',not missing)
manifest=json.loads((out/'HANDOFF_SHA256.json').read_text(encoding='utf-8'))
check('handoff hashes match',all(hashlib.sha256((out/r['path']).read_bytes()).hexdigest()==r['sha256'] for r in manifest))
with zipfile.ZipFile(out/'VRITRA_RESEARCH_HANDOFF.zip') as z:
    check('ZIP integrity',z.testzip() is None)
    check('no third-party downloads or binaries in shareable ZIP',all(not n.endswith(('.exe','.pyc')) and not n.startswith(('source/','research/')) for n in z.namelist()))
check('no placeholder TODOs',not re.search(r'\b(TODO|TBD|PLACEHOLDER)\b',report))
print(json.dumps({'passed':len(checks),'checks':checks,'missing_local_links':missing},indent=2))
Path('research/deliverable_validation.json').write_text(json.dumps(checks,indent=2)+'\n',encoding='utf-8')
