"""Fetch or reassemble the precompiled portable shader pack (.lospv) for release packaging.

Sources inspected in priority order:
1. LO_PORTABLE_SHADER_PACK environment variable (if pointing to an existing file)
2. Existing nonempty destination file
3. Private build input directory (out/build-input/shaders/ or out/ppc-input/shaders/):
   - Whole file: portable_vk.lospv
   - Split parts: portable_vk.lospv.* (e.g. .00, .01)
4. GitHub release asset:
   - Download LostOdysseyRecomp-shader-pack-vk12-*.zip from draft/published release
   - Optionally use a pinned previous release as a build input
5. Local candidate paths (for local development builds):
   - D:/Mihoyo/LostOdysseyRecomp-windows-x64/shaders/portable_vk.lospv
   - /mnt/d/Mihoyo/LostOdysseyRecomp-windows-x64/shaders/portable_vk.lospv
   - ROOT / shaders / portable_vk.lospv
"""
from __future__ import annotations
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]


def try_copy(source: Path, destination: Path) -> bool:
    if source.is_file() and source.resolve() != destination.resolve():
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
        return True
    return False


def try_assemble_parts(parts_dir: Path, destination: Path) -> bool:
    if not parts_dir.is_dir():
        return False
    parts = sorted(parts_dir.glob("portable_vk.lospv.*"))
    if not parts:
        return False
    destination.parent.mkdir(parents=True, exist_ok=True)
    temp_target = destination.with_suffix(".tmp")
    try:
        with open(temp_target, "wb") as out_f:
            for part in parts:
                with open(part, "rb") as in_f:
                    shutil.copyfileobj(in_f, out_f, length=1024 * 1024)
        temp_target.replace(destination)
        return True
    except Exception:
        temp_target.unlink(missing_ok=True)
        return False


def try_download_release_asset(tag: str, destination: Path) -> bool:
    try:
        with tempfile.TemporaryDirectory(prefix="shader-pack-dl-") as tmp:
            tmp_dir = Path(tmp)
            cmd = ["gh", "release", "download"]
            if tag:
                cmd.append(tag)
            cmd.extend(["--pattern", "LostOdysseyRecomp-shader-pack-vk12-*.zip", "--dir", str(tmp_dir)])
            subprocess.run(cmd, check=True, capture_output=True, text=True)
            archives = list(tmp_dir.glob("LostOdysseyRecomp-shader-pack-vk12-*.zip"))
            if not archives:
                return False
            with zipfile.ZipFile(archives[0], "r") as zf:
                for member in zf.namelist():
                    if member.endswith("portable_vk.lospv"):
                        destination.parent.mkdir(parents=True, exist_ok=True)
                        with zf.open(member) as src, open(destination, "wb") as dst:
                            shutil.copyfileobj(src, dst, length=1024 * 1024)
                        return True
    except Exception as e:
        print(f"Release asset download attempt notice: {e}", file=sys.stderr)
    return False


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target-dir", type=Path, required=True,
                        help="Target shaders directory (e.g. runtime_dir/shaders)")
    parser.add_argument("--tag", type=str, default="",
                        help="Release tag to look up on GitHub (e.g. v0.5.20)")
    parser.add_argument("--fallback-tag", type=str, default="",
                        help="Pinned published release to use when the requested release has no shader asset")
    parser.add_argument("--required", action=argparse.BooleanOptionalAction, default=True,
                        help="Fail if unavailable; --no-required explicitly permits no bundled pack")
    args = parser.parse_args()

    target_dir = args.target_dir.resolve()
    target_pack = target_dir / "portable_vk.lospv"
    env_override = os.environ.get("LO_PORTABLE_SHADER_PACK")
    if env_override and not Path(env_override).is_file():
        parser.error("LO_PORTABLE_SHADER_PACK is set but does not name a file")
    # An explicit candidate always wins over an old destination.
    if target_pack.is_file() and target_pack.stat().st_size and not env_override:
        print(f"Existing shader pack: {target_pack.stat().st_size} bytes")
        export_env(target_pack)
        return 0
    if target_pack.is_file() and not target_pack.stat().st_size:
        target_pack.unlink()

    acquired = False

    # 2. Check LO_PORTABLE_SHADER_PACK environment variable
    env_override = os.environ.get("LO_PORTABLE_SHADER_PACK")
    if env_override and Path(env_override).is_file():
        print(f"Acquiring shader pack from LO_PORTABLE_SHADER_PACK: {env_override}")
        acquired = Path(env_override).resolve() == target_pack.resolve() or try_copy(Path(env_override), target_pack)

    # 3. Check private build input directories (whole or split parts)
    if not acquired:
        for input_dir in [ROOT / "out/shader-input/shaders", ROOT / "out/build-input/shaders", ROOT / "out/ppc-input/shaders"]:
            if (input_dir / "portable_vk.lospv").is_file():
                print(f"Acquiring shader pack from {input_dir / 'portable_vk.lospv'}")
                acquired = try_copy(input_dir / "portable_vk.lospv", target_pack)
                if acquired:
                    break
            if try_assemble_parts(input_dir, target_pack):
                print(f"Reassembled shader pack parts from {input_dir}")
                acquired = True
                break

    # 4. Check GitHub release asset
    if not acquired:
        tag = args.tag.strip() or os.environ.get("RELEASE_TAG", "")
        print(f"Attempting to download shader pack asset from GitHub release ({tag or 'latest'})...")
        acquired = try_download_release_asset(tag, target_pack)
        if not acquired and args.fallback_tag and args.fallback_tag != tag:
            print(f"Attempting pinned shader build input from {args.fallback_tag}...")
            acquired = try_download_release_asset(args.fallback_tag, target_pack)

    # 5. Check local development locations
    if not acquired:
        local_candidates = [
            Path(r"D:\Mihoyo\LostOdysseyRecomp-windows-x64\shaders\portable_vk.lospv"),
            Path("/mnt/d/Mihoyo/LostOdysseyRecomp-windows-x64/shaders/portable_vk.lospv"),
            ROOT / "shaders/portable_vk.lospv",
        ]
        for candidate in local_candidates:
            if candidate.is_file():
                print(f"Acquiring shader pack from local path: {candidate}")
                acquired = try_copy(candidate, target_pack)
                if acquired:
                    break

    if not acquired or not target_pack.is_file() or not target_pack.stat().st_size:
        target_pack.unlink(missing_ok=True)
        if args.required:
            sys.exit("Error: Could not retrieve portable shader pack (portable_vk.lospv) from any source.")
        else:
            print("Warning: Portable shader pack not found. Packaging will proceed without bundled shaders.")
            return 0

    print(f"Portable shader pack staged: {target_pack.stat().st_size} bytes")

    export_env(target_pack)
    return 0


def export_env(pack_path: Path):
    github_env = os.environ.get("GITHUB_ENV")
    if github_env and Path(github_env).is_file():
        with open(github_env, "a", encoding="utf-8") as f:
            f.write(f"LO_PORTABLE_SHADER_PACK={pack_path.resolve()}\n")
        print(f"Exported LO_PORTABLE_SHADER_PACK to GITHUB_ENV: {pack_path.resolve()}")


if __name__ == "__main__":
    sys.exit(main())
