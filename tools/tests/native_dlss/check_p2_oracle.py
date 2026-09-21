#!/usr/bin/env python3
"""Read-only P2 recording-evidence triage. Exit 0 is NOT DLSS acceptance.

Only supplied JSONL or frame-*/p2-oracle.jsonl in one capture is read. No asset scan, game execution, color profile
creation or renderer configuration changes occur. v1 traces do not prove GPU
execution or capture every intervening attachment write / texture transform.
"""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
import struct
import sys

SCHEMA = "lostodyssey.p2-oracle-evidence.v1"
PRODUCER = "b4b4d54a7a2d6b96"
COPY = "cda578aef1724fdc"
MAX_LINE = 1024 * 1024
MAX_BYTES = 64 * 1024 * 1024


def unique_object(pairs: list[tuple]) -> dict:
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def reject_constant(value: str):
    raise ValueError(f"non-finite JSON number: {value}")


def uint(value, label: str, maximum: int = (1 << 64) - 1) -> int:
    if type(value) is not int or not 0 <= value <= maximum:
        raise ValueError(f"{label}: expected unsigned integer")
    return value


def identity(record: dict, frame: int) -> tuple:
    return (frame, *(uint(record[k], k) for k in ("guest_base", "guest_format")))


def constants(draw: dict) -> dict:
    bank = draw["ps_constants"]
    if bank["bank_base"] != "0x4400":
        raise ValueError("PS constant bank is not 0x4400")
    result = {}
    for entry in bank["values"]:
        index = uint(entry["constant"], "constant", 255)
        words = entry["u32"]
        if index in result or entry["register"].lower() != f"0x{0x4400 + 4 * index:04x}":
            raise ValueError("duplicate constant or incorrect register address")
        if not isinstance(words, list) or len(words) != 4:
            raise ValueError("constant must contain four raw u32 words")
        result[index] = [uint(word, "constant word", 0xffffffff) for word in words]
    missing = set(range(11)) | {255}
    missing -= result.keys()
    if missing:
        raise ValueError(f"missing PS constants: {sorted(missing)}")
    return result


def extent(value, label: str, count: int = 2) -> tuple:
    if not isinstance(value, list) or len(value) != count:
        raise ValueError(f"{label}: incorrect component count")
    return tuple(uint(v, label, 0xffffffff) for v in value)


def input_lines(path: Path):
    # The renderer writes a separate JSONL in each of three frame directories.
    # Enumerate only this fixed metadata pattern, never textures/shaders/assets.
    files = sorted(path.glob("frame-*/p2-oracle.jsonl")) if path.is_dir() else [path]
    if not files:
        raise ValueError("no frame-*/p2-oracle.jsonl in the supplied capture directory")
    total = 0
    for file in files:
        with file.open("rb") as stream:
            line = 0
            while raw := stream.readline(MAX_LINE + 1):
                line += 1
                total += len(raw)
                if len(raw) > MAX_LINE or total > MAX_BYTES:
                    raise ValueError("trace exceeds bounded input size")
                if raw.strip():
                    yield f"{file.parent.name}/{file.name}:{line}", raw


