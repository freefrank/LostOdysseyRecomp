"""Reproduce the comparison-switch defect in the published v1.1.0 Android APK.

Requires pyelftools and unicorn. Executes only ARM64 code in an emulator with
controlled guest constants, not an Android process, GPU, audio device or game.
No APK, XEX, or extracted game bytes are stored in this repository.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import struct
import zipfile

from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE, UC_HOOK_MEM_READ
from unicorn.arm64_const import (UC_ARM64_REG_X0, UC_ARM64_REG_X1,
    UC_ARM64_REG_X30, UC_ARM64_REG_PC, UC_ARM64_REG_SP)

APK_SHA = "41ec5d557cc78fdc72feb00754a48e7b2851730c27344eaa1619f6478850877a"
LIB_SHA = "9230eaa36d5ec4045c643872a25cb8865edc79bf36a5a32164625616e2952f70"
LABELS = [0x82F8100C, 0x82F81014, 0x82F8101C, 0x82F81024,
          0x82F8102C, 0x82F81034, 0x82F81040]
CTX, GUEST, MISSING, STOP = 0x10000000, 0x30000000, 0x20000000, 0x20001000
CASES = [(0., 0., 0x28), (1., 0., 0x18), (0., 1., 0x30), (.25, 0., 8),
         (.6, .6, 0x10), (0., .5, 0x20), (.2, .3, None), (1., .5, -1)]


class Native:
    def __init__(self, content, published):
        self.content = content
        self.file = io.BytesIO(content)
        self.elf = ELFFile(self.file)
        self.symbols = {}
        for name in (".dynsym", ".symtab"):
            section = self.elf.get_section_by_name(name)
            if section:
                self.symbols.update({s.name: (int(s["st_value"]), int(s["st_size"]))
                                     for s in section.iter_symbols()})
        self.uc = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
        self.pages = set()
        self.misses = []
        for address, size in [(CTX, 0x12000), (MISSING, 8192),
            (GUEST + 0x501000, 4096), (GUEST + 0x82000000, 4096),
            (GUEST + 0x82007000, 4096), (GUEST + 0x82F80000, 8192),
            (GUEST + 0x84DA2000, 4096)]:
            self.map(address, size)
        # Mirrors only MissingFunction's observed return effect; log collection
        # and host mutexes are outside this controlled native execution test.
        self.uc.mem_write(MISSING, bytes.fromhex("1f0000f9c0035fd6"))
        if published:
            self.load_published()
        else:
            total = sum(s["p_memsz"] for s in self.elf.iter_segments() if s["p_type"] == "PT_LOAD")
            if total > 1024 * 1024:
                raise ValueError("Expected a small, statically linked reduced-fixture ELF")
            for segment in self.elf.iter_segments():
                if segment["p_type"] == "PT_LOAD":
                    self.map(segment["p_vaddr"], segment["p_memsz"])
                    self.uc.mem_write(segment["p_vaddr"], segment.data())
        self.uc.hook_add(UC_HOOK_CODE, self.on_code)

    def map(self, address, size):
        for page in range(address & ~4095, (address + size + 4095) & ~4095, 4096):
            if page not in self.pages:
                self.uc.mem_map(page, 4096)
                self.pages.add(page)

    def read(self, address, size):
        for segment in self.elf.iter_segments():
            if segment["p_type"] == "PT_LOAD" and segment["p_vaddr"] <= address < segment["p_vaddr"] + segment["p_filesz"]:
                self.file.seek(address - segment["p_vaddr"] + segment["p_offset"])
                return self.file.read(size)
        raise ValueError(f"Unmapped ELF address {address:#x}")

    def load_published(self):
        wanted = ["__imp__sub_82F80F48", "__imp__sub_82F80FF0", "__imp__sub_82F81038",
                  "__imp__sub_82F81044"] + [f"__imp__sub_{x:X}" for x in LABELS if x not in (0x82F8100C, 0x82F81040)]
        for name in wanted:
            address, size = self.symbols[name]
            for page in range(address & ~4095, (address + size + 4095) & ~4095, 4096):
                self.map(page, 4096)
                self.uc.mem_write(page, self.read(page, 4096))
        address, size = self.symbols["PPCFuncMappings"]
        raw = self.read(address, size)
        self.known = {struct.unpack_from("<I", raw, i)[0] for i in range(0, size, 16)}
        if 0x82F8100C in self.known or 0x82F81040 in self.known:
            raise ValueError("The pinned binary unexpectedly registers the missing case labels")

    def on_code(self, uc, address, size, data):
        if address == MISSING:
            self.misses.append({"ctr": hex(self.word(0x108)), "lr": hex(self.word(0x100)), "r3": self.word(0)})

    def word(self, offset):
        return struct.unpack("<Q", self.uc.mem_read(CTX + offset, 8))[0]

    def store(self, offset, value):
        self.uc.mem_write(CTX + offset, struct.pack("<Q", value))

    def run(self, name):
        self.uc.reg_write(UC_ARM64_REG_X0, CTX)
        self.uc.reg_write(UC_ARM64_REG_X1, GUEST)
        self.uc.reg_write(UC_ARM64_REG_X30, STOP)
        self.uc.reg_write(UC_ARM64_REG_SP, CTX + 0x11000)
        self.uc.emu_start(self.symbols[name][0], STOP, count=10000)
        if self.uc.reg_read(UC_ARM64_REG_PC) != STOP:
            raise ValueError("Native helper exceeded its bounded instruction budget")
        return self.word(0)

    def recover_table(self):
        self.uc.mem_write(CTX, bytes(8192))
        self.store(0xC0, 0x500000)  # r24: source base of the pointer words decoded as lwz
        self.store(0, 0x28)
        reads = []

        def trace(uc, access, address, size, value, data):
            if GUEST + 0x501000 <= address < GUEST + 0x502000:
                reads.append(address - GUEST - 0x500000)

        hook = self.uc.hook_add(UC_HOOK_MEM_READ, trace)
        self.run("__imp__sub_82F80FF0")
        self.uc.hook_del(hook)
        # The published native translation reads seven volatile words through
        # r24 and leaves the last in r23. PPC lwz r23,disp(r24) encodes 0x82F8dddd.
        labels = [0x82F80000 | offset for offset in reads]
        if labels != LABELS:
            raise ValueError(f"Unexpected recovered table: {labels}")
        return labels

    def prepare_guest(self, published):
        for i, label in enumerate(LABELS):
            self.uc.mem_write(GUEST + 0x82F80FF0 + 4 * i, struct.pack(">I", label))
            if published:
                name = f"__imp__sub_{label:X}"
                target = self.symbols[name][0] if label in self.known and name in self.symbols else MISSING
                offset = 2 * label + 0x7EEA0000 - 0x100000000
                self.uc.mem_write(GUEST + offset, struct.pack("<Q", target))
        # Controlled constants exercise every class; these are not a capture
        # of the reporter's unknown floating-point inputs or game image.
        for offset, value in [(0x82000E50, 0.), (0x82007784, 1.), (0x82000D80, 1.)]:
            self.uc.mem_write(GUEST + offset, struct.pack(">f", value))

    def exercise(self, published):
        self.prepare_guest(published)
        results = []
        for index, (a, b, mask) in enumerate(CASES if published else CASES[:7]):
            for flags in range(64):
                self.uc.mem_write(CTX, bytes(8192))
                self.store(0, flags)
                self.store(0x100, 0x82F86788)
                if published:
                    self.uc.mem_write(CTX + 0x150, struct.pack("<dd", a, b))
                else:
                    self.store(0x58, index)  # reduced fixture skips classification
                expected = 0 if mask == -1 else int(mask is None or flags & mask == 0)
                actual = self.run("__imp__sub_82F80F48")
                results.append(dict(case=index, flags=flags, expected=expected, actual=actual,
                                    ok=actual == expected, boolean_ok=bool(actual) == bool(expected)))
        return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apk", required=True, type=Path)
    parser.add_argument("--candidate-elf", type=Path,
                        help="optional linked ARM64 reduced fixture, not a complete game runtime")
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    apk = args.apk.read_bytes()
    if hashlib.sha256(apk).hexdigest() != APK_SHA:
        raise ValueError("This inspection is pinned to the official Android v1.1.0 APK")
    with zipfile.ZipFile(io.BytesIO(apk)) as archive:
        library = archive.read("lib/arm64-v8a/libmain.so")
    if hashlib.sha256(library).hexdigest() != LIB_SHA:
        raise ValueError("Unexpected published native library")
    old = Native(library, True)
    recovered = old.recover_table()
    before = old.exercise(True)
    summary = dict(apk_sha256=APK_SHA, libmain_sha256=LIB_SHA,
        recovered_pointer_words=[hex(v) for v in recovered], published_checks=len(before),
        wrong_values=sum(not r["ok"] for r in before),
        wrong_boolean_results=sum(not r["boolean_ok"] for r in before),
        missing_dispatches=len(old.misses),
        controlled_reported_signature=next(r for r in before if r["case"] == 6 and r["flags"] == 0x2C))
    if (summary["wrong_values"], summary["wrong_boolean_results"], summary["missing_dispatches"]) != (172, 84, 128):
        raise ValueError("Published native negative control did not reproduce")
    result = dict(summary=summary, published=before, missing=old.misses)
    if args.candidate_elf:
        fixed = Native(args.candidate_elf.read_bytes(), False)
        after = fixed.exercise(False)
        summary.update(reduced_candidate_checks=len(after),
                       reduced_candidate_wrong=sum(not r["ok"] for r in after))
        result["reduced_candidate"] = after
        if summary["reduced_candidate_wrong"] or fixed.misses:
            raise ValueError("The reduced candidate switch changed a case result")
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
