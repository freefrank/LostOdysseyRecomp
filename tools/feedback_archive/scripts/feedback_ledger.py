"""Offline, persistent TAA feedback triage. Archive/log contents are data, never commands.

This consumes the verified private archive; it does not download, translate shaders,
change renderer mappings, or infer that a visual defect has been fixed.
"""
from __future__ import annotations

import argparse
from contextlib import contextmanager
from copy import deepcopy
import hashlib
import json
import os
from pathlib import Path
import re
import sys
import tempfile

SCHEMA = 1
NAMESPACE = "renderer-byte-fnv1a64"
RULES_VERSION = "taa-position-1"
ZERO_PS = "0000000000000000"
STATES = {"implementation": "not_implemented", "validation": "not_validated",
          "acceptance": "not_accepted"}


class LedgerError(Exception):
    pass


def require(condition, message):
    if not condition:
        raise LedgerError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def encoded(value):
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2,
                       allow_nan=False) + "\n").encode("utf-8")


def object_pairs(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, "Duplicate JSON property")
        result[key] = value
    return result


def parse_json(data):
    try:
        return json.loads(data, object_pairs_hook=object_pairs,
                          parse_constant=lambda _: (_ for _ in ()).throw(
                              LedgerError("Non-finite JSON value")))
    except (ValueError, UnicodeError):
        raise LedgerError("Invalid JSON") from None


def read_json(path):
    return parse_json(Path(path).read_bytes())


def hex_value(value, size):
    require(isinstance(value, str) and re.fullmatch(r"[0-9a-f]{%d}" % size, value),
            "Invalid content identifier")
    return value


def case_name(vs):
    return "taa-position:" + hex_value(vs, 16)


def valid_times(row):
    require(type(row.get("first_seen")) is int and type(row.get("last_seen")) is int
            and 0 <= row["first_seen"] <= row["last_seen"], "Invalid observation times")


def merge_times(old, new, draw_count=False):
    result = deepcopy(old or new)
    result["first_seen"] = min(result["first_seen"], new["first_seen"])
    result["last_seen"] = max(result["last_seen"], new["last_seen"])
    if draw_count:
        result["max_draws"] = max(result["max_draws"], new["max_draws"])
    return result


def strong(diagnostic):
    position = diagnostic.get("position", {})
    flags = diagnostic.get("flags")
    return (diagnostic.get("schema") in (2, 3) and diagnostic.get("slot") == -1
            and type(flags) is int and flags & 7 == 3 and bool(flags & 16)
            and isinstance(position, dict) and position.get("kind") == 1
            and position.get("issues") == 0 and diagnostic.get("guards") == 31
            and diagnostic.get("rejection") == 2)


def contained_file(root, relative):
    path = root / relative
    require(path.resolve().is_relative_to(root.resolve()), "Archive path escapes root")
    return path


