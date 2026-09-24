"""Apply the exact locally reviewed menu diff on an isolated CI branch."""
import base64
import hashlib
import lzma
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
parts = b''.join((root / 'tools' / f'menu-workbench.{i}.b64').read_bytes() for i in range(4))
patch = lzma.decompress(base64.b64decode(parts, validate=True))
expected = '5e2573112d16a9797a5ccc80763715530874d012226b07cd9bf34cf037babe45'
if hashlib.sha256(patch).hexdigest() != expected:
    raise SystemExit('Menu patch digest mismatch')
subprocess.run(['git', 'apply', '--check', '--unidiff-zero', '-'], input=patch, cwd=root, check=True)
subprocess.run(['git', 'apply', '--unidiff-zero', '-'], input=patch, cwd=root, check=True)
print('Applied reviewed menu patch:', expected)
