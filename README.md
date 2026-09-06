<div align="center">

# Lost Odyssey Recompiled

**An experimental native PC port of Lost Odyssey for Xbox 360.**

PowerPC static recompilation · Xenos shaders · Windows / D3D12

[简体中文](README.zh-CN.md) · [Status](docs/STATUS.md) · [Build guide](docs/BUILDING.md) · [Documentation](docs/README.md)

</div>

---

> **Early development.** Selected opening battles and early exploration routes run. This is not a complete or fully compatible port: rendering, audio and progression issues remain. No game assets are included.

## About

LostOdysseyRecomp translates PowerPC game code into C++ with **XenonRecomp**, implements Xbox 360 services on the host, and translates Xenos shaders for **plume**. The tested platform is **Windows / Direct3D 12**. Linux and Vulkan remain development targets.

Reliable gameplay and faithful rendering come first. The local [settings menu](docs/notes/settings-menu.md) adds English, Japanese, Korean, Traditional and Simplified Chinese UI, game language, FXAA and output-resolution scaling; DLSS and frame generation are disabled placeholders. Higher internal rendering resolutions, unlocked frame rates, HDR and upscaling remain future work. Fullscreen modes still need desktop acceptance testing.

## Game edition and languages

Development uses the **Asian multilingual release** supplied by the project owner. The local Disc 1 XEX has title ID `4D5307FA`, media ID `39F7D748`, title/base version `0.0.0.4` and region mask `0x00FFF900`. This mask is not an Asia-only retail identifier; match the executable details rather than relying on a region label alone.

Current runtime testing uses **English**. The source edition is multilingual, but that does not mean every language is implemented or verified in this port. Other regional executables and title updates are not validated. See [edition evidence](docs/notes/xex.md).

## Current progress

_Reviewed September 5, 2026._

| Area | Evidence and limits |
| :--- | :--- |
| Title and input | Animated background, menus, SDL controllers and keyboard input work in tested scenes. |
| Gameplay | Opening battles and selected encounters run; independent testing reached Gorge camp. No complete playthrough. |
| Graphics | Geometry, material and post-battle whiteout fixes exist. Shadows, fire-hit effects, Ring outer ring and broken-crate effects remain problematic. |
| Audio | XMA decoding, stereo PCM output and a loop-boundary correction are implemented. Background audio and dialogue can still disappear. |
| Saves and debug | Manual saving is confirmed in the development build. F1 supports battle victory, coordinate bookmarks, same-map POI teleport and current map ID/name. |
| Stability | Persistent logs and GPU stall diagnostics exist. The reported camp hang is not conclusively fixed. |

Some encounter and storage changes remain **local and uncommitted**. These results describe the development workspace, not a clean-checkout guarantee. See the [status ledger](docs/STATUS.md).

## Build and run

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
| Debug menu | F1 |

Rumble is disabled by default; `LO_CONTROLLER_RUMBLE=1` enables it. Use a controller's right trigger for Ring actions; a shoulder binding is not a trigger binding.

## Development

| Directory | Contents |
| :--- | :--- |
| `LostOdysseyRecomp/` | Host kernel, graphics, audio, input and debugging |
| `LostOdysseyRecompLib/` | Configuration; ignored `private/` game data and generated `ppc/` code |
| `tools/` | Recompilers, dependency patches and Ghidra scripts |
| `thirdparty/` | Rendering, audio and other dependencies |
| `docs/` | Current status, guides, research and historical archives |

[Roadmap](docs/ROADMAP.md) · [Handoff](docs/notes/handoff.md) · [Rendering tests](docs/notes/rendering-validation.md) · [Audio](docs/notes/audio-output.md) · [Archive](docs/archive/README.md)

## Credits and game data

With research and tools from [UnleashedRecomp](https://github.com/hedge-dev/UnleashedRecomp), [re:Blue](https://github.com/zolaware/reblue), [XenonRecomp](https://github.com/hedge-dev/XenonRecomp), [XenosRecomp](https://github.com/hedge-dev/XenosRecomp), [plume](https://github.com/renderbag/plume) and [Xenia](https://github.com/xenia-project/xenia). Audio uses the pinned [Xenia FFmpeg fork](https://github.com/xenia-project/FFmpeg), with its [license](thirdparty/ffmpeg-LICENSE.txt).

Lost Odyssey and its assets belong to their respective owners. This is an unofficial project. Supply data extracted from your own discs; do not submit game executables, resource archives, textures, audio, video, generated game code or captures. Dependencies retain their respective licenses.
