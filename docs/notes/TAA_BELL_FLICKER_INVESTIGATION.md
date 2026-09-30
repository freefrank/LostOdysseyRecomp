# TAA Bell-Stand Flicker Investigation (Issue #46 adjacent)

Status: investigation open. The exact-stationary MV plus stationary color-clip
candidate received user confirmation of a clear stability improvement, but
residual shimmer remains and the Bell Vulkan 4K issue is not resolved.
The validated baseline was committed and pushed to `origin/mv` as
`ae7df0fb051980a32410423284199c7cebd9dfc3`; this is not a formal release.
The independent `history_fp16` experiment passed technical checks but has not
been accepted as a visual fix.
Ledger: all 18 `taa-position` cases remain `needs_review` / `not_implemented` /
`not_validated` / `not_accepted`. This note binds no historical review to new evidence.

## Previous diagnostic controls (opt-in; default behavior unchanged)

File: `LostOdysseyRecomp/gpu/renderer.cpp`.

- `LO_TEMPORAL_LOG_ALL=1`: per-frame `renderer temporal f...` summary plus
  gates/camera/probe details for every frame, no start frame needed. Previously
  only the first 256 frames after `LO_TEMPORAL_LOG_START_FRAME` were logged,
  so late-game frames (f2349, f31390, ...) were invisible.
- `LO_TEMPORAL_DRAW_LOG_ALL=1`: per-draw jitter records without a start frame.
  Existing VS/geometry filters still apply.
- `LO_TEMPORAL_DRAW_LOG_UNKNOWN=1`: additionally records draws that sit in the
  scene viewport but have no position-matrix slot (`temporalSlot<0`). This
  catches every unwhitelisted scene draw in one run instead of guessing hashes.

## Uncommitted candidate repair

The current working tree allows the cubic footprint to include the predicted
primary depth surface plus one background depth surface, and adds a guarded
`stabilizeStationaryGeometry` path. The latter applies
only when the geometry is stable and the motion vector is trusted; for nearly
stationary geometry (`0.002` to `0.125` pixel motion), it smoothly returns the
history weight from `31/33` toward `0.85`. Existing rejection and clamp rules
remain active. Set `LO_TAA_STATIONARY_HISTORY=0` to disable this path for a
comparison.

`LO_TAA_ACCEPTANCE=1` enables an independent diagnostic resolve from the
candidate history to display. It does not color or alter the normal history
path. The candidate is present in the uncommitted working tree at the time of
this note; no release or user acceptance is implied.

The second candidate adds general static-coverage handling. Its GPU project
passed 811 checks, and the runtime build succeeded. A two-surface ownership
swap fixture using the same `31/33` weight changed peak-to-peak variation from
`128` to `10` and the mean from `132` to `135.875`; these use CPU-uploaded
deterministic inputs and a production GPU consumer, rather than complete
geometry-production validation. Eight safety-rejection classes also passed.

## Evidence so far

- `render-17897018090240431-f9876` (3 frames, AA=3): camera stable
  (`candidate_ready=true`, identical VP, depth alloc 14) but
  `temporal_history_verified=false`. Its `shader.jsonl` only covers f0-f255, so
  no jitter counts exist for the captured frames.
- `render-17897637349644857-f2349` (bell scene): UI stable, only the bell stand
  shimmers, TAA-only. Static check: 45 VS present, 34 whitelisted, 11 not.
- `render-17897643027668846-f31390` (AA=3) vs
  `render-17897644282377816-f36731` (AA=0): same camera (identical VP, 988
  draws). The bell region moves in both; TAA amplifies pre-existing motion.
- `render-17897654353054983-f2111` with `LO_TEMPORAL_LOG_ALL`: f2111/f2112 show
  `reused=false gap=true`, jitter 0/0/0, gates
  `invalid_history|previous_incomplete|epoch_changed`. Root cause of the gap:
  capture readbacks stall past the 250 ms threshold, which resets history and
  bumps the epoch every capture frame. Captures therefore cannot observe
  steady-state TAA.
- Live play log (3491 temporal frames): 2381 frames with `reused=true
  gap=false`, ~1800-1856 jittered draws, 0 misses, 1-2 unknowns. Gates clean
  (`mask=0x0`). Global history blending works; the defect is local.
- Unknown-draw hunt: all four unknowns (`6318`, `81217dc9`, `69349140`,
  `8bbd`) plus earlier suspects (`760a`, `f95a`) are micro-draws (3-12
  indices), none is bell geometry. Bell depth draws are `b030` (first 25
  draws, confirmed via draw-step previews + live draw log).
