"""Bounded Windows native-command probe for title or Uhra gameplay.

Requires psutil. Measures OS thread CPU separately from guest-swap wall timing.
Uhra defaults to stationary Continue input. Source game data and baseline state are read-only.
"""
from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

import psutil

from native_probe_log import NativeProbeLog, frontend_delta, frontend_coverage, scene_window, probe_environment


def metadata(root: Path) -> list[tuple[str, int, int]]:
    paths = [root / "settings.ini"]
    for name in ("save", "profile"):
        paths.extend(p for p in (root / name).rglob("*") if p.is_file())
    return sorted((str(p.relative_to(root)), p.stat().st_size, p.stat().st_mtime_ns)
                  for p in paths if p.is_file())


def thread_names(pid: int) -> dict[int, str]:
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.OpenThread.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel.OpenThread.restype = wintypes.HANDLE
    kernel.GetThreadDescription.argtypes = [wintypes.HANDLE, ctypes.POINTER(ctypes.c_void_p)]
    kernel.GetThreadDescription.restype = ctypes.c_long
    kernel.LocalFree.argtypes = [ctypes.c_void_p]
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    names = {}
    for thread in psutil.Process(pid).threads():
        handle = kernel.OpenThread(0x0800, False, thread.id)
        if not handle:
            continue
        name = ctypes.c_void_p()
        try:
            if kernel.GetThreadDescription(handle, ctypes.byref(name)) >= 0 and name.value:
                names[thread.id] = ctypes.wstring_at(name)
        finally:
            if name.value:
                kernel.LocalFree(name)
            kernel.CloseHandle(handle)
    return names


def cpu_snapshot(pid: int) -> dict:
    proc = psutil.Process(pid)
    times = proc.cpu_times()
    return {"monotonic": time.perf_counter(),
            "process_seconds": times.user + times.system,
            "threads": {t.id: t.user_time + t.system_time for t in proc.threads()}}


def close_owned_windows(pid: int) -> bool:
    user = ctypes.WinDLL("user32", use_last_error=True)
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    user.EnumWindows.argtypes = [callback_type, wintypes.LPARAM]
    user.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    user.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
    posted = False

    @callback_type
    def visit(hwnd, unused):
        nonlocal posted
        owner = wintypes.DWORD()
        user.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value == pid:
            posted = bool(user.PostMessageW(hwnd, 0x0010, 0, 0)) or posted
        return True

    user.EnumWindows(visit, 0)
    return posted


