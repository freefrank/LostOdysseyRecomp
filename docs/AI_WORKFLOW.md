# AI Collaboration Workflow for the Recomp

[简体中文](AI_WORKFLOW.zh-CN.md)

Date: 2026-10-18

This guide summarizes how the Lost Odyssey recompilation port (LORecomp) was developed from September to October 2026. During that time, one maintainer developed the project with AI coding assistants. Claude Code ran the main session. Codex CLI and several subagents did the specific work. This guide covers the process, the subagent rules, and the pitfalls to avoid when porting other Xbox 360 games. For engineering trade-offs, see the [engineering principles](LORecomp_ENGINEERING_PRINCIPLES.zh-CN.md).

## 1. Roles

| Role | Responsible for | Does not do |
|---|---|---|
| Maintainer | Sets direction and makes decisions. Judges picture problems by eye. Playtests and accepts. Posts publicly on Reddit/Discord and similar. | Does not act as a process gate. Daily decisions go to the main agent. |
| Main agent (Claude Code main session) | Communicates with the maintainer. Splits tasks and writes handoff prompts. Reviews diffs and evidence. Merges, releases, replies to issues. Writes memory. | Does not personally run long investigations or builds. |
| Codex CLI | Long tasks: static audits, implementation plus builds, reproduction and stress tests on remote hosts. | Does not merge or release. Hands results to the main agent for review. |
| Claude subagent | Short parallel tasks: code lookup, documentation lookup, mechanical edits, documentation sync, board sync. | Does not delegate further. Does not go beyond the assigned scope. |
| Test hosts | AMD handheld (Linux, Proton D3D12 / native Vulkan), Mac, Android tablet, Switch/PS5 test consoles. | The local machine runs the game only when allowed. |

The maintainer gives the main agent full permission. Within that permission, the agent acts directly and reports when done. It stops to ask only in three cases: the action is destructive, it affects others outside the project, or the permission is not enough.

## 2. Basic Principles

- **Runtime efficiency comes first.** This is a hobby project. Do not add enterprise-level tests, validation, or defensive code. Detect changes at the source. Do not poll or rehash every frame. When fixing a bug, prefer removing unnecessary work over adding another switch.
- **Factory defaults match the original.** The original runs at 720p30. Every enhancement is turned on by the player. Test resolutions go in the config file, not in code. Behavior that the original already has (such as distance fade) is not treated as a bug.
- **Conclusions need evidence.** Implementation, validation, acceptance, and release are four separate things and are recorded separately. Release does not mean acceptance. Green CI does not mean the game runs.
- **A sound argument is enough to ship.** For a hard-to-reproduce race, a clear argument about what each side does while the lock is held is enough. Reproduction is not required. But a partial test must not be treated as full validation.
- **Public text stays short.** Text for players does not include test data. Before posting, revise it once more to remove AI-style phrasing.
- **No AI attribution.** Commits, PRs, issues, and comments do not contain Co-Authored-By or "Generated with".

## 3. Rules for Subagents and Codex

### 3.1 Role Table

| Role | Purpose | Model | Permissions |
|---|---|---|---|
| explorer | Locate code and trace call chains. Use when many files must be read but only a conclusion is needed. | Sonnet | Read-only. Use CodeGraph first if it is available. |
| researcher | Documentation and release notes for external libraries, APIs, and tools. | Sonnet | Read-only. Use ctx7 first for documentation. |
| worker | Mechanical changes with a clear spec, writing scripts, running builds. | Sonnet | Can write. |
| docs-sync | Keeps the bilingual READMEs in sync, CHANGELOG, STATUS/ROADMAP, release records. | Haiku | Edits documentation only. Does not commit or push. |
| project-manager | GitHub Project board and TODO sync. | Haiku | Edits the board and TODOs only. |
| general-purpose / Plan | Debugging, review, planning. | Opus, set explicitly | Depends on the task. |
| Codex (`codex exec`) | Long investigations. Implementation, builds, remote testing. | Model set explicitly | Full permissions inside its own independent worktree. |

