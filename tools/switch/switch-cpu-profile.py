#!/usr/bin/env python3
"""From ChanseyIsTheBest/UnleashedRecomp-NX (GPL-3.0); default paths adapted to Lost Odyssey Recomp.

Names the code addresses of the Switch CPU profiler ([Switch] SwitchCpuProfiler = true) and of the
stall watchdog ([Switch] SwitchStallWatchSeconds).

The game writes, every 30 seconds, blocks like this to stderr.log (os/switch/cpu_profiler_switch.cpp):

    [cpu profile] 30.0 s, module base 0x..., 0 samples dropped; run tools/switch-cpu-profile.py ...
      thread guest 82E3A2C8: 11873 samples while running, 3120 while waiting
         4.1%  +0x1a2b3c0
         ...
        system calls in 3120 samples; from (return address, then stack):
          52.0%  wait +0x0123456 +0x0234567 ...

Each "+0x..." is an offset from the start of the executable (the profiled lines are 16-byte code lines, the
"wait" ones return addresses: the call into the system call, then return addresses found on the stack).
This script maps the offsets to the functions of the ELF the NRO was made from (recompiled game functions
are named sub_82XXXXXX after their PowerPC address), adds up each function's samples, and prints, per
thread and for all threads together, the functions that took the most CPU time and where the threads
waited from.

    python tools/switch/switch-cpu-profile.py stderr.log [--elf out/switch/LostOdysseyRecomp.elf]
                                                  [--top 25] [--report N]

--report N uses only the Nth [cpu profile] block (1 = first, -1 = last; default: all of them added up).
The ELF must be the one of the NRO that wrote the log.

The "[stall]" and "[hitch]" lines of the stall watchdog (os/switch/stall_watch_switch.cpp) are printed at
the end with every offset named, and so are the "[crash]" lines of crash.log (os/switch/crash_switch.cpp):
`python tools/switch-cpu-profile.py crash.log --elf ...`.

--samples cpu-samples.bin (LO_CPU_PROFILE_SAMPLES; the log argument is then that file) reads every kept sample
instead: per thread, self time per function (every sample, not only the hottest lines) and, for guest threads,
inclusive time per guest function from the guest call chains, the callers of the hottest functions, and
--folded FILE writes the stacks in flamegraph.pl's folded format. --from/--to S keep the samples taken that
many seconds after the sampler started (about the game's start); --session N picks a session in the file
(default: the last one).

--hot-list FILE [--hot-count 300] [--hot-min-samples 2] writes the recompiled functions with the most samples
(all threads together, at least --hot-min-samples) to FILE in the format of
UnleashedRecompLib/config/hot_functions.txt, which tools/switch-direct-calls.py marks hot.
"""

import argparse
import bisect
import collections
import os
import re
import shutil
import struct
import subprocess
import sys

REPORT_RE = re.compile(r'^\[cpu profile\] ([0-9.]+) s')
THREAD_RE = re.compile(r'^  thread (.+): (\d+) samples while running, (\d+) while waiting')
LINE_RE = re.compile(r'^\s+([0-9.]+)%\s+\+0x([0-9a-fA-F]+)')
SYSCALLS_RE = re.compile(r'^\s+system calls in (\d+) samples')
WAIT_RE = re.compile(r'^\s+([0-9.]+)%\s+wait((?:\s+\+0x[0-9a-fA-F]+)*)')
OFFSET_RE = re.compile(r'\+0x([0-9a-fA-F]+)')


def find_nm():
    for name in ('aarch64-none-elf-nm', 'aarch64-none-elf-nm.exe'):
        path = shutil.which(name)
        if path:
            return path
    devkitpro = os.environ.get('DEVKITPRO') or next(
        (d for d in ('/opt/devkitpro', r'C:\devkitPro') if os.path.isdir(d)), r'C:\devkitPro')
    candidate = os.path.join(devkitpro, 'devkitA64', 'bin', 'aarch64-none-elf-nm.exe')
    if os.path.exists(candidate):
        return candidate
    candidate = os.path.join(devkitpro, 'devkitA64', 'bin', 'aarch64-none-elf-nm')
    if os.path.exists(candidate):
        return candidate
    sys.exit('aarch64-none-elf-nm not found (devkitA64); set DEVKITPRO or put it on PATH')


