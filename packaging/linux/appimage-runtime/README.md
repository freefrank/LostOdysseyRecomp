# AppImage runtime

The runtime is built from [AppImage/type2-runtime at
8f39b89e2ac31e1640b3d3f7e9a5108e6ce805fa](https://github.com/AppImage/type2-runtime/tree/8f39b89e2ac31e1640b3d3f7e9a5108e6ce805fa).
The source archive SHA-256 and Alpine 3.21.0 image digest are fixed in
`Dockerfile`. The upstream installer fixes and checks libfuse 3.15.0 and
squashfuse 0.5.2. APK packages follow the Alpine 3.21 repository; their actual
versions are recorded in `build-packages.txt`, so the build is not claimed to
be bit reproducible across APK updates.

Run `bash tools/release/build_appimage_runtime.sh` from the repository root.
It exports `out/tools/appimage-runtime/` using Docker BuildKit, without
requiring host FUSE or a new host glibc. `package_appimage.py` passes the
absolute runtime path through linuxdeploy's documented `LDAI_RUNTIME_FILE`.
It compares the packaged runtime against the selected binary, allowing only
appimagetool's 16-byte `.digest_md5` field to change.

`automatic-extract.patch` is independently written against the MIT upstream
source and distributed under the same MIT license in `licenses/patch.txt`.
The behavior was investigated using
[hard-corps-uprising-recompiled v0.6.1](https://github.com/saulob/hard-corps-uprising-recompiled/tree/a7b9d093334db68daaa6265d7f090a877078f6bd/patches/appimage-runtime).
That repository is GPL-2.0 and its patch directory has no separate license;
its patch and build scripts were not copied.

An ordinary launch tries FUSE first. If the mount readiness pipe closes
without a successful mount, it uses upstream extraction/launch code with a
per-launch `mkdtemp` directory (0700). Explicit `--appimage-mount` still fails
when no mount can be provided. The patch fixes the extraction argument-array
terminator and musl `nftw` descriptor limit, preserves `APPIMAGE`, `APPDIR`,
`ARGV0` and `OWD`, waits through interruptions, forwards SIGINT/SIGTERM/SIGHUP,
and removes the extraction tree after the payload exits. Upstream's explicit
`NO_CLEANUP` opt-out remains available. SIGKILL or machine shutdown cannot run
process cleanup.

The package includes the runtime and dependency license notices plus
`runtime-relink.tar.gz` under
`AppImage-runtime/` at the AppDir root, outside the `usr` tree reused by
Flatpak. The archive contains
the patched runtime source, the original libfuse source archive, upstream
libfuse patch, relinkable runtime object, static dependency libraries and
`relink.sh`. This supplies the LGPL libfuse replacement/relink path. Rebuild
libfuse with the included upstream recipe, replace `libfuse3.a`, then run
`relink.sh` in Alpine 3.21 with `clang`, `musl-dev` and `binutils` installed.

`tools/tests/appimage_runtime_test.py` wraps a harmless Python payload with
the actual runtime and SquashFS. It checks zero/Unicode/empty arguments,
exit status, environment paths, simultaneous launches, cleanup and (without
FUSE) supervisor/process-group termination. The release workflow hides
`/dev/fuse` in a private mount namespace for this check; no game runs.
