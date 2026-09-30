#!/usr/bin/env python3
"""Summarize unmapped VS evidence across all frames of explicit F1 captures.

Reads metadata and candidate HLSL only, serially. Never edits the production map.
"""

import argparse
import json
from pathlib import Path
import zipfile

try:
    from . import jitter_candidates as jitter, trace
except ImportError:
    import jitter_candidates as jitter
    import trace


STAGES = ("before_first_resolve", "at_or_after_first_resolve", "resolve_boundary_unknown")
ERRORS = (OSError, ValueError, KeyError, TypeError, AttributeError, zipfile.BadZipFile)
# FinalizeDebugCapture writes dummyBindings beside these actual draw-drop counters.
DROP_COUNTERS = {"mode", "shader", "pitch", "pipeline", "upload", "index", "scissor",
                 "draws", "shaders"}
REQUIRED_DROP_COUNTERS = DROP_COUNTERS - {"draws", "shaders"}


def error_record(error):
    return {"type": type(error).__name__, "message": str(error)}


def stage_for(draw, boundary):
    if boundary is None:
        return STAGES[2]
    return STAGES[0] if draw < boundary else STAGES[1]


def capture_count_gaps(info, directories, consecutive=False):
    counts = {}
    for field in ("requested_frames", "completed_frames"):
        value = info.get(field, "")
        if not value.isdigit():
            return ["capture_frame_counts_unknown"]
        counts[field] = int(value)
    gaps = []
    if counts["requested_frames"] < 1 or counts["requested_frames"] != counts["completed_frames"]:
        gaps.append("capture_frame_count_mismatch")
    if len(directories) != counts["completed_frames"]:
        gaps.append("capture_frame_directories_missing_or_extra")
    ids = sorted(int(trace.FRAME.fullmatch(name)[1]) for name in directories)
    if len(ids) != len(set(ids)):
        gaps.append("duplicate_frame_identity")
    if consecutive:
        first, last = info.get("first_frame", ""), info.get("last_attempted_frame", "")
        if not first.isdigit() or not last.isdigit():
            gaps.append("consecutive_frame_range_unknown")
        elif (not ids or ids[0] != int(first) or ids[-1] != int(last)
              or any(b != a + 1 for a, b in zip(ids, ids[1:]))):
            gaps.append("consecutive_frame_range_mismatch")
    return gaps


def inspect_frame(capture, directory, mapping):
    result = {"directory": directory, "frame": int(trace.FRAME.fullmatch(directory)[1]),
              "gaps": [], "errors": [], "unmapped_draws": [], "candidates": []}
    frame = trace.parse_frame(capture, directory)
    result.update(header=frame["header"], footer=frame["footer"], end=frame["end"],
                  trace_draw_count=len(frame["draws"]))
    if frame["end"] is None:
        result["gaps"].append("missing_frame_end")
    drop_lines = [line for line in frame["footer"] if line.startswith("drops ")]
    result["drop_records"] = drop_lines
    result["drop_counters"] = {}
    result["diagnostic_counters"] = {}
    result["unclassified_drop_fields"] = {}
    if not drop_lines:
        result["gaps"].append("drops_unspecified")
    for line in drop_lines:
        values = trace.fields(line)
        drops = {key: value for key, value in values.items() if key in DROP_COUNTERS}
        result["drop_counters"].update(drops)
        result["diagnostic_counters"].update({key: value for key, value in values.items() if key == "dummy_bindings"})
        unknown = {key: value for key, value in values.items() if key not in DROP_COUNTERS and key != "dummy_bindings"}
        result["unclassified_drop_fields"].update(unknown)
        if unknown:
            result["gaps"].append("unclassified_drop_fields")
        if not drops or any(not value.isdigit() or int(value) != 0 for value in drops.values()):
            result["gaps"].append("trace_drops_or_unknown_drop_count")
    if not REQUIRED_DROP_COUNTERS.issubset(result["drop_counters"]):
        result["gaps"].append("drop_counters_incomplete")
    if not frame["draws"]:
        result["gaps"].append("no_draws")
        return result
    ids = [draw["id"] for draw in frame["draws"]]
    if ids != sorted(set(ids)):
        result["gaps"].append("draw_ids_not_strictly_increasing")
    result["before_draw"] = max(ids) + 1
    text = capture.read_text(f"{directory}/render-state.txt")
    resolves = [int(match[1]) for match in jitter.RESOLVE.finditer(text)]
    boundary = min(resolves) if resolves else None
    result["first_resolve_draw"] = boundary
    if boundary is None:
        result["gaps"].append("missing_resolve_boundary")
    missing_shaders = []
    non_shader_commands, unknown_execution_draws, unknown_commands = [], [], []
    for draw, state in trace.iter_draw_states(frame):
        shader = draw.get("shader") or {}
        vs = shader.get("vs")
        if not vs:
            mode = state.get(0x2208)
            mode = mode & 7 if mode is not None else None
            record = {"draw": draw["id"], "ordinal": draw["ordinal"], "rb_mode": mode,
                      "events": draw["events"]}
            # Draw logs before DrawImpl dispatches mode 6 to Resolve(), which
            # returns before graphics shader selection. Resolve event draw IDs
            # use the incremented counter, so do not join them by equality.
            if mode == 6:
                non_shader_commands.append({**record, "kind": "resolve_copy"})
                continue
            missing_shaders.append(draw["id"])
            if mode in (4, 5):
                unknown_execution_draws.append({**record, "kind": "graphics_shader_unknown"})
            else:
                unknown_commands.append({**record, "kind": "command_execution_unknown"})
            continue
        vs = vs.lower()
        if vs not in mapping:
            result["unmapped_draws"].append({"draw": draw["id"], "ordinal": draw["ordinal"],
                "vs": vs, "ps": shader.get("ps"), "ps_status": shader.get("ps_status"),
                "stage": stage_for(draw["id"], boundary)})
    result["missing_shader_draws"] = missing_shaders
    result["non_shader_commands"] = non_shader_commands
    result["unknown_execution_draws"] = unknown_execution_draws
    result["unknown_commands"] = unknown_commands
    if unknown_execution_draws:
        result["gaps"].append("missing_shader_identity")
    if unknown_commands:
        result["gaps"].append("unknown_command_execution")
    # Keep parsed unknown draws even if camera JSON or candidate analysis fails.
    try:
        analysis = jitter.analyze_frame(capture, directory, mapping, before_draw=max(ids) + 1)
        result["scene_vp_available"] = analysis["scene_vp_available"]
        result["incomplete_depth_draws"] = analysis["incomplete_depth_draws"]
        if not analysis["scene_vp_available"]:
            result["gaps"].append("missing_scene_vp")
        if analysis["incomplete_depth_draws"]:
            result["gaps"].append("incomplete_depth_evidence")
        result["candidates"] = analysis["candidates"]
    except ERRORS as error:
        result["scene_vp_available"] = None
        result["errors"].append(error_record(error))
        result["gaps"].append("candidate_analysis_failed")
    for candidate in result["candidates"]:
        for draw in candidate["draws"]:
            draw["stage"] = stage_for(draw["draw"], boundary)
    return result


