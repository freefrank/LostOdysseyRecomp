# v0.5.0 release preparation

This is a historical preparation record for the Windows x64 v0.5.0 development candidate. It is
not a published release note and does not define the current source version. The candidate was
built and verified locally, but had not been committed, pushed, tagged, deployed or published at
the time of this record. Current feature batches use the 0.4.xx development sequence while the
target milestone remains v0.5.0; see [current status](STATUS.md) for the active mapping.

## Current release scope — 2026-09-08

The user confirmed Windows D3D12/Vulkan as the release scope; DX11 is future work outside the v0.5.0 commitment. Other GPU coverage awaits user feedback. Installer, updater and Debug Menu UI passes are complete in later local source; directory-filter repair and runtime02 validation read all three real DLC packages without crashes, while rewards and dungeon gameplay remain unverified. Issue #12 production validation remains bounded and reporter acceptance, full-game coverage and release remain pending. Source 0.4.23 includes the unified Simplified Chinese labels, one-click Graphics save/apply and the original guest return path. Its fixture and one hidden Windowed D3D12 runtime path passed at 144 DPI; broader GPU/DPI coverage and new user visual acceptance remain separate. The 0.4.23 development package is recorded at `out/v0.5.0/settings-replacement-flow/packages/LostOdysseyRecomp-windows-x64-v0.4.23-df59dcab-dev.zip`; publication remains pending. The 0.4.22 asset package and hash remain historical. Intermediate 0.4.xx builds remain internal and will be delivered together in v0.5.0.

The menu-asset implementation uses the selected installed `LO.fpi` package's native `Maru23`, optional same-package `Abc` fallback and `UI_MAIN_00` assets. It adds no bundled game assets, Python runtime or extra runtime DLL; the static MIT `lzokay` dependency is licensed separately. See [menu asset evidence](notes/menu-original-assets.md).

## Dependency and package checks

| Area | Current preparation result | Boundary |
| --- | --- | --- |
| Runtime dependencies | Build-adjacent official Microsoft DXC v1.8.2407 x64 `dxcompiler.dll` and `dxil.dll` pair, with matching license/provenance records | Verified in the candidate; no dependency upgrade is implied |
| Vulkan loader | Supplied by the installed graphics driver; no Vulkan SDK loader is bundled | Windows/RTX 5080 validation only; AMD/Intel remain open |
| Payload allowlist | 49 payload files and 42 license files; excludes game data, saves, profiles, settings, shader caches, generated source, logs and PDBs | Candidate-only package; no public download is claimed |
| Installer | Python standard-library backend; frozen Tk/PyInstaller `InstallGame.exe`; direct executable startup and bounded folder/XEX/ISO/GOD discovery | Frozen-media and DPI breadth remain open |
| Updater | Formal packages may check for a matching GitHub release; development packages skip automatic update checks | No hosted release or multi-platform updater delivery is claimed |

The dependency source and package rules are maintained in [release packaging](notes/release-packaging.md)
and [installing](INSTALLING.md). The published baseline remains [v0.4.2](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.4.2).

The local candidate is [LostOdysseyRecomp-windows-x64-v0.5.0-147bffb2-dev.zip](../out/v0.5.0/release-readiness/package/LostOdysseyRecomp-windows-x64-v0.5.0-147bffb2-dev.zip), 43,862,583 bytes, SHA256 `1afcc6b56550bfb9d41e4799b0f747beda8002f0739ddead9a4c81959eaffcbb`. The main executable is 82,460,160 bytes with SHA256 `b88c0d3e1a5a250757e61428bb9a0d4e66529b4fda2d38471c51a66de50a62b2`. Manifest hashes, CRCs, duplicate-member checks, DXC provenance, licenses and top-level PE imports passed; see `out/v0.5.0/release-readiness/package-validation.json` and `manifest.json`.

The unique Release clang-cl/Ninja main build completed in session `4497` and packaging completed in session `47942`. The checks did not run the game, installer or feature suites. The package is a local development candidate only; v0.4.2 remains the published baseline.

## Backend capabilities and limits

| Backend | Capability checked | Current limit |
| --- | --- | --- |
| D3D12 (default) | Native FL 11 device, DXIL/Shader Model 6.0 and binding tier 2 or better; the accepted Windows path uses the native D3D12 device and queue lifecycle | Windows RTX 5080 scope; no claim for every GPU or the full game |
| Vulkan (optional) | Vulkan API 1.2 or newer with geometry shader, buffer device address, 64-bit integer, scalar block layout and required descriptor/resource limits; SPIR-V is the shader format and the loader comes from the driver | Windows RTX 5080 Maps 2/3/12 and bounded lifecycle coverage; AMD/Intel, other platforms and full-game coverage remain open |
| DX11 | Recognized as an explicit unsupported request; selection can report it and fall back through the supported D3D12/Vulkan policy | No DX11 runtime integration, device validation or scene acceptance |

The accepted rendering checks include the existing bounded FXAA/SMAA/TAA paths and their documented
Map-scoped comparisons; they do not establish whole-game anti-aliasing quality or cross-GPU behavior.
See the [current status](STATUS.md) and the retained [Vulkan report](../out/v0.5.0/vulkan/REPORT.md)
for the exact validation boundary.

## Validation retained for the candidate

The user accepted the bounded D3D12/Vulkan scene evidence. The lifecycle report records hidden
window/swapchain resize, reset/re-init, owner-thread cleanup and controlled process restart for
the actual video/presentation/Plume path. It omits guest, renderer, audio, draw, present, capture,
full-game and cross-GPU coverage; see [lifecycle evidence](../out/v0.5.0/backend-lifecycle/REPORT.md).
Typed backend cache and capability checks remain at the [cache report](../out/v0.5.0/backend-completion/cache/REPORT.md)
and [selection report](../out/v0.5.0/backend-completion/selection/REPORT.md). Existing shader failures,
DX11 support, other GPUs and complete playthrough coverage remain open.

## Publication gate

Before any future publication, verify the final source/version/stamp/tag agreement, the clean-checkout
guard, final local acceptance evidence and publication state. Hosted CI is not required by the current
user instruction; no new CI run is claimed. The local candidate's source/build provenance, DXC
provenance, PE imports, ZIP contents, manifest hashes and CRCs have already passed. This record does
not claim a 0.5.0 tag, anonymous download or public release.
