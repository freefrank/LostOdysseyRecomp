# Project reconciliation — 2026-10-06

Receipt for the maintainer's v0.9.0 lane decisions of 2026-10-06 and four stale manifest records. Only Project items and the two roadmaps changed; no Issue, pull request, comment, build or game run was touched.

## Decisions applied

- **Character shadows** (`character-shadow-flow-2026-10-01`): the maintainer reported the shadows fixed. The shadow-projection fixes of v0.8.10 to v0.8.30 (PRs #186, #197 and #218) shipped in the meantime; no capture was taken and no separate fix was made. Paused / Deferred / v0.9.0 → Done / Released / v0.8.30. Both roadmaps mark the v0.9.0 bullet `[x]` with the same note.
- **Linux AArch64** (`linux-aarch64-platform`): moved past v1.0.0. No ARM64 package is planned before then; the cross-build path of PR #60 stays in the source. In Progress / In progress / v0.9.0 → Paused / Deferred / After v1.0.0 (the Release field is free text). Both roadmaps record the move on the v0.8.0 item 9 and list the packaging in the later backlog.

## Stale records corrected

The first plan reported four Status conflicts where the Project already held the maintainer's newer value; the manifest was updated to match and the other fields brought in line:

| Key | Was (manifest) | Now (manifest and Project) |
| --- | --- | --- |
| `issue-49-physical-pixel-window-coords` | Todo / Not started | Done / Deferred (closed 2026-10-05; PR #230 in v0.8.30 delay-loads d3d12/dxgi) |
| `issue-30-dof-toggle-slider` | Todo / Not started | Done / Released / v0.8.30 (PR #228) |
| `issue-85-minimap-toggle` | Paused / Deferred | Done / Released / v0.8.30 (PR #229, README) |
| `issue-114-container-enemy-softlock` | Paused | Awaiting validation / Implemented / Next release (PR #243, merged 2026-10-06; decision-table test only, no in-game run) |

## Plan and readback

First plan: 2 updated, 4 conflicts. After the manifest corrections: 6 updated, 254 unchanged, 0 conflicts, 21 operations. `--apply` succeeded; the Project read back the six items with the values above, and a new plan showed 260 unchanged and 0 operations.

## v0.8.37 release

Receipt for the v0.8.37 release record (published 2026-10-06T07:46:23Z, tag commit `bfa6c824`, not accepted). Manifest changes only; the Project was not written, no Issue, pull request or comment was touched, and no game was run. The live Issue and release state was read with `gh` at about 07:49 UTC.

| Key | Change |
| --- | --- |
| `release-v0-8-37` (new) | Kind Release, Done / Released / v0.8.37, 2026-10-06. Publication facts, the Gitea run (318, API id 710), the five assets with sizes and SHA-256 values, the macOS image checks, the work included and the open acceptance items. There is no v0.8.30 release record in the manifest (the latest earlier one is `release-v0-8-6`), so the shape follows that one |
| `issue-114-container-enemy-softlock` | Awaiting validation stays; Delivery Implemented → Released; Release Next release → v0.8.37. PR #243 has a decision-table test only. Live state: the Issue is **closed** (the maintainer closed it at 2026-10-06T07:11:17Z, after the 07:01 UTC comment that the fix ships in the next release), not open as the request assumed; the manifest keeps Awaiting validation because no reporter confirmation and no in-game run exist |
| `issue-54-japanese-cutscene-language` | Done stays; Delivery Awaiting validation → Released; Release v0.6.3 → v0.8.37. The 2026-10-06 cause (an upper-case voice code from the host language hook) and PR #242 are in the evidence and in a dated body section; the 2026-09-26 text is kept |
| `issue-214-mali-black-screen` (new) | Bug, Awaiting validation / Released / v0.8.37. The Issue is open (closed by the PR #241 merge at 05:38:59Z, reopened by the maintainer at 06:36:06Z); no Mali device ran the fix |
| `issue-199-android-fullscreen` (new) | Feature, Done / Released / v0.8.37 (closed by the PR #240 merge at 05:49:55Z); no device with a display cutout was checked |
| `issue-220-cgi-voice-language` (new) | Bug, Done / Released / v0.8.37 (closed by the PR #242 merge at 06:46:54Z); checked from a trace, not by listening |

No record exists yet for #237 (closed with PR #238, shipped in v0.8.37) or the earlier v0.8.7 to v0.8.30 releases; they are not added here.

Plan, `python -B tools/project_management/sync.py` (plan only, `--apply` not run): 4 created, 2 updated, 258 unchanged, 0 conflicts, 43 operations. The two updates are `issue-114-container-enemy-softlock` and `issue-54-japanese-cutscene-language`; the four creations are the release record and the three Issues above. The apply and the readback are for the maintainer's session.

Apply notes: the first `--apply` stopped at the #114 field batch with `Column value must be a valid value for text column`; the record's Evidence had 1,105 characters, above the Project text-field limit (1,024). The Delivery and Release values of that batch had already been written. The Evidence was shortened to 725 characters and the apply completed (4 created, 1 updated). The following plan showed one conflict: `issue-214-mali-black-screen` Status was `In Progress` on the Project (set by the Project's own workflow when the maintainer reopened #214 at 06:36 UTC) while the manifest wanted `Awaiting validation`; the tracked value was aligned to the remote and the desired value applied. A final plan showed 264 unchanged and 0 operations.

## v0.8.39 release

Receipt for the v0.8.39 release record (published 2026-10-06T08:48:26Z, tag commit `fd7d82ce`, not accepted). Manifest changes only; the Project was not written, no Issue, pull request or comment was touched, and no game was run. The live release state and asset digests were read with `gh release view v0.8.39` and `git ls-remote`; the PR bodies of #248 and #249 were read with `gh pr view`.

| Key | Change |
| --- | --- |
| `release-v0-8-39` (new) | Kind Release, Done / Released / v0.8.39, 2026-10-06. Publication facts, the Gitea run (335, API id 727), the five assets with sizes and SHA-256 values, the macOS image checks, the two PRs included and the open acceptance items. The shape follows `release-v0-8-37` |
| `shader-delivery-v080` | Status Todo → Done; Delivery In progress → Released; Release v0.9.0 → v0.8.39. The last step (cache cleanup, PR #248, merged 07:50:09Z) is checked in the body, a dated "Released" section was added and the Evidence carries the measurement (Proton Direct3D 12 only) and "not accepted" |
| `pipeline-first-use-stalls` (new) | Feature, Graphics performance, In Progress / In progress / v0.9.0. P0 and P1 (PR #249, merged 08:24:01Z) shipped in v0.8.39 with the psvita and M1 Max numbers; P2 to P4 are open. No item for this plan existed in the manifest, so it was created. Delivery is "In progress", not "Released", because three of five phases are not started |

No Issue is linked to PR #248 or #249, so no Issue record changed. #237 and the v0.8.7 to v0.8.30 release records are still not in the manifest.

Plan, `python -B tools/project_management/sync.py` (plan only, `--apply` not run): 2 created, 1 updated, 263 unchanged, 0 conflicts, 24 operations. The two creations are `release-v0-8-39` and `pipeline-first-use-stalls`; the update is `shader-delivery-v080`. The apply and the readback are for the maintainer's session.

## v0.8.44 release

Receipt for the v0.8.44 release record (published 2026-10-06T17:34:21Z, tag commit `504e7cd0`, not accepted). Manifest changes only; `sync-state.json` was not edited, the sync tool was not run (no plan, no `--apply`), the Project was not written, no Issue, pull request or comment was touched, and no game was run. The live release state and asset digests were read with `gh release view v0.8.44` and `git ls-remote`; the PR merge times and Issue states with `gh pr view` and `gh issue view`; the PR bodies of #250, #252, #254, #255 and #256 with `gh pr view`.

| Key | Change |
| --- | --- |
| `release-v0-8-44` (new) | Kind Release, Done / Released / v0.8.44, 2026-10-06. Publication facts, the Gitea run (743), the five assets with sizes and SHA-256 values, the macOS image checks, the six PRs included and the open acceptance items. The shape follows `release-v0-8-39` |
| `issue-253-android-ctrl-movable` (new) | Feature, Input, Done / Released / v0.8.44. The Issue was closed by the PR #254 merge at 16:37:44Z; checked on one tablet only; no reporter confirmation |
| `issue-251-android-iso-importer-error` (new) | Bug, Installation, Awaiting validation / Released / v0.8.44. The Issue is open; the cause is a copy cut at 4 GB by FAT32 storage, so v0.8.44 only improves the error message; the maintainer replied after the release |
| `issue-172-fsr-scaling-performance` (new) | Bug, Graphics performance, In Progress / Released / v0.8.44. PR #189 (v0.8.10) and PR #250 (v0.8.44) shipped; the Issue stays open for further work. Delivery is "Released" for the shipped PRs while Status stays "In Progress" because the Issue is open; this is a judgment call, change it if the Project's convention differs |
| `issue-199-android-fullscreen` | Evidence extended with the regression (bars returned after SDL's window-style command) and PR #256 (v0.8.44). Status Done and Release v0.8.37 are unchanged |

No record exists yet for the earlier v0.8.7 to v0.8.30 releases or for #237; they are not added here. The README item count was brought to 270 with this apply.

Plan and apply (maintainer's session, 2026-10-06 after publication): the plan reported 3 created, 2 updated, 265 unchanged, 0 conflicts, 36 operations. `issue-172-fsr-scaling-performance` counted as updated rather than created because the Project already held Issue #172 as an item (the Project workflow adds reopened Issues), so the manifest key bound to that item. `--apply` wrote the 5 items and `sync-state.json`; the re-plan afterwards reported 0 operations over 270 items. Readback with `gh project item-list`: the four v0.8.44 items show Status/Delivery/Release as listed above.

## Drift reconciliation after v0.8.44

Receipt for closing the gap between the manifest and the live GitHub state. Manifest and documentation changes only; this pass did not edit `sync-state.json`, did not run the sync tool, touched no Issue, pull request or comment, and ran no game. Checked on 2026-10-06 after the v0.8.44 publication: `items.json` against `gh issue list --state all` (107 Issues, 12 open: #40, #48, #104, #151, #167, #172, #174, #198, #201, #202, #214, #251) and against the Project listing (272 items: 270 managed, the keyless Issue #17 link and the keyless Issue #219 item that the Project workflow added). Issue states, comments, PR merge times and merge commits were read with `gh issue view` and `gh pr view`; releases, run ids, assets and DMG checks come from the `v0.8.7` to `v0.8.30` sections of [STATUS](../STATUS.md), [CHANGELOG](../../CHANGELOG.md) and [MACOS_RELEASE](../MACOS_RELEASE.md). Where a fact is not recorded there (for example a DMG check), the record says so.

| Key | Kind | Status / Delivery / Release |
| --- | --- | --- |
| `release-v0-8-7` (new) | Release | Done / Released / v0.8.7 (2026-10-03) |
| `release-v0-8-10` (new) | Release | Done / Released / v0.8.10 (2026-10-04) |
| `release-v0-8-15` (new) | Release | Done / Released / v0.8.15 (2026-10-04) |
| `release-v0-8-21` (new) | Release | Done / Released / v0.8.21 (2026-10-04) |
| `release-v0-8-30` (new) | Release | Done / Released / v0.8.30 (2026-10-05) |
| `issue-176-steam-deck-shadow-flicker` (new, #176) | Bug | Done / Released / v0.8.10 |
| `issue-183-flickering-shadows` (new, #183) | Bug | Done / Released / v0.8.15 |
| `issue-185-android-adreno-start-crash` (new, #185) | Bug | Done / Released / v0.8.21 |
| `issue-203-sea-of-baus-flickering-sky` (new, #203) | Bug | Done / Released / v0.8.21 |
| `issue-212-remaining-taa-dlss-flicker` (new, #212) | Bug | Done / Released / v0.8.30 |
| `issue-219-enemy-fade-in-first-battle` (new key, #219) | Bug | Done / Released / v0.8.30 |
| `issue-237-ao-polygons-white-flashes` (new, #237) | Bug | Done / Released / v0.8.37 |
| `issue-194-android-save-backup-and-lt` (new, #194) | Feature | Done / Released / v0.8.30 |
| `issue-239-xbox-360-trainers` (new, #239) | Feature | Done / Superseded / none |
| `issue-104-faster-menu-animation` (new, #104) | Feature | Todo / Not started / none |
| `issue-151-animated-options-menu` (new, #151) | Feature | Todo / Not started / none |
| `issue-174-surround-5-1-audio` (new, #174) | Feature | Todo / Not started / none |
| `issue-198-rumble-intensity` (new, #198) | Feature | Todo / Not started / none |
| `issue-201-display-choice` (new, #201) | Feature | Todo / Not started / none |
| `issue-202-gpu-choice` (new, #202) | Feature | Todo / Not started / none |
| `anti-aliasing-consolidation-remove-experimental-taa` (edited) | Feature | Todo / Not started; Release `Next release` cleared (none); a dated sentence added to the Evidence |

The release records follow `release-v0-8-39`: job timings, tag objects, assets and DMG checks are quoted from the STATUS and MACOS_RELEASE sections. The v0.8.7 and v0.8.15 DMGs have no recorded checks, so their DMG criterion stays unchecked, and the run, tag and assets of v0.8.15 were read after v0.8.21 was published (the body says so). Issue items use the Issue's exact title (two spaces after `[Bug]` for #176 and after `[Feature]` for #194), the Issue created date as Start date, and Evidence under 1,024 characters.

### Judgment calls

- **Live state differs from STATUS on three Issues.** STATUS predates these comments. #176: the reporter wrote "Thanks so much, you solved!" at 2026-10-04T12:52:53Z, after v0.8.10 was published. #203: the reporter wrote "It's fixed, no more flickering!" at 2026-10-05T15:10:59Z, after v0.8.21. Neither names a build or a test, so both are recorded as reporter-reported, not as acceptance (the #175 precedent of the 2026-10-03 receipt). #237: on 2026-10-06T17:31:35Z the reporter posted a video and a save showing the outlines still appear during enemy attacks on v0.8.39 (the boss fight after the door). The Issue is still closed. The item stays Done / Released / v0.8.37 because the maintainer closed it (the #183 precedent, whose reporter also reported remaining flicker), and the Evidence carries the follow-up. Whether to reopen #237 or open a follow-up is the maintainer's decision. STATUS and the roadmaps still say the reporter has not replied for these three.
- **#239.** The maintainer closed it at 2026-10-06T05:03:27Z with "its already there." and no PR. No usable precedent exists: the #114, #116 and #121 closures of the 2026-10-03 receipt followed a fix, `issue-85-minimap-toggle` shipped a README line, and `issue-49` was Done / Deferred with a code change. So the item is Feature / Done / Superseded with no Release; the Evidence says the existing cheat functions cover the request and no change was made.
- **#194** is a Feature in Save & Storage: PR #222 (save ZIP export and import) and PR #223 (on-screen LT) shipped in v0.8.30. It is a ZIP export and import, not cloud sync, and no device run is recorded.
- **#219.** The Project already holds Issue #219 as a keyless item (the Project workflow adds Issues). The sync binds the new key to that item, so it counts as an update, not a creation, and the item's Project field values will be set to the desired ones.
- **#90** (closed 2026-10-03T06:50:52Z: HDR passed, save states postponed) gets no new item; it stays covered by the drafts `hdr-output-tonemapping` and `save-state-support`.
- **Open requests without items** (#104, #151, #174, #198, #201, #202) follow `issue-167-cutscene-audio-desync`: Todo / Not started, no Release, no dates, and "No maintainer decision or implementation is recorded."
- **Left as they are.** `issue-199-android-fullscreen` keeps Release v0.8.37 (Release is the first release that carried the delivery, the convention of the "HDR Release" note in the 2026-10-03 receipt; the PR #256 regression fix in v0.8.44 is already in its Evidence). `issue-114-container-enemy-softlock` keeps Awaiting validation / Released / v0.8.37 although the Issue was closed 2026-10-06T07:11:17Z (closure is not reporter acceptance). `native-macos-platform` and `hdr-output-tonemapping` keep Release v0.7.35 (same convention).
- **`anti-aliasing-consolidation-remove-experimental-taa`.** The "TAA (Experimental)" option is still in `LostOdysseyRecomp/settings/menu.cpp`, so it stays Todo / Not started. Its Release text "Next release" was stale after thirteen releases. The maintainer cleared the Project's Release field directly (`gh project item-edit --clear`, about 2026-10-06T18:10Z; the readback shows no Release) and set the last-synced value in `sync-state.json` to null. `items.json` now has `Release: null` (the sync tool skips null) and a dated sentence in the Evidence; to stay under the field limit the Evidence lost the parenthetical on the capture header's version.

Plan and apply (maintainer's session, 2026-10-06, after the Release clear above): the plan reported 19 created, 2 updated, 269 unchanged, 0 conflicts, 171 operations. The 19 creations are the five release drafts, seven Issue items (#176, #183, #185, #203, #212, #237, #194), #239 and the six open requests; the two updates are #219 (bound to the keyless Issue item the Project already held) and the anti-aliasing item's Evidence (its cleared Release produces no operation because the tool skips null). `--apply` wrote them; the re-plan afterwards reported 0 operations over 290 items. Readback with `gh project item-list`: 291 Project items (290 managed plus the keyless Issue #17 link), and the new items show the Status / Delivery / Release values listed above. Live reporter replies read during this pass and copied into the Evidence fields: #176 (2026-10-04 12:52 UTC, solved, build not named), #203 (2026-10-05 15:10 UTC, fixed, build not named) and #237 (2026-10-06 17:31 UTC, a video and a save showing the outlines still appearing during enemy attacks on v0.8.39; the Issue stays closed, the item stays Done / Released / v0.8.37, and reopening or a follow-up is the maintainer's call).

## Frame-generation research done

The maintainer marked `frame-generation-research` done on 2026-10-06. Paused / Deferred / v0.9.0 → Done / Released / v0.7.35: Direct3D 12 DLSS-G and FSR frame generation shipped in v0.7.9, and Vulkan DLSS-G and experimental Vulkan FSR 3.1 frame generation in v0.7.35, the first release with all four provider/API combinations of the v0.7.0 plan. The Evidence and a dated body section say so. No Issue, pull request, comment, build or game run was touched.

Plan: 0 created, 1 updated, 289 unchanged, 0 conflicts, 5 operations. `--apply` succeeded; the re-plan showed 290 unchanged and 0 operations, and `gh project item-list` read the item back as Done / Released / v0.7.35.

## First-use pipeline stalls: P2 to P4 merged

Receipt for the `pipeline-first-use-stalls` record after PRs #259 to #264 merged (2026-10-06T22:52:01Z to 2026-10-07T00:24:17Z; not released, not accepted). Manifest and documentation changes only; `sync-state.json` was not edited, **the sync tool was not run (no plan, no `--apply`)**, the Project was not written or read, no Issue, pull request or comment was touched, and no build or game run was made. The plan and apply are for the maintainer's session. Live state read with `gh` at about 00:26 UTC on 2026-10-07: `gh pr view` for #259 to #264 (all MERGED; merge commits `b7592d06`, `7f3806b3`, `2c8f7d07`, `75c75e3e`, `6760b23e`, `627009a9`), `gh release list` (the latest release is still v0.8.44, published 2026-10-06T17:34:21Z, before every merge; `git tag --contains b7592d06` lists no tag) and `gh release view shader-packs` (`pipelines_corpus-80afef9497d4bb14.bin`, 409,184 B, SHA-256 `80afef9497d4bb14b600c91693d48e9f876fcb6f00c546f44f634b377fac24dc` as GitHub lists it, equal to the `index.json` entry; updated 2026-10-07T00:15:30Z; `index.json` lists 8 entries, one with renderer `pipeline-corpus`).

| Key | Change |
| --- | --- |
| `pipeline-first-use-stalls` | Status In Progress → Awaiting validation; Delivery In progress → Implemented; Release v0.9.0 unchanged. The scope list checks P2, P3 and P4 with their PRs, merge times and merge commits; the old "P2 to P4 are not started" sentence is kept as an "as of 2026-10-06" statement and a dated "Merged for v0.9.0 - 2026-10-07" section was added (what each PR does, the corpus publication, the measurement hosts, the not-tested list). The Evidence was rewritten to the current boundary (812 characters; the P0 and P1 numbers stay in the body). Six PR links and the `shader-packs` prerelease were added to the sources and `source_refs` |

Judgment calls, each a one-field change if the maintainer's convention differs:

- **Status.** "Awaiting validation" follows the `issue-114-container-enemy-softlock` precedent in this file (merged, not released, decision-table test only). "In Progress" (the `dynamic-mfg` and `native-object-motion` precedent for Implemented work with open scope) is the alternative; I chose Awaiting validation because every phase of the plan is now merged and what remains is validation on platforms that were not tested and release. "Done" is not used: no player or maintainer acceptance is recorded, and the plan's acceptance targets were not measured on the tablet.
- **Release.** v0.9.0 is kept as the maintainer's 2026-10-05 schedule, not "Next release" (that value went stale across thirteen releases for the anti-aliasing item above). P0 and P1 shipped earlier, in v0.8.39; this is in the Evidence and the body, not in the Release field, because the item's remaining phases land in v0.9.0.
- **Delivery.** "Implemented" is the state of P2 to P4; P0 and P1 are released. The previous receipt used "In progress" only because three of five phases were not started.
- **Roadmap marker.** Both roadmaps keep the item `[~]`, flip the P2, P3 and P4 sub-boxes to `[x]` and carry the same dated update. The bullet stays partial: nothing is released or accepted and Android is untested.

Evidence gaps, kept open: Android (TB321FU, Turnip), NVIDIA and Intel Windows Vulkan drivers and a cold Metal system cache were not tested for P2 to P4; the note also lists P4 on native Linux Vulkan as not measured. Its P3 line that the battle scene-ID read had not been checked in a real battle may be overtaken by the later corpus tour (271 battle recipes tagged on psvita and the M1 Max); the note does not say so, so it is not claimed here.

Plan and apply (maintainer's session, 2026-10-07 about 00:40 UTC): the plan reported 0 created, 1 updated, 289 unchanged, 3 conflicts, 4 operations. `--apply` wrote the four field changes of `pipeline-first-use-stalls`; the re-plan showed 0 operations, and `gh project item-list` read the item back as Awaiting validation / Implemented / v0.9.0. The three conflicts were left alone because they are the maintainer's own Status changes on the Project, not part of this record: `issue-40-controller-mod-support` and `issue-251-android-iso-importer-error` show Done (both Issues closed as completed on 2026-10-06, 20:47Z and 22:20Z), and `issue-104-faster-menu-animation` shows Done although the Issue is open again (closed 22:23:54Z and reopened 22:24:38Z by the maintainer). Their manifest records still need a decision.

## Maintainer decisions, 2026-10-07

- **Project values win for three Issues.** The maintainer's own Status changes on the Project are kept and copied into the manifest: `issue-40-controller-mod-support` and `issue-251-android-iso-importer-error` → Done (both Issues closed as completed on 2026-10-06), `issue-104-faster-menu-animation` → Done (the Issue was closed and reopened by the maintainer on 2026-10-06; the Project value is kept as the maintainer set it). Manifest and `sync-state.json` were edited; no Project write was needed for these.
- **`temporal-phased-p0-common-contracts-probe` moved to the backlog.** In Progress / Validated / v0.9.0 → Paused / Deferred / no Release, the convention of the other backlog items. The Release field was cleared on the Project with `gh project item-edit --clear` and set to null in `sync-state.json` (the sync tool skips null); the Evidence and a dated body section record the move. Both roadmaps list the remaining coverage in the later backlog.

Plan and apply: 0 created, 1 updated, 289 unchanged, 0 conflicts, 4 operations; the re-plan showed 0 operations and 0 conflicts over 290 items, and `gh project item-list` read the four items back with the values above.

## First-use pipeline stalls accepted, 2026-10-06 (Android)

After the tablet run (TB321FU, Turnip, debug build of main `2b5044af`; numbers in the plan note's "Android 实测" section) the maintainer accepted the effect and asked for no further tuning ("能生效就可以"). `pipeline-first-use-stalls`: Awaiting validation / Implemented / v0.9.0 → Done / Implemented / v0.9.0 (Delivery stays Implemented until a release carries it); Evidence and a dated body section record the Android result. Both roadmaps mark the item `[x]` with a dated update. Plan: 1 updated, 289 unchanged, 0 conflicts, 3 operations; `--apply` succeeded, the re-plan showed 0 operations, and the Project read the item back as Done / Implemented / v0.9.0.

## Settings batch merged, 2026-10-07 (#104, #198, #174, #202, #201)

Receipt for the five request items after PRs #269, #270, #271, #274 and #276 merged on 2026-10-07 (not released, latest release v0.8.44, published 2026-10-06T17:34:21Z). Manifest and documentation changes only; `sync-state.json` was not edited, **the sync tool was not run (no plan, no `--apply`)**, the Project was not written, no Issue, pull request or comment was touched, and no build or game run was made. The plan and apply are for the maintainer's session.

Live state read with `gh` at about 07:20 UTC on 2026-10-07: `gh pr view` for #269 (merged 03:01:14Z, `35e22a28`), #270 (03:17:30Z, `3182adab`), #271 (03:35:13Z, `a5fa8961`), #274 (03:42:11Z, `475d8c65`) and #276 (07:08:08Z, `6ed90748`), all MERGED; `gh issue view` for #104 (closed 03:01:15Z), #198 (03:17:31Z), #174 (03:35:15Z), #202 (03:42:13Z) and #201 (07:08:30Z), all CLOSED as completed; `gh release list` (v0.8.44 is the latest). A read-only `gh project item-list` shows Status Done for all five items already (set after the Issues closed; the manifest still said Todo for four of them), with Delivery Not started and no Release.

| Key | Status / Delivery / Release (manifest) |
| --- | --- |
| `issue-104-faster-menu-animation` | Done / Not started → Implemented / none; Evidence rewritten: the menu-animation request was declined, the delivered follow-up is the saved fast-forward settings (PR #269) |
| `issue-174-surround-5-1-audio` | Todo / Not started → Done / Implemented / none (PR #271) |
| `issue-198-rumble-intensity` | Todo / Not started → Done / Implemented / none (PR #270) |
| `issue-201-display-choice` | Todo / Not started → Done / Implemented / none (PRs #274, #276; the maintainer's three-monitor test accepted 2026-10-07) |
| `issue-202-gpu-choice` | Todo / Not started → Done / Implemented / none (PR #274) |

All Evidence fields are under 1,024 characters.

Judgment calls, each a one-field change if the maintainer's convention differs:

- **Status Done for all five.** The Project already holds Done and the Issues are closed, and the maintainer's own Project values win (the 2026-10-07 decisions above). Done is not acceptance: #174 (no real 5.1 speakers; a commenter offered to test), #198 (no physical controller) and #202 (no Optimus or multi-adapter Direct3D 12 run) carry their gaps in the Evidence, as `issue-199-android-fullscreen` and `issue-251-android-iso-importer-error` do. `Awaiting validation` is the alternative for these three; it would differ from the Project.
- **Delivery Implemented, Release none.** Merged, not released, the `issue-200-taa-proton-silent-exit` and `issue-179-sdr-white-levels` convention. "Next release" is not used (it went stale for the anti-aliasing item). Release is set when a release carries the work.
- **#201 acceptance.** The maintainer's three-monitor Windows test and acceptance on 2026-10-07 comes from the maintainer's report to the session. No Issue or PR comment records it (PR #276's body says the maintainer was retesting), so the Evidence says so.
- **#104.** Done here records the maintainer's closure and the saved fast-forward settings, not a faster menu animation; the maintainer declined that on 2026-09-30 and 2026-10-06.
- **Start date** stays unset for all five, like the other merged-unreleased Issue items.
- **Not itemized.** The removal of exclusive fullscreen (PR #274) and the rename to Fullscreen (PR #276) have no Issue or item of their own; they stay in the CHANGELOG and the notes (the "not itemized" precedent of the 2026-10-03 receipt).

Expected plan before the `sync-state.json` fix: 1 updated (#104) and 4 Status conflicts. The last-synced Statuses of #174, #198, #201 and #202 in `sync-state.json` are still Todo while the Project reads Done (the Project values are the maintainer's, and the manifest already matches them). After copying Done for those four into `sync-state.json`, as on the 2026-10-07 decisions above, the plan should report 0 created and 5 updated, writing Delivery and Evidence.

Plan and apply (maintainer's session, 2026-10-07 about 07:35 UTC): `sync-state.json` took the Project's Done for #174, #198, #201 and #202 first, as the section above says. The plan then reported 0 created, 5 updated, 285 unchanged, 0 conflicts, 10 operations; `--apply` wrote them, the re-plan showed 0 operations over 290 items, and `gh project item-list` read #104, #174, #198, #201 and #202 back as Done / Implemented / no Release.
