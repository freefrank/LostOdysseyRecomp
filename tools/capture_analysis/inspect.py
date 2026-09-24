#!/usr/bin/env python3
"""Summarize an F1 capture ZIP or extracted capture directory without extracting it."""

import argparse
from contextlib import contextmanager
import json
from pathlib import Path
import re
import zipfile


FRAME = re.compile(r"frame-\d+-f\d+")
DRAW = re.compile(r"draw (\d+) ")
RESOLVE = re.compile(
    r"resolve (\S+) draw=(\d+) address=(\S+) width=(\d+) height=(\d+) "
    r"plume_format=(\d+) bpp=(\d+) raw_ok=(\w+)"
)


def safe_name(name):
    """Validate a ZIP-relative or render-state-relative POSIX path."""
    if (not name or "\\" in name or ":" in name or name.startswith("/") or
            any(part in ("", ".", "..") for part in name.rstrip("/").split("/"))):
        raise ValueError(f"unsafe capture path: {name!r}")
    return name.rstrip("/")


@contextmanager
def open_capture(source):
    """Yield (capture-relative index, read_text, byte_size). Read only on demand."""
    if source.is_dir():
        root = (source / "capture" if (source / "capture" / "capture-info.txt").is_file()
                else source).resolve()
        if not (root / "capture-info.txt").is_file():
            raise ValueError("capture-info.txt missing from capture directory")
        names = {safe_name(p.relative_to(root).as_posix()): p for p in root.rglob("*") if p.is_file()}

        def path_for(name):
            path = (root / safe_name(name)).resolve()
            if not path.is_relative_to(root):
                raise ValueError(f"capture path escapes directory: {name}")
            return path

        def read_text(name):
            return path_for(name).read_text(encoding="utf-8")

        def byte_size(name):
            return path_for(name).stat().st_size

        yield names, read_text, byte_size
    elif source.is_file() and zipfile.is_zipfile(source):
        with zipfile.ZipFile(source) as archive:
            files = {}
            for info in archive.infolist():
                name = safe_name(info.filename)
                if not info.is_dir():
                    if name in files:
                        raise ValueError(f"duplicate ZIP member: {name}")
                    files[name] = info
            roots = [name[:-len("capture-info.txt")] for name in files
                     if name.endswith("capture-info.txt") and
                     (name == "capture-info.txt" or name.endswith("/capture-info.txt"))]
            if len(roots) != 1:
                raise ValueError("ZIP must have exactly one capture-info.txt")
            prefix = roots[0]
            names = {name[len(prefix):]: info for name, info in files.items()
                     if name.startswith(prefix) and name != prefix}

            def read_text(name):
                with archive.open(names[safe_name(name)]) as stream:
                    return stream.read().decode("utf-8")

            def byte_size(name):
                return names[safe_name(name)].file_size

            yield names, read_text, byte_size
    else:
        raise ValueError("--input must be an F1 ZIP or extracted capture directory")


def summarize(source):
    with open_capture(source) as (names, read_text, byte_size):
        frames = sorted({name.split("/", 1)[0] for name in names
                         if "/" in name and FRAME.fullmatch(name.split("/", 1)[0])})
        if not frames:
            raise ValueError("no frame-*-f* folders in capture")
        result = {"capture_info": read_text("capture-info.txt").splitlines(), "frames": []}
        for folder in frames:
            state = f"{folder}/render-state.txt"
            if state not in names:
                raise ValueError(f"missing {state}")
            lines = read_text(state).splitlines()
            draw_count = 0
            last_draw = None
            shader_events = 0
            resolves = []
            provenance = {}
            footer = []
            for line in lines:
                if match := DRAW.match(line):
                    draw_count += 1
                    last_draw = int(match.group(1))
                elif line.startswith("shaders "):
                    shader_events += 1
                elif line.startswith("resolve_provenance "):
                    parts = line.split(" ", 2)
                    if len(parts) == 3:
                        provenance[safe_name(parts[1])] = parts[2]
                elif line.startswith(("end ", "drops ")):
                    footer.append(line)
                elif line.startswith("resolve "):
                    match = RESOLVE.match(line)
                    if not match:
                        raise ValueError(f"unrecognized resolve in {state}: {line}")
                    file, draw, address, width, height, fmt, bpp, raw_ok = match.groups()
                    file = safe_name(file)
                    raw_file = f"{folder}/{file}"
                    width, height, bpp = int(width), int(height), int(bpp)
                    resolves.append({"file": file, "draw": int(draw), "address": address,
                                     "width": width, "height": height, "plume_format": int(fmt),
                                     "bpp": bpp, "raw_ok": raw_ok == "true",
                                     "raw_bytes": byte_size(raw_file) if raw_file in names else None,
                                     "expected_bytes": width * height * bpp})
            for entry in resolves:
                if entry["file"] in provenance:
                    entry["provenance"] = provenance[entry["file"]]
            result["frames"].append({"directory": folder,
                                     "header": lines[:7],
                                     "draw_count": draw_count,
                                     "last_draw": last_draw,
                                     "shader_event_count": shader_events,
                                     "footer": footer,
                                     "screenshot": f"{folder}/screenshot.bmp" in names,
                                     "resolves": resolves})
        return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path, help="F1 ZIP or extracted capture root")
    parser.add_argument("--output", required=True, type=Path, help="new JSON summary file")
    args = parser.parse_args(argv)
    source, output = args.input.resolve(), args.output.resolve()
    if output == source or (source.is_dir() and output.is_relative_to(source)):
        parser.error("--output must be outside the input capture")
    if output.exists():
        parser.error("--output must be a new file")
    try:
        result = summarize(source)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
