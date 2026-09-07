# Repository agent workflow

## Documentation agent

Use an on-demand subagent named `docs_sync` for documentation synchronization after a feature or fix is accepted, and before a requested commit or release when behavior, validation, or publication status has changed. Also invoke it for explicit documentation synchronization requests. Small typo-only edits can be handled directly.

Read `docs/agents/documentation.md` and give the subagent the relevant change summary, test evidence, user acceptance, release state, and a bounded list of files to edit. The parent agent continues independent work and reviews the documentation diff before completing the task. If subagents are unavailable, perform the same workflow locally and state that limitation.

This is a repository workflow, not a scheduled or continuously running service. The documentation subagent must not delegate recursively. It does not grant permission to commit, push, publish, or operate the game.

## Tests and cleanup

Choose proportionate checks that verify meaningful behavior; do not add tests for reversible, low-impact edits or tests that merely restate the implementation. Use the selected suites in `tools/tests/README.md`; do not implicitly build or run everything. Once relevant checks pass, repeat or broaden them only for new changes, failures or unresolved concerns. Keep test CI separate from release packaging. Before handoff, remove only disposable files created by the current task, preserving evidence needed for review, user data and unrelated work.
