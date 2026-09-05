# Project status

Reviewed **2026-09-05** against code, runtime evidence and user feedback. This is the current ledger; dated investigation notes describe individual experiments.

## Published versus local

Code checkpoint: runtime `2a4f47d`, debug/encounter `42c543f`, graphics `c4fc9e4`. These commits include the later repairs and tests. Submission does not change the validation boundaries below.

| Scope | Repository status | Validation |
|---|---|---|
| Title, geometry/material and post-battle whiteout fixes; basic debug victory/storage | Published through `d944148` and earlier | Selected opening scenes |
| Persistent logs and GPU stall stages | Published `81f955d` | Seven-second test-thread suspension detected; resumed at 30 fps |
| XMA decoding, stereo PCM and loop handling | Published `21d6523` | Build, loop tests and runtime samples; sound gaps remain |
| Encounter recovery, printf/switch repair, storage refinements | Committed `2a4f47d` / `42c543f` | Selected natural victories and save/reload checks |
| Coordinate and POI teleport | Published with this feature | Hypocenter backend gates/POIs; Gorge window controls, movement and return |
| Map ID/name menu | Published with this feature | Title unknown state; map 2 Hypocenter, map 4 Gorge, live transition from 2 to 3 Edge of Wasteland |

Local results do not guarantee identical behavior from a clean checkout. See [dependency patches](../tools/patches/README.md).

## Active issues

Current implementation and evidence are summarized in the [2026-09-05 work report](WORK_REPORT_2026-09-05.md). The latest local executable is **10F4D144…**; its shadow-clear change has passed build and mapping tests, but has **not yet been run in game**.

| # | User report / request | Current result and remaining work |
|---|---|---|
| 1 | Kaim/enemy self-shadow flicker | Open. Polygon offset and explicit zero-LOD sampling are implemented; no complete visual validation. |
| 2 | Fire-hit black/red checker flicker | Open. Console reference and captured lighting inputs retained; Xenia also glitches. |
| 3 | Encounter shadows absent/flickering | Local clear bug identified: partial clears wiped an entire aliased shadow target. Tile/MSAA coverage mapping implemented and tested; in-game atlas and visual regression pending. |
| 4 | Ring outer ring missing | Verified in selected encounters: visible changing outer ring, RT release, Good and 101 damage; repeated after natural victory. See [Ring evidence](notes/battle-ring-resource.md). |
| 5 | Broken crates show black effects | Open; no verified fix. |
| 6 | Current map ID/name | Published, verified in opening areas. See [map info](notes/debug-map-info.md). |
| 7 | Optional save-anywhere | Local backend and save/restart/load verified, including native save-point behavior and camp permissions. Desktop checkbox interaction/layout pending. See [save-anywhere](notes/save-anywhere.md). |
| 8 | Camp/window hangs | Big-endian critical-section deadlock fixed and tested. Separate intermittent GPU query/wait pointer corruption remains unresolved. A successful route is not a complete stability result. |
| 9 | Sound output and missing voices | Initial output published. Local I/O locking, XMA command/cursor and packet-skip fixes tested. Camp → vehicle CG → city gate/control passed without the former stable decoder errors. Full dialogue audibility and long-run stability remain unverified. |

Manual save success was confirmed by the user. These are scoped results, not full-game completion. See [audio](notes/audio-output.md), [critical sections](notes/critical-section-endian.md) and [query failures](notes/third-map-hang.md).

## Outside validated support

Full-game compatibility, WMV decoding, four-disc integration, native Linux/Vulkan gameplay, unlocked frame rates, HDR and upscaling. Rumble is disabled by default; GPU occlusion results remain approximations.

## Maintenance

[Roadmap](ROADMAP.md) · [Handoff](notes/handoff.md) · [Research index](notes/README.md) · [Archive](archive/README.md)

Private data and `out/` captures are not distributed. Update this ledger and both READMEs when support changes. Record implementation, test scenario and limitations separately.
