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