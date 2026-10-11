# Live v0.5.7 CPU and 4K profiling — 2026-09-13

> **Historical checkpoint.** Version, issue, process and pending-work statements below describe their original date. Recheck current delivery and acceptance in [STATUS](../STATUS.md) and [ROADMAP](../ROADMAP.md).

This record covers diagnosis, two local CPU implementations and limited runtime comparisons; it does not establish stable whole-game performance or player acceptance.

## 1080p sampling

The external Win64 sampler ran once against the running v0.5.7 game for 15.0186 seconds. It collected 13,975 samples across 43 threads. Sampling failure count and thread-recovery failure count were both zero. Render thread TID 17932 accumulated 12.5781 seconds of OS CPU time, approximately 84.27% of one sampled core. The same window recorded 55.71 FPS / 17.95 ms. The retained comparison values were 52.43 FPS / 19.07 ms baseline and 44.39 FPS / 22.53 ms after sampling; reported GPU 3D utilization was 49.91%.

The initial wall-sampling evidence prioritized the CPU render/frame-production/D3D12 submission chain. The Release executable has no debug directory or PDB, so that wall sampler alone provided no source-function or substage attribution; the later machine-code correspondence and WPR findings below establish a specific function hotspot. The sampler itself costs about one CPU core, and a short clang compile overlapped the capture. Those limits apply to the initial diagnosis; later implementation and fixed-scene measurements are recorded below, without claiming whole-game performance acceptance.

## 4K sampling and focus boundary

The separate 4K observation used the published v0.5.7 runtime at effective 3840×2160 internal rendering and 1920×1200 output, with no allocation fallback. During the 15.002-second sample it measured 32.37 FPS / 30.90 ms, approximately 82.6% render-thread single-core use and 72.60% GPU 3D use. The before/after observations were 33.69 / 29.68 ms / 72.68% and 34.15 / 29.28 ms / 80.74%; they are not a controlled A/B. All 32 read-only focus samples remained foreground and visible, so the focus-loss report was not reproduced. No CPU/GPU primary-cause, shader/pass-time or performance-fix claim follows.

## Follow-up

An optional `Run-TAA-Scene-Profile-20260913.cmd` enables `LO_RENDER_TIMING=1`, `LO_FRAME_TIMING=1` and `LO_VERTEX_TIMING=1` for a later candidate launch without changing settings. The 4K report is separate from the 1080p observation and must not be used to infer a single-variable resolution effect.

## WPR stack capture and hotspot

With the user's administrator help, a minimal WPR capture completed successfully. The ETL SHA-256 is `92bb6507e190be31eb00071b49d4f2477987cdb6df8dbe73eb5dc75a3fb84f4a`. Its valid UTC window was 08:08:47.988–08:09:00.863 (12.87454 seconds); event loss was zero, the trace was not truncated, and `PerfInfo/CollectionEnd` confirmed a 1 ms sampling interval.

Render TID 17932 produced 10,752 context samples. 186 `SampledProfile.NonProcess` DPC/ISR contexts were excluded, leaving 10,566 ordinary execution samples, all with stacks. `gpu::geometry_prepare::SampledContent::Visit<Matches>` accounted for 2,474 self samples (23.4147%) and 2,729 inclusive samples (25.8281%) of that ordinary-render denominator. The inclusive total includes 251 `memcmp` leaf samples and four kernel child samples. The optional all-context self ratio is 23.010%; it uses a different denominator and must not be mixed with the ordinary-render percentages.

The hotspot maps to the sampled-content comparison loop in `geometry_prepare.h:66/76/81`, with callers reaching the renderer at `renderer.cpp:3012/3031`. The identity is a cross-build inference supported by independent machine-code, unique-prefix, callee, MAP and source evidence. The original executable has no PDB, so this is a function-level CPU clue rather than source-line timing or proof of the sole frame bottleneck. The existing `52/325` and `39/210` wall-sample matches remain supporting clues only. Evidence: `out/cpu-stack-20260913/elevated/analysis/hotspot-summary.json`, `sampled-content-callers.json`, `sample-interval-events.json` and `cpu-stacks.etl`.