def validate_compact(diagnostic):
    """Check the bounded structures consumed here; the Worker owns full wire validation."""
    require(len(encoded(diagnostic)) <= 192 * 1024, "Compact diagnostic too large")
    span = diagnostic.get("frameSpan")
    require(type(span) is int and 1 <= span <= 32 and type(diagnostic.get("complete")) is bool,
            "Invalid compact window span/completeness")
    capabilities = diagnostic.get("capabilities")
    require(isinstance(capabilities, dict) and capabilities.get("version") == 1,
            "Unsupported compact capabilities")
    require(diagnostic.get("backend") in ("d3d12", "vulkan"), "Invalid compact backend")
    for name in ("sparseSupported", "sparseWindowLinked", "gpuCompletion", "colorImages"):
        require(type(capabilities.get(name)) is bool, "Invalid compact capability flag")
    require(capabilities["sparseWindowLinked"] is False and capabilities["gpuCompletion"] is False
            and capabilities["colorImages"] is False, "Unsupported compact evidence scope")
    require(isinstance(capabilities.get("bindingPairs"), list)
            and len(capabilities["bindingPairs"]) <= 24
            and all(isinstance(p, str) and re.fullmatch(r"[0-9a-f]{16}:[0-9a-f]{16}", p)
                    for p in capabilities["bindingPairs"]), "Invalid compact binding coverage")
    counters = diagnostic.get("counters")
    require(isinstance(counters, dict), "Invalid compact counters")
    for group in ("source", "summary", "binding", "sparse", "compact"):
        require(isinstance(counters.get(group), dict) and counters[group]
                and all(type(n) is int and 0 <= n <= 0xffffffff for n in counters[group].values()),
                "Invalid compact reason counters")
    delivery, pending = diagnostic.get("delivery"), diagnostic.get("pending")
    require(isinstance(delivery, dict) and delivery.get("scope") == "since-consent-reset",
            "Invalid compact delivery scope")
    for group in ("summary", "source", "binding", "sparse", "compact"):
        counts = delivery.get(group)
        require(isinstance(counts, dict) and set(counts) == {"accepted", "transportFailed", "httpRejected"}
                and all(type(n) is int and 0 <= n <= 0xffffffff for n in counts.values()),
                "Invalid compact delivery counters")
    require(isinstance(pending, dict) and set(pending) == {"summary", "source", "binding", "sparseFrames"}
            and all(type(n) is int and 0 <= n <= 0xffffffff for n in pending.values()),
            "Invalid compact pending counts")
    frames = diagnostic.get("frames")
    require(isinstance(frames, list) and 1 <= len(frames) <= 32, "Invalid compact frame count")
    offsets = set()
    for frame in frames:
        require(isinstance(frame, dict) and type(frame.get("offset")) is int
                and 0 <= frame["offset"] < span and frame["offset"] not in offsets,
                "Invalid compact frame offset")
        offsets.add(frame["offset"])
        for name in ("taa", "ready", "completed", "reused", "historyCaptured", "cameraChecks",
                     "sameEpoch", "resetAfterFrame", "sparseReady"):
            require(type(frame.get(name)) is bool, "Invalid compact frame flag")
        for name in ("sceneRejection", "historyRejection"):
            require(type(frame.get(name)) is int and 0 <= frame[name] <= 0xffffffff,
                    "Invalid compact history rejection")
        require(type(frame.get("previousFrameDelta")) is int and -1 <= frame["previousFrameDelta"] <= 65535,
                "Invalid compact previous frame delta")
    for group, capacity, schema in (("pairs", 24, 2), ("bindings", 8, 3)):
        entries = diagnostic.get(group)
        require(isinstance(entries, list) and len(entries) <= capacity, "Invalid compact child count")
        for entry in entries:
            require(isinstance(entry, dict) and isinstance(entry.get("record"), dict), "Invalid compact child")
            positions = ("first", "last") if group == "pairs" else ("offset",)
            require(all(type(entry.get(name)) is int and 0 <= entry[name] < span for name in positions),
                    "Invalid compact child frame reference")
            if group == "pairs":
                require(entry["first"] <= entry["last"], "Invalid compact pair frame range")
            record = entry["record"]
            hex_value(record.get("vs"), 16)
            hex_value(record.get("ps"), 16)
            require(type(record.get("draws")) is int and record["draws"] > 0,
                    "Invalid compact sampled occurrences")
            require("schema" not in record and "namespace" not in record, "Unexpected compact child envelope")
            if schema == 3:
                require(isinstance(record.get("consumer"), dict) and isinstance(record.get("texture"), dict)
                        and isinstance(record.get("psC0"), list) and len(record["psC0"]) == 4,
                        "Invalid compact binding evidence")


def compact_children(diagnostic):
    for group, schema in (("pairs", 2), ("bindings", 3)):
        for index, entry in enumerate(diagnostic[group]):
            # Window metadata and occurrence counts cannot change a shader review.
            semantic = {key: value for key, value in entry["record"].items() if key != "draws"}
            semantic.update(schema=schema, namespace=NAMESPACE)
            yield group, index, entry, semantic


def shader_semantic_id(diagnostic):
    return digest(encoded({key: value for key, value in diagnostic.items()
                           if key not in ("draws", "build", "backend", "gpu", "driver")}))


def load_archive(root):
    """Validate all consumed metadata before any ledger write. Blobs were verified by archive."""
    root = Path(root)
    data = root / "feedback" / "data"
    require((data / "observations").is_dir() and (data / "shader_sources").is_dir(),
            "Archive observations or shader_sources directory missing")
    observations, sources = {}, {}
    for path in sorted((data / "observations").rglob("*.json")):
        row = read_json(path)
        require(isinstance(row, dict), "Invalid observation row")
        ident = hex_value(row.get("id"), 64)
        require(path.stem == ident, "Observation filename does not match ID")
        raw = row.get("diagnostic")
        require(isinstance(raw, str) and digest(raw.encode("utf-8")) == ident,
                "Diagnostic SHA-256 mismatch")
        diagnostic = parse_json(raw)
        require(isinstance(diagnostic, dict) and type(diagnostic.get("schema")) is int
                and diagnostic["schema"] in (1, 2, 3, 4), "Unsupported diagnostic schema")
        require(diagnostic.get("namespace") == NAMESPACE, "Unsupported diagnostic namespace")
        if diagnostic["schema"] == 4:
            validate_compact(diagnostic)
        else:
            hex_value(diagnostic.get("vs"), 16)
            hex_value(diagnostic.get("ps"), 16)
        valid_times(row)
        require(type(row.get("max_draws")) is int and row["max_draws"] > 0,
                "Invalid max_draws")
        item = {"id": ident, "diagnostic": diagnostic, "first_seen": row["first_seen"],
                "last_seen": row["last_seen"], "max_draws": row["max_draws"],
                "archive_path": path.relative_to(root).as_posix()}
        require(ident not in observations, "Duplicate observation ID in archive")
        observations[ident] = item
    for path in sorted((data / "shader_sources").rglob("*.json")):
        row = read_json(path)
        require(isinstance(row, dict) and row.get("stage") in ("vs", "ps"),
                "Invalid shader source stage")
        sha = hex_value(row.get("sha256"), 64)
        fnv = hex_value(row.get("renderer_hash"), 16)
        require(path.stem == sha and path.parent.name == row["stage"],
                "Shader source path does not match stage/SHA")
        if "namespace" in row:
            require(row["namespace"] == NAMESPACE, "Unsupported source namespace")
        valid_times(row)
        require(type(row.get("byte_length")) is int and 0 < row["byte_length"] <= 65536
                and row["byte_length"] % 4 == 0, "Invalid shader source byte_length")
        relative = f"feedback/data/programs/{row['stage']}/{sha}.bin"
        require(contained_file(root, relative).is_file(), "Referenced shader payload missing")
        key = row["stage"] + ":" + sha
        require(key not in sources, "Duplicate source stage/SHA in archive")
        sources[key] = {"stage": row["stage"], "sha256": sha, "renderer_hash": fnv,
                        "byte_length": row["byte_length"], "first_seen": row["first_seen"],
                        "last_seen": row["last_seen"], "payload_path": relative}
    return observations, sources


