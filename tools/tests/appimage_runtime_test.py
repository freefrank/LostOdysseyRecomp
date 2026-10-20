"""Exercise the real AppImage runtime with a harmless, argument-reporting AppRun."""
import argparse
import concurrent.futures
import json
import os
import signal
from pathlib import Path
import subprocess
import tempfile
import time


PAYLOAD = r'''#!/usr/bin/env python3
import json, os, pathlib, sys, time
root = pathlib.Path(os.environ['APPDIR'])
mounted = any(line.split()[4].replace(r'\040', ' ') == str(root) and ' - fuse' in line
              for line in pathlib.Path('/proc/self/mountinfo').read_text().splitlines())
print(json.dumps(dict(args=sys.argv[1:], APPIMAGE=os.environ['APPIMAGE'],
    ARGV0=os.environ['ARGV0'], APPDIR=str(root), OWD=os.environ.get('OWD'),
    mode=root.stat().st_mode & 0o777, mounted=mounted)), flush=True)
time.sleep(30 if sys.argv[1:] == ['--wait'] else 0.2)
sys.exit(37 if sys.argv[1:] else 0)
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--mode', choices=('no-fuse', 'mount'), required=True)
    args = parser.parse_args()
    runtime = args.runtime.resolve()
    with tempfile.TemporaryDirectory(prefix='lo-runtime-test-') as temporary:
        base = Path(temporary)
        appdir = base / 'AppDir'
        appdir.mkdir()
        (appdir / 'AppRun').write_text(PAYLOAD)
        (appdir / 'AppRun').chmod(0o755)
        squashfs = base / 'payload.squashfs'
        subprocess.run(['mksquashfs', str(appdir), str(squashfs), '-noappend',
                        '-comp', 'zstd', '-quiet', '-processors', '2'], check=True)
        image = base / 'image with spaces 中文.AppImage'
        with image.open('wb') as output:
            output.write(runtime.read_bytes())
            output.write(squashfs.read_bytes())
        image.chmod(0o755)
        environment = dict(os.environ, TMPDIR=str(base))
        for key in ('APPIMAGE_EXTRACT_AND_RUN', 'NO_CLEANUP', 'APPIMAGE', 'APPDIR', 'ARGV0'):
            environment.pop(key, None)
        if args.mode == 'mount':
            environment.pop('FUSERMOUNT_PROG', None)

        def launch(arguments):
            result = subprocess.run([str(image), *arguments], cwd=base, env=environment,
                                    capture_output=True, text=True, timeout=30)
            if args.mode == 'mount':
                # The FUSE daemon removes the mount after the payload closes fd 1023.
                for _ in range(50):
                    if not any(base.glob('.mount_*')):
                        break
                    time.sleep(0.1)
            assert result.returncode == (37 if arguments else 0), result
            data = json.loads(result.stdout)
            assert data['args'] == arguments, data
            assert data['APPIMAGE'] == str(image), data
            assert data['ARGV0'] == str(image), data
            assert data['OWD'] == str(base), data
            assert data['mounted'] == (args.mode == 'mount'), (data, result.stderr)
            if args.mode == 'no-fuse':
                assert data['mode'] == 0o700, data
                assert not Path(data['APPDIR']).exists(), data
                assert 'FUSE mount unavailable' in result.stderr, result.stderr
            return data

        # No arguments exercises the runtime's argv NULL terminator. Two
        # simultaneous launches must never reuse or remove each other's tree.
        empty = launch([])
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
            results = list(pool.map(launch, [['a b', '', '中文', '--flag=value'], ['second']]))
        assert results[0]['APPDIR'] != results[1]['APPDIR'], results
        for _ in range(50):
            if not any(base.glob('.mount_*')):
                break
            time.sleep(0.1)
        assert not list(base.glob('.mount_*')), 'mount directory leaked'
        assert not list(base.glob('appimage_extracted_*')), 'extraction directory leaked'
        if args.mode == 'no-fuse':
            # Terminate as soon as extraction begins, before AppRun is ready.
            process = subprocess.Popen([str(image), '--wait'], env=environment,
                                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                       start_new_session=True)
            deadline = time.monotonic() + 10
            while not list(base.glob('appimage_extracted_*')):
                assert time.monotonic() < deadline, 'extraction did not begin'
                time.sleep(0.001)
            process.terminate()
            assert process.wait(timeout=10) == 143, process.returncode
            assert not list(base.glob('appimage_extracted_*')), 'startup termination leaked'
            for group in (False, True):
                process = subprocess.Popen([str(image), '--wait'], env=environment,
                                           stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                           text=True, start_new_session=True)
                data = json.loads(process.stdout.readline())
                if group:
                    os.killpg(process.pid, signal.SIGTERM)
                else:
                    process.terminate()
                assert process.wait(timeout=10) == 143, process.returncode
                assert not Path(data['APPDIR']).exists(), data
            mount = subprocess.run([str(image), '--appimage-mount'], env=environment,
                                   capture_output=True, timeout=10)
            assert mount.returncode != 0 and not mount.stdout, mount
        print(json.dumps(dict(mode=args.mode, empty=empty, concurrent=results,
                              cleanup=True, exit_status=37,
                              signal_cleanup=args.mode == 'no-fuse'), ensure_ascii=False))


if __name__ == '__main__':
    main()
