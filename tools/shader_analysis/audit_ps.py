"""Static translated PS texture/depth data-flow triage on explicit HLSL inputs.

Executes source lines in order, not branches/loops; findings are candidates only.
No runtime resource binding, depth identity, or pass inference is performed.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re

try:
    from .audit_vs import normalize_constants
except ImportError:  # Direct CLI invocation.
    from audit_vs import normalize_constants


TOKEN = re.compile(r"\b(r\d+|i\d+|xePV|ps)(?:\.([xyzw]{1,4}))?\b")
ASSIGN = re.compile(r"^\s*(r\d+|xePV|ps|oC\d+|oDepthVec)(?:\.([xyzw]{1,4}))?\s*=\s*(.*);\s*$")
FETCH = re.compile(r"XeTex\w*\(tex(?:2D|3D|Cube)_(\d+),\s*XeSampler\(\d+u\),\s*(r\d+(?:\.[xyzw]+)?)")
CLIP_CALL = re.compile(r"\b([A-Za-z_]\w*)\s*\(")
CLIP_KNOWN_CALLS = {
    "abs", "clamp", "cos", "dot", "exp2", "float", "float2", "float3", "float4",
    "frac", "log2", "max", "min", "rcp", "rsqrt", "saturate", "sin", "sqrt",
    "XeConst", "XeSampler", "XeTex2D", "XeTex2DLevelZero", "XeTex3D",
    "XeTex3DLevelZero", "XeTexCube", "XeTexCubeLevelZero", "XeTextureResult",
}


def analyze_clip_reads(text, explicit_clip_inputs):
    """Conservative source-order review; a true candidate is not mapping authorization."""
    clips = sorted(set(explicit_clip_inputs))
    if not clips or any(not isinstance(n, int) or isinstance(n, bool) or not 0 <= n <= 15 for n in clips):
        raise ValueError("clip inputs must be explicit TEXCOORD indices from 0 to 15")
    main = re.search(r"\bvoid\s+main\s*\(", text)
    if not main:
        raise ValueError("translated PS main is missing")
    opening = text.find("{", main.end())
    closing = text.rfind("}")
    if opening < 0 or closing <= opening:
        raise ValueError("translated PS main body is incomplete")
    first_line = text[:opening].count("\n") + 1
    state = {f"i{n}": [{f"i{n}.{component}"} for component in "xyzw"] for n in clips}
    xy_reads, w_reads, unsupported = [], [], []

    def dependencies(expression):
        result = set()
        for match in TOKEN.finditer(expression):
            name, swizzle = match.groups()
            values = state.get(name, [set() for _ in "xyzw"])
            for component in swizzle or ("x" if name == "ps" else "xyzw"):
                result |= values["xyzw".index(component)]
        return result

    for offset, original in enumerate(text[opening + 1:closing].splitlines(), 1):
        line_no = first_line + offset - 1
        line = original.split("//", 1)[0].strip()
        if not line or line in ("{", "}"):
            continue
        if re.search(r"\b(if|else|for|while|switch|do|discard|return|break|continue)\b|\?", line):
            unsupported.append({"line": line_no, "reason": "main control flow", "text": line})
        declaration = re.fullmatch(r"(?:float|float[234])\s+(r\d+|xePV|ps|xeDbgTex)\s*=\s*(?:0(?:\.0)?|float[234]\(0(?:\.0)?\))\s*;", line)
        if declaration:
            state[declaration[1]] = [set() for _ in "xyzw"]
            continue
        match = ASSIGN.match(line)
        if not match:
            unsupported.append({"line": line_no, "reason": "unparsed assignment or operation", "text": line})
            continue
        target, swizzle, expression = match.groups()
        # The translator initializes rN from iN before guest instructions.
        # Preserve lanes exactly: treating this as a union would make a W-only
        # read appear to consume clip X/Y.
        direct_input = re.fullmatch(r"(i\d+)(?:\.([xyzw]{1,4}))?", expression.strip())
        if direct_input and target.startswith("r") and not swizzle and not direct_input[2]:
            state[target] = [set(v) for v in state.get(direct_input[1], [set() for _ in "xyzw"])]
            continue
        deps = dependencies(expression)
        xy = sorted(d for d in deps if d.endswith((".x", ".y")))
        w = sorted(d for d in deps if d.endswith(".w"))
        if xy:
            xy_reads.append({"line": line_no, "inputs": xy, "text": line})
        if w:
            w_reads.append({"line": line_no, "inputs": w, "text": line})
        unknown_calls = sorted(set(CLIP_CALL.findall(expression)) - CLIP_KNOWN_CALLS)
        if unknown_calls:
            unsupported.append({"line": line_no, "reason": "unknown call: " + ", ".join(unknown_calls), "text": line})
        if re.search(r"<<|>>|[&|^~%]|==|!=|<=|>=|(?<![<>=])[<>](?![<>=])", expression):
            unsupported.append({"line": line_no, "reason": "unknown expression operator", "text": line})
        values = state.setdefault(target, [set() for _ in "xyzw"])
        for component in swizzle or ("x" if target == "ps" else "xyzw"):
            values["xyzw".index(component)] = deps.copy()
    return {"clip_inputs": clips, "clip_xy_reads": xy_reads, "clip_w_reads": w_reads,
            "unsupported": unsupported,
            "candidate_no_clip_xy_reads": not xy_reads and not unsupported,
            "limits": "Conservative source-order taint; requires manual VS output and PS review; never authorizes runtime mapping."}


def analyze(path, clip_inputs=None):
    text = path.read_text(encoding="utf-8-sig")
    if "void main" not in text or "SV_Target" not in text:
        return None
    start = text.index("void main")
    first_line = text[:start].count("\n") + 1
    body = text[start:]
    registers = {f"i{i}": [{f"i{i}.{component}"} for component in "xyzw"] for i in range(16)}

    def deps(expression):
        result = set()
        for match in TOKEN.finditer(expression):
            name, swizzle = match.groups()
            values = registers.get(name, [set() for _ in range(4)])
            for component in swizzle or ("x" if name == "ps" else "xyzw"):
                result |= values["xyzw".index(component)]
        result |= {"c" + n for n in re.findall(r"c\[(\d+)\]", normalize_constants(expression))}
        return result

    fetches, inverses, depth_outputs = [], [], []
    for offset, line in enumerate(body.splitlines()):
        match = ASSIGN.match(line)
        if not match:
            continue
        name, swizzle, expression = match.groups()
        swizzle = swizzle or ("x" if name == "ps" else "xyzw")
        dependencies = deps(expression)
        if fetch := FETCH.search(expression):
            slot, coordinate = int(fetch[1]), fetch[2]
            coordinate_deps = deps(coordinate)
            fetches.append({"line": first_line + offset, "slot": slot, "coordinate": coordinate,
                            "coordinate_dependencies": sorted(coordinate_deps), "expression": expression})
            dependencies |= {f"sample{slot}"}
        if "rcp(" in expression:
            sampled = sorted(d for d in dependencies if d.startswith("sample"))
            inverses.append({"line": first_line + offset, "dependencies": sorted(dependencies),
                             "sample_sources": sampled, "expression": expression})
            dependencies |= {f"inverse_{source}" for source in sampled}
        if name == "oDepthVec":
            depth_outputs.append({"line": first_line + offset, "dependencies": sorted(dependencies),
                                  "expression": expression})
        values = registers.setdefault(name, [set() for _ in range(4)])
        for component in swizzle:
            values["xyzw".index(component)] = dependencies.copy()
    indirect = [f for f in fetches if any(d.startswith("sample") for d in f["coordinate_dependencies"])]
    inverse_indirect = [f for f in indirect if any(d.startswith("inverse_sample") for d in f["coordinate_dependencies"])]
    result = {"hash": path.stem.removeprefix("ps_"), "path": str(path),
            "hlsl_sha256": hashlib.sha256(text.encode()).hexdigest(),
            "texture_slots": sorted({f["slot"] for f in fetches}), "fetches": fetches,
            "reciprocals": inverses, "dependent_read_count": len(indirect),
            "inverse_sample_dependent_reads": inverse_indirect, "depth_outputs": depth_outputs,
            "branches_or_loops": bool(re.search(r"\b(if|for|while)\s*\(", body)),
            "dynamic_constants": bool(re.search(r"\bXeConst\s*\(|\bc\[(?!\d+\])", normalize_constants(body))),
            "candidate_inverse_sample_projection": bool(inverse_indirect), "triage_only": True}
    if clip_inputs:
        result["clip_input_review"] = analyze_clip_reads(text, clip_inputs)
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--hlsl", action="append", required=True, type=Path,
                        help="Explicit PS HLSL file or nonrecursive directory; repeatable")
    parser.add_argument("--output", required=True, type=Path, help="New JSON report")
    parser.add_argument("--clip-input", action="append", type=int, metavar="N",
                        help="Explicit copied clip TEXCOORD index 0..15; repeatable; conservative review only")
    args = parser.parse_args(argv)
    try:
        if args.clip_input and any(not 0 <= n <= 15 for n in args.clip_input):
            raise ValueError("--clip-input must be in 0..15")
        if args.output.exists():
            raise ValueError(f"output already exists: {args.output}")
        paths = []
        for source in args.hlsl:
            if source.is_dir():
                paths.extend(sorted(source.glob("ps_*.hlsl")))
            elif source.is_file() and source.name.startswith("ps_") and source.suffix == ".hlsl":
                paths.append(source)
            else:
                raise ValueError(f"missing or non-PS HLSL input: {source}")
        if not paths:
            raise ValueError("no PS HLSL in supplied inputs")
        rows = []
        for path in paths:
            item = analyze(path, args.clip_input)
            if item is None:
                raise ValueError(f"no translated PS entry or SV_Target: {path}")
            rows.append(item)
        summary = {"no_texture_reads": sum(not r["texture_slots"] for r in rows),
                   "projective_reciprocals": sum(bool(r["reciprocals"]) for r in rows),
                   "dependent_read_shaders": sum(bool(r["dependent_read_count"]) for r in rows),
                   "inverse_sample_projection_candidates": [r["path"] for r in rows if r["candidate_inverse_sample_projection"]],
                   "depth_output_shaders": [r["path"] for r in rows if r["depth_outputs"]],
                   "with_control_flow": sum(r["branches_or_loops"] for r in rows)}
        report = {"hlsl_inputs": list(map(str, args.hlsl)), "pixel_shader_count": len(rows),
                  "summary": summary, "shaders": rows,
                  "limits": "Static source-order taint triage only; branch/loop behavior and runtime binding identity unresolved."}
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(json.dumps({"count": len(rows), "summary": summary}))
        return 0
    except (OSError, ValueError, UnicodeError) as error:
        parser.exit(1, f"error: {error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
