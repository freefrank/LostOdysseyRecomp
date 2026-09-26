# Issue #40 UI Resource Map and Asset Archive

This document archives game UI packages, texture atlases, fonts, and configuration offsets located during the investigation of Issue #40 (gamepad button prompt / PlayStation glyph mapping).

Detailed documentation in Simplified Chinese is maintained in [issue-40-ui-resource-map.zh-CN.md](issue-40-ui-resource-map.zh-CN.md).

## Implementation & Acceptance Status (Updated 2026-09-26)

- **Host UI (Recomp)**: Settings menu, Installer UI, and Debug overlay render PlayStation glyphs (Cross, Circle, Square, Triangle, L1, R1, L2, R2, and Options/Share), passing offline automated tests.
- **Guest Runtime**: Automatic PlayStation controller prompt replacement is implemented and verified in the guest game runtime. This corrects previous notes stating only common was audited and guest replacement was unimplemented.
  - Controller detection: SDL tracks the most recently active gamepad.
  - GPU upload intercept: Content-hash matching identifies two confirmed controller atlases:
    1. `rpmenurescommon_int.xxx` `Icon_Page_0` (hash prefix `8e181...`, BC3, 256x128).
    2. English font `Texture2D_1` (SHA-256: `cfd30b830a2fbf5d12520ea5c6cc2a6a3a37d534f8f4c7f846fdc50520ba8229`, BC3, 256x128).
  - Both atlases are confirmed via authentic BC3 matching and a production upload fixture. Guest rendering swaps between original Xbox 360 textures and immutable PlayStation replacement textures under GPU stop guards and retirement review.
  - Replaces action buttons (Cross/Circle/Square/Triangle), shoulder buttons (L1/R1/L2/R2), and pause menu Start/Select (Options/Create), with fixes for previously omitted pause menu Start/Select and shoulder prompts.
- **User Acceptance**:
  - Development build: `HEAD d677a48-dirty`, binary SHA-256 `1ae34ffd7eedcc415816c30b6e74b31d5ca2c0d936cd6aadbf72b49aeca709a4`.
  - The user reviewed pause menu and cutscene behavior on real hardware and explicitly accepted the implementation ("验收通过"); uncommitted and unpublished.
- **Validation Scope & Limits**:
  - Reused existing host, HID, glyph, and GPU test suites and builds without rerunning.
  - Acceptance is bounded to real-device testing on the user's setup and verified scenes; it does not claim universal controller hardware compatibility or 100% full-game playthrough coverage.
  - **Issue #40 Scope**: Issue #40 also requested general Mod support, which was not implemented in this scope. Issue #40 is not claimed as fully completed.

## Package Byte Offsets

All positions are documented as exact byte offsets (not sector numbers). Source paths are relative to `LostOdysseyRecompLib/private/disc1/`.

| Container | Package Path | Byte Offset | Length | Notes |
| :--- | :--- | :--- | :--- | :--- |
| `xenon_loc.fpd` | `rpmenurescommon_int.xxx` | `25069568` | `892105` | Common UI menu assets |
| `xenon_loc.fpd` | `rpfontscommon_int.xxx` | `23947264` | `616018` | Font package (decompressed size: `871782`) |
| `xenon_loc.fpd` | `rpmenuresbattle_int.xxx` | `24854528` | `213267` | Battle UI menu assets |
| `xenon_loc.fpd` | `rpmenuresfield_int.xxx` | `25962496` | `168576` | Field UI menu assets |
| `xenon_sys.fpd` | `bin/xenon/sys/enginefonts.xxx` | `15126528` | `104030` | System engine fonts |
| `lo.fpd` | `rpgame/config/xenon/cooked/coalesced.ini` | `0` | `209499` | RPInput bindings (`XboxTypeS_A=XPad_A`, reported record) |

*Note: `coalesced.ini` input bindings reflect prior audit reports; no separate extracted copy is committed.*

## Batch Export Metrics (`ui-export`)

The batch asset extraction pipeline documented in `tools/ui_assets/` processed all four game discs:
- **Scope**: 4 discs screened, 560 package entries selected (`xenon_sys.fpd`: 72, `xenon_vfx.fpd`: 8, `xenon_loc.fpd`: 480).
- **Extracted Images**: 931 standalone PNGs exported (including 351 font pages).
- **Contact Sheets**: 82 visual contact sheets generated, indexed in `out/issue40/ui-export/sheets/INDEX.txt`.
- **Failures & Success Rate**: 36 package failures across 9 distinct package paths with unsupported layouts (e.g. `battlemenu.xxx`, `rpgameover.xxx`, `rpnavi.xxx`). For all parsed packages, object decode failures were 0 (`failed_objects: 0`).

## Common Package Overview (`rpmenurescommon`)

