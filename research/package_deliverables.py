from pathlib import Path
import csv, hashlib, json, re, zipfile
import markdown

out=Path('deliverables')
report=(out/'VRITRA_ENGINEERING_REPORT.md').read_text(encoding='utf-8')
with (out/'requirements.csv').open('w',newline='',encoding='utf-8-sig') as f:
    w=csv.writer(f);w.writerow(['Requirement_ID','Priority','Requirement_and_source','Owner_and_acceptance','Status'])
    for line in report.splitlines():
        if re.match(r'\| R\d\d [ABC] \|',line):
            cells=[x.strip() for x in line.strip('|').split('|')]
            rid,priority=cells[0].split()
            w.writerow([rid,priority,cells[1],cells[2],'PENDING - team hardware evidence required'])

rows=[]
for name in ['source_manifest.json','additional_manifest.json']:
    rows.extend(json.loads(Path('research',name).read_text(encoding='utf-8')))
bundle=json.loads(Path('research/bundle_manifest.json').read_text(encoding='utf-8'))
text_scanned=sum(r.get('lines',0) for r in bundle)
source=[
'# Source register and coverage',
'',
'Review date: 28 October 2026, Asia/Calcutta. The report distinguishes verified documentation from design proposals and unverified hardware behavior.',
'',
'## Authority and local evidence',
'',
'1. Current team clarification: passive drogue immediately after drone release; ram-air secondary at600 ±10 m. This overrides the initial interpretation of the brief.',
'2. Current `01_PROJECT_BRIEF_AND_PARTS.md` plus current board BUILD_LIST/KiCad files control the fixed parts and pins. The older final-build memory contains superseded Teensy/P4 maps; do not choose its first map.',
'3. Official mission guidelines control requirements. Relevant complete pages were extracted; pages15–18 were rendered. Radio assignment pages1–2 were visually inspected, including team025 row18. The supplied July radio plan is superseded.',
'4. Current repository was inspected for drivers, telemetry packing, commands, mission state, logger, board tests, camera and GCS schema/parsing/simulation. Code line references refer to the supplied snapshot.',
'5. Earlier chats, CDR, design reports and engineering-memory files explain history and conflicts; an AI answer or simulation is not device documentation or a passed test.',
'',
'## Archive coverage and limits',
'',
f'- All **{len(bundle)} supplied files** were inventoried with byte count and SHA256. **{sum("extracted" in r for r in bundle)} text/structured files** were extracted, including **{sum("pages" in r for r in bundle)} PDFs** and both Word files. Extracted material totals{str(text_scanned)} lines. Nested ZIP member lists were recorded.',
'- All supplied chat text was scanned (about13.3 million characters), with targeted review of relevant decisions and current hardware discussions. This is not a claim of human-style line-by-line reading of every historical chat token or visual inspection of every archive image.',
'- Current board net/build lists and source drivers were checked against the brief. Historical Gerbers, CAD/STEP/STL meshes and all nested archive variants were not independently rebuilt, rendered or electrically/mechanically qualified. Empty DRC reports do not verify the soldered article.',
'- The original ZIP and extracted source were not changed. No physical board, GNSS sky reception, RF transmission, actuator, acoustic output, power endurance or drop test was available.',
'- Full-source manifests and extracted pages remain under `research/` in the workspace; downloaded third-party documents are excluded from the shareable handoff ZIP.',
'',
'## Public primary sources retrieved',
'',
'HTTP failures below are recorded rather than silently substituted with another part. Some documents could be read through web extraction even when the local download failed.',
'',
'| ID | Source | Retrieval | Local evidence |',
'|---|---|---|---|',
]
for r in rows:
    status=str(r.get('status','ERROR'))
    file=r.get('file','not downloaded')
    source.append(f"|{r['id']}|[Official/vendor source]({r['url']})|{status}|{file}|")
