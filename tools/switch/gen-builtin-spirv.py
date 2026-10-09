#!/usr/bin/env python3
"""Precompile the runtime's builtin host shaders to SPIR-V for the Switch.

The PC runtime compiles its own host shaders (presentation, blits, SMAA, TAA,
AO, ...) from HLSL with DXC at first use and caches the result. DXC does not
run on Horizon, so the Switch build embeds those binaries instead
(LostOdysseyRecomp/os/switch/builtin_spirv.inc, looked up by the same FNV-1a
key as CompileCachedHlsl).

How it works:
  1. The HLSL sources are taken from the runtime sources themselves: string
     expressions are copied verbatim into a small host C++ program, which
     prints each source exactly as the runtime builds it.
  2. Each (source, entry, profile) is compiled with the pinned DXC
     (tools/XenosRecomp/thirdparty/dxc-bin) using CompileCachedHlsl's
     Vulkan arguments.
  3. The SPIR-V words are written to the .inc file.

Run on Linux x64 from the repository root:
    python3 tools/switch/gen-builtin-spirv.py
Re-run it whenever one of these shaders changes. A shader the Switch build
cannot find is written to state/logs/missing-shaders on the SD card: copy the
file unchanged into tools/switch/extra-builtin-shaders/ and re-run.
"""
import os
import re
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GPU = ROOT / "LostOdysseyRecomp" / "gpu"
DXC_DIR = ROOT / "tools" / "XenosRecomp" / "thirdparty" / "dxc-bin"
OUTPUT = ROOT / "LostOdysseyRecomp" / "os" / "switch" / "builtin_spirv.inc"
# Sources the console dumped to state/logs/missing-shaders (built at run time,
# e.g. from a game shader), kept as <key>_<entry>_<profile>.hlsl.
EXTRA = ROOT / "tools" / "switch" / "extra-builtin-shaders"
KEY_SUFFIX = b"lo-dxc-vulkan12-dx-layout-v1"


def extract_expression(path, anchor, start_pattern):
    """Text of the initializer that follows start_pattern (after anchor)."""
    text = path.read_text(encoding="utf-8")
    base = 0
    if anchor:
        base = text.index(anchor)
    match = re.compile(start_pattern).search(text, base)
    if not match:
        raise SystemExit(f"{path.name}: '{start_pattern}' not found after '{anchor}'")
    i = match.end()
    out_start = i
    depth = 0
    while i < len(text):
        c = text[i]
        if c == "R" and text[i + 1] == '"':
            close = text.index("(", i)
            delim = text[i + 2:close]
            end = text.index(")" + delim + '"', close)
            i = end + len(delim) + 2
            continue
        if c == '"':
            i += 1
            while text[i] != '"':
                i += 2 if text[i] == "\\" else 1
            i += 1
            continue
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        elif c == ";" and depth == 0:
            return text[out_start:i].strip()
        i += 1
    raise SystemExit(f"{path.name}: unterminated initializer for '{start_pattern}'")


