#!/usr/bin/env python3
"""Translate one recompiled function's PPC listing into a C++ native body.

Differences from XenonRecomp's output:
- the function's private stack slots (N(r1) with N < SLOT_LIMIT, or at/above the frame size) are C++ locals;
- single-precision fadds/fsubs/fmuls run in float (double rounding through double is innocuous for them);
  fmadds/fmsubs are computed in double, then rounded to float, as the recompiled code does.
Usage: ppc2native.py LISTING START_ADDRESS FRAME_SIZE FUNC_NAME > out.inc
LISTING: the '// insn' comment lines and 'loc_XXXXXXXX:' labels of the recompiled function, in order.
"""
import re, sys

listing, start, frame, name = sys.argv[1], int(sys.argv[2], 16), int(sys.argv[3]), sys.argv[4]
# Optional: LOOP_START LOOP_END ENTRY_BRANCH (addresses) to promote the loop's fixed-address memory to locals.
loop = tuple(int(x, 16) for x in sys.argv[5:8]) if len(sys.argv) >= 8 else None
SLOT_LIMIT = 384
SKIP_CALLS = {0x82B7A6B0, 0x82B7B130}          # savegprlr_14, savefpr_14 (prologue)
RESTFPR = 0x82B7B17C
RESTGPRLR = 0x82B7A700

lines = []
for raw in open(listing):
    raw = raw.rstrip('\n')
    m = re.match(r'^loc_([0-9A-F]{8}):', raw.strip())
    if m:
        lines.append(('label', int(m.group(1), 16)))
        continue
    t = raw.strip()
    if t.startswith('//'):
        t = t[2:].strip()
    if t:
        lines.append(('insn', t))

# Instruction addresses: sequential from the start; labels must agree.
addr = start
items = []
for kind, v in lines:
    if kind == 'label':
        if v != addr:
            sys.exit(f'label {v:#x} at computed address {addr:#x}')
        items.append(('label', v, None))
    else:
        items.append(('insn', addr, v))
        addr += 4
labels = {a for k, a, _ in items if k == 'label'}

def slot(off):
    return off < SLOT_LIMIT or off >= frame

# ---- loop promotion analysis --------------------------------------------------------------------------------------
# In the loop, a pointer is fixed when it is a register the loop never writes, or a value loaded from a frame slot the
# loop never stores; a memory operand at such a pointer plus a constant has a fixed address. Those addresses get C++
# locals: read once before the loop, then loads read the local and stores write the local and the memory. Assumes the
# loop's other stores (indexed, at addresses that change) never land on them; Check (LO_NATIVE_*=2) verifies the
# result, and the native compares the fixed addresses at loop entry and logs (once) if two are the same.
promote = {}   # instruction address -> (kind 'load'/'store', key)
dropped, write_through, dirty = set(), set(), []
keys = []      # (base, offset) in first-seen order
def parse_mem(x):
    m = re.fullmatch(r'(-?\d+)\((r\d+)\)', x)
    return (int(m.group(1)), m.group(2)) if m else None
