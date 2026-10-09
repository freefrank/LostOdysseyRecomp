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
    devkitpro = os.environ.get('DEVKITPRO', r'C:\devkitPro')
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


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('log')
    parser.add_argument('--elf', default=os.path.join(os.path.dirname(__file__), '..', '..', 'out', 'switch',
                                                    'LostOdysseyRecomp.elf'))
    parser.add_argument('--top', type=int, default=25)
    parser.add_argument('--report', type=int, default=None)
    parser.add_argument('--waits', type=int, default=8, help='wait sites printed per thread')
    parser.add_argument('--hot-list', default=None, help='write the hottest recompiled functions to this file')
    parser.add_argument('--hot-count', type=int, default=300)
    parser.add_argument('--hot-min-samples', type=float, default=2.0)
    args = parser.parse_args()

    starts, names = load_symbols(args.elf)
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
