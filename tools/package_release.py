"""Build a portable Windows release using an explicit runtime payload allowlist."""
import argparse
import hashlib
import json
import re
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
DXC_URL = 'https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.8.2505.1/dxc_2025_07_14.zip'
DXC_SHA = '9ad895a6b039e3a8f8c22a1009f866800b840a74b50db9218d13319e215ea8a4'


def sha(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def run(*args, **kwargs):
    return subprocess.check_output(args, cwd=ROOT, text=True, **kwargs).strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=ROOT / 'out/build/release')
    parser.add_argument('--output', type=Path, default=ROOT / 'out/releases')
    parser.add_argument('--version', default='')
    args = parser.parse_args()
    if args.version and not re.fullmatch(r'v[0-9]+\.[0-9]+(?:\.[0-9]+)?', args.version):
        raise SystemExit('Version must have the form v0.1 or v0.1.0.')
    build, output = args.build.resolve(), args.output.resolve()
    runtime = build / 'LostOdysseyRecomp/LostOdysseyRecomp.exe'
    if not runtime.is_file():
        raise SystemExit('Build the Release runtime with tools/build_release.bat first.')
    output.mkdir(parents=True, exist_ok=True)
    dxc = output / 'dxc.zip'
    if not dxc.exists() or sha(dxc) != DXC_SHA:
        urllib.request.urlretrieve(DXC_URL, dxc)
    if sha(dxc) != DXC_SHA:
        raise SystemExit('DXC download checksum mismatch')
    commit = run('git', 'rev-parse', 'HEAD')
    dirty = bool(run('git', 'diff', '--name-only', '--ignore-submodules'))
    if args.version and dirty:
        raise SystemExit('Versioned releases require a clean source checkout.')
    name = 'LostOdysseyRecomp-windows-x64-' + (args.version or commit[:8]) + ('-dev' if dirty else '')
    package_zip = output / (name + '.zip')
    if package_zip.exists():
        raise SystemExit(f'Release already exists: {package_zip}')
    with tempfile.TemporaryDirectory(prefix='package-', dir=output) as temporary:
        work = Path(temporary)
        package = work / name
        package.mkdir()
        subprocess.run([sys.executable, '-m', 'PyInstaller', '--noconfirm', '--clean', '--onefile',
                        '--windowed', '--name', 'InstallGame', '--distpath', str(package),
                        '--workpath', str(work / 'freeze'), '--specpath', str(work),
                        str(ROOT / 'tools/installer/installer.py')], cwd=ROOT, check=True)
        shutil.copy2(runtime, package / runtime.name)
        shutil.copy2(ROOT / 'docs/INSTALLING.md', package / 'README.md')
        licenses = package / 'licenses'
        licenses.mkdir()
        shutil.copy2(ROOT / 'LICENSE', licenses / 'LostOdysseyRecomp.txt')
        with zipfile.ZipFile(dxc) as archive:
            for dll in ('dxcompiler.dll', 'dxil.dll'):
                matches = [n for n in archive.namelist() if n.lower().replace('\\', '/').endswith('bin/x64/' + dll)]
                if len(matches) != 1:
                    raise SystemExit(f'DXC package missing x64 {dll}')
                (package / dll).write_bytes(archive.read(matches[0]))
            for file in archive.namelist():
                if Path(file).name.lower().startswith(('license', 'notice')) and not file.endswith('/'):
                    (licenses / ('DXC-' + Path(file).name)).write_bytes(archive.read(file))
        dependencies = [ROOT / 'thirdparty/SDL', ROOT / 'thirdparty/plume', ROOT / 'thirdparty/o1heap',
                        ROOT / 'thirdparty/unordered_dense', ROOT / 'tools/XenonRecomp',
                        build / '_deps/lo_ffmpeg-src']
        for directory in dependencies:
            found = []
            for file in directory.rglob('*'):
                if file.is_file() and file.name.lower().startswith(('license', 'copying', 'copyright')):
                    rel = file.relative_to(directory)
                    destination = licenses / directory.name / rel
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(file, destination)
                    found.append(file)
            if not found:
                raise SystemExit(f'Missing dependency license files: {directory.name}')
        # Python and Tk are embedded in the one-file importer.
        import PyInstaller
        frozen_root = Path(PyInstaller.__file__).parent
        for directory, label in ((Path(sys.base_prefix), 'Python'), (frozen_root, 'PyInstaller')):
            for file in directory.glob('LICENSE*'):
                shutil.copy2(file, licenses / (label + '-' + file.name))
        for file in (Path(sys.base_prefix) / 'tcl').glob('*/license*'):
            shutil.copy2(file, licenses / ('TclTk-' + file.parent.name + '-' + file.name))
        # Fail packaging if a runtime dependency would require the developer's PATH.
        import pefile
        import os
        system = Path(os.environ['SystemRoot']) / 'System32'
        dependencies_report = {}
        for binary in package.glob('*'):
            if binary.suffix.lower() not in ('.exe', '.dll'):
                continue
            pe = pefile.PE(str(binary))
            imported = [e.dll.decode() for e in getattr(pe, 'DIRECTORY_ENTRY_IMPORT', [])]
            imported += [e.dll.decode() for e in getattr(pe, 'DIRECTORY_ENTRY_DELAY_IMPORT', [])]
            pe.close()
            dependencies_report[binary.name] = imported
            for dll in imported:
                # MSVC redist is not guaranteed on a clean Windows install.
                redist = dll.lower().startswith(('vcruntime', 'msvcp', 'concrt'))
                if (package / dll).exists():
                    continue
                if redist or not (dll.lower().startswith(('api-ms-', 'ext-ms-')) or (system / dll).exists()):
                    raise SystemExit(f'Unbundled dependency: {binary.name} -> {dll}')
        (package / 'manifest.json').write_text(json.dumps({
            'commit': commit, 'version': args.version, 'development_build': dirty, 'dxc_sha256': DXC_SHA,
            'dependencies': dependencies_report,
            'files': {p.relative_to(package).as_posix(): sha(p) for p in package.rglob('*') if p.is_file()},
        }, indent=2), encoding='utf-8')
        shutil.make_archive(str(package_zip.with_suffix('')), 'zip', work, name)
    package_zip.with_suffix('.zip.sha256').write_text(sha(package_zip) + '  ' + package_zip.name + '\n')
    print(package_zip)


if __name__ == '__main__':
    main()
