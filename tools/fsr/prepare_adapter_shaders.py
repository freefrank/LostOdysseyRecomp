"""Compile the two application conversion shaders into portable SPIR-V headers."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--glslang", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    manifest = {"compiler_sha256": hashlib.sha256(args.glslang.read_bytes()).hexdigest(), "shaders": {}}
    for name in ["fsr_prepare", "fsr_present"]:
        source = root / "LostOdysseyRecomp/gpu/shaders" / (name + ".comp")
        binary = output / (name + ".spv")
        command = [str(args.glslang.resolve()), "-V", "--target-env", "vulkan1.2", "-S", "comp",
                   "-o", str(binary), str(source)]
        subprocess.run(command, check=True)
        data = binary.read_bytes()
        words = struct.unpack("<" + "I" * (len(data) // 4), data)
        if words[0] != 0x07230203:
            raise RuntimeError("Invalid SPIR-V output")
        header = output / (name + "_spv.h")
        symbol = "lo_" + name + "_spv"
        body = "\n".join("    " + ", ".join(f"0x{w:08x}u" for w in words[i:i + 8]) + ","
                         for i in range(0, len(words), 8))
        header.write_text(f"#pragma once\n#include <cstdint>\n#include <cstddef>\ninline constexpr uint32_t {symbol}[] = {{\n{body}\n}};\ninline constexpr size_t {symbol}_size = sizeof({symbol});\n", encoding="utf-8")
        manifest["shaders"][name] = {"source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
                                      "spirv_sha256": hashlib.sha256(data).hexdigest(),
                                      "header_sha256": hashlib.sha256(header.read_bytes()).hexdigest(),
                                      "command": command}
    (output / "adapter-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
