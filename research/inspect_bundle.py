from pathlib import Path
import sys, hashlib, json, zipfile, re
sys.path.insert(0, str(Path(__file__).parent / 'deps'))
import pymupdf

root = Path('source/VRITRA_CanSat_025_Handover_2026-10-28')
out = Path('research/extracted')
out.mkdir(exist_ok=True, parents=True)
manifest=[]
text_ext={'.md','.txt','.ino','.cpp','.h','.py','.m','.json','.html','.log','.csv','.dat','.yml','.properties'}
for p in sorted(root.rglob('*')):
    if not p.is_file(): continue
    raw=p.read_bytes()
    row={'path':p.relative_to(root).as_posix(),'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest()}
    text=''
    if p.suffix.lower()=='.pdf':
        doc=pymupdf.open(p)
        text='\n'.join(f'\n--- PDF PAGE {i+1} ---\n'+page.get_text(sort=True) for i,page in enumerate(doc))
        row['pages']=len(doc)
        if 'Frequency_Assignments' in p.name:
            for i,page in enumerate(doc):
                page.get_pixmap(matrix=pymupdf.Matrix(1.8,1.8)).save(out/f'frequency_page_{i+1}.png')
        if 'Mission_Requirements' in p.name:
            for i in [14,15,16,17]:
                doc[i].get_pixmap(matrix=pymupdf.Matrix(1.4,1.4)).save(out/f'guidelines_page_{i+1}.png')
    elif p.suffix.lower()=='.docx':
        import xml.etree.ElementTree as ET
        with zipfile.ZipFile(p) as z:
            tree=ET.fromstring(z.read('word/document.xml'))
            text='\n'.join(''.join(t.itertext()) for t in tree.iter('{http://schemas.openxmlformats.org/wordprocessingml/2006/main}t'))
    elif p.suffix.lower() in text_ext:
        text=raw.decode('utf-8',errors='replace')
    elif p.suffix.lower()=='.kicad_pcb':
        t=raw.decode('utf-8',errors='replace')
        text='\n'.join(line.strip() for line in t.splitlines() if re.search(r'\(net |\(property "(?:Reference|Value)"|\(pad |\(number ',line))
    elif p.suffix.lower()=='.zip':
        with zipfile.ZipFile(p) as z:
            row['members']=[{'path':a.filename,'bytes':a.file_size} for a in z.infolist()]
    if text:
        dst=out/(row['path'].replace('/','__')+'.txt')
        dst.write_text(text,encoding='utf-8')
        row['extracted']=str(dst).replace('\\','/')
        row['lines']=len(text.splitlines())
        row['headings']=[s[:180] for s in text.splitlines() if s.startswith('#')][:80]
    manifest.append(row)
Path('research/bundle_manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
print(json.dumps({'files':len(manifest),'text_files':sum('extracted' in m for m in manifest),'pdfs':sum('pages' in m for m in manifest),'total_bytes':sum(m['bytes'] for m in manifest)},indent=2))
