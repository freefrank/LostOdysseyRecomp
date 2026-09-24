"""Collect explicitly supplied microcode directories with deduplicated provenance."""

import argparse
from collections import Counter
import json
from pathlib import Path
import re


NAME = re.compile(r"(?:vs|ps)_[0-9a-f]{16}\.bin\Z")


def fnv(data):
    # Same FNV-1a/64 identity as tools/generate_shader_index.py and resource_scan.h.
    value = 0xCBF29CE484222325
    for byte in data:
        value = ((value ^ byte) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return value


def collect(sources):
    code = {}
    origins = {}
    groups = {}
    for label, directory in sources:
        if not directory.is_dir():
            raise ValueError(f"source directory missing: {directory}")
        if label in groups:
            raise ValueError(f"duplicate source label: {label}")
        names = []
        for path in sorted(directory.glob("*.bin")):
            if not path.is_file() or not NAME.fullmatch(path.name):
                raise ValueError(f"invalid microcode filename: {path}")
            data = path.read_bytes()
            if len(data) < 12 or len(data) > 262144 or len(data) % 12:
                raise ValueError(f"invalid microcode size: {path}")
            if path.name[3:19] != f"{fnv(data):016x}":
                raise ValueError(f"microcode hash mismatch: {path}")
            if path.name in code and code[path.name] != data:
                raise ValueError(f"conflicting microcode: {path}")
            code[path.name] = data
            origins.setdefault(path.name, []).append({"label": label, "path": str(path)})
            names.append(path.name)
        groups[label] = names
    return code, {"groups": groups, "sources": origins,
                  "summary": {"unique_sources": len(code),
                              "stages": dict(Counter(name[:2] for name in code))}}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", action="append", required=True, metavar="LABEL=DIR",
                        help="Nonrecursive *.bin input directory; repeat for each source")
    parser.add_argument("--output", required=True, type=Path,
                        help="New directory for source/*.bin and provenance.json")
    args = parser.parse_args(argv)
    sources = []
    for spec in args.source:
        label, separator, directory = spec.partition("=")
        if not separator or not label or not directory:
            parser.error("--source must be LABEL=DIR")
        sources.append((label, Path(directory)))
    try:
        if args.output.exists():
            raise ValueError(f"output already exists: {args.output}")
        code, report = collect(sources)
        if not code:
            raise ValueError("no microcode in supplied directories")
        target = args.output / "source"
        target.mkdir(parents=True)
        for name, data in sorted(code.items()):
            (target / name).write_bytes(data)
        (args.output / "provenance.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(report["summary"]))
        return 0
    except (OSError, ValueError) as error:
        parser.exit(1, f"error: {error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
