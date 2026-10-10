#!/usr/bin/env python3
"""Build image mod ZIPs and LOTEX2 runtime texture packs as mod folders (mod format v2) or top-level
overlay files, without modifying imported game files (Python 3.10+)."""
from __future__ import annotations

import argparse
import csv
import json
import os
import re
import shutil
import sqlite3
import struct
import subprocess
import sys
import tempfile
import zipfile
from concurrent.futures import ThreadPoolExecutor
from contextlib import closing
from pathlib import Path, PurePosixPath

MAGIC = b"LOTEX1\r\n"
HEADER = struct.Struct("<8sIIII")
MAX_PIXELS = 16 * 1024 * 1024
MAX_FILE = HEADER.size + 4096 + 4 * MAX_PIXELS
MAGIC2 = b"LOTEX2\r\n"
HEADER2 = struct.Struct("<8sIIQIIIIIIQII")  # 64 bytes
XENOS_FORMATS = {"G8": 2, "A8R8G8B8": 6, "DXT1": 18, "DXT3": 19, "DXT5": 20}
DEFAULT_TEXCONV = Path("C:/Users/freefrank/worktrees/LostOdysseyRecomp/_keep/tools/texconv-may2026/texconv.exe")
# DDS payloads: DXGI format -> (texconv name, bytes per 4x4 block).
BC_FORMATS = {71: ("BC1_UNORM", 8), 77: ("BC3_UNORM", 16), 80: ("BC4_UNORM", 8), 98: ("BC7_UNORM", 16)}
BC_FOURCC = {b"DXT1": 71, b"DXT5": 77, b"ATI1": 80, b"BC4U": 80}
FOLDERS = {"image": "images", "font": "fonts", "model": "models", "movie": "movies"}
# Folders under overlay/, top-level or in a mod folder.
KIND_FOLDERS = ("images", "fonts", "models", "movies", "textures", "text")
# api_version=2 display metadata: UTF-8 bytes per field, as the runtime checks them.
META_LIMITS = {"name": 128, "version": 64, "author": 128, "description": 1024}


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


