# Debug Menu render-state capture

Reviewed **2026-09-08**. Published v0.4.2 includes background ZIP compression, cleanup after success and default three-log retention. The actual export and retention results below belong to the identified capture candidate; v0.4.2 CI, package and anonymous-download checks pass as recorded in [STATUS.md](../STATUS.md), reusing the existing functional evidence. Published v0.4.1 introduced three-frame captures with a shared runtime log and smaller contents; the original single-frame capture shipped in v0.2.1. Capture remains diagnostic and does not repair rendering or establish compatibility.

<a id="background-archive-dev"></a>

## 2026-09-07 candidate evidence: background ZIP and three-log retention

After all three requested frames and the runtime-log snapshot are written and closed, compression now runs in a background worker. The F1 status reports **后台压缩 ZIP，可继续游戏 / Compressing ZIP in background**. Ordinary frame processing only polls for completion, so it no longer waits for ZIP compression; the capture button remains busy until archiving and cleanup finish. GPU readbacks and capture file writes still pause rendering. This change does not turn F1 into an uninterrupted performance or temporal-stability recording.

The worker receives only the completed directory path, not renderer resources. On Windows it uses the system PowerShell executable and .NET `Optimal` compression, without a visible console, at below-normal priority. It retains the 60-second timeout and writes a sibling `.zip.partial`, then renames that file to `.zip` after the compressor succeeds. Only then is the matching plain `captures/render-*` directory removed; unrelated captures are not swept. Existing ZIP or partial paths cause failure instead of replacement.

Compression failure preserves the raw directory and removes this attempt's partial file where possible. ZIP success followed by source-cleanup failure reports both outcomes separately: the ZIP remains valid and some source files may remain. A normal SDL window close waits for a started archive before exiting; closing before all three frames have been captured does not create a complete archive. The Windows job object limits an abrupt process exit from leaving an orphan compressor, but forced termination is not a successful-export guarantee.

Default startup logging now keeps the successfully opened current log plus the two newest other `runtime-<digits>.log` files by numeric filename timestamp. The current file is protected even if the clock moves backward. Cleanup does not recurse, follow non-regular entries, or use modification time to decide which log is newest. Active or undeletable older files remain for a later launch, so the folder can temporarily contain more than three. A failed default log open does not trigger pruning; custom `LO_LOG_FILE` paths, including disabled logging, do not opt into retention. The existing current-process snapshot contract remains unchanged.

Implementation: [archive worker](../../LostOdysseyRecomp/os/capture_archive.cpp), [renderer lifecycle](../../LostOdysseyRecomp/gpu/renderer.cpp), [window-close wait](../../LostOdysseyRecomp/gpu/video.cpp) and [log retention](../../LostOdysseyRecomp/os/log_file.cpp). The isolated source uses `taa-fix` commit `5d4f5c9` plus only this capture/retention change, excluding unrelated Issue #7 and recompiler edits. The main executable and both CMake fixtures build successfully. Final EXE SHA256 is `5fc9190cc7eb97eb1eae567dadaad310837b368631b988c08d23306b2faafa39`.

Real-file `LoCaptureArchiveTest` checks pass for caller progress during compression, Unicode/apostrophe/backtick/dollar-sign paths, successful source removal, ZIP/partial collisions, unreadable-source preservation, saved-ZIP cleanup failure, future destruction waiting and rejection outside a capture child directory. Independent Python checks validate CRCs for three fixture ZIPs and all bytes of the 8 MiB synthetic payload. `LoLogCaptureTest` passes against both the working tree with the separate crash diagnostics and clean-HEAD retention sources: numeric/mtime ordering conflicts, clock rollback, active and read-only files, retry, unrelated entries, continued logging and existing snapshot/alias/concurrency cases are covered. These Windows results do not exercise POSIX file locking or all timeout/forced-exit paths.

Five actual-executable initialization runs produce retained default-log counts of 1/2/3/3/3; a sixth run using custom `LO_LOG_FILE` leaves neighboring logs unchanged. These deliberately missing-game runs exit 1 before game loading. Evidence: `out/capture-background/startup-retention-validation.json`.

