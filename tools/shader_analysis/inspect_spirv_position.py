"""Trace arithmetic feeding directly decorated SPIR-V BuiltIn Position stores.

Uses a caller-supplied SPIR-V core grammar; no compiler, disassembler, or
vendor cache is needed. Indirect/member decorations and dynamic control flow
require separate review. SPIR-V may be wrapped in a cache header.
"""

import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import struct


MAGIC = struct.pack("<I", 0x07230203)


def analyze(data, grammar):
    offset = data.find(MAGIC)
    if offset < 0:
        raise ValueError("SPIR-V magic not found")
    raw = data[offset:]
    if len(raw) < 20 or len(raw) % 4:
        raise ValueError("truncated SPIR-V words")
    words = struct.unpack(f"<{len(raw)//4}I", raw)
    if words[3] == 0:
        raise ValueError("invalid SPIR-V id bound")
    entries, definitions = [], {}
    decorations = defaultdict(list)
    member_positions = []
    stores = defaultdict(list)
    i = 5
    while i < len(words):
        length, opcode = words[i] >> 16, words[i] & 65535
        if length == 0 or i + length > len(words):
            raise ValueError(f"invalid instruction at word {i}")
        if opcode not in grammar:
            raise ValueError(f"opcode {opcode} missing from supplied grammar")
        operation = grammar[opcode]
        name = operation["opname"]
        operands = list(words[i+1:i+length])
        result_id, refs, cursor = None, [], 0
        for operand in operation.get("operands", []):
            kind, quantifier = operand["kind"], operand.get("quantifier")
            if cursor >= len(operands):
                break
            if kind == "IdResult":
                result_id = operands[cursor]
            elif kind == "IdRef":
                refs.extend(operands[cursor:] if quantifier == "*" else operands[cursor:cursor+1])
            elif kind == "PairIdRefIdRef":
                refs.extend(operands[cursor:])
            if quantifier == "*":
                break
            # Strings occupy as many words as needed to reach the first NUL.
            if kind == "LiteralString":
                while cursor < len(operands) and 0 not in struct.pack("<I", operands[cursor]):
                    cursor += 1
            cursor += 1
        entry = {"word": i, "op": name, "operands": operands, "id": result_id, "refs": refs}
        entries.append(entry)
        if result_id is not None:
            definitions[result_id] = entry
        if name == "OpDecorate" and len(operands) >= 2:
            decorations[operands[0]].append(operands[1:])
        if name == "OpMemberDecorate" and len(operands) >= 4 and operands[2:4] == [11, 0]:
            member_positions.append({"struct_id": operands[0], "member": operands[1]})
        if name == "OpStore" and len(operands) >= 2:
            stores[operands[0]].append(operands[1])
        i += length
    position = sorted(r for r, d in decorations.items() if [11, 0] in d)
    pending = [value for r in position for value in stores[r]]
    seen = set()
    while pending:
        ref = pending.pop()
        if ref in seen:
            continue
        seen.add(ref)
        pending.extend(stores[ref])
        if ref in definitions:
            pending.extend(definitions[ref]["refs"])
    arithmetic = [{"word": e["word"], "op": e["op"], "id": e["id"],
                   "operands": e["operands"], "noContraction": [42] in decorations.get(e["id"], [])}
                  for e in entries if e["id"] in seen and (e["op"].startswith("OpF") or e["op"] in
                                                             ("OpDot", "OpVectorTimesScalar", "OpExtInst"))]
    return {"cacheSha256": hashlib.sha256(data).hexdigest(), "offset": offset,
            "position": position, "member_positions_untraced": member_positions,
            "invariantIds": sorted(r for r, d in decorations.items() if [18] in d),
            "noContractionCount": sum([42] in d for d in decorations.values()),
            "positionArithmetic": arithmetic,
            "allArithmeticCounts": dict(Counter(e["op"] for e in entries if e["op"] in
                                                ("OpDot", "OpFAdd", "OpFMul", "OpExtInst")))}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--spirv", action="append", required=True, type=Path,
                        help="Explicit .spv file or nonrecursive directory; repeatable")
    parser.add_argument("--grammar", required=True, type=Path, help="SPIR-V core JSON grammar")
    parser.add_argument("--output", required=True, type=Path, help="New JSON report")
    args = parser.parse_args(argv)
    try:
        if args.output.exists():
            raise ValueError(f"output already exists: {args.output}")
        grammar = {x["opcode"]: x for x in json.loads(args.grammar.read_text(encoding="utf-8"))["instructions"]}
        paths = []
        for source in args.spirv:
            if source.is_dir():
                paths.extend(sorted(source.glob("*.spv")))
            elif source.is_file() and source.suffix == ".spv":
                paths.append(source)
            else:
                raise ValueError(f"missing or non-SPIR-V input: {source}")
        if not paths:
            raise ValueError("no SPIR-V files in supplied inputs")
        rows = []
        for path in paths:
            try:
                rows.append({"path": str(path), **analyze(path.read_bytes(), grammar)})
            except ValueError as error:
                raise ValueError(f"{path}: {error}") from error
        report = {"spirv_inputs": list(map(str, args.spirv)), "grammar": str(args.grammar),
                  "shaders": rows,
                  "limits": "Only directly decorated BuiltIn Position stores are traced; member-decorated interface blocks and control flow are untraced. No numerical equivalence is established."}
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(json.dumps({"count": len(rows), "direct_position_stores": sum(bool(r["position"]) for r in rows)}))
        return 0
    except (OSError, ValueError, KeyError, TypeError, UnicodeError) as error:
        parser.exit(1, f"error: {error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
