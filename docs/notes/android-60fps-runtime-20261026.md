# Android 60 FPS investigation: missing comparison switch and audio follow-up

The Android 1.1.0 binary contains a reproducible recompilation defect in
`sub_82F80F48`: a missing seven-way local switch and an incomplete shared return.
The fix restores its function extent and switch metadata. It does not establish
that the sustained Android 60 FPS slowdown is resolved. No hardware gameplay
validation or full regenerated game build was possible here.

## Supplied evidence and chronology

All six members of `Lost Odyssey Logs 26 ottobre .zip` were read: device metadata,
both native stderr files and three runtime logs. They identify Android 16,
Xiaomi 14/Adreno 750, Turnip R8 and version 1.1.0, runtime build revision
`fcfda1311f6e-dirty`. The source baseline is official main
`f9da0ae9bdd481a5ea4fe6431f3a4724d8261147`.

The long evening session (`runtime-1793046357179088.log`) contains:

| Elapsed seconds | Observation |
|---|---|
| 273.291 | `WAIT_REG_MEM stalled 5s` at 30 FPS, before selecting 60. |
| 741.737 | Internal resolution saved as 1440; target remains 30. |
| 1041.415 | Audio counters: 238 empty-queue observations / 33 clock resets. |
| 1047.224–1047.304 | Settings select **1080 and 60 together**; effective target and host cap become 60. |
| 1061.550 | Audio counters rise to 265 / 35. |
| 1264.494 | Counters already reach 1056 / 90. |
| 1265.616 | Missing dispatch: CTR `0x82f81040`, LR `0x82f86788`, r3 `0x2c`, r4 `0x5e5390c`. |
| 1300.381 | Audio counters 1610 / 177; SDL queue errors and drops remain zero. |
| 1316.072–1316.082 | Target returns to 30, retaining 1080. |
| 1327.225–1407.220 | Counters remain 1993 / 266; submission cadence recovers to approximately ten seconds per 1875 frames. |
| 1421.749 | Swap watchdog: `window event pump`, presenter idle, submitted/completed serials 82645 / 82643, after returning to 30. |

The morning session is configured at 30 and has its own `WAIT_REG_MEM` warning
at 1607.023 seconds. The middle session ends during startup at 3.777 seconds.
No session records `GPU fence wait failed`, device loss or `xma frame decode
failed`. Neither `LO_FRAME_TIMING` nor `LO_RENDER_TIMING` was enabled. Ten-second
renderer reports cannot establish the reported instantaneous 2–3 FPS minimum.

Changing resolution and scenes prevents a controlled 30/60 comparison. The
missing dispatch is approximately 218 seconds after selecting 60, after severe
audio degradation has begun. It is not the initial trigger shown by these logs.
The pinned SDL Android event pump can block while backgrounded; the final
watchdog does not by itself prove a Vulkan fence deadlock. The archive does not
establish whether backgrounding caused that particular wait.

## Confirmed defect: a local switch treated as function dispatch

The observed LR is in `sub_82F86538`, whose published translation calls
`sub_82F80F48`. That helper classifies two floating-point arguments, loads a
seven-entry table at `0x82F80FF0`, and executes `bctr` at `0x82F80FEC` using index
r11. Its exact table is:

| Index | Local label | Selection mask |
|---|---|---|
| 0 | `0x82F8100C` | `0x28` |
| 1 | `0x82F81014` | `0x18` |
| 2 | `0x82F8101C` | `0x30` |
| 3 | `0x82F81024` | `0x08` |
| 4 | `0x82F8102C` | `0x10` |
| 5 | `0x82F81034` | `0x20` |
| 6 | `0x82F81040` | unconditional true |

Cases 0–5 return whether `(flags & mask) == 0`; case 6 returns one. The shared
tail compares r11 at `0x82F81038`, conditionally sets r10 at `0x82F81040`, copies
r10 to r3 at `0x82F81044`, and returns at `0x82F81048`.

Without switch metadata, the published code uses global guest-function lookup
for local case labels. Labels `0x82F8100C` and `0x82F81040` are missing from its
mapping. `Memory::MissingFunction` replaces r3 with zero. Other cases branch to
a manually bounded `0x82F81038` fragment of size `0xC`; its zero branch stops
before copying r10 to r3. This independently corrupts the comparison result,
even when no missing-function fallback is called.

