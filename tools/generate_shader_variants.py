"""Regenerate metadata in resource_variants.h from original FPD containers and XEX.

The inventory is the original uncompressed resource inventory: its `shaders`
object maps names to [FPD path, container offset]. No runtime shader cache is
accepted or consulted. The output contains layouts, fetch metadata, sizes and
hashes only, never shader microcode. This is a maintainer tool, not a build step.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct


IMAGE_SHA256 = "cb756b46092e448923517bf660b44ad1a0651c2860882b43ae021f4ad4290f71"
# [stream, byte offset, engine type, engine semantic, index], recovered by
# executing the constant declaration constructors in the SHA-checked XEX.
# SDK conversion is 0x825CD030; engine semantics map to the D3D values below.
DECLARATIONS = {
    0x823B8848: [(0, 0, 3, 0, 0), (0, 12, 3, 4, 0), (0, 24, 3, 5, 0),
                 (0, 36, 2, 1, 0), (0, 44, 1, 2, 0), (0, 48, 4, 1, 3),
                 (0, 64, 2, 1, 1), (0, 72, 4, 1, 2)],
    0x823B8B70: [(0, 0, 3, 0, 0), (0, 12, 3, 4, 0), (0, 24, 3, 5, 0),
                 (0, 36, 2, 1, 0), (0, 44, 1, 2, 0), (0, 48, 4, 1, 1)],
    0x824980E8: [(0, 0, 3, 0, 0)],
    0x82555510: [(0, 0, 4, 0, 0), (0, 16, 2, 1, 0), (0, 24, 4, 7, 0), (0, 40, 8, 7, 1)],
    0x825CDB08: [(0, 0, 4, 0, 0), (0, 16, 2, 1, 0)],
    0x82763DE0: [(0, 0, 2, 0, 0)],
    0x827A0CC8: [(0, 0, 2, 0, 0), (0, 8, 2, 1, 0)],
    0x827A3928: [(0, 0, 3, 0, 0), (0, 12, 3, 4, 0), (0, 24, 3, 5, 0),
                 (0, 36, 2, 1, 0), (0, 44, 1, 2, 0), (0, 48, 4, 1, 1)],
}
SEMANTICS = (0, 5, 1, 2, 3, 6, 7, 10)
FORMAT_CASES = {1: 0x825CD0E0, 2: 0x825CD0EC, 3: 0x825CD0F8, 4: 0x825CD104, 8: 0x825CD124}


def require(value, message):
    if not value:
        raise ValueError(message)


def fnv(data):
    result = 0xcbf29ce484222325
    for byte in data:
        result = ((result ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    return result


def read_source(path, offset, name):
    with Path(path).open('rb') as stream:
        stream.seek(offset)
        header = stream.read(36)
        require(len(header) == 36, f"Truncated container: {name}")
        flags, virtual, physical, _, ct, _, shader, _, _ = struct.unpack('>9I', header)
        require(flags == 0x102a1101 and 36 <= virtual <= 65536 and 12 <= physical <= 262144,
                f"Invalid VS container: {name}")
        require(36 <= ct <= virtual - 16 and 36 <= shader <= virtual - 36, f"Invalid metadata: {name}")
        stream.seek(offset)
        data = stream.read(virtual + physical)
        require(len(data) == virtual + physical, f"Truncated VS data: {name}")
    require(struct.unpack_from('>I', data, ct + 12)[0] == 0xfffe0300, f"Invalid VS stage: {name}")
    sh = struct.unpack_from('>9I', data, shader)
    start, size, first, count = sh[0], sh[1], shader + 36 + sh[6] * 4, sh[7]
    require(12 <= size <= 262144 and size % 12 == 0 and start + size <= physical,
            f"Invalid microcode size: {name}")
    require(first + count * 4 <= virtual and count <= 96, f"Invalid fetch list: {name}")
    metadata = struct.unpack_from(f'>{count}I', data, first)
    require(all((element & 4095) * 12 + 12 <= size for element in metadata), f"Fetch outside code: {name}")
    require(len({element & 4095 for element in metadata}) == count, f"Duplicate fetch slot: {name}")
    code = data[virtual + start:virtual + start + size]
    code_hash = fnv(code)
    require(name == f'vs_{code_hash:016x}.bin', f"Source hash mismatch: {name}")
    return code_hash, size, metadata


def generate(image_path, inventory_path):
    image = image_path.read_bytes()
    require(hashlib.sha256(image).hexdigest() == IMAGE_SHA256, "Unsupported XEX image; fixed layouts require revalidation")
    formats = {}
    for engine_type, pc in FORMAT_CASES.items():
        hi, lo = struct.unpack_from('>II', image, pc - 0x82000000)
        require(hi >> 26 == 15 and lo >> 26 == 24, "Unexpected format mapping instructions")
        formats[engine_type] = (hi & 65535) << 16 | (lo & 65535)
    widths = image[0x216538:0x216538 + 64]
    masks = struct.unpack_from('>8H', image, 0x185a40)
    elements, declarations, semantics = [], [], []
    for entry, layout in DECLARATIONS.items():
        mapped = [(formats[t], offset, stream, SEMANTICS[usage], index) for stream, offset, t, usage, index in layout]
        extent = max((offset + widths[fmt & 63] * 4 + 3) // 4 for fmt, offset, _, _, _ in mapped)
        declarations.append((entry, len(elements), len(mapped), extent))
        elements.extend(mapped)
        semantics.append({(usage, index) for _, _, _, usage, index in mapped})
    rows, fetches = [], []
    inventory = json.loads(inventory_path.read_text(encoding='utf-8'))['shaders']
    for name, (path, offset) in sorted(inventory.items()):
        if not name.startswith('vs_'):
            continue
        code_hash, size, metadata = read_source(path, offset, name)
        required = {((e >> 12) & 15, (e >> 16) & 15) for e in metadata}
        if not metadata or not any(required <= available for available in semantics):
            continue
        rows.append((code_hash, size, len(fetches), len(metadata)))
        fetches.extend(metadata)
    require(len(rows) == 77, f"Expected the validated 77 fixed-declaration sources; found {len(rows)}")
    require(len(fetches) <= 65535, "Metadata exceeds table index range")
    lines = ['// Generated by tools/generate_shader_variants.py. Metadata only; no microcode.',
             f'// Original XEX image SHA256: {IMAGE_SHA256}',
             '// SDK width table 0x82216538; component swizzle masks 0x82185A40.',
             'inline constexpr uint8_t widths[] = {']
    lines.extend('    ' + ', '.join(str(x) for x in widths[i:i + 16]) + ',' for i in range(0, 64, 16))
    lines.extend(['};', 'inline constexpr uint16_t swizzleMasks[] = {',
                  '    ' + ', '.join(f'0x{x:04x}' for x in masks), '};', 'inline constexpr Element elements[] = {'])
    lines.extend(f'    {{0x{fmt:08x}, {offset}, {stream}, {usage}, {index}}},' for fmt, offset, stream, usage, index in elements)
    lines.extend(['};', 'inline constexpr Declaration declarations[] = {'])
    lines.extend(f'    {{{first}, {count}, {extent}}}, // Original constructor 0x{entry:08X}' for entry, first, count, extent in declarations)
    lines.extend(['};', 'inline constexpr uint32_t fetchMetadata[] = {'])
    lines.extend('    ' + ', '.join(f'0x{x:08x}' for x in fetches[i:i + 8]) + ',' for i in range(0, len(fetches), 8))
    lines.extend(['};', 'inline constexpr Source sources[] = {'])
    lines.extend(f'    {{0x{code_hash:016x}ULL, {size}, {first}, {count}}},' for code_hash, size, first, count in rows)
    lines.append('};')
    return '\n'.join(lines) + '\n'


def generate_links(inventory_path, fixed_metadata):
    """Compile SDK semantic/link metadata into bounded edit plans, never code."""
    names = [f'vs_{h}.bin' for h in re.findall(r'\{0x([0-9a-f]{16})ULL,', fixed_metadata)]
    require(len(names) == 77, 'Fixed source table is not the validated 77-source set')
    sources, pixels = {}, {}
    inventory = json.loads(inventory_path.read_text(encoding='utf-8'))['shaders']
    for name, (path, offset) in sorted(inventory.items()):
        if name.startswith('vs_') and name not in names:
            continue
        with Path(path).open('rb') as stream:
            stream.seek(offset)
            header = stream.read(36)
            require(len(header) == 36, 'Truncated SDK header')
            hs = struct.unpack('>9I', header)
            require(hs[0] in (0x102a1100, 0x102a1101) and 36 <= hs[1] <= 65536 and
                    12 <= hs[2] <= 262144 and 36 <= hs[6] <= hs[1] - 32, f'Invalid SDK container: {name}')
            data = header + stream.read(hs[1] + hs[2] - 36)
        require(len(data) == hs[1] + hs[2], 'Truncated shader container')
        sh = struct.unpack_from(f'>{(hs[1] - hs[6]) // 4}I', data, hs[6])
        require(sh[1] % 12 == 0 and 12 <= sh[1] <= 262144 and sh[0] + sh[1] <= hs[2], 'Invalid code bounds')
        code_hash = fnv(data[hs[1] + sh[0]:hs[1] + sh[0] + sh[1]])
        require(name == f'{name[:2]}_{code_hash:016x}.bin', 'Original shader hash mismatch')
        vertex = name.startswith('vs_')
        require(vertex == (hs[0] == 0x102a1101), 'Shader stage mismatch')
        count = (sh[5] >> 5) & 31
        first = 9 + sh[6] + sh[7] if vertex else 8
        require(first + count <= len(sh), 'Interpolation metadata outside container')
        interps = sh[first:first + count]
        require(len({x & 255 for x in interps}) == count, 'Duplicate interpolation semantic')
        item = dict(hash=code_hash, size=sh[1], flags=sh[5], interps=interps, sh=sh)
        if vertex:
            lists = []
            for interp in interps:
                cursor = first + count + ((interp >> 16) & 4095)
                indices = []
                while True:
                    require(cursor < len(sh), 'Unterminated interpolation patch list')
                    patch = sh[cursor]
                    require((patch & 4095) * 12 + 12 <= sh[1], 'Interpolation patch outside code')
                    indices.append(patch & 4095)
                    if patch & 4096:
                        break
                    cursor += 1
                lists.append(indices)
            item['patches'] = lists
            sources[name] = item
        else:
            pixels.setdefault((sh[5], interps), item)
    pixel_list = list(pixels.values())
    edits, plans = [], []
    for source_index, name in enumerate(names):
        vertex = sources[name]
        if vertex['flags'] & 0x40000 or ((vertex['sh'][2] >> 24) & 7) == 7:
            continue
        by_usage = {i & 255: i for i in vertex['interps']}
        seen = set()
        for pixel_index, pixel in enumerate(pixel_list + [None]):
            if pixel and pixel['flags'] & 0x20000:
                continue
            wanted = {i & 255: i for i in pixel['interps']} if pixel else {}
            if any(u not in by_usage or ((v >> 12) & ~(by_usage[u] >> 12) & 15) for u, v in wanted.items()):
                continue
            changes = {}
            for interp, indices in zip(vertex['interps'], vertex['patches']):
                target = wanted.get(interp & 255)
                if target is None:
                    changes.update((index, 255) for index in indices)
                elif (target ^ interp) & 0xf00:
                    changes.update((index, (target >> 8) & 15) for index in indices)
            changes = tuple(sorted(changes.items()))
            if not changes or changes in seen:
                continue
            seen.add(changes)
            plans.append((source_index, pixel_index if pixel else 65535, len(edits), len(changes)))
            edits.extend(changes)
    require(len(edits) < 65536, 'Link edits exceed index range')
    lines = ['// Generated by tools/generate_shader_variants.py --linked-header. Metadata only.',
             '// SDK 0x827B8150 compatibility; 0x827B7E88 deletion; 0x827B7F58 remapping.',
             '#pragma once', '#include <cstdint>', 'namespace xenos::resources::variants::detail::link {',
             'struct Pixel { uint64_t hash; uint32_t size; };',
             'struct Edit { uint16_t instruction; uint8_t output; }; // 255 deletes the export',
             'struct Plan { uint16_t source, pixel, first, count; }; // pixel 65535 is null PS',
             'inline constexpr Pixel pixels[] = {']
    lines.extend(f"    {{0x{p['hash']:016x}ULL, {p['size']}}}," for p in pixel_list)
    lines.extend(['};', 'inline constexpr Edit edits[] = {'])
    lines.extend(f'    {{{instruction}, {output}}},' for instruction, output in edits)
    lines.extend(['};', 'inline constexpr Plan plans[] = {'])
    lines.extend(f'    {{{source}, {pixel}, {first}, {count}}},' for source, pixel, first, count in plans)
    lines.extend(['};', '}'])
    return '\n'.join(lines) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('inventory', type=Path)
    parser.add_argument('header', type=Path)
    parser.add_argument('--check', action='store_true', help='Verify deterministic metadata without writing')
    parser.add_argument('--linked-header', type=Path, help='Also generate original SDK output-link metadata')
    args = parser.parse_args()
    metadata = generate(args.image, args.inventory)
    before = args.header.read_text(encoding='utf-8')
    start, end = '// BEGIN GENERATED METADATA\n', '// END GENERATED METADATA'
    require(before.count(start) == 1 and before.count(end) == 1, 'Header metadata markers missing or ambiguous')
    prefix, remaining = before.split(start)
    _, suffix = remaining.split(end)
    after = prefix + start + metadata + end + suffix
    if args.check:
        require(after == before, 'Generated metadata differs from checked-in header')
    else:
        args.header.write_text(after, encoding='utf-8', newline='\n')
    if args.linked_header:
        linked = generate_links(args.inventory, metadata)
        if args.check:
            require(args.linked_header.read_text(encoding='utf-8') == linked, 'Generated link metadata differs')
        else:
            args.linked_header.write_text(linked, encoding='utf-8', newline='\n')
    print('Verified 77 source hashes, 8 original declaration layouts, metadata only')


if __name__ == '__main__':
    main()
