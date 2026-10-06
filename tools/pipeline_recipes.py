#!/usr/bin/env python3
"""Inspect and combine pipeline recipe files (pipelines*.bin, pipelines_corpus.bin).

Usage:
  python tools/pipeline_recipes.py info FILE...
  python tools/pipeline_recipes.py merge OUT IN...      # union; scenes merge
  python tools/pipeline_recipes.py subtract OUT A B     # recipes of A whose keys are not in B
  python tools/pipeline_recipes.py strip OUT IN         # drop scene tags

Reads file versions 1 and 2 (gpu/pipeline_cache.h). Keys of recipe version 1
are normalized like gpu::pipeline_cache::Normalize; output is always file
version 2, recipe version 2, with the first input's translator version. A
shipped corpus (shaders/pipelines_corpus.bin) is a merge of learned files.
"""
import struct
import sys

MAGIC = b"LOPSO001"
HEADER = 48
KEY = 64
SLOTS = 8
RECORD_V1 = KEY + 8
RECORD_V2 = KEY + SLOTS * 4 + 8
RECIPE_VERSION = 2
MANY = 0xFFFFFFFF
NO_BLEND = 0x00010001
KINDS = {1: "map", 2: "battle"}


def fnv(data):
    h = 0xCBF29CE484222325
    for byte in data:
        h = ((h ^ byte) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h


def normalize(key):
    """Mirror of gpu::pipeline_cache::Normalize on the 64-byte encoded key."""
    vs, ps, blend, control, cull, mask, prim, rt, depth, flags, ref, ref_back, bias, slope = \
        struct.unpack("<QQ12I", key)
    control &= ~0x8
    depth_on = depth != 0 and control & 2
    stencil_on = depth != 0 and control & 1
    if not depth_on:
        control &= ~0x72
        bias = slope = 0
    if not stencil_on:
        control &= 0x76
        ref = ref_back = 0
    elif not control & 0x80:
        control &= 0x000FFFFF
        ref_back = 0
    cull &= 0x7
    if slope == 0x80000000:
        slope = 0
    blend = blend & 0x1FFF1FFF if mask & 0xF else NO_BLEND
    if prim not in (1, 2, 3, 6, 8):
        prim = 4
    return struct.pack("<QQ12I", vs, ps, blend, control & 0xFFFFFFFF, cull, mask, prim, rt, depth, flags, ref,
                       ref_back, bias, slope)


def load(path):
    """Returns (translator version, {key bytes: [scene tags]}) in file order."""
    data = open(path, "rb").read()
    if data[:8] != MAGIC or struct.unpack_from("<Q", data, 40)[0] != fnv(data[:40]):
        raise SystemExit(f"{path}: not a recipe file")
    file_version, shader_version, recipe_version, record, count = struct.unpack_from("<5I", data, 8)
    if file_version not in (1, 2) or record != (RECORD_V1 if file_version == 1 else RECORD_V2):
        raise SystemExit(f"{path}: unsupported file version {file_version}")
    if recipe_version > RECIPE_VERSION or len(data) != HEADER + count * record:
        raise SystemExit(f"{path}: unsupported recipe version {recipe_version} or bad length")
    recipes = {}
    for i in range(count):
        raw = data[HEADER + i * record: HEADER + (i + 1) * record]
        if struct.unpack_from("<Q", raw, record - 8)[0] != fnv(raw[:record - 8]):
            raise SystemExit(f"{path}: record {i} checksum mismatch")
        key = raw[:KEY] if recipe_version >= RECIPE_VERSION else normalize(raw[:KEY])
        tags = [t for t in struct.unpack_from(f"<{SLOTS}I", raw, KEY) if t] if file_version == 2 else []
        add_scenes(recipes.setdefault(key, []), tags)
    return shader_version, recipes


def add_scenes(scenes, tags):
    for tag in tags:
        if scenes == [MANY] or tag in scenes:
            continue
        if tag == MANY or len(scenes) == SLOTS:
            scenes[:] = [MANY]
        else:
            scenes.append(tag)


def write(path, shader_version, recipes):
    body = bytearray()
    for key, scenes in recipes.items():
        record = key + struct.pack(f"<{SLOTS}I", *(scenes + [0] * (SLOTS - len(scenes))))
        body += record + struct.pack("<Q", fnv(record))
    header = bytearray(HEADER)
    header[:8] = MAGIC
    struct.pack_into("<6I", header, 8, 2, shader_version, RECIPE_VERSION, RECORD_V2, len(recipes), 0)
    struct.pack_into("<Q", header, 32, fnv(body))
    struct.pack_into("<Q", header, 40, fnv(header[:40]))
    with open(path, "wb") as out:
        out.write(header + body)
    print(f"{path}: {len(recipes)} recipes")


def scene_name(tag):
    return "many" if tag == MANY else f"{KINDS.get(tag >> 28, tag >> 28)} {tag & 0x0FFFFFFF}"


def info(paths):
    for path in paths:
        shader_version, recipes = load(path)
        per_scene = {}
        for scenes in recipes.values():
            for tag in scenes:
                per_scene[tag] = per_scene.get(tag, 0) + 1
        untagged = sum(1 for scenes in recipes.values() if not scenes)
        print(f"{path}: {len(recipes)} recipes, translator {shader_version}, {untagged} untagged, "
              f"{per_scene.get(MANY, 0)} in more than {SLOTS} scenes, {len([t for t in per_scene if t != MANY])} scenes")
        for tag, count in sorted(per_scene.items(), key=lambda item: -item[1])[:40]:
            print(f"  {scene_name(tag):<16} {count}")


def main(argv):
    if len(argv) < 2:
        raise SystemExit(__doc__)
    command, args = argv[1], argv[2:]
    if command == "info" and args:
        info(args)
    elif command == "merge" and len(args) >= 2:
        merged, version = {}, None
        for path in args[1:]:
            shader_version, recipes = load(path)
            version = version or shader_version
            for key, scenes in recipes.items():
                add_scenes(merged.setdefault(key, []), scenes)
        write(args[0], version, merged)
    elif command == "subtract" and len(args) == 3:
        version, a = load(args[1])
        _, b = load(args[2])
        write(args[0], version, {key: scenes for key, scenes in a.items() if key not in b})
    elif command == "strip" and len(args) == 2:
        version, recipes = load(args[1])
        write(args[0], version, {key: [] for key in recipes})
    else:
        raise SystemExit(__doc__)


if __name__ == "__main__":
    main(sys.argv)
