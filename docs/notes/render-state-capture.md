# Debug Menu render-state capture

Status: **2026-09-06, released in v0.2.1**. This diagnostic feature was not included in v0.2. It is not an AMD rendering fix or a compatibility claim.

## Use

Open the Windows F1 Debug Menu and scroll to **截取渲染状态 / Capture render state**. The button queues capture of the next complete rendered frame. The status changes from waiting to capturing, then compressing and finally the saved ZIP path. Incomplete capture or failed compression is reported with the raw output path. The button is disabled while a request is busy; another request during that interval is ignored. A paused or stalled renderer must produce a new frame before the request can finish.

Output is created under `captures/render-<timestamp>-f<frame>` relative to the process working directory, with a sibling `render-<timestamp>-f<frame>.zip` produced automatically after a successful capture. The absolute ZIP path is displayed in the status. Raw files are retained on both compression success and failure. Export performs GPU readbacks, file writes and compression and can stall the game. Avoid treating capture-time frame rate as normal performance.

## Contents

| File | Meaning |
|---|---|
| `screenshot.bmp`, `screenshot.ppm` | Final resolved frontbuffer for the captured frame; BMP opens directly on Windows. This is the game's resolved surface, not a desktop screenshot or necessarily the final display-scaled image. |
| `render-state.txt` | Frame, GPU/driver identity, draw parameters, first-draw register snapshot, subsequent register changes, shader hashes, resolve dimensions/formats and completion/drop information. Register indexes and values are hexadecimal. |
| `<shader-hash>.hlsl` | Available translated shaders referenced by captured draws, written once per hash. |
| `f<frame>_seq<sequence>_<address>.ppm` | Intermediate resolved color/depth previews. These are 8-bit RGB previews; floating-point values are clamped to 0–1. |
| Matching `.bin` | Resolve data with row padding removed, in the recorded host texture format and little-endian packed rows. Use the recorded width, height, bytes per pixel and format when interpreting it. |
| Matching `.f32` for R32_FLOAT | Packed float32 values for depth resolves, preserving precision lost by the preview. |

Draw-step previews may also be exported by the existing diagnostic path. The register trace records attempted draws; dropped draws and submitted draws can differ. Inspect the final trace line and file contents when assessing whether a capture is complete.

## Implementation and limits

The [Debug Menu](../../LostOdysseyRecomp/debug/menu_window.cpp) requests capture through the thread-safe [renderer interface](../../LostOdysseyRecomp/gpu/renderer.h). The [renderer](../../LostOdysseyRecomp/gpu/renderer.cpp) begins at a frame boundary and exports state while processing that frame. The [command processor](../../LostOdysseyRecomp/gpu/command_processor.cpp) finishes capture at the swap using its frontbuffer. `LO_DEBUG_CAPTURE_SWAP` provides an optional deterministic request for isolated validation.

On Windows, compression uses the system Windows PowerShell executable and .NET `ZipFile` with `Optimal` compression, launched without a visible console. It writes `.zip.partial` and renames it to `.zip` only after successful completion; the process has a 60-second timeout. On failure, the temporary archive is removed where possible and the raw directory remains available.

This is a diagnostic export, **not a replayable GPU capture**: it does not serialize all buffers, textures, commands and synchronization needed to reconstruct execution. FP16 previews hide values outside 0–1; use the raw data to investigate those values. Readback stalls make it unsuitable for performance comparisons. A build without the plume renderer reports that capture is unavailable.

## Validation

The build passed (`out/build-render-capture.log`). On an isolated NVIDIA RTX 5080 title/menu run:

- A deterministic request captured frame 400 in approximately 0.376 seconds. The real Win32 F1 menu exposed button 103 with the expected bilingual label; `BM_CLICK` on that control triggered a second capture at frame 2869. Both completed and rendering returned to about 30 fps.
- `out/render-capture-test/validation.json` records both captures. Each has a 1280×720 BMP and PPM with identical decoded pixels, 16 raw resolves whose sizes equal width × height × bytes per pixel, 21 HLSL files and `screenshot=true` in its completion trace.
- Runtime evidence is in `out/render-capture-test/runtime.log`. This covers a title/menu scene and the actual button command path, not physical mouse usability, complete UI layout/DPI acceptance, battle rendering or AMD behavior.
- The latest build deduplicates shader exports with a set and checks shader output stream failure explicitly. The pre-ZIP preview SHA256 was `4B282C003386B236C5E16A15E27733B85593FF74AB5030195783406707B3CDF7`. With `captures` deliberately created as a file, `out/render-capture-failure-test/runtime.log` reported the directory-creation failure at 6.154 seconds and continued rendering at about 30 fps beyond 28 seconds without crashing. This validates that specific output-directory failure, not every disk-error path.

The local preview is `out/render-capture-preview/LostOdysseyRecomp.exe`; both owned isolated processes were stopped, and the original executable baseline was restored and hash-checked. The feature is included in published v0.2.1. No game operation, commit, push or publication was performed by the documentation workflow.

## Automatic ZIP follow-up

The ZIP-enabled build passed (`out/build-render-capture-zip.log`). Isolated ZIP validation passed in `out/render-capture-zip-test/validation.json`: frame 400 produced 60 files, 91,363,708 raw bytes and an 11,701,650-byte ZIP. Python `ZipFile.testzip` passed, archive filenames exactly matched the raw directory and every archived file SHA256 matched its original. The runtime log records raw completion at 16.591 seconds and ZIP completion at 17.916 seconds, about 1.325 seconds for compression. Rendering returned to about 30 fps through 38.885 seconds after ZIP completion; the owned isolated process was stopped. Timeout and other compression failure paths have not been exercised. This addition is included in published v0.2.1.

## Official v0.2.1 package validation

The published package passed all 44 manifest hashes and installer self-test. Its isolated cold-cache run rendered a visually checked German title menu for 54 seconds. Frame 400 produced a 12,307,207-byte ZIP with 60 entries; CRC checks and every archived file SHA256 matched the raw output. ZIP completion was logged at 43.798 seconds, followed by about 30 fps from 48.758 through 54.757 seconds. Evidence: `out/release-v0.2.1/package-validation.json` and `out/release-v0.2.1/smoke/runtime.log` with its captures. The test process exited, the main executable baseline remained `135DCA79`, and user saves were untouched. This remains title/menu diagnostic validation, not AMD or battle-rendering acceptance.

See [project status](../STATUS.md) and [research index](README.md).