def harvest_program():
    """C++ source that prints every builtin shader as entry\\0profile\\0size\\0source."""
    rel = lambda p: os.path.relpath(p, GPU).replace(os.sep, "/")
    locals_ = {
        "presentation": extract_expression(GPU / "presentation.cpp", None, r"const char \*source\s*=\s*"),
        "presentationUi": extract_expression(GPU / "presentation.cpp", None, r"const char \*uiSource\s*=\s*"),
        "transferPs": extract_expression(GPU / "renderer.cpp", "void CompileTransferShader()", r"const char\* psSrc\s*=\s*"),
        "blitVs": extract_expression(GPU / "renderer.cpp", "void CompileBlitShaders()", r"const char\* vsSrc\s*=\s*"),
        "blitPs": extract_expression(GPU / "renderer.cpp", "void CompileBlitShaders()", r"const char\* psSrc\s*=\s*"),
        "rectListGs": extract_expression(GPU / "renderer.cpp", "void CompileRectListGs()", r"std::string src\s*=\s*"),
        "temporalAa": extract_expression(GPU / "temporal_aa.cpp", None, r"\nconst char\* source\s*=\s*"),
        "motionMask": extract_expression(GPU / "motion_replay_gpu.h", None, r"static constexpr const char\* kMaskShader\s*=\s*"),
        "smaaHead": extract_expression(GPU / "shader" / "smaa_pipeline.h", None, r"std::string hlsl\s*=\s*"),
        "smaaTail": extract_expression(GPU / "shader" / "smaa_pipeline.h", None, r"hlsl\+=(?=R)"),
    }
    lines = [
        "#include <cstdio>",
        "#include <string>",
        f'#include "{rel(GPU / "bloom_prefilter.h")}"',
        f'#include "{rel(GPU / "scene_copy_promotion_shaders.h")}"',
        f'#include "{rel(GPU / "shader" / "ambient_occlusion_hlsl.h")}"',
        '#include "../../thirdparty/smaa/SMAA_source.h"',
    ]
    for name, expr in locals_.items():
        lines.append(f"static const std::string {name} = {expr};")
    lines += [
        "static void Emit(const std::string& s, const char* entry, const char* profile) {",
        "    std::printf(\"%s%c%s%c%zu%c\", entry, 0, profile, 0, s.size(), 0);",
        "    std::fwrite(s.data(), 1, s.size(), stdout);",
        "}",
        "static std::string Smaa() {",
        "    // shader/smaa_pipeline.h, Vulkan branch",
        "    std::string hlsl = smaaHead, smaa = smaaSource;",
        '    smaa.insert(smaa.find("SamplerState LinearSampler"), "[[vk::binding(3,0)]] ");',
        '    smaa.insert(smaa.find("SamplerState PointSampler"), "[[vk::binding(4,0)]] ");',
        "    return hlsl + smaa + smaaTail;",
        "}",
        "int main() {",
        '    Emit(presentation, "vertex", "vs_6_0");',
        '    Emit(presentation, "pixel", "ps_6_0");',
        '    Emit(presentation, "gainPixel", "ps_6_0");',
        '    Emit(presentationUi, "vertexUi", "vs_6_0");',
        '    Emit(presentationUi, "pixelUi", "ps_6_0");',
        '    Emit(transferPs, "main", "ps_6_0");',
        '    Emit(blitVs, "main", "vs_6_0");',
        '    Emit(blitPs, "main", "ps_6_0");',
        '    Emit(gpu::bloom_prefilter::PixelShader, "main", "ps_6_0");',
        '    Emit(gpu::scene_copy_promotion::RgbaShader, "main", "ps_6_0");',
        '    Emit(gpu::scene_copy_promotion::RgbShader, "main", "ps_6_0");',
        '    Emit(rectListGs, "main", "gs_6_0");',
        '    Emit(temporalAa, "vertex", "vs_6_0");',
        '    Emit(temporalAa, "pixel", "ps_6_0");',
        '    Emit(temporalAa, "displayPixel", "ps_6_0");',
        '    Emit(gpu::ao::Shader, "vertex", "vs_6_0");',
        '    Emit(gpu::ao::Shader, "visibility", "ps_6_0");',
        '    Emit(gpu::ao::Shader, "composite", "ps_6_0");',
        '    Emit(gpu::ao::Shader, "compositePair", "ps_6_0");',
        '    Emit(motionMask, "vertex", "vs_6_0");',
        '    Emit(motionMask, "pixel", "ps_6_0");',
        '    for (const char* e : {"edge", "weight", "neighborhood"}) Emit(Smaa(), e, "ps_6_0");',
        "}",
    ]
    return "\n".join(lines) + "\n"


