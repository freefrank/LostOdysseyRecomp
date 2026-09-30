# AppImage compatibility baseline

AppImageHub PR [5825](https://github.com/AppImage/appimage.github.io/pull/5825#issuecomment-5862425226)
failed before a window appeared: v0.7.3's executable and bundled dependencies
required `GLIBC_2.38`, and the host C++ library lacked `GLIBCXX_3.4.31/3.4.32`.
Installing FUSE or repacking the same binaries does not fix that ABI mismatch.

## Build and package

The Release workflow builds **all Linux sources and system dependencies on
Ubuntu 22.04 / glibc 2.35**, using Clang/LLD 18 and GCC 13 C++ headers from
Jammy-compatible repositories. CMake is pinned to 3.31.6. The installer rejects
a different distribution and verifies the LLVM repository signing key.
The existing Sandy Bridge/AVX target and Windows build are unchanged.

`tools/package_appimage.py` explicitly passes the build compiler's
`libstdc++.so.6` and `libgcc_s.so.1` to linuxdeploy's `--library` option, which
bypasses its normal library exclusion and deploys RPATHs and copyright files.
It does not bundle glibc or the host GPU driver. `--cxx-compiler` defaults to
`CXX` (otherwise `clang++`); use the same compiler/flags as the runtime build.

Before compression and again after extracting the final AppImage, the packager
checks every shipped ELF, including DXC/DLSS libraries loaded dynamically:

- x86_64 architecture and imported `GLIBC_*` versions no newer than 2.35.
- Bundled GCC runtime libraries provide all requested symbol versions.
- AppRun and relative AppDir links resolve inside the package.

The final extracted image also receives a loader-only `ldd` check without
`LD_LIBRARY_PATH`, `LD_PRELOAD`, or `LD_AUDIT`. Missing dependencies and resolution
of the main executable's GCC runtimes outside the package are fatal. Extraction
requires no FUSE mount; no GPU or private game data is initialized by this check.
Flatpak is then built from this same verified extracted AppDir.

## Release and verification

The existing `LostOdysseyRecomp-linux-x64-<tag>.AppImage` name is intentionally
retained: already-released updaters match it exactly. AppImageHub's naming
warning is non-fatal. Published historical assets/tags are not changed.

The v0.7.9 Release CI run [36378342125](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/36378342125)
completed the Ubuntu 22.04 release build, ABI and loader checks, and Flatpak
reuse from the same AppDir. The Linux toolchain installed `clang-tools-18`
18.1.8 for the build. These results establish the package baseline and do not
constitute AppImageHub re-review, normal FUSE mounting, or actual GPU/game
execution on Ubuntu 22.04 or Steam Deck; those remain follow-up checks.

Local source-level checks for this patch: Python syntax, workflow YAML, POSIX
shell syntax, and five focused ABI parsing regressions. The full Linux release
build is now covered by the v0.7.9 CI record above; GPU/game execution and
AppImageHub/FUSE acceptance remain unverified.
