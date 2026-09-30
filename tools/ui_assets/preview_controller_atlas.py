"""Preview the CPU PlayStation glyph patch against an explicitly provided UI atlas.

The proprietary atlas is read only from --atlas. All image output goes to
--output (normally under ignored out/); no source bitmap is embedded in Git.
"""

import argparse
from pathlib import Path
import subprocess
import tempfile

from PIL import Image, ImageDraw


ATLAS_SIZE = (256, 128)
FACE_CELLS = (("A", (0, 72, 36, 108)), ("B", (36, 72, 72, 108)),
              ("X", (72, 72, 108, 108)), ("Y", (108, 72, 144, 108)))
SHOULDER_CELLS = (("LB", (0, 0, 36, 36)), ("RB", (36, 0, 72, 36)),
                  ("LT", (72, 0, 108, 36)), ("RT", (108, 0, 144, 36)))
SYSTEM_CELLS = (("BACK", (108, 36, 144, 72)), ("START", (144, 36, 180, 72)))
PAINTED = ((2, 74, 34, 108), (38, 74, 70, 108), (74, 74, 106, 108),
           (110, 74, 142, 108), (2, 4, 34, 32), (38, 4, 71, 32),
           (76, 2, 104, 34), (112, 2, 140, 34),
           (108, 36, 144, 72), (144, 36, 180, 72))


def sample_sheet(original: Image.Image, patched: Image.Image, cells: tuple, output: Path) -> None:
    sheet = Image.new("RGB", (len(cells) * 72 + 20, 2 * 62 + 18), (23, 28, 37))
    draw = ImageDraw.Draw(sheet)
    for row, (source, caption) in enumerate(((original, "XBOX"), (patched, "PS"))):
        draw.text((8, 14 + row * 62), caption, fill=(193, 202, 220))
        for column, (name, bounds) in enumerate(cells):
            x, y = 54 + column * 72, 14 + row * 62
            tile = source.crop(bounds).resize((18, 18), Image.Resampling.LANCZOS)
            sheet.paste(tile, (x, y), tile)
            draw.text((x, y + 23), name, fill=(200, 210, 230))
    sheet.save(output)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--atlas", required=True, type=Path, help="source Icon_Page_0 PNG; never modified")
    parser.add_argument("--patcher", required=True, type=Path, help="compiled controller_atlas_glyph_test executable")
    parser.add_argument("--output", required=True, type=Path, help="new or existing ignored output directory")
    args = parser.parse_args()

    original = Image.open(args.atlas).convert("RGBA")
    if original.size != ATLAS_SIZE:
        parser.error(f"expected atlas {ATLAS_SIZE}, received {original.size}")
    args.output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=args.output) as temp:
        source, destination = Path(temp) / "source.rgba", Path(temp) / "patched.rgba"
        source.write_bytes(original.tobytes())
        subprocess.run((str(args.patcher.resolve()), str(source), str(destination)), check=True)
        patched = Image.frombytes("RGBA", ATLAS_SIZE, destination.read_bytes())

    changed = 0
    protected = PAINTED
    system_tiles = tuple(bounds for _, bounds in SYSTEM_CELLS)
    for y in range(ATLAS_SIZE[1]):
        for x in range(ATLAS_SIZE[0]):
            before, after = original.getpixel((x, y)), patched.getpixel((x, y))
            within_glyph = any(x0 <= x < x1 and y0 <= y < y1 for x0, y0, x1, y1 in protected)
            within_system = any(x0 <= x < x1 and y0 <= y < y1 for x0, y0, x1, y1 in system_tiles)
            if before != after and (not within_glyph or (before[3] == 0 and not within_system)):
                raise RuntimeError(f"non-glyph or transparent source pixel changed: ({x},{y})")
            if before[3] != after[3] and not within_system:
                raise RuntimeError(f"alpha changed outside BACK/START tiles: ({x},{y})")
            changed += before != after
    if changed < 2500:
        raise RuntimeError("the PlayStation glyph patch changed too few pixels")

    background = Image.new("RGBA", ATLAS_SIZE, (23, 28, 37, 255))
    before, after = background.copy(), background.copy()
    before.alpha_composite(original)
    after.alpha_composite(patched)
    preview = Image.new("RGB", (ATLAS_SIZE[0] * 2, ATLAS_SIZE[1]), (23, 28, 37))
    preview.paste(before.convert("RGB"), (0, 0))
    preview.paste(after.convert("RGB"), (ATLAS_SIZE[0], 0))
    preview.save(args.output / "atlas-compare-1x.png")
    patched.save(args.output / "atlas-playstation.png")
    sample_sheet(original, patched, FACE_CELLS, args.output / "faces-18px.png")
    sample_sheet(original, patched, SHOULDER_CELLS, args.output / "shoulders-18px.png")
    sample_sheet(original, patched, SYSTEM_CELLS, args.output / "share-menu-18px.png")


if __name__ == "__main__":
    main()
