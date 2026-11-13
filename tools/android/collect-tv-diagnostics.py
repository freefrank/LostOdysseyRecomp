#!/usr/bin/env python3
"""Collect TV evidence without changing device configuration or requiring root."""
import argparse
from pathlib import Path
import subprocess
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--adb', default='adb')
    parser.add_argument('--serial')
    parser.add_argument('--game', action='store_true', help='collect the runtime log instead of standalone TV probe')
    parser.add_argument('--out', type=Path, default=Path('tv-diagnostics.zip'))
    args = parser.parse_args()
    adb = [args.adb] + (['-s', args.serial] if args.serial else [])
    package = 'io.github.freefrank.lostodyssey' + ('' if args.game else '.tvprobe')
    commands = {
        'display.txt': ['shell', 'dumpsys', 'display'],
        'window.txt': ['shell', 'dumpsys', 'window', 'displays'],
        'surface-layers.txt': ['shell', 'dumpsys', 'SurfaceFlinger', '--list'],
        'input.txt': ['shell', 'dumpsys', 'input'],
        'audio.txt': ['shell', 'dumpsys', 'audio'],
        'audio-flinger.txt': ['shell', 'dumpsys', 'media.audio_flinger'],
        'logcat-tv.txt': ['logcat', '-d', '-v', 'threadtime', '-s', 'LOTV:I', 'SDL:I', '*:S'],
        'tv-display.log': ['exec-out', 'run-as', package, 'cat', '/sdcard/Android/data/' + package + '/files/logs/tv-display.log'],
    }
    with zipfile.ZipFile(args.out, 'w', zipfile.ZIP_DEFLATED) as archive:
        for name, command in commands.items():
            result = subprocess.run(adb + command, capture_output=True, timeout=30)
            archive.writestr(name, result.stdout)
            if result.returncode:
                archive.writestr(name + '.error.txt', result.stderr or b'Command failed; no evidence collected.')
    print(args.out.resolve())


if __name__ == '__main__':
    main()
