#!/usr/bin/env python3
"""Writes every SPIR-V shader of a Vulkan shader pack (portable_vk.lospv) to <dir>/vs_<hash>.spv / ps_<hash>.spv,
the input of deko3d_shader_tool. Needs the 'zstandard' module.

    extract_spirv.py portable_vk.lospv <dir>
"""
import os
import struct
import sys

import zstandard


def main():
    pack, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    d = open(pack, 'rb').read()
    pos = 8 + 8 + 32
    idxoff, idxbytes, _ = struct.unpack_from('<QQQ', d, pos)
    pos += 24
    records, uniq, blocks, _ = struct.unpack_from('<IIII', d, pos)
    ix = memoryview(d)[idxoff:idxoff + idxbytes]
    p = 0

    def u32():
        nonlocal p
        v = struct.unpack_from('<I', ix, p)[0]
        p += 4
        return v

    def u64():
        nonlocal p
        v = struct.unpack_from('<Q', ix, p)[0]
        p += 8
        return v

    u32(); u64(); u64(); u32(); n = u32(); p += n
    entries = []
    for _ in range(records):
        h = u64(); fl = u32(); u32(); u64(); u64(); u32(); p += 32; u32(); blob = u32()
        entries.append((bool(fl & 1), h, blob))
    blobs = [(u32(), u32(), u32()) for _ in range(uniq)]
    blks = []
    for _ in range(blocks):
        o = u64(); c = u32(); r = u32(); p += 32
        blks.append((o, c, r))
    dc = zstandard.ZstdDecompressor()
    cache = {}
    for pixel, h, blob in entries:
        b, o, s = blobs[blob]
        if b not in cache:
            bo, bc, br = blks[b]
            cache = {b: dc.decompress(d[bo:bo + bc], max_output_size=br)}
        with open(os.path.join(out, f'{"ps" if pixel else "vs"}_{h:016x}.spv'), 'wb') as f:
            f.write(cache[b][o:o + s])
    print(f'{records} shaders written to {out}')


if __name__ == '__main__':
    main()
