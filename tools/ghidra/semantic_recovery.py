"""Check pinned PPC bodies and run selected semantic oracles in a shared batch.

The tracked family manifest is the pin. No generator, source scan, or hash cache is
used to decide whether an original body is still the selected one.
"""

from __future__ import annotations

import argparse
import hashlib
from collections import Counter
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

from semantic_batch import ROOT, compile_and_run, compiler_environment

SEMANTICS = ROOT / "LostOdysseyRecompSemantics"
DEFAULT_PPC = Path.home() / "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc"
ADDRESS = re.compile(r"[0-9A-Fa-f]{8}\Z")
SOURCE = re.compile(r"ppc_recomp\.\d+\.cpp\Z")
SUITE = re.compile(r"[A-Za-z0-9_-]+\Z")
BODY_LABEL = re.compile(r"^loc_([0-9A-Fa-f]{8}):$", re.M)
DIRECT_SYMBOL = re.compile(r"(?<![\w])sub_([0-9A-Fa-f]{8})\s*\(")
NATIVE_SYMBOL = re.compile(r"(?<![\w])__imp__([A-Za-z][A-Za-z0-9_]*)\s*\(")
BRANCH_TARGET = re.compile(r"^b\w*\s+(?:cr\d+,)?0x([0-9A-Fa-f]{8})$", re.I)


def _json(path: Path) -> dict:
    document = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(document, dict):
        raise ValueError(f"expected JSON object: {path}")
    return document


def _write(path: Path, data: dict) -> None:
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def _source_name(entry: dict) -> str:
    raw = entry.get("source", entry.get("generated_ppc_path"))
    if not isinstance(raw, str):
        raise ValueError("entry needs source or generated_ppc_path")
    parts = raw.replace("\\", "/").split("/")
    if parts[:-1] not in ([], ["LostOdysseyRecompLib", "ppc"]):
        raise ValueError(f"unexpected PPC source path: {raw}")
    name = parts[-1]
    if not SOURCE.fullmatch(name):
        raise ValueError(f"unexpected PPC source name: {raw}")
    return name


def _line_number(entry: dict) -> int:
    values = [entry[k] for k in ("source_line", "line") if k in entry]
    if not values or any(type(v) is not int or v < 1 for v in values) or len(set(values)) != 1:
        raise ValueError(f"invalid source line for {entry.get('address')}: {values}")
    return values[0]


