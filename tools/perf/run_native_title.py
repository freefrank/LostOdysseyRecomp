"""Bounded Windows title probe. Launches only an isolated executable/profile copy.

Requires psutil. Measures OS thread CPU separately from guest-swap wall timing.
No player input, source modification, game-data write, or dependency installation.
"""
from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time

import psutil


def metadata(root: Path) -> list[tuple[str, int, int]]:
    paths = [root / "settings.ini"]
    for name in ("save", "profile"):
        paths.extend(p for p in (root / name).rglob("*") if p.is_file())
    return sorted((str(p.relative_to(root)), p.stat().st_size, p.stat().st_mtime_ns)
                  for p in paths if p.is_file())


def latest_frame(log: Path) -> int:
    if not log.exists():
        return 0
    with log.open("rb") as stream:
        stream.seek(max(0, log.stat().st_size - 131072))
        matches = re.findall(rb"frame timing completed=(\d+)", stream.read())
    return int(matches[-1]) if matches else 0


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


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("build", "baseline", "game", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--mode", choices=("off", "registers", "all"), required=True)
    parser.add_argument("--backend", choices=("d3d12", "vulkan"), default="d3d12")
    parser.add_argument("--sample-seconds", type=int, default=15)
    parser.add_argument("--warmup-frame", type=int, default=600)
    parser.add_argument("--startup-timeout", type=int, default=120)
    args = parser.parse_args()
    if os.name != "nt":
        parser.error("Windows is required")
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
    settings.update(width="1280", height="720", window_mode="0", frame_rate="60",
                    graphics_backend="0" if args.backend == "d3d12" else "1",
                    antialiasing="0", upscaler="0", internal_resolution="720",
                    frame_generation_provider="0", automatic_updates="0", skip_shader_prebuild="1")
    settings_path.write_text("\n".join(f"{k}={v}" for k, v in settings.items()) + "\n", encoding="utf-8")
    env = {k: v for k, v in os.environ.items()
           if not k.startswith(("LO_", "VK_LAYER", "VK_INSTANCE_LAYERS"))}
    env.update(LO_NATIVE_COMMANDS="0" if args.mode == "off" else args.mode,
               LO_BACKGROUND="1", LO_AUDIO_MUTE="1", LO_DLSS_FG="0", LO_FG_PROVIDER="off",
               LO_FRAME_TIMING="1", LO_LOG_FILE=str(run / "runtime.log"),
               LO_SHADER_CACHE_DIR=str(run / "shader-cache"),
               LO_SCREENSHOT_REQUEST=str(run / "screenshot-request.txt"),
               LO_SCREENSHOT_PATH=str(run / "scene.ppm"))
    result = {"mode": args.mode, "backend": args.backend, "executable": str(build / "LostOdysseyRecomp.exe"),
              "input": "none", "hidden": True, "muted": True,
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
            deadline = time.monotonic() + args.startup_timeout
            while proc.poll() is None and time.monotonic() < deadline and latest_frame(log) < args.warmup_frame:
                time.sleep(0.2)
            if proc.poll() is not None or latest_frame(log) < args.warmup_frame:
                result["failure"] = "startup did not reach the requested completed-frame boundary"
            else:
                names = thread_names(proc.pid)
                first_frame = latest_frame(log)
                first = cpu_snapshot(proc.pid)
                deadline = time.monotonic() + args.sample_seconds
                while proc.poll() is None and time.monotonic() < deadline:
                    time.sleep(0.2)
                if proc.poll() is None:
                    last = cpu_snapshot(proc.pid)
                    last_frame = latest_frame(log)
                    frames = last_frame - first_frame
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
                    # Readback is outside the timing window.
                    (run / "screenshot-request.txt").write_text("1 1\n", encoding="ascii")
                    deadline = time.monotonic() + 8
                    while proc.poll() is None and time.monotonic() < deadline and not list(run.glob("scene_*.ppm")):
                        time.sleep(0.2)
                else:
                    result["failure"] = "process exited during CPU sample"
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
