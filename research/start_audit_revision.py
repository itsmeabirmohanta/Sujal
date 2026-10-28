from pathlib import Path
import shutil,hashlib,json
root=Path('research/audit_v2');root.mkdir(parents=True,exist_ok=True)
original=root/'original_report.md'
if not original.exists():
    original.write_bytes(Path('deliverables/VRITRA_ENGINEERING_REPORT.md').read_bytes())
    manifest=[]
    for p in Path('deliverables').rglob('*'):
        if p.is_file():manifest.append({'path':p.as_posix(),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
    (root/'original_manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
print('Original report and deliverable hashes retained for the revision audit.')