def load_symbols(elf):
    output = subprocess.run([find_nm(), '-n', '-C', '--defined-only', elf], check=True,
                            capture_output=True, text=True, errors='replace').stdout
    starts, names = [], []
    for line in output.splitlines():
        parts = line.split(' ', 2)
        if len(parts) != 3 or parts[1] not in ('T', 't', 'W', 'w'):
            continue
        address = int(parts[0], 16)
        name = parts[2]
        # Recompiled functions: keep the bare sub_XXXXXXXX (drop the parameter list).
        if name.startswith('sub_') and '(' in name:
            name = name[:name.index('(')]
        if starts and starts[-1] == address:
            continue
        starts.append(address)
        names.append(name)
    return starts, names


def symbolize(starts, names, offset):
    index = bisect.bisect_right(starts, offset) - 1
    return names[index] if index >= 0 else '?'


def symbolize_bucket(starts, names, offset):
    # The sampler counts 16-byte buckets: one that another function starts in is named after both, since the
    # samples may belong to either (e.g. "__libnx_exception_returnentry / __aarch64_read_tp").
    name = symbolize(starts, names, offset)
    index = bisect.bisect_right(starts, offset + 15) - 1
    if index >= 0 and starts[index] > offset and names[index] != name:
        return name + ' / ' + names[index]
    return name


def symbolize_return(starts, names, offset):
    # A return address follows the call; the call itself can be the last instruction of its function.
    return symbolize(starts, names, offset - 1)


def parse(log, which):
    reports = []
    watchdog = []   # "[stall]" and "[hitch]" lines, in order
    current = None
    thread = None
    # Several threads can have the same name (guest threads started at the same address): each one's
    # percentages are of its own samples, so they are turned into samples before the threads are merged.
    running = 0
    syscalls = 0
    with open(log, encoding='utf-8', errors='replace') as f:
        for raw in f:
            line = raw.rstrip('\r\n')
            if line.startswith('[stall]') or line.startswith('[hitch]') or line.startswith('[crash]'):
                watchdog.append(line)
                continue
            m = REPORT_RE.match(line)
            if m:
                current = {'seconds': float(m.group(1)), 'threads': collections.OrderedDict()}
                reports.append(current)
                thread = None
                continue
            if current is None:
                continue
            m = THREAD_RE.match(line)
            if m:
                thread = current['threads'].setdefault(m.group(1), {'running': 0, 'waiting': 0, 'lines': [],
                                                                    'syscalls': 0, 'waits': []})
                running = int(m.group(2))
                syscalls = 0
                thread['running'] += running
                thread['waiting'] += int(m.group(3))
                continue
            m = LINE_RE.match(line)
            if m and thread is not None:
                thread['lines'].append((float(m.group(1)) / 100.0 * running, int(m.group(2), 16)))
                continue
            m = SYSCALLS_RE.match(line)
            if m and thread is not None:
                syscalls = int(m.group(1))
                thread['syscalls'] += syscalls
                continue
            m = WAIT_RE.match(line)
            if m and thread is not None:
                callers = [int(x, 16) for x in OFFSET_RE.findall(m.group(2))]
                thread['waits'].append((float(m.group(1)) / 100.0 * syscalls, callers))
                continue
            if not line.startswith('  '):
                thread = None
    if not reports and not watchdog:
        sys.exit(f'no [cpu profile], [stall], [hitch] or [crash] lines in {log} (was SwitchCpuProfiler = true?)')
    if which is not None and reports:
        reports = [reports[which - 1 if which > 0 else which]]
    return reports, watchdog


def describe_callers(starts, names, callers):
    # Named call chain, innermost first, consecutive repeats folded.
    chain = []
    for caller in callers:
        name = symbolize_return(starts, names, caller)
        if not chain or chain[-1] != name:
            chain.append(name)
    return ' <- '.join(chain) if chain else '(no return address)'


