# Project status

## 对白倍速修复（2026-09-05 最新结果）

用户已确认原始对白和修复后的游戏录音听感正常。XMA 跨包帧结束时曾额外跳过续接包中的新帧，导致对白被截短；现已保留这些帧，不改变采样率或音量。同段装甲车对白从 2129 帧恢复到完整 4062 帧，游戏录音相对原始音轨的时间斜率从 0.547 恢复为 1.000。另有 34 个多声道缓冲、4264 帧解码零错误。

本地验证构建 `135DCA79` 已安装，存档和配置文件校验未变。验证范围为该过场与上述音频样本；其他对白、战斗声音和循环子帧边界仍需覆盖。详细证据见 [音频记录](notes/audio-output.md)。下方较早实验的未完成状态按各自日期理解。

Reviewed **2026-09-05** against code, runtime evidence and user feedback. This is the current ledger; dated investigation notes describe individual experiments.

## Published versus local

Code checkpoint: runtime `2a4f47d`, debug/encounter `42c543f`, graphics `c4fc9e4`. These commits include the later repairs and tests. Submission does not change the validation boundaries below.

| Scope | Repository status | Validation |
|---|---|---|
| Title, geometry/material and post-battle whiteout fixes; basic debug victory/storage | Published through `d944148` and earlier | Selected opening scenes |
| Persistent logs and GPU stall stages | Published `81f955d` | Seven-second test-thread suspension detected; resumed at 30 fps |
| XMA decoding, stereo PCM and loop handling | Published `21d6523` | Build, loop tests and runtime samples; sound gaps remain |
| Encounter recovery, printf/switch repair, storage refinements | Committed `2a4f47d` / `42c543f` | Selected natural victories and save/reload checks |
| Coordinate and POI teleport | Published with this feature | Hypocenter backend gates/POIs; Gorge window controls, movement and return |
| Map ID/name menu | Published with this feature | Title unknown state; map 2 Hypocenter, map 4 Gorge, live transition from 2 to 3 Edge of Wasteland |

Local results do not guarantee identical behavior from a clean checkout. See [dependency patches](../tools/patches/README.md).

## Active issues

Local `7ABDBAFD` prepares resource shaders with logical CPU threads minus one (minimum one). On this 16-thread PC, 15 workers reduced preparation from 53.4 to 6.7 seconds in a same-binary 2000-shader comparison; all 1998 successful DXIL outputs are byte-identical, with the same two failures. Cache reuse and Map12 loading pass. Resource scanning and PSO creation are unchanged. See [shader preparation](notes/shader-preparation.md).

Current implementation and evidence are summarized in the [2026-09-05 work report](WORK_REPORT_2026-09-05.md). Local executable **14F09F15…** also disables stale guest pixel shaders in depth-only draws. Camp movement tests retain Kaim ground shadows after this repair; other maps and encounters still need visual regression. The earlier **9693E361…** batched-clear startup repair remains included. See [startup crash](notes/startup-depth-clear-crash.md).

| # | User report / request | Current result and remaining work |
|---|---|---|
| 1 | Kaim/enemy self-shadow flicker | Open. User confirms dynamic shadows flicker on characters in Map 13. Polygon offset and explicit zero-LOD sampling are implemented; the remaining character-surface failure needs scene-specific capture. Map 12 poster black patches are fixed in local AC3B3A73: same-binary 300-frame A/B and shallow-depth GPU occlusion test pass. Installed into the main build directory; no game launched. See [poster depth repair](notes/map12-poster-depth.md). A shared cause with Map13 remains unproven. |
| 2 | Fire-hit black/red checker flicker | Open. Console reference and captured lighting inputs retained; Xenia also glitches. |
| 3 | Ground character shadows disappear across maps | User now confirms ground projections are basically fixed on the current build. 14F09F15 binds no guest PS for mode 5, fixing stale depth-exporting PS use in stencil volumes; camp movement captures and targeted GPU stencil tests pass. This does not close character-surface shadow flicker or claim exhaustive full-game validation. See [shadow evidence](notes/shadow-texture-lod.md). |
| 4 | Ring outer ring missing | Confirmed working by the user on 328352E9 with a physical controller. Hold RT after confirming the attack to start contraction, then release when the rings overlap. Earlier missing-ring reports in this session occurred while waiting for the ring without holding RT; logs showed progress 0 and size 1280. No new rendering fix was needed. See [Ring evidence](notes/battle-ring-resource.md). |
| 5 | Broken crates show black effects | Open; no verified fix. |
| 6 | Current map ID/name | Published, verified in opening areas. See [map info](notes/debug-map-info.md). |
| 7 | Optional save-anywhere | Local backend and save/restart/load verified, including native save-point behavior and camp permissions. Reorganized F1 menu on FB225C29 places the toggle at the top and supports resizing/scrolling. Isolated Win32 tests cover three sizes and checkbox callbacks; no new real-game save/reload or multi-DPI validation. See [save-anywhere](notes/save-anywhere.md). |
| 8 | Camp/window hangs | Big-endian critical-section deadlock fixed and tested. Separate intermittent GPU query/wait pointer corruption remains unresolved. A successful route is not a complete stability result. |
| 9 | Sound output and missing voices | Initial output published. Local I/O locking, XMA command/cursor and packet-skip fixes tested. Camp → vehicle CG → city gate/control passed without the former stable decoder errors. Full dialogue audibility and long-run stability remain unverified. |

Manual save success was confirmed by the user. These are scoped results, not full-game completion. See [audio](notes/audio-output.md), [critical sections](notes/critical-section-endian.md) and [query failures](notes/third-map-hang.md).

## Outside validated support

Full-game compatibility, WMV decoding, four-disc integration, native Linux/Vulkan gameplay, unlocked frame rates, HDR and upscaling. Rumble is disabled by default; GPU occlusion results remain approximations.

## Maintenance

[Roadmap](ROADMAP.md) · [Handoff](notes/handoff.md) · [Research index](notes/README.md) · [Archive](archive/README.md)

Private data and `out/` captures are not distributed. Update this ledger and both READMEs when support changes. Record implementation, test scenario and limitations separately.
