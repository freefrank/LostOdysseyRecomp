# Completed history coverage

Snapshot: 2026-09-08. Canonical branch: `taa-fix`; HEAD: `147bffb214cd3650f3eadb810e86f7237511ac23`.

## Deliverable and scope

The initial history inventory contributes 77 records to the consolidated [manifest](items.json): 29 Bug, 25 Feature, 12 Infrastructure, 3 Research and 8 Release. The 69 non-release records describe distinct delivered behavior or bounded investigations; the eight release records describe publication milestones. Pending roadmap requirements belong to the separate pending inventory.

The review covered all 91 commits reachable from HEAD, all eight local release tags, the complete current CHANGELOG, and the relevant STATUS, WORK_REPORT and notes for rendering, runtime/recompiler, input, audio, storage, setup, diagnostics, shader preparation, tests and release delivery. The canonical history spans 2026-09-03 through 2026-09-08 in UTC.

`git log --all` contains 135 commits. The 44 additional commits on the pre-public-cleanup archive each match a canonical HEAD-history commit by exact author timestamp and subject (44/44). They are treated as rewritten historical counterparts, not 44 additional tasks. This is semantic history deduplication, not a claim that old and sanitized trees or commit objects are byte-identical. No additional unique feature milestone was found among those entries.

## Granularity and deduplication

- Group small commits that implement one user-visible result: for example controller initialization/hotplug, XMA command/cursor/continuation fixes, and importer/setup iterations. Preserve independently meaningful results such as manual-save overwrite, optional save-anywhere, the Ring combat resource and encounter animation.
- Separate distinct rendering faults with different evidence: character index geometry, postprocess format/color handling, physical aliases, stencil/depth lighting, post-battle whiteout, title packed mips, Map12 poster depth, the Map3 tire and six battle terrain paths.
- Separate infrastructure from its later behavior: shader discovery versus recorded pipeline precreation; render-state export versus three-frame process logs versus background ZIP creation versus default-log retention.
- Record bounded research separately from production fixes: vendor temporal-upscaling feasibility, the four-disc TAA contract scan and the enemy-disappearance capture diagnosis do not assert implementation of their remaining recommendations.
- Keep release milestones for delivery tracking without replacing individual feature/fix records. For records covering an evolving feature, `release` identifies the release containing the stated combined scope. Importer and first-launch records therefore reference their v0.1 introduction and v0.2 extension rather than claiming the entire feature first appeared in v0.2.
- Use stable English keys independent of date, title wording and GitHub issue number, so later synchronization can update the same item.

## Source and date rules

Current explicit publication evidence in CHANGELOG and the latest STATUS release sections takes precedence over older development snapshots. In particular v0.4.2 is published; old sections that still say local-only or unpublished describe earlier checkpoints. Historical candidate executable hashes and validation limitations remain historical evidence and are not relabeled as new tests of the official package. The stale global CHANGELOG checked-through footer does not override its dated v0.4.2 section.

Implementation/research dates use the UTC calendar dates of the earliest and latest cited work commits. These are evidenced checkpoints, not inferred dates of the original request, continuous work duration or user acceptance. Release rows use the tagged source checkpoint and the documented publication date. Human notes may show the preceding local date. Unknown dates must remain null; no date is inferred from task order or a desired milestone.

The release dates below come from current repository publication records. This subtask did not perform live GitHub API verification or mutate GitHub Projects, issues or releases. It incorporated the parent/docs_sync live Issue reconciliation, now recorded at `docs/STATUS.md#live-issue-reconciliation`, with its exact comment references; that newer evidence overrides stale open/pending-save statements.

| Release | Tagged source commit | Recorded publication date (UTC) |
| --- | --- | --- |
| v0.1 | `2a3ffccc36cc` | 2026-09-06 |
| v0.2 | `dcc946299cdc` | 2026-09-06 |
| v0.2.1 | `906d7c039f7e` | 2026-09-06 |
| v0.2.2 | `f03efe370d44` | 2026-09-07 |
| v0.3.0 | `fba7ae4f3cf8` | 2026-09-07 |
| v0.4.0 | `40362d78285d` | 2026-09-07 |
| v0.4.1 | `eb43f108d2cd` | 2026-09-08 |
| v0.4.2 | `2ed7a2d658bc` | 2026-09-08 |

There is no local v0.1.1 release tag or corresponding publication record; no such milestone was invented.

## Commit coverage accounting

84 of the 91 canonical commits are cited directly by at least one item. Every implementation commit is represented. The following seven commits do not require another independent completed-work record:

