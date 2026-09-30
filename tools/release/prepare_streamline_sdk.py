#!/usr/bin/env python3
"""Verify and extract the pinned official Streamline SDK release archive."""
import argparse
import hashlib
import json
from pathlib import Path
from zipfile import ZipFile


ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / 'tools/tests/streamline_fg/sdk-manifest.json'
RUNTIME_FILES = (
    'sl.interposer.dll', 'sl.common.dll', 'sl.dlss_g.dll',
    'sl.reflex.dll', 'sl.pcl.dll', 'nvngx_dlssg.dll',
    'NvLowLatencyVk.dll',
)
LICENSE_FILES = (
    'license.txt', '3rd-party-licenses.md',
    'bin/x64/nvngx_dlss.license.txt',
    'bin/x64/reflex.license.txt',
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', type=Path, required=True)
    parser.add_argument('--dest', type=Path, required=True)
    args = parser.parse_args()

    manifest = json.loads(MANIFEST.read_text(encoding='utf-8'))
    archive = args.archive.resolve()
    if archive.name != manifest['asset'] or archive.stat().st_size != manifest['asset_size']:
        raise SystemExit('Unexpected Streamline SDK archive name or size.')
    with archive.open('rb') as source:
        digest = hashlib.file_digest(source, 'sha256').hexdigest()
    if digest != manifest['asset_sha256']:
        raise SystemExit('Streamline SDK archive digest differs from pinned release.')

    with ZipFile(archive) as source:
        names = set(source.namelist())
        selected = [name for name in names if name.startswith('include/') and not name.endswith('/')]
        selected += [f'bin/x64/{name}' for name in RUNTIME_FILES]
        selected += list(LICENSE_FILES)
        missing = [name for name in selected if name not in names]
        if not selected or missing:
            raise SystemExit(f'Streamline SDK archive is incomplete: {missing}')
        dest = args.dest.resolve()
        for name in sorted(selected):
            target = dest.joinpath(*Path(name).parts)
            target.parent.mkdir(parents=True, exist_ok=True)
            with source.open(name) as content, target.open('wb') as output:
                while block := content.read(1024 * 1024):
                    output.write(block)
    print(f'Prepared verified Streamline {manifest["release"]} SDK at {dest}')


if __name__ == '__main__':
    main()
