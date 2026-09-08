"""Only the new release provenance guards; no packaging or app execution."""
import sys, tempfile, subprocess, json
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import build_provenance as p

checks=[]
def check(ok,label):
    if not ok: raise AssertionError(label)
    checks.append(label)
def reject(fn,label):
    try:fn()
    except (ValueError,subprocess.CalledProcessError):checks.append(label);return
    raise AssertionError(label)
def init(path):
    path.mkdir();p.git(path,'init','-q');p.git(path,'config','user.name','Fixture');p.git(path,'config','user.email','fixture@invalid')
def commit(path):p.git(path,'add','-A');p.git(path,'commit','-qm','fixture')

with tempfile.TemporaryDirectory(prefix='lo-package-guards-') as temporary:
    base=Path(temporary);r=base/'repo';init(r)
    (r/'source.cpp').write_text('one\n');commit(r)
    a=p.source_state(r);check(not a['dirty'],'clean source')
    (r/'source.cpp').write_text('staged\n');p.git(r,'add','source.cpp')
    check(p.source_state(r)['dirty'],'staged-only dirty')
    p.git(r,'reset','--hard','HEAD')
    (r/'new.cpp').write_text('untracked');check(p.source_state(r)['dirty'],'untracked-only dirty');(r/'new.cpp').unlink()
    p.git(r,'tag','v0.5.0');p.git(r,'tag','v0.5')
    binary=base/'runtime.exe';binary.write_bytes(b'fixture binary A')
    stamp={'source_version':'0.5.0','source':a,'binary_sha256':p.sha(binary)}
    p.atomic_json(binary.with_suffix('.exe.build.json'),stamp)
    check(p.read_stamp(binary,'0.5.0')['source']['commit']==a['commit'],'matching linked stamp')
    check(p.validate_formal(r,'v0.5','0.5.0',a,[stamp])=='v0.5.0','two-component normalization')
    check(p.validate_formal(r,'v0.5.0','0.5.0',a,[stamp])=='v0.5.0','matching tag/version/build')
    reject(lambda:p.validate_formal(r,'v0.6.0','0.5.0',a,[stamp]),'formal version mismatch')
    reject(lambda:p.validate_formal(r,'v0.05.0','0.5.0',a,[stamp]),'absent exact tag rejected')
    reject(lambda:p.read_stamp(binary,'0.5.1'),'linked stamp version mismatch')
    binary.write_bytes(b'changed');reject(lambda:p.read_stamp(binary,'0.5.0'),'binary hash mismatch');binary.write_bytes(b'fixture binary A')
    (r/'source.cpp').write_text('two\n');commit(r);b=p.source_state(r)
    reject(lambda:p.validate_formal(r,'v0.5.0','0.5.0',b,[stamp]),'A build / B checkout same-version rejected')
    p.git(r,'tag','-f','v0.5.0')
    reject(lambda:p.validate_formal(r,'v0.5.0','0.5.0',b,[stamp]),'retagged B cannot relabel binary A')
    dirty_stamp=dict(stamp,source=dict(a,dirty=True))
    reject(lambda:p.validate_formal(r,'v0.5.0','0.5.0',b,[dirty_stamp]),'dirty build cannot become formal')

    sub=base/'origin';init(sub);(sub/'file.cpp').write_text('base\n');commit(sub)
    p.git(r,'-c','protocol.file.allow=always','submodule','add','-q',str(sub),'thirdparty/plume')
    target=r/'thirdparty/plume';(target/'file.cpp').write_text('patched\n')
    patch=r/'tools/patches/plume-lostodyssey.patch';patch.parent.mkdir(parents=True)
    patch.write_bytes(p.git(target,'diff','--binary','HEAD'))
    commit(r)
    state=p.source_state(r)
    check(not state['dirty'] and state['submodules'][0]['reason']=='exact tracked build patch','exact approved patch accepted')
    before=p.git(target,'status','--porcelain=v1');p.source_state(r)
    check(before==p.git(target,'status','--porcelain=v1'),'private index preserves real status')
    (target/'file.cpp').write_text('unknown\n');check(p.source_state(r)['dirty'],'unknown submodule tracked change')
    p.git(target,'add','file.cpp');(target/'file.cpp').write_text('patched\n')
    check(p.source_state(r)['dirty'],'unknown staged submodule with expected working file')
    p.git(target,'reset','HEAD','--','file.cpp')
    (target/'new.h').write_text('unknown');check(p.source_state(r)['dirty'],'unknown submodule untracked file');(target/'new.h').unlink()
    p.git(target,'config','user.name','Fixture');p.git(target,'config','user.email','fixture@invalid');commit(target)
    check(p.source_state(r)['dirty'],'submodule commit differs from pinned gitlink')
print(json.dumps({'passed':len(checks),'checks':checks},indent=2))
