#!/usr/bin/env python3
"""Build the dual-screen companion's theme from YOUR game files.

The companion ships no game art. This script crops the menu pieces it uses
(character portraits, brushed-metal panels, gauges, icons) out of the PNGs that
tools/ui_assets/export.py decodes from your own disc1, and writes a folder you
copy to the device:

    Android/data/io.github.freefrank.lostodyssey/files/companion-theme/

Usage:
    python tools/ui_assets/export.py --private <folder with disc1> --output ui-export --decoder <LoUiAssetDecoder>
    python tools/companion_theme.py --export ui-export --output companion-theme [--language spa]
"""
import argparse
import json
from pathlib import Path

from PIL import Image

LANGUAGES = ("spa", "int", "fra", "ita", "deu", "jpn")
CHARACTERS = ("kaim", "seth", "ming", "sarah", "jansen", "cooke", "mack", "sed", "tolten")

# (source texture, x, y, w, h) in the original atlas pixels.
CROPS = {
    # Wide portraits used by the camp menu: two columns of five 91 px rows.
    **{f"face_{n}": ("UI_MAIN_01", 0 if i % 2 == 0 else 216, (i // 2) * 91, 193, 91)
       for i, n in enumerate(CHARACTERS)},
    # Small portraits (formation / lists), one row of nine.
    **{f"face_small_{n}": ("UI_MAIN_00", 1 + i * 49, 303, 47, 38) for i, n in enumerate(CHARACTERS)},
    "panel": ("UI_MAIN_00", 0, 3, 509, 244),        # brushed-metal window with the notched corner
    "plate": ("UI_MAIN_00", 129, 487, 322, 50),     # metal name plate
    "track": ("UI_MAIN_00", 1, 545, 507, 28),       # dark gauge track
    "bar_panel": ("UI_MAIN_00", 1, 577, 507, 83),   # metal bar with a light inset
    "bottom_bar": ("UI_MAIN_00", 0, 971, 324, 52),
    "frame_small": ("UI_MAIN_00", 96, 256, 102, 44),
    "frame_small2": ("UI_MAIN_00", 224, 256, 102, 44),
    "gauge_strip": ("UI_MAIN_02", 0, 0, 340, 64),
    "metal_tile": ("UI_MAIN_02", 340, 0, 172, 64),
    "labels": ("window", 112, 0, 60, 77),           # PV/PM style labels
    "cursor": ("window", 5, 125, 40, 25),           # white arrow cursor
    "triangle": ("window", 183, 91, 25, 19),
    "compass": ("window", 108, 900, 87, 87),
    "buttons": ("Icon_Page_0", 0, 0, 256, 128),
    "navi_icons": ("FIELD_NAVI_ICON", 0, 0, 128, 64),
    "navi_skin": ("FIELD_NAVI_SKIN", 0, 0, 256, 256),
    "brushed": ("UI_MAIN_00", 6, 64, 390, 128),     # seamless brushed metal from the window body
    "icon_weapon": ("UI_MAIN_00", 0, 704, 32, 32),
    "icon_item": ("UI_MAIN_00", 0, 671, 31, 32),
    "icon_scroll": ("UI_MAIN_00", 0, 772, 46, 48),
    "icon_quill": ("UI_MAIN_00", 52, 772, 41, 48),
    "icon_emblem": ("UI_MAIN_00", 185, 772, 44, 48),
    "icon_map": ("UI_MAIN_00", 229, 772, 47, 48),
    "icon_ring": ("UI_MAIN_00", 277, 772, 46, 48),
    "icon_book": ("UI_MAIN_00", 322, 772, 48, 48),
    "icon_gear": ("UI_MAIN_00", 370, 772, 44, 48),
}

# Controller glyphs: a 36 px grid in Icon_Page_0, trimmed to their pixels.
BUTTONS = {
    "btn_lb": (0, 0), "btn_rb": (1, 0), "btn_lt": (2, 0), "btn_rt": (3, 0),
    "btn_dpad": (2, 1), "btn_back": (3, 1), "btn_start": (4, 1),
    "btn_a": (0, 2), "btn_b": (1, 2), "btn_x": (2, 2), "btn_y": (3, 2),
}


def find(export: Path, language: str, name: str) -> Path:
    order = [language] + [l for l in LANGUAGES if l != language]
    for lang in order:
        hits = sorted(export.glob(f"images/disc*/bin/xenon/loc/{lang}/menu/rpmenurescommon_{lang}.xxx/*_{name}.png"))
        if hits:
            return hits[0]
    raise SystemExit(f"{name}.png not found under {export}/images (run tools/ui_assets/export.py first)")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--export", required=True, type=Path, help="output folder of tools/ui_assets/export.py")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--language", default="spa", choices=LANGUAGES)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    atlases = {}
    written = {}
    for key, (texture, x, y, w, h) in CROPS.items():
        if texture not in atlases:
            atlases[texture] = Image.open(find(args.export, args.language, texture)).convert("RGBA")
        atlas = atlases[texture]
        crop = atlas.crop((x, y, min(x + w, atlas.width), min(y + h, atlas.height)))
        crop.save(args.output / f"{key}.png", optimize=True)
        written[key] = {"file": f"{key}.png", "w": crop.width, "h": crop.height}
    page = Image.open(find(args.export, args.language, "Icon_Page_0")).convert("RGBA")
    for key, (col, row) in BUTTONS.items():
        cell = page.crop((col * 36, row * 36, col * 36 + 36, row * 36 + 36))
        box = cell.getbbox()
        if box:
            cell = cell.crop(box)
        cell.save(args.output / f"{key}.png", optimize=True)
        written[key] = {"file": f"{key}.png", "w": cell.width, "h": cell.height}
    (args.output / "theme.json").write_text(json.dumps({"version": 1, "pieces": written}, indent=1), encoding="utf-8")
    print(f"{len(written)} pieces written to {args.output}")


if __name__ == "__main__":
    main()
