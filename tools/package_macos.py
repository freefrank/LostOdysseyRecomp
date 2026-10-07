"""Package the macOS runtime as a signed .app bundle in a ZIP or a disk image.

Without --identity the bundle is ad-hoc signed and runs on the building Mac.
With a Developer ID identity it is signed with the hardened runtime, and
--notarize submits it to Apple, staples the ticket and re-verifies it, so the
ZIP opens without Gatekeeper warnings on any Mac. See docs/MACOS_RELEASE.md.

  python3 tools/package_macos.py                                   # local, ad-hoc
  python3 tools/package_macos.py --release \
      --identity "Developer ID Application: Name (TEAMID)" --notarize lo-notary
"""
import argparse
import plistlib
import shutil
import subprocess
import tempfile
from pathlib import Path
from portable_shader_pack_payload import stage_shader_pack_license

ROOT = Path(__file__).resolve().parents[1]
BUNDLE_ID = "io.github.freefrank.LostOdysseyRecomp"
ICON = ROOT / "packaging/linux/io.github.freefrank.LostOdysseyRecomp.png"
MINIMUM_MACOS = "15.0"


def git(*args):
    return subprocess.check_output(("git", *args), cwd=ROOT, text=True).strip()


def source_version(build):
    stamp = build / "LostOdysseyRecomp/source-version.txt"
    if not stamp.is_file():
        raise SystemExit("Build the runtime first: source-version.txt is missing.")
    return stamp.read_text(encoding="utf-8").strip()


def asset_tag(version, requested, release):
    if requested:
        return requested
    if release:
        # The updater looks for LostOdysseyRecomp-macos-arm64-<release tag>.dmg.
        return f"v{version}"
    return f"v{version}-{git('rev-parse', 'HEAD')[:8]}-dev"


def make_icns(png, destination, temporary):
    """Build an .icns from the 256 px PNG with sips and iconutil (both ship with macOS)."""
    iconset = Path(temporary) / "LostOdysseyRecomp.iconset"
    iconset.mkdir()
    for size in (16, 32, 128, 256):
        for scale in (1, 2):
            pixels = size * scale
            if pixels > 256:
                continue
            suffix = "" if scale == 1 else "@2x"
            subprocess.run(["sips", "-z", str(pixels), str(pixels), str(png),
                            "--out", str(iconset / f"icon_{size}x{size}{suffix}.png")],
                           check=True, stdout=subprocess.DEVNULL)
    subprocess.run(["iconutil", "-c", "icns", str(iconset), "-o", str(destination)], check=True)


def info_plist(version):
    return {
        "CFBundleDevelopmentRegion": "en",
        "CFBundleDisplayName": "Lost Odyssey Recomp",
        "CFBundleExecutable": "LostOdysseyRecomp",
        "CFBundleIconFile": "LostOdysseyRecomp",
        "CFBundleIdentifier": BUNDLE_ID,
        "CFBundleInfoDictionaryVersion": "6.0",
        "CFBundleName": "LostOdysseyRecomp",
        "CFBundlePackageType": "APPL",
        "CFBundleShortVersionString": version,
        "CFBundleVersion": version,
        "LSApplicationCategoryType": "public.app-category.role-playing-games",
        "LSMinimumSystemVersion": MINIMUM_MACOS,
        "NSHighResolutionCapable": True,
        # Game controllers keep working while the game window is focused.
        "GCSupportsControllerUserInteraction": True,
    }


def stage_licenses(licenses):
    licenses.mkdir(parents=True, exist_ok=True)
    shutil.copy2(ROOT / "LICENSE", licenses / "LostOdysseyRecomp.txt")
    shutil.copy2(ROOT / "thirdparty/miniz-UNLICENSE.txt", licenses / "miniz-UNLICENSE.txt")
    shutil.copy2(ROOT / "thirdparty/licenses/XeGTAO.txt", licenses / "XeGTAO.txt")
    shutil.copy2(ROOT / "thirdparty/nlohmann-json-LICENSE.txt", licenses / "nlohmann-json-LICENSE.txt")
    shutil.copy2(ROOT / "thirdparty/lzokay/LICENSE", licenses / "lzokay-LICENSE.txt")
    shutil.copy2(ROOT / "LostOdysseyRecomp/install/FONT-PROVENANCE.md", licenses / "FONT-PROVENANCE.md")
    shutil.copy2(ROOT / "thirdparty/SDL/test/unifont-15.1.05-license.txt", licenses / "Unifont-OFL-1.1.txt")
    shutil.copytree(ROOT / "thirdparty/dxc-licenses", licenses / "DXC")
    # Statically linked on macOS: FFmpeg (LGPL, XMA decoder) and SPIRV-Cross (Metal shaders).
    shutil.copy2(ROOT / "thirdparty/ffmpeg-LICENSE.txt", licenses / "ffmpeg-LICENSE.txt")
    shutil.copy2(ROOT / "thirdparty/SPIRV-Cross/LICENSE", licenses / "SPIRV-Cross-LICENSE.txt")


