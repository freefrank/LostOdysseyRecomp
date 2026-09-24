"""Conservative static guest oPos def-use triage of explicitly supplied VS HLSL.

Control flow is unresolved; a projection slot candidate does not authorize a
jitter whitelist or establish camera/pass/viewport identity.
"""

import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import re


ASSIGN = re.compile(r"^(?:(?:float[1-4]?|uint|int|bool)\s+)?(r\d+|xePV|ps|oPos|a0|aL)(?:\.([xyzw]+))?\s*(=|\+=|\*=|-=|/=)\s*(.*);$")
REF = re.compile(r"\b(r\d+|xePV|ps|oPos|a0|aL)(?:\.([xyzw]+))?\b")
CONST = re.compile(r"\bc\[(\d+)([^\]]*)\](?:\.([xyzw]+))?")
ROW = re.compile(r"(?:\*\s*c\[(\d+)\]\.[xyzw]{4}|c\[(\d+)\]\.[xyzw]{4}\s*\*|dot\([^;]*?c\[(\d+)\]\.[xyzw]{4})")


def analyze(text):
    lines = text.splitlines()
    start = next((i for i, line in enumerate(lines) if "r0.x = float(xeVertexId);" in line), None)
    if start is None:
        return {"classification": "unresolved_no_guest_entry", "slots": [], "constants": [], "slice": []}
    end = next((i for i in range(start, len(lines)) if "if ((xeFlags & 8u)" in lines[i]), len(lines))
    body = lines[start:end]
    control = [start+i+1 for i, line in enumerate(body) if re.search(r"\b(if|for|while|switch)\s*\(", line)]
    nodes, state, writes, unknown = [], {}, [], []
    for index, line in enumerate(body):
        stripped = line.strip()
        match = ASSIGN.fullmatch(stripped)
        if not match:
            if re.search(r"\boPos\b.*=", stripped):
                unknown.append(start+index+1)
            continue
        lhs, mask, op, rhs = match.groups()
        mask = mask or ("x" if lhs in ("ps", "a0", "aL") else "xyzw")
        references = list(REF.finditer(rhs))
        constants = [(int(m[1]), bool(m[2].strip())) for m in CONST.finditer(rhs)]
        mixed = bool(re.search(r"\b(dot|normalize|length|distance|cross|mul)\s*\(", rhs))
        fetched = "XeVF_" in rhs
        updates = {}
        for component_index, component in enumerate(mask):
            dependencies = set()
            if op != "=":
                dependencies.update(state.get((lhs, component), set()))
            if not fetched:
                for ref in references:
                    var, swizzle = ref.groups()
                    swizzle = swizzle or ("x" if var in ("ps", "a0", "aL") else "xyzw")
                    used = swizzle if mixed else swizzle[component_index % len(swizzle)] if len(swizzle) == len(mask) or len(swizzle) == 1 else swizzle
                    for c in used:
                        dependencies.update(state.get((var, c), set()))
            node = dict(line=start+index+1, text=stripped, deps=dependencies, constants=constants,
                        fetched=fetched, component=component, rhs=rhs)
            node_id = len(nodes)
            nodes.append(node)
            updates[(lhs, component)] = {node_id} | (state.get((lhs, component), set()) if control else set())
            if lhs == "oPos" and index > 1:
                writes.append(node_id)
        state.update(updates)

    def closure(seeds):
        found, pending = set(), list(seeds)
        while pending:
            n = pending.pop()
            if n not in found:
                found.add(n)
                pending.extend(nodes[n]["deps"])
        return found

    final = set().union(*(state.get(("oPos", c), set()) for c in "xyzw"))
    ids = closure(final)
    wids = closure(state.get(("oPos", "w"), set()))
    constants = sorted({value for n in ids for value, relative in nodes[n]["constants"] if not relative})
    relative = any(relative for n in ids for value, relative in nodes[n]["constants"]) or any("XeConst(" in nodes[n]["rhs"] for n in ids)
    wconstants = sorted({value for n in wids for value, relative in nodes[n]["constants"] if not relative})
    source_lines = {nodes[n]["line"]: nodes[n]["text"] for n in ids}
    rows = defaultdict(list)
    for line, statement in sorted(source_lines.items()):
        for match in ROW.finditer(statement):
            rows[int(next(g for g in match.groups() if g is not None))].append(line)
    blocks = []
    for slot in sorted(rows):
        if all(slot+i in rows for i in range(4)):
            used = [max(rows[slot+i]) for i in range(4)]
            blocks.append(dict(slot=slot, lines=used, last=min(used), in_w=all(slot+i in wconstants for i in range(4))))
    last = max((block["last"] for block in blocks), default=-1)
    final_blocks = [block for block in blocks if block["last"] == last]
    slots = sorted({block["slot"] for block in final_blocks})
    if unknown or control:
        classification = "unresolved_control_flow"
    elif slots and any(block["in_w"] for block in final_blocks):
        classification = "matrix_position_candidate"
    elif slots:
        classification = "matrix_position_constant_w"
    elif not constants and any(nodes[n]["fetched"] for n in ids):
        classification = "direct_vertex_position"
    elif not constants:
        classification = "constant_or_vertex_id_position"
    else:
        classification = "other_position_expression"
    return dict(classification=classification, slots=slots, constants=constants,
                w_constants=wconstants, relative_position_constants=relative, control_lines=control,
                unparsed_position_writes=unknown, matrix_blocks=blocks,
                position_write_lines=sorted({nodes[n]["line"] for n in writes}),
                slice=[dict(line=line, text=statement) for line, statement in sorted(source_lines.items())])


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--hlsl", action="append", required=True, type=Path,
                        help="Explicit VS HLSL file or nonrecursive directory; repeatable")
    parser.add_argument("--output", required=True, type=Path, help="New JSON report")
    args = parser.parse_args(argv)
    try:
        if args.output.exists():
            raise ValueError(f"output already exists: {args.output}")
        paths = []
        for source in args.hlsl:
            if source.is_dir():
                paths.extend(sorted(source.glob("vs_*.hlsl")))
            elif source.is_file() and source.name.startswith("vs_") and source.suffix == ".hlsl":
                paths.append(source)
            else:
                raise ValueError(f"missing or non-VS HLSL input: {source}")
        if not paths:
            raise ValueError("no VS HLSL in supplied inputs")
        rows = [{"shader": path.stem[3:], "path": str(path), **analyze(path.read_text(encoding="utf-8-sig"))}
                for path in paths]
        report = {"hlsl_inputs": list(map(str, args.hlsl)), "summary": dict(Counter(r["classification"] for r in rows)),
                  "shaders": rows, "limits": "Static def-use only; control flow unresolved; no candidate authorizes jitter without observed camera/pass/viewport agreement."}
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(json.dumps({"count": len(rows), "classifications": report["summary"]}))
        return 0
    except (OSError, ValueError, UnicodeError) as error:
        parser.exit(1, f"error: {error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
