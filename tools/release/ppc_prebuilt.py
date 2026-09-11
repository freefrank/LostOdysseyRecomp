"""Export, restore and validate a private, source-bound PPC static library bundle."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import ppc_codegen

LIBRARY = "LostOdysseyRecompLib.lib"
CHUNK_SIZE = 40 * 1024 * 1024


def digest(data):
    return hashlib.sha256(data).hexdigest()


def fingerprint(root):
    ppc_codegen.check(root)
    manifest = json.loads((root / "LostOdysseyRecompLib/ppc/codegen-manifest.json").read_text())
    paths = [root / "LostOdysseyRecomp/gpu/ppc_mmio.h"]
    paths += [p for p in (root / "tools/XenonRecomp/thirdparty/simde").rglob("*")
              if p.is_file() and p.suffix in {".h", ".hpp", ".inl"}]
    if len(paths) < 2:
        raise ValueError("Missing simde headers")
    headers = {p.relative_to(root).as_posix(): digest(p.read_bytes()) for p in sorted(paths)}
    cmake = {name: digest((root / name).read_bytes().replace(b"\r\n", b"\n"))
             for name in ("CMakeLists.txt", "LostOdysseyRecompLib/CMakeLists.txt")}
    return {"inputs": manifest["inputs"], "outputs": manifest["outputs"],
            "headers": headers, "cmake": cmake}


def compile_contract(root, build_dir):
    cache = (build_dir / "CMakeCache.txt").read_text()
    if not re.search(r"^CMAKE_BUILD_TYPE:STRING=Release$", cache, re.M):
        raise ValueError("PPC prebuilt requires Release")
    compiler_files = list((build_dir / "CMakeFiles").glob("*/CMakeCXXCompiler.cmake"))
    if len(compiler_files) != 1:
        raise ValueError("Cannot identify C++ compiler")
    compiler = compiler_files[0].read_text()
    def value(name):
        match = re.search(r"set\(" + name + r'\s+"?([^"\s)]+)', compiler)
        return match[1] if match else ""
    if (value("CMAKE_CXX_COMPILER_ID") != "Clang"
            or value("CMAKE_CXX_SIMULATE_ID") != "MSVC"
            or value("CMAKE_CXX_COMPILER_ARCHITECTURE_ID") != "x64"):
        raise ValueError("PPC prebuilt requires clang-cl x64")
    ninja = (build_dir / "build.ninja").read_text()
    contracts = []
    for block in re.split(r"\n(?=build )", ninja):
        first = block.splitlines()[0]
        if not re.search(r": CXX_COMPILER__(?:LostOdysseyRecompLib|LoPpcCompileContract)_", first):
            continue
        if "ppc_recomp." not in first and "ppc_func_mapping.cpp" not in first:
            continue
        fields = {}
        for key in ("FLAGS", "INCLUDES", "DEFINES"):
            found = re.search(r"^  " + key + r" = (.*)$", block, re.M)
            raw = found[1] if found else ""
            raw = raw.replace("$:", ":").replace("$ ", " ").replace("\\", "/")
            raw = re.sub(re.escape(root.as_posix()), lambda _: "$ROOT", raw, flags=re.I)
            fields[key] = raw.strip()
        contracts.append(fields)
    if not contracts or any(c != contracts[0] for c in contracts):
        raise ValueError("Missing or inconsistent PPC compile rules")
    flags = contracts[0]["FLAGS"]
    if not re.search(r"(?:^|\s)[/-]MT(?:\s|$)", flags) or re.search(r"flto|/GL(?:\s|$)", flags, re.I):
        raise ValueError("PPC prebuilt requires non-LTO /MT")
    return {"configuration": "Release", "compiler": "clang-cl", "architecture": "x64",
            "runtime": "MT", "lto": False, **contracts[0]}, value("CMAKE_CXX_COMPILER_VERSION")


def load_manifest(bundle):
    manifest = json.loads((bundle / "manifest.json").read_text())
    if manifest.get("schema") != 1 or manifest["library"]["name"] != LIBRARY:
        raise ValueError("Unsupported PPC bundle manifest")
    chunks = manifest["chunks"]
    if not chunks:
        raise ValueError("Empty PPC bundle")
    for index, chunk in enumerate(chunks):
        if chunk["name"] != f"ppc-{index:04d}.bin" or not 0 < chunk["size"] <= CHUNK_SIZE:
            raise ValueError("Invalid PPC chunk name or size")
    if sum(c["size"] for c in chunks) != manifest["library"]["size"]:
        raise ValueError("PPC bundle size mismatch")
    return manifest


def validate_file(path, expected):
    if path.stat().st_size != expected["size"] or ppc_codegen.digest(path) != expected["sha256"]:
        raise ValueError(f"PPC bundle hash/size mismatch: {path.name}")


def export(root, build_dir, output):
    before = fingerprint(root)
    contract, version = compile_contract(root, build_dir)
    subprocess.run(["cmake", "--build", str(build_dir), "--target", "LostOdysseyRecompLib", "--parallel", "4"], check=True)
    if before != fingerprint(root) or contract != compile_contract(root, build_dir)[0]:
        raise ValueError("PPC inputs changed during build")
    write_bundle_from_built(root, build_dir, output)


def write_bundle_from_built(root, build_dir, output):
    """Freeze an already-built library without invoking the native build again."""
    before = fingerprint(root)
    contract, version = compile_contract(root, build_dir)
    library = build_dir / "LostOdysseyRecompLib" / LIBRARY
    original_stat = library.stat()
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        raise ValueError("Export output must be empty")
    chunks = []
    total = hashlib.sha256()
    size = 0
    with library.open("rb") as stream:
        while data := stream.read(CHUNK_SIZE):
            name = f"ppc-{len(chunks):04d}.bin"
            (output / name).write_bytes(data)
            chunks.append({"name": name, "size": len(data), "sha256": digest(data)})
            total.update(data)
            size += len(data)
    final_stat = library.stat()
    if ((original_stat.st_size, original_stat.st_mtime_ns, original_stat.st_ino)
            != (final_stat.st_size, final_stat.st_mtime_ns, final_stat.st_ino)
            or ppc_codegen.digest(library) != total.hexdigest()
            or before != fingerprint(root) or contract != compile_contract(root, build_dir)[0]):
        raise ValueError("PPC inputs or library changed while exporting")
    manifest = {"schema": 1, "library": {"name": LIBRARY, "size": size, "sha256": total.hexdigest()},
                "chunks": chunks, "fingerprint": before, "contract": contract,
                "provenance": {"compiler_version": version}}
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


def restore(bundle, output):
    manifest = load_manifest(bundle)
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ppc-restore-", dir=output) as temporary:
        library = Path(temporary) / LIBRARY
        with library.open("wb") as stream:
            for chunk in manifest["chunks"]:
                source = bundle / chunk["name"]
                if source.is_symlink():
                    raise ValueError("PPC chunk cannot be a symlink")
                validate_file(source, chunk)
                stream.write(source.read_bytes())
        validate_file(library, manifest["library"])
        library.replace(output / LIBRARY)
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


def check(root, bundle, build_dir=None):
    manifest = load_manifest(bundle)
    validate_file(bundle / LIBRARY, manifest["library"])
    if manifest["fingerprint"] != fingerprint(root):
        raise ValueError("PPC prebuilt inputs/outputs changed; export a fresh bundle")
    if build_dir and manifest["contract"] != compile_contract(root, build_dir)[0]:
        raise ValueError("PPC prebuilt compile contract changed; export a fresh bundle")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    for name in ("export", "restore", "check"):
        command = commands.add_parser(name)
        if name != "restore":
            command.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
            command.add_argument("--build-dir", type=Path, required=name == "export")
        if name != "export":
            command.add_argument("--bundle", type=Path, required=True)
        if name != "check":
            command.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.command == "export":
            export(args.root.resolve(), args.build_dir.resolve(), args.output.resolve())
        elif args.command == "restore":
            restore(args.bundle.resolve(), args.output.resolve())
        else:
            check(args.root.resolve(), args.bundle.resolve(), args.build_dir.resolve() if args.build_dir else None)
    except (OSError, ValueError, KeyError, TypeError, subprocess.CalledProcessError) as error:
        print(f"PPC prebuilt {args.command} failed: {error}", file=sys.stderr)
        return 1
    print(f"PPC prebuilt {args.command}: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
