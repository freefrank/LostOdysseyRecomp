#!/usr/bin/env python3
"""Export reviewed F1 depth/material register pairs as a compact C++ fixture.

Every material draw, hash and position slot is explicit. This script does not
choose shader mappings, translate microcode, or edit production code.
"""

import argparse
import json
from pathlib import Path
import re

try:
    from . import trace
    from .jitter_candidates import PAIR_REGS, camera_bits, first_resolve_draw, missing_pair_evidence, register_words
except ImportError:  # Direct script invocation.
    import trace
    from jitter_candidates import PAIR_REGS, camera_bits, first_resolve_draw, missing_pair_evidence, register_words


DRAW_SPEC = re.compile(r"(\d+):([0-9a-fA-F]{16}):(\d+)$")
HASH = re.compile(r"[0-9a-fA-F]{16}$")
IDENTIFIER = re.compile(r"[A-Za-z_]\w*$")


def parse_draw_spec(text: str) -> tuple[int, str, int]:
    match = DRAW_SPEC.fullmatch(text)
    if not match or int(match[3]) > 12:
        raise ValueError(f"invalid draw specification {text!r}; expected ID:16HEX:SLOT (slot 0..12)")
    return int(match[1]), match[2].lower(), int(match[3])


def _capture_words(state, start, count):
    values = register_words(state, range(start, start + count))
    if any(value is None for value in values):
        raise ValueError(f"missing captured constants at 0x{start:04x}")
    return values


def collect(capture, directory: str, specs: list[tuple[int, str, int]],
            depth_vs: str, depth_slot: int, before_draw: int | None = None) -> list[dict]:
    if not 0 <= depth_slot <= 4 or any(not 0 <= slot <= 12 for _, _, slot in specs):
        raise ValueError("compact fixture supports material slots 0..12 and depth slots 0..4")
    frame = trace.parse_frame(capture, directory)
    cutoff = before_draw if before_draw is not None else first_resolve_draw(capture.read_text(f"{directory}/render-state.txt"))
    if cutoff <= 0:
        raise ValueError("draw boundary must be positive")
    scene_path = f"{directory}/temporal-scene.json"
    if scene_path not in capture.names:
        raise ValueError(f"missing {scene_path}")
    vp = json.loads(capture.read_text(scene_path)).get("vp_u32")
    if not isinstance(vp, list) or len(vp) != 16:
        raise ValueError("temporal scene has no 16-word VP")
    requested = {draw: (shader, slot) for draw, shader, slot in specs}
    if len(requested) != len(specs):
        raise ValueError("duplicate draw ID")
    if any(draw >= cutoff for draw in requested):
        raise ValueError(f"requested draw occurs at/after draw boundary {cutoff}")
    depth_records = []
    rows = []
    for draw, state in trace.iter_draw_states(frame):
        draw_id = draw["id"]
        if draw_id >= cutoff:
            break
        shader = draw.get("shader")
        if not shader:
            continue
        geometry = tuple(draw["header"].get(key) for key in ("base", "indices", "indexed"))
        world = register_words(state, range(0x4000, 0x4010))
        fetch95 = register_words(state, (0x48be, 0x48bf))
        pass_state = register_words(state, PAIR_REGS)
        missing = missing_pair_evidence(draw["header"], state)
        if shader.get("vs", "").lower() == depth_vs and shader.get("ps_status") == "not_bound":
            if not missing and state.get(0x2208) == 5 and state.get(0x2200, 0) != 0 and camera_bits(state, depth_slot) == vp:
                depth_records.append({"id": draw_id, "geometry": geometry, "world": world,
                    "fetch95": fetch95, "pass_state": pass_state,
                    "constants": _capture_words(state, 0x4000, 32)})
        if draw_id not in requested:
            continue
        expected_vs, slot = requested[draw_id]
        if missing:
            raise ValueError(f"draw {draw_id} missing exact pair evidence: {', '.join(missing)}")
        if shader.get("vs", "").lower() != expected_vs:
            raise ValueError(f"draw {draw_id} VS identity differs from explicit specification")
        ps = shader.get("ps", "").lower()
        if shader.get("ps_status") != "bound" or not HASH.fullmatch(ps) or ps == "0"*16:
            raise ValueError(f"draw {draw_id} has no valid bound material PS")
        if camera_bits(state, slot) != vp:
            raise ValueError(f"draw {draw_id} slot {slot} does not equal captured scene VP")
        matches = [record for record in depth_records if
                   record["geometry"] == geometry and record["world"] == world and
                   record["fetch95"] == fetch95 and record["pass_state"] == pass_state]
        if len(matches) != 1:
            raise ValueError(f"draw {draw_id} has {len(matches)} exact earlier depth companions")
        depth = matches[0]
        row = {"draw": draw_id, "depth_draw": depth["id"], "vs": expected_vs,
               "ps": ps, "depth_vs": depth_vs, "slot": slot,
               "vertex": _capture_words(state, 0x4000, 64),
               "vertex_late": _capture_words(state, 0x4000 + 254*4, 8),
               "depth": depth["constants"], "pixel": _capture_words(state, 0x4400, 64)}
        if row["vertex"][slot*4:slot*4+16] != row["depth"][depth_slot*4:depth_slot*4+16]:
            raise ValueError(f"draw {draw_id} depth camera differs from material camera")
        rows.append(row)
    if set(requested) != {row["draw"] for row in rows}:
        raise ValueError("some requested draws were absent before first resolve")
    return rows


