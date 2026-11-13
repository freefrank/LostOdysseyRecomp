# Optional read-only Android thermal telemetry

Set `LO_THERMAL_STATS=1` to record the OS thermal status and current thermal
headroom once every 10 seconds, next to `LO_FRAME_STATS`. Unset it or use `0`
to disable it. The default path performs no Android thermal API calls and no
per-frame clock reads for this feature. An enabled run adds periodic Binder
queries on the render thread: keep the option identical in both benchmark arms.
This diagnostic is not a sustained-FPS or battery-life improvement by itself.

The native manager/status API (API 30) and optional headroom API (API 31) are
resolved with `dlsym`; the existing minimum Android API is not raised and no
new library is packaged. An absent API, failed manager or unavailable headroom
is reported safely. Non-finite headroom never becomes a control input. A manager
is released before closing its library. No listeners or worker threads are added.

This deliberately does not reproduce the Performance Hint experiment rejected
in [PR #443](https://github.com/freefrank/LostOdysseyRecomp/pull/443): it heated
the fast cores and worsened FPS/p99. Affinity, performance hints, resolution,
frame target, audio, input and game clocks remain unchanged. OS thermal status
is not proof that a particular CPU/GPU frequency is capped; correlate it with
frequency/GPU and displayed-frame data before designing any adaptive policy.

`python tools/tests/android_thermal_test.py` passes nine host lifecycle checks
against the production `host_scheduling.cpp`: off, zero/malformed opt-ins,
working API, API-30/no-headroom, NaN, missing mandatory API, failed manager and
missing library. The test verifies one query in 1,000 rapid frame calls and
release at exit. Desktop syntax compilation also passes. Stub Android headers
and a fake shared library are used: this is not an NDK build or an on-device
result. API 26/29 fallback, real API 30/31+ OEM behavior, pause/resume and
long-session overhead still require an Android device.

Reference: https://developer.android.com/ndk/reference/group/thermal
