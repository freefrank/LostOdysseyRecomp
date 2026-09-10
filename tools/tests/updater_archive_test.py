"""Focused staging regression; uses the same ZIP writer as package_release.py."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import zipfile


def main():
    executable = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='lo-updater-archive-') as temporary:
        work = Path(temporary)
        package = work / 'LostOdysseyRecomp-test'
        (package / 'bin').mkdir(parents=True)
        payload = {'LostOdysseyRecomp.exe': b'synthetic executable',
                   'bin/runtime.dll': b'synthetic runtime'}
        for name, data in payload.items():
            (package / name).write_bytes(data)
        manifest = {'version': '9.9.9', 'development_build': False,
                    'files': {name: hashlib.sha256(data).hexdigest()
                              for name, data in payload.items()}}
        (package / 'manifest.json').write_text(json.dumps(manifest), encoding='utf-8')
        release = Path(shutil.make_archive(str(work / 'release'), 'zip', work, package.name))
        with zipfile.ZipFile(release) as archive:
            entries = [(entry.filename, archive.read(entry)) for entry in archive.infolist()]
        assert entries[0][0] == package.name + '/'

        checks = 0

        def check(label, contents=None, error=None):
            nonlocal checks
            archive = release
            if contents is not None:
                archive = work / f'{label}.zip'
                with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as output:
                    for name, data in contents:
                        output.writestr(name, data)
            operation = work / label
            result = subprocess.run([str(executable), '--stage-archive', str(archive),
                                     str(operation), '9.9.9'], capture_output=True, text=True)
            if error is None:
                assert result.returncode == 0, (label, result.stderr)
                for name, data in payload.items():
                    assert (operation / 'stage' / name).read_bytes() == data, label
            else:
                assert result.returncode != 0 and error in result.stderr, (label, result.stderr)
            checks += 1
            print(f'PASS: {label}')

        check('release-shutil-root-directory')
        check('implicit-root', [(n, d) for n, d in entries if not n.endswith('/')])
        check('root-directory-last', entries[1:] + entries[:1])
        check('multiple-directory-roots', entries + [('other/', b'')], 'multiple package roots')
        check('top-level-file', [('stray.txt', b'x')] + entries, 'one package root directory')
        check('absolute-root', [('/' + n, d) for n, d in entries], 'one package root directory')
        check('parent-root', [(n.replace(package.name, '..', 1), d) for n, d in entries],
              'unsafe package root')
        check('traversal', entries + [(package.name + '/../escape.dll', b'x')], 'unsafe component')
        check('duplicate-payload', entries + [(package.name + '/LOSTODYSSEYRECOMP.EXE', b'x')],
              'duplicate payload paths')
        check('unlisted-payload', entries + [(package.name + '/extra.dll', b'x')], 'manifest allowlist')
        check('hash-mismatch', [(n, b'corrupt' if n.endswith('runtime.dll') else d)
                                for n, d in entries], 'SHA256 mismatch')
        check('missing-manifest', [(n, d) for n, d in entries if not n.endswith('manifest.json')],
              'root manifest.json')
        print(f'Archive staging: {checks} checks, 0 failures')


if __name__ == '__main__':
    main()
