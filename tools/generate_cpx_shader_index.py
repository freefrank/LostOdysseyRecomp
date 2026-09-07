"""Generate metadata-only CPX shader locations using the production CPU decoder.

Supply a user-owned game directory containing disc1..disc4, or one disc directory.
The output path must be new; inspect its diff before replacing the built-in header.
No game resource, shader bytecode, cache or executable is bundled in the output.
"""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent / "tests"))
from run import ROOT, compiler_environment, run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "out/tools/cpx-index")
    parser.add_argument("--additional-root", type=Path, action="append", default=[], help="Merge another edition into the same metadata")
    args = parser.parse_args()
    if args.output.exists():
        parser.error("output already exists; choose a new file")
    build = args.build_dir.resolve()
    build.mkdir(parents=True, exist_ok=True)
    executable = build / ("generate_cpx_shader_index.exe" if sys.platform == "win32" else "generate_cpx_shader_index")
    if sys.platform == "win32":
        env = compiler_environment()
        search_path = next(v for k, v in env.items() if k.upper() == "PATH")
        compiler = shutil.which("clang-cl", path=search_path)
        if not compiler:
            parser.error("clang-cl not found")
        run([compiler, "/nologo", "/std:c++20", "/EHsc", "/O2", "/MT", "/ILostOdysseyRecomp",
             "tools/generate_cpx_shader_index.cpp", f"/Fo{build / 'generator.obj'}", f"/Fe{executable}"], env=env)
    else:
        run(["c++", "-std=c++20", "-O2", "-ILostOdysseyRecomp", "tools/generate_cpx_shader_index.cpp", "-o", executable])
    run([executable, args.root.resolve(), args.output.resolve(), *(p.resolve() for p in args.additional_root)])


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        sys.exit(error.returncode)
