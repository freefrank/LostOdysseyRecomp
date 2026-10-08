# Project reconciliation — 2026-10-08

The maintainer closed three Issues as completed on 2026-10-07 and set their Project Status to Done by hand. The next plan reported three Status conflicts (remote `Done`, manifest `Todo` / `Awaiting validation` / `In Progress`). Following the README, the manifest takes the remote value; nothing on the Project was overridden.

| Item | Issue | Change | Evidence boundary |
| --- | --- | --- | --- |
| `issue-167-cutscene-audio-desync` | [#167](https://github.com/freefrank/LostOdysseyRecomp/issues/167), closed 2026-10-07 19:28:19 UTC | Status Done; Delivery `Not started` → `Research complete`; Evidence updated | A reporter said the drift was not extremely bad and the Japanese voice lip sync is wrong by design; the maintainer asked for a new Issue if the sync is worse than it should be. No runtime change. |
| `issue-214-mali-black-screen` | [#214](https://github.com/freefrank/LostOdysseyRecomp/issues/214), closed 2026-10-07 19:27:08 UTC | Status Done; Evidence updated (Delivery Released, Release v0.8.37 unchanged) | PR #241's BC decode fallback; no Mali device was run and no reporter confirmation is recorded. |
| `issue-172-fsr-scaling-performance` | [#172](https://github.com/freefrank/LostOdysseyRecomp/issues/172), closed 2026-10-07 22:59:34 UTC | Status Done; Evidence updated (Delivery Released, Release v0.8.44 unchanged) | Adds PR #305 (merged 2026-10-08, not yet released); the reporter was asked on 2026-10-08 to rerun after the next release. No reporter confirmation is recorded. |

Release keeps the first release that carried the delivery; the unreleased #305 is in the #172 Evidence only.

## Apply and readback

- Before: `{"created": 0, "updated": 0, "unchanged": 291, "conflicts": 3, "operations": 0}`. `sync-state.json` Status for the three items was set to the remote `Done`.
- Plan after the manifest edit: 3 items, 4 operations (#167 Delivery and three Evidence fields), 0 conflicts. Applied.
- Plan after apply: `{"created": 0, "updated": 0, "unchanged": 291, "conflicts": 0, "operations": 0}`.
- Readback with `gh project item-list 3 --owner freefrank`: #167 Done / Research complete / no Release (Evidence 546 characters); #172 Done / Released / v0.8.44 (762); #214 Done / Released / v0.8.37 (768).

## v0.8.61 release

Receipt for the v0.8.61 release record (published 2026-10-08T08:33:11Z, tag commit `787fed28`, not accepted). Manifest and documentation changes only; `sync-state.json` was not edited and **`--apply` was not run**. A read-only plan was run for this record (its report was written outside the repository). No Issue, pull request or comment was touched, and no build or game run was made.

Live state read on 2026-10-08 between about 08:35 and 16:35 UTC: `gh release view v0.8.61` (not draft, not prerelease, five assets with digests) and `gh release list` (Latest); `git ls-remote origin` for the tag object `4d83681b`, the tag commit and `main` (`5d1958b4`, one file removed after the tag); `gh pr view` for the 34 PRs among #283 to #322 (#284, #286, #287, #291 and #319 are missing or deleted, #307 is an Issue) and `gh pr list --search merged:...` for the window (35 PRs with #281, none merged after publication), each merge commit checked against the tag with `git merge-base --is-ancestor`; `gh issue view` for #30, #48, #151, #172, #275, #282, #307 and `gh issue list --state open`; the Gitea API through `tea api` for runs 504 (API id 896) and 500 (API id 892) with their jobs, and the log of the Linux job of run 896 ("Check published shader packs"); `gh release view shader-packs` (asset names, sizes and upload times); `gh project item-list 3 --owner freefrank`. The release body was compared with the output of `tools/release/extract_release_notes.py` and differs only in a trailing newline.

| Key | Change |
| --- | --- |
| `release-v0-8-61` (new) | Kind Release, Done / Released / v0.8.61, 2026-10-08. Publication facts, runs 504 and 500, the five assets with sizes and SHA-256 values, the macOS image checks, the PR groups, the changed pack contracts and the open acceptance items. The shape follows `release-v0-8-53` |
| `issue-151-animated-options-menu` | Todo / Not started → Done / Released / v0.8.61 (the Project already held Done for Status). Evidence rewritten (PR #317, closed with the merge) |
| `issue-48-motion-sickness-qol` | Todo / Not started → Done / Released / v0.8.61 (the Project already held Done: the maintainer closed the Issue by hand at 08:40:14Z, after the release comment). Evidence says the head-bob toggle is not delivered |
| `issue-275-button-prompt-style`, `issue-282-startup-black-screen`, `issue-307-flickering-boat` (new) | Linked Issues, Done / Released / v0.8.61, built like `issue-253-android-ctrl-movable` |
| `sdl3-runtime-migration` (new) | Draft without an Issue for PR #289 (Kind Infrastructure, Area Runtime), Done / Released / v0.8.61, with a Chinese body and the maintainer's 2026-10-07 platform runs in the Evidence |
| `issue-172-fsr-scaling-performance` | Evidence only: PR #305 shipped in v0.8.61; Release stays v0.8.44 |
| `issue-30-dof-toggle-slider` | Evidence only: the Motion blur and Dynamic shadows switches of PR #318; Release stays v0.8.30 |
| `anti-aliasing-consolidation-remove-experimental-taa` | Evidence only: v0.8.61 drops the "(Experimental)" label from TAA (PR #321); the standalone TAA option remains |

All Evidence fields are under 1,000 characters (the new and changed ones; two older items already sit at 1,007 and 1,018).

Judgment calls, each a one-field change if the maintainer's convention differs:

- **#282 is Released / v0.8.61.** The reporter's fix was to empty the NVIDIA shader-cache folders and the Issue was closed before PR #300 merged; what shipped is the diagnostics. The alternative is Research complete with no Release, as for #167.
- **#48 is Done / Released / v0.8.61 although half of the request is open.** The maintainer closed it as completed; the Evidence names the missing head-bob toggle. The alternative is Delivery Implemented.
- **`sdl3-runtime-migration` is Kind Infrastructure.** The PR is a `feat:` and the Changelog lists the change for players, so Feature is the alternative (one field).
- **Releases of #172 and #30 are unchanged** (v0.8.44 and v0.8.30): the Release field keeps the first release that carried the delivery, as in the earlier receipts.
- **Not itemized.** #265 (closed 2026-10-07 at 19:27:18Z, no code change), the adaptive occlusion culling of PR #316, the render-thread and GPU trims, the Metal depth-only change (PR #306), the SPIR-V address changes (PRs #313 and #315), the Discord, Ko-fi and triage tooling, and the documentation PRs have no item of their own; they stay in the Changelog, STATUS and the release record.
- **Shader packs.** The release record states that the contracts changed since v0.8.53 and that the new packs were published on 2026-10-08 at 03:52 UTC, before the tag. The release item says the release itself published none.

Evidence gaps, kept open: no game run with any v0.8.61 package and no download of the four CI packages; the draft deletion and tag move come from the maintainer, and the cause of the first Linux failure from the message of `787fed28` (the failed job's log was not read); the Mac `publish_shader_packs.py --check` result is the maintainer's report; the maintainer's comments on #30, #151, #275 and #48 were read, not the reporters' reactions.

Opened after the release: [#323](https://github.com/freefrank/LostOdysseyRecomp/issues/323) ("Crashing when building Pipeline on Vulkan", 2026-10-08 at 16:14 UTC; the reporter writes that it only happens on "today's build"). It has no item and was not triaged for this record.

Plan (read-only, from the worktree at about 16:25 UTC): `{"created": 5, "updated": 5, "unchanged": 286, "conflicts": 1, "operations": 55}`. Created: the three Issue items, `sdl3-runtime-migration` and `release-v0-8-61`. Updated: `issue-151-animated-options-menu` (Delivery, Release, Evidence), `issue-48-motion-sickness-qol` (Delivery, Release, Evidence) and Evidence only for the three others. The one conflict is not from this change: `issue-167-cutscene-audio-desync` Status, Project `In Progress` against manifest `Done` (the previous section of this file wrote Done to the manifest after reading Done back, and #167 was closed as completed on 2026-10-07 at 19:28:19Z and never reopened). Someone set the Project value after that readback. It needs the maintainer's decision before `--apply`: either set Done on the Project, or copy `In Progress` into `items.json` and `sync-state.json`. The Status of #151 and #48 matches the Project (Done), so `sync-state.json` needs no hand edit.

Apply, readback and the README item count (296 in the manifest after this change; the README still says 290) are for the maintainer's session.

## v0.9.0 release

Receipt for the v0.9.0 release record (published 2026-10-08T18:31:57Z, tag commit `8d33c146`, not accepted). Manifest and documentation changes only; `sync-state.json` and the Project README were not edited and **`--apply` was not run**. A read-only plan was run (its report was written outside the repository). No Issue, pull request or comment was touched, and no build or game run was made.

Live state read on 2026-10-08, the last read at about 18:47 UTC: `gh release view v0.9.0` (not draft, not prerelease, five assets with digests) and `gh release list` (Latest); `git ls-remote origin` for the tag object `83c48bb5` and the tag commit; `git log --first-parent v0.8.61..v0.9.0` (nine commits) with `git merge-base --is-ancestor` for #324 to #329 (all in the tag) and #331 (not in the tag: merged 18:28:56Z, after the tag push); `gh pr list --state merged`; `gh issue view` for #172, #323, #330 and #332 and `gh issue list --state open`; the Gitea API through `tea api` for run 511 (API id 904), its jobs and the logs of the Linux and publish jobs; `gh release view shader-packs`. The release body was compared with `tools/release/extract_release_notes.py` and differs only in a trailing newline.

| Key | Change |
| --- | --- |
| `release-v0-9-0` (new) | Kind Release, Done / Released / v0.9.0, 2026-10-08. Publication facts, run 511, the five assets with sizes and SHA-256 values, the macOS image checks, the PRs and the open acceptance items. The shape follows `release-v0-8-61` |
| `aspect-ratio-setting` (new) | Draft without an Issue for PR #326 (Kind Feature, Area Graphics presentation), Done / Released / v0.9.0, with a Chinese body |
| `issue-172-fsr-scaling-performance` | Evidence only: the reporter's rerun on v0.8.61 (no change beyond the margin of error) replaces "no reply is recorded", and the v1.0.0 plan is named; Release stays v0.8.44 |
| `ultrawide-fov-layout` | Evidence only: PR #326 replaces the Widescreen switch; Release stays v0.6.7 |

All Evidence fields are under 1,000 characters (the new and changed ones).

Judgment calls, each a one-field change if the maintainer's convention differs:

- **#326 is Done / Released, not Awaiting validation**, as the v0.8.61 items were, although the in-game Save path was not run; the Evidence and the body say so.
- **#331 is outside the release.** It merged after the tag, and its item `sr-motion-replay-moving-only` (Todo, v1.0.0) is unchanged.
- **Not itemized.** #323, #330 and #332 (open Issues, opened on 2026-10-08), the triage and bug-template changes (#327, #328), the documentation and Project PRs (#324, #325, #329) and the deletion of `HANDOFF.md`. They stay in STATUS and the Roadmap.

Evidence gaps, kept open: no game run with any v0.9.0 package and no download of the four CI packages; the hold on publication (the `HOLD.txt` asset, removed before publishing), the manual `gh release edit` and the Mac-side checks (build time, VALID, signature, bundle version, equal digests, `publish_shader_packs.py --check`) are the maintainer's session account (the publish job's log shows the package-set assertion failing, not the asset name); the crash log attached to #323 was not read.

Plan (read-only, from the worktree at about 18:45 UTC): `{"created": 2, "updated": 2, "unchanged": 295, "conflicts": 0, "operations": 21}`; the plan before the edit was `{"created": 0, "updated": 0, "unchanged": 297, "conflicts": 0, "operations": 0}`. Created: `release-v0-9-0` and `aspect-ratio-setting`. Updated: Evidence of `issue-172-fsr-scaling-performance` and `ultrawide-fov-layout`. Apply, readback and the README item count (297 now, 299 after the apply) are for the maintainer's session.
