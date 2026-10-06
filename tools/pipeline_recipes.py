#!/usr/bin/env python3
"""Merge and count pipeline recipe files (pipelines.bin / pipelines_vk12_1.bin).

  python tools/pipeline_recipes.py stats A.bin [B.bin ...]
  python tools/pipeline_recipes.py merge OUT.bin A.bin [B.bin ...]

Format (LostOdysseyRecomp/gpu/pipeline_cache.h): a 48-byte header, then
72-byte records (64-byte key: vs, ps, 12 words; FNV-1a of the key). Keys are
compared byte for byte; inputs must share the file, shader and recipe versions.
"""
import struct
import sys

MAGIC = b"LOPSO001"
HEADER, KEY, RECORD, MAX_RECORDS = 48, 64, 72, 16384
# plume RenderFormat values the renderer accepts (ValidPipelineRecipe in gpu/renderer.cpp):
# UNKNOWN, R16G16B16A16_FLOAT, R32G32_FLOAT, R8G8B8A8_UNORM, R16G16_FLOAT, R32_FLOAT; depth D32_FLOAT_S8_UINT.
COLOR_FORMATS, DEPTH_FORMATS = {0, 10, 16, 20, 26, 34}, {0, 33}


def valid(key):
    """pipeline_cache::IsValid plus the renderer's ValidPipelineRecipe; one bad key makes Load drop the file."""
    vs = struct.unpack_from("<Q", key)[0]
    (blend, depth, cull, mask, prim, rt, ds, flags, sref, sref_back, bias, slope) = struct.unpack_from("<12I", key, 16)
    slope_f = struct.unpack("<f", struct.pack("<I", slope))[0]
    return (not cull & ~0x3807 and not mask & ~0xF and not sref & ~0xFFFFFF and not sref_back & ~0xFFFFFF
            and flags == 0 and slope_f == slope_f and abs(slope_f) != float("inf")
            and vs != 0 and prim <= 32 and rt in COLOR_FORMATS and ds in DEPTH_FORMATS)


def fnv(data):
    h = 0xCBF29CE484222325
    for b in data:
        h = ((h ^ b) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h


def load(path):
    data = open(path, "rb").read()
    if len(data) < HEADER or data[:8] != MAGIC:
        sys.exit(f"{path}: not a recipe file")
    if struct.unpack_from("<Q", data, 40)[0] != fnv(data[:40]):
        sys.exit(f"{path}: header checksum mismatch")
    versions = struct.unpack_from("<III", data, 8)
    count = struct.unpack_from("<I", data, 24)[0]
    if len(data) != HEADER + count * RECORD:
        sys.exit(f"{path}: record framing does not match count {count}")
    keys = []
    for i in range(count):
        record = data[HEADER + i * RECORD: HEADER + (i + 1) * RECORD]
        if struct.unpack_from("<Q", record, KEY)[0] != fnv(record[:KEY]):
            sys.exit(f"{path}: record {i} checksum mismatch")
        if not valid(record[:KEY]):
            sys.exit(f"{path}: record {i} has state the renderer rejects")
        keys.append(record[:KEY])
    return versions, keys


def shaders(keys):
    return len({k[:8] for k in keys}), len({k[8:16] for k in keys})


def line(name, keys, extra=""):
    vs, ps = shaders(keys)
    print(f"{name}: {len(keys)} recipes, {vs} VS, {ps} PS{extra}")


def main():
    if len(sys.argv) < 3 or sys.argv[1] not in ("stats", "merge"):
        sys.exit(__doc__)
    merge = sys.argv[1] == "merge"
    out, inputs = (sys.argv[2], sys.argv[3:]) if merge else (None, sys.argv[2:])
    union, seen, versions = [], set(), None
    for path in inputs:
        v, keys = load(path)
        if versions is None:
            versions = v
        elif v != versions:
            sys.exit(f"{path}: versions {v} differ from {versions}")
        new = [k for k in dict.fromkeys(keys) if k not in seen]
        seen.update(new)
        union.extend(new)
        line(path, keys, f", {len(new)} new")
    line("union", union)
    if not merge:
        return
    if len(union) > MAX_RECORDS:
        sys.exit(f"union has {len(union)} recipes, over the runtime limit {MAX_RECORDS}")
    body = b"".join(k + struct.pack("<Q", fnv(k)) for k in union)
    header = bytearray(HEADER)
    header[:8] = MAGIC
    struct.pack_into("<IIIIII", header, 8, *versions, RECORD, len(union), 0)
    struct.pack_into("<Q", header, 32, fnv(body))
    struct.pack_into("<Q", header, 40, fnv(bytes(header[:40])))
    with open(out, "wb") as f:
        f.write(bytes(header) + body)
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