if loop:
    lstart, lend, lentry = loop
    body = [(a, t) for k, a, t in items if k == 'insn' and lstart <= a <= lend]
    labels_in = {a for k, a, _ in items if k == 'label' and lstart <= a <= lend}
    written = set(); stored_slots = set()
    for a, t in body:
        op, _, rest = t.partition(' ')
        args = [x.strip() for x in rest.split(',')] if rest else []
        if op in ('stw', 'stfs'):
            mem = parse_mem(args[1])
            if mem and mem[1] == 'r1': stored_slots.add(mem[0])
        elif op not in ('cmpwi', 'cmplwi', 'dcbt', 'stfsx', 'bne', 'beq', 'blt', 'bge', 'bgt', 'ble', 'b') and args and re.fullmatch(r'r\d+', args[0]):
            written.add(args[0])
    # registers assigned between a forward branch and its target lose what is known about them at the target
    clear_at = {}
    for i, (a, t) in enumerate(body):
        op, _, rest = t.partition(' ')
        if op in ('bne', 'beq', 'blt', 'bge', 'bgt', 'ble'):
            tgt = int(rest.split(',')[-1], 16)
            if a < tgt <= lend:
                regs = set()
                for b2, t2 in body:
                    if a < b2 < tgt:
                        o2, _, r2 = t2.partition(' ')
                        a2 = [x.strip() for x in r2.split(',')] if r2 else []
                        if a2 and re.fullmatch(r'r\d+', a2[0]) and o2 not in ('stw', 'stfs', 'cmpwi', 'cmplwi', 'dcbt'): regs.add(a2[0])
                clear_at.setdefault(tgt, set()).update(regs)
    # Where each base points at loop entry: the setup code from the start up to the loop, evaluated on
    # (object, offset) values for the object r3 points to at entry, and (sp, offset) for the frame.
    val = {'r3': ('obj', 0), 'r1': ('sp', 0)}
    slotval = {}
    for k2, a2, t2 in items:
        if k2 != 'insn' or a2 >= lstart: continue
        o2, _, r2 = t2.partition(' ')
        g = [x.strip() for x in r2.split(',')] if r2 else []
        if o2 == 'bl':
            if int(g[0], 16) in SKIP_CALLS or int(g[0], 16) == RESTFPR: continue  # register saves keep r3..r12
            for r in ['r0'] + [f'r{i}' for i in range(3, 13)]: val.pop(r, None)
            continue
        if o2 == 'stwu': continue
        if o2 == 'mr' or (o2 == 'rotlwi' and int(g[2]) == 0):
            if g[1] in val: val[g[0]] = val[g[1]]
            else: val.pop(g[0], None)
            continue
        if o2 == 'li': val[g[0]] = ('const', int(g[1])); continue
        if o2 == 'lis': val[g[0]] = ('const', int(g[1]) << 16); continue
        if o2 == 'ori':
            if g[1] in val and val[g[1]][0] == 'const': val[g[0]] = ('const', val[g[1]][1] | int(g[2], 0))
            else: val.pop(g[0], None)
            continue
        if o2 == 'add':
            x, y = val.get(g[1]), val.get(g[2])
            if x and y and x[0] == 'const' and y[0] != 'const': x, y = y, x
            if x and y and y[0] == 'const': val[g[0]] = (x[0], x[1] + y[1])
            else: val.pop(g[0], None)
            continue
        if o2 in ('addi', 'addis'):
            k = int(g[2]) << (16 if o2 == 'addis' else 0)
            if g[1] in val: val[g[0]] = (val[g[1]][0], val[g[1]][1] + k)
            else: val.pop(g[0], None)
            continue
        if o2 == 'rlwinm' and int(g[2]) == 0 and g[1] in val and val[g[1]][0] == 'sp':
            val[g[0]] = val[g[1]]  # an alignment of a frame pointer: still the frame (offset approximate)
            continue
        if o2 == 'subf':  # rd = rb - ra
            x, y = val.get(g[2]), val.get(g[1])
            if x and y and x[0] == y[0]: val[g[0]] = ('const', x[1] - y[1])
            elif x and y and y[0] == 'const': val[g[0]] = (x[0], x[1] - y[1])
            else: val.pop(g[0], None)
            continue
        if o2 == 'stw':
            mem = parse_mem(g[1])
            if mem and mem[1] == 'r1':
                if g[0] in val: slotval[mem[0]] = val[g[0]]
                else: slotval.pop(mem[0], None)
            continue
        if o2 == 'lwz':
            mem = parse_mem(g[1])
            if mem and mem[1] == 'r1' and mem[0] in slotval: val[g[0]] = slotval[mem[0]]
            else: val.pop(g[0], None)
            continue
        if g and re.fullmatch(r'r\d+', g[0]) and o2 not in ('stfs', 'stfsx', 'cmpwi', 'cmplwi', 'dcbt', 'stvx128'):
            val.pop(g[0], None)
    def located(key):
        (kind, what), off = key
        v = slotval.get(what) if kind == 'slot' else val.get(what)
        return None if v is None or v[0] not in ('obj', 'sp', 'const') else (v[0], v[1] + off)

    # Registers whose every write in the loop is 'addi r, r, k'.
    carried_by_add = set()
    writes = {}
    for a, t in body:
        op, _, rest = t.partition(' ')
        args = [x.strip() for x in rest.split(',')] if rest else []
        if args and re.fullmatch(r'r\d+', args[0]) and op not in ('stw', 'stfs', 'stfsx', 'cmpwi', 'cmplwi', 'dcbt', 'bne', 'beq', 'blt', 'bge', 'bgt', 'ble', 'b'):
            writes.setdefault(args[0], []).append((op, args))
    for r, ws in writes.items():
        if all(op == 'addi' and args[1] == r for op, args in ws): carried_by_add.add(r)

    def scan(key_ranges):
        """One pass over the loop: fixed-address accesses, indexed accesses with their ranges, and the range of every
        value stored at a fixed address (None when one of them is not known)."""
        sym, rng = {}, {}
        # Registers the loop only advances by constants (r = r + k), starting from a frame pointer: stay in the frame.
        frame_carried = {r for r in carried_by_add if val.get(r, (None,))[0] == 'sp'}
        def base_of(r):
            if r in sym: return sym[r]
            if r not in written: return (('reg', r), 0)
            return None
        def in_frame(r):
            if r in frame_carried: return True
            b = base_of(r)
            w = located(b) if b is not None else None
            return w is not None and w[0] == 'sp'
        accesses, dynamic, stored = [], [], {}
        slotrng = {}      # ranges carried through the frame's slots (an index spilled and reloaded)
        for a, t in body:
            if a in clear_at:
                for r in clear_at[a]: sym.pop(r, None); rng.pop(r, None)
            op, _, rest = t.partition(' ')
            args = [x.strip() for x in rest.split(',')] if rest else []
            if op in ('lfsx', 'stfsx'):
                ra, rb = args[1], args[2]
                def kind_of(r):
                    b = base_of(r)
                    w = located(b) if b is not None else None
                    return w[0] if w is not None else None
                # A frame pointer plus anything but an object pointer: the frame, never the object (the offsets here
                # are constants, indexes and per-sample counters within one pass; the Check mode verifies).
                if (in_frame(ra) and kind_of(rb) != 'obj') or (in_frame(rb) and kind_of(ra) != 'obj'):
                    dynamic.append(('load' if op == 'lfsx' else 'store', ('sp', 0), None)); continue
                idx, bas = (ra, rb) if ra in rng else (rb, ra)
                b = base_of(bas)
                where = located(b) if b is not None else None
                if where is not None and where[0] == 'sp':
                    dynamic.append(('load' if op == 'lfsx' else 'store', where, None)); continue  # the frame: never the object
                if where is not None and idx in rng:
                    dynamic.append(('load' if op == 'lfsx' else 'store', where, (rng[idx][0], rng[idx][1] + 3)))
                else:
                    if key_ranges is not None and key_ranges is not EMPTY:
                        sys.stderr.write(f'  indexed {op} at {a:#x} unknown: {t} index {idx} {rng.get(idx)} base {bas} {b} {where}\n')
                    dynamic.append(('load' if op == 'lfsx' else 'store', None, None))
                continue
            if op == 'clrlwi':
                rng[args[0]] = (0, (1 << (32 - int(args[2]))) - 1); sym.pop(args[0], None); continue
            if op == 'rlwinm' and args[1] in rng and int(args[3]) == 0 and int(args[4]) == 31 - int(args[2]):
                lo, hi = rng[args[1]]; sh = int(args[2])
                rng[args[0]] = (lo << sh, hi << sh); sym.pop(args[0], None); continue
            if op == 'addi' and args and args[1] in rng:
                lo, hi = rng[args[1]]; k = int(args[2])
                rng[args[0]] = (lo + k, hi + k); sym.pop(args[0], None); continue
            if op in ('lwz', 'lfs', 'stw', 'stfs'):
                d, ra = parse_mem(args[1])
                where = None
                if ra != 'r1':
                    b = base_of(ra)
                    if b is not None:
                        key = (b[0], b[1] + d)
                        accesses.append((a, 'load' if op in ('lwz', 'lfs') else 'store', key))
                        where = located(key)
                if op == 'stw' and where is not None:
                    r_ = rng.get(args[0])
                    prev = stored.get(where, 'none')
                    if r_ is None or prev is None: stored[where] = None
                    elif prev == 'none': stored[where] = r_
                    else: stored[where] = (min(prev[0], r_[0]), max(prev[1], r_[1]))
                if op == 'stw' and ra == 'r1':
                    if args[0] in rng: slotrng[d] = rng[args[0]]
                    else: slotrng.pop(d, None)
                if op == 'lwz':
                    rng.pop(args[0], None)
                    if where is not None and key_ranges.get(where): rng[args[0]] = key_ranges[where]
                    if ra == 'r1' and d in slotrng: rng[args[0]] = slotrng[d]
                    if ra == 'r1' and d not in stored_slots and slot(d): sym[args[0]] = (('slot', d), 0)
                    else: sym.pop(args[0], None)
                continue
            if args and re.fullmatch(r'r\d+', args[0]) and op not in ('stw', 'stfs', 'stfsx', 'cmpwi', 'cmplwi', 'dcbt'):
                rng.pop(args[0], None)
            if op == 'addi' and args:
                b = base_of(args[1])
                if b is not None: sym[args[0]] = (b[0], b[1] + int(args[2]))
                else: sym.pop(args[0], None)
                continue
            if op == 'mr':
                b = base_of(args[1])
                if b is not None: sym[args[0]] = b
                else: sym.pop(args[0], None)
                if args[1] in rng: rng[args[0]] = rng[args[1]]
                continue
            if args and re.fullmatch(r'r\d+', args[0]) and op not in ('stw', 'stfs', 'stfsx', 'cmpwi', 'cmplwi', 'dcbt'):
                sym.pop(args[0], None)
        return accesses, dynamic, stored

    # A word the loop only stores masked values in (a delay line's index) holds such a value when loaded, assuming it
    # came into the loop the same way (verified by the Check mode). Two passes: the stores' ranges, then the loads.
    EMPTY = {}
    _, _, stored = scan(EMPTY)
    key_ranges = {w: r for w, r in stored.items() if r not in (None, 'none')}
    accesses, dynamic, _ = scan(key_ranges)

    unresolved = 0
    for a, kind, key in accesses:
        where = located(key)
        if where is None or where[0] != 'obj':
            unresolved += 1
            sys.stderr.write(f'  unresolved {kind} at {a:#x}: {key}\n')
            continue
        ck = ('obj', where[1])
        if ck not in keys: keys.append(ck)
        promote[a] = (kind, keys.index(ck))
    sys.stderr.write(f'loop: {unresolved} accesses with unknown bases left in memory\n')
    # Indexed accesses as object ranges. A fixed word an indexed store can reach is not promoted; one an indexed load
    # can reach is written through at every store; the others are written back once, after the loop.
    ranges = []
    unknown_dynamic = False
    for kind, where, r in dynamic:
        if where is not None and where[0] == 'sp': continue
        if where is None or r is None:
            unknown_dynamic = True; continue
        ranges.append((kind, where[1] + r[0], where[1] + r[1]))
    def reached(off, kind):
        return any(k == kind and lo <= off + 3 and off <= hi for k, lo, hi in ranges)
    dropped = {k for k, key in enumerate(keys) if reached(key[1], 'store')}
    write_through = set(range(len(keys))) if unknown_dynamic else {k for k, key in enumerate(keys) if reached(key[1], 'load')}
    for a in list(promote):
        if promote[a][1] in dropped: del promote[a]
    dirty = sorted({k for a, (kind, k) in promote.items() if kind == 'store' and k not in write_through})
    sys.stderr.write(f'loop: {len(ranges)} indexed accesses in the object, unknown {unknown_dynamic}; {len(dropped)} words '
                     f'not promoted, {len(write_through)} written through, {len(dirty)} written back after the loop\n')
    sys.stderr.write(f'loop: {len(keys)} fixed addresses, {sum(1 for v in promote.values() if v[0]=="load")} loads, '
                     f'{sum(1 for v in promote.values() if v[0]=="store")} stores promoted\n')