RAW_CHUNK = struct.Struct('<IIQQQQIIII')
RAW_NAME = struct.Struct('<I48s')
RAW_RECORD = struct.Struct('<IBBBBII')


def read_samples(path):
    # cpu-samples.bin: chunks (os/switch/cpu_profiler_switch.cpp, RawChunk), grouped into sessions by start tick.
    sessions = collections.OrderedDict()
    with open(path, 'rb') as f:
        data = f.read()
    pos = 0
    while pos + RAW_CHUNK.size <= len(data):
        magic, version, base, freq, start, end, name_count, record_bytes, dropped, interval = RAW_CHUNK.unpack_from(data, pos)
        if magic != 0x53434F4C or version != 1:
            sys.exit(f'{path}: bad chunk at byte {pos}')
        pos += RAW_CHUNK.size
        names = {}
        for _ in range(name_count):
            slot, name = RAW_NAME.unpack_from(data, pos)
            names[slot] = name.split(b'\0', 1)[0].decode('utf-8', 'replace')
            pos += RAW_NAME.size
        session = sessions.setdefault(start, {'names': {}, 'records': [], 'dropped': 0, 'freq': freq,
                                              'chunks': 0, 'end': 0})
        session['names'].update(names)
        session['dropped'] += dropped
        session['chunks'] += 1
        session['end'] = max(session['end'], (end - start) / freq)
        stop = pos + record_bytes
        while pos < stop:
            time_us, slot, depth, flags, _, pc, lr = RAW_RECORD.unpack_from(data, pos)
            pos += RAW_RECORD.size
            chain = struct.unpack_from(f'<{depth}I', data, pos) if depth else ()
            pos += 4 * depth
            session['records'].append((time_us, slot, flags, pc, lr, chain))
    return list(sessions.values())


def guest_functions(starts, names):
    # Guest address -> recompiled function, from the sub_XXXXXXXX names (hooks and bodies alike).
    table = {}
    for name in names:
        m = re.fullmatch(r'(?:__imp__)?sub_([0-9A-F]{8})', name)
        if m:
            table[int(m.group(1), 16)] = 'sub_' + m.group(1)
    guest_starts = sorted(table)
    return guest_starts, [table[a] for a in guest_starts]


