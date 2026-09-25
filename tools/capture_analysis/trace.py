#!/usr/bin/env python3
"""Read F1 draw traces and cumulative register state without extracting captures."""

import argparse
from contextlib import contextmanager
import json
from pathlib import Path
import re
import zipfile


FRAME = re.compile(r"frame-\d+-f(\d+)")
REGISTER = re.compile(r"([0-9a-fA-F]{4}) ([0-9a-fA-F]{8})\Z")
DRAW = re.compile(r"draw (\d+)(?: (.*))?\Z")


def safe_name(name):
    """Normalize ZIP separators, then reject absolute and escaping paths."""
    normalized = name.replace("\\", "/").rstrip("/")
    if (not normalized or name.startswith(("/", "\\")) or ":" in normalized or
            any(part in ("", ".", "..") for part in normalized.split("/"))):
        raise ValueError(f"unsafe capture path: {name!r}")
    return normalized


def fields(text):
    """Preserve unknown tokens instead of inventing values for missing fields."""
    values = {}
    for token in text.split():
        key, separator, value = token.partition("=")
        if separator:
            values[key] = value
    return values


class Capture:
    def __init__(self, names, reader):
        self.names = frozenset(names)
        self._reader = reader

    def read_bytes(self, name):
        return self._reader(safe_name(name))

    def read_text(self, name):
        return self.read_bytes(name).decode("utf-8")

    def frame_names(self):
        frames = {name.split("/", 1)[0] for name in self.names
                  if "/" in name and FRAME.fullmatch(name.split("/", 1)[0])}
        return sorted(frames, key=lambda name: (int(FRAME.fullmatch(name).group(1)), name))


@contextmanager
def load_capture(source):
    """Yield a read-only Capture for a ZIP or extracted F1 directory."""
    source = Path(source)
    if source.is_dir():
        root = source / "capture" if (source / "capture" / "capture-info.txt").is_file() else source
        root = root.resolve()
        if not (root / "capture-info.txt").is_file():
            raise ValueError("capture-info.txt missing from capture directory")
        paths = {}
        for path in root.rglob("*"):
            if path.is_file():
                name = safe_name(path.relative_to(root).as_posix())
                resolved = path.resolve()
                if not resolved.is_relative_to(root):
                    raise ValueError(f"capture path escapes directory: {name}")
                paths[name] = resolved
        yield Capture(paths, lambda name: paths[name].read_bytes())
    elif source.is_file() and zipfile.is_zipfile(source):
        with zipfile.ZipFile(source) as archive:
            files = {}
            for info in archive.infolist():
                name = safe_name(info.filename)
                if not info.is_dir() and not info.filename.endswith("\\"):
                    if name in files:
                        raise ValueError(f"duplicate ZIP member: {name}")
                    files[name] = info
            roots = [name[:-len("capture-info.txt")] for name in files
                     if name == "capture-info.txt" or name.endswith("/capture-info.txt")]
            if len(roots) != 1:
                raise ValueError("ZIP must have exactly one capture-info.txt")
            prefix = roots[0]
            members = {name[len(prefix):]: info for name, info in files.items() if name.startswith(prefix)}
            yield Capture(members, lambda name: archive.read(members[name]))
    else:
        raise ValueError("--input must be an F1 ZIP or extracted capture directory")


def select_frames(capture, requested=None):
    """Select by numeric renderer frame or exact directory name, in capture order."""
    available = capture.frame_names()
    if not available:
        raise ValueError("no frame-*-f* folders in capture")
    if not requested:
        return available
    wanted = set(requested)
    selected = [name for name in available
                if name in wanted or FRAME.fullmatch(name).group(1) in wanted]
    found = {item for item in wanted if any(name == item or FRAME.fullmatch(name).group(1) == item
                                              for name in selected)}
    if found != wanted:
        raise ValueError(f"frames not found: {', '.join(sorted(wanted - found))}")
    return selected


def parse_frame(capture, frame_name):
    """Parse draw deltas; use iter_draw_states for each draw's cumulative state."""
    if not FRAME.fullmatch(frame_name):
        raise ValueError(f"invalid frame directory: {frame_name}")
    name = f"{frame_name}/render-state.txt"
    if name not in capture.names:
        raise ValueError(f"missing {name}")
    result = {"directory": frame_name, "frame": int(FRAME.fullmatch(frame_name).group(1)),
              "header": [], "draws": [], "events": [], "footer": [], "end": None}
    current = None
    for line in capture.read_text(name).splitlines():
        match = DRAW.fullmatch(line)
        if match:
            current = {"ordinal": len(result["draws"]), "id": int(match.group(1)),
                       "raw_header": line, "header": fields(match.group(2) or ""),
                       "raw_shader": None, "shader": None,
                       "register_changes": {}, "events": []}
            result["draws"].append(current)
        elif match := REGISTER.fullmatch(line):
            if current is None:
                result["header"].append(line)
            else:
                current["register_changes"][int(match.group(1), 16)] = int(match.group(2), 16)
        elif line.startswith("shaders ") and current is not None:
            current["raw_shader"] = line
            current["shader"] = fields(line[len("shaders "):])
        elif line.startswith(("end ", "drops ")):
            result["footer"].append(line)
            if line.startswith("end "):
                result["end"] = fields(line[len("end "):])
        elif current is None:
            result["header"].append(line)
        else:
            current["events"].append(line)
            result["events"].append({"after_draw": current["id"], "line": line})
    return result


def iter_draw_states(frame):
    """Yield (draw, cumulative_state); state is reused and mutates on next iteration."""
    state = {}
    for draw in frame["draws"]:
        state.update(draw["register_changes"])
        yield draw, state


def check_new_output(source, output):
    source, output = Path(source).resolve(), Path(output).resolve()
    if output == source or (source.is_dir() and output.is_relative_to(source)):
        raise ValueError("--output must be outside the input capture")
    if output.exists():
        raise ValueError("--output must be a new file")
    return output


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path, help="F1 ZIP or extracted capture root")
    parser.add_argument("--output", required=True, type=Path, help="new JSON trace file")
    parser.add_argument("--frames", help="comma-separated renderer frame numbers or frame directories")
    args = parser.parse_args(argv)
    try:
        output = check_new_output(args.input, args.output)
        with load_capture(args.input) as capture:
            names = select_frames(capture, args.frames.split(",") if args.frames else None)
            result = {"capture_info": capture.read_text("capture-info.txt").splitlines(),
                      "frames": [parse_frame(capture, name) for name in names]}
        output.parent.mkdir(parents=True, exist_ok=True)
        with output.open("x", encoding="utf-8") as file:
            json.dump(result, file, indent=2)
            file.write("\n")
    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
