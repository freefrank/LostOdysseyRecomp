#!/usr/bin/env python3
"""Match LO_TEXTURE_FINGERPRINT_LOG uploads against exported texture keys.

The runtime log (gpu/renderer.cpp) has one row per distinct uploaded texture;
the export index comes from `--export-assets <dir> --export-kinds fingerprints`
(or textures). Both carry the same XXH3-64 `fingerprint` of the base level's
blocks after the endian swap, plus a `fingerprint_tiled` of the stored tiled
extent. This prints how many uploads match a key and lists the misses.
"""
from __future__ import annotations

import argparse
import csv
import sys
from collections import Counter, defaultdict
from pathlib import Path

# Xenos texture formats as logged by the renderer; names as in index.csv.
XENOS_FORMATS = {2: "G8", 6: "A8R8G8B8", 18: "DXT1", 19: "DXT3", 20: "DXT5"}


def read_csv(path: Path) -> list[dict]:
    with path.open(encoding="utf-8-sig", newline="") as file:
        return list(csv.DictReader(file))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("index", type=Path, help="textures/index.csv from --export-assets")
    parser.add_argument("logs", type=Path, nargs="+", help="LO_TEXTURE_FINGERPRINT_LOG files")
    parser.add_argument("--misses", type=int, default=20, help="unmatched uploads to list")
    args = parser.parse_args(argv)

    index = read_csv(args.index)
    by_linear: dict[tuple, list[str]] = defaultdict(list)
    by_tiled: dict[tuple, list[str]] = defaultdict(list)
    for row in index:
        size = (row["format"], int(row["width"]), int(row["height"]))
        by_linear[(row["fingerprint"],) + size].append(row["key"])
        by_tiled[(row["fingerprint_tiled"],) + size].append(row["key"])

    uploads: dict[tuple, dict] = {}
    for log in args.logs:
        for row in read_csv(log):
            uploads.setdefault((row["fingerprint"], row["format"], row["width"], row["height"]), row)

    totals: Counter = Counter()
    misses = []
    for row in uploads.values():
        name = XENOS_FORMATS.get(int(row["format"]))
        eligible = name is not None and row["tiled"] == "1" and row["source_mip"] == "0"
        group = name or f"format {row['format']}"
        totals[(group, "uploads")] += 1
        if not eligible:
            totals[(group, "not eligible")] += 1
            continue
        size = (name, int(row["width"]), int(row["height"]))
        linear = by_linear.get((row["fingerprint"],) + size)
        tiled = by_tiled.get((row["fingerprint_tiled"],) + size)
        totals[(group, "eligible")] += 1
        if linear:
            totals[(group, "linear")] += 1
            totals[(group, "keys")] += len(linear)
        if tiled:
            totals[(group, "tiled")] += 1
        if not linear and not tiled:
            misses.append(row)

    groups = sorted({g for g, _ in totals})
    print(f"{'format':10s} {'uploads':>8s} {'eligible':>8s} {'linear':>8s} {'tiled':>8s}  match")
    for g in groups:
        eligible = totals[(g, "eligible")]
        rate = f"{100 * totals[(g, 'linear')] / eligible:.1f}%" if eligible else "-"
        print(f"{g:10s} {totals[(g, 'uploads')]:8d} {eligible:8d} {totals[(g, 'linear')]:8d} {totals[(g, 'tiled')]:8d}  {rate}")
    eligible = sum(totals[(g, "eligible")] for g in groups)
    linear = sum(totals[(g, "linear")] for g in groups)
    tiled = sum(totals[(g, "tiled")] for g in groups)
    print(f"total: {linear}/{eligible} eligible uploads match by fingerprint"
          f" ({100 * linear / max(eligible, 1):.1f}%), {tiled} by the tiled fingerprint")
    if misses:
        print(f"unmatched ({len(misses)}), first {min(args.misses, len(misses))}:")
        for row in sorted(misses, key=lambda r: (r["format"], -int(r["width"]) * int(r["height"])))[:args.misses]:
            print(f"  fmt {row['format']} {row['width']}x{row['height']} guest {row['guest_width']}x{row['guest_height']}"
                  f" endian {row['endian']} pitch {row['pitch_blocks']} packed ({row['packed_x']},{row['packed_y']})"
                  f" mips {row['mip_levels']} at {row['address']} frame {row['frame']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