def request_screenshot(proc: subprocess.Popen, run: Path, serial: int) -> str | None:
    before = set(run.glob("scene_*.ppm"))
    (run / "screenshot-request.txt").write_text(f"{serial} 1\n", encoding="ascii")
    deadline = time.monotonic() + 8
    while proc.poll() is None and time.monotonic() < deadline:
        new = set(run.glob("scene_*.ppm")) - before
        # The file becomes visible before readback finishes writing it. The
        # runtime success receipt is the publication boundary, not existence.
        log = run / "runtime.log"
        if new and log.exists():
            with log.open('rb') as stream:
                stream.seek(max(0, log.stat().st_size - 131072))
                tail = stream.read()
            for image in sorted(new):
                if (image.name.encode() + b" (ok)") in tail:
                    return image.name
        time.sleep(0.2)
    return None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("build", "baseline", "game", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--mode", choices=("off", "registers", "all"), required=True)
    parser.add_argument("--native-frontend", choices=("off", "mesh"), default="off",
                        help="ordinary SDK pre-flush frontend; independent of legacy --mode")
    parser.add_argument("--prepared-tail", action="store_true",
                        help="enable post-SDK-preparation continuation; requires mesh frontend")
    parser.add_argument("--render-timing", action="store_true",
                        help="explicit diagnostic renderer timing; excluded from ordinary performance acceptance")
    parser.add_argument("--frontend-stats", action="store_true",
                        help="diagnostic fallback and remaining PM4 opcode counts")
    parser.add_argument("--scene", choices=("title", "uhra"), default="title")
    parser.add_argument("--movement", choices=("stationary", "fixed-swaps"), default="stationary",
                        help="stationary is required for cross-build comparisons")
    parser.add_argument("--expected-map-id", type=int,
                        help="map ID verified from the chosen Uhra save; otherwise manual scene review is required")
    parser.add_argument("--scene-stats", action="store_true",
                        help="enable LO_GPU_STATS scene heartbeats and renderer CPU timers")
    parser.add_argument("--backend", choices=("d3d12", "vulkan"), default="d3d12")
    parser.add_argument("--fps", type=int, choices=(60, 120), default=60,
                        help="native game-frame target for the isolated profile")
    parser.add_argument("--sample-seconds", type=int, default=15)
    parser.add_argument("--warmup-frame", type=int)
    parser.add_argument("--startup-timeout", type=int)
    args = parser.parse_args()
    if args.prepared_tail and args.native_frontend != "mesh":
        parser.error("--prepared-tail requires --native-frontend mesh")
    if args.warmup_frame is None:
        args.warmup_frame = 3300 if args.scene == "uhra" else 600
    if args.startup_timeout is None:
        args.startup_timeout = 180 if args.scene == "uhra" else 120
    if os.name != "nt":
        parser.error("Windows is required")
    if args.scene_stats and args.scene != "uhra":
        parser.error("--scene-stats requires --scene uhra")
    if not (5 <= args.sample_seconds <= 120 and 1 <= args.warmup_frame <= 18000
            and 10 <= args.startup_timeout <= 600):
        parser.error("invalid bounded duration or frame")
    build, baseline, game, run = (p.resolve() for p in
                                (args.build, args.baseline, args.game, args.output))
    if run.exists() or run == baseline or run.is_relative_to(game) or run.is_relative_to(baseline):
        parser.error("output must be new and outside the baseline and game")
    if not (build / "LostOdysseyRecomp.exe").is_file() or not (game / "default.xex").is_file():
        parser.error("executable or default.xex is missing")
    before = metadata(baseline)
    run.mkdir(parents=True)
    for name in ("settings.ini", "profile", "save", "shaders"):
        source = baseline / name
        if source.is_dir():
            shutil.copytree(source, run / name)
        elif source.is_file():
            shutil.copy2(source, run / name)
    # Dependencies from the same installed baseline; the candidate EXE wins.
    for root in (baseline, build):
        for source in root.iterdir():
            if source.is_file() and (source.suffix.lower() == ".dll" or
                                    source.name == "LostOdysseyRecomp.exe"):
                shutil.copy2(source, run / source.name)
    settings_path = run / "settings.ini"
    settings = dict(line.split("=", 1) for line in settings_path.read_text(encoding="utf-8").splitlines()
                    if "=" in line)
    settings.update(width="1280", height="720", window_mode="0", frame_rate=str(args.fps),
                    graphics_backend="0" if args.backend == "d3d12" else "1",
                    antialiasing="0", upscaler="0", internal_resolution="720",
                    frame_generation_provider="0", automatic_updates="0", skip_shader_prebuild="1")
    settings_path.write_text("\n".join(f"{k}={v}" for k, v in settings.items()) + "\n", encoding="utf-8")
    env = probe_environment(args, run, os.environ)
    with (run / "LostOdysseyRecomp.exe").open("rb") as executable:
        executable_sha256 = hashlib.file_digest(executable, "sha256").hexdigest()
    result = {"mode": args.mode, "scene": args.scene, "backend": args.backend,
              "target_fps": args.fps,
              "scene_stats": args.scene_stats,
              "native_frontend": args.native_frontend, "prepared_tail": args.prepared_tail,
              "render_timing": args.render_timing, "frontend_stats": args.frontend_stats,
              "measurement_kind": "diagnostic" if (args.scene_stats or args.render_timing or args.frontend_stats) else "ordinary",
              "frontend_counter_scope": "executed CP backend calls; independent receipt swap window, not host GPU draws",
              "executable": str(build / "LostOdysseyRecomp.exe"),
              "executable_sha256": executable_sha256,
              "baseline": str(baseline), "game": str(game),
              "input": "none" if args.scene == "title" else "Continue; " + args.movement,
              "movement": args.movement, "expected_map_id": args.expected_map_id,
              "cross_build_input_comparable": args.movement == "stationary",
              "scene_review_required": args.scene == "uhra" and args.expected_map_id is None,
              "hidden": True, "muted": True,
              "cpu_scope": "OS user+kernel thread CPU; frame boundaries sampled from one-second log receipts",
              "same_input_image_replay": False, "forced_stop": False}
    proc = None
    try:
        with (run / "stdout.log").open("wb") as stdout, (run / "stderr.log").open("wb") as stderr:
            proc = subprocess.Popen([str(run / "LostOdysseyRecomp.exe"), "--game", str(game), "--quiet-kernel"],
                                    cwd=run, env=env, stdin=subprocess.DEVNULL, stdout=stdout, stderr=stderr,
                                    creationflags=subprocess.CREATE_NO_WINDOW)
            result["pid"] = proc.pid
            (run / "run.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
            log = run / "runtime.log"
            probe = NativeProbeLog(log)
            deadline = time.monotonic() + args.startup_timeout
            def ready() -> bool:
                scene_ready = probe.ready(args.scene, args.warmup_frame, args.scene_stats)
                if args.scene == "uhra" and args.expected_map_id is not None:
                    scene_ready = scene_ready and probe.scene['map_id'] == args.expected_map_id
                acknowledged = args.native_frontend == "off" or probe.frontend is not None
                return scene_ready and acknowledged

            while proc.poll() is None and time.monotonic() < deadline and not ready():
                time.sleep(0.2)
            if proc.poll() is not None or not ready():
                result["failure"] = "startup did not reach the requested scene/frame or acknowledge native frontend"
            else:
                # Both boundary images are outside CPU sampling. Allow the
                # first readback to retire before taking the initial CPU time.
                result['start_screenshot'] = request_screenshot(proc, run, 1)
                if result['start_screenshot'] is None:
                    raise RuntimeError("start-boundary screenshot was not produced")
                settle = time.monotonic() + 2
                while proc.poll() is None and time.monotonic() < settle:
                    time.sleep(0.2)
                    probe.poll()
                if proc.poll() is not None or not ready():
                    raise RuntimeError("scene/frame readiness was lost before CPU sample")
                names = thread_names(proc.pid)
                probe.poll()
                result['start_scene'] = probe.scene
                first_scene_changes = probe.scene_changes
                first_frame = probe.completed
                first_generation = probe.generation
                first_frontend = probe.frontend
                result["start_frontend"] = first_frontend
                result["start_frontend_diagnostics"] = probe.diagnostics
                if args.scene_stats:
                    result["start_heartbeat"] = probe.uhra_heartbeat()
                first = cpu_snapshot(proc.pid)
                deadline = time.monotonic() + args.sample_seconds
                while proc.poll() is None and time.monotonic() < deadline:
                    time.sleep(0.2)
                    probe.poll()
                if proc.poll() is None:
                    last = cpu_snapshot(proc.pid)
                    probe.poll()
                    last_frame = probe.completed
                    result["end_frontend"] = probe.frontend
                    result["end_frontend_diagnostics"] = probe.diagnostics
                    result["frontend_execution_window"] = frontend_delta(first_frontend, probe.frontend)
                    execution = result["frontend_execution_window"]
                    result['frontend_coverage'] = frontend_coverage(execution)
                    if args.native_frontend == "mesh" and (not execution or not execution["native_draws"]):
                        result["failure"] = "no actual native backend draw demonstrated (skipped mesh commands do not qualify)"
                    if args.scene == "uhra":
                        result['end_scene'] = probe.scene
                        result['scene_window_valid'] = scene_window(
                            result['start_scene'], probe.scene, probe.scene_changes == first_scene_changes,
                            last_frame)
                        if not result['scene_window_valid']:
                            result['failure'] = "map identity changed, became unavailable, or stopped refreshing during sample"
                    if args.prepared_tail and (not execution or not execution.get("prepared_draws", 0)):
                        result["failure"] = "no post-preparation native draw executed in the sample"
                    if probe.generation != first_generation:
                        result["failure"] = "runtime log was replaced or truncated during CPU sample"
                    frames = last_frame - first_frame
                    if frames <= 0:
                        result["failure"] = "no completed-frame progress during CPU sample"
                    deltas = {}
                    for tid, cpu in last["threads"].items():
                        if tid not in first["threads"]:
                            continue
                        label = names.get(tid) or str(tid)
                        deltas[label] = deltas.get(label, 0.0) + (cpu - first["threads"][tid]) * 1000
                    result.update(start_frame=first_frame, end_frame=last_frame, frames=frames,
                                  sample_wall_seconds=last["monotonic"] - first["monotonic"],
                                  process_cpu_ms=(last["process_seconds"] - first["process_seconds"]) * 1000,
                                  thread_cpu_ms=deltas,
                                  cmdproc_cpu_ms_per_logged_frame=deltas.get("GPU CmdProc", 0) / frames
                                  if frames and "GPU CmdProc" in deltas else None)
                    if args.scene_stats:
                        result["end_heartbeat"] = probe.uhra_heartbeat()
                        if result["end_heartbeat"] is None:
                            result["failure"] = "Uhra high-draw scene was lost during CPU sample"
                    result['end_screenshot'] = request_screenshot(proc, run, 2)
                    if result['end_screenshot'] is None:
                        result['failure'] = "end-boundary screenshot was not produced"
                else:
                    result["failure"] = "process exited during CPU sample"
    except (OSError, RuntimeError, psutil.Error) as error:
        result['failure'] = str(error)
    finally:
        if proc is not None and proc.poll() is None:
            result["close_posted"] = close_owned_windows(proc.pid)
            try:
                proc.wait(timeout=15)
            except subprocess.TimeoutExpired:
                result["forced_stop"] = True
                proc.terminate()
                proc.wait(timeout=10)
        result["exit_code"] = proc.returncode if proc is not None else None
        result["baseline_preserved"] = before == metadata(baseline)
        result["screenshots"] = [p.name for p in run.glob("scene_*.ppm")]
        (run / "summary.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps(result, indent=2))
    return 0 if result["exit_code"] == 0 and not result.get("failure") and result["baseline_preserved"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
