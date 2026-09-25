#!/usr/bin/env python3
"""Export explicitly selected, identity-verified private shader programs for review."""
import argparse
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))
from tools.feedback_archive.scripts import archive_feedback, feedback_ledger as ledger

ERRORS = (OSError, ValueError, TypeError, KeyError, ledger.LedgerError)


def selection_from(path):
    selected = ledger.read_json(path)
    ledger.require(isinstance(selected, dict) and bool(selected)
                   and set(selected) <= {"vs", "ps"}, "Selection accepts only vs and ps arrays")
    result = {}
    for stage, values in selected.items():
        ledger.require(isinstance(values, list), "Selection stage must be an array")
        hashes = [ledger.hex_value(value, 16) for value in values]
        ledger.require(len(hashes) == len(set(hashes)), "Duplicate hash in selection stage")
        result[stage] = set(hashes)
    ledger.require(any(result.values()), "Selection contains no programs")
    return result


def prepare(archive, selection):
    """Read only metadata and selected payloads; return a fully verified small batch."""
    root = Path(archive).resolve()
    ledger.require(root.is_dir(), "Archive directory missing")
    matches = {(stage, fnv): [] for stage, hashes in selection.items() for fnv in hashes}
    for stage, hashes in selection.items():
        if not hashes:
            continue
        directory = ledger.contained_file(root, f"feedback/data/shader_sources/{stage}")
        ledger.require(directory.is_dir(), f"Shader source stage missing: {stage}")
        for path in directory.rglob("*.json"):
            ledger.contained_file(root, path.relative_to(root))
            row = ledger.read_json(path)
            ledger.require(isinstance(row, dict) and row.get("stage") == stage, "Shader source stage mismatch")
            fnv = ledger.hex_value(row.get("renderer_hash"), 16)
            sha = ledger.hex_value(row.get("sha256"), 64)
            ledger.require(path.parent.name == stage and path.stem == sha, "Shader source path/identity mismatch")
            ledger.require(row.get("namespace", ledger.NAMESPACE) == ledger.NAMESPACE, "Unsupported source namespace")
            if fnv in hashes:
                matches[(stage, fnv)].append((path, row))
    programs = []
    for (stage, fnv), rows in sorted(matches.items()):
        ledger.require(bool(rows), f"Selected shader source missing: {stage}:{fnv}")
        ledger.require(len(rows) == 1, f"Ambiguous shader source identity: {stage}:{fnv}")
        path, row = rows[0]
        ledger.valid_times(row)
        length = row.get("byte_length")
        ledger.require(type(length) is int and 0 < length <= 65536 and length % 4 == 0,
                       "Invalid shader source byte length")
        relative = f"feedback/data/programs/{stage}/{row['sha256']}.bin"
        payload = ledger.contained_file(root, relative)
        ledger.require(payload.is_file(), f"Selected shader payload missing: {stage}:{fnv}")
        ledger.require(payload.stat().st_size == length, "Shader payload length mismatch")
        with payload.open("rb") as stream:
            data = stream.read(length + 1)
        ledger.require(len(data) == length, "Shader payload length changed")
        ledger.require(ledger.digest(data) == row["sha256"], "Shader payload SHA-256 mismatch")
        ledger.require(archive_feedback.renderer_hash(data) == fnv, "Shader payload renderer byte-FNV mismatch")
        programs.append(({"stage": stage, "renderer_hash": fnv, "sha256": row["sha256"],
            "byte_length": length, "metadata_path": path.relative_to(root).as_posix(),
            "payload_path": relative, "first_seen": row["first_seen"], "last_seen": row["last_seen"],
            "output_file": f"{stage}_{fnv}.bin", "verified": ["length", "sha256", "renderer-byte-fnv1a64"]}, data))
    return programs


def export(archive, selection_path, output):
    root, output = Path(archive).resolve(), Path(output).resolve()
    ledger.require(not output.exists(), "Output must be a new directory")
    ledger.require(not output.is_relative_to(root), "Output must be outside archive")
    selection = selection_from(selection_path)
    programs = prepare(root, selection)
    provenance = {"schema": "lostodyssey.selected-programs.v1", "archive": str(root),
        "selection_path": str(Path(selection_path).resolve()),
        "selection": {stage: sorted(values) for stage, values in selection.items()},
        "programs": [record for record, _ in programs],
        "limits": "Identity verification only; no translation, map change, GPU or visual validation."}
    # No destination mutation precedes full selection and payload validation.
    output.mkdir(parents=True, exist_ok=False)
    for record, data in programs:
        with (output / record["output_file"]).open("xb") as stream:
            stream.write(data)
    with (output / "provenance.json").open("x", encoding="utf-8") as stream:
        json.dump(provenance, stream, indent=2)
        stream.write("\n")
    return provenance


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", required=True, type=Path)
    parser.add_argument("--selection", required=True, type=Path, help='JSON object: {"vs": [16-hex hashes], "ps": [...]}')
    parser.add_argument("--output", required=True, type=Path, help="New directory outside archive")
    args = parser.parse_args(argv)
    try:
        result = export(args.archive, args.selection, args.output)
    except ERRORS as error:
        parser.error(str(error))
    print(json.dumps({"programs": len(result["programs"]), "output": str(args.output.resolve())}))


if __name__ == "__main__":
    main()