def aggregate(captures):
    shaders = {}
    for capture in captures:
        for frame in capture["frames"]:
            evidence = {}
            for candidate in frame.get("candidates", []):
                for draw in candidate["draws"]:
                    evidence.setdefault((candidate["vs"], draw["draw"]), []).append({
                        "slot": candidate["candidate_slot"], "hlsl": candidate["hlsl"],
                        "position_chain_hint": candidate["position_chain_hint"],
                        "position_window_constants": candidate["position_window_constants"],
                        "strict_depth_pairs": draw["matching_depth_draws"],
                        "geometry_depth_pairs": draw["matching_geometry_depth_draws"],
                        "runtime_jitter": draw.get("runtime_jitter"),
                        "texture_bindings": draw.get("texture_bindings", []),
                        "missing_evidence": draw["missing_evidence"],
                        "pass_registers": draw["pass_registers"]})
            for draw in frame.get("unmapped_draws", []):
                entry = shaders.setdefault(draw["vs"], {"vs": draw["vs"], "occurrences": []})
                slots = evidence.get((draw["vs"], draw["draw"]), [])
                if frame["errors"]:
                    status = "analysis_error"
                elif not frame.get("scene_vp_available"):
                    status = "camera_unknown"
                elif not slots:
                    status = "no_exact_camera_slot_match"
                else:
                    status = "candidate_requires_review"
                entry["occurrences"].append({"capture": capture["input"],
                    "directory": frame["directory"], "frame": frame["frame"], **draw,
                    "status": status, "slots": slots})
    for entry in shaders.values():
        occurrences = entry["occurrences"]
        entry["capture_count"] = len({o["capture"] for o in occurrences})
        entry["frame_count"] = len({(o["capture"], o["directory"]) for o in occurrences})
        entry["draw_count"] = len(occurrences)
        entry["candidate_slots"] = sorted({s["slot"] for o in occurrences for s in o["slots"]})
        entry["stage_counts"] = {stage: sum(o["stage"] == stage for o in occurrences) for stage in STAGES}
        entry["position_hint_draw_count"] = sum(any(s["position_chain_hint"] for s in o["slots"]) for o in occurrences)
        entry["strict_depth_paired_draw_count"] = sum(any(s["strict_depth_pairs"] for s in o["slots"]) for o in occurrences)
        entry["geometry_depth_paired_draw_count"] = sum(any(s["geometry_depth_pairs"] for s in o["slots"]) for o in occurrences)
        entry["before_resolve_position_and_depth_draw_count"] = sum(
            o["stage"] == STAGES[0] and any(s["position_chain_hint"] and s["strict_depth_pairs"]
                                           for s in o["slots"]) for o in occurrences)
        entry["unknown_or_unmatched_draw_count"] = sum(not o["slots"] for o in occurrences)
    return sorted(shaders.values(), key=lambda e: (-e["before_resolve_position_and_depth_draw_count"],
        -e["strict_depth_paired_draw_count"], -e["position_hint_draw_count"],
        -e["geometry_depth_paired_draw_count"], -e["draw_count"], e["vs"]))


