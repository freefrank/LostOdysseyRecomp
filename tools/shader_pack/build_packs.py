"""Build the distribution shader packs with a runtime build, on Windows or Linux.

One entry for every pack the game downloads at startup:

  vulkan  portable_vk.lospv    Vulkan on Windows, Linux and Android, and Metal on macOS
  d3d12   portable_dx12.lospd  Direct3D 12 (Windows only)

and, on request, the Nintendo Switch pack (switch/portable_vk.lospv): Vulkan with
the constants in uniform buffers (LO_VK_UBO_CONSTANTS). It is copied to the
console by hand and never staged.

  python tools/shader_pack/build_packs.py --game <disc1> [--renderer vulkan d3d12]

For each renderer the runtime runs with --prepare-shaders-only and
LO_SHADER_EXPORT_PACK in a work folder under --output, keeping a shader cache
there so a rerun with the same source needs no DXC. --sources adds shader
sources learned during play (the `source` folder of a game's shader cache, or a
collection of them) to the ones the runtime finds in the game files, so the
pack also covers shaders that appear only at run time. LoShaderPackTool then
checks the pack against the runtime contract of disc 1 (verify-runtime).
--stage hands the packs to tools/release/publish_shader_packs.py, which stages
them with the merged index; uploading stays a separate --publish run of that
script.
"""
from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WINDOWS = os.name == "nt"
EXE = "LostOdysseyRecomp.exe" if WINDOWS else "LostOdysseyRecomp"
TOOL = "LoShaderPackTool.exe" if WINDOWS else "LoShaderPackTool"
PACKS = {"vulkan": "portable_vk.lospv", "d3d12": "portable_dx12.lospd", "switch": "switch/portable_vk.lospv"}
# Settings each pack is exported and verified with.
PACK_ENV = {"vulkan": {"LO_GRAPHICS_API": "vulkan"}, "d3d12": {"LO_GRAPHICS_API": "d3d12"},
            "switch": {"LO_GRAPHICS_API": "vulkan", "LO_VK_UBO_CONSTANTS": "1"}}
# Settings under which the runtime ignores or replaces distribution packs.
OVERRIDES = ("LO_SHADER_PACK_PATH", "LO_NO_PORTABLE_SHADER_PACK", "LO_SHADER_FULL_SCAN", "LO_SHADER_HLSL_DIR",
             "LO_SHADER_RETRY_FAILURES", "LO_SHADER_EXPORT_PACK", "LO_NO_SHADER_PREPARE")


def default_runtime() -> Path | None:
    preset = "windows-clang" if WINDOWS else "linux-clang"
    for directory in (ROOT / "out/build" / preset / "LostOdysseyRecomp", ROOT / "out/build/linux/LostOdysseyRecomp"):
        if (directory / EXE).is_file():
            return directory
    return None


