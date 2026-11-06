#!/usr/bin/env python3
"""One-command regression run over the in-game scenarios.

Runs every scenario in tools/scenarios (or the ones named with --only), then
compares each result with a local baseline: pass/fail, median FPS, median GPU
time and the final screenshot. The baseline lives in out/regression/baseline and
is never committed (screenshots are game images); `accept` makes a run the
baseline. With --switch the scenarios run on a console through its debug server
(tools/scenario.py --switch); its runs and baseline are kept apart, in
out/regression/switch, and compare and accept take --switch for them.

Usage:
  python3 tools/regression.py run [--only numara disc1-ship ...] [--accept]
  python3 tools/regression.py run --switch 192.168.1.87 [--only disc1-ship ...] [--accept]
  python3 tools/regression.py compare out/regression/runs/<timestamp>
  python3 tools/regression.py accept out/regression/runs/<timestamp>

Comparison rules (per scenario, [checks] in the TOML can override):
  - a scenario that fails its own checks fails;
  - median FPS more than 5% (and 1 FPS) below the baseline fails;
  - median GPU time more than 20% (and 0.5 ms) above the baseline is a warning
    (at a 30 FPS cap the GPU clocks down and the same scene measures 13-16 ms
    across runs; uncapped scenarios are covered by the FPS rule);
  - the final screenshot, reduced to 160x90, differs from the baseline's by more
    than max_image_diff (default 40, mean absolute difference 0-255) or its mean
    brightness moves by more than 30: fails. compare_image = false skips it
    (scenes that are not deterministic, such as the attract sequence).
Same-scene runs measured 0-15 (static) and up to ~27 (walking with TAA); a
black scene measured ~155.
"""

from __future__ import annotations

import argparse
import json
import shutil
import sys
from datetime import datetime
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import scenario  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parents[1]
SCENARIOS = REPO_ROOT / "tools/scenarios"
REGRESSION = REPO_ROOT / "out/regression"
BASELINE = REGRESSION / "baseline"
THUMB = (160, 90)


