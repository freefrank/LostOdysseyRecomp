"""AppImage packager collects linuxdeploy output before the temp dir is deleted."""
import importlib.util
import os
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
import sys


ROOT = Path(__file__).resolve().parents[2]


def create_mock_apprun(cmd):
    appdir = Path(cmd[cmd.index('--appdir') + 1])
    entry = appdir / 'AppRun'
    entry.write_bytes(b'mock executable')
    entry.chmod(0o755)


def load_packager():
    spec = importlib.util.spec_from_file_location('package_appimage', ROOT / 'tools/package_appimage.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class PackageAppImageTests(unittest.TestCase):
    def test_collects_appimage_from_linuxdeploy_cwd_before_temp_cleanup(self):
        module = load_packager()
        with tempfile.TemporaryDirectory(prefix='lo-package-appimage-') as tmp:
            base = Path(tmp)
            binaries = base / 'build' / 'LostOdysseyRecomp'
            binaries.mkdir(parents=True)
            (binaries / 'LostOdysseyRecomp').write_bytes(b'mock-runtime')
            (binaries / 'libdxcompiler.so').write_bytes(b'mock-dxc')
            output = base / 'output'
            calls = []

            def fake_linuxdeploy(cmd, **kwargs):
                calls.append(list(cmd))
                self.assertIn('--desktop-file', cmd)
                self.assertIn('--icon-file', cmd)
                self.assertEqual(cmd[cmd.index('--exclude-library') + 1], 'libwayland*')
                if '--output' not in cmd:
                    create_mock_apprun(cmd)
                    return
                cwd = Path(kwargs['cwd'])
                produced = cwd / 'LostOdysseyRecomp-x86_64.AppImage'
                produced.write_bytes(b'successful mock package')
                self.assertTrue(produced.exists())
                self.assertIn('--appdir', cmd)
                self.assertEqual(cmd[cmd.index('--output') + 1], 'appimage')
                # Exclusion must be requested while linuxdeploy resolves
                # dependencies, before this call emits the sealed image.
                self.assertIn('--exclude-library', cmd)
                self.assertEqual(cmd[cmd.index('--exclude-library') + 1], 'libwayland*')

            argv = [
                'package_appimage.py',
                '--build', str(base / 'build'),
                '--output', str(output),
                '--version', 'v0.5.13',
            ]
            with patch.object(sys, 'argv', argv), \
                    patch.object(module.shutil, 'which', return_value='linuxdeploy'), \
                    patch.object(module.subprocess, 'run', side_effect=fake_linuxdeploy):
                module.main()

            destination = output / 'LostOdysseyRecomp-linux-x64-v0.5.13.AppImage'
            checksum = destination.with_suffix('.AppImage.sha256')
            self.assertEqual(len(calls), 2)
            self.assertNotIn('--output', calls[0])
            self.assertIn('--output', calls[1])
            self.assertTrue(destination.is_file())
            self.assertEqual(destination.read_bytes(), b'successful mock package')
            self.assertTrue(checksum.is_file())
            self.assertIn(destination.name, checksum.read_text(encoding='utf-8'))

    def test_relative_linuxdeploy_is_resolved_before_subprocess_cwd_change(self):
        module = load_packager()
        with tempfile.TemporaryDirectory(prefix='lo-relative-linuxdeploy-') as tmp:
            base = Path(tmp)
            binaries = base / 'build' / 'LostOdysseyRecomp'
            binaries.mkdir(parents=True)
            (binaries / 'LostOdysseyRecomp').write_bytes(b'mock-runtime')
            (binaries / 'libdxcompiler.so').write_bytes(b'mock-dxc')
            tool_dir = base / 'out' / 'tools' / 'linuxdeploy'
            tool_dir.mkdir(parents=True)
            if os.name == 'nt':
                tool = tool_dir / 'linuxdeploy.cmd'
                tool.write_text('@echo off\r\n', encoding='ascii')
                relative = r'out\tools\linuxdeploy\linuxdeploy.cmd'
            else:
                tool = tool_dir / 'linuxdeploy'
                tool.write_text('#!/bin/sh\n')
                tool.chmod(0o755)
                relative = 'out/tools/linuxdeploy/linuxdeploy'

            captured = {}

            def fake_linuxdeploy(cmd, **kwargs):
                captured['cmd'] = list(cmd)
                captured['cwd'] = kwargs['cwd']
                if '--output' not in cmd:
                    create_mock_apprun(cmd)
                else:
                    Path(kwargs['cwd'], 'LostOdysseyRecomp-x86_64.AppImage').write_bytes(b'ok')

            argv = [
                'package_appimage.py',
                '--build', str(base / 'build'),
                '--output', str(base / 'output'),
                '--version', 'v0.5.13',
                '--linuxdeploy', relative,
            ]
            previous = Path.cwd()
            try:
                os.chdir(base)
                self.assertEqual(module.shutil.which(relative), relative.replace('\\', os.sep)
                                 if os.name == 'nt' else relative)
                with patch.object(sys, 'argv', argv), \
                        patch.object(module.subprocess, 'run', side_effect=fake_linuxdeploy):
                    module.main()
            finally:
                os.chdir(previous)

            self.assertTrue(Path(captured['cmd'][0]).is_absolute())
            self.assertEqual(Path(captured['cmd'][0]).resolve(), tool.resolve())
            self.assertNotEqual(Path(captured['cwd']).resolve(), base.resolve())
            destination = base / 'output' / 'LostOdysseyRecomp-linux-x64-v0.5.13.AppImage'
            self.assertTrue(destination.is_file())

    def test_relative_linuxdeploy_executable_survives_real_subprocess_cwd_change(self):
        module = load_packager()
        with tempfile.TemporaryDirectory(prefix='lo-relative-linuxdeploy-run-') as tmp:
            base = Path(tmp)
            binaries = base / 'build' / 'LostOdysseyRecomp'
            binaries.mkdir(parents=True)
            (binaries / 'LostOdysseyRecomp').write_bytes(b'mock-runtime')
            (binaries / 'libdxcompiler.so').write_bytes(b'mock-dxc')
            tool_dir = base / 'out' / 'tools' / 'linuxdeploy'
            tool_dir.mkdir(parents=True)
            if os.name == 'nt':
                tool = tool_dir / 'linuxdeploy.cmd'
                tool.write_text(
                    '@echo off\r\n'
                    'echo mock executable>"%~2\\AppRun"\r\n'
                    'if not "%~9"=="--output" exit /b 0\r\n'
                    'echo successful mock package>LostOdysseyRecomp-x86_64.AppImage\r\n',
                    encoding='ascii')
                relative = r'out\tools\linuxdeploy\linuxdeploy.cmd'
            else:
                tool = tool_dir / 'linuxdeploy'
                tool.write_text("#!/bin/sh\nprintf '%s' 'mock executable' > \"$2/AppRun\"\n"
                                "chmod +x \"$2/AppRun\"\n"
                                '[ "$9" = "--output" ] || exit 0\n'
                                "printf '%s' 'successful mock package' > LostOdysseyRecomp-x86_64.AppImage\n")
                tool.chmod(0o755)
                relative = 'out/tools/linuxdeploy/linuxdeploy'

            argv = [
                'package_appimage.py',
                '--build', str(base / 'build'),
                '--output', str(base / 'output'),
                '--version', 'v0.5.13',
                '--linuxdeploy', relative,
            ]
            previous = Path.cwd()
            try:
                os.chdir(base)
                with patch.object(sys, 'argv', argv):
                    module.main()
            finally:
                os.chdir(previous)

            destination = base / 'output' / 'LostOdysseyRecomp-linux-x64-v0.5.13.AppImage'
            self.assertTrue(destination.is_file())
            self.assertTrue(destination.stat().st_size > 0)
            self.assertTrue(tool.exists())
            self.assertIn('successful mock package', destination.read_text(encoding='utf-8'))

    def test_missing_entry_stops_before_output_plugin(self):
        module = load_packager()
        with tempfile.TemporaryDirectory(prefix='lo-missing-apprun-') as tmp:
            base = Path(tmp)
            binaries = base / 'build/LostOdysseyRecomp'
            binaries.mkdir(parents=True)
            (binaries / 'LostOdysseyRecomp').write_bytes(b'mock-runtime')
            (binaries / 'libdxcompiler.so').write_bytes(b'mock-dxc')
            output = base / 'output'
            argv = ['package_appimage.py', '--build', str(base / 'build'),
                    '--output', str(output), '--version', 'v0.5.14']
            with patch.object(sys, 'argv', argv), \
                    patch.object(module.shutil, 'which', return_value='linuxdeploy'), \
                    patch.object(module.subprocess, 'run') as run:
                with self.assertRaisesRegex(SystemExit, 'Invalid AppRun entry'):
                    module.main()
            self.assertEqual(run.call_count, 1)
            self.assertNotIn('--output', run.call_args.args[0])
            self.assertEqual(list(output.glob('*.AppImage*')), [])

    @unittest.skipIf(os.name == 'nt', 'POSIX symlink and executable permissions')
    def test_entry_validation_rejects_broken_escaping_and_nonexecutable_targets(self):
        module = load_packager()
        with tempfile.TemporaryDirectory(prefix='lo-invalid-apprun-') as tmp:
            base = Path(tmp)
            appdir = base / 'AppDir'
            appdir.mkdir()
            entry = appdir / 'AppRun'
            entry.symlink_to('missing')
            with self.assertRaisesRegex(SystemExit, 'Invalid AppRun entry'):
                module.validate_apprun(appdir)
            entry.unlink()
            outside = base / 'external'
            outside.write_bytes(b'executable')
            outside.chmod(0o755)
            entry.symlink_to(outside)
            with self.assertRaisesRegex(SystemExit, 'inside the AppDir'):
                module.validate_apprun(appdir)
            entry.unlink()
            target = appdir / 'runtime'
            target.write_bytes(b'executable')
            target.chmod(0o644)
            entry.symlink_to('runtime')
            with self.assertRaisesRegex(SystemExit, 'not executable'):
                module.validate_apprun(appdir)
            target.chmod(0o755)
            module.validate_apprun(appdir)


    def test_dlss_runtime_bundled_with_license_and_symlinks(self):
        module = load_packager()
        with tempfile.TemporaryDirectory(prefix='lo-dlss-appimage-') as tmp:
            base = Path(tmp)
            binaries = base / 'build' / 'LostOdysseyRecomp'
            binaries.mkdir(parents=True)
            (binaries / 'LostOdysseyRecomp').write_bytes(b'mock-runtime')
            (binaries / 'libdxcompiler.so').write_bytes(b'mock-dxc')
            (binaries / 'libnvidia-ngx-dlss.so.310.9.1').write_bytes(b'mock-dlss-so')
            output = base / 'output'

            argv = [
                'package_appimage.py',
                '--build', str(base / 'build'),
                '--output', str(output),
                '--version', 'v0.6.11',
                '--dry-layout',
            ]

            # 1. Missing DLSS license triggers exit when neither candidate path exists
            with patch.object(module, 'ROOT', base / 'fake_root'):
                with patch.object(sys, 'argv', argv):
                    with self.assertRaisesRegex(SystemExit, 'DLSS SDK license is missing'):
                        module.main()

            # 2. Provide mock DLSS SDK license in candidate path under module.ROOT
            fake_root = base / 'fake_root'
            sdk_dir = fake_root / 'out/deps/nvidia-dlss'
            sdk_dir.mkdir(parents=True, exist_ok=True)
            (sdk_dir / 'LICENSE.txt').write_text('mock license', encoding='utf-8')

            with patch.object(module, 'ROOT', fake_root):
                with patch.object(sys, 'argv', argv):
                    # Dry layout creates layout and prints paths without linuxdeploy
                    module.main()


if __name__ == '__main__':
    unittest.main(verbosity=2)
