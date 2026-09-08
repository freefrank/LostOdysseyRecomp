"""Only the two newly reviewed build/package source replacement boundaries."""
import json, sys, tempfile
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import build_provenance as p

checks = []
def rejects(fn, label):
    try: fn()
    except ValueError: checks.append(label); return
    raise AssertionError(label)
with tempfile.TemporaryDirectory(prefix='lo-provenance-race-') as temporary:
    r = Path(temporary)/'repo'; r.mkdir(); p.git(r, 'init', '-q')
    p.git(r, 'config', 'user.name', 'Fixture'); p.git(r, 'config', 'user.email', 'fixture@invalid')
    source = r/'source.cpp'; source.write_text('original\n')
    p.git(r, 'add', '-A'); p.git(r, 'commit', '-qm', 'fixture')
    captured = {'source_version': '0.5.0', 'source': p.source_state(r)}
    source.write_text('edited during build\n')
    rejects(lambda: p.validate_link_source(captured, '0.5.0', p.source_state(r)), 'same-commit source edit during build rejected')
    binary = Path(temporary)/'runtime.exe'; binary.write_bytes(b'original linked binary')
    stamp = {'binary_sha256': p.sha(binary)}
    binary.write_bytes(b'replaced while installer freezes')
    rejects(lambda: p.validate_staged_binaries([binary], [stamp]), 'package-stage binary replacement rejected')
print(json.dumps({'passed': len(checks), 'checks': checks}, indent=2))
