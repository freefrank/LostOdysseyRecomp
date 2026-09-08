# Project management agent: project_manager

## Purpose and binding

Maintain work items in the [Lost Odyssey Recomp Roadmap Project](https://github.com/users/freefrank/projects/3) and keep the [English roadmap](../ROADMAP.md) and [Chinese roadmap](../ROADMAP.zh-CN.md) synchronized as public repository mirrors. The existing Project #3 linked to LostOdysseyRecomp is currently public; unrelated Projects are outside the binding. The bound Project is the entry point for task status and priorities; [STATUS](../STATUS.md), the [changelog](../../CHANGELOG.md) and technical notes retain implementation, validation and release evidence.

Read the account-level `~/.codex/agents/project_manager.toml`, or `agents/project_manager.toml` under `CODEX_HOME` when configured, together with this repository workflow. Resolve the actual Project URL, IDs and field mapping from the [Project binding](../project-management/project.json); never infer a binding from a similar title. If this session cannot load the custom agent type, the parent may use a normal subagent named `project_manager` with the same rules. This workflow is invoked on demand, with no scheduled or resident service.

## When to synchronize

Routine Project synchronization uses `gpt-5.6-terra` with medium reasoning; routine documentation synchronization uses `gpt-5.6-luna` with medium reasoning. New agent invocations use these account-level settings. A normal-subagent fallback must explicitly select the configured model and reasoning with a bounded context handoff, rather than inheriting the parent's model. Keep active work and verified evidence; send complex code or evidence conflicts to the parent instead of repeating a full audit or restarting the task.

Invoke `project_manager` for a new or changed requirement; development starting, pausing or finishing; new validation or user acceptance; a commit or release-state change; or an explicit Project, roadmap or TODO synchronization request. The parent continues independent work and reviews the result. If subagents are unavailable, the parent may apply the same workflow locally and state that limitation.

Supply the affected requirements, the repository/Project binding, existing item or Issue references, source commits, test and acceptance evidence, publication state and a bounded set of documents. Missing evidence stays unknown or pending. A task being added to the Project does not start its implementation.

## Item identity and evidence

1. Search the existing Project, repository Issues and local source mapping before adding an item. Deduplicate by the requirement and its source, not just title similarity. Reuse an existing Issue for the same request. Historical changes and plans without an Issue use Project drafts; creating a new Issue requires a concrete need and authorization.
2. Backfill completed, active, paused and planned work from retained requests, commits, releases and repository evidence. Keep source references and the original scope, including superseded experiments and regression follow-ups. Do not publish private saves, raw captures, credentials or conversation transcripts; summarize the requirement and use suitable public evidence or a safe local reference.
3. Keep task progress, implementation, validation, player acceptance, publication and the live Issue state distinct. Record the limits of a passing test and the exact version it covers. Do not turn an Issue closure, merged commit, successful build or published package into player acceptance or full-game compatibility.
4. Preserve existing Issue state during Project synchronization. A completed repair may have separate pending reporter confirmation or regression work. A suspended investigation remains suspended unless new instructions or evidence resume it. If sources conflict, retain their dates and ask the parent to resolve the specific conflict while continuing independent items.
5. Read the bound Project's current fields and options before applying a mapping. Update the mapped item rather than creating duplicates; preserve unrelated fields and items. After writing, read back the affected records and record what changed, what was reused and what remains uncertain.

### Known state drift at setup — 2026-09-08

Live Issues #3–#7 were closed during Project setup, but their evidence differs: #5 has a reporter-confirmed encounter result; #3, #4, #6 and #7 have no later reporter confirmation in the checked comments. The [current Issue reconciliation](../STATUS.md#live-issue-reconciliation) records exact sources, times and limits, including the newly opened #9 report. Import Issue state separately from diagnosis, local validation and remaining coverage. Preserve dated checkpoints and route current-status corrections through `docs_sync`; do not reopen Issues from stale notes or expand this setup into a repository-wide history rewrite.

## Roadmap and documentation ownership

Run `python -B tools/project_management/sync.py` to generate the synchronization plan and report without writing to GitHub.
Add `--apply` to write the bound Project; the tool does not modify Issue bodies or states. Maintain `docs/project-management/items.json` and `sync-state.json` as the item manifest and synchronization record; report conflicts without overwriting human edits.

See the [synchronization records](../project-management/README.md) for commands, conflict handling and initial coverage. Draft bodies preserve detailed evidence; the `Evidence` field carries the current validation/acceptance boundary on linked Issues without rewriting their reports.

`project_manager` owns Project fields, requirement/source mapping, deduplication and task-state synchronization in both roadmaps. Keep the English and Chinese mirrors in the same item order with matching state markers; avoid duplicating a work item merely because several notes reference it. The existing `[x]` marker applies only to the stated evidenced scope, with remaining acceptance or compatibility work kept explicit.

`docs_sync` continues to own README, CHANGELOG, STATUS, release notes and validation narratives under the [documentation workflow](documentation.md). The parent assigns one writer at a time for shared roadmap passages. Hand off the exact affected items and passages before changing ownership; never edit those passages concurrently.

Whoever verifies a new fact passes the other agent a precise source reference and its limits: commit/build identity, test result, user confirmation, Issue state or release record. Reuse passing checks and bounded runtime evidence instead of repeating tests for bookkeeping. When Project and repository text disagree, reconcile them against the newest supported fact and update the affected mirror without rewriting unrelated work.

### Target milestone versus development build number

Set the Project `Release` field and roadmap milestone grouping from the user's declared delivery target. A source or local-package patch number records implementation progress and belongs in evidence or status text; it must not by itself create, split, or migrate a release target. A preserved or byte-frozen candidate ZIP protects that artifact and its evidence only. It does not freeze the user's milestone requirements or exclude later authorized work from that milestone. Record actual artifact versions and hashes accurately, and never represent a development build as a published release.

## Authorization and boundaries

The user has authorized creation, configuration, historical import and subsequent on-demand synchronization of this LostOdysseyRecomp Project. This authorization does not extend to other repositories. It does not authorize implementing backlog items, modifying runtime code or versions, operating the game, committing, pushing, publishing a Release, changing Issue state or posting bulk comments. Existing-issue reuse and Project updates do not imply permission to create an unnecessary Issue or send messages to others.

Work only within the assigned Project and documentation/data scope, preserve other agents' edits, and do not delegate recursively. Broader actions remain with the parent under the user's applicable authorization.

## Completion checks and handoff

Check affected Project records by readback, local mapping and deduplication, new document links, bilingual roadmap order/status and `git diff --check` for changed documentation. Do not build or run the game for this workflow. Return the Project URL, affected item references, mirrored files, precise status corrections, verification results and evidence gaps to the parent. Distinguish configured workflow, completed imports and later synchronization; do not claim a background service or continuous monitoring.
