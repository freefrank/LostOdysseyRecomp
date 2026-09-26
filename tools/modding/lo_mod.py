#!/usr/bin/env python3
"""Build v1 image mod ZIPs without modifying imported game files (Python 3.10+)."""
from __future__ import annotations

import argparse
import csv
import json
import re
import struct
import sys
import zipfile
from pathlib import Path, PurePosixPath

MAGIC = b"LOTEX1\r\n"
HEADER = struct.Struct("<8sIIII")
MAX_PIXELS = 16 * 1024 * 1024
MAX_FILE = HEADER.size + 4096 + 4 * MAX_PIXELS
FOLDERS = {"image": "images", "font": "fonts", "model": "models", "movie": "movies"}


def text(value: object) -> str:
    if (not isinstance(value, str) or not value or len(value.encode("utf-8")) > 4096
            or any(ord(c) < 32 or ord(c) == 127 for c in value)):
        raise ValueError("expected nonempty UTF-8 text without control characters (max 4096 bytes)")
    return value


def integer(value: object, low: int, high: int) -> int:
    if type(value) is not int or not low <= value <= high:
        raise ValueError(f"expected integer in [{low}, {high}]")
    return value


def relative(value: object) -> str:
    value = text(value).replace("\\", "/")
    if value.startswith("/") or ":" in value or ".." in value.split("/"):
        raise ValueError("paths must be relative, without drive letters or '..'")
    return value


def make_key(package: str, export_index: int, object_name: str) -> str:
    package, object_name = text(package), text(object_name)
    if (package != package.strip(" \t\r\n") or object_name != object_name.strip(" \t\r\n")
            or any(c in package for c in "#=") or any(c in object_name for c in "#:=/\\")):
        raise ValueError("invalid package/object identity")
    package = relative(package)
    if package.split("/")[-1] in ("", "."):
        raise ValueError("package must name a file")
    package = str(PurePosixPath(package))
    if package == ".":
        raise ValueError("package must name a file")
    # Only ASCII case folds; Unicode bytes are unchanged, matching the C++ API.
    package = package.translate(str.maketrans("ABCDEFGHIJKLMNOPQRSTUVWXYZ", "abcdefghijklmnopqrstuvwxyz"))
    return text(f"{package}#{integer(export_index, 0, 0xffffffff)}:{object_name}")


def canonical_key(value: str) -> str:
    package, sep, tail = text(value).rpartition("#")
    index, colon, name = tail.partition(":")
    if not sep or not colon or not re.fullmatch(r"[0-9]+", index):
        raise ValueError("expected <package>#<zero-based export_index>:<object>")
    return make_key(package, int(index), name)


def overlay_path(key: str, kind: str = "image") -> str:
    if kind not in FOLDERS:
        raise ValueError("unknown asset kind")
    hash_value = 14695981039346656037
    for byte in canonical_key(key).encode("utf-8"):
        hash_value = ((hash_value ^ byte) * 1099511628211) & 0xffffffffffffffff
    suffix = ".lotex" if kind == "image" else ".loasset"
    return f"overlay/{FOLDERS[kind]}/key-fnv1a64-{hash_value:016x}{suffix}"


def dimensions(width: object, height: object) -> tuple[int, int]:
    width, height = integer(width, 1, 8192), integer(height, 1, 8192)
    if width * height > MAX_PIXELS:
        raise ValueError("image exceeds 64 MiB of RGBA pixels")
    return width, height


def encode(key: str, width: int, height: int, rgba: bytes) -> bytes:
    key_bytes = canonical_key(key).encode("utf-8")
    width, height = dimensions(width, height)
    if len(rgba) != width * height * 4:
        raise ValueError("RGBA byte count does not match dimensions")
    return HEADER.pack(MAGIC, width, height, len(key_bytes), 1) + key_bytes + rgba


def inspect(data: bytes) -> dict:
    if not HEADER.size <= len(data) <= MAX_FILE:
        raise ValueError("invalid LOTEX1 file length")
    magic, width, height, key_size, pixel_format = HEADER.unpack_from(data)
    dimensions(width, height)
    if magic != MAGIC or pixel_format != 1 or not 1 <= key_size <= 4096:
        raise ValueError("invalid LOTEX1 magic, format or key length")
    if len(data) != HEADER.size + key_size + width * height * 4:
        raise ValueError("truncated LOTEX1 payload or unexpected trailing data")
    key = data[HEADER.size:HEADER.size + key_size].decode("utf-8")
    if canonical_key(key) != key:
        raise ValueError("embedded key is not canonical")
    return {"key": key, "width": width, "height": height, "format": "RGBA8",
            "overlay_path": overlay_path(key)}


