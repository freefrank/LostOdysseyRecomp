#!/usr/bin/env python3
"""Survey code addresses referenced from data (vtables, function pointer tables)
that the recompiler has no function start for.
Usage: find_vtable_targets.py <image.bin> <ppc dir>"""
import bisect, glob, re, struct, sys
BASE = 0x82000000
TEXT0, TEXT1 = 0x82290000, 0x82290000 + 0xE4AA6C
PDATA, PDATA_SIZE = 0x8221F600, 0x62798

def main(image, ppc_dir):
    img = open(image, "rb").read()
    u32 = lambda a: struct.unpack_from(">I", img, a - BASE)[0]
    funcs = sorted((u32(PDATA + i * 8), ((u32(PDATA + i * 8 + 4) >> 8) & 0x3FFFFF) * 4)
                   for i in range(PDATA_SIZE // 8))
    pstarts = [b for b, _ in funcs]
    def in_pdata(a):
        j = bisect.bisect_right(pstarts, a) - 1
        return j >= 0 and funcs[j][0] <= a < funcs[j][0] + funcs[j][1]
    starts = set()
    for f in glob.glob(f"{ppc_dir}/ppc_func_mapping.cpp"):
        starts |= set(int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{8}), [A-Za-z_]", open(f).read()))
    refs = {}
    end = BASE + len(img)
    for a in range(BASE, end, 4):
        if TEXT0 <= a < TEXT1:
            continue
        w = u32(a)
        if TEXT0 <= w < TEXT1 and (w & 3) == 0:
            refs.setdefault(w, []).append(a)
    missing = {t: r for t, r in refs.items() if t not in starts}
    ends_prev = lambda t: u32(t - 4) in (0x4E800020, 0x4E800420, 0) or (u32(t - 4) >> 26) == 18
    rows = []
    for t, r in sorted(missing.items()):
        rows.append((t, len(r), in_pdata(t), ends_prev(t), u32(t)))
    print(f"data refs into text: {len(refs)} distinct targets, {len(missing)} without a function start")
    print(f"  of those inside a pdata function: {sum(1 for x in rows if x[2])}")
    print(f"  not in pdata, previous word ends a function: {sum(1 for x in rows if not x[2] and x[3])}")
    print(f"  not in pdata, previous word does not: {sum(1 for x in rows if not x[2] and not x[3])}")
    for t, n, inp, ep, w in rows:
        if not inp:
            print(f"{t:08X} refs={n} prev_end={int(ep)} first={w:08X}")

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
