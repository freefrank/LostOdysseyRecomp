# Project reconciliation — 2026-10-12

Receipt for the Project apply that follows the docs archive (#430). That PR moved outdated notes and handoffs to `docs/archive/` and pointed the `items.json` draft bodies at the new paths; this run copied those bodies to the Project. No field value, Issue, pull request or comment changed.

## Run

From `main` at `90a7aa3c`:

- Plan: `{"created": 0, "updated": 92, "unchanged": 216, "conflicts": 1, "operations": 92}`. All 92 operations were `update draft text`.
- `python -B tools/project_management/sync.py --apply`: same counts, exit code 2 because of the conflict below. `sync-state.json` was rewritten by the run (92 body hashes).
- Plan after the apply: `{"created": 0, "updated": 0, "unchanged": 308, "conflicts": 1, "operations": 0}`.
- Readback (`gh project item-list 3 --owner freefrank --format json`): 309 items; all 231 draft bodies equal their `items.json` bodies.

## Open conflict

`issue-37-portforge-integration` is still absent from the Project, as in the [2026-10-10 reconciliation](reconciliation-2026-10-10.md). Issue #37 itself no longer exists (`gh api repos/freefrank/LostOdysseyRecomp/issues/37` returns 404). The tool does not recreate it; the maintainer decides whether to drop the record or keep it as a draft.

## Removed record

The maintainer chose to remove `issue-37-portforge-integration`: the record and its `sync-state.json` entry are gone, and the roadmap line no longer cites Issue #37. The PortForge manifest itself stays and is updated with each release.

Plan after the removal (read-only; no Project write needed): `{"created": 0, "updated": 0, "unchanged": 308, "conflicts": 0, "operations": 0}`, exit code 0.

## v1.0.0 scope decisions (later on 2026-10-12)

The maintainer narrowed v1.0.0 to one gate:

- `android-frame-time-v100` stays in v1.0.0, now In Progress. Its acceptance changed to a hot frame-interval p95 at or below about 18 ms at the factory 720p in hot Uhra on the TB321FU ("p95 60fps其实也够了"); p99 is no longer the gate. PRs #443, #444, #445 and #447 shipped in v0.9.32; hot Uhra measures p95 21.3-22.6 ms.
- `render-pass-merging-v100` moved past v1.0.0 (Paused / Deferred): the phase 0 measurements (PR #405) found no on-chip gain worth the change.
- `sr-motion-replay-moving-only` (#172) is not a v1.0.0 gate (Release `After v1.0.0`); it may still land earlier.
- Two stale Issue records were aligned with the Project: #369 and #265, both closed as completed by the maintainer that day, now Done.

Plan: `{"created": 0, "updated": 5, "unchanged": 303, "conflicts": 0, "operations": 14}`; applied with the same counts, exit code 0. Plan after the apply: `{"created": 0, "updated": 0, "unchanged": 308, "conflicts": 0, "operations": 0}`. The English and Chinese roadmaps carry the same change.