def analyze(path: Path) -> dict:
    producers, resolves, chains, issues = {}, {}, [], []
    previous_frame = -1
    for line_number, (location, raw) in enumerate(input_lines(path), 1):
        try:
            event = json.loads(raw, object_pairs_hook=unique_object, parse_constant=reject_constant)
        except (UnicodeError, ValueError) as error:
            raise ValueError(f"{location}: {error}") from error
        try:
            if not isinstance(event, dict) or event.get("schema") != SCHEMA:
                raise ValueError("unsupported event schema")
            frame = uint(event["renderer_frame"], "renderer_frame")
            if frame < previous_frame:
                raise ValueError("renderer frames moved backwards; do not combine capture sessions")
            previous_frame = frame
            event["_location"] = location
            if event["event"] == "capture":
                if event.get("encoding_claim") != "unknown":
                    raise ValueError("capture metadata unexpectedly claims a color encoding")
                continue
            if event["event"] == "resolve":
                if event["kind"] == "color":
                    destination = event["destination"]
                    resolves[identity(destination, frame)] = (line_number, event)
                continue
            if event["event"] != "draw":
                raise ValueError("unrecognized event type")
            shader = event["shader"]["ps"].lower()
            if shader not in (PRODUCER, COPY):
                continue
            allocation = uint(event["destination"]["allocation"], "draw allocation")
            # Invalidate an earlier producer before validating a later write.
            if shader == PRODUCER:
                producers.pop((frame, allocation), None)
            words = constants(event)
            if shader == PRODUCER:
                producers[frame, allocation] = (line_number, event, words)
                continue
            sampled = event["sampled_slot0_resolved"]
            if not isinstance(sampled, dict):
                raise ValueError("scene copy has no recorded resolved slot0 source")
            if uint(sampled["write_frame"], "write_frame") != frame:
                raise ValueError("scene copy samples a stale frame")
            pair = resolves.get(identity(sampled, frame))
            if pair is None:
                raise ValueError("missing prior color resolve")
            resolve_line, resolve = pair
            target, source = resolve["destination"], resolve["source"]
            for field in ("allocation", "write_version", "write_ordinal", "host_format"):
                if uint(sampled[field], field) != uint(target[field], field):
                    raise ValueError(f"resolve-to-copy {field} mismatch")
            if extent(sampled["rect"], "sampled rect", 4) != extent(target["rect"], "resolve rect", 4):
                raise ValueError("resolve-to-copy rectangle mismatch")
            producer = producers.get((frame, uint(source["allocation"], "source allocation")))
            if producer is None or producer[0] >= resolve_line:
                raise ValueError("missing prior tone-map producer for resolve allocation")
            producer_line, draw, producer_words = producer
            if uint(draw["destination"]["host_format"], "producer format") != uint(source["host_format"], "resolve source format"):
                raise ValueError("producer-to-resolve format mismatch")
            if extent(draw["destination"]["extent"], "producer extent") != extent(source["extent"], "source extent"):
                raise ValueError("producer-to-resolve extent mismatch")
            for field in ("cpu_serial", "geometry_epoch", "consumer", "input", "output", "output_rect"):
                if draw["plan"][field] != event["plan"][field]:
                    raise ValueError(f"producer-to-copy plan {field} mismatch")
            exponent_bits = producer_words[10][0]
            exponent = struct.unpack("<f", struct.pack("<I", exponent_bits))[0]
            if not math.isfinite(exponent) or exponent <= 0:
                raise ValueError("producer c10.x is not finite and positive")
            chains.append({"renderer_frame": frame, "producer_line": producer_line,
                           "resolve_line": resolve_line, "copy_line": line_number,
                           "producer_source": draw["_location"], "resolve_source": resolve["_location"],
                           "copy_source": location,
                           "producer_c10_x_bits": f"0x{exponent_bits:08x}", "producer_c10_x": exponent,
                           "producer_constants_u32": producer_words, "copy_constants_u32": words,
                           "source_allocation": source["allocation"], "resolved_allocation": target["allocation"],
                           "copy_destination_allocation": allocation, "plan": event["plan"]})
        except (KeyError, TypeError, AttributeError, ValueError) as error:
            issues.append({"line": line_number, "source": location, "reason": str(error)})
    frames = sorted({chain["renderer_frame"] for chain in chains})
    longest = current = 0
    previous = None
    for frame in frames:
        current = current + 1 if previous is not None and frame == previous + 1 else 1
        longest = max(longest, current)
        previous = frame
    if longest < 3:
        issues.append({"line": None, "reason": "need at least three consecutive frames with complete recorded chains"})
    return {"schema": "lostodyssey.p2-oracle-review.v1",
            "status": "incomplete" if issues else "ready_for_manual_color_review",
            "color_encoding": "unknown", "p2_accepted": False,
            "scope": "recorded metadata only; not GPU execution, full binding provenance or color-transfer qualification",
            "frames": frames, "longest_consecutive_run": longest, "chains": chains, "issues": issues}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path, help="one three-frame capture directory, or a combined p2-oracle.jsonl")
    parser.add_argument("--output", type=Path, help="optional review JSON; never replaces the input trace")
    args = parser.parse_args(argv)
    try:
        if args.output and (args.output.resolve() == args.trace.resolve() or
                            (args.trace.is_dir() and args.output.name == "p2-oracle.jsonl")):
            raise ValueError("output must differ from input")
        report = analyze(args.trace)
        text = json.dumps(report, indent=2, allow_nan=False) + "\n"
        if args.output:
            args.output.write_text(text, encoding="utf-8")
        else:
            print(text, end="")
        return 0 if report["status"] == "ready_for_manual_color_review" else 2
    except (OSError, ValueError, RecursionError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
