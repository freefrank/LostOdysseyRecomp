# Project reconciliation — 2026-10-14

Receipt for the Project sync after the v0.9.52 release (published 2026-10-14T04:07:16Z). It records the v0.9.52 release record, two closed Issues with their fix evidence, two open Android Issues, the Android experimental label removal, the render-pass merge step that is already on `main`, the PM4 assessment, the mod-loader progress, and two maintainer TODOs that were added as backlog drafts. No Issue, pull request, comment, commit or push was changed by this sync.

## Run

From the worktree on `docs/pm-sync-after-v0952` (base `origin/main` 128e36d5):

- Plan 1: `{"created": 6, "updated": 4, "unchanged": 304, "conflicts": 0, "operations": 57}`.
- `python -B tools/project_management/sync.py --apply`: same counts, exit code 0. The report went to the scratchpad, not `out/`.
- Plan 2 (after the apply): `{"created": 0, "updated": 0, "unchanged": 314, "conflicts": 0, "operations": 0}`, exit code 0.
- Readback (`gh project item-list 3 --owner freefrank --format json`): 315 items (314 managed records plus the keyless Issue #17 link). Every touched item was read back: Status, Delivery, Release, Kind, Area, start date and draft body presence matched. The full Evidence text was not compared word by word; the Evidence lengths are all under 1,000 characters.
- Round 2 (coordinator decisions, same day): plan `{"created": 1, "updated": 1, "unchanged": 313, "conflicts": 0, "operations": 13}`; apply same counts, exit 0; re-plan `{"created": 0, "updated": 0, "unchanged": 315, "conflicts": 0, "operations": 0}`, exit 0. Readback: 316 items; `render-pass-merging-v100` and `issue-492-android-black-artifacts-app-switch` matched the expected values and bodies.
- Project: <https://github.com/users/freefrank/projects/3>

Live GitHub checks before writing: `gh pr list --state all` (PR states and merge dates for #440 to #489) and `gh pr view` for #414, #403, #404, #405, #459, #478 and #457 (body); `gh issue list --state all` and `gh issue view` for #441, #453, #454, #456, #477, #490, #491 and #492, with events and comments for #454 and #477; `gh release view v0.9.52` (published, not a draft) and `gh release view v0.9.52 --json assets`; `git merge-base --is-ancestor` for #414 (in v0.9.32 and v0.9.52, not v0.9.22), and for #455, #478, #481, #482 and #486 against `v0.9.52`.

## Changed records

| Record | Before | After |
| --- | --- | --- |
| `experimental-android-platform` | Title "Experimental Android platform support"; In Progress; Released; Release v0.8.5 | Title "Android platform support (no longer experimental)"; Done; Released; Release v0.8.5 (unchanged). Evidence starts with the 2026-10-14 close (PRs #481 and #482, in v0.9.52) and keeps the unverified list. Body has a "Status — 2026-10-14" section. |
| `remove-pm4-translator` | Paused / Deferred | Paused / Deferred (unchanged). Evidence and body add the 2026-10-13 assessment: not worth it before v1.0 (upper bound about 1–1.4 ms per thread vs a ~4 ms gap; PR #89 measured no gain). |
| `render-pass-merging-v100` | Paused / Deferred / After v1.0.0 | In Progress / In progress / v1.0.0 (round 2, coordinator decision: the maintainer asked to continue on 2026-10-14). Evidence records the branch `perf/constant-edram-transfer` (GitHub, Gitea and local; no PR yet), the shipped step PR #414 (merged 2026-10-11, in v0.9.32; phase 1 about 0.2 ms; TB321FU passes 291 to 172), the 2026-10-14 hot 720p baseline (p95 21.4-21.6 ms vs the 18 ms gate) and the open measurement checkpoint. Body has a "Status — 2026-10-14" section. No result is accepted. |
| `texture-replacement-mod-loader` | In Progress / Released / v0.7.0 | Unchanged fields. Evidence now lists the v0.9.52 work (#463, #467 to #469, #472, #473, #474, #475, #476, #480, #486), keeps the remaining scope, and adds the 4 GiB limit as a pending maintainer decision. Body has a 2026-10-14 reconciliation note. |
| `android-frame-time-v100` | In Progress / v1.0.0 | Unchanged. #459 and the 2026-10-12 p95 gate were already recorded in the 2026-10-12 receipt. |

## Created records

| Record | Type | Status / Delivery / Release | Basis |
| --- | --- | --- | --- |
| `release-v0-9-52` | Draft (release record) | Done / Released / v0.9.52, dates 2026-10-14 | Published release; five assets with GitHub API size and digest; published, not accepted. Other PRs stay in CHANGELOG.md and the release page, as with `release-v0-9-22`. |
| `issue-454-steam-face-buttons` | Issue #454 (linked) | Done / Released / v0.9.52, start 2026-10-12 | Fix PR #455 is in v0.9.52. The maintainer closed the Issue as completed at 2026-10-14 05:22Z. No reporter confirmation is recorded. |
| `issue-477-android-startup-display-setup` | Issue #477 (linked) | Done / Released / v0.9.52, start 2026-10-14 | Fix PR #478 is in v0.9.52. The reporter wrote "it runs properly now" at 04:43Z and closed the Issue. This confirms startup on the reporter's device only. |
| `issue-491-android-no-sound-after-app-switch` | Issue #491 (linked, open) | In Progress / In progress / no Release, start 2026-10-14 | Open. Analysis in progress. No fix is recorded; PR #461 is not claimed as the fix. |
| `dlss-nr-settings-preview-blank` | Draft (maintainer TODO) | Todo / Not started, Area Settings | Maintainer note of 2026-10-13. Not an Issue; no owner, priority or date. |
| `fg-multiplier-restart-required` | Draft (maintainer TODO) | Todo / Not started, Area Frame generation | Maintainer note of 2026-10-13. Not an Issue; no owner, priority or date. |
| `issue-492-android-black-artifacts-app-switch` | Issue #492 (linked, open; round 2) | In Progress / In progress / no Release, start 2026-10-14 | Open, same reporter as #491; analysis in progress together with #491. No fix is recorded. |

## Conflicts with the brief and judgment calls

1. **Render-pass merging (resolved in round 2).** The first pass kept the item Paused / Deferred / After v1.0.0 because no open PR existed for the constant-EDRAM work. The coordinator then confirmed that the work is active: the maintainer asked to continue on 2026-10-14, branch `perf/constant-edram-transfer` exists on GitHub, Gitea and locally, and a measurement checkpoint is running on the TB321FU. The item is now In Progress with the v1.0.0 target. The 2026-10-12 pause is recorded as superseded in its body. No pass-merge result is accepted.
2. **#477 wording.** The brief said "awaiting reporter". The live state is different: the reporter confirmed startup on v0.9.52 and closed the Issue himself at 04:43Z. The record is Done with that scope. The title's "crash after shader download" is not covered by any recorded test.
3. **Closed Issues without reporter confirmation (#454).** The maintainer closed it. I used Done with the missing confirmation written in Evidence, which matches the existing closed-Issue records (for example #199, #251, #253 and #214). "Awaiting validation" was not used for these two; a parent decision could change that.
4. **Android experimental label.** The item is Done as instructed. The unverified scope (16 KB pages, other devices and GPUs, audio, controller hot-plug, acceptance) stays in Evidence and body, so the closure does not claim it.
5. **DLSS-NR preview and FG multiplier.** I added them as backlog drafts, following the `streamline-vulkan-present-after-write` precedent for maintainer-raised backlog. They have no priority, owner or date.

## Items not mapped

- **#453** (open, created 2026-10-12): the game keeps asking to download shaders. PR #457 does not cite it, so it is not linked to the shader-bundle work.
- **#441 and #456** (closed 2026-10-13): faster menus and saving, and faster item interaction. The closest record is `loading-and-save-speed-2026-10-10`, but its scope (loading and fewer steps) differs from an 8x-speed request. Not edited.
- **#490** (closed 2026-10-14, "Mods on Linux?"): not mapped.
- **PR #485** (Mali PanVK recommendation): not linked to `issue-214-mali-black-screen`. That record says no Mali device was tested, and #485 is not claimed as its fix.
- **PR #463 to #476 and #486** beyond the mod-loader Evidence: they are described in the release record only.

## Left for the maintainer or parent

- `docs/ROADMAP.md` and `docs/ROADMAP.zh-CN.md` still describe Android as experimental and still carry the PM4 entry. These files are outside the authorized scope, so they were not edited. They need a change in the roadmap pair, by the parent or docs_sync.
- Whether to close `texture-replacement-mod-loader`. It stays In Progress.
- The 4 GiB archive limit for full-map model mods on discs 3 and 4 (decision pending). Note only; no record until decided.
- The README table has no row for the 2026-10-12 receipt. It was not backfilled here.
- The sync-state.json file was rewritten by the apply and is uncommitted, as are items.json, this receipt and the README row. The main agent reviews and ships them.

## Round 3: mod-loader closure, Android checkpoint, archive links

Applied in the worktree `pm-sync-20261014b` (branch `docs/pm-sync-20261014b`, base `origin/main` 150d94a2). Scope: the maintainer's closure of `texture-replacement-mod-loader`, the Android frame-time checkpoint, the Android resume state of #491 and #492, and the links to five archived fixed-bug notes.

Run:

- Plan 1 before edits: 315 unchanged, 1 conflict. `issue-491` Status was Done in the Project (GitHub closed the Issue when PR #496 merged) and In Progress in the manifest. The conflict was cleared by copying the readback value into `sync-state.json`, then setting the desired Status to Awaiting validation (see judgment call 1).
- Plan 2 after the manifest edits: `{"created": 0, "updated": 12, "unchanged": 303, "conflicts": 0, "operations": 19}`.
- `--apply`: 12 updated, exit 0.
- Readback (`gh project item-list 3`, 316 items): every touched item matched its Status, Delivery, Release, Evidence (all under 1,024 characters) and draft body. The new body text was found in the readback.
- Plan 3: `{"created": 0, "updated": 0, "unchanged": 315, "conflicts": 0, "operations": 0}`, exit 0.

Live GitHub checks before writing: `gh issue view` for #491 (closed COMPLETED 06:27Z by PR #496), #492 (open), #477 (closed); `gh pr list --state all` for #461, #463, #467 to #469, #472 to #476, #480, #486, #495, #496 and #498 (all merged); `git merge-base --is-ancestor` for the eleven mod-loader PRs against `v0.9.52` (all contained). `e6bd76f4` (#495) is on `origin/main` but not in `v0.9.52`. PR #496 says "Fixes #491, refs #492"; PR #498 says "Refs #492".

Changed records:

| Record | Before | After |
| --- | --- | --- |
| `texture-replacement-mod-loader` | In Progress / Released / v0.7.0 | Done / Released / v0.9.52, on the maintainer's decision. Body has a "2026-10-14 (closed)" section with the eleven shipped PRs, the 4 GiB limit as a known limitation with no new item, and the statement that closing is not acceptance. "stage_detail: Not started" is removed. The original scope checkboxes are kept and marked as superseded. Evidence is rewritten to 685 characters. |
| `android-frame-time-v100` | In Progress / In progress / v1.0.0 | Fields unchanged. A "Status — 2026-10-14" section records #495 (merged to main, not released; transfer passes 1.19 to 0.68 ms GPU, whole frame flat, per the maintainer's report; device, scene and sample count not recorded), checkpoint 2 (hot p95 tail is CPU scheduling under thermal caps, not GPU; render-thread priority experiment running, no result), and #496 and #498 as related resume work, not gate evidence. The gate is still open. Sources add #495, #496 and #498. |
| `render-pass-merging-v100` | Evidence and body said "no PR yet" | Evidence and body say the constant-EDRAM transfer work is merged as PR #495 (not released). Status and the "no pass-merge result accepted" statement are unchanged. |
| `issue-491-android-no-sound-after-app-switch` | Project Done (automatic close); manifest In Progress / In progress | Awaiting validation / Implemented / no Release. Evidence: closed 06:27Z by PR #496, merged to main as 758dd93d, not in a release, not run on a device, reporter has not confirmed, closure is not verification. |
| `issue-492-android-black-artifacts-app-switch` | In Progress / In progress | Fields unchanged. Evidence: PR #496 (1 s frame-plan hold) and PR #498 (landscape lock, event watcher) reference #492 and are partial; a portrait period longer than 1 s still resets the plan twice; the Issue stays open. |
| `issue14-cage-crash-investigation` | Evidence linked `docs/notes/issue12-root-cause.md` | Link changed to `docs/archive/notes/issue12-root-cause.md`. Other fields unchanged. |
| Six drafts and the Kalm Issue record (`gpu-resolve-resource-lifetime`, `physical-memory-render-aliases`, `post-battle-whiteout`, `title-packed-mip-background`, `manual-save-overwrite-reload`, `save-content-compatibility`, `issue12-kalm-daughter-funeral-flower-crash`) | Source and body links to `docs/notes/<one of the five>` | Links point to `docs/archive/notes/<same name>`. Pinned permalinks to commit `147bffb` are kept, because that revision still contains the file. The Kalm Issue body is not synced, so its change is in the manifest only. |

Archive links:

- Five files to move with `git mv` into `docs/archive/notes/` in the same docs PR:
  1. `docs/notes/issue12-root-cause.md`
  2. `docs/notes/physical-alias-rendering.md`
  3. `docs/notes/post-battle-whiteout.md`
  4. `docs/notes/save-storage.md`
  5. `docs/notes/title-packed-mips.md`
- Selection: the `docs/notes/README.md` labels each one 历史 (dated record of a fixed bug), each has a Done linked item, and each fix is recorded as shipped in v0.1 or v0.5.0. For `save-storage.md`, the linked `save-content-compatibility` stays Awaiting validation for the wider storage combinations; the manual-save fix itself is recorded as confirmed by the user in the note.
- Not moved: `critical-section-endian.md` (参考; it is linked from `gpu-query-pointer-corruption`, which is In Progress and states that this fix does not close it), `fire-hit-rendering.md` (the note says not fixed), `audio-output.md` (its linked audio regression is Awaiting validation), and `disc-selection.md` (its linked chapter-disc regression is In progress).

Judgment calls:

1. `issue-491` is Awaiting validation, not Done. The #454 and #477 precedent uses Done for maintainer-closed Issues, but #491 was closed by the PR's automatic reference and the reporter has not confirmed. A parent decision can change this to Done.
2. `texture-replacement-mod-loader` Release is v0.9.52, following the closure instruction. The first delivery, v0.7.0, stays in Evidence and the body.
3. The archive path is `docs/archive/notes/`, the location named in `docs/notes/README.md` and used by the archive links already in `items.json`, not the literal `docs/archive/` in the request.
4. #492 keeps Delivery In progress, because the merged PRs only reference the Issue.

Not changed:

- `docs/ROADMAP.md` and `docs/ROADMAP.zh-CN.md` lines 41 and 56 still describe the mod scope as open ("broader guest texture/model consumers ... remain"). The mod-loader item's `source_lines` points to line 206, but the roadmap has 121 lines at this commit. Not edited here; the parent or docs_sync should reconcile both language files.
- Non-pinned `docs/notes/` references that still do not resolve after the moves: `fps-0.5.4.md`, `issue12-claude-handoff.md`, `native-migration-boundaries.md`, `native-migration-first-draw.json`, `native-renderer-front-end.md` and `progressive-native-migration-plan.md` (found in neither `docs/notes/` nor `docs/archive/notes/`), and five `jitter-*-2026-10-01.md` notes that are archived but still linked from `docs/notes/`. The jitter notes are not clearly fixed and were left alone.
- `issue-14` is Status Done with Delivery In progress and an "Investigation active" Evidence. This pairing is inconsistent and was left for the maintainer.
- The Android resume fixes are not on the frame-time gate. Their issue records are `issue-491` and `issue-492`.

## Round 4: maintainer decisions of 2026-10-14

Applied in the worktree `pm-sync-20261014d` (branch `docs/pm-sync-20261014d`, base `origin/main` 88f56454). Scope: the maintainer's 2026-10-14 decisions on the v1.0.0 items, the Android acceptance, the render-pass closure, the motion replay start and the DLSS-NR and FG drafts. Nothing was committed or pushed.

Run:

- Plan before edits: `{"created": 0, "updated": 0, "unchanged": 318, "conflicts": 0, "operations": 0}`. The manifest matched the Project.
- Plan after edits: `{"created": 0, "updated": 5, "unchanged": 313, "conflicts": 0, "operations": 25}`.
- `python -B tools/project_management/sync.py --apply`: the same five records updated, exit 0. The report went to the scratchpad, not `out/`.
- Plan after the apply: `{"created": 0, "updated": 0, "unchanged": 318, "conflicts": 0, "operations": 0}`, exit 0.
- Readback (`gh project item-list 3 --owner freefrank --format json`): 319 items (318 managed records plus the keyless Issue #17 link). The five touched items matched Status, Delivery, Release, Area and their Evidence (all under 1,024 characters), and their body sections were present.
- Live checks: `perf/mv-moving-only` is at `origin/main` 88f56454 with a clean worktree. `fix/nr-preview-fg-multiplier` exists and is checked out in another worktree. No pull request for either branch was visible at sync time.
- Project: <https://github.com/users/freefrank/projects/3>

Changed records:

| Record | Before | After |
| --- | --- | --- |
| `android-frame-time-v100` | Awaiting validation / Implemented / v1.0.0 | Done / Validated / v1.0.0. Evidence and body record the 2026-10-14 acceptance under the new criterion (60.0 fps hot plus displayed intervals over 20 ms); the p95 gate is retired. |
| `render-pass-merging-v100` | In Progress / In progress / v1.0.0 | Done / Implemented / v1.0.0. Remaining phases dropped, not deferred. |
| `sr-motion-replay-moving-only` | Todo / Not started / After v1.0.0 | In Progress / In progress / v1.0.0. The 2026-10-12 "not a v1.0.0 gate" sentence stays as history and is marked superseded. |
| `dlss-nr-settings-preview-blank` | Todo / Not started / no Release / Settings | In Progress / In progress / v1.0.0 / Graphics/Settings |
| `fg-multiplier-restart-required` | Todo / Not started / no Release / Frame generation | In Progress / In progress / v1.0.0 / Graphics/Settings |

Judgment calls:

1. Render-pass merging is Delivery Implemented, not Released. PR #495 is merged to `main` but is not in v0.9.52, and Released would claim a release that does not contain it. PR #414 shipped in v0.9.32.
2. The DLSS-NR and FG request is recorded in the two existing drafts from Round 1, which came from the same 2026-10-13 note, not as a third record. The two bugs stay separate. The brief asked for "Graphics/Settings" for the item, so both drafts use that Area; the FG draft's previous Area was "Frame generation" (revert if the maintainer wants the provider area).
3. `v100-ui-separation-production-handoff` and `linux-aarch64-platform` need no field change. Both are already Paused / Deferred / After v1.0.0 in the manifest and in the Project (readback). They were not edited.
4. The full-map edge model mod has no record, as instructed. The `texture-replacement-mod-loader` Evidence still lists the 4 GiB archive limit as a pending maintainer decision from Round 3; it was not changed in this round.

Open v1.0.0 items after this round (readback, Release "v1.0.0" and Status not Done or Cancelled):

- `sr-motion-replay-moving-only`: In Progress / In progress.
- `dlss-nr-settings-preview-blank`: In Progress / In progress.
- `fg-multiplier-restart-required`: In Progress / In progress.

Roadmap: `docs/ROADMAP.md` and `docs/ROADMAP.zh-CN.md` were updated in the same change. The Android item is `[x]` and the render-pass item is `[x]`. The motion replay and DLSS-NR/FG items are `[~]` in the v1.0.0 section, and the #172 clause was removed from the later backlog in both languages.

Not verified here: the Android acceptance rests on the 2026-10-14 maintainer decision and the recorded measurements, not on a new run. No build, game or test was run in this round.
