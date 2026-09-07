"""Exercise runtime path decoding without loading or operating the game."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def main():
    # Windows CI may redirect stdout using a legacy code page.
    sys.stdout.reconfigure(encoding="utf-8")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('runtime', type=Path)
    args = parser.parse_args()
    runtime = args.runtime.resolve(strict=True)
    root = Path(tempfile.mkdtemp(prefix='lo-startup-path-')).resolve()
    print(f'Fixtures and logs: {root}')
    env = dict(os.environ, LO_HEADLESS='1', LO_LOG_FILE='runtime.log')
    for name in ('ascii', 'acute\u00b4game', 'prime\u2032game', '\u5b58\u6863 game'):
        directory = root / name
        game = directory / 'game' / 'disc1'
        game.mkdir(parents=True)
        # A readable, intentionally invalid image must reach parsing, then exit.
        # No original assets, guest execution, renderer, or user saves are used.
        (game / 'default.xex').write_bytes(b'Unicode path probe - not an XEX image')
        command = [str(runtime), '--game', str(game)]
        for implicit in (False, True):
            if implicit:
                executable = directory / runtime.name
                shutil.copy2(runtime, executable)
                command = [str(executable)]
            result = subprocess.run(command, cwd=directory, env=env, timeout=30,
                                    capture_output=True,
                                    creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
            log_path = directory / 'runtime.log'
            log = log_path.read_text(encoding='utf-8', errors='strict')
            log_path.rename(directory / ('implicit.log' if implicit else 'explicit.log'))
            assert result.returncode == 1, (name, implicit, result.returncode)
            assert f'game root: {game}' in log, (name, implicit, 'changed game path', log)
            assert 'failed to parse XEX image' in log, (name, implicit, 'file did not reach parser', log)
            assert 'failed to read' not in log, (name, implicit, 'file read failed')
            print(f'PASS: {name} / {"implicit" if implicit else "--game"}')


if __name__ == '__main__':
    main()
