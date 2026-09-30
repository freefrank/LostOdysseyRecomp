# Installer and Windows release pipeline

The native SDL installer and its `ScanContent`/`InstallContent` flow run from
`LostOdysseyRecomp.exe`. The Python `tools/installer` sources and frozen
`InstallGame.exe` helper have been removed. Release packaging does not ship a
separate `InstallGame.exe` or `LostOdysseyUpdater.exe`; the native importer and
updater run from the single runtime binary. The CMake `LostOdysseyUpdater`
target remains available for host-side fixtures.

## Published v0.5.20 — 2026-09-17

The published release notes are in the matching `v0.5.20` section of `CHANGELOG.md`.
It includes host EDRAM unsigned format clamping (Issue #38), f2358 TAA jitter fixes,
the relocatable portable Vulkan shader pack (`.lospv`) architecture, shader and
pipeline preparation worker scaling, and the integrated in-game debug overlay and
pure software cross-platform settings rasterizer from the menu branch.

Both the Windows portable ZIP and Linux AppImage packages bundle `shaders/portable_vk.lospv`
directly out-of-the-box via automated CI packaging (`tools/release/fetch_shader_pack.py`),
providing instant first-launch startup with all 28,482 shaders ready without runtime DXC calls.

## Published v0.5.14 — 2026-09-16

The published release notes are in the matching `v0.5.14` section of
`CHANGELOG.md`. It includes the embedded installer/updater fixes, Linux
XDG/AppImage support, and the R3 notified-wait modernization. Release CI
`35065717899` succeeded; Linux Release job `104695450320` succeeded on attempt
1. The Linux x64 AppImage is 43,162,104 bytes with SHA-256
`a9912d2a258f17a2fea1a4d7f99c9589b538e66efd25225b1ade74af606e6196`.
The Windows ZIP is 32,915,456 bytes with SHA-256
`c21224ed985ada3502e25b42dd9e9379cb95749b0f06cea6d838f4d60843c09d`.
Publication completed at 2026-09-16T07:18:53Z from source/tag commit
`caf8060d99e5f1e52aec9d545331efe59e0c01e8`; Steam Deck compatibility is not
complete.

The 47-file Windows manifest and Linux format/sidecar checks passed, and all
four public ZIP/AppImage assets and sidecars matched their recorded bytes and
hashes after redirect downloads. The AppImage has not been used in a real game
or update run. Reuse the recorded focused fixtures; real network updates,
complete interactive import and full-game validation remain outside this
release's evidence.

Packaging correction: a user launch of the original Linux AppImage exposed
`execv` `ENOENT` because the desktop entry at the AppDir root was not discovered
by linuxdeploy, leaving the package without `AppRun`. `tools/package_appimage.py`
now deploys metadata explicitly in two stages and checks the internal executable
before image generation. Linux packaging regression checks passed 5/5, and a
repacked extract-and-run reached the expected no-game-files exit in WSL. The
replacement Linux asset and sidecar are published and anonymously verified
(43,162,104 bytes, SHA-256 `0991df9aca8e930fa8eacbd8afe99b7a3a81e940dfa740fcfbdba34cd54c4d0a`); the original
runtime and library content hashes were unchanged in the repacked extract.

## Current v0.5.0 CI delivery — 2026-09-09

Release CI `34362242667` succeeded for tag/main `28be72f02649cf87126dd9f1a604ada6cdd380c5`. The Windows ZIP is 44,020,136 bytes with SHA256 `e8391a2353a7206398b2dca24a2d73bccdea7b946d55cbfca4e7573648e95312`; source identity is `5038af3b3561ffce579a78158fb088c8d5f27df7705ba140e0e56bd11624e9cc`. See `out/v0.5.0/release-finalization/ci-34362242667/CI-DELIVERY.json` and `REPORT.md`. That CI artifact predates the current shader-priority candidate and remains historical until the parent replaces it. Draft release `385591785` is not public and has no publication timestamp or verified anonymous public download. The current candidate's four capture-confirmed c7 paths and shader-anomaly priority are documented in `out/v0.5.0/performance-fix/shader-priority-0.5.0/REPORT.md`; no game run was performed for it. Publication must wait for final asset/tag/download verification.

## Local build

Prepare submodules and generate PPC sources as described in BUILDING.md. Then:

```powershell
tools/build_release.bat
python -m venv out/release-venv
out/release-venv/Scripts/python.exe -m pip install -r tools/release/requirements.txt
out/release-venv/Scripts/python.exe tools/package_release.py
```

The dedicated `out/build/release` directory uses clang-cl, Release and static CRT.
`LO_BUILD_JOBS` defaults to 4; the hosted workflow uses 2 to limit memory pressure from the
generated C++ files. Existing development builds are not overwritten.

Packaging uses the pinned Microsoft DXC v1.8.2407 x64 `dxcompiler.dll` and `dxil.dll` pair
already staged beside the tested build. The pair is byte-checked against the official archive
and recorded in `thirdparty/dxc-licenses/PROVENANCE.json`; its matching license files are
included in the package. No Windows system DLLs are copied from the developer's machine. A PE
import audit rejects missing non-system dependencies, including an accidental dependency on an
unbundled Visual C++ runtime. The Vulkan loader is supplied by the graphics driver, not bundled
as a Vulkan SDK component.

Only explicitly selected payload files enter the ZIP. Game data, saves, settings, shader caches,
private build inputs, generated source, logs and PDBs are not packaged. The manifest records the
source commit, development state, payload checksums and DLL imports. The package has a separate
SHA256 file. Dependency license texts accompany the binaries, including
`licenses/FONT-PROVENANCE.md` and the complete SDL-sourced `licenses/Unifont-OFL-1.1.txt` notice for
the embedded installer font.

## GitHub Actions

`release.yml` builds when dispatched manually or when a `v*` tag is pushed.
It uses hosted Windows 2022. Private inputs are checked out from a pinned commit of
`freefrank/LostOdysseyRecomp-build-inputs` using a read-only deploy key stored in the
`LO_BUILD_INPUT_KEY` Actions secret. Pull-request tests never access this key or repository.
It produces a downloadable Actions artifact; a version tag also creates a **draft** GitHub release.

Draft release notes come from the matching version section in the tagged `CHANGELOG.md`.
`tools/release/extract_release_notes.py` accepts a linked or plain version heading (for example,
`### [v0.4.0](...)` or `### v0.2`), retaining its body and nested headings until the next heading
at the same or a higher level. Missing, duplicate or empty version sections fail release creation;
there is no generated-notes fallback. Existing releases retain their reviewed notes and assets.
Run the focused extraction checks with
`python -m unittest discover -s tools/tests -p test_release_notes.py -v`.

The public checkout intentionally does not contain the original XEX or generated PPC files.
The private repository contains only the supported Disc 1 default.xex and a provenance note.
The input is validated against the importer's pinned SHA256 before code generation. Game
contents are never printed or uploaded as artifacts. Missing or wrong input fails the build.
For another private repository, update the checkout repository/ref and install a read-only
deploy key. Local provisioning accepts `LO_BUILD_XEX_PATH`; the helper also supports a private
HTTPS `LO_BUILD_XEX_URL` for alternate hosts.

The pipeline builds xexdump and XenonRecomp, generates the image symbol list and PPC code,
applies the checked-in dependency patches, builds Release, then packages the runtime.
It requires no ISO or archive assets on the runner. A successful local build is not evidence
that the hosted workflow has run; check the actual Actions result before publishing.

## Local prebuilt PPC library — historical checkpoint

The release workflow adds a `rebuild_ppc` boolean input, defaulting to `false`.
At this historical checkpoint, the default path restored the PPC static library and receipts from the immutable
private `ppc/<key>` branch selected by the input/compiler key. The XEX input still
comes from the separately pinned private input commit and is checked against its
pinned SHA256. Both paths
run the existing checkout and `Generate game code` step with its validation; the
prebuilt path skips only generated PPC C++ compilation. Setting `rebuild_ppc: true`
selects the retained source compilation path, which is needed when an old version
tag does not contain `ppc_prebuilt.py`.

Locally, `LO_PREBUILT_PPC_DIR` is optional and points CMake at the restored bundle.
CMake imports `LostOdysseyRecompLib.lib` and omits generated PPC C++ compilation;
clearing the variable selects the normal source build. The contract is x64
clang-cl, Release, `/MT` static CRT and non-LTO. Retain codegen input/output
receipts, simde/mmio and CMake hashes, compile flags/includes, shard metadata and
SHA256 for shards and the restored library.

For offline bundle diagnostics, from `cmd` or an x64 Developer Command Prompt,
initialize the compiler environment and export to the new empty directory:

```cmd
call tools\setup_windows.bat
python tools\release\ppc_prebuilt.py export --build-dir out\build\fps-0.5.4 --output out\ppc-export
```

Then restore and check the fresh export from PowerShell:

```powershell
python tools/release/ppc_prebuilt.py restore --bundle out/ppc-export --output out/ppc-prebuilt
python tools/release/ppc_prebuilt.py check --bundle out/ppc-prebuilt --build-dir out/build/fps-0.5.4
```

`export` only incrementally builds the PPC library and is retained for offline
diagnostics. At that historical checkpoint, the upload path was the post-build hook or
`ppc_sync.py sync`; it resolved PPC by immutable `ppc/<key>` branch and did not
require a PPC workflow SHA update. Do not place bundles in the public checkout or
release assets.

The standalone `test-ppc-prebuilt.yml` workflow runs the 13 synthetic bundle
checks separately from release packaging; those checks pass, and actionlint
1.7.12 passes for both workflows. The real Release/x64 clang-cl PPC-only export
also succeeded: four shards, 138,454,798 bytes, SHA256
`ba3e4c4dff009d6d8e844c007186a6e5040266875bca6423f8fe26f8d27fb21b`. Restore and
the isolated CMake `LoPpcPrebuiltCheck` passed with the `/MT` non-LTO contract.
The runtime Ninja dependency/link graph has zero PPC compile commands and
references the imported library; the runtime was not relinked or launched.

The four shards were uploaded to the existing private repository at commit
`77f076e0e03966736cbf8919ce793bafadce82d9`, and API readback matched the local
manifest and all four blob IDs. Evidence is retained in
`out/ppc-evidence/upload-verification.json` and `runtime-graph.json`.

The implementation was pushed to `main` at commit
[`2b5b1d1d0d3c0a1a2d5404cdc29bbf9f3aa75e4e`](https://github.com/freefrank/LostOdysseyRecomp/commit/2b5b1d1d0d3c0a1a2d5404cdc29bbf9f3aa75e4e).
The standalone synthetic PPC workflow completed successfully in
[run 34553414428](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34553414428).
That historical checkpoint predates the published v0.5.6 release.

Historical status, 2026-09-10: resolving the library by `ppc/<key>` instead of a
pinned private SHA is pushed to github/main as
[`2c0456c`](https://github.com/freefrank/LostOdysseyRecomp/commit/2c0456c).
Hosted [PPC prebuilt tests](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34565564964)
passed; the v0.5.6 hosted Release CI and publication subsequently passed.
At that checkpoint, this work was Unreleased and was not in published v0.5.4.

## Local PPC auto-sync — historical checkpoint

Historical status, 2026-09-10: the auto-sync source is pushed to github/main as
[`2c0456c`](https://github.com/freefrank/LostOdysseyRecomp/commit/2c0456c).
Hosted [PPC prebuilt tests](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34565564964)
passed; the v0.5.6 Release subsequently passed. At the historical checkpoint, the
workflow was absent from published v0.5.4 and had no hosted release end-to-end run.
User gameplay acceptance remains separate.

The local post-build hook requires `git config --local lo.ppcAutoSync true`.
The CMake option reads that setting; if an existing cache is `OFF`, reconfigure
with `-DLO_PPC_AUTO_SYNC=ON` as needed, while `OFF` disables the hook. CMake alone
does not grant the script's upload authorization. After a successful source PPC library build, it invokes
`ppc_sync.py sync --already-built`. At that historical checkpoint, a matching input
and compiler-argument SHA256 key reused the existing immutable private branch without
compilation or upload; a changed key created `ppc/<key>` with dynamically sized shards
of at most 40 MiB and retained older branches.

The hook is excluded for CI, imported libraries and `LO_PPC_SYNC_ACTIVE`. Release
callers reuse the already-built library. Other configurations may create the
independent `out/build/ppc-sync-Release` configuration for a one-time Release PPC
build with the hook disabled, preventing recursion. Sync failures surface as a
build or retry failure and do not silently fall back. The `key` command is
read-only and does not use the network; `--force` is a one-run manual override,
while `--already-built` is for the internal hook.

Nineteen synthetic sync cases pass. Separately, the built-library roundtrip and
change-during-build cases pass. The real CMake `LoPpcAutoSync` target passed after
re-archiving the library, with zero PPC C++ compilation and no runtime build or
launch. It created private branch `ppc/4d21302a4eef224c82691878fbcb6cd2f427b60d676b3e692e78598257b5d1b4`
at commit `5e80263491b39dc0012146dd3a31cf5eea533225`; the subsequent same-key
sync reported unchanged. Sparse-clone restore and `check` passed against the
isolated Release contract, whose key matched. Evidence is retained in
`out/ppc-auto-sync-evidence/build-sync.log`, `github-output.txt`,
`github-output-second.txt` and `out/ppc-sync/receipt.json`. The auto-sync source is pushed to github/main as
[`2c0456c`](https://github.com/freefrank/LostOdysseyRecomp/commit/2c0456c);
hosted [PPC prebuilt tests](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34565564964)
passed, and a new Release remains pending.

`export` requires a new empty output directory; `restore` may use an existing
output directory according to its normal merge/replace behavior. At the historical
checkpoint, the PPC flow, local validation and private upload were complete. Hosted release end-to-end
validation, a new release, and user acceptance remained pending at that checkpoint.

## Current PPC main synchronization — 2026-09-13

The PPC synchronization update publishes an ordinary fast-forward of the private
`freefrank/LostOdysseyRecomp-build-inputs` `main` branch, preserving unrelated archive
files and retaining the existing `ppc/<key>` branches as historical build-selection
references. Concurrent advances are retried within a bounded limit; the flow does not
create new PPC branches. The latest private cache identity is already on private `main`
at `9e387adc045fe3b8ba4e6d1956812d11050bc9ed`. The implementation remains unreleased;
real GitHub Release CI has not yet proved this new workflow.

Release CI checks out private `main` with the SSH deploy key, validates the PPC
fingerprint and compile contract before restoring the library, and records the immutable
private checkout HEAD in the PPC identity artifact. A mismatch fails with guidance to
synchronize the matching cache or dispatch with `rebuild_ppc` enabled. `lo.ppcAutoSync`
remains unset locally; this documentation does not claim that the hook is enabled
automatically. `actionlint` passed for the workflow change. The PPC sync suite
passed 23 tests, including six bare-Git integration cases for single-ref main updates,
unrelated blob preservation, stale PPC cleanup, bounded concurrent retry behavior,
same-key no-op, identity rejection and policy rejection. Two isolated synthetic workflow
checks also passed: a matching manifest recorded the checkout commit, while an input
mismatch was rejected. Runtime/gameplay validation remains separate.

## Published v0.5.6 release — 2026-09-13

The merged `main` source version 0.5.6 was built by Release CI `34726533463` for
source commit `7124f4b3912df715167acf01f469974045cc3e08`; publication completed at
`2026-09-13T00:05:47Z`. The producer PPC key
`50b8ad415be405b302252558e0fd960913c3ce6a15d991ae3607142f1a3821a5` was uploaded at
private commit `6a6ed03152431a232165e35b19b7f94f09bbbda9`. The CI-compatible key
`d89197759478260d7e135b654993d57cff30127f1d41cf30c410fd0ae4be27f4` was consumed
from private commit `a6cd91ea35261dd202b78e93b4acb65973369d07`. The library SHA-256
remained `ba3e4c4dff009d6d8e844c007186a6e5040266875bca6423f8fe26f8d27fb21b`; no
generated PPC C++ compilation or new PPC static-library link occurred. The existing
library was linked into the runtime. The 19 input representation differences
were explained by five line-ending and fourteen symlink placeholder/target-content
differences; generated outputs, PPC headers and the Release contract remained
identical. `lo.ppcAutoSync=false` remains unchanged. This records CI compatibility,
not source-level key normalization.

The published ZIP is 44,255,182 bytes with SHA-256
`ad6616480fa8905936b3b36d202deb2dad356f07670a2e0f1570984016e897d9`; all 50
manifest files passed hash/CRC and clean source-version provenance checks. The
standalone updater is 849,920 bytes with SHA-256
`d3356d3fcac410e3ee86c012dc4971ffa4ee507b76f28eebf79e2c575a7eaf74` and matches
the ZIP copy. All four anonymous downloads returned HTTP 200 with verified hashes
and sizes. Evidence: `out/v0.5.6/release/{published-release.json,public-download-check.json,delivery-verification.json,ci-run.json,ci-ppc-consumption.json,ppc-ci-upload.json,release-source.json}`.

## Verification

Hosted build 34010819664 completed successfully for commit `2a3ffcc` and supplied v0.1.
The published ZIP SHA256 is
`f7ffb324c789d2d86e3df6c7d715066dff70ea6ec276de910e0113c08d98d9d1` (38,183,265 bytes).
Only the download filename was changed; the archive bytes match the CI artifact. Isolated
validation ran the installer self-test with a system-only PATH and the packaged game for
40 seconds. The shader index covered 52 files with no full scans; game language 9 was selected.

```powershell
python -m unittest discover -s tools/tests -p test_import_game.py -v
python tools/installer/import_game.py <source>
python tools/installer/import_game.py <source> --destination out/import-test/game
```

Without `--destination`, inspection is read-only and prints discovered XEX identities.
The tests use synthetic images and XEX headers, not redistributed game data. They cover
folder/XEX import, padded ISO, GOD hash-group boundaries, cancellation, incomplete resources,
wrong title, unsafe paths, malformed directories, missing chunks and existing-disc protection.

The supplied X360CH176 GOD directory was identified as the supported four-disc set. X360CH123
was identified as a different Title ID, 4D5307DF, demonstrating why folder names alone are
insufficient for compatibility checks.

Local validation on 2026-09-05: all four GOD discs imported into an isolated directory;
all 60 output files (21,768,192,000 bytes) matched the existing extraction by SHA256.
Fourteen fixture tests passed, including concurrent-import locking and source-XEX mutation.
The complete Release build and ZIP dependency audit passed. The frozen installer initialized
with only Windows system directories on PATH. The packaged game then completed a cold shader
scan/preparation and reached the title screen, running for 80 seconds before test cleanup.
Loaded-module paths confirmed both DXC DLLs came from the package. The existing two shader
preparation failures remained (2 of 2,000), not a new packaging failure. Runtime smoke evidence
is under `out/package-smoke`, import hashes under `out/installer-validation/verification.json`.
The two Actions workflows passed actionlint locally; no hosted CI run has been performed yet.
The v0.5.0 development candidate was built and package-checked locally. Its ZIP is
`LostOdysseyRecomp-windows-x64-v0.5.0-147bffb2-dev.zip` (43,862,583 bytes, SHA256
`1afcc6b56550bfb9d41e4799b0f747beda8002f0739ddead9a4c81959eaffcbb`). The main build used
Release clang-cl/Ninja (session 4497); packaging exited 0 (session 47942). The candidate has not
been committed, pushed, tagged, deployed or published, and hosted CI/anonymous download remain
future publication checks.

The current package starts through LostOdysseyRecomp.exe directly (Windows GUI subsystem).
Without explicit `--game`, it anchors portable settings/logs/cache to the executable directory,
reads `game-path.txt` or `game/disc1`, and opens the bundled importer when assets are missing.
The importer returns to the game automatically when launched this way. CMD/PowerShell launch
scripts are no longer included. Explicit `--game` retains the caller's working directory for tests.

First launch with no settings.ini opens a native setup window before XAM/guest initialization.
It offers five interface and game languages, output resolution, display mode and FXAA; DLSS/FG
remain disabled placeholders. Saving uses the existing atomic settings writer. Cancellation
does not create settings or start the guest. Existing settings bypass setup; `--setup` reopens it.
Background/headless diagnostics bypass automatic setup; `--setup` explicitly forces it.

Validated five setup UI languages, cancellation, and same-process selection of game language 9:
the resulting original game main menu displayed Simplified Chinese on the first run, without
restart (`out/first-run-validation/game-language.png`). Automatic first-run invocation from an
unrelated working directory and clean cancellation were also tested. The built-in shader index
completed in 989 ms during that cold start; see [index evidence](shader-resource-index.md).

References: [GitHub workflow syntax](https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-syntax),
[Microsoft DXC release](https://github.com/microsoft/DirectXShaderCompiler/releases/tag/v1.8.2407),
and the [v0.5.0 release preparation matrix](../RELEASE-v0.5.0.md).