def fnv1a64(data):
    h = 0xCBF29CE484222325
    for b in data:
        h ^= b
        h = (h * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h


def main():
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        src = GPU / "_switch_builtin_harvest.cpp"  # next to the headers it includes
        src.write_text(harvest_program(), encoding="utf-8")
        try:
            exe = tmp / "harvest"
            subprocess.run(["g++", "-std=c++20", "-O0", "-o", str(exe), str(src)], check=True)
        finally:
            src.unlink()
        blob = subprocess.run([str(exe)], check=True, stdout=subprocess.PIPE).stdout

        shaders, pos = [], 0
        while pos < len(blob):
            entry_end = blob.index(b"\0", pos)
            profile_end = blob.index(b"\0", entry_end + 1)
            size_end = blob.index(b"\0", profile_end + 1)
            entry = blob[pos:entry_end].decode()
            profile = blob[entry_end + 1:profile_end].decode()
            size = int(blob[profile_end + 1:size_end])
            source = blob[size_end + 1:size_end + 1 + size]
            pos = size_end + 1 + size
            shaders.append((source, entry, profile))

        for path in sorted(EXTRA.glob("*.hlsl")):
            key_hex, rest = path.stem.split("_", 1)
            parts = rest.rsplit("_", 3)  # profile is "<stage>_<major>_<minor>"
            entry, profile = parts[0], "_".join(parts[1:])
            source = path.read_bytes()
            key = fnv1a64(source + b"\0" + entry.encode() + b"\0" + profile.encode() + KEY_SUFFIX)
            if f"{key:016x}" != key_hex:
                raise SystemExit(f"{path.name}: content hashes to {key:016x} (file changed after the dump?)")
            shaders.append((source, entry, profile))

        dxc = DXC_DIR / "bin" / "x64" / "dxc-linux"
        env = dict(os.environ, LD_LIBRARY_PATH=str(DXC_DIR / "lib" / "x64"))
        records = []
        for index, (source, entry, profile) in enumerate(shaders):
            hlsl = tmp / f"{index}.hlsl"
            spv = tmp / f"{index}.spv"
            hlsl.write_bytes(source)
            args = [str(dxc), "-E", entry, "-T", profile, "-HV", "2021",
                    "-Wno-parentheses-equality", "-Wno-unused-value", "-all-resources-bound",
                    "-spirv", "-fspv-target-env=vulkan1.2", "-fvk-use-dx-layout"]
            if profile.startswith("vs_"):
                args.append("-fvk-invert-y")
            args += ["-O3", "-Qstrip_debug", "-Fo", str(spv), str(hlsl)]
            result = subprocess.run(args, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            if result.returncode != 0:
                print(f"skip {entry} {profile}: {result.stderr.strip()[:400]}", file=sys.stderr)
                continue
            key = fnv1a64(source + b"\0" + entry.encode() + b"\0" + profile.encode() + KEY_SUFFIX)
            records.append((key, entry, profile, spv.read_bytes()))
            print(f"{key:016x} {profile} {entry}: {spv.stat().st_size} bytes")

    seen, out = set(), [
        "// Generated by tools/switch/gen-builtin-spirv.py; do not edit.",
        "// Builtin host shaders as SPIR-V for the Switch (no DXC on the console).",
        "// Key: FNV-1a 64 of source '\\0' entry '\\0' profile \"lo-dxc-vulkan12-dx-layout-v1\".",
    ]
    table = []
    for i, (key, entry, profile, data) in enumerate(records):
        if key in seen:
            continue
        seen.add(key)
        words = struct.unpack(f"<{len(data) // 4}I", data)
        out.append(f"static const uint32_t kBuiltinSpirv{i}[] = {{")
        for j in range(0, len(words), 8):
            out.append("    " + ",".join(f"0x{w:08x}" for w in words[j:j + 8]) + ",")
        out.append("};")
        table.append(f'    {{0x{key:016x}ull, "{entry}", "{profile}", kBuiltinSpirv{i}, sizeof(kBuiltinSpirv{i})}},')
    out.append("static const BuiltinSpirv kBuiltinSpirv[] = {")
    out += table
    out.append("};")
    OUTPUT.write_text("\n".join(out) + "\n", encoding="utf-8")
    print(f"wrote {OUTPUT.relative_to(ROOT)}: {len(table)} shaders")


if __name__ == "__main__":
    main()
