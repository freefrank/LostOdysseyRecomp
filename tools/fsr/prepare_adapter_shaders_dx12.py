"""Compile the FSR color/depth conversion shaders to signed DXIL headers."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dxc", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    dxc, output = args.dxc.resolve(), args.output.resolve()
    if not (dxc.parent / "dxil.dll").is_file():
        parser.error("dxil.dll must be beside dxc.exe for signed DXIL")
    output.mkdir(parents=True, exist_ok=True)
    manifest = {"api": "D3D12", "compiler_sha256": sha(dxc),
                "dxil_sha256": sha(dxc.parent / "dxil.dll"), "shaders": {}}
    for name in ("fsr_prepare", "fsr_present"):
        source = root / "LostOdysseyRecomp/gpu/shaders" / (name + ".hlsl")
        binary = output / (name + ".dxil")
        command = [str(dxc), "-T", "cs_6_0", "-E", "main", "-O3", "-Fo", str(binary), str(source)]
        subprocess.run(command, check=True)
        data = binary.read_bytes()
        if data[:4] != b"DXBC":
            raise RuntimeError(f"Invalid DXIL output: {name}")
        symbol = "lo_" + name + "_dxil"
        body = "\n".join("    " + ", ".join(f"0x{byte:02x}" for byte in data[i:i + 16]) + ","
                         for i in range(0, len(data), 16))
        header = output / (name + "_dxil.h")
        header.write_text(f"#pragma once\n#include <cstdint>\n#include <cstddef>\n"
                          f"inline constexpr uint8_t {symbol}[] = {{\n{body}\n}};\n"
                          f"inline constexpr size_t {symbol}_size = sizeof({symbol});\n", encoding="utf-8")
        manifest["shaders"][name] = {"source_sha256": sha(source), "dxil_sha256": sha(binary),
                                    "header_sha256": sha(header), "command": command}
    (output / "adapter-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
