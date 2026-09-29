"""Apply the already staged tail source; never access a game or user profile."""
from pathlib import Path
import hashlib
import subprocess
import tempfile

root = Path('.github')
expected = [
    'af13f7eee0101248ce715d825135982c37232196',
    '952c7f8b77fabf7e438a3dc2142dd6230812ac40',
    'b9be81d5716fce02e0cfa3fe3fbb7aa2150d5ef6',
    '69dd61f73d9fab63a06de32971c173855b10d25f',
    '90481d7ee7531938ee18558879ec29759b6f6498',
]
parts = []
for number, digest in enumerate(expected):
    data = (root / f'native-tail-text-{number}.patch').read_bytes()
    actual = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
    if actual != digest:
        raise SystemExit(f'Staged part {number} differs; no source changed')
    parts.append(data)
# These five saved parts contain complete runtime/config/document changes.
# The following perf README hunk was interrupted; apply it below explicitly.
patch = b''.join(parts).split(b'diff --git a/tools/perf/README.md', 1)[0]
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory) / 'prepared.patch'
    path.write_bytes(patch)
    subprocess.run(['git', 'apply', '--unidiff-zero', '--check', '--index', str(path)], check=True)
    subprocess.run(['git', 'apply', '--unidiff-zero', '--index', str(path)], check=True)

def replace(path, old, new):
    file = Path(path)
    text = file.read_text(encoding='utf-8')
    if text.count(old) != 1:
        raise SystemExit('Unexpected source context: ' + path + ' / ' + old[:50])
    file.write_text(text.replace(old, new), encoding='utf-8', newline='\n')

replace('tools/perf/native_probe_log.py',
    "    if any(key not in first or key not in last or last[key] < first[key] for key in counters):",
    "    if 'prepared_draws' in first or 'prepared_draws' in last:\n        counters += ('prepared_draws',)\n    if any(key not in first or key not in last or last[key] < first[key] for key in counters):")
replace('tools/perf/native_probe_log.py',
    '    if args.scene == "uhra":',
    '    if getattr(args, "prepared_tail", False):\n        if args.native_frontend != "mesh":\n            raise ValueError("prepared tail requires mesh frontend")\n        env["LO_NATIVE_FRONTEND_PREPARED"] = "1"\n    if args.scene == "uhra":')
replace('tools/perf/run_native_title.py',
    '    parser.add_argument("--render-timing", action="store_true",',
    '    parser.add_argument("--prepared-tail", action="store_true",\n                        help="enable post-SDK-preparation continuation; requires mesh frontend")\n    parser.add_argument("--render-timing", action="store_true",')
replace('tools/perf/run_native_title.py',
    '    args = parser.parse_args()',
    '    args = parser.parse_args()\n    if args.prepared_tail and args.native_frontend != "mesh":\n        parser.error("--prepared-tail requires --native-frontend mesh")')
replace('tools/perf/run_native_title.py',
    '              "native_frontend": args.native_frontend,',
    '              "native_frontend": args.native_frontend, "prepared_tail": args.prepared_tail,')
replace('tools/perf/run_native_title.py',
    '                    if probe.generation != first_generation:',
    '                    if args.prepared_tail and (not execution or not execution.get("prepared_draws", 0)):\n                        result["failure"] = "no post-preparation native draw executed in the sample"\n                    if probe.generation != first_generation:')
with Path('tools/perf/README.md').open('a', encoding='utf-8', newline='\n') as out:
    out.write('\n### Prepared SDK tail experiment\n\nAdd `--prepared-tail` with `--native-frontend mesh`. This separately enables\n`LO_NATIVE_FRONTEND_PREPARED=1` and requires an increase in executed\n`prepared_draws`, not just commands or skipped predicates. Keep the flag\nabsent for the original mesh control. Regenerate PPC before building.\nSee [the continuation contract](../../docs/notes/native-prepared-tail.md).\n')
# Strong SDK entry declarations already exist in the canonical shared header.
# Its weak-symbol linkage must not be replaced by an ad-hoc extern-C declaration.
replace('tools/tests/native_prepared_sdk_oracle.cpp',
    'extern "C" PPC_FUNC(sub_823C6860);\nextern "C" PPC_FUNC(sub_827B56B0);',
    '// sub_823C6860/sub_827B56B0 use the canonical shared-header declarations.')
# Existing manual entry-only oracle remains usable after the config gains the
# new callbacks; it must declare callbacks embedded in the selected functions.
replace('tools/tests/native_mesh_sdk_oracle.py',
    "    source += [bodies[address] for address in sorted(bodies)]",
    "    for name in ('NativePreparedIndexed', 'NativePreparedAuto'):\n        if any(f'if ({name}(' in body for body in bodies.values()):\n            arguments = ','.join(['PPCRegister&'] * 8)\n            source.append(f'extern bool {name}({arguments});')\n    source += [bodies[address] for address in sorted(bodies)]")
subprocess.run(['git', 'add', 'tools/perf/native_probe_log.py', 'tools/perf/run_native_title.py',
                'tools/perf/README.md', 'tools/tests/native_prepared_sdk_oracle.cpp',
                'tools/tests/native_mesh_sdk_oracle.py'], check=True)
subprocess.run(['git', 'diff', '--cached', '--check'], check=True)
print('PREPARED_SOURCE_APPLIED: runtime/config/probe/docs; not GPU validation')