The same window had four heartbeat points but only three complete 60-swap intervals: 180 swaps over 9.109 seconds, or 19.7607 guest swap-heartbeat FPS / 50.6056 ms. Draws ranged from 2,236 to 2,291 and GPU 3D was 91.7253%; all 12 focus samples were foreground and not minimized. This is a guest swap heartbeat rate, not an external display measurement, and does not provide GPU pass timing or a focus-loss comparison.

The user reports a local 20 W TDP cap to approximate a lower-power device, with observed rates of 30–35 FPS at 40 W and 18–20 FPS at 20 W. These are user-provided settings and observations, not sampled package-power measurements or results from an actual Steam Deck. They are numerically consistent with the two 4K windows, but the capture did not directly log historical TDP settings and cannot establish a controlled power-cap A/B. Extra CPU work can lengthen the render thread's execution and may contend for a shared power budget that limits GPU frequency; 91.7253% GPU utilization alone cannot exclude that mechanism. No same-window CPU/GPU power or clock data is available, so power transfer cannot be quantified. The current optimization comparison holds the user-selected 40 W setting; any future power comparison must record CPU/GPU power, clocks and FPS together.

## 60 W observations

After the user changed the local TDP setting to 60 W, two short read-only counter windows used the same PID 25588 and published v0.5.7 executable hash under the same effective 4K/output condition (3840×2160 / 1920×1200). The 60 W value is user-set context; package power, CPU/GPU power and clocks were not measured.

| Window | Focus coverage | Guest swap heartbeat | Render CPU | GPU 3D | Draws |
| --- | --- | ---: | ---: | ---: | ---: |
| session-03 | 0/16 foreground; background, not minimized | 35.6030 FPS / 28.0875 ms; 480 swaps / 13.482 s | 84.2523% | 88.1438% | 2,233–2,293 |
| session-04 | 16/16 foreground, visible, not minimized | 40.5954 FPS / 24.6333 ms; 600 swaps / 14.780 s | 85.2615% | 80.6547% | 2,236–2,308 |

These are guest heartbeat rates, not externally measured display frames. Session-03 was the background condition and session-04 the user-authorized foreground window. Timing, scheduling and scene state were uncontrolled, so the pair neither establishes a focus A/B nor refutes the user's focus-loss observation. No function sampling occurred in these windows; the earlier WPR hotspot percentages remain specific to that WPR window. Evidence: `out/profiling-live-20260913/session-03-60w-observation/{summary.json,report.md}` and `session-04-60w-foreground/{summary.json,report.md}`.

## Isolated function benchmark

One isolated native-function ABBA benchmark completed in 3.60 seconds. Twenty semantic checks passed, all 72 timed batches retained identical checksums, and the benchmark covered equal sampled content with a shared CRT `memcmp`. The original function measured approximately 4.28–4.34 microseconds per call and the candidate 0.286–0.458 microseconds, a 9.45–14.99× isolated ratio. The original and candidate loops contain 376 versus 28 static instructions; their complete functions occupy 1,914 versus 369 bytes. This measures function cost, not game FPS or frame savings.

The original arm used Clang 19.1.5 and the candidate Clang 22.1.8; this function's source was matched, but the environments were not controlled to differ only by compiler. No LLVM-only causality is claimed. At this earlier diagnostic stage no production CPU code had yet changed and no CPU performance acceptance had been performed. The existing candidate also contains the TAA coverage fix, so a game comparison against it would measure the whole candidate rather than a compiler-only change. Evidence: `out/profiling-live-20260913/native-function-benchmark/RESULTS.md` and `results.json`.

## Production CPU implementation and runtime build

The subsequent production change adds `EqualSampleBlock64` in `geometry_prepare.h`: four explicit groups of unaligned 128-bit comparisons combined with AND/movemask, selected by `Matches` only when the compared block is exactly 64 bytes. Other lengths retain the original `memcmp` path. The 8,192-byte threshold, 512-byte head/tail checks, 64×64 sampling and two-slot wait contract are retained. The two page-tail protection guards cover the input pages; they are not a claim of universal before/after guard coverage.

