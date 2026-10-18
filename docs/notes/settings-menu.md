# Settings menu and display options

[简体中文](settings-menu.zh-CN.md)

> **Topic reference.** The implementation and measurements below apply only to the version and scope noted for each item. For current delivery and acceptance status, see [project status](../STATUS.md) and [roadmap](../ROADMAP.md).

## 2026-10-07 current settings menu and corrections (merged to main, not yet released)

The latest release is still [v0.8.44](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.8.44), which does not include the items in this section. The PRs below were merged on 2026-10-07 (read with `gh pr view`): [#269](https://github.com/freefrank/LostOdysseyRecomp/pull/269) (03:01:14Z, `35e22a28`), [#270](https://github.com/freefrank/LostOdysseyRecomp/pull/270) (03:17:30Z, `3182adab`), [#271](https://github.com/freefrank/LostOdysseyRecomp/pull/271) (03:35:13Z, `a5fa8961`), [#274](https://github.com/freefrank/LostOdysseyRecomp/pull/274) (03:42:11Z, `475d8c65`), [#276](https://github.com/freefrank/LostOdysseyRecomp/pull/276) (07:08:08Z, `6ed90748`). Issues #104, #198, #174, #202 and #201 are closed. Closure does not mean the reporter accepted the change.

**Display mode.** Only "Windowed" and "Fullscreen" (borderless). Exclusive fullscreen was removed in #274: the menu and the Windows first-run setup dialog no longer offer it, the SDL exclusive path (Vulkan) and the DXGI `ResizeTarget`/`SetFullscreenState` code have been deleted, and a saved `window_mode=2` now loads as fullscreen. Mode changes still force a swap chain rebuild through a window size request. From #276 on, "Borderless fullscreen" is renamed "Fullscreen" in all interface languages.

**Two new rows on the Graphics page.**
- GPU (`gpu_device`, saved by adapter name, takes effect after restart): lists "Auto" and the adapters reported by the current backend. If the name does not exist, the adapter cannot be opened, or a required feature is missing, it falls back to Auto and writes a warning. Hidden when there is only one adapter or on Android (#202).
- Display (`display_name` + `display_index`; the index is used only to tell apart displays with the same name): the label includes the index, model, size and desktop position, for example `2: M27P20 · 3840×2160 (3840, 0)`. After saving, the window moves to that display at once (it leaves fullscreen, centres, then returns to the saved mode), and fullscreen also uses it. "Auto" does not move the window. If the saved display no longer exists, it falls back to Auto. Hidden when there is only one display or on Android. After switching displays, a "Keep this display?" prompt appears. If the user keeps it, or does not respond within 5 seconds, or chooses to revert, the old choice is written back to `settings.ini` and the window moves back to the original display. Changing only the mode or other settings does not ask. Display changes caused by Win+Shift+arrow keys or by dragging the window are followed in both windowed and fullscreen modes, the saved display choice is updated to match, and "Auto" stays Auto (#201).

**Save row.** When focus is on the "Save" row, Start/Enter now saves. On other rows it still only moves focus to "Save". Before this, Enter only moved focus, so the display choice was never written (#276).

**Two new rows on the Sound page.**
- Vibration (`vibration`, 0–100%, default 100, takes effect immediately, with a short preview pulse while adjusting; the minimum is off. `LO_CONTROLLER_RUMBLE=0` still turns it off entirely) (#198, #270).
- Audio output (`audio_output`: Stereo / 5.1 Surround, default Stereo, switches immediately): 5.1 sends the game's own six-channel mix directly. If the device has fewer than 6 channels, it falls back to stereo downmix and shows a note in the help line. 7.1 devices are mapped by SDL. On Windows, the system speakers must first be set to 5.1 or 7.1 (#174, #271).

**Voice volume (#394).** A row below "Sound effect volume" on the Sound page (`voice_volume`, 0–100%, default 100, step 4; also shown on the title screen). In the original game, cutscene dialogue is the language audio track of a cutscene stream: the channel that plays `snd\stream\be0200.xwv` (the path is at channel +127) has a second voice at +124, which `sub_82B6D7E0` opens from the language folder by the same file name (`snd\int\stream\be0200.xwv`). Like the stream, it follows the sound effect volume. `patches/voice_volume.cpp` hooks the stream volume update `sub_82B6C650(channel, voice)`. When the update is for this language track, the category gain returned by `sub_82B71E68` becomes master volume × voice volume. Voices in sound packs (.xse), such as battle shouts and some short cutscene lines, share one pack with sound effects and still follow the sound effect volume. After a change, the value is applied through `sub_82870E38`, the same call as the original volume slider. Streams that start later use the new value. Whether a playing stream updates at once was not measured. Measured on psvita in the opening battle be_0200 (voice 100 vs 0): in seconds with dialogue, the voice band is 0.4–6.6 dB lower. In seconds without dialogue, the difference is within ±0.2 dB.

**F1 debug menu.** The fast-forward switch, mode (hold / toggle) and rate (2–8×) on the Cheats page are written to `fast_forward`, `fast_forward_mode` and `fast_forward_rate` in `settings.ini` and restored at the next launch. Defaults: off / hold / 2× (#104, #269). #104 originally asked for faster game menu animations. The maintainer replied that this is the original experience and no separate switch will be added, and suggested using the debug menu's fast-forward. What was delivered is saving the fast-forward settings.

**Scope of checks and gaps** (from the PR bodies and maintainer notices; not re-run):
- Display switching: the maintainer tested it on a Windows PC with three displays and accepted it on 2026-10-07 (from the maintainer's notice; the PR #276 body says retesting was still in progress, and the Issue has no comment record). Mixed DPI, native Linux and Android were not tested.
- GPU selection: on psvita (Proton Vulkan), auto selects RADV. Naming `llvmpipe` gives llvmpipe. A missing name writes a warning and returns to RADV. On this machine D3D12 lists only one adapter, so the row is hidden. On M1 Max (Metal), selection is by name, and unknown names fall back. Optimus / multi-adapter Direct3D 12 was not tested.
- 5.1: on psvita (Proton/WASAPI, recording from an empty output), each of the six channels has sound on the output. Dialogue is in the centre, and 98% of LFE is below 150 Hz. The result folded back to stereo is within 0.1 dB of a stereo recording. Two-channel devices fall back, and 7.1 devices go through the reopen path. Real 5.1 speakers and native Linux / macOS / Android audio were not tested. In #174, a user said they have a 5.1 device that can be tested (2026-10-05).
- Vibration: `LoHidTest` with a virtual SDL controller. The feel of the strength levels was not confirmed with a physical controller (such as a DualSense).
- Fast-forward settings: on psvita (Proton D3D12), with on / toggle / 4× set and then restarted, the log shows `debug menu: fast-forward on (toggle, 4x)`. The exit path from inside the game was not tested.

**Corrections to older statements below** (the old text is kept as the 2026-09-05 record):
- "Window / borderless fullscreen / exclusive fullscreen", "DXGI exclusive state ... switching", "borderless and exclusive fullscreen are implemented ...": exclusive fullscreen no longer exists. The current modes are windowed and fullscreen. The limit at that time, "no fullscreen acceptance on a visible desktop yet", remains a record of that point. For display switching acceptance, see above.
- "After applying display settings there is a 15-second confirmation period": the whole-page confirmation no longer exists (`78730ffb`, 0.4.23; settings are saved once, and `menu_flow_test.cpp` asserts that one click completes the save with no Keep changes). The 15-second string left in `translations.h` is not used by the menu. The only timed confirmation now is the 5-second display keep/revert prompt above.

## 2026-09-19 Issue #54 language selection safety fix (pending release at the time)

The language menu now accepts only counts and indexes within the native language table range. When the count is zero or above capacity, or the index is out of range, it does not read the corresponding language table and does not rewrite the current selection. Invalid menu items show as `—`, so a clamped value is not used to fake a language. Menu capacity follows the native parser's 16-item limit. The existing `82481BE8` USA/Europe host override semantics are unchanged. Text and voice selections are still handled separately.

A default-off, bounded `LO_TRACE_LANGUAGE=1` trace is added. It can record the raw lookup arguments, the host/original return pointers, the configured count and index at menu open/apply/default/close, the mapped ID, and the valid cached UTF-16 and returned IDs of `82481E78`/`82481F40`. The close snapshot is taken after apply and before close. The trace is for later problem investigation. The root cause of the specific cutscene voice problem is not confirmed. There is no save, and no live reproduction was done in this round. The fix in this section was pending release when it was written. #54/#55 were closed later, but the closure record does not supply the live evidence above.

## 2026-09-14 Europe/US subtitle mapping included in v0.5.12

The USA/Europe FMV / event subtitle language mapping fix in the `82481BE8` hook is included in the released [v0.5.12](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.12). The local check of the Spanish opening FMV with `game_language=5` and Vulkan at 3840×2160 passed and was accepted. On 2026-09-15 the original reporter confirmed the Spanish opening FMV subtitles in v0.5.12 (Vulkan and Direct3D 12). German, French and Italian FMV coverage was also confirmed. Issue [#27](https://github.com/freefrank/LostOdysseyRecomp/issues/27) is closed. Full event coverage and whole-game validation are still not verified. The release check did not re-verify the 50-file manifest.

2026-09-05, implementation and validation record. Following [reblue](https://github.com/zolaware/reblue), the new menu is reconnected to the original entry, and settings data is separated from drawing. The Blue Dragon game addresses or menu implementation were not copied.

## Features

On Windows / D3D12, four categories open from the game's original Settings entry: Game, Sound, Graphics and Language. The F1 debug menu keeps its own separate entry.

- Keeps text speed, subtitles, voice language, battle cursor memory, auto-input for the back row, camera up/down and left/right inversion, the confirm button, music/sound effect volume, and the original brightness description. Adds restore of the original game defaults.
- The settings interface and game text are both available in English, Japanese, Korean, Traditional Chinese and Simplified Chinese. Interface language previews immediately. Game language takes effect after a restart when saved, and the save success message says a restart is needed. The old unverified German / French / Spanish / Italian options have been removed.
- Graphics settings include an FXAA switch, output resolution, and window / borderless fullscreen / exclusive fullscreen. Output resolution is the final display scaling. **It does not raise the in-game internal rendering resolution.** Borderless uses the desktop size. (Correction 2026-10-07: exclusive fullscreen has been removed; see the top of this page.)
- DLSS and frame generation are disabled placeholders. They do not load the SDK and do not produce upscaling or frame insertion.
- After applying display settings there is a 15-second confirmation period. If the user does not confirm or presses cancel, the settings revert. If a display switch fails, the old settings are restored. After confirmation, `settings.ini` in the working directory is written atomically. (Correction 2026-10-07: the whole-page confirmation period was removed long ago; see the top of this page.)
- The original game options change the original game configuration object directly, reusing its apply function and the original save confirmation. New game setup still follows the original flow. The user's save/profile is not separately overwritten.

The categories sit in a horizontal row at the top. The selected item has a highlight and a gold underline. On a gamepad, LB/RB switch categories, the D-pad selects and adjusts, confirm selects, and cancel goes back. Keyboard keeps the existing mappings: Q/W, arrow keys, and Z/X. The mouse can click categories and option values, and right-click adjusts in reverse. The original confirm button setting also applies to the new menu.

## Integration points

- `settings/config.*`: host config reading, preview and atomic save. The game language is fixed for the current process after launch.
- `settings/menu.*`: `822F19B0` hook for updating the original settings task. Config object `8326A068`, with the data pointer taken from object storage. `82870E38` applies volume/voice and others, and `828710A0` restores defaults.
- The original menu has 13 row positions internally, and the screenshot actually shows 11 items. Internal ID 11 is a hidden graphics position; ID 12 is the visible brightness description. The original opening flow for ID 12 is reused.
- The new interface is drawn off-screen by the host, the texture is uploaded, and it is presented through the GPU. It does not use a separate settings window that covers the desktop. The settings page skips FXAA.
- `gpu/presentation.*`: full-frame scaling, keeping aspect ratio with black bars, and FXAA. The original resolve texture used after presentation is restored to COPY_SOURCE, matching the game renderer state.
- `video.cpp`: SDL operations stay on the window message thread. DXGI exclusive state is switched on the presentation thread, which then runs ResizeBuffers. `LO_BACKGROUND` forces window mode, so tests do not change the desktop display mode. (Correction 2026-10-07: DXGI exclusive switching was removed with exclusive fullscreen; see the top of this page.)
- `LO_SCREENSHOT_PRESENTED=1`: reads the final swap chain image to verify the actual scaled output. The default screenshot behaviour still reads the original game frame.

## Validated and boundaries

- `LoPresentationTest`: the original resolution gives identical pixels. Non-matching aspect output has correct black bars. A diagonal edge with FXAA produces 32 intermediate brightness pixels, and flat areas stay unchanged.
- `out/settings-preview-run2`: original entry, switching among the four pages, the first Chinese interface (later changed to Traditional at the user's request), and the original exit save confirmation. Read back from the final swap chain.
- `out/settings-apply-run2`: 1920×1080 preview, timeout after 15 seconds back to 1280×720; confirmed save at 1600×900. The save confirmation choice No returns to the new page, and choosing Yes continues the new game. Fixed the problem where the confirm button, still held when returning to the menu, was processed twice.
- `out/settings-language-tw`: restarted at 1600×900 with game language 8 through `settings.ini`. The original main menu showed "新遊戲 / 繼續 / 千年之夢" in Traditional Chinese, and the original brightness description showed in Traditional Chinese. Returning from the brightness page to the new Graphics page works.
- `out/settings-language-tw2/graphics.png`: actual swap chain screenshot of the final Traditional settings page. Opening and returning from the original brightness page passed again.
- The final test build `out/settings-preview.exe` is `0DD904CA`. Compilation and LoPresentationTest passed. The full build's main program is saved separately as `out/settings-full-build.exe`, and the main directory was restored from the verified `out/voice-fixed.exe/.pdb` to `135DCA79`. No visible game was started, and tests used separate directories.
- Borderless and exclusive fullscreen are implemented, but background tests forbid desktop mode changes, so **there is no fullscreen acceptance on a visible desktop yet**. Different displays, DPI, switching away and focus loss need later hands-on checks. (Correction 2026-10-07: exclusive fullscreen no longer exists. For later display switching acceptance, see the top of this page.)
- This is not a statement of full-game language, audio or rendering compatibility. The Japanese-only multi-level subtitle mode and different resource versions still need coverage.


## 2026-09-05 local language resource check

Directly inspected four `xenon_loc.fpd` files, each 87,785,472 bytes, all with SHA256 `87d88853eee59a496066988c60c39e7e20023171e6691c09210ad3d6666be69a`. In UTF-16BE text surrounded by the same scene control codes, at least five actual translations were confirmed. They are not settings enums or a font character table:

| Text language | Offset in file | Same battle hint sample |
|---|---|---|
| Traditional Chinese | 0x1830 | 魔法岩的護盾效果解除了。 |
| English | 0x1662866 | Magic Rock's barrier effect deactivated. |
| Japanese | 0x2077038 | 魔岩石のバリア効果が解除された |
| Korean | 0x32a6838 | 마암석의 바리어 효과가 풀렸다. |
| Simplified Chinese | 0x40EE830 | 魔岩石的魔法盾效果被解除 |

The same council story scene also has matching dialogue in several languages, which supports that these resources really contain translations. The results are saved in `out/language-resource-audit.json`. German, French, Spanish and Italian text was not verified in this round. The claim of seven or eight fully supported languages cannot be made, and it has not been shown that these five are all the languages in the archive. The local resources are only confirmed to exist. Apart from the earlier English / Traditional Chinese, the runtime selection mapping, completeness, fonts and loading for the other languages are not yet accepted. The local resources cannot be used to infer that every original Asian retail disc includes Simplified Chinese, and the source and modification history of this resource is not verified.

The earlier conclusion, drawn from the product description, that local text was limited to English and Traditional Chinese has been withdrawn. Following the user's existing request, no Simplified interface was added and the current game language options were not changed. The running test build was not operated on or restarted.


## Five-language and horizontal category update

The user has now explicitly asked to add Simplified and the other confirmed resource languages, overriding the earlier preference of "no Simplified Chinese interface". The UI's persisted values keep 0=English and 1=Traditional, and add 2=Japanese, 3=Korean, 4=Simplified. Game language uses the original program's `0x832455F0` table mapping: 1=INT, 2=JPN, 7=KOR, 8=CHI, 9=SCH. The desktop system region is not used to guess Simplified or Traditional. An invalid or old unsupported game language value falls back to English.

All settings items, categories, operation hints, and failure / revert / save status are provided in all five languages. The title and categories use the same translation table. The mouse hit area for the horizontal category row is adjusted together with the drawing coordinates. The keyboard/gamepad page switching mapping stays Q/W and LB/RB.


Five-language regression complete: `out/settings-five-languages` verifies host translation and the top horizontal page switching in three rounds, for Japanese, Korean and Simplified. `out/settings-five-brightness` further confirms that the original Japanese and Korean brightness descriptions actually load. Only the Simplified Chinese setting with ID 9 once fell back to English. The cause was the language alias fallback in the retail configuration. The `82481BE8` host hook returns the SCH pointer from the original executable table only when Simplified is selected, the current language registry is queried, and the request is 0/9. For other languages and calls the original function still runs. `out/settings-sch-resolved/simplified/shot_1034.png` confirms that the original Simplified settings and brightness description display correctly.

The new `out/settings-preview.exe/.pdb` includes this fix, and an identical copy is saved as `out/settings-language-preview.exe/.pdb`. The main build still keeps 135DCA79. The old test build is saved as `out/settings-preview-0DD904CA.exe/.pdb`. These are all time-limited isolated copies, all exited, and the user's save/profile was not changed. Nothing committed or pushed. The validation is not a statement of full-game translation completeness.

The user then authorised commit/push. This settings menu source, tests and documentation were synced with that commit. The "not committed" wording above describes only the experimental state at the time.

## 2026-09-14 Europe/US subtitle mapping current status

The current `82481BE8` host hook no longer returns the original executable language table pointer only for Simplified Chinese (SCH). When `r3=0x8336A5F0` and the request is `r4=0` or the current language ID, host `GameLanguage()` IDs 1–9 all take the same return path. The ID order of the guest table `0x832455F0` is 1=INT, 2=JPN, 3=DEU, 4=FRA, 5=SPA, 6=ITA, 7=KOR, 8=CHI, 9=SCH. For IDs missing from the runtime allowlist, the original function falls back to ID 0 (INT). When the caller first queries with `r4=0`, this stops Europe/US event and FMV subtitles from selecting the matching language suffix. This records the current fix. It does not rewrite the scope of the 2026-09-05 SCH historical validation below.

## 2026-09-06 languages by game version (included in v0.2)

The game language options are now chosen automatically from the XEX version of the installed data. The Europe/US version uses 1=English, 2=日本語, 3=Deutsch, 4=Français, 5=Español, 6=Italiano. The Asian version continues to use 1/2/7/8/9. The first launch and the in-game menu share the same list. In old configs, a game language that does not apply to the current version falls back to English. The settings interface translation is still the original five languages, English / Traditional / Japanese / Korean / Simplified. No German / French / Spanish / Italian interface translation was added.

At runtime, the data directory is determined first, then the version is detected, and finally settings are loaded or the first-run setup is opened. If resources are missing, import is completed first, so the first-run setup shows the correct language. Version detection reads XEX metadata. Full SHA256 support checks are run by the importer. The runtime list selection must not be taken as proof that any XEX is compatible.

The Release build and 11 rounds of hidden first-run setup tests passed. They cover the exact option lists of both versions, saving of all language IDs, and fallback for old incompatible configs. The record is `out/eu-import-audit/setup-tests/results.json`. Launches in all six languages passed. The English, German, French, Spanish and Italian main menus were confirmed visually. The Japanese title menu itself uses English, and resource languages were confirmed separately on the original Japanese brightness / settings page. The validation covers only menus and settings, not gameplay. See the [Europe support record](europe-support.md) for details. This update was pushed to `dcc9462` and officially released in v0.2. The v0.1 history is unchanged.

This round also corrected the host voice selector. It previously had three fixed options: English, Japanese and Korean. It now reads the list filled in by the original game resources, and the names and loop count follow the list. The original functions `82482028/82482038` correspond to the `+419` count and the `+288` language ID array in registry `8336A5F0`. The host read is limited to at most 10 items. The original Europe/US settings page shows five voice options: English, Japanese, German, French and Italian. The latest Release build passed. The Europe/US host voice loop was verified with actual screenshots: Français → Italiano → English → 日本語 → Deutsch → Français (`out/eu-import-audit/voice-eu/options.png`). The log matches IDs 1/2/3/4/6. The Asian version regression was also confirmed visually: English → 日本語 → 한국어 → English → 日本語 → 한국어 (`out/eu-import-audit/voice-asia/options.png`), with IDs 1/2/7. This only shows the menu options. It does not prove that full voice playback is correct for each language.

## v0.2 release validation

The official release package went live on 2026-09-06 at 17:16 UTC. The importer self-test in the package, the full manifest check, and a 30-second actual render start under a minimal PATH all passed. The German title menu was confirmed visually. See the [release record](../STATUS.md) for details. The old build names and hashes above are kept as historical evidence. Full language voice coverage and chapter story are still not covered.
