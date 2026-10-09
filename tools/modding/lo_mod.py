#!/usr/bin/env python3
"""Build v1 image mod ZIPs and LOTEX2 runtime texture packs without modifying imported game files (Python 3.10+)."""
from __future__ import annotations

import argparse
import csv
import json
import re
import shutil
import sqlite3
import struct
import sys
import zipfile
from contextlib import closing
from pathlib import Path, PurePosixPath

MAGIC = b"LOTEX1\r\n"
HEADER = struct.Struct("<8sIIII")
MAX_PIXELS = 16 * 1024 * 1024
MAX_FILE = HEADER.size + 4096 + 4 * MAX_PIXELS
MAGIC2 = b"LOTEX2\r\n"
HEADER2 = struct.Struct("<8sIIQIIIIIIQII")  # 64 bytes
XENOS_FORMATS = {"G8": 2, "A8R8G8B8": 6, "DXT1": 18, "DXT3": 19, "DXT5": 20}
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


def mip_bytes(width: int, height: int, mips: int) -> int:
    return sum(max(1, width >> i) * max(1, height >> i) * 4 for i in range(mips))


def encode2(fingerprint: int, xenos_format: int, original: tuple[int, int], size: tuple[int, int],
            mips: list[bytes], key: str = "") -> bytes:
    """LOTEX2 type 1 (RGBA8) file; `mips` are the top-first level byte strings."""
    if xenos_format not in XENOS_FORMATS.values():
        raise ValueError("unsupported Xenos format")
    scale = size[0] // original[0] if original[0] else 0
    if (scale not in (1, 2, 4, 8) or size != (original[0] * scale, original[1] * scale)
            or max(size) > 8192):
        raise ValueError(f"invalid scale {size} for original {original}")
    if not 1 <= len(mips) <= max(size).bit_length() or sum(map(len, mips)) != mip_bytes(*size, len(mips)):
        raise ValueError("mip chain does not match the payload dimensions")
    key_bytes = key.encode("utf-8")
    payload = b"".join(mips)
    return (HEADER2.pack(MAGIC2, HEADER2.size + len(key_bytes), 1, fingerprint, xenos_format,
                         *original, *size, len(mips), len(payload), len(key_bytes), 0)
            + key_bytes + payload)


def inspect2(data: bytes) -> dict:
    if len(data) < HEADER2.size:
        raise ValueError("truncated LOTEX2 header")
    (magic, header_size, payload_type, fingerprint, xenos_format, ow, oh, pw, ph, mips, payload_size,
     key_size, reserved) = HEADER2.unpack_from(data)
    if magic != MAGIC2 or reserved != 0 or header_size != HEADER2.size + key_size:
        raise ValueError("invalid LOTEX2 magic, header size or reserved field")
    if payload_type != 1:
        raise ValueError(f"unsupported LOTEX2 payload type {payload_type}")
    names = {v: k for k, v in XENOS_FORMATS.items()}
    if xenos_format not in names:
        raise ValueError(f"unknown Xenos format {xenos_format}")
    if not (ow and oh and pw % ow == 0 and pw // ow in (1, 2, 4, 8) and ph == oh * (pw // ow)
            and max(pw, ph) <= 8192):
        raise ValueError("invalid payload scale")
    if not 1 <= mips <= max(pw, ph).bit_length():
        raise ValueError("invalid mip count")
    if payload_size != mip_bytes(pw, ph, mips) or len(data) != header_size + payload_size:
        raise ValueError("payload size mismatch (truncated data or trailing bytes)")
    return {"format": "LOTEX2", "fingerprint": f"{fingerprint:016x}", "original_format": names[xenos_format],
            "original": f"{ow}x{oh}", "payload": f"{pw}x{ph}", "scale": pw // ow, "mips": mips,
            "payload_bytes": payload_size, "key": data[HEADER2.size:header_size].decode("utf-8")}


def inspect(data: bytes) -> dict:
    if data[:8] == MAGIC2:
        return inspect2(data)
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


def image_consumer(row: sqlite3.Row, key: str, width: int, height: int) -> str:
    """Static routing evidence from menu_assets.cpp, not runtime acceptance."""
    if row["format"] != 7 or any(n < 128 or n > 2048 or n & (n - 1) for n in (width, height)):
        return "no_runtime_consumer"
    package = key.partition("#")[0]
    match = re.fullmatch(r"bin/xenon/loc/(int|chi|jpn|kor|sch)/menu/(rpmenurescommon|rpfontscommon)_\1\.xxx", package)
    if match:
        if match[2] == "rpmenurescommon" and row["object"] == "UI_MAIN_00" and (width, height) == (512, 1024):
            return "native_menu_atlas"
        if (match[2] == "rpfontscommon" and row["parent_class"] == "Font"
                and row["parent_name"] in ("Maru23", "LocTit1", "Abc") and not row["parent_error"]):
            # Ownership is indexed; the Font native page-reference array is not.
            return "native_font_page_candidate"
    return "no_runtime_consumer"


