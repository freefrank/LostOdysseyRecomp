#!/usr/bin/env python3
"""Compare two game-free production-hook executables; never infer GPU or game FPS."""
import argparse
import json
import os
import platform
import re
import statistics
import subprocess
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("--rounds", type=int, default=7)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.rounds < 3:
        parser.error("use at least three alternating rounds")
    env = {k: v for k, v in os.environ.items() if not k.startswith("LO_SHADOW_")}
    paths = [args.baseline.resolve(), args.candidate.resolve()]

    def run(which, flag):
        return subprocess.run([str(paths[which]), flag], env=env, check=True,
                              text=True, capture_output=True).stdout.strip()

    observations = [run(i, "--observe") for i in range(2)]
    samples = [[], []]
    loads = [0, 0]
    for round_index in range(args.rounds):
        for i in (0, 1) if round_index % 2 == 0 else (1, 0):
            result = run(i, "--bench")
            match = re.fullmatch(r"frames=(\d+) ns_per_init=([\d.]+) loads_last_init=(\d+)", result)
            if not match:
                raise ValueError(f"unexpected benchmark output: {result}")
            samples[i].append(float(match[2]))
            loads[i] = int(match[3])
    medians = [statistics.median(values) for values in samples]
    report = {
        "scope": "synthetic CPU hook initialization with instrumented guest memory; no GPU or game",
        "platform": platform.platform(), "machine": platform.machine(),
        "rounds": args.rounds, "observations": dict(zip(("baseline", "candidate"), observations)),
        "ns_per_init": dict(zip(("baseline", "candidate"), samples)),
        "median_ns_per_init": dict(zip(("baseline", "candidate"), medians)),
        "loads_last_init": dict(zip(("baseline", "candidate"), loads)),
        "median_cpu_time_change_percent": (medians[1] / medians[0] - 1) * 100,
    }
    text = json.dumps(report, indent=2) + "\n"
    if args.output:
        args.output.write_text(text)
    print(text, end="")


if __name__ == "__main__":
    main()
