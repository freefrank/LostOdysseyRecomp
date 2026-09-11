"""Generate PPC sources with an input/output manifest, or reject stale sources.

Requires Python 3.11+. Run build_tools.bat before generate. The executable
receipt records binary and source hashes after a successful tool build.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import tomllib

MANIFEST = "codegen-manifest.json"
RECOVERY = "Run tools/build_tools.bat, then python tools/ppc_codegen.py generate."


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def snapshot(root, paths):
    return {p.relative_to(root).as_posix(): digest(p) for p in sorted(set(paths))}


def tool_inputs(root):
    generator = root / "tools/XenonRecomp"
    return [root / "tools/xexdump/CMakeLists.txt", root / "tools/build_tools.bat"] + [p for p in generator.rglob("*") if p.is_file() and
            (p.suffix in {".cpp", ".c", ".h", ".hpp", ".inl", ".cmake"}
             or p.name == "CMakeLists.txt")]


def layout(root):
    config = root / "LostOdysseyRecompLib/config/LostOdysseyRecomp.toml"
    main = tomllib.loads(config.read_text(encoding="utf-8"))["main"]
    output = (config.parent / main["out_directory_path"]).resolve()
    if output != root / "LostOdysseyRecompLib/ppc":
        raise ValueError("Unexpected PPC output directory")
    inputs = list(config.parent.rglob("*.toml"))
    inputs.append((config.parent / main["file_path"]).resolve())
    for key in ("patch_file_path", "switch_table_file_path"):
        if main.get(key):
            inputs.append((config.parent / main[key]).resolve())
    sources = tool_inputs(root)
    inputs += sources
    inputs += [root / "tools/ppc_codegen.py"]
    return config, output, inputs, sources


def outputs(root, output):
    files = [p for p in output.iterdir() if p.suffix in {".cpp", ".h"}]
    if not any(p.name.startswith("ppc_recomp.") for p in files) or not (output / "ppc_func_mapping.cpp").exists():
        raise ValueError("Missing generated PPC sources")
    for name in ("ppc_config.h", "ppc_context.h", "ppc_recomp_shared.h"):
        if not (output / name).is_file():
            raise ValueError(f"Missing generated header: {name}")
    expected_context = '#pragma once\n#include "ppc_config.h"\n\n' + (
        root / "tools/XenonRecomp/XenonUtils/ppc_context.h").read_text(encoding="utf-8")
    if (output / "ppc_context.h").read_text(encoding="utf-8") != expected_context:
        raise ValueError("Generated PPC context does not match generator context")
    result = {}
    for path in sorted(files):
        data = path.read_bytes()
        if path.suffix == ".cpp" and re.search(rb"switch\s*\(\s*ctx\.r\d+\.u64\s*\)", data):
            raise ValueError(f"Obsolete 64-bit jump-table switch in {path.name}")
        result[path.relative_to(root).as_posix()] = hashlib.sha256(data).hexdigest()
    return result


def check(root):
    _, output, inputs, _ = layout(root)
    manifest = json.loads((output / MANIFEST).read_text(encoding="utf-8"))
    if manifest.get("schema") != 1:
        raise ValueError("Unsupported codegen manifest")
    if manifest["inputs"] != snapshot(root, inputs):
        raise ValueError("PPC generation inputs changed")
    if manifest["outputs"] != outputs(root, output):
        raise ValueError("Generated PPC sources changed")


def stamp_tool(root, executable):
    sources = tool_inputs(root)
    receipt = {"binary": digest(executable), "sources": snapshot(root, sources)}
    executable.with_suffix(".codegen.json").write_text(json.dumps(receipt, indent=2), encoding="utf-8")


def generate(root, executable):
    config, output, inputs, sources = layout(root)
    binary_hash = digest(executable)
    stamp = json.loads(executable.with_suffix(".codegen.json").read_text(encoding="utf-8"))
    if stamp != {"binary": binary_hash, "sources": snapshot(root, sources)}:
        raise ValueError("XenonRecomp tool build receipt does not match its binary/source inputs")
    before = snapshot(root, inputs)
    # Invalidate the old receipt even if the generator fails halfway through.
    (output / MANIFEST).unlink(missing_ok=True)
    output.mkdir(parents=True, exist_ok=True)
    # The generator skips identical existing files and can return zero on errors.
    # Give it an empty generated-file set, retaining the previous set for rollback.
    with tempfile.TemporaryDirectory(prefix="ppc-codegen-", dir=output.parent) as temporary:
        backup = Path(temporary)
        old_files = [p for p in output.iterdir() if p.suffix in {".cpp", ".h"}]
        moved = []
        try:
            for path in old_files:
                path.rename(backup / path.name)
                moved.append(path.name)
            subprocess.run([str(executable), str(config),
                            str(root / "tools/XenonRecomp/XenonUtils/ppc_context.h")],
                           cwd=root, check=True)
            if before != snapshot(root, inputs) or binary_hash != digest(executable):
                raise ValueError("Generation inputs or executable changed while generating")
            receipt = {"schema": 1, "inputs": before, "outputs": outputs(root, output),
                       "generator_sha256": binary_hash}
            (output / MANIFEST).write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
        except BaseException:
            for name in moved:
                (output / name).unlink(missing_ok=True)
                (backup / name).rename(output / name)
            (output / MANIFEST).unlink(missing_ok=True)
            raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=["check", "generate", "stamp-tool"])
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--executable", type=Path)
    args = parser.parse_args()
    root = args.root.resolve()
    try:
        if args.command == "check":
            check(root)
        else:
            operation = stamp_tool if args.command == "stamp-tool" else generate
            operation(root, (args.executable or root / "out/build/tools/XenonRecomp/XenonRecomp/XenonRecomp.exe").resolve())
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        print(f"PPC codegen check failed: {error}\n{RECOVERY}", file=sys.stderr)
        return 1
    print(f"PPC codegen {args.command}: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
