# Temporal jitter coverage review — 2026-09-25

This note records a bounded discovery pass and its subsequent reviewed mapping batch. It separates the pre-implementation coverage snapshot from the implemented source/synthetic scope; it does not claim GPU pixel validation, player acceptance, or release completion.

## Inputs and current boundary

The pre-implementation feedback snapshot was read at repository HEAD `4191350b`, checkpoint `2026-09-23T19:42:37Z`, with map size 84. The completed stream accepted 218,795/218,795 observations: schema 1/2/3/4 counts were 517/155,498/62,321/459, with zero duplicates and errors. It covered 2,596 sources (287 VS and 2,309 PS) and 1,563 VS/PS pairs. Feedback records remain private and are not a substitute for current player counts or the review ledger.

The capture reports are `out/jitter-coverage-20260925/family-review.md`, `path-review.md`, and `late-pass-review.md`; the final capture summary is `capture-coverage-v2.json`. It scanned 7 captures and 21 frames with `metadata_scanned`, zero errors, zero gaps, 357 non-shader-copy records, and 14 unmapped VS hashes. Two late scene-material candidates are especially relevant: `e810cfacc107fd3c` at slot 7 and `2078ccaa70d44732` at slot 8. Both remain candidates pending controlled runtime and image evidence.

## Initial discovery snapshot

- All 84 map entries at the start occurred in the 287 source-backed VS set from the feedback archive. Of 203 unmapped source programs, 140 had actual diagnostic records and 63 remained source-only without a diagnostic pair; these categories were not a uniform position-bug set. The implemented batch is recorded below.
- The static family review contains 29 fixed and 192 finite linked-family candidates. These are prioritization results, not authorization to expand `PositionVPSlot`.
- `81217dc973d5dc31` has four observed VS/PS pairs and 532 metadata records, but no guard-31 evidence; related matrix records have guards 16 or 29 and lack an anchor or exact camera. It remains a lower-priority family candidate despite its runtime observations.
- Only 8 of the 84 explicit mappings fall inside the limited finite whole-disc scan. Runtime fetch/link combinations are a recurring source of hashes outside that inventory.
- The self-anchor guard remains a caveat: a depth-tested mapped draw can compare its own matrix and depth allocation successfully. Independent scene-depth pairing and pass evidence are still required.

Past-ledger filtering found 79 unmapped VS / 185 VS/PS pairs with high-priority candidate signals (`slot=-1`, `positionkind=1`, `issues=0`, `guards=31`, `rejection=2`, with the required flags mask). These were high-priority candidates, not confirmed bugs. The exact source-prioritized metadata-record counts are `09f67586057d7083`=24, `2d458def192151ac`=12, `6d3d954bb6d86bc1`=31, and `bda41a11626a545c`=162; they were not frame counts, and the newer PS associations were not yet all reviewed at this initial stage.

The 79-source slot distribution is 64 slot 7, 8 slot 8, 4 slot 0, 2 slot 4, and 1 slot 1, counted by distinct VS. Exact archived programs for 78 candidates were translated and compiled successfully, and their statically identified position slots agree with telemetry; the remaining `e810` slot-7 program was independently inspected in the late-pass capture. This establishes source/telemetry agreement, not visual correctness. The private `feedback-source-review.md` records this scope and the source identities.

The exact reviewed PS pairings for `09f67586057d7083` and `2d458def192151ac`, and the no-PS depth path `6d3d954bb6d86bc1`, had no identified clip-XY screen-sample side effect. For `bda41a11626a545c`, that conclusion covered only PS `a9e9542e2c60029a`; its five other observed PS pairings remained unreviewed at this initial stage. `b60f` slot 8 and `02d8`/`3f522` slot 7 had clip-XY/W texture-0 coupling and required producer binding evidence; the same producer gate applied to the late `e810` and `2078` candidates. The 79 set had 75 nonzero position-output masks. Six exact PS programs were deeply reviewed, and one additional path had no bound PS; the full set of 185 VS/PS pairs was incompletely reviewed at this initial stage.

The diagnostic queue is reproducible using the maintained [feedback coverage tool](../../tools/feedback_archive/README.md) and [capture coverage tool](../../tools/capture_analysis/README.md). Private per-pair provenance and representative records stay in `out/jitter-coverage-20260925/player-coverage.json`; the compact candidate list is `strong-unmapped-summary.json`. These outputs are inputs for review, never executable dependencies.

The pre-implementation reports preserve the observed geometry, viewport, target/depth, UV/lighting and projected-lookup limits. No runtime mapping was added from that initial coverage pass. The next gate from that snapshot was to subdivide the 79-source candidate queue, review exact draw occurrence and independent depth pairing, and then require candidate-specific implementation and same-scene visual evidence.

## Implemented mapping batch

The subsequent reviewed batch added 35 VS mappings to the 84-entry map, bringing the current map to 119 entries. The additions are distributed as slot 0: 3, slot 1: 1, slot 4: 2, slot 7: 27, and slot 8: 2. Across all 79 candidate VS and their 186 observed pairs, 35 VS / 65 pairs were included in the batch; 44 VS remain held, comprising 42 clip-XY screen-producer cases and two observed mixed-camera mismatches: `bda41a11626a545c` (48 records with guard 29) and `eb5f611c4321708e` (39 records with guard 29).

The review manifest [`feedback_mapping_20260925.json`](../../tools/shader_analysis/reviews/feedback_mapping_20260925.json) records all 79 source SHA identities, associated PS SHA identities, slots, families, and decisions. The reviewed fixture is [`feedback_mapping_cases.h`](../../tools/tests/feedback_mapping_cases.h); the runtime map is `LostOdysseyRecomp/gpu/temporal_scene.h`. All 167 exact PS identities were verified; 8 reused existing HLSL and 159 were translated with zero DXC failures. This is source and synthetic CPU scope, not a full-shader GPU run. The fixture uses one representative PS per VS; the 65 pair set has separate static review in the manifest.

`LoTemporalJitterTest --feedback-mapping-batch` passed 216,625 checks for 35 VS × 3 synthetic banks × 32 phases × 720p/1080p/1440p/4K. Maximum XY displacement error was 0.002689 px; the unknown-shader old-upload negative control reached 0.487806 px separation. Off, unknown, missing-camera, incompatible-viewport, camera-bit, depth-mismatch and finite-guard rejection banks passed, with VP Z/W, non-VP constants and PS banks preserved. The test does not execute all PS/varying paths, so PS safety remains source-review evidence.

No runtime executable was deployed or launched, and no player acceptance or release claim follows from this batch. The two mixed-camera cases remain held because a newly mapped draw could establish a self-anchor; guards and source agreement do not independently prove scene identity.

The reusable tooling now includes `export_programs.py` for explicit, identity-verified program selection (four focused tests passed) and `audit_ps.py --clip-input N` for conservative component-read triage (five focused tests passed). The latter preserves input lanes, reports clip-XY/W reads, and marks control flow or unknown operations unsupported, including fixed host epilogues. Its candidate flag requires manual review and never authorizes a mapping. The OpenCode skill and tools index link these steps; the batch's temporary review script was removed while its private reports were retained.
