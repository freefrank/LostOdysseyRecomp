#!/usr/bin/env python3
"""Run scripted in-game scenarios and check their results.

Each scenario is a TOML file (see tools/scenarios/README.md): an optional save
slot, swap-stamped button presses for fixed menus, event-driven steps that wait
for the scene to load, and checks. Every run gets its own folder with the
runtime log, stdout, screenshots and result.json; the game's own saves,
settings and logs are never touched.

Usage:
  python3 tools/scenario.py run tools/scenarios/*.toml [--saves DIR] [--bin PATH]
  python3 tools/scenario.py list tools/scenarios/*.toml
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import signal
import statistics
import subprocess
import sys
import time
import tomllib
from datetime import datetime
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BIN = REPO_ROOT / "out/build/macos-gpu/LostOdysseyRecomp/LostOdysseyRecomp"
DEFAULT_GAME = REPO_ROOT / "LostOdysseyRecompLib/private/disc1"
DEFAULT_SAVES = REPO_ROOT / "out/drive-city/save"
DEFAULT_OUT = REPO_ROOT / "out/scenarios"
CRASH_REPORTS = Path.home() / "Library/Logs/DiagnosticReports"

# XInput button masks, as LO_TEST_INPUT_FILE expects them.
BUTTONS = {
    "up": 0x1, "down": 0x2, "left": 0x4, "right": 0x8, "start": 0x10, "back": 0x20,
    "lb": 0x100, "rb": 0x200, "a": 0x1000, "b": 0x2000, "x": 0x4000, "y": 0x8000,
}
DRAWS_RE = re.compile(r"render timing frame=(\d+) draws=(\d+)")
FRAME_MS_RE = re.compile(r"present timing completed=\d+ .*?frame_ms=([0-9.]+)")
GPU_MS_RE = re.compile(r"gpu_queue_batches_elapsed_ms=([0-9.]+)")
# One-second summaries (LO_FRAME_TIMING_SUMMARY=1): game time advanced per real second.
GAME_TIME_RE = re.compile(r"frame timing completed=\d+ .*?game_time_ratio=([0-9.]+)")
# Settings every run starts from, so results do not depend on the player's
# settings.ini; a scenario's [settings] table overrides single keys.
BASELINE_SETTINGS = {"frame_rate": 30, "internal_resolution": 0, "antialiasing": 0,
                     "upscaler": 0, "window_mode": 0, "variable_refresh_rate": 0}
FAILURES = {
    "guest bug check": re.compile(r"KeBugCheck"),
    "guest disc failure": re.compile(r"guest disc failure|dirty disc error"),
    "shader compile failure": re.compile(r"failed to compile|compile-failed"),
    "error line": re.compile(r"\[error\]"),
}


def load_scenario(path: Path) -> dict:
    data = tomllib.loads(path.read_text(encoding="utf-8"))
    data.setdefault("name", path.stem)
    data.setdefault("description", "")
    data.setdefault("save", "")
    data.setdefault("timeout", 150)
    data.setdefault("buttons", "")
    data.setdefault("timeline", "")
    data.setdefault("step", [])
    data.setdefault("checks", {})
    data.setdefault("settings", {})
    data.setdefault("env", {})
    timeline = []
    letters = {"s": "start", "a": "a", "b": "b", "x": "x", "y": "y", "u": "up", "d": "down", "l": "left", "r": "right"}
    for entry in filter(None, (e.strip() for e in data["timeline"].split(","))):
        button, _, seconds = entry.partition("@")
        if button not in letters or not seconds:
            raise ValueError(f"{path.name}: bad timeline entry '{entry}'")
        timeline.append((float(seconds), BUTTONS[letters[button]]))
    data["timeline"] = sorted(timeline)
    for step in data["step"]:
        for button in step.get("press", "").split("+") if step.get("press") else []:
            if button not in BUTTONS:
                raise ValueError(f"{path.name}: unknown button '{button}'")
    return data


def crash_reports() -> set[Path]:
    return set(CRASH_REPORTS.glob("LostOdysseyRecomp-*.ips")) if CRASH_REPORTS.exists() else set()


class Run:
    """One scenario execution: prepares the folder, drives input, collects evidence."""

    def __init__(self, scenario: dict, args: argparse.Namespace, root: Path):
        self.s = scenario
        self.args = args
        self.dir = root / scenario["name"]
        self.run_dir = self.dir / "run"
        self.shots = self.dir / "shots"
        self.log = self.dir / "runtime.log"
        self.stdout = self.dir / "stdout.log"
        self.input = self.dir / "input.txt"
        self.shot_request = self.dir / "shot-request.txt"
        self.serial = 1
        self.shot_serial = 0

    def prepare(self) -> None:
        if self.dir.exists():
            shutil.rmtree(self.dir)
        self.shots.mkdir(parents=True)
        (self.run_dir / "save").mkdir(parents=True)
        if self.s["save"]:
            source = Path(self.args.saves) / self.s["save"]
            if not (source / "save.bin").is_file():
                raise FileNotFoundError(f"save '{self.s['save']}' not found in {self.args.saves}")
            shutil.copytree(source, self.run_dir / "save" / source.name, copy_function=shutil.copy2)
        source = Path(self.args.bin).parent / "settings.ini"
        lines = source.read_text(encoding="utf-8").splitlines() if source.exists() else []
        overrides = {**BASELINE_SETTINGS, **self.s["settings"]}
        kept = [line for line in lines if line.split("=", 1)[0] not in overrides]
        kept += [f"{key}={int(value)}" for key, value in overrides.items()]
        (self.run_dir / "settings.ini").write_text("\n".join(kept) + "\n", encoding="utf-8")
        self.input.write_text("1 0 0 0 0\n", encoding="ascii")
        self.shot_request.write_text("0 0\n", encoding="ascii")

    def env(self) -> dict:
        env = dict(os.environ)
        for key in ("LO_SCREENSHOT_EVERY", "LO_SCREENSHOT_SWAP", "LO_AUTO_START", "LO_AUTO_STICK"):
            env.pop(key, None)
        env.update({
            "LO_BACKGROUND": "1", "LO_AUDIO_MUTE": "1", "LO_NO_UPDATE": "1",
            "LO_FRAME_TIMING": "1", "LO_RENDER_TIMING": "1",
            "LO_LOG_FILE": str(self.log),
            "LO_DEBUG_LOG": "1",
            "LO_TEST_INPUT_FILE": str(self.input),
            "LO_SCREENSHOT_REQUEST": str(self.shot_request),
            "LO_SCREENSHOT_PATH": str(self.shots / "shot.ppm"),
            "LO_SHADER_CACHE_DIR": str(Path(self.args.bin).parent / "cache/shaders"),
            "LO_AUTO_PULSE": str(self.s.get("pulse", 6)),
        })
        env.update({key: str(value) for key, value in self.s["env"].items()})
        # An empty [env] value removes the variable, e.g. LO_BACKGROUND = "" for a
        # foreground window (HDR output needs one).
        for key in [key for key, value in self.s["env"].items() if str(value) == ""]:
            env.pop(key, None)
        # Swap-stamped presses; an empty schedule must not fall back to "s".
        env["LO_AUTO_BUTTONS"] = self.s["buttons"] or "s@999999999"
        return env

    def send(self, mask: int = 0, x: int = 0, y: int = 0, polls: int = 20) -> None:
        self.serial += 1
        self.input.write_text(f"{self.serial} {mask:x} {x} {y} {polls}\n", encoding="ascii")

    def screenshot(self) -> None:
        self.shot_serial += 1
        self.shot_request.write_text(f"{self.shot_serial} 1\n", encoding="ascii")

    def tail(self) -> str:
        try:
            with open(self.log, "rb") as f:
                f.seek(max(f.seek(0, os.SEEK_END) - 262144, 0))
                return f.read().decode("utf-8", "replace")
        except OSError:
            return ""

    def execute(self) -> dict:
        self.prepare()
        before = crash_reports()
        cmd = [str(self.args.bin), "--game", str(self.args.game), "--quiet-kernel"]
        started = time.time()
        with open(self.stdout, "wb") as out:
            proc = subprocess.Popen(cmd, cwd=self.run_dir, env=self.env(), stdout=out, stderr=subprocess.STDOUT)
        steps = list(self.s["step"])
        timeline = list(self.s["timeline"])
        step_started = None
        max_draws = 0
        timed_out = False
        try:
            while proc.poll() is None:
                elapsed = time.time() - started
                if elapsed >= self.s["timeout"]:
                    timed_out = True
                    break
                # Wall-clock presses: independent of the frame rate, unlike "buttons".
                if timeline and elapsed >= timeline[0][0]:
                    self.send(timeline.pop(0)[1], polls=self.s.get("pulse", 6))
                for match in DRAWS_RE.finditer(self.tail()):
                    max_draws = max(max_draws, int(match.group(2)))
                if steps:
                    step = steps[0]
                    ready = max_draws >= step.get("wait_draws", 0) and elapsed >= step.get("wait_seconds", 0)
                    if step_started is None and ready:
                        step_started = time.time()
                        mask = 0
                        for button in filter(None, step.get("press", "").split("+")):
                            mask |= BUTTONS[button]
                        stick = step.get("stick", [0, 0])
                        if mask or stick != [0, 0]:
                            self.send(mask, stick[0], stick[1], step.get("polls", 20))
                        if step.get("screenshot"):
                            self.screenshot()
                    if step_started is not None and time.time() - step_started >= step.get("hold_seconds", 1):
                        steps.pop(0)
                        step_started = None
                time.sleep(0.1 if timeline else 0.5)
        finally:
            if proc.poll() is None:
                self.screenshot()
                time.sleep(2)
                proc.send_signal(signal.SIGTERM)
                try:
                    proc.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    proc.kill()
        return self.check(proc.returncode, timed_out, time.time() - started, crash_reports() - before, steps)

    def check(self, code: int | None, timed_out: bool, elapsed: float, crashes: set[Path], pending: list) -> dict:
        text = self.log.read_text(encoding="utf-8", errors="replace") if self.log.exists() else ""
        checks = self.s["checks"]
        draws = [int(m.group(2)) for m in DRAWS_RE.finditer(text)]
        frame_ms = [float(m.group(1)) for m in FRAME_MS_RE.finditer(text)]
        gpu_ms = [float(m.group(1)) for m in GPU_MS_RE.finditer(text)]
        # The second half of the run: after loading, in the scene being checked.
        tail = frame_ms[len(frame_ms) // 2:]
        gpu_tail = gpu_ms[len(gpu_ms) // 2:]
        game_time = [float(m.group(1)) for m in GAME_TIME_RE.finditer(text)]
        game_tail = game_time[len(game_time) // 2:]
        min_draws = checks.get("min_draws", 0)
        result = {
            "scenario": self.s["name"], "description": self.s["description"], "save": self.s["save"],
            "elapsed_s": round(elapsed, 1), "exit_code": code, "ran_to_timeout": timed_out,
            "max_draws": max(draws, default=0), "median_fps": round(1000 / statistics.median(tail), 1) if tail else None,
            "median_gpu_ms": round(statistics.median(gpu_tail), 2) if gpu_tail else None,
            "median_game_time_ratio": round(statistics.median(game_tail), 3) if game_tail else None,
            "screenshots": sorted(p.name for p in self.shots.glob("*.ppm")),
            "crash_reports": [str(p) for p in sorted(crashes)], "unfinished_steps": len(pending),
            "counts": {name: len(pattern.findall(text)) for name, pattern in FAILURES.items()},
            "failures": [],
        }
        failures = result["failures"]
        if crashes:
            failures.append(f"crash report: {sorted(crashes)[-1].name}")
        if not timed_out:
            failures.append(f"game exited early (code {code}) after {result['elapsed_s']} s")
        if pending:
            failures.append(f"{len(pending)} step(s) never ran (scene not reached)")
        for name, count in result["counts"].items():
            limit = checks.get("max_errors", 0) if name == "error line" else 0
            if count > limit:
                failures.append(f"{name}: {count}")
        if min_draws and result["max_draws"] < min_draws:
            failures.append(f"scene not reached: max draws {result['max_draws']} < {min_draws}")
        if checks.get("min_fps") and (result["median_fps"] or 0) < checks["min_fps"]:
            failures.append(f"median FPS {result['median_fps']} < {checks['min_fps']}")
        if checks.get("game_time_ratio"):
            low, high = checks["game_time_ratio"]
            ratio = result["median_game_time_ratio"]
            if ratio is None or not low <= ratio <= high:
                failures.append(f"game time ratio {ratio} outside [{low}, {high}] (set LO_FRAME_TIMING_SUMMARY=1)")
        result["passed"] = not failures
        (self.dir / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
        return result


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=["run", "list"])
    ap.add_argument("scenarios", nargs="+", type=Path)
    ap.add_argument("--bin", default=str(DEFAULT_BIN))
    ap.add_argument("--game", default=str(DEFAULT_GAME))
    ap.add_argument("--saves", default=str(DEFAULT_SAVES), help="folder with converted save slots (userNN)")
    ap.add_argument("--out", default=str(DEFAULT_OUT))
    args = ap.parse_args()
    args.bin, args.game, args.saves = (str(Path(p).resolve()) for p in (args.bin, args.game, args.saves))
    scenarios = [load_scenario(p) for p in args.scenarios]
    if args.command == "list":
        for s in scenarios:
            print(f"{s['name']:24} save={s['save'] or '-':8} timeout={s['timeout']:>4}s  {s['description']}")
        return 0
    root = Path(args.out).resolve() / datetime.now().strftime("%Y%m%d-%H%M%S")
    results = []
    for s in scenarios:
        print(f"== {s['name']}: {s['description']}", flush=True)
        result = Run(s, args, root).execute()
        results.append(result)
        status = "PASS" if result["passed"] else "FAIL"
        print(f"   {status}  draws={result['max_draws']} fps={result['median_fps']} "
              f"time={result['elapsed_s']}s" + ("".join(f"\n   - {f}" for f in result["failures"])), flush=True)
    (root / "summary.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
    passed = sum(r["passed"] for r in results)
    print(f"\n{passed}/{len(results)} passed; evidence in {root}")
    return 0 if passed == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