- Bloom isolation: TAA on + `LO_DISABLE_BLOOM_PREFILTER=1` made flicker worse,
  so the TAA-only bloom prefilter was mitigating, not causing, the shimmer.
  The flicker source is upstream (jittered scene).

### Candidate automated evidence

The GPU test project built once and passed 791 checks. In its 32-phase
stationary-geometry cycle, peak-to-peak variation fell from `10` to `4` and
the mean moved from `127` to `126`; motion recovery, depth rejection, alpha,
and raw/camera-only checks also passed. In the same-instance normal/no-jitter/
no-history-16-frame comparison, the four Bell ROI deltas for normal mode were
`0.93/2.53/1.49/1.19`, while the no-jitter comparison was approximately zero.
The acceptance diagnostic accepted `97.6%`–`99.5%` of frames continuously and
had a color rejection rate below `0.13%`.

These are automated and diagnostic results from `out/bell-resume/gpu-test.log`.

### Exact-stationary and color-clip candidate evidence

The current MV source candidate uses `exactStationary`: identical geometry and
raster inputs plus the actual vertex-shader constant reads must match bit for
bit. Canonical generated-HLSL literals are parsed strictly; relative or
unknown reads fall back to the full bank. Explicitly unused shared values and
pixel-shader flags are excluded, and zero is not treated as a speed threshold.
Fourteen CPU exactness checks and fifteen usage checks passed. A bounded GPU
literal-zero suite passed 543 checks across 32 jitter phases, rigid/skinned
paths and invalid-input protection. These are additional candidate checks; the
older 826-check suite was not rerun.

The live `stationary_color_clip` control defaults to `0`. With stable geometry,
strict zero original motion, valid depth and the other guards satisfied, it
changes an out-of-range color hard rejection into clamp-and-blend; orange
diagnostic pixels identify color accepted after clipping. The color group
passed 25 GPU checks and the new parser field tests passed. The current live
run used `snap=1`, `motion_min=.002` and `colorclip=1`; it remains diagnostic.
In `out/bell-resume/live-tuning/color-off|color-on/result.json`, same-session
32-frame values changed upper `0.60561→0.50872` (16%), video
`0.61189→0.52842` (13.6%) and ground `0.47061→0.31962` (32%); these are not
performance measurements. The exact-source-MV frame 877 evidence records
video and ground at 100% zero and upper at 99.412% zero. The 0.95 stationary
weight comparison was independently tested, gave only about 4% video benefit,
and was restored to `31/33`.

The user has now visually reviewed the exact-stationary MV plus stationary
color-clip candidate and reports that it is clearly steadier, while residual
shimmer remains. This confirms partial visual improvement for the observed Bell
scene but does not establish a complete fix or close the investigation.

### History FP16 precision candidate

The independent `history_fp16` switch has now passed 50 precision-only GPU
checks covering RGBA8 source plus FP16 history updates, final SDR/alpha,
HistoryOwner round trips and reset behavior, and formats `127/129`. The FP16
parser, Python half-trace, stationary-range and HTML syntax checks also passed;
the runtime build is recorded in `build-history-precision-runtime.log`.
Same-executable 32-frame values were RGBA8 `31/33`: upper/video/ground
`0.5168837/0.5271268/0.3196223`; FP16 `31/33`:
`0.5533177/0.5797869/0.3611376`; FP16 is therefore not claimed as a fix or
recommended default. FP16 `63/65` measured `0.4818415/0.4930559/0.2660568`,
and `127/129` measured `0.4366672/0.4451076/0.2160987`; RGBA8 `127/129`
measured `0.3996239/0.4103040/0.1550984`. Higher stationary weights are
diagnostic only and may produce motion trailing.

The latest user observation of the higher-weight candidate was only a slight,
hard-to-distinguish improvement by eye. No obvious trailing was observed, but
the user reported lower frame rate, so this candidate is not accepted. The
working controls have been restored to the baseline RGBA8 history with weight
`31/33`.

### Uhra Main Street follow-up

The user identified the upper steel frame on Uhra Main Street as the active
flicker region. In the older same-scene baseline, eight-frame source/output
means changed from `7.760` to `0.356` for the steel ROI, `2.779` to `0.034` for
the wall, and `2.004` to `0.013` for the ground, showing effective history
composition and a localized residual. A prior multi-layer outline candidate
made the scene feel slightly calmer but reduced 60 FPS to roughly 55–58; it
was disabled and 60 FPS returned. PresentMon from the same short sample showed
TAA-on GPU/CPU busy `7.098/16.877 ms` versus AA-off `5.018/16.514 ms`, about
`2.08 ms` GPU and `0.36 ms` CPU overhead; this is not a broad performance
claim.

