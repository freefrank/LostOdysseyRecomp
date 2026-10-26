# XMA work while the selected input waits for a refill

The supplied Android logs show underflows and clock resets independently of
compressed-frame failures. The longest session reaches `starved=12916` and
`clock_resets=1874`, with no SDL queue drops or commit errors. These counters
do not identify a single cause or prove that the decoder caused every gap.

The production worker has a reproducible no-progress case: the selected input
is invalid, the alternate input is valid, and no decoded PCM remains.
`ProcessPacket` immediately returns, but `Work` tests only `anyInputValid()`
and retries 4096 times while holding the global XMA mutex. Guest lock/kick
commands and other decoder contexts cannot acquire that mutex meanwhile.
If the output ring is empty, its equal cursors also incorrectly invalidate the
output even though nothing was written and another input is still pending.

Stop the decode loop when the selected input is unavailable and pending PCM
has drained. Leave an empty output writable in that wait; retain the existing
retirement signal when both inputs are exhausted. A full ring is still retired.
No input switching, packet skips, FFmpeg error policy, audio clock, buffering
target, sample conversion or synchronization primitive changes.

`tools/tests/xma_streaming` compiles the whole production `apu/xma.cpp` with
guest memory and decoder stand-ins. The decoder stand-in records exact input
bytes; it does not validate acoustic quality. The unmodified baseline fails
both the instrumented 4096-call wait and, independently, empty-output state.
The candidate passes 1509 checks covering pending PCM, exhausted/full output,
split 15-bit headers, large frames spanning packets, delayed alternate-buffer
refills, interleaved stream skips and deterministic random frame layouts.
Linux Release and ASan/UBSan pass. NDK r28c/API-26 arm64 compiles and links the
fixture; the full production XMA unit also compiles against the real pinned
FFmpeg headers, with only guest/runtime boundary stand-ins.

The logs do not include compressed data or context snapshots for these exact
refill states. This fix is verified in the fixture, but neither the frequency
of this path on the reported session nor its audible benefit has been measured.
It does not resolve the separate `Channel transform bit`/`Reserved bit` failures.
In this pinned XMAFRAMES decoder a malformed or unsupported frame can be
consumed without output, so receive returns EAGAIN (`-11`) after a successful
send. The accompanying decoder warning is relevant evidence; `-11` alone is
not proof of malformed input. Real captures and listening tests remain needed.