source.extend([
'',
'## Additional primary sources consulted through web tools',
'',
'- [ST LSM6DSO32 datasheet](https://www.st.com/resource/en/datasheet/lsm6dso32.pdf), [AN5473](https://www.st.com/resource/en/application_note/an5473-lsm6dso32-alwayson-3d-accelerometer-and-3d-gyroscope-stmicroelectronics.pdf), [official driver](https://github.com/STMicroelectronics/lsm6dso32-pid). Local ST PDF retrieval timed out; the official driver and self-test source were downloaded.',
'- [TDK ICM-20948 DS-000189](https://product.tdk.com/system/files/dam/doc/product/sensor/mortion-inertial/imu/data_sheet/ds-000189-icm-20948-v1.5.pdf), with [SparkFun register reference](https://github.com/sparkfun/SparkFun_ICM-20948_ArduinoLibrary). TDK search excerpts and official register descriptions were available; complete local TDK PDF download was denied. Do not regard every timing/self-test limit as independently verified here.',
'- [ADI DS3231 datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/ds3231.pdf). Web text was available; local download timed out.',
'- [Quectel L89 product/document index](https://www.quectel.com/product/gnss-l89-r2/) lists HA protocol v1.1, hardware v1.4 and AGNSS notes, but those downloads require an account. The accessible v1.0 protocol and v1.2 hardware guide were used with explicit installed-firmware verification gates.',
'- [7Semi L89HA official library](https://github.com/7semi-solutions/7semi-L89HA-GNSS-Module-Arduino-Library), also supplied in the ZIP. It is a board-vendor library, not an Airoha register-level SDK.',
'- [P4 UART v5.5](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-reference/peripherals/uart.html), [MCPWM v5.5](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-reference/peripherals/mcpwm.html), [FreeRTOS v5.5](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-reference/system/freertos_idf.html), [Arduino LEDC](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/ledc.html). Stable/latest pages can drift; verify against the locked core.',
'- [RadioLib LR1120 API at7.7.1](https://github.com/jgromes/RadioLib/blob/7.7.1/src/modules/LR11x0/LR1120.h) and [implementation](https://github.com/jgromes/RadioLib/blob/7.7.1/src/modules/LR11x0/LR1120.cpp); [Semtech calculator](https://www.semtech.com/design-support/lora-calculator).',
'- [NASA standard atmosphere](https://ntrs.nasa.gov/api/citations/19770009539/downloads/19770009539.pdf) for the declared atmosphere model; [NASA parafoil wind/aerodynamic estimation](https://ntrs.nasa.gov/citations/20200011546); [parafoil real-time guidance research](https://arxiv.org/abs/2105.00715).',
'- [ArduPilot parachute documentation](https://docs.ardupilot.org/plane/docs/common-parachute.html) is a deployment failsafe reference, not a demonstrated ram-air guidance mode. Fixed-wing automatic landing code cannot be copied as a qualified parafoil controller.',
'',
'## Unclosed source/measurement questions',
'',
'- No verified public AG3335M firmware SDK, complete HA-specific EPO injection transaction, firmware-matched dynamic model or geofence command was obtained. No LC29H/LC76G command was assumed interchangeable.',
'- Exact installed flash suffix, ESC, Hall sensor, steering buck, camera sensor and servo-programming configuration remain team identification tasks.',
'- No manufacturer guarantee was found for the fitted servos’ loss-of-signal holding, backdrive torque or stall current. These are measurements, not constants to invent.',
'- The canopy geometry conflict remains unresolved. Supplied CFD/FEA/Monte Carlo claims were not rerun or adopted as flight performance.',
'- Reference frame/log/journal codecs, scheduling helpers, scalar estimation, wind fit and planner blocks now have host checks. Complete board adapters, persistent storage writes, path constraints, device drivers and GUI integration remain outstanding; see IMPLEMENTATION_STATUS.md.',
'',
'## Source hashes',
'',
'The workspace JSON manifests contain SHA256 values for downloaded source files. The handoff has its own manifest so future edits can be distinguished from this review.',
])
(out/'SOURCE_REGISTER.md').write_text('\n'.join(source)+'\n',encoding='utf-8')

# Markdown HTML is provided for readable local browsing/printing, not as an app.
css='''body{max-width:1180px;margin:40px auto;padding:0 24px;font:16px/1.6 system-ui,Segoe UI,sans-serif;color:#172331;background:#fff}h1,h2,h3{line-height:1.25;color:#163952}h1{font-size:32px}h2{border-top:2px solid #dbe5eb;padding-top:22px;margin-top:40px}a{color:#145a86}table{border-collapse:collapse;width:100%;font-size:14px;margin:18px 0;display:block;overflow:auto}th,td{border:1px solid #c8d4dc;padding:9px 12px;vertical-align:top}th{background:#eaf1f5;text-align:left}tr:nth-child(even){background:#f8fafb}pre{padding:16px;background:#f0f4f7;overflow:auto;border-radius:6px;line-height:1.45}code{font-family:Consolas,monospace;font-size:13px}p>code,li>code{background:#f0f4f7;padding:2px 4px}strong{color:#123a54}@media print{body{max-width:none;margin:12mm;font-size:10pt;padding:0}table{display:table;font-size:8pt}pre{white-space:pre-wrap}h2,h3{break-after:avoid}tr{break-inside:avoid}a{color:inherit}}'''
md=markdown.Markdown(extensions=['tables','fenced_code','toc'])
body=md.convert(report)
nav='<nav aria-label="Report contents"><details><summary>Contents</summary>'+md.toc+'</details></nav>'
(out/'VRITRA_ENGINEERING_REPORT.html').write_text('<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>VRITRA engineering review — 28 October 2026</title><style>'+css+'</style></head><body>'+nav+body+'</body></html>',encoding='utf-8')
files=[]
for p in sorted(out.rglob('*')):
    if p.is_file() and '__pycache__' not in p.parts and p.name not in ['HANDOFF_SHA256.json','VRITRA_RESEARCH_HANDOFF.zip'] and p.suffix not in ['.exe','.pyc']:
        files.append({'path':p.relative_to(out).as_posix(),'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
(out/'HANDOFF_SHA256.json').write_text(json.dumps(files,indent=2)+'\n',encoding='utf-8')
with zipfile.ZipFile(out/'VRITRA_RESEARCH_HANDOFF.zip','w',zipfile.ZIP_DEFLATED) as z:
    for p in sorted(out.rglob('*')):
        if p.is_file() and '__pycache__' not in p.parts and p.suffix not in ['.exe','.zip','.pyc']:
            z.write(p,p.relative_to(out))
print(json.dumps({'requirements':sum(1 for l in report.splitlines() if re.match(r'\| R\d\d [ABC] \|',l)),
                  'report_words':len(report.split()),'source_rows':len(rows),'handoff_files':len(files)},indent=2))
