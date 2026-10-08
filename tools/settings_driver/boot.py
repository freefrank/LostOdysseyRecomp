#!/usr/bin/env python3
"""Boot a runtime build into the field and keep it running until <run>/release exists.

  python tools/settings_driver/boot.py --game DISC1_DIR [--exe EXE] [--run DIR]
                                       [--stage-from DIR] [KEY=VALUE ...]

Windows only. Drive the running game from another shell with drive.py and burst.py.

  --game        game data folder of disc 1                  (env LO_SETTINGS_GAME, required)
  --exe         runtime to copy into the run folder         (env LO_SETTINGS_EXE; default: the
                windows-clang build under <repo>/out/build)
  --run         run folder                                  (env LO_SETTINGS_RUN; default:
                <repo>/out/settings-run)
  --stage-from  a finished run folder with save/, profile/, shaders/, shader-cache/,
                settings.ini and the runtime DLLs, copied into --run while --run has no
                shaders/ yet (env LO_SETTINGS_STAGE_FROM). Its save has to reach the field
                from the title screen with the A/START presses of LO_AUTO_BUTTONS below.
  KEY=VALUE     environment variables for the game; they replace the defaults below.

Delete the run folder after a run that changed settings or the save: it is staged
again only when shaders/ is missing. The game stops after 30 minutes at the latest.
"""
import argparse
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DEFAULT_EXE = REPO / "out" / "build" / "windows-clang" / "LostOdysseyRecomp" / "LostOdysseyRecomp.exe"


def env_path(name):
    value = os.environ.get(name)
    return Path(value) if value else None


def stage(source, run):
    run.mkdir(parents=True, exist_ok=True)
    for item in source.iterdir():
        if item.suffix == ".dll":
            try:
                os.link(item, run / item.name)
            except OSError:  # another volume
                shutil.copy2(item, run / item.name)
    for name in ("save", "profile", "shaders", "shader-cache"):
        shutil.copytree(source / name, run / name)
    for name in ("settings.ini", "taa-collection.ini"):
        if (source / name).exists():
            shutil.copy2(source / name, run / name)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--game", type=Path, default=env_path("LO_SETTINGS_GAME"))
    ap.add_argument("--exe", type=Path, default=env_path("LO_SETTINGS_EXE") or DEFAULT_EXE)
    ap.add_argument("--run", type=Path, default=env_path("LO_SETTINGS_RUN") or REPO / "out" / "settings-run")
    ap.add_argument("--stage-from", type=Path, default=env_path("LO_SETTINGS_STAGE_FROM"))
    ap.add_argument("overrides", nargs="*", metavar="KEY=VALUE")
    args = ap.parse_args()
    if not args.game:
        ap.error("the game folder of disc 1 is needed: pass --game or set LO_SETTINGS_GAME")
    if not args.game.is_dir():
        ap.error(f"game folder {args.game} not found")
    if not args.exe.is_file():
        ap.error(f"runtime {args.exe} not found: build it, or pass --exe / set LO_SETTINGS_EXE")
    if any("=" not in kv for kv in args.overrides):
        ap.error("extra arguments must be KEY=VALUE")
    run = args.run.resolve()
    if not (run / "shaders").exists():
        if not args.stage_from:
            ap.error(f"{run} is not staged: pass --stage-from or set LO_SETTINGS_STAGE_FROM")
        if not (args.stage_from / "shaders").is_dir():
            ap.error(f"{args.stage_from} has no shaders/: it must be a finished run folder")
        stage(args.stage_from, run)
    log = run / "runtime.log"
    (run / "LostOdysseyRecomp.exe").unlink(missing_ok=True)
    shutil.copy2(args.exe, run / "LostOdysseyRecomp.exe")
    for name in ("input.txt", "shots.txt", "release", "runtime.log"):
        (run / name).unlink(missing_ok=True)
    shutil.rmtree(run / "shots", ignore_errors=True)
    (run / "shots").mkdir()

    env = {k: v for k, v in os.environ.items() if not k.upper().startswith("LO_")}
    env.update(LO_AUDIO_MUTE="1", LO_TRACE_MAP_INFO="1", LO_LOG_FILE=str(log),
               LO_AUTO_BUTTONS="s@120,a@240,a@360,a@480,a@700,a@900", LO_AUTO_PULSE="6",
               LO_TEST_INPUT_FILE=str(run / "input.txt"), LO_TEST_INPUT_TICKS="1",
               LO_SCREENSHOT_REQUEST=str(run / "shots.txt"), LO_SCREENSHOT_PATH=str(run / "shots" / "shot.ppm"),
               LO_SCREENSHOT_PRESENTED="1",
               LO_SHADER_PACK_DOWNLOAD="0", LO_SHADER_CACHE_DIR=str(run / "shader-cache"))
    for kv in args.overrides:
        k, v = kv.split("=", 1)
        env[k] = v
    proc = subprocess.Popen([str(run / "LostOdysseyRecomp.exe"), "--game", str(args.game), "--quiet-kernel"],
                            cwd=run, env=env, stdout=(run / "stdout.log").open("wb"), stderr=subprocess.STDOUT)
    (run / "pid").write_text(str(proc.pid))
    start = time.time()
    booted = False
    while proc.poll() is None and not (run / "release").exists():
        if not booted and log.exists() and "current map available=true" in log.read_text("utf-8", "replace"):
            booted = True
            print(f"booted after {time.time() - start:.0f}s", flush=True)
        if time.time() - start > 1800:
            break
        time.sleep(1)
    subprocess.run(["taskkill", "/PID", str(proc.pid), "/F"], capture_output=True)
    print("stopped", proc.poll(), flush=True)


if __name__ == "__main__":
    sys.exit(main())