The new debug candidate adds `jitter_scale`, `stationary_multi_surface` and
`gpu_timing`, with stage timing reported in state. The current running build
records cumulative `total_ms`, while the running candidate
reports `last_ms`. With Uhra user02 loaded, `jitter_scale=.5` largely removed
horizontal shimmer; `.35` did not improve it further and was restored to `.5`.
With `stationary_multi_surface=1`, the user found diagonal edges steadier,
with no obvious motion trailing and roughly 60 FPS. Short PresentMon samples
for `.5`/multi-v2 were CPU busy `16.8234/16.816 ms`, GPU busy
`7.0976/7.0372 ms`, and present intervals `16.9901/16.9816 ms`; these
differences are within sample noise. The running state is AA3, jitter1,
jitter_scale `.5`, multi-surface1, snap1, colorclip1, FP16 off and weight
`31/33`. Over 314 frames, replay draws averaged about 732.6 per frame, with
replay `1.356 ms`, TAA `0.447 ms` and mask `0.128 ms`; gpu timing is now off.
This is local debug-candidate visual acceptance for Uhra only, not a default,
whole-map, cross-platform or release fix. FP16 remains opt-in and unaccepted.

### Dated current Uhra acceptance — 2026-09-19

The accepted local debug candidate uses internal 3840x2160 with 4K output,
`jitter_scale=.5`, `stationary_multi_surface=1`, `snap_stationary=1`,
`stationary_color_clip=1`, `history_fp16=0`, and
`moving_bilinear_fallback=0`; history and MV inputs were valid. On
`debug-moving-bilinear-opt.exe` (SHA-256
`00FC2575AD0D515EBC7A68CB2F26110981B271024974E4C870078A2F54E3E3A0`), the
user said “这一版已经不错了。可以接受” and observed about 60 FPS. This is
local Uhra debug acceptance only; slight shimmer remains and it does not cover
global, default, cross-platform or release behavior.

At 1080p internal to 4K output, the steel models still shimmer severely.
`.25` improved static content but worsened moving-camera shimmer, so `.5` was
restored. The steel ROI temporal byte delta changed from `10.15` to `0.483`.
Moving diagnostics showed third-layer rejection `5.44%` for steel versus
`.075%` for the wall; experimental `moving_bilinear_fallback=1` reduced steel
rejection to `.69%` without obvious perceptual benefit at about 60 FPS and
remains disabled. An earlier fallback improved appearance but reduced 60 FPS to
55–58 and remains unaccepted. No specific material shader is identified.

### Main renderer integration — local Uhra visual acceptance

After the local Uhra acceptance, the accepted policy was wired into the normal
renderer path. When no live-panel snapshot is active it uses
`jitter_scale=.5`, `snap_stationary=1`, `stationary_color_clip=1` and
`stationary_multi_surface=1`, with RGBA8 history, stationary weight `31/33`,
`history_fp16=0` and `moving_bilinear_fallback=0`. TAA enables geometric motion
vectors by default; `LO_MV_ENABLE=0` remains an explicit comparison switch,
and motion replay is skipped for non-TAA modes.

The local CMake RelWithDebInfo build passed, `LoMotionVectorTest` passed 71
checks covering both default-enabled and `LO_MV_ENABLE=0` comparison paths,
and the main EXE/PDB were deployed with matching build-output hashes. The
deployed index-signature-cache candidate has SHA-256
`2BC90983FC5F866E16F52ACD0B7642347CAD1792FF67A687A2551817EA53707E`; the
previous binary is preserved as `LostOdysseyRecomp.exe.pre-index-hash-20260919`.
The normal executable's background, muted native-Continue run loaded the save
on Vulkan. `runtime-1789846180585750.log` recorded motion replay ready and
consumed at frames 960, 1080, 1200, 1320 and 1440 without a live-panel override.
In the same Uhra save on Vulkan at 4K internal/output with a 120 FPS target and
no pacing, hidden muted A-B-A-B captures measured heartbeat means of
54.61 FPS for the separate Release build and 54.57 FPS for the former
RelWithDebInfo main executable, versus 60.34/59.00 FPS for the candidate.
Release `/O2`/`/Ob2` remained at 54.61 FPS. Separate TAA-collector samples
measured 54.78 FPS disabled versus 55.68 FPS enabled, so these samples do not
support the collector as the main cause. An asm-profiler sample pointed at
`memcmp` and a Vulkan-driver nearest symbol, but sampling alone does not
precisely attribute the cost. The index-signature cache reuses exact-content
index fingerprints while retaining complete source-byte verification.
`LoVertexCacheTest` passes 3,668,948 checks; the motion audit reports steady
tracked/matched/replay 988 with failed 0.

