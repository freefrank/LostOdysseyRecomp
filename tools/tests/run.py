"""Run explicitly selected regression suites; no default full-suite run."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SUITES = {
    "importer": "Python importer fixtures; no compiler or game data",
    "shader-index": "Standalone resource scanner fixtures",
    "shaders": "Resource scanner, CPX decoder and dynamic VS fixtures",
    "pipeline": "Standalone pipeline recipe persistence fixtures",
    "storage": "Built LoStorageTest: disc selection and save round trips",
    "hid": "Built LoHidTest: controller and keyboard input",
    "startup": "Built runtime: synthetic Unicode startup paths (headless)",
}
FIXTURES = {
    "shader-index": [("shader_resource_scan_test", True)],
    "shaders": [("shader_resource_scan_test", True), ("cpx_decode_test", False),
                ("shader_resource_variants_test", True)],
    "pipeline": [("pipeline_cache_test", True)],
}


def run(command, *, cwd=ROOT, env=None):
    print("+ " + subprocess.list2cmdline([str(arg) for arg in command]), flush=True)
    subprocess.run(command, cwd=cwd, env=env, check=True)


def compiler_environment():
    # setup_windows exports vcvars/SDK discovery without changing the caller.
    result = subprocess.run(
        ["cmd.exe", "/d", "/s", "/c", 'call tools\\setup_windows.bat >nul && set'],
        cwd=ROOT, capture_output=True, text=True, check=True)
    return dict(line.split("=", 1) for line in result.stdout.splitlines()
                if "=" in line and not line.startswith("="))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("suites", nargs="*", metavar="SUITE")
    parser.add_argument("--list", action="store_true", help="list suites without running checks")
    parser.add_argument("--build-dir", type=Path, default=ROOT / "out/build/release",
                        help="existing CMake build directory for storage/hid/startup")
    args = parser.parse_args()
    if args.list:
        for name, description in SUITES.items():
            print(f"{name:13} {description}")
        return
    if not args.suites or any(name not in SUITES for name in args.suites):
        parser.error("select one or more suites from: " + ", ".join(SUITES))
    os.environ["PYTHONDONTWRITEBYTECODE"] = "1"
    build = args.build_dir.resolve() / "LostOdysseyRecomp"
    binaries = {"storage": build / "LoStorageTest.exe", "hid": build / "LoHidTest.exe",
                "startup": build / "LostOdysseyRecomp.exe"}
    for name in args.suites:
        if name in binaries and not binaries[name].is_file():
            parser.error(f"missing {binaries[name]}; build that target explicitly first")
    compiler_env = compiler_environment() if any(name in FIXTURES for name in args.suites) else None
    compiler = None
    if compiler_env:
        search_path = next(value for key, value in compiler_env.items() if key.upper() == "PATH")
        compiler = shutil.which("clang-cl", path=search_path)
        if not compiler:
            parser.error("clang-cl was not found after Windows toolchain setup")
    output = ROOT / "out/tests"
    output.mkdir(parents=True, exist_ok=True)
    completed = set()
    # Only this newly allocated directory is removed, including on failure.
    with tempfile.TemporaryDirectory(prefix="scratch-", dir=output) as temporary:
        scratch = Path(temporary).resolve()
        if not scratch.is_relative_to(output.resolve()):
            raise RuntimeError("scratch directory escaped test output")
        for name in dict.fromkeys(args.suites):
            print(f"Running {name}", flush=True)
            if name == "importer":
                run([sys.executable, "-B", "-m", "unittest", "discover", "-s", "tools/tests",
                     "-p", "test_import_game.py", "-v"])
            elif name in FIXTURES:
                bin_dir = output / "bin"
                bin_dir.mkdir(exist_ok=True)
                for source, takes_scratch in FIXTURES[name]:
                    if source in completed:
                        continue
                    executable = bin_dir / (source + ".exe")
                    command = [compiler, "/nologo", "/std:c++20", "/EHsc", "/O2", "/MT",
                               "/ILostOdysseyRecomp", f"tools/tests/{source}.cpp",
                               f"/Fo{bin_dir / (source + '.obj')}", f"/Fe{executable}"]
                    if source != "shader_resource_scan_test":
                        command += ["/W4", "/WX"]
                    run(command, env=compiler_env)
                    run([executable, *([scratch] if takes_scratch else [])])
                    completed.add(source)
            elif name == "storage":
                run([sys.executable, "-B", ROOT / "tools/tests/disc_set_test.py", binaries[name]], cwd=scratch)
                for variant in ("ascii", "unicode"):
                    for mode in ("write", "read", "overwrite", "read-overwritten"):
                        run([binaries[name], mode, scratch / variant,
                             *(["unicode"] if variant == "unicode" else [])], cwd=scratch)
            elif name == "hid":
                run([binaries[name]], cwd=scratch)
            elif name == "startup":
                run([sys.executable, "-B", ROOT / "tools/tests/startup_path_test.py", binaries[name]])


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        sys.exit(error.returncode)