### 3.2 Models and Count

- Do not inherit the parent session's model. Custom agents use the model in their definition. Built-in agents always get `opus` or `sonnet` set explicitly.
- Keep the number of agents running at once to a single-digit count. Do not run unbounded fan-out (for example, giving every finding its own verification agent), or the quota runs out quickly.
- When the Claude weekly quota is tight, give long tasks to Codex. The main session then only reviews. Check usage before starting a large task.

### 3.3 When to Delegate

Good cases for delegation:
- Several independent pieces of work that can run in parallel.
- Searches that read many files but need only the conclusion.
- Long builds, tests, and tours.
- Reproduction that needs a remote host.

Cases not to delegate:
- Small jobs that take one or two steps.
- Tightly coupled work that needs repeated back-and-forth edits.
- A task that has already been handed off.

By default, the main agent does only these: communication, splitting, review, merging, releasing, and very small immediate actions.

### 3.4 How to Write a Handoff Prompt

A prompt that can run on its own includes at least these parts:

1. **Working directory**: an independent worktree and its base commit. Do not touch the main checkout. Do not put it in a synced folder.
2. **Goal**, and what counts as done.
3. **Existing evidence**: log paths, reproduction conditions, directions already ruled out.
4. **Leads**: files, functions, and addresses that may be relevant. Mark which ones are guesses.
5. **Rules**: test host rules (locks, stop only your own processes), whether the game may run locally, which remote must not be pushed to, which configs must not be changed, no AI attribution.
6. **Validation requirements**: how far to compile, whether to do a full link, which tests to run.
7. **Delivery**: the report path and language. Whether to open a PR. By default, open a ready PR but do not merge it.

The appendix has a template.

### 3.5 Review Is the Main Agent's Job

A subagent report is input, not a conclusion. The main agent must read the diff itself and check each claim against the evidence. Check these categories in particular:

- Version attribution: which version a fix actually shipped in, verified with `git tag --contains <commit>`.
- Whether the change missed other platforms: if a key name, interface, or format was changed, search every place that reads it. Android and macOS code counts too.
- Parts that CI does not cover: PR CI does not link the final exe. Problems such as a duplicate hook definition only show up in a full local link.
- Concurrency fixes: require an explanation of what each side does while the lock is held. A forced-reproduction check only shows that the old problem is fixed.

### 3.6 Monitoring and Changing Direction

- Codex runs in a visible terminal tab, and the maintainer can press Ctrl-C at any time. Logs and the final message are written to files. The main agent starts a background watcher, which sends a notification when it sees the end marker.
- Do not use long blocking polls. CI and release builds also run as background commands that notify when they finish.
- If the direction changes, stop the task and resume the original session with the new instructions (`codex exec resume`). Do not start a new one.
- On a shared remote host, several tasks may queue for the lock at once. The main agent coordinates the order.

## 4. From Issue to Release

### 4.1 Triage

- First download the attachments to `_keep/<issue>/` locally. Read the complete logs from start to finish.
- Put similar reports in one table. For each crash, list which map, which battle, or how many seconds after which video it happened. Then look for common graphics cards, backends, settings, mods, and scenes across players and versions.
- The automatic triage bot is only a starting point. It often links unrelated issues.
- When the evidence is not enough, ask the player for a debug log and a save. Debug logging is now on by default, and DRED for D3D12 is also enabled.

### 4.2 Locating the Code

- If CodeGraph is available, use it first. Do not grep the whole repository root.
- Hand static audits to Codex. Require it to give the file:line for each release, reuse, or overwrite point, and to say whether a fence or lock protects it.
- Reverse-engineering techniques that do not need a disassembler:
  - Each line of the generated PPC code has a comment. Address = function start + 4 × instruction index.
  - Find classes and functions in the UE3 natives table in the image. Compare the vtables of a subclass and its parent to find overridden methods.
  - Use hardware write breakpoints (DR0–DR3) to find out what writes to an address. Then walk back up the guest call stack along r1.
  - Use a per-frame trace first to sort the problem into a category. For example, camera position smoothing or erratic facing shows a game-logic problem, not a rendering problem.