def _metadata(entry: dict, body: str) -> None:
    address = entry["address"]
    comments = [line.split("// ", 1)[1].rstrip() for line in body.splitlines()
                if re.match(r"^\s*// ", line)]
    instructions = entry.get("instructions", entry.get("instruction_sequence"))
    # Digest pins keep private generated code out of public manifests. The
    # verified complete body supplies the instruction sequence in that mode.
    if instructions is None and "body_sha256" in entry:
        if entry.get("instruction_count") != len(comments):
            raise ValueError(f"instruction count changed: {address}")
    else:
        if not isinstance(instructions, list) or any(not isinstance(x, str) for x in instructions):
            raise ValueError(f"missing instruction list: {address}")
        if comments != [x.rstrip() for x in instructions]:
            raise ValueError(f"instruction sequence changed: {address}")
        if "instruction_count" in entry and entry["instruction_count"] != len(instructions):
            raise ValueError(f"instruction count changed: {address}")

    cfg = entry.get("cfg")
    if cfg is not None:
        if isinstance(cfg, dict):
            labels = cfg.get("labels")
            branches = cfg.get("branches", cfg.get("branch_instructions"))
        elif isinstance(cfg, list):
            labels, branches = None, cfg
        else:
            raise ValueError(f"unsupported CFG: {address}")
        if labels is not None:
            expected = [str(x).removeprefix("loc_").upper() for x in labels]
            actual = [x.upper() for x in BODY_LABEL.findall(body)]
            if expected != actual:
                raise ValueError(f"CFG labels changed: {address}")
        if branches is not None:
            if not isinstance(branches, list):
                raise ValueError(f"invalid CFG branches: {address}")
            remaining = iter(comments)
            for branch in branches:
                if not isinstance(branch, str) or not any(
                    candidate.lower() == branch.rstrip().lower() for candidate in remaining
                ):
                    raise ValueError(f"CFG branch changed: {address}: {branch}")

    direct = next((entry[key] for key in ("direct_calls", "calls", "callees") if key in entry), None)
    if direct is not None:
        if not isinstance(direct, list) or any(not isinstance(x, str) or not ADDRESS.fullmatch(x)
                                                  for x in direct):
            raise ValueError(f"invalid direct calls: {address}")
        declared = Counter(x.upper() for x in direct)
        # A few older manifests name ABI helper branch addresses instead of C++
        # helper symbols. Each declared address must still be a PPC branch target.
        targets = Counter(match.group(1).upper() for instruction in comments
                          if (match := BRANCH_TARGET.fullmatch(instruction)))
        if declared - targets:
            raise ValueError(f"direct call target changed: {address}")
        symbols = Counter(x.upper() for x in DIRECT_SYMBOL.findall(body))
        # Some older manifests omit tail branches from direct_calls. Permit
        # only that recorded convention; the complete body is still pinned.
        tail = Counter(match.group(1).upper() for instruction in comments
                       if instruction.lower().startswith("b 0x")
                       if (match := BRANCH_TARGET.fullmatch(instruction)))
        if symbols - (declared + tail):
            raise ValueError(f"direct call body changed: {address}")
    if "native_calls" in entry:
        native = entry["native_calls"]
        if not isinstance(native, list) or sorted(native) != sorted(NATIVE_SYMBOL.findall(body)):
            raise ValueError(f"native calls changed: {address}")


def validate_manifests(paths: list[Path], ppc_root: Path = DEFAULT_PPC,
                       source_cache: dict[str, list[str]] | None = None) -> list[dict]:
    """Read each PPC file once, compare every complete body, then validate metadata."""
    if not paths:
        raise ValueError("at least one manifest is required")
    source_lines = source_cache if source_cache is not None else {}
    seen: set[str] = set()
    checked: list[dict] = []
    for manifest_path in paths:
        document = _json(Path(manifest_path))
        entries = document.get("entries")
        if not isinstance(entries, list) or not entries:
            raise ValueError(f"manifest needs nonempty entries: {manifest_path}")
        bodies = []
        pin_identities = {}
        for entry in entries:
            if not isinstance(entry, dict):
                raise ValueError(f"invalid entry: {manifest_path}")
            address = entry.get("address")
            if not isinstance(address, str) or not ADDRESS.fullmatch(address) or address != address.upper():
                raise ValueError(f"invalid entry address: {address}")
            if address in seen:
                raise ValueError(f"duplicate entry address: {address}")
            seen.add(address)
            name = _source_name(entry)
            line_number = _line_number(entry)
            if name not in source_lines:
                source_lines[name] = (Path(ppc_root) / name).read_text(encoding="utf-8").splitlines(keepends=True)
            lines = source_lines[name]
            start = line_number - 1
            symbol = entry.get("symbol")
            if symbol is None:
                symbol = f"__imp__sub_{address}"
            elif not isinstance(symbol, str) or not re.fullmatch(
                    r"__imp____(?:save|rest)vmx_[0-9]{1,3}", symbol):
                raise ValueError(f"unsupported named compiler helper: {symbol}")
            header = f"PPC_FUNC_IMPL({symbol}) {{"
            if start >= len(lines) or lines[start].rstrip("\r\n") != header:
                raise ValueError(f"PPC source location changed: {address} {name}:{line_number}")
            end = start + 1
            while end < len(lines) and lines[end].rstrip("\r\n") != "}":
                if lines[end].startswith("PPC_FUNC_IMPL("):
                    raise ValueError(f"unterminated PPC body: {address}")
                end += 1
            if end == len(lines):
                raise ValueError(f"unterminated PPC body: {address}")
            source_body = "".join(lines[start:end + 1])
            body = entry.get("translated_body")
            # Older manifests include the closing line's newline; newer ones
            # omit it. The complete interior and closing brace must still match.
            digest = entry.get("body_sha256")
            if digest is not None:
                # read_text normalizes CRLF; omit only the final newline.
                actual = hashlib.sha256(source_body.removesuffix("\n").encode("utf-8")).hexdigest()
                if not isinstance(digest, str) or not re.fullmatch(r"[0-9a-f]{64}", digest) or digest != actual:
                    raise ValueError(f"complete body digest changed: {address} {name}:{line_number}")
            if body is not None:
                if body not in (source_body, source_body.removesuffix("\n")):
                    raise ValueError(f"complete translated body changed: {address} {name}:{line_number}")
            elif digest is None:
                raise ValueError(f"missing complete body pin: {address}")
            body = source_body.removesuffix("\n")
            _metadata(entry, body)
            bodies.append(body)
            pin_identities[address] = (name, line_number, source_body)
        checked.append({"manifest": str(Path(manifest_path).resolve()),
                        "schema": document.get("schema", document.get("schema_version")),
                        "entries": [e["address"] for e in entries],
                        "sources": sorted({_source_name(e) for e in entries}),
                        "pin_identities": pin_identities,
                        "original_cpp": ("\n".join(bodies)).encode("utf-8")})
    return checked


