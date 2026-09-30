"""Package the precompiled portable shader pack (.lospv) or startup bundle into a release asset archive."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[2]


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(1024 * 1024):
            h.update(chunk)
    return h.hexdigest()


def detect_version() -> str:
    try:
        tag = subprocess.check_output(
            ["git", "describe", "--tags", "--exact-match"],
            cwd=ROOT, text=True, stderr=subprocess.DEVNULL
        ).strip()
        if tag:
            return tag
    except Exception:
        pass
    try:
        ver = subprocess.check_output(
            ["git", "describe", "--tags", "--always"],
            cwd=ROOT, text=True, stderr=subprocess.DEVNULL
        ).strip()
        if ver:
            return ver
    except Exception:
        pass
    return "v0.5.20"


def find_pack_tool() -> Path | None:
    candidates = [
        ROOT / "out/build/windows-clang/LostOdysseyRecomp/LoShaderPackTool.exe",
        ROOT / "out/build/linux-clang/LostOdysseyRecomp/LoShaderPackTool",
        Path(r"D:\Mihoyo\LostOdysseyRecomp-windows-x64\LoShaderPackTool.exe"),
        Path("/mnt/d/Mihoyo/LostOdysseyRecomp-windows-x64/LoShaderPackTool"),
    ]
    for c in candidates:
        if c.is_file() and not c.is_symlink():
            return c
    return None


def verify_portable_pack(pack_path: Path, tool: Path) -> dict:
    result = subprocess.run([str(tool), "verify", str(pack_path)], check=True,
                            capture_output=True, text=True, timeout=600)
    report = json.loads(result.stdout)
    if not report.get("all_payloads_verified") or report.get("file_bytes") != pack_path.stat().st_size:
        raise ValueError("verifier did not validate the staged artifact")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pack", type=Path, help="Path to portable_vk.lospv")
    parser.add_argument("--bundle", type=Path, help="Path to legacy startup_vk12_v1.bundle")
    parser.add_argument("--output", type=Path, default=ROOT / "out/releases",
                        help="Output directory for packaged release assets")
    parser.add_argument("--version", type=str, default="",
                        help="Release version string (e.g. v0.5.15)")
    args = parser.parse_args()

    version = args.version.strip() or detect_version()
    if not version.startswith("v"):
        version = "v" + version

    args.output.mkdir(parents=True, exist_ok=True)

    # 1. Check portable pack candidates first unless bundle explicitly requested
    pack_path = args.pack
    if not pack_path and not args.bundle:
        candidates = [
            ROOT / "shaders/portable_vk.lospv",
            Path(r"D:\Mihoyo\LostOdysseyRecomp-windows-x64\shaders\portable_vk.lospv"),
            Path("/mnt/d/Mihoyo/LostOdysseyRecomp-windows-x64/shaders/portable_vk.lospv"),
        ]
        for c in candidates:
            if c.is_file():
                pack_path = c
                break

    if pack_path and pack_path.is_file():
        tool = find_pack_tool()
        if not tool:
            sys.exit("Error: LoShaderPackTool executable not found. Build it before packaging.")

        print(f"Verifying portable shader pack: {pack_path}...")
        try:
            report = verify_portable_pack(pack_path, tool)
        except Exception as e:
            sys.exit(f"Error: shader pack verification failed: {e}")

        pack_size = pack_path.stat().st_size
        print(f"Source pack: {pack_path} ({pack_size / (1024*1024):.2f} MB)")
        print(f"  Records:          {report.get('records')}")
        print(f"  Unique binaries:  {report.get('unique_binaries')}")
        print(f"  Blocks:           {report.get('blocks')}")
        print(f"  Contract:         {report.get('contract')}")

        zip_name = f"LostOdysseyRecomp-shader-pack-vk12-{version}.zip"
        zip_path = args.output / zip_name

        print(f"Packaging into {zip_path}...")
        with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
            zf.write(pack_path, arcname="shaders/portable_vk.lospv")
            license_path = ROOT / "thirdparty/zstd-LICENSE.txt"
            if license_path.is_file():
                zf.write(license_path, arcname="licenses/zstd-LICENSE.txt")

        zip_size = zip_path.stat().st_size
        zip_hash = sha256_file(zip_path)
        sha_path = zip_path.with_suffix(".zip.sha256")
        sha_path.write_text(f"{zip_hash}  {zip_name}\n", encoding="utf-8")

        print(f"\nSuccess! Created portable shader pack release asset:")
        print(f"  Archive:  {zip_path} ({zip_size / (1024*1024):.2f} MB)")
        print(f"  Checksum: {sha_path} ({zip_hash})")
        print(f"\nPlayers can extract shaders/portable_vk.lospv directly into the game directory for instant zero-compile startup.")
        return

    # 2. Legacy startup bundle packaging
    bundle_path = args.bundle
    if not bundle_path:
        candidates = [
            ROOT / "cache/shaders/startup_vk12_v1.bundle",
            Path(r"D:\Mihoyo\LostOdysseyRecomp-windows-x64\cache\shaders\startup_vk12_v1.bundle"),
            Path("/mnt/d/Mihoyo/LostOdysseyRecomp-windows-x64/cache/shaders/startup_vk12_v1.bundle"),
        ]
        for c in candidates:
            if c.is_file():
                bundle_path = c
                break

    if not bundle_path or not bundle_path.is_file():
        sys.exit("Error: Neither portable_vk.lospv nor startup_vk12_v1.bundle found.")

    bundle_size = bundle_path.stat().st_size
    print(f"Source bundle: {bundle_path} ({bundle_size / (1024*1024):.2f} MB)")
    bundle_hash = sha256_file(bundle_path)
    print(f"Bundle SHA-256: {bundle_hash}")

    zip_name = f"LostOdysseyRecomp-shader-bundle-vk12-{version}.zip"
    zip_path = args.output / zip_name

    print(f"Compressing into {zip_path}...")
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
        zf.write(bundle_path, arcname="cache/shaders/startup_vk12_v1.bundle")

    zip_size = zip_path.stat().st_size
    zip_hash = sha256_file(zip_path)
    sha_path = zip_path.with_suffix(".zip.sha256")
    sha_path.write_text(f"{zip_hash}  {zip_name}\n", encoding="utf-8")

    print(f"\nSuccess! Created shader bundle archive:")
    print(f"  Archive:  {zip_path} ({zip_size / (1024*1024):.2f} MB)")
    print(f"  Checksum: {sha_path} ({zip_hash})")


if __name__ == "__main__":
    main()
