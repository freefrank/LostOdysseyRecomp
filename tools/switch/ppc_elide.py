#!/usr/bin/env python3
"""Removes the register write-backs nothing needs from the recompiled code (docs/SWITCH-BENCHMARKS.md).

With argument_as_local, a recompiled function keeps r3-r12 and f1-f13 in C locals, writes each one it changed anywhere back
to PPCContext before every call and return, and reads them all back after every call. In the hottest functions about 14% of
the instructions are these stores and reloads. Two rules remove most of them:

  Clean registers: a store is a no-op when the register hasn't changed since it last matched ctx. No assumption at all.

  Dead registers: whole-program liveness finds what each function reads (live_in) and what its callers read after it returns
  (needed). A dirty store before a direct call goes if the callee doesn't read it and the caller doesn't read it after the
  call: the guest's compiler used link-time code generation, so a value may live in a volatile register across a call it
  knows about. A dirty store before a return goes if it's neither a return register nor needed. Indirect calls keep every
  dirty store, and functions replaced by host code count as reading and writing everything.

The reloads left unused are dead code for GCC. The analysis reads the generated C, not PowerPC: `goto`, `return;` and calls
give the control flow, and a register named in a statement counts as read, unless the statement writes the whole register.

  ppc_elide.py --in out/ppc-orig --out LostOdysseyRecompLib/ppc --runtime LostOdysseyRecomp --toml <config>
"""
import argparse
import glob
import multiprocessing
import os
import re
import sys
import time

TRACKED = [f'r{i}' for i in range(3, 13)] + [f'f{i}' for i in range(1, 14)]
BIT = {name: 1 << i for i, name in enumerate(TRACKED)}
ALL = (1 << len(TRACKED)) - 1
RET = BIT['r3'] | BIT['r4'] | BIT['f1']  # what any caller may read after a return

FUNC = re.compile(r'^PPC_FUNC_IMPL\(__imp__(sub_[0-9A-F]+)\) \{')
STORE = re.compile(r'^\tctx\.([rf]\d+)(?:\.u64)? = \1(?:\.u64)?;$')
RELOAD = re.compile(r'^\t([rf]\d+)(?:\.u64)? = ctx\.\1(?:\.u64)?;$')
DIRECT = re.compile(r'\b(?:__imp__)?(sub_[0-9A-F]+)\(ctx, base\);')
ANY_CALL = re.compile(r'\(ctx, base\);|PPC_CALL_INDIRECT_FUNC\(')
LABEL = re.compile(r'^(loc_[0-9A-F]+):')
GOTO = re.compile(r'goto (loc_[0-9A-F]+);')
WHOLE = re.compile(r'^\t+([rf]\d+)\.(?:u64|s64|f64) = (.*)$')
TOKEN = re.compile(r'\b([rf]\d+)\b')
PLAIN_CALL = re.compile(r'(?<![\w])(sub_[0-9A-F]+)\(ctx, base\);')
READ_ONLY = ('PPC_STORE', 'PPC_FAST_STORE', 'if (', 'goto ', 'switch', 'case ', 'cr', 'xer', 'default:', 'ctx.lr', 'ctr.')
CALL_IN_CONDITION = re.compile(r'\b[A-Za-z_]\w*\(')
# The register save and restore helpers leave every argument alone.
HELPER = re.compile(r'\b__(?:save|rest)(?:gpr|fpr|vmx)\w*\(ctx, base\);')
DECLARATION = ('PPCRegister ', 'PPCCRRegister ', 'PPCXERRegister ', 'PPCVRegister ', 'PPC_FUNC_PROLOGUE')


def mask(text):
    m = 0
    for name in TOKEN.findall(text):
        m |= BIT.get(name, 0)
    return m


class Unit:
    """A straight run of statements. use, kill: for liveness. gen: registers it may change."""
    __slots__ = ('use', 'kill', 'gen', 'succ', 'pred', 'call', 'indirect', 'helper', 'returns', 'after_call', 'set_in', 'dirty_in')

    def __init__(self):
        self.use = self.kill = self.gen = 0
        self.succ = []
        self.pred = []
        self.call = None
        self.indirect = self.helper = self.returns = False
        self.after_call = self.set_in = self.dirty_in = 0


