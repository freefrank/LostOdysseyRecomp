#!/usr/bin/env python3
"""Compare cumulative draw traces from selected frames of one F1 capture."""

import argparse
from collections import Counter
import json
from pathlib import Path
import zipfile

from trace import check_new_output, load_capture, parse_frame, select_frames


MISSING = object()


def compare_frames(first, second):
    """Pair draws by ordinal, retaining explicit unknown shader/end metadata."""
    a, b = first["draws"], second["draws"]
    left_state, right_state = {}, {}
    mismatched = set()
    register_counts = Counter()
    register_draws = []
    shader_changes, header_changes, draw_id_changes = [], [], []
    for ordinal, (left, right) in enumerate(zip(a, b)):
        left_state.update(left["register_changes"])
        right_state.update(right["register_changes"])
        for register in left["register_changes"].keys() | right["register_changes"].keys():
            if left_state.get(register, MISSING) == right_state.get(register, MISSING):
                mismatched.discard(register)
            else:
                mismatched.add(register)
        if left["id"] != right["id"]:
            draw_id_changes.append({"ordinal": ordinal, "first": left["id"], "second": right["id"]})
        if left["raw_header"] != right["raw_header"]:
            header_changes.append({"ordinal": ordinal, "first": left["header"], "second": right["header"],
                                   "first_raw": left["raw_header"], "second_raw": right["raw_header"]})
        if left["raw_shader"] != right["raw_shader"]:
            shader_changes.append({"ordinal": ordinal, "first": left["shader"], "second": right["shader"],
                                   "first_raw": left["raw_shader"], "second_raw": right["raw_shader"]})
        if mismatched:
            register_draws.append({"ordinal": ordinal, "first_id": left["id"],
                                   "second_id": right["id"], "register_count": len(mismatched)})
            register_counts.update(mismatched)
    return {
        "first": first["directory"], "second": second["directory"],
        "draw_counts": {"first": len(a), "second": len(b), "paired": min(len(a), len(b))},
        "unpaired_draws": {"first": [draw["id"] for draw in a[len(b):]],
                           "second": [draw["id"] for draw in b[len(a):]]},
        "draw_id_changes": draw_id_changes, "header_changes": header_changes,
        "shader_changes": shader_changes, "register_changed_draws": register_draws,
        "register_change_counts": {f"{register:04x}": count for register, count in sorted(register_counts.items())},
        "metadata": {
            "first": {"end": first["end"], "footer": first["footer"],
                      "missing_shader_draws": [draw["id"] for draw in a if draw["shader"] is None]},
            "second": {"end": second["end"], "footer": second["footer"],
                       "missing_shader_draws": [draw["id"] for draw in b if draw["shader"] is None]},
        },
    }


def compare_capture(source, requested=None):
    with load_capture(source) as capture:
        names = select_frames(capture, requested)
        if len(names) < 2:
            raise ValueError("comparison needs at least two selected frames")
        frames = [parse_frame(capture, name) for name in names]
    return {"frames": [{"directory": frame["directory"], "frame": frame["frame"],
                        "draw_count": len(frame["draws"]), "end": frame["end"]} for frame in frames],
            "comparisons": [compare_frames(a, b) for a, b in zip(frames, frames[1:])]}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path, help="F1 ZIP or extracted capture root")
    parser.add_argument("--output", required=True, type=Path, help="new JSON comparison file")
    parser.add_argument("--frames", help="comma-separated renderer frame numbers or frame directories")
    args = parser.parse_args(argv)
    try:
        output = check_new_output(args.input, args.output)
        result = compare_capture(args.input, args.frames.split(",") if args.frames else None)
        output.parent.mkdir(parents=True, exist_ok=True)
        with output.open("x", encoding="utf-8") as file:
            json.dump(result, file, indent=2)
            file.write("\n")
    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