def render_header(rows: list[dict], namespace: str, source: str) -> str:
    if not IDENTIFIER.fullmatch(namespace):
        raise ValueError("namespace must be a C++ identifier")

    def words(values):
        return "\n".join("        " + ",".join(f"0x{value:08x}" for value in values[i:i+4]) +
                         ("," if i+4 < len(values) else "") for i in range(0, len(values), 4))

    lines = ["#pragma once", "#include <array>", "#include <cstdint>",
             f"// Exact cumulative register banks from {source} before the first resolve.",
             "// Each depth companion shares index geometry, fetch95, world, camera and pass viewport.",
             "// Captured constants support arithmetic checks; local vertex positions remain synthetic.",
             f"namespace {namespace} {{",
             "struct Draw { uint64_t vs, ps, depthVs; unsigned slot, draw, depthDraw;",
             "    std::array<uint32_t,64> vertex; std::array<uint32_t,8> vertexLate;",
             "    std::array<uint32_t,32> depth; std::array<uint32_t,64> pixel; };",
             "inline constexpr Draw draws[]{"]
    for row in rows:
        lines.append(f"    {{0x{row['vs']}ull,0x{row['ps']}ull,0x{row['depth_vs']}ull,{row['slot']},{row['draw']},{row['depth_draw']}, {{")
        for name in ("vertex", "vertex_late", "depth", "pixel"):
            if name != "vertex":
                lines.append("    }, {")
            lines.append(words(row[name]))
        lines.append("    }},")
    lines.extend(["};", "}", ""])
    return "\n".join(lines)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--frame", required=True, type=int)
    parser.add_argument("--draw", required=True, action="append", help="ID:16HEX:SLOT; repeat for each reviewed draw")
    parser.add_argument("--depth-vs", required=True, help="reviewed depth VS 16-digit hash")
    parser.add_argument("--depth-slot", required=True, type=int)
    parser.add_argument("--before-draw", type=int, help="explicit exclusive draw boundary; defaults to first resolve")
    parser.add_argument("--namespace", required=True)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args(argv)
    if args.output.exists():
        parser.error("output already exists")
    if args.input.is_dir() and args.output.resolve().is_relative_to(args.input.resolve()):
        parser.error("output must be outside input capture")
    try:
        specs = [parse_draw_spec(text) for text in args.draw]
        if not HASH.fullmatch(args.depth_vs) or not 0 <= args.depth_slot <= 4:
            raise ValueError("depth shader hash invalid or slot outside 0..4 fixture range")
        with trace.load_capture(args.input) as capture:
            directories = [directory for directory in capture.frame_names()
                           if re.search(rf"-f{args.frame}$", directory)]
            if len(directories) != 1:
                raise ValueError("requested frame not found or ambiguous")
            rows = collect(capture, directories[0], specs, args.depth_vs.lower(), args.depth_slot, args.before_draw)
        output = render_header(rows, args.namespace, f"frame f{args.frame}")
    except (OSError, ValueError, KeyError) as exc:
        parser.error(str(exc))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(output, encoding="utf-8")


if __name__ == "__main__":
    main()
