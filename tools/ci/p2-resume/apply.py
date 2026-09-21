from pathlib import Path
import base64,gzip,hashlib,json,subprocess
c=Path(__file__).resolve().parent
r=json.loads((c/'request.json').read_text())
def git(*args): return subprocess.check_output(['git',*args],cwd='source',text=True).strip()
assert git('rev-parse','HEAD')==r['base']
if (c/'feature.patch').exists():
    patch=(c/'feature.patch').read_bytes()
else:
    encoded=base64.b64encode((c/'patch.gz').read_bytes()).decode().rstrip('=')
    for old,new in r.get('base64_replacements',[]):
        assert encoded.count(old)==1
        encoded=encoded.replace(old,new)
    patch=gzip.decompress(base64.b64decode(encoded+'==='))
assert hashlib.sha256(patch).hexdigest()==r['patch_sha256']
p=Path('reviewed.patch').resolve();p.write_bytes(patch)
patches=[p]
if (c/'compile-fix.patch').exists() and r.get('base64_replacements'):
    patches.append(c/'compile-fix.patch')
for p in patches:
    paths=[line.split('\t')[-1] for line in git('apply','--numstat',str(p)).splitlines()]
    assert paths and all(x.startswith(('LostOdysseyRecomp/gpu/','tools/tests/','docs/notes/')) and '..' not in Path(x).parts for x in paths)
    git('apply','--unidiff-zero','--check',str(p));git('apply','--unidiff-zero',str(p))
    git('add','--',*paths)
git('diff','--cached','--check')
