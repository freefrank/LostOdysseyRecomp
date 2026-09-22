#!/usr/bin/env python3
"""Fetch and pin the audited NVIDIA DLSS SDK repository for CI release builds."""
import argparse
from pathlib import Path
import subprocess
import sys

DEFAULT_REMOTE = 'https://github.com/NVIDIA/DLSS.git'
DEFAULT_COMMIT = '374959484e79a640feaba44c93ac8cfb0a03f5b5'


def run(cmd, cwd=None):
    subprocess.run(cmd, cwd=cwd, check=True)


def verify_sdk(dest: Path):
    inc = dest / 'include' / 'nvsdk_ngx_vk.h'
    win_lib = dest / 'lib' / 'Windows_x86_64' / 'x64' / 'nvsdk_ngx_s.lib'
    win_rel = dest / 'lib' / 'Windows_x86_64' / 'rel' / 'nvngx_dlss.dll'
    linux_lib = dest / 'lib' / 'Linux_x86_64' / 'libnvsdk_ngx.a'
    linux_rel = dest / 'lib' / 'Linux_x86_64' / 'rel' / 'libnvidia-ngx-dlss.so.310.9.1'
    license_file = dest / 'LICENSE.txt'

    missing = []
    for f in [inc, win_lib, win_rel, linux_lib, linux_rel, license_file]:
        if not f.is_file():
            missing.append(str(f))
    if missing:
        raise SystemExit(f"DLSS SDK verification failed. Missing required files:\n" + "\n".join(missing))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dest', type=Path, default=Path('out/deps/nvidia-dlss'),
                        help='Destination directory for DLSS SDK')
    parser.add_argument('--remote', default=DEFAULT_REMOTE, help='Git remote URL')
    parser.add_argument('--commit', default=DEFAULT_COMMIT, help='Target Git commit SHA')
    args = parser.parse_args()

    dest = args.dest.resolve()
    if (dest / '.git').exists():
        try:
            head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=dest, text=True).strip()
            if head == args.commit:
                verify_sdk(dest)
                print(f"DLSS SDK already present and verified at {dest} (commit {head})")
                return
        except Exception:
            pass

    dest.parent.mkdir(parents=True, exist_ok=True)
    if not dest.exists():
        print(f"Cloning DLSS SDK into {dest}...")
        run(['git', 'init', str(dest)])
        run(['git', 'remote', 'add', 'origin', args.remote], cwd=dest)

    print(f"Fetching commit {args.commit}...")
    run(['git', 'fetch', '--depth', '1', 'origin', args.commit], cwd=dest)
    run(['git', 'checkout', args.commit], cwd=dest)

    verify_sdk(dest)
    print(f"DLSS SDK successfully fetched and verified at {dest} (commit {args.commit})")


if __name__ == '__main__':
    main()