The fix declares the complete helper `[0x82F80F48, 0x82F81050)` with size `0x108`
and the seven local labels for dispatch `0x82F80FEC`, register 11. Production
analysis now emits local control flow including the complete shared return.
Both declarations are also placed in their manual metadata sources so the
regeneration tools retain them. Existing neighboring fragment declarations are
preserved. A four-byte standalone entry at `0x82F81040` would still truncate the
shared return and would not repair the missing switch.

There is no FPS conditional, platform-specific workaround, clock change or
missing-call suppression. The comparison is incorrect independently of frame
rate; the supplied 60 FPS run happens to exercise the missing label.

## Evidence from the published ARM64 binary

The official release asset was downloaded and checked against its published
SHA-256 digest. No APK or extracted proprietary bytes are included in this
contribution.

- APK SHA-256: `41ec5d557cc78fdc72feb00754a48e7b2851730c27344eaa1619f6478850877a`.
- `libmain.so` SHA-256: `9230eaa36d5ec4045c643872a25cb8865edc79bf36a5a32164625616e2952f70`.

The binary accidentally translates the table words as `lwz r23,disp(r24)`.
Executing that translation under Unicorn recovers the ordered seven pointer
words from its volatile-read addresses. The exported ELF symbols, guest mapping,
comparison helper and mask/return fragments provide independent inspection of
the dispatch and return behavior. `inspect_release.py` pins both hashes and
repeats this bounded ARM64 execution without Android, a GPU or an audio device.

With controlled guest constants and floating inputs, 512 published-helper calls
produce 172 incorrect return values, including 84 incorrect Boolean results and
128 missing dispatches. A controlled call with the log's CTR/LR/r3 signature
reproduces expected one versus actual zero. The reporter's actual floating inputs
are unknown; this is a reproduction of the defect, not a replay of the game.

## Regression tests and their limits

`tools/tests/guest_entry` loads the production function/switch configuration and
uses the patched production `Recompiler::Analyse` and emitter. Its reduced PPC
fixture selects r11 directly instead of copying the floating classification
prelude; table, case masks and shared return reproduce the inspected structure.
The test executes every case, all six flag bits, and nonzero upper r3 bits.

- Linux Clang 22.1.8, Release: **28002** checks pass.
- Same generated fixture with AddressSanitizer/UndefinedBehaviorSanitizer: passes.
- Negative control removing only the switch: the unchanged checker fails.
- Negative control removing both new metadata declarations: fails. Selecting
  case 1 alone also fails with zero missing lookups, isolating the shared-return
  truncation from missing case dispatch.
- The corrected generated switch compiles and links as a static AArch64 ELF.
  Under Unicorn, all **448** reduced case/flag combinations return correctly.
  This is not an NDK build or Android gameplay test. The existing context header
  produces an FPCR assembly operand-width warning when cross-compiling.
- The Linux/Windows workflow builds the positive fixture and requires both
  negative controls to fail for the expected comparison assertion.
- `LoGuestDispatchTest` exercises the actual regenerated helper with controlled
  float classes and flags. It requires the private generated game sources and
  was not executable in this environment.
- Supported host review regressions: **46/46** pass with GCC 13.3 and ASan/UBSan.
  The Linux-home namespace fixture is unavailable in this host; it is excluded
  from that count. SDL updater fixtures were not configured locally.

The local Clang wrapper adds `-include cstdlib` for its bundled libc++ 22 / fmt
11 combination. No project runtime flag is changed. Leak detection is disabled
in this traced host; address and undefined-behavior instrumentation remain on.
A full root configure stops because the private `LostOdysseyRecompLib/ppc`
sources are absent. No complete game executable or installable tested APK is
provided by these host checks.

## Audio: existing #727 is relevant but not a new contribution

