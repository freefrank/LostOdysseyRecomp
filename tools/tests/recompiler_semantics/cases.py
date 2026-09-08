"""Independent exact-integer PPC ISA expectations and bounded instruction words.

ISA: PowerPC User Instruction Set Architecture 2.02, Book I (IBM), effective
addresses p15, CR0 p50, rotate/insert p71-76, arithmetic shifts p79-80.
https://powerpc.dev/general/PPC_Vers202_Book1_public.pdf
No implementation expression is imported from the production generator.
"""
import json

U32 = (1 << 32) - 1
U64 = (1 << 64) - 1
BLR = 0x4E800020


def x(xo, rs=4, ra=3, rb=5, rc=0):
    return 31 << 26 | rs << 21 | ra << 16 | rb << 11 | xo << 1 | rc


def m(op, sh, mb, me, rc=0, rs=4):
    return op << 26 | rs << 21 | 3 << 16 | sh << 11 | mb << 6 | me << 1 | rc


def md(xo, sh, mb, rc=0):
    return (30 << 26 | 4 << 21 | 3 << 16 | (sh & 31) << 11 |
            (mb & 31) << 6 | (mb & 32) | xo << 2 | (sh >> 5) << 1 | rc)


def mask(mb, me):
    # ISA bit numbers run MSB to LSB. Walking the inclusive range independently
    # handles wraparound instead of repeating the emitter's shift formula.
    result, bit = 0, mb
    while True:
        result |= 1 << (63 - bit)
        if bit == me:
            return result
        bit = (bit + 1) % 64


def signed(value, bits):
    value &= (1 << bits) - 1
    return value - (1 << bits) if value >> (bits - 1) else value


def rotate(value, amount, bits=64):
    amount %= bits
    return ((value << amount) | (value >> ((bits - amount) % bits))) & ((1 << bits) - 1)


def word_rotate(value, amount):
    word = value & U32
    return rotate((word << 32) | word, amount)


def cr0(value, so):
    value = signed(value, 32)
    return (int(value < 0) << 3) | (int(value > 0) << 2) | (int(value == 0) << 1) | so


def literal(value):
    return f"0x{value & U64:X}ULL"


