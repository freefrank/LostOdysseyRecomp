# TAA bloom prefilter candidate (2026-09-12)

> **Investigation record.** Findings and open items are bounded to the recorded captures and dates. For current issue, delivery and acceptance status, see [STATUS](../STATUS.md) and [ROADMAP](../ROADMAP.md).

The rear enemy eye flicker was reproduced in the user's current battle scene
with TAA enabled and disappeared when the user turned TAA off. The latest
render-state export shows stable scene resolve input followed by a large
change in the bloom stages. In the selected rear-eye ROI, the three exported
pre-bloom values are 2,155, 2,172 and 2,163, while the corresponding bloom
composite values are 45,381, 1,613 and 4,443, all in the 8-bit preview metric.
The composite values therefore vary by roughly 28x across the three frames.
Both the distant large red eyes and the nearer paired eyes are affected. This
evidence identifies the bloom input path as the current repair candidate; it
does not show that the temporal history algorithm itself is the source.

The first candidate added an HDR16 area filter at 1280x720 before the existing bloom
chain. Each output texel integrates the exact overlapping source area, keeping
negative HDR values and alpha in float32 accumulation before writing HDR16.
That first candidate applied it only when all of the following matched: TAA
with active jitter, the identified vertex/pixel shader pair (`2f6bbed8149a7804` /
`7c260eacff1d681d`), a complete current-frame 1280x720 resolve, and a larger
HDR16 source texture. `LO_DISABLE_BLOOM_PREFILTER=1` restores the original
bloom input for a controlled comparison. The normal executable and user game
data are not changed by this candidate.

The standalone `LoBloomPrefilterTest` passed on both D3D12 and Vulkan. Its
checks cover HDR negative values and alpha, exact 1.5x and 3x area weights,
and nine subpixel locations for a small bright point; the sparse nine-point
sampling used by the old path misses eight of those locations in the fixture.
The production filter was also run against exported HDR16 inputs for frames
10170–10172 and matched an independent 3x3 area-average reference within the
expected half-precision output error. Evidence is retained in
`out/taa-bloom-fix/gpu-d3d12.log`, `out/taa-bloom-fix/gpu-vulkan.log`, the
per-frame filter logs, and `out/taa-bloom-fix/capture-filter-validation.json`.

This section records the first local `0.5.6-hotfix1` development candidate.
Its build and fixture validation are complete, while its same-scene result was
partial as recorded below. It was not a complete fix or a release.

## 2026-09-12 follow-up: partial improvement and linear sampling candidate

The first candidate was run in the original battle scene by the user (PID
37988). The runtime log confirms the intended `3840x2160 -> 1280x720` prefilter
guard was hit. The user's visual result was “有所改善，仍会闪” (improved,
but still flickers), so the defect remains open and the first candidate is not
accepted as a complete fix.

A new export from that run (`render-17892670940156607-f4768`) narrows the
remaining variation to the first bloom expansion. In the left-rear ROI, the
HDR scene values at seq08 are `391.967 / 363.971 / 363.971`; the corresponding
low-resolution seq09 ROI values are `4.8221 / 3.1039 / 3.1039`. These HDR
values are internal floating-point lighting data and do not describe HDR
display output. The result supports checking the first bloom sampling step
after the area prefilter.

The second candidate changes only the bloom sampling used after the prefilter:
when `bloomFiltered` is active and the renderer is not in the temporal display
pass, the bloom source uses linear minification and magnification sampling.
The later blur passes already use linear sampling. The second game build
completed successfully and is staged as
`LostOdysseyRecomp-taa-bloom-linear-test.exe` (SHA256
`7DDC3E91E1E309EDDBDDB7137750D45D65C0CD26A7654ADDC8ECE1C085C77D82`).

