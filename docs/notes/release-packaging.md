# Installer and Windows release pipeline

The portable importer lives in `tools/installer`. Its backend uses only the Python standard
library. The graphical frontend uses Tk and is frozen into a single InstallGame.exe using
PyInstaller. No interpreter installation is needed on the destination machine.

## Local build

Prepare submodules and generate PPC sources as described in BUILDING.md. Then:

```powershell
tools/build_release.bat
python -m venv out/installer-venv
out/installer-venv/Scripts/python.exe -m pip install -r tools/release/requirements.txt
out/installer-venv/Scripts/python.exe tools/package_release.py
```

The dedicated `out/build/release` directory uses clang-cl, Release and static CRT.
`LO_BUILD_JOBS` defaults to 4; the hosted workflow uses 2 to limit memory pressure from the
generated C++ files. Existing development builds are not overwritten.

Packaging downloads the fixed Microsoft DXC v1.8.2505.1 archive and verifies its pinned SHA256.
Both x64 dxcompiler.dll and dxil.dll come from that same archive. No Windows system DLLs are
copied from the developer's machine. A PE import audit rejects missing non-system dependencies,
including an accidental dependency on an unbundled Visual C++ runtime.

Only explicitly selected payload files enter the ZIP. Game data, saves, settings, shader caches,
private build inputs, generated source, logs and PDBs are not packaged. The manifest records the
source commit, development state, payload checksums and DLL imports. The package has a separate
SHA256 file. Dependency license texts accompany the binaries.

## GitHub Actions

`test-importer.yml` runs public fixture tests on pushes and pull requests without game data.
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
[Microsoft DXC release](https://github.com/microsoft/DirectXShaderCompiler/releases/tag/v1.8.2505.1).