def compose_batches(paths: list[Path]) -> dict:
    """Keep every oracle while assigning stable, unique receipt names."""
    if not paths:
        raise ValueError("at least one batch is required")
    families = []
    names = set()
    sources = []
    for path in paths:
        source = str(path.resolve())
        batch = _json(path)
        if batch.get("schema") != "semantic-recovery-batch-v1":
            raise ValueError(f"unexpected batch schema: {path}")
        items = batch.get("families")
        if not isinstance(items, list) or not items:
            raise ValueError(f"batch needs nonempty families: {path}")
        sources.append(source)
        for family in items:
            if not isinstance(family, dict):
                raise ValueError(f"invalid family in batch: {path}")
            original_name = family.get("name")
            if not isinstance(original_name, str) or not SUITE.fullmatch(original_name):
                raise ValueError(f"invalid family name in batch: {path}")
            name = original_name
            suffix = 2
            while name in names:
                name = f"{original_name}-{suffix}"
                suffix += 1
            names.add(name)
            item = dict(family)
            item.update({"name": name, "original_name": original_name,
                         "source_batch": source})
            families.append(item)
    return {"schema": "semantic-recovery-batch-v1",
            "scope": "Composed existing semantic recovery batches; no mapping credit",
            "source_batches": sources, "families": families}


def _track_pins(checked: dict, seen: dict, allow_shared: bool) -> None:
    for address, identity in checked["pin_identities"].items():
        previous = seen.get(address)
        if previous is not None:
            if not allow_shared:
                raise ValueError(f"duplicate batch address: {address}")
            if previous != identity:
                raise ValueError(f"conflicting shared pin: {address}")
        else:
            seen[address] = identity


def check_composed_batch(batch: dict, ppc_root: Path) -> list[dict]:
    """Read-only pin/source check for a proposed one-build batch."""
    seen = {}
    source_cache = {}
    plan = []
    for family in batch["families"]:
        manifest = family.get("manifest")
        harness = family.get("harness")
        prelude = family.get("prelude", "")
        if not all(isinstance(value, str) for value in (manifest, harness, prelude)):
            raise ValueError(f"invalid manifest, harness or prelude: {family['name']}")
        manifest_path = (ROOT / manifest).resolve()
        harness_path = (ROOT / harness).resolve()
        if not manifest_path.is_file() or not harness_path.is_file():
            raise FileNotFoundError(f"missing manifest or harness: {family['name']}")
        checked = validate_manifests([manifest_path], ppc_root, source_cache)[0]
        _track_pins(checked, seen, allow_shared=True)
        sources = _source_paths(family, None)
        plan.append({"name": family["name"],
                     "original_name": family["original_name"],
                     "source_batch": family["source_batch"],
                     "manifest": checked["manifest"],
                     "harness": str(harness_path),
                     "entries": checked["entries"],
                     "semantic_sources": [str(path) for path in sources],
                     "receipt_name": f"{family['name']}-result.json"})
    return plan