def key_address(key):
    return f'uint32_t(object + {key[1]})'

out = []
slots = set()
calls = set()
def emit(s): out.append('    ' + s)

def gpr(r):
    return 'sp' if r == 'r1' else r

def ea(d, ra):
    if ra == 'r0':
        return f'uint32_t({d})'
    return f'uint32_t({gpr(ra)} + {d})' if d else gpr(ra)

def mask(mb, me):
    m = 0
    if mb <= me:
        for i in range(mb, me + 1): m |= 1 << (31 - i)
    else:
        for i in range(0, me + 1): m |= 1 << (31 - i)
        for i in range(mb, 32): m |= 1 << (31 - i)
    return m

used_gpr, used_fpr, used_vr = set(), set(), set()
for kind, a, t in items:
    if kind == 'label':
        emit(f'loc_{a:08X}:;')
        continue
    op, _, args = t.partition(' ')
    args = [x.strip() for x in args.split(',')] if args else []
    for x in args:
        if re.fullmatch(r'r\d+', x) and x != 'r1': used_gpr.add(x)
        if re.fullmatch(r'f\d+', x): used_fpr.add(x)
        if re.fullmatch(r'v\d+', x): used_vr.add(x)
        m = re.fullmatch(r'-?\d+\((r\d+)\)', x)
        if m and m.group(1) != 'r1': used_gpr.add(m.group(1))
    c = f'// {t}'
    def dmem(x):
        m = re.fullmatch(r'(-?\d+)\((r\d+)\)', x)
        return int(m.group(1)), m.group(2)
    if op in ('mflr', 'dcbt'):
        emit(c + ' (no effect here)'); continue
    if op == 'ld':
        d, ra = dmem(args[1])
        if ra == 'r1' and d < 0: emit(c + ' (stack probe)'); continue
        sys.exit('ld ' + t)
    if op == 'stwu':
        emit(c + ' (frame set up at entry)'); continue
    if loop and a == loop[2]:
        # loop entry: read every promoted word once (keys are distinct offsets in the object)
        for k, key in enumerate(keys):
            if k not in dropped: emit(f'm{k} = PPC_FAST_LOAD_U32({key_address(key)});')
    if loop and a == loop[1] + 4:
        # loop exit: the words stored in the loop and not written through
        for k in dirty:
            emit(f'PPC_FAST_STORE_U32({key_address(keys[k])}, m{k}); // write-back after the loop')
    if a in promote:
        kind, k = promote[a]
        reg = args[0]
        d, ra = dmem(args[1])
        e = ea(d, ra)
        if op == 'lwz': emit(f'{reg} = m{k}; {c} [fixed]')
        elif op == 'lfs': emit(f'{reg} = F(m{k}); {c} [fixed]')
        elif k not in write_through:
            emit(f'm{k} = {"U(" + reg + ")" if op == "stfs" else reg}; {c} [fixed, written back after the loop]')
        elif op == 'stw': emit(f'm{k} = {reg}; PPC_FAST_STORE_U32({e}, m{k}); {c} [fixed]')
        else: emit(f'm{k} = U({reg}); PPC_FAST_STORE_U32({e}, m{k}); {c} [fixed]')
        continue
    if op in ('lwz', 'stw', 'lfs', 'stfs'):
        d, ra = dmem(args[1]); reg = args[0]
        if ra == 'r1' and slot(d):
            slots.add(d); s = f's{d if d >= 0 else "m" + str(-d)}'
            if op == 'lwz': emit(f'{reg} = {s}; {c}')
            elif op == 'stw': emit(f'{s} = {reg}; {c}')
            elif op == 'lfs': emit(f'{reg} = F({s}); {c}')
            else: emit(f'{s} = U({reg}); {c}')
            continue
        e = ea(d, ra)
        if op == 'lwz': emit(f'{reg} = PPC_FAST_LOAD_U32({e}); {c}')
        elif op == 'stw': emit(f'PPC_FAST_STORE_U32({e}, {reg}); {c}')
        elif op == 'lfs': emit(f'{reg} = F(PPC_FAST_LOAD_U32({e})); {c}')
        else: emit(f'PPC_FAST_STORE_U32({e}, U({reg})); {c}')
        continue
    if op in ('lfsx', 'stfsx'):
        reg, ra, rb = args
        e = f'uint32_t({gpr(rb)})' if ra == 'r0' else f'uint32_t({gpr(ra)} + {gpr(rb)})'
        if op == 'lfsx': emit(f'{reg} = F(PPC_FAST_LOAD_U32({e})); {c}')
        else: emit(f'PPC_FAST_STORE_U32({e}, U({reg})); {c}')
        continue
    if op == 'addi':
        rd, ra, imm = args
        emit(f'{gpr(rd)} = {gpr(ra) if ra != "r0" else "0"} + uint32_t({int(imm)}); {c}'); continue
    if op == 'addis':
        rd, ra, imm = args
        emit(f'{gpr(rd)} = {gpr(ra) if ra != "r0" else "0"} + uint32_t({int(imm) << 16}); {c}'); continue
    if op == 'li': emit(f'{args[0]} = uint32_t({int(args[1])}); {c}'); continue
    if op == 'lis': emit(f'{args[0]} = uint32_t({int(args[1]) << 16}); {c}'); continue
    if op == 'ori': emit(f'{args[0]} = {gpr(args[1])} | {int(args[2], 0)}u; {c}'); continue
    if op == 'mr': emit(f'{gpr(args[0])} = {gpr(args[1])}; {c}'); continue
    if op == 'add': emit(f'{args[0]} = {gpr(args[1])} + {gpr(args[2])}; {c}'); continue
    if op == 'subf': emit(f'{args[0]} = {gpr(args[2])} - {gpr(args[1])}; {c}'); continue
    if op in ('rlwinm', 'clrlwi', 'rotlwi'):
        if op == 'rlwinm': rd, rs, sh, mb, me = args[0], args[1], int(args[2]), int(args[3]), int(args[4])
        elif op == 'clrlwi': rd, rs, sh, mb, me = args[0], args[1], 0, int(args[2]), 31
        else: rd, rs, sh, mb, me = args[0], args[1], int(args[2]), 0, 31
        rot = f'std::rotl({gpr(rs)}, {sh})' if sh else gpr(rs)
        m = mask(mb, me)
        emit(f'{rd} = {rot}' + (f' & {m:#010x}u' if m != 0xFFFFFFFF else '') + f'; {c}')
        continue
    if op in ('cmpwi', 'cmplwi'):
        cr, ra, imm = args
        assert cr == 'cr6'
        if op == 'cmpwi': emit(f'cmpA = int32_t({gpr(ra)}); cmpB = {int(imm)}; cmpSigned = true; {c}')
        else: emit(f'cmpUA = {gpr(ra)}; cmpUB = {int(imm, 0)}u; cmpSigned = false; {c}')
        continue
    if op in ('beq', 'bne', 'blt', 'bgt', 'ble', 'bge'):
        cr, tgt = args; tgt = int(tgt, 16); assert cr == 'cr6' and tgt in labels, t
        rel = {'beq': '==', 'bne': '!=', 'blt': '<', 'bgt': '>', 'ble': '<=', 'bge': '>='}[op]
        emit(f'if (cmpSigned ? (cmpA {rel} cmpB) : (cmpUA {rel} cmpUB)) goto loc_{tgt:08X}; {c}')
        continue
    if op == 'b':
        tgt = int(args[0], 16)
        if tgt in labels: emit(f'goto loc_{tgt:08X}; {c}')
        elif tgt == RESTGPRLR: emit(f'goto done; {c}')
        else: sys.exit('b ' + t)
        continue
    if op == 'bl':
        tgt = int(args[0], 16)
        if tgt in SKIP_CALLS or tgt == RESTFPR: emit(c + ' (register saves: not needed)'); continue
        calls.add(tgt)
        emit(f'CALL(sub_{tgt:08X}, {a + 4:#010x}u); {c}')
        continue
    if op in ('fadds', 'fsubs', 'fmuls'):
        sym = {'fadds': '+', 'fsubs': '-', 'fmuls': '*'}[op]
        emit(f'{args[0]} = {args[1]} {sym} {args[2]}; {c}'); continue
    if op in ('fmadds', 'fmsubs'):
        d, fa, fc, fb = args
        sign = '' if op == 'fmadds' else '-'
        emit(f'{d} = float(std::fma(double({fa}), double({fc}), {sign}double({fb}))); {c}'); continue
    # VMX, per element only here: lanes kept in guest element order (each word byte-swapped), flushing on.
    if op in ('lvx128', 'stvx128'):
        v, ra, rb = args
        e = f'(uint32_t({gpr(rb)}) & ~15u)' if ra == 'r0' else f'(uint32_t({gpr(ra)} + {gpr(rb)}) & ~15u)'
        if op == 'lvx128': emit(f'{v} = LoadVector(base, {e}); {c}')
        else: emit(f'StoreVector(base, {e}, {v}); {c}')
        continue
    if op == 'lvlx':
        v, ra, rb = args
        emit(f'{v} = LoadVectorLeft(base, uint32_t({gpr(ra)} + {gpr(rb)})); {c}'); continue
    if op == 'vspltw':
        d, s_, imm = args
        emit(f'{d} = vdupq_laneq_f32({s_}, {int(imm)}); {c}'); continue
    if op in ('vmulfp128', 'vaddfp', 'vsubfp'):
        f = {'vmulfp128': 'vmulq_f32', 'vaddfp': 'vaddq_f32', 'vsubfp': 'vsubq_f32'}[op]
        emit(f'FLUSH_ON; {args[0]} = {f}({args[1]}, {args[2]}); {c}'); continue
    if op == 'vmaddfp':
        d, va, vc, vb = args
        emit(f'FLUSH_ON; {d} = vfmaq_f32({vb}, {va}, {vc}); {c}'); continue
    sys.exit('unhandled: ' + t)

print('// Generated by ppc2native.py from the recompiled listing of', name)
print('// GPRs:', ' '.join(sorted(used_gpr, key=lambda r: int(r[1:]))))
print('// FPRs:', ' '.join(sorted(used_fpr, key=lambda r: int(r[1:]))))
print('// VRs:', ' '.join(sorted(used_vr, key=lambda r: int(r[1:]))))
print('// calls:', ' '.join(f'sub_{x:08X}' for x in sorted(calls)))
print('// slots:', ' '.join(str(s) for s in sorted(slots)))
print('// fixed:', len(keys))
print('\n'.join(out))
