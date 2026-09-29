# City performance capture

`drive-city.ps1` is an **active game driver**, not a read-only analyzer. It
launches the supplied Windows build, sends automated menu and movement inputs,
requests screenshots, writes logs and a summary, then stops the process it
started. The game can also change its save files. Provide an isolated build,
output directory, and the player's save directory explicitly. The script
compares save metadata before and after but does not restore saves.

```powershell
pwsh -File tools/perf/drive-city.ps1 -Help
pwsh -File tools/perf/drive-city.ps1 `
  -RunDirectory 'C:\path\to\isolated-game-build' `
  -OutputDirectory 'C:\path\to\capture-results' `
  -PlayerSaveDirectory 'C:\path\to\player-save'
```

The driver stores screenshots under `<RunDirectory>/shots2`, `pid.txt` under
`<RunDirectory>`, logs and input controls under the system temporary directory,
and `drive-summary.json` / `drive-status.json` under `<OutputDirectory>`. If
available it runs the adjacent `classify-city-timing.ps1` on the resulting log.

`analyze-city-comparison.py` only reads saved summary and log files; its JSON
results go to stdout. Relative `log` and `retained_log` fields resolve next to
the summary file, independent of the shell's working directory:

```sh
python tools/perf/analyze-city-comparison.py /path/to/run-a/drive-summary.json /path/to/run-b/drive-summary.json
```

## DLSS FG game capture

`run-fg-game.ps1` is the bounded game driver for experimental Windows
DLSS/FSR and Streamline FG paths. Use `-Backend D3D12|Vulkan` and
`-Upscaler Dlss|Fsr|Off` to override the isolated run configuration; `-Quality`
accepts values `0..3`. For the reusable D3D12 path, `-FgProvider Settings|Legacy|Off|Dlss|Fsr`,
`-FgMode Fixed|Dynamic`, `-FgMultiplier 2..16`, and `-FgTargetFps 0..1000`
select the provider and runtime policy for an explicit provider. These values
are passed as
`LO_FG_PROVIDER`, `LO_FG_MODE`, `LO_FG_MULTIPLIER`, and `LO_FG_TARGET_FPS`, and
are recorded in `run.json`. `Settings` leaves FG selection to the copied
`settings.ini` and the in-game Frame Generation section; it does not inject
FG environment overrides. `Legacy` preserves the existing `LO_DLSS_FG`
selection; `-DisableFg` cannot be combined with `Settings`, `Dlss`, or `Fsr`.
`-DisableObjectMotion` sets `LO_MV_REPLAY=0` for a
camera/depth hybrid comparison and records `object_motion=false` in `run.json`.
`-CaptureMode Diagnostic|Lightweight` selects the capture overhead; `Diagnostic`
is the default and enables `LO_RENDER_TIMING` plus motion logging, while
`Lightweight` leaves those diagnostics disabled for lower-overhead runs. The
mode is recorded in `run.json` as `capture_mode`, `render_timing`, and `mv_log`.
PowerShell syntax parsing passed. The parser check itself does not launch the
game; bounded runtime evidence is documented separately below.
`-Background` runs the bounded driver without foreground interaction and uses
the owned window close path; it cannot be combined with `-WindowCycle`.
`-DisableHybridMotion` sets `LO_SR_HYBRID_MV=0` only for the isolated child
process and records `hybrid_motion=false` in `run.json`. `-HiddenResizeCycle`
requires `-Background`; it resizes the owned hidden SDL window around 35 seconds
and restores its original client size around 50 seconds. The before/after window
and foreground measurements, actual client-size changes, and failures are saved
in `hidden-resize-cycle.json`.
`-CaptureScreenshots` asks the game to write serialised internal screenshots
through `screenshot-request.txt` in the run directory. Both options are
recorded in `run.json` and are intended for isolated evidence runs.
The request file must receive a new nonzero serial and count, for example
`1 1`, before the game writes a requested screenshot. `-InputRequestPath`
supplies a bounded input request file through `LO_TEST_INPUT_FILE`, allowing
scripted menu navigation in the isolated child; the path is recorded in
`run.json`.
`-ValidationLayerDirectory` optionally injects the Vulkan validation layer from
the supplied directory. The directory must contain
`VkLayer_khronos_validation.json`; the driver enables synchronization
validation and records the requested directory and settings in `run.json`.
The validation log and loaded-layer evidence still determine whether a run is
clean; passing this option alone is not a validation result.
FG remains the experimental Windows Vulkan path. The
driver copies `settings.ini`, `save`,
`profile`, and `shaders` from the supplied baseline into a new output run,
copies the executable and required DLLs, sets `LO_DLSS_FG=1` (or `0` with
`-DisableFg`), mutes audio, uses an isolated shader cache, enters the Uhra route,
and sends bounded movement/input pulses. It stops only the process it started;
the script records executable identity, stdout/stderr, runtime log, termination
state, and baseline metadata preservation.

The bounded D3D12 Uhra camera-only runs used `-DisableObjectMotion` for DLSS
Quality, FSR Quality, DLAA and FSR Native AA; each ran 65 seconds with exit 0,
no forced stop and preserved the baseline. These runs cover the camera/depth
hybrid fallback and do not establish broader scene coverage or player
acceptance.

The initial 2026-09-27/28 bounded D3D12 FG checks were muted background runs. The DLSS2
SR Off run (`run02-dlss2-offsr`, 75 seconds) exited 0 with the baseline preserved;
after the same-queue repair its metadata, ordered-input and matched-input flags
were true, with `active=true` and `accepted=true`. Its hidden resize completed
2560×1440 → 2048×1152 → 2560×1440 and runtime recovery was logged, while
`generated_intervals=0`. The FSR FG SR Off run (`run03-fsr2-offsr`, 75 seconds)
and the dynamic DLSS FG target-144 plus FSR SR Quality run (`run04`, 75 seconds)
also exited 0 with `forced_stop=false`, preserved baselines, and completed the
same hidden resize and recovery. `run03` recorded 2,459 FSR generation
dispatches. Those dispatches are SDK work, not display-frame measurements, and
the generic `actual_presents=0` field is unfilled for this FSR path.
`run05-fsr2-dlsssr` used FSR FG with DLSS SR Quality for 65 seconds; it exited 0,
preserved the baseline, reported `active=true` and `accepted=true`, and recorded
1,682 FSR generation dispatches. These runs are bounded background evidence and
do not establish physical display rate, image quality, pacing, UI separation,
provider parity or a complete playthrough.

The first foreground muted Uhra run, `run06-front-dlss2-offsr`, exited 0 with
`forced_stop=false` and the baseline preserved. Its final sample recorded
`source=3720`, `generated_intervals=2703`, and `actual_presents=5407`; the desktop
capture showed the Uhra scene with OSD 120. PresentMon started too late and its
CSV is unusable. `run07-front-dynamic-fsrsr` used dynamic DLSS FG target 144
with FSR Quality and exited 0 with the baseline preserved; its final sample
recorded `source=3240`, `generated_intervals=2247`, and `actual_presents=6752`.
A continuous interval showed 120 source frames to about 360 presents. PresentMon
captured 1,428 display events on one swapchain, all `Composed: Flip`, with mean
6.954472 ms, median 6.9444 ms, and p95 6.9691 ms, approximately 144 OS display
events per second. Treat these as OS display events rather than physical scanout
measurements. The desktop capture showed the Uhra scene with OSD 144.
`run08-front-fsr2-offsr` ran 75 seconds with FSR FG and SR Off, exited 0 with
the baseline preserved, and recorded 2,965 FSR generation dispatches. PresentMon
captured 1,192 `Composed: Flip` events on one swapchain with mean 8.34884077 ms,
median 6.9485 ms, and p95 13.8963 ms, approximately 119.8 OS display events per
second; the desktop capture showed Uhra with OSD 119. `run09-front-fgoff` ran
SR Off with `LO_FG_PROVIDER=off` overriding `LO_DLSS_FG=1` for 65 seconds, exited
0 with the baseline preserved, and recorded zero snapshot and ordered-input
samples because the provider was not initialized. PresentMon captured 596 events
with mean 16.72094614 ms, median 13.9121 ms, and p95 27.7699 ms, approximately
59.8 OS display events per second. All nine summaries are retained in
`out/acceptance/results.json`; all exited 0 without forced stop, preserved the
baseline, and had no `[error]` log entries. Seven SDK warnings remained on each
foreground DLSS run, so these runs are not SDK or validation-clean evidence.
Foreground evidence is limited to the local RTX 5080, driver 616.56, Uhra and
65–75 second windows. Hidden resize evidence covers only the three background
runs. The checks do not establish complete playthrough, HUD/UI separation, broad
scene coverage, frame-by-frame interpolation image quality or physical scanout.

The FG game path must be built with `LO_ENABLE_STREAMLINE_FG=ON` and a local
pinned Streamline SDK. The runtime flag is opt-in and defaults to off. A typical paired
capture is:

```powershell
pwsh -File tools/perf/run-fg-game.ps1 `
  -BuildDirectory 'C:\path\to\fg-build' `
  -BaselineDirectory 'C:\path\to\baseline-install' `
  -OutputDirectory 'C:\path\to\fg-on-run' `
  -GameDirectory 'C:\path\to\game-data' `
  -Seconds 120

pwsh -File tools/perf/run-fg-game.ps1 `
  -BuildDirectory 'C:\path\to\fg-build' `
  -BaselineDirectory 'C:\path\to\baseline-install' `
  -OutputDirectory 'C:\path\to\fg-off-run' `
  -GameDirectory 'C:\path\to\game-data' `
  -Seconds 120 -DisableFg
