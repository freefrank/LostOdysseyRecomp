#!/usr/bin/env python3
"""Lists the texture-info specializations of the game's pipeline recipes for deko3d_shader_tool variants.

    python recipe_variants.py <spv dir> pipelines_corpus.bin [pipelines_vk12_1.bin ...] > variants.txt
    deko3d_shader_tool variants variants.dkcache < variants.txt

Output lines:
  "<spv dir>/<vs_|ps_><hash>.spv <slot>=<word> ...": the words a recipe was drawn with (the renderer's texture-info
      specialization constants: constant id = fetch slot);
  "vsout <vs.spv> <ps.spv | ->": every recipe's vertex shader with its pixel shader (or none), for the variant that
      stores only the outputs that pixel shader reads (plume_deko3d's vsOutputs, constant 905).
Recipe files are gpu/pipeline_cache.h's format (file versions 1-3; only version 3 has texture-info words).
"""
import os
import struct
import sys

HEADER = 48
KEY = 64
SCENES = 32


def read(path):
    data = open(path, 'rb').read()
    if data[:8] != b'LOPSO001':
        raise SystemExit(f'{path}: not a recipe file')
    version, _shader, _recipe, record_bytes, count, texinfo_bytes = struct.unpack_from('<6I', data, 8)
    keys = []
    for i in range(count):
        vs, ps = struct.unpack_from('<QQ', data, HEADER + i * record_bytes)
        keys.append((vs, ps))
    infos = {}
    off = HEADER + count * record_bytes
    end = off + texinfo_bytes
    while off + 8 <= end:
        index, mask = struct.unpack_from('<II', data, off)
        n = bin(mask).count('1')
        words = struct.unpack_from(f'<{n}I', data, off + 8)
        off += 8 + 4 * n
        slots = [b for b in range(32) if mask >> b & 1]
        infos.setdefault(keys[index], set()).add(tuple(zip(slots, words)))
    return version, keys, infos


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    spv = sys.argv[1]
    lines = set()
    missing = set()
    for path in sys.argv[2:]:
        version, keys, infos = read(path)
        for vs, ps in keys:
            v = os.path.join(spv, f'vs_{vs:016x}.spv')
            p = os.path.join(spv, f'ps_{ps:016x}.spv') if ps else '-'
            if not os.path.exists(v) or (ps and not os.path.exists(p)):
                missing.add(v if not os.path.exists(v) else p)
                continue
            lines.add(f'vsout {v} {p}')
        print(f'{path}: version {version}, {len(keys)} recipes, {sum(len(v) for v in infos.values())} texture-info entries',
              file=sys.stderr)
        for (vs, ps), entries in infos.items():
            for entry in entries:
                values = ' '.join(f'{slot}={word}' for slot, word in entry)
                for prefix, h in (('vs', vs), ('ps', ps)):
                    if not h:
                        continue
                    f = os.path.join(spv, f'{prefix}_{h:016x}.spv')
                    if not os.path.exists(f):
                        missing.add(f)
                        continue
                    lines.add(f'{f} {values}')
    for line in sorted(lines):
        print(line)
    print(f'{len(lines)} shader specializations, {len(missing)} shaders not in the SPIR-V folder', file=sys.stderr)


if __name__ == '__main__':
    main()
