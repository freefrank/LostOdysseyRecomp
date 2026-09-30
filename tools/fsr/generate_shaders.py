"""Offline FSR 3.1.4 Vulkan shader preparation (run on Windows, consume anywhere)."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess

COMMIT = "c6efa6bf7f2027b3ec94f28578bb5965eabb9e55"
LUMA_PASS = "ffx_fsr3upscaler_luma_instability_pass"
LUMA_CALLBACK = Path("sdk/include/FidelityFX/gpu/fsr3upscaler/ffx_fsr3upscaler_callbacks_glsl.h")
LUMA_DECLARATION = (
    "layout (set = 0, binding = FSR3UPSCALER_BIND_UAV_LUMA_HISTORY, rgba8) "
    "uniform image2D  rw_luma_history;"
)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_luma_compatibility_include(sdk, output):
    upstream = sdk / LUMA_CALLBACK
    source = upstream.read_text(encoding="utf-8")
    if source.count(LUMA_DECLARATION) != 1:
        raise RuntimeError("Pinned SDK luma history GLSL declaration changed")
    overlay_root = output / "_compat"
    overlay = overlay_root / "fsr3upscaler" / LUMA_CALLBACK.name
    overlay.parent.mkdir(parents=True, exist_ok=True)
    overlay.write_text(source.replace(LUMA_DECLARATION,
        LUMA_DECLARATION.replace("rgba8", "rgba16f")), encoding="utf-8")
    return upstream, overlay_root, overlay


def validate_partial_inputs(manifest, sdk, output, compiler, glslang):
    if manifest.get("sdk_commit") != COMMIT or manifest.get("compiler_sha256") != sha(compiler) \
            or manifest.get("glslang_sha256") != sha(glslang):
        raise RuntimeError("Existing FSR shader manifest has different SDK or compiler inputs")
    if manifest.get("sdk_sources") != source_hashes(sdk):
        raise RuntimeError("Existing FSR shader manifest has different SDK sources")
    if len(list(output.glob("*_permutations.h"))) != 40:
        raise RuntimeError("Partial regeneration requires all 40 baseline permutation families")
    for name, expected in manifest.get("headers", {}).items():
        if not (output / name).is_file() or sha(output / name) != expected:
            raise RuntimeError(f"Existing FSR shader header differs from manifest: {name}")


def prune_old_luma_headers(output):
    families = sorted(output.glob(f"{LUMA_PASS}*_permutations.h"))
    if len(families) != 4:
        raise RuntimeError("Expected four luma instability permutation families")
    active = set()
    for family in families:
        match = re.search(r'^#include "([^"/]+\.h)"', family.read_text(encoding="utf-8"), re.MULTILINE)
        if not match or not match.group(1).startswith(LUMA_PASS):
            raise RuntimeError(f"Cannot identify generated luma shader referenced by {family.name}")
        active.add(match.group(1))
    for header in output.glob(f"{LUMA_PASS}*.h"):
        if not header.name.endswith("_permutations.h") and header.name not in active:
            header.unlink()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--threads", type=int, default=2)
    parser.add_argument("--only-luma-fix", action="store_true",
                        help="regenerate only the four affected luma-instability shader families")
    args = parser.parse_args()
    sdk, output = args.sdk.resolve(), args.output.resolve()
    head = subprocess.check_output(["git", "-C", str(sdk), "rev-parse", "HEAD"], text=True).strip()
    if head != COMMIT or args.threads < 1:
        parser.error("SDK commit must match pinned v1.1.4 and threads must be positive")
    if subprocess.check_output(["git", "-C", str(sdk), "status", "--porcelain", "--untracked-files=no"], text=True):
        parser.error("SDK tracked inputs must be unmodified")
    output.mkdir(parents=True, exist_ok=True)
    compiler = sdk / "sdk/tools/binary_store/FidelityFX_SC.exe"
    glslang = sdk / "sdk/tools/binary_store/glslangValidator.exe"
    gpu = sdk / "sdk/include/FidelityFX/gpu"
    previous = None
    if args.only_luma_fix:
        manifest_path = output / "manifest.json"
        if not manifest_path.is_file():
            parser.error("--only-luma-fix requires an existing complete manifest")
        previous = json.loads(manifest_path.read_text(encoding="utf-8"))
        validate_partial_inputs(previous, sdk, output, compiler, glslang)
    upstream_callback, overlay_root, overlay_callback = write_luma_compatibility_include(sdk, output)
    # Exact upstream Vulkan and FSR3Upscaler CMake options, with bounded workers.
    base = [str(compiler), "-reflection", "-deps=gcc", "-DFFX_GPU=1",
            "-compiler=glslang", f"-glslangexe={glslang}", "-e", "CS",
            "--target-env", "vulkan1.2", "-S", "comp", "-Os", "-DFFX_GLSL=1",
            f"-num-threads={args.threads}", f"-I{gpu}", f"-I{gpu / 'fsr3upscaler'}"]
    for key, value in {"UPSAMPLE_SAMPLERS_USE_DATA_HALF": 0, "ACCUMULATE_SAMPLERS_USE_DATA_HALF": 0,
                       "REPROJECT_SAMPLERS_USE_DATA_HALF": 1, "POSTPROCESSLOCKSTATUS_SAMPLERS_USE_DATA_HALF": 0,
                       "UPSAMPLE_USE_LANCZOS_TYPE": 2}.items():
        base.append(f"-DFFX_FSR3UPSCALER_OPTION_{key}={value}")
    for key in ["REPROJECT_USE_LANCZOS_TYPE", "HDR_COLOR_INPUT", "LOW_RESOLUTION_MOTION_VECTORS",
                "JITTERED_MOTION_VECTORS", "INVERTED_DEPTH", "APPLY_SHARPENING"]:
        base.append(f"-DFFX_FSR3UPSCALER_OPTION_{key}={{0,1}}")
    commands = []
    first_include = next(i for i, arg in enumerate(base) if arg.startswith("-I"))
    for shader in sorted((sdk / "sdk/src/backends/vk/shaders/fsr3upscaler").glob("*.glsl")):
        for suffix, half in [("", 0), ("_wave64", 0), ("_16bit", 1), ("_wave64_16bit", 1)]:
            prefix = (base[:first_include] + [f"-I{overlay_root}"] + base[first_include:]
                      if shader.stem == LUMA_PASS else base)
            command = prefix + [f"-name={shader.stem}{suffix}", f"-DFFX_HALF={half}",
                              f"-output={output}", str(shader)]
            commands.append(command)
            if not args.only_luma_fix or shader.stem == LUMA_PASS:
                print(shader.stem + suffix, flush=True)
                subprocess.run(command, cwd=output, check=True)
    prune_old_luma_headers(output)
    headers = sorted(output.glob("*.h"))
    if len(list(output.glob("*_permutations.h"))) != 40:
        raise RuntimeError("Expected ten passes times four permutation families")
    shutil.copyfile(sdk / "LICENSE.txt", output / "LICENSE-FidelityFX.txt")
    manifest = {"sdk_commit": COMMIT, "sdk_version": "1.1.4", "fsr_version": "3.1.4",
                "api": "Vulkan", "compiler_sha256": sha(compiler), "glslang_sha256": sha(glslang),
                "commands": commands, "headers": {p.name: sha(p) for p in headers},
                "license_sha256": sha(output / "LICENSE-FidelityFX.txt"),
                "sdk_sources": source_hashes(sdk),
                "shader_compatibility": {
                    "reason": "FSR3 luma history storage image is RGBA16F in the pinned SDK host",
                    "upstream": LUMA_CALLBACK.as_posix(),
                    "upstream_sha256": sha(upstream_callback),
                    "generated_override": overlay_callback.relative_to(output).as_posix(),
                    "generated_override_sha256": sha(overlay_callback),
                    "replacement": "rw_luma_history rgba8 to rgba16f"}}
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


def source_hashes(sdk):
    folders = ["sdk/include", "sdk/src/shared", "sdk/src/components/fsr3upscaler",
               "sdk/src/backends/shared", "sdk/src/backends/vk"]
    return {p.relative_to(sdk).as_posix(): sha(p) for folder in folders
            for p in sorted((sdk / folder).rglob("*")) if p.is_file()}


if __name__ == "__main__":
    main()