### 4.3 Reproduction

- **Running locally**: only when the maintainer allows it, and only with the maintainer's own install. Before running, take a lock, back up the files to be replaced, and record their hashes. After the run, restore them by hash. Write logs to a separate directory so the game does not clear the player's existing logs.
- **Shared test host**: every run holds the lock. Stop only your own processes, identified by the run path in `/proc/<pid>/cmdline`. Do not use `pkill -f`. Put commands in a `.sh` file. Do not put the exe path on the ssh command line.
- **Picture problems**: run in the foreground, stop at a fixed scene, change one variable at a time, state clearly which configuration is on screen, and wait for the maintainer to judge by eye. If automatic metrics conflict with what the eye sees, the eye wins. First confirm whether the problem appears only on one GPU vendor, and only then bisect by version.
- **Late-game scenes**: use diagnostic map jumps, disc requests, and Scenario Jump to build test saves. Do not touch players' saves.
- **Hard-to-reproduce races**: use a diagnostic switch to force a longer time window. But a forced reproduction can only show that the old race is fixed. It cannot show that the new lock will not deadlock.

### 4.4 Fixing

- One worktree per task. Keep it outside the synced folder.
- Write code the same way as the code around it. Keep the change small. Performance comes first.
- **Hook rules**:
  - Before adding `PPC_FUNC(sub_X)`, check whether someone has already hooked the same function. If so, call your logic from the existing hook. Do not define a second one.
  - After changing the TOML of a midasm hook, regenerate the PPC code.
  - PR CI does not link the exe. So any change that adds a hook needs a full local Release link before it is merged.
- **Locks**: call no API and do no waiting while holding a lock. Before merging, write down what each side does while the lock is held.

### 4.5 Validation

- Scale validation to the impact of the change. By default, run one smoke test. Run a matrix only when someone asks for it. Keep baselines and control variables, and use ABBA ordering for comparisons.
- Draw conclusions only after the tests finish. "Not reproduced" does not mean "fixed".

### 4.6 PRs and Merging

- Open a ready PR. Review the diff. Wait until all required checks pass, then merge. Use squash merge with `--match-head-commit`, so that what gets merged is the exact commit that was reviewed. Do not bypass branch protection. Do not force-push shared branches.
- For PRs that change only documentation or only literal values (such as version numbers), it is enough to review the diff and merge.
- Before pushing to a branch, confirm that its PR is still open. The maintainer merges quickly. A commit pushed one step too late lands on a branch that has already been merged.

### 4.7 Release Checklist

1. **Release PR**: change the CHANGELOG's Unreleased heading to the version heading. Use UTC dates, written only after confirmation. Update the CMake version number and the file names in the install docs at the same time.
2. **Tag**: create an annotated tag on the squash commit. Push it only to origin.
3. **Build**: GitHub Actions builds all installers and publishes them automatically. The main agent waits in the background and does not poll.
4. **Check**: the number and size of assets, and whether the release notes match the matching section of the CHANGELOG.
5. **Record**: update the package manifest (`.forge.json`). docs-sync writes the release records (STATUS/ROADMAP/MACOS_RELEASE). project-manager syncs the board when needed. Everything goes through one record PR.
6. **Notify**: under each related issue, reply with one line and the release link. Close the issue only after the player confirms.
7. **Memory**: update the release snapshot in Basic Memory and the local memory.

If a regression is found: revert right away, publish a new version, and then tell the public. Do not stack an unverified fix on top.

## 5. Public Communication

