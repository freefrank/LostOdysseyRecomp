#!/usr/bin/env python3
"""Triage unlisted position shaders in an existing F1 capture.

Exact camera/geometry matches are candidates for manual HLSL and visual review;
this tool never changes the production shader map.
"""

import argparse
import json
from pathlib import Path
import re

try:
    from . import trace
except ImportError:  # Direct script invocation.
    import trace


CASE = re.compile(r"case\s+0x([0-9a-fA-F]{1,16})ull|return\s+(-?\d+)\s*;")
RESOLVE = re.compile(r"^resolve\s+\S+\s+draw=(\d+)\b", re.MULTILINE)
PASS_REGS = (0x2000, 0x2001, 0x2002, 0x2080, 0x2081, 0x2082,
             *range(0x210f, 0x2115), 0x2206, 0x2208)
PAIR_REGS = (0x2000, 0x2002, 0x2080, 0x2081, 0x2082,
             *range(0x210f, 0x2115), 0x2200, 0x2206)
COMPARE_REGS = tuple(sorted(set(PASS_REGS) | set(PAIR_REGS) | {0x2201}))


def position_slots(header: Path) -> dict[str, int]:
    source = header.read_text(encoding="utf-8")
    start = source.find("inline int PositionVPSlot(")
    end = source.find("default:return -1;", start)
    if start < 0 or end < 0:
        raise ValueError("PositionVPSlot switch not found")
    result: dict[str, int] = {}
    pending: list[str] = []
    for match in CASE.finditer(source[start:end]):
        if match[1]:
            pending.append(match[1].lower().zfill(16))
        else:
            slot = int(match[2])
            for shader in pending:
                if shader in result and result[shader] != slot:
                    raise ValueError(f"conflicting PositionVPSlot for {shader}")
                result[shader] = slot
            pending.clear()
    if pending:
        raise ValueError("unfinished PositionVPSlot case group")
    return result


def camera_bits(state: dict[int, int], slot: int) -> list[int | None]:
    return [state.get(0x4000 + 4 * slot + i) for i in range(16)]


def register_words(state: dict[int, int], addresses) -> tuple[int | None, ...]:
    return tuple(state.get(address) for address in addresses)


def missing_pair_evidence(fields: dict, state: dict[int, int]) -> list[str]:
    missing = [key for key in ("base", "indices", "indexed") if key not in fields]
    missing.extend(f"0x{address:04x}" for address in
                   (*range(0x4000, 0x4010), 0x48be, 0x48bf, *PAIR_REGS)
                   if address not in state)
    return missing


def first_resolve_draw(text: str) -> int:
    draws = [int(match[1]) for match in RESOLVE.finditer(text)]
    if not draws:
        raise ValueError("frame has no resolve draw; pre-resolve boundary is unknown")
    return min(draws)


def position_window_constants(hlsl: str) -> set[int]:
    """Weak lexical hint from the four rows immediately feeding the oPos write.

    This does not trace swizzles, branches or temporaries; reviewers must inspect
    the shader before assigning a production slot.
    """
    lines = hlsl.splitlines()
    writes = [i for i, line in enumerate(lines) if re.search(r"\boPos\.xyzw\s*=\s*xePV\.xyzw", line)]
    if not writes:
        return set()
    end = writes[0]
    return {int(value) for line in lines[max(0, end-18):end]
            for value in re.findall(r"XeConst\((\d+)\)", line)}