def _output_dir(path: Path) -> Path:
    output = path.resolve()
    if output.is_relative_to(ROOT) or output.is_relative_to(Path.home() / "ownCloud"):
        raise ValueError("batch output must be outside the checkout and ownCloud")
    output.mkdir(parents=True, exist_ok=True)
    return output


def _source_paths(family: dict, library_build: Path | None) -> list[Path]:
    sources = family.get("sources", [])
    if not isinstance(sources, list) or any(not isinstance(x, str) for x in sources):
        raise ValueError(f"invalid sources: {family['name']}")
    resolved = [(ROOT / x).resolve() for x in sources]
    for path in resolved:
        if not path.is_file() or not path.is_relative_to(SEMANTICS / "src"):
            raise ValueError(f"semantic source missing or outside src: {path}")
    if library_build:
        cmake = (SEMANTICS / "CMakeLists.txt").read_text(encoding="utf-8")
        allowed = {(SEMANTICS / match).resolve() for match in re.findall(
            r"src/[A-Za-z0-9_]+\.cpp", cmake)}
        for path in resolved:
            if path not in allowed:
                raise ValueError(f"source not in semantics CMake target: {path}")
    return resolved


def _build_library(build_dir: Path, output: Path,
                   library_file: Path | None = None,
                   native_environment: dict | None = None) -> tuple[Path, float]:
    build_dir = build_dir.resolve()
    cache_path = build_dir / "CMakeCache.txt"
    if not cache_path.is_file():
        raise ValueError(f"not a configured CMake build: {build_dir}")
    cache = cache_path.read_text(encoding="utf-8", errors="replace").splitlines()
    source = next((line.split("=", 1)[1].strip() for line in cache
                   if line.startswith("CMAKE_HOME_DIRECTORY:")), None)
    if not source or Path(source).resolve() not in (ROOT.resolve(), SEMANTICS.resolve()):
        raise ValueError(f"CMake build source differs from this checkout: {source}")
    if library_file:
        library_file = library_file.resolve()
        if (library_file.name != "LostOdysseyRecompSemantics.lib" or
                not library_file.is_relative_to(build_dir)):
            raise ValueError(f"library file must be inside this build: {library_file}")
    environment = native_environment if native_environment is not None else compiler_environment()
    search_path = next(v for k, v in environment.items() if k.upper() == "PATH")
    cmake = shutil.which("cmake", path=search_path)
    if cmake is None:
        raise RuntimeError("cmake unavailable after native compiler setup")
    command = [cmake, "--build", str(build_dir), "--target",
               "LostOdysseyRecompSemantics", "--parallel", "1"]
    started = time.perf_counter()
    log_path = output / "semantic-library-build.log"
    with log_path.open("w", encoding="utf-8") as log:
        completed = subprocess.run(command, cwd=ROOT, env=environment,
                                   stdout=log, stderr=subprocess.STDOUT,
                                   text=True, timeout=1800)
    elapsed = round(time.perf_counter() - started, 3)
    if completed.returncode:
        raise subprocess.CalledProcessError(completed.returncode, command)
    config = next((line.split("=", 1)[1].strip() for line in cache
                   if line.startswith("CMAKE_BUILD_TYPE:")), "")
    name = "LostOdysseyRecompSemantics.lib"
    configs = [config] if config else ["Debug", "Release", "RelWithDebInfo", "MinSizeRel"]
    candidates = [library_file] if library_file else [
        *(build_dir / "LostOdysseyRecompSemantics" / mode / name for mode in configs),
        *(build_dir / mode / name for mode in configs),
        build_dir / "LostOdysseyRecompSemantics" / name,
        build_dir / name,
    ]
    found = [path for path in candidates if path.is_file()]
    if len(found) > 1:
        raise ValueError(f"multiple semantics libraries in standard locations; pass --library-file: {found}")
    if not found:
        raise FileNotFoundError("built semantics library missing; pass --library-file for a "
                                f"nonstandard output path under {build_dir}")
    return found[0].resolve(), elapsed


