"""Publish opted-in local PPC builds to immutable private input branches."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from urllib.parse import quote

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ppc_prebuilt

REPOSITORY = "freefrank/LostOdysseyRecomp-build-inputs"
REMOTE = f"https://github.com/{REPOSITORY}.git"
GIT_AUTH = ["git", "-c", "credential.helper=", "-c", "credential.helper=!gh auth git-credential"]


def run(command, *, cwd=None, check=True):
    environment = dict(os.environ, GIT_TERMINAL_PROMPT="0", GH_PROMPT_DISABLED="1")
    return subprocess.run(command, cwd=cwd, check=check, text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=environment)


def identity(root, build_dir):
    evidence = {"fingerprint": ppc_prebuilt.fingerprint(root),
                "contract": ppc_prebuilt.compile_contract(root, build_dir)[0]}
    key = hashlib.sha256(json.dumps(evidence, sort_keys=True, separators=(",", ":"),
                                    ensure_ascii=True).encode("utf-8")).hexdigest()
    return key, evidence


def enabled(root):
    result = run(["git", "-C", str(root), "config", "--local", "--bool", "--get", "lo.ppcAutoSync"], check=False)
    if result.returncode == 1:
        return False
    result.check_returncode()
    return result.stdout.strip().lower() == "true"


def remote_commit(key):
    ref = f"refs/heads/ppc/{key}"
    result = run([*GIT_AUTH, "ls-remote", "--exit-code", REMOTE, ref], check=False)
    if result.returncode == 2:
        return None
    result.check_returncode()
    matches = [line.split() for line in result.stdout.splitlines() if line.strip()]
    if len(matches) != 1 or len(matches[0]) != 2 or matches[0][1] != ref or not re.fullmatch(r"[0-9a-f]{40,64}", matches[0][0]):
        raise ValueError("Unexpected private PPC ref response")
    return matches[0][0]


def remote_manifest(commit):
    path = f"repos/{REPOSITORY}/contents/ppc/manifest.json?ref={quote(commit, safe='')}"
    return json.loads(run(["gh", "api", path, "-H", "Accept: application/vnd.github.raw+json"]).stdout)


def validate_manifest(manifest, evidence):
    if manifest.get("schema") != 1 or any(manifest.get(k) != v for k, v in evidence.items()):
        raise ValueError("Remote PPC manifest does not match source/compile identity")
    library = manifest["library"]
    chunks = manifest["chunks"]
    if library.get("name") != ppc_prebuilt.LIBRARY or not chunks:
        raise ValueError("Invalid remote PPC library metadata")
    for index, chunk in enumerate(chunks):
        if chunk.get("name") != f"ppc-{index:04d}.bin":
            raise ValueError("Invalid remote PPC chunk name")
        if not isinstance(chunk.get("size"), int) or not 0 < chunk["size"] <= ppc_prebuilt.CHUNK_SIZE:
            raise ValueError("Invalid remote PPC chunk size")
    if sum(c["size"] for c in chunks) != library.get("size"):
        raise ValueError("Invalid remote PPC library size")
    if any(not re.fullmatch(r"[0-9a-f]{64}", entry.get("sha256", "")) for entry in [library, *chunks]):
        raise ValueError("Invalid remote PPC digest")


def receipt(root, key, commit):
    path = root / "out/ppc-sync/receipt.json"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps({"key": key, "commit": commit, "repository": REPOSITORY}, indent=2) + "\n", encoding="utf-8")


def is_release(build_dir):
    return bool(re.search(r"^CMAKE_BUILD_TYPE:STRING=Release$", (build_dir / "CMakeCache.txt").read_text(), re.M))


def release_build(root, caller):
    if is_release(caller):
        return caller
    build_dir = root / "out/build/ppc-sync-Release"
    run(["cmake", "-S", str(root), "-B", str(build_dir), "-G", "Ninja",
         "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_C_COMPILER=clang-cl", "-DCMAKE_CXX_COMPILER=clang-cl",
         "-DLO_BUILD_RECOMP_LIB=ON", "-DLO_BUILD_RUNTIME=OFF", "-DLO_BUILD_TOOLS=OFF",
         "-DLO_PREBUILT_PPC_DIR=", "-DLO_PPC_AUTO_SYNC=OFF"])
    return build_dir


def publish(root, build_dir, key, evidence, already_built):
    parent = root / "out/ppc-sync"
    parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="upload-", dir=parent) as temporary:
        checkout = Path(temporary)
        def git(*args, check=True):
            return run([*GIT_AUTH, *args], cwd=checkout, check=check)
        git("init", "--quiet")
        git("remote", "add", "origin", REMOTE)
        # Each immutable cache branch is independent of the XEX/main history.
        git("symbolic-ref", "HEAD", f"refs/heads/ppc/{key}")
        (checkout / ".gitattributes").write_text("ppc/*.bin binary\nppc/*.json text eol=lf\n", encoding="utf-8")
        writer = ppc_prebuilt.write_bundle_from_built if already_built else ppc_prebuilt.export
        writer(root, build_dir, checkout / "ppc")
        if identity(root, build_dir) != (key, evidence):
            raise ValueError("PPC source/contract changed during synchronization")
        manifest = ppc_prebuilt.load_manifest(checkout / "ppc")
        validate_manifest(manifest, evidence)
        git("add", "--", ".gitattributes", "ppc")
        git("-c", "user.name=PPC build sync", "-c", "user.email=ppc-sync@users.noreply.github.com",
            "commit", "--quiet", "-m", f"Cache PPC build {key}")
        commit = git("rev-parse", "HEAD").stdout.strip()
        # Recheck after building: another machine may have published meanwhile.
        concurrent = remote_commit(key)
        if concurrent:
            other = remote_manifest(concurrent)
            validate_manifest(other, evidence)
            return concurrent, other
        pushed = git("push", "origin", f"HEAD:refs/heads/ppc/{key}", check=False)
        if pushed.returncode:
            concurrent = remote_commit(key)
            if not concurrent:
                pushed.check_returncode()
            other = remote_manifest(concurrent)
            validate_manifest(other, evidence)
            return concurrent, other
        return commit, manifest


def sync(root, caller, already_built=False, force=False):
    if os.environ.get("CI") or os.environ.get("GITHUB_ACTIONS") or os.environ.get("LO_PPC_SYNC_ACTIVE"):
        print("PPC sync: skipped (CI or recursive build)")
        return None
    if not force and not enabled(root):
        print("PPC sync: skipped (enable with git config --local lo.ppcAutoSync true)")
        return None
    cache = (caller / "CMakeCache.txt").read_text()
    imported = re.search(r"^LO_PREBUILT_PPC_DIR:[^=]+=(.*)$", cache, re.M)
    if imported and imported[1].strip():
        print("PPC sync: skipped (caller uses an imported PPC library)")
        return None
    previous = os.environ.get("LO_PPC_SYNC_ACTIVE")
    os.environ["LO_PPC_SYNC_ACTIVE"] = "1"
    try:
        build_dir = release_build(root, caller)
        key, evidence = identity(root, build_dir)
        commit = remote_commit(key)
        if commit:
            expected = remote_manifest(commit)
            validate_manifest(expected, evidence)
            print(f"PPC sync: unchanged ({key})")
        else:
            commit, expected = publish(root, build_dir, key, evidence,
                                       already_built and build_dir == caller)
        # Read back by immutable commit and ensure the branch still names it.
        if remote_commit(key) != commit:
            raise ValueError("PPC branch changed or disappeared during synchronization")
        actual = remote_manifest(commit)
        validate_manifest(actual, evidence)
        for field in ("schema", "fingerprint", "contract", "library", "chunks"):
            if actual[field] != expected[field]:
                raise ValueError("PPC upload readback metadata mismatch")
        if identity(root, build_dir) != (key, evidence):
            raise ValueError("PPC inputs changed during synchronization")
        receipt(root, key, commit)
        print(f"PPC sync: ready ppc/{key} ({commit})")
        return key, commit
    finally:
        if previous is None:
            os.environ.pop("LO_PPC_SYNC_ACTIVE", None)
        else:
            os.environ["LO_PPC_SYNC_ACTIVE"] = previous


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    for name in ("key", "sync"):
        command = commands.add_parser(name)
        command.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
        command.add_argument("--build-dir", type=Path, required=True)
        if name == "key":
            command.add_argument("--github-output", type=Path)
        else:
            command.add_argument("--already-built", action="store_true")
            command.add_argument("--force", action="store_true")
    args = parser.parse_args()
    try:
        root, build_dir = args.root.resolve(), args.build_dir.resolve()
        if args.command == "key":
            key, _ = identity(root, build_dir)
            print(key)
            if args.github_output:
                with args.github_output.open("a", encoding="utf-8") as output:
                    output.write(f"key={key}\n")
        else:
            sync(root, build_dir, args.already_built, args.force)
    except (OSError, ValueError, KeyError, TypeError, subprocess.CalledProcessError) as error:
        detail = error.stderr.strip() if isinstance(error, subprocess.CalledProcessError) and error.stderr else str(error)
        print(f"PPC sync failed: {detail}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
