#!/usr/bin/env python3
"""Sum draw-time pipeline creations (misses) per scene from a runtime log.

Usage: python tools/pipeline_misses.py <runtime log> [--frames N]

Reads the per-frame "renderer: pipeline misses" lines (always written) and,
when the run had LO_PIPELINE_MISS_LOG=1, the per-creation "pipeline miss"
lines. A scene is the map id, or the battle number plus its map. Draws that
waited for a scene prefetch job count their wait as stall time ("wait ms").
With LO_RENDER_TIMING=1 it also lists the frames with the longest draw_ms
("render timing" lines), which include draw-time shader module creation: on
Metal, MSL compilation of pack shaders costs more than the pipelines.
"""
import argparse
import re
import sys
from collections import defaultdict

FIELD = re.compile(r"(\w+)=(\S+)")
SUMMARY = "renderer: pipeline misses "
DETAIL = "renderer: pipeline miss "
PREPARE = "renderer: pipeline preparation:"
WAITS = "renderer: pipeline prefetch waits "
TIMING = "render timing frame="


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

    scenes = defaultdict(lambda: {"frames": 0, "misses": 0, "ms": 0.0, "worst": 0.0, "prefetched": 0, "wait": 0.0,
                                  "detail": 0, "recipe": 0, "new_vs": 0, "new_ps": 0, "known_pair": 0})
    worst = []
    prepares = []
    timing = []  # (draw_ms, frame, pipelines, pipeline_ms)
    with open(args.log, encoding="utf-8", errors="replace") as log:
        for line in log:
            if TIMING in line:
                f = dict(FIELD.findall(line.split("render timing ", 1)[1]))
                timing.append((float(f.get("draw_ms", 0)), int(f.get("frame", 0)), int(f.get("pipelines", 0)),
                               float(f.get("pipeline_ms", 0))))
            elif PREPARE in line:
                prepares.append(line.split(PREPARE, 1)[1].strip())
            elif SUMMARY in line or WAITS in line:
                f = fields(line, SUMMARY if SUMMARY in line else WAITS)
                s = scenes[scene(f)]
                wait = float(f.get("prefetch_wait_ms", 0))
                ms = float(f.get("ms", 0)) + wait
                s["frames"] += 1
                s["misses"] += int(f.get("count", 0))
                s["ms"] += ms
                s["wait"] += wait
                s["prefetched"] += int(f.get("prefetched", 0))
                s["worst"] = max(s["worst"], ms)
                worst.append((ms, int(f.get("frame", 0)), int(f.get("count", 0)), scene(f)))
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
    if timing:
        print(f"render timing: {len(timing)} frames, longest draw_ms:")
        for draw_ms, frame, pipelines, pipeline_ms in sorted(timing, reverse=True)[:args.frames]:
            print(f"  frame {frame}: {draw_ms:.1f} ms ({pipelines} pipelines, {pipeline_ms:.1f} ms)")
    if not scenes:
        print("no pipeline misses logged")
        return 0
    detail = any(s["detail"] for s in scenes.values())
    prefetch = any(s["prefetched"] or s["wait"] for s in scenes.values())
    header = f"{'scene':<24} {'frames':>6} {'misses':>6} {'total ms':>9} {'worst ms':>9}"
    if prefetch:
        header += f" {'prefetched':>10} {'wait ms':>8}"
    if detail:
        header += f" {'in recipes':>10} {'new VS':>6} {'new PS':>6} {'known VS+PS':>11}"
    print(header)
    total = {"frames": 0, "misses": 0, "ms": 0.0}
    for name, s in sorted(scenes.items(), key=lambda item: -item[1]["ms"]):
        row = f"{name:<24} {s['frames']:>6} {s['misses']:>6} {s['ms']:>9.1f} {s['worst']:>9.1f}"
        if prefetch:
            row += f" {s['prefetched']:>10} {s['wait']:>8.1f}"
        if detail:
            row += f" {s['recipe']:>10} {s['new_vs']:>6} {s['new_ps']:>6} {s['known_pair']:>11}"
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