def export_consumer(key: str, width: int, height: int) -> str:
    """Routing evidence for exported rows; fonts cannot be confirmed without the inventory."""
    package, _, tail = key.partition("#")
    if (re.fullmatch(r"bin/xenon/loc/(int|chi|jpn|kor|sch)/menu/rpmenurescommon_\1\.xxx", package)
            and tail.partition(":")[2] == "UI_MAIN_00" and (width, height) == (512, 1024)):
        return "native_menu_atlas"
    return "no_runtime_consumer"


def export_catalog(path: Path, object_name: str | None = None, package: str | None = None) -> list[dict]:
    """Read the textures/index.csv written by --export-assets (PNG paths are relative to it)."""
    wanted = make_key(package, 0, "Object").partition("#")[0] if package is not None else None
    result: dict[str, dict] = {}
    with path.open(encoding="utf-8-sig", newline="") as file:
        reader = csv.DictReader(file)
        needed = {"key", "file", "width", "height"}
        if not needed.issubset(reader.fieldnames or []):
            raise ValueError("index.csv is missing required columns: " + ", ".join(sorted(needed)))
        for line, row in enumerate(reader, 2):
            try:
                key = canonical_key(row["key"])
                if object_name is not None and key.rpartition(":")[2] != object_name:
                    continue
                if wanted is not None and key.partition("#")[0] != wanted:
                    continue
                width, height = dimensions(int(row["width"]), int(row["height"]))
                image = (path.resolve().parent / relative(row["file"])).resolve()
            except (ValueError, TypeError) as exc:
                raise ValueError(f"{path}:{line}: {exc}") from exc
            if key in result and (result[key]["width"], result[key]["height"]) != (width, height):
                raise ValueError("index.csv contains inconsistent dimensions for " + key)
            result[key] = {"key": key, "width": width, "height": height, "image_path": str(image),
                           "consumer": export_consumer(key, width, height)}
    return [result[key] for key in sorted(result)]


def database_catalog(path: Path, object_name: str | None = None, package: str | None = None,
                     content_sha256: str | None = None, runtime_only: bool = False,
                     limit: int | None = 100) -> list[dict]:
    """Read eligible Image identities, retaining every package-content variant."""
    if limit is not None:
        integer(limit, 1, 10000)
    clauses = ["c.cls='Texture2D'", "c.mod_key!=''", "c.key_error=''",
               "c.property_error=''", "p.status='ok'"]
    params = []
    if object_name is not None:
        clauses.append("c.object=?")
        params.append(text(object_name))
    if package is not None:
        clauses.append("substr(c.mod_key,1,instr(c.mod_key,'#')-1)=?")
        params.append(make_key(package, 0, "Object").partition("#")[0])
    if content_sha256 is not None:
        if not re.fullmatch(r"[0-9a-fA-F]{64}", content_sha256):
            raise ValueError("--content-sha256 must be a full 64-digit package SHA-256")
        clauses.append("c.sha256=?")
        params.append(content_sha256.lower())
    result = []
    with closing(sqlite3.connect(path.resolve().as_uri() + "?mode=ro", uri=True)) as db:
        db.row_factory = sqlite3.Row
        db.execute("PRAGMA query_only=ON")
        db.execute("BEGIN")
        meta = {r[0]: json.loads(r[1]) for r in db.execute(
            "SELECT key,value FROM metadata WHERE key IN ('schema_version','complete')")}
        if type(meta.get("schema_version")) is not int or meta["schema_version"] != 1:
            raise ValueError("expected asset inventory schema_version 1")
        if meta.get("complete") is not True:
            raise ValueError("asset inventory is incomplete; finish scan/reparse first")
        rows = db.execute("""SELECT c.*,parent.class_name AS parent_class,
            parent.name AS parent_name,parent.property_error AS parent_error
            FROM catalog c JOIN payloads p ON p.sha256=c.sha256
            JOIN exports e ON e.sha256=c.sha256 AND e.export_index=c.export_index
            LEFT JOIN exports parent ON parent.sha256=e.sha256 AND parent.export_index=e.outer_ref-1
            WHERE """ + " AND ".join(clauses) + " ORDER BY c.mod_key,c.sha256,c.id", params)
        for row in rows:
            try:
                key = make_key(row["package"], row["export_index"], row["object"])
            except (ValueError, TypeError) as exc:
                raise ValueError(f"catalog contains an invalid identity at asset {row['id']}: {exc}") from exc
            if key != row["mod_key"]:
                raise ValueError("catalog contains an inconsistent Mod key: " + key)
            try:
                width, height = dimensions(row["width"], row["height"])
            except (ValueError, TypeError):
                continue  # Metadata-only rows without a usable image size.
            consumer = image_consumer(row, key, width, height)
            if runtime_only and consumer == "no_runtime_consumer":
                continue
            sources = [dict(source) for source in db.execute(
                "SELECT disc,archive,offset,length FROM files WHERE sha256=? AND path=? ORDER BY disc,archive,offset",
                (row["sha256"], row["package"]))]
            result.append({"key": key, "width": width, "height": height, "image_path": "",
                           "sha256": row["sha256"], "consumer": consumer,
                           "overlay_path": overlay_path(key), "sources": sources})
            if limit is not None and len(result) >= limit:
                break
    return result


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