- **Issue replies**: one or two sentences. Say what changed and when it can be obtained. Do not include test data. Ask only for information that is really needed.
- **CHANGELOG**: one line per entry, in words players understand. Do not include evidence.
- **README**: written for players. Does not include validation history or internal details.
- **Public posts**: posted by the maintainer. The agent writes only drafts.

## 6. Documentation, Board, and Memory

### 6.1 Division of Repository Documents

| Document | Covers | Who edits it |
|---|---|---|
| `README.md` / `README.zh-CN.md` | Introduction for players, install entry point, feedback channels. The two languages are kept in sync. | docs-sync |
| `CHANGELOG.md` | Version history. One section per version, in both Chinese and English, one line per entry. | Fix PRs add entries to Unreleased. At release, the heading changes to the version. |
| `docs/STATUS.md` | Current source, validation, acceptance, and release status. One section per release. | docs-sync |
| `docs/ROADMAP*.md` | Work that is not finished yet. | docs-sync, kept consistent with the board |
| `docs/notes/` | Investigation process and evidence for each subsystem. English by default. Versions with a Chinese translation are saved as `.zh-CN.md`. | The agent that did the investigation. docs-sync adds translations. |
| `docs/archive/` | Outdated handoffs, progress logs, and timelines for bugs that are already fixed. | Moved in with `git mv`, and all links that point to them are fixed. |
| `docs/project-management/` | Board sync manifest (`items.json`), sync status, and the receipt for each reconciliation. | project-manager |

- Documents are written in English by default. Existing Chinese versions are kept as `.zh-CN.md`, and they link to the English version and back.
- Each status is maintained in one place only. Other places link to it instead of copying it.
- Old investigation notes are historical evidence. Their conclusions are not rewritten. When needed, add a dated line that points to the current status.
- Implementation, validation, acceptance, and release are written separately. A closed issue does not mean the reporter confirmed the fix. A merged PR does not mean it was verified in the game. A release does not mean the game was completed from start to finish.

### 6.2 How to Use docs-sync

**When to call it**: after a fix or feature is accepted; after a release, to write the release record; when the maintainer explicitly asks for documentation changes. The main agent edits small typos and formatting issues directly.

**What to give it**:
- What changed, and the matching commit or PR;
- Validation results, and what was not covered;
- Acceptance and release status (the live GitHub state is the reference);
- The list of files it may modify;
- A reference to an older commit, for example "write it in the structure of the release record at `<commit>`".

**What it does not do**: it does not change code, commit, push, release, or delegate to others.

**What it returns**: which files it changed, which outdated statements it corrected (old statement → live evidence → new wording), which checks it ran (`git diff --check`, links, bilingual consistency), and which evidence is missing.

**What the main agent checks**:
- Version attribution: use `git tag --contains` to verify each claim that a version includes a fix.
- Link type: an issue must not be written as a `/pull/` link.
- Whether it wrote "accepted" on the maintainer's behalf.

It uses a lightweight model. It suits following a template for synchronization, not technical judgment. When it finds conflicting evidence, it should report back, not investigate the code on its own.

### 6.3 How to Use project-manager (PM agent)

The GitHub Project board is the lifecycle view for each piece of work. Its fields include status, delivery, release version, and an evidence summary. The repository file `docs/project-management/items.json` mirrors the board. A sync script reconciles the two.

**When to call it**: when the maintainer asks for a board sync, or when the main agent gives it a clear status change (new request, in development, awaiting validation, accepted, released). It is not a resident service and does not run on a schedule by itself.

**What to give it**: the status change, the evidence source (commit, PR, log), the acceptance and release status, and the Project and repository it may write to.

**Process**:
1. Generate the sync plan.
2. Resolve conflicts.
3. Apply the plan.
4. Generate the plan again. It must show 0 operations.
5. Read the board back to confirm that the fields were actually written.
6. Submit a reconciliation receipt as a documentation PR.