def load_document(path, kind):
    if path is None:
        return None
    document = read_json(path)
    require(isinstance(document, dict) and document.get("schema") == SCHEMA
            and isinstance(document.get("cases"), dict), f"Invalid {kind} document")
    for ident, entry in document["cases"].items():
        require(isinstance(entry, dict) and ident.startswith("taa-position:")
                and case_name(ident.split(":", 1)[1]) == ident, f"Invalid {kind} case")
    return document


def new_case(vs, title=None):
    return {"vs": vs, "title": title or f"TAA position candidate {vs}",
            "evidence": {"observations": {}, "sources": {}, "pairs": [],
                         "strong_ids": [], "context": {"mapping_unknown": True}},
            "fingerprint": None, "review": None, "historical_reviews": [],
            "review_status": "needs_review", **STATES, "history": []}


def validate_state(value):
    require((isinstance(value, str) and bool(value.strip())) or isinstance(value, dict),
            "State must be a nonempty string or object")


def validate_review(review):
    require(isinstance(review, dict), "Invalid review document")
    require(isinstance(review.get("decision"), str) and bool(review["decision"].strip()),
            "Review decision required")
    require(isinstance(review.get("summary"), str), "Review summary required")
    for key in ("evidence", "gaps"):
        require(isinstance(review.get(key, []), list)
                and all(isinstance(x, str) for x in review.get(key, [])),
                "Review evidence/gaps must be string arrays")
    require(isinstance(review.get("scope", ""), str), "Review scope must be a string")
    for key in STATES:
        if key in review:
            validate_state(review[key])


def load_ledger(path):
    if not Path(path).exists():
        return {"schema": SCHEMA, "namespace": NAMESPACE,
                "analysis_rules_version": RULES_VERSION, "cases": {}}
    ledger = read_json(path)
    require(isinstance(ledger, dict) and ledger.get("schema") == SCHEMA
            and ledger.get("namespace") == NAMESPACE and isinstance(ledger.get("cases"), dict),
            "Unsupported ledger document")
    compact = ledger.get("compact_windows", {})
    require(isinstance(compact, dict), "Invalid retained compact windows")
    for ident, window in compact.items():
        hex_value(ident, 64)
        require(isinstance(window, dict) and window.get("id") == ident
                and isinstance(window.get("diagnostic"), dict)
                and window["diagnostic"].get("schema") == 4
                and window["diagnostic"].get("namespace") == NAMESPACE,
                "Invalid retained compact window")
        validate_compact(window["diagnostic"])
        valid_times(window)
    for ident, case in ledger["cases"].items():
        require(isinstance(case, dict) and case_name(case.get("vs")) == ident,
                "Invalid ledger case identity")
        evidence = case.get("evidence", {})
        require(isinstance(evidence, dict) and isinstance(evidence.get("observations"), dict)
                and isinstance(evidence.get("sources"), dict), "Invalid ledger evidence")
        require(isinstance(case.get("history"), list)
                and isinstance(case.get("historical_reviews"), list), "Invalid ledger history")
        if case.get("fingerprint") is not None:
            hex_value(case["fingerprint"], 64)
        for state in STATES:
            validate_state(case.get(state))
        for ident, observation in evidence["observations"].items():
            hex_value(ident, 64)
            require(observation.get("id") == ident and isinstance(observation.get("diagnostic"), dict)
                    and observation["diagnostic"].get("vs") == case["vs"], "Invalid retained observation")
            valid_times(observation)
            require(type(observation.get("max_draws")) is int and observation["max_draws"] > 0,
                    "Invalid retained max_draws")
        for window_id, references in evidence.get("compact_refs", {}).items():
            require(window_id in compact and isinstance(references, list), "Missing retained compact window")
            diagnostic = compact[window_id]["diagnostic"]
            for reference in references:
                require(isinstance(reference, dict) and reference.get("group") in ("pairs", "bindings")
                        and type(reference.get("index")) is int, "Invalid compact child reference")
                children = diagnostic[reference["group"]]
                require(0 <= reference["index"] < len(children)
                        and children[reference["index"]]["record"]["vs"] == case["vs"],
                        "Compact child reference does not match case")
        if case.get("review") is not None:
            validate_review(case["review"])
            hex_value(case["review"].get("fingerprint"), 64)
    return ledger


