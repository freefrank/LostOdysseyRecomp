---
name: lo-render-flicker
description: Diagnose and repair LostOdysseyRecomp flicker, black materials, or mismatched render layers from F1 ZIP or directory captures, with shader and draw evidence plus focused regressions.
---

# Lost Odyssey render-capture investigation

Use this for LostOdysseyRecomp rendering captures and their corresponding source fixes. Work in the repository containing `LostOdysseyRecomp/gpu/renderer.cpp`. Resolve every `tools/`, `docs/`, and source path below from that repository root (use `git rev-parse --show-toplevel`), not from this skill directory. Read `AGENTS.md`, `tools/capture_analysis/README.md`, and relevant tool `--help` first. If `.codegraph/` exists, use CodeGraph before locating or reading indexed code; fall back when unavailable.

## Establish what happened

- Record the exact input capture, runtime version/revision, backend/GPU, frame IDs, output size, completed frames and drops. Reconstruct configuration from the captured run, not current settings or a similarly named executable.
- Run `tools/capture_analysis/inspect.py` for the archive structure. Windows ZIP members may use backslashes. Frame-local metadata lives in `frame-*-f*/`, not necessarily the archive root. Do not extract every large surface just to inspect metadata.
- View normal and abnormal screenshots, then compare selected early color resolves. Use `preview.py` for selective images/ROI summaries and `image_diff.py` for two extracted images. Record the earliest stage that visibly contains the anomaly; a later stage retaining it does not establish that stage as the cause. ROI metrics support the actual viewed region, not arbitrary whole-frame scores.
- Distinguish stored AA settings from the actual frame-plan consumer and evaluation records. DLAA uses the DLSS SR route. Zero vendor evaluations exclude an observed vendor-output artifact, but do not prove raster input jitter was off. `candidate_ready`, CPU metadata, and configured modes do not prove history completion, GPU execution, or visible stability.

## Narrow the draw evidence

- Use `compare_traces.py` to reconstruct cumulative register deltas and compare selected frames. A shader record missing from one draw is unknown; do not inherit a previous draw's shader or align different scenes solely by draw number.
- Use `jitter_candidates.py` for exact captured camera-slot matches, known mapping coverage, and possible depth companions. Limit the search to draws before the earliest anomalous resolve when justified by images. A candidate report is a shortlist, not permission to add a hash.
- Pair material and depth using index base/count, position fetch stream, stride/layout, world transform, exact unmodified camera words, viewport/VTE/scissor, depth target and ordered passes. Record missing or ambiguous evidence. A raw depth target register match is not independently proven host allocation identity.
- Inspect each shortlisted VS HLSL or translated microcode. Trace the actual `oPos` arithmetic and clip-coordinate varyings; confirm which four constant registers feed projection and whether they also drive UV, lighting, skinning, or another output. Inspect the paired PS purpose, depth writes and relevant texture/sampler bindings. Reuse `tools/shader_analysis/` helpers as static triage; they do not replace dataflow review.
- If coverage is complete or position evidence disagrees, follow the earliest anomalous stage into resource writes/bindings, depth/blend/culling, alpha, synchronization or shader translation. Do not force every flicker into a missing jitter mapping. When the capture is insufficient, record a precise missing observation and leave an explicit TODO rather than an unvalidated workaround.

## Proactive coverage