The prior area-filter fixture and its D3D12/Vulkan plus captured-input checks
remain passed. The linear-only fixture exited successfully on both D3D12
and Vulkan. It covers 65 horizontal, vertical and diagonal phase responses,
HDR negative RGB and alpha preservation, and adjacent red-step bounds of at
most `0.0625` for a full-scale step of `2`. The bounds include texture-filter
weight quantization and HDR16 output rounding; a very weak diagonal tail may
quantize to zero. These are filter-response checks and do not establish game
visual acceptance. This second candidate was superseded by the third candidate
below; the current lower-eye result remains pending. The new results are retained in
`out/taa-bloom-fix/gpu-linear-d3d12.log` and
`out/taa-bloom-fix/gpu-linear-vulkan.log`.

## 2026-09-12 follow-up: third candidate and geometry tracing

The third rendering candidate removes the transient `temporalJitter` gate
while retaining the AA3 condition. Captures 1653–1655 hit the candidate path
on every frame, and the upper robot's seq09 output remains stable at about
4.4773 or below. The user confirmed that the upper large robot no longer
flickers; the lower enemy eyes still flicker. This is partial scene acceptance
only and does not close the lower-eye defect.

An undisturbed asynchronous trace for frames 2687–2694 records eight frames
with `history reused=true` and `gap=false` in the `taa-eye-trace` directory.
The lower red-eye draws have valid depth and mostly accept history, so no TAA
history threshold was changed. The lower candidate shader pair is VS
`b755` / PS `420630`, with a constant red emissive multiplier of 5. Eight
color/depth draw pairs match existing VP slots 233 / 230 for VS `31bde`, including the original VP, fetch, world and
bone data. This does not indicate a missing shader map entry or prove that
the geometry source has been identified.

The diagnostic build adds optional geometry capture controls. Set
`LO_GEOMETRY_CAPTURE_WITH_RESOLVE_TRACE=1` together with the existing
`LO_RESOLVE_TRACE_REQUEST` to capture the first frame of each request.
`LO_GEOMETRY_CAPTURE_VS`, `LO_GEOMETRY_CAPTURE_VS2` and
`LO_GEOMETRY_CAPTURE_INDEX_COUNT` narrow the selected draws, while
`LO_TEMPORAL_DRAW_LOG_VS2` permits paired temporal draw logging. Captures
include metadata for the frame and submitted draw. The diagnostic engine was
built separately (SHA256
`A05A45933DF6F34E92FC1C9C6604CA0193B8832DEE43A63537D5C05776E6E44E`) and is
launched by `Diagnose-TAA-Eye-Geometry.cmd`. It is being used to obtain actual
uploaded geometry for the same scene; it is not a new rendering fix and does
not establish a lower-eye repair. Earlier GPU fixture results are reused.

The subsequent actual geometry capture for frame 7208 supplied four matching
draw pairs. Each pair has identical VP, world, active-bone, vertex-buffer and
index data across the compared submissions. This excludes a CPU upload
mismatch for those pairs. The subsequent GPU comparison is described below.

## 2026-09-12 follow-up: cross-backend geometry evidence

The bounded comparison covers four eye draw pairs, each with 84 indices.
D3D12 compute replay produces identical clip-coordinate bits. Vulkan
offscreen rasterization produces identical color/depth coverage masks across
all 32 jitter phases with an EQUAL depth comparison. These results do not
support changing the production Invariant decoration. The user subsequently
marked a broader row of small, distant eyes, including 144-index eye draws
whose geometry was excluded by the earlier 84-index capture filter. The four
tested pairs therefore do not establish complete coverage of the reported
flicker.

The new live diagnostic build is staged by `Diagnose-TAA-Live.cmd` (SHA256
`EA4DF2A135D49207094716A6518279064B239AE98DA010B4371169EBCA273E25`). Its
`LO_TAA_DIAGNOSTIC_REQUEST` file accepts exactly `serial aa jitter history bloom`;
the default override is `-1`, AA accepts `0` or `3`, and the
other selections accept `0` or `1`. The control is process-scoped. Generic
`ffff0020` / `ffff0021` pre/post-AA trace requests also work with AA Off.
`LO_GEOMETRY_CAPTURE_INDEX_COUNT=84` and
`LO_GEOMETRY_CAPTURE_INDEX_COUNT2=144` capture either matching index count.
Runtime control validation is still pending the user's same-scene run. This
build is diagnostic tooling only; the rendering algorithm and production
invariant remain unchanged. Earlier bloom GPU tests are reused.