def parse_dds(data: bytes) -> dict:
    """Validate a BC1/BC3/BC4/BC7 UNORM 2D single-slice DDS file and return its shape."""
    if len(data) < 128 or data[:4] != b"DDS " or struct.unpack_from("<I", data, 4)[0] != 124:
        raise ValueError("payload is not a DDS file")
    height, width = struct.unpack_from("<II", data, 12)
    depth, mips = struct.unpack_from("<II", data, 24)
    mips = mips or 1
    fourcc = data[84:88]
    caps2 = struct.unpack_from("<I", data, 112)[0]
    offset = 128
    if fourcc == b"DX10":
        if len(data) < 148:
            raise ValueError("truncated DDS DX10 header")
        dxgi, dimension, misc, array = struct.unpack_from("<IIII", data, 128)
        if dimension != 3 or misc & 4 or array != 1:
            raise ValueError("DDS must be a 2D texture with one slice and no cube map")
        offset = 148
    elif fourcc in BC_FOURCC:
        dxgi = BC_FOURCC[fourcc]
    else:
        raise ValueError("unsupported DDS pixel format")
    if dxgi not in BC_FORMATS:
        raise ValueError(f"unsupported DDS format {dxgi}")
    if caps2 & 0xfe00 or depth > 1:
        raise ValueError("DDS must not be a cube map or volume")
    if width % 4 or height % 4 or not width or not height:
        raise ValueError("DDS level 0 must be a multiple of 4")
    block = BC_FORMATS[dxgi][1]
    expected = sum(((max(1, width >> i) + 3) // 4) * ((max(1, height >> i) + 3) // 4) * block for i in range(mips))
    if len(data) != offset + expected:
        raise ValueError("DDS size does not match its header")
    return {"dxgi": dxgi, "format": BC_FORMATS[dxgi][0], "width": width, "height": height, "mips": mips}


def encode2(fingerprint: int, xenos_format: int, original: tuple[int, int], size: tuple[int, int],
            mips: list[bytes], key: str = "", dds: bytes | None = None) -> bytes:
    """LOTEX2 type 1 (RGBA8) file; `mips` are the top-first level byte strings.
    With `dds` (a complete BC DDS file) it writes type 2 instead and `mips` is ignored."""
    if xenos_format not in XENOS_FORMATS.values():
        raise ValueError("unsupported Xenos format")
    scale = size[0] // original[0] if original[0] else 0
    if (scale not in (1, 2, 4, 8) or size != (original[0] * scale, original[1] * scale)
            or max(size) > 8192):
        raise ValueError(f"invalid scale {size} for original {original}")
    key_bytes = key.encode("utf-8")
    if dds is not None:
        info = parse_dds(dds)
        if (info["width"], info["height"]) != size or not 1 <= info["mips"] <= max(size).bit_length():
            raise ValueError("DDS size or mip count does not match the payload dimensions")
        payload_type, count, payload = 2, info["mips"], dds
    else:
        if not 1 <= len(mips) <= max(size).bit_length() or sum(map(len, mips)) != mip_bytes(*size, len(mips)):
            raise ValueError("mip chain does not match the payload dimensions")
        payload_type, count, payload = 1, len(mips), b"".join(mips)
    return (HEADER2.pack(MAGIC2, HEADER2.size + len(key_bytes), payload_type, fingerprint, xenos_format,
                         *original, *size, count, len(payload), len(key_bytes), 0)
            + key_bytes + payload)


def inspect2(data: bytes) -> dict:
    if len(data) < HEADER2.size:
        raise ValueError("truncated LOTEX2 header")
    (magic, header_size, payload_type, fingerprint, xenos_format, ow, oh, pw, ph, mips, payload_size,
     key_size, reserved) = HEADER2.unpack_from(data)
    if magic != MAGIC2 or reserved != 0 or header_size != HEADER2.size + key_size:
        raise ValueError("invalid LOTEX2 magic, header size or reserved field")
    if payload_type not in (1, 2):
        raise ValueError(f"unsupported LOTEX2 payload type {payload_type}")
    names = {v: k for k, v in XENOS_FORMATS.items()}
    if xenos_format not in names:
        raise ValueError(f"unknown Xenos format {xenos_format}")
    if not (ow and oh and pw % ow == 0 and pw // ow in (1, 2, 4, 8) and ph == oh * (pw // ow)
            and max(pw, ph) <= 8192):
        raise ValueError("invalid payload scale")
    if not 1 <= mips <= max(pw, ph).bit_length():
        raise ValueError("invalid mip count")
    if (payload_size != mip_bytes(pw, ph, mips) if payload_type == 1 else pw % 4 or ph % 4) \
            or len(data) != header_size + payload_size:
        raise ValueError("payload size mismatch or level 0 not a multiple of 4 (truncated data or trailing bytes)")
    extra = {}
    if payload_type == 2:
        dds = parse_dds(data[header_size:])
        if (dds["width"], dds["height"], dds["mips"]) != (pw, ph, mips):
            raise ValueError("DDS size or mip count does not match the LOTEX2 header")
        extra = {"payload_type": "DDS", "dds_format": dds["format"]}
    return {"format": "LOTEX2", "fingerprint": f"{fingerprint:016x}", "original_format": names[xenos_format],
            "original": f"{ow}x{oh}", "payload": f"{pw}x{ph}", "scale": pw // ow, "mips": mips,
            "payload_bytes": payload_size, "key": data[HEADER2.size:header_size].decode("utf-8"), **extra}


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


def display(field: str, value: object) -> str:
    """One api_version=2 metadata value: a single line of UTF-8 without control characters."""
    if not isinstance(value, str) or not value.strip(" \t"):
        raise ValueError(f"{field} must be nonempty text")
    value = value.strip(" \t")
    try:
        size = len(value.encode("utf-8"))
    except UnicodeEncodeError as exc:
        raise ValueError(f"{field} is not valid UTF-8 text") from exc
    if size > META_LIMITS[field] or any(ord(c) < 32 or 127 <= ord(c) <= 159 for c in value):
        raise ValueError(f"{field} must be one line of at most {META_LIMITS[field]} UTF-8 bytes without control characters")
    return value


def manifest(identity: str, priority: int = 0, meta: dict | None = None) -> str:
    """api_version=2 mod.ini. The game finds the files under the mod's overlay/ folder by name."""
    lines = ["api_version=2", f"id={mod_id(identity)}"]
    for field in META_LIMITS:
        if meta and meta.get(field) is not None:
            lines.append(f"{field}={display(field, meta[field])}")
    lines += [f"priority={integer(priority, -(1 << 31), (1 << 31) - 1)}", "enabled=true"]
    return "\n".join(lines) + "\n"


def pack(spec_path: Path, output: Path, layout: str, meta: dict | None = None) -> int:
    if layout not in ("standalone", "overlay"):
        raise ValueError("layout must be standalone or overlay")
    with spec_path.open("rb") as file:
        raw = file.read(1024 * 1024 + 1)
    if len(raw) > 1024 * 1024:
        raise ValueError("mod specification exceeds 1 MiB")
    spec = json.loads(raw.decode("utf-8-sig"))
    if not isinstance(spec, dict) or set(spec) - {"api_version", "id", "priority", "images", *META_LIMITS}:
        raise ValueError("unknown top-level mod specification fields")
    integer(spec.get("api_version"), 1, 1)
    identity = mod_id(spec.get("id"))
    priority = integer(spec.get("priority", 0), -(1 << 31), (1 << 31) - 1)
    # Metadata from mod.json; command-line values replace it.
    fields = {field: spec[field] for field in META_LIMITS if field in spec}
    fields.update({field: value for field, value in (meta or {}).items() if value is not None})
    ini = manifest(identity, priority, fields)
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
                    # Mod folder: the same overlay/ names inside mods/<id>/, found by name.
                    add(("mods/" if layout == "overlay" else f"mods/{identity}/") + name, data)
                if layout == "standalone":
                    add(f"mods/{identity}/mod.ini", ini.encode("utf-8"))
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


def find_texconv(path: Path | None) -> Path:
    found = path if path is not None else (DEFAULT_TEXCONV if DEFAULT_TEXCONV.is_file()
                                           else (Path(shutil.which("texconv")) if shutil.which("texconv") else None))
    if found is None or not Path(found).is_file():
        raise ValueError("--payload dds needs Microsoft texconv (https://github.com/microsoft/DirectXTex/releases); "
                         "pass --texconv <path> or put it on PATH")
    return Path(found)


def convert_dds(texconv: Path, folder: Path, jobs: list[dict], batch: int = 128, workers: int = 4) -> None:
    """Compress folder/fp-<hex>.png to .dds next to it, one texconv call per format batch."""
    groups: dict[str, list[dict]] = {}
    for job in jobs:
        groups.setdefault(job["format"], []).append(job)
    calls = []
    for fmt, items in groups.items():
        for start in range(0, len(items), batch):
            calls.append((fmt, items[start:start + batch], len(calls)))

    def run(call) -> None:
        fmt, items, number = call
        listing = folder / f"list-{number}.txt"
        listing.write_text("".join(f"{folder / ('fp-%016x.png' % item['fingerprint'])}\n" for item in items),
                           encoding="utf-8", newline="\n")
        result = subprocess.run([str(texconv), "-nologo", "-y", "-m", "0", "-f", fmt, "-o", str(folder),
                                 "-flist", str(listing)], capture_output=True, text=True, encoding="utf-8", errors="replace", check=False)
        if result.returncode != 0:
            raise ValueError(f"texconv failed ({result.returncode}): {(result.stdout + result.stderr)[-400:]}")
        for item in items:
            if not (folder / f"fp-{item['fingerprint']:016x}.dds").is_file():
                raise ValueError(f"texconv produced no output for {item['key']}: {result.stdout[-400:]}")

    with ThreadPoolExecutor(max_workers=workers) as pool:
        list(pool.map(run, calls))


def texture_pack(index: Path, images: Path, images_index: Path | None, output: Path, layout: str,
                 identity: str | None, priority: int = 0, name_filter: str | None = None,
                 fingerprints: Path | None = None, test: str | None = None, mips: bool = False,
                 payload: str = "rgba8", texconv: Path | None = None, bc7_all: bool = False,
                 meta: dict | None = None) -> dict:
    if payload not in ("rgba8", "dds"):
        raise ValueError("payload must be rgba8 or dds")
    if payload == "dds":
        texconv = find_texconv(texconv)
    try:
        from PIL import Image
    except ImportError as exc:
        raise ValueError("PNG conversion requires Pillow: python -m pip install Pillow") from exc
    ini_text = None
    if layout == "standalone":
        # Mod folder (format v2): <output>/<id>/mod.ini plus <id>/overlay/textures/.
        identity = mod_id(identity)
        ini = output / identity / "mod.ini"
        folder = output / identity / "overlay" / "textures"
        if ini.exists():
            # Adding to an existing mod folder keeps its mod.ini; v1 mods do not load overlay/ files.
            if not re.search(r"^\s*api_version\s*=\s*2\s*$", ini.read_text(encoding="utf-8-sig"), re.M):
                raise ValueError(f"{ini} is not api_version=2; the game does not load overlay/ files of v1 mods")
        else:
            ini_text = manifest(identity, priority, meta)
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
    jobs: list[dict] = []
    scratch = tempfile.TemporaryDirectory(prefix="lo_mod_") if payload == "dds" else None
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
        if payload == "dds":
            if size[0] % 4 or size[1] % 4:
                skip("not_multiple_of_4"); continue
            name = f"fp-{fingerprint:016x}.lotex2"
            target = folder / name
            if target.exists():
                skip("exists"); continue
            done.add(fingerprint)
            if row["format"] == "A8R8G8B8":  # Host texture is B, G, R, A.
                r, g, b, a = rgba.split()
                rgba = Image.merge("RGBA", (b, g, r, a))
            fmt = "BC4_UNORM" if row["format"] == "G8" else (
                "BC1_UNORM" if row["format"] == "DXT1" and not bc7_all else "BC7_UNORM")
            rgba.save(Path(scratch.name) / f"fp-{fingerprint:016x}.png", compress_level=1)
            jobs.append({"fingerprint": fingerprint, "xenos": xenos, "original": original, "size": size,
                         "key": key, "format": fmt, "name": name, "target": target})
            continue
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
    if payload == "dds":
        try:
            convert_dds(texconv, Path(scratch.name), jobs)
            for job in jobs:
                with open(Path(scratch.name) / f"fp-{job['fingerprint']:016x}.dds", "rb") as file:
                    dds = file.read()
                info = parse_dds(dds)
                if info["format"] != job["format"]:
                    raise ValueError(f"texconv wrote {info['format']}, expected {job['format']}")
                data = encode2(job["fingerprint"], job["xenos"], job["original"], job["size"], [], job["key"], dds)
                job["target"].parent.mkdir(parents=True, exist_ok=True)
                try:
                    with job["target"].open("xb") as file:
                        file.write(data)
                except FileExistsError:
                    skip("exists"); continue
                stats["written"] += 1
                stats["bytes"] += len(data)
        finally:
            scratch.cleanup()
    if ini_text is not None and stats["written"]:
        with ini.open("x", encoding="utf-8", newline="\n") as file:
            file.write(ini_text)
    return stats


def language_clean(pack: Path, original: Path | None, output: Path | None) -> dict[str, int]:
    """Copy a language pack keeping only translated entries: values that differ
    from the untranslated export (original/text beside the pack by default).
    Untranslated entries would otherwise ship the game's own text and, from an
    export without the DLC, put the discs' versions over the DLC's."""
    pack = pack.resolve()
    ini = pack / "language.ini"
    if not ini.is_file():
        raise ValueError(f"{pack} has no language.ini")
    reference = (original or pack.parent / "original").resolve()
    if (reference / "text").is_dir():
        reference = reference / "text"
    if not reference.is_dir():
        raise ValueError(f"no untranslated export at {reference}; pass --original <export>/original")
    target = (output or pack.parent / "share" / pack.name).resolve()
    if target.exists() and any(target.iterdir()):
        raise FileExistsError(f"{target} is not empty")
    stats = {"files": 0, "entries": 0, "dropped_files": 0, "dropped_entries": 0, "unmatched_files": 0}
    for source in sorted((pack / "text").rglob("*.json")):
        relative_path = source.relative_to(pack / "text")
        data = json.loads(source.read_text(encoding="utf-8"))
        base_file = reference / relative_path
        if base_file.is_file():
            base = json.loads(base_file.read_text(encoding="utf-8"))
            kept = {key: value for key, value in data.items() if base.get(key) != value}
        else:
            kept = data  # Nothing to compare with: keep it as it is.
            stats["unmatched_files"] += 1
        stats["dropped_entries"] += len(data) - len(kept)
        if not kept:
            stats["dropped_files"] += 1
            continue
        destination = target / "text" / relative_path
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(json.dumps(kept, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
        stats["files"] += 1
        stats["entries"] += len(kept)
    target.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(ini, target / "language.ini")
    settings = dict(line.split("=", 1) for line in ini.read_text(encoding="utf-8-sig").splitlines()
                    if "=" in line and not line.lstrip().startswith(("#", ";")))
    if settings.get("name", "").strip() == settings.get("id", "").strip():
        print("warning: language.ini still has name= set to the id; Settings will show it as the language name")
    stats["output"] = str(target)
    return stats


def overlay_to_mod(mods: Path, identity: str, priority: int = 0, meta: dict | None = None,
                   dry_run: bool = False) -> tuple[list[tuple[Path, Path]], str]:
    """Move the kind folders of <mods>/overlay into <mods>/<id>/overlay and write an api_version=2 mod.ini.
    Folders are renamed, never copied, so a large pack moves at once; on failure, moved folders go back."""
    identity = mod_id(identity)
    if identity.lower() == "overlay":
        raise ValueError("the mod id 'overlay' is reserved")
    source, target = mods / "overlay", mods / identity
    if not source.is_dir():
        raise ValueError(f"{source} is not a folder")
    if target.exists():
        raise FileExistsError(f"{target} already exists")
    ini_text = manifest(identity, priority, meta)
    moves = [(child, target / "overlay" / child.name) for child in sorted(source.iterdir())
             if child.name.lower() in KIND_FOLDERS and child.is_dir()]
    if not moves:
        raise ValueError(f"{source} has none of the folders {', '.join(KIND_FOLDERS)}")
    if dry_run:
        return moves, ini_text
    (target / "overlay").mkdir(parents=True)
    done: list[tuple[Path, Path]] = []
    try:
        for old, new in moves:
            os.rename(old, new)  # Fails across volumes instead of copying.
            done.append((old, new))
        with (target / "mod.ini").open("x", encoding="utf-8", newline="\n") as file:
            file.write(ini_text)
    except BaseException:
        for old, new in reversed(done):
            os.rename(new, old)
        (target / "mod.ini").unlink(missing_ok=True)
        for folder in (target / "overlay", target):
            try:
                folder.rmdir()
            except OSError:
                pass
        raise
    try:
        source.rmdir()  # Only when nothing else was left in it.
    except OSError:
        pass
    return moves, ini_text


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
    def add_metadata(command_parser) -> None:
        for field in META_LIMITS:
            command_parser.add_argument(f"--{field}", help=f"mod folder {field} for mod.ini (at most {META_LIMITS[field]} UTF-8 bytes)")
    pack_parser = sub.add_parser("pack", help="compile PNG artwork and create an installable ZIP")
    pack_parser.add_argument("spec", type=Path)
    pack_parser.add_argument("--output", type=Path, required=True)
    pack_parser.add_argument("--layout", choices=("standalone", "overlay"), default="standalone",
                             help="standalone: mods/<id>/ mod folder (default); overlay: top-level mods/overlay/ files")
    add_metadata(pack_parser)
    tex_parser = sub.add_parser("texture-pack", help="write LOTEX2 runtime texture replacements (experimental)")
    tex_parser.add_argument("--index", type=Path, required=True, help="export textures/index.csv with a fingerprint column")
    tex_parser.add_argument("--images", type=Path, required=True, help="PNG root mirroring the export layout")
    tex_parser.add_argument("--images-index", type=Path, help="index.csv with key,file columns (default: <images>/index.csv)")
    tex_parser.add_argument("--output", type=Path, required=True, help="mods root; existing files are never overwritten")
    tex_parser.add_argument("--layout", choices=("overlay", "standalone"), default="overlay",
                            help="overlay: <output>/overlay/textures (default); standalone: mod folder <output>/<id>/")
    tex_parser.add_argument("--id", help="standalone mod id")
    tex_parser.add_argument("--priority", type=int, default=0)
    add_metadata(tex_parser)
    tex_parser.add_argument("--filter", help="case-insensitive substring of the texture key")
    tex_parser.add_argument("--fingerprints", type=Path, help="runtime fingerprint log csv; keep only fingerprints seen in game")
    tex_parser.add_argument("--test", choices=("tint", "nearest4"), help="transform the original PNGs for runtime verification")
    tex_parser.add_argument("--mips", action="store_true", help="rgba8 only: write the full mip chain (default: 1 level; dds always has the full chain)")
    tex_parser.add_argument("--payload", choices=("rgba8", "dds"), default="rgba8",
                            help="rgba8 (default, large) or dds: BC1/BC4/BC7 compressed via Microsoft texconv")
    tex_parser.add_argument("--texconv", type=Path, help="path to texconv.exe (default: the known local copy, else PATH)")
    tex_parser.add_argument("--bc7-all", action="store_true", help="dds: use BC7 for DXT1 originals too (default BC1)")
    convert_parser = sub.add_parser("overlay-to-mod", help="move a mods folder's top-level overlay/ into a mod folder "
                                    "<mods>/<id>/ with an api_version=2 mod.ini (renames folders, no copies)")
    convert_parser.add_argument("mods", type=Path, help="the folder that holds overlay/")
    convert_parser.add_argument("--id", required=True)
    convert_parser.add_argument("--priority", type=int, default=0)
    add_metadata(convert_parser)
    convert_parser.add_argument("--dry-run", action="store_true", help="print the moves and mod.ini without changing anything")
    inspect_parser = sub.add_parser("inspect", help="validate a LOTEX1/LOTEX2 file and print its identity")
    inspect_parser.add_argument("file", type=Path)
    clean_parser = sub.add_parser("language-clean",
                                  help="copy a language pack with only the entries you translated, ready to share")
    clean_parser.add_argument("pack", type=Path, help="the pack folder (holds language.ini and text/)")
    clean_parser.add_argument("--original", type=Path,
                              help="untranslated export (default: original/ beside the pack, from --export-language-pack)")
    clean_parser.add_argument("--output", type=Path, help="new folder (default: share/<pack folder name> beside the pack)")
    args = parser.parse_args(argv)
    meta = {field: getattr(args, field, None) for field in META_LIMITS}
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
            count = pack(args.spec, args.output, args.layout, meta)
            print(f"Packed {count} image(s): {args.output} ({args.layout})")
        elif args.command == "texture-pack":
            if args.layout == "overlay" and any(meta.values()):
                raise ValueError("mod metadata needs --layout standalone")
            stats = texture_pack(args.index, args.images, args.images_index, args.output, args.layout, args.id,
                                 args.priority, args.filter, args.fingerprints, args.test, args.mips,
                                 args.payload, args.texconv, args.bc7_all, meta)
            written, total = stats.pop("written"), stats.pop("bytes")
            skipped = ", ".join(f"{k}={v}" for k, v in sorted(stats.items())) or "none"
            print(f"Wrote {written} texture(s), {total} bytes ({total / 1048576:.1f} MiB) to {args.output} ({args.layout})")
            print(f"Skipped: {skipped}")
        elif args.command == "overlay-to-mod":
            moves, ini_text = overlay_to_mod(args.mods, args.id, args.priority, meta, args.dry_run)
            for old, new in moves:
                print(f"{'Would move' if args.dry_run else 'Moved'} {old} -> {new}")
            print(f"{'Would write' if args.dry_run else 'Wrote'} {args.mods / args.id / 'mod.ini'}:\n{ini_text}", end="")
        elif args.command == "inspect":
            with args.file.open("rb") as file:
                data = file.read(MAX_FILE + 1)
            print(json.dumps(inspect(data), ensure_ascii=False, indent=2))
        elif args.command == "language-clean":
            stats = language_clean(args.pack, args.original, args.output)
            print(f"Kept {stats['entries']} translated entries in {stats['files']} files; dropped "
                  f"{stats['dropped_entries']} untranslated entries and {stats['dropped_files']} files"
                  + (f"; {stats['unmatched_files']} files had no original to compare and were kept"
                     if stats["unmatched_files"] else "")
                  + f": {stats['output']}")
        return 0
    except (OSError, ValueError, TypeError, KeyError, csv.Error, sqlite3.Error, zipfile.BadZipFile) as exc:
        print(f"lo_mod: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
