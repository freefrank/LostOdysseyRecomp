"""Build a portable Windows release using an explicit runtime payload allowlist."""
import argparse
import hashlib
import importlib.metadata
import json
import re
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from build_provenance import source_state, read_stamp, validate_formal, validate_staged_binaries

ROOT = Path(__file__).resolve().parents[1]
INSTALLER_ICON = ROOT / 'assets/lost-odyssey-recomp.ico'
DXC_LICENSES = ROOT / 'thirdparty/dxc-licenses'


def sha(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def run(*args, **kwargs):
    return subprocess.check_output(args, cwd=ROOT, text=True, **kwargs).strip()


def validated_dxc_payload(runtime_directory):
    """Keep the exact compiler/validator pair used by the built runtime."""
    provenance = json.loads((DXC_LICENSES / 'PROVENANCE.json').read_text(encoding='utf-8'))
    for name in ('dxcompiler.dll', 'dxil.dll'):
        path = runtime_directory / name
        if not path.is_file() or sha(path) != provenance['files'][name]:
            raise SystemExit(f'Built {name} does not match the validated DXC pair; rebuild or update its provenance and validation.')
    for name, digest in provenance['licenses'].items():
        if sha(DXC_LICENSES / name) != digest:
            raise SystemExit(f'DXC license checksum mismatch: {name}')
    return provenance


def pyinstaller_license_payload():
    # Wheels put COPYING.txt in dist-info/licenses, not in the Python module.
    distribution = importlib.metadata.distribution('pyinstaller')
    files = {f'PyInstaller-{Path(str(file)).name}': Path(distribution.locate_file(file))
             for file in distribution.files or ()
             if Path(str(file)).name.lower().startswith(('copying', 'license'))}
    if not files or any(not file.is_file() for file in files.values()):
        raise SystemExit('Missing installed PyInstaller license files.')
    return files


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
    updater = build / 'LostOdysseyRecomp/LostOdysseyUpdater.exe'
    if not runtime.is_file():
        raise SystemExit('Build the Release runtime with tools/build_release.bat first.')
    if not updater.is_file():
        raise SystemExit('Build the LostOdysseyUpdater release helper before packaging.')
    version_stamp = runtime.parent / 'source-version.txt'
    if not version_stamp.is_file():
        raise SystemExit('Build the runtime to produce its linked source-version.txt before packaging.')
    source_version = version_stamp.read_text(encoding='utf-8').strip()
    if not re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+', source_version):
        raise SystemExit('The linked runtime source version is invalid.')
    output.mkdir(parents=True, exist_ok=True)
    dxc = validated_dxc_payload(runtime.parent)
    state = source_state(ROOT)
    try:
        stamps = [read_stamp(binary, source_version) for binary in (runtime, updater)]
        if len({stamp['source']['identity'] for stamp in stamps}) != 1:
            raise ValueError('Runtime and updater were not built from the same captured source state.')
        normalized_version = validate_formal(ROOT, args.version, source_version, state, stamps)
    except (ValueError, subprocess.CalledProcessError) as error:
        raise SystemExit(f'Release provenance check failed: {error}')
    commit = stamps[0]['source']['commit']
    # Untagged local candidates are development artifacts even from clean source.
    dirty = state['dirty'] or any(stamp['source']['dirty'] for stamp in stamps)
    development = not args.version
    name = 'LostOdysseyRecomp-windows-x64-' + (normalized_version or f'v{source_version}-{commit[:8]}') + ('-dev' if development else '')
    package_zip = output / (name + '.zip')
    if package_zip.exists():
        raise SystemExit(f'Release already exists: {package_zip}')
    with tempfile.TemporaryDirectory(prefix='package-', dir=output) as temporary:
        work = Path(temporary)
        package = work / name
        package.mkdir()
        subprocess.run([sys.executable, '-m', 'PyInstaller', '--noconfirm', '--clean', '--onefile',
                        '--windowed', '--name', 'InstallGame', '--icon', str(INSTALLER_ICON),
                        '--add-data', str(INSTALLER_ICON) + ';.', '--distpath', str(package),
                        '--workpath', str(work / 'freeze'), '--specpath', str(work),
                        str(ROOT / 'tools/installer/installer.py')], cwd=ROOT, check=True)
        shutil.copy2(runtime, package / runtime.name)
        shutil.copy2(updater, package / updater.name)
        validate_staged_binaries([package / runtime.name, package / updater.name], stamps)
        shutil.copy2(ROOT / 'docs/INSTALLING.md', package / 'README.md')
        licenses = package / 'licenses'
        licenses.mkdir()
        shutil.copy2(ROOT / 'LICENSE', licenses / 'LostOdysseyRecomp.txt')
        shutil.copy2(ROOT / 'thirdparty/miniz-UNLICENSE.txt', licenses / 'miniz-UNLICENSE.txt')
        shutil.copy2(ROOT / 'thirdparty/nlohmann-json-LICENSE.txt', licenses / 'nlohmann-json-LICENSE.txt')
        for dll in ('dxcompiler.dll', 'dxil.dll'):
            shutil.copy2(runtime.parent / dll, package / dll)
        shutil.copytree(DXC_LICENSES, licenses / 'DXC')
        dependencies = [ROOT / 'thirdparty/SDL', ROOT / 'thirdparty/plume', ROOT / 'thirdparty/o1heap',
                        ROOT / 'thirdparty/unordered_dense', ROOT / 'thirdparty/smaa', ROOT / 'tools/XenonRecomp',
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
        for file in Path(sys.base_prefix).glob('LICENSE*'):
            shutil.copy2(file, licenses / ('Python-' + file.name))
        for license_name, file in pyinstaller_license_payload().items():
            shutil.copy2(file, licenses / license_name)
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
            'commit': commit, 'build_commit': commit, 'packaging_commit': state['commit'],
            'version': normalized_version, 'source_version': source_version, 'development_build': development,
            'dirty': dirty, 'build_provenance': stamps, 'packaging_source': state,
            'dxc_sha256': dxc['archive_sha256'], 'dxc': dxc,
            'dependencies': dependencies_report,
            'files': {p.relative_to(package).as_posix(): sha(p) for p in package.rglob('*') if p.is_file()},
        }, indent=2), encoding='utf-8')
        shutil.make_archive(str(package_zip.with_suffix('')), 'zip', work, name)
    package_zip.with_suffix('.zip.sha256').write_text(sha(package_zip) + '  ' + package_zip.name + '\n')
    print(package_zip)


if __name__ == '__main__':
    main()