The first capture run reached frame 400 but was stopped by the validator when strict UTF-8 decoding found local-code-page bytes in two existing capture path messages. It is not a successful runtime ZIP result. Both messages now convert the path explicitly to UTF-8; the rebuilt executable passed the separate runtime check below. Build/fixture evidence: `out/capture-background/{build.json,archive-fixture-validation.json,retention-test/validation.txt,cmake-archive-test.log,cmake-log-test.log}`; invocation is in [test instructions](../../tools/tests/README.md).

### Actual-game validation

An isolated 720p title/menu run in a Chinese/backtick working directory captured frames 400/401/402. Its ZIP is 35,587,653 bytes, SHA256 `d3ea6cbe4fbf5d6782972426b1d45c1f958cf3e729878fd35e479f5bdeb10705`, with 132 entries, 48 correctly sized raw resolves and 21 shared shaders. CRC checks pass; the archived 754,943-byte log is valid UTF-8 and an exact current-process prefix. Only the ZIP remains for this capture, with no raw source directory or partial archive.

Compression started at log time 41.461 seconds and completed at 45.398 seconds, a 3.937-second window. Per-frame renderer records advance from 403 through 520 during that interval, with completed-present samples at 406/437/467/497. Complete one-second windows after capture readback are about 30 FPS. The first sampled window includes readback stalls and cannot be used as normal-speed evidence. These records establish rendering progress during this archive; they do not establish zero compression overhead or uninterrupted capture frames.

This run retained the current default log plus `runtime-5.log` and `runtime-6.log`, and preserved `notes.log`. It reused the same task's isolated shader cache prepared during the first run, so no cold-cache conclusion is drawn. The owned process was ended and its seed/executable hashes were unchanged. Evidence: `out/capture-background/runtime-validation.json`.

The tested executable and manifest were subsequently deployed to the existing installation. The previous `fe39a9c9…` executable and manifest are retained under local `out/capture-background/user-backup/`. All 45 installed manifest hashes match; six protected save/profile/settings/game-path files retain their hashes. Only the executable and manifest were updated, and the original installation was not launched during deployment verification. Evidence: `out/capture-background/deployment.json`.

Normal-close waiting has source review and the future-destruction fixture; the actual SDL close route, timeout and forced termination were not separately exercised. These changes are on the `taa-fix` development branch and are not included in a published release, with no new player acceptance or enemy-disappearance/TAA repair. The published v0.4.1 and v0.2.1 records below retain their original synchronous-compression and raw-file-retention behavior.

## Published v0.4.1: three-frame capture

This section records the published release behavior. The local background worker and successful-export source cleanup above are not included in v0.4.1.

Open the Windows **F1 Debug Menu** and select **截取渲染状态 / Capture render state**, the first button. One request captures the next three consecutive rendered frames and produces one ZIP. Progress identifies each frame before compression, then displays the absolute saved ZIP path. The button stays disabled while capture is busy. A paused or stalled renderer must produce new frames to complete the request.

The raw directory is `captures/render-<timestamp>-f<first-frame>` relative to the process working directory; its sibling `.zip` is created only after all three frames complete successfully. Raw files are retained. A frame or sequence failure stops the request and reports incomplete output instead of a successful three-frame archive; compression failure also leaves the raw directory available. `capture-info.txt` records `requested_frames=3`, `completed_frames`, `first_frame`, `last_attempted_frame` and `status` (`capturing`, `complete` or `incomplete`). If the output directory cannot be written, even this record may be unavailable.

### Current contents

