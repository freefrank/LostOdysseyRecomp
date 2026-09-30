#!/usr/bin/env python3
"""Linux port of .omo/cpu-perf/drive-city-3c6t.ps1 — reusable Uhra-city benchmark.

Replicates the proven Windows flow instead of the fixed-frame-only
run_city_bench.sh approach:

  * bootstrap menu navigation with LO_AUTO_BUTTONS (no always-on stick —
    holding the stick during menus skews selection into Settings);
  * poll the runtime log tail for ``frame N stats: draws=`` /
    ``render timing frame= draws=`` (not the coarse heartbeat);
  * city = draws >= 800 for 3 consecutive samples -> send walk input through
    LO_TEST_INPUT_FILE, hold 28 s with NO log tailing, then terminate;
  * screenshot marks at swaps 200/400/600/800/1000 + one city-entry shot;
  * save-integrity check + JSON summary with city/menu timing stats.

Usage:
  python3 tools/drive_city.py [--variant c0] [--hold 28] [--deadline 300]
                              [--run-dir /home/freefrank/perf/lo/run]
                              [--game /var/home/freefrank/Games/LO/]
                              [--bin /home/freefrank/build/lo/c0/.../LostOdysseyRecomp]
                              [--taskset 0x3F] [--dry-run]
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from datetime import datetime, timezone
from pathlib import Path

DRAW_STATS_RE = re.compile(r"frame (\d+) stats: draws=(\d+)")
TIMING_RE = re.compile(r"render timing frame=(\d+) draws=(\d+).*?"
                       r"draw_ms=([0-9.]+).*?vertex_ms=([0-9.]+).*?"
                       r"bind_ms=([0-9.]+).*?record_ms=([0-9.]+).*?"
                       r"fence_wait_ms=([0-9.]+)")
FULL_TIMING_RE = re.compile(
    r"render timing frame=(\d+) draws=(\d+).*?draw_ms=([0-9.]+).*?"
    r"vertex_ms=([0-9.]+).*?bind_ms=([0-9.]+).*?record_ms=([0-9.]+).*?"
    r"fence_wait_ms=([0-9.]+).*?rt_acquire_ms=([0-9.]+).*?taa_ms=([0-9.]+).*?"
    r"nested_flush_ms=([0-9.]+).*?shader_lookup_ms=([0-9.]+).*?"
    r"pipeline_lookup_ms=([0-9.]+).*?scene_copy_ms=([0-9.]+).*?"
    r"gpu_queue_batches_elapsed_ms=([0-9.unknown]+).*?gpu_batches=(\d+)")

SHOT_MARKS = (200, 400, 600, 800, 1000)
CITY_DRAWS = 800
CITY_STREAK = 3


def log_tail(path: Path, max_bytes: int = 65536) -> list[str]:
    try:
        with open(path, "rb") as f:
            f.seek(0, os.SEEK_END)
            size = f.tell()
            f.seek(max(size - max_bytes, 0))
            return f.read().decode("utf-8", "replace").splitlines()
    except OSError:
        return []


def snapshot_saves(root: Path) -> dict[str, tuple[int, str]]:
    out: dict[str, tuple[int, str]] = {}
    if not root.exists():
        return out
    for p in sorted(root.rglob("*")):
        if p.is_file():
            st = p.stat()
            out[str(p)] = (st.st_size, datetime.fromtimestamp(
                st.st_mtime, tz=timezone.utc).isoformat())
    return out


def prepare_saves(source: Path, target: Path, stamp: str) -> Path | None:
    """Stage the baseline before moving an existing run save out of the way."""
    if not source.exists():
        return None
    staged = target.parent / f".save-staging-{stamp}"
    backup = target.parent / f"save.pre-benchmark-{stamp}"
    try:
        shutil.copytree(source, staged, copy_function=shutil.copy2,
                        symlinks=True)
        if target.exists():
            target.rename(backup)
        try:
            staged.rename(target)
        except OSError:
            if backup.exists():
                backup.rename(target)
            raise
    finally:
        if staged.exists():
            shutil.rmtree(staged)
    return backup if backup.exists() else None


def save_paths_overlap(source: Path, target: Path) -> bool:
    source = source.resolve()
    target = target.resolve()
    return source == target or source in target.parents or target in source.parents


def send_input(path: Path, serial: int, mask: int = 0, x: int = 0,
               y: int = 0, polls: int = 0) -> str:
    line = f"{serial} {mask:x} {x} {y} {polls}"
    path.write_text(line + "\n", encoding="ascii")
    return line


def parse_args() -> argparse.Namespace:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--variant", default="c0")
    ap.add_argument("--hold", type=float, default=120.0,
                    help="city_hold seconds (long route into streets)")
    ap.add_argument("--walk-polls", type=int, default=6000,
                    help="walk pulse length, refreshed during hold (max 6000)")
    ap.add_argument("--walk-refresh", type=float, default=20.0,
                    help="re-send walk pulse every N seconds during hold")
    ap.add_argument("--deadline", type=float, default=300.0)
    ap.add_argument("--run-dir", default="/home/freefrank/perf/lo/run")
    ap.add_argument("--game", default="/var/home/freefrank/Games/LO/")
    ap.add_argument("--bin", default=None)
    ap.add_argument("--save-src", default="/home/freefrank/Downloads/save")
    ap.add_argument("--taskset", default=None,
                    help="e.g. 0x3F to pin like the 3c6t run; default none")
    ap.add_argument("--auto-buttons",
                    default="s@300,d@600,a@900,a@1200,a@1500,a@1800",
                    help="proven path: START, Down->Continue, Continue, "
                         "Last Saved Game (=latest user01), spares")
    ap.add_argument("--auto-pulse", default="6")
    ap.add_argument("--log-dir", default="/tmp/lo-city-logs")
    ap.add_argument("--foreground", action="store_true",
                    help="show the game window (omit LO_BACKGROUND) for watching")
    ap.add_argument("--dry-run", action="store_true")
    return ap.parse_args()


def main() -> int:
    args = parse_args()
    run_dir = Path(args.run_dir)
    bin_path = Path(args.bin) if args.bin else Path(
        f"/home/freefrank/build/lo/{args.variant}/LostOdysseyRecomp/LostOdysseyRecomp")
    save_root = run_dir / "save"
    save_src = Path(args.save_src)
    if save_paths_overlap(save_src, save_root):
        print("error: --save-src and --run-dir/save must not overlap", file=sys.stderr)
        return 2
    log_dir = Path(args.log_dir)
    stamp = f"{time.time_ns()}"
    session_log = log_dir / f"runtime-{stamp}.log"
    cmd = [str(bin_path), "--game", args.game, "--quiet-kernel"]
    if args.taskset:
        cmd = ["taskset", args.taskset, *cmd]
    print(f"bin={bin_path}\nrun={run_dir}\nlog={session_log}")
    print("cmd=" + " ".join(cmd))
    if args.dry_run:
        return 0

    log_dir.mkdir(parents=True, exist_ok=True)
    run_dir.mkdir(parents=True, exist_ok=True)
    control_dir = Path(tempfile.mkdtemp(prefix="lo-city-control-"))
    input_path = control_dir / "input.txt"
    shot_request = control_dir / "shots.txt"
    shots_dir = run_dir / "shots"
    shots_dir.mkdir(parents=True, exist_ok=True)
    input_path.write_text("1 0 0 0 0\n", encoding="ascii")
    shot_request.write_text("0 0\n", encoding="ascii")

    # Restore saves with timestamps intact (user01 must stay newer than user00),
    # then snapshot — the restore itself must not count as a game modification.
    backup = prepare_saves(save_src, save_root, stamp)
    if backup:
        print(f"previous run save preserved at {backup}")
    before = snapshot_saves(save_root)

    env = dict(os.environ)
    env.update({
        "LO_NO_UPDATE": "1",
        "LO_AUDIO_MUTE": "1",
        "LO_RENDER_TIMING": "1",
        "LO_GPU_STATS": "1",
        "LO_FRAME_TIMING": "1",
        "LO_TEST_INPUT_FILE": str(input_path),
        "LO_SCREENSHOT_REQUEST": str(shot_request),
        "LO_SCREENSHOT_PATH": str(shots_dir / "shot.ppm"),
        "LO_AUTO_BUTTONS": args.auto_buttons,
        "LO_AUTO_PULSE": args.auto_pulse,
        "LO_LOG_FILE": str(session_log),
        # NOTE: no LO_AUTO_STICK here — walk input is sent only on city entry,
        # an always-on stick skews menu selection (observed Settings trap).
    })
    if not args.foreground:
        env["LO_BACKGROUND"] = "1"
    env.pop("LO_SCREENSHOT_EVERY", None)
    env.pop("LO_SCREENSHOT_SWAP", None)
    for k in ("DISPLAY", "WAYLAND_DISPLAY", "XDG_RUNTIME_DIR", "XAUTHORITY"):
        if k in os.environ and k not in env:
            env[k] = os.environ[k]
    env.setdefault("DISPLAY", ":0")
    env.setdefault("WAYLAND_DISPLAY", "wayland-0")
    env.setdefault("XDG_RUNTIME_DIR", "/run/user/1000")
    # Foreground needs Xwayland auth; non-interactive ssh usually lacks it.
    env.setdefault("XAUTHORITY", "/run/user/1000/.mutter-Xwaylandauth.IN3BV3")

    print(f"control={control_dir}")

    started = time.time()
    try:
        sudo = subprocess.run(["sudo", "-n", "ryzenadj", "-a", "15000",
                               "-b", "15000", "-c", "15000"],
                              capture_output=True, timeout=20)
        print(f"ryzenadj rc={sudo.returncode}")
    except Exception as e:  # noqa: BLE001 — best effort TDP lock
        print(f"ryzenadj skipped: {e}")

    try:
        proc = subprocess.Popen(cmd, cwd=str(run_dir), env=env,
                                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except OSError:
        if backup:
            shutil.rmtree(save_root)
            backup.rename(save_root)
        elif save_src.exists() and save_root.exists():
            shutil.rmtree(save_root)
        shutil.rmtree(control_dir)
        raise
    print(f"pid={proc.pid}")
    status_path = Path(args.run_dir) / ".." / "drive-status.json"

    def write_status(phase: str, extra: dict | None = None) -> None:
        try:
            status_path.write_text(json.dumps({
                "phase": phase, "pid": proc.pid,
                "elapsed_s": round(time.time() - started, 1),
                "extra": extra or {},
                "time": datetime.now(timezone.utc).isoformat(),
            }), encoding="utf-8")
        except OSError:
            pass

    # Wait for the session log to appear.
    log_path = session_log
    for _ in range(80):
        if proc.poll() is not None:
            break
        if log_path.exists() and log_path.stat().st_size > 0:
            break
        time.sleep(0.4)
    write_status("log", {"log": str(log_path)})

    serial = 2
    shot_serial = 1
    last_swap = 0
    last_draws = 0
    last_shot_swap = -1
    streak = 0
    phase = "boot"
    city_start = 0.0
    last_refresh = 0.0
    walked = False

    def request_shot(swap: int) -> None:
        nonlocal shot_serial, last_shot_swap
        if swap < 0 or swap == last_shot_swap:
            return
        shot_serial += 1
        shot_request.write_text(f"{shot_serial} 1\n", encoding="ascii")
        last_shot_swap = swap

    deadline = started + args.deadline
    try:
        while time.time() < deadline:
            if proc.poll() is not None:
                write_status("exited", {"code": proc.returncode})
                break
            if phase == "city_hold":
                held = time.time() - city_start
                # Refresh walk pulse + route screenshot without log tailing.
                if held >= last_refresh + args.walk_refresh:
                    serial += 1
                    send_input(input_path, serial, 0, 0, 28000,
                               args.walk_polls)
                    # Force a route screenshot: fresh serial always triggers,
                    # even though last_swap is frozen during hold.
                    shot_serial += 1
                    shot_request.write_text(f"{shot_serial} 1\n",
                                            encoding="ascii")
                    last_refresh = held
                write_status(phase, {"swap": last_swap, "draws": last_draws,
                                     "walked": walked, "log": str(log_path)})
                if held >= args.hold:
                    phase = "done"
                    break
                time.sleep(0.6)  # no tail during hold (log-contention guard)
                continue
            for line in log_tail(log_path):
                m = DRAW_STATS_RE.search(line) or TIMING_RE.search(line)
                if m:
                    last_swap, last_draws = int(m.group(1)), int(m.group(2))
            for mark in SHOT_MARKS:
                if last_swap >= mark > last_shot_swap:
                    request_shot(last_swap)
            streak = streak + 1 if last_draws >= CITY_DRAWS else 0
            if phase == "boot" and last_swap >= 1:
                phase = "load"
            if streak >= CITY_STREAK and phase not in ("city_hold", "done"):
                if not walked:
                    serial += 1
                    send_input(input_path, serial, 0, 0, 28000,
                               args.walk_polls)
                    walked = True
                city_start = time.time()
                last_refresh = 0.0
                phase = "city_hold"
                request_shot(last_swap)
            write_status(phase, {"swap": last_swap, "draws": last_draws,
                                 "walked": walked, "log": str(log_path)})
            time.sleep(0.6)
    finally:
        if proc.poll() is None:
            proc.send_signal(signal.SIGTERM)
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()

    after = snapshot_saves(save_root)
    saves_changed = before != after

    city, menu = [], []
    if log_path.exists():
        text = log_path.read_text(encoding="utf-8", errors="replace")
        for line in text.splitlines():
            m = FULL_TIMING_RE.search(line)
            if not m:
                continue
            row = {"frame": int(m.group(1)), "draws": int(m.group(2)),
                   "draw_ms": float(m.group(3)), "vertex_ms": float(m.group(4)),
                   "bind_ms": float(m.group(5)), "record_ms": float(m.group(6)),
                   "fence_wait_ms": float(m.group(7)),
                   "rt_acquire_ms": float(m.group(8)), "taa_ms": float(m.group(9)),
                   "gpu_batches": int(m.group(15))}
            (city if row["draws"] >= CITY_DRAWS else
             menu if 70 <= row["draws"] <= 250 else []).append(row)

    def avg(rows: list[dict], k: str) -> float | None:
        return round(sum(r[k] for r in rows) / len(rows), 3) if rows else None

    def mx(rows: list[dict], k: str) -> float | None:
        return max((r[k] for r in rows), default=None)

    summary = {
        "pid": proc.pid, "variant": args.variant, "log": str(log_path),
        "control_dir": str(control_dir), "phase_end": phase,
        "hold_s": args.hold, "walk_polls": args.walk_polls,
        "elapsed_s": round(time.time() - started, 1),
        "original_saves_changed": saves_changed,
        "city_frames": len(city), "menu_frames": len(menu),
        "city": {k: avg(city, k) for k in
                 ("draws", "draw_ms", "vertex_ms", "bind_ms", "record_ms",
                  "fence_wait_ms", "rt_acquire_ms", "taa_ms", "gpu_batches")},
        "city_draws_max": mx(city, "draws"),
        "city_draw_ms_max": mx(city, "draw_ms"),
        "menu": {k: avg(menu, k) for k in ("draws", "draw_ms", "fence_wait_ms")},
        "last_swap": last_swap, "last_draws": last_draws, "walked": walked,
    }
    out_path = Path(args.run_dir) / ".." / f"drive-summary-{args.variant}.json"
    out_path.write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(json.dumps(summary, indent=2))
    write_status("finished", summary)
    return 0 if phase == "done" else 2


if __name__ == "__main__":
    sys.exit(main())
