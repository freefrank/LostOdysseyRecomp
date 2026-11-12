#!/usr/bin/env python3
"""Packs deko3d_shader_tool's DKS1 records into deko3d_shaders.lodk (LostOdysseyRecomp/gpu/deko3d_shader_cache.cpp).

    pack_dkcache.py out.lodk cache_a.dkcache [cache_b.dkcache ...]

Records cut short (an interrupted tool run) are skipped; duplicate keys keep the first. Each DKSH is stored
zstd-compressed (level 19) when that is smaller. Needs the 'zstandard' module (pip install zstandard).
"""
import struct
import sys

import zstandard

RECORD_MAGIC = 0x31534B44  # "DKS1"
PACK_MAGIC = 0x4B444F4C    # "LODK"


def records(path):
    data = open(path, 'rb').read()
    pos = 0
    while pos + 16 <= len(data):
        magic, size, key = struct.unpack_from('<IIQ', data, pos)
        if magic != RECORD_MAGIC or pos + 16 + size > len(data):
            break
        yield key, data[pos + 16:pos + 16 + size]
        pos += 16 + size


def main():
    out, inputs = sys.argv[1], sys.argv[2:]
    entries = {}
    for path in inputs:
        for key, dksh in records(path):
            entries.setdefault(key, dksh)
    keys = sorted(entries)
    cctx = zstandard.ZstdCompressor(level=19)
    blobs = []
    for key in keys:
        raw = entries[key]
        packed = cctx.compress(raw)
        blobs.append(packed if len(packed) < len(raw) else raw)
    header = struct.pack('<IIII', PACK_MAGIC, 1, len(keys), 0)
    offset = len(header) + 24 * len(keys)
    index = bytearray()
    for key, blob in zip(keys, blobs):
        index += struct.pack('<QQII', key, offset, len(blob), len(entries[key]))
        offset += len(blob)
    with open(out, 'wb') as f:
        f.write(header)
        f.write(index)
        for blob in blobs:
            f.write(blob)
    raw_total = sum(len(v) for v in entries.values())
    print(f'{out}: {len(keys)} shaders, {raw_total / 2**20:.1f} MiB of DKSH, {offset / 2**20:.1f} MiB packed')


if __name__ == '__main__':
    main()
