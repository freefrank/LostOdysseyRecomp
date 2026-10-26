"""Package the Linux runtime as an AppImage using linuxdeploy."""
import argparse
import filecmp
import hashlib
import os
import re
import shutil
import subprocess
import tempfile
from pathlib import Path
from portable_shader_pack_payload import stage_shader_pack_license
from appimage_compat import CompatibilityError, compiler_libraries, validate_abi, validate_loader

ROOT = Path(__file__).resolve().parents[1]
LINUX_PACKAGING = ROOT / "packaging/linux"
# The runtime loads the DLSS snippet from usr/bin/ngx (os/runtime_libraries.h).
# linuxdeploy rewrites the RPATH of every ELF file directly in usr/bin and
# anywhere under usr/lib, and NGX does not load a snippet whose NVIDIA
# signature no longer matches, so it stays in a subfolder linuxdeploy skips.
NGX_SNIPPET = "libnvidia-ngx-dlss.so.310.9.1"
NGX_DIRECTORY = "usr/bin/ngx"
APPIMAGE_RUNTIME = ROOT / "out/tools/appimage-runtime/runtime-x86_64"


def validate_image_runtime(image, runtime):
    # appimagetool fills only this 16-byte format field for an unsigned image.
    # Locate it in the selected runtime; still compare the ELF section table,
    # code and every other byte to detect a plugin ignoring --runtime-file.
    expected = bytearray(runtime.read_bytes())
    sections = subprocess.check_output(['readelf', '--wide', '--sections', str(runtime)], text=True)
    digest = re.search(r'\.digest_md5\s+\S+\s+[0-9a-fA-F]+\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)', sections)
    if not digest or int(digest[2], 16) != 16:
        raise SystemExit("Selected runtime has no 16-byte AppImage digest field")
    offset = int(digest[1], 16)
    if offset + 16 > len(expected):
        raise SystemExit("Selected runtime has an invalid AppImage digest field")
    with image.open('rb') as actual:
        prefix = bytearray(actual.read(len(expected)))
    if len(prefix) == len(expected):
        prefix[offset:offset + 16] = expected[offset:offset + 16]
    if prefix != expected:
        raise SystemExit("AppImage does not contain the selected runtime byte for byte")


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


def validate_appdir_links(appdir):
    root = appdir.resolve()
    for entry in (appdir / "usr").rglob("*"):
        if entry.is_symlink():
            if entry.readlink().is_absolute() or not entry.resolve(strict=True).is_relative_to(root):
                raise SystemExit(f"AppDir symlink leaves the AppDir: {entry}")


