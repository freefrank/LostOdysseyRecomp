"""build_icons.py: render the icon SVGs in assets/icon and write every platform's icon file.

Needs rsvg-convert (librsvg) with the Liberation Serif font, and Pillow. On Windows, rsvg-convert runs
through WSL (`wsl rsvg-convert`). Writes:
  assets/lost-odyssey-recomp.png   1024 px: README logo and the macOS .icns source
  assets/lost-odyssey-recomp.ico   16-48 px from icon-small.svg, 64-256 px from icon.svg
  packaging/linux/io.github.freefrank.LostOdysseyRecomp.png   256 px
  packaging/android/runtime/src/main/res/mipmap-*/ic_launcher{,_foreground,_background,_monochrome}.png
"""
import io
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
RES = ROOT / "packaging/android/runtime/src/main/res"
DENSITIES = {"mdpi": 1, "hdpi": 1.5, "xhdpi": 2, "xxhdpi": 3, "xxxhdpi": 4}
SMALL_MAX = 48  # icon-small.svg up to this size


def wsl_path(path):
    drive, rest = os.path.splitdrive(str(Path(path).resolve()))
    return f"/mnt/{drive[0].lower()}{rest.replace(os.sep, '/')}"


def render(svg, size):
    """PNG bytes of one SVG at size x size."""
    with tempfile.TemporaryDirectory(dir=HERE) as temporary:
        target = Path(temporary) / "out.png"
        if shutil.which("rsvg-convert"):
            command = ["rsvg-convert", "-w", str(size), "-h", str(size), str(svg), "-o", str(target)]
        else:
            command = ["wsl.exe", "rsvg-convert", "-w", str(size), "-h", str(size), wsl_path(svg), "-o", wsl_path(target)]
        subprocess.run(command, check=True)
        return Image.open(io.BytesIO(target.read_bytes())).convert("RGBA")


def desktop(size):
    return render(HERE / ("icon-small.svg" if size <= SMALL_MAX else "icon.svg"), size)


def main():
    desktop(1024).save(ROOT / "assets/lost-odyssey-recomp.png", optimize=True)
    desktop(256).save(ROOT / "packaging/linux/io.github.freefrank.LostOdysseyRecomp.png", optimize=True)

    sizes = [16, 20, 24, 32, 40, 48, 64, 96, 128, 256]
    frames = [desktop(size) for size in sizes]
    frames[-1].save(ROOT / "assets/lost-odyssey-recomp.ico", format="ICO", sizes=[(s, s) for s in sizes],
                    append_images=frames[:-1])

    for density, scale in DENSITIES.items():
        folder = RES / f"mipmap-{density}"
        desktop(round(48 * scale)).save(folder / "ic_launcher.png", optimize=True)
        layer = round(108 * scale)
        for name in ("foreground", "background", "monochrome"):
            render(HERE / f"android-{name}.svg", layer).save(folder / f"ic_launcher_{name}.png", optimize=True)
    print("icons written")


if __name__ == "__main__":
    main()
