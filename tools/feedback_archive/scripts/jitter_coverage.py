#!/usr/bin/env python3
"""Stream an offline private feedback archive into a bounded VS/PS evidence report."""
import argparse
from collections import Counter
import json
from pathlib import Path
import sys

# Direct invocation must work from a private archive cwd, without importing code
# from that archive or depending on installed packages.
PACKAGE = Path(__file__).resolve().parent
sys.path.insert(0, str(PACKAGE.parents[2]))
from tools.capture_analysis.jitter_candidates import position_slots
from tools.feedback_archive.scripts import feedback_ledger as ledger

ERRORS = (OSError, ValueError, TypeError, KeyError, AttributeError, ledger.LedgerError)
FIELDS = ("slot", "candidates", "flags", "guards", "rejection", "reason",
          "position.version", "position.kind", "position.slot", "position.issues", "position.outputs")
LIMIT = 8


def read_private(root, path):
    ledger.contained_file(root, path.relative_to(root))
    return ledger.read_json(path)


def value_at(record, field):
    value = record
    for key in field.split("."):
        if not isinstance(value, dict) or key not in value:
            return "<missing>"
        value = value[key]
    return value


def histogram_key(value):
    if isinstance(value, (dict, list)):
        return "<invalid structured value>"
    return json.dumps(value, ensure_ascii=False, sort_keys=True)


def note_error(errors, path, error):
    errors["count"] += 1
    errors["types"][type(error).__name__] += 1
    if len(errors["examples"]) < 32:
        errors["examples"].append({"path": str(path), "type": type(error).__name__, "message": str(error)})


def load_sources(root, errors):
    sources, seen = {}, set()
    source_root = root / "feedback/data/shader_sources"
    ledger.require(source_root.is_dir(), "shader_sources directory missing")
    for path in source_root.rglob("*.json"):
        try:
            row = read_private(root, path)
            ledger.require(isinstance(row, dict) and row.get("stage") in ("vs", "ps"), "Invalid source stage")
            stage = row["stage"]
            sha = ledger.hex_value(row.get("sha256"), 64)
            fnv = ledger.hex_value(row.get("renderer_hash"), 16)
            ledger.require(path.stem == sha and path.parent.name == stage, "Source path/identity mismatch")
            ledger.require(row.get("namespace", ledger.NAMESPACE) == ledger.NAMESPACE, "Unsupported source namespace")
            ledger.valid_times(row)
            size = row.get("byte_length")
            ledger.require(type(size) is int and 0 < size <= 65536 and size % 4 == 0, "Invalid source byte length")
            ledger.require((stage, sha) not in seen, "Duplicate source identity")
            seen.add((stage, sha))
            relative = f"feedback/data/programs/{stage}/{sha}.bin"
            payload = ledger.contained_file(root, relative)
            present = payload.is_file()
            matches = present and payload.stat().st_size == size
            sources.setdefault((stage, fnv), []).append({**row, "metadata_path": path.relative_to(root).as_posix(),
                "payload_path": relative, "payload_present": present, "payload_size_matches": matches})
            if not matches:
                note_error(errors, path.relative_to(root), ValueError("Shader payload missing or size mismatch"))
        except ERRORS as error:
            note_error(errors, path.relative_to(root), error)
    return sources


def new_pair(vs, ps, mapping):
    return {"vs": vs, "ps": ps, "current_mapping_slot": mapping.get(vs),
        "current_mapping": "mapped" if vs in mapping else "unmapped",
        "observation_count": 0, "record_count": 0, "static_position_candidate_record_count": 0, "groups": Counter(),
        "evidence_histograms": {name: Counter() for name in FIELDS},
        "captured_mapping_relation": Counter(), "max_draws_observed": 0,
        "max_child_draws_observed": 0, "first_seen": None, "last_seen": None,
        "examples": [], "gap_counts": Counter()}