| Location | Contents |
|---|---|
| `capture-info.txt` | Requested/completed frame counts, sequence identity and export status. |
| `frame-01-fN/`, `frame-02-fN+1/`, `frame-03-fN+2/` | Separate diagnostics for each consecutive renderer frame; actual directory names contain numeric frame IDs. |
| Each frame: `screenshot.bmp` | Final resolved guest frontbuffer. It is not a desktop screenshot or proof of final display scaling/AA. |
| Each frame: `render-state.txt` | Frame and GPU/driver identity, source version, configured graphics settings, draw/register trace, shader hashes, resolve dimensions/formats and completion/drop information. |
| Each frame: `temporal-surfaces.json`, `temporal-scene.json` | Surface identity and observed scene/depth metadata. These records do not establish that TAA ran. |
| Each frame: resolve `.bin` and `.ppm` files | All exported resolves retain packed raw data and viewable previews. Interpret `.bin` using the recorded format, dimensions and bytes per pixel; R32_FLOAT depth remains little-endian float32 in `.bin`. Previews clamp floating-point values to 0–1. |
| `shaders/<shader-hash>.hlsl` | Available translated shaders, deduplicated across all three frames. |
| `runtime.log` | One snapshot of the active process log file, flushed after the last captured frame and before ZIP creation, when available. |
| `runtime-log-status.txt` | Last captured frame and `status=included` with the filename/scope, or `status=unavailable` with a reason. |

The default export omits draw-step previews, duplicate `screenshot.ppm` and duplicate depth `.f32`; it preserves per-frame register traces, resolve previews and raw depth/color data. Set `LO_DEBUG_CAPTURE_DRAW_STEPS=1` before launching the intended diagnostic process to include draw-step previews. The separate `LO_DUMP_RESOLVE_SEQ` / `LO_DUMP_RESOLVE_DIR` diagnostic path still writes its depth `.f32` files.

The trace records configured output size, internal-resolution selection, window mode, AA, scaling quality and frame-rate setting. AA IDs are 0 Off, 1 FXAA, 2 SMAA and 3 experimental TAA. Settings and the source-version string do not establish effective per-draw AA, physical output dimensions or exact executable identity; inspect the surfaces, draw state and runtime evidence as well.

The [logger snapshot helper](../../LostOdysseyRecomp/os/log_file.cpp) retains the absolute path opened by this process and holds the logger mutex while flushing and copying the whole file. It does not choose another file by modification time or truncate the snapshot. A custom `LO_LOG_FILE` path is decoded as UTF-8; because the log opens in append mode, reusing that path may also retain earlier sessions. `LO_LOG_FILE=0`, an unopened log or a snapshot error produces an unavailable reason without discarding the render capture; a possible partial snapshot is removed where possible. ZIP-completion messages and later runtime output occur after the snapshot and are therefore outside its scope.

GPU readbacks, file writes and compression can stall execution. Three consecutive renderer frames provide a bounded sequence, not an uninterrupted performance recording or proof of temporal stability. The register trace precedes host-side constant adjustments. The newly reported AMD blur/flicker root-cause investigation is suspended at the user's request; this export change does not resolve that report or reopen the separate suspended first-battle investigation.

### Development validation and package boundary

The integrated three-frame build and isolated 720p title/menu export passed (EXE SHA256 `246585e67d854d43c49c1b050b5610f1b1f9b799946adb185b5c3b038aebe229`). The implementation was subsequently included in the r2 development package and v0.4.1. The earlier frozen `0.4.1-dev` package (ZIP SHA256 prefix `98993a88`, EXE `6d3bc037`) contains the single-frame log addition and source/configuration metadata, but not the three-frame layout or default size reductions.

v0.4.1 was published at **2026-09-08 00:03:50 UTC** from `eb43f108d2cdd3aef682a32b202425c28d168472`. Release CI, all 45 manifest entries, installer self-test, eight headless startup-path checks and anonymous download verification passed. Official EXE SHA256 is `9e0e13d991830de84d7fb85ac7a2543f779dbf7936ee5acd4cabe7cce5b2c57f`, with source version `0.4.1`; capture code is unchanged from r2. These official-package checks did not load a game or repeat capture/GPU validation. The actual three-frame export evidence remains the development run below; see [release evidence](../STATUS.md) for package hashes and verification records.