def hex16(value: object) -> int:
    value = str(value).strip().lower()
    if not re.fullmatch(r"[0-9a-f]{16}", value):
        raise ValueError("expected a 16 digit hex fingerprint")
    return int(value, 16)


def read_rows(path: Path, needed: set[str]) -> list[dict]:
    with path.open(encoding="utf-8-sig", newline="") as file:
        reader = csv.DictReader(file)
        missing = needed - set(reader.fieldnames or [])
        if missing:
            raise ValueError(f"{path} is missing columns: " + ", ".join(sorted(missing)))
        return list(reader)


def test_transform(image, name: str):
    """RGBA image transformed for runtime verification (tint keeps size, nearest4 is 4x)."""
    from PIL import Image
    if name == "tint":
        r, g, b, a = image.split()
        r = r.point([min(255, int(v * 1.6 + 0.5)) for v in range(256)])
        g = g.point([int(v * 0.5 + 0.5) for v in range(256)])
        b = b.point([int(v * 0.5 + 0.5) for v in range(256)])
        return Image.merge("RGBA", (r, g, b, a))
    if name == "nearest4":
        return image.resize((image.width * 4, image.height * 4), Image.NEAREST)
    raise ValueError("unknown --test transform")


def texture_pack(index: Path, images: Path, images_index: Path | None, output: Path, layout: str,
                 identity: str | None, priority: int = 0, name_filter: str | None = None,
                 fingerprints: Path | None = None, test: str | None = None, mips: bool = False) -> dict:
    try:
        from PIL import Image
    except ImportError as exc:
        raise ValueError("PNG conversion requires Pillow: python -m pip install Pillow") from exc
    if layout == "standalone":
        identity = mod_id(identity)
        priority = integer(priority, -(1 << 31), (1 << 31) - 1)
        base = output / identity
        folder = base / "textures"
    elif layout == "overlay":
        folder = output / "overlay" / "textures"
    else:
        raise ValueError("layout must be standalone or overlay")
    seen = None
    if fingerprints is not None:
        seen = {hex16(row["fingerprint"]) for row in read_rows(fingerprints, {"fingerprint"})}
    files = {}
    for row in read_rows(images_index or images / "index.csv", {"key", "file"}):
        if row["file"]:
            files[canonical_key(row["key"])] = relative(row["file"])
    stats: dict[str, int] = {"written": 0, "bytes": 0}

    def skip(reason: str) -> None:
        stats[reason] = stats.get(reason, 0) + 1

    done: set[int] = set()
    manifest = []
    for row in read_rows(index, {"key", "width", "height", "format", "fingerprint"}):
        key = canonical_key(row["key"])
        if name_filter and name_filter.lower() not in key.lower():
            continue
        if not row["fingerprint"].strip():
            skip("no_fingerprint"); continue
        fingerprint = hex16(row["fingerprint"])
        if seen is not None and fingerprint not in seen:
            skip("not_in_log"); continue
        if fingerprint in done:
            skip("duplicate_fingerprint"); continue
        xenos = XENOS_FORMATS.get(row["format"])
        if xenos is None:
            skip("unsupported_format"); continue
        source = files.get(key)
        if source is None:
            skip("no_image"); continue
        path = images / source
        if not path.is_file():
            skip("missing_file"); continue
        original = dimensions(int(row["width"]), int(row["height"]))
        with Image.open(path) as png:
            if png.format != "PNG":
                skip("not_png"); continue
            rgba = png.convert("RGBA")
        if test:
            if rgba.size != original:
                skip("size_mismatch"); continue
            rgba = test_transform(rgba, test)
        scale = rgba.width // original[0]
        if (scale not in (1, 2, 4, 8) or rgba.size != (original[0] * scale, original[1] * scale)
                or max(rgba.size) > 8192):
            skip("bad_scale"); continue
        size = rgba.size
        levels = [rgba.tobytes()]
        while mips and max(rgba.size) > 1:
            rgba = rgba.resize((max(1, rgba.width >> 1), max(1, rgba.height >> 1)), Image.BOX)
            levels.append(rgba.tobytes())
        data = encode2(fingerprint, xenos, original, size, levels, key)
        name = f"fp-{fingerprint:016x}.lotex2"
        done.add(fingerprint)
        target = folder / name
        if target.exists():
            skip("exists"); continue
        folder.mkdir(parents=True, exist_ok=True)
        with target.open("xb") as file:
            file.write(data)
        stats["written"] += 1
        stats["bytes"] += len(data)
        manifest.append(f"texture:{fingerprint:016x}=textures/{name}")
    if layout == "standalone" and manifest:
        ini = base / "mod.ini"
        if ini.exists():
            have = ini.read_text(encoding="utf-8").splitlines()
            with ini.open("a", encoding="utf-8", newline="\n") as file:
                file.write("".join(line + "\n" for line in manifest if line not in have))
        else:
            with ini.open("x", encoding="utf-8", newline="\n") as file:
                file.write("\n".join(["api_version=1", f"id={identity}", f"priority={priority}",
                                      "enabled=true"] + manifest) + "\n")
    return stats


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    key_parser = sub.add_parser("key", help="print a canonical key and overlay path")
    key_parser.add_argument("--package", required=True)
    key_parser.add_argument("--export-index", type=int, required=True)
    key_parser.add_argument("--object", required=True)
    for command in ("catalog", "init"):
        command_parser = sub.add_parser(command, help="select Texture2D entries from an export manifest or asset inventory")
        source = command_parser.add_mutually_exclusive_group(required=True)
        source.add_argument("--manifest", type=Path, help="legacy exported-image manifest.csv")
        source.add_argument("--database", type=Path, help="read-only asset inventory catalog.sqlite")
        source.add_argument("--export-index", type=Path, help="textures/index.csv from LostOdysseyRecomp.exe --export-assets")
        command_parser.add_argument("--object")
        command_parser.add_argument("--package")
        command_parser.add_argument("--content-sha256", help="database only: select one package-content variant")
        if command == "catalog":
            command_parser.add_argument("--runtime-only", action="store_true", help="database only: native menu atlas/font-page candidates")
            command_parser.add_argument("--limit", type=int, help="database only: maximum results (default 100; max 10000)")
        if command == "init":
            command_parser.add_argument("--allow-unwired", action="store_true", help="database or export index: allow experimental images, including unverified font-page candidates")
            command_parser.add_argument("--image", help="PNG path relative to the new JSON specification (required unless --export-index; then the exported PNG is copied there, default art/<object>.png)")
            command_parser.add_argument("--id", required=True)
            command_parser.add_argument("--priority", type=int, default=100)
            command_parser.add_argument("--output", type=Path, required=True)
    pack_parser = sub.add_parser("pack", help="compile PNG artwork and create an installable ZIP")
    pack_parser.add_argument("spec", type=Path)
    pack_parser.add_argument("--output", type=Path, required=True)
    pack_parser.add_argument("--layout", choices=("standalone", "overlay"), default="standalone")
    tex_parser = sub.add_parser("texture-pack", help="write LOTEX2 runtime texture replacements (experimental)")
    tex_parser.add_argument("--index", type=Path, required=True, help="export textures/index.csv with a fingerprint column")
    tex_parser.add_argument("--images", type=Path, required=True, help="PNG root mirroring the export layout")
    tex_parser.add_argument("--images-index", type=Path, help="index.csv with key,file columns (default: <images>/index.csv)")
    tex_parser.add_argument("--output", type=Path, required=True, help="mods root; existing files are never overwritten")
    tex_parser.add_argument("--layout", choices=("overlay", "standalone"), default="overlay")
    tex_parser.add_argument("--id", help="standalone mod id")
    tex_parser.add_argument("--priority", type=int, default=0)
    tex_parser.add_argument("--filter", help="case-insensitive substring of the texture key")
    tex_parser.add_argument("--fingerprints", type=Path, help="runtime fingerprint log csv; keep only fingerprints seen in game")
    tex_parser.add_argument("--test", choices=("tint", "nearest4"), help="transform the original PNGs for runtime verification")
    tex_parser.add_argument("--mips", action="store_true", help="write the full mip chain (default: 1 level)")
    inspect_parser = sub.add_parser("inspect", help="validate a LOTEX1/LOTEX2 file and print its identity")
    inspect_parser.add_argument("file", type=Path)
    args = parser.parse_args(argv)
    try:
        if args.command == "key":
            key = make_key(args.package, args.export_index, args.object)
            print(json.dumps({"key": key, "overlay_path": overlay_path(key)}, ensure_ascii=False, indent=2))
        elif args.command in ("catalog", "init"):
            if args.database:
                # Init must see all matches; a display limit must never hide ambiguity.
                items = database_catalog(args.database, args.object, args.package, args.content_sha256,
                                         getattr(args, "runtime_only", False),
                                         (args.limit if args.limit is not None else 100) if args.command == "catalog" else None)
            elif args.export_index:
                if args.content_sha256 or getattr(args, "runtime_only", False) or getattr(args, "limit", None) is not None:
                    raise ValueError("--content-sha256, --runtime-only and --limit require --database")
                items = export_catalog(args.export_index, args.object, args.package)
            else:
                if (args.content_sha256 or getattr(args, "runtime_only", False)
                        or getattr(args, "limit", None) is not None or getattr(args, "allow_unwired", False)):
                    raise ValueError("--content-sha256, --runtime-only, --limit and --allow-unwired require --database or --export-index")
                items = catalog(args.manifest, args.object, args.package)
            if args.command == "catalog":
                print(json.dumps(items, ensure_ascii=False, indent=2))
            else:
                if len(items) != 1:
                    raise ValueError(f"matched {len(items)} eligible resources/content variants; select exactly one with --object, --package and (for database variants) --content-sha256")
                item = items[0]
                if (args.database or args.export_index) and item["consumer"] == "no_runtime_consumer" and not args.allow_unwired:
                    raise ValueError("selected image has no current runtime consumer; --allow-unwired permits experimental packaging only")
                if args.database and item["consumer"] == "native_font_page_candidate" and not args.allow_unwired:
                    raise ValueError("font-page reference is unverified; --allow-unwired permits experimental packaging only")
                if args.image is None and not args.export_index:
                    raise ValueError("--image is required")
                image = args.image or "art/" + re.sub(r"[^A-Za-z0-9_.-]", "_", item["key"].rpartition(":")[2]) + ".png"
                spec = {"api_version": 1, "id": mod_id(args.id),
                        "priority": integer(args.priority, -(1 << 31), (1 << 31) - 1),
                        "images": [{"key": item["key"], "source": relative(image),
                                    "width": item["width"], "height": item["height"]}]}
                if args.output.exists():
                    raise FileExistsError(f"{args.output} already exists")
                args.output.parent.mkdir(parents=True, exist_ok=True)
                if args.export_index:
                    # Start from the exported artwork; never replace the author's own file.
                    base = args.output.resolve().parent
                    target = (base / spec["images"][0]["source"]).resolve()
                    if not target.is_relative_to(base):
                        raise ValueError("--image must stay inside the specification directory")
                    target.parent.mkdir(parents=True, exist_ok=True)
                    with open(item["image_path"], "rb") as src, target.open("xb") as dst:
                        shutil.copyfileobj(src, dst)
                    print(f"Copied {target}")
                with args.output.open("x", encoding="utf-8") as file:
                    file.write(json.dumps(spec, ensure_ascii=False, indent=2) + "\n")
                print(f"Created {args.output}")
        elif args.command == "pack":
            count = pack(args.spec, args.output, args.layout)
            print(f"Packed {count} image(s): {args.output} ({args.layout})")
        elif args.command == "texture-pack":
            stats = texture_pack(args.index, args.images, args.images_index, args.output, args.layout, args.id,
                                 args.priority, args.filter, args.fingerprints, args.test, args.mips)
            written, total = stats.pop("written"), stats.pop("bytes")
            skipped = ", ".join(f"{k}={v}" for k, v in sorted(stats.items())) or "none"
            print(f"Wrote {written} texture(s), {total} bytes ({total / 1048576:.1f} MiB) to {args.output} ({args.layout})")
            print(f"Skipped: {skipped}")
        elif args.command == "inspect":
            with args.file.open("rb") as file:
                data = file.read(MAX_FILE + 1)
            print(json.dumps(inspect(data), ensure_ascii=False, indent=2))
        return 0
    except (OSError, ValueError, TypeError, KeyError, csv.Error, sqlite3.Error, zipfile.BadZipFile) as exc:
        print(f"lo_mod: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
