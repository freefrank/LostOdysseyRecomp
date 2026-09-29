"""Inspect generated SDK control flow, not game assets or a generated-code export.

Run canonical tools/ppc_codegen.py first. Output is ignored experimental evidence;
static callers are not dynamic draw frequency or runtime coverage.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import re
import subprocess


def inspect(root: Path, output: Path, seeds: set[str]) -> None:
    functions: dict[str, dict] = {}
    callers: list[dict] = []
    for path in sorted((root / "LostOdysseyRecompLib/ppc").glob("ppc_recomp.*.cpp")):
        text = path.read_text(encoding="utf-8")
        for match in re.finditer(r"PPC_FUNC_IMPL\(__imp__sub_([0-9A-Fa-f]{8})\) \{.*?\n\}", text, re.S):
            address = match[1].upper()
            body = match[0]
            calls = {c.upper() for c in re.findall(r"\bsub_([0-9A-Fa-f]{8})\(ctx, base\)", body)}
            number = int(address, 16)
            sdk = 0x823B0000 <= number < 0x823F0000 or 0x82790000 <= number < 0x827D0000
            if not sdk and not calls & seeds:
                continue
            line = text.count("\n", 0, match.start()) + 1
            identity = {"address": address, "file": str(path.relative_to(root)), "line": line}
            if calls & seeds:
                callers.append(dict(identity, calls=sorted(calls & seeds)))
            if not sdk:
                continue
            flow = [[line + offset, source.strip()] for offset, source in enumerate(body.splitlines())
                    if source.strip().startswith(("// ", "loc_"))]
            functions[address] = dict(identity, calls=sorted(calls), control_flow=flow)
    selected = seeds | {c["address"] for c in callers if c["address"] in functions}
    for _ in range(2):
        selected |= {callee for address in list(selected)
                     for callee in functions.get(address, {}).get("calls", []) if callee in functions}
    if not seeds <= functions.keys():
        raise RuntimeError(f"Missing SDK seed functions: {seeds - functions.keys()}")
    output.mkdir(parents=True, exist_ok=False)
    metadata = {"source": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
                "seeds": sorted(seeds), "selected": sorted(selected), "callers": callers,
                "dynamic_frequency": "not measured"}
    (output / "index.json").write_text(json.dumps(metadata, indent=2), encoding="utf-8")
    for address in sorted(selected):
        if address in functions:
            (output / f"{address}.json").write_text(json.dumps(functions[address], indent=2), encoding="utf-8")
    print(f"Extracted {len(selected)} bounded SDK contracts; no game assets exported.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--seed", action="append", default=[])
    args = parser.parse_args()
    seeds = {s.removeprefix("0x").upper() for s in args.seed} or {
        "823C6468", "823C6860", "827B56B0", "823C6CB8", "823C78E0", "823C7A40"}
    if any(not re.fullmatch(r"[0-9A-F]{8}", s) for s in seeds):
        parser.error("Seeds must be eight-digit guest SDK function addresses")
    inspect(args.root.resolve(), args.output.resolve(), seeds)