def parse_body(lines):
    """-> units, and the registers this function reloads after a call."""
    units = [Unit()]
    labels = {}
    falls = [True]
    branches = [[]]
    extra = []    # (from, to): the edge past an if block
    blocks = []   # the unit before each open block; None for a switch
    reloaded = 0

    def start():
        units.append(Unit())
        falls.append(True)
        branches.append([])

    for line in lines:
        match = LABEL.match(line)
        if match:
            start()
            labels[match.group(1)] = len(units) - 1
            continue
        stripped = line.strip()
        if stripped.startswith('//'):
            start()
            continue
        reload = RELOAD.match(line)
        if reload:
            reloaded |= BIT.get(reload.group(1), 0)
            continue
        if not stripped or STORE.match(line) or stripped.startswith(DECLARATION):
            continue
        in_block = any(b is not None for b in blocks)
        if HELPER.search(line):
            start()
            units[-1].indirect = units[-1].helper = True
            start()
            continue
        direct = DIRECT.search(line)
        if direct:
            start()
            units[-1].call = direct.group(1)
            start()
            continue
        if ANY_CALL.search(line):
            start()
            units[-1].indirect = True
            start()
            continue
        current = units[-1]
        if stripped == 'return;':
            current.returns = True
            if not in_block:
                falls[-1] = False
            start()
            continue
        if stripped == '} else {':
            opened = blocks.pop()
            start()
            extra.append((opened, len(units) - 1))
            blocks.append(len(units) - 2)
            continue
        if stripped.endswith('{') and not stripped.startswith('PPC'):
            current.use |= mask(line) & ~current.kill
            if stripped.startswith('switch'):
                blocks.append(None)
                branches[-1].extend(GOTO.findall(line))
                continue
            blocks.append(len(units) - 1)
            start()
            continue
        if stripped == '}':
            opened = blocks.pop() if blocks else None
            if opened is not None:
                start()
                extra.append((opened, len(units) - 1))
            continue
        branches[-1].extend(GOTO.findall(line))
        if stripped.startswith('goto ') and not in_block:
            falls[-1] = False
        whole = WHOLE.match(line)
        if whole:
            current.use |= mask(whole.group(2)) & ~current.kill
            current.kill |= BIT.get(whole.group(1), 0)
            current.gen |= BIT.get(whole.group(1), 0)
        else:
            current.use |= mask(line) & ~current.kill
            condition = stripped.startswith('if (')
            if (condition and CALL_IN_CONDITION.search(stripped[3:])) or (not condition and not stripped.startswith(READ_ONLY)):
                current.gen |= mask(line)  # a partial write, or a hook that takes registers by reference

    for index, unit in enumerate(units):
        if falls[index]:
            if index + 1 < len(units):
                unit.succ.append(index + 1)
            else:
                unit.use |= ALL  # runs off the end, into whatever follows
        for target in branches[index]:
            if target in labels:
                unit.succ.append(labels[target])
            else:
                unit.use |= ALL
    for source, target in extra:
        units[source].succ.append(target)
    return units, reloaded


def parse_file(path):
    functions = {}
    with open(path, errors='surrogateescape') as f:
        lines = f.read().split('\n')
    i = 0
    while i < len(lines):
        match = FUNC.match(lines[i])
        if not match:
            i += 1
            continue
        j = i + 1
        while j < len(lines) and lines[j] != '}':
            j += 1
        functions[match.group(1)] = parse_body(lines[i + 1:j])
        i = j + 1
    return functions


def forward(units, entry, transfer, attribute):
    """May-analysis over the units: the union of what reaches each one."""
    work = list(range(len(units)))
    for u in units:
        setattr(u, attribute, 0)
    if units:
        setattr(units[0], attribute, entry)
    while work:
        i = work.pop()
        unit = units[i]
        out = transfer(unit, getattr(unit, attribute))
        for s in unit.succ:
            value = getattr(units[s], attribute) | out
            if value != getattr(units[s], attribute):
                setattr(units[s], attribute, value)
                work.append(s)


