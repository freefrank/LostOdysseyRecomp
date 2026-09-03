#!/usr/bin/env python3
"""Print XEX2 header information: execution info, file format, imports, base address.
Usage: xex_info.py <default.xex>
"""
import struct
import sys

NUL = bytes(1)

OPT_KEYS = {
    0x000002FF: "resource_info",
    0x000003FF: "file_format_info",
    0x00000405: "delta_patch_descriptor",
    0x000005FF: "base_reference",
    0x000080FF: "bounding_path",
    0x00010100: "entry_point",
    0x00010201: "image_base_address",
    0x00010001: "unknown_10001",
    0x000103FF: "import_libraries",
    0x00018002: "checksum_timestamp",
    0x00018102: "enabled_for_callcap",
    0x00018200: "enabled_for_fastcap",
    0x000183FF: "original_pe_name",
    0x000200FF: "static_libraries",
    0x00020104: "static_libraries_ex",
    0x00020200: "tls_info",
    0x00020401: "default_stack_size",
    0x00020402: "default_filesystem_cache_size",
    0x00020403: "default_heap_size",
    0x00030000: "page_heap_size_and_flags",
    0x00040006: "execution_info",
    0x00040310: "title_workspace_size",
    0x00040404: "game_ratings",
    0x00040501: "lan_key",
    0x000405FF: "xbox360_logo",
    0x000406FF: "multidisc_media_ids",
    0x00040900: "alternate_title_ids",
    0x00040A01: "additional_title_memory",
    0x00040B01: "export_by_name",
}


def unpack_version(v):
    return f"{v >> 28}.{(v >> 24) & 0xF}.{(v >> 8) & 0xFFFF}.{v & 0xFF}"


def main(path):
    b = open(path, "rb").read()
    assert b[:4] == b"XEX2", "not an XEX2"
    module_flags, pe_off, _, sec_off, count = struct.unpack_from(">IIIII", b, 4)
    print(f"module_flags={module_flags:#x} pe_data_offset={pe_off:#x} security_info_offset={sec_off:#x} opt_headers={count}")
    hdr = {}
    for i in range(count):
        key, val = struct.unpack_from(">II", b, 0x18 + i * 8)
        hdr[key] = val
        name = OPT_KEYS.get(key, f"unknown_{key:08x}")
        print(f"  {key:08x} {name:<32} {val:#010x}")

    hs, img_size, _, img_flags, load_addr = struct.unpack_from(">IIIII", b, sec_off)
    print(f"\nsecurity: header_size={hs:#x} image_size={img_size:#x} image_flags={img_flags:#x} load_address={load_addr:#x}")
    region = struct.unpack_from(">I", b, sec_off + 0x178)[0]
    media_flags = struct.unpack_from(">I", b, sec_off + 0x17C)[0]
    print(f"          game_regions={region:#010x} allowed_media_types={media_flags:#010x}")

    if 0x00040006 in hdr:
        o = hdr[0x00040006]
        media_id, version, base_version, title_id = struct.unpack_from(">IIII", b, o)
        platform, exec_type, disc, discs, savegame_id = struct.unpack_from(">BBBBI", b, o + 16)
        print(f"\nexecution: title_id={title_id:08X} media_id={media_id:08X} version={version:#010x} base_version={base_version:#010x}")
        print(f"           platform={platform} exec_type={exec_type} disc={disc}/{discs} savegame_id={savegame_id:#x}")

    if 0x000003FF in hdr:
        o = hdr[0x000003FF]
        size, enc, comp = struct.unpack_from(">IHH", b, o)
        enc_s = {0: "none", 1: "normal(AES)"}.get(enc, str(enc))
        comp_s = {0: "none", 1: "basic", 2: "normal(LZX)", 3: "delta"}.get(comp, str(comp))
        print(f"\nfile_format: encryption={enc_s} compression={comp_s}")
        if comp == 1:
            nblocks = (size - 8) // 8
            blocks = [struct.unpack_from(">II", b, o + 8 + i * 8) for i in range(nblocks)]
            print(f"  basic blocks={nblocks} total_data={sum(d for d, _ in blocks):#x} total_zero={sum(z for _, z in blocks):#x}")
        elif comp == 2:
            ws, fsize = struct.unpack_from(">II", b, o + 8)
            print(f"  lzx window={ws:#x} first_block_size={fsize:#x}")

    if 0x00010100 in hdr:
        print(f"\nentry_point={hdr[0x00010100]:#x}")
    if 0x00010201 in hdr:
        print(f"image_base={hdr[0x00010201]:#x}")

    if 0x000183FF in hdr:
        o = hdr[0x000183FF]
        n = struct.unpack_from(">I", b, o)[0]
        print(f"original_pe_name={b[o + 4:o + n].split(NUL)[0].decode('latin-1')}")

    if 0x000406FF in hdr:
        o = hdr[0x000406FF]
        n = struct.unpack_from(">I", b, o)[0]
        ids = []
        for i in range((n - 4) // 16):
            digest = b[o + 4 + i * 16:o + 4 + i * 16 + 12]
            mid = struct.unpack_from(">I", b, o + 4 + i * 16 + 12)[0]
            if mid:
                ids.append(f"{mid:08X}")
        print(f"multidisc_media_ids={' '.join(ids)}")

    key = 0x000103FF
    if key in hdr:
        o = hdr[key]
        size, str_size, str_count = struct.unpack_from(">III", b, o)
        names = [n.decode("latin-1") for n in b[o + 12:o + 12 + str_size].split(NUL) if n]
        p = o + 12 + str_size
        print("\nimport libraries:")
        while p < o + size:
            lsize, _digest, import_id, ver, ver_min, name_idx, nimports = struct.unpack_from(">I20sIIIHH", b, p)
            print(f"  {names[name_idx]:<16} version={unpack_version(ver)} min={unpack_version(ver_min)} imports={nimports}")
            p += lsize

    if 0x000200FF in hdr:
        o = hdr[0x000200FF]
        size = struct.unpack_from(">I", b, o)[0]
        n = (size - 4) // 16
        print(f"\nstatic libraries ({n}):")
        for i in range(n):
            name = b[o + 4 + i * 16:o + 12 + i * 16].rstrip(NUL).decode("latin-1")
            maj, mi, bu, qfe = struct.unpack_from(">HHHH", b, o + 12 + i * 16)
            print(f"  {name:<10} {maj}.{mi}.{bu}.{qfe & 0x7FFF}")


if __name__ == "__main__":
    main(sys.argv[1])