The focused sample-block fixture passed 33,927 checks once with Clang 22.1.8 and once with the portable Clang 19.1.5 build, including direct-object disassembly evidence for the 19.1.5 `Equal64`/`Matches64` branch and no `vpinsrb`. The older 20 semantic cases, 72 ABBA batches and full geometry suite were not rerun. Evidence: `out/cpu-implementation-review-20260913/{implementation-status.json,sample-block-test-result.json,clang19-check/clang19-validation.json}`.

A single incremental runtime Release build completed in 47.786 seconds with seven actions, Clang 22.1.8, source version 0.5.7, and executable SHA-256 `401cf4d4d4f6eafdd5ca6b9350a26741fba45db4d2afc7259eacabf4208bafdf` (83,438,080 bytes). Its source identity is `5a0433ccbf01e858365ed66cb1182049f1efa4e92df64cfea979d8e46d53002a`; PPC input, compile contract and library hash checks passed without PPC recompilation, and local auto-sync remained off. The build head was `1c93c397af7bbeb240feabf90d895f9dfdac8a9c` in a dirty working tree. This is build provenance only; runtime CPU acceptance remains pending.

## Fixed-scene 40 W comparison

The user-selected 40 W setting was used for a fixed `user00` silent background benchmark with effective 3840×2160 internal rendering, 1920×1200 output and D3D12. The release-static run measured 46.787 FPS mean, 37.627 FPS 1% low, 22.326 ms p95 and 23.859 ms p99. The CPU-SIMD run measured 47.074 FPS mean, 42.687 FPS 1% low, 22.090 ms p95 and 22.560 ms p99. The mean increase was 0.61%, which is neither a stable 60 FPS result nor whole-game acceptance.

Both runs had approximately 807–810 draws per frame on the fixed camera, not the earlier F1 plaza's 2,236-draw route. Vertex-match time changed 2.279→0.488 ms (−78.6%), vertex time 3.107→1.362 ms, and draw time 13.217→11.892 ms (−10%). These wall timers overlap and must not be added. GPU queue time changed 20.782→20.661 ms, excluding Present/compositor; this exceeds a 16.67 ms frame budget for this 4K view, so faster CPU stages did not produce 60 FPS.

Render-thread OS CPU time changed from 34.34375 to 32.140625 CPU seconds (−6.41%). Total process CPU time changed from 72.8125 to 73.25 seconds, while per-frame process CPU time remained 34.459→34.454 ms; top non-render threads increased by 2.53125 seconds without a source attribution. A matching FNV tag cross-check identifies the render thread. Compiler validation against Release CI confirms the release and candidate used the same PPC library (138,454,798 bytes, SHA-256 `ba3e4c4dff009d6d8e844c007186a6e5040266875bca6423f8fe26f8d27fb21b`, provenance Clang 22.1.8), excluding PPC library codegen as the difference; host runtime compiler and TAA mapping differences remain, so this is not a compiler-only A/B.

The CPU-SIMD executable `LostOdysseyRecomp-cpu-simd-20260913.exe` was copied beside the unchanged primary executable and its launcher was not executed in this comparison record. Original settings, profile and save hashes remained unchanged. At completion of this diagnostic pass, no production CPU performance acceptance had been established; later release publication is recorded below. Evidence: `out/cpu-opt-40w-20260913/comparison-static-window.json`, `thread-cpu-analysis.json` and `out/cpu-implementation-review-20260913/ppc-release-ci-identity-readonly.json`.

Evidence: `out/profiling-live-20260913/session-01/report.md`, `out/profiling-live-20260913/session-02-4k/report.md`, `out/profiling-live-20260913/session-02-4k/analysis.json`, `out/cpu-stack-20260913/elevated/cpu-stacks.etl`, `out/cpu-stack-20260913/elevated/analysis/`, `out/profiling-live-20260913/native-function-benchmark/`, and `out/taa-live-20260913/delivery.json`.