def run_batch(batch_path: Path, output_path: Path, ppc_root: Path,
              library_build: Path | None = None,
              library_file: Path | None = None,
              msvc_runtime: str | None = None,
              allow_shared_pins: bool = False,
              native_environment: dict | None = None) -> dict:
    output = _output_dir(output_path)
    receipt_path = output / "semantic-recovery-result.json"
    result = {"status": "failed", "phase": "batch_validation", "batch": str(batch_path.resolve()),
              "output": str(output), "families": []}
    _write(receipt_path, result)
    started = time.perf_counter()
    try:
        batch = _json(batch_path)
        if "source_batches" in batch:
            result["source_batches"] = batch["source_batches"]
        families = batch.get("families")
        if not isinstance(families, list) or not families:
            raise ValueError("batch needs nonempty families")
        names = [family.get("name") if isinstance(family, dict) else None for family in families]
        if any(not isinstance(name, str) or not SUITE.fullmatch(name) for name in names):
            raise ValueError("each family needs a simple name")
        if len(set(names)) != len(names):
            raise ValueError("duplicate family names")
        # Invalidate every old pass before validating even the first manifest.
        for family in families:
            item = {"name": family["name"], "status": "failed", "phase": "validation",
                    "receipt": str(output / f"{family['name']}-result.json")}
            for key in ("source_batch", "original_name"):
                if key in family:
                    item[key] = family[key]
            result["families"].append(item)
            _write(Path(item["receipt"]), item)
        _write(receipt_path, result)
        runtime_error = ("--msvc-runtime is required with --library-build"
                         if library_build and msvc_runtime is None else
                         f"unsupported MSVC runtime: {msvc_runtime}"
                         if msvc_runtime is not None and
                         msvc_runtime not in ("MD", "MDd", "MT", "MTd") else None)
        if runtime_error:
            for item in result["families"]:
                item["error"] = f"ValueError: {runtime_error}"
                _write(Path(item["receipt"]), item)
            raise ValueError(runtime_error)
        result["msvc_runtime"] = msvc_runtime or "MD"
        prepared = []
        seen: dict[str, tuple[str, int, str]] = {}
        source_cache: dict[str, list[str]] = {}
        for family, item in zip(families, result["families"]):
            try:
                manifest = family.get("manifest")
                harness = family.get("harness")
                prelude = family.get("prelude", "")
                if not isinstance(manifest, str) or not isinstance(harness, str) or not isinstance(prelude, str):
                    raise ValueError("manifest, harness, and prelude must be strings")
                manifest_path = (ROOT / manifest).resolve()
                harness_path = (ROOT / harness).resolve()
                if not manifest_path.is_file() or not harness_path.is_file():
                    raise FileNotFoundError(f"missing manifest or harness: {family['name']}")
                checked = validate_manifests([manifest_path], ppc_root, source_cache)[0]
                _track_pins(checked, seen, allow_shared_pins)
                sources = _source_paths(family, library_build)
                item.update({"manifest": checked["manifest"], "entries": checked["entries"],
                             "ppc_sources": checked["sources"], "harness": str(harness_path),
                             "semantic_sources": [str(x) for x in sources],
                             "header": str(Path(ppc_root).resolve() / "ppc_context.h")})
                _write(Path(item["receipt"]), item)
                prepared.append((family, item, checked, harness_path, prelude, sources))
            except Exception as exc:
                item["error"] = f"{type(exc).__name__}: {exc}"
                _write(Path(item["receipt"]), item)
                raise
        result["phase"] = "compiler_setup"
        for item in result["families"]:
            item["phase"] = "compiler_setup"
            _write(Path(item["receipt"]), item)
        _write(receipt_path, result)
        setup_started = time.perf_counter()
        try:
            environment = (native_environment if native_environment is not None
                           else compiler_environment())
        except Exception as exc:
            for item in result["families"]:
                item["error"] = f"{type(exc).__name__}: {exc}"
                _write(Path(item["receipt"]), item)
            raise
        finally:
            result["compiler_setup_seconds"] = round(time.perf_counter() - setup_started, 3)
        library = None
        if library_build:
            result["phase"] = "library_build"
            result["library_build_log"] = str(output / "semantic-library-build.log")
            for item in result["families"]:
                item["phase"] = "library_build"
                _write(Path(item["receipt"]), item)
            _write(receipt_path, result)
            try:
                library, elapsed = _build_library(library_build, output, library_file,
                                                  native_environment=environment)
            except Exception as exc:
                for item in result["families"]:
                    item["error"] = f"{type(exc).__name__}: {exc}"
                    _write(Path(item["receipt"]), item)
                raise
            result["library"] = str(library)
            result["library_build_seconds"] = elapsed
        failed_families = []
        for family, item, checked, harness, prelude, sources in prepared:
            result["phase"] = f"oracle:{family['name']}"
            item["phase"] = "compile"
            _write(Path(item["receipt"]), item)
            _write(receipt_path, result)
            original = prelude.encode("utf-8") + b"\n" + checked["original_cpp"]
            family_error = None
            try:
                run = compile_and_run(family["name"], original, harness.read_bytes(),
                                      [] if library else sources, output,
                                      extra_include_dirs=[Path(ppc_root).resolve()],
                                      semantic_library=library,
                                      msvc_runtime=msvc_runtime or "MD",
                                      native_environment=environment)
            except Exception as exc:
                family_error = exc
                failed_families.append(family["name"])
            finally:
                # compile_and_run writes the phase and error even on failure.
                item.update(_json(Path(item["receipt"])))
                item.update({"manifest": checked["manifest"], "entries": checked["entries"],
                             "ppc_sources": checked["sources"], "harness": str(harness),
                             "semantic_sources": [str(x) for x in sources],
                             "header": str(Path(ppc_root).resolve() / "ppc_context.h"),
                             "receipt": item["receipt"]})
                _write(Path(item["receipt"]), item)
                _write(receipt_path, result)
            if family_error is not None:
                continue
            item.update(run)
        if failed_families:
            result["phase"] = "oracles_complete"
            raise RuntimeError(f"failed oracle families: {', '.join(failed_families)}")
        result["status"] = "passed"
        result["phase"] = "complete"
    except Exception as exc:
        result["error"] = f"{type(exc).__name__}: {exc}"
        raise
    finally:
        result["elapsed_seconds"] = round(time.perf_counter() - started, 3)
        _write(receipt_path, result)
        print(f"result={result['status']} phase={result['phase']} receipt={receipt_path} "
              f"elapsed={result['elapsed_seconds']}s "
              f"library={result.get('library', 'per-family sources')} "
              f"build_log={result.get('library_build_log', '-')}", flush=True)
        for item in result["families"]:
            print(f"family={item['name']} status={item['status']} phase={item['phase']} "
                  f"entries={','.join(item.get('entries', []))} "
                  f"header={item.get('header', '?')} sources={','.join(item.get('semantic_sources', []))} "
                  f"receipt={item['receipt']} log={output / (item['name'] + '.log')} "
                  f"compile={item.get('compile_seconds', '-')}s "
                  f"execute={item.get('execute_seconds', '-')}s", flush=True)
    return result