| Commit | Disposition |
| --- | --- |
| `e4a9172` | Suspended Kaim-shadow investigation checkpoint; excluded from completed research and left to the pending inventory. The later accepted Map3 tire fix has its own evidence. |
| `397a53f` | v0.2.2 release preparation folded into its final tagged release and publication records. |
| `1dd836a` | Code-checkpoint and pending-validation summary; underlying delivered behavior appears in the corresponding fix records. |
| `20be539` | Repair-outcome handoff consolidation; underlying delivered fixes are represented individually. |
| `116d895` | Early title-renderer status note; renderer foundation and later fault-specific fixes carry the result. |
| `d0ffced` | Translator/backend design handoff; implemented translator and renderer foundations carry the result. |
| `2de7d7f` | Early draw statistics/renderer-route note; diagnostic and renderer foundations carry the result. |

## Status and acceptance interpretation

All records in this completed-history inventory use `workflow_status: Done` for their explicitly bounded delivery. Draft bodies retain `delivery`, `validation`, `acceptance_status`, `acceptance_boundary` and `record_scope`; linked Issues use the Project's `Evidence` field for the current boundary, with full details retained in the consolidated manifest. Issue bodies are not rewritten. Done does not authorize closing a linked issue or declaring a broader scene, device or whole game fixed.

Specific distinctions that must survive import:

- Unicode path handling is implemented and released. Issue #4 is closed following the maintainer repair statement; the complete reported crash was not reproduced and no subsequent reporter confirmation is recorded.
- Issue #5 has two delivered indirect-entry corrections with targeted checks. The reporter supplied the USA/Europe Lv10 ValleyRoad save and confirmed that the same encounter passed after the boundary repair (2026-09-07 22:25 UTC); the Issue is closed. This supersedes the old missing-save/pending-retest checkpoint. Only broader encounter, edition and exact official-package regression remains separate.
- Issue #6 is closed after the maintainer stated v0.4.2 fixes it. The delivered allocation diagnostics do not themselves establish original-machine recovery or its root cause; no post-release reporter log or retest is recorded.
- Issue #7 has a delivered Council switch correction and PPC repairs, candidate-scene traversal/save/reload evidence and native crash diagnostics. Issue #7 is closed after the maintainer repair/release explanation; reporter acceptance and a full-playthrough guarantee are separate. The official v0.4.2 artifact received package and focused fixture verification, not a repeat of the entire game/import/startup flow.
- The six battle TAA paths passed their documented CPU and captured Map3 save-loaded encounter checks. The continuous capture and resolved TAA output prove the recorded bounds; they do not supply a missing temporal EndFrame/history-gap summary. Player confirmation of this exact terrain result remains distinct from the earlier user-accepted Map3 tire fix.
- The enemy-disappearance capture review is a completed diagnosis of three exported frames. It is not a new disappearance fix or continuous 32-phase runtime proof.
- Original accepted AMD partial-resolve work remains separate from later AMD reports and suspended hypotheses. A completed local investigation or NVIDIA control does not validate all AMD hardware.
- The background ZIP test demonstrates continued rendering during compression; it does not eliminate the capture readback pause. Log retention covers the documented Windows fixtures and user path; POSIX execution was not performed.

Existing issue links are association evidence. Issues #4, #5, #6 and #7 are already closed; their closure and reporter acceptance are recorded separately. Imported implementation items must not automatically change those tracker states. Ongoing regression, investigation or reporter-acceptance work belongs in distinct pending items linked back to the completed delivery.

## Boundaries and unknowns

This inventory covers repository-recoverable history, not every private conversation, deleted/unreachable commit, unsaved request or undocumented experiment. Current uncommitted handoff files, project-management scaffolding and backend proposals were not treated as historical releases. There was no build, game run, release upload or application mutation in this task.

Pending items are intentionally not represented as completed work: alternate graphics backends/platforms, Vulkan/DX11/Linux/Steam Deck/Switch, IME, remaining scene effects and GPU waits, unresolved shader cases and runtime pipeline gaps, native object-motion inputs, vendor upscalers/frame generation/HDR/120 FPS, paused language work and suspended rendering hypotheses. The separate pending inventory owns their detailed scope. The unsuccessful/suspended Gitea Actions route is not represented as an operational release pipeline; the recorded successful release path is GitHub Actions.

Some earliest foundation behavior has only generation/build, a bounded scene or qualitative player evidence. Such limits are retained rather than replaced with later broad compatibility claims. No whole-game correctness or exhaustive hardware coverage is implied.

## Inventory checks

The JSON was parsed; all 77 stable keys were unique; dates were ordered; source commit references resolved; referenced repository documents and heading anchors were checked. All 44 archive counterparts reconciled by author timestamp and subject. These are inventory integrity checks, not new validation of game behavior.