def analyse(functions, reloaded, opaque):
    # What each function may change: everything it writes and what it calls may write.
    may_write = {name: 0 for name in functions}
    changed = True
    while changed:
        changed = False
        for name, units in functions.items():
            value = may_write[name]
            for u in units:
                value |= u.gen
                if u.helper:
                    continue
                if u.indirect or (u.call is not None and (u.call in opaque or u.call not in functions)):
                    value = ALL
                elif u.call is not None:
                    value |= may_write[u.call]
            if value != may_write[name]:
                may_write[name] = value
                changed = True

    # Registers that can hold a value set for an indirect callee: arguments set since the last call, or that survive it.
    def set_transfer(unit, value):
        if unit.helper:
            return value
        if unit.indirect:
            return RET
        if unit.call is not None:
            written = ALL if unit.call in opaque or unit.call not in functions else may_write[unit.call]
            return (value & ~written) | RET
        return value | unit.gen

    for name, units in functions.items():
        forward(units, ALL, set_transfer, 'set_in')
        # Registers that may differ from ctx: a call syncs what the function reloads after it. r11 and r12 aren't.
        synced = reloaded[name]
        forward(units, 0, lambda unit, value: value & ~synced if unit.indirect or unit.call is not None else value | unit.gen,
                'dirty_in')

    live_in = {name: 0 for name in functions}
    needed = {name: 0 for name in functions}
    changed = True
    rounds = 0
    while changed:
        rounds += 1
        changed = False
        for name, units in functions.items():
            live = [0] * len(units)
            exit_use = RET | needed[name]
            again = True
            while again:
                again = False
                for i in range(len(units) - 1, -1, -1):
                    unit = units[i]
                    out = 0
                    for s in unit.succ:
                        out |= live[s]
                    if unit.returns:
                        out |= exit_use
                    if unit.call is not None:
                        inner = ALL if unit.call in opaque or unit.call not in functions else live_in[unit.call]
                        value = out | (inner & unit.set_in if unit.call in opaque or unit.call not in functions else inner)
                        unit.after_call = out
                    elif unit.helper:
                        value = out
                    elif unit.indirect:
                        # An indirect callee keeps to the ABI: what's read after it comes back in r3, r4 or f1.
                        value = unit.set_in
                        unit.after_call = out
                    else:
                        value = (out & ~unit.kill) | unit.use
                    if value != live[i]:
                        live[i] = value
                        again = True
            entry = live[0] if units else ALL
            if entry != live_in[name]:
                live_in[name] = entry
                changed = True
            for unit in units:
                if unit.call in needed:
                    want = needed[unit.call] | (unit.after_call & ~RET)
                    if want != needed[unit.call]:
                        needed[unit.call] = want
                        changed = True
        print(f'  round {rounds}', flush=True)

    keeps = {}
    for name, units in functions.items():
        events = []
        for u in units:
            if u.call is not None:
                inner = ALL if u.call in opaque or u.call not in functions else live_in[u.call]
                events.append(u.dirty_in & (inner | u.after_call))
            elif u.indirect:
                events.append(u.dirty_in)
            elif u.returns:
                events.append((u.dirty_in | u.gen) & (RET | needed[name]))
        keeps[name] = events
    return keeps, live_in


def direct_calls(lines, known):
    """Calls the strong name: sub_X is a weak alias of __imp__sub_X, which GCC can't inline. -> lines, declarations"""
    names = set()

    def replace(match):
        name = match.group(1)
        if name in known:
            names.add(name)
            return f'__imp__{name}(ctx, base);'
        return match.group(0)

    for i, line in enumerate(lines):
        if '(ctx, base);' in line and not line.lstrip().startswith('//'):
            lines[i] = PLAIN_CALL.sub(replace, line)
    return [f'extern "C" PPC_FUNC(__imp__{name});' for name in sorted(names)]