def serve_requests(lines, ppc_root: Path, library_build: Path,
                   library_file: Path | None, msvc_runtime: str) -> None:
    """Process JSON-line batches with one native environment per server."""
    environment = compiler_environment()
    for line in lines:
        if not line.strip():
            continue
        output = None
        receipt = None
        started_run = False
        marker = {"done": True, "status": "failed"}
        try:
            request = json.loads(line)
            if not isinstance(request, dict):
                raise ValueError("serve request must be a JSON object")
            if request.get("action") == "exit":
                print("SEMANTIC_RECOVERY_DONE " +
                      json.dumps({"done": True, "status": "exit"}), flush=True)
                return
            batches = request.get("batches")
            raw_output = request.get("output")
            if (not isinstance(batches, list) or not batches or
                    any(not isinstance(path, str) for path in batches) or
                    not isinstance(raw_output, str)):
                raise ValueError("serve request needs batches and output")
            output = _output_dir(Path(raw_output))
            receipt = output / "semantic-recovery-result.json"
            _write(receipt, {"status": "failed", "phase": "compose",
                             "output": str(output), "families": []})
            paths = [(ROOT / path).resolve() for path in batches]
            combined = output / "composed-batch.json"
            _write(combined, compose_batches(paths))
            started_run = True
            result = run_batch(combined, output, ppc_root, library_build,
                               library_file, msvc_runtime,
                               allow_shared_pins=True,
                               native_environment=environment)
            marker["status"] = result["status"]
        except Exception as exc:
            marker["error"] = f"{type(exc).__name__}: {exc}"
            if receipt is not None and not started_run:
                _write(receipt, {"status": "failed", "phase": "compose",
                                 "output": str(output), "families": [],
                                 "error": marker["error"]})
        if output is not None:
            marker["output"] = str(output)
        if receipt is not None:
            marker["receipt"] = str(receipt)
        print("SEMANTIC_RECOVERY_DONE " + json.dumps(marker), flush=True)


