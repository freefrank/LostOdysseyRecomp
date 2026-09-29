"""Temporary, source-specific transport. Removed with the integration commit."""
from pathlib import Path
import base64
import hashlib
import json
import lzma
import subprocess

FILES = [
('LostOdysseyRecomp/gpu/command_processor.cpp','0bc4a954547cd05ee37d74f5c1737c85166753b2057d0dc58abf0b1d26bdc257','6c87526677da86305cdae351018fce7a22d553846ead720a3d4481287cfc16c3'),
('LostOdysseyRecomp/gpu/command_processor.h','006092693c112101a0be9f2027121eb97899ea052d8dfca66f381b3b93b19617','524c0e7eb99af83139377bffed8bf57007733890e958ca1cb54872ff01cd2033'),
('LostOdysseyRecomp/gpu/draw_state.h','8c6f15e94039e2b680e2467f6bea4fc73ea763b873800451c7933d6bb16bf99b','c7a872ece1aee3c62594f0a5d61905f55af9417143e1c7c980b490940d12b0d3'),
('LostOdysseyRecomp/gpu/draw_state_cache.h',None,'b574d3b16b14983c714d16551ebe6ac88989b48a2fe4aea7622c1d9d42576571'),
('LostOdysseyRecomp/gpu/native_mesh.h',None,'f4440d39ae8b334ff8c5ef4631daae79e431f1eb64c43c09cc3541c68dc6567d'),
('LostOdysseyRecomp/gpu/native_mesh_hooks.cpp',None,'48df7a530bef3d5b6db4151272d964e5bb83504432645c833e6426d7484a7f69'),
('tools/tests/native_mesh_test.cpp',None,'df112605488620fa4d6a47f4d922b8d4f9b036d027fb9d1c663d73e6d5fe0be7'),
('tools/tests/native_mesh_sdk_oracle.py',None,'1fc7f7200f75ae159e102a12c961b4f531624bdcdec3f34b5a1342e88e2d6fcb'),
('tools/tests/native_mesh_sdk_oracle.cpp',None,'3c8f61c0d7d2cf50336ff4ab14258917c0635911f1f3316b8122e11f3c7827c5'),
('CMakeLists.txt','754e7b482530fa08e02c840d82ef3ef1a14bb97d91e37aaadebc96743886a147','8c1aa2a00a1d9e28bd26d90511d3e58d51fb26784be86ed749584e3950e57d39'),
('LostOdysseyRecomp/CMakeLists.txt','4625ca2ff77429785d8ceda42ade4d2a789e6d63fb0b46dd6124db45df6d89cf','7e3781641088db9340e50e566514511d1705eaf9b8a5fcac34ee4c237a082c4d'),
]
for name, before, after in FILES:
    path = Path(name)
    actual = hashlib.sha256(path.read_bytes()).hexdigest() if path.exists() else None
    if actual != before:
        raise SystemExit(f'Refuse to overwrite changed source: {name}: {actual}')
encoded = ''.join(Path(f'tools/native-mesh-stage-{i}.b64').read_text().strip() for i in range(3))
patch = lzma.decompress(base64.b64decode(encoded, validate=True))
if hashlib.sha256(patch).hexdigest() != '3070d7aaaab5c77ea6b8d26b080ccf8d360f06bfd22c466c6653e446975e55b3':
    raise SystemExit('Reviewed patch checksum mismatch')
output = Path('out/native-p2'); output.mkdir(parents=True, exist_ok=True)
patch_path = output / 'reviewed.patch'; patch_path.write_bytes(patch)
subprocess.run(['git','apply','--check',str(patch_path)],check=True)
subprocess.run(['git','apply',str(patch_path)],check=True)
for name, before, after in FILES:
    actual = hashlib.sha256(Path(name).read_bytes()).hexdigest()
    if actual != after:
        raise SystemExit(f'Unexpected resulting source: {name}: {actual}')
(output/'SOURCE_SHA256.json').write_text(json.dumps(FILES,indent=2)+'\n',encoding='utf-8')
(output/'FILES.txt').write_text('\n'.join(name for name,_,_ in FILES)+'\n',encoding='utf-8')
print('Reviewed mesh integration applied; all input and output hashes match.')
