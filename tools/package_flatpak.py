"""Build a local Flatpak from this checkout and explicit, offline build inputs.

Stages Git-tracked working-tree files (including nested submodules), the explicit
Plume patch header, existing generated PPC output and selected private build
inputs. FSR sources come from the prepared shader manifest. Only the cleaned
install tree is exported. Requires flatpak-builder/flatpak on the build host;
never installs or launches the resulting bundle.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess


APP_ID = "io.github.freefrank.LostOdysseyRecomp"
BRANCH = "dev"
SDK_COMMITS = {
    "ngx": "374959484e79a640feaba44c93ac8cfb0a03f5b5",
    "fsr": "c6efa6bf7f2027b3ec94f28578bb5965eabb9e55",
    "ffmpeg": "15ece0882e8d5875051ff5b73c5a8326f7cee9f5",
    "zstd": "f8745da6ff1ad1e7bab384bd1f9d742439278e99",
}
SDK_FILES = {
    "ngx": ("include/nvsdk_ngx_vk.h", "lib/Linux_x86_64/libnvsdk_ngx.a",
            "lib/Linux_x86_64/rel/libnvidia-ngx-dlss.so.310.9.1", "LICENSE.txt"),
    "fsr": ("sdk/src/components/fsr3upscaler/ffx_fsr3upscaler.cpp", "LICENSE.txt"),
    "ffmpeg": ("libavcodec/avcodec.h", "COPYING.LGPLv2.1"),
    "zstd": ("build/cmake/CMakeLists.txt",),
}
TEMPLATE = "packaging/linux/io.github.freefrank.LostOdysseyRecomp.json"
PPC_PATH = "LostOdysseyRecompLib/ppc"
PRIVATE_INPUTS = {
    "default_xex": "LostOdysseyRecompLib/private/disc1/default.xex",
    "image_disc1": "LostOdysseyRecompLib/private/image_disc1.bin",
    "image_sym": "LostOdysseyRecompLib/private/image_disc1.bin.sym",
}
SHADER_INPUTS = ("manifest.json", "adapter-manifest.json", "fsr_prepare_spv.h",
                  "fsr_present_spv.h", "LICENSE-FidelityFX.txt")
PLUME_PATCH_HEADER = Path("thirdparty/plume/plume_log.h")
SDL_PATCH = Path("packaging/linux/patches/sdl2-pipewire-node-type.patch")
SDL_PATCH_UPSTREAM = "https://github.com/libsdl-org/SDL/commit/6be87ceb33a9aad3bf5204bb13b3a5e8b498fd26"
SDL_PATCH_COMMAND = (
    "python3 -c 'from pathlib import Path; "
    f'p=Path("{SDL_PATCH.as_posix()}"); '
    's=Path("thirdparty/SDL/src/audio/pipewire/SDL_pipewire.c"); '
    'b=p.read_bytes().replace(b"\\r\\n", b"\\n"); '
    'Path(".flatpak-sdl-pipewire.patch").write_bytes('
    'b.replace(b"\\n", b"\\r\\n") if b"\\r\\n" in s.read_bytes() else b)'
    "' && patch -d thirdparty/SDL -p1 --binary --fuzz=0 --forward --batch "
    '-i ../../.flatpak-sdl-pipewire.patch && rm .flatpak-sdl-pipewire.patch'
)
FSR_SOURCE_DIRS = ("sdk/include/", "sdk/src/shared/", "sdk/src/components/fsr3upscaler/",
                   "sdk/src/backends/shared/", "sdk/src/backends/vk/")
INSTALLED = {
    "manifest.json",  # flatpak-builder --finish-only metadata
    "bin/LostOdysseyRecomp",
    "bin/libnvidia-ngx-dlss.so.310.9.1",
    "bin/libnvidia-ngx-dlss.so",
    "bin/libnvidia-ngx-dlss.so.1",
    "bin/shaders/portable_vk.lospv",
    "lib/libdxcompiler.so",
    "share/licenses/lost-odyssey-recomp/NVIDIA-DLSS/LICENSE.txt",
    "share/licenses/lost-odyssey-recomp/NVIDIA-DLSS/NOTICE.txt",
    "share/licenses/lost-odyssey-recomp/LICENSE-FidelityFX.txt",
    "share/licenses/lost-odyssey-recomp/zstd-LICENSE.txt",
    "share/applications/io.github.freefrank.LostOdysseyRecomp.desktop",
    "share/icons/hicolor/256x256/apps/io.github.freefrank.LostOdysseyRecomp.png",
    "share/metainfo/io.github.freefrank.LostOdysseyRecomp.metainfo.xml",
}
OPTIONAL_APPSTREAM = {
    f"share/app-info/icons/flatpak/{size}x{size}/{APP_ID}.{extension}"
    for size in (64, 128) for extension in ("png", "jxl")
} | {f"share/app-info/xmls/{APP_ID}.xml.gz"}


def git_output(repo: Path, *args: str) -> bytes:
    return subprocess.check_output(("git", "-C", str(repo), *args), stderr=subprocess.PIPE)


def required_file(path: Path, label: str) -> Path:
    if path.is_symlink() or not path.is_file():
        raise ValueError(f"Missing regular {label}: {path}")
    return path


def within(path: Path, root: Path, label: str) -> Path:
    path, root = path.resolve(), root.resolve()
    if not path.is_relative_to(root):
        raise ValueError(f"{label} must be inside {root}: {path}")
    return path


def safe_relative(name: str) -> Path:
    path = PurePosixPath(name)
    if not name or path.is_absolute() or any(part in (".", "..") for part in path.parts) or "\\" in name:
        raise ValueError(f"Invalid staged path: {name!r}")
    return Path(*path.parts)


def tracked_paths(repo: Path, pathspecs: tuple[str, ...] = ()) -> tuple[list[Path], dict[str, str]]:
    """Enumerate the index, then copy the corresponding current worktree bytes."""
    paths: list[Path] = []
    commits: dict[str, str] = {}

    def visit(directory: Path, prefix: Path) -> None:
        commits[prefix.as_posix() if prefix.parts else "."] = git_output(directory, "rev-parse", "HEAD").decode().strip()
        scope = ("--", *pathspecs) if not prefix.parts and pathspecs else ()
        entries = git_output(directory, "ls-files", "--stage", "-z", *scope)
        for entry in entries.split(b"\0"):
            if not entry:
                continue
            metadata, raw = entry.split(b"\t", 1)
            mode, _, stage = metadata.split(b" ")
            relative = safe_relative(os.fsdecode(raw))
            if stage != b"0":
                raise ValueError(f"Unmerged Git index entry: {prefix / relative}")
            if mode == b"160000":
                submodule = directory / relative
                if not (submodule / ".git").exists():
                    raise ValueError(f"Submodule not initialized: {submodule}")
                visit(submodule, prefix / relative)
            elif mode in (b"100644", b"100755", b"120000"):
                source = directory / relative
                if not source.exists() and not source.is_symlink():
                    raise ValueError(f"Tracked worktree file missing: {source}")
                paths.append(prefix / relative)
            else:
                raise ValueError(f"Unsupported tracked file mode {mode!r}: {prefix / relative}")

    visit(repo, Path())
    return sorted(set(paths)), commits


def copy_selected(source_root: Path, staged_root: Path, selected: list[Path]) -> None:
    for relative in selected:
        source = source_root / relative
        destination = staged_root / relative
        if destination.exists() or destination.is_symlink():
            raise ValueError(f"Duplicate staged path: {relative}")
        destination.parent.mkdir(parents=True, exist_ok=True)
        if source.is_symlink():
            link = os.readlink(source)
            if Path(link).is_absolute() or not source.resolve(strict=True).is_relative_to(source_root.resolve()):
                raise ValueError(f"Tracked symlink leaves source tree: {source}")
            destination.symlink_to(link)
        elif source.is_file():
            shutil.copy2(source, destination)
        else:
            raise ValueError(f"Missing regular staged input: {source}")


def stage_checkout(repo: Path, staged: Path, ppc: Path, codegen_manifest: Path,
                   private: dict[str, Path]) -> dict[str, str]:
    repo, ppc, codegen_manifest = repo.resolve(), ppc.resolve(), codegen_manifest.resolve()
    if within(ppc, repo, "PPC directory") != repo / PPC_PATH or codegen_manifest != ppc / "codegen-manifest.json":
        raise ValueError("PPC and codegen-manifest must belong to this checkout's LostOdysseyRecompLib/ppc")
    manifest = json.loads(required_file(codegen_manifest, "codegen manifest").read_text(encoding="utf-8"))
    outputs = manifest.get("outputs")
    if manifest.get("schema") != 1 or not isinstance(outputs, dict) or not outputs:
        raise ValueError("Existing codegen manifest has no recorded PPC output set")
    selected, commits = tracked_paths(repo)
    # The local Plume patch adds this header without adding it to the submodule index.
    required_file(repo / PLUME_PATCH_HEADER, "Plume patch header")
    selected.append(PLUME_PATCH_HEADER)
    required_file(repo / SDL_PATCH, "SDL PipeWire patch")
    selected.append(SDL_PATCH)
    for name, expected in PRIVATE_INPUTS.items():
        if within(private[name], repo, name) != repo / expected:
            raise ValueError(f"{name} must be {expected} in the same checkout")
        required_file(private[name], name)
        selected.append(Path(expected))
    for name in outputs:
        relative = safe_relative(name)
        if not relative.is_relative_to(PPC_PATH):
            raise ValueError(f"Generated file outside PPC: {relative}")
        required_file(repo / relative, "generated PPC output")
        selected.append(relative)
    selected.append(Path(PPC_PATH) / "codegen-manifest.json")
    copy_selected(repo, staged, sorted(set(selected)))
    return commits


def stage_git_sdk(source: Path, destination: Path, label: str) -> str:
    if not (source / ".git").exists():
        raise ValueError(f"Missing Git checkout for {label}: {source}")
    # The NGX checkout has an uninitialized NVIDIAImageScaling submodule, which
    # is unrelated to its Vulkan headers/bootstrap/runtime. Stage only those inputs.
    selected, commits = tracked_paths(source, ("include", *SDK_FILES["ngx"])) if label == "ngx" else tracked_paths(source)
    commit = commits["."]
    if commit != SDK_COMMITS[label]:
        raise ValueError(f"{label} checkout {commit} differs from pinned {SDK_COMMITS[label]}")
    for name in SDK_FILES[label]:
        required_file(source / name, f"{label} SDK input")
    copy_selected(source, destination, selected)
    return commit


def stage_fsr_sdk(source: Path, destination: Path, shaders: Path) -> str:
    """Stage the prepared artifact's SDK sources; CI does not ship its Git checkout."""
    manifest = json.loads(required_file(shaders / "manifest.json", "FSR shader manifest").read_text(encoding="utf-8"))
    sources = manifest.get("sdk_sources")
    commit = manifest.get("sdk_commit")
    if commit != SDK_COMMITS["fsr"] or not isinstance(sources, dict) or not sources:
        raise ValueError("FSR shader manifest lacks pinned SDK sources")
    selected = {Path("LICENSE.txt")}
    for name in sources:
        relative = safe_relative(name)
        if not any(name.startswith(folder) for folder in FSR_SOURCE_DIRS):
            raise ValueError(f"FSR shader manifest source outside SDK directories: {name}")
        selected.add(relative)
    if Path(SDK_FILES["fsr"][0]) not in selected:
        raise ValueError("FSR shader manifest lacks required upscaler source")
    for relative in selected:
        required_file(source / relative, "FSR SDK input")
    copy_selected(source, destination, sorted(selected))
    return commit


