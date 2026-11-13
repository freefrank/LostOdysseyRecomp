"""Compile the production presentation sources with/without optional SGSR1.

No game data, Vulkan device, Android SDK or full runtime build is required.
Usage: python tools/tests/sgsr1_shader_test.py --dxc /path/to/dxc
"""
import argparse
import os
from pathlib import Path
import re
import subprocess
import struct
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def raw_string(source, name, delimiter=""):
    match = re.search(r"\b" + re.escape(name) + r'\s*=\s*R"' + delimiter +
                      r'\((.*?)\)' + delimiter + r'";', source, re.S)
    if not match:
        raise RuntimeError(f"production shader {name} not found")
    return match[1]


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--dxc", type=Path, required=True)
    args = parser.parse_args()
    source = (ROOT / "LostOdysseyRecomp/gpu/presentation.cpp").read_text()
    prefix = raw_string(source, "sourcePrefix")
    suffix = raw_string(source, "sourceSuffix")
    compute = raw_string(source, "computeSource")
    sgsr = raw_string((ROOT / "LostOdysseyRecomp/gpu/shader/sgsr1_source.h").read_text(),
                      "kSgsr1Source", "SGSR")
    environment = os.environ.copy()
    library = args.dxc.resolve().parents[2] / "lib/x64"
    environment["LD_LIBRARY_PATH"] = str(library) + os.pathsep + environment.get("LD_LIBRARY_PATH", "")
    count = 0
    with tempfile.TemporaryDirectory(prefix="lo-sgsr1-shaders-") as temporary:
        directory = Path(temporary)
        for enabled in (False, True):
            text = prefix + (sgsr if enabled else "") + suffix
            text += "\n#define PRESENT_GROUP_X 16\n#define PRESENT_GROUP_Y 16\n" + compute
            shader = directory / "presentation.hlsl"
            shader.write_text(text)
            for entry, profile in (("vertex", "vs_6_0"), ("pixel", "ps_6_0"),
                                   ("gainPixel", "ps_6_0"), ("presentCompute", "cs_6_0")):
                output = directory / f"{entry}.spv"
                command = [str(args.dxc.resolve()), str(shader), "-E", entry, "-T", profile,
                           "-HV", "2021", "-O3", "-all-resources-bound", "-spirv",
                           "-fspv-target-env=vulkan1.2", "-fvk-use-dx-layout", "-Fo", str(output)]
                if profile.startswith("vs_"):
                    command.append("-fvk-invert-y")
                subprocess.run(command, check=True, env=environment)
                bytecode = output.read_bytes()
                if bytecode[:4] != b"\x03\x02\x23\x07":
                    raise RuntimeError("compiler did not produce SPIR-V")
                words = struct.unpack("<" + "I" * (len(bytecode) // 4), bytecode)
                index = 5
                while index < len(words):
                    size, opcode = words[index] >> 16, words[index] & 0xFFFF
                    if not size or index + size > len(words):
                        raise RuntimeError("invalid SPIR-V instruction length")
                    if opcode == 22 and words[index + 2] == 16:  # OpTypeFloat
                        raise RuntimeError("presentation unexpectedly requires native FP16")
                    index += size
                count += 1
                print(f"PASS SGSR1={int(enabled)} {entry} ({output.stat().st_size} bytes)")
    print(f"{count} production shader variants compiled; no device performance measured")


if __name__ == "__main__":
    main()
