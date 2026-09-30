"""Export an AppImage AppDir's Linux runtime as a standalone Flatpak.

The AppDir is the only binary input. No source compilation takes place here.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
from release.version import normalize_release_version


ROOT = Path(__file__).resolve().parent.parent
TEMPLATE = ROOT / "packaging/linux/io.github.freefrank.LostOdysseyRecomp.json"
APP_ID = "io.github.freefrank.LostOdysseyRecomp"
REQUIRED = {
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
    f"share/applications/{APP_ID}.desktop",
    f"share/icons/hicolor/256x256/apps/{APP_ID}.png",
    f"share/metainfo/{APP_ID}.metainfo.xml",
}

# Run inside the selected Platform before export. This does not launch the game
# or write into /app. It also checks libraries loaded by name at runtime.
RUNTIME_PROBE = r"""
import ctypes
import os
import pathlib
import subprocess
import sys

env = os.environ.copy()
env['LD_LIBRARY_PATH'] = '/app/lib:/app/bin' + (':' + env['LD_LIBRARY_PATH'] if env.get('LD_LIBRARY_PATH') else '')
checked = 0
for root in ('/app/bin', '/app/lib'):
    for directory, _, names in os.walk(root):
        for name in names:
            path = pathlib.Path(directory, name)
            if path.is_symlink() or not path.is_file():
                continue
            with path.open('rb') as stream:
                if stream.read(4) != b'\x7fELF':
                    continue
            result = subprocess.run(['ldd', '-r', str(path)], env=env, text=True,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            if result.returncode or any(s in result.stdout.lower() for s in
                                        ('not found', 'undefined symbol', 'symbol lookup error')):
                sys.exit('Unresolved runtime dependency in ' + str(path) + ':\n' + result.stdout)
            checked += 1
for library in ('/app/lib/libdxcompiler.so', '/app/bin/libnvidia-ngx-dlss.so.310.9.1',
                'libpipewire-0.3.so.0', 'libpulse.so.0'):
    ctypes.CDLL(library)
print('Flatpak runtime probe: ' + str(checked) + ' ELF files and four dlopen libraries resolved')
"""


def source_version() -> str:
    content = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    match = re.search(r"\bproject\(LostOdysseyRecomp\s+VERSION\s+(\d+\.\d+\.\d+)\b", content)
    if not match:
        raise ValueError("Cannot determine checkout source version")
    return match.group(1)


def manifest_config() -> dict:
    manifest = json.loads(TEMPLATE.read_text(encoding="utf-8"))
    if (manifest.get("app-id") != APP_ID or manifest.get("runtime") != "org.freedesktop.Platform"
            or manifest.get("sdk") != "org.freedesktop.Sdk"
            or manifest.get("command") != "LostOdysseyRecomp"):
        raise ValueError("Flatpak metadata does not match this application")
    if not re.fullmatch(r"\d+\.\d+", str(manifest.get("runtime-version", ""))):
        raise ValueError("Flatpak runtime version is invalid")
    permissions = manifest.get("finish-args")
    if not isinstance(permissions, list) or not permissions or not all(isinstance(p, str) and p.startswith("--") for p in permissions):
        raise ValueError("Flatpak permissions are invalid")
    if manifest.get("modules") or manifest.get("sdk-extensions"):
        raise ValueError("Flatpak metadata must not request a source build or SDK extension")
    return manifest


def inspect_payload_tree(files: Path) -> None:
    if not files.is_dir():
        raise ValueError(f"AppDir usr/files directory missing: {files}")
    root = files.resolve()
    found = set()
    for entry in files.rglob("*"):
        if entry.is_dir() and not entry.is_symlink():
            continue
        name = entry.relative_to(files).as_posix()
        found.add(name)
        if entry.is_symlink():
            if (Path(os.readlink(entry)).is_absolute()
                    or not entry.resolve(strict=True).is_relative_to(root)):
                raise ValueError(f"Symlink leaves payload tree: {name}")
        elif not entry.is_file():
            raise ValueError(f"Unsupported payload entry: {name}")
        if name in REQUIRED:
            continue
        parts = Path(name).parts
        if parts[0] == "lib" and re.fullmatch(r"lib[^/]+\.so(?:\.[0-9A-Za-z._-]+)?", parts[-1]):
            continue
        if len(parts) == 4 and parts[:2] == ("share", "doc") and parts[-1] == "copyright":
            continue
        raise ValueError(f"Unexpected payload path (possible source/private input): {name}")
    missing = REQUIRED - found
    if missing:
        raise ValueError(f"Missing required AppDir payload: {sorted(missing)}")
    for alias in ("libnvidia-ngx-dlss.so", "libnvidia-ngx-dlss.so.1"):
        link = files / "bin" / alias
        if not link.is_symlink() or os.readlink(link) != "libnvidia-ngx-dlss.so.310.9.1":
            raise ValueError(f"NGX alias is not relative to its runtime: {link}")
    if not (files / "bin/LostOdysseyRecomp").is_file():
        raise ValueError("Runtime ELF missing")


def run(argv: list[str]) -> None:
    subprocess.run(argv, check=True)


def package(args: argparse.Namespace) -> dict:
    appdir = args.appdir.resolve(strict=True)
    source = appdir / "usr"
    output = args.output.resolve()
    if output.exists():
        raise ValueError(f"Output directory already exists: {output}")
    if output == appdir or output.is_relative_to(appdir) or appdir.is_relative_to(output):
        raise ValueError("Output and AppDir must not contain each other")
    inspect_payload_tree(source)
    version = source_version()
    if args.version and normalize_release_version(args.version).split("-", 1)[0] != version:
        raise ValueError(f"Release version must match checkout source version {version}")
    branch = "stable" if args.version else "dev"
    commit = subprocess.check_output(("git", "-C", str(ROOT), "rev-parse", "HEAD"), text=True).strip()
    metadata = manifest_config()
    output.mkdir(parents=True)
    build = output / "builder"
    repo = output / "repo"
    run(["flatpak", "build-init", "--arch=x86_64", str(build), APP_ID, metadata["sdk"],
         metadata["runtime"], metadata["runtime-version"]])
    shutil.copytree(source, build / "files", symlinks=True, dirs_exist_ok=True, copy_function=shutil.copy2)
    inspect_payload_tree(build / "files")
    run(["flatpak", "build", "--runtime", "--env=LD_LIBRARY_PATH=/app/lib:/app/bin",
         str(build), "python3", "-c", RUNTIME_PROBE])
    run(["flatpak", "build-finish", *metadata["finish-args"],
         f"--command={metadata['command']}", str(build)])
    run(["flatpak", "build-export", str(repo), str(build), branch])
    bundle_name = (f"LostOdysseyRecomp-linux-x64-{args.version}.flatpak" if args.version else
                   f"LostOdysseyRecomp-v{version}-{commit[:8]}-dev.flatpak")
    bundle = output / bundle_name
    run(["flatpak", "build-bundle", str(repo), str(bundle), APP_ID, branch])
    if not bundle.is_file() or not bundle.stat().st_size:
        raise ValueError("Flatpak bundler did not produce a nonempty bundle")
    record = {"version": version, "branch": branch, "packaging_commit": commit,
              "appdir": str(appdir),
              "runtime_ref": f"{metadata['runtime']}/x86_64/{metadata['runtime-version']}",
              "bundle": bundle_name, "size": bundle.stat().st_size}
    (output / "source.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    return record


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--appdir", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--version", default="", help="Formal release tag, for example v0.7.3")
    args = parser.parse_args()
    try:
        result = package(args)
    except (OSError, ValueError, KeyError, json.JSONDecodeError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Flatpak packaging failed: {error}\n")
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