def catalog(path: Path, object_name: str | None = None, package: str | None = None) -> list[dict]:
    result: dict[str, dict] = {}
    with path.open(encoding="utf-8-sig", newline="") as file:
        reader = csv.DictReader(file)
        needed = {"status", "cls", "package", "export_index", "object", "width", "height"}
        if not needed.issubset(reader.fieldnames or []):
            raise ValueError("manifest.csv is missing required columns: " + ", ".join(sorted(needed)))
        for line, row in enumerate(reader, 2):
            if row.get("status") != "exported" or row.get("cls") != "Texture2D":
                continue
            if object_name is not None and row.get("object") != object_name:
                continue
            try:
                key = make_key(row["package"], int(row["export_index"]), row["object"])
                if package is not None and key != make_key(package, int(row["export_index"]), row["object"]):
                    continue
                width, height = dimensions(int(row["width"]), int(row["height"]))
            except (ValueError, TypeError) as exc:
                raise ValueError(f"{path}:{line}: {exc}") from exc
            item = {"key": key, "width": width, "height": height, "image_path": row.get("image_path", "")}
            if key in result and (result[key]["width"], result[key]["height"]) != (width, height):
                raise ValueError("manifest contains inconsistent dimensions for " + key)
            result[key] = item
    return [result[key] for key in sorted(result)]


def mod_id(value: object) -> str:
    value = text(value)
    if len(value) > 128 or value in (".", "..") or not re.fullmatch(r"[A-Za-z0-9_.-]+", value):
        raise ValueError("mod id must contain 1-128 ASCII letters, digits, '.', '_' or '-'")
    return value


