<div align="center">

# Lost Odyssey Recomp

**An experimental native PC port of Lost Odyssey for Xbox 360.**

Windows x64 · Direct3D 12 · Vulkan · PowerPC static recompilation

<img src="docs/images/title-screen.png" alt="Lost Odyssey title screen — Press START" width="960">

Optional TAA shader collection asks for consent during first-time setup, or when an existing player next opens Settings. It sends bounded shader summaries and compressed 32-frame sparse camera-motion/depth sequences (including jitter and camera matrices) to `lo.dotslash.pro`; schema 2 summaries may also include conservative position evidence for unknown vertex shaders, while schema 1 remains supported. Disable it in Settings → Language. No raw logs, local paths, saves, color images or shader source are uploaded. Shader anomaly summaries take priority over the lower-priority temporal archive. See [collection details](tools/taa-collector/README.md).

### [Download v0.4.2](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.4.2) · [Installation guide](docs/INSTALLING.md) · [Report an issue](https://github.com/freefrank/LostOdysseyRecomp/issues)

[简体中文](README.zh-CN.md) · [Project status](docs/STATUS.md) · [Roadmap](docs/ROADMAP.md) · [Maintainer Project (public)](https://github.com/users/freefrank/projects/3) · [Build from source](docs/BUILDING.md)

</div>

> [!IMPORTANT]
> **This project is still in early testing.** Opening areas and selected scenes have been tested; a complete playthrough has not. Rendering and stability issues remain. You must supply your own supported game files.

## New in v0.4.2

Fixes the reproduced Uhra Council cutscene crash and nine further PowerPC translation defects, and writes native crash details to automatic runtime logs. Extends TAA to six verified battle paths; enemy-disappearance flicker remains unresolved. F1 compresses completed captures in the background and removes their raw folder only after success. Default logging retains the current file plus the two newest earlier logs, protecting active files and custom paths. Frame capture itself can still pause rendering.

See the [changelog](CHANGELOG.md), [validation scope](docs/STATUS.md) and [Council investigation](docs/notes/issue7-cutscene-crash.md) for the completed checks and remaining coverage.

## Previously in v0.4.1

v0.4.1 fixes the reported Map3 tire-shadow flicker with TAA enabled, confirmed by the user at the original position, and adds three-frame F1 exports with the runtime log.

## Previously in v0.4.0

Real internal resolution up to 4K, SMAA and experimental camera-based TAA, Standard/High filtering, saved frame-rate controls and automatic Asian/USA-Europe CPX indexing are included. Settings text renders at output resolution and Debug labels switch independently between English and Simplified Chinese. See the [changelog](CHANGELOG.md). Existing translated-shader caches rebuild after updating.

## Previously in v0.3.0

v0.3.0 is released. It expands discovery into compressed shader resources and XEX sources, generates bounded vertex-shader variants, and prepares previously recorded graphics pipelines on later launches. Initial scanning and compilation may take several minutes; later launches reuse caches. Coverage remains incomplete and this does not eliminate all stutter. See [validation details](docs/notes/shader-preparation.md).

## Fixed in v0.2.2

Fixes tested AMD black/dark scenes and depth-of-field output, plus Windows Unicode installation, startup-argument and save paths. NVIDIA RTX 5080 targeted regression and user visual acceptance passed; the official package also passed a Map 12 launch from a Chinese working directory. Issue #4 was closed after the path-repair explanation, but the local checks did not reproduce the complete reported gameplay crash and no later reporter acceptance is recorded. See [current Issue evidence](docs/STATUS.md#live-issue-reconciliation), [v0.2.2 release notes](docs/notes/release-0.2.2.md) and the [changelog](CHANGELOG.md).

## New in v0.2.1

**v0.2.1 is released.** It adds F1 next-frame render capture with automatic ZIP output and simultaneous SDL-mapped controllers/keyboard input, including E/R triggers. Capture is diagnostic, not an AMD fix.

## New in v0.2

**v0.2 is released.** It adds the audited USA, Europe four-disc edition, edition-specific game and voice language choices, import before first-run setup, and automatic access to imported discs. Once all four discs are imported, no manual disc swap is required. Both editions passed controlled original-manager switching tests; chapter-boundary gameplay and a complete playthrough remain unverified. See the [v0.2 release notes](docs/RELEASE-v0.2.md).

## Start playing

1. **Download and extract** the entire Windows release ZIP to a writable folder.
2. **Run `LostOdysseyRecomp.exe`** and import your game files when prompted. The importer accepts an extracted folder, `default.xex`, an XDVDFS ISO or a GOD container.
3. **Choose your language and graphics settings.** The game continues after setup and shader preparation.

No Python or Visual Studio installation is needed for the release package. Later launches reuse the shader cache. Keep your save and profile folders when updating.

The updater checks GitHub's latest Release. A higher numeric version updates normally; an equal numeric version with a different `-suffix`, such as `0.5.0-hotfix1`, also triggers an update. The updater policy is targeted for source version 0.5.1 and is not in the published v0.5.0 binary. Once a v0.5.1 Release is published, existing v0.5.0 clients can upgrade to it by numeric version; later suffix changes are handled by the new policy.

| Requirement | Supported configuration |
| :--- | :--- |
| System | Windows x64, AVX-capable CPU, Direct3D 12 or Vulkan graphics driver |
| Game data | Audited Europe, Asia or USA, Europe edition; Disc 1 is required to start |
| Additional discs | Import with `InstallGame.exe`; later-disc progression is not fully verified |

See the [installation guide](docs/INSTALLING.md) for accepted disc versions, file locations and updating.

## In-game screenshots

| Ring combat | City exploration |
| :---: | :---: |
| ![Kaim attacking with the Ring timing interface](docs/images/ring-battle.png) | ![Exploring the industrial city](docs/images/city-exploration.png) |

*Unmodified screenshots from development builds leading up to v0.1.*

**DLC in current v0.5.0 Windows builds:** Open `InstallGame.exe`, choose **Files** or **Folder**, and let the importer recognize game discs and Lost Odyssey STFS DLC automatically. Three real DLC packages were imported and the runtime read their headers, indexes and payloads in 24 reads total without a crash; imported files and user data remained unchanged. Rewards and dungeon gameplay remain unverified. See [installation instructions](docs/INSTALLING.md#automatic-content-import).

## Current features

| Feature | What to expect |
| :--- | :--- |
| Game importer | Folder, XEX, ISO and GOD input; original source files are copied |
| First-launch setup | Language and graphics settings before game initialization |
| Language settings | English, Japanese, Korean, Traditional and Simplified Chinese interface options; game language selection |
| Graphics settings | Auto/manual internal resolution up to 4K, Off/FXAA/SMAA/experimental TAA, Standard/High filtering, 30/60 FPS and output/display controls; fullscreen and mixed DPI need more testing |
| Settings menu assets | Selected installed language assets provide the native Maru23/Abc font path and original grey panel/gear treatment; Graphics settings save/apply on one click, restart-required changes offer Now/Later, and Back returns directly to the previous menu without the original confirmation dialog |
| Shader preparation | Built-in resource index, parallel compilation and cache reuse |
| CPU use | Reduced unnecessary CPU polling; one matched D3D12 scene measured 15.6% → 4.3% process CPU with both captures near 60 presents/s |
| Input and debug | Controller and keyboard input; English/Simplified Chinese F1 menu with capture, map information and same-map POI teleport |

DLSS, FSR and frame generation are not implemented; v0.4.0 removes the former disabled controls. HDR remains future work.

**Current development:** The current source and release target are both 0.5.0; the 0.4.19–0.4.23 feature history is retained as internal development history rather than separate releases. Current Windows builds provide D3D12 and Vulkan, automatic game/DLC recognition, original-style Settings with one-click Graphics save/apply, Now/Later restart handling, direct return to the previous menu without the original confirmation dialog, shader-cache reuse and reduced unnecessary CPU polling. Shader identity reuse, geometry preparation and precise pacing brought the fixed Map16 4K measurement from 48.01 to 59.76 RTSS FPS; other scenes and sustained whole-game performance remain unverified. The current release candidate also prioritizes capture-confirmed shader anomalies and retains optional sparse camera data for future temporal research. The candidate executable is recorded in [release preparation](docs/RELEASE-v0.5.0.md); publication state is tracked separately. The recorded Windows scope remains bounded: other GPUs need feedback, DX11/Linux/macOS/Switch are future work, DLC rewards/dungeons and full-game coverage remain unverified.

The v0.5.0 delivery scope is Windows D3D12/Vulkan; DX11, Linux, macOS and the experimental Switch port with u/Adoky are future work, and other GPU coverage awaits user feedback. Intermediate 0.4.xx versions remain internal. The matched D3D12 CPU comparison is bounded to its recorded scene and hardware; it is not a whole-game or Vulkan benchmark. Whole-game compatibility and two known shader failures remain open.

## Validation and remaining work

The v0.4.1 Windows package passed release CI, all 45 manifest entries, the installer self-test and eight startup-path checks. Its public download and checksum match the validated package. These official-package checks did not load a game or repeat GPU validation; the Map3 TAA/Off comparison and player acceptance belong to the preceding r2 candidate, whose rendering code is unchanged in the release.

Both audited editions previously passed isolated v0.4.0 official-package Map2 startup at Auto 1080p/TAA with verified bundled compiler libraries. That bounded static scene check does not establish full-game compatibility.

TAA remains experimental, lacks native object-motion vectors and falls back to SMAA on unsupported paths. Selected movement, dialogue and Ring core-timing checks passed for 60 FPS, but whole-game locked 60 and precise Ring release/Perfect are unverified. The unvalidated 120 FPS option requires `LO_EXPERIMENTAL_120=1`; otherwise it runs at an effective 60 FPS.

For a TAA flicker, ghosting or missing-object report, attach the complete log from `logs/runtime-<timestamp>.log`; while the problem is visible, use **F1 → Capture render state** and upload the resulting ZIP when possible. Screenshots or video are welcome, and if capture fails, send the complete log and explain what happened. Report through [GitHub Issues](https://github.com/freefrank/LostOdysseyRecomp/issues), including a concise description, reproduction steps and expected/actual behavior. See the [bug report template](.github/ISSUE_TEMPLATE/bug_report.md).

Reliable gameplay and faithful rendering come first. The project translates PowerPC code into C++ with **XenonRecomp**, implements Xbox 360 services on the host, and renders translated Xenos shaders through **plume**.

The v0.2 package passed hosted Windows CI, manifest and importer checks, and a 30-second isolated rendered launch. Ground-shadow and poster fixes have targeted validation; the user confirmed Ring timing works with RT. The accelerated-dialogue issue is resolved and user-confirmed; the repair passed comparison against the original audio in the tested vehicle scene.

**Open defects:** fire-hit and broken-crate effects, and intermittent GPU query/wait failures. Character-surface shadows, additional audio scenes and broader progression remain regression coverage. Two known shader-preparation failures remain. These results do not establish full-game compatibility.

[Detailed status and evidence](docs/STATUS.md) · [v0.2 release notes](docs/RELEASE-v0.2.md)

<details>
<summary><strong>Game edition and compatibility details</strong></summary>

The two supported editions correspond to [Lost Odyssey (Europe, Asia) (En,Ja,Zh,Ko) (Disc 1), Redump 39111](https://redump.info/disc/39111) and [Lost Odyssey (USA, Europe) (En,Ja,Fr,De,Es,It) (Disc 1), Redump 11817](https://redump.info/disc/11817). The former is called the Asian edition here: Disc 1 has title ID `4D5307FA`, media ID `39F7D748`, title/base version `0.0.0.4` and XeMID `MS204204H0X14`. The USA, Europe Disc 1 has media ID `368DE6DD`, version `0.0.0.3` and XeMID `MS204203W0X14`. These identities match the audited sets; a complete ISO hash comparison against Redump has not been performed. The importer strictly checks each supported XEX hash; a region label alone is insufficient.

v0.2 also supports the audited **USA, Europe version 0.0.0.3** four-disc set, with strict XEX checks and protection against mixing editions. Game-language choices follow the installed edition: English/Japanese/German/French/Spanish/Italian for USA, Europe; the audited Europe, Asia resources retain English/Japanese/Korean/Traditional Chinese/Simplified Chinese choices. The Redump Zh label does not independently establish that Simplified Chinese is present in every retail copy. The settings interface retains its existing five translations. **This support is not included in v0.1.** See [USA, Europe support and validation](docs/notes/europe-support.md).

v0.2 automatically selects an already imported disc when the original game requests it, with no disc-selection button required. With all four discs imported, players do not need to swap discs manually. Storage tests and controlled original-manager 1 → 2 → 3 → 4 → 1 sequences passed for both audited editions. Chapter-boundary gameplay remains unverified. This feature is not included in v0.1; see [disc-selection evidence](docs/notes/disc-selection.md).

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

| Action | Keyboard |
| :--- | :--- |
| Start / Back | Enter / Backspace |
| A / B / X / Y | Z / X / A / S |
| D-pad / left stick | Arrow keys / I, J, K, L |
| Left / right shoulder | Q / W |
| Left / right trigger (v0.2.1) | E / R |
| Debug menu | F1 |

**v0.2.1 input update:** all SDL-mapped controllers and the keyboard can be used without selecting an active device. Inputs merge into player 1; this is not multiplayer support. Hotplug and mixed input passed SDL virtual-device tests; physical models and gameplay switching still need validation. Unmapped joysticks need an SDL controller mapping. See [input evidence](docs/notes/controller-input.md).

Rumble is disabled by default; `LO_CONTROLLER_RUMBLE=1` enables it. For Ring actions, published v0.2 uses the controller's right trigger; v0.2.1 also maps R to RT. A shoulder binding is not a trigger binding.

### Development

| Directory | Contents |
| :--- | :--- |
| `LostOdysseyRecomp/` | Host kernel, graphics, audio, input and debugging |
| `LostOdysseyRecompLib/` | Configuration; ignored `private/` game data and generated `ppc/` code |
| `tools/` | Recompilers, dependency patches and Ghidra scripts |
| `thirdparty/` | Rendering, audio and other dependencies |
| `docs/` | Current status, guides, research and historical archives |

[Roadmap](docs/ROADMAP.md) · [Handoff](docs/notes/handoff.md) · [Rendering tests](docs/notes/rendering-validation.md) · [Audio](docs/notes/audio-output.md) · [Archive](docs/archive/README.md)

</details>

## Credits and game data

With research and tools from [UnleashedRecomp](https://github.com/hedge-dev/UnleashedRecomp), [re:Blue](https://github.com/zolaware/reblue), [XenonRecomp](https://github.com/hedge-dev/XenonRecomp), [XenosRecomp](https://github.com/hedge-dev/XenosRecomp), [plume](https://github.com/renderbag/plume) and [Xenia](https://github.com/xenia-project/xenia). Audio uses the pinned [Xenia FFmpeg fork](https://github.com/xenia-project/FFmpeg), with its [license](thirdparty/ffmpeg-LICENSE.txt).

Lost Odyssey and its assets belong to their respective owners. This is an unofficial project. Supply data extracted from your own discs; do not submit game executables, resource archives, textures, audio, video, generated game code or captures. Dependencies retain their respective licenses.