def samples_main(args, starts, names):
    sessions = read_samples(args.log)
    if not sessions:
        sys.exit(f'no samples in {args.log}')
    session = sessions[args.session - 1 if args.session > 0 else args.session]
    guest_starts, guest_names = guest_functions(starts, names)

    def guest_of_return(address):
        # A return address follows the bl: the call is the instruction before it.
        index = bisect.bisect_right(guest_starts, address - 4) - 1
        return guest_names[index] if index >= 0 else f'?{address:08X}'

    def host_name(offset):
        return '(outside the executable)' if offset == 0xFFFFFFFF else symbolize(starts, names, offset)

    def leaf_guest(host):
        m = re.fullmatch(r'(?:__imp__)?(sub_[0-9A-F]{8})', host)
        return m.group(1) if m else None

    lo = (args.window_from or 0) * 1e6
    hi = (args.window_to or 1e12) * 1e6
    records = [r for r in session['records'] if lo <= r[0] < hi]
    if not records:
        sys.exit('no samples in that window')
    first, last = records[0][0] / 1e6, records[-1][0] / 1e6
    print(f'session {sessions.index(session) + 1} of {len(sessions)}: {len(session["records"])} samples in '
          f'{session["chunks"]} chunk(s), {session["dropped"]} dropped; window {first:.1f}-{last:.1f} s, '
          f'{len(records)} samples. A sample is taken every 2 ms of a thread that ran since the previous one.\n')

    # Threads with the same name (guest workers started at one address) are told apart by slot.
    counts = collections.Counter(session['names'].values())
    seen = collections.Counter()
    labels = {}
    for slot in sorted(session['names']):
        name = session['names'][slot]
        seen[name] += 1
        labels[slot] = f'{name} #{seen[name]} (slot {slot})' if counts[name] > 1 else name

    by_thread = collections.defaultdict(list)
    for r in records:
        by_thread[r[1]].append(r)

    folded = collections.Counter()
    for slot, rs in sorted(by_thread.items(), key=lambda item: -len(item[1])):
        if len(rs) < args.min_samples:
            continue
        total = len(rs)
        syscalls = sum(1 for r in rs if r[2] & 1)
        guest = sum(1 for r in rs if r[2] & 2)
        self_time = collections.Counter()
        inclusive = collections.Counter()
        callers = collections.defaultdict(collections.Counter)
        depths = []
        for time_us, _, flags, pc, lr, chain in rs:
            leaf = host_name(pc)
            if flags & 1:
                leaf = f'[system call] {leaf}'
            self_time[leaf] += 1
            # Guest chain, innermost first: the guest LR may be a return address inside the leaf itself.
            frames = []
            own = leaf_guest(leaf)
            for i, address in enumerate(chain):
                if address < 0x82000000:
                    continue   # the thread's first frame (0) or a guest LR that is not a code address
                function = guest_of_return(address)
                if i == 0 and function == own:
                    continue
                if not frames or frames[-1] != function:
                    frames.append(function)
            depths.append(len(frames))
            on_stack = set(frames)
            if own:
                on_stack.add(own)
            for function in on_stack:
                inclusive[function] += 1
            caller = frames[0] if frames else (host_name(lr) if lr != 0xFFFFFFFF else '?')
            callers[leaf][caller] += 1
            if args.folded:
                folded[';'.join([labels[slot]] + frames[::-1] + [leaf])] += 1
        print(f'thread {labels[slot]}: {total} samples ({total * 0.002:.1f} s at 2 ms), {100 * syscalls / total:.1f}% in '
              f'system calls, {100 * guest / total:.0f}% with a guest chain'
              + (f' (median depth {sorted(depths)[len(depths) // 2]})' if guest else ''))
        print('  self (where the samples were):')
        for function, n in self_time.most_common(args.top):
            top_callers = ', '.join(f'{c} {100 * k / n:.0f}%' for c, k in callers[function].most_common(3))
            print(f'  {100 * n / total:5.1f}%  {n:7d}  {function}   <- {top_callers}')
        if guest:
            print('  inclusive (guest function anywhere on the guest stack, or the leaf):')
            for function, n in inclusive.most_common(args.top):
                print(f'  {100 * n / total:5.1f}%  {n:7d}  {function}')
        print()

    if args.folded:
        with open(args.folded, 'w', encoding='utf-8', newline='\n') as f:
            for stack, n in folded.most_common():
                f.write(f'{stack} {n}\n')
        print(f'folded stacks written to {args.folded}')


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('log')
    parser.add_argument('--elf', default=os.path.join(os.path.dirname(__file__), '..', '..', 'out', 'switch',
                                                    'LostOdysseyRecomp.elf'))
    parser.add_argument('--top', type=int, default=25)
    parser.add_argument('--report', type=int, default=None)
    parser.add_argument('--waits', type=int, default=8, help='wait sites printed per thread')
    parser.add_argument('--samples', action='store_true', help='the file is cpu-samples.bin (LO_CPU_PROFILE_SAMPLES)')
    parser.add_argument('--from', dest='window_from', type=float, default=None, help='--samples: start, seconds')
    parser.add_argument('--to', dest='window_to', type=float, default=None, help='--samples: end, seconds')
    parser.add_argument('--session', type=int, default=-1, help='--samples: session in the file (1 = first)')
    parser.add_argument('--min-samples', type=int, default=500, help='--samples: threads with fewer are left out')
    parser.add_argument('--folded', default=None, help='--samples: write folded stacks (flamegraph.pl) here')
    parser.add_argument('--hot-list', default=None, help='write the hottest recompiled functions to this file')
    parser.add_argument('--hot-count', type=int, default=300)
    parser.add_argument('--hot-min-samples', type=float, default=2.0)
    args = parser.parse_args()

    starts, names = load_symbols(args.elf)
    if args.samples:
        samples_main(args, starts, names)
        return
    reports, watchdog = parse(args.log, args.report)

    seconds = sum(r['seconds'] for r in reports)
    per_thread = collections.OrderedDict()   # thread -> function -> samples
    per_thread_waits = collections.OrderedDict()   # thread -> call chain -> samples
    running = collections.Counter()
    waiting = collections.Counter()
    syscalls = collections.Counter()
    for report in reports:
        for name, thread in report['threads'].items():
            running[name] += thread['running']
            waiting[name] += thread['waiting']
            syscalls[name] += thread['syscalls']
            functions = per_thread.setdefault(name, collections.Counter())
            for samples, offset in thread['lines']:
                functions[symbolize_bucket(starts, names, offset)] += samples
            waits = per_thread_waits.setdefault(name, collections.Counter())
            for samples, callers in thread['waits']:
                waits[describe_callers(starts, names, callers)] += samples

    everything = collections.Counter()
    if reports:
        print(f'{len(reports)} report(s), {seconds:.0f} s; samples every 2 ms while a thread runs. '
              f'Only the 150 hottest code lines of each thread per report are in the log, so the totals below '
              f'cover most, not all, of each thread\'s time.\n')

        for name, functions in sorted(per_thread.items(), key=lambda item: -running[item[0]]):
            total = running[name]
            if total == 0:
                continue
            busy = 100.0 * total / max(1, total + waiting[name])
            print(f'thread {name}: {total} samples running ({busy:.0f}% of its samples)')
            for function, samples in functions.most_common(args.top):
                print(f'  {100.0 * samples / total:5.1f}%  {samples:8.0f}  {function}')
            waits = per_thread_waits.get(name)
            if waits and syscalls[name] > 0:
                print(f'  in system calls in {syscalls[name]} samples, from:')
                for chain, samples in waits.most_common(args.waits):
                    print(f'    {100.0 * samples / syscalls[name]:5.1f}%  {chain}')
            everything.update(functions)
            print()

        grand = sum(running.values())
        print(f'all threads ({grand} samples running):')
        for function, samples in everything.most_common(args.top):
            print(f'  {100.0 * samples / max(1, grand):5.1f}%  {samples:8.0f}  {function}')

    if watchdog:
        print(f'\nstall watchdog ({len(watchdog)} lines):')
        def name_offset(m):
            # The program counter is an instruction of its own function, everything else a return address.
            offset = int(m.group(2), 16)
            name = symbolize(starts, names, offset) if m.group(1) else symbolize_return(starts, names, offset)
            return f'{m.group(1) or ""}+0x{m.group(2)}={name}'

        for line in watchdog:
            print(re.sub(r'(pc )?\+0x([0-9a-fA-F]+)', name_offset, line))

    if args.hot_list:
        # Samples of a hooked function's recompiled body are named __imp__sub_X, its hook sub_X: both mean
        # the body's definition (__imp__sub_X in ppc/), which is what gets marked.
        hot = collections.Counter()
        for function, samples in everything.items():
            m = re.fullmatch(r'(?:__imp__)?sub_([0-9A-F]{8})(?:\.[\w.]+)?', function)
            if m:
                hot[m.group(1)] += samples
        chosen = [(address, samples) for address, samples in hot.most_common(args.hot_count)
                  if samples >= args.hot_min_samples]
        with open(args.hot_list, 'w', encoding='utf-8', newline='\n') as f:
            f.write('# Guest functions the Switch CPU profile found hot (tools/switch-cpu-profile.py --hot-list, from\n')
            f.write(f'# {os.path.basename(args.log)}: {len(reports)} report(s), all threads, at least '
                    f'{args.hot_min_samples:g} samples). tools/switch-direct-calls.py marks their definitions\n')
            f.write('# PPC_HOT_FUNC_IMPL: GCC optimises them harder and places them together in .text.hot.\n')
            for address, _ in chosen:
                f.write(f'{address}\n')
        print(f'\n{len(chosen)} hot functions written to {args.hot_list}')


if __name__ == '__main__':
    main()