**Common pitfalls**:
- Text fields hold at most 1024 characters. A longer value makes the whole batch fail, even though the fields before it in the batch were already written. Check the lengths before applying.
- When an issue is reopened, the board's built-in workflow changes its status to In Progress. The next plan then reports a conflict. Use the actual value on the board and write it back into the manifest.
- When the maintainer changes a status on the board by hand, the old record in the manifest shows up as a conflict. Update the manifest to match the board. Do not overwrite the maintainer's change.
- The sync script skips fields whose value is null. So to clear a text field, use `gh project item-edit --clear` directly, then read back to confirm.

**What it does not do**: it does not invent dates, priorities, or acceptance status, does not bulk-delete, and does not overwrite content written by people. It does not implement features, release, or post comments.

**Division of work with docs-sync**: PM manages the lifecycle of work items and TODOs. docs-sync manages narrative documents, the CHANGELOG, and release records. The two never edit the same file at once. Evidence is passed between them, and tests are not rerun.

### 6.4 Memory System

| Layer | What it stores | Rules |
|---|---|---|
| Repository docs | Technical evidence and current project status | See 6.1 |
| Basic Memory (shared across tools; Claude and Codex both read and write it) | Conclusions and methods: project index, release snapshots, follow-up items, issue root-cause ledger, topic notes | See below |
| Local agent memory | Tool habits, the maintainer's preferences and corrections | See below |

**Basic Memory maintenance conventions**:
- Each project has one folder, with an index note as the entry point: reading order, topic map, issue index, and maintenance conventions.
- Volatile state (releases, issues, PRs) is written only in the "release snapshot" and "follow-up items" notes. Each entry has a date and a source. Other notes contain only conclusions and methods.
- One-off small problems go into the "Issue root-cause ledger" for the current month. They do not get their own note.
- Search before writing. If a topic already exists, add a dated section to it. Do not create a duplicate note.
- New notes must be added to the index. End each new note with a `## Navigation` section that links back to the index.
- When an old conclusion is overturned, write a `[correction]` entry. Keep the original record.

**Local memory format**:
- One item per file. Each file starts with frontmatter (name, one-line description, type: user / feedback / project / reference). There is also an index, with one line per file.
- Feedback memories must include **Why** (why the maintainer said it) and **How to apply** (when it applies and what to do).
- Do not store things that already exist in the repository or git history. Do not store things that are useful only for the current conversation.
- When a memory is outdated, update or delete it. When the index gets too long, merge similar entries into one line.

**Reminder hooks**:
- At session start, a reminder asks the agent to read the project index and related notes first.
- After several PRs are merged or a version is released, or after many conversation turns, a hook blocks once before the session ends and reminds the agent to write memory.

**Neither kind of memory may store**: keys, tokens, credentials, or full raw conversation text.

## 7. Common Pitfalls When Porting Other Xbox 360 Games

### 7.1 Recompilation and Builds

- **Memory barriers**: PPC `lwsync`, `sync`, and `eieio` must become real memory barriers in the generated code. PPC source files copied from an old directory may lack them, and on ARM this causes strange crashes. Before suspecting guest code, confirm that the generated code is up to date.
- **Duplicate hook definitions**: one guest function can have only one `PPC_FUNC` hook. If CI does not link, duplicate definitions slip through until the release.
- **Regenerate midasm hooks**: if you change only the TOML and do not regenerate the PPC code, the hook will not take effect at all.
- **Global values may be pointers**: some globals actually hold a pointer to an object. If a register is compared against the global's address constant, the patch fails silently and never fires.
- **Do not commit private inputs**: XEX files, generated PPC, and game data do not go into the repository. CI gets private inputs through a deploy key.

### 7.2 Kernel and XAM

