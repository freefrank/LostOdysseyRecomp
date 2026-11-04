"""Check the portable Windows ZIP payload without launching the runtime."""
import contextlib
import io
import json
import os
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest.mock import patch
import zipfile


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import package_release as release  # noqa: E402


class PackageReleaseTests(unittest.TestCase):
    def test_zip_contains_only_the_five_steam_artwork_files_with_manifest_hashes(self):
        with tempfile.TemporaryDirectory(prefix='lo-windows-package-') as temporary:
            root = Path(temporary)
            build = root / 'build'
            runtime = build / 'LostOdysseyRecomp'
            runtime.mkdir(parents=True)
            for filename in ('LostOdysseyRecomp.exe', 'dxcompiler.dll', 'dxil.dll'):
                (runtime / filename).write_bytes(b'fixture binary')
            (runtime / 'source-version.txt').write_text('1.0.6', encoding='utf-8')
            sources = (
                'docs/INSTALLING.md', 'LICENSE', 'thirdparty/miniz-UNLICENSE.txt',
                'thirdparty/licenses/XeGTAO.txt', 'thirdparty/licenses/SnapdragonGSR.txt',
                'thirdparty/signalsmith-stretch/LICENSE.txt',
                'thirdparty/signalsmith-stretch/signalsmith-linear/LICENSE.txt',
                'thirdparty/nlohmann-json-LICENSE.txt', 'thirdparty/lzokay/LICENSE',
                'LostOdysseyRecomp/install/FONT-PROVENANCE.md',
                'thirdparty/SDL/test/unifont-15.1.05-license.txt',
                'tools/modding/mo2/game_lostodysseyrecomp.py',
                'tools/modding/mo2/lostodysseyrecomp_export.py',
                'thirdparty/dxc-licenses/LICENSE.txt',
            )
            for relative in sources:
                path = root / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b'fixture source')
            for relative in ('thirdparty/SDL', 'thirdparty/plume', 'thirdparty/o1heap',
                             'thirdparty/unordered_dense', 'thirdparty/smaa',
                             'tools/XenonRecomp', 'build/_deps/lo_ffmpeg-src'):
                path = root / relative / 'LICENSE'
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b'fixture dependency license')
            artwork = root / 'assets/steam'
            artwork.mkdir(parents=True)
            filenames = ('portrait.png', 'header.png', 'hero.png', 'logo.png', 'icon.png')
            for filename in filenames:
                (artwork / filename).write_bytes((ROOT / 'assets/steam' / filename).read_bytes())
            (artwork / 'private.txt').write_text('not in the package', encoding='utf-8')
            output = root / 'output'
            fake_pefile = types.SimpleNamespace(PE=lambda _: types.SimpleNamespace(close=lambda: None))
            argv = ['package_release.py', '--build', str(build), '--output', str(output)]
            with patch.object(release, 'ROOT', root), \
                    patch.object(release, 'DXC_LICENSES', root / 'thirdparty/dxc-licenses'), \
                    patch.object(release, 'run', return_value='a' * 40), \
                    patch.object(release, 'stage_shader_pack_license'), \
                    patch.dict(sys.modules, {'pefile': fake_pefile}), \
                    patch.dict(os.environ, {'SystemRoot': str(root / 'Windows')}), \
                    patch.object(sys, 'argv', argv), contextlib.redirect_stdout(io.StringIO()):
                release.main()
            archive = next(output.glob('*.zip'))
            with zipfile.ZipFile(archive) as package:
                prefix = archive.stem + '/'
                art_paths = {prefix + 'assets/steam/' + filename for filename in filenames}
                self.assertEqual({name for name in package.namelist() if '/assets/steam/' in name
                                  and not name.endswith('/')}, art_paths)
                manifest = json.loads(package.read(prefix + 'manifest.json'))
                for filename in filenames:
                    relative = 'assets/steam/' + filename
                    payload = package.read(prefix + relative)
                    self.assertEqual(payload, (artwork / filename).read_bytes())
                    self.assertEqual(manifest['files'][relative], release.hashlib.sha256(payload).hexdigest())


if __name__ == '__main__':
    unittest.main()
