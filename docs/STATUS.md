# Project status

## Published v0.5.2 — 2026-09-10

Source version **0.5.2** is published at [GitHub](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.2), with Release CI `34446196620` succeeding for commit `08a0192713435892f3c1b772abc9ab31bca36359`. The ZIP is 44,209,471 bytes with SHA256 `5d20e569b74c75418cefc9fdd5537477ce4b0a9ca976ed9d64ec77771f78ce7c`; the standalone updater is 849,920 bytes with SHA256 `4aa5e491a2bff885c668cdc4473488fd05acf44e56eba2aa9d4c242a76a4db6b`. All 50 payload hashes and CRC checks passed. The release adds original VS/PS microcode to F1 captures, consent-gated incremental D1 uploads and bounded nonblocking collection.

The recorded runtime evidence remains tied to the retained source-0.5.0 development binary. Release CI built new 0.5.2 executables; existing functional validation was reused without another game run. Credential scans found no management credentials in the audited source or expanded release package. All four public assets returned HTTP 200 and matched the audited hashes in anonymous download verification.

The standalone updater retains the behavior validated for [v0.5.1](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.1). The separate updater download is byte-identical to the helper inside the 0.5.2 ZIP.

## Current validation and limits

The retained D3D12 RTX 5080 background run used source-0.5.0 executable SHA256 `1beb8a50c5bef1ebf0fb147338b33d558a2193e87f82b0b8c579ecbcb856959d`, 1280×720 experimental TAA, 60 FPS target and Map 16 Main Street. Two periodic uploads added 23 then 32 programs, for 55 total (15 VS / 40 PS, 19,188 bytes). All received sources matched the local cache by bytes, SHA-256, renderer FNV and length; the final snapshot had 55 source rows and associations without duplicate keys, plus 28 matching structured diagnostics. Map 16 windows measured 59.67 FPS / p95 18.036 ms before the second upload and 59.84 FPS / p95 18.067 ms during it. This is bounded background acceptance, not F1 product export, opt-out A/B, universal zero-overhead proof, Vulkan/AMD coverage, flicker repair acceptance or whole-game validation. Evidence: runtime report (`out/v0.5.0/shader-source-collection/runtime/REPORT.md`, retained locally), D1 verification (`out/v0.5.0/shader-source-collection/runtime/D1-VERIFICATION.md`, retained locally) and shader-source report (`out/v0.5.0/shader-source-collection/REPORT.md`, retained locally).

Product F1 ZIP/manifest and immediate post-capture upload remain pending. The attempted `LO_CAPTURE_REQUEST` was a legacy trace request. The latest flicker diagnostic used older executable `ececed95`, before the four c7 paths; it is not a regression acceptance result. F1 frame costs were 1.690/3.156/1.561 seconds, and the later background ZIP attempt timed out at 325.117→385.166 seconds with the original directory retained. See flicker analysis (`out/v0.5.0/taa-flicker-20260910/analysis/REPORT.md`, retained locally).

The current Windows delivery scope is D3D12/Vulkan. DX11, Linux, macOS, experimental Switch work, other GPU coverage, DLC rewards/dungeons and full-game compatibility remain separate validation areas. Open work-item status and priorities are maintained in the [public Maintainer Project](https://github.com/users/freefrank/projects/3); completed change history is in the [CHANGELOG](../CHANGELOG.md).

<a id="live-issue-reconciliation"></a>

## Issue and acceptance boundary

GitHub Issue state, reporter acceptance and implementation evidence are separate facts. Use the [public Maintainer Project](https://github.com/users/freefrank/projects/3) for current work-item state and the [CHANGELOG](../CHANGELOG.md) for completed releases. Dated implementation and investigation evidence remains in `docs/notes/`.

Earlier Issue closure sources and their acceptance limits are preserved in the [archived reconciliation](archive/STATUS-2026-09-10.md#live-issue-reconciliation).

## Archived status snapshot

The former detailed status ledger, including historical candidate identities and dated validation narratives, is preserved in [STATUS-2026-09-10.md](archive/STATUS-2026-09-10.md). It is historical evidence and does not define the current source or release state.