def add_record(pair, record, row, path, group, index, window):
    pair["record_count"] += 1
    pair["groups"][group] += 1
    semantic = dict(record)
    if window is not None:
        semantic["schema"] = 2 if group == "schema4.pairs" else 3
    pair["static_position_candidate_record_count"] += bool(ledger.strong(semantic))
    pair["max_draws_observed"] = max(pair["max_draws_observed"], row["max_draws"])
    child_draws = record.get("draws")
    if type(child_draws) is int:
        pair["max_child_draws_observed"] = max(pair["max_child_draws_observed"], child_draws)
    pair["first_seen"] = min(pair["first_seen"], row["first_seen"]) if pair["first_seen"] is not None else row["first_seen"]
    pair["last_seen"] = max(pair["last_seen"], row["last_seen"]) if pair["last_seen"] is not None else row["last_seen"]
    for name, histogram in pair["evidence_histograms"].items():
        value = value_at(record, name)
        histogram[histogram_key(value)] += 1
        if value == "<missing>" or isinstance(value, (dict, list)):
            pair["gap_counts"][name + "_missing_or_unusable"] += 1
    slot = record.get("slot")
    current = pair["current_mapping_slot"]
    relation = ("captured_unmapped_current_mapped" if slot == -1 and current is not None else
                "captured_slot_matches_current" if current is not None and slot == current else
                "captured_slot_differs_current" if current is not None and type(slot) is int else
                "current_unmapped" if current is None else "captured_slot_unknown")
    pair["captured_mapping_relation"][relation] += 1
    # At most eight representative observations per pair, never whole windows.
    sample = {"group": group, "entry_index": index, "diagnostic": record}
    example = next((example for example in pair["examples"] if example["observation_id"] == row["id"]), None)
    if example is not None:
        example["records"].append(sample)
    elif len(pair["examples"]) < LIMIT:
        example = {"observation_id": row["id"], "path": path, "records": [sample]}
        if window is not None:
            example["window"] = {key: window.get(key) for key in
                ("schema", "build", "backend", "gpu", "driver", "frameSpan", "complete", "capabilities")}
        pair["examples"].append(example)