```

This is a capture harness, not an acceptance test. The 2026-09-27 bounded
RTX 5080 Vulkan runs are retained under `out/fg-game-20260926/`: the first run
hit a 150% DPI present/render-awareness and `OUT_OF_DATE` rebuild loop;
`run02-dpi` then completed 100 seconds with 3,551 generated intervals and 8,111
presents after the PMv2 opt-in fix. `run03-immediate` completed 100 seconds with VSync
disabled, 4,162 generated intervals, 9,322 presents, SDK errors 0, and a valid
desktop capture. The application log's SDK error count is not a validation-layer
clean result. `run05-window-cycle` also completed 70 seconds with 2,599 generated
intervals and 6,199 presents, but its AltEnter attempts produced no AltEnter or
resize log, so it is not a WindowCycle pass. The driver only sends the shortcut;
a pass requires runtime mode, resize, and recovery logs. These are bounded runtime results;
they do not establish
physical 120 FPS, complete image-quality coverage, provider acceptance, or UI
separation. The UI separation path remains unavailable.

Issue #70 local substitute-scene evidence is retained under
`out/issue70-runtime/`. D3D12 and Vulkan each used 2560×1440 DLAA, 16× AF, a
120 FPS cap, FG disabled, object motion enabled, muted audio, background
handling, and isolated state for about 120 seconds; both exited 0 through the
owned-window close path and preserved the baseline. `scene_3195.png` and
`scene_2507.png` show the Uhra city/plaza route. The 75–115 second Diagnostic
window recorded 2,400 D3D12 accepted-present intervals with mean/p95/p99
`16.666/17.977/18.792 ms`, and 2,323 Vulkan intervals with
`17.219/20.471/23.045 ms`. Both recorded zero AF misses/table creates and zero
sampler-version splits, with two arena splits. This is a single substitute
scene window without before/after A/B or a final Lightweight rerun; it does not
establish an FPS gain or stable 60 FPS.

### PR73 Vulkan object attribution

When `run-fg-game.ps1` is given `-ValidationLayerDirectory`, it also sets
`LO_VK_OBJECT_TRACE=1` for that isolated process. Keep **both** `validation.log`
and `stderr.log`, plus `runtime.log` and `sl.log`, from the same run. The
validation layer writes VUIDs to `validation.log`; `VK_OBJECT_TRACE` records
host-facing swapchain images, initial Present waits per swapchain, host/FSR
image-view create/destroy routes, framebuffer mappings, point-pipeline shader
identities, and existing renderer fence-retirement events in `stderr.log`.

The trace is off unless explicitly enabled. It is event-bounded (8,192 events,
with a truncation warning), not a live-resource scan or GPU synchronization
mechanism. A `route` label identifies the intercepted call path, not every
consumer of a resource; Streamline's private dispatch table is not intercepted.
Missing trace entries must not be used to assign ownership to the SDK. Trace
runs are diagnostic runs, not performance baselines. This instrumentation does
not fix Present hazards, authorize ImageView destruction, or prove display FPS.

## Native command title probe

`run_native_title.py` is an active, bounded Windows driver requiring installed `psutil`. It copies the supplied baseline profile/save/settings/shaders into a new output, runs the same candidate with `--mode off|registers|all`, applies 720p/60 FPS, SR/FG off and shader prebuild skipped to both paths, remains hidden and muted, sends no input, measures OS thread CPU, requests a screenshot outside the timing sample, and closes only its own process. Baseline metadata is checked after the run. The completed-frame denominator is sampled from one-second log receipts; separate-process images do not constitute identical-input replay. See [the implementation note](../../docs/notes/native-command-bypass.md) for arguments and boundaries.