- **Async completion is too fast**: the emulated XAM and IO layers often complete immediately. The game previously relied on real hardware latency to hide its own races, so they surface here. For example, a completion signal can be overwritten by a "busy" state written later. When fixing these, keep the lock as small as possible.
- **Critical sections**: fields are stored big-endian. Calling anything that waits on another thread while holding the lock can deadlock.
- **Import bridges can pause**: a guest thread can be paused by the host menu at the import entry point. Lock entry and exit must avoid passing through such a pause point.
- **Check Xenia's stubs before copying them**: a sleep that Xenia added for one game can slow the audio thread by an order of magnitude in another game.
- **XMA**: a decode that returns -11 means the frame was really rejected. It does not mean "more data is needed".
- **Save contents**: the save folder name is the content name. After a crash, the metadata may be all zeros. Trust the actual save data.

### 7.3 Memory

- **Page size**: Apple Silicon and PS5 use 16 KiB pages. They cannot express physical aliases at 4 KiB offsets. The approach is to not map that view (E), compute the physical address from the address, and leave the PPC semantics unchanged.
- **4 GiB window**: the guest address space should ideally land on the preferred base address. Each platform reserves memory differently, so verify each one separately.

### 7.4 GPU and Rendering

- **Temporal flicker**: flicker under TAA, DLSS, and FSR almost always comes from a pass that has no jitter applied. Fixing it in any one temporal AA also fixes the others.
- **Post-process texture offset**: shaders compute texel offsets from the guest texture size. So when the internal resolution is raised, the post-process coverage stays at the 720p size.
- **Resource lifetime**:
  - Release must wait for the GPU fence.
  - If the GPU batch changes in the middle of a draw, descriptor sets already obtained cannot be used again.
  - A cache keyed on raw pointers will hit an old object when the address is reused.
  - All three kinds of problems show up as DEVICE_REMOVED.
- **Pass all render state**: when state such as the blend constant is not passed to the host, the game's fades stop working.
- **Driver differences**:
  - Driver-level upscaling on AMD (RSR, FSR upgrade) breaks Vulkan.
  - The Adreno system driver supports only Vulkan 1.1. Use Turnip.
  - On Turnip, `vkGetFenceStatus` blocks while waiting. On Android, do not poll fences.
- **Shaders**: compiling on first use causes stutter. Precompiled packs need a version contract. If the contract does not match, everything falls back to compiling on the spot.

### 7.5 Game Logic and High Frame Rates

- **Fixed-step resamplers**: logic that resamples at fixed 12.5 ms steps consumes at most one history entry per step. If the frame time is shorter than the step, it breaks.
- **Frame-counted timers**: they run faster at high frame rates.
- **Fixed prompt hold times**: these can be shortened safely. But first confirm that later stages wait for async results on their own and do not depend on this hold.

### 7.6 Platform and Input

- **libc++ timed waits**: on Android and macOS, a semaphore wait with a timeout wakes every 8 ms. Long-running threads should not use timed waits.
- **Gamepad button swap**: with SDL3 and Steam Input, Nintendo-layout gamepads have their buttons swapped twice.
- **Signing**: macOS packages have no Developer ID. Android packages are signed with the debug key. The install instructions must say so.

## Appendix: Handoff Prompt Template

```markdown
# <issue number>: <one-sentence goal>

Repository: <owner/repo>. Working directory: <independent worktree>, branch <branch>, based on <commit>.
Do not change the main checkout. Do not put it in a synced folder.

## Evidence
- Logs: <path>; reproduction conditions: <…>; ruled out: <…>

## Leads (guesses, need verification)
- <file:line / function / address>

## What to do
1. <step>
2. The fix targets only the proven cause. Keep the change minimal and match the style of the surrounding code.

## Rules
- Test host: hold the lock; stop only your own processes; do not use pkill -f; do not run the game locally.
- Do not push to <remote>; do not change <config>; no AI attribution.

## Validation
- Compile + full link; <test or run>.

## Delivery
- Write the report in Chinese to <path>: conclusions, file:line, evidence, and what is not yet verified.
- Open a ready PR. Do not merge.
```