Contains 32 exports (indices 0 to 31):
- `BACKYARD` (512x128, BC3 Format 7): Backyard battle interface assets.
- `Dot` (4x4, A8R8G8B8 Format 2): Base fill texture.
- `FIELD_NAVI_ICON` (128x64, BC3): Navigation compass icons.
- `FIELD_NAVI_SKIN` (256x256, BC3): Navigation compass skin.
- `Icon_Page_0` (256x128, BC3): **Confirmed controller icon atlas 1** (LB/RB/LT/RT/LS/RS and bottom row ABXY; active in guest PS replacement).
- `LO_UI_PASS` (512x128, BC3): Passcode and digit graphics.
- `Result` (512x1024, BC3): Battle result screen frame.
- `TUTORIAL` (32x32, BC3): Tutorial hint icons.
- `UI_FONT` (128x64, BC3): Latin font glyphs.
- `UI_MAIN_00` (512x1024, BC3): UI background elements and decorative emblems.
- `UI_MAIN_01` (512x512, BC3): Background tint and shading texture.
- `UI_MAIN_02` (512x64, BC3): Progress bar and meter elements.
- `window` (256x1024, BC3): Generic dialog and window frames.
- `WM_NEW_CURSOR` (128x64, BC3): Cursor indicators for world map and menus.
- Non-texture exports: Materials (`MatBrightness`, `MatColorReverse`), expressions, redirectors, and `SeekFreeShaderCache`.

## Button Prompt Evidence

All referenced image slices and contact sheets are stored locally under `out/issue40/` and are not committed to source control.

- **Confirmed (Controller Atlas 1)**: `rpmenurescommon_int.xxx` export index 21, `Icon_Page_0` (256x128, BC3, manifest ID `#00695`, hash prefix `8e181...`). Displays shoulder buttons/triggers (LB, RB, LT, RT), analog sticks (LS, RS), and action buttons A (green), B (red), X (blue), Y (yellow). Active in guest PS replacement.
- **Confirmed (Controller Atlas 2)**: `rpfontscommon_int.xxx` export `Texture2D_1` (256x128, BC3, SHA-256 `cfd30b830a2fbf5d12520ea5c6cc2a6a3a37d534f8f4c7f846fdc50520ba8229`). Active in guest PS replacement for font and pause menu streams.
- **Confirmed (HUD & Pause Menu Prompts)**: Historical capture `shot_12213.ppm` (camp Open prompt) and pause menus. Latest build replaces action buttons, shoulder buttons, and pause menu Start/Select (Options/Create) with verified PlayStation glyphs.
- **Excluded**: `UI_MAIN_00` at `y=705, x=193..351` (`Btn_Row1_1` through `Btn_Row1_5`) are 5 colored emblems, not controller buttons.
- **Candidate (Unconfirmed)**: `UI_MAIN_00` at `y=737, x=257..323` (`Btn_Row2_4` through `Btn_Row2_7`) are 4 white circular rings (`20x20`). They lack letter markings and remain candidates.

## Fonts Scope (`rpfontscommon_int`)

- `Maru23`: 1 page (`512x512`), 199 glyphs (`rpfontscommon_Maru23_p0.ppm` / `Maru23_Page0.png`).
- Other exported pages: `Abc` (2 pages), `Arial18` (3 pages), `BigNum` (4 pages), `LocTit1` (3 pages), `LocTit2` (1 page), `Meiyo18` (1 page), `Meiyo26` (4 pages).
- `Texture2D_1`: Verified controller prompt atlas 2, actively replaced at runtime.
- These records are strictly limited to verified exported pages; no claim is made that all game fonts have been audited or ruled out.

## Maintainable Tooling (`tools/ui_assets/`)

A maintainable tool suite exists under `tools/ui_assets/`:
- `tools/ui_assets/decode.cpp`: Decoder supporting tiled BC3, BC1, and A8R8G8B8 format 2 with CPX/LZO unpacking.
- `tools/ui_assets/export.py`: Multi-disc archive scanning, package filtering, and contact sheet generation.
- `tools/ui_assets/CMakeLists.txt`: Build configuration for `LoUiAssetDecoder`.
- `tools/ui_assets/README.md`: Reproduction commands and format documentation.

Production code in `LostOdysseyRecomp/settings/menu_assets.cpp` remains untouched (line 160 continues to enforce `width >= 128 && height >= 128`).

### Windows Reproduction Commands

```bat
call tools\setup_windows.bat
cmake -S tools\ui_assets -B out\issue40\ui-export\build -G Ninja -DCMAKE_CXX_COMPILER=clang-cl
cmake --build out\issue40\ui-export\build --target LoUiAssetDecoder
python -B tools\ui_assets\export.py --private LostOdysseyRecompLib\private --output out\issue40\ui-export
```