def stage_shader_inputs(source: Path, destination: Path) -> None:
    shader_manifest = json.loads(required_file(source / "manifest.json", "FSR shader manifest").read_text(encoding="utf-8"))
    headers = shader_manifest.get("headers")
    if shader_manifest.get("sdk_commit") != SDK_COMMITS["fsr"] or not isinstance(headers, dict) or not headers:
        raise ValueError("FSR shader headers do not identify the pinned SDK")
    selected = {safe_relative(name) for name in headers}
    selected.update(map(Path, SHADER_INPUTS))
    for relative in selected:
        required_file(source / relative, "prepared FSR shader input")
    copy_selected(source, destination, sorted(selected))


def prepare_manifest(template: Path, staged: Path, output: Path) -> dict:
    manifest = json.loads(required_file(template, "Flatpak manifest").read_text(encoding="utf-8"))
    module, = manifest["modules"]
    source, = module["sources"]
    if manifest["app-id"] != APP_ID or module["name"] != "LostOdysseyRecomp" or source != {"type": "dir", "path": "__STAGED_SOURCE__"}:
        raise ValueError("Flatpak template must use only the explicit staged source placeholder")
    commands = module["build-commands"]
    if len(commands) != 5 or commands[0] != SDL_PATCH_COMMAND or "-DLO_REQUIRE_DLSS=ON" not in commands[1] or "-DLO_REQUIRE_FSR=ON" not in commands[1] or "-DFETCHCONTENT_FULLY_DISCONNECTED=ON" not in commands[1] or "-DSDL_PIPEWIRE=ON" not in commands[1]:
        raise ValueError("Flatpak build command is missing its offline required-provider contract")
    source["path"] = str(staged.resolve())
    (output / "build-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return manifest


def inspect_install_tree(files: Path) -> None:
    if not files.is_dir():
        raise ValueError(f"Flatpak-builder install tree missing: {files}")
    names = {path.relative_to(files).as_posix() for path in files.rglob("*") if path.is_file() or path.is_symlink()}
    missing = INSTALLED - names
    if missing:
        raise ValueError(f"Install missing required runtime/license/shader files: {sorted(missing)}")
    if extra := names - INSTALLED - OPTIONAL_APPSTREAM:
        raise ValueError(f"Install contains unexpected paths (including possible sources/private): {sorted(extra)}")
    for name in names:
        entry = files / name
        if entry.is_symlink() and (Path(os.readlink(entry)).is_absolute() or not entry.resolve(strict=True).is_relative_to(files.resolve())):
            raise ValueError(f"Install symlink leaves export tree: {name}")
    for alias in ("libnvidia-ngx-dlss.so", "libnvidia-ngx-dlss.so.1"):
        link = files / "bin" / alias
        if not link.is_symlink() or os.readlink(link) != "libnvidia-ngx-dlss.so.310.9.1":
            raise ValueError(f"NGX alias is not relative to its fixed runtime: {link}")


def source_version(repo: Path) -> str:
    content = required_file(repo / "CMakeLists.txt", "root CMakeLists.txt").read_text(encoding="utf-8")
    match = re.search(r"\bproject\(LostOdysseyRecomp\s+VERSION\s+(\d+\.\d+\.\d+)\b", content)
    if not match:
        raise ValueError("Cannot determine checkout's real source version")
    return match.group(1)


def sha256(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def run(argv: list[str]) -> None:
    subprocess.run(argv, check=True)


def package(args: argparse.Namespace) -> dict:
    repo = args.source.resolve(strict=True)
    output = args.output.resolve()
    if output.exists():
        raise ValueError(f"Output directory already exists: {output}")
    if output == repo or repo.is_relative_to(output):
        raise ValueError("Output must not contain the source checkout")
    inputs = {name: getattr(args, name).resolve() for name in PRIVATE_INPUTS}
    for name in ("ngx_sdk", "fsr_sdk", "fsr_shaders", "shader_pack", "ffmpeg_source", "zstd_source"):
        path = getattr(args, name).resolve(strict=True)
        if path == output or path.is_relative_to(output):
            raise ValueError(f"{name} may not come from the new output tree")
    required_file(args.shader_pack.resolve(), "portable shader pack")
    version = source_version(repo)
    root_status = git_output(repo, "status", "--porcelain=v1", "--untracked-files=normal", "--ignore-submodules=none")
    output.mkdir(parents=True)
    staged = output / "source"
    staged.mkdir()
    commits = stage_checkout(repo, staged, args.ppc.resolve(), args.codegen_manifest.resolve(), inputs)
    external = staged / "flatpak-inputs"
    if external.exists():
        raise ValueError("Git source unexpectedly contains flatpak-inputs")
    external.mkdir()
    sdk_commits = {}
    for name, source in (("ngx", args.ngx_sdk), ("ffmpeg", args.ffmpeg_source),
                         ("zstd", args.zstd_source)):
        sdk_commits[name] = stage_git_sdk(source.resolve(), external / name, name)
    fsr_manifest_commit = stage_fsr_sdk(args.fsr_sdk.resolve(), external / "fsr",
                                       args.fsr_shaders.resolve())
    stage_shader_inputs(args.fsr_shaders.resolve(), external / "fsr-shaders")
    shutil.copy2(args.shader_pack.resolve(), external / "portable_vk.lospv")
    manifest = prepare_manifest(repo / TEMPLATE, staged, output)
    app_id = manifest["app-id"]
    runtime_ref = f'{manifest["runtime"]}/x86_64/{manifest["runtime-version"]}'
    dirty = bool(root_status) or any(git_output(getattr(args, name).resolve(), "status", "--porcelain=v1", "--untracked-files=normal", "--ignore-submodules=none") for name in ("ngx_sdk", "ffmpeg_source", "zstd_source"))
    source = {"version": version, "commit": commits["."], "dirty": dirty,
              "submodules": {key: value for key, value in commits.items() if key != "."},
              "sdk_commits": sdk_commits, "fsr_manifest_commit": fsr_manifest_commit,
              "sdl_patch": {"path": SDL_PATCH.as_posix(), "upstream": SDL_PATCH_UPSTREAM,
                            "sha256": sha256(repo / SDL_PATCH)},
              "runtime_ref": runtime_ref,
              "sdk": manifest["sdk"], "sdk_extensions": manifest.get("sdk-extensions", []),
              "app_id": app_id, "branch": BRANCH}
    build = output / "builder"
    manifest_path = output / "build-manifest.json"
    builder = ["flatpak-builder", f"--default-branch={BRANCH}",
               f"--state-dir={output / 'builder-state'}"]
    run([*builder, "--force-clean", "--build-only", str(build), str(manifest_path)])
    run([*builder, "--finish-only", str(build), str(manifest_path)])
    inspect_install_tree(build / "files")
    run([*builder, f"--repo={output / 'repo'}", "--export-only", str(build), str(manifest_path)])
    bundle = output / f"LostOdysseyRecomp-v{version}-{commits['.'][:8]}-dev.flatpak"
    run(["flatpak", "build-bundle", str(output / "repo"), str(bundle), app_id, BRANCH])
    source["bundle"] = bundle.name
    source["sha256"] = sha256(bundle)
    (output / (bundle.name + ".sha256")).write_text(f'{source["sha256"]}  {bundle.name}\n', encoding="utf-8")
    (output / "source.json").write_text(json.dumps(source, indent=2) + "\n", encoding="utf-8")
    return source


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("source", "output", "ppc", "codegen-manifest", "default-xex",
                   "image-disc1", "image-sym", "ngx-sdk", "fsr-sdk", "fsr-shaders",
                   "shader-pack", "ffmpeg-source", "zstd-source"):
        parser.add_argument("--" + option, type=Path, required=True)
    args = parser.parse_args()
    try:
        result = package(args)
    except (OSError, ValueError, KeyError, json.JSONDecodeError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Flatpak packaging failed: {error}\n")
    print(json.dumps({"bundle": result["bundle"], "sha256": result["sha256"],
                      "source": result["commit"], "version": result["version"]}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
