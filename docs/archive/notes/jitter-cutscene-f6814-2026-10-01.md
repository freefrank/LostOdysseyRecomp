# Cutscene depth and floor jitter — 2026-10-01

> **Historical checkpoint.** Version, issue, process and pending-work statements below describe their original date. Recheck current delivery and acceptance in [STATUS](../STATUS.md) and [ROADMAP](../ROADMAP.md).

The user reported a new flicker in a cutscene on v0.7.25 (Vulkan, 3840x2160, DLAA requested) and took an F1 capture, `render-17908971077376209-f6813.zip`. The cutscene was paused for the capture, so frames 6814 and 6815 are identical and show the paused image, not the flicker itself. The scene has a large gear-like ring in front and glowing circles on the floor. Earlier in the same run, the runtime suspect log reported both shaders below at frame 6181: `e9b8dd7e7c5a3425` as `depth_writer_after_jittered_geometry` (draw 90, depth write) and `d31e2122a3b51434` as `after_jittered_geometry` (draw 282). Their camera companion was the jittered `52e4405f97159d2f`, with the same world and the same camera.

## Offline review

The capture's `render-state.txt` (frame 6814) has both shaders unjittered (`slot=-1`) after jittered scene-camera draws:

| Draw | VS / PS | Camera | Depth | Notes |
|---|---|---|---|---|
| 2 | `52e4405f97159d2f` / none | c4–c7 | write | Mapped slot 4, jittered. First scene-camera draw. |
| 92 | `e9b8dd7e7c5a3425` / `afd81c283884ce86` | c8–c11 | write | Same camera bits as draw 2. |
| 283–287 | `97f07e5d73418e64`, `99c2b4b0960a9ccd` / `d55a20d004031279` | c0–c3 | stencil | Mapped slot 0, jittered; draw 287 resolves `0xb0d9000`. |
| 289 | `d31e2122a3b51434` / `4907455386b2b291` | c8–c11 | test, no write | Samples that resolve as tex0 (`resolve_age=0`, 3840x2160). |

Shader review, from `shaders/<hash>.hlsl` in the capture:

- `e9b8`: world position through c0–c3, then c8–c11 into `oPos` and its copy `o2`. c7 transforms the material UV (`o0`), c12 is the eye position for the view vector (`o3`), c4–c6 a rotation basis. PS `afd8` reads only `i2.w`, which jitter does not change.
- `d31e`: the same position path into `oPos` and its copy `o5`. c7 is the UV transform, c12 a light direction, c13 the eye position, c4–c6 the basis. PS `4907` computes `rcp(i5.w) * i5.xy * c0.yx + c0.zw` and samples tex0 there, the same lookup as the f6131 late floor (`2078` + `4013`).
- c8–c11 of both draws are bit-identical to `52e4` c4–c7 and to c0–c3 of the tex0 producers. Since the producers are jittered with that camera, the tex0 lookup must follow the raster jitter.

The 2026-09-30 feedback archive has no record of either VS, so these two pairs are the only observed partners. Mapping is VS-wide like the other slot-8 entries, so a new PS partner reopens this review.

## Implementation and validation

`PositionVPSlot` maps both shaders to slot 8 (`temporal_scene.h`). The renderer still requires the scene-camera anchor, the exact unmodified camera bits, the same depth allocation and a matching viewport before it jitters a draw.

`LoTemporalJitterTest --captured-f6814-cutscene` uses the exact register banks of draws 2, 92, 284 and 289 (`tools/tests/f6814_cutscene_jitter_capture.h`). For 32 phases at 2560x1440 and 3840x2160 it checks that the jittered clip of each late draw equals the 52e4 depth reference for the same point, that Z, W, world, UV, lighting and pixel constants stay unchanged, that d31e's tex0 lookup lands on the jittered producer pixel, and that a camera or depth-allocation mismatch is rejected. It passed 132,484 checks. Without the mapping, the clip and tex0 lookup were up to 0.488 px off. The full test passed 3,935,294 checks.

| Fact | State |
|---|---|
| Source and capture available | Yes |
| Offline program review | Done (above) |
| Implemented mapping | Done |
| Automated validation | Passed |
| Scene validation in the playing cutscene | Pending |
| Player acceptance | Pending |
