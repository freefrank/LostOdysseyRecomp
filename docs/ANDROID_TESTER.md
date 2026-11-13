# Android tester diagnostics (experimental)

On Android, open **Settings → System → Android diagnostics** with the ordinary
settings controls (LB/RB change category). The separate LB+RB debug-overlay
chord is unchanged. Switch Tester mode on, choose a profile, then select
**Recording → Start**. Stop the recording and select **Export diagnostics**;
Android's document picker asks where to save the ZIP. Export also works while
recording or after an interrupted process, if a trace was already written.
Tester mode starts off on every process start and its choices are not persisted.

## Profiles and data

- **Standard:** present-call intervals and wall/CPU time, renderer frame/draw
  count, audio queue counters, movie lifecycle/guest update, process CPU at
  one-second intervals, optional Android thermal state and battery temperature.
- **Advanced:** also brackets renderer and presentation GPU queue batches
  with the existing timestamp-query machinery and samples XMA send/receive
  wall time, synchronous inline pipeline creation and per-thread
  `/proc/self/task` CPU ticks. Queries are read only
  after existing submission fences; no new GPU wait is introduced.
- **Intensive:** the advanced probes with process sampling every 250 ms;
  recording stops after two minutes. It may alter performance. Thread sampling
  is limited to once per second, CPU frequency and thermal headroom to once
  per ten seconds. Headroom can be unavailable even on a supported API.

Per-core scaling frequencies are attempted from public, readable sysfs files;
Android may deny access. Missing values are blank or `null`. Battery temperature
is not GPU or CPU temperature. Headroom is Android's estimate, not a measured
temperature. CPU percentages are process/thread CPU time divided by elapsed
time for one logical core, so process CPU can exceed 100%.

`frame_timing.csv` uses monotonic nanoseconds. `interval_ns` is the interval
between accepted present calls, not a display scanout timestamp. The present
wall time and thread CPU time measure the host present call. Draw count is the
renderer frame most recently submitted. GPU columns are cumulative completed
timestamped queue intervals and completed-batch counts; they are not GPU load
percentage, isolated shader duration or an exact per-frame GPU time. The XMA
columns are cumulative decoder send/receive attempts, failures and elapsed
wall time since recording started, not thread CPU time. Movie fields are guest
frame update/PTS observations, not isolated video decoder time or A/V sync.
Audio's queue-empty counter is not a measured hardware underrun count. Frames
that were never submitted/presented cannot be inferred from these fields.
Pipeline counts and wall times cover inline `CreatePipeline` attempts only;
background pipeline jobs and cache hits are excluded. The two shader hashes
belong to the latest such attempt, not to every shader used by the frame.

The ZIP includes device/build metadata, selected non-identifying settings,
frame, CPU, thread, frequency and thermal CSVs, summary and event information,
and up to 2 MiB from the newest runtime log. GPU hardware utilization/frequency,
CPU/GPU sensor temperatures, A/V synchronization and display latency are not
implemented. Zero counts mean no recorded observations, not necessarily that
the underlying subsystem was available. The export may include no runtime log
if none exists. The log can contain file paths despite limited path redaction:
review the archive before sharing it. Nothing is uploaded automatically.

The slow-interval detector uses 1.3 times the configured frame interval with a
25 ms floor; it identifies intervals, not their causes or dropped frames. Each
of up to three windows saves up to 30 seconds before and 10 seconds after a
trigger, subject to data availability, process termination and a 4 MiB window
cap. The background writer keeps up to four 2 MiB segments per CSV stream and
at most three private sessions. A bounded native queue drops samples instead of
blocking presentation, with its dropped count in `session.json`. Recent context
is also bounded to 16,384 samples. Files left by an interrupted process remain
exportable; a session marked `recording: true` may have ended abruptly.

## Validation still needed

The host settings fixture, native recorder tests and JVM trace tests do not run
the game, SDL lifecycle, an Android driver or a physical controller. Build the
complete app with the project's authorized generated PPC/native inputs and its
signing key, then test on Android 8 and a recent Android device. On Xiaomi 14,
verify controller navigation, start/stop/export, background/resume, a CGI, the
intro boss and the two-minute limit. Compare repeated identical scenes with
the same build, graphics settings, temperature and diagnostic mode off versus
each profile; report frame-time percentiles, sustained temperature and power.
No device run, APK build or overhead measurement is claimed by this change.

## Checks run during implementation

- Standalone settings suite: 13 CTest cases passed, including the Android
  settings input fixture and the bounded native recorder.
- JVM export/thread-parser tests: two JUnit cases and the standalone trace
  export test passed.
- All runtime Java sources compiled against Android API 35, the pinned SDL
  Java sources and AndroidX Core 1.15.0. Only the generated resource-ID class
  was substituted; this is not an APK packaging or lint result.
- NDK 28.2, ARM64/API 26: JNI object compilation and syntax checks of
  `video.cpp`, `renderer.cpp`, `audio.cpp`, `xma.cpp` and `movie_replace.cpp`
  passed. These used a minimal generated-PPC header boundary, the pinned
  FFmpeg headers and the project's Plume patches. No guest code or complete
  runtime was linked. The video check used FSR feature flags set to zero.

The complete APK still requires the project's authorized private build inputs.
