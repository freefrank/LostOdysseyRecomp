#!/usr/bin/env python3
"""Inventory FPI paths and export selected UI textures/fonts without writing game data."""
import argparse
import csv
import hashlib
import json
import re
import struct
import subprocess
from collections import Counter
from pathlib import Path

from PIL import Image, ImageDraw

ARCHIVES = ("LO.fpd", "xenon_chr.fpd", "xenon_event.fpd", "xenon_field.fpd",
            "xenon_obj.fpd", "xenon_scr.fpd", "xenon_sys.fpd", "xenon_vfx.fpd",
            "xenon_world.fpd", "xenon_battle.fpd", "xenon_loc.fpd", "xenon_mov.fpd", "xenon_snd.fpd")
ALPHABET = "\0" + "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_.\\"
SUFFIXES = ("", "_sndw", "_scrw", "_mapw", "_lvdw", "_navw", "_colw", "_camw",
            "_map", "_cam", "_bx", "_mw", "_a", "_d", "_f", "_m", "_p", "_u", "_w", "_0", "_1", "_2", "_00", "_01",
            "_0mw", "_nav", "_elgt", "_000a0", "_010a0", "_020a0", "_030a0", "_040a0")


def index_files(disc):
    data = (disc / "LO.fpi").read_bytes()
    def u16(p): return struct.unpack_from("<H", data, p)[0]
    def u32(p): return struct.unpack_from("<I", data, p)[0]
    if not 64 <= len(data) <= 2 * 1024 * 1024 or u32(8) != 0x10000:
        raise ValueError(f"invalid FPI: {disc}")
    dictionary, extensions, begin, count = u32(40), u32(44), u32(32), u16(26)
    if not 1 <= count <= 64 or begin < 64 or begin + count * 48 > len(data):
        raise ValueError(f"invalid FPI table: {disc}")
    def unpack(p):
        word = u16(p)
        value = ALPHABET[word // 40 % 40] + ALPHABET[word // 1600]
        for i in range(word % 40):
            word = u16(p + 2 + i * 2)
            value += ALPHABET[word % 40] + ALPHABET[word // 40 % 40] + ALPHABET[word // 1600]
        return value.split("\0", 1)[0].lower()
    def name(bits):
        if not bits & 0x3ffff: return ""
        path = unpack(dictionary + (bits & 0x3ffff) * 2) + SUFFIXES[bits >> 18 & 31]
        ext = bits >> 23 & 31
        return path + ("." + unpack(dictionary + u16(extensions + (ext - 1) * 2) * 2) if ext else "")
    files = []
    for i in range(count):
        ar = begin + i * 48
        archive = name(u32(ar + 24))
        if archive not in {s.lower() for s in ARCHIVES}: continue
        base = ar + u32(ar + 4)
        prefix = name(u32(ar + 20))
        pending = [(0, u16(ar + 2), 0, prefix + "\\" if prefix else "")]
        visited = set()
        while pending:
            first, length, depth, parent = pending.pop()
            if depth >= 16 or first + length > 65536: raise ValueError("invalid FPI tree")
            for j in range(first, first + length):
                if j in visited: raise ValueError("overlapping FPI tree")
                visited.add(j)
                p = base + j * 24
                bits = u32(p)
                path = parent + name(bits)
                if bits & 0x10000000:
                    pending.append((u32(p + 20), u16(p + 14), depth + 1, path + "\\"))
                else:
                    files.append(dict(disc=disc.name, archive=archive, path=path,
                                      offset=(u32(p + 8) & 0xffffff) * 2048, length=u32(p + 16)))
    return files


def selected(path):
    """UI package families and their localized variants; no scene-wide guesses."""
    low = path.lower().replace("/", "\\")
    if not low.endswith((".xxx", ".upk")): return False
    parts = low.split("\\")
    stem = Path(parts[-1]).stem
    # Character/prop '_ui' and scene 'suimen' are not interface packages.
    if any(part in {"chr", "obj"} for part in parts): return False
    return (bool(re.search(r"(menu|font|hud|tutorial|controller|button|icon|dialog|guide|interface|(?:^|[_-])ui(?:[_-]|$))", stem)) or
            any(part in {"menu", "ui", "fonts", "hud", "tutorial", "staffroll"} for part in parts[:-1]) or
            stem in {"defaultuiskin", "engineresources", "enginefonts", "rpbattlecommon", "rpfieldcommon",
                     "uiarcpackage", "rpworldmapcommon", "rpworldmap", "rpgameover", "rpnavi",
                     "rpstaffroll", "title", "titleparts", "vwimages"} or "uitargetring" in stem)


def safe(value):
    return re.sub(r"[^a-zA-Z0-9_.-]+", "_", value).strip("._") or "unnamed"


def group_name(row):
    parts = row["package"].replace("\\", "/").split("/")
    language = parts[3] if len(parts) > 4 and parts[2] == "loc" else "system"
    stem = parts[-1].split(".")[0]
    category = "tutorial" if stem.startswith("tutorial") else stem.split("_" + language)[0]
    if language == "system": category = parts[2] if len(parts) > 2 else "other"
    return "/".join(map(safe, (row["disc"], language, category)))


def sheets(output, objects):
    groups = {}
    seen = set()
    for row in objects:
        if row["status"] == "exported" and row["image_path"] not in seen:
            seen.add(row["image_path"])
            groups.setdefault(group_name(row), []).append(row)
    contact = []
    for group, rows in sorted(groups.items()):
        for page in range(0, len(rows), 16):
            page_rows = rows[page:page + 16]
            sheet = Image.new("RGB", (1200, 928), (116, 116, 116))
            draw = ImageDraw.Draw(sheet)
            for cell, row in enumerate(page_rows):
                left = cell % 4 * 300
                top = cell // 4 * 232
                with Image.open(output / row["image_path"]) as raw:
                    image = raw.convert("RGBA")
                    image.thumbnail((276, 178), Image.Resampling.LANCZOS)
                at_x = left + (300 - image.width) // 2
                at_y = top + (185 - image.height) // 2
                checker = Image.new("RGBA", image.size, (198, 198, 198))
                tiles = ImageDraw.Draw(checker)
                for x in range(0, image.width, 16):
                    for y in range(0, image.height, 16):
                        if (x // 16 + y // 16) % 2:
                            tiles.rectangle((x, y, min(x + 15, image.width - 1), min(y + 15, image.height - 1)), fill=(235, 235, 235))
                checker.alpha_composite(image)
                sheet.paste(checker.convert("RGB"), (at_x, at_y))
                short_package = row["package"].replace("\\", "/").split("/")[-1].split(".")[0]
                draw.text((left + 6, top + 186), f"#{row['id']:05d} {short_package[:27]}", fill=(255, 255, 255))
                draw.text((left + 6, top + 202), f"{row['object'][:33]}  {row['width']}x{row['height']}", fill=(255, 255, 255))
            path = Path("sheets") / group / f"{page // 16 + 1:03d}.png"
            (output / path).parent.mkdir(parents=True, exist_ok=True)
            sheet.save(output / path)
            contact.append(dict(path=path.as_posix(), group=group, image_ids=[r["id"] for r in page_rows]))
    (output / "sheets" / "INDEX.txt").write_text(
        "Contact sheets: group / page | first and last manifest IDs\n" +
        "".join(f"{item['path']} | #{item['image_ids'][0]:05d}-#{item['image_ids'][-1]:05d}\n" for item in contact),
        encoding="utf-8")
    return contact


def export_assets(args, inventory):
    decoder = args.decoder or args.output / "build" / ("LoUiAssetDecoder.exe" if __import__("os").name == "nt" else "LoUiAssetDecoder")
    if not decoder.is_file(): raise RuntimeError(f"decoder binary missing: {decoder}")
    import shutil
    raw_dir = args.output / "_decode_raw"
    raw_dir.mkdir(exist_ok=False)
    objects, packages, cache = [], [], {}
    source_files = {args.private / d / "LO.fpi" for d in inventory["discs"]}
    source_files.update(args.private / item["disc"] / item["archive"] for item in inventory["selected_entries"])
    original = {str(path): (path.stat().st_size, path.stat().st_mtime_ns) for path in source_files}
    try:
        for number, entry in enumerate(inventory["selected_entries"], 1):
            archive = args.private / entry["disc"] / entry["archive"]
            package = {"id": number, **entry, "language":
                       (entry["path"].split("\\")[3] if entry["path"].startswith("bin\\xenon\\loc\\") else None)}
            packages.append(package)
            try:
                with archive.open("rb") as source:
                    source.seek(entry["offset"])
                    payload = source.read(entry["length"])
                if len(payload) != entry["length"]: raise ValueError("short package read")
                digest = hashlib.sha256(payload).hexdigest()
                package["sha256"] = digest
                if digest in cache:
                    package["decoded_from"] = cache[digest][0]
                    rows = cache[digest][1]
                else:
                    command = [str(decoder.resolve()), str(archive.resolve()), str(entry["offset"]),
                               str(entry["length"]), str(raw_dir.resolve())]
                    result = subprocess.run(command, capture_output=True, text=True, timeout=90)
                    if result.returncode:
                        raise RuntimeError(result.stderr.strip() or f"decoder exited {result.returncode}")
                    rows = []
                    for line in result.stdout.splitlines():
                        cells = line.split("\t")
                        if len(cells) != 12: raise ValueError(f"decoder returned malformed record: {line[:100]}")
                        cls, export_index, name, owner, obj_offset, obj_len, width, height, fmt, rgba, error, refs = cells
                        row = dict(export_index=int(export_index), object=name, owner=owner,
                                   object_offset=int(obj_offset), object_length=int(obj_len), cls=cls,
                                   width=int(width), height=int(height), format=int(fmt) if fmt != "-1" else None,
                                   font_page_exports=[int(p) for p in refs.split(",") if p],
                                   status="failed" if error else "font" if cls == "Font" else "exported", error=error)
                        if rgba and not error:
                            raw_path = raw_dir / rgba
                            png_path = (Path("images") / entry["disc"] /
                                        Path(*[safe(part) for part in entry["path"].split("\\")]) /
                                        f"{int(export_index):04d}_{safe(name)}.png")
                            png = args.output / png_path
                            png.parent.mkdir(parents=True, exist_ok=True)
                            data = raw_path.read_bytes()
                            if len(data) != row["width"] * row["height"] * 4:
                                row["status"] = "failed"; row["error"] = "RGBA output length mismatch"
                            else:
                                Image.frombytes("RGBA", (row["width"], row["height"]), data).save(png)
                                row["image_path"] = png_path.as_posix()
                            raw_path.unlink()
                        rows.append(row)
                    cache[digest] = (number, rows)
                for row in rows:
                    objects.append({"id": len(objects) + 1, "package_id": number, "disc": entry["disc"],
                                    "source_container": f"{entry['disc']}/{entry['archive']}",
                                    "source_offset": entry["offset"], "source_length": entry["length"],
                                    "package": entry["path"], "language": package["language"], **row})
            except Exception as exc:
                package["error"] = str(exc)
            if number % 50 == 0: print(f"processed {number}/{len(inventory['selected_entries'])} selected extents", flush=True)
    finally:
        shutil.rmtree(raw_dir)
    if {str(path): (path.stat().st_size, path.stat().st_mtime_ns) for path in source_files} != original:
        raise RuntimeError("source FPI/FPD size or modification time changed during export")
    contact = sheets(args.output, objects)
    errors = [r for r in objects if r["status"] == "failed"]
    failure_packages = [p for p in packages if p.get("error")]
    result = dict(scope=inventory["selection"], discs=inventory["discs"],
                  indexed_files=inventory["indexed_files"], packages=packages,
                  objects=objects, contact_sheets=contact,
                  summary=dict(package_entries=len(packages), unique_decoded_packages=len(cache),
                               packages_without_images=sum(not any(r["package_id"] == p["id"] for r in objects) and not p.get("error") for p in packages),
                               failed_packages=len(failure_packages), exported_pngs=len({r["image_path"] for r in objects if r["status"] == "exported"}),
                               source_mappings_with_png=sum(bool(r.get("image_path")) for r in objects),
                               failed_objects=len(errors), font_objects=sum(r["cls"] == "Font" for r in objects),
                               contact_sheets=len(contact)))
    (args.output / "manifest.json").write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
    with (args.output / "manifest.csv").open("w", encoding="utf-8-sig", newline="") as target:
        columns = ("id", "package_id", "disc", "source_container", "source_offset", "source_length", "package", "language",
                   "export_index", "object", "owner", "object_offset", "object_length", "cls", "width", "height", "format",
                   "font_page_exports", "status", "image_path", "error")
        writer = csv.DictWriter(target, fieldnames=columns, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(objects)
        writer.writerows({"package_id": p["id"], "disc": p["disc"], "source_container": f"{p['disc']}/{p['archive']}",
                          "source_offset": p["offset"], "source_length": p["length"], "package": p["path"],
                          "language": p["language"], "cls": "Package", "status": "failed", "error": p["error"]}
                         for p in failure_packages)
    print(json.dumps(result["summary"], ensure_ascii=False))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--private", type=Path, default=Path("LostOdysseyRecompLib/private"))
    parser.add_argument("--output", type=Path, default=Path("out/issue40/ui-export"))
    parser.add_argument("--inventory-only", action="store_true")
    parser.add_argument("--decoder", type=Path, help="compiled LoUiAssetDecoder (defaults to output/build)")
    args = parser.parse_args()
    if args.output.exists() and not args.inventory_only and any(
            (args.output / name).exists() for name in ("manifest.json", "manifest.csv", "images", "sheets", "_decode_raw")):
        parser.error("export already exists; choose a new output directory")
    discs = sorted(p for p in args.private.glob("disc*") if (p / "LO.fpi").is_file())
    if not discs: parser.error("no disc*/LO.fpi found")
    all_files = [item for disc in discs for item in index_files(disc)]
    candidates = [item for item in all_files if selected(item["path"])]
    args.output.mkdir(parents=True, exist_ok=True)
    inventory = dict(discs=[d.name for d in discs], indexed_files=len(all_files), selected_entries=candidates,
                     by_archive=dict(Counter(item["archive"] for item in candidates)),
                     selection=".xxx/.upk UI/menu/font/HUD/tutorial/controller/button/icon/dialog/guide/interface/world map/title/credits package names or UI/menu/fonts/HUD/tutorial/staffroll directories; engine resource and common UI families; exclude chr/obj meshes and textures")
    (args.output / "inventory.json").write_text(json.dumps(inventory, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"discs={len(discs)} indexed_files={len(all_files)} selected_entries={len(candidates)} by_archive={inventory['by_archive']}")
    if not args.inventory_only: export_assets(args, inventory)


if __name__ == "__main__": main()
