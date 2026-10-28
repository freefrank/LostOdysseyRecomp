#!/usr/bin/env python3
"""Fetch and pin the audited NVIDIA DLSS SDK repository for CI release builds."""
import argparse
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys

DEFAULT_REMOTE = 'https://github.com/NVIDIA/DLSS.git'
DEFAULT_COMMIT = '374959484e79a640feaba44c93ac8cfb0a03f5b5'


def run(cmd, cwd=None):
    subprocess.run(cmd, cwd=cwd, check=True)


def remove_tree(path: Path):
    """Delete a checkout, including git's read-only object files on Windows."""
    def retry_writable(func, target, _):
        os.chmod(target, stat.S_IWRITE)
        func(target)
    if path.exists():
        handler = {'onexc' if sys.version_info >= (3, 12) else 'onerror': retry_writable}
        shutil.rmtree(path, **handler)


def restore_from_cache(cache: Path, dest: Path, commit: str) -> bool:
    """Copy a cached checkout of the pinned commit into an empty destination."""
    if dest.exists() or not (cache / '.git').is_dir():
        return False
    print(f"Restoring DLSS SDK from cache {cache}...")
    shutil.copytree(cache, dest, symlinks=True)
    try:
        head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=dest, text=True).strip()
        if head != commit:
            raise SystemExit(f'cached checkout is at {head}')
        verify_sdk(dest)
    except (OSError, subprocess.CalledProcessError, SystemExit) as error:
        # A damaged cache entry is dropped and the SDK is fetched again.
        print(f"Discarding unusable DLSS SDK cache: {error}")
        remove_tree(dest)
        try:
            remove_tree(cache)
        except OSError as cleanup_error:
            print(f"Could not remove the cache entry: {cleanup_error}")
        return False
    return True


def store_in_cache(dest: Path, cache: Path):
    """Publish a verified checkout to the cache; concurrent writers keep the first copy."""
    if cache.exists():
        return
    cache.parent.mkdir(parents=True, exist_ok=True)
    staging = cache.with_name(f'{cache.name}.tmp-{os.getpid()}')
    remove_tree(staging)
    shutil.copytree(dest, staging, symlinks=True)
    try:
        os.replace(staging, cache)
        print(f"Cached DLSS SDK at {cache}")
    except OSError:
        remove_tree(staging)


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
    parser.add_argument('--cache-root', type=Path,
                        help='Optional local cache directory; CI caches --dest with actions/cache; '
                             'the verified checkout is kept in <root>/nvidia-dlss-<commit>')
    args = parser.parse_args()

    dest = args.dest.resolve()
    cache = Path(args.cache_root).resolve() / f'nvidia-dlss-{args.commit}' if args.cache_root else None
    if cache:
        restore_from_cache(cache, dest, args.commit)
    if (dest / '.git').exists():
        try:
            head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=dest, text=True).strip()
            if head == args.commit:
                verify_sdk(dest)
                print(f"DLSS SDK already present and verified at {dest} (commit {head})")
                if cache:
                    store_in_cache(dest, cache)
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
    if cache:
        store_in_cache(dest, cache)


if __name__ == '__main__':
    main()
