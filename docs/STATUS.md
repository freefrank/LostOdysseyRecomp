# Project status

## Published v0.5.2 — 2026-09-10

Source version **0.5.2** is published at [GitHub](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.2), with Release CI `34446196620` succeeding for commit `08a0192713435892f3c1b772abc9ab31bca36359`. The ZIP is 44,209,471 bytes with SHA256 `5d20e569b74c75418cefc9fdd5537477ce4b0a9ca976ed9d64ec77771f78ce7c`; the standalone updater is 849,920 bytes with SHA256 `4aa5e491a2bff885c668cdc4473488fd05acf44e56eba2aa9d4c242a76a4db6b`. All 50 payload hashes and CRC checks passed. The release adds original VS/PS microcode to F1 captures, consent-gated incremental D1 uploads and bounded nonblocking collection.

The recorded runtime evidence remains tied to the retained source-0.5.0 development binary. Release CI built new 0.5.2 executables; existing functional validation was reused without another game run. Credential scans found no management credentials in the audited source or expanded release package. All four public assets returned HTTP 200 and matched the audited hashes in anonymous download verification.

The standalone updater retains the behavior validated for [v0.5.1](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.1). The separate updater download is byte-identical to the helper inside the 0.5.2 ZIP.

## v0.5.3 release preparation

The current local documentation and source changes are prepared for v0.5.3. The release adds compact opt-in TAA diagnostics, schema 3 binding evidence and the updater ZIP-root staging fix while preserving the v0.5.2 wire protocol identifiers and historical package hashes. Publication, release assets and player acceptance are pending; no new game or visual validation is claimed.

## Current validation and limits

### TAA binding evidence — 0.5.2 development build

The 0.5.2 development client now adds an opt-in schema 3 binding-evidence record for a bounded TAA draw. It preserves the VS/PS renderer hashes and draw classification, then records the consumer slot and phase, guest and uploaded VP values, raster viewport and jitter bits, selected PS `c0` values, and the referenced texture's format, extents, resolve rectangle, producer state, producer draw count, frame ages and resolve gap. The record uses an independent fixed 64-entry POD queue; collection is batched in groups of eight and is triggered by the existing 60-second background cadence or F1 request. It does not add GPU readback, a new wait, pointer or address data, or a jitter mapping.

The production build completed successfully with executable SHA-256 `c3711463fd4d21f17b68af27cecd2df35851af49e9567e956596b48a7ae1e259` and size 82,904,064 bytes. CPU queue and producer fixtures passed 82 and 32 checks with zero producer allocations. The updated 9,761-byte, eight-record serializer passed three real C++ → Worker → SQLite uploads with HTTP 200 responses; replay preserved eight deduplicated rows, 1,048 record leaf values and 640 IEEE bit words. Evidence: `out/v0.5.2/taa-binding-collection/production-build-resume.json`, `run.log` and `worker-integration.json`.

The deployed Worker version is `4db7e156-8a21-4f57-8f0d-bacd0c4576ab`; its health response advertises schemas 1, 2 and 3, with the existing D1 and rate-limit bindings and persistent logs disabled. No D1 schema migration was needed. There is still no new game run, F1 menu acceptance or visual acceptance for the `e810cfacc107fd3c` path, so schema 3 remains diagnostic evidence and does not authorize a jitter mapping.

For schema 3, `draws` and D1 `max_draws` count samples of the same recorded state, not all draws in a frame or a total frame draw count. `producerDraws` is a conservative CPU writer count; values above 65,535 become `0` with `Unknown` state. `Uniform` records consistency among CPU-recorded writer matrix and jitter values, without proving GPU completion, per-pixel use or recursive texture dependency. Detailed serializer and artifact evidence is in `out/v0.5.2/taa-binding-collection/REPORT.md`.

### Compact automatic diagnostics — local development

The new compact diagnostic contract uses the existing opt-in and records one fixed 32-frame CPU window at most every 180 seconds. It bounds the window at 24 VS/PS pairs and eight binding records, then includes final TAA history/rejection booleans, coverage reason counters, pending counts and delivery results. The request is capped at 32 KiB and uses a strict compact receipt; sparse GPU data is D3D12-only and remains unlinked to this window. No color preview, raw F1 ZIP, random/session/player/device identifier or new GPU completion claim is added.

The local Worker validator passed 9/9 checks in `out/v0.5.2/compact-diagnostics/worker-tests.json`. The C++ fixture passed 68 checks with zero allocations; its corrected 18,905-byte request passed two loopback HTTP 200 uploads into the actual SQLite schema, preserving one deduplicated row and all window fields. The 0.5.2 client build completed at 2026-09-10 16:42:36 UTC with SHA-256 `59f039ea84404c1ab85095a95a10b32d435bf1d39b1ca610b38d15edb44ce62e` and size 82,946,560 bytes. Eight new ledger checks and one archive check passed; the installed wrapper's current archive sync remained unchanged across 18 cases with identical source hashes and modification times. The private archive schema 4 allowlist is published in commit `728d6030980a7be39feca493319f54bfb1a62e74`, and Worker deployment `14e74c54-1213-4240-9877-047f35cdda75` advertises schemas 1 through 4 with temporal and shader-source capabilities enabled. Existing F1 force flush remains optional and is not a dependency. The client release build and player acceptance remain separate release gates.

