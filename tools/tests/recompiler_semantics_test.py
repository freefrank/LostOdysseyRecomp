"""Decode, generate and execute PPC semantics regressions without game assets."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess

from run import compiler_environment

ROOT = Path(__file__).resolve().parents[2]
FIXTURE = ROOT / "tools/tests/recompiler_semantics"
RECOMP = ROOT / "tools/XenonRecomp"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path, help="new evidence/build directory")
    parser.add_argument("--baseline-dir", type=Path,
        help="optional pre-fix recompiler.cpp and ppc_context.h for real-generator negative controls")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    env = compiler_environment()
    search_path = next(v for k, v in env.items() if k.upper() == "PATH")
    compiler, cmake = (shutil.which(name, path=search_path) for name in ["clang-cl", "cmake"])
    if not compiler or not cmake:
        raise RuntimeError("clang-cl, CMake and the Windows SDK are required")
    spec = importlib.util.spec_from_file_location("semantics_cases", FIXTURE / "cases.py")
    cases = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(cases)
    metadata = cases.write_cases(output)
    steps = []

    def run(name, command, timeout=180):
        print(name, flush=True)
        proc = subprocess.run([str(arg) for arg in command], cwd=output,
            env=env, capture_output=True, timeout=timeout)
        (output / f"{name}.log").write_bytes(proc.stdout + proc.stderr)
        steps.append(dict(name=name, command=list(map(str, command)), exit_code=proc.returncode))
        (output / "steps.json").write_text(json.dumps(steps, indent=2), encoding="utf-8")
        if proc.returncode:
            raise RuntimeError(f"{name} failed ({proc.returncode}); see its log")
        return proc

    variants = {"current": [RECOMP / "XenonRecomp/recompiler.cpp", RECOMP / "XenonUtils/ppc_context.h"]}
    if args.baseline_dir:
        variants["baseline"] = [args.baseline_dir.resolve() / name for name in ["recompiler.cpp", "ppc_context.h"]]
    summaries = {}
    for variant, sources in variants.items():
        directory = output / variant
        directory.mkdir()
        snapshot = directory / "source"
        snapshot.mkdir()
        for source in sources:
            shutil.copyfile(source, snapshot / source.name)
        for name in ["cases.inc", "instructions.txt"]:
            shutil.copyfile(output / name, directory / name)
        shutil.copyfile(FIXTURE / "check.cpp", directory / "check.cpp")
        run(variant + "-configure", [cmake, "-S", FIXTURE, "-B", directory / "build", "-G", "Ninja",
            "-DCMAKE_C_COMPILER=clang-cl", "-DCMAKE_CXX_COMPILER=clang-cl",
            "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
            f"-DRECOMPILER_SOURCE={(snapshot / 'recompiler.cpp').as_posix()}"])
        run(variant + "-build-generator", [cmake, "--build", directory / "build", "--target",
            "RecompilerSemanticsGenerate", "-j", "4"], timeout=300)
        run(variant + "-generate", [directory / "build/RecompilerSemanticsGenerate.exe",
            directory / "instructions.txt", directory / "generated.cpp"])
        run(variant + "-compile", [compiler, "/nologo", "/std:c++20", "/EHsc", "/O2", "/MT",
            "-march=sandybridge", f"/I{snapshot}", f"/I{RECOMP / 'thirdparty/simde'}",
            directory / "check.cpp", f"/Fo{directory / 'check.obj'}", f"/Fe{directory / 'check.exe'}"], timeout=300)
        result = run(variant + "-execute", [directory / "check.exe"])
        observed = [json.loads(line) for line in result.stdout.decode().splitlines()]
        if [entry["id"] for entry in observed] != list(range(len(metadata))):
            raise RuntimeError("missing, repeated or reordered native fixture observations")
        summary = {}
        for entry, description in zip(observed, metadata):
            group = summary.setdefault(description["group"], dict(total=0, failed=0))
            group["total"] += 1
            group["failed"] += not entry["ok"]
            entry.update(description)
        (directory / "observations.json").write_text(json.dumps(observed, indent=2), encoding="utf-8")
        (directory / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
        (directory / "source-sha256.json").write_text(json.dumps({str(p): digest(p) for p in [
            *snapshot.glob("*"), *FIXTURE.glob("*.*"), Path(__file__).resolve(),
            RECOMP / "XenonRecomp/recompiler.h", RECOMP / "XenonRecomp/recompiler_config.cpp",
            directory / "generated.cpp", directory / "cases.inc", directory / "check.exe"]}, indent=2), encoding="utf-8")
        summaries[variant] = summary
    (output / "summary.json").write_text(json.dumps(summaries, indent=2), encoding="utf-8")
    print(json.dumps(summaries, indent=2), flush=True)
    if any(group["failed"] for group in summaries["current"].values()):
        raise RuntimeError("current generator failed semantic checks; see observations.json")
    if "baseline" in summaries:
        defects = {"sraw_ca", "srad_ca", "rlwimi", "rc_missing", "update_ea", "atomic_wrap",
            "absolute_address", "bdnzf_bi", "blrl", "ctr_alignment"}
        if any(summaries["baseline"][name]["failed"] == 0 for name in defects):
            raise RuntimeError("baseline did not expose every known defect class")
        if summaries["baseline"]["rc_controls"]["failed"]:
            raise RuntimeError("baseline existing record-bit controls failed")
    print(f"PASS: {len(metadata)} real generated PPC cases" +
        (" and pre-fix generator negative controls" if args.baseline_dir else ""), flush=True)


if __name__ == "__main__":
    main()
