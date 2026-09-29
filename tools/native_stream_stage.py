"""Temporary reviewed-source transport; removed by the integration commit."""
from pathlib import Path
import base64, hashlib, json, lzma, subprocess
FILES = [
['LostOdysseyRecomp/gpu/command_processor.cpp','6c87526677da86305cdae351018fce7a22d553846ead720a3d4481287cfc16c3','e364ff42ab820d1127074f21ff2b76f623d241ee3cbadc60b4e5f781bfeb0adc'],
['LostOdysseyRecomp/gpu/command_processor.h','524c0e7eb99af83139377bffed8bf57007733890e958ca1cb54872ff01cd2033','023eb3dcd6e555ab12ef4e830aca3e8aa64a94232ea459e8ee5dca9c29bed2d3'],
['LostOdysseyRecomp/gpu/native_mesh.h','f4440d39ae8b334ff8c5ef4631daae79e431f1eb64c43c09cc3541c68dc6567d','3426e59d77deb410a38ebc0ae4d4e22fa45e38639b30b00b23eebd836ddad376'],
['tools/tests/native_mesh_test.cpp','df112605488620fa4d6a47f4d922b8d4f9b036d027fb9d1c663d73e6d5fe0be7','6992ba4ef94930e147fa15e454343dbcca295090be7e0a77e80ed07aadc617b6'],
['tools/tests/native_mesh_sdk_oracle.cpp','3c8f61c0d7d2cf50336ff4ab14258917c0635911f1f3316b8122e11f3c7827c5','c37943cdfee6d17e93f56f68a3803a578b58e5066c6b31459cc46fcc5b3c36a2'],
['tools/tests/native_mesh_sdk_oracle.py','1fc7f7200f75ae159e102a12c961b4f531624bdcdec3f34b5a1342e88e2d6fcb','d68b257321345a138047e4630bd77ecf3df8a6853a42fa4f250cab093d451986'],
['tools/perf/run_native_title.py','713ab33d8e9ff8a307f084d79ac980aece470cce74235f6417c19d8e3ab4a8b4','7ae8d05eb500e539ee6814ab3b3815f608823a611e37dee1eec8c5026a199b75'],
['tools/perf/native_probe_log.py',None,'d370f52940863b5cb4c619c4d94b97bff8bdb6b26f6bfb32cbffa44736c64985'],
['tools/tests/native_probe_log_test.py',None,'1d9bb18f2ae20125b4d6e70ed834f0c859832e1d40987ba8fb17bd3790ce3562']]
for name,before,after in FILES:
    path=Path(name)
    actual=hashlib.sha256(path.read_bytes()).hexdigest() if path.exists() else None
    if actual!=before: raise SystemExit(f'Refuse changed source: {name}: {actual}')
patch=lzma.decompress(base64.b64decode(Path('tools/native-stream-stage.b64').read_text().strip(),validate=True))
if hashlib.sha256(patch).hexdigest()!='71f6d0ce62daeb147b2f383055d497cef3809bdebb1f9a70f179f4a662ea0c4d':
    raise SystemExit('Reviewed patch checksum mismatch')
out=Path('out/native-p3');out.mkdir(parents=True,exist_ok=True)
path=out/'reviewed.patch';path.write_bytes(patch)
subprocess.run(['git','apply','--check',str(path)],check=True)
subprocess.run(['git','apply',str(path)],check=True)
for name,before,after in FILES:
    if hashlib.sha256(Path(name).read_bytes()).hexdigest()!=after: raise SystemExit(f'Result mismatch: {name}')
(out/'SOURCE_SHA256.json').write_text(json.dumps(FILES,indent=2)+'\n',encoding='utf-8')
print('Reviewed stream and probe source applied; input and output hashes match.')