def seed_cases(ledger, seed):
    if seed is None:
        return
    for ident, entry in seed["cases"].items():
        vs = hex_value(entry.get("vs"), 16)
        require(case_name(vs) == ident, "Seed VS does not match case ID")
        require(isinstance(entry.get("title", ""), str), "Invalid seed title")
        existed = ident in ledger["cases"]
        case = ledger["cases"].setdefault(ident, new_case(vs, entry.get("title")))
        for state in STATES:
            if state in entry:
                validate_state(entry[state])
                if not existed:
                    case[state] = deepcopy(entry[state])
        historical = entry.get("historical_review")
        if historical is not None:
            validate_review(historical)
            historical = {k: deepcopy(v) for k, v in historical.items() if k not in STATES}
            source_id = digest(encoded(historical))
            if all(item["import_id"] != source_id for item in case["historical_reviews"]):
                case["historical_reviews"].append({"import_id": source_id,
                                                   "binding": "unbound", **historical})
                case["history"].append({"event": "historical_review_imported",
                                         "import_id": source_id, "binding": "unbound"})


def context_entry(context, ident, previous):
    if context is None:
        return deepcopy(previous)
    result = deepcopy(context["cases"].get(ident, {"mapping_unknown": True}))
    if "mapping_slot" in result:
        slot = result["mapping_slot"]
        require(slot is None or (type(slot) is int and 0 <= slot <= 252), "Invalid mapping_slot")
    pairs = result.get("binding_pairs", [])
    require(isinstance(pairs, list) and all(isinstance(p, str) and
            re.fullmatch(r"[0-9a-f]{16}:[0-9a-f]{16}", p) for p in pairs),
            "Invalid context binding_pairs")
    result["binding_pairs"] = sorted(set(pairs))
    # Global source hashes and capture time belong to provenance, not review dependencies.
    result.pop("provenance", None)
    return result


def dependencies(case, analysis_version):
    evidence = case["evidence"]
    result = {"rules_version": RULES_VERSION, "analysis_version": analysis_version,
            "namespace": NAMESPACE, "vs": case["vs"],
            "diagnostic_ids": sorted(evidence["observations"]),
            "sources": {key: {"sha256": value["sha256"], "available": value["available"]}
                        for key, value in sorted(evidence["sources"].items())},
            "pairs": evidence["pairs"], "context": evidence["context"]}
    # Preserve pre-schema4 fingerprints when no compact shader evidence exists.
    # Content IDs change for counters, frames and metadata; semantic child IDs do not.
    if evidence.get("compact_semantic_ids"):
        result["compact_semantic_ids"] = evidence["compact_semantic_ids"]
    return result


