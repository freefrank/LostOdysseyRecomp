#!/usr/bin/env python3
"""Send inputs and take screenshots in a game started by boot.py.

  python tools/settings_driver/drive.py [--run DIR] STEP...

--run  run folder of boot.py (env LO_SETTINGS_RUN; default <repo>/out/settings-run)

Steps: w<seconds>; p<hexmask>[x<count>] (A=1000 B=2000 X=4000 Y=8000 START=10 Up=1
Down=2 Left=4 Right=8 LB=100 RB=200); s:<name> presented screenshot -> <run>/png/<name>.png;
g:<text> wait until a new runtime.log line contains text (30 s). Needs Pillow.
"""
import argparse
import os
import sys
import time
from pathlib import Path
from PIL import Image

REPO = Path(__file__).resolve().parents[2]
RUN = OUT = LOG = STATE = None
serial = offset = 0


def default_run():
    return Path(os.environ.get("LO_SETTINGS_RUN") or REPO / "out" / "settings-run")


def setup(run):
    """Point the module at a run folder; command serials continue from <run>/serial.txt."""
    global RUN, OUT, LOG, STATE, serial, offset
    RUN = Path(run).resolve()
    if not RUN.is_dir():
        sys.exit(f"run folder {RUN} not found: start boot.py first, or pass --run / set LO_SETTINGS_RUN")
    OUT = RUN / "png"
    OUT.mkdir(exist_ok=True)
    LOG = RUN / "runtime.log"
    STATE = RUN / "serial.txt"
    serial = int(STATE.read_text()) if STATE.exists() else int(time.time()) % 100000 * 10
    offset = LOG.stat().st_size if LOG.exists() else 0


def write(name, text):
    global serial
    serial += 1
    STATE.write_text(str(serial))
    tmp = RUN / (name + ".tmp")
    tmp.write_text(f"{serial} {text}\n")
    for _ in range(40):
        try:
            tmp.replace(RUN / name)
            return
        except PermissionError:
            time.sleep(0.05)


def new_lines():
    global offset
    if not LOG.exists():
        return []
    with LOG.open("rb") as f:
        f.seek(offset)
        data = f.read()
    cut = data.rfind(b"\n") + 1
    offset += cut
    return data[:cut].decode("utf-8", "replace").splitlines()


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--run", type=Path, default=default_run())
    ap.add_argument("steps", nargs="*", metavar="STEP")
    args = ap.parse_args()
    setup(args.run)
    for step in args.steps:
        if step.startswith("w"):
            time.sleep(float(step[1:]))
        elif step.startswith("p"):
            mask, _, count = step[1:].partition("x")
            for _ in range(int(count or 1)):
                write("input.txt", f"{mask} 0 0 6")
                time.sleep(0.5)
        elif step.startswith("g:"):
            end = time.time() + 30
            found = False
            while time.time() < end and not found:
                found = any(step[2:] in line for line in new_lines())
                time.sleep(0.2)
            print(f"wait {step[2:]!r}: {found}", flush=True)
        elif step.startswith("s:"):
            for f in (RUN / "shots").glob("*.ppm"):
                f.unlink()
            write("shots.txt", "1")
            end = time.time() + 10
            while time.time() < end and not list((RUN / "shots").glob("*.ppm")):
                time.sleep(0.2)
            time.sleep(0.5)
            shots = list((RUN / "shots").glob("*.ppm"))
            for f in shots:
                Image.open(f).convert("RGB").save(OUT / f"{step[2:]}.png")
                f.unlink()
            print(f"shot {step[2:]}: {'ok' if shots else 'missing'}", flush=True)
        else:
            sys.exit(f"unknown step {step!r}")
    for line in new_lines():
        if "calibration" in line or "settings:" in line or "screenshot" in line and "failed" in line:
            print(" ", line.split("]", 2)[-1].strip()[:200])


if __name__ == "__main__":
    main()
