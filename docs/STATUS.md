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

Current implementation and evidence are summarized in the [2026-09-05 work report](WORK_REPORT_2026-09-05.md). The latest local executable is **10F4D144…**; its shadow-clear change has passed build and mapping tests, but has **not yet been run in game**.

| # | User report / request | Current result and remaining work |
|---|---|---|
| 1 | Kaim/enemy self-shadow flicker | Open. Polygon offset and explicit zero-LOD sampling are implemented; no complete visual validation. |
| 2 | Fire-hit black/red checker flicker | Open. Console reference and captured lighting inputs retained; Xenia also glitches. |
| 3 | Encounter shadows absent/flickering | Local clear bug identified: partial clears wiped an entire aliased shadow target. Tile/MSAA coverage mapping implemented and tested; in-game atlas and visual regression pending. |
| 4 | Ring outer ring missing | Verified in selected encounters: visible changing outer ring, RT release, Good and 101 damage; repeated after natural victory. See [Ring evidence](notes/battle-ring-resource.md). |
| 5 | Broken crates show black effects | Open; no verified fix. |
| 6 | Current map ID/name | Published, verified in opening areas. See [map info](notes/debug-map-info.md). |
| 7 | Optional save-anywhere | Local backend and save/restart/load verified, including native save-point behavior and camp permissions. Desktop checkbox interaction/layout pending. See [save-anywhere](notes/save-anywhere.md). |
| 8 | Camp/window hangs | Big-endian critical-section deadlock fixed and tested. Separate intermittent GPU query/wait pointer corruption remains unresolved. A successful route is not a complete stability result. |
| 9 | Sound output and missing voices | Initial output published. Local I/O locking, XMA command/cursor and packet-skip fixes tested. Camp → vehicle CG → city gate/control passed without the former stable decoder errors. Full dialogue audibility and long-run stability remain unverified. |

Manual save success was confirmed by the user. These are scoped results, not full-game completion. See [audio](notes/audio-output.md), [critical sections](notes/critical-section-endian.md) and [query failures](notes/third-map-hang.md).

## Outside validated support

Full-game compatibility, WMV decoding, four-disc integration, native Linux/Vulkan gameplay, unlocked frame rates, HDR and upscaling. Rumble is disabled by default; GPU occlusion results remain approximations.

## Maintenance

[Roadmap](ROADMAP.md) · [Handoff](notes/handoff.md) · [Research index](notes/README.md) · [Archive](archive/README.md)

Private data and `out/` captures are not distributed. Update this ledger and both READMEs when support changes. Record implementation, test scenario and limitations separately.
