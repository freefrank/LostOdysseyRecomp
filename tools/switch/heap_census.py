#!/usr/bin/env python3
"""Compare two heap census snapshots (os/switch/heap_census.cpp, LO_HEAP_CENSUS=1).

The console writes one snapshot every LO_HEAP_CENSUS_SECONDS to state/heap-census/<seconds>.txt
(lo_debug.py get state/heap-census/000600.txt). This prints what grew between two of them:
uordblks, the chunk size classes (exact) and the call sites (sampled), with the frames named
from the ELF the console runs (addr2line).

    python3 tools/switch/heap_census.py OLD.txt NEW.txt --elf out/switch/LostOdysseyRecomp.elf
    python3 tools/switch/heap_census.py NEW.txt --elf ...      # one snapshot: the sites holding the most
"""
import argparse
import os
import shutil
import subprocess
import sys


def load(path):
    snap = {"classes": {}, "sites": {}}
    with open(path) as f:
        for line in f:
            parts = line.split()
            if not parts:
                continue
            key = parts[0]
            if key in ("seconds",):
                snap[key] = float(parts[1])
            elif key in ("period", "uordblks", "arena"):
                snap[key] = int(parts[1])
            elif key == "ops":
                snap["ops"] = {parts[i]: int(parts[i + 1]) for i in range(1, len(parts) - 1, 2)}
            elif key == "samples":
                snap["samples"] = {"samples": int(parts[1])}
                snap["samples"].update({parts[i]: int(parts[i + 1]) for i in range(2, len(parts) - 1, 2)})
            elif key == "class":
                snap["classes"][int(parts[1])] = (int(parts[2]), int(parts[3]))
            elif key == "site":
                live, estimate, chunk, sampled, last = (int(x) for x in parts[1:6])
                frames = tuple(int(x, 16) for x in parts[6:])
                old = snap["sites"].get(frames)
                if old:  # the same frames twice cannot happen; merge to be safe
                    live, estimate, chunk, sampled = (live + old[0], estimate + old[1], chunk + old[2], sampled + old[3])
                snap["sites"][frames] = (live, estimate, chunk, sampled, last)
    return snap


def addr2line_tool():
    for name in ("aarch64-none-elf-addr2line",):
        found = shutil.which(name) or (os.path.exists(f"/opt/devkitpro/devkitA64/bin/{name}") and f"/opt/devkitpro/devkitA64/bin/{name}")
        if found:
            return found
    return None


def name_frames(elf, addresses):
    """Return addresses -> 'function (file:line)'; return addresses point after the call, so look up address - 1."""
    tool = addr2line_tool()
    if not elf or not tool or not addresses:
        return {}
    addresses = sorted(addresses)
    out = subprocess.run([tool, "-f", "-C", "-i", "-e", elf] + [f"{a - 1:x}" for a in addresses],
                         capture_output=True, text=True).stdout.splitlines()
    # -i can print several (function, location) pairs per address: ask one at a time when the counts disagree.
    if len(out) != 2 * len(addresses):
        names = {}
        for a in addresses:
            lines = subprocess.run([tool, "-f", "-C", "-e", elf, f"{a - 1:x}"], capture_output=True, text=True).stdout.splitlines()
            names[a] = shorten(lines[0] if lines else "?", lines[1] if len(lines) > 1 else "")
        return names
    return {a: shorten(out[2 * i], out[2 * i + 1]) for i, a in enumerate(addresses)}


def shorten(function, location):
    if len(function) > 110:
        function = function[:107] + "..."
    location = location.split(" (discriminator")[0]
    for marker in ("/work/",):
        if marker in location:
            location = location.split(marker, 1)[1]
    return f"{function} ({location})" if location and not location.startswith("??") else function


def kib(n):
    return f"{n / 1024:+,.1f} KiB" if n else "0"


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("snapshots", nargs="+", help="one or two snapshot files (older first)")
    parser.add_argument("--elf", help="the console's LostOdysseyRecomp.elf, to name the frames")
    parser.add_argument("--top", type=int, default=20, help="sites listed (default 20)")
    parser.add_argument("--frames", type=int, default=6, help="frames shown per site (default 6)")
    args = parser.parse_args()
    if len(args.snapshots) > 2:
        parser.error("one or two snapshots")
    new = load(args.snapshots[-1])
    old = load(args.snapshots[0]) if len(args.snapshots) == 2 else {"classes": {}, "sites": {}, "seconds": 0.0,
                                                                    "uordblks": 0, "ops": {}}
    span = new.get("seconds", 0) - old.get("seconds", 0)
    print(f"{args.snapshots[-1]}: {new.get('seconds', 0):.0f} s, period {new.get('period')}, "
          f"uordblks {new.get('uordblks', 0):,}")
    if len(args.snapshots) == 2:
        grow = new["uordblks"] - old["uordblks"]
        print(f"since {args.snapshots[0]} ({span:.0f} s): uordblks {grow:+,} bytes"
              f" ({grow / span / 1024:+.2f} KiB/s)" if span > 0 else "")
        ops = {k: new["ops"].get(k, 0) - old["ops"].get(k, 0) for k in new.get("ops", {})}
        if span > 0:
            print("calls per second: " + ", ".join(f"{k} {v / span:,.0f}" for k, v in ops.items()))
    print()
    print("chunk size class        net count      net bytes")
    total = 0
    for c in sorted(set(new["classes"]) | set(old["classes"])):
        n = new["classes"].get(c, (0, 0))
        o = old["classes"].get(c, (0, 0))
        dc, db = n[0] - o[0], n[1] - o[1]
        total += db
        if dc or db:
            print(f"  [{1 << c:>10,}, {1 << (c + 1):>10,})  {dc:+10,}  {db:+14,}")
    print(f"  all classes                     {total:+14,}")
    print()
    rows = []
    for frames in set(new["sites"]) | set(old["sites"]):
        n = new["sites"].get(frames, (0, 0, 0, 0, 0))
        o = old["sites"].get(frames, (0, 0, 0, 0, 0))
        rows.append((n[1] - o[1], n[0] - o[0], n[2] - o[2], n[3] - o[3], n[4], n[1], frames))
    rows.sort(key=lambda r: -r[0])
    shown = rows[:args.top]
    names = name_frames(args.elf, {a for r in shown for a in r[6][:args.frames]})
    title = "sites by growth of the live estimate" if len(args.snapshots) == 2 else "sites by live estimate"
    print(f"{title} (sampled; estimate = max(size, period) per allocation held):")
    for i, (dest, dlive, dchunk, dsampled, last, estimate, frames) in enumerate(shown, 1):
        print(f"#{i}: estimate {kib(dest)} (now {estimate / 1024:,.0f} KiB), held {dlive:+,}, chunk bytes {dchunk:+,}, "
              f"sampled {dsampled:+,}, last size {last:,}")
        for a in frames[:args.frames]:
            print(f"      {a:8x}  {names.get(a, '')}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