def rewrite(path, out_path, keeps, opaque, direct):
    with open(path, errors='surrogateescape') as f:
        lines = f.read().split('\n')
    declarations = direct_calls(lines, direct) if direct else []
    out = []
    removed = kept = 0
    counted = {}
    current = None
    event = 0
    i = 0
    while i < len(lines):
        line = lines[i]
        match = FUNC.match(line)
        if match:
            current = match.group(1)
            event = 0
        elif line == '}':
            if current:
                counted[current] = event
            current = None
        if current is None:
            out.append(line)
            i += 1
            continue
        if STORE.match(line):
            j = i
            while STORE.match(lines[j]):
                j += 1
            stripped = lines[j].strip()
            boundary = stripped == 'return;' or ANY_CALL.search(lines[j])
            keep = keeps[current][event] if boundary and current not in opaque else ALL
            for k in range(i, j):
                name = STORE.match(lines[k]).group(1)
                if name in BIT and not keep & BIT[name]:
                    removed += 1
                else:
                    out.append(lines[k])
                    kept += 1
            i = j
            continue
        if line.strip() == 'return;' or (ANY_CALL.search(line) and not line.lstrip().startswith('//')):
            event += 1
        out.append(line)
        i += 1
    if declarations:
        first = next(i for i, line in enumerate(out) if line.startswith('__attribute__') or FUNC.match(line))
        out[first:first] = declarations
    with open(out_path, 'w', errors='surrogateescape') as f:
        f.write('\n'.join(out))
    return removed, kept, counted


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--in', dest='source', required=True)
    parser.add_argument('--out', required=True)
    parser.add_argument('--runtime', required=True)
    parser.add_argument('--toml', required=True)
    parser.add_argument('--direct-calls', action='store_true', help='call the strong names, so GCC can inline guest functions')
    args = parser.parse_args()

    start = time.time()
    paths = sorted(glob.glob(os.path.join(args.source, 'ppc_recomp.*.cpp')))
    with multiprocessing.Pool(6) as pool:
        parsed = pool.map(parse_file, paths)
    functions = {}
    reloaded = {}
    for part in parsed:
        for name, (units, synced) in part.items():
            functions[name] = units
            reloaded[name] = synced
    print(f'{len(functions)} functions parsed in {time.time() - start:.0f} s', flush=True)

    # Functions replaced by host code, and those that keep their registers in ctx (setjmp and the like).
    opaque = set()
    pattern = re.compile(r'\b(?:PPC_FUNC|GUEST_FUNCTION_HOOK|GUEST_FUNCTION_STUB|PPC_FUNC_IMPL)\(\s*(?:__imp__)?(sub_[0-9A-F]+)')
    for root, _, files in os.walk(args.runtime):
        for name in files:
            if name.endswith(('.cpp', '.h', '.inl')):
                with open(os.path.join(root, name), errors='ignore') as f:
                    opaque.update(pattern.findall(f.read()))
    with open(args.toml, errors='ignore') as f:
        text = f.read()
    block = text[text.index('non_volatile_in_context'):]
    opaque.update('sub_' + a.upper() for a in re.findall(r'0x([0-9A-Fa-f]{8})', block[:block.index(']')]))
    print(f'{len(opaque)} functions opaque', flush=True)

    keeps, live_in = analyse(functions, reloaded, opaque)
    print('functions reading at entry:', {n: sum(1 for v in live_in.values() if v & BIT[n]) for n in TRACKED}, flush=True)

    os.makedirs(args.out, exist_ok=True)
    removed = kept = 0
    for path in paths:
        r, k, counted = rewrite(path, os.path.join(args.out, os.path.basename(path)), keeps, opaque,
                                set(functions) - opaque if args.direct_calls else None)
        removed += r
        kept += k
        for name, events in counted.items():
            if events != len(keeps[name]):
                sys.exit(f'{name}: {events} calls and returns in the text, {len(keeps[name])} in the analysis')
    print(f'stores removed {removed}, kept {kept} ({100 * removed / max(1, removed + kept):.1f}%), {time.time() - start:.0f} s')


if __name__ == '__main__':
    main()
