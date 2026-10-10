# Project reconciliation — 2026-10-10

Receipt for the v0.9.22 release record (published 2026-10-10T05:10:10Z, tag object `a34cd171` peeled to `d3888a0d`, not accepted). Manifest and documentation changes only: `items.json` and this file and the README were edited; `sync-state.json` was not edited and **`--apply` was not run**. Read-only plans were run before and after the edit (reports written outside the repository). No Project write, Issue, pull request or comment was touched, and no build or game run was made.

Live state read on 2026-10-09 and 2026-10-10: `gh release view v0.9.22` (not draft, not prerelease, Latest, five assets with digests) and `gh release list`; `git ls-remote origin` for the tag and `main` (both `d3888a0d`); `git log --first-parent v0.9.0..v0.9.22` (46 commits, PRs #331 to #386); `gh pr list --state merged` for the merge times and merge commits of the PRs cited; `git merge-base --is-ancestor` for the PRs named in the release body against `v0.9.22` (PRs #359 and #360 are not in the tag; they were removed from the body), and a check that every PR number in the body is one of the 46 commits; `gh issue view` for #151, #174, #265, #323, #332, #339, #342 and #369, with their comments; `gh project` read through `tools/project_management/sync.py` (plan mode only).

## Changes

| Key | Change | Evidence boundary |
| --- | --- | --- |
| `issue-174-surround-5-1-audio` | Status Done → Awaiting validation; Release v0.8.53 → v0.9.22; Delivery Released kept; Evidence rewritten | Reopened (maintainer comment 2026-10-09T19:45:52Z). PR #362 (v0.9.22) picks 5.1 from the WASAPI mix format for an optical-output case; PRs #365, #366 and #367 add Matrix surround output. The maintainer asked for a new log if still stereo (2026-10-10T05:13:30Z). No reporter confirmation of v0.9.22. |
| `issue-151-animated-options-menu` | Evidence only: PR #347 (v0.9.22) moves the Settings menu like the retail menus; Release stays v0.8.61 | Issue closed 2026-10-08; the change is a follow-on under the same Issue. No game run with v0.9.22. |
| `issue-369-more-language-options` | Delivery null → Released; Release null → v0.9.22; Evidence rewritten; Status In Progress kept | Experimental Windows language packs shipped in v0.9.22 (PRs #374, #378, #381, #382, #383, #384). Not delivered: a Brazilian Portuguese language in the game (the maintainer's 2026-10-09 comment: modders can add it) and font glyph additions. The Issue stays open. |
| `issue-265-upscaled-cutscenes-sharp-text` (new, linked) | Kind Feature, Area Video playback, Status Todo, Delivery Research complete, Release none | Reopened (state REOPENED; last update 2026-10-09T19:30:39Z). Only PR #345, a research note on HD and high-frame-rate CG replacement. Not scheduled. |
| `issue-323-vulkan-pipeline-build-crash` (new, linked) | Kind Bug, Area Graphics backends, Status Done, Delivery Released, Release v0.9.22 | Fixes PRs #334 and #338 shipped in v0.9.22. Closed as completed 2026-10-08T21:56:02Z. A reporter's "It worked!" was on a test build, not v0.9.22. |
| `issue-332-exclusive-fullscreen` (new, linked) | Kind Feature, Area Graphics presentation, Status Done, Delivery Superseded, Release v0.9.22 | Exclusive fullscreen is not restored (maintainer, 2026-10-08). The alternative is render-resolution supersampling, PR #335 (v0.9.22). Closed as completed 2026-10-08T19:37:36Z. |
| `issue-339-main-menu-settings` (new, linked) | Kind Feature, Area Settings, Status Done, Delivery Released, Release v0.9.22 | Closed as completed 2026-10-08T21:55:30Z, before PR #373 merged (2026-10-10T00:27:21Z). PR #373 opens Settings from the title menu. Installing content from the main menu is not delivered. |
| `issue-342-npc-culling` (new, linked) | Kind Feature, Area Graphics quality, Status Done, Delivery Released, Release v0.9.22 | PR #348 adds Settings > Graphics > Culling, 0% to 200%. Closed as completed 2026-10-09T22:54:42Z. No game run recorded. |
| `release-v0-9-22` (new, draft) | Kind Release, Area Release Engineering, Status Done, Delivery Released, Release v0.9.22, Start and Target date 2026-10-10 | Publication facts and the five assets with digests; the scope of PRs #331 to #386 grouped by area; the open acceptance items. Follows `release-v0-9-0`. |

Release field: the v0.9.22 value is set on #174, #369 and the new items, and on no other existing item. The #174 value changes from v0.8.53 for the reason in the judgment calls.

## Judgment calls

Each is a one-field change if the maintainer's convention differs.

- **#174 is Awaiting validation / Released / v0.9.22.** The Issue was reopened (state REOPENED) after the v0.8.53 delivery, and the fix for the reported case is the v0.9.22 change. `issue-114-container-enemy-softlock` is the precedent for both Status and Release: after its reopen it is Awaiting validation / Released with Release v0.8.37, the release of the fix, not v0.7.35, the first delivery. The alternative is Release v0.8.53, the first release that carried the feature, which keeps the Board column under a release the reporter did not test.
- **#151 keeps Release v0.8.61.** The brief's two instructions conflict here: "older Release → v0.9.22" against "follow how v0.9.0 and v0.8.61 items were recorded", where #172 kept v0.8.44 and #30 kept v0.8.30 after later PRs. The parent should resolve it. I followed the recorded precedent, and the v0.9.22 change (PR #347) is in Evidence. The alternative is Release v0.9.22.
- **#369 is In Progress / Released / v0.9.22.** The shipped part (experimental language packs) is released and the Issue stays open, as `texture-replacement-mod-loader` (In Progress / Released / v0.7.0) is recorded. The undelivered Brazilian Portuguese language and glyph additions are stated in Evidence. The alternative is Delivery In progress.
- **#332 is Done / Superseded.** The requested mode was declined, and the render-resolution alternative replaced it. The alternatives are Delivery Released (the alternative shipped) or Status Cancelled (the request was declined).
- **#339 is Done / Released although the Issue closed before PR #373.** The maintainer's close (2026-10-08) came before the title-menu change (2026-10-10). The Evidence says so, and that content install is not delivered.
- **#265 is a Todo research item.** It was reopened and has a research note, but no delivery. The alternative is no item until the maintainer schedules it.
- **#323 is a new Bug item.** The Issue was opened and closed after v0.9.0 and its fixes are in v0.9.22. The v0.9.0 record left it unitemized because it was still open; it is now closed and shipped.
- **Start date** is unset for the new Issue items (no date was invented) and set to the publication date for the release draft, as in `release-v0-9-0`.

## Plan results

Plan before the manifest edit (read-only, scratch report): `{"created": 0, "updated": 0, "unchanged": 299, "conflicts": 1, "operations": 0}`. The one conflict is `issue-37-portforge-integration` (previously synced item absent from the Project); it predates this record.

Plan after the manifest edit (read-only, scratch report): `{"created": 6, "updated": 3, "unchanged": 296, "conflicts": 1, "operations": 56}`, exit code 2 because of the same `issue-37` conflict.

- Created: `issue-265-upscaled-cutscenes-sharp-text`, `issue-323-vulkan-pipeline-build-crash`, `issue-332-exclusive-fullscreen`, `issue-339-main-menu-settings`, `issue-342-npc-culling` (each "link issue" with its non-null fields and Source key), and `release-v0-9-22` ("create draft" with Kind, Area, Status, Delivery, Release, Start and Target dates, Evidence and Source key).
- Updated: `issue-151-animated-options-menu` (Evidence), `issue-174-surround-5-1-audio` (Status, Release, Evidence), `issue-369-more-language-options` (Delivery, Release, Evidence).
- Conflicts: `issue-37-portforge-integration` only.

`sync-state.json` was not edited on purpose. In a plan-only handoff it must keep the values the Project holds: the conflict check compares remote values with it, and recording the new values now would hide a real change. `--apply` writes it item by item. Apply, readback and the post-apply plan are for the main agent. Expected after `--apply`: `{"created": 0, "updated": 0, "unchanged": 305, "conflicts": 1, "operations": 0}` (306 manifest items minus the still-absent `issue-37`, which remains a conflict).

## Pitfalls for the apply

- **1024-character text fields.** Every Evidence this record writes is at most 1000 characters (longest: `issue-174-surround-5-1-audio`, 993). `assembly-level-performance-analysis-tool` already sits at 1018 and was not changed.
- **Non-atomic batches.** The apply writes item by item and saves `sync-state.json` after each item. A failure can leave some fields written. Read the Project back and run the plan again before retrying; do not repeat a `item-add` for an item already linked.
- **Reopen flips Status.** #174 and #265 are in state REOPENED, with maintainer activity on 2026-10-09. The Project still held Done for #174 when planned, so no automatic flip was observed; the plan sets Awaiting validation for #174 and Todo for #265. Check the readback Status for both.
- **Linked Issues.** The new Issue items are added with `gh project item-add`, which sets no Status; the plan sets Kind, Area, Status, Delivery, Release and Evidence afterwards.

## Not changed

- `issue-37-portforge-integration`: the conflict above. It needs the maintainer's decision whether to recreate the Project item or to remove the manifest record; the tool does not recreate it on its own.
- `assembly-level-performance-analysis-tool`: Evidence is 1018 characters, under the cap but near it.
- `sr-motion-replay-moving-only` (Todo, v1.0.0): PR #331 (docs) merged after v0.9.0 and is in v0.9.22. The item records a schedule, not a delivery, so it stays.
- `texture-replacement-mod-loader` (In Progress, Released v0.7.0): v0.9.22 changes texture replacement (PRs #351, #353, #356, #361), not tracked there. Unchanged; the maintainer decides whether to extend it.
- Feature PRs without an Issue and without an item: DLSS 5 neural rendering (#341), the DLSS model choice (#370), the Mods page (#384) and the Mod Organizer 2 plugin (#344). The v0.9.0 record created a draft for a feature PR without an Issue (#326). Not created here, because the brief asked for Issue-linked records and no item for these was requested; the maintainer decides whether to add drafts.
- Tooling, documentation and test PRs in the range (for example #336, #354, #355, #363, #380, #371, #350, #345 beyond the #265 item) have no item. They stay in the release record and in CHANGELOG.md.

## Evidence gaps

- No game run with any v0.9.22 package. The five assets were not downloaded, unpacked or verified against the digests here.
- The Gitea Actions run for v0.9.22 was not read.
- The PR bodies of the cited PRs were not read; the Evidence uses commit subjects, merge times and the maintainers' comments.
- The reporter's reply to the reopened #174 is pending. The crash log attached to #323 was not read.
- The #342 zoom case and the #332 DLSS, FSR and XeSS supersampling were not checked on any machine in this record.

## Applied

The maintainer ran `python -B tools/project_management/sync.py --apply` from this worktree on 2026-10-10. Its report: `{"created": 6, "updated": 3, "conflicts": 1, "operations": 56}`, the conflict being `issue-37-portforge-integration` as above; `sync-state.json` was rewritten by the run. The plan run afterwards reported `{"created": 0, "updated": 0, "unchanged": 305, "conflicts": 1, "operations": 0}`.

Readback (`gh project item-list 3 --owner freefrank --format json`, 306 Project items): `release-v0-9-22` Done / Released / v0.9.22; `issue-323-vulkan-pipeline-build-crash`, `issue-339-main-menu-settings` and `issue-342-npc-culling` Done / Released / v0.9.22; `issue-332-exclusive-fullscreen` Done / Superseded / v0.9.22; `issue-265-upscaled-cutscenes-sharp-text` Todo / Research complete / no Release; `issue-174-surround-5-1-audio` Awaiting validation / Released / v0.9.22; `issue-369-more-language-options` In Progress / Released / v0.9.22; `issue-151-animated-options-menu` Done / Released / v0.8.61. Every Evidence field read back at 440 to 993 characters.

## Scope decisions — 2026-10-10

The maintainer made two v1.0.0 scope decisions on 2026-10-10. This section applies them to the manifest, the Project and both roadmaps. Only `items.json`, `sync-state.json` (rewritten by the apply), the two roadmaps, this file and the README were changed. No Issue, pull request, comment, code or build was touched, and no game was run. The research notes behind the decisions are local (`out/v100-research`) and are not in the repository.

### Changes

| Key | Change | Evidence boundary |
| --- | --- | --- |
| `v100-ui-separation-production-handoff` | Release `v1.0.0` → `After v1.0.0`; Status Paused and Delivery Deferred kept; the `v1.0.0:` title prefix removed; a `## Status — 2026-10-10` section added before `## Sources` | A scope decision only: no implementation or validation. The 18–30 day (full) and 6–10 day (HUDless) figures are research estimates, not measurements. |
| `android-frame-time-v100` (new draft) | Kind Feature, Area Graphics performance, Status Todo, Delivery Not started, Release v1.0.0, dates unset; body from the research note with the scope narrowed to the three named changes; Evidence 744 characters | Scheduled for v1.0.0, not started. The TB321FU baseline has not been measured. |

Roadmaps (English and Chinese): the HUDless line was removed from the v1.0.0 section and added to the backlog paragraph; an Android frame-time line was added to the v1.0.0 section, linking the Project.

### Judgment calls

- **Title prefix removed** from the HUDless item. The `v1.0.0:` prefix marks items for v1.0.0, and `linux-aarch64-platform` has no prefix after its move to After v1.0.0. The alternative is to keep the prefix.
- **Status section before Sources**, so Sources stays last, as in `loading-and-save-speed-2026-10-10`. The brief said "end of body".
- **Android scope.** `fast-linked pipeline replacement` was removed from the completion criteria, and the three named changes stay. The adaptive-occlusion check and the optional OPPO pair were kept as measurement and validation criteria, not optimization scope. If "范围只包括" is read strictly, both can be removed.
- **Android Evidence** keeps the draft text with the final "Not started." followed by the schedule sentence "Scheduled for v1.0.0 by the maintainer on 2026-10-10." Length 744 characters.
- **Chinese roadmap** uses 挪到, the verb already in that paragraph, not 移到.
- **Source URLs** for the Android item are the pull requests from the draft. The annotation on pull request 268 was dropped from the URL field; the body keeps it.

### #174 brought in line with the Project

`issue-174-surround-5-1-audio` was the second conflict in the plan results below: the Project Status read `Done`, while the manifest and sync state held `Awaiting validation` from the v0.9.22 record. Issue #174 is CLOSED as COMPLETED at 2026-10-10T08:30:58Z, and the reporter wrote "Works perfectly now, thanks!" at 2026-10-10T09:26:16Z, after the maintainer's v0.9.22 reply. The close moved the Project item to Done. As with the earlier stale Status records, the manifest followed the remote value: Status `Done` in `items.json` and in the tracked `sync-state.json` value, and the Evidence tail now records the close and the reporter's reply (1000 characters). Plan: created 0, updated 1, unchanged 306, conflicts 1 (`issue-37` only), operations 1 (Evidence). Apply wrote it; the re-plan reported created 0, updated 0, unchanged 307, conflicts 1, operations 0. Readback: "[Feature] 5.1 Audio" Done / Released / v0.9.22, Evidence 1000 characters.

### Plan results

- Baseline before edits: created 0, updated 0, unchanged 306, conflicts 2 (`issue-37-portforge-integration`, `issue-174-surround-5-1-audio` Status), operations 0.
- Plan after edits, before apply: created 1, updated 1, unchanged 305, conflicts 2, operations 10. The operations were: HUDless `update draft text` and Release; Android `create draft` and seven fields.
- Apply: created 1, updated 1, unchanged 305, conflicts 2, operations 10, exit code 2 because of the two conflicts only.
- Re-plan after apply: created 0, updated 0, unchanged 307, conflicts 2, operations 0, exit code 2 (the same two conflicts).

### Readback

`gh project item-list 3 --owner freefrank --limit 400`, 308 items.

- `v100-ui-separation-production-handoff`: title "Deliver production HUDless/UI separation handoff"; Paused / Deferred / After v1.0.0; Infrastructure; Temporal rendering; the body contains the 2026-10-10 Status section.
- `android-frame-time-v100`: title "v1.0.0: Faster Android frames with the same image"; Todo / Not started / v1.0.0; Feature; Graphics performance; Evidence 744 characters; the body contains the schedule line and no fast-link wording.

The Project holds 308 items: the 307 managed items (`issue-37` absent) plus the keyless Issue #17 link.

### Count correction

The README said 306 managed items. The baseline plan showed 307 before this change; `loading-and-save-speed-2026-10-10` (#389) had been added to the manifest without updating that count. The manifest now holds 308 items, and the README was corrected to 308 and 307 present in the Project.

### Evidence gaps

- No build and no game run. The Android baseline on the TB321FU has not been taken.
- The 18–30 and 6–10 day figures and the Android timing targets are estimates, not measurements.
- The research notes are local and are not in the repository.