def validate_ngx_snippet(source, appdir):
    """The packaged snippet must be NVIDIA's file byte for byte."""
    packaged = appdir / NGX_DIRECTORY / NGX_SNIPPET
    if packaged.is_symlink() or not packaged.is_file():
        raise SystemExit(f"Signed NGX snippet missing: {packaged}")
    if not filecmp.cmp(source, packaged, shallow=False):
        raise SystemExit(f"Packaging modified the signed NGX snippet: {packaged}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=ROOT / "out/build/linux")
    parser.add_argument("--output", type=Path, default=ROOT / "out/releases")
    parser.add_argument("--version", default="")
    parser.add_argument("--linuxdeploy", default="linuxdeploy")
    parser.add_argument("--runtime-file", type=Path, default=APPIMAGE_RUNTIME,
                        help="Pinned runtime built by tools/release/build_appimage_runtime.sh")
    parser.add_argument("--cxx-compiler", default=os.environ.get("CXX", "clang++"),
                        help="Build compiler used to locate the bundled GCC runtimes")
    parser.add_argument("--appdir", type=Path, help="Persist the final deployed AppDir at this new path")
    parser.add_argument("--dry-layout", action="store_true", help="Create and list an AppDir without linuxdeploy")
    args = parser.parse_args()
    appdir_destination = args.appdir.absolute() if args.appdir else None
    if appdir_destination and (appdir_destination.exists() or appdir_destination.is_symlink()):
        raise SystemExit(f"AppDir destination already exists: {appdir_destination}")
    if appdir_destination and args.dry_layout:
        raise SystemExit("--appdir requires linuxdeploy; it cannot be used with --dry-layout")
    build = args.build.resolve()
    runtime = build / "LostOdysseyRecomp/LostOdysseyRecomp"
    dxc = build / "LostOdysseyRecomp/libdxcompiler.so"
    for path in (runtime, dxc):
        if not path.is_file():
            raise SystemExit(f"Missing Linux build artifact: {path}")
    tag = asset_tag(build, args.version)
    # Existing released updaters match this name exactly. A cosmetic rename
    # for AppImageHub would break their upgrade path.
    name = f"LostOdysseyRecomp-linux-x64-{tag}"
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="appimage-", dir=output) as temporary:
        appdir = Path(temporary) / "LostOdysseyRecomp.AppDir"
        (appdir / "usr/bin").mkdir(parents=True)
        (appdir / "usr/lib").mkdir(parents=True)
        shutil.copy2(runtime, appdir / "usr/bin/LostOdysseyRecomp")
        shutil.copy2(dxc, appdir / "usr/lib/libdxcompiler.so")

        # Include the DLSS Linux runtime if present. NGX finds the versioned
        # file by itself: nothing links against it, so no aliases are needed.
        dlss_runtime = runtime.parent / NGX_SNIPPET
        if dlss_runtime.is_file():
            (appdir / NGX_DIRECTORY).mkdir()
            shutil.copy2(dlss_runtime, appdir / NGX_DIRECTORY / NGX_SNIPPET)

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
                raise SystemExit(f"{NGX_SNIPPET} is packaged but DLSS SDK license is missing.")

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
        stage_shader_pack_license(appdir / "usr/share/licenses/lost-odyssey-recomp")
        shutil.copy2(ROOT / "thirdparty/licenses/XeGTAO.txt",
                     appdir / "usr/share/licenses/lost-odyssey-recomp/XeGTAO.txt")
        shutil.copy2(ROOT / "thirdparty/licenses/SnapdragonGSR.txt",
                     appdir / "usr/share/licenses/lost-odyssey-recomp/SnapdragonGSR.txt")
        for source, license_name in (("LICENSE.txt", "Signalsmith-Stretch.txt"),
                             ("signalsmith-linear/LICENSE.txt", "Signalsmith-Linear.txt")):
            shutil.copy2(ROOT / "thirdparty/signalsmith-stretch" / source,
                         appdir / "usr/share/licenses/lost-odyssey-recomp" / license_name)
        desktop = LINUX_PACKAGING / "io.github.freefrank.LostOdysseyRecomp.desktop"
        icon = LINUX_PACKAGING / "io.github.freefrank.LostOdysseyRecomp.png"
        metainfo = LINUX_PACKAGING / "io.github.freefrank.LostOdysseyRecomp.metainfo.xml"
        applications = appdir / "usr/share/applications"
        icons = appdir / "usr/share/icons/hicolor/256x256/apps"
        metainfo_dir = appdir / "usr/share/metainfo"
        applications.mkdir(parents=True)
        icons.mkdir(parents=True)
        metainfo_dir.mkdir(parents=True)
        shutil.copy2(desktop, applications / desktop.name)
        shutil.copy2(icon, icons / icon.name)
        shutil.copy2(metainfo, metainfo_dir / metainfo.name)
        steam_artwork = appdir / "usr/share/lost-odyssey-recomp/steam"
        steam_artwork.mkdir(parents=True)
        for filename in ("portrait.png", "header.png", "hero.png", "logo.png", "icon.png"):
            shutil.copy2(ROOT / "assets/steam" / filename, steam_artwork / filename)
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
        runtime_file = args.runtime_file.resolve()
        if not runtime_file.is_file():
            raise SystemExit(f"Missing AppImage runtime: {runtime_file}; run tools/release/build_appimage_runtime.sh")
        # LDAI_RUNTIME_FILE is the output plugin's documented --runtime-file
        # forwarding option. Override ambient values instead of trusting them.
        image_env = dict(os.environ, LDAI_RUNTIME_FILE=str(runtime_file))
        runtime_licenses = runtime_file.parent / "licenses"
        # The runtime itself wraps SquashFS outside usr. Keep its notices and
        # relink materials outside usr too: Flatpak reuses usr, not this wrapper.
        license_destination = appdir / "AppImage-runtime"
        shutil.copytree(runtime_licenses, license_destination)
        for filename in ("runtime-relink.tar.gz", "build-packages.txt", "runtime.sha256", "patch.sha256"):
            shutil.copy2(runtime_file.parent / filename, license_destination / filename)
        command = [deploy, "--appdir", str(appdir),
                   "--desktop-file", str(desktop), "--icon-file", str(icon),
                   "--exclude-library", "libwayland*"]
        # Explicit --library force-deploys these normally excluded libraries,
        # including their copyright files and origin-relative RPATHs.
        for library in compiler_libraries(args.cxx_compiler):
            command.extend(("--library", str(library)))
        # linuxdeploy can succeed without creating AppRun. Check the deployed
        # entry before invoking the output plugin, rather than shipping that warning.
        subprocess.run(command, cwd=temporary, check=True)
        validate_apprun(appdir)
        validate_appdir_links(appdir)
        validate_abi(appdir)
        subprocess.run([*command, "--output", "appimage"], cwd=temporary, env=image_env, check=True)
        validate_apprun(appdir)
        validate_appdir_links(appdir)
        produced = next(Path(temporary).glob("*.AppImage"), None)
        if produced is None:
            raise SystemExit("linuxdeploy did not produce an AppImage")
        if not produced.stat().st_size:
            raise SystemExit("linuxdeploy produced an empty AppImage")
        validate_image_runtime(produced, runtime_file)
        print(f"AppImage runtime SHA-256: {hashlib.sha256(runtime_file.read_bytes()).hexdigest()}")
        # Test the final image, not just the pre-compression staging directory.
        # Extraction does not require FUSE, and ldd does not initialize the GPU.
        extracted = Path(temporary) / "verify"
        extracted.mkdir()
        subprocess.run([str(produced), "--appimage-extract"], cwd=extracted,
                       stdout=subprocess.DEVNULL, check=True, timeout=180)
        final_appdir = extracted / "squashfs-root"
        validate_apprun(final_appdir)
        validate_appdir_links(final_appdir)
        validate_abi(final_appdir)
        validate_loader(final_appdir)
        if dlss_runtime.is_file():
            validate_ngx_snippet(dlss_runtime, final_appdir)
        destination = output / f"{name}.AppImage"
        shutil.move(str(produced), destination)
        if appdir_destination:
            appdir_destination.parent.mkdir(parents=True, exist_ok=True)
            # Flatpak consumes the same bytes that were verified above.
            shutil.copytree(final_appdir, appdir_destination, symlinks=True)
            validate_apprun(appdir_destination)
            validate_appdir_links(appdir_destination)
            print(f"AppDir: {appdir_destination}")
        print(f"SelectAsset: {destination.name}")


if __name__ == "__main__":
    try:
        main()
    except CompatibilityError as error:
        raise SystemExit(str(error)) from error
