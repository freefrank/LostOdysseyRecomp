#!/usr/bin/env python3
"""Press Y (opens the camp menu) while 45 presented frames are captured.

  python tools/settings_driver/burst.py [--run DIR]

Saves the frames quarter-size to <run>/png/burst and prints each frame's mean luma.
--run: run folder of boot.py (env LO_SETTINGS_RUN; default <repo>/out/settings-run).
Needs Pillow.
"""
import argparse
import time
from pathlib import Path
from PIL import Image

import drive


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--run", type=Path, default=drive.default_run())
    drive.setup(ap.parse_args().run)
    run = drive.RUN
    for f in (run / "shots").glob("*.ppm"):
        f.unlink()
    drive.write("shots.txt", "45")
    time.sleep(0.15)
    drive.write("input.txt", "8000 0 0 6")
    time.sleep(6)
    out = drive.OUT / "burst"
    out.mkdir(exist_ok=True)
    for f in sorted((run / "shots").glob("*.ppm"), key=lambda p: int(p.stem.split("_")[-1])):
        im = Image.open(f).convert("RGB")
        im.reduce(4).save(out / (f.stem + ".png"))
        print(f.stem, round(sum(im.convert("L").getdata()) / (im.width * im.height), 1))
        f.unlink()


if __name__ == "__main__":
    main()