The three-frame ZIP contains separate directories for frames 400/401/402, 132 entries, 48 raw resolves with verified lengths and 21 shared HLSL files. CRC checks and every archived file's bytes match the retained raw output. Its 706,805-byte `runtime.log` is an exact prefix of the current process log, includes completion of all three frames and excludes a newer unrelated log. Rendering continued beyond frame 420 after compression; the owned process was ended and the seed/executable hashes stayed unchanged. Evidence: `out/render-log-20260907/three-frame-validation.json`. The initial offline validator incorrectly required a shader file for an unbound zero hash; correcting that assertion allowed the same retained ZIP and subsequent runtime log to pass without a source change or another game run. Sequence-failure and opt-in draw-preview runtime paths remain untested by this run.

The production `LoLogCaptureTest` fixture passed after adding explicit source-alias rejection (`out/render-log-20260907/fixture-fixed.log`). An earlier single-frame implementation (EXE `e030418d`) passed an isolated title/menu export: 64 ZIP entries, the included log matched the active log prefix, a newer unrelated log was excluded, and rendering continued afterward. The frozen `6d3bc037` executable adds the fixture-tested alias guard; the earlier ZIP run does not establish the later three-frame path. See [test instructions](../../tools/tests/README.md).

Removing the omitted categories from the older supplied single-frame ZIP, while retaining its original compressed entry streams, estimates 200.30 MiB → 77.73 MiB (61.19% smaller). The actual three-frame title/menu ZIP is 35,727,693 bytes (34.07 MiB). Its 720p scene is not directly comparable with the older 1440p battle sample; neither result is a measured runtime speedup or an AMD visual acceptance result.

## Historical single-frame capture: v0.2.1

The following format and validation describe the original release, first included in v0.2.1 and absent from v0.2. They do not establish the current three-frame behavior.

### Use

Open the Windows F1 Debug Menu and scroll to **截取渲染状态 / Capture render state**. The button queues capture of the next complete rendered frame. The status changes from waiting to capturing, then compressing and finally the saved ZIP path. Incomplete capture or failed compression is reported with the raw output path. The button is disabled while a request is busy; another request during that interval is ignored. A paused or stalled renderer must produce a new frame before the request can finish.

Output is created under `captures/render-<timestamp>-f<frame>` relative to the process working directory, with a sibling `render-<timestamp>-f<frame>.zip` produced automatically after a successful capture. The absolute ZIP path is displayed in the status. Raw files are retained on both compression success and failure. Export performs GPU readbacks, file writes and compression and can stall the game. Avoid treating capture-time frame rate as normal performance.

### Contents

| File | Meaning |
|---|---|
| `screenshot.bmp`, `screenshot.ppm` | Final resolved frontbuffer for the captured frame; BMP opens directly on Windows. This is the game's resolved surface, not a desktop screenshot or necessarily the final display-scaled image. |
| `render-state.txt` | Frame, GPU/driver identity, draw parameters, first-draw register snapshot, subsequent register changes, shader hashes, resolve dimensions/formats and completion/drop information. Register indexes and values are hexadecimal. |
| `<shader-hash>.hlsl` | Available translated shaders referenced by captured draws, written once per hash. |
| `f<frame>_seq<sequence>_<address>.ppm` | Intermediate resolved color/depth previews. These are 8-bit RGB previews; floating-point values are clamped to 0–1. |
| Matching `.bin` | Resolve data with row padding removed, in the recorded host texture format and little-endian packed rows. Use the recorded width, height, bytes per pixel and format when interpreting it. |
| Matching `.f32` for R32_FLOAT | Packed float32 values for depth resolves, preserving precision lost by the preview. |

Draw-step previews may also be exported by the existing diagnostic path. The register trace records attempted draws; dropped draws and submitted draws can differ. Inspect the final trace line and file contents when assessing whether a capture is complete.

### Implementation and limits