def analyze(inputs, mapping_header):
    mapping = jitter.position_slots(mapping_header)
    captures = []
    # Repeating a path is not an independent capture observation.
    paths = list(dict.fromkeys(Path(path).resolve() for path in inputs))
    for path in paths:
        record = {"input": str(path), "capture_info": [], "frames": [], "errors": [], "gaps": []}
        captures.append(record)
        try:
            with trace.load_capture(path) as capture:
                info = {}
                try:
                    record["capture_info"] = capture.read_text("capture-info.txt").splitlines()
                    info = trace.fields(" ".join(record["capture_info"]))
                    if info.get("status") != "complete":
                        record["gaps"].append("capture_completion_unknown_or_incomplete")
                except ERRORS as error:
                    record["errors"].append(error_record(error))
                directories = capture.frame_names()
                record["gaps"].extend(capture_count_gaps(info, directories,
                    any("Frames are consecutive rendered frames." in line for line in record["capture_info"])))
                if not directories:
                    record["gaps"].append("no_frames")
                for directory in directories:
                    try:
                        frame = inspect_frame(capture, directory, mapping)
                    except ERRORS as error:
                        frame = {"directory": directory, "frame": int(trace.FRAME.fullmatch(directory)[1]),
                                 "gaps": ["frame_trace_unavailable"], "errors": [error_record(error)]}
                    record["frames"].append(frame)
        except ERRORS as error:
            record["errors"].append(error_record(error))
    candidates = aggregate(captures)
    frames = [frame for capture in captures for frame in capture["frames"]]
    incomplete = any(c["errors"] or c["gaps"] for c in captures) or any(f["errors"] or f["gaps"] for f in frames)
    return {"schema": "lostodyssey.jitter-coverage.v1", "mapping_header": str(Path(mapping_header).resolve()),
        "mapped_shader_count": len(mapping), "captures": captures, "unmapped_shaders": candidates,
        "summary": {"capture_count": len(captures), "frame_count": len(frames),
            "frames_with_errors": sum(bool(f["errors"]) for f in frames),
            "captures_with_errors": sum(bool(c["errors"]) for c in captures),
            "frames_with_gaps": sum(bool(f["gaps"]) for f in frames),
            "unknown_execution_draw_count": sum(len(f.get("unknown_execution_draws", [])) for f in frames),
            "non_shader_command_count": sum(len(f.get("non_shader_commands", [])) for f in frames),
            "unknown_command_count": sum(len(f.get("unknown_commands", [])) for f in frames),
            "unique_unmapped_vs": len(candidates), "unmapped_draw_count": sum(e["draw_count"] for e in candidates),
            "coverage": "insufficient_metadata" if incomplete else "metadata_scanned",
            "decision": "insufficient_evidence" if incomplete else
                        "manual_review_required" if candidates else "no_unmapped_vs_observed_not_a_rendering_pass"},
        "limits": ["All parsed draws are analyzed with the explicit exclusive max(draw ID)+1 cutoff.",
            "First resolve separates trace order only; neither early nor later draws have proven scene/pixel coverage.",
            "Strict depth pairs match captured fields, not host allocation identity or GPU completion; multiple pairs remain ambiguous.",
            "Geometry depth pairs allow different pass states and report those differences; they are not authorization to map a shader.",
            "Position hints are lexical, and unmatched/missing evidence is not a passing mapping review.",
            "No images, binary surfaces or shader payload hashes were read; runtime/GPU provenance is only the recorded metadata."]}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", action="append", required=True, type=Path, help="Repeat for each ZIP or extracted capture")
    parser.add_argument("--mapping", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path, help="New JSON file outside every input")
    args = parser.parse_args(argv)
    try:
        for source in args.input:
            trace.check_new_output(source, args.output)
        result = analyze(args.input, args.mapping)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open("x", encoding="utf-8") as output:
            json.dump(result, output, indent=2)
            output.write("\n")
    except ERRORS as error:
        parser.error(str(error))
    print(json.dumps(result["summary"], sort_keys=True))


if __name__ == "__main__":
    main()