def analyze(archive, mapping_path):
    root = Path(archive).resolve()
    mapping = position_slots(Path(mapping_path))
    errors = {"count": 0, "types": Counter(), "examples": []}
    sources = load_sources(root, errors)
    observations = root / "feedback/data/observations"
    ledger.require(observations.is_dir(), "observations directory missing")
    pairs, seen = {}, set()
    schemas, unknown_schemas = Counter(), Counter()
    files = accepted = duplicate_ids = 0
    for path in observations.rglob("*.json"):
        files += 1
        relative = path.relative_to(root).as_posix()
        try:
            row = read_private(root, path)
            ledger.require(isinstance(row, dict), "Invalid observation row")
            ident = ledger.hex_value(row.get("id"), 64)
            ledger.require(path.stem == ident, "Observation path/identity mismatch")
            raw = row.get("diagnostic")
            ledger.require(isinstance(raw, str) and ledger.digest(raw.encode()) == ident, "Diagnostic content ID mismatch")
            if ident in seen:
                duplicate_ids += 1
                raise ledger.LedgerError("Duplicate observation ID")
            seen.add(ident)
            diagnostic = ledger.parse_json(raw)
            ledger.require(isinstance(diagnostic, dict), "Invalid diagnostic object")
            schema = diagnostic.get("schema")
            if type(schema) is not int or schema not in (1, 2, 3, 4):
                unknown_schemas[histogram_key(schema)] += 1
                raise ledger.LedgerError("Unsupported diagnostic schema")
            ledger.require(diagnostic.get("namespace") == ledger.NAMESPACE, "Unsupported diagnostic namespace")
            ledger.valid_times(row)
            ledger.require(type(row.get("max_draws")) is int and row["max_draws"] > 0, "Invalid max_draws")
            if schema == 4:
                ledger.validate_compact(diagnostic)
                children = [("schema4." + group, index, entry["record"])
                            for group in ("pairs", "bindings") for index, entry in enumerate(diagnostic[group])]
            else:
                ledger.hex_value(diagnostic.get("vs"), 16)
                ledger.hex_value(diagnostic.get("ps"), 16)
                children = [(f"schema{schema}", None, diagnostic)]
            accepted += 1
            schemas[str(schema)] += 1
            touched = set()
            for group, index, record in children:
                key = (record["vs"], record["ps"])
                pair = pairs.get(key)
                if pair is None:
                    pair = pairs[key] = new_pair(*key, mapping)
                add_record(pair, record, row, relative, group, index, diagnostic if schema == 4 else None)
                touched.add(key)
            for key in touched:
                pairs[key]["observation_count"] += 1
        except ERRORS as error:
            note_error(errors, relative, error)
    for pair in pairs.values():
        pair["sources"] = {stage: sources.get((stage, pair[stage]), []) for stage in ("vs", "ps")}
        for stage, identities in pair["sources"].items():
            if not identities and not (stage == "ps" and pair["ps"] == ledger.ZERO_PS):
                pair["gap_counts"][stage + "_source_missing"] += 1
            if len(identities) > 1:
                pair["gap_counts"][stage + "_renderer_hash_multiple_payloads"] += 1
            if any(not item["payload_size_matches"] for item in identities):
                pair["gap_counts"][stage + "_payload_unavailable"] += 1
    entries = sorted(pairs.values(), key=lambda p: (p["current_mapping"] == "mapped",
        -p["static_position_candidate_record_count"], -p["observation_count"], p["vs"], p["ps"]))
    vs_summary = {}
    for pair in entries:
        item = vs_summary.setdefault(pair["vs"], {"vs": pair["vs"], "current_mapping": pair["current_mapping"],
            "current_mapping_slot": pair["current_mapping_slot"], "ps": [], "record_count": 0})
        item["ps"].append(pair["ps"])
        item["record_count"] += pair["record_count"]
    checkpoint = None
    if (root / "feedback/checkpoint.json").is_file():
        try:
            checkpoint = read_private(root, root / "feedback/checkpoint.json")
        except ERRORS as error:
            note_error(errors, "feedback/checkpoint.json", error)
    return {"schema": "lostodyssey.player-jitter-coverage.v1", "archive": str(root),
        "mapping_header": str(Path(mapping_path).resolve()), "checkpoint": checkpoint,
        "summary": {"observation_files": files, "accepted_observations": accepted,
            "duplicate_observation_ids": duplicate_ids, "schemas": schemas, "unknown_schemas": unknown_schemas,
            "errors": errors["count"], "unique_vs": len(vs_summary), "unique_vs_ps": len(pairs),
            "source_metadata_count": sum(len(items) for items in sources.values()),
            "mapped_vs": sum(p["current_mapping"] == "mapped" for p in vs_summary.values()),
            "unmapped_vs": sum(p["current_mapping"] == "unmapped" for p in vs_summary.values())},
        "errors": errors, "vertices": list(vs_summary.values()), "pairs": entries,
        "limits": ["Records are streamed; only identity keys, histograms and up to eight sample observations per VS/PS are retained.",
            "Observation and child record counts are not frames, players or devices. max_draws is a maximum and is never summed.",
            "Current mapping comes from the explicit source file, not the captured slot; older unmapped records may now be mapped.",
            "Position slots, guards and reason numbers are retained as reported, not confirmed defects or safe mapping instructions.",
            "Static candidate rank reuses the ledger's position/guard filter; source-only shaders do not create observed VS/PS cases.",
            "Shader payload existence/size is checked; binary payloads are not read or rehashed. Metadata identities remain archive declarations.",
            "Schema 4 CPU windows and lexical/static position evidence do not prove GPU completion or visual correctness.",
            "Errors are counted with at most 32 examples; representative observations are the first encountered, not exhaustive."]}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", required=True, type=Path)
    parser.add_argument("--mapping", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args(argv)
    try:
        output = args.output.resolve()
        ledger.require(not output.exists(), "Output must be a new file")
        ledger.require(not output.is_relative_to(args.archive.resolve()), "Output must be outside archive")
        result = analyze(args.archive, args.mapping)
        output.parent.mkdir(parents=True, exist_ok=True)
        with output.open("x", encoding="utf-8") as stream:
            json.dump(result, stream, indent=2, ensure_ascii=False)
            stream.write("\n")
    except ERRORS as error:
        parser.error(str(error))
    print(json.dumps(result["summary"], sort_keys=True))


if __name__ == "__main__":
    main()
