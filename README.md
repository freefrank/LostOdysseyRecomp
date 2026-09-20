<div align="center">

# Lost Odyssey Recomp

**An experimental native PC port of Lost Odyssey for Xbox 360.**

Windows x64 · Direct3D 12 · Vulkan · PowerPC static recompilation

<img src="docs/images/title-screen.png" alt="Lost Odyssey title screen — Press START" width="960">

Optional diagnostics are off by default and can be disabled in Settings. See [Privacy](PRIVACY.md).

### [Latest download](https://github.com/freefrank/LostOdysseyRecomp/releases/latest) · [Installation guide](docs/INSTALLING.md) · [Report an issue](https://github.com/freefrank/LostOdysseyRecomp/issues)

[简体中文](README.zh-CN.md) · [Changelog](CHANGELOG.md) · [Projects](https://github.com/users/freefrank/projects/3) · [Build from source](docs/BUILDING.md)

</div>

> [!IMPORTANT]
> **This project is still in early testing.** Opening areas and selected scenes have been tested; a complete playthrough has not. Rendering and stability issues remain. You must supply your own supported game files.

## v0.6.7 release

Published release [v0.6.7](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.7) on 2026-09-20T20:09:28Z from source `f92c24d` via Release CI [35533399325](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35533399325) as the latest public release. It adds an in-game Graphics menu Widescreen switch and expanded 21:9 resolution presets (1720×720, 2560×1080, 3440×1440, 3840×1600, 5120×2160), with closest vertical height matching when toggling aspect ratio, automatic detection for existing configurations, and synchronization with first-launch setup across 5 languages. Issue #17 is resolved and closed.

> [!WARNING]
> **Ultrawide support remains EXPERIMENTAL across diverse hardware and aspect ratio combinations.**

See the [changelog](CHANGELOG.md#v067--2026-09-20) and [development status](docs/STATUS.md) for validation limits.

## v0.6.6 release

Published release [v0.6.6](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.6) (reissued 2026-09-20 from source `c6cbd1f` via Release CI [35527543573](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35527543573)). It introduces initial native ultrawide (21:9) support (Issue #17), a shadow-map rendering repair across all aspect ratios and high internal resolutions, and Linux AppImage updater rollback-preserving cleanup.

The shadow fix repairs effective-height render-target caching and depth-only rasterization modes 4 and 5; shadows were confirmed fixed in user testing of the affected scene. Reissue packages have been verified and uploaded; the initial `c953bb5` packages are superseded, and players who downloaded the earlier build should redownload to get the fix.

See the [changelog](CHANGELOG.md#v066--2026-09-20) and [development status](docs/STATUS.md) for validation limits.

## v0.6.3 release

Published at [GitHub Release v0.6.3](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.3) on 2026-09-19T23:56:08Z. It restores bounded sampled comparison for large vertex-cache hits to reduce CPU comparison cost while keeping small vertex buffers and index-cache source validation exact. `LoVertexCacheTest` passed 3,668,957 focused checks; no release-binary performance or full-game result is claimed. It also includes the Issue #54 language-menu safety correction, Issue #53 file-I/O locking and bounded diagnostics, deterministic I/O lifetime regression coverage, and platform-native asynchronous F1 render-state archives. The Windows ZIP and Linux AppImage, plus their sidecars, passed package hash and public delivery checks. Linux native GPU, Steam Deck, AppImage runtime and broader gameplay remain pending. See the [changelog](CHANGELOG.md#v063--2026-09-19).

## v0.6.2 release

Published release: [v0.6.2](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.2). The Windows ZIP and Linux AppImage include the shader set; this release has no separate shader package. v0.6.2 packages experimental geometric motion-vector replay and the accepted main-path TAA policy. The normal TAA path uses 0.5 jitter scale, stationary motion snapping, stationary color clipping and multi-surface history with RGBA8 history at `31/33`; experimental FP16 history and moving bilinear fallback remain off. On Vulkan with an RTX 5080, the same Uhra 4K scene was accepted by the user at about 60 FPS. This is scene- and machine-limited evidence, not whole-game or cross-platform acceptance.

The candidate comparison measured 60.34/59.00 FPS against 54.61 FPS for a separate Release build and 54.57 FPS for the previous RelWithDebInfo main binary in hidden muted A-B-A-B captures without pacing. The 1080p-internal to 4K moving-camera limitation, broader scene coverage, D3D12 replay PSO follow-up, Linux native GPU and Steam Deck validation remain open.

## v0.6.1 release

Published release: [v0.6.1](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.1). It checks for updates before importing game data on Windows and Linux. A newer release opens an app-branded prompt with release notes and **Install** or **Later** actions; accepting applies the update and relaunches before import. Download progress remains in the existing updater window.

Release CI [35374267882](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35374267882) passed the Windows and Linux release jobs and focused regressions. Live update acceptance, physical controller input and network downloading remain unverified.

## v0.6.0 release

Published release: [v0.6.0](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.0). It provides Windows x64 and native Linux x64 packages. The Windows ZIP and Linux AppImage bundle the portable Vulkan shader pack so supported installations can start without a long first-run compilation step. The bundle covers the tested shader set; an uncovered shader can still compile on demand and may cause a brief hitch.

- **Large performance and stability pass**: repaired shader and pipeline preparation, wait and thread lifetime, presentation, clock, updater, geometry cache, and Linux runtime paths. The earlier 15 W measurements used sampled cache matching and are not final v0.6.0 FPS evidence; Steam Deck hardware acceptance remains open.
- **Native Linux release**: Vulkan ELF and AppImage packaging are included in the release scope. Linux validation currently covers WSL2 with Mesa Dozen; native Linux GPU, AppImage update transactions, Steam Deck, and full-game playthrough remain open.
- **Hardened importer**: final writes, flushes and closes are checked before publication, XDVDFS scanning follows 2048-byte boundaries, and the destination browser can create and enter a folder with the button, `F2`, or controller `Y`.
- **Real source validation**: the importer recognized all four USA/Europe disc images under `G:/ROMS/US`; an isolated Disc 1 import completed successfully. Four-disc installation, interactive UI acceptance, and gameplay remain unverified.

The next **v0.7.0** milestone is planned to continue performance work and add quality-of-life features, DLSS/FSR scaling, frame generation, and a macOS release.

The release packages and standalone shader pack are available from the [v0.6.0 release](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.0). Release CI passed the required audit and Windows/Linux packaging gates. The published artifacts were verified against their SHA-256 sidecars; native Linux GPU, Steam Deck, AppImage update transactions, and full-game playthrough remain outside the verified scope.

Earlier release details are maintained in the [changelog](CHANGELOG.md).

## Start playing

1. **Download and extract** the entire Windows release ZIP to a writable folder.
2. **Run `LostOdysseyRecomp.exe`** and import your game files when prompted. The importer accepts an extracted folder, `default.xex`, an XDVDFS ISO or a GOD container.
3. **Choose your language and graphics settings.** The game continues after setup and shader preparation.

No Python or Visual Studio installation is needed for the release package. Later launches reuse the shader cache. Keep your save and profile folders when updating.

The published updater checks GitHub's latest Release: a higher numeric version updates, and an equal numeric version with a different `-suffix` also triggers an update. The v0.5.7 release additionally permits recovery from an empty updater-only folder and stale or malformed local metadata. After a successful update, the helper asks whether to launch the game and defaults to **No**; silent runs complete without launching. Download integrity checks, safe extraction and rollback remain enabled.

| Requirement | Supported configuration |
| :--- | :--- |
| System | Windows x64, AVX-capable CPU, Direct3D 12 or Vulkan graphics driver |
| Game data | Audited Europe, Asia or USA, Europe edition; Disc 1 is required to start |
| Additional discs | Import additional discs or DLC from the built-in importer in `LostOdysseyRecomp.exe`; later-disc progression is not fully verified |

See the [installation guide](docs/INSTALLING.md) for accepted disc versions, file locations and updating.

## In-game screenshots

| Ring combat | City exploration |
| :---: | :---: |
| ![Kaim attacking with the Ring timing interface](docs/images/ring-battle.png) | ![Exploring the industrial city](docs/images/city-exploration.png) |

*Unmodified screenshots from development builds leading up to v0.1.*

## Current features

| Feature | What to expect |
| :--- | :--- |
| Game importer | Folder, XEX, ISO and GOD input; originals stay untouched, and staged copies check final writes before publication |
| First-launch setup | Language and graphics settings before game initialization |
| Language settings | English, Japanese, Korean, Traditional and Simplified Chinese interface options; game language selection |
| Graphics settings | Auto/manual internal resolution up to 4K, 16:9 / 21:9 resolution presets with Widescreen toggle, Off/FXAA/SMAA/experimental TAA, Standard/High filtering, 30/60 FPS and output/display controls; fullscreen and mixed DPI need more testing |
| Settings menu | Original game fonts and menu styling; one-click Graphics save/apply and Now/Later restart choices |
| Shader preparation | Bundled portable Vulkan shader pack (.lospv), memory-adaptive parallel compilation, interactive skip, and cache reuse |
| CPU use | Reduced unnecessary polling and reuse of rendering work |
| Input and debug | Controller and keyboard input; English/Simplified Chinese in-game overlay debug menu (F1 or LB+RB) with capture, map information and same-map POI teleport |

Published packages import game discs and supported DLC with **Files** or **Folder** in `LostOdysseyRecomp.exe`. The same built-in importer can be reopened when assets are missing. See the [installation guide](docs/INSTALLING.md#automatic-content-import) and [development status](docs/STATUS.md) for validation limits.

Validation progress and remaining work are tracked in the [Maintainer Project](https://github.com/users/freefrank/projects/3).

<details>
<summary><strong>Game edition and compatibility details</strong></summary>

The two supported editions correspond to [Lost Odyssey (Europe, Asia) (En,Ja,Zh,Ko) (Disc 1), Redump 39111](https://redump.info/disc/39111) and [Lost Odyssey (USA, Europe) (En,Ja,Fr,De,Es,It) (Disc 1), Redump 11817](https://redump.info/disc/11817). The former is called the Asian edition here: Disc 1 has title ID `4D5307FA`, media ID `39F7D748`, title/base version `0.0.0.4` and XeMID `MS204204H0X14`. The USA, Europe Disc 1 has media ID `368DE6DD`, version `0.0.0.3` and XeMID `MS204203W0X14`. These identities match the audited sets; a complete ISO hash comparison against Redump has not been performed. The importer strictly checks each supported XEX hash; a region label alone is insufficient.

The **USA, Europe version 0.0.0.3** four-disc set is supported, with strict XEX checks and protection against mixing editions. Game-language choices follow the installed edition: English/Japanese/German/French/Spanish/Italian for USA, Europe; the audited Europe, Asia resources retain English/Japanese/Korean/Traditional Chinese/Simplified Chinese choices. See [edition details](docs/notes/europe-support.md).

With all four discs imported, the game selects them automatically; no manual disc swap is needed. See [disc handling](docs/notes/disc-selection.md).

Language options do not imply a complete playthrough in every language. Regional builds outside the audited sets, title updates and modified XEX files are not validated. See [edition evidence](docs/notes/xex.md).

</details>

<details>
<summary><strong>Build commands and repository layout</strong></summary>

### Build and run

Prepare your own extracted data, dependencies and generated sources using the [build guide](docs/BUILDING.md). Helper scripts discover installed tools; custom paths can be supplied through environment variables.

```powershell
.\tools\build_runtime.bat
$gameData = (Resolve-Path .\LostOdysseyRecompLib\private\disc1).Path
Push-Location .\out\build\windows-clang\LostOdysseyRecomp
.\LostOdysseyRecomp.exe --game $gameData --quiet-kernel
Pop-Location
```

Keep the working directory consistent so the intended save/profile folders are used.

**Startup and failure logs.** Each normal launch writes `logs/runtime-<timestamp>.log` in the working directory and mirrors output to `stderr`; set `LO_LOG_FILE=<path>` to choose another file, or `LO_LOG_FILE=0` to disable the duplicate file sink. v0.5.11 additionally records the Windows build, process/native architecture, source/build revision, PE image metadata, compiler, startup memory baseline, GPU, raw driver version, vendor/type and `reported_device_memory_bytes`. When reporting a startup or renderer failure, attach the complete current runtime log and include the executable/source version, backend, GPU and driver details recorded near startup. The diagnostic records preserve raw API codes and the failed resource or allocation context, but they are investigation evidence and do not by themselves identify a root cause. See the [build and logging guide](docs/BUILDING.md) for the path and retention rules.

For a visual issue, press **F1** while it is visible and choose **Capture render state**. Wait for the background archive to finish, then attach the archive at the path shown by the status message: Windows produces a `.zip` archive and Linux produces a `.tar.gz` archive. If archiving fails, the raw capture folder is retained for recovery.

| Action | Keyboard |
| :--- | :--- |
| Start / Back | Enter / Backspace |
| A / B / X / Y | Z / X / A / S |
| D-pad / left stick | Arrow keys / I, J, K, L |
| Left / right shoulder | Q / W |
| Left / right trigger | E / R |
| Debug menu | F1 / Gamepad LB+RB |

SDL-mapped controllers and the keyboard can be used together for player 1. Unmapped joysticks need an SDL controller mapping. See [input details](docs/notes/controller-input.md).

Rumble is disabled by default; `LO_CONTROLLER_RUMBLE=1` enables it. For Ring actions, use the controller's right trigger or the R key.

### Development

| Directory | Contents |
| :--- | :--- |
| `LostOdysseyRecomp/` | Host kernel, graphics, audio, input and debugging |
| `LostOdysseyRecompLib/` | Configuration; ignored `private/` game data and generated `ppc/` code |
| `tools/` | Recompilers, dependency patches, Ghidra scripts and the optional [assembly profiler](tools/asm-profiler/README.md) |
| `thirdparty/` | Rendering, audio and other dependencies |
| `docs/` | Current status, guides, research and historical archives |

[Roadmap](docs/ROADMAP.md) · [Handoff](docs/notes/handoff.md) · [Rendering tests](docs/notes/rendering-validation.md) · [TAA live debug](docs/TAA_LIVE_DEBUG.md) · [Audio](docs/notes/audio-output.md) · [Archive](docs/archive/README.md)

</details>

## Sponsors

Thank you to **Cristian** and **Whitesun** for supporting the project on Ko-fi.

## Credits and game data

With research and tools from [UnleashedRecomp](https://github.com/hedge-dev/UnleashedRecomp), [re:Blue](https://github.com/zolaware/reblue), [XenonRecomp](https://github.com/hedge-dev/XenonRecomp), [XenosRecomp](https://github.com/hedge-dev/XenosRecomp), [plume](https://github.com/renderbag/plume) and [Xenia](https://github.com/xenia-project/xenia). Audio uses the pinned [Xenia FFmpeg fork](https://github.com/xenia-project/FFmpeg), with its [license](thirdparty/ffmpeg-LICENSE.txt).

Lost Odyssey and its assets belong to their respective owners. This is an unofficial project. Supply data extracted from your own discs; do not submit game executables, resource archives, textures, audio, video, generated game code or captures. Dependencies retain their respective licenses.
