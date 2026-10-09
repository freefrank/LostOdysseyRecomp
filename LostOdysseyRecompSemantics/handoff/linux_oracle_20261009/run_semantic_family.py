import sys,json,os,subprocess,shutil
from pathlib import Path
sys.path.insert(0,str(Path('tools/ghidra').resolve()))
from semantic_recovery import validate_manifests
name=sys.argv[1];f=json.loads(Path('LostOdysseyRecompSemantics/recovery_batches',name+'.json').read_text())['families'][0]
c=validate_manifests([Path(f['manifest'])],Path('LostOdysseyRecompLib/ppc'))[0]
p=Path('out/private-inputs',name+'-original.cpp');p.write_bytes(f['prelude'].encode()+b'\n'+c['original_cpp']+b'\n'+Path(f['harness']).read_bytes())
for h in ('object_sort_engine61_oracle_fixture.h',):
 s=Path('LostOdysseyRecompSemantics/tests',h)
 if s.exists():shutil.copy(s,'/tmp/semantic-linux-oracle')
args=['/tmp/semantic-clang/usr/bin/clang++-19','-std=c++20','-O2','-msse4.1','-Wno-ignored-attributes']
for inc in ['/tmp/semantic-linux-oracle','LostOdysseyRecompLib/ppc','tools/XenonRecomp/thirdparty/simde','LostOdysseyRecompSemantics/include','LostOdysseyRecompSemantics/tests']:args+=['-I',inc]
exe='/tmp/'+name+'-original';args += [str(p),'/tmp/semantic-library-clang/libLostOdysseyRecompSemantics.a','-o',exe]
env=dict(os.environ,LD_LIBRARY_PATH='/tmp/semantic-clang/usr/lib/llvm-19/lib')
with open('/tmp/'+name+'-compile.log','w') as log:r=subprocess.run(args,env=env,stdout=log,stderr=log)
if r.returncode:
 for line in Path('/tmp/'+name+'-compile.log').read_text().splitlines():
  if 'error:' in line or 'undefined reference' in line:print(line)
 sys.exit(r.returncode)
r=subprocess.run([exe],capture_output=True,text=True,timeout=60);print(r.stdout+r.stderr,end='')
Path('/tmp/'+name+'-result.json').write_text(json.dumps({'family':name,'status':'passed' if r.returncode==0 else 'failed','platform':'Linux x64 Clang19 + mmap guest-window overlay','summary':r.stdout+r.stderr,'original_body_check':'passed','library':'/tmp/semantic-library-clang/libLostOdysseyRecompSemantics.a'},indent=2)+'\n');sys.exit(r.returncode)
