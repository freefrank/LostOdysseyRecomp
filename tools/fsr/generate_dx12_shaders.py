"""Prepare pinned FSR 3.1.4 D3D12 DXIL permutations offline on Windows."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


COMMIT = "c6efa6bf7f2027b3ec94f28578bb5965eabb9e55"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_hashes(sdk):
    folders = ["sdk/include", "sdk/src/shared", "sdk/src/components/fsr3upscaler",
               "sdk/src/backends/shared", "sdk/src/backends/dx12"]
    return {p.relative_to(sdk).as_posix(): sha(p) for folder in folders
            for p in sorted((sdk / folder).rglob("*")) if p.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--dxc-dir", type=Path, required=True,
                        help="directory containing dxcompiler.dll and dxil.dll")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--threads", type=int, default=2)
    args = parser.parse_args()
    sdk, dxc_dir, output = args.sdk.resolve(), args.dxc_dir.resolve(), args.output.resolve()
    if args.threads < 1 or subprocess.check_output(
            ["git", "-C", str(sdk), "rev-parse", "HEAD"], text=True).strip() != COMMIT:
        parser.error("SDK commit must match pinned v1.1.4 and threads must be positive")
    if subprocess.check_output(["git", "-C", str(sdk), "status", "--porcelain",
                                "--untracked-files=no"], text=True):
        parser.error("SDK tracked inputs must be unmodified")
    compiler = sdk / "sdk/tools/binary_store/FidelityFX_SC.exe"
    dxc = dxc_dir / "dxcompiler.dll"
    dxil = dxc_dir / "dxil.dll"
    if not compiler.is_file() or not dxc.is_file() or not dxil.is_file():
        parser.error("FidelityFX_SC.exe, dxcompiler.dll and dxil.dll are required")
    output.mkdir(parents=True, exist_ok=True)
    gpu = sdk / "sdk/include/FidelityFX/gpu"
    environment = os.environ.copy()
    environment["PATH"] = str(dxc_dir) + os.pathsep + environment.get("PATH", "")
    base = [str(compiler), "-reflection", "-deps=gcc", "-DFFX_GPU=1",
            "-compiler=dxc", f"-dxcdll={dxc}", "-E", "CS", "-DFFX_HLSL=1",
            f"-num-threads={args.threads}", f"-I {gpu}", f"-I {gpu / 'fsr3upscaler'}"]
    for key, value in {"UPSAMPLE_SAMPLERS_USE_DATA_HALF": 0,
                       "ACCUMULATE_SAMPLERS_USE_DATA_HALF": 0,
                       "REPROJECT_SAMPLERS_USE_DATA_HALF": 1,
                       "POSTPROCESSLOCKSTATUS_SAMPLERS_USE_DATA_HALF": 0,
                       "UPSAMPLE_USE_LANCZOS_TYPE": 2}.items():
        base.append(f"-DFFX_FSR3UPSCALER_OPTION_{key}={value}")
    for key in ["REPROJECT_USE_LANCZOS_TYPE", "HDR_COLOR_INPUT",
                "LOW_RESOLUTION_MOTION_VECTORS", "JITTERED_MOTION_VECTORS",
                "INVERTED_DEPTH", "APPLY_SHARPENING"]:
        base.append(f"-DFFX_FSR3UPSCALER_OPTION_{key}={{0,1}}")
    commands = []
    for shader in sorted((sdk / "sdk/src/backends/dx12/shaders/fsr3upscaler").glob("*.hlsl")):
        for suffix, half, wave64 in [("", 0, False), ("_wave64", 0, True),
                                     ("_16bit", 1, False), ("_wave64_16bit", 1, True)]:
            command = base + [f"-name={shader.stem}{suffix}", f"-DFFX_HALF={half}",
                              "-T", "cs_6_6" if wave64 else "cs_6_2",
                              "-DFFX_HLSL_SM=66" if wave64 else "-DFFX_HLSL_SM=62"]
            if wave64:
                command.append('-DFFX_PREFER_WAVE64=[WaveSize(64)]')
            if half:
                command.append("-enable-16bit-types")
            command += [f"-output={output}", str(shader)]
            commands.append(command)
            print(shader.stem + suffix, flush=True)
            subprocess.run(command, cwd=output, env=environment, check=True)
    headers = sorted(output.glob("*.h"))
    if len(list(output.glob("*_permutations.h"))) != 40:
        raise RuntimeError("Expected ten passes times four permutation families")
    shutil.copyfile(sdk / "LICENSE.txt", output / "LICENSE-FidelityFX.txt")
    manifest = {"sdk_commit": COMMIT, "sdk_version": "1.1.4", "fsr_version": "3.1.4",
                "api": "D3D12", "compiler_sha256": sha(compiler),
                "dxcompiler_sha256": sha(dxc), "dxil_sha256": sha(dxil),
                "commands": commands, "headers": {p.name: sha(p) for p in headers},
                "license_sha256": sha(output / "LICENSE-FidelityFX.txt"),
                "sdk_sources": source_hashes(sdk)}
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