def sign(bundle, identity):
    """Sign the bundle, nested code first. A Developer ID signature uses the hardened
    runtime and a secure timestamp, which notarization requires; ad-hoc ("-") runs on
    this Mac only (Apple Silicon requires some signature)."""
    common = ["codesign", "--force", "--sign", identity or "-"]
    if identity:
        common += ["--options", "runtime", "--timestamp"]
    else:
        common += ["--timestamp=none"]
    subprocess.run(common + [str(bundle / "Contents/MacOS/libdxcompiler.dylib")], check=True)
    subprocess.run(common + [str(bundle)], check=True)
    subprocess.run(["codesign", "--verify", "--strict", "--deep", str(bundle)], check=True)


def notarize(bundle, profile, temporary):
    """Submit to Apple's notary service, staple the ticket to the app and check Gatekeeper."""
    upload = Path(temporary) / "notarize.zip"
    subprocess.run(["ditto", "-c", "-k", "--keepParent", str(bundle), str(upload)], check=True)
    subprocess.run(["xcrun", "notarytool", "submit", str(upload), "--keychain-profile", profile, "--wait"],
                   check=True)
    subprocess.run(["xcrun", "stapler", "staple", str(bundle)], check=True)
    subprocess.run(["xcrun", "stapler", "validate", str(bundle)], check=True)
    subprocess.run(["spctl", "--assess", "--type", "execute", "--verbose", str(bundle)], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=ROOT / "out/build/macos-gpu")
    parser.add_argument("--output", type=Path, default=ROOT / "out/releases")
    parser.add_argument("--version", default="")
    parser.add_argument("--dry-layout", action="store_true", help="Create and list the bundle without zipping")
    parser.add_argument("--dmg", action="store_true",
                        help="Write a disk image with the app and an Applications link instead of a ZIP")
    parser.add_argument("--release", action="store_true",
                        help="Name the asset v<version> (the updater's name) instead of a -dev build")
    parser.add_argument("--identity", default="",
                        help='codesign identity, e.g. "Developer ID Application: Name (TEAMID)"; default ad-hoc')
    parser.add_argument("--notarize", default="", metavar="PROFILE",
                        help="notarytool keychain profile (xcrun notarytool store-credentials PROFILE ...)")
    args = parser.parse_args()
    build = args.build.resolve()
    runtime = build / "LostOdysseyRecomp/LostOdysseyRecomp"
    dxc = build / "LostOdysseyRecomp/libdxcompiler.dylib"
    for path in (runtime, dxc):
        if not path.is_file():
            raise SystemExit(f"Missing macOS build artifact: {path}")
    version = source_version(build)
    if args.notarize and not args.identity:
        raise SystemExit("--notarize needs --identity (a Developer ID Application certificate).")
    if args.release and not args.identity:
        raise SystemExit("--release needs --identity: release ZIPs must be Developer ID signed and notarized.")
    tag = asset_tag(version, args.version, args.release)
    name = f"LostOdysseyRecomp-macos-arm64-{tag}"
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="macos-app-", dir=output) as temporary:
        bundle = Path(temporary) / "LostOdysseyRecomp.app"
        executables = bundle / "Contents/MacOS"
        resources = bundle / "Contents/Resources"
        executables.mkdir(parents=True)
        resources.mkdir(parents=True)
        shutil.copy2(runtime, executables / "LostOdysseyRecomp")
        # The runtime loads DXC from @executable_path.
        shutil.copy2(dxc, executables / "libdxcompiler.dylib")
        with (bundle / "Contents/Info.plist").open("wb") as plist:
            plistlib.dump(info_plist(version), plist)
        make_icns(ICON, resources / "LostOdysseyRecomp.icns", temporary)
        licenses = resources / "licenses"
        stage_licenses(licenses)
        stage_shader_pack_license(licenses)
        sign(bundle, args.identity)
        if args.notarize:
            notarize(bundle, args.notarize, temporary)
        if args.dry_layout:
            print(f"Bundle: {bundle}")
            for path in sorted(bundle.rglob("*")):
                if path.is_file():
                    print(path.relative_to(bundle.parent).as_posix())
            print(f"SelectAsset: {name}.dmg")
            return
        destination = output / f"{name}{'.dmg' if args.dmg else '.zip'}"
        if destination.exists():
            raise SystemExit(f"Output already exists: {destination}")
        if args.dmg:
            # Drag-to-install layout: the app next to a link to /Applications.
            image_root = Path(temporary) / "image"
            image_root.mkdir()
            shutil.move(str(bundle), image_root / bundle.name)
            (image_root / "Applications").symlink_to("/Applications")
            subprocess.run(["hdiutil", "create", "-volname", "Lost Odyssey Recomp", "-srcfolder", str(image_root),
                            "-fs", "HFS+", "-format", "UDZO", "-ov", str(destination)], check=True)
            if args.identity:
                subprocess.run(["codesign", "--sign", args.identity, "--timestamp", str(destination)], check=True)
        else:
            # ditto keeps the bundle's signature, permissions and extended attributes.
            subprocess.run(["ditto", "-c", "-k", "--keepParent", str(bundle), str(destination)], check=True)
        print(f"SelectAsset: {destination.name}")


if __name__ == "__main__":
    main()
