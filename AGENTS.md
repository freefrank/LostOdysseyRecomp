# Repository agent workflow

## Documentation agent

Use an on-demand subagent named `docs_sync` for documentation synchronization after a feature or fix is accepted, and before a requested commit or release when behavior, validation, or publication status has changed. Also invoke it for explicit documentation synchronization requests. Small typo-only edits can be handled directly.

Read `docs/agents/documentation.md` and give the subagent the relevant change summary, test evidence, user acceptance, release state, and a bounded list of files to edit. The parent agent continues independent work and reviews the documentation diff before completing the task. If subagents are unavailable, perform the same workflow locally and state that limitation.

This is a repository workflow, not a scheduled or continuously running service. The documentation subagent must not delegate recursively. It does not grant permission to commit, push, publish, or operate the game.
