#!/usr/bin/env python3
"""Locate the compiler helper functions XenonRecomp needs (save/restore gpr/fpr/vmx)
by byte pattern in a flat image dumped with tools/xexdump.

Usage: find_ppc_helpers.py <image.bin> <image_base_hex>
Patterns come from tools/XenonRecomp/README.md.
"""
import sys

PATTERNS = {
    "restgprlr_14_address": bytes.fromhex("e9c1ff68"),           # ld   r14, -0x98(r1)
    "savegprlr_14_address": bytes.fromhex("f9c1ff68"),           # std  r14, -0x98(r1)
    "restfpr_14_address":   bytes.fromhex("c9ccff70"),           # lfd  f14, -0x90(r12)
    "savefpr_14_address":   bytes.fromhex("d9ccff70"),           # stfd f14, -0x90(r12)
    "restvmx_14_address":   bytes.fromhex("3960fee07dcb60ce"),   # li r11,-0x120 ; lvx  v14,r11,r12
    "savevmx_14_address":   bytes.fromhex("3960fee07dcb61ce"),   # li r11,-0x120 ; stvx v14,r11,r12
    "restvmx_64_address":   bytes.fromhex("3960fc00100b60cb"),   # li r11,-0x400 ; lvx128  v64,r11,r12
    "savevmx_64_address":   bytes.fromhex("3960fc00100b61cb"),   # li r11,-0x400 ; stvx128 v64,r11,r12
}


def main(path, base_hex):
    img = open(path, "rb").read()
    base = int(base_hex, 16)
    ok = True
    for name, pat in PATTERNS.items():
        hits = []
        i = img.find(pat)
        while i >= 0:
            if i % 4 == 0:
                hits.append(base + i)
            i = img.find(pat, i + 1)
        if len(hits) == 1:
            print(f"{name} = 0x{hits[0]:08X}")
        else:
            ok = False
            print(f"# {name}: {len(hits)} candidates: " + " ".join(f"0x{h:08X}" for h in hits))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1], sys.argv[2]))