def read_ppm(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    fields, pos = [], 0
    while len(fields) < 4:
        while data[pos:pos + 1].isspace():
            pos += 1
        if data[pos:pos + 1] == b"#":
            pos = data.index(b"\n", pos) + 1
            continue
        end = pos
        while not data[end:end + 1].isspace():
            end += 1
        fields.append(data[pos:end])
        pos = end
    if fields[0] != b"P6" or fields[3] != b"255":
        raise ValueError(f"{path}: not an 8-bit binary PPM")
    return int(fields[1]), int(fields[2]), data[pos + 1:]


def thumbnail(path: Path) -> bytes:
    """Point-sampled RGB thumbnail; enough to catch black, missing or wrong scenes."""
    width, height, pixels = read_ppm(path)
    tw, th = THUMB
    out = bytearray()
    for ty in range(th):
        row = ((ty * 2 + 1) * height // (th * 2)) * width * 3
        for tx in range(tw):
            i = row + ((tx * 2 + 1) * width // (tw * 2)) * 3
            out += pixels[i:i + 3]
    return bytes(out)


def write_thumbnail(path: Path, rgb: bytes) -> None:
    path.write_bytes(b"P6\n%d %d\n255\n" % THUMB + rgb)


def luma(rgb: bytes) -> float:
    n = len(rgb) // 3
    return sum(0.299 * rgb[i] + 0.587 * rgb[i + 1] + 0.114 * rgb[i + 2] for i in range(0, len(rgb), 3)) / n


def mean_diff(a: bytes, b: bytes) -> float:
    return sum(abs(x - y) for x, y in zip(a, b)) / len(a)


def final_shot(run_dir: Path) -> Path | None:
    shots = sorted((run_dir / "shots").glob("*.ppm"), key=lambda p: int(p.stem.split("_")[-1]))
    return shots[-1] if shots else None


def scenario_files(only: list[str] | None) -> list[Path]:
    files = sorted(SCENARIOS.glob("*.toml"))
    if only:
        names = set(only)
        files = [f for f in files if f.stem in names]
        missing = names - {f.stem for f in files}
        if missing:
            raise SystemExit(f"unknown scenario(s): {', '.join(sorted(missing))}")
    return files


def compare(run_root: Path) -> tuple[list[dict], bool]:
    baseline = json.loads((BASELINE / "baseline.json").read_text()) if (BASELINE / "baseline.json").exists() else {}
    rows, ok = [], True
    for result_file in sorted(run_root.glob("*/result.json")):
        result = json.loads(result_file.read_text())
        name = result["scenario"]
        checks = scenario.load_scenario(SCENARIOS / f"{name}.toml")["checks"] if (SCENARIOS / f"{name}.toml").exists() else {}
        base = baseline.get(name)
        row = {"scenario": name, "passed": result["passed"], "fps": result.get("median_fps"),
               "gpu_ms": result.get("median_gpu_ms"), "notes": list(result["failures"]), "status": "PASS"}
        if not result["passed"]:
            row["status"] = "FAIL"
        if base:
            row["base_fps"], row["base_gpu_ms"] = base.get("median_fps"), base.get("median_gpu_ms")
            fps, base_fps = row["fps"] or 0, base.get("median_fps") or 0
            if base_fps and fps < base_fps * 0.95 and base_fps - fps >= 1:
                row["status"] = "FAIL"
                row["notes"].append(f"FPS {fps} < baseline {base_fps}")
            gpu, base_gpu = row["gpu_ms"], base.get("median_gpu_ms")
            if gpu and base_gpu and gpu > base_gpu * 1.20 and gpu - base_gpu >= 0.5:
                if row["status"] == "PASS":
                    row["status"] = "WARN"
                row["notes"].append(f"GPU {gpu} ms > baseline {base_gpu} ms")
            shot, base_shot = final_shot(result_file.parent), BASELINE / name / "final.ppm"
            if checks.get("compare_image", True) and shot and base_shot.exists():
                current, reference = thumbnail(shot), read_ppm(base_shot)[2]
                row["image_diff"] = round(mean_diff(current, reference), 1)
                row["luma"], row["base_luma"] = round(luma(current), 1), round(luma(reference), 1)
                limit = checks.get("max_image_diff", 40)
                if row["image_diff"] > limit or abs(row["luma"] - row["base_luma"]) > 30:
                    row["status"] = "FAIL"
                    row["notes"].append(f"screenshot differs: diff {row['image_diff']} (max {limit}), "
                                        f"brightness {row['base_luma']} -> {row['luma']}")
        else:
            row["notes"].append("no baseline")
        ok &= row["status"] != "FAIL"
        rows.append(row)
    return rows, ok


def report(run_root: Path, rows: list[dict], ok: bool) -> None:
    def cell(value, base=None):
        if value is None:
            return "-"
        return f"{value}" if base is None else f"{value} ({base})"
    lines = [f"# Regression run {run_root.name}", "",
             f"Result: **{'PASS' if ok else 'FAIL'}**; values in parentheses are the baseline.", "",
             "| Scenario | Status | FPS | GPU ms | Image diff | Notes |", "|---|---|---|---|---|---|"]
    for r in rows:
        lines.append(f"| {r['scenario']} | {r['status']} | {cell(r['fps'], r.get('base_fps'))} | "
                     f"{cell(r['gpu_ms'], r.get('base_gpu_ms'))} | {cell(r.get('image_diff'))} | "
                     f"{'; '.join(r['notes'])} |")
    (run_root / "report.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    (run_root / "report.json").write_text(json.dumps({"passed": ok, "rows": rows}, indent=2), encoding="utf-8")
    width = max(len(r["scenario"]) for r in rows) if rows else 10
    print()
    for r in rows:
        extra = f" img={r['image_diff']}" if "image_diff" in r else ""
        print(f"{r['scenario']:<{width}}  {r['status']:4}  fps={cell(r['fps'], r.get('base_fps'))} "
              f"gpu={cell(r['gpu_ms'], r.get('base_gpu_ms'))}{extra}" +
              "".join(f"\n{'':<{width}}    - {n}" for n in r["notes"]))
    print(f"\n{'PASS' if ok else 'FAIL'}; report: {run_root / 'report.md'}")


def accept(run_root: Path) -> None:
    baseline = {}
    staging = REGRESSION / "baseline.new"
    shutil.rmtree(staging, ignore_errors=True)
    staging.mkdir(parents=True)
    for result_file in sorted(run_root.glob("*/result.json")):
        result = json.loads(result_file.read_text())
        if not result["passed"]:
            print(f"skipped {result['scenario']}: it failed its own checks")
            continue
        baseline[result["scenario"]] = {k: result.get(k) for k in ("median_fps", "median_gpu_ms", "max_draws")}
        baseline[result["scenario"]]["run"] = run_root.name
        shot = final_shot(result_file.parent)
        if shot:
            (staging / result["scenario"]).mkdir()
            write_thumbnail(staging / result["scenario"] / "final.ppm", thumbnail(shot))
    # Scenarios not in this run keep their previous baseline entry.
    previous = json.loads((BASELINE / "baseline.json").read_text()) if (BASELINE / "baseline.json").exists() else {}
    for name, entry in previous.items():
        if name not in baseline:
            baseline[name] = entry
            if (BASELINE / name).exists():
                shutil.copytree(BASELINE / name, staging / name)
    (staging / "baseline.json").write_text(json.dumps(baseline, indent=2), encoding="utf-8")
    shutil.rmtree(BASELINE, ignore_errors=True)
    staging.rename(BASELINE)
    print(f"baseline updated from {run_root} ({len(baseline)} scenarios)")


def use_switch_folders() -> None:
    """A console's figures are another machine's: its own runs and baseline."""
    global REGRESSION, BASELINE
    REGRESSION = REPO_ROOT / "out/regression/switch"
    BASELINE = REGRESSION / "baseline"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="command", required=True)
    run = sub.add_parser("run")
    run.add_argument("--only", nargs="+")
    run.add_argument("--accept", action="store_true", help="make this run the baseline if it passes")
    run.add_argument("--bin", default=str(scenario.DEFAULT_BIN))
    run.add_argument("--game", default=str(scenario.DEFAULT_GAME))
    run.add_argument("--saves", default=str(scenario.DEFAULT_SAVES))
    scenario.add_switch_arguments(run)
    for name in ("compare", "accept"):
        sp = sub.add_parser(name)
        sp.add_argument("run_dir", type=Path)
        sp.add_argument("--switch", action="store_true", help="against the console baseline (out/regression/switch)")
    args = ap.parse_args()
    if args.switch:
        use_switch_folders()

    if args.command == "accept":
        accept(args.run_dir.resolve())
        return 0
    if args.command == "compare":
        rows, ok = compare(args.run_dir.resolve())
        report(args.run_dir.resolve(), rows, ok)
        return 0 if ok else 1

    args.bin, args.game, args.saves = (str(Path(p).resolve()) for p in (args.bin, args.game, args.saves))
    files = scenario_files(args.only)
    run_root = REGRESSION / "runs" / datetime.now().strftime("%Y%m%d-%H%M%S")
    run_root.mkdir(parents=True)
    started = datetime.now()
    for i, path in enumerate(files, 1):
        s = scenario.load_scenario(path)
        print(f"[{i}/{len(files)}] {s['name']}: {s['description']}", flush=True)
        result = scenario.make_run(s, args, run_root).execute()
        print(f"   {'PASS' if result['passed'] else 'FAIL'}  fps={result['median_fps']} "
              f"gpu={result.get('median_gpu_ms')} ms", flush=True)
    rows, ok = compare(run_root)
    report(run_root, rows, ok)
    print(f"took {(datetime.now() - started).seconds // 60} min")
    if args.accept and ok:
        accept(run_root)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
