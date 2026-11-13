"""Exercise the production TransferRegion body against an eager FP16 pixel oracle.

Only graphics API calls are modeled. This is not a GPU/hardware benchmark.
"""
import argparse
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--cxx", default="g++")
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--baseline", action="store_true", help="Compare pixels/counts without requiring the new fast-path decisions")
    parser.add_argument("--renderer", type=Path, default=ROOT / "LostOdysseyRecomp/gpu/renderer.cpp")
    args = parser.parse_args()
    source = args.renderer.read_text(encoding="utf-8")
    begin = "            void TransferRegion(HostTexture& src, HostTexture& dst, uint32_t srcClass, uint32_t dstClass)"
    end = "            uint32_t transfers = 0;"
    if source.count(begin) != 1 or source.count(end) != 1:
        raise RuntimeError("Production extraction boundary changed")
    body = source[source.index(begin):source.index(end)]
    template = Path(__file__).with_suffix(".template.cpp").read_text(encoding="utf-8")
    args.out.mkdir(parents=True, exist_ok=True)
    fixture = args.out / "edram_partial_clear.cpp"
    fixture.write_text(template.replace("/* PRODUCTION_TRANSFER_REGION */", body), encoding="utf-8")
    executable = args.out / "edram_partial_clear"
    flags = ["-std=c++20", "-Wall", "-Wextra", "-Werror", "-DLO_GPU_PLUME=1"]
    if args.baseline:
        flags.append("-DLO_TEST_BASELINE=1")
    flags += (["-O1", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
              if args.sanitize else ["-O2"])
    subprocess.run([args.cxx, *flags, "-I", str(ROOT / "LostOdysseyRecomp"), str(fixture),
                    "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