## Current evidence boundary: synchronized capture and opt-in HDR candidate

The current audit corrects the earlier live evidence. The old live trace and
F1 readback did not wait for the GPU fence before mapping capture data, and
`n2` was not the final HDR source. Their same-frame pixel analysis and the
reported 87% improvement are withdrawn as repair evidence. The standalone
bloom and geometry GPU tests remain valid, and the user's report that the
upper target improved remains valid; the lower-eye flicker remains open.

The new rendering candidate is disabled by default. Synthetic `ffff0030`
captures the actual first-bloom source and `ffff0031` captures the accumulated
HDR output. The optional sixth `hdr=0/1` field in `control.txt` enables the
candidate; the existing five-field form defaults to `hdr=0`. The trace buffer
capacity is 192 MiB for two 4K HDR surfaces, depth and final RGBA8 output.
TAA resources are released by submission serial, so only data covered by the
relevant fence is recycled.

Production Vulkan checks passed exact reset behavior for HDR8, negative `-0.5`,
RGB `2` and alpha `0.375`. They also passed the prefix release case where A
completed and B remained recorded but unsubmitted, while the A prefix was
released and B remained correct. No old test was rerun. The game candidate
build completed with SHA256
`69EFC0812E47309EDE725AEFB0A69C5D3F6B07E01D78379A94F237D4935DC146` and is
launched by `Test-TAA-Eyes-4.cmd`. Same-scene routing and visual validation
remain pending; this candidate is unaccepted, uncommitted and unreleased.

## Current lighting and material follow-up

The user's HDR candidate feedback is “明显改善但仍闪”. The latest capture
(`PixPin_2026-09-12_23-38-48.mp4`) shows that the symptom affects whole
lighting, including face and body dark-surface transitions, rather than only
isolated red points. Reliable eight-frame `ffff0030/ffff0031` data is finite,
has alpha 0, uses the same camera and tone-map version route, and has
`gap=false`. Red-point HDR-stage variation fell by 86%, but whole-face/body
black-surface to textured-surface alternation remains in the original HDR
input. In the no-jitter comparison, face changes are near zero and the body
dark-surface ratio is stable at about 16%; ground variation is independent and
cannot all be attributed to TAA.

Three material vertex shaders (`3c86f4a89d220ee8`, `f3b9f20b3d3a62d5`,
`e7b38eb08c70e5e1`) passed the c7–c10 position, complete varying, and matching
depth/index/world/camera audits; slot 7 was added. The focused
`LoTemporalJitterTest --captured-static-layers` selector passed 131,457 checks
across 32 phases, two worlds, and 720p/4K. Existing tests were not rerun.

The subsequent game candidate built with HDR default off and materials default on;
`Test-TAA-Lighting-5.cmd` launches it and isolates `taa-material-trace`.
Its SHA256 is
`F983446111909E353BB34A871CCD15E9B010F4C6B62311A3B4F2D062DA5180BE`.
The seventh control field optionally accepts `materials=0/1` and bypasses only
the three new material VS paths. At that stage, live same-scene A/B validation
was pending; the later accepted result is recorded below.

## Accepted material-path fix

The user then tested the HDR-off/materials-on candidate in the same scene and
reported: “到了，不闪了” for the whole lighting target. The read-only process
check identified PID 26188 running
`LostOdysseyRecomp-taa-material-candidate.exe`, with runtime record
`runtime-1789278806155312` and SHA256
`F983446111909E353BB34A871CCD15E9B010F4C6B62311A3B4F2D062DA5180BE`.
The accepted candidate uses HDR `0` and materials `1` by default.

The repair covers the three material vertex shader paths recorded above:
their missing jitter had left depth and material positions mismatched. The
focused captured-static-layers selector remains the recorded 131,457-check
result, and earlier bloom GPU checks remain valid. This is user acceptance for
the reproduced whole-lighting scene, not a whole-game or cross-hardware claim.
The repair is included in published v0.5.7 (source commit `6112c06`); the accepted result remains limited to the reported lighting-flicker scene. Other scenes or hardware remain unverified.
