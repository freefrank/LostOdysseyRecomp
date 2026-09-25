---
name: lo-render-flicker
description: Script-first LostOdysseyRecomp flicker triage and bounded fixes; reuse reviewed evidence and involve the LLM only for new or contradictory findings.
---

# Script-first render investigation

Work from the game repository root. Follow project rules; use `tools/README.md` to locate tools and read only the relevant subdirectory documentation when needed. Private inputs/reports belong in ignored `out/`; reusable code belongs in `tools/`.

## Default: one agent, existing scripts

- The current agent owns the task. Fixer/reviewer agents are optional roles, not mandatory stages. Do not launch them unless the user requests delegation. Routine scans, sorting, identity checks, builds, tests, and Git operations use scripts/CLI.
- Select the route below; do not execute every tool for every report. Existing coverage CLIs print a compact `summary` and save the full JSON. Read that summary first, then use Python to select at most five relevant rows for the initial LLM context. Never print entire coverage reports, HLSL directories, or feedback archives into context.
- Reuse evidence when input identity, program bytes, PS pairings, relevant pass/binding state, tool semantics, and tested code are unchanged. Compare the archive checkpoint before scanning again. Reconcile saved mapping classifications with the current map; a new map does not require rereading all raw observations.
- Consult `tools/shader_analysis/reviews/feedback_mapping_20260925.json` before reopening known candidates. Reuse unchanged implemented/held decisions. New source, PS pairing, binding evidence or a contradictory capture reopens only affected cases. Translate only previously untranslated program bytes.
- Do not broaden a single capture into a whole-archive audit unless requested. Batch repetitive operations in scripts; parallel shell jobs do not require parallel LLM agents. Keep reports brief: change, evidence, remaining gap.

## Choose one route

| Input/task | Script route |
| --- | --- |
| New flicker capture | `tools/capture_analysis/inspect.py` for provenance/completeness; `preview.py` for selected screenshots/early resolves; `jitter_candidates.py` for relevant frame/draw candidates. |
| Compare affected frames | `compare_traces.py`; use `image_diff.py` only for a useful selected ROI. |
| Requested capture coverage | `tools/capture_analysis/coverage.py` with explicit inputs, current `--mapping`, new `--output`; inspect its stdout summary first. Scan full draw intervals. |
| Requested player-feedback coverage | `tools/feedback_archive/scripts/jitter_coverage.py` with explicit private `--archive`, current `--mapping`, new `--output`; reuse a matching existing snapshot. |
| New shader evidence | `export_programs.py` with explicit selection and identity checks; existing `LoShaderTool` for missing translations; `audit_vs.py` and `audit_ps.py --clip-input N` for selected sources. |

Commands and arguments are in `tools/capture_analysis/README.md`, `tools/feedback_archive/README.md`, and `tools/shader_analysis/README.md`. Read `--help` only for an unfamiliar command. Missing evidence goes into a precise held reason; repeated speculation is not progress.

## LLM decision gate

Inspect only new/contradictory source slices or unresolved visual/pass relationships. Verify actual run revision, consumer, frames and drops; configured AA or zero vendor evaluations do not prove raster jitter was off. Locate the earliest bad image stage when investigating a visible symptom.

Before mapping, prove the exact four VP constants and their position/clip dependencies, preserved UV/light/skin data, and independent camera/depth/pass agreement. Review **all observed PS pairings**, including weak pairs. Match geometry, world transform, unmodified camera words, viewport/VTE and ordered depth use; a register address alone is not allocation identity. `guards=31` needs the unknown-slot/flags/depth context and is not sufficient by itself. Hold mixed-camera uses and clip-XY/W screen sampling until producer, extent and temporal alignment are proven. Dynamic indices/control flow and unsupported audit results remain unresolved; no script candidate flag authorizes mapping.

## Fix and finish

Edit only proven `PositionVPSlot` cases; preserve runtime guards. Reuse an equivalent reviewed projection evaluator or add the smallest independent arithmetic regression, with old-behavior/rejection controls and unchanged Z/W and unrelated constants. Label synthetic inputs. Run only the affected build/test once; reuse success for docs, commit and push. CPU evidence is not GPU/visual acceptance.

Use the same agent for routine scoped documentation updates in this economical workflow. Do not create a second review or documentation round for bookkeeping. Retain exact commands/results and held reasons in one case report. Keep the changelog/current status accurate. Captures alone do not authorize game launch, executable replacement, gameplay control or publication; preserve saves/configs. Commit/push only within the user's current authorization.