- Use `tools/capture_analysis/coverage.py` for proactive discovery across complete draw intervals and all captured frames. The before-first-resolve cutoff is appropriate only when investigating a specific known symptom whose earliest bad stage is established; proactive coverage must scan the full draw range so later material/light passes are not silently omitted.
- Use `tools/feedback_archive/scripts/jitter_coverage.py` only with an explicit private `--archive`, current mapping file, and new `--output` outside the archive. Reuse the current map and group source records by VS/PS before ranking candidates; do not repeatedly surface old `slot=-1` records that are already mapped. Counts are diagnostic record and pair counts, never player counts or frame counts.
- Use `tools/feedback_archive/scripts/export_programs.py` after selecting candidates from all observed VS/PS pairings, not only strong-pair filters. Pass explicit `--archive`, selection JSON, and a new `--output` outside the archive. Its preflight requires one stage/hash source, matching payload length, SHA-256, and renderer byte-FNV; use the resulting payloads and `provenance.json` for exact source review. Successful export is source verification, not GPU or visual validation.
- Keep source-only programs separate from programs with diagnostic records. A source inventory, a ledger hit, and a current observed draw establish different evidence levels; none alone establishes a position bug or authorizes a runtime mapping.
- A `guards=31` record is a candidate gate only when `unknownslot=-1` and its flags agree with the depth evidence; it does not replace VS/PS purpose review. Pause a hash when camera evidence is mismatched (`guard=29`) or when its clip-XY/W output feeds screen texture sampling until the producer binding and temporal relation are established.
- Static `c[N]` and literal `XeConst(N)` recognition is bounded. Dynamic constant indexing and unresolved control/data flow remain unknown and require manual translated-HLSL review.
- For PS clip-coordinate review, `audit_ps.py --clip-input N` (N=0..15) is an optional component read. Any branch or unknown flow, including fixed host epilogues, remains unsupported; `candidate_no_clip_xy_reads` is a manual-review candidate and never mapping authorization.
- When matching geometry across passes, depth, blend, scissor, or mode differences can be legitimate pass changes. Review the full draw contract manually rather than rejecting a candidate solely on register inequality or accepting it from geometry equality alone.
- A self-anchored depth-tested draw is not independent scene proof. For clip-XY/W texture sampling, trace the producer texture, extent/crop, and temporal relation before treating a candidate as safe.

## Apply and verify a bounded correction

- For a proven mapping omission, edit the exact cases in `gpu/temporal_scene.h::PositionVPSlot`. Preserve camera/depth/viewport/finite-value guards and independent UV/light constants. Do not infer a slot from frequency, visual similarity, a hash list, or a static matrix candidate alone.
- Use `export_jitter_fixture.py` only after reviewing the candidate hashes/slots and their depth partners. It exports constants; it must not modify the production mapping or claim its output is an independent oracle.
- Add a focused selector to the existing `tools/tests/temporal_jitter_test.cpp` where appropriate. Independently evaluate the captured shader's position instruction order, including swizzles. Reuse an existing evaluator only after proving the instructions equivalent. Preserve captured provenance, distinguish synthetic vertex samples from captured vertices, cover relevant jitter phases/sizes, preserve Z/W and unrelated constants, and include old-behavior and rejection controls.
- Build and run the smallest affected target/selector. Reuse passing results across documentation, commit and release steps. Tests of constants or CPU arithmetic do not establish raster coverage, GPU pixels, FPS, or game acceptance.
- For actual runtime validation, preserve the user's working executable/configuration/saves. Compare the same build source, scene/camera and one controlled change; identify frame phase when relevant. Do not launch, replace a running build or move the player's character merely because a ZIP was supplied. Coordinate the needed gameplay observation within the user's authorized scope. Keep automated checks in the background and avoid stealing focus.

## Collaboration and delivery

OpenCode agents supplied with this workflow:

- `lo-render-investigator`: owns provenance, images, earliest-stage reasoning and integration; delegates only independent bounded work.
- `lo-render-fixer`: owns the agreed source/fixture paths and affected validation; receives existing evidence and already-passing checks.
- `lo-render-reviewer`: independently checks slot/dataflow safety, assumptions and evidence boundaries without repeating tests.

Use parallel analysis only where it avoids duplicate work; pass file ownership and do not revert another worker's changes. The main agent owns final judgment. Without these agents, run the same workflow locally.

Keep reusable code in `tools/`, private captures/extractions/reports in ignored `out/<case>/`, and no scripts that depend on executable code under `out/`. Do not index `out/` or commit raw game shaders/images. Synchronize the scoped changelog/investigation notes through the project's `docs_sync` process when available. Report observed defect, proven cause versus hypothesis, source change, exact validation, user acceptance and release separately. Commit/push/release only within current authorization.

Example request: “Use lo-render-flicker to investigate this F1 capture, identify the first bad render stage, fix only what the draw/shader evidence supports, and run the affected regression.”
