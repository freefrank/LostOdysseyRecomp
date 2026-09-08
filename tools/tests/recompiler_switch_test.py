"""Generate, compile and execute PPC word-switch fixtures; no game or assets."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

from run import compiler_environment

ROOT = Path(__file__).resolve().parents[2]
FIXTURE = ROOT / "tools/tests/recompiler_switch"
RECOMP = ROOT / "tools/XenonRecomp"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path,
                        help="new directory for generator, compiled cases and logs")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    env = compiler_environment()
    search_path = next(v for k, v in env.items() if k.upper() == "PATH")
    compiler = shutil.which("clang-cl", path=search_path)
    cmake = shutil.which("cmake", path=search_path)
    if not compiler or not cmake:
        raise RuntimeError("clang-cl and CMake are required")
    results = []

    def run(name, command, *, expected=0, timeout=120):
        print(name, flush=True)
        result = subprocess.run([str(arg) for arg in command], cwd=output,
                                env=env, capture_output=True, timeout=timeout)
        (output / f"{name}.log").write_bytes(result.stdout + result.stderr)
        results.append({"name": name, "command": [str(x) for x in command],
                        "exit_code": result.returncode})
        (output / "results.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
        if expected is not None and result.returncode != expected:
            raise RuntimeError(f"{name} failed ({result.returncode}); see {name}.log")
        return result

    run("configure", [cmake, "-S", FIXTURE, "-B", output / "build", "-G", "Ninja",
        "-DCMAKE_C_COMPILER=clang-cl", "-DCMAKE_CXX_COMPILER=clang-cl",
        "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"])
    run("build-generator", [cmake, "--build", output / "build", "--target",
        "RecompilerSwitchGenerate", "-j", "4"], timeout=240)
    run("generate", [output / "build/RecompilerSwitchGenerate.exe", output / "generated.cpp"])
    generated = (output / "generated.cpp").read_text(encoding="utf-8")
    # This validates fixture discovery; behavioral assertions below establish
    # semantics. Do not silently turn a missing generated switch into a pass.
    if len(re.findall(r"switch \(ctx\.r11\.u32\)", generated)) != 2:
        raise RuntimeError("expected two production word-selector switches")
    shutil.copyfile(FIXTURE / "check.cpp", output / "check.cpp")
    compile_args = [compiler, "/nologo", "/std:c++20", "/EHsc", "/O2", "/MT",
                    "-march=sandybridge", f"/I{RECOMP / 'XenonUtils'}",
                    f"/I{RECOMP / 'thirdparty/simde'}", output / "check.cpp"]
    run("compile", [*compile_args, f"/Fo{output / 'check.obj'}", f"/Fe{output / 'check.exe'}"])
    run("normal", [output / "check.exe", "normal"])
    run("word-selector", [output / "check.exe"])

    # A mutation negative control restores only the old generated selector.
    # Execute ordinary cases first to distinguish a broken fixture from the
    # high-word bug. Trap unreachable UB explicitly: a small synthetic switch
    # may otherwise optimize into a 32-bit lookup that masks the bad selector,
    # unlike the larger game function's 64-bit host jump table.
    old = output / "old-selector"
    old.mkdir()
    (old / "generated.cpp").write_text(generated.replace("switch (ctx.r11.u32)",
        "switch (ctx.r11.u64)"), encoding="utf-8")
    shutil.copyfile(FIXTURE / "check.cpp", old / "check.cpp")
    run("compile-old-selector", [*compile_args[:-1], "-fsanitize=unreachable",
        "-fsanitize-trap=unreachable", old / "check.cpp",
        f"/Fo{old / 'check.obj'}", f"/Fe{old / 'check.exe'}"])
    run("old-normal", [old / "check.exe", "normal"])
    negative = run("old-high-word", [old / "check.exe"], expected=None, timeout=15)
    if negative.returncode & 0xFFFFFFFF != 0xC000001D:
        raise RuntimeError("old high-word selector did not reach the unreachable trap")
    (output / "source-sha256.json").write_text(json.dumps({str(path.relative_to(ROOT)):
        hashlib.sha256(path.read_bytes()).hexdigest() for path in [
            RECOMP / "XenonRecomp/recompiler.cpp", RECOMP / "XenonUtils/ppc_context.h",
            *FIXTURE.glob("*"), Path(__file__).resolve()]}, indent=2), encoding="utf-8")
    print("PASS: generated word switches and old-selector negative control", flush=True)


if __name__ == "__main__":
    main()