def analyze_frame(capture, directory: str, mapping: dict[str, int], before_draw: int | None = None) -> dict:
    frame = trace.parse_frame(capture, directory)
    cutoff = before_draw if before_draw is not None else max((draw["id"] for draw in frame["draws"]), default=-1) + 1
    if cutoff <= 0:
        raise ValueError("empty frame or nonpositive draw boundary")
    cutoff_source = "explicit" if before_draw is not None else "full_frame"
    scene_path = f"{directory}/temporal-scene.json"
    scene = json.loads(capture.read_text(scene_path)) if scene_path in capture.names else None
    vp = scene.get("vp_u32") if scene else None
    if vp is not None and (len(vp) != 16 or not all(isinstance(v, int) for v in vp)):
        raise ValueError(f"invalid temporal scene VP in {scene_path}")
    depth_draws = []
    candidates = {}
    unlisted = set()
    position_hints = {}
    incomplete_depth_draws = []
    for draw, state in trace.iter_draw_states(frame):
        if draw["id"] >= cutoff:
            break
        shader = draw.get("shader")
        if not shader or not shader.get("vs"):
            continue
        vs = shader["vs"].lower()
        slot = mapping.get(vs)
        fields = draw["header"]
        geometry = (fields.get("base"), fields.get("indices"), fields.get("indexed"))
        world = register_words(state, range(0x4000, 0x4010))
        fetch95 = register_words(state, (0x48be, 0x48bf))
        pair_state = register_words(state, PAIR_REGS)
        missing = missing_pair_evidence(fields, state)
        if slot is not None:
            if vp and shader.get("ps_status") == "not_bound" and camera_bits(state, slot) == vp:
                depth_missing = missing + (["0x2208"] if 0x2208 not in state else [])
                if depth_missing:
                    incomplete_depth_draws.append({"draw": draw["id"], "vs": vs, "missing_evidence": depth_missing})
                elif state.get(0x2208) == 5 and state.get(0x2200, 0) != 0:
                    # Mode/depth state is only a depth-pass hint. The draw's
                    # actual raster result is not reconstructed here.
                    depth_draws.append((draw["id"], vs, geometry, world, fetch95, pair_state,
                                        fields.get("prim"), {reg: state.get(reg) for reg in COMPARE_REGS}))
            continue
        unlisted.add(vs)
        if not vp:
            continue
        # Search all complete constant matrices. A byte-exact match can arise
        # from unused constants; HLSL position dependency still needs review.
        for candidate_slot in range(253):
            if camera_bits(state, candidate_slot) != vp:
                continue
            matches = [{"draw": prior_id, "vs": prior_vs} for
                       prior_id, prior_vs, prior_geometry, prior_world, prior_fetch, prior_pass, _, _ in depth_draws
                       if not missing and geometry == prior_geometry and world == prior_world and fetch95 == prior_fetch
                       and pair_state == prior_pass]
            # Late lighting intentionally changes depth writes, stencil, blend
            # and scissor. Keep its geometry association separate from the strict
            # same-pass pair used by fixture export; neither proves pixel coverage.
            geometry_matches = []
            for prior_id, prior_vs, prior_geometry, prior_world, prior_fetch, _, prior_prim, prior_state in depth_draws:
                if (missing or fields.get("prim") is None or fields.get("prim") != prior_prim or
                        geometry != prior_geometry or world != prior_world or fetch95 != prior_fetch):
                    continue
                differences = {f"0x{reg:04x}": {"depth": prior_state.get(reg), "candidate": state.get(reg)}
                               for reg in COMPARE_REGS
                               if prior_state.get(reg) != state.get(reg)}
                geometry_matches.append({"draw": prior_id, "vs": prior_vs, "pass_differences": differences})
            key = (vs, candidate_slot)
            hlsl_path = f"shaders/{vs}.hlsl"
            if vs not in position_hints:
                position_hints[vs] = position_window_constants(capture.read_text(hlsl_path)) if hlsl_path in capture.names else set()
            entry = candidates.setdefault(key, {"vs": vs, "candidate_slot": candidate_slot,
                "hlsl": hlsl_path if hlsl_path in capture.names else None,
                "position_window_constants": sorted(position_hints[vs]),
                "position_chain_hint": all(candidate_slot+i in position_hints[vs] for i in range(4)),
                "draws": []})
            entry["draws"].append({"draw": draw["id"], "ps": shader.get("ps"),
                "indices": fields.get("indices"), "index_base": fields.get("base"),
                "matching_depth_draws": matches,
                "matching_geometry_depth_draws": geometry_matches,
                "runtime_jitter": draw.get("runtime_jitter"),
                "texture_bindings": draw.get("texture_bindings", []),
                "missing_evidence": missing,
                "pass_registers": {f"0x{reg:04x}": f"0x{state[reg]:08x}" for reg in PASS_REGS if reg in state}})
    return {"directory": directory, "frame": frame["frame"], "before_draw": cutoff,
            "cutoff_source": cutoff_source,
            "scene_vp_available": vp is not None,
            "unlisted_vs_before_cutoff": sorted(unlisted),
            "incomplete_depth_draws": incomplete_depth_draws,
            "candidates": sorted(candidates.values(),
                key=lambda entry: (-int(entry["position_chain_hint"]), -sum(bool(d["matching_depth_draws"]) for d in entry["draws"]),
                                   entry["vs"], entry["candidate_slot"]))}


def analyze(input_path: Path, mapping_header: Path, frame_number: int | None = None,
            before_draw: int | None = None) -> dict:
    mapping = position_slots(mapping_header)
    with trace.load_capture(input_path) as capture:
        directories = capture.frame_names()
        if frame_number is not None:
            directories = [d for d in directories if re.search(rf"-f{frame_number}$", d)]
        if not directories:
            raise ValueError("requested capture frame not found")
        frames = [analyze_frame(capture, directory, mapping, before_draw) for directory in directories]
    return {"schema": "lostodyssey.jitter-candidates.v1", "input": str(input_path),
            "mapping_header": str(mapping_header), "mapped_shader_count": len(mapping),
            "decision": "camera, position-window and depth-pass geometry hints require manual HLSL and raster review before mapping",
            "frames": frames}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--mapping", required=True, type=Path)
    parser.add_argument("--frame", type=int)
    parser.add_argument("--before-draw", type=int, help="exclusive draw limit; default scans the complete frame")
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args(argv)
    if args.output.exists():
        parser.error("output already exists")
    if args.input.is_dir() and args.output.resolve().is_relative_to(args.input.resolve()):
        parser.error("output must be outside input capture")
    try:
        result = analyze(args.input, args.mapping, args.frame, args.before_draw)
    except (OSError, ValueError, KeyError) as exc:
        parser.error(str(exc))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