def export(runtime: Path, game: Path, renderer: str, output: Path, sources: list[Path]) -> Path:
    work = output / f"work-{renderer}"
    learned = work / "shader-cache/source"
    learned.mkdir(parents=True, exist_ok=True)
    for folder in sources:
        for item in folder.iterdir():
            if item.is_file():
                shutil.copy2(item, learned / item.name)
    # A startup bundle hit skips source discovery, so learned sources added
    # since the last run would be missed. The shader store keeps compiled
    # shaders, so discovery without the bundle needs no DXC for them.
    for bundle in (work / "shader-cache").glob("startup_*.bundle"):
        bundle.unlink()
    executable = runtime / EXE
    if WINDOWS:
        # The runtime loads DXC and SDKs beside itself; run a copy so the build
        # directory gets no logs, caches or settings.
        for item in runtime.iterdir():
            if item.is_file() and (item.suffix.lower() == ".dll" or item.name == EXE):
                shutil.copy2(item, work / item.name)
        executable = work / EXE
    pack = output / PACKS[renderer]
    pack.parent.mkdir(parents=True, exist_ok=True)
    pack.unlink(missing_ok=True)
    env = {key: value for key, value in os.environ.items() if not key.startswith("LO_")}
    env.update(LO_BACKGROUND="1", LO_NO_UPDATE="1", LO_SHADER_PACK_DOWNLOAD="0", LO_AUDIO_MUTE="1",
               LO_SHADER_CACHE_DIR=str(work / "shader-cache"),
               LO_SHADER_EXPORT_PACK=str(pack), LO_LOG_FILE=str(work / "runtime.log"), **PACK_ENV[renderer])
    print(f"{renderer}: preparing shaders with {executable} (log {work / 'runtime.log'})", flush=True)
    started = time.monotonic()
    with open(work / "stdout.log", "wb") as log:
        result = subprocess.run([str(executable), "--game", str(game), "--prepare-shaders-only"], cwd=work, env=env,
                                stdout=log, stderr=subprocess.STDOUT)
    if result.returncode != 0 or not pack.is_file():
        raise SystemExit(f"{renderer}: export failed (exit {result.returncode}); see {work / 'runtime.log'}")
    print(f"{renderer}: {pack} {pack.stat().st_size} bytes in {time.monotonic() - started:.0f} s", flush=True)
    return pack


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--game", type=Path, required=True, help="disc 1 game folder (contains default.xex)")
    parser.add_argument("--renderer", nargs="+", choices=sorted(PACKS),
                        default=["vulkan", "d3d12"] if WINDOWS else ["vulkan"])
    parser.add_argument("--runtime", type=Path, help="build folder with the runtime and LoShaderPackTool")
    parser.add_argument("--tool", type=Path, help="LoShaderPackTool (default: beside the runtime)")
    parser.add_argument("--image", type=Path, default=ROOT / "LostOdysseyRecompLib/private/image_disc1.bin",
                        help="xexdump image of disc 1")
    parser.add_argument("--output", type=Path, default=ROOT / "out/shader-pack-build")
    parser.add_argument("--sources", type=Path, nargs="*", default=[],
                        help="folders of learned shader sources to include")
    parser.add_argument("--stage", action="store_true", help="stage the packs with publish_shader_packs.py")
    args = parser.parse_args()

    if inherited := [key for key in OVERRIDES if os.environ.get(key)]:
        raise SystemExit(f"unset developer shader settings first: {', '.join(inherited)}")
    if "d3d12" in args.renderer and not WINDOWS:
        raise SystemExit("the d3d12 pack needs Windows")
    runtime = args.runtime or default_runtime()
    if runtime is None or not (runtime / EXE).is_file():
        raise SystemExit("no runtime build found; build it (tools/build_runtime.bat on Windows) or pass --runtime")
    tool = args.tool or runtime / TOOL
    if not tool.is_file():
        raise SystemExit(f"{tool} is missing; build the LoShaderPackTool target")
    game = args.game.resolve()
    if not (game / "default.xex").is_file() or not args.image.is_file():
        raise SystemExit("--game must contain default.xex and --image must exist")
    if missing := [str(folder) for folder in args.sources if not folder.is_dir()]:
        raise SystemExit(f"--sources folder missing: {', '.join(missing)}")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)

    packs = []
    for renderer in args.renderer:
        pack = export(runtime.resolve(), game, renderer, output, args.sources)
        env = {key: value for key, value in os.environ.items() if not key.startswith("LO_")}
        env.update(PACK_ENV[renderer])
        report = subprocess.run([str(tool), "verify-runtime", str(pack), str(args.image)], capture_output=True,
                                text=True, env=env)
        if report.returncode != 0:
            raise SystemExit(f"{renderer}: verify-runtime failed: {report.stderr.strip() or report.stdout.strip()}")
        (pack.parent / f"{pack.name}.report.json").write_text(report.stdout, encoding="utf-8")
        print(f"{renderer}: verify-runtime passed", flush=True)
        if renderer != "switch":
            packs.append(pack)
    if not packs:
        return 0

    publish = [sys.executable, str(ROOT / "tools/release/publish_shader_packs.py"), "--tool", str(tool),
               "--image", str(args.image), *map(str, packs)]
    if args.stage:
        return subprocess.run(publish).returncode
    print("Stage (add --publish to upload):\n  " + " ".join(publish))
    return 0


if __name__ == "__main__":
    sys.exit(main())
