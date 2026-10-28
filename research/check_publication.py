"""Check source integrity, file sizes and likely credentials without printing secrets."""
from pathlib import Path
import hashlib,json,re,zipfile
root=Path('source/VRITRA_CanSat_025_Handover_2026-10-28')
manifest=json.loads(Path('research/bundle_manifest.json').read_text(encoding='utf-8'))
bad=[]
for entry in manifest:
    p=root/entry['path']
    if not p.is_file() or hashlib.sha256(p.read_bytes()).hexdigest()!=entry['sha256']:bad.append(entry['path'])
assert not bad, 'Source integrity mismatch: '+str(bad)
patterns={
 'github_token':rb'(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{30,})',
 'provider_key':rb'sk-(?:proj-|ant-)?[A-Za-z0-9_-]{32,}',
 'google_key':rb'AIza[0-9A-Za-z_-]{35}',
 'aws_key':rb'AKIA[0-9A-Z]{16}',
 'private_key':rb'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----',
}
hits=[]
for p in root.rglob('*'):
    if not p.is_file():continue
    data=p.read_bytes()
    for name,regex in patterns.items():
        if re.search(regex,data):hits.append({'file':p.relative_to(root).as_posix(),'kind':name})
    if zipfile.is_zipfile(p):
        with zipfile.ZipFile(p) as z:
            for member in z.infolist():
                if member.file_size>50_000_000:continue
                contents=z.read(member)
                for name,regex in patterns.items():
                    if re.search(regex,contents):hits.append({'file':p.relative_to(root).as_posix()+'!'+member.filename,'kind':name})
archive=Path('VRITRA_CanSat_025_Handover_2026-10-28.zip')
with zipfile.ZipFile(archive) as z:assert z.testzip() is None
result={'source_files_verified':len(manifest),'source_hash_mismatches':bad,'credential_pattern_hits':hits,
 'credential_scan_scope':'Common recognizable credential patterns in supplied files and one nested ZIP/container level; not proof of absence of all secrets.',
 'original_zip_bytes':archive.stat().st_size,'original_zip_sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),
 'publication_consent':'User explicitly selected public full archive including chats and notes on 2026-10-28.'}
Path('research/publication_audit.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print(json.dumps(result,indent=2))
if hits:raise SystemExit('Inspect flagged files locally before publishing; values intentionally omitted.')