def pack(spec_path: Path, output: Path, layout: str) -> int:
    if layout not in ("standalone", "overlay"):
        raise ValueError("layout must be standalone or overlay")
    with spec_path.open("rb") as file:
        raw = file.read(1024 * 1024 + 1)
    if len(raw) > 1024 * 1024:
        raise ValueError("mod specification exceeds 1 MiB")
    spec = json.loads(raw.decode("utf-8-sig"))
    if not isinstance(spec, dict) or set(spec) - {"api_version", "id", "priority", "images"}:
        raise ValueError("unknown top-level mod specification fields")
    integer(spec.get("api_version"), 1, 1)
    identity = mod_id(spec.get("id"))
    priority = integer(spec.get("priority", 0), -(1 << 31), (1 << 31) - 1)
    images = spec.get("images")
    if not isinstance(images, list) or not 1 <= len(images) <= 4096:
        raise ValueError("images must contain between 1 and 4096 entries")
    base = spec_path.resolve().parent
    entries = []
    keys, destinations = set(), set()
    for image in images:
        if not isinstance(image, dict) or set(image) != {"key", "source", "width", "height"}:
            raise ValueError("each image requires exactly key, source, width and height")
        key = canonical_key(image["key"])
        width, height = dimensions(image["width"], image["height"])
        source = (base / relative(image["source"])).resolve()
        if not source.is_relative_to(base) or not source.is_file():
            raise ValueError("source must be an existing file inside the specification directory")
        name = overlay_path(key)
        if key in keys or name in destinations:
            raise ValueError("duplicate asset key or destination hash collision: " + key)
        keys.add(key); destinations.add(name)
        entries.append((key, source, width, height, name))
    entries.sort(key=lambda entry: entry[0])
    try:
        from PIL import Image
    except ImportError as exc:
        raise ValueError("PNG conversion requires Pillow: python -m pip install Pillow") from exc
    output.parent.mkdir(parents=True, exist_ok=True)
    # Exclusive creation: never replace an existing archive or user's working files.
    with output.open("xb") as file:
        try:
            with zipfile.ZipFile(file, "w") as archive:
                lines = ["api_version=1", f"id={identity}", f"priority={priority}", "enabled=true"]
                def add(name: str, data: bytes) -> None:
                    info = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
                    info.compress_type = zipfile.ZIP_DEFLATED
                    info.external_attr = 0o100644 << 16
                    archive.writestr(info, data)
                for key, source, width, height, name in entries:
                    if source.stat().st_size > 128 * 1024 * 1024:
                        raise ValueError("source PNG exceeds 128 MiB")
                    with Image.open(source) as image:
                        if image.format != "PNG" or getattr(image, "n_frames", 1) != 1:
                            raise ValueError("source must be a non-animated PNG")
                        if image.size != (width, height):
                            raise ValueError(f"{source}: expected {width}x{height}, got {image.size}")
                        data = encode(key, width, height, image.convert("RGBA").tobytes())
                    if layout == "overlay":
                        destination = "mods/" + name
                    else:
                        local = "images/" + PurePosixPath(name).name
                        destination = f"mods/{identity}/{local}"
                        lines.append(f"image:{key}={local}")
                    add(destination, data)
                if layout == "standalone":
                    manifest = ("\n".join(lines) + "\n").encode("utf-8")
                    if len(manifest) > 1024 * 1024:
                        raise ValueError("generated mod.ini exceeds 1 MiB")
                    add(f"mods/{identity}/mod.ini", manifest)
        except BaseException:
            # Close before unlinking on Windows as well.
            file.close()
            output.unlink(missing_ok=True)
            raise
    return len(entries)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    key_parser = sub.add_parser("key", help="print a canonical key and overlay path")
    key_parser.add_argument("--package", required=True)
    key_parser.add_argument("--export-index", type=int, required=True)
    key_parser.add_argument("--object", required=True)
    for command in ("catalog", "init"):
        command_parser = sub.add_parser(command, help="read exported Texture2D entries from manifest.csv")
        command_parser.add_argument("--manifest", type=Path, required=True)
        command_parser.add_argument("--object")
        command_parser.add_argument("--package")
        if command == "init":
            command_parser.add_argument("--image", required=True, help="PNG path relative to the new JSON specification")
            command_parser.add_argument("--id", required=True)
            command_parser.add_argument("--priority", type=int, default=100)
            command_parser.add_argument("--output", type=Path, required=True)
    pack_parser = sub.add_parser("pack", help="compile PNG artwork and create an installable ZIP")
    pack_parser.add_argument("spec", type=Path)
    pack_parser.add_argument("--output", type=Path, required=True)
    pack_parser.add_argument("--layout", choices=("standalone", "overlay"), default="standalone")
    inspect_parser = sub.add_parser("inspect", help="validate a LOTEX1 file and print its identity")
    inspect_parser.add_argument("file", type=Path)
    args = parser.parse_args(argv)
    try:
        if args.command == "key":
            key = make_key(args.package, args.export_index, args.object)
            print(json.dumps({"key": key, "overlay_path": overlay_path(key)}, ensure_ascii=False, indent=2))
        elif args.command in ("catalog", "init"):
            items = catalog(args.manifest, args.object, args.package)
            if args.command == "catalog":
                print(json.dumps(items, ensure_ascii=False, indent=2))
            else:
                if len(items) != 1:
                    raise ValueError(f"matched {len(items)} distinct resources; specify --object and --package to select exactly one")
                item = items[0]
                spec = {"api_version": 1, "id": mod_id(args.id),
                        "priority": integer(args.priority, -(1 << 31), (1 << 31) - 1),
                        "images": [{"key": item["key"], "source": relative(args.image),
                                    "width": item["width"], "height": item["height"]}]}
                args.output.parent.mkdir(parents=True, exist_ok=True)
                with args.output.open("x", encoding="utf-8") as file:
                    file.write(json.dumps(spec, ensure_ascii=False, indent=2) + "\n")
                print(f"Created {args.output}")
        elif args.command == "pack":
            count = pack(args.spec, args.output, args.layout)
            print(f"Packed {count} image(s): {args.output} ({args.layout})")
        elif args.command == "inspect":
            with args.file.open("rb") as file:
                data = file.read(MAX_FILE + 1)
            print(json.dumps(inspect(data), ensure_ascii=False, indent=2))
        return 0
    except (OSError, ValueError, TypeError, KeyError, csv.Error, zipfile.BadZipFile) as exc:
        print(f"lo_mod: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
