"""Synchronize the portable shader pack (.lospv) onto the private build-inputs repository."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
REPOSITORY = "freefrank/LostOdysseyRecomp-build-inputs"
REMOTE = f"https://github.com/{REPOSITORY}.git"
GIT_AUTH = ["git", "-c", "credential.helper=", "-c", "credential.helper=!gh auth git-credential"]
CHUNK_SIZE = 80 * 1024 * 1024


def run(command, *, cwd=None, check=True):
    env = dict(os.environ, GIT_TERMINAL_PROMPT="0", GH_PROMPT_DISABLED="1")
    return subprocess.run(command, cwd=cwd, check=check, text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(1024 * 1024):
            h.update(chunk)
    return h.hexdigest()


def find_tool() -> Path | None:
    candidates = [
        ROOT / "out/build/release/LostOdysseyRecomp/LoShaderPackTool.exe",
        ROOT / "out/build/release/LostOdysseyRecomp/LoShaderPackTool",
        ROOT / "out/build/windows-clang/LostOdysseyRecomp/LoShaderPackTool.exe",
        ROOT / "out/build/linux-clang/LostOdysseyRecomp/LoShaderPackTool",
        Path(r"D:\Mihoyo\LostOdysseyRecomp-windows-x64\LoShaderPackTool.exe"),
        Path("/mnt/d/Mihoyo/LostOdysseyRecomp-windows-x64/LoShaderPackTool"),
    ]
    for c in candidates:
        if c.is_file() and not c.is_symlink():
            return c
    return None


def verify_pack(pack_path: Path, tool: Path) -> dict:
    result = subprocess.run([str(tool), "verify", str(pack_path)], check=True,
                            capture_output=True, text=True, timeout=600)
    report = json.loads(result.stdout)
    if not report.get("all_payloads_verified") or report.get("file_bytes") != pack_path.stat().st_size:
        raise ValueError("Shader pack verification failed")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pack", type=Path, help="Path to portable_vk.lospv")
    parser.add_argument("--dry-run", action="store_true", help="Prepare bundle without pushing")
    args = parser.parse_args()

    pack_path = args.pack
    if not pack_path:
        candidates = [
            ROOT / "shaders/portable_vk.lospv",
            Path(r"D:\Mihoyo\LostOdysseyRecomp-windows-x64\shaders\portable_vk.lospv"),
            Path("/mnt/d/Mihoyo/LostOdysseyRecomp-windows-x64/shaders/portable_vk.lospv"),
        ]
        for c in candidates:
            if c.is_file():
                pack_path = c
                break

    if not pack_path or not pack_path.is_file():
        sys.exit("Error: portable_vk.lospv not found. Specify with --pack.")

    tool = find_tool()
    if tool:
        print(f"Verifying shader pack with {tool.name}...")
        report = verify_pack(pack_path, tool)
        print(f"Verified: {report['records']} records, {report['file_bytes']} bytes")
    else:
        print("Warning: LoShaderPackTool not found; skipping pre-verification")
        report = {}

    total_sha256 = sha256_file(pack_path)
    total_size = pack_path.stat().st_size

    with tempfile.TemporaryDirectory(prefix="shader-sync-") as tmp:
        worktree = Path(tmp) / "worktree"
        worktree.mkdir()
        bundle_dir = Path(tmp) / "bundle"
        bundle_dir.mkdir()

        print(f"Chunking {pack_path.name} into {CHUNK_SIZE // (1024*1024)} MB parts...")
        chunks = []
        with open(pack_path, "rb") as f:
            idx = 0
            while data := f.read(CHUNK_SIZE):
                chunk_name = f"portable_vk.lospv.{idx:02d}"
                chunk_path = bundle_dir / chunk_name
                chunk_path.write_bytes(data)
                chunk_sha = hashlib.sha256(data).hexdigest()
                chunks.append({"name": chunk_name, "size": len(data), "sha256": chunk_sha})
                print(f"  {chunk_name}: {len(data)} bytes ({chunk_sha[:12]}...)")
                idx += 1

        manifest = {
            "schema": 1,
            "target": "portable_vk.lospv",
            "size": total_size,
            "sha256": total_sha256,
            "records": report.get("records", 0),
            "chunks": chunks
        }
        (bundle_dir / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        (bundle_dir / ".gitattributes").write_text("portable_vk.lospv.* binary\nmanifest.json text eol=lf\n", encoding="utf-8")

        if args.dry_run:
            print("Dry run requested; skipping git push.")
            return 0

        def git(*cmd, check=True):
            return run([*GIT_AUTH, *cmd], cwd=worktree, check=check)

        print(f"Fetching {REMOTE} main branch...")
        git("init", "--quiet")
        # The private repository also holds many feedback records. Only the
        # shader bundle is needed here; avoid materializing unrelated files.
        git("sparse-checkout", "set", "--cone", "shaders")
        git("remote", "add", "origin", REMOTE)

        for attempt in range(3):
            git("fetch", "--depth", "1", "--no-tags", "origin", "refs/heads/main")
            base = git("rev-parse", "FETCH_HEAD").stdout.strip()
            git("checkout", "--detach", "--force", base)

            existing_manifest_path = worktree / "shaders/manifest.json"
            if existing_manifest_path.is_file():
                try:
                    existing = json.loads(existing_manifest_path.read_text(encoding="utf-8"))
                    if existing.get("sha256") == total_sha256:
                        print(f"Private shader pack already up to date ({total_sha256[:12]}).")
                        return 0
                except Exception:
                    pass

            target_shaders = worktree / "shaders"
            if target_shaders.exists():
                shutil.rmtree(target_shaders)
            shutil.copytree(bundle_dir, target_shaders)

            git("add", "-A", "--", "shaders")
            git("-c", "user.name=Shader pack sync", "-c", "user.email=shader-sync@users.noreply.github.com",
                "commit", "--quiet", "-m", f"Sync portable shader pack {total_sha256[:12]} ({report.get('records', 0)} records)")
            commit = git("rev-parse", "HEAD").stdout.strip()
            print(f"Pushing commit {commit} to {REMOTE} main...")
            pushed = git("push", "origin", "HEAD:refs/heads/main", check=False)
            if not pushed.returncode:
                print(f"Successfully synchronized shader pack to {REPOSITORY}:main ({commit})")
                return 0
            print(f"Push failed, retrying attempt {attempt + 1}...")

        sys.exit("Error: Failed to push shader pack to private repository.")


if __name__ == "__main__":
    sys.exit(main())