[PR #727](https://github.com/freefrank/LostOdysseyRecomp/pull/727), still a draft
at inspection, already fixes a reproduced XMA input-refill wait. Official code
can call non-progressing `ProcessPacket` up to 4096 times while holding the
global decoder mutex. Equal empty output cursors can also retire pending output
without a write. Its candidate yields on the selected invalid input and retains
pending output while preserving full/exhausted-output retirement.

The existing production-unit fixture passes **1509** checks under ASan/UBSan.
Against official 1.1.0, it independently fails both the no-progress and output
retirement assertions. Integration with this switch correction is kept separate
from the focused contribution. A combined host review run passes **47/47**
checks. The archive does not establish that this exact refill state occurred
when audio degraded; audible recovery still requires a hardware comparison.

`starved` counts an empty SDL input queue before submission, not an AAudio
hardware underrun. `clock_resets` counts an audio-driver scheduling miss beyond
its existing eight-frame horizon (about 42.7 ms), not a decoder reset. These
counts cannot distinguish callback time, decoder mutex waits, host scheduling,
pause/resume or device behavior. No buffer increase or audio-clock change is
introduced.

## Other inspected paths

- `gpu/frame_pacer.h`, `gpu/frame_rate.h`, `debug/frame_timing.cpp` and CP pacing:
  merged #391 already selects immediate guest presentation at 60. Host deadlines
  discard overdue debt. Existing tests cover rate pairs, transitions and
  overload/recovery. No new defect was reproduced there.
- Virtual 60 Hz VBlank and guest interrupts run independently of the host
  deadline. No circular dependency is established by the supplied traces.
- Vulkan renderer and presentation slot recycling: no unbounded command backlog,
  failed fence or device loss is established by this archive.
- `WAIT_REG_MEM` also stalls at 30. Its cause remains unproven. Extending the
  timeout or skipping the wait would hide a synchronization defect.
- Android uses History occlusion. Later synchronous-query waits lack the caller
  and slot attribution needed to distinguish Fast same-frame work and captures.
  No occlusion policy or rendering-quality change is justified here.
- Vulkan submission masks (#709) and XMA cursor normalization (#712) are already
  merged and are not duplicated. Thread priority changes without scheduling
  evidence would not establish a fix.

## Hardware acceptance

1. Regenerate the real game PPC sources with this metadata. Require the
   `0x82F80FEC` dispatch to lower to local cases and all branches to include the
   return at `0x82F81048`; build/run `LoGuestDispatchTest`. Check generator output
   for new errors before producing an APK.
2. Build a debuggable APK from the focused branch and, separately, the integration
   branch containing #727. Compare against unmodified 1.1.0 from the same private
   input/toolchain. No NDK or Android device is available in this environment.
3. On Xiaomi 14 use the same save/scene, Turnip R8, 1080 resolution, effects,
   VSync and display refresh. Disable fast-forward. Run a 30 FPS baseline,
   restart directly at 60, and separately run 30→60→30 without changing resolution.
   Repeat at lower resolution, again keeping settings fixed within each pair.
4. For a **debuggable** APK, the launcher supports:

   ```sh
   adb shell am force-stop io.github.freefrank.lostodyssey
   adb shell am start -n io.github.freefrank.lostodyssey/.RuntimeActivity \
     --es LO_DEBUG_LOG 1 --es LO_FRAME_TIMING 1 --es LO_FRAME_STATS 10
   ```

   A release APK can ignore these extras even if debug-signed. Verify the startup
   `LO_* switches` line. Keep diagnostics identical between builds. Start with
   `LO_RENDER_TIMING` disabled; use a short separate capture if needed.
5. Run at least 10 minutes per 60 FPS case, including a battle, the Gate of Uhra
   and a CG transition. Repeat from a cooled device and in a prolonged session.
   Check speech/music continuity, video sync and animation/game speed. Do not
   background the sustained run; test pause/resume separately. Export full logs.
6. Compare counter deltas per elapsed second, audio submission cadence and frame
   timing percentiles. Require no recurrence of this missing local-case dispatch.
   Hardware acceptance also requires no progressive collapse, recovery after
   60→30 and no regression in 30 FPS behavior; occasional GPU-limited frame misses
   alone do not reject the runtime fix.

Open: the dominant source of audio-driver deadline misses, both register waits,
direct-at-60 behavior and sustained hardware acceptance. The confirmed guest
comparison correction is necessary independently of those unresolved causes.
