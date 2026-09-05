# Project status

Reviewed **2026-09-05** against code, runtime evidence and user feedback. This is the current ledger; dated investigation notes describe individual experiments.

## Published versus local

| Scope | Repository status | Validation |
|---|---|---|
| Title, geometry/material and post-battle whiteout fixes; basic debug victory/storage | Published through `d944148` and earlier | Selected opening scenes |
| Persistent logs and GPU stall stages | Published `81f955d` | Seven-second test-thread suspension detected; resumed at 30 fps |
| XMA decoding, stereo PCM and loop handling | Published `21d6523` | Build, loop tests and runtime samples; sound gaps remain |
| Encounter recovery, printf/switch repair, storage refinements | Local uncommitted code | Selected natural victories and save/reload checks |
| Coordinate and POI teleport | Published with this feature | Hypocenter backend gates/POIs; Gorge window controls, movement and return |
| Map ID/name menu | Investigation only | Numeric IDs found; full feature pending |

Local results do not guarantee identical behavior from a clean checkout. See [dependency patches](../tools/patches/README.md).

## Active issues

| # | User report / request | Status and next validation |
|---|---|---|
| 1 | Kaim/enemy self-shadow flicker | Open. Missing polygon offset is a lead, not a verified fix. |
| 2 | Fire-hit black/red checker flicker | Open. Xenia also glitches; use the user's console reference. |
| 3 | Encounter shadows absent/flickering | Open. Separate from animation recovery. |
| 4 | Ring outer ring missing | Open. Resource/switch crash repairs do not prove ring rendering; test held/released RT. |
| 5 | Broken crates show black effects | Open. Capture destruction and render passes. |
| 6 | Current map ID/name in debug menu | Pending. Match native records and localized labels. |
| 7 | Optional save-anywhere | Pending. CheckSavePoint activates interaction; it is not a pure permission check. |
| 8 | Window hangs on reaching Gorge camp | Unresolved. Independent route reached camp and restored control; latest user run had not hung. Neither proves a fix. |
| 9 | Sound output; background audio/dialogue disappear | Partial. Output and loop-boundary repair exist; remaining voices and loop subframes need validation. |

Manual save success was confirmed by the user. A separate process loaded a copied **Gorge** save and reached camp naturally. No camp save/reload or full playthrough is claimed. See [camp investigation](notes/third-map-hang.md).

## Outside validated support

Full-game compatibility, WMV decoding, four-disc integration, native Linux/Vulkan gameplay, unlocked frame rates, HDR and upscaling. Rumble is disabled by default; GPU occlusion results remain approximations.

## Maintenance

[Roadmap](ROADMAP.md) · [Handoff](notes/handoff.md) · [Research index](notes/README.md) · [Archive](archive/README.md)

Private data and `out/` captures are not distributed. Update this ledger and both READMEs when support changes. Record implementation, test scenario and limitations separately.
