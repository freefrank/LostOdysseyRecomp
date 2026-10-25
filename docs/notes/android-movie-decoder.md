# Android replacement-movie decoder

Implementation: MediaCodec through JNI, using the VM attachment already provided
by SDL. No new runtime library, FFmpeg decoder, demuxer or Vulkan import path.
`ByteSource`, `ParseMp4`, the replacement session/clock and the two-plane NV12
renderer remain shared with Windows. This is experimental until device playback
has been checked; a successful build is not playback validation.

The format contract remains H.264 8-bit 4:2:0, progressive, BT.709 limited-range
SDR, matching the replacement shader. This change does not add color management
or conversion for BT.601/full-range/HDR files. MediaCodec's inferred color labels
are not used to reject untagged clips: they may be inferred from dimensions.
Original WMV decoding still runs for audio, subtitles, skip and movie lifetime.
This does not claim to fix original-WMV performance or dream audio issue #645.

## Ownership and fallback

- Regular Android codecs are filtered by their advertised format support, then
  tried in hardware/unknown/software order. API 29+ classification is Android's
  report; other vendor codecs on API 26–28 remain unknown. `--software` only uses
  positively identified software codecs.
- CSD holds Annex B SPS/PPS. Samples retain decode order and receive presentation
  timestamps with a reversible nonnegative bias for negative edit-list times.
  Rewind reconfigures and supplies CSD again, avoiding early-flush CSD loss.
- A format change is not EOF. Input EOS is sent once; output is drained including
  a final picture carrying EOS. Startup-only codec fallback never replays a
  replacement that already published a frame.
- Image planes are copied by their own row/pixel strides, ByteBuffer position
  and limit, and the Image crop. Odd crop origins are rejected rather than
  changing chroma phase. No codec-backed pointer escapes `Next`; Image and output
  buffer ownership are released before publishing the CPU frame.
- The session retains guest pacing until its first picture is ready, while still
  observing guest timestamps. A decode error marks the replacement failed so the
  existing guest WMV becomes visible immediately. Natural EOF retains the existing
  shorter-clip tail behavior.
- Cancellation reads a session-owned atomic flag. No foreign thread calls into
  MediaCodec. Dequeue waits are at most 10 ms, with a five-second no-progress
  timeout; vendor configure/stop/release calls themselves have no application
  timeout guarantee. Android background/reclaim behavior still needs device tests.

## Reproducible checks

Host data/layout and JNI protocol checks, with a JDK and the pinned SDL headers:

```sh
git submodule update --init --depth 1 thirdparty/SDL tools/XenonRecomp
git -C tools/XenonRecomp submodule update --init --depth 1 thirdparty/fmt
cmake -S tools/tests/movie -B build/movie -DCMAKE_BUILD_TYPE=Debug
cmake --build build/movie --parallel 4
ctest --test-dir build/movie --output-on-failure
```

The data check exercises real plain/fragmented MP4 fixtures, shared Annex B
conversion, cropped/padded planes, planar and overlapping chroma views, and
last-row bounds under ASan/UBSan. The JNI check runs production backend code with
real JNI in a JVM and **scripted Android classes**, with `-Xcheck:jni` and UBSan.
It covers startup fallback, missing/invalid output, reordered PTS, EOS, rewind,
cancellation, software forcing and pre-29 classification. It does not decode H.264.
The session check exercises startup pacing, frame publication, cancellation and
fallback through the actual player with a controllable decoder.
The workflow also runs the existing Media Foundation decode tests on Windows.

The existing game-free Android probe now accepts a movie file through an intent.
Build it with the pinned SDK/NDK using `:app:assembleDebug`; the dedicated movie
workflow builds/lints the APK without private game inputs. For a connected device:

```sh
adb install -r packaging/android/app/build/outputs/apk/debug/app-debug.apk
adb shell run-as io.github.freefrank.lostodyssey.probe mkdir -p files
adb shell run-as io.github.freefrank.lostodyssey.probe tee files/movie.mp4 < tools/tests/data/testsrc-64x64-bframes.mp4 > /dev/null
adb shell am force-stop io.github.freefrank.lostodyssey.probe
adb shell am start -n io.github.freefrank.lostodyssey.probe/.ProbeActivity --es movie /data/user/0/io.github.freefrank.lostodyssey.probe/files/movie.mp4
adb shell run-as io.github.freefrank.lostodyssey.probe cat files/probe-report.txt
```

Add `--ez software true` to the launch for a software-only
run. Copy the fragmented fixture or a locally generated 720p/1080p BT.709 clip in
the same way. The probe reports count, PTS order, EOS, first-picture rewind and
decode/conversion timings. It does not display frames or play audio.

Before calling playback supported, test real Adreno and Mali devices with the
project's supported graphics drivers, plus API 26–28 where available. Compare the
same intro and later movies with/without replacement at 29.97/30/59.94/60 fps.
Check display cadence, A/V drift, skip, short replacements, background/resume,
memory and thermal behavior. Repeated frames expected from refresh-rate mismatch
must be separated from decoder starvation. Keep full-game/APK compilation,
physical decoder tests and game playback results distinct.