def sync_data(ledger, root, observations, sources, seed, context):
    ledger = deepcopy(ledger)
    seed_cases(ledger, seed)
    for observation in observations.values():
        diagnostic = observation["diagnostic"]
        if diagnostic["schema"] == 4:
            windows = ledger.setdefault("compact_windows", {})
            old = windows.get(observation["id"])
            if old:
                require(old["diagnostic"] == diagnostic, "Retained compact content changed")
            windows[observation["id"]] = merge_times(old, observation, True)
        else:
            if strong(diagnostic):
                vs = diagnostic["vs"]
                ledger["cases"].setdefault(case_name(vs), new_case(vs))
    compact_grouped = {}
    for window_id, window in sorted(ledger.get("compact_windows", {}).items()):
        for group, index, entry, semantic in compact_children(window["diagnostic"]):
            vs = semantic["vs"]
            if strong(semantic):
                ledger["cases"].setdefault(case_name(vs), new_case(vs))
            compact_grouped.setdefault(vs, {}).setdefault(window_id, []).append(
                {"group": group, "index": index})
    if context is not None:
        version = context.get("analysis_version", "1")
        require(isinstance(version, str) and bool(version), "Invalid context analysis_version")
        ledger["analysis_version"] = version
        ledger["context_provenance"] = deepcopy(context.get("provenance", {}))
    analysis_version = ledger.setdefault("analysis_version", "1")
    ledger["analysis_rules_version"] = RULES_VERSION
    source_index = {}
    for source in sources.values():
        source_index.setdefault(source["stage"] + ":" + source["renderer_hash"], {})[source["sha256"]] = source
    grouped = {}
    for ident, observation in observations.items():
        if observation["diagnostic"]["schema"] != 4:
            grouped.setdefault(observation["diagnostic"]["vs"], {})[ident] = observation
    for ident, case in sorted(ledger["cases"].items()):
        evidence = case["evidence"]
        for key, observation in grouped.get(case["vs"], {}).items():
            old = evidence["observations"].get(key)
            if old is not None:
                require(old["diagnostic"] == observation["diagnostic"], "Retained diagnostic content changed")
            evidence["observations"][key] = merge_times(old, observation, True)
        evidence["strong_ids"] = sorted(key for key, observation in evidence["observations"].items()
                                        if strong(observation["diagnostic"]))
        pairs = {o["diagnostic"]["ps"] for o in evidence["observations"].values()}
        window_refs = compact_grouped.get(case["vs"], {})
        if window_refs:
            evidence["compact_refs"] = window_refs
            semantic_ids, compact_strong = set(), set()
            for window_id, references in window_refs.items():
                window = ledger["compact_windows"][window_id]
                children = {(group, index): semantic for group, index, _, semantic
                            in compact_children(window["diagnostic"])}
                for reference in references:
                    semantic = children[(reference["group"], reference["index"])]
                    semantic_id = shader_semantic_id(semantic)
                    semantic_ids.add(semantic_id)
                    if strong(semantic):
                        compact_strong.add(semantic_id)
                    pairs.add(semantic["ps"])
            existing_ids = {shader_semantic_id(o["diagnostic"]) for o in evidence["observations"].values()}
            evidence["compact_semantic_ids"] = sorted(semantic_ids - existing_ids)
            evidence["compact_strong_ids"] = sorted(compact_strong)
        evidence["pairs"] = sorted(pairs)
        wanted = {"vs:" + case["vs"]} | {"ps:" + ps for ps in evidence["pairs"] if ps != ZERO_PS}
        for key in sorted(wanted):
            stage, fnv = key.split(":")
            bucket = evidence["sources"].setdefault(key, {"stage": stage, "renderer_hash": fnv,
                                                          "sha256": [], "records": {}})
            for sha, source in source_index.get(key, {}).items():
                old = bucket["records"].get(sha)
                if old:
                    require(all(old.get(k) == source[k] for k in
                                ("stage", "sha256", "renderer_hash", "byte_length", "payload_path")),
                            "Retained source identity changed")
                bucket["records"][sha] = merge_times(old, source)
            bucket["sha256"] = sorted(bucket["records"])
            bucket["available"] = []
            for sha, record in sorted(bucket["records"].items()):
                hex_value(sha, 64)
                expected = f"feedback/data/programs/{stage}/{sha}.bin"
                require(record.get("payload_path") == expected, "Invalid retained payload path")
                if contained_file(Path(root), expected).is_file():
                    bucket["available"].append(sha)
            bucket["ambiguous"] = len(bucket["sha256"]) > 1
            bucket["missing"] = not bucket["available"]
        evidence["no_ps"] = ZERO_PS in evidence["pairs"]
        evidence["context"] = context_entry(context, ident, evidence["context"])
        refs = list(evidence["observations"].values())
        refs += [ledger["compact_windows"][window_id] for window_id in window_refs]
        evidence["first_seen"] = min((o["first_seen"] for o in refs), default=None)
        evidence["last_seen"] = max((o["last_seen"] for o in refs), default=None)
        # A compact row's max_draws counts window occurrences, not shader draws.
        compact_draws = [ledger["compact_windows"][window_id]["diagnostic"][ref["group"]][ref["index"]]["record"]["draws"]
                         for window_id, references in window_refs.items() for ref in references]
        evidence["max_draws"] = max([o["max_draws"] for o in evidence["observations"].values()] + compact_draws, default=0)
        old_fingerprint = case["fingerprint"]
        fingerprint = digest(encoded(dependencies(case, analysis_version)))
        if fingerprint != old_fingerprint:
            case["history"].append({"event": "evidence_changed", "previous": old_fingerprint,
                                     "fingerprint": fingerprint})
            case["fingerprint"] = fingerprint
        review = case["review"]
        case["review_status"] = "current" if review and review["fingerprint"] == fingerprint else "needs_review"
    return ledger