The same CPU-SIMD executable was then measured for 45 seconds at the same fixed `user00` view with only isolated internal resolution changed. At effective 1920×1080, it reached 59.981 mean FPS, 56.151 FPS 1% low, 17.048/17.248 ms p95/p99, 5.843 ms mean GPU queue and 15.006 process-CPU ms per paired frame. The retained 3840×2160 result was 47.074 FPS, 20.661 ms GPU queue and 34.454 process-CPU ms per paired frame. The isolated test settings were restored to 4K; installed settings remained untouched. This supports a fixed-view 4K pixel/GPU-load limit; it is not a quality-setting recommendation, F1-plaza result or whole-game 60 FPS acceptance. Evidence: `out/cpu-opt-40w-20260913/cpu-simd-resolution-comparison.{json,md}`.

Offline WPR analysis also identified a second concrete CPU cost in the earlier capture: `getenv` accounted for 450/4,393 ordinary samples (10.2436% self) on TID 26308. Cross-build machine-code and caller evidence maps this to the Release `LO_QUERY_TRACE` hook in `debug/query_trace.cpp` (`PPC_FUNC(sub_823CDCA8)`); `create` performed the same environment lookup on each call, and no runtime environment-writer path was found. The production change now caches presence once in a shared helper, preserves value `0` as enabled, and keeps guest-call order and diagnostic results unchanged. It does not alter `poll_wait`, GPU slots or PPC. Evidence: `out/cpu-env-review-20260913/{README.md,finding.json}` and `out/cpu-stack-20260913/elevated/analysis/nonrender-threads.md`.

The combined CPU-SIMD, query-trace-cache and seven-path TAA candidate built successfully in 33.818 seconds with Clang 22.1.8. `LostOdysseyRecomp-cpu-optimized-20260913.exe` is 83,438,080 bytes with SHA-256 `10fc929307e293d6169d858ef974c7cccc2f09831391ae0ef6b46fe9e0bbb711` and source identity `3b64f57b994095c7fa26bb1db97144667aa81cea75d9a461a27867b894395bf7`. Source version remains 0.5.7; PPC was not recompiled and auto-sync was off. The independently named executable was delivered beside the unchanged primary executable; the installed launcher was not started. Evidence: `out/cpu-query-runtime-20260913/{build-result.json,delivery.json}`.

The final sequential fixed-40 W/4K comparison used two N2126 windows with the same Clang 22, TAA seven-path mapping, PPC library and static library. The retained CPU-SIMD baseline measured 47.073714 mean FPS, 42.68743 FPS 1% low, 22.0897/22.5605 ms p95/p99, 34.454374 process-CPU ms per frame and 20.661152 ms GPU queue; the combined candidate measured 47.088493, 41.39641, 22.0346/22.4937 ms, 33.984008 ms and 20.654641 ms respectively. Process CPU fell 1.37%, while render-thread CPU was effectively unchanged (32.140625→32.171875 CPU seconds); query-trace rows and runtime errors were zero with the environment variables absent. Draw mean changed 809.565→818.655 and one candidate interval reached 42.1679 ms, so the sequential NPC variation and single tail frame do not establish a stable CPU/FPS or power benefit. Power, clocks and temperature were not measured; whole-game performance acceptance remains pending. Evidence: `out/cpu-query-runtime-20260913/new-vs-simd-comparison.json` and `out/cpu-query-runtime-20260913/{build-result.json,delivery.json}`.

## Authorized follow-up

The user authorized a reversible optimization pass at a 40 W setting using the existing benchmark scene and data. The implementation, runtime build provenance and fixed-scene comparison are recorded above. At completion of the diagnostic pass, the result was a local diagnostic only; no whole-game performance acceptance had been established.

The user subsequently authorized the v0.5.8 commit, push and release workflow. The source/tag commit `6e6f11cf56ef69082f5be5b049e5d48d58154415` was published through Release CI `34764115203`; ZIP verification covered all 50 manifest files, CRCs, version 0.5.8 and clean commit provenance. This note's source-0.5.7 runtime evidence remains historical evidence for the v0.5.8 release inclusion; it does not constitute new v0.5.8 gameplay or player acceptance.
