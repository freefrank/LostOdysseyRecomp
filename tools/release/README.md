# Release packaging

Release packaging assembles the Windows ZIP, Linux AppImage and Flatpak bundle.
Shader packs are not part of them; see the end of this page.
The macOS disk image is not built here: it is made on a Mac and uploaded to the
release by hand ([macOS releases](../../docs/MACOS_RELEASE.md)).
The current release workflow checks that the expected package files exist and
are nonempty before publication. It does not require a repository-wide hash,
provenance manifest or SHA-256 sidecar.

```sh
python tools/release/extract_release_notes.py \
  --changelog CHANGELOG.md --version v0.7.10 --output /path/to/release-notes.md
```

The notes extractor reads only the matching changelog entry. ZIP installation
still performs ordinary archive parsing, CRC, path protection and rollback; it
does not add a downloaded-file size or SHA check and does not launch the game.

Release notes also appear in the automatic updater. Keep each language to 2–4
short, player-facing bullets; put test counts, commit IDs, CI logs and validation
details in technical notes rather than the changelog entry.

After a release is public, point the PortForge manifest (`.forge.json`) at it and
commit the change with the release record:

```sh
python tools/release/update_forge_manifest.py v0.9.27
```

The script replaces the version in every build and checks each download URL
before writing.

The v0.7.9 Windows transition ZIP carried the old SHA map in `files`, allowing
already published v0.7.3 updaters to upgrade automatically. The v0.7.9 updater
ignores the map values and installs the downloaded archive directly after
ordinary HTTP/I/O, ZIP CRC, path and rollback handling. Future Windows ZIPs
created by `tools/package_release.py` write a SHA-256 string for every payload
under `manifest.files`, restoring the manifest shape expected by the legacy
v0.7.0–v0.7.3 updaters. This does not restore runtime hash validation: the
current updater consumes paths only. The v0.7.10–v0.7.20 packages were already
published without this compatibility map and are not changed retroactively.

For the v0.7.9 prerelease, the original Windows ZIP, Linux AppImage,
and stable Linux Flatpak bundle remain available. Optional portable shader
assets belong to later releases. Do
not publish the Flatpak runtime archive, `release-source.json`, or standalone
checksum/source-list assets; checksum and source records remain CI or local
internal validation artifacts.

Releases up to v0.7.25 bundled the Vulkan pack in the Windows ZIP and Linux
packages; v0.7.10 also published the DX12 pack as a separate asset. After
v0.7.25 shipped a pack its runtime rejected, the jobs verified a pinned pack
from the private build-inputs repository before bundling it.

Release workflow design: the Linux job compiles once, creates a persistent
AppImage AppDir, and exports the stable Flatpak by reusing that AppDir's
`usr` tree. It does not perform a second source compilation for Flatpak.
The workflow exports a stable Flatpak directly as a CI artifact next to the
Windows ZIP and AppImage. The workflow runs on GitHub-hosted runners
(`.github/workflows/release.yml`, see [GitHub Actions checks and
releases](../../docs/notes/ci.md)): after all package jobs succeed, the
publication job downloads their GitHub Actions v4 artifacts and uploads missing
assets to the GitHub draft (Windows ZIP, AppImage, Flatpak and Android APK),
then checks that the release holds those packages and at most one more
asset, `LostOdysseyRecomp-macos-arm64-<tag>.dmg`, which is built on a Mac and
uploaded by hand ([macOS releases](../../docs/MACOS_RELEASE.md#disk-image-v085)), and
that every asset is uploaded and nonempty. Any other asset fails the check; the
shader packs live on the `shader-packs` release. Re-runs validate an
existing public release without changing its publication state.
The v0.7.9 Release CI [36378342125](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/36378342125)
passed all five jobs. Its Linux job installed `clang-tools-18` 18.1.8, built
the Ubuntu 22.04-compatible AppImage baseline, checked AppImage ABI/loader
behavior and reused the same AppDir `usr` tree for Flatpak. The three public
download URLs returned HTTP 200 after redirects. These checks did not run the
game and do not claim SHA verification. The historical packaging workflow
record reports AppImage fixture 8/8 and Flatpak Python fixtures 6/6; those
results are retained as release history. Evidence:
`out/release-workflow-reuse/flatpak-package.log`, `flatpak-source.json`, and
`install-check.log`. The Linux job compiles the source once, retains the
AppImage AppDir, and exports the stable Flatpak by reusing its `usr` tree.

Shader packs are published apart from version releases, with
`tools/release/publish_shader_packs.py`, to the `shader-packs` prerelease; the
game downloads the pack for its renderer at startup
([procedure](../../docs/PORTABLE_SHADER_PACK.md#publishing)). The Linux release
job runs the script with `--check` after building: a version release stops
unless the index lists both contracts of its runtime (Vulkan, also used by
Metal and Android, and DirectX 12), so a translator, option or discovery change
needs new packs published first. `tools/shader_pack/build_packs.py` builds and
checks them on Windows or Linux.