def write_cases(output):
    functions, metadata, scalars, bodies = {}, [], [], []

    def function(name, words, skip_lr=False):
        spec = (int(skip_lr), tuple(words))
        assert name not in functions or functions[name] == spec
        functions[name] = spec
        return name

    def instruction(name, word):
        return function(name, [word, BLR])

    def identify(group, name, **values):
        index = len(metadata)
        metadata.append(dict(id=index, group=group, name=name, **values))
        return index

    def scalar(group, name, a, b, expected, *, old=0x1122334455667788,
               alias=0, so=1, ca=-1, rc=False, result_mask=U64):
        regs = [old, a & U64, b & U64]
        if alias == 1:
            regs[0] = a & U64  # RA=RS=r3
        dest = 2 if alias == 2 else 0  # RA=RB=r5
        expected &= result_mask
        record = cr0(expected, so) if rc else 10
        index = identify(group, name, registers=regs, result_register=dest + 3,
            expected=expected, mask=result_mask, expected_ca=ca, expected_cr0=record, so=so)
        scalars.append(f'{{{index}, __imp__{name}, {{{", ".join(map(literal, regs))}}}, '
            f'{literal(expected)}, {literal(result_mask)}, {dest}, {ca}, {record}, {so}}},')

    # Saturating arithmetic shifts: amount uses six/seven low bits, while CA
    # depends on ALL shifted-out original bits. Python has no host shift UB.
    for bits, mnemonic, xo in [(32, "sraw", 792), (64, "srad", 794)]:
        for alias in range(3):
            for rc in range(2):
                name = instruction(f"{mnemonic}_a{alias}_rc{rc}",
                    x(xo, rs=3 if alias == 1 else 4, ra=5 if alias == 2 else 3, rc=rc))
                for source in [0, 8, (1 << (bits - 1)) - 1, 1 << (bits - 1),
                               (1 << (bits - 1)) + 1, (1 << bits) - 1]:
                    for amount in [0, 1, bits - 1, bits, bits + 1, 2 * bits - 1,
                                   2 * bits, (1 << 32) + bits]:
                        n = amount % (2 * bits)
                        value = signed(source, bits)
                        expected = value >> n
                        carry = int(value < 0 and (source & ((1 << n) - 1)) != 0)
                        scalar(mnemonic + "_ca", name, source, amount, expected,
                            alias=alias, ca=carry, rc=bool(rc), so=(amount & 1))

    # All 1,024 MB/ME pairs, normal and source/destination alias forms.
    for mb in range(32):
        for me in range(32):
            for alias in [False, True]:
                amount = (mb + 3 * me) % 32
                name = instruction(f"rlwimi_{mb}_{me}_{int(alias)}",
                    m(20, amount, mb, me, rs=3 if alias else 4))
                source = 0xABCDEF0012345678 if not alias else 0x1122334455667788
                bitmask = mask(mb + 32, me + 32)
                expected = (word_rotate(source, amount) & bitmask) | (0x1122334455667788 & ~bitmask)
                scalar("rlwimi", name, source, 0, expected, alias=int(alias))

    # Exact results plus 32-bit-mode CR0 and both sticky SO states. Undefined
    # upper words of mulhw are deliberately masked out; signed division avoids
    # zero divisors and MIN/-1, whose result is not defined by this contract.
    def record_result(name, a, b):
        old = 0x1122334455667788
        if name == "cntlzw": return 32 - (a & U32).bit_length()
        if name == "cntlzd": return 64 - a.bit_length()
        if name == "mulhw": return (signed(a, 32) * signed(b, 32)) >> 32
        if name == "mulld": return signed(a, 64) * signed(b, 64)
        if name == "divd": return (-1 if signed(a, 64) < 0 else 1) * (abs(signed(a, 64)) // b)
        if name == "nand": return ~(a & b)
        if name == "nor": return ~(a | b)
        if name == "orc": return a | ~b
        if name == "rldicl": return rotate(a, 5) & mask(4, 63)
        if name == "rldicr": return rotate(a, 5) & mask(0, 50)
        if name == "rldimi": return (rotate(a, 5) & mask(4, 58)) | (old & ~mask(4, 58))
        if name == "rotldi": return rotate(a, 5)
        if name == "rotlw": return rotate(a & U32, b, 32)
        if name == "sld": return a << b
        if name == "srd": return a >> b
        if name in ("srad", "sradi"): return signed(a, 64) >> b
        if name == "rlwimi": return (word_rotate(a, 5) & mask(40, 55)) | (old & ~mask(40, 55))
        if name == "slw": return (a << b) & U32
        if name == "srw": return (a & U32) >> b
        if name == "sraw": return signed(a, 32) >> b
        return signed(a, {"extsb": 8, "extsh": 16, "extsw": 32}[name])

    record_words = {
        "cntlzw": x(26, rb=0, rc=1), "cntlzd": x(58, rb=0, rc=1),
        "mulhw": x(75, rs=3, ra=4, rc=1), "mulld": x(233, rs=3, ra=4, rc=1),
        "divd": x(489, rs=3, ra=4, rc=1), "nand": x(476, rc=1),
        "nor": x(124, rc=1), "orc": x(412, rc=1),
        "rldicl": md(0, 5, 4, 1), "rldicr": md(1, 5, 50, 1),
        "rldimi": md(3, 5, 4, 1), "rotldi": md(0, 5, 0, 1),
        "rotlw": m(23, 5, 0, 31, 1), "sld": x(27, rc=1),
        "srd": x(539, rc=1), "srad": x(794, rc=1), "sradi": x(826, rb=2, rc=1),
        "rlwimi": m(20, 5, 8, 23, 1), "slw": x(24, rc=1), "srw": x(536, rc=1),
        "sraw": x(792, rc=1), "extsb": x(954, rb=0, rc=1),
        "extsh": x(922, rb=0, rc=1), "extsw": x(986, rb=0, rc=1),
    }
    controls = {"slw", "srw", "sraw", "extsb", "extsh", "extsw"}
    for mnemonic, word in record_words.items():
        name = instruction("record_" + mnemonic, word)
        for source in [0, 8, 0xFFFFFFFF80000001, 0x100000008]:
            for so in range(2):
                scalar("rc_controls" if mnemonic in controls else "rc_missing", name, source, 2,
                    record_result(mnemonic, source, 2), so=so, rc=True,
                    result_mask=U32 if mnemonic == "mulhw" else U64)

    def body_case(group, name, setup, condition, actual, expected, **info):
        index = identify(group, name, **info)
        bodies.append('{ PPCContext ctx{};\n' + setup +
            f'\nresult({index}, {condition}, {actual}, {literal(expected)});\n}}')

    # Update-form accesses all target small committed guest pages; full-width
    # arithmetic and the low-word address are independently checked. Stores
    # with RA=RS must write the pre-update value, including stwu r1-style use.
    updates = {"lbzu": (35, 1, False), "lwzu": (33, 4, False), "ldu": (58, 8, False),
               "stbu": (39, 1, True), "stwu": (37, 4, True), "stdu": (62, 8, True),
               "stwux": (0, 4, True)}
    for mnemonic, (op, size, store) in updates.items():
        for alias in ([False, True] if store else [False]):
            for label, ra, offset in [("plain", 0xDEADBEEF000000F0, 0x1020),
                    ("carry", 0xDEADBEEFFFFFFFF0, 0x1020),
                    ("borrow", 0xDEADBEEF00000010, -0x20),
                    ("wrap", 0xFFFFFFFFFFFFFFF0, 0x1020)]:
                rs = 3 if alias else 5
                name = f"{mnemonic}_{label}_a{int(alias)}"
                word = x(183, rs=rs, ra=3, rb=4) if not op else (
                    op << 26 | rs << 21 | 3 << 16 | (offset & 0xFFFF) | int(size == 8))
                instruction(name, word)
                expected_ra = (ra + offset) & U64
                address = expected_ra & U32
                value = ra if alias else 0x8877665544332211
                expected_value = (value if store else 0x123456789ABCDEF1) & ((1 << (8 * size)) - 1)
                setup = (f'ctx.r3.u64={literal(ra)};ctx.r4.u64={literal(offset)};ctx.r5.u64={literal(value)};'
                    f'seed(base+{literal(address)},0x123456789ABCDEF1ULL,{size});'
                    f'__imp__{name}(ctx,base);')
                memory_value = f'read(base+{literal(address)},{size})'
                condition = f'ctx.r3.u64=={literal(expected_ra)} && '
                condition += (memory_value if store else 'ctx.r5.u64') + f'=={literal(expected_value)}'
                body_case("update_ea", name, setup, condition, 'ctx.r3.u64', expected_ra,
                    ra=ra, offset=offset, expected_ea=expected_ra, store_alias=alias)
        if mnemonic == "stwux":
            name = instruction("stwux_rb_high", x(183, rs=5, ra=3, rb=4))
            ra, rb = 0x12345678FFFFFFF0, 0xDEADBEEF00001020
            ea = (ra + rb) & U64
            body_case("update_ea", name,
                f'ctx.r3.u64={literal(ra)};ctx.r4.u64={literal(rb)};ctx.r5.u64=0x12345678;'
                f'__imp__{name}(ctx,base);',
                f'ctx.r3.u64=={literal(ea)} && read(base+0x1010,4)==0x12345678',
                'ctx.r3.u64', ea, ra=ra, rb=rb)

    # Atomic paths use real volatile/interlocked loads/stores. Decoys at
    # base+4GiB+0x1010 distinguish a pointer-first sum from 32-bit guest wrap.
    for zero in [False, True]:
        for wide in [False, True]:
            for pair in [False, True]:
                size = 8 if wide else 4
                name = f"atomic_{size}_zero{int(zero)}_pair{int(pair)}"
                ra = 0 if zero else 3
                words = [x(84 if wide else 20, rs=5, ra=ra, rb=4)]
                if pair: words.append(x(214 if wide else 150, rs=6, ra=ra, rb=4, rc=1))
                function(name, words + [BLR])
                for wrap in ([False] if zero else [False, True]):
                    a = 0xDEADBEEFFFFFFFF0 if wrap else 0
                    b = 0x1234567800000000 + (0x1020 if wrap else 0x1010)
                    wanted = 0x1122334455667788 if wide else 0x11223344
                    stored = 0x9988776655443322 if wide else 0x55443322
                    setup = (f'seed(base+0x1010,0x1122334455667788ULL,8);'
                        f'seed(base+0x100001010ULL,0xAABBCCDDEEFF0011ULL,8);'
                        f'ctx.r3.u64={literal(a)};ctx.r4.u64={literal(b)};ctx.r6.u64=0x9988776655443322ULL;'
                        f'ctx.xer.so=1;__imp__{name}(ctx,base);')
                    cond = f'ctx.r5.u64=={literal(wanted)}'
                    if pair:
                        cond += (f' && crBits(ctx.cr0)==3 && read(base+0x1010,{size})=={literal(stored)}'
                            ' && read(base+0x100001010ULL,8)==0xAABBCCDDEEFF0011ULL')
                    body_case("atomic_wrap", name, setup, cond, 'ctx.r5.u64', wanted, wrap=wrap)

    # All eight base memory macros are exercised through decoded D/DS-form
    # instructions, not direct macro calls, with absolute and register controls.
    for size, load_op, store_op in [(1, 34, 38), (2, 40, 44), (4, 32, 36), (8, 58, 62)]:
        for store in [False, True]:
            for mode in ["negative", "positive", "register"]:
                immediate = -16 if mode == "negative" else 0x1010 if mode == "positive" else 0
                ra = 3 if mode == "register" else 0
                name = instruction(f"absolute_{size}_{int(store)}_{mode}",
                    (store_op if store else load_op) << 26 | 5 << 21 | ra << 16 | (immediate & 0xFFFF))
                address = 0x1010 if mode == "positive" else 0xFFFFFFF0
                val = 0x8877665544332211 & ((1 << (8 * size)) - 1)
                expect = val if store else 0x123456789ABCDEF1 & ((1 << (8 * size)) - 1)
                setup = (f'seed(base+{literal(address)},0x123456789ABCDEF1ULL,{size});'
                    f'seed(base-16,0xFEDCBA9876543210ULL,{size});ctx.r3.u64=0xDEADBEEFFFFFFFF0ULL;'
                    f'ctx.r5.u64={literal(val)};__imp__{name}(ctx,base);')
                actual = f'read(base+{literal(address)},{size})' if store else 'ctx.r5.u64'
                cond = actual + f'=={literal(expect)}'
                if store: cond += f' && read(base-16,{size})=={literal(0xFEDCBA9876543210 & ((1 << (size * 8)) - 1))}'
                body_case("absolute_address", name, setup, cond, actual, expect)

    # Full BI selection and low-word CTR termination, including decrement
    # carrying into/out of the high word. Each branch returns a distinct value.
    fields = ["lt", "gt", "eq", "so"]
    for bi in range(32):
        name = function(f"bdnzf_{bi}", [16 << 26 | bi << 16 | 12,
            0x3860000B, BLR, 0x38600016, BLR])
        for value in [0, 1]:
            for ctr in [0, 1, 2, 0x100000001, 0x100000002]:
                after = (ctr - 1) & U64
                expected = 22 if (after & U32) != 0 and value == 0 else 11
                # Opposite EQ catches incorrect BI%4 routing; set target last.
                setup = (f'ctx.ctr.u64={literal(ctr)};ctx.cr{bi // 4}.eq={1-value};'
                    f'ctx.cr{bi // 4}.{fields[bi % 4]}={value};__imp__{name}(ctx,base);')
                body_case("bdnzf_bi", name, setup,
                    f'ctx.r3.u64=={expected} && ctx.ctr.u64=={literal(after)}',
                    'ctx.r3.u64', expected, bi=bi, bit=value, initial_ctr=ctr)

    # Old LR must be captured before LK writes PC+4; call returns to following
    # code, whereas tail BCTR/BNECTR stop there. Include skipLr configurations.
    for mnemonic, word in [("blrl", 0x4E800021), ("bctrl", 0x4E800421),
                           ("bctr", 0x4E800420), ("bnectr", 0x4C820420)]:
        for skip in [False, True]:
            name = function(f"{mnemonic}_skip{int(skip)}", [word, 0x38C60001, BLR], skip)
            for low in range(4):
                for taken in ([False, True] if mnemonic == "bnectr" else [True]):
                    initial_lr = 0xDEADBEEF00002000 + low
                    seen_lr = 0x1004 if mnemonic in ("blrl", "bctrl") and not skip else initial_lr
                    expected_after = int(not taken or mnemonic in ("blrl", "bctrl"))
                    setup = (f'calls=0;targetSeen=0;linkSeen=0;trapSeen=false;ctx.lr={literal(initial_lr)};'
                        f'ctx.ctr.u64={literal(0x1234567800002000 + low)};ctx.cr0.eq={int(not taken)};'
                        f'__imp__{name}(ctx,base);')
                    cond = f'!trapSeen && calls=={int(taken)} && ctx.r6.u64=={expected_after}'
                    cond += (f' && targetSeen==0x2000 && linkSeen=={literal(seen_lr)} && ctx.r7.u64==1'
                             if taken else f' && ctx.lr=={literal(initial_lr)}')
                    body_case("blrl" if mnemonic == "blrl" else "ctr_alignment", name,
                        setup, cond, 'targetSeen', 0x2000 if taken else 0, low_bits=low, taken=taken, skip_lr=skip)

    # A callee changes host flush mode. A following scalar FP operation must
    # re-establish its mode even if the same mode was selected before BLRL.
    fadds = 59 << 26 | 3 << 21 | 4 << 16 | 5 << 11 | 21 << 1
    name = function("blrl_csr", [fadds, 0x4E800021, fadds, BLR])
    body_case("blrl", name,
        'calls=0;trapSeen=false;ctx.lr=0x2000;ctx.f4.f64=1.0;ctx.f5.f64=2.0;'
        f'__imp__{name}(ctx,base);',
        '!trapSeen && calls==1 && ctx.f3.f64==3.0 && (ctx.fpscr.getcsr() & PPCFPSCRRegister::FlushMask)==0',
        'ctx.fpscr.getcsr() & PPCFPSCRRegister::FlushMask', 0)

    (output / "instructions.txt").write_text("".join(
        f'{name} {skip} ' + " ".join(f"{word:08x}" for word in words) + "\n"
        for name, (skip, words) in functions.items()), encoding="utf-8")
    (output / "cases.json").write_text(json.dumps(metadata, indent=2), encoding="utf-8")
    source = '''struct ScalarCase {
unsigned id; PPCFunc* fn; uint64_t regs[3], expected, mask;
unsigned dest; int ca; unsigned cr, so;
};
static const ScalarCase scalarCases[]{
''' + "\n".join(scalars) + '''
};
static void runCases(uint8_t* base) {
for (const auto& test : scalarCases) {
    PPCContext ctx{};
    ctx.r3.u64=test.regs[0];ctx.r4.u64=test.regs[1];ctx.r5.u64=test.regs[2];
    ctx.xer.so=test.so;ctx.xer.ca=1;ctx.cr0.lt=1;ctx.cr0.eq=1;
    test.fn(ctx,base);
    const uint64_t value=(test.dest==0?ctx.r3.u64:test.dest==1?ctx.r4.u64:ctx.r5.u64)&test.mask;
    const bool ok=value==test.expected && (test.ca<0 || ctx.xer.ca==test.ca) && crBits(ctx.cr0)==test.cr;
    result(test.id,ok,value,test.expected);
}
''' + "\n".join(bodies) + "\n}\n"
    (output / "cases.inc").write_text(source, encoding="utf-8")
    return metadata