The [Debug Menu](../../LostOdysseyRecomp/debug/menu_window.cpp) requests capture through the thread-safe [renderer interface](../../LostOdysseyRecomp/gpu/renderer.h). The [renderer](../../LostOdysseyRecomp/gpu/renderer.cpp) begins at a frame boundary and exports state while processing that frame. The [command processor](../../LostOdysseyRecomp/gpu/command_processor.cpp) finishes capture at the swap using its frontbuffer. `LO_DEBUG_CAPTURE_SWAP` provides an optional deterministic request for isolated validation.

On Windows, compression uses the system Windows PowerShell executable and .NET `ZipFile` with `Optimal` compression, launched without a visible console. It writes `.zip.partial` and renames it to `.zip` only after successful completion; the process has a 60-second timeout. On failure, the temporary archive is removed where possible and the raw directory remains available.

This is a diagnostic export, **not a replayable GPU capture**: it does not serialize all buffers, textures, commands and synchronization needed to reconstruct execution. FP16 previews hide values outside 0–1; use the raw data to investigate those values. Readback stalls make it unsuitable for performance comparisons. A build without the plume renderer reports that capture is unavailable.

### Validation

The build passed (`out/build-render-capture.log`). On an isolated NVIDIA RTX 5080 title/menu run:

- A deterministic request captured frame 400 in approximately 0.376 seconds. The real Win32 F1 menu exposed button 103 with the expected bilingual label; `BM_CLICK` on that control triggered a second capture at frame 2869. Both completed and rendering returned to about 30 fps.
- `out/render-capture-test/validation.json` records both captures. Each has a 1280×720 BMP and PPM with identical decoded pixels, 16 raw resolves whose sizes equal width × height × bytes per pixel, 21 HLSL files and `screenshot=true` in its completion trace.
- Runtime evidence is in `out/render-capture-test/runtime.log`. This covers a title/menu scene and the actual button command path, not physical mouse usability, complete UI layout/DPI acceptance, battle rendering or AMD behavior.
- The latest build deduplicates shader exports with a set and checks shader output stream failure explicitly. The pre-ZIP preview SHA256 was `4B282C003386B236C5E16A15E27733B85593FF74AB5030195783406707B3CDF7`. With `captures` deliberately created as a file, `out/render-capture-failure-test/runtime.log` reported the directory-creation failure at 6.154 seconds and continued rendering at about 30 fps beyond 28 seconds without crashing. This validates that specific output-directory failure, not every disk-error path.

The local preview is `out/render-capture-preview/LostOdysseyRecomp.exe`; both owned isolated processes were stopped, and the original executable baseline was restored and hash-checked. The feature is included in published v0.2.1. No game operation, commit, push or publication was performed by the documentation workflow.

### Automatic ZIP follow-up

The ZIP-enabled build passed (`out/build-render-capture-zip.log`). Isolated ZIP validation passed in `out/render-capture-zip-test/validation.json`: frame 400 produced 60 files, 91,363,708 raw bytes and an 11,701,650-byte ZIP. Python `ZipFile.testzip` passed, archive filenames exactly matched the raw directory and every archived file SHA256 matched its original. The runtime log records raw completion at 16.591 seconds and ZIP completion at 17.916 seconds, about 1.325 seconds for compression. Rendering returned to about 30 fps through 38.885 seconds after ZIP completion; the owned isolated process was stopped. Timeout and other compression failure paths have not been exercised. This addition is included in published v0.2.1.

### Official v0.2.1 package validation

The published package passed all 44 manifest hashes and installer self-test. Its isolated cold-cache run rendered a visually checked German title menu for 54 seconds. Frame 400 produced a 12,307,207-byte ZIP with 60 entries; CRC checks and every archived file SHA256 matched the raw output. ZIP completion was logged at 43.798 seconds, followed by about 30 fps from 48.758 through 54.757 seconds. Evidence: `out/release-v0.2.1/package-validation.json` and `out/release-v0.2.1/smoke/runtime.log` with its captures. The test process exited, the main executable baseline remained `135DCA79`, and user saves were untouched. This remains title/menu diagnostic validation, not AMD or battle-rendering acceptance.

See [project status](../STATUS.md) and [research index](README.md).