Window completeness means 32 final CPU frame snapshots, not all draws or bindings and not GPU completion. Counters describe observation calls within the window; delivery counts are cumulative since the consent/device reset. Existing summary, source, binding and sparse streams use HTTP 200 acceptance, while compact delivery requires the validated compact receipt. The wire carries source version and protocol build information; an unknown `runtimeCommit` does not identify the exact executable.

The private [research archive repository](https://github.com/freefrank/LostOdysseyRecomp-build-inputs) is now enabled by a daily GitHub Action. Its first successful run is [Action 34493751731](https://github.com/freefrank/LostOdysseyRecomp-build-inputs/actions/runs/34493751731), with content-addressed deduplication and a data commit beginning `8da2b10e`. The first snapshot validated 3,401 diagnostics, 431 unique VS/PS programs, 909 GPU associations, 31 temporal records and 462 payloads. This archive is a private, long-term research copy; it has no D1-style automatic expiry.

### Updater validation

The local v0.5.2 updater fixes ZIP staging for the explicit top-level directory entry emitted by Python `shutil.make_archive`; the entry's trailing slash is accepted while the archive must still contain exactly one package root. Standalone updater text and error dialogs are English, and native buttons are requested with `en-US`.

The focused archive runner passed 12/12 cases, including successful release-style staging, implicit and root-last directory handling, and rejection of multiple roots, top-level files, absolute or parent roots, traversal, duplicate and unlisted payloads, hash mismatch and missing `manifest.json`. At that earlier updater checkpoint, the updater-only `LoUpdaterTest` build succeeded while the game executable was not rebuilt; the retained embedded-updater evidence therefore belongs to that checkpoint. The later 0.5.2 full game build includes the updated embedded updater source, but the updater transaction was not re-accepted. Render-only UI checks passed with English text and font assertions and no clipping; a live native dialog was not exercised. This local candidate is unpublished and has no user acceptance, network update or full update transaction. Evidence: `out/updater-fix/REPORT.md`, `out/updater-fix/archive-test.log` and `out/updater-fix/ui/render-manifest.json`.

The retained D3D12 RTX 5080 background run used source-0.5.0 executable SHA256 `1beb8a50c5bef1ebf0fb147338b33d558a2193e87f82b0b8c579ecbcb856959d`, 1280×720 experimental TAA, 60 FPS target and Map 16 Main Street. Two periodic uploads added 23 then 32 programs, for 55 total (15 VS / 40 PS, 19,188 bytes). All received sources matched the local cache by bytes, SHA-256, renderer FNV and length; the final snapshot had 55 source rows and associations without duplicate keys, plus 28 matching structured diagnostics. Map 16 windows measured 59.67 FPS / p95 18.036 ms before the second upload and 59.84 FPS / p95 18.067 ms during it. This is bounded background acceptance, not F1 product export, opt-out A/B, universal zero-overhead proof, Vulkan/AMD coverage, flicker repair acceptance or whole-game validation. Evidence: runtime report (`out/v0.5.0/shader-source-collection/runtime/REPORT.md`, retained locally), D1 verification (`out/v0.5.0/shader-source-collection/runtime/D1-VERIFICATION.md`, retained locally) and shader-source report (`out/v0.5.0/shader-source-collection/REPORT.md`, retained locally).

Product F1 ZIP/manifest and immediate post-capture upload remain pending. The attempted `LO_CAPTURE_REQUEST` was a legacy trace request. The latest flicker diagnostic used older executable `ececed95`, before the four c7 paths; it is not a regression acceptance result. F1 frame costs were 1.690/3.156/1.561 seconds, and the later background ZIP attempt timed out at 325.117→385.166 seconds with the original directory retained. See flicker analysis (`out/v0.5.0/taa-flicker-20260910/analysis/REPORT.md`, retained locally).

The current Windows delivery scope is D3D12/Vulkan. DX11, Linux, macOS, experimental Switch work, other GPU coverage, DLC rewards/dungeons and full-game compatibility remain separate validation areas. Open work-item status and priorities are maintained in the [public Maintainer Project](https://github.com/users/freefrank/projects/3); completed change history is in the [CHANGELOG](../CHANGELOG.md).

<a id="live-issue-reconciliation"></a>

## Issue and acceptance boundary

GitHub Issue state, reporter acceptance and implementation evidence are separate facts. Use the [public Maintainer Project](https://github.com/users/freefrank/projects/3) for current work-item state and the [CHANGELOG](../CHANGELOG.md) for completed releases. Dated implementation and investigation evidence remains in `docs/notes/`.

Earlier Issue closure sources and their acceptance limits are preserved in the [archived reconciliation](archive/STATUS-2026-09-10.md#live-issue-reconciliation).

## Archived status snapshot

The former detailed status ledger, including historical candidate identities and dated validation narratives, is preserved in [STATUS-2026-09-10.md](archive/STATUS-2026-09-10.md). It is historical evidence and does not define the current source or release state.
