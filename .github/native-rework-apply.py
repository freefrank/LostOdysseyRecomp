"""One-shot source transport; no game, credentials or private inputs."""
from pathlib import Path
import base64
import gzip
import hashlib
import subprocess
import tempfile

root = Path('.github')
def part(name):
    return (root / ('native-rework-stage1-' + name + '.b64')).read_text().strip()

encoded = part('0') + part('1')[:4069] + part('1-suffix') + part('2a') + part('2b') + part('2c')
patch = gzip.decompress(base64.b64decode(encoded, validate=True))
expected = '9e40834a45d89d4ef76bbf649f2f7f00f516684e1471687af45e340e41468527'
if hashlib.sha256(patch).hexdigest() != expected:
    raise SystemExit('Staged source checksum mismatch; no source changed')
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory) / 'reviewed.patch'
    path.write_bytes(patch)
    subprocess.run(['git', 'apply', '--check', '--index', str(path)], check=True)
    subprocess.run(['git', 'apply', '--index', str(path)], check=True)
subprocess.run(['git', 'diff', '--cached', '--check'], check=True)
print('SOURCE_PATCH_SHA256=' + expected)
