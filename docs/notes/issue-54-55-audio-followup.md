# Issues #54 / #55: audio follow-up and validation

Date: 2026-09-25. Source baseline: `346cdd513b1ad6c2672618ada257cc3519a02ce3`.

## Scope and status

This change fixes an XMA diagnostic blind spot. It does **not** establish a fix
for missing voice lines (#55) or Japanese audio in localized CGI scenes (#54).
Both issues must remain open pending scene-level verification. No guest audio,
movie, subtitle, language-selection, scheduling, or decode-state behavior changes
are included. There is no full runtime-build or gameplay-validation claim.

## Evidence and confirmed omission

[#55's original report](https://github.com/freefrank/LostOdysseyRecomp/issues/55)
contains 16 early XMA warnings, followed by much later silent dialogue with no
contemporaneous decode warning. The audited `apu/xma.cpp` used a process-wide
`static unsigned errors` and `if (errors++ < 16)`. After the initial 16 warnings,
normal later failures were invisible to that logger, regardless of context
release/reuse or elapsed time. Absence of a later warning therefore cannot rule
out a later decoder anomaly. This does not prove one occurred at the reported
scene: the warning budget explains an evidence gap, not the missing speech.

The old log also combined `avcodec_send_packet` and `avcodec_receive_frame`
results into one `result`. A negative send result means receive was not called.
The same numeric EAGAIN result has different meanings at these two stages:
send backpressure requires receiving output before retrying the unaccepted
input; receive EAGAIN means additional input is needed. See the
[FFmpeg send/receive contract](https://www.ffmpeg.org/doxygen/6.1/group__lavc__encdec.html).
No retry, flush, extra receive, or packet-replay behavior is added by this change.
A logged receive EAGAIN alone does not prove compressed-data corruption.

## Implemented change

`apu/xma_decode_reporting.h` provides a dependency-free reporter used by the
existing failure branch in `apu/xma.cpp`:

- Keep the first 16 detailed warnings. Subsequently emit at most one warning
  per ten seconds, on a qualifying failure; there is no timer or new thread.
- Add a monotonically increasing process-wide failure ordinal and
  `suppressed_global` count for events suppressed since the last warning.
  That count may include other contexts, not just the context on the current line.
- Add `stage=send|receive|frame-layout`, `send_result`, `receive_called`, and
  safe frame-layout fields. Do not read stale AVFrame fields after a failed call.
- Keep private packet-capture eligibility limited to the first 16 failures,
  still opt-in through `LO_XMA_ERROR_CAPTURE_DIR`. Late warning renewal does not
  renew captures. Preserve the first-failure-only input-buffer dump and existing
  no-overwrite check. Add stage information to the private state sidecar.

The reporter is shared across contexts and is used under the existing XMA
`g_mutex`. Successful decode work does not consult its clock or counters.
Short bursts within the ten-second interval can still be suppressed; a later
emitted warning summarizes them. There is no final suppressed-warning flush.
Use `LO_AUDIO_DIAGNOSTICS=1` for per-context counts, including release/clear
summaries, and a fresh targeted process when collecting a suspect scene.

## Tests performed

The actual helper used by the runtime is covered by
`tools/tests/xma_decode_reporting_test.cpp`. No game files or FFmpeg installation
are required. From the repository root:

```sh
mkdir -p out/tests
c++ -std=c++20 -Wall -Wextra -Werror -pedantic \
  -I LostOdysseyRecomp tools/tests/xma_decode_reporting_test.cpp \
  -o out/tests/xma_decode_reporting_test
out/tests/xma_decode_reporting_test
```

| Local test | Result |
| --- | --- |
| GCC 14.2.0, C++20, warnings-as-errors | 68 checks passed |
| Clang 17.0.0, C++20, warnings-as-errors | 68 checks passed |
| Clang 17.0.0, `-O2 -DNDEBUG` | 68 checks passed |
| Clang 17.0.0, AddressSanitizer + UndefinedBehaviorSanitizer | 68 checks passed |
| Negative control: replace recurring emission with the original lifetime-only policy | Test fails at the expected late-warning assertion |

The tests simulate 27 elapsed hours; they do not run the game for 27 hours.
They cover the first-16 budget, a late seventeenth failure, exact ten-second
boundaries, burst suppression and accounting, 1,000 spaced failures without
capture-budget expansion, and the three diagnostic stages. Assertions remain
active with `NDEBUG`. The copied baseline runtime file was checked against
Git blob `b04a7b4383502f37b6c9cfece8f10327e5f99a2e` before patching.

Not performed: full application compilation/linking, real XMA input decoding,
Windows/Linux game playback, hearing/transcribing the affected lines, or
replaying the attached saves. These isolated tests validate diagnostics only.

## Remaining #54 investigation

The [previous maintainer fix](https://github.com/freefrank/LostOdysseyRecomp/issues/54#issuecomment-5745812354)
was explicitly a voice-menu selection safety correction, not a confirmed fix
for the affected cutscene. A later report distinguishes CGI from working
in-engine scenes. The
[latest reproduction details](https://github.com/freefrank/LostOdysseyRecomp/issues/54#issuecomment-5827571879)
already provide `disk3_save.zip`, memory-lamp scene `3-2-2`, and `1-6-8` after
Bogimoray. An earlier comment also supplies `user04.zip`; do not ask again for
these already-provided files. These saves have not been replayed in this audit.

Run a fresh, isolated profile/save copy with `LO_TRACE_LANGUAGE=1`. The existing
language trace has its own 2,048-observation lifetime cap; check for its
truncation message. This patch does not change that independent cap. Record
the selected voice option, mapped guest language ID, effective voice cache,
movie/resource identifier, opened file/offset, and selected audio stream/track
at the actual CGI playback boundary. Existing language hooks alone do not
supply all movie/track evidence; add a boundary-specific hook only after the
real player and its parameters are identified. Keep text language, voice-menu
index, guest language ID and movie track ID distinct. Do not fill the Spanish
voice-menu gap or force an English track without asset/call-site evidence.

## Scene verification matrix (all pending here)

| Issue | Scene | Comparison and acceptance |
| --- | --- | --- |
| #54 | Memory lamp `3-2-2`, first Gohtza CGI on Disc 3 | Explicit English and Italian voice selections; compare a Japanese-voice control. Actual speech must follow the chosen voice. |
| #54 | Memory lamp `1-6-8`, CGI after Bogimoray | Same language comparisons; repeat in one session to test cached selection. |
| #54 | Following in-engine train-station scene | Confirm the working localized dialogue remains correct; changing text alone must not silently override a separate voice choice. |
| #55 | Leaving Uhra, Jansen drunk | Previously reported fixed; retain as a regression control, not proof of other scenes. |
| #55 | Disc 1 final-boss intermission / Obsidian Miasma, including Seth's shout | Correlate each expected line with event/resource request, XMA context, decoder result and final mix. |
| #55 | Disc 2, after Seth's first dream following the boat landing | Repeat the same per-line correlation and compare a known-good reference. |

For #55 use `LO_AUDIO_DIAGNOSTICS=1` and the existing
`LO_AUDIO_CAPTURE_REQUEST` facility: write a fresh nonzero `serial seconds`
request at the affected scene, with a duration in the supported 0-60 second
range. The request-file parent directory must already exist. A zero duration
stops capture. Capture names must be fresh; existing output is not overwritten.
The original startup-only `LO_AUDIO_CAPTURE` is not enough for a late scene.
Keep captures, game assets and personal logs private and out of commits.

Read `stage` and `receive_called` before interpreting an EAGAIN result. An
allocated context and nonzero mixed PCM do not prove the expected voice line
was requested, decoded, routed, or audible. First distinguish missing event
requests, wrong bank/language selection, decoder backpressure/input starvation,
and mixer/routing loss. Change playback behavior only with evidence for the
responsible boundary and a corresponding regression test.
