#!/usr/bin/env python3
"""Generate explicit `functions = [...]` boundaries for XenonRecomp.

Inputs: flat image (xexdump), switch_tables.toml, previous XenonRecomp run log and ppc output.
Rules:
  * every `// ERROR <addr>` branch target in the ppc output becomes a function
    (size: up to the next known symbol, capped at the end of the enclosing .pdata function)
  * every switch table whose labels are not covered by the enclosing .pdata function gets
    an explicit function from the nearest symbol before the table to the next pdata/bl symbol
    after the furthest label
  * extra manual entries can be listed in config/function_bounds_manual.txt (address size)
Usage: gen_function_bounds.py <image.bin> <switch_tables.toml> <ppc dir> <out .inc> [manual.txt]
"""
import bisect, glob, re, struct, sys

BASE = 0x82000000
TEXT0, TEXT1 = 0x82290000, 0x82290000 + 0xE4AA6C
PDATA, PDATA_SIZE = 0x8221F600, 0x62798


def main(image, switch_toml, ppc_dir, out, manual=None):
    img = open(image, "rb").read()
    u32 = lambda a: struct.unpack_from(">I", img, a - BASE)[0]
    funcs = sorted((u32(PDATA + i * 8), ((u32(PDATA + i * 8 + 4) >> 8) & 0x3FFFFF) * 4)
                   for i in range(PDATA_SIZE // 8))
    pstarts = [b for b, _ in funcs]

    def pfunc(a):
        j = bisect.bisect_right(pstarts, a) - 1
        if j >= 0 and funcs[j][0] <= a < funcs[j][0] + funcs[j][1]:
            return funcs[j]
        return None, None

    bl_targets = set()
    for a in range(TEXT0, TEXT1, 4):
        w = u32(a)
        if (w >> 26) == 18 and (w & 1):
            li = w & 0x03FFFFFC
            if li & 0x02000000:
                li -= 0x04000000
            t = li if (w & 2) else a + li
            if TEXT0 <= t < TEXT1:
                bl_targets.add(t)

    # Branch targets accumulate across runs in <out dir>/branch_targets.txt
    acc_path = out.rsplit("/", 1)[0] + "/branch_targets.txt"
    errs = set()
    try:
        errs |= set(int(x, 16) for x in open(acc_path).read().split())
    except FileNotFoundError:
        pass
    for f in glob.glob(f"{ppc_dir}/ppc_recomp.*.cpp"):
        errs |= set(int(x, 16) for x in re.findall(r"// ERROR ([0-9A-F]+)", open(f).read()))
    with open(acc_path, "w") as fh:
        fh.write("\n".join(f"{a:08X}" for a in sorted(errs)) + "\n")

    hard = sorted(set(pstarts) | bl_targets)          # symbols that terminate a function
    soft = sorted(set(hard) | errs)                    # symbols that may start one

    def next_after(a, syms):
        k = bisect.bisect_right(syms, a)
        return syms[k] if k < len(syms) else TEXT1

    bounds = {}
    for t in errs:
        e = next_after(t, soft)
        pb, psz = pfunc(t)
        if pb is not None:
            e = min(e, pb + psz)
        bounds[t] = max(bounds.get(t, 0), e - t)

    txt = open(switch_toml).read()
    for blk in txt.split("[[switch]]")[1:]:
        sb = int(re.search(r"base = (0x[0-9A-Fa-f]+)", blk).group(1), 16)
        labels = [int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]+", blk.split("labels")[1])]
        d = re.search(r"default = (0x[0-9A-Fa-f]+)", blk)
        if d:
            labels.append(int(d.group(1), 16))
        f = soft[bisect.bisect_right(soft, sb) - 1]
        pb, psz = pfunc(sb)
        if pb is not None and f == pb and all(pb <= l < pb + psz for l in labels):
            continue
        hi = max(labels + [sb])
        e = next_after(hi, hard)
        if pb is not None and f == pb:
            e = min(e, pb + psz)
        bounds[f] = max(bounds.get(f, 0), e - f)

    if manual:
        for line in open(manual):
            line = line.split("#")[0].strip()
            if line:
                a, s = line.split()
                bounds[int(a, 16)] = int(s, 16)

    with open(out, "w") as fh:
        for a, s in sorted(bounds.items()):
            fh.write(f"    {{ address = 0x{a:08X}, size = 0x{s:X} }},\n")
    print(f"branch targets={len(errs)} explicit functions={len(bounds)}")


if __name__ == "__main__":
    main(*sys.argv[1:])
