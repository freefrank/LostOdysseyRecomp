# Developer-tool recovery record — 2026-09-24

This note records the bounded recovery inventory and cleanup outcome for reusable Codex and developer-tool material. The tracked migration and registered old worktree cleanup are complete; two empty scratch roots remain as documented residue.

## Recovered candidates

The private feedback archive source was migrated into the repository-owned `tools/feedback_archive/` layout. The maintained archive and ledger scripts, 47 synthetic checks, skill references, and compact diagnostic references are now documented there. Remote access remains explicitly configured and read-only for collection, separate from any private repository publication. Real feedback, game data, credentials, deployment identifiers, and private fixtures are outside the migration.

| Sanitized source material | Maintained destination | Boundary |
|---|---|---|
| Private archive collector and ledger | `tools/feedback_archive/scripts/` | Explicit private archive and deployment parameters; no embedded account or database identifiers. |
| Synthetic archive and ledger checks | `tools/feedback_archive/tests/` | Synthetic SQLite and diagnostic inputs only; no real feedback or private fixtures. |
| Triage skill, review rules, and compact diagnostic guidance | `tools/feedback_archive/skills/lo-feedback-triage/` | Review state stays separate from implementation, validation, and acceptance. |
| Render investigation skill and agents | `tools/opencode/` | Repository-owned OpenCode source installed into ignored `.opencode/`; no private session state migrated. |

The repository-owned OpenCode render workflow is already documented under [`tools/opencode/README.md`](../../tools/opencode/README.md). Capture analysis utilities remain under [`tools/capture_analysis/README.md`](../../tools/capture_analysis/README.md).

## Historical Git metadata incident

An earlier synchronization incident overwrote Git metadata and was recovered to a verified commit after isolating backups. The original handoff records the historical recovery directory and handling constraints in [`v0.5.0-rendering-handoff-2026-09-09.md`](v0.5.0-rendering-handoff-2026-09-09.md). That private source remains evidence, not current repository state; this note intentionally omits personal machine paths, device identifiers, credentials, and old draft instructions.

The nested private clone backup is preserved as `out/codex-tool-recovery/backup/build-inputs-d4feb179.bundle`. Its refs `HEAD` and `origin/main` were verified during the inventory, and the bundle verification succeeded again at closeout. Seven dirty documentation files and fifteen ignored historical report/script copies (22 files total) were byte-checked against their source, with a reverse-check patch recorded for the migrated set; the final hashes matched. The main PPC directory remains in place.

## Cleanup outcome

The old `lo-v050-release-9a69dc1` and `lo-issue-triage-main` scratch worktrees were removed with force and their Git registrations were cleared. The removal commands ended with `Permission denied` after leaving empty root directories; `Get-ChildItem` confirmed both roots are empty. A follow-up `Remove-Item` for those empty directories was rejected by automatic approval policy, with no bypass attempted. Those two empty roots are retained as cleanup-incomplete residue; no worktree contents or registrations remain.

The global `lo-feedback-triage` broken junction was safely repaired to the repository-owned `tools/feedback_archive/skills/lo-feedback-triage` source. The unrelated OpenCode legacy/Windows 7 worktree, private feedback archive, historical Git metadata recovery, runner SDK, and main PPC directory remain intentionally preserved.

## Current limits

Obsolete installer files in a runner checkout remain historical and are not a migration source; the maintained feedback archive and OpenCode workflow are the repository-owned replacements. Existing roadmap and project-management files may be concurrently modified and are not part of this recovery note.
