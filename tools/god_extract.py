#!/usr/bin/env python3
"""Extract files from an Xbox 360 GOD (Games on Demand / SVOD) container.

Layout (verified against Lost Odyssey discs, see docs/notes/xex.md):
  * Data files are 0xA290000 bytes each.
  * Each data file = 1 level-1 hash block + 203 x (1 level-0 hash block + 204 data blocks).
  * Block size 0x1000. Stripping the hash blocks yields a plain XDVDFS (GDF) image
    whose volume descriptor sits at virtual offset 0x10000.

Usage:
  god_extract.py <header-file | .data dir> <out-dir> [--list]
"""
import os
import struct
import sys

BLOCK = 0x1000
BLOCKS_PER_HASH = 0xCC              # data blocks per level-0 hash block
HASH_GROUPS_PER_FILE = 0xCB         # level-0 groups per data file
BLOCKS_PER_FILE = BLOCKS_PER_HASH * HASH_GROUPS_PER_FILE   # 0xA1C4
SECTOR = 0x800
GDF_MAGIC = b"MICROSOFT*XBOX*MEDIA"
ATTR_DIR = 0x10


class Svod:
    def __init__(self, data_dir):
        names = sorted(n for n in os.listdir(data_dir) if n.startswith("Data"))
        if not names:
            raise SystemExit(f"no Data#### files in {data_dir}")
        self.files = [open(os.path.join(data_dir, n), "rb") for n in names]

    def _locate(self, virt):
        blk, inblk = divmod(virt, BLOCK)
        fi, fb = divmod(blk, BLOCKS_PER_FILE)
        group, gb = divmod(fb, BLOCKS_PER_HASH)
        foff = BLOCK + (group + 1) * BLOCK + fb * BLOCK + inblk
        contiguous = (BLOCKS_PER_HASH - gb) * BLOCK - inblk
        return fi, foff, contiguous

    def copy(self, virt, n, sink):
        while n > 0:
            fi, foff, avail = self._locate(virt)
            if fi >= len(self.files):
                raise EOFError(f"virtual offset {virt:#x} beyond last data file")
            take = min(n, avail)
            fh = self.files[fi]
            fh.seek(foff)
            chunk = fh.read(take)
            if len(chunk) != take:
                raise EOFError(f"short read in Data{fi:04d} at {foff:#x}")
            sink(chunk)
            virt += take
            n -= take

    def read(self, virt, n):
        out = bytearray()
        self.copy(virt, n, out.extend)
        return bytes(out)


def parse_dir(svod, sector, size):
    """Return list of (name, start_sector, size, attr) for one directory."""
    if size == 0 or sector == 0:
        return []
    data = svod.read(sector * SECTOR, size)
    entries = []
    stack = [0]
    seen = set()
    while stack:
        off = stack.pop()
        if off in seen or off + 14 > len(data):
            continue
        seen.add(off)
        left, right, start, fsize, attr, nlen = struct.unpack_from("<HHIIBB", data, off)
        if left == 0xFFFF and right == 0xFFFF:
            continue  # sector padding
        name = data[off + 14: off + 14 + nlen].decode("ascii", "replace")
        entries.append((name, start, fsize, attr))
        if right:
            stack.append(right * 4)
        if left:
            stack.append(left * 4)
    return entries


def walk(svod, sector, size, rel=""):
    for name, start, fsize, attr in parse_dir(svod, sector, size):
        path = os.path.join(rel, name) if rel else name
        if attr & ATTR_DIR:
            yield path, None, None, True
            yield from walk(svod, start, fsize, path)
        else:
            yield path, start, fsize, False


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    list_only = "--list" in sys.argv
    if len(args) < 1 or (not list_only and len(args) < 2):
        print(__doc__)
        sys.exit(2)
    src = args[0]
    data_dir = src if os.path.isdir(src) else src + ".data"
    svod = Svod(data_dir)

    vd = svod.read(0x10000, SECTOR)
    if vd[:20] != GDF_MAGIC or vd[0x7EC:0x7EC + 20] != GDF_MAGIC:
        raise SystemExit("XDVDFS volume descriptor not found at 0x10000; unexpected layout")
    root_sec, root_size = struct.unpack_from("<II", vd, 20)

    total = 0
    count = 0
    entries = list(walk(svod, root_sec, root_size))
    for path, start, fsize, is_dir in entries:
        if not is_dir:
            total += fsize
            count += 1
    print(f"{count} files, {total / 2**30:.2f} GiB")

    if list_only:
        for path, start, fsize, is_dir in entries:
            print(f"{'<dir>':>12}  {path}" if is_dir else f"{fsize:12d}  {path}")
        return

    out = args[1]
    done = 0
    for path, start, fsize, is_dir in entries:
        dst = os.path.join(out, path)
        if is_dir:
            os.makedirs(dst, exist_ok=True)
            continue
        os.makedirs(os.path.dirname(dst) or out, exist_ok=True)
        if os.path.exists(dst) and os.path.getsize(dst) == fsize:
            done += fsize
            continue
        with open(dst, "wb") as fh:
            if fsize:
                svod.copy(start * SECTOR, fsize, fh.write)
        done += fsize
        print(f"[{done * 100 // max(total, 1):3d}%] {path}", flush=True)
    print("done")


if __name__ == "__main__":
    main()
