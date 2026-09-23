"""Package the Linux runtime as an AppImage using linuxdeploy."""
import argparse
import hashlib
import os
import shutil
import subprocess
import tempfile
from pathlib import Path
from portable_shader_pack_payload import stage_portable_shader_pack

ROOT = Path(__file__).resolve().parents[1]
LINUX_PACKAGING = ROOT / "packaging/linux"


def git(*args):
    return subprocess.check_output(("git", *args), cwd=ROOT, text=True).strip()


def asset_tag(build, requested):
    if requested:
        return requested
    stamp = build / "LostOdysseyRecomp/source-version.txt"
    if not stamp.is_file():
        raise SystemExit("Provide --version or build the runtime with source-version.txt.")
    source = stamp.read_text(encoding="utf-8").strip()
    commit = git("rev-parse", "HEAD")[:8]
    return f"v{source}-{commit}-dev"


def validate_apprun(appdir):
    entry = appdir / "AppRun"
    try:
        target = entry.resolve(strict=True)
    except (OSError, RuntimeError) as error:
        raise SystemExit(f"Invalid AppRun entry: {error}") from error
    if not target.is_relative_to(appdir.resolve()) or not target.is_file():
        raise SystemExit("AppRun must resolve to a regular file inside the AppDir")
    if not os.access(target, os.X_OK):
        raise SystemExit("AppRun target is not executable")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=ROOT / "out/build/linux")
    parser.add_argument("--output", type=Path, default=ROOT / "out/releases")
    parser.add_argument("--version", default="")
    parser.add_argument("--linuxdeploy", default="linuxdeploy")
    parser.add_argument("--dry-layout", action="store_true", help="Create and list an AppDir without linuxdeploy")
    args = parser.parse_args()
    build = args.build.resolve()
    runtime = build / "LostOdysseyRecomp/LostOdysseyRecomp"
    dxc = build / "LostOdysseyRecomp/libdxcompiler.so"
    for path in (runtime, dxc):
        if not path.is_file():
            raise SystemExit(f"Missing Linux build artifact: {path}")
    tag = asset_tag(build, args.version)
    name = f"LostOdysseyRecomp-linux-x64-{tag}"
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="appimage-", dir=output) as temporary:
        appdir = Path(temporary) / "LostOdysseyRecomp.AppDir"
        (appdir / "usr/bin").mkdir(parents=True)
        (appdir / "usr/lib").mkdir(parents=True)
        shutil.copy2(runtime, appdir / "usr/bin/LostOdysseyRecomp")
        shutil.copy2(dxc, appdir / "usr/lib/libdxcompiler.so")

        # Include DLSS Linux runtime if present
        dlss_runtime = runtime.parent / "libnvidia-ngx-dlss.so.310.9.1"
        if dlss_runtime.is_file():
            # NGX loader searches next to the executable (usr/bin) and in system/app library paths (usr/lib)
            canonical_so = appdir / "usr/bin/libnvidia-ngx-dlss.so.310.9.1"
            shutil.copy2(dlss_runtime, canonical_so)

            def make_link_or_copy(source_rel, target_path, source_abs):
                if not target_path.exists():
                    try:
                        target_path.symlink_to(source_rel)
                    except OSError:
                        shutil.copy2(source_abs, target_path)

            # In usr/bin: provide unversioned and .so.1 links to canonical
            make_link_or_copy("libnvidia-ngx-dlss.so.310.9.1", appdir / "usr/bin/libnvidia-ngx-dlss.so", canonical_so)
            make_link_or_copy("libnvidia-ngx-dlss.so.310.9.1", appdir / "usr/bin/libnvidia-ngx-dlss.so.1", canonical_so)

            # In usr/lib: provide relative symlink to ../bin/libnvidia-ngx-dlss.so.310.9.1, with fallback to copy
            make_link_or_copy("../bin/libnvidia-ngx-dlss.so.310.9.1", appdir / "usr/lib/libnvidia-ngx-dlss.so.310.9.1", canonical_so)
            make_link_or_copy("libnvidia-ngx-dlss.so.310.9.1", appdir / "usr/lib/libnvidia-ngx-dlss.so", canonical_so)
            make_link_or_copy("libnvidia-ngx-dlss.so.310.9.1", appdir / "usr/lib/libnvidia-ngx-dlss.so.1", canonical_so)

            # Stage DLSS License & Notice (mandatory when bundling runtime)
            dlss_sdk_root = None
            for candidate in [
                ROOT / "out/deps/nvidia-dlss",
                ROOT / ".cache/deps/nvidia-dlss-37495948",
            ]:
                if (candidate / "LICENSE.txt").is_file():
                    dlss_sdk_root = candidate
                    break
            if not dlss_sdk_root:
                raise SystemExit("libnvidia-ngx-dlss.so.310.9.1 is packaged but DLSS SDK license is missing.")

            dlss_lic_dest = appdir / "usr/share/licenses/lost-odyssey-recomp/NVIDIA-DLSS"
            dlss_lic_dest.mkdir(parents=True, exist_ok=True)
            shutil.copy2(dlss_sdk_root / "LICENSE.txt", dlss_lic_dest / "LICENSE.txt")
            notice_text = (
                "This software contains source code and/or runtime components provided by NVIDIA Corporation.\n"
                "NVIDIA DLSS SDK Version: 310.9.1 (commit 374959484e79a640feaba44c93ac8cfb0a03f5b5)\n"
            )
            (dlss_lic_dest / "NOTICE.txt").write_text(notice_text, encoding="utf-8")
        fsr_license = runtime.parent / "licenses/LICENSE-FidelityFX.txt"
        if fsr_license.is_file():
            fsr_licenses = appdir / "usr/share/licenses/lost-odyssey-recomp"
            fsr_licenses.mkdir(parents=True, exist_ok=True)
            shutil.copy2(fsr_license, fsr_licenses / fsr_license.name)
        stage_portable_shader_pack(runtime.parent, appdir / "usr/bin",
                                   appdir / "usr/share/licenses/lost-odyssey-recomp")
        desktop = LINUX_PACKAGING / "io.github.freefrank.LostOdysseyRecomp.desktop"
        icon = LINUX_PACKAGING / "io.github.freefrank.LostOdysseyRecomp.png"
        applications = appdir / "usr/share/applications"
        icons = appdir / "usr/share/icons/hicolor/256x256/apps"
        applications.mkdir(parents=True)
        icons.mkdir(parents=True)
        shutil.copy2(desktop, applications / desktop.name)
        shutil.copy2(icon, icons / icon.name)
        if args.dry_layout:
            print(f"AppDir: {appdir}")
            for path in sorted(appdir.rglob("*")):
                if path.is_file():
                    print(path.relative_to(appdir).as_posix())
            print(f"SelectAsset: {name}.AppImage")
            return
        deploy = shutil.which(args.linuxdeploy)
        if not deploy:
            raise SystemExit(f"linuxdeploy not found: {args.linuxdeploy}")
        # shutil.which keeps a relative directory path as given. Resolve it
        # before linuxdeploy runs with cwd=temporary, or CI's
        # out/tools/linuxdeploy/linuxdeploy is looked up in the temp dir.
        deploy = str(Path(deploy).resolve())
        command = [deploy, "--appdir", str(appdir),
                   "--desktop-file", str(desktop), "--icon-file", str(icon),
                   "--exclude-library", "libwayland*"]
        # linuxdeploy can succeed without creating AppRun. Check the deployed
        # entry before invoking the output plugin, rather than shipping that warning.
        subprocess.run(command, cwd=temporary, check=True)
        validate_apprun(appdir)
        subprocess.run([*command, "--output", "appimage"], cwd=temporary, check=True)
        validate_apprun(appdir)
        produced = next(Path(temporary).glob("*.AppImage"), None)
        if produced is None:
            raise SystemExit("linuxdeploy did not produce an AppImage")
        destination = output / f"{name}.AppImage"
        shutil.move(str(produced), destination)
        checksum_path = destination.with_suffix(".AppImage.sha256")
        checksum = hashlib.sha256(destination.read_bytes()).hexdigest()
        checksum_path.write_text(f"{checksum}  {destination.name}\n", encoding="utf-8")
        print(f"SelectAsset: {destination.name}")
        print(f"SelectAsset: {checksum_path.name}")


if __name__ == "__main__":
    main()
