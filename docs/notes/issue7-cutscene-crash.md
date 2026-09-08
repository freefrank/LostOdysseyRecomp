# Issue #7: cutscene closure and missing crash diagnostics

## v0.4.2 published — 2026-09-08

Published v0.4.2 includes the Council word-switch repair, nine further PPC semantics corrections and automatic crash-log capture. The exact semantics-r2 candidate completed the full Council scene, restored control, saved and independently reloaded; its frozen v0.4.1 runtime excluded the TAA and background-export changes now combined in v0.4.2. These candidate results are not an official v0.4.2 executable run. CI, package identity/integrity and anonymous download verification pass, as recorded in [STATUS.md](../STATUS.md); functional evidence is reused without repeated application tests. The [Issue #7 reply](https://github.com/freefrank/LostOdysseyRecomp/issues/7#issuecomment-5580160254) links the fix and release; the issue remains open for original-reporter confirmation, and whole-game coverage remains outstanding.

The dated entries below preserve each candidate checkpoint and its validation and publication state.

## 2026-09-07 continuation: reproducible Council crash

A native pre-Council save has now been created and independently reloaded. The exact diagnostic-r2 executable (`c6fc3011ae13`) reproduces a read access violation twice during Kaim's Map 22 audience (`rt_016_1`), about 45 seconds into the scene. The second run records process exit `0xC0000005`. Its automatic runtime log contains the complete essential crash context and optional host stack. This supersedes the earlier lack of a usable local save or a scene reproduction below.

The owned runs use Asia Disc 1, English, FXAA, 1280x720 and 30 FPS. Native slot 06 (`save/user05`) reloads in Map 15 / The Central Station Square at the eastern Main Street exit. Ordinary rightward movement enters Map 16, then the automatic escort reaches Map 22. No teleport or save-anywhere activation is needed to reload or use this slot. Map 22's scripted audience does not permit opening the save menu; the entrance save preserves a controllable starting point. The earlier councilors-only `rt_014`/`rt_015` sequence also uses Map 22 but is a different event.

The fresh log reads the same `rt_016_1b` event set as the attachment. Both final reads map to the last `0x6608` bytes of `np_u39d0_d.xxx`, at package-relative offset `0x347a4`. The corresponding package and final chunk are byte-identical between the two local editions. This establishes a matching event and loading endpoint, without proving the remote package bytes or attributing the fault to a damaged asset.

### Word-switch root cause and local correction

The matching private address map resolves host RVA `0x2139A9A` to `__imp__sub_829DFD58 + 0x31A`. A guest 64-bit `addi` turns zero-extended `0xFFFFFFFF` into `0x100000000`. The guest `cmplwi` bounds check and `rlwinm` table addressing use the low 32 bits, selecting valid case zero; the generated host switch incorrectly uses `.u64`. Its optimized host table load adds `4 * 0x100000000`, exactly explaining the logged invalid address. The jump-table metadata already exists.

The [generator correction](../../tools/patches/XenonRecomp-lostodyssey.patch) emits a `.u32` switch selector. All 843 current project tables were checked against their original word-sized bounds instructions, including the two manually supplied tables. Arithmetic instruction semantics, case labels and default handling are unchanged. The independent generated-code fixture passes 109 checks, including high words, the exact carry case, guest out-of-range branches and guest CTR. Restoring the old selector passes 20 ordinary inputs and then reaches an instrumented unreachable trap on the carry case; this synthetic trap is distinct from the game's observed access violation. See the [test entry point](../../tools/tests/README.md#recompiler-word-switch-regression).

The candidate `v0.4.1-issue7-switch-r1` (EXE SHA256 prefix `86c720a712e4`) completes the full scene from the same native save. It passes the original loading milestone, advances the Council dialogue without a cutscene skip, returns to Map 16 and restores movement and menus. After the guard dialogue and tutorial, a new native slot 07 is written. A separate process independently reloads that slot into the same Map 16 position and confirms normal movement. Save-anywhere only exposes the native save action after the scene; it is not enabled during the independent reload. Both candidate runs have zero crash records and are ended by the harness after verification, rather than establishing natural-shutdown coverage.

This candidate preserves every r2 runtime/GPU object. A baseline relink reproduced the r2 EXE and MAP byte for byte; regeneration changed only 843 selector widths in 159 translation units, with 88 guest objects reused. Evidence and hashes are in `out/issue7-switch-fix/BUILD.md` and `out/issue7-map22-save/runtime-validation.json`. The repair is verified for this local scene and remains uncommitted and unpublished. There is no external player acceptance; the original attachment has no exception context, so equivalence to either external reporter's fault still needs their confirmation. Issue #7 remains open. The separate published `v0.4.1` and battle-TAA development package do not contain this correction.

Evidence: `out/issue7-map22-save/REPORT.md`, `run-01/runtime.log`, `reload-01/{runtime.log,exit-result.json,resource-correlation.json,shot_5660.png}`, the native checkpoint manifest and save-package audit; `out/issue7-switch-fix/{GENERATOR-FIX.md,generator-only.patch,fixture-04/results.json}`. The save ZIP SHA256 prefix is `1536e5a7528d`; its native `save.bin` prefix is `372f42e1950b`. Private saves and captures are not committed with this documentation.

## 2026-09-07 follow-up: semantics implementation

The [recompiler audit's nine confirmed scalar, address and branch defects](recompiler-width-audit.md#2026-09-07-implementation-and-regression) now have production corrections and independent native regression coverage. Fresh decoder/generator builds pass all 3,258 cases; the frozen pre-fix generator fails 1,533 of the same inputs, and 48 existing Rc controls pass both variants. The original 109 word-switch checks and old-selector negative control still pass.

The new `v0.4.1-issue7-semantics-r2` candidate (EXE SHA256 prefix `87e6eb6a3ea0`) rebuilds all 247 generated C++ units with the updated context header, retaining the diagnostic-r2 runtime/GPU objects. Its own run follows Map 15 → 16 → 22 → 16, advancing dialogue with A without skipping the cutscene. Map 22 lasts from 255.392 to 541.632 seconds in this log. Ordinary Main Street movement works afterward, and the native save writes all 206,000 requested bytes with status 0 to slot 07 (`save/user06`). A fresh process reloads that same save into the recorded Map 16 position and confirms ordinary movement again. The save SHA256 prefix is `1eea50368da2`; the original seed hashes remain unchanged.

This regression uses Asia Disc 1, English, FXAA, 1280x720, 30 FPS and copied shader caches. Save-anywhere only opens the native save menu after the scene and restored control; no teleport is used, and the independent reload does not enable save-anywhere. Both owned processes are stopped by the harness with exit code 1 after verification, so this is not natural-shutdown coverage. It establishes the tested Council/save path, not whole-game compatibility, natural reachability of the newly corrected BLRL path or acceptance by the original reporters. The switch-r1 results above retain their earlier executable's scope.

The [tracked XenonRecomp patch](../../tools/patches/XenonRecomp-lostodyssey.patch) now includes the original low-word switch correction and all nine semantics fixes. Applying it to the pinned submodule HEAD reconstructs the tested Git-normalized source exactly, preserving every earlier patch change; its SHA256 prefix is `9a3208a3eb4a`.

The local development ZIP is 42,672,159 bytes, with SHA256 prefix `515c03e36e98`; its EXE matches the runtime above. All 46 payload files plus the manifest pass member-hash and ZIP CRC checks. It contains no game data, saves, profiles or caches. The source changes and package remain uncommitted and unpublished, with no new player acceptance, and exclude the separate TAA and background-capture changes. Local evidence is in `out/issue7-semantics-fix/`: `runtime-validation.json`, `build-review/REPORT.md`, `delivery/delivery-audit.json`, `implementation/{IMPLEMENTATION.md,generated-delta-summary.json,patch-sync.json}` and `tests/REPORT.md`.

## Earlier 2026-09-07 follow-up: similar-error scan

An [offline recompiler audit](recompiler-width-audit.md) found additional address-writeback, wrapping-mask, carry and indirect-control semantic defects, with latent forms separated from current code occurrences. All 44,523 independent evaluations of the 843 known switch tables matched; no second concrete table error was found. This scan made no production correction or game run and does not reopen the locally verified Council repair. New findings have no player acceptance or demonstrated gameplay symptoms.

## Earlier 2026-09-07 diagnostic investigation

The white-room cutscene closure reported in [Issue #7](https://github.com/freefrank/LostOdysseyRecomp/issues/7) has not been reproduced locally or confirmed fixed. The attached log exposes a separate, reproducible diagnostic defect: native crash reports went to `stderr` without reaching the automatic `logs/runtime-*.log` file. A local development change repairs that reporting path. It is uncommitted and unpublished, and is excluded from the separate `v0.4.1` release. There is no player acceptance for the reported cutscene.

## What the attachment establishes

The original reporter lists an RX 7800 XT and `v0.3.0`. The [attached runtime log](https://github.com/user-attachments/files/31930294/runtime-1788822056266554.log) comes from another commenter and records a GTX 1070, USA/Europe resources, English, 1280×720, FXAA and approximately 30 FPS. Its installation directory contains `v0.2.1`, but that name does not verify the executable version. The log also lacks the source-version marker introduced in `v0.4.0`.

The attachment has 603,324 bytes and 7,623 lines, with SHA256 `5a32e8e20336167fbd6ed0266dea0e02fe802f297448c8715ee0ee7a082bee24`. It ends at 200.508 seconds after a successful read from `xenon_event.fpd`, with no exception code, process exit status or normal shutdown marker. The shader compilation warning at 2.274 seconds is approximately 198 seconds earlier; the log does not establish it as the closure trigger.

Mapping the logged reads against local USA/Europe resource metadata gives `u35_0 → u3n_0 → u36_0 → u41_0`, with `rt_016_1b` resources starting at 193.225 seconds. The last request, offset `0x3afbfa4` and length `0x6608`, ends at the local `np_u39d0_d.xxx` package boundary. Eleven logged archive sizes match the local resources, but the reporter's FPI and resource hashes are unavailable. This mapping identifies a scene lead; it does not identify a corrupt or faulting asset.

### 2026-09-07 correction: native map IDs

An earlier version of this note incorrectly paired the map-definition IDs in `out/map-info-current-01/display-fixed.log` (lines 78–86) with font-text display IDs (lines 411–419). That produced the incorrect sequence “14 Sandstone Region → 15 Near the Capital → 16 Great Gate → 22 Uhra - Main Street.” The retained log is historical evidence of that mistaken association, not a valid native-ID/name table. The resource definition `22 = u41_0_scrw` was correct.

Direct reading from the owned diagnostic-r2 process now resolves the native names from the array at `0x832C9728`, using the same map-definition ID indexing as production [map_info.cpp](../../LostOdysseyRecomp/debug/map_info.cpp): each entry is a 12-byte string descriptor pointing to UTF-16BE text. Evidence: `out/issue7-map22-save/native-map-labels.json` and `native_maps.py`, read from PID 68248 / EXE SHA256 prefix `c6fc3011ae13`.

| Native map ID | Resource | Native name |
|---|---|---|
| 14 | `u35_0` | Monorail - The Central Station |
| 15 | `u3n_0` | The Central Station Square |
| 16 | `u36_0` | Main Street |
| 22 | `u41_0` | Uhra Council - Council Hall Interior |

The mapped route is therefore central station → station square → main street → **Map 22, Council Hall Interior**. The same native array identifies Map 13 as Inside the Monorail Car and Map 23 as the Council Chairman's Office. The reported meeting and white-room scene fit the first arrival in Uhra followed by the Council meeting with Gongora; the exact cutscene/fault association remains an inference until reproduced. The [public walkthrough](https://walkthrough.freeola.com/game/9481/xbox-360/lost-odyssey.html) describes the early route from the armored transport through the gate, tower and monorail, then the central station and station square before the automatic Council scene.

## Reporting change

The production [log sink](../../LostOdysseyRecomp/os/log_file.cpp) opens an independent append-only emergency handle before a crash. The [crash handler](../../LostOdysseyRecomp/os/crash_handler.cpp) writes exception/access details, host registers, thread and source version first, then module/RVA information and readable PPC context before optional guest-memory and symbol diagnostics. These writes bypass the normal logger mutex and CRT file locks. Invalid guest context and malformed or unreadable dump operands are handled without direct dereferencing.

Native exceptions retain their original exit code. Explicit main-thread `std::terminate` and `SIGABRT` receive diagnostic records; MSVC's per-thread terminate behavior means an uncaught main-thread C++ exception and a new worker's default terminate can take different recorded paths. A valid runtime sink takes priority over mirroring to a Windows `stderr` pipe, which could otherwise block when full. The optimized build retains a private `.map` file for address lookup; this file is excluded from the package.

This remains in-process crash reporting. External process termination and fail-fast paths can bypass it, stack-exhaustion coverage is unverified, and unavailable storage cannot be made reliable by the handler. A source-version string alone does not distinguish a local diagnostic build from the release it is based on; preserve the executable hash and package manifest with the report.

## Validation and evidence

The retained baseline probe causes a real write access violation while holding the logger mutex. It exits with `0xC0000005`: `stderr` contains the crash report, while the runtime log contains only its pre-fault record. The candidate probe preserves that exit code and writes the exception to both files. This verifies the diagnostic defect and its correction independently of the reported game scene.

The final main build, `LoLogCaptureTest` and all 14 `LoCrashCaptureTest` child-process cases passed. Coverage includes a full redirected `stderr` pipe and faults while the normal logger and CRT stream locks are held. The `v0.4.1-issue7-diagnostic-r2` package's actual executable was then checked in an isolated headless process: after normal guest entry, an induced execute access violation produced its exception and source version in the automatically created runtime log and exited with `0xC0000005`. This establishes the integrated logging path, without reproducing the original cutscene.

The local r2 ZIP has SHA256 prefix `77805a9f7e6a`; its EXE has prefix `c6fc3011ae13`. All 46 manifest payload hashes and ZIP CRC checks passed. These package checks establish artifact identity and integrity, not gameplay recovery or publication.

Local evidence is retained under `out/issue7-investigation/`: `REPORT.md`, `log-analysis.json`, `resource-timeline.json`, the baseline/candidate probe results, `crash-capture-verified/results.json`, `production-crash-r2/result.json` and `delivery-audit-r2.json`. The source attachment, earlier r1 evidence and private build address maps remain with that investigation. Reproduction commands and coverage boundaries are in the [test guide](../../tools/tests/README.md#crash-capture-diagnostics).

## Evidence needed to continue

The attachment loads `save/user16/save.bin`, but that pre-cutscene save is not attached to the issue. Initially no suitable pre-cutscene save was known locally. A reporter's save slot and associated metadata, or a fresh same-route log from an identified diagnostic build, remain useful reproduction evidence. Preserve the actual executable/package identity with either route. Original-scene recovery and original-hardware behavior remain unverified; diagnostic test passes do not close Issue #7.

Later on 2026-09-07, the user offered to progress through the game and then requested a native save near the Map 22 point of interest. A complete older `user03` monorail-car save has since been found and copied into the isolated `out/issue7-map22-save/run-01` workspace, where diagnostic r2 has been started. A new target-position save is still being prepared; the original closure has not been reproduced and no fix acceptance is recorded.