@contextmanager
def ledger_lock(path):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    lock = Path(str(path) + ".lock")
    try:
        descriptor = os.open(lock, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    except FileExistsError:
        raise LedgerError("Ledger lock exists; another writer or interrupted operation requires attention") from None
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
            stream.write(str(os.getpid()) + "\n")
        yield
    finally:
        lock.unlink()


def atomic_write(path, data):
    path = Path(path)
    if path.exists() and path.read_bytes() == data:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, name = tempfile.mkstemp(prefix=path.name + ".", suffix=".tmp", dir=path.parent)
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(name, path)
    finally:
        if os.path.exists(name):
            os.unlink(name)
    return True


def sync(archive, ledger_path, seed_path=None, context_path=None):
    with ledger_lock(ledger_path):
        ledger = load_ledger(ledger_path)
        observations, sources = load_archive(archive)
        seed = load_document(seed_path, "seed")
        context = load_document(context_path, "context")
        updated = sync_data(ledger, archive, observations, sources, seed, context)
        checkpoint_path = Path(archive) / "feedback" / "checkpoint.json"
        checkpoint = read_json(checkpoint_path) if checkpoint_path.is_file() else None
        if checkpoint is not None:
            require(isinstance(checkpoint, dict) and checkpoint.get("schema") == 1
                    and type(checkpoint.get("through")) is int and checkpoint["through"] >= 0,
                    "Invalid archive checkpoint")
        updated["archive_input"] = {"checkpoint_through": checkpoint["through"] if checkpoint else None,
                                    "observations": len(observations), "shader_sources": len(sources)}
        changed = atomic_write(ledger_path, encoded(updated))
    return updated, changed


def review(ledger_path, ident, review_path):
    with ledger_lock(ledger_path):
        ledger = load_ledger(ledger_path)
        require(ident in ledger["cases"], "Case not found; sync the ledger first")
        case = ledger["cases"][ident]
        require(case["fingerprint"] is not None, "Case has no synchronized evidence fingerprint")
        item = read_json(review_path)
        validate_review(item)
        require(set(item) <= {"decision", "summary", "evidence", "gaps", "scope"} | set(STATES),
                "Unsupported review fields")
        result = {key: deepcopy(value) for key, value in item.items() if key not in STATES}
        result["fingerprint"] = case["fingerprint"]
        state_changed = any(key in item and item[key] != case[key] for key in STATES)
        if case["review"] != result or state_changed:
            case["review"] = result
            for key in STATES:
                if key in item:
                    case[key] = deepcopy(item[key])
            case["review_status"] = "current"
            case["history"].append({"event": "review_recorded", "review": deepcopy(result),
                                     "states": {key: deepcopy(case[key]) for key in STATES}})
        atomic_write(ledger_path, encoded(ledger))
    return ledger


def safe_text(value):
    if not isinstance(value, str):
        value = json.dumps(value, ensure_ascii=False, sort_keys=True)
    # Display imported text literally; suppress HTML, Markdown links, and table injection.
    value = value.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
    for character in "\\`*_{}[]()#!|":
        value = value.replace(character, "\\" + character)
    return " ".join(value.splitlines())


def source_label(bucket):
    if bucket["ambiguous"]:
        return f"ambiguous ({len(bucket['sha256'])} SHA-256 variants; {len(bucket['available'])} available)"
    return "available" if bucket["available"] else "missing in archive"


def compact_report(ledger, case):
    refs = case["evidence"].get("compact_refs", {})
    if not refs:
        return []
    ordered = sorted(refs, key=lambda key: (ledger["compact_windows"][key]["last_seen"], key), reverse=True)
    lines = ["", f"Compact evidence: {len(refs)} retained content IDs; showing the latest {min(3, len(refs))}.",
             "Frame offsets join only this window's pairs, CPU history and bindings. Sparse GPU sequences are unlinked.",
             "Missing in archive is an availability fact. Queue/failure counters cannot assign a cause to an individual missing shader."]
    for window_id in ordered[:3]:
        window = ledger["compact_windows"][window_id]
        diagnostic = window["diagnostic"]
        caps = diagnostic["capabilities"]
        lines += ["", f"- Window `{window_id}`: {safe_text(diagnostic['backend'])}; "
                  f"{diagnostic['frameSpan']} relative frames; complete={diagnostic['complete']}; "
                  f"build {safe_text(diagnostic.get('build', 'unknown'))}; "
                  f"runtime {safe_text(caps.get('runtimeVersion', 'unknown'))} / "
                  f"commit {safe_text(caps.get('runtimeCommit', 'unknown'))}."]
        binding_refs = []
        referenced_pairs = set()
        for ref in refs[window_id]:
            entry = diagnostic[ref["group"]][ref["index"]]
            pair = entry["record"]["vs"] + ":" + entry["record"]["ps"]
            referenced_pairs.add(pair)
            if ref["group"] == "bindings":
                binding_refs.append(f"bindings[{ref['index']}]@{entry['offset']}")
            else:
                lines.append(f"  Pair `{pair}` at offsets {entry['first']}..{entry['last']}; "
                             f"`pairs[{ref['index']}]`; sampled occurrences {entry['record']['draws']}.")
        if binding_refs:
            lines.append("  Binding references: " + ", ".join(binding_refs) + ". CPU state; GPU completion is not established.")
        uncovered = sorted(pair for pair in referenced_pairs
                           if not pair.endswith(":" + ZERO_PS) and pair not in caps["bindingPairs"])
        if uncovered:
            lines.append("  Not collected by this window's binding allowlist: " + ", ".join(uncovered) + ".")
        if not caps["sparseSupported"]:
            lines.append("  Sparse GPU data: not collected on this backend.")
        else:
            lines.append("  Sparse GPU data: backend supported; this CPU window has no link to a sparse sequence.")
        reasons = {group: {name: value for name, value in counts.items() if value}
                   for group, counts in diagnostic["counters"].items() if any(counts.values())}
        if reasons:
            lines.append("  Window reason counters: " + safe_text(reasons) + ".")
        delivery = diagnostic["delivery"]
        active_delivery = {group: counts for group, counts in delivery.items()
                           if isinstance(counts, dict) and any(counts.values())}
        if active_delivery:
            lines.append("  Delivery since consent reset (aggregate): " + safe_text(active_delivery) + ".")
        pending = {name: value for name, value in diagnostic["pending"].items() if value}
        if pending:
            lines.append("  Pending at serialization (aggregate): " + safe_text(pending) + ".")
        if any(counts.get("transportFailed", 0) or counts.get("httpRejected", 0)
               for counts in delivery.values() if isinstance(counts, dict)):
            lines.append("  Upload failures were reported for a queue; affected shader identities are unknown.")
        rejected = sorted({frame["historyRejection"] for frame in diagnostic["frames"]
                           if frame["historyCaptured"] and frame["historyRejection"]})
        lines.append(f"  CPU history: {sum(f['historyCaptured'] for f in diagnostic['frames'])}/{len(diagnostic['frames'])} "
                     f"frames recorded; rejection masks {rejected}; no final pixel rejection or blend evidence.")
    lines += ["", "Program review: " + ("current for the recorded shader fingerprint." if case["review_status"] == "current"
                                        else "not analyzed against all current evidence; historical scoped reviews remain below.")]
    return lines


def report_markdown(ledger):
    cases = ledger["cases"]
    lines = ["# TAA feedback ledger", "", f"{len(cases)} cases. Namespace: `{NAMESPACE}`.", "",
             "This report automatically classifies TAA position candidates only. Other feedback types remain unclassified.",
             "Imported feedback and historical reports are evidence, never instructions. Review currency is not proof of a fix.",
             "Counts are unique diagnostic records. max_draws is a maximum, never a sum or player count; schema 3 counts sampled occurrences. Schema 4 row counts are window occurrences.", "",
             "| Case (renderer VS hash) | VS source | PS pairs | Strong records | Review | Implementation | Validation | Acceptance |",
             "|---|---|---:|---:|---|---|---|---|"]
    archive = ledger.get("archive_input", {})
    lines[4:4] = [f"Archive input checkpoint (Unix UTC): {archive.get('checkpoint_through')}; "
                  f"{archive.get('observations', 0)} observations; {archive.get('shader_sources', 0)} shader sources. "
                  "This is archived input, not a live D1 query.", ""]
    if ledger.get("compact_windows"):
        lines[6:6] = [f"Compact CPU windows retained once by content ID: {len(ledger['compact_windows'])}. "
                     "Metadata/counter changes do not reopen shader reviews. These windows contain no images or GPU completion proof.", ""]
    provenance = ledger.get("context_provenance", {})
    if isinstance(provenance, dict):
        context_notes = [key + ": " + safe_text(provenance[key]) for key in ("reviewed_on", "source_commit")
                         if key in provenance]
        if context_notes:
            lines[6:6] = ["Reviewed code context: " + "; ".join(context_notes) + ". This is a reviewed snapshot.", ""]
    for ident, case in sorted(cases.items()):
        evidence = case["evidence"]
        bucket = evidence["sources"]["vs:" + case["vs"]]
        values = [case["vs"], source_label(bucket), str(len(evidence["pairs"])),
                  str(len(evidence["strong_ids"]) + len(evidence.get("compact_strong_ids", []))), case["review_status"],
                  case["implementation"], case["validation"], case["acceptance"]]
        lines.append("| " + " | ".join(safe_text(v) for v in values) + " |")
    for ident, case in sorted(cases.items()):
        evidence = case["evidence"]
        lines += ["", "## " + safe_text(ident), "", safe_text(case["title"]), "",
                  f"Evidence: {len(evidence['observations'])} legacy records ({len(evidence['strong_ids'])} strong); "
                  f"{len(evidence.get('compact_refs', {}))} compact windows "
                  f"({len(evidence.get('compact_strong_ids', []))} strong child states); max_draws {evidence['max_draws']}.",
                  f"Review: **{case['review_status']}**. Fingerprint: `{case['fingerprint']}`.", "",
                  "Sources and exact VS/PS pairs:", ""]
        for key, bucket in sorted(evidence["sources"].items()):
            hashes = ", ".join(bucket["sha256"]) or "none"
            lines.append(f"- `{key}`: {source_label(bucket)}; SHA-256: {hashes}.")
        if evidence["no_ps"]:
            lines.append("- PS `0000000000000000`: no PS; not missing microcode.")
        for ps in evidence["pairs"]:
            lines.append(f"- Pair `{case['vs']}:{ps}`.")
        lines.extend(compact_report(ledger, case))
        gaps = []
        for key, bucket in sorted(evidence["sources"].items()):
            if bucket["missing"]:
                gaps.append(f"Archive source missing: {key}")
            if bucket["ambiguous"]:
                gaps.append(f"FNV collision/ambiguity: {key}; review every SHA-256 variant")
        context = evidence["context"]
        if context.get("mapping_unknown"):
            gaps.append("Current mapping context unknown")
        if context.get("mapping_slot", "unknown") is None:
            gaps.append("Reviewed mapping context has no position mapping for this VS")
        uncovered = [] if context.get("mapping_unknown") else [
            ps for ps in evidence["pairs"] if ps != ZERO_PS and
            f"{case['vs']}:{ps}" not in context.get("binding_pairs", [])]
        if uncovered:
            coverage = "Binding collection in the reviewed code context does not cover these PS pairs: " + ", ".join(uncovered)
            if context.get("binding_evidence_required") is True:
                gaps.append(coverage)
            else:
                lines += ["", "Collection coverage (informational): " + coverage + "."]
                if "binding_evidence_required" not in context:
                    lines.append("Whether binding evidence is needed for these pairs has not been established.")
        if case["review"]:
            current = case["review"]
            lines += ["", "Manual review: " + safe_text(current["decision"]) + ". " + safe_text(current["summary"]),
                      "Scope: " + safe_text(current.get("scope", "")),
                      f"Review fingerprint: `{current['fingerprint']}` ({case['review_status']})."]
            for reference in current.get("evidence", []):
                lines.append("Evidence reference: " + safe_text(reference))
            if case["review_status"] != "current":
                gaps.append("Manual conclusion predates current evidence; retain it as history and review the delta")
                for gap in current.get("gaps", []):
                    lines.append("Prior manual gap (needs recheck): " + safe_text(gap))
            else:
                gaps.extend(current.get("gaps", []))
        for historical in case["historical_reviews"]:
            lines += ["", "Historical review (unbound): " + safe_text(historical["decision"]) + ". " + safe_text(historical["summary"]),
                      "Scope: " + safe_text(historical.get("scope", ""))]
            for gap in historical.get("gaps", []):
                lines.append("Historical gap (unbound, not a current task): " + safe_text(gap))
            for reference in historical.get("evidence", []):
                lines.append("Evidence reference: " + safe_text(reference))
        if case["historical_reviews"] and not case["review"]:
            gaps.append("Reconcile historical conclusions and gaps against current evidence")
        lines += ["", "Gaps:", ""]
        lines += ["- " + safe_text(gap) for gap in dict.fromkeys(gaps)] or ["- No recorded gap; manual validation is still required."]
    return "\n".join(lines) + "\n"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    command = commands.add_parser("sync", help="Merge verified archive metadata without dropping historical evidence")
    command.add_argument("--archive", required=True, type=Path)
    command.add_argument("--ledger", required=True, type=Path)
    command.add_argument("--seed", type=Path)
    command.add_argument("--context", type=Path)
    command = commands.add_parser("report", help="Render the persisted ledger as Markdown")
    command.add_argument("--ledger", required=True, type=Path)
    command.add_argument("--output", type=Path)
    command = commands.add_parser("review", help="Bind a human review to the current evidence fingerprint")
    command.add_argument("--ledger", required=True, type=Path)
    command.add_argument("--case", required=True)
    command.add_argument("--file", required=True, type=Path)
    args = parser.parse_args(argv)
    try:
        if args.command == "sync":
            ledger, changed = sync(args.archive, args.ledger, args.seed, args.context)
            print(f"{'Updated' if changed else 'Unchanged'}: {len(ledger['cases'])} TAA cases")
        elif args.command == "review":
            review(args.ledger, args.case, args.file)
            print("Review recorded against current evidence fingerprint")
        else:
            require(args.ledger.is_file(), "Ledger does not exist")
            data = report_markdown(load_ledger(args.ledger))
            if args.output:
                require(args.output.resolve() != args.ledger.resolve(), "Report must not overwrite ledger")
                atomic_write(args.output, data.encode("utf-8"))
            else:
                print(data, end="")
        return 0
    except (LedgerError, OSError, KeyError, TypeError) as error:
        # Do not echo submitted JSON, credentials, or raw program contents.
        if isinstance(error, LedgerError):
            print(f"feedback-ledger: {error}", file=sys.stderr)
        else:
            print(f"feedback-ledger: {type(error).__name__}; input not updated", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