def _head_json(paths: list[str]) -> dict[str, dict]:
    request = "".join(f"HEAD:{path}\n" for path in paths).encode("utf-8")
    completed = subprocess.run(["git", "cat-file", "--batch"], cwd=ROOT,
                               input=request, capture_output=True, check=True)
    data = completed.stdout
    offset = 0
    documents = {}
    for path in paths:
        newline = data.index(b"\n", offset)
        header = data[offset:newline].decode("ascii")
        parts = header.split()
        if len(parts) != 3 or parts[1] != "blob":
            raise ValueError(f"missing HEAD blob: {path}: {header}")
        length = int(parts[2])
        payload = data[newline + 1:newline + 1 + length]
        documents[path] = json.loads(payload.decode("utf-8"))
        offset = newline + 2 + length
    return documents


def progress(runtime_wrappers: int | None = None) -> dict:
    listing = subprocess.run(["git", "ls-tree", "-r", "--name-only", "HEAD",
                              "LostOdysseyRecompSemantics"], cwd=ROOT,
                             capture_output=True, text=True, check=True).stdout.splitlines()
    manifests = [path for path in listing if path.endswith("families.json")]
    recovery = "LostOdysseyRecompSemantics/recovery.json"
    documents = _head_json([recovery, *manifests])
    individual = {item["address"].upper() for item in documents[recovery]["functions"]}
    family_entries = [item["address"].upper() for path in manifests
                      for item in documents[path].get("entries", [])]
    family = set(family_entries)
    result = {"source": "HEAD tracked JSON only", "individual": len(individual),
              "family_count": len(manifests), "family_entries": len(family_entries),
              "family_unique": len(family), "overlaps": len(individual & family),
              "unique": len(individual | family), "cached_total": 62627,
              "cached_entry_mapping_percent": round(100 * len(individual | family) / 62627, 3),
              "scope_note": "entry address mapping against fixed cached baseline, not a refreshed whole-game census or full semantic completion"}
    if runtime_wrappers is not None:
        result["runtime_wrappers_supplied"] = runtime_wrappers
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    check = sub.add_parser("check", help="verify complete pinned bodies and manifest metadata")
    check.add_argument("--manifest", nargs="+", type=Path, required=True)
    check.add_argument("--ppc-root", type=Path, default=DEFAULT_PPC)
    run = sub.add_parser("run", help="validate and run selected families")
    run.add_argument("--batch", type=Path, required=True)
    run.add_argument("--output", type=Path, required=True)
    run.add_argument("--ppc-root", type=Path, default=DEFAULT_PPC)
    run.add_argument("--library-build", type=Path)
    run.add_argument("--library-file", type=Path,
                     help="exact .lib under --library-build for nonstandard CMake layouts")
    run.add_argument("--msvc-runtime",
                     help="MD, MDd, MT or MTd; required with --library-build")
    compose = sub.add_parser("compose-batches",
                             help="run existing batches with one library build and separate family receipts")
    compose.add_argument("--batch", nargs="+", type=Path, required=True)
    compose.add_argument("--output", type=Path,
                         help="output directory outside checkout and ownCloud; required unless --dry-run")
    compose.add_argument("--ppc-root", type=Path, default=DEFAULT_PPC)
    compose.add_argument("--library-build", type=Path,
                         help="configured semantics CMake build; required unless --dry-run")
    compose.add_argument("--library-file", type=Path)
    compose.add_argument("--msvc-runtime",
                         help="MD, MDd, MT or MTd; required unless --dry-run")
    compose.add_argument("--dry-run", action="store_true",
                         help="check all pins and sources without writing receipts or building")
    serve = sub.add_parser("serve", help="process JSON-line composed batches in one native environment")
    serve.add_argument("--ppc-root", type=Path, default=DEFAULT_PPC)
    serve.add_argument("--library-build", type=Path, required=True)
    serve.add_argument("--library-file", type=Path)
    serve.add_argument("--msvc-runtime", required=True,
                       help="MD, MDd, MT or MTd")
    stats = sub.add_parser("progress", help="count address mapping from HEAD JSON")
    stats.add_argument("--runtime-wrappers", type=int)
    args = parser.parse_args()
    try:
        if args.command == "check":
            checked = validate_manifests(args.manifest, args.ppc_root)
            print(json.dumps([{k: v for k, v in item.items()
                               if k not in ("original_cpp", "pin_identities")}
                              for item in checked], indent=2))
        elif args.command == "run":
            if args.library_file and not args.library_build:
                raise ValueError("--library-file requires --library-build")
            run_batch(args.batch, args.output, args.ppc_root,
                      args.library_build, args.library_file, args.msvc_runtime)
        elif args.command == "compose-batches":
            batch = compose_batches(args.batch)
            if args.dry_run:
                plan = check_composed_batch(batch, args.ppc_root)
                print(json.dumps({"status": "checked", "source_batches": batch["source_batches"],
                                  "families": plan}, indent=2))
            else:
                if args.output is None or args.library_build is None:
                    raise ValueError("--output and --library-build are required for compose-batches")
                if args.library_file and not args.library_build:
                    raise ValueError("--library-file requires --library-build")
                output = _output_dir(args.output)
                combined = output / "composed-batch.json"
                _write(combined, batch)
                run_batch(combined, output, args.ppc_root, args.library_build,
                          args.library_file, args.msvc_runtime,
                          allow_shared_pins=True)
        elif args.command == "serve":
            if args.msvc_runtime not in ("MD", "MDd", "MT", "MTd"):
                raise ValueError(f"unsupported MSVC runtime: {args.msvc_runtime}")
            serve_requests(sys.stdin, args.ppc_root, args.library_build,
                           args.library_file, args.msvc_runtime)
        else:
            if args.runtime_wrappers is not None and args.runtime_wrappers < 0:
                raise ValueError("runtime wrappers must be nonnegative")
            print(json.dumps(progress(args.runtime_wrappers), indent=2))
    except Exception as exc:
        print(f"semantic recovery {args.command} failed: {type(exc).__name__}: {exc}",
              file=sys.stderr)
        raise SystemExit(1) from exc


if __name__ == "__main__":
    main()
