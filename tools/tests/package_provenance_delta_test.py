"""Four new review boundaries; retain the earlier 19 guard results."""
import json, sys, tempfile
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import build_provenance as p

checks = []
def check(ok, label):
    if not ok: raise AssertionError(label)
    checks.append(label)
def init(path):
    path.mkdir(); p.git(path, 'init', '-q')
    p.git(path, 'config', 'user.name', 'Fixture')
    p.git(path, 'config', 'user.email', 'fixture@invalid')
def commit(path):
    p.git(path, 'add', '-A'); p.git(path, 'commit', '-qm', 'fixture')

with tempfile.TemporaryDirectory(prefix='lo-provenance-delta-') as temporary:
    base = Path(temporary); r = base/'repo'; origin = base/'origin'
    init(origin); (origin/'source.cpp').write_text('base\n'); commit(origin)
    init(r); (r/'source.cpp').write_text('root\n'); commit(r)
    p.git(r, '-c', 'protocol.file.allow=always', 'submodule', 'add', '-q', str(origin), 'dependency'); commit(r)
    sub = r/'dependency'
    (sub/'source.cpp').write_text('first dirty content\n'); a = p.source_state(r)
    (sub/'source.cpp').write_text('other dirty content\n'); b = p.source_state(r)
    check(a['identity'] != b['identity'] and a['submodules'][0]['tracked_diff_sha256'] != b['submodules'][0]['tracked_diff_sha256'], 'unknown submodule dirty content fingerprint')
    (sub/'new.h').write_text('one'); a = p.source_state(r)
    (sub/'new.h').write_text('two'); b = p.source_state(r)
    check(a['identity'] != b['identity'], 'unknown submodule untracked content fingerprint')
    (sub/'new.h').unlink()
    p.git(sub, 'config', 'user.name', 'Fixture'); p.git(sub, 'config', 'user.email', 'fixture@invalid'); commit(sub)
    p.git(r, 'add', 'dependency'); state = p.source_state(r)
    check(state['dirty'] and state['submodules'][0]['reason'] == 'staged gitlink differs from HEAD', 'staged gitlink advance remains dirty')
    p.git(r, 'rm', '--cached', '-f', 'dependency'); state = p.source_state(r)
    check(state['dirty'] and state['submodules'][0]['indexed_commit'] is None, 'staged gitlink deletion remains dirty')
print(json.dumps({'passed': len(checks), 'checks': checks}, indent=2))
