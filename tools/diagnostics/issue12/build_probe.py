"""Compile the Issue #12 probe overlay and link it with the frozen bridge inputs (never runs the game)."""
from pathlib import Path
import hashlib, json, re, shutil, subprocess, sys, time

OUT = Path(__file__).resolve().parent
TRIAGE = OUT.parent
ROOT = TRIAGE.parents[1]
BRIDGE = TRIAGE / 'bridge'
FROZEN = BRIDGE / 'frozen-current-build'
SEM = ROOT / 'out/issue7-semantics-fix/build'
VARIANT = sys.argv[1] if len(sys.argv) > 1 else 'default'
BIN = OUT / ('bin' if VARIANT == 'default' else 'bin-' + VARIANT)
SRC = OUT / 'issue12_probe.cpp'

def sha(p):
    with Path(p).open('rb') as f: return hashlib.file_digest(f, 'sha256').hexdigest()

def main():
    inputs = json.loads((BRIDGE / 'inputs.json').read_text())
    for name, expected in inputs['frozen_link_inputs'].items():
        assert sha(FROZEN / name) == expected, 'frozen input changed: ' + name
    assert sha(SEM / 'guest-fixed.lib') == inputs['guest_library_sha256']
    def compiler_environment():
        # Same discovery as tools/tests/run.py, but tolerant of the console code page.
        result = subprocess.run(['cmd.exe', '/d', '/s', '/c', 'call tools\setup_windows.bat >nul && set'],
                                cwd=ROOT, capture_output=True, check=True)
        text = result.stdout.decode('utf-8', errors='replace')
        return dict(line.split('=', 1) for line in text.splitlines() if '=' in line and not line.startswith('='))
    env = compiler_environment()
    search = next(v for k, v in env.items() if k.upper() == 'PATH')
    tool = lambda name: shutil.which(name, path=search)
    BIN.mkdir(exist_ok=True)
    compile_record = json.loads((BRIDGE / 'compile.json').read_text())
    command = list(compile_record['command'])
    command[0] = tool('clang-cl')
    base_command = [x for x in command if x != '/showIncludes']
    assert base_command[-2] == '--'
    extra_objects = []
    sources = [SRC] + ([OUT / 'gc_render_flush.cpp'] if VARIANT == 'gcflush' else [])
    for index, source in enumerate(sources):
        unit_obj = BIN / (source.name + '.obj')
        unit_command = [('/Fo' + str(unit_obj)) if x.startswith('/Fo') else x for x in base_command]
        if VARIANT == 'gcflush': unit_command.insert(1, '-DISSUE12_EXTERNAL_GC_HOOKS')
        unit_command[-1] = str(source)
        started = time.monotonic()
        with (BIN / ('compile.log' if index == 0 else 'compile-%d.log' % index)).open('w', encoding='utf-8') as log:
            result = subprocess.run(unit_command, cwd=OUT, env=env, stdout=log, stderr=subprocess.STDOUT)
        (BIN / ('compile.json' if index == 0 else 'compile-%d.json' % index)).write_text(json.dumps(dict(command=unit_command, returncode=result.returncode, elapsed_seconds=time.monotonic() - started), indent=2))
        if result.returncode: raise RuntimeError('Compile failed; inspect ' + str(BIN) + ' compile logs')
        if index == 0: obj = unit_obj
        else: extra_objects.append(unit_obj)
    structure = json.loads((BRIDGE / 'current-link-structure.json').read_text())
    libraries = [str(FROZEN / n) if (FROZEN / n).is_file() else n for n in structure['libraries']]
    libraries = [str(SEM / 'guest-fixed.lib') if x == str(FROZEN / 'LostOdysseyRecompLib/LostOdysseyRecompLib.lib') else x for x in libraries]
    assert str(SEM / 'guest-fixed.lib') in libraries
    exe = BIN / 'LostOdysseyRecomp.exe'
    objects = list(structure['objects'])
    if VARIANT in ('nopoll', 'gcflush'):
        # v0.4.2 behaviour for guest polling loops: no host poll_wait hooks, plain yield on Sleep(0).
        objects = [o for o in objects if not o.endswith('cpu\poll_wait.cpp.obj')]
        assert len(objects) == len(structure['objects']) - 1
    args = ['/nologo', *(str(FROZEN / n) for n in objects), str(obj), *(str(o) for o in extra_objects), '/out:' + str(exe),
        '/implib:' + str(BIN / 'LostOdysseyRecomp.lib'), '/pdb:' + str(BIN / 'LostOdysseyRecomp.pdb'), '/version:0.0',
        '/machine:x64', '/INCREMENTAL:NO', '/subsystem:console', '/SUBSYSTEM:WINDOWS', '/ENTRY:mainCRTStartup',
        '/MAP:' + str(BIN / 'LostOdysseyRecomp.map'), *libraries]
    rsp = BIN / 'link.rsp'
    rsp.write_text('\n'.join(subprocess.list2cmdline([a]) for a in args), encoding='utf-8')
    (BIN / 'intermediate').mkdir(exist_ok=True)
    command = [tool('cmake'), '-E', 'vs_link_exe', '--msvc-ver=1944', '--intdir=' + str(BIN / 'intermediate'),
        '--rc=' + tool('rc'), '--mt=' + tool('llvm-mt'), '--manifests', '--', tool('lld-link'), '@' + str(rsp)]
    started = time.monotonic()
    with (BIN / 'link.log').open('w', encoding='utf-8') as log:
        result = subprocess.run(command, cwd=BIN, env=env, stdout=log, stderr=subprocess.STDOUT)
    (BIN / 'link.json').write_text(json.dumps(dict(command=command, returncode=result.returncode, elapsed_seconds=time.monotonic() - started), indent=2))
    if result.returncode: raise RuntimeError('Link failed; inspect bin/link.log')
    artifact = dict(diagnostic='issue12-lifetime-probe-r1-' + VARIANT, variant=VARIANT, official_0_4_2=False, source_version_embedded='0.5.1',
        purpose='Record 0xE0 alloc/free and UObject purge history; validate scene-proxy material records before DrawDynamicElements',
        environment=['LO_ISSUE12_PROBE_FILE', 'LO_ISSUE12_SKIP_STALE'], hooks=['sub_82295B08', 'sub_82298990', 'sub_823CB350'] + (['sub_8249A568', 'sub_822FD0A8 (gc_render_flush)'] if VARIANT == 'gcflush' else []),
        files={p.name: sha(p) for p in (exe, BIN / 'LostOdysseyRecomp.map', obj, *extra_objects)}, probe_source_sha256=sha(SRC),
        guest_library_sha256=sha(SEM / 'guest-fixed.lib'), linked_frozen_objects=len(objects), compiled_host_units=1, rebuilt_guest_units=0)
    (BIN / 'artifact.json').write_text(json.dumps(artifact, indent=2))
    print(json.dumps(artifact, indent=2))

if __name__ == '__main__':
    main()