The user reviewed the same Uhra steel-frame scene in the foreground at 4K
internal/output and reported about 60 FPS with image quality unchanged and
acceptable. A separate normal run recorded 1,882–2,605 draws/frame and
38–55 FPS while moving the view; its conditions differ from the fixed A-B-A-B
comparison and it is not used as a direct performance comparison. This remains
bounded to Uhra and the local machine. The accepted 4K Uhra scene result does
not establish whole-game, cross-platform or release acceptance, and the known
1080p-internal to 4K moving-camera limitation remains open.

### Preliminary real Bell candidate result

The current 32-phase real-scene comparison shows partial improvement only:
the red ROI delta changes from `0.9096` to `0.5652`, cyan from `2.5112` to
`2.1208`, white from `1.4418` to `1.0725`, and green from `1.1751` to
`0.9403`, corresponding to roughly 16%–38% improvement across the reported
regions. The remaining heatmap is concentrated on the outline and is strongly
associated with pixels that experienced history rejection; approximately 17%
of the region contributes 63% of the residual. The Windows runtime candidate
is still being refined, including a stable-grid cross-phase depth-surface
swap investigation.

This preliminary result does not establish a perceptual fix, whole-game
coverage, cross-hardware coverage, release readiness, or user acceptance.

For the second candidate's `LO_TAA_ACCEPTANCE=2` diagnostic, rejection causes
are color-coded without modifying history: blue reactive, yellow current/replay
depth mismatch, black primary/support mismatch, magenta third layer, cyan
invalid or boundary, red accepted, and green color rejected. The current
32-phase visual comparison improved the video ROI from baseline `1.4946` to
`1.13965`, and the upper-beam ROI from `1.30728` to `0.82713`; residuals
remain. The second candidate was also rejected in foreground review: the user
still saw obvious shimmer. Candidates `607dd6f1…` and `3b942ee…` both failed
visual acceptance.

### User visual acceptance result

The user reviewed candidate `607dd6f1…` in the Bell scene and reported that
the shimmer remained obvious and had not improved. The primary visible defect
was the upper crossbar/support metal edge rather than the earlier lower Bell
ROI; a distant ground seam also showed slight shimmer. The matched 4K ROI was
`(x=1062, y=255, w=1326, h=188)` and the video ROI was
`(x=1166, y=299, w=98, h=90)` with match `0.966`. In that video ROI, delta
changed from `1.4946` to `1.2826` (about 14%); jitter-off remained `0`.
Acceptance was `95.45%` with `0.73%` color rejection. This candidate failed
user visual acceptance. The second candidate includes the general
static-coverage fallback, but both candidates still failed visual acceptance.
The local live-debug panel and launcher are available for bounded diagnosis.
Native Continue now has runtime evidence: it loaded `save/user01/save.bin`,
entered the Bell scene, and reported `history_reused=true`, `motion_ready=true`
and `motion_consumed=true` at 3840x2160. The tool itself has not received
separate user acceptance, and the underlying shimmer remains unaccepted.

## Prior verdict before motion-vector and coverage candidates

- The resolve shader (`temporal_aa.cpp`) validates all depth taps, clamps
  history into the current 3x3 neighborhood (active in TAA mode), and
  constrains cubic negative lobes. No defect found in the blend itself.
- Bell scene `f1848` full table: depths `702c`/`b030`, material `4053` slot 7
  (~300 draws, 7 PS pairs), shadow `99c2`/`d55` compensated, all applied,
  0 misses. Mapping and compensation are complete.
- Jitter-off (TAA on, `1 3 0 -1 -1`) stops the flicker but also removes the
  supersampling benefit: expected, and it proves jitter is the driver, not a
  broken blend.
- History weight down to 0.1 changes nothing: the flicker is not ghosting
  proportion. Ruled out as a lever.
- Stable-grid mode (skips display reconstruction, jitter still on) still
  flickers: display reconstruction ruled out.
- The historical pre-motion-vector verdict was that current-frame aliasing on
  thin swaying geometry sampled at changing Halton phases was unfixable without
  motion vectors. Motion-vector replay and consumption now exist, but the two
  current candidates still fail visual acceptance; this historical sentence is
  not a current diagnosis or acceptance claim.
  `LO_TEMPORAL_HISTORY_WEIGHT` stays as an opt-in diagnostic only; default
  0.85 unchanged. AA mode choice remains the user lever.

## HDR note (original Issue #46)

The swap chain is fixed `R8G8B8A8_UNORM` (`video.cpp`); the game emits no HDR
signal and switches no DXGI color space. `HDR_area linear` in bloom logs is an
internal pre-tonemap stage. Washed-out flicker that disappears when monitor HDR
is off points at OS SDR-to-HDR mapping, not a proven game HDR bug. Kept
separate from the TAA work above.
