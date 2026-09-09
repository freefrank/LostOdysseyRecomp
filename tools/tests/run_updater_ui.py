"""Build one direct updater UI fixture and render on its private inactive desktop."""
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
SOURCES = (
    'LostOdysseyRecomp/updater/progress.cpp',
    'LostOdysseyRecomp/updater/progress.h',
    'LostOdysseyRecomp/settings/window_chrome.h',
    'LostOdysseyRecomp/settings/desktop_ui.h',
    'tools/tests/updater_ui_test.cpp',
)


def hashes():
    return {path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest() for path in SOURCES}


def compiler_environment():
    # cmd's Unicode output preserves non-ASCII environment values independently
    # of the Python UTF-8 mode and the current console code page.
    result = subprocess.run(
        ['cmd.exe', '/d', '/u', '/s', '/c', 'call tools\\setup_windows.bat >nul && set'],
        cwd=ROOT, capture_output=True, check=True)
    return dict(line.split('=', 1) for line in result.stdout.decode('utf-16-le').splitlines()
                if '=' in line and not line.startswith('='))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'out/v0.5.0/ui-modernization/updater')
    parser.add_argument('--render-only', action='store_true', help='refresh actual UI captures without repeating functional checks')
    parser.add_argument('--render-narrow-only', action='store_true', help='refresh only the affected narrow Chinese capture')
    args = parser.parse_args()
    render_only = args.render_only or args.render_narrow_only
    output = args.output.resolve()
    build = output / 'fixture-build'
    build.mkdir(parents=True, exist_ok=True)
    captured = hashes()
    environment = compiler_environment()
    compiler = shutil.which('clang-cl', path=next(v for k, v in environment.items() if k.upper() == 'PATH'))
    if not compiler:
        raise RuntimeError('clang-cl unavailable')
    executable = build / 'updater_ui_test.exe'
    command = [compiler, '/nologo', '/std:c++20', '/EHsc', '/O2', '/MT', '/W4', '/WX', '/utf-8',
               '/I' + str(ROOT / 'LostOdysseyRecomp'), str(ROOT / SOURCES[0]), str(ROOT / SOURCES[-1]),
               '/Fo' + str(build) + '/', '/Fe' + str(executable), '/link', 'user32.lib', 'gdi32.lib']
    compiled = subprocess.run(command, cwd=ROOT, env=environment, capture_output=True,
                              text=True, encoding='utf-8', errors='replace')
    (output / 'compile.log').write_text(compiled.stdout + compiled.stderr, encoding='utf-8')
    (output / 'compile-command.json').write_text(json.dumps(command, indent=2), encoding='utf-8')
    compiled.check_returncode()
    if hashes() != captured:
        raise RuntimeError('UI source changed while compiling; coordinate ownership before running')
    user32 = ctypes.windll.user32
    user32.GetForegroundWindow.restype = wintypes.HWND
    user32.OpenDesktopW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    user32.OpenDesktopW.restype = wintypes.HANDLE
    user32.CloseDesktop.argtypes = [wintypes.HANDLE]
    foreground = user32.GetForegroundWindow()
    fixture_command = [str(executable), str(output)]
    if args.render_narrow_only:
        fixture_command.append('--render-narrow-only')
    elif render_only:
        fixture_command.append('--render-only')
    result = subprocess.run(fixture_command, cwd=output, capture_output=True,
                            text=True, creationflags=subprocess.CREATE_NO_WINDOW)
    log_name = 'render-narrow.log' if args.render_narrow_only else 'render.log' if render_only else 'fixture.log'
    (output / log_name).write_text(result.stdout + result.stderr, encoding='utf-8')
    match = re.search(r'desktop=(LoUpdaterUi-\d+)', result.stdout)
    remaining = user32.OpenDesktopW(match[1], 0, False, 1) if match else None
    if remaining:
        user32.CloseDesktop(remaining)
    manifest = {'source_sha256': captured, 'exit_code': result.returncode,
                'binary_sha256': hashlib.sha256(executable.read_bytes()).hexdigest(),
                'private_desktop': match[1] if match else None,
                'private_desktop_released_after_exit': bool(match) and not bool(remaining),
                'foreground_unchanged': user32.GetForegroundWindow() == foreground,
                'game_started': False, 'network_requests': False, 'synthetic_ui_values': True,
                'functional_checks_run': not render_only,
                'narrow_only': args.render_narrow_only,
                'render_method': 'WM_PRINT with PRF_CLIENT/PRF_CHILDREN on a private inactive desktop; no SwitchDesktop call'}
    result.check_returncode()
    if not manifest['private_desktop_released_after_exit']:
        raise RuntimeError('Private desktop cleanup was not established')
    from PIL import Image
    manifest['previews'] = {}
    pattern = 'updater-narrow-zh.bmp' if args.render_narrow_only else 'updater-*.bmp'
    for bitmap in sorted(output.glob(pattern)):
        png = bitmap.with_suffix('.png')
        with Image.open(bitmap) as image:
            image.save(png)
            manifest['previews'][png.name] = {'dimensions': image.size,
                                             'sha256': hashlib.sha256(png.read_bytes()).hexdigest()}
    manifest_name = 'render-narrow-manifest.json' if args.render_narrow_only else 'render-manifest.json' if render_only else 'manifest.json'
    (output / manifest_name).write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    main()
