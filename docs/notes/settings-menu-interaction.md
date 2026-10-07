# Settings menu interaction and redraw — 2026-09-24

> **Historical checkpoint.** Version, issue, process and pending-work statements below describe their original date. Recheck current delivery and acceptance in [STATUS](../STATUS.md) and [ROADMAP](../ROADMAP.md).

Source repair, not a published-release or full-game acceptance claim.

## Interaction

The Graphics tab now has one **Anti-aliasing / Upscaling** choice:
Off, FXAA, SMAA, TAA, DLSS, FSR 3.1. Quality is visible for DLSS/FSR;
FSR sharpness is visible only for FSR, immediately after quality. Save is the
last logical and visible row. All 11 graphics rows fit the existing viewport.
The serialized AA, provider and per-provider quality IDs are unchanged.
Selecting a legacy AA mode disables the provider. Selecting a provider retains
legacy AA for the existing unsupported-scene fallback; the frame plan still
chooses one temporal consumer.

In the Settings menu A no longer cycles option values. Left/Right adjusts them;
A still confirms Save, brightness calibration, restore defaults and dialogs.
The existing swapped A/B preference is respected. Mouse clicks on values still
adjust them (right-click reverses; the left arrow in a many-choice selector goes
left). Mouse action buttons still confirm. Start/Enter still focuses Save
without submitting it on the same tick. F1 Cheats confirmation controls are not
changed by this patch.

Update, 2026-10-07 (merged to `main`, not yet in a release; latest release
v0.8.44): Start/Enter on the focused Save row now saves, and on other rows it
still only moves the focus to Save
([PR #276](https://github.com/freefrank/LostOdysseyRecomp/pull/276), merged
2026-10-07T07:08:08Z as `6ed90748`; Enter had only moved the focus, so a display
choice was never written). The Graphics tab has also gained GPU and Display rows
(hidden with one adapter or display), so "all 11 graphics rows" above is the
2026-09-24 count. See [settings menu](settings-menu.md).

## Redraw cost

Inspection found no fixed one-second wait for a fresh input edge. `FilterInput`
queues it immediately; 350 ms / 100 ms intervals apply only to held navigation.
The renderer, however, was filtering the full textured backdrop repeatedly at
output resolution for every changed menu snapshot. This runs on the presentation
path and can visibly stall navigation.

The fix caches one static backdrop per presentation thread. Width, height and
an owning immutable asset pointer form the cache key. Text, values, focus,
scrolling and dialogs are still rendered dynamically at native output resolution.
First use, output-size changes and changed asset sets rebuild the backdrop;
this does not claim to remove first-load asset I/O or GPU presentation delays.

A local Clang Release synthetic-atlas fixture measured a 3840x2160 cold redraw
at 517.924 ms and a warm focus-change median at 16.173 ms. Corresponding warm
medians were 1.835 ms at 1280x720 and 3.962 ms at 1920x1080. These are isolated
CPU raster costs in the development container, not measured player input latency,
Steam Deck timings, or a guaranteed frame rate. Timing is reported, not used as
a machine-dependent pass/fail threshold.

## Verification

```
cmake -S tools/tests/settings_menu -B out/settings-menu -DCMAKE_BUILD_TYPE=Release
cmake --build out/settings-menu --config Release --parallel 2
ctest --test-dir out/settings-menu -C Release --output-on-failure
```

- The entire production `menu.cpp` is compiled with synthetic external service
  boundaries. Local Linux: 304 assertions covering A/no-cycle across tabs,
  left/right changes, provider/quality persistence, conditional rows, sharpness
  bounds, navigation, immediate fresh input, mouse arrows and Save, swapped
  confirmation, Start+A suppression and modal confirmation.
- 21 native renderer checks cover cold/warm pixel equality after focus changes,
  720p/1080p/4K and equal-area portrait resizing, changed assets, missing assets,
  language/values/hidden rows/dialogs and per-thread cache isolation.
- A separate before/after comparison against the unmodified renderer produced
  byte-identical full images at 720p, 1080p and 4K with a synthetic menu atlas.
- The local suites also passed AddressSanitizer/UndefinedBehaviorSanitizer;
  the existing overlay controller regression passed unchanged. English/Chinese
  previews were rendered from the actual published menu snapshots using fallback
  fonts, not installed game assets.
- `.github/workflows/settings-menu-tests.yml` runs the standalone suites on
  Windows and Linux. A passing offline workflow is not a full runtime build.

Still unverified: the player's actual ~1-second delay, installed original-font
render cost, live DLSS/FSR transitions, gamepad hardware and full-game builds.
The existing full-runtime `menu_flow_test.cpp` expectations were updated for
merged selection and row order, but that complete Windows fixture requires its
normal runtime dependencies and is not the standalone suite above.
