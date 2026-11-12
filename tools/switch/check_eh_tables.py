#!/usr/bin/env python3
"""Check that every C++ exception table in an ELF points inside its own function part.

The unwinder reads an FDE's exception table (LSDA) relative to the FDE's start. With -freorder-blocks-and-partition and
-fno-reorder-functions, GCC 15.2 keeps a function's .cold part in the hot part's section and writes the cold part's
table from the hot start (#35). Every call site and landing pad in it is then off by the hot part's size: a throw
through the .cold part runs a wrong landing pad. In v1.1.0 menu_assets' Check(bool) jumped into the next function,
which threw again, until the 128 KiB GPU CmdProc stack ran out. v1.1.0 had 1,213 such tables of 5,669.

    python3 tools/switch/check_eh_tables.py out/build/switch-pgo-use/LostOdysseyRecomp/LostOdysseyRecomp.elf

Exit status 1 when a table has a call site or landing pad outside its FDE.
"""
import bisect
import struct
import sys
from pathlib import Path


class Elf:
    def __init__(self, path):
        self.data = Path(path).read_bytes()
        d = self.data
        if d[:4] != b"\x7fELF" or d[4] != 2 or d[5] != 1:
            sys.exit(f"{path}: not a little-endian ELF64 file")
        shoff, = struct.unpack_from("<Q", d, 0x28)
        shentsize, shnum, shstrndx = struct.unpack_from("<HHH", d, 0x3A)
        headers = [struct.unpack_from("<IIQQQQIIQQ", d, shoff + i * shentsize) for i in range(shnum)]
        names = headers[shstrndx]
        self.sections = {}
        for h in headers:
            name_off, kind, _, addr, off, size, link = h[:7]
            end = d.index(b"\0", names[4] + name_off)
            self.sections[d[names[4] + name_off:end].decode()] = (addr, off, size, kind, link)
        self.headers = headers

    def section(self, name):
        return self.sections.get(name)

    def reader(self, name):
        """(address, bytes) of a section, so pc-relative values resolve to addresses."""
        s = self.section(name)
        if not s:
            return None
        addr, off, size = s[:3]
        return addr, self.data[off:off + size]

    def symbols(self):
        """Sorted (address, name) of the function symbols."""
        s = self.section(".symtab")
        if not s:
            return [], []
        _, off, size, _, link = s
        strtab = self.headers[link]
        out = []
        for i in range(0, size, 24):
            name, info, _, _, value, _ = struct.unpack_from("<IBBHQQ", self.data, off + i)
            if info & 0xF == 2 and name:
                start = strtab[4] + name
                out.append((value, self.data[start:self.data.index(b"\0", start)].decode(errors="replace")))
        out.sort()
        return [a for a, _ in out], [n for _, n in out]


class Cursor:
    def __init__(self, base, blob, pos=0):
        self.base, self.blob, self.pos = base, blob, pos

    def u8(self):
        self.pos += 1
        return self.blob[self.pos - 1]

    def uleb(self):
        value = shift = 0
        while True:
            byte = self.u8()
            value |= (byte & 0x7F) << shift
            shift += 7
            if byte < 0x80:
                return value

    def sleb(self):
        value = shift = 0
        while True:
            byte = self.u8()
            value |= (byte & 0x7F) << shift
            shift += 7
            if byte < 0x80:
                return value - (1 << shift) if byte & 0x40 else value

    def encoded(self, enc):
        """A DW_EH_PE value; indirect values are left as the pointer's address (only compared, never followed)."""
        if enc == 0xFF:
            return None
        where = self.base + self.pos
        fmt = enc & 0x0F
        if fmt == 0x01:
            value = self.uleb()
        elif fmt == 0x09:
            value = self.sleb()
        else:
            size, code = {0x00: (8, "<Q"), 0x02: (2, "<H"), 0x03: (4, "<I"), 0x04: (8, "<Q"),
                          0x0A: (2, "<h"), 0x0B: (4, "<i"), 0x0C: (8, "<q")}[fmt]
            value, = struct.unpack_from(code, self.blob, self.pos)
            self.pos += size
        if enc & 0x70 == 0x10:
            value = (value + where) & 0xFFFFFFFFFFFFFFFF
        return value


def fdes(elf):
    """(start, end, lsda address or None) of every FDE in .eh_frame."""
    base, blob = elf.reader(".eh_frame")
    cies = {}
    pos = 0
    while pos + 4 <= len(blob):
        length, = struct.unpack_from("<I", blob, pos)
        if length == 0:
            break
        if length == 0xFFFFFFFF:
            sys.exit("64-bit DWARF .eh_frame entries are not supported")
        cie_id, = struct.unpack_from("<I", blob, pos + 4)
        if cie_id:
            cie_pos = pos + 4 - cie_id
            if cie_pos not in cies:
                c = Cursor(base, blob, cie_pos + 9)
                aug = blob[c.pos:blob.index(b"\0", c.pos)].decode()
                c.pos += len(aug) + 1
                c.uleb(), c.sleb(), c.uleb()
                fde_enc, lsda_enc = 0, 0xFF
                if aug.startswith("z"):
                    c.uleb()
                    for ch in aug[1:]:
                        if ch == "R":
                            fde_enc = c.u8()
                        elif ch == "L":
                            lsda_enc = c.u8()
                        elif ch == "P":
                            c.encoded(c.u8())
                cies[cie_pos] = (aug, fde_enc, lsda_enc)
            aug, fde_enc, lsda_enc = cies[cie_pos]
            c = Cursor(base, blob, pos + 8)
            start = c.encoded(fde_enc)
            size = c.encoded(fde_enc & 0x0F)
            lsda = None
            if aug.startswith("z"):
                c.uleb()
                if lsda_enc != 0xFF:
                    lsda = c.encoded(lsda_enc) or None
            yield start, start + size, lsda
        pos += 4 + length


def call_sites(table, address, start):
    """(site start, site end, landing pad or 0) of the LSDA at address, read for the FDE that starts at start."""
    base, blob = table
    c = Cursor(base, blob, address - base)
    lp_start = c.encoded(c.u8())
    if lp_start is None:
        lp_start = start
    if c.u8() != 0xFF:
        c.uleb()
    site_enc = c.u8()
    end = c.pos + c.uleb()
    while c.pos < end:
        site = c.encoded(site_enc & 0x0F)
        length = c.encoded(site_enc & 0x0F)
        pad = c.encoded(site_enc & 0x0F)
        c.uleb()
        yield start + site, start + site + length, lp_start + pad if pad else 0


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    elf = Elf(sys.argv[1])
    if not elf.section(".eh_frame"):
        sys.exit("no .eh_frame section")
    table = elf.reader(".gcc_except_table")
    addresses, names = elf.symbols()

    def name(address):
        i = bisect.bisect_right(addresses, address) - 1
        return f"{names[i]}+{address - addresses[i]:#x}" if i >= 0 else f"{address:#x}"

    checked = bad = 0
    for start, end, lsda in fdes(elf):
        if not lsda or not table or not table[0] <= lsda < table[0] + len(table[1]):
            continue
        checked += 1
        wrong = [(s, e, p) for s, e, p in call_sites(table, lsda, start)
                 if not start <= s <= e <= end or (p and not start <= p < end)]
        if wrong:
            bad += 1
            if bad <= 10:
                s, e, p = wrong[0]
                pad = f", landing pad {name(p)}" if p else ""
                print(f"{start:#x}-{end:#x} {name(start)}: call site {s:#x}-{e:#x}{pad} is outside it")
    print(f"{bad} of {checked} exception tables point outside their function part")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
