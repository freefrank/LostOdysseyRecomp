#!/usr/bin/env python3
"""Sum draw-time pipeline creations (misses) per scene from a runtime log.

Usage: python tools/pipeline_misses.py <runtime log> [--frames N]

Reads the per-frame "renderer: pipeline misses" lines (always written) and,
when the run had LO_PIPELINE_MISS_LOG=1, the per-creation "pipeline miss"
lines. A scene is the map id, or the battle number plus its map. Runs with
draw-time pipeline workers add their "pipeline workers" lines: sibling builds
queued and finished, draws served by them (hits, needs LO_PIPELINE_MISS_LOG=1),
draws skipped while building (LO_PIPELINE_ASYNC=1) and time spent waiting.
"""
import argparse
import re
import sys
from collections import defaultdict

FIELD = re.compile(r"(\w+)=(\S+)")
SUMMARY = "renderer: pipeline misses "
DETAIL = "renderer: pipeline miss "
WORKERS = "renderer: pipeline workers "
PREPARE = "renderer: pipeline preparation:"


def fields(line, marker):
    return dict(FIELD.findall(line.split(marker, 1)[1]))


def scene(f):
    battle = f.get("battle", "0")
    return f"battle {battle} (map {f.get('map', '-')})" if battle != "0" else f"map {f.get('map', '-')}"


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("log")
    parser.add_argument("--frames", type=int, default=5, help="worst frames to list (default 5)")
    args = parser.parse_args()

    scenes = defaultdict(lambda: {"frames": 0, "misses": 0, "ms": 0.0, "worst": 0.0,
                                  "detail": 0, "recipe": 0, "new_vs": 0, "new_ps": 0, "known_pair": 0,
                                  "jobs": 0, "queued": 0, "built": 0, "hits": 0, "skipped": 0, "wait_ms": 0.0})
    worst = []
    prepares = []
    with open(args.log, encoding="utf-8", errors="replace") as log:
        for line in log:
            if PREPARE in line:
                prepares.append(line.split(PREPARE, 1)[1].strip())
            elif SUMMARY in line:
                f = fields(line, SUMMARY)
                s = scenes[scene(f)]
                ms = float(f.get("ms", 0))
                s["frames"] += 1
                s["misses"] += int(f.get("count", 0))
                s["ms"] += ms
                s["worst"] = max(s["worst"], ms)
                worst.append((ms, int(f.get("frame", 0)), int(f.get("count", 0)), scene(f)))
                s["wait_ms"] += float(f.get("wait_ms", 0))
            elif WORKERS in line:
                f = fields(line, WORKERS)
                s = scenes[scene(f)]
                s["jobs"] += 1
                for key in ("queued", "built", "hits", "skipped"):
                    s[key] += int(f.get(key, 0))
            elif DETAIL in line:
                f = fields(line, DETAIL)
                s = scenes[scene(f)]
                s["detail"] += 1
                s["recipe"] += f.get("recipe") == "1"
                s["new_vs"] += f.get("vs_seen") == "0"
                s["new_ps"] += f.get("ps_seen") == "0"
                s["known_pair"] += f.get("vs_seen") == "1" and f.get("ps_seen") == "1"

    for prepare in prepares:
        print(f"prebuild: {prepare}")
    if not scenes:
        print("no pipeline misses logged")
        return 0
    detail = any(s["detail"] for s in scenes.values())
    jobs = any(s["jobs"] for s in scenes.values())
    header = f"{'scene':<24} {'frames':>6} {'misses':>6} {'total ms':>9} {'worst ms':>9}"
    if detail:
        header += f" {'in recipes':>10} {'new VS':>6} {'new PS':>6} {'known VS+PS':>11}"
    if jobs:
        header += f" {'queued':>6} {'built':>6} {'hits':>6} {'skipped':>7} {'wait ms':>8}"
    print(header)
    total = {"frames": 0, "misses": 0, "ms": 0.0}
    for name, s in sorted(scenes.items(), key=lambda item: -item[1]["ms"]):
        row = f"{name:<24} {s['frames']:>6} {s['misses']:>6} {s['ms']:>9.1f} {s['worst']:>9.1f}"
        if detail:
            row += f" {s['recipe']:>10} {s['new_vs']:>6} {s['new_ps']:>6} {s['known_pair']:>11}"
        if jobs:
            row += f" {s['queued']:>6} {s['built']:>6} {s['hits']:>6} {s['skipped']:>7} {s['wait_ms']:>8.1f}"
        print(row)
        for key in total:
            total[key] += s[key]
    print(f"{'total':<24} {total['frames']:>6} {total['misses']:>6} {total['ms']:>9.1f}")
    if args.frames:
        print(f"\nworst frames:")
        for ms, frame, count, name in sorted(worst, reverse=True)[:args.frames]:
            print(f"  frame {frame}: {count} misses, {ms:.1f} ms, {name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
