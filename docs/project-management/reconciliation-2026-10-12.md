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
