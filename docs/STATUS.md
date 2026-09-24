# Project status

## Unreleased (Post-v0.6.11 Work in Progress) / 未发布（v0.6.11 之后工作进展）

Status as of 2026-09-24:
The latest bounded P2 battle follow-up is an unpublished checkpoint based on the pushed integration at `a4792244a195545847a33b7708969c108268e361`. Candidate `cb41c99` (source identity `d78d80f3add32494c53f67702d27d52c5c5e23aa82d690339cba046768c6357e`) passed CPU checks for 768 clips across three exact shader-slot mappings. GPU fixtures compiled six mapped microcodes to DXIL and SPIR-V and passed Vulkan replay checks for alpha discard, palette motion, and two-quad geometry. A manual RTX 5080 Vulkan battle run recorded 2,476 completed FSR uses with zero motion-vector first-failure records in that run. This does not establish user visual acceptance or whole-game coverage. `02d8` remains unmapped; `f6` is fixture-only and was not observed in that game run. P2 remains in progress. See the [dated handoff](notes/fsr-dlss-fg-codex-handoff.zh-CN.md) for the exact validation boundaries and outstanding platform/scene work.

Following the publication of v0.6.11, diagnostics and fixes were implemented on the development branch. Commit `0625923` resolved temporal history resets and jitter preservation across frame gaps (BR-01) as well as multithreaded capability snapshot races (BR-02). Subsequent work in this delivery includes:
- **Graphics Menu Stability & Runtime Feedback (BR-03 & GraphicsRow)**: Replaced raw positional indexing with a shared `GraphicsRow` (`0..10`) enum across navigation, actions, help text, and test fixtures. Active DLSS effect reporting requires verified production renderer target adoption and checked Vulkan submission, distinguishing persistent capability/failure latches from dynamic fallbacks (motion pending, unknown color encoding, feature recreation, promotion mapping failure, no eligible drawables, waiting for initial results, input probe mode, and stopped status) using triple identity filtering (`deviceEpoch`, `requestSignature`, `geometryEpoch`).
- **Structured DLSS Diagnostics Logging**: Logs settings save feedback (upscaler, quality, frame-rate) and structured DLSS status transitions (`Off`, `AwaitingExecution`, `Submitted`, `Fallback`, `NeedsVulkanRestart`, `DeviceUnavailable`, `TemporaryFallback`, `InputProbeOnly`, `GpuStopped`) with readable reasons, extents, and execution context. Repetitive transitions are deduplicated and recovery is logged as standard `Submitted`.
- **Pre-Present Swapchain Screenshot Capture**: In F1 render-state capture, `screenshot.bmp` now captures the final pre-present swapchain backbuffer (reflecting DLSS, letterboxing, and post-processing when active) ahead of display presentation, while `guest-frontbuffer.bmp` preserves the resolved host texture with paired `XE_SWAP` tickets, frame IDs, and submission serials/fences. Normal frames without capture requests execute zero readback allocations, copies, or memory mappings.
- **DLAA Depth View Lifetime UAF Fix**: Resolved an access violation in `VulkanTextureView` destructor when switching from DLSS Quality to DLAA by binding retired motion stencil views to `externalDepth` identity and releasing views upon GPU completion before texture destruction.
- **Synchronous NGX Evaluate Capture**: Controller captures isolated pre-Evaluate input color copies and post-Evaluate scratch output copies (restoring `VK_IMAGE_LAYOUT_GENERAL` ahead of composite/UI), exporting `dlss-evaluations.json`, `dlss-input-NNN.bin` / `-preview.bmp`, and `dlss-output-NNN.bin` / `-preview.bmp` (RGBA8/RGBA16F) with sub-pixel jitter, reset flags, and execution identities. Non-evaluated frames record explicit zero-evaluate fallback reasons without mock images.
- **Verification & Build Baseline**:
  - CPU tests: `LoDlssRuntimeStatusTest` (37 checks), `LoDlssCapabilitySnapshotTest` (43 checks re-run with revised Active semantics), `LoVideoSubmissionStopTest` (14 checks), `LoDlssStatusLogTest` (400 logger checks), `LoPresentCaptureTest --case close`, and `LoDlssEvaluateCaptureContractTest --evaluate-capture-contract-only`.
  - Hardware fixtures (RTX 5080): `LoPresentCaptureTest` (4 D3D12/Vulkan cases), `motion_replay_gpu_test.exe --depth-retirement-only` (26 Vulkan D32S8 checks), and `LoNativeDlssRendererTest.exe --evaluate-capture-only`.
  - Live session: Quality -> DLAA switching and export of frames 2961–2963 ran without crash in live user session (runtime-1790127353540651.log).
  - Executable target `LostOdysseyRecomp` completed linking successfully: `build/LostOdysseyRecomp/LostOdysseyRecomp.exe` (93,635,072 bytes, SHA-256 `08d50e3774d18a02d4f6eaf2267472e9fab75db36e3ee970980aa96faf641e9d`, UTC 2026-09-23 02:32:58 / local 2026-09-22 20:32:58 -0600, source fingerprint `bdd9539ee4f176bdda0d9660bb5621b8a90a09acf8f8faa8427c10f2075c2688`).
- **Remaining Scope**: Visual quality, motion response, fine lines, occlusion, and player acceptance remain unverified. K-01 diagnostic cleanup, broader graphics menu proposals, and official release publication remain pending.

---

## v0.6.11 published / v0.6.11 已发布

v0.6.11 was published on 2026-09-22T06:44:37Z from tag/source commit
`3daba372ea34c93b65b55c25ee5daa4f4ed5573d` via Release CI
[35687931776](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35687931776) as the
latest public release (non-draft, non-prerelease). It packages experimental native NVIDIA DLSS Super
Resolution (SR) and DLAA support, in-game Graphics menu upscaler options, list viewport scrolling,
and official packaged NVIDIA NGX runtime libraries on Windows and Linux.

Release and delivery verification:
- Release CI [35687931776](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35687931776) succeeded from commit `3daba372ea34c93b65b55c25ee5daa4f4ed5573d`. Public release packages are available at [GitHub Release v0.6.11](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.11).
- Windows ZIP `LostOdysseyRecomp-windows-x64-v0.6.11.zip` SHA-256 matches sidecar and GitHub digest: `362592d16f45bc56fd686c4f0bc4c4e23e8a82eafffdcd8f6c77971ed3c9e823`.
- Linux AppImage `LostOdysseyRecomp-linux-x64-v0.6.11.AppImage` SHA-256 matches sidecar and GitHub digest: `2e2ada519ae7b3b63be306d7d301bf26be677c0af08f13b7653c2e54c9727bf9`.
- Both package files and their `.sha256` sidecars returned HTTP 200. Windows manifest confirms clean non-development version `v0.6.11`, build commit `3daba37`, and bundled `nvngx_dlss.dll` matching the audited SDK 310.9.1 (`374959484e79a640feaba44c93ac8cfb0a03f5b5`) with verified LICENSE and NOTICE. Linux AppImage SquashFS inspection confirms canonical `usr/bin/libnvidia-ngx-dlss.so.310.9.1`, library symlinks, license, and notice.
- Published release notes match the extracted CHANGELOG section.

Status & Verification Limits:
- **Experimental Status**: DLSS and DLAA remain experimental features with bounded verification. Gate 3 is not approved. Visual quality, motion response, fine lines, occlusion, UI elements, reset behavior, and full player acceptance are not claimed.
- **Implemented Scope**:
  - Official NVIDIA DLSS SDK `310.9.1` integration with static CRT on Windows and static libraries on Linux.
  - Plume Vulkan bridge hooks (`VulkanExtensionHooks` / `VulkanExtensionStatus`), external command boundaries, and capability probes (`LoNativeDlssProbe`, `LoNativeDlssReportTest`).
  - CPU frame planner supporting 24-word snapshot packets, request signatures, geometry epochs, and exact NGX output sizing queries for 16:9, 21:9, and non-standard drawables.
  - Renderer capture of pre-TAA color, single-channel R32 depth, unjittered geometric motion vectors, and fence-qualified resource retirement.
  - Experimental DLAA mode (quality index 3) operating 1:1 input to output on supported RTX hardware, backed by 350 CPU contract checks (`LoNativeDlaaTest`).
  - Persistent NGX session controller (`NGX_VULKAN_CREATE_DLSS_EXT1`, `NGX_VULKAN_EVALUATE_DLSS_EXT`), monotonic submission-serial tracking, SDR color encoding bypass, and target promotion architecture with parked low-resolution fallbacks.
  - In-game Settings menu: **Upscaler** (`Off`, `DLSS`), **DLSS quality** (`Quality`, `Balanced`, `Performance`, `DLAA`, hidden when Upscaler is Off), Internal resolution row removed from UI (persisted value retained in configuration for legacy fallbacks), list viewport scrolling (>11 rows) with hidden-row awareness and indicators, and Start/Enter focus-jump to Save without saving (same-tick confirm suppressed).
  - MSVC compilation fix for native DLSS test fixture using `/utf-8` in `tools/tests/native_dlss/CMakeLists.txt`.
- **Evidence Baseline**:
  - CPU test suites passed (8/8 native DLSS suites including 62 frame-plan checks and 350 DLAA checks).
  - Menu rendering tests (`LoMenuRenderTest`) passed synthetic overflowing list pixel checks (>11 rows with hidden row and indicators).
  - Menu flow tests (`LoMenuFlowTest`) passed navigation, hidden-row skipping, pointer click boundaries, and Start/Enter focus jumps.
  - Windows and Linux release packaging jobs passed in Release CI.
  - Bounded live-game production run on an RTX 5080 confirmed Quality NGX SR (`1707x960 -> 2560x1440`, DisplayEncoded color, reversed-Z depth, 1 Create, 24 retained successful Evaluate records under a 128-record cap). Runtime logs show NGX availability on RTX 5080, sizing across 1440p and 4K switches, and transient motion pipeline pending fallbacks; these do not establish full DLAA image-quality or player acceptance.
- Detailed verification records are maintained in [Native DLSS Validation](notes/native-dlss-validation.md) and [Native DLAA Initial (zh-CN)](notes/native-dlaa-initial.zh-CN.md).

v0.6.11 已发布：

v0.6.11 已于 2026-09-22T06:44:37Z 从 tag/source commit `3daba372ea34c93b65b55c25ee5daa4f4ed5573d` 通过 Release CI [35687931776](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35687931776) 正式发布为 latest 公开版本（非 draft、非 prerelease）。包含实验性原生 NVIDIA DLSS 超分辨率（SR）与 DLAA 支持、游戏内图形设置缩放技术选项、长列表视口滚动以及 Windows/Linux 正式发布包官方 NVIDIA NGX 运行库打包。

发布与资产核验：
- Release CI [35687931776](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35687931776) 构建成功，公开资产见 [GitHub Release v0.6.11](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.11)。
- Windows ZIP `LostOdysseyRecomp-windows-x64-v0.6.11.zip` SHA-256 与 sidecar 和 GitHub digests 一致：`362592d16f45bc56fd686c4f0bc4c4e23e8a82eafffdcd8f6c77971ed3c9e823`。
- Linux AppImage `LostOdysseyRecomp-linux-x64-v0.6.11.AppImage` SHA-256 与 sidecar 和 GitHub digests 一致：`2e2ada519ae7b3b63be306d7d301bf26be677c0af08f13b7653c2e54c9727bf9`。
- 两平台包及各自 `.sha256` sidecar 均返回 HTTP 200。Windows manifest 报告版本 `v0.6.11`、commit `3daba37`，内置 `nvngx_dlss.dll` 与 SDK 310.9.1 严格吻合；Linux AppImage 经 SquashFS 解构核实包含 `usr/bin/libnvidia-ngx-dlss.so.310.9.1`、库软链及 License/Notice 文件。
- 公开 Release 说明与 CHANGELOG 提取一致。

当前开发分支未发布进展（2026-09-22）：
- 提交 `0625923` 已修复时序历史时钟推进与长间隔抖动丢失（BR-01），并通过按值快照消除了能力查询并发数据竞争（BR-02）。
- 本轮工作区实现并验证了图形菜单稳定行索引枚举（`GraphicsRow` 0..10）与真实渲染器执行反馈闭环（BR-03），细化回退状态并在 checked Vulkan 提交成功后才报告 `Active`。
- 增加了结构化 DLSS 运行时状态日志与保存设置参数记录；F1 渲染状态捕获的 `screenshot.bmp` 改进为提交给呈现系统的最终交换链画面，并实现了同次 NGX Evaluate 输入/输出缓冲及元数据捕获（`dlss-evaluations.json`、输入输出 raw 与 preview）。
- 修复了从 DLSS Quality 切换至 DLAA 时外部 depth 纹理先于已退休 stencil 视图销毁引起的访问违规（UAF）问题，经实机验证会话持续运行且成功导出帧 2961–2963 无崩溃。
- 最新完整游戏可执行文件目标本地增量构建成功（产物 `build/LostOdysseyRecomp/LostOdysseyRecomp.exe`，93,635,072 字节，SHA-256 `08d50e3774d18a02d4f6eaf2267472e9fab75db36e3ee970980aa96faf641e9d`，UTC 2026-09-23 02:32:58 / 当地 2026-09-22 20:32:58 -0600）。
- 真实游戏画质、运动表现、边缘与遮挡效果及玩家整体验收未宣称完成；K-01 等诊断路径与公开发布仍待后续开展。

## v0.6.7 published / v0.6.7 已发布

v0.6.7 was published on 2026-09-20T20:09:28Z from tag/source commit
`f92c24da03816b4c0c7664fbc8589169a205b555` via Release CI
[35533399325](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35533399325) as the
latest public release (non-draft, non-prerelease). It packages the in-game Graphics menu
Widescreen switch and expanded 21:9 resolution presets, resolving and closing Issue #17.

A Widescreen toggle switch is added to the Graphics settings menu (`tab == 2`, row 2), directly above Output resolution (`row 3`). The switch categorizes Output resolution presets into two aspect-ratio groups:
- **Widescreen Off (16:9)**: 1280×720, 1600×900, 1920×1080, 2560×1440, and 3840×2160.
- **Widescreen On (21:9)**: 1720×720, 2560×1080, 3440×1440, 3840×1600, and 5120×2160.

The toggle state is inferred directly from the current configured width and height (`width * 9 > height * 16`), requiring no new INI keys. Existing configurations at 3440×1440 automatically show Widescreen On and index into the 21:9 list. When toggling the switch between 16:9 and 21:9, `FindNearestResolutionIndex` selects the closest preset by vertical height; for equidistant heights (such as 900p between 720p and 1080p), the higher tier is chosen. Selecting Save graphics settings persists the choice and transitions through the display-change state machine; cancelling or navigating back without saving discards changes and keeps the existing configuration. The first-run setup resolution list in `first_run.cpp` is synchronized to include matching presets, and strings and help text have been added for 5 languages (English, Japanese, Korean, Traditional Chinese, Simplified Chinese).

Release and delivery verification:
- Release CI [35533399325](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35533399325)
  succeeded from source commit `f92c24da03816b4c0c7664fbc8589169a205b555`. Public release packages
  are available at [GitHub Release v0.6.7](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.7).
- Windows ZIP `LostOdysseyRecomp-windows-x64-v0.6.7.zip` is 211,105,409 bytes with SHA-256
  `372324811075bc89ac30b3f9786fa7a5a93f02d8ee22b854980fa5acf7a9e279`.
- Linux AppImage `LostOdysseyRecomp-linux-x64-v0.6.7.AppImage` is 220,846,584 bytes with SHA-256
  `de99f9eeea3a83984faa09b098fc8469b8a9adc64974e63a710e9311a05f8bc4`.
- Both package files and their `.sha256` sidecars returned HTTP 200; downloaded hashes match
  sidecars and GitHub release digests. The Windows manifest confirms version `v0.6.7`, commit / build /
  packaging SHA `f92c24da03816b4c0c7664fbc8589169a205b555`, and `dirty=false`. Published release
  notes match the extracted CHANGELOG section.
- Implementation & user acceptance: Focused checks passed for `menu_flow_test` (auto-derivation,
  five ultrawide tiers cycling, height preservation when returning to 16:9, cancel discard, and Save
  state machine) and `menu_render_test` layout snapshot `out/snapshots/menu_1280x720_21_9.png`. In local
  runtime testing on the latest build, the user confirmed the menu and display change functionality
  works as expected, and authorized closing Issue #17 (now closed).
- Remaining limits: Ultrawide support remains experimental across diverse hardware and aspect ratio
  combinations; testing does not claim exhaustive verification across all GPUs and resolutions.

v0.6.7 已发布：

v0.6.7 已于 2026-09-20T20:09:28Z 从 tag/source commit
`f92c24da03816b4c0c7664fbc8589169a205b555` 通过 Release CI
[35533399325](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35533399325) 正式发布为
latest 公开版本（非 draft、非 prerelease）。版本包含游戏内图形设置“宽屏”开关及扩充的 21:9 分辨率预设，解决并关闭 Issue #17。

在“设置” -> “图形”中，“输出分辨率”上方新增宽屏切换开关：
- **宽屏关（16:9）**：1280×720、1600×900、1920×1080、2560×1440 与 3840×2160。
- **宽屏开（21:9）**：1720×720、2560×1080、3440×1440、3840×1600 与 5120×2160。

开关状态由当前配置宽高动态推导，不增加额外 INI 字段；原有 3440×1440 配置自动识别为宽屏开。切换比例时按高度差最近匹配目标档位（等距选较高档位，如 900p 转 1080p）。保存应用新分辨率并写入磁盘，取消不改动配置。首次启动列表同步包含对应预设，已适配英、日、韩、繁中、简中五语言。

发布与资产核验：
- Release CI [35533399325](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35533399325)
  从源码 `f92c24da03816b4c0c7664fbc8589169a205b555` 构建成功，公开资产见 [GitHub Release v0.6.7](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.7)。
- Windows ZIP `LostOdysseyRecomp-windows-x64-v0.6.7.zip` 大小为 211,105,409 字节，SHA-256 为
  `372324811075bc89ac30b3f9786fa7a5a93f02d8ee22b854980fa5acf7a9e279`。
- Linux AppImage `LostOdysseyRecomp-linux-x64-v0.6.7.AppImage` 大小为 220,846,584 字节，SHA-256 为
  `de99f9eeea3a83984faa09b098fc8469b8a9adc64974e63a710e9311a05f8bc4`。
- 两平台包及各自 `.sha256` sidecar 均返回 HTTP 200；下载包 hash 与 sidecar 和 GitHub digests 一致。Windows manifest 报告版本 `v0.6.7`、commit/build/packaging SHA 均为上述 commit 且 `dirty=false`。公开 Release 说明与 CHANGELOG 提取一致。
- 实现与用户验收：`menu_flow_test`（3440×1440 自动推导、五档循环、返回 16:9 保留 2160 高度、取消不保存及 Save 状态机）与 `menu_render_test` 渲染快照（`out/snapshots/menu_1280x720_21_9.png`）均通过。用户在最新构建实机测试中明确确认功能正常，并授权关闭 Issue #17（已关闭）。
- 剩余限制：超宽屏在多样化硬件与多分辨率组合下仍保持实验性，不虚构所有 GPU 与分辨率的全游戏穷尽验证。

## v0.6.6 published & reissued / v0.6.6 已发布与同版本重新发布

v0.6.6 was initially published on 2026-09-20T08:10:24Z from tag/source commit
`c953bb56857330a2238869b306ffda98fe41bcdd`. A same-version reissue was released on
2026-09-20 from source commit `c6cbd1f62414a46c00c6312559edcd8d217bc9ee` via Release
CI [35527543573](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35527543573)
to package a shadow-map rendering repair across all aspect ratios and high internal
resolutions. The initial `c953bb5` packages lacked this repair and are superseded;
players must redownload the release packages to obtain the fix.

The native ultrawide implementation allows internal render targets to follow aspect
ratios beyond 16:9 using Hor+ projection adjustments applied before derived matrices
and view-frustum culling, preserving perspective geometry across the expanded field
of view. HUD elements are constrained to a 16:9 safe region, and video playback applies
ordered left/right pillarbox bars. FramePlan manages queue epoch tracking and
render-target catalog roles.

Shadow fix & user testing: The shadow fix corrects effective-height render target
caching to avoid unnecessary 640×640 recreation and updates depth rasterization
without color writes to cover modes 4 and 5 while preserving `SV_Depth` and alpha.
In the tested scene, the user confirmed shadows are fixed. A temporary user report
of elevated CPU usage was traced to background system activity (`bun`) rather than
a game engine regression. Acceptance of the shadow fix remains limited to the tested
scene.

User note and limitations: Ultrawide support remains EXPERIMENTAL and currently ONLY
3440×1440 is supported (2560×1080 is not currently advertised or supported despite
appearing as an unverified UI option).

Release and validation verification:
- Reissued v0.6.6 release: Release CI [35527543573](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35527543573)
  succeeded from source `c6cbd1f62414a46c00c6312559edcd8d217bc9ee`. Replacement packages
  are verified and uploaded: Windows ZIP is 211,105,610 bytes with SHA-256
  `bed792e563ad31f0a167621f98f92fee6d83da1540943fe85bf95e286cb90180`; Linux AppImage is
  220,846,584 bytes with SHA-256 `4c5ac5e5ba763d108d972ca7c11d4dfafbcfb3a3efdb1325186b788e2bd0f946`.
  Downloaded hashes match sidecars and GitHub digests, and the Windows manifest reports
  clean version `0.6.6` with the correct source shader pack confirmed.
- Superseded initial v0.6.6 release: Release CI [35497779401](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35497779401)
  from commit `c953bb5` (Windows ZIP 211,105,665 bytes SHA-256 `c1bcae49...`, Linux AppImage
  220,842,488 bytes SHA-256 `19ff373a...`).
- Shadow fix validation: `windows-clang` runtime build passed (runtime SHA
  `1d7b75c98856a4da922de98088a5c066b95cf318636a1bf59bd1a15c7537d195`), and
  `LoTargetMappingTest` passed 7 checks. Existing unit fixtures passed for
  `LoFramePlanTest` (18 checks), resolution calculation (40 checks), and temporal
  math. In Vulkan live testing with bundled shaders, a native save loaded at 13.93s
  and captured two frames during scene transition at swaps 382–383, verifying
  3440×1472 padded color/depth allocations and 3440×1440 resolve content.
- Remaining limits: Comprehensive visual Hor+, HUD positioning, dynamic window
  resizing, broader scene shadow validation, failure injection paths, other backends
  (Direct3D 12), and full player acceptance remain pending.

v0.6.6 已发布与同版本重新发布：

v0.6.6 初版已于 2026-09-20T08:10:24Z 从 tag/source commit
`c953bb56857330a2238869b306ffda98fe41bcdd` 发布。随后于 2026-09-20 从 source commit
`c6cbd1f62414a46c00c6312559edcd8d217bc9ee` 通过 Release CI [35527543573](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35527543573)
完成同版本重新发布，以纳入针对全比例及高内部分辨率下的阴影贴图渲染修复。初版发布的 `c953bb5` 资产不含该修复并已被替代；玩家需重新下载发布包以获取修复。

原生超宽屏实现允许内部渲染目标跟随 16:9 以外的显示比例，并在派生矩阵计算与视锥裁剪前应用
Hor+ 投影调整，在拓展视野中保持正确的透视几何结构。HUD 界面元素被限制在 16:9 安全区内，
视频播放期间添加有序左右立柱黑边。FramePlan 负责队列周期跟踪与渲染目标分类角色管理。

阴影修复与用户测试：修正 effective-height 渲染目标缓存以避免不必要的 640×640 重建，并在无颜色写入的深度光栅化中覆盖模式 4 与 5，同时保留 `SV_Depth` 与 alpha。在受影响测试场景中，用户确认阴影已恢复正常。此前反馈的 CPU 占用上升经查为后台系统进程（`bun`）导致，非游戏回归。阴影修复的验收仅限于当前测试场景。

用户提示与限制：超宽屏支持仍为实验性（EXPERIMENTAL），目前仅支持 3440×1440（界面虽有 2560×1080 选项但尚未验证支持，请勿作为受支持分辨率使用）。

发布与验证核验：
- 重新发布的 v0.6.6 资产：Release CI [35527543573](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35527543573)
  从源码 `c6cbd1f62414a46c00c6312559edcd8d217bc9ee` 构建成功。替换包均已核验并上传：Windows ZIP 为 211,105,610 字节，
  SHA-256 为 `bed792e563ad31f0a167621f98f92fee6d83da1540943fe85bf95e286cb90180`；Linux AppImage 为 220,846,584 字节，
  SHA-256 为 `4c5ac5e5ba763d108d972ca7c11d4dfafbcfb3a3efdb1325186b788e2bd0f946`。下载包 hash 与 sidecar 和 GitHub digests 一致，
  Windows manifest 报告版本 `0.6.6`、clean 并确认内置着色器包。
- 已被替代的初版 v0.6.6 资产：Release CI [35497779401](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35497779401)
  从 commit `c953bb5` 构建（Windows ZIP 211,105,665 字节 SHA-256 `c1bcae49...`，Linux AppImage 220,842,488 字节 SHA-256 `19ff373a...`）。
- 阴影修复验证：`windows-clang` 运行时构建通过（运行时 SHA `1d7b75c98856a4da922de98088a5c066b95cf318636a1bf59bd1a15c7537d195`），
  `LoTargetMappingTest` 通过 7 项检查。既有 `LoFramePlanTest`（18 项）、分辨率计算（40 项）与时序数学测试均通过。在 Vulkan
  搭配内置着色器包测试中，原生存档于 13.93 秒成功载入，并在场景过渡期间捕获 swap 382–383 的两帧，确认 3440×1472（对齐）分配及 3440×1440 resolve 画面。
- 剩余限制：跨场景完整实机 Hor+ 视觉呈现、HUD 排布、动态窗口大小调整、更广场景阴影验证、故障注入路径、Direct3D 12 等其他图形后端及完整玩家验收仍待完成。

## v0.6.3 published / v0.6.3 已发布

v0.6.3 was published on 2026-09-19T23:56:08Z from tag/source commit
`93bdbc1ccae7652e38dc80db24a9d25a34a72a47`. It promotes the bounded sampled
comparison for large vertex-cache hits, the Issue #54 language-menu safety
correction, the Issue #53 file-I/O locking and bounded diagnostics, deterministic
I/O lifetime regression coverage, and platform-native asynchronous F1 archive
export. The focused vertex-cache fixture passed 3,668,957 checks; the other
validation records are reused from their existing bounded evidence. No new
game run, release-binary performance result or player acceptance is claimed.
Release CI [35476569158](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35476569158)
passed Windows and Linux packaging on its first attempt. The Windows ZIP and
Linux AppImage plus their sidecars matched package hashes and GitHub digests;
public sidecars returned HTTP 200. The Windows manifest reports version/source
version `0.6.3`, commit `93bdbc1` and `dirty=false`; all 49 payload hashes and
the embedded shader were verified. Linux native GPU, Steam Deck, AppImage
runtime and broader gameplay remain unverified.

v0.6.3 已于 2026-09-19T23:56:08Z 从 tag/source commit
`93bdbc1ccae7652e38dc80db24a9d25a34a72a47` 发布。版本包含大顶点缓存命中的有界采样比较、Issue #54 语言菜单安全修正、Issue #53 文件 I/O 锁范围修正与有界诊断、确定性的 I/O 生命周期回归覆盖，以及使用平台归档格式的异步 F1 归档导出。定向顶点缓存 fixture 通过 3,668,957 项检查；其余验证记录沿用已有的有界证据。本次未新增游戏运行、发布二进制性能结果或玩家验收结论。Release CI [35476569158](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35476569158) 首次通过 Windows/Linux 打包。Windows ZIP、Linux AppImage 及其 sidecar 与包 hash 和 GitHub digest 一致，公开 sidecar 返回 HTTP 200。Windows manifest 报告版本／source version 为 `0.6.3`、commit `93bdbc1`、`dirty=false`；49 个 payload hash 和内置 shader 均已核验。Linux 原生 GPU、Steam Deck、AppImage 运行时和更广游戏流程仍未验证。

## Issue #57 vertex-cache sampling in published v0.6.3 / Issue #57 已发布 v0.6.3 中的顶点缓存采样

The published v0.6.3 source prioritizes vertex-cache CPU cost: small
vertex buffers still use exact comparison, while large vertex-cache hits use
bounded head/tail and strided samples. Index-cache hits retain complete
source-byte verification. This can miss a synthetic mutation outside the
sampled bytes, but no known game bug has been caused by sampling. The focused
Clang `-O2` `LoVertexCacheTest` passed 3,668,957 checks, including sampled
changes, small-buffer exactness and the selected large-vertex blind-spot
policy. This is source-level fixture evidence only; no game run, release-binary
performance result or player acceptance is claimed. A reliable low-cost vertex
write/invalidation mechanism remains backlog work.

公开 v0.6.3 源码优先降低顶点缓存 CPU 成本：小顶点缓冲仍使用精确比较，大顶点缓存命中改用有界的头尾片段和跨区采样；index cache 命中继续保留完整源字节校验。采样字节之外的合成修改可能漏检，但目前没有任何已知游戏 bug 由采样引起。Clang `-O2` 定向 `LoVertexCacheTest` 通过 3,668,957 项检查，覆盖采样变化、小缓冲精确性和已选择的大顶点采样盲区策略。这只是源码 fixture 证据，不代表实机运行、发布二进制性能或玩家验收。可靠且低成本的顶点写入／失效机制仍列入 backlog。

## Issue #54 language-menu safety correction / Issue #54 语言菜单安全修正

The language menu now ignores invalid table counts and indices without reading or rewriting the selected language, shows `—` for unavailable entries, and follows the native parser's 16-entry capacity. The existing `82481BE8` USA/Europe host-language mapping and independent text/voice semantics are unchanged. `LO_TRACE_LANGUAGE=1` enables bounded opt-in tracing for lookup, menu and native-cache stages. The specific cause of the reported cutscene voice issue remains unconfirmed; no save was available and no real-game reproduction was performed. The correction is included in published v0.6.3 and does not establish Issue #54 acceptance.

语言菜单现在会忽略无效语言表数量和索引，不读取或改写当前选择；无效项显示为 `—`，容量遵循原生 parser 的 16 项限制。既有 `82481BE8` USA/Europe 宿主语言映射及文字／配音独立语义保持不变。`LO_TRACE_LANGUAGE=1` 可开启默认关闭且有界的 lookup、菜单和原生缓存阶段追踪。具体过场配音问题的根因尚未确认；没有存档，本次未做实机复现。修正已包含在公开 v0.6.3 中，也不代表 Issue #54 已完成验收。

## Experimental geometric motion vectors (v0.6.2 / v0.6.2 已纳入)

Development progress on the experimental geometric motion replay pipeline:
- Polygon-offset gating allows self-consistent constant depth bias while continuing to reject slope bias and non-finite values.
- Depth-only replay pixel shader wrapper defines `XE_SAMPLE(t, s, uv)`, resolving DXIL/SPIR-V compilation errors when microcode has no pixel program.
- Background asynchronous compilation for generated replay shaders is throttled to at most 2 concurrent jobs, preventing scene loading hangs and black screens.
- Stable occurrence-order matching handles repeated `DrawHistoryKey` instances across adjacent frames instead of blanket rejection, raising automated Bell debug matching from ~533/955 to a stable 955/955 matched/replay draws (`ready=true consume=true`) and closing the reactive mask coverage gap.
- Verification: Windows runtime build passed; `motion_vector_test` passed 40 lifecycle and occurrence checks; `motion_replay_gpu_test --compile-only` passed 11 DXIL and SPIR-V compilation checks; automated Vulkan execution is stable.
- Boundaries and user feedback: The user confirmed motion vector consumption (`consume=true`) is active and beneficial. Visible shimmer/jitter in the Bell sequence remains present under investigation as a separate TAA issue; Bell visual quality is NOT marked resolved or accepted. D3D12 replay PSO creation returns `E_INVALIDARG 0x80070057` and remains tracked follow-up work.

实验性几何运动矢量（motion replay）开发进展：
- 修正多边形偏移（polygon offset）门控，允许自洽的恒定深度偏移（depth bias），同时继续拒绝斜率偏移与非有限值。
- 在仅深度（depth-only）replay 像素着色器包装中补齐 `XE_SAMPLE` 定义，解决 DXIL/SPIR-V 编译失败。
- 后台异步编译生成的 replay 着色器，并发数上限为 2，避免场景加载过程中的卡顿与黑屏。
- 采用帧内稳定提交顺序（occurrence）配对重复 `DrawHistoryKey`，替代此前的整帧丢弃策略；Bell 自动化调试场景匹配数由约 533/955 提升至稳定的 955/955 matched/replay（`ready=true consume=true`），消除 reactive 遮罩缺口。
- 验证：Windows 运行时构建通过；`motion_vector_test` 通过 40 项生命周期与 occurrence 检查；`motion_replay_gpu_test --compile-only` 通过 11 项 DXIL/SPIR-V 编译检查；Vulkan 自动化运行稳定。
- 边界与用户反馈：用户已确认 MV 被正常消费（`consume=true`）；Bell 场景中的可见抖动依然存在，属于继续排查的 TAA 问题，未标记为已修复或已验收。D3D12 下 replay PSO 创建返回 `E_INVALIDARG 0x80070057`，仍为已记录的后续待办。

## v0.6.2 published / v0.6.2 已发布

The v0.6.2 release applies the accepted Uhra TAA policy to the normal
TAA path: 0.5 jitter scale, stationary motion snapping, stationary color
clipping and multi-surface history, with RGBA8 history at `31/33`; FP16 history
and moving bilinear fallback remain off. Geometric motion vectors are enabled
by default for TAA, with `LO_MV_ENABLE=0` retained as a comparison switch.

On Vulkan with an RTX 5080, the user accepted image quality in the same Uhra
4K internal/output steel-frame scene at about 60 FPS. Hidden muted A-B-A-B
captures without pacing measured 60.34/59.00 FPS for the candidate, compared
with 54.61 FPS for a separate Release build and 54.57 FPS for the former
RelWithDebInfo main binary. MV audit steady tracked/matched/replay was 988 with
failed 0; `LoMotionVectorTest` passed 71 checks and `LoVertexCacheTest` passed
3,668,948 checks. These are local, scene-bounded results. The 1080p-internal
to 4K moving-camera limitation, broader scene coverage and D3D12 replay PSO
follow-up remain open. [v0.6.2](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.2) was published on
2026-09-19T21:16:38Z from tag/source commit
`7f99786f302b4ef3e5f672eacdba2b7a62972fda`. Release CI
[35467796768](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35467796768)
succeeded on its second attempt for Windows/Linux Release packaging. The final
delivery has four public assets: Windows ZIP, Linux AppImage and their
`.sha256` sidecars; both packages include the shader set and there is no
separate shader package. The Windows manifest reports version/source version
`0.6.2`, commit `7f99786` and `dirty=false`; package hashes match sidecars and
public sidecars returned HTTP 200. The Windows ZIP SHA-256 is
`99495f62315f44bfa1eb34ce294b8b08c9e173193e86482c2dfa4427b10963c7`; the Linux
AppImage SHA-256 is
`a4542b8eeee6b5ac27f4dc8e4e8f0ec184e1f0c7620e8846429a1455b3942ecd`.
The first CI attempt failed after both platform compilations because the draft
lacked shader input. The successful attempt downloaded a temporary v0.6.1
shader ZIP from the draft and verified its runtime compatibility; that input
was removed before publication. Later releases can fall back to the published
v0.6.1 asset.

v0.6.2 已将 Uhra 验收过的 TAA 策略应用到正常 TAA 路径：0.5 抖动幅度、静止运动
snap、静止颜色裁剪和多表面 history，RGBA8 history 权重为 `31/33`；FP16 history 和
moving bilinear fallback 仍关闭。TAA 默认启用几何运动矢量，`LO_MV_ENABLE=0` 仍可作为
对照开关。

RTX 5080 的 Vulkan、Uhra 4K 内部／输出同一钢架场景中，用户以约 60 FPS 接受画质。隐藏
静音、无 pacing 的 A-B-A-B 对照中，候选为 60.34/59.00 FPS，独立 Release 构建为 54.61
FPS，之前的 RelWithDebInfo 主程序为 54.57 FPS。MV audit 的 steady tracked/matched/replay
为 988，failed 为 0；`LoMotionVectorTest` 通过 71 项，`LoVertexCacheTest` 通过 3,668,948
项。这些是本机和限定场景结果。1080p internal 到 4K output 的移动相机限制、更广场景覆盖和
D3D12 replay PSO 后续工作仍开放。v0.6.2 已公开发布，Windows/Linux 包和各自
`.sha256` sidecar 已完成公开交付核验，但 Linux 原生 GPU、Steam Deck 和更广游戏流程
仍未实测。

The main branch workflow was simplified to retain Release packaging, Release-input
validation, online PPC source compilation and Issue triage. Seven redundant
workflow files were removed, and four older workflow records were disabled. Later shader-pack
input fallback uses the published v0.6.1 asset with runtime verification, and no
prebuilt PPC library is committed. These workflow changes are source-history
facts, separate from the bounded runtime acceptance above.

主分支 workflow 已简化，仅保留 Release 打包、Release 输入校验、Actions 在线 PPC 源码
编译和 Issue triage；七个冗余 workflow 文件已删除，另有四条历史 workflow 记录已停用。后续 shader
输入回退使用已发布的 v0.6.1 资产并进行运行时核验，不提交 prebuilt PPC 库。这些 workflow
改动属于源码历史事实，与上面的限定实机验收分开。

## v0.6.1 published / v0.6.1 已发布

The automatic updater now checks for a newer release before game-data import on Windows and Linux. A newer release opens an app-branded SDL prompt with release notes and Install/Later actions; accepting applies the update and relaunches before import, while declining or an offline check continues normally. Headless and background runs skip this UI. Windows runtime build, focused Windows/Linux prompt tests and changed Linux syntax checks passed. Live network update acceptance, physical controller input and GUI acceptance remain unverified.

Windows 和 Linux 的自动更新器现会在导入游戏资料前检查新版本。发现新版本时打开带有应用品牌的 SDL 提示，显示发布说明以及“安装／稍后”操作；接受后在导入前应用更新并重新启动，拒绝更新或无法联网时继续正常流程。无头和后台运行会跳过这套界面。Windows 运行时构建、Windows/Linux 更新提示专项测试和修改后的 Linux 语法检查已通过；在线更新接受、实体手柄和 GUI 验收仍未验证。

v0.6.1 已于 2026-09-18T17:49:18Z 从 `ebd2ef13969a28fcabf26a4eb2dafa9c09ca965d` 发布到 [GitHub Release](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.1)。[Release CI 35374267882](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35374267882) 的 Windows/Linux 发布任务及两平台定向回归均通过。公开六个资产的 sidecar 均返回 HTTP 200；Windows ZIP、Linux AppImage 和 shader pack 的 SHA-256 分别为 `fb0fdfc823c53515eac300d7596c7bfe98127402ba90476dd093b8c82e80c689`、`5ef83615d4eb16922e51f74ebaf6002aedd51bbb1bb2eb3dd19964a79317b47c` 和 `387a23b9328b8136847d48b37b574fddd600a526eb837807fbb08b758c6de4d9`。该 shader pack 与 v0.6.0 字节相同。

## v0.6.0 published / v0.6.0 已发布

The current main source repairs the remaining 0.6.0 audit findings. Large
vertex and index cache hits compare complete source content; the index cache is
bounded to a 64 MiB payload budget. `tools/drive_city.py --dry-run` is
read-only and protects save paths. The PPC timebase shares the pause-aware
high-resolution game clock. Linux update apply cleans completed staging and
restores the previous AppImage after a direct launch failure, while standalone
Windows recovery uses the helper as its runner source.

当前 main 源码已修复 0.6.0 审计剩余问题：大顶点和索引缓存命中会完整比较源内容，索引缓存有效载荷限制为 64 MiB；`tools/drive_city.py --dry-run` 不写入文件并保护存档路径；PPC timebase 与感知暂停的高精度游戏时钟统一；Linux 更新完成后清理暂存，直接启动失败时恢复旧 AppImage；Windows 独立恢复路径使用 helper 作为 runner 来源。

Focused evidence: Clang `-O2` vertex-cache, geometry and prerelease fixtures
passed 3,668,947, 16,809,648 and 16,438 checks respectively; five benchmark
save-safety cases passed; the independent WSL pause test passed; and isolated
POSIX apply fixtures passed success cleanup and launch-failure rollback. These
are release source checks. They do not establish full-game behavior, final
release-binary performance, a real AppImage update, a real Windows package
transaction, or Steam Deck acceptance.

定向证据：Clang `-O2` 顶点缓存、geometry 和 prerelease fixture 分别通过
3,668,947、16,809,648 和 16,438 项检查；基准工具存档安全测试 5 项通过；
WSL 独立暂停测试通过；POSIX 更新隔离 fixture 的成功清理和启动失败回滚通过。
这些是发布源码检查，不代表全游戏行为、最终发布二进制性能、真实 AppImage
更新、真实 Windows 安装包事务或 Steam Deck 验收。

v0.6.0 已于 2026-09-18T15:21:34Z 在 [GitHub Release](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.0)
公开发布，来源为 annotated tag 的 commit
`4b4b6c617172d43c7a73477881263e6e542d6cdf`。[Release CI 35359206991](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35359206991)
的审计、Windows Release 和 Linux Release 均通过。公开 Release 包含六个附件：Windows ZIP、Linux
AppImage、独立 shader pack ZIP 及各自 SHA-256 校验文件；三个主体包的 hash 分别为
`7ebcad6c2660ea6bcce801df3e6eb0f9bb6faef87e5108898618c1144031d161`、
`46c1c10e9dbaf0dc5db490aa3fd2f5b195eea8ccdbd7e35a0996f978292d3901` 和
`387a23b9328b8136847d48b37b574fddd600a526eb837807fbb08b758c6de4d9`。此前首次 CI 的
shader 输入失败仍保留在该历史运行记录中；当前发布资产已完成 sidecar 与 GitHub digest 核对。

2026-09-18 的真实光盘验证使用当前 importer 源码和只读目录
`G:/ROMS/US`：`ScanContent` 找到 USA/Europe 四张光盘镜像，每张 15 个文件，
`packages=0`、`rejected=0`。随后将 Disc 1 导入隔离目录，74.44 秒完成，未报告
错误或警告；目标目录包含 15 个资源文件和 `import-info.json`，共
5,712,711,997 字节，记录的 Disc 1 元数据和 XEX SHA-256 一致，暂存目录与导入锁
均已清理。这覆盖真实扫描和一次 Disc 1 事务，不代表四盘完整安装、交互 UI 验收、
游戏运行或全部输出字节比较。

The 2026-09-18 real-disc validation used the current importer sources and the
read-only `G:/ROMS/US` tree. `ScanContent` found all four USA/Europe disc
images, with 15 files per disc, `packages=0` and `rejected=0`. An isolated Disc
1 import completed in 74.44 seconds with no error or warning; its destination
contained 15 resource files and `import-info.json` totalling 5,712,711,997
bytes, with matching Disc 1 metadata and XEX SHA-256. Staging and lock files
were cleaned up. This covers real scanning and one Disc 1 transaction; it does
not establish a four-disc install, interactive UI acceptance, gameplay or a
full output byte comparison.

The first release attempt recorded in [CI 35355375239](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35355375239)
failed at portable shader-pack fetching because its private pinned input had no
shader files, and the earlier fallback pack then failed runtime-contract verification. That historical Draft state is superseded by the published release and
the successful [CI 35359206991](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35359206991).
The standalone pack reuses the v0.5.20 28,482-record pack contents under the v0.6.0
asset name. This documents release provenance; it does not expand runtime validation.

## Published v0.5.20 — 2026-09-17

The published release contains host EDRAM unsigned format clamping (Issue #38), f2358 TAA jitter compensation, the relocatable portable Vulkan shader pack (`.lospv`) distribution architecture, shader/pipeline preparation worker scaling, complete removal of the PowerPC prebuilt synchronization mechanism in favor of direct online compilation from source, and the integrated in-game debug overlay and cross-platform settings rasterizer from the menu branch. It was published at [GitHub Release v0.5.20](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.20) on 2026-09-17T20:09:32Z. Source version is `0.5.20`.
- **Host EDRAM unsigned format clamping (Issue #38)**: In `LostOdysseyRecomp/gpu/renderer.cpp` and `LostOdysseyRecomp/gpu/shader/xenos_translator.cpp`, correct host EDRAM clamping for unsigned formats (formats 0, 1, 2, 3, 10, 12, including 7e3 `COLOR_2_10_10_10_FLOAT`) with a strict `0.0` lower bound, resolving inverted/black light fixtures in Numara Castle (Philosopher's Chamber). Bumped shader cache `Version` from 22 to 23 in `LostOdysseyRecomp/gpu/shader/cache.h` to invalidate stale DXIL binaries.
- **TAA jitter compensation in f2358**: In `PositionVPSlot` (`LostOdysseyRecomp/gpu/temporal_scene.h`), register missing static scene and lighting vertex shaders (`0x69e9adcf2e1b6887`, `0x6a8c2c78737dc94c`, `0xa20d6099a44e2cd5` to Slot 7 and `0x6761469677f921c6` to Slot 8), eliminating inter-frame phase jitter artifacts on stairs and the save point light sphere in scene `f2358`.
- **Portable Vulkan shader pack (`.lospv`)**: Strips HLSL sources, diagnostics, and failure records, serializing only verified SPIR-V bytecode and essential `TranslatedShader` metadata. Deduplicates SPIR-V bytecode via SHA-256 and applies chunked Zstandard block compression (~1 MiB blocks). Decoupled from host paths and host DXC DLL hashes (28,482 shaders in 169.9 MB). Enables 1.2s zero-compile startup on Linux/WSL2 with lazy GPU module creation.
- **Shader prebuild scaling & skip**: Dynamically scales concurrent DXC and pipeline workers based on host RAM and CPU threads. Interactive skip support (ESC/Space/B) and `skip_shader_prebuild` setting in `settings.ini`.
- **PowerPC prebuilt sync removal**: Completely removed the PowerPC prebuilt synchronization mechanism (`LO_PREBUILT_PPC_DIR`, `ppc_sync.py`, `ppc_prebuilt.py`). Both Windows and Linux builds now compile `LostOdysseyRecompLib` PowerPC recompilation from source directly during CI builds, simplifying the build pipeline and eliminating static library caching across version bumps, paving the way for future ARM64 support.
- **Release shader pack bundling**: The portable Vulkan shader pack (`shaders/portable_vk.lospv`) is automatically downloaded via `tools/release/fetch_shader_pack.py` and bundled into both Windows portable ZIP and Linux AppImage release packages, eliminating first-run shader compilation for end users. A standalone `LostOdysseyRecomp-shader-pack-vk12-v0.5.20.zip` is also published as a release asset.
- **Integrated menu overlay**: Full cross-platform software rasterized settings menu and in-game debug overlay with complete keyboard and gamepad navigation.
- **WSL Linux build workflow**: Added `tools/build_linux.sh` and `tools/build_wsl.bat` for fast on-demand incremental builds.

Focused verification and bounded evidence:
- All release assets published and verified:
  - `LostOdysseyRecomp-windows-x64-v0.5.20.zip` + `.sha256`
  - `LostOdysseyRecomp-linux-x64-v0.5.20.AppImage` + `.sha256`
  - `LostOdysseyRecomp-shader-pack-vk12-v0.5.20.zip` + `.sha256`
- Unit and integration fixtures `LoPortableShaderPackTest.exe` (56 checks), `LoPortableShaderPackIntegrationTest.exe` (24 checks), `LoTemporalJitterTest.exe` (2,319,037 checks), `LoMenuRenderTest.exe`, `LoHidTest`, `LoHostUiCompositeTest`, and `LoDebugOverlayTest` built and passed.
- Converted full Windows startup bundle (28,482 shaders) into `shaders/portable_vk.lospv` (169.9 MB, verified by `LoShaderPackTool verify`).
- Linux ELF executed in WSL2 Manjaro with Mesa Dozen pointing to Windows game directory (`/mnt/d/Mihoyo/LostOdysseyRecomp-windows-x64`), hitting `portable shader pack hit: 28482 records, 27726 unique binaries` and achieving 1.2s zero-compile startup with 0 DXC calls.
- Packaged `LostOdysseyRecomp-shader-pack-vk12-v0.5.20.zip` (167.95 MB) with verified SHA-256 manifest.
- Fast WSL incremental build and deployment verified using `tools/build_wsl.bat`.

Validation limits and open boundaries:
- Verification covers Vulkan backend on Windows and WSL2 Linux with Mesa Dozen; native Direct3D 12 startup bundle remains separate.
- Packaged `.lospv` contains 28,482 shaders discovered from the tested game version; unencountered shaders continue to use local on-demand compilation.
- Linux AppImage is verified to launch into game selection / setup under WSL2; native Linux ICD, Steam Deck hardware, and full-game playthrough remain open.

## Host EDRAM format clamping and f2358 TAA jitter fixes — historical development checkpoint — 2026-09-17

The source fixes host EDRAM unsigned format clamping (Issue #38) and registers missing static scene and lighting vertex shaders for TAA jitter compensation in scene `f2358` on the `menu` branch (commit `7484518`; these fixes are now included in published release v0.5.20).
- **Host EDRAM unsigned format clamping (Issue #38)**: In `LostOdysseyRecomp/gpu/renderer.cpp` and `LostOdysseyRecomp/gpu/shader/xenos_translator.cpp`, correct host EDRAM clamping for unsigned formats (formats 0, 1, 2, 3, 10, 12, including 7e3 `COLOR_2_10_10_10_FLOAT`). Clamping lower bound is now strictly `0.0` for unsigned targets, preventing additive blending passes from accumulating negative light and tone-mapping `log2` from triggering NaNs / black voids in Numara Castle (Philosopher's Chamber). Bumped shader cache `Version` from 22 to 23 in `LostOdysseyRecomp/gpu/shader/cache.h` to invalidate stale DXIL binaries.
- **TAA jitter compensation in f2358**: In `PositionVPSlot` (`LostOdysseyRecomp/gpu/temporal_scene.h`), register missing static scene and lighting vertex shaders (`0x69e9adcf2e1b6887`, `0x6a8c2c78737dc94c`, `0xa20d6099a44e2cd5` to Slot 7 and `0x6761469677f921c6` to Slot 8), eliminating inter-frame camera jitter phase mismatch artifacts on stairs and the save point light sphere in scene `f2358`.

Focused verification and bounded evidence:
- Unit and regression fixtures `LoTemporalJitterTest.exe` (2,319,037 checks) and `LoMenuRenderTest.exe` built and passed.
- Render comparisons generated and verified: black diamond voids eliminated; lantern structure, lighting and bloom restored cleanly; TAA inter-frame jitter phase matched.
- Committed as `7484518` on `menu` branch and pushed to `origin/menu` (Gitea) and `github/menu`. `CHANGELOG.md` has been updated in both English and Chinese.

Validation limits and open boundaries:
- Verification is bounded to the reported scenes (Numara Castle Philosopher's Chamber and f2358 stairs/save point); does not constitute a full-game playthrough or player visual acceptance across all scenes.
- Clamping enforces `0.0` lower bound for unsigned host EDRAM formats; non-EDRAM or other shader arithmetic edge cases remain subject to future scene discoveries.
- Development code was committed on the `menu` branch; these fixes are now included in published release v0.5.20.

## Published v0.5.14 — 2026-09-16

The published release contains the embedded installer/updater work, accepted audit fixes, Linux XDG/AppImage support, and the R3 notified-wait CPU modernization. It was published at [GitHub Release v0.5.14](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.14) on 2026-09-16T07:18:53Z from source/tag commit `caf8060d99e5f1e52aec9d545331efe59e0c01e8`. Release CI `35065717899` succeeded; Linux Release job `104695450320` succeeded on attempt 1. The Windows ZIP is 32,915,456 bytes with SHA-256 `c21224ed985ada3502e25b42dd9e9379cb95749b0f06cea6d838f4d60843c09d`; the original Linux AppImage is 43,162,104 bytes with SHA-256 `a9912d2a258f17a2fea1a4d7f99c9589b538e66efd25225b1ade74af606e6196`.

Validation is bounded to the previously recorded R3 checks and the focused installer, updater, importer, AppImage script, and WSL path fixtures. The Windows package verification covered all 47 manifest files; Linux format and sidecar checks passed, and all four public assets matched their recorded bytes and hashes after redirect downloads. A user launch of the original Linux AppImage exposed `execv` `ENOENT`; the corrected packaging script passes 5/5 regression checks, and a repacked extract-and-run reached the expected no-game-files exit in WSL. The corrected Linux asset and checksum are published and anonymously verified (43,162,104 bytes, SHA-256 `0991df9aca8e930fa8eacbd8afe99b7a3a81e940dfa740fcfbdba34cd54c4d0a`); no real gameplay or live update run has been performed.

Packaging correction: the original AppImage lacked `AppRun` because linuxdeploy did not discover a desktop entry placed at the AppDir root. The packaging script now deploys metadata explicitly in two stages and checks the internal executable before generating the image. The original runtime and library content hashes were unchanged in the repacked extract.

The dated development checkpoints below retain their original pre-release status and validation boundaries. Their installer/updater, Linux packaging and CPU wait changes are now included in v0.5.14.

## R3 CPU waiting-path modernization and native Vulkan test — historical development checkpoint — 2026-09-15

The source implements the R3 CPU waiting-path modernization on the local `deck` branch (uncommitted development checkpoint, source version remains `0.5.13`, not a release).
- **Condition-variable kernel waits**: Adds `LostOdysseyRecomp/notified_wait.h` with predicate/deadline condition-variable helpers (`notified_wait::For` and `notified_wait::Until`). Replaces 200 µs polling sleep loops in `kernel/imports.cpp` for finite-timeout Event, Semaphore, and Mutant waits with condition-variable predicate and deadline waits while preserving consume, recursive ownership, and timeout semantics.
- **GPU command processor notification**: `gpu/command_processor.{h,cpp}` notifies on write-pointer updates (`SetWritePointer`) and shutdown (`Shutdown`), replacing the arbitrary 200-iteration yield loop in `WorkerMain` with a bounded 500 µs `notified_wait::For` wait while preserving SDL event pumping (`video::PumpEvents()`).
- **Direct fixture**: Adds `LoNotifiedWaitTest` (`tools/tests/notified_wait_test.cpp`) to `LostOdysseyRecomp/CMakeLists.txt` covering pre-notification, early wake, deadline timeout, and CommandProcessor write-pointer wake behavior.

Focused verification and bounded evidence:
- Windows unit fixture `LoNotifiedWaitTest` passed all cases: pre-notify, early wake, deadline, and CP-pointer wake.
- Existing regression fixture `LoPollWaitTest` passed without regression.
- Full Windows target compiled cleanly; diff check passed.
- Native Linux build and codegen passed after applying maintained dependency patches.
- Actual 15 W native Vulkan run on AMD Radeon 8060S:
  - Created and resized 1280x720 swapchain, selected Vulkan backend, started guest runtime.
  - Power limits configured to STAPM 15 W / Fast 25 W / Slow 20 W.
  - Startup shader preparation at allowed 60 W completed 28,484 known shaders (28,482 ready, 2 deterministic failures), then runtime consumed the startup bundle.
  - Ran 90 seconds with zero errors or fatal diagnostics.
  - Frame rates: target was 60 FPS; stable earlier windows observed around 58.46–58.58 FPS; later heavier scene observed around 37–40 FPS.

Validation limits and open boundaries:
- Do NOT claim locked 60 FPS (heavier scene drops to 37–40 FPS at 15 W).
- Bounded 90-second run only; no full-game playthrough, cutscene progression, or long-term stability validation.
- No player visual acceptance has been performed.
- Development code is uncommitted on the local `deck` branch; no push, PR, CI run, or GitHub Release exists for this checkpoint.

## Unified main binary installer and updater - unpublished development - 2026-09-15

The source unifies the content importer and updater into the single `LostOdysseyRecomp.exe` runtime binary. Separate `InstallGame.exe` and `LostOdysseyUpdater.exe` helper executables are excluded from the release payload; the legacy updater target remains available for fixtures. The Python/Tk `tools/installer` sources, their unittest fixtures and `test-importer.yml` have been removed. Release CI reads the Disc 1 XEX SHA-256 from `LostOdysseyRecomp/install/import_game.cpp`. Source version remains `0.5.13` (development executable, not a release).

Key changes across the checkpoint (committed through `fffa572` and the follow-up commits below):
- **Installer controller & UI**: Mixed disc and DLC discovery and import (`ScanContent`/`InstallContent`), transactional staging, cancellable background scanning and import via worker thread event queue, game path preserved for DLC-only runs, and partial-disc preservation on DLC error. Error reading now uses destination `InstallResult` object. Escape/B during scan requests cancellation and immediately returns to the source browser, with late finished scans intentionally not auto-transitioning. Review source and destination paths are converted via UTF-8 and width-clipped; DLC display names are width-clipped (168 px) to avoid overwriting the Files column. UI visual updates include Lost Odyssey game-style brushed steel panels, graphic folder and file icons, vertically centered selection bar, and precise UTF-8 width-measured text truncation with CJK glyph rendering. Review screen action button navigation is constrained to Left/Right only (Up/Down navigation preserves current selection). Source, destination, and review screen footers replace the text PAD label with a cached 32x32 transparent dark-cross D-pad bitmap icon with pale triangular arrows. Left stick controller navigation maps SDL `LEFTX`/`LEFTY` axis motion to directional input using strongest-axis resolution with deadzone 16000 and held-threshold 10000, initial 350 ms delay and 120 ms repeat rate, window focus and worker-busy input guards, and controller device hotplug handling.
- **Embedded updater**: Integrated into main executable startup before XEX inspection, installer dispatch and game initialization. Normal startup still allocates guest memory in a global constructor; only private apply mode skips that allocation. Reads lightweight preferences (`automatic_updates` and `ui_language`) without altering edition game language. Parses English and Chinese release notes from GitHub Release API payload with consent dialog before downloading. Updates run from a private copy of the main binary in `--apply-plan` mode, executing the update and auto-launching the updated binary upon completion when configured. Silent malformed `--apply-plan` invocations exit with code 1 without showing UI.
- **Packaging payload**: `package_release.py` updated to package only `LostOdysseyRecomp.exe`, DXC runtime libraries, licenses, and manifests (3/3 payload tests pass). Upstream font notices (`FONT-PROVENANCE.md` and `Unifont-OFL-1.1.txt`) are included.
- **Python installer retired**: Removed `tools/installer`, its unittest fixtures and `test-importer.yml`. `fetch_build_input.py` reads the Asia Disc 1 XEX SHA-256 from `LostOdysseyRecomp/install/import_game.cpp`.
- **Cross-platform importer fixes**: Win32 `HANDLE`/`GetCurrentProcessId` lock and PID dependencies in importer replaced with POSIX `open`/`flock` and `getpid` fallbacks. Synthetic importer tests pass on Linux WSL g++ C++20 (`LoImportGameTest`).

Focused verification and bounded evidence:
- Configured Windows main build and installer controller tests passed (`LoInstallerControllerTest`), with 14 explicit navigation and axis mapper unit checks passing.
- Lead personally verified real keyboard navigation: Right arrow highlights Change destination, Up/Down leaves selection unchanged, Right clamps at Change source, Left clamps at Start import, and Right + Enter from Start import opens the custom destination picker (`out/navigation-destination.png`).
- Visual captures recorded under repository `out/`: `navigation-review-right.png`, `navigation-review-vertical.png`, `navigation-review-end.png`, `navigation-review-start.png`, `navigation-destination.png`, and `navigation-source.png`.
- An independent visual Oracle opened 5 navigation captures and returned a bounded PASS on icon rendering and button highlight state. Prior source Oracle review bounded PASS noted missing relative screenshot paths as documentation artifact rather than code defect.
- Synthetic importer transactions, cancellation, mixed-edition, and DLC tests pass (`LoImportGameTest`). Prior tests were reused, not rerun.
- Real local sources verified: 4 USA/Europe ISOs (`G:/ROMS/US`), 4 Asia GOD discs + 3 DLC packages (`G:/ROMS/X360CH176`), and 4 extracted Asia disc folders (`D:/Mihoyo/LostOdysseyRecomp-windows-x64/game`). Original source files remained untouched; no copy or import was executed during source checks.
- Lead personally drove real keyboard navigation in the source browser to `G:/ROMS/X360CH176`, scanned 4 Asia GOD discs and 3 DLC packages, observed the fixed review layout at `out/installer-god-review-fixed.png`, and opened the custom destination picker at `out/installer-destination.png`. Input automation initially failed due to missing key scan codes and was corrected, confirming no application keyboard defect.
- Updater startup preferences, changelog extraction, apply plan, and restart tests pass (5/5 in `LoUpdaterStartupOrderTest`).
- Production main runner startup/restart test passed (`LoUpdaterTest --startup-restart` using parent/ready handshake, staged dummy replacement, and verified marker `updated process started`).
- Interactive UI check: Lead observed `out/installer_capture.png` showing Chinese folder name readable, selected row vertically centered, folder icons visible, and direction-key navigation functional (`.update` -> `CMakeFiles`).

Validation limits and open boundaries:
- Neither real analog stick controller hardware nor full interactive UI visual acceptance is claimed; verification remains bounded to keyboard navigation, mapper unit checks, and captured frames.
- Extracted DLC detection/import is verified for the supplied three packages: `LoImportGameTest --extracted-dlc` imported all three into an isolated temporary destination, compared every payload and sidecar byte, verified unchanged duplicates, rejected modified/missing payloads, traversal and overlapping destinations, and verified mid-copy cancellation cleanup. The original directories were read-only. Main build passed; lead's live scan of `D:/Mihoyo/LostOdysseyRecomp-windows-x64/game` displayed four Asia discs and three Ready DLC entries (`out/extracted-dlc-review.png`). This is not an interactive full-import or gameplay acceptance claim. The manifest's source SHA-256 is retained provenance, not proof of re-authenticating the original STFS archive.
- Real network update downloads and in-place installed-game binary replacement have NOT been tested against live GitHub releases.
- No full live import run has been executed inside the interactive UI.
- No full-game gameplay testing was conducted with the unified binary.
- Game visual fidelity and actual controller hardware acceptance are NOT established.
- Residual UI visual state and aesthetic fidelity across all screens remain unverified.
- Linux POSIX updater and source packaging support are implemented below; macOS remains untested, and no prebuilt Linux package is published.
- POSIX lock fix resolves non-Windows compilation in importer only; it does NOT claim the game engine runs or is compatible across all platforms.

## Linux installer/importer/updater and packaging support — unpublished development — 2026-09-15

The source adds Linux installer, importer, updater, and packaging support in an unpublished development checkpoint (source version remains `0.5.13`, not a release).
- **SDL installer and missing-disc handling**: Linux uses the existing SDL `ShowInstallerUI` and built-in `file_browser` without requiring a desktop document portal. Startup with missing `default.xex` or `--install` invokes `RunHost`.
- **Writable user paths & Flatpak isolation**: Added `os/user_paths.h` supporting XDG directories (`XDG_CONFIG_HOME`, `XDG_DATA_HOME`, `XDG_STATE_HOME`) when running in non-portable mode. Detects read-only install directories (`!IsExecutableDirWritable`), suppresses `chdir` into read-only executable paths on Linux, and maps Flatpak `DataDir` to `/var/data`. Game path discovery (`settings::game_path`) and `game-path.txt` read/write use `ConfigDir`/`DataDir` when `!UsePortableLayout()`.
- **POSIX updater & AppImage self-update**: Integrated POSIX SHA256 (`import_crypto`), libcurl HTTP transport (`posix_http.cpp`), and POSIX startup check (`posix_startup.cpp`) targeting GitHub asset `LostOdysseyRecomp-linux-x64-<tag>.AppImage`. On update acceptance, executes apply mode via Linux binary rename on `$APPIMAGE` and `execv`.
- **Flatpak update notification**: In Flatpak environments (`FLATPAK_ID` or `/.flatpak-info`), updater returns `StartupStatus::ExternalUpdateAvailable`, never writes to `/app`, and advises `flatpak update io.github.freefrank.LostOdysseyRecomp`.
- **Linux SDL UI & process handoff**: Adds SDL confirmation and download progress UI (`posix_ui.cpp`, `progress_posix.inl`), and launches updater helper in `main.cpp` using `posix_spawn` with `--apply-plan` and `--wait-process`.
- **Linux packaging specifications**: Added desktop file, 256x256 icon, AppStream metainfo, and Flatpak manifest (`packaging/linux/io.github.freefrank.LostOdysseyRecomp.json`, targeting `org.freedesktop.Platform 24.08` with `filesystem=host`). Added `tools/package_appimage.py` generating an AppDir layout with `linuxdeploy`. CMake configures UNIX install rules with `$ORIGIN` RPATH, linking libcurl on UNIX only.
- **Release CI Linux job**: `.github/workflows/release.yml` adds `release-linux` on `ubuntu-24.04` compiling PPC from source (`LO_PREBUILT_PPC_DIR` empty, as the Windows prebuilt `.lib` is `clang-cl /MT` only) and packaging the AppImage. The existing Windows ZIP job is unchanged.
- **Retained platform boundaries**: The first-run HWND setup wizard remains a Win32 dialog with a stubbed `SaveConfig` on Linux; the F1 in-game debug menu remains a Win32 stub. No Flathub submission has been made, and native Steam Deck sniper runtime build is not packaged.

Focused verification and bounded evidence:
- Unit test fixtures pass: `LoGamePathTest` PASS, `LoUserPathsTest` PASS, `LoUpdaterPosixSha256Test` compiled cleanly.
- `LoUpdaterSdlUiTest` fixture coverage verified by worker.
- AppImage packaging script verified locally with `package_appimage.py --dry-layout` on Windows.
- No hosted Linux CI execution has run yet for the new `release-linux` job.

Validation limits and open boundaries:
- Bounded to unit tests, dry layout, and local component checks; no live Linux GitHub Release download or in-place update has been executed.
- No Flathub submission or package publication has occurred; Flatpak manifest is a source-build specification only.
- Steam Deck sniper runtime native packaging is not implemented.
- First-run settings GUI and F1 debug menu remain Win32-specific.
- This checkpoint is unpublished development; no release exists for it.

## Installer/updater audit fixes — unpublished development — 2026-09-16

The follow-up audit fixes the main binary's installer and updater failure paths while source version remains `0.5.13`. STFS DLC imports now require successful open/write/flush/close results for the payload and all three sidecars before publication. Installer scans clear stale selections and reject failed or empty results; a retry clears the prior cancellation state. Windows apply mode recognizes `--apply-plan` as an independent argument. Linux resolves `ProfileDir` under the XDG data directory, retains the `LO_PROFILE_DIR` override, and handles profile-directory creation failure without throwing. AppImage packaging excludes `libwayland*` during linuxdeploy dependency deployment before AppImage output generation; extracted DLC ancestor traversal now tolerates trailing separators and supports cancellation.

The user accepted this development batch for commit. Repeated focused verification passed with exit 0: `LoInstallerControllerTest`, `LoUpdaterApplyArgumentsTest` (8 checks), `LoImportGameTest --dlc-io`, and the AppImage script checks (3/3). The Windows `LoUserPathsTest` cannot cover Linux behavior; the separate WSL Manjaro fixture already passed portable, XDG, changed-CWD, `LO_PROFILE_DIR` override and Flatpak paths. The Windows main target incremental build completed successfully; existing deprecated compiler warnings remain.

Non-blocking follow-up remains: add a root guard for the `ExistingDlcPayloadMatches` ancestor walk (the only current caller generates `dest/dlc/<hexID>`, so trailing-slash reachability is unconfirmed), and add explicit close-result coverage for extracted DLC. Disc-resource and `import-info.json` finalization coverage is recorded in the later importer hardening checkpoint below. No repeated-import hang is established.

Acceptance is limited to this development batch and its focused checks. No real AppImage package, live network or in-place update, complete interactive import or full-game playthrough has been performed, and no release has been published.

## Importer hardening and destination folders — unpublished development — 2026-09-18

The importer now treats final file close results as part of the publication transaction for disc resources and `import-info.json`. A write, flush or close failure aborts staging before publication, preventing a damaged resource from being reported as a completed import without adding a full-file reread. XDVDFS signature scanning advances in 2048-byte steps, and installation reuses the identity-verified reader while retaining the final identity recheck.

The destination browser can create a folder from its button, `F2`, or destination-page controller `Y`. It provides a unique default name, supports keyboard renaming, enters and selects the new folder after creation, and does not start an import automatically. Collision, permission and read-only-directory errors are surfaced. The source browser's existing `Y` behavior is unchanged.

Focused validation passed: `LoImportGameTest` covered resource and JSON open/write/flush/close failure injection, staging abort, rollback and retry; synthetic ISO locator cases covered standard, padded Chinese-path with an unaligned decoy, and chunk-boundary inputs; folder helper cases passed; and `installer_ui.cpp` passed the WSL SDL2 syntax check. These are synthetic and compile checks. No real interactive game import or runtime installer click-through has been performed, and this checkpoint is unpublished.

## Published v0.5.13 — Alt+Enter window/fullscreen toggle — 2026-09-14

The source change adds an **Alt+Enter** presentation toggle between **Windowed** and **Borderless**. It does not select DXGI exclusive fullscreen, and `DXGI_MWA_NO_ALT_ENTER` remains set. The chord accepts SYSKEY scancode-only `RETURN`, `windowID=0`, left Alt, right Alt and AltGr (`KMOD_RALT|KMOD_CTRL` or `KMOD_MODE`). On Win32, `GetAsyncKeyState(VK_MENU)` is combined with left/right Alt handling because both Alt keys report key code 18 and right Alt may arrive as Ctrl without `KMOD_ALT`. `FitBorderless` is best-effort and cannot roll a successful toggle back to Windowed; Shift/GUI rejection, placement and debounce remain unchanged.

The implementation is in `LostOdysseyRecomp/gpu/window_mode.h` and `LostOdysseyRecomp/gpu/video.cpp`, with corresponding fixture coverage in `tools/tests/game_window_pixels_test.cpp`. The focused `LoGameWindowPixelsTest --rendering-fixes-only` run passed after the SYSKEY, `windowID=0`, right-Alt and AltGr/`VK_MENU=18` fixture updates.

Live local acceptance on 2026-09-14 confirmed left Alt+Enter and right Alt+Enter in the actual game window. The accepted run used a local RelWithDebInfo build of the same chord code, Vulkan on an RTX 5080, `window_mode=0` Windowed, a 3840×2160-class display, and `LO_NO_UPDATE=1 --game disc1`. This is local shortcut acceptance, not acceptance of the published package.

Published at [GitHub Release v0.5.13](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.13) on 2026-09-14T23:36:59Z. Release CI [34908617463](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34908617463) succeeded after the synchronized PPC identity was retargeted; source/tag commit `545af9be26ebafed364f68c1b2725c10b5b03206` and annotated tag `v0.5.13` are published. CI used the prebuilt PPC artifact from `LostOdysseyRecomp-build-inputs`, not `rebuild_ppc`; cache key `bdf6dce2af1112a1dd0e1ed161bd66a2d7959ed1f174ad049a684b74a4367fc7` came from private commit `d4feb17917189a5b9f051b48b783f5fc62081658`. Only the root `CMakeLists.txt` hash changed versus v0.5.12; the PPC contract and library chunks were unchanged.

The Windows ZIP `LostOdysseyRecomp-windows-x64-v0.5.13.zip` is 44,304,038 bytes with SHA-256 `a993071c24f7324a3ae3a0dbe24688afc60450d3da9f1a78fbf532c69f65bacd`; the inner runtime is 83,536,896 bytes with SHA-256 `d144762040db8918e34282f429044833266fdd3f5b9c48bf221c27752fbc6e58`; the inner updater is 846,336 bytes with SHA-256 `5e5c7957947969431ae499a3acc18536dc5940aaef0598fd7ba2faa2f6c76b96`. The ZIP hash matches its sidecar, and both public ZIP and `.sha256` downloads returned HTTP 302 to GitHub release assets; followed downloads returned HTTP 200 and matched the recorded hash and size.

The published package was not launched for gameplay validation. Whole-game, DXGI exclusive-fullscreen, mixed-DPI and mouse validation remain unverified; publication checks do not claim 50-file manifest re-verification or packaged gameplay acceptance.

## Published v0.5.12 — 2026-09-14

Published at [GitHub Release v0.5.12](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.12) on 2026-09-14T21:34:13Z. Release CI [34895591364](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34895591364) succeeded for source/tag commit `36e574b64e0cbb11ddcdc7f68cbc36214b5c084f` after PPC cache synchronization. CI used the prebuilt PPC artifact from `LostOdysseyRecomp-build-inputs`, not `rebuild_ppc`. The ZIP is 44,304,445 bytes with SHA-256 `7cc99618cee509bdea000b736772344de60279f4a439a4f876cc7d49d4c8e60a`; the runtime is 83,536,384 bytes with SHA-256 `d394c173cfa8ecc6ea57c1b9671a1a574ae02d2cd4bb76008392caca66686974`; the updater is 846,336 bytes with SHA-256 `36787c2314d9d554010193a4f020eb8d1a848292920e40af3bb7fb44f1a3893e`. The ZIP hash matches its sidecar, and both public ZIP and `.sha256` downloads returned HTTP 302 to GitHub release assets. The PPC cache key was `cb4a75c2d3fa6e3d5f92e3431e7006f77fd7927dd97f05d27eb488ea4cf87e44`, from private commit `5df5b6efb38537386f6c9522ae9126d800032559`.

The release contains the USA/Europe FMV and event subtitle language-table mapping fix. The local `game_language=5` USA/Europe Spanish opening-FMV check was accepted on 2026-09-14 using Vulkan at 3840×2160 and a startup-bundle hit. On 2026-09-15 the original reporter confirmed Spanish opening FMV subtitles on v0.5.12 (Vulkan and Direct3D 12), and DE/FR/IT FMV coverage was confirmed. Issue [#27](https://github.com/freefrank/LostOdysseyRecomp/issues/27) is closed. Complete event coverage and whole-game validation remain unverified. No 50-file manifest re-verification is claimed.

## Published v0.5.11 — 2026-09-14

Published at [GitHub Release v0.5.11](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.11) on 2026-09-14T02:09:50Z. Release CI [34797755460](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34797755460) succeeded for source/tag commit `624729cdb1263b96061b1fa14d4d1c5ba0b50239`. The ZIP is 44,304,683 bytes with SHA-256 `5de068c4e77c82feb0bbe7cfcf1dacbca3d44aa94bde064f7f59f5ad6944e132`; the runtime is `33a460410b7f397187a12ac6984718c1716997936127f94f5dbcb2ef40315283`; the updater is 846,336 bytes with SHA-256 `dd38ab687b7ab0713e3ce6b29bce058bfd4b3a7ffebe8d266335532ba6f21750` and matches the ZIP. All 50 payload hashes/CRCs, clean provenance, four public asset downloads and PPC consumption passed. Existing diagnostics and bounded logging checks were reused; no new gameplay acceptance is claimed.

CI consumed PPC key `921d26c12c98de289e659f30f490e09b23d8e2ca08624690e38f1b3dd9294f51` from private commit `ad34fdc295f374c4787c13343be4af7ae3facafc`, matching the synchronized receipt and reusing the existing library. Evidence: `out/v0.5.11/release/{release-source.json,ci-run.json,ci-ppc-consumption.json,delivery-verification.json,public-download-check.json,published-release.json}`.

## Europe FMV subtitle mapping — included in published v0.5.12 — 2026-09-14

The host-side `82481BE8` PPC hook now returns the original executable language-table pointer for host `GameLanguage()` IDs 1–9 when `r3=0x8336A5F0` and `r4` is 0 or the current ID. The guest table at `0x832455F0` maps those IDs to INT/JPN/DEU/FRA/SPA/ITA/KOR/CHI/SCH. This extends the earlier SCH-only alias repair: callers that pre-filter with `r4=0` no longer receive the ID-0 English suffix when selecting USA/Europe event and FMV subtitle packages.

A local USA/Europe Disc 1 test with `game_language=5`, Vulkan at 3840×2160 and a startup-bundle hit was accepted by the user for the opening FMV subtitles on 2026-09-14. On 2026-09-15 the original reporter confirmed Spanish opening FMV subtitles on v0.5.12 (Vulkan and Direct3D 12), and DE/FR/IT FMV coverage was confirmed. Issue [#27](https://github.com/freefrank/LostOdysseyRecomp/issues/27) is closed. Complete event coverage and whole-game validation remain unverified. No new fixture was added. The fix is included in published v0.5.12; publication checks did not re-verify the 50-file manifest.

## Published v0.5.10 — 2026-09-13

Published at [GitHub Release v0.5.10](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.10) on 2026-09-13T21:20:10Z. Release CI [34783107248](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34783107248) passed for source/tag commit `db63ebaf50fed9612ca66a498f162ad6be69a54e`. The clean ZIP is 44,288,884 bytes with SHA-256 `e1b6b9a84bcf0360f104db2e001ca5e740552834b554a4bf5812dd9c8f52f6eb`; runtime SHA-256 is `25c3c83366143b5f74943ee0cd88789cca0042b3d4a06a7ef986fa8d8de94995`. All 50 manifest payload hashes and ZIP CRCs passed; all four public assets matched anonymous HTTP downloads, sizes, SHA-256 and API digests. CI consumed PPC key `481e10e3e18a083fbc8f3207422a2065c5ba548c39b2588336b02f3da75effdf` from private commit `eb883038c92fdfc0e154b5e3f8b61e346a98034e`. Existing functional checks were reused; no new gameplay or frame-time acceptance is claimed.

The release adds 11 captured Vulkan TAA paths accepted for the reported flicker scenes, plus original CPX/declaration coverage for startup shader preparation. Startup frame-time benefit and whole-game coverage remain unverified. Evidence: `out/release-v0.5.10/`.

## Published v0.5.9 — 2026-09-13

The v0.5.9 package adds conservative Vulkan depth-clear coalescing for 720 compatible EDRAM tile rectangles, preserving D3D12 mapping output, holes and uncleared regions. It also includes texture-key avalanche mixing, captured-shader identity caching, graphics descriptor same-handle suppression, same-framebuffer Plume rebind suppression and per-`GpuSlot` immutable descriptor reuse; the two-slot/fence contract remains unchanged. TAA behavior is retained from v0.5.8.

The matched 45-second static-view comparison used copied `user01` state, 3840x2160 internal rendering, Vulkan, AA3, a 60 FPS cap and 60 W AMD AI MAX+395 power. Mean FPS improved 7.49638→43.47614, with GPU time 132.91221→21.06364 ms. This remains bounded pre-release candidate evidence, not a clean release-build benchmark and not 4K60, 1080p60 at 15 W, whole-game validation or player acceptance. The binding-cache fixture passed the retained duplicate, replacement, incompatible-prefix and post-fence cases; the masked-load register-SIMD experiment was reverted after slower constants and no FPS benefit.

Release CI `34778434518` succeeded for source/tag commit `d26ee8b021784d7232b5319d816227867f98050d` and published the package at 2026-09-13 19:50:00 UTC. The clean package is 44,269,995 bytes with SHA-256 `fd71bf65f92b242f81a350b5b6e97ea7f1991107a2c30e91ece2dd261bac0591`; its runtime hash is `ef93c01db40433fb6d463357ea3e6979b3a1f45cf0d2d3f81be2fcf3f8885faa`. All 50 manifest payload hashes and CRCs passed, four public assets matched anonymous HTTP/size/hash/API digest checks, and CI consumed PPC key `ab194913725bd44df7ea9e248d4e60c561ac4d73f7b87d4c5bb080613bc5567a` from private commit `6433064e547a9460249b1162b938ac0b2c332688` without PPC recompilation. The retained measured candidate predates the release source commit; evidence and detailed boundaries are recorded in the [Vulkan depth-clear performance note](notes/vulkan-depth-clear-performance-2026-09-13.md). Player acceptance and 1080p60 at 15 W remain pending.

## Published v0.5.8 — 2026-09-13

The v0.5.8 source change set includes the seven current-scene TAA `PositionVPSlot` mappings, the bounded sampled-content SIMD comparison, the `LO_QUERY_TRACE` presence cache and the synchronized private PPC cache `main` state. Release CI `34764115203` passed for source/tag commit `6e6f11cf56ef69082f5be5b049e5d48d58154415`; the package was published at GitHub on 2026-09-13. The TAA and CPU evidence is reused from the repository-relative records under `out/`; no source-0.5.7 gameplay result is presented as v0.5.8 package validation.

The reused checks include the 33,927-case Clang 19.1.5/22.1.8 fixtures, the original WPR diagnosis and limited fixed-scene 40 W comparisons (4K about 47 FPS; 1080p about 59.98 FPS). These do not establish stable whole-game performance, power benefit or original-scene TAA visual acceptance. ZIP verification covered all 50 manifest files, CRCs, version 0.5.8 and clean commit provenance.

Evidence: `out/v0.5.8/release-verification.{json,md}`, `out/cpu-query-runtime-20260913/REPORT.md`, `out/taa-live-20260913/` and the detailed [CPU diagnostic note](notes/cpu-live-profile-2026-09-13.md). Anonymous downloads of all four published assets matched bytes, hashes, sidecars and API digests. The PPC key `ec7f34708ff870d6ec940a7a4fe83686d4ec5802344934c9084b85e2cf113c3d` matched private commit `65a6869ea7ec36987f1ca7026c0588d4fa6c40d7`; no new v0.5.8 gameplay or player visual acceptance is claimed.

## Published v0.5.6 — 2026-09-13

Source version **0.5.6** is published at [GitHub](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.6), with Release CI [34726533463](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34726533463) succeeding for source commit `7124f4b3912df715167acf01f469974045cc3e08` and publication at `2026-09-13T00:05:47Z`. The Windows ZIP is 44,255,182 bytes with SHA-256 `ad6616480fa8905936b3b36d202deb2dad356f07670a2e0f1570984016e897d9`; the standalone updater is 849,920 bytes with SHA-256 `d3356d3fcac410e3ee86c012dc4971ffa4ee507b76f28eebf79e2c575a7eaf74` and is byte-identical to the copy extracted from the ZIP. All 50 manifest files passed hash and CRC checks with clean source-version provenance. The four public assets passed anonymous HTTP 200 and hash/size verification. The release includes the updater manifest transaction fix, Issue #16 particle-material compatibility fallback, PPC prebuilt CI path and the city renderer work; bounded gameplay validation remains separate.

The published package runtime SHA-256 is `1fff598e1a0872da2a7728ceb9921aa2e4a1bff0bd818c224827b84eaf3f5aaa`; CI package verification did not repeat gameplay. The separate local main executable `d50c240d24bcd6cda7a1abc23107fa97f11d18dc5a68167310da6e6f892fe0ed` supplies the retained D3D12/local Asia Disc 3 target-scene validation. Whole-game, Vulkan, other-region and player acceptance remain unverified.

Evidence: `out/v0.5.6/release/{published-release.json,public-download-check.json,delivery-verification.json,ci-run.json,ci-ppc-consumption.json,ppc-ci-upload.json,release-source.json}`.

## Published v0.5.4 — 2026-09-11

Source version **0.5.4** is published at [GitHub](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.4), with Release CI [34550200618](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34550200618) succeeding for commit `2ad94d418bb0478417ab9589109f1f685ed92eb3`. The ZIP is 44,237,061 bytes with SHA-256 `104ced8b60c16cd1b9013543a3940c9ed8d7cf904c3d05a6a8ef8d591f51d218`; the standalone updater is 848,896 bytes with SHA-256 `7285d0f24331387973a44e4240353d7857f1967163440fb49ba0546c7b9dd844` and is byte-identical to the copy extracted from the ZIP. All 50 manifest files passed hash, CRC, version 0.5.4 and clean-build provenance checks. The runtime executable hash is `3c3b4073f1b7abbcce38747dc08763335ac019edf560df849177176d0399f949`.

All four public assets matched the local verification artifacts and returned anonymous HTTP 200 responses; at that historical checkpoint, the GitHub latest-release API reported v0.5.4. Evidence: `out/v0.5.4/release/{delivery-verification.json,public-download-check.json,published-release.json}`. The release includes the PPC generation guard, optional external assembly profiler, extended F1 archive wait and confirmed installer drag-dispatch fix. Focused validation and package integrity passed; no whole-game, visual or complete F1 acceptance is claimed.

## Published v0.5.3 — 2026-09-10

Source version **0.5.3** is published at [GitHub](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.3), with Release CI [34505504344](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34505504344) succeeding for commit `6fa9adc281c5693ac0af8a47fec815b20c29ff50`. The ZIP is 44,237,807 bytes with SHA-256 `53beb197b753fa26c5c436f37c4b03bb636857c7390171d3ce6353940a9d81d0`; the standalone updater is 848,896 bytes with SHA-256 `1ad8a0b6e605f050376d59050bf94d598bbd9ec2965610df30cd5e6083fcc5a2`. The 50-file manifest, hashes, CRCs, clean-build provenance and version checks passed; the runtime executable hash is `9dbcef81412c683d4fd76d5a13c16663c3893b4e0944804f8aaf6e1b7fd1f2c0`.

The release adds compact opt-in TAA diagnostics, schema 3 binding evidence, the updater ZIP-root staging fix and private feedback archiving. TAA remains diagnostic research: no new player visual acceptance, complete update transaction or flicker fix is claimed. Evidence is retained under `out/v0.5.3/release/`.

The v0.5.3 entry is retained as historical release provenance. Its asset digests and validation boundaries remain unchanged.

## Published v0.5.2 — 2026-09-10

Source version **0.5.2** is published at [GitHub](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.2), with Release CI `34446196620` succeeding for commit `08a0192713435892f3c1b772abc9ab31bca36359`. The ZIP is 44,209,471 bytes with SHA256 `5d20e569b74c75418cefc9fdd5537477ce4b0a9ca976ed9d64ec77771f78ce7c`; the standalone updater is 849,920 bytes with SHA256 `4aa5e491a2bff885c668cdc4473488fd05acf44e56eba2aa9d4c242a76a4db6b`. All 50 payload hashes and CRC checks passed. The release adds original VS/PS microcode to F1 captures, consent-gated incremental D1 uploads and bounded nonblocking collection.

The recorded runtime evidence remains tied to the retained source-0.5.0 development binary. Release CI built new 0.5.2 executables; existing functional validation was reused without another game run. Credential scans found no management credentials in the audited source or expanded release package. All four public assets returned HTTP 200 and matched the audited hashes in anonymous download verification.

The standalone updater retains the behavior validated for [v0.5.1](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.1). The separate updater download is byte-identical to the helper inside the 0.5.2 ZIP.

## Current validation and limits

### Startup and lower-layer failure diagnostics — published v0.5.11 — 2026-09-14

The v0.5.11 release adds diagnostic evidence for Issues [#6](https://github.com/freefrank/LostOdysseyRecomp/issues/6) and [#22](https://github.com/freefrank/LostOdysseyRecomp/issues/22). Plume D3D12/Vulkan failures route through the runtime logger with the original HRESULT, VkResult or Win32 code, API context and bounded repeated-failure reporting. Renderer initialization identifies the failed resource stage, byte count and slot. Startup records the actual Windows build through `RtlGetVersion`, process/native architecture, source/build revision, compiler, a startup memory baseline and a detectable Wine version; the video device log records GPU, raw driver version, vendor/type and `reported_device_memory_bytes` (on Vulkan UMA this can come from a `DEVICE_LOCAL` heap, without implying that every device reports dedicated memory). PE timestamp and image-size fields remain image metadata; the executable hash is the artifact identity. Main-thread guest-address allocation failures retain a pre-logger `FailureInfo` with operation/API, raw error, parameters, timestamp/thread/process context and allocation-time memory status, then report it separately from the later memory snapshot. Terminal WinHTTP failures retain the API and raw Win32 code; GPU adapter/renderer failure formatting uses a fixed stack buffer and `EmergencyWrite` fallback. Existing snapshots, retention and crash `EmergencyWrite` behavior are unchanged.

The independent Release build linked `LostOdysseyRecomp.exe` and the four targets with clang-cl 22.1.8 at historical source 0.5.10 revision `d8cdf39ca637-dirty`; the source and documentation now target v0.5.11 without rebuilding this validation binary. `LoPlumeLogTest` passed 23 checks, the extended existing `LoMemoryFailureTest` passed 259, `LoUpdaterHttpFailureTest` passed 94, and `LoRenderResolutionGpuTest --diagnostic-log <path>` passed: a real D3D12 invalid-width 16385 texture reported `0x80070057` once with API, raw HRESULT and resource context, then a legal 1x1 resource succeeded. The environment banner and runtime snapshot also passed. The extended build provenance check passed 25 checks, including acceptance of patch-added files while verifying the complete expected tree. Evidence: `out/diagnostic-logging-20260914/REPORT.md`, `out/v0.5.11/release/provenance-after-fix.json`, its stdout/stderr/exit-code records and `gpu-runtime-snapshot.log`. Later isolated D3D12 and Vulkan processes using EXE SHA-256 `975f0b31dd831f916b417df0a90b634010179cfed340861d03582ebea5a1650f` also recorded all new startup fields in complete runtime logs; D3D12 reached guest swap 181, while Vulkan was closed after the log check before renderer initialization. These are logging-recording checks, not complete game/backend acceptance; no shader, draw, network or user-scene acceptance was performed. Issues #6 and #22 remain open with unknown root causes.

Published at [GitHub Release v0.5.11](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.11) on 2026-09-14T02:09:50Z. Release CI [34797755460](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34797755460) succeeded for source/tag commit `624729cdb1263b96061b1fa14d4d1c5ba0b50239`; package, updater, payload/CRC, public-download and PPC provenance verification passed. The v0.5.11 package includes this diagnostic instrumentation. No additional gameplay or reporter acceptance is claimed.

### Linux first-playable evaluation — historical planning checkpoint — 2026-09-13

The [Linux port evaluation](notes/linux-port-evaluation-2026-09-13.md) preserves the original first-playable direction and its then-current conclusion: Vulkan-only, unbundled ELF using host Mesa, SDL2 and X11/XWayland. Its statement that Linux had not been configured, compiled or run is historical to that checkpoint and must not be read as current status. The published Windows v0.5.11 release and its publication facts are unchanged.

### Linux first-playable — local implementation and WSL validation — 2026-09-14

The `linux` working tree now implements a Vulkan-only, unbundled Linux ELF path. The `linux-clang` CMake preset uses Ninja, clang/clang++ and RelWithDebInfo; Linux compiles generated PowerPC sources directly instead of consuming the Windows prebuilt library. Host path handling, case-folded resource resolution, Linux CRT mappings, fixed guest mapping validation, SDL Vulkan presentation, swapchain-format fallback and the Linux DXC `dlopen` path are included. An explicit `--game` candidate is isolated to the supplied install root, `disc1` or `default.xex`; when `--game` is used, launch CWD must already be the ELF directory because executable-directory chdir is skipped.

The resulting ELF at `out/build/linux-clang/LostOdysseyRecomp/LostOdysseyRecomp` was built with clang 22.1.8 and run in WSL2 Manjaro on 2026-09-14. The user watched the first-playable window for approximately ten minutes and closed it; the process exited 0. The run recognized the supplied `disc1`, loaded the XEX, created a 1280x720 Vulkan swapchain, prepared 28,484 known shaders (28,482 ready; two rejected during preparation), submitted frames and produced nonzero audio. Vulkan was provided by Mesa Dozen wrapping Microsoft Direct3D12 on an NVIDIA GeForce RTX 5080; Dozen is not conformant, so this is WSL Vulkan-on-D3D12 evidence rather than native Linux NVIDIA/Mesa validation. The log also confirms a nonempty SHA-256 identity for the adjacent `libdxcompiler.so` and the Vulkan backend reaching ready state.

This is accepted first-playable window/boot evidence for the tested WSL path, not whole-game validation, native-Linux-GPU acceptance, Steam Deck support, an AppImage/Flatpak/installer package, CI coverage or a Linux GitHub Release. Linux remains local and unreleased; source version remains 0.5.11. Evidence is retained in `out/build/linux-clang/LostOdysseyRecomp/logs/runtime-1789367206166882.log`.

### Card A city measurement — experiment-only on published v0.5.11 — 2026-09-14

**Implementation:** No runtime source was changed. This is an experiment-only measurement of the already-published v0.5.11 Windows package, using source version 0.5.11 at commit `624729cdb1263b96061b1fa14d4d1c5ba0b50239`. The ZIP SHA-256 is `5de068c4e77c82feb0bbe7cfcf1dacbca3d44aa94bde064f7f59f5ad6944e132`; the runtime SHA-256 is `33a460410b7f397187a12ac6984718c1716997936127f94f5dbcb2ef40315283`.

**Validation:** The bounded protocol used hidden 1280x720 D3D12, AA3, a 60 FPS cap and an isolated `user01` city route. It collected 1,836 city frames with `draws >= 800`; `over_budget` was 0%. `fence_wait_ms` mean/max was 0.0007/0.1371, with zero frames over 1 ms; nested flush remained zero; descriptor, upload and arena splits were all zero at D3D12 1800. `gpu_batches` mean/max was 1.0005/3, `gpu_queue` mean/max was 0.8115/1.5575 with zero samples over 16.67 ms, and `draw_ms` mean/max was 3.4395/6.3522. The dated evidence is [Card A city measurement](notes/cpu-card-a-city-2026-09-14.md).

**Acceptance and interpretation:** `exhausted_resource_class=null`. Do not add GPU slots, raise the 1800/2048 limit or add rings; under guide §9.2, add-slot and raise-limit work should remain low priority. The next measurement is Card B's current profile; Card B was not measured here and `LO_VERTEX_TIMING` was not set. This is not 4K, Vulkan, a 3C6T-restricted run or whole-game validation, and it is not player acceptance. The player's `settings.ini` was not mutated. Historical source-0.5.4 `fence_wait` 15.40 ms and two-slot 1.67 ms figures remain historical context and must not replace this baseline. A later same-package run with `LO_VERTEX_TIMING=1` is recorded in the [Card B city measurement](notes/cpu-card-b-city-2026-09-14.md).

**Publication:** This record does not change the published v0.5.11 release, source version or runtime package, and there is no new GitHub Release. No user acceptance of a CPU performance result has been given.

### Card B city measurement — experiment-only on published v0.5.11 — 2026-09-14

**Implementation:** No runtime source was changed. This is an experiment-only remeasurement of the already-published v0.5.11 Windows package with `LO_VERTEX_TIMING=1`, using source version 0.5.11 at commit `624729cdb1263b96061b1fa14d4d1c5ba0b50239`. The ZIP SHA-256 is `5de068c4e77c82feb0bbe7cfcf1dacbca3d44aa94bde064f7f59f5ad6944e132`; the runtime SHA-256 is `33a460410b7f397187a12ac6984718c1716997936127f94f5dbcb2ef40315283`; the executable is 83,536,384 bytes.

**Validation:** The bounded protocol used hidden 1280x720 D3D12, AA3, a 60 FPS cap and isolated `user01`; Down was never sent. `original_saves_changed=false` and `player_saves_changed=false`, and the player's `settings.ini` was untouched. The run produced 1,846 city frames and 891 menu frames with `over_budget=0%`; `last_swap=1920` and `last_draws=893`. The runtime log is `%TEMP%\\lo-city-logs\\runtime-639250002081609096.log`. The vertex histogram emitted 3,622 timing lines, including 1,846 city vertex frames.

City vertex-stage means in milliseconds were `find=0.0693`, `match=0.1535`, `erase=0.0098`, `capture=0.0422`, `copy=0.0091` and `insert=0.0777`, for `stage_sum=0.3616`; p95 was 0.4297 and maximum 0.6323. `match_share=0.424`, `vertex_ms=0.476`, `shader_lookup=0.051`, `pipeline_lookup=0.03` and `draw_ms=3.82`. Vertex-cache means were 1,014 calls and 289 uploads; mean bytes were 168,594 with a maximum of 932,120, `cache_after` maxed at 65,536, buckets were 131,072 and summed rehashes were zero. Evictions averaged 175.7 and summed to 324,386; this is cache-cap turnover, not texture hash-chain activity. Evidence is retained in the [Card B city measurement](notes/cpu-card-b-city-2026-09-14.md); the ignored persisted result is `out/perf-ring/card-b-results.json`.

**Acceptance and interpretation:** `exhausted_resource_class=null`. This experiment provides no basis for B1 (`implement=false`): it did not emit a memcmp-length histogram, `LO_VERTEX_TIMING` is wall time rather than a length histogram, and `EqualSampleBlock64` already covers 64-byte comparisons. It provides no basis for B2 (`implement=false`): no texture-chain snapshot was collected, and lookup is not hot after the avalanche change; do not add another mix. It provides no basis for B3 (`implement=false`): no new guest spin was measured; retain 32 polls followed by yield and `GpuPoll` at 50 microseconds. Do not expand SIMD, hash mixing or poll-wait work from this run. A later same-log Card C prepare gate is recorded separately. This is not 4K, Vulkan, a 3C6T-restricted run or whole-game validation, and it is not player acceptance. Historical source-0.5.8 memcmp 7% render-self and hash-chain 84 figures must not replace this baseline.

**Publication:** This record does not change the published v0.5.11 release, source version or runtime package, and there is no new GitHub Release. No player or CPU-performance acceptance has been given.

### Card C prepare gate — experiment-only on published v0.5.11 — 2026-09-14

**Implementation:** No runtime source was changed and no new city drive was launched. This is an experiment-only prepare gate on the already-published v0.5.11 Windows package, using source version 0.5.11 at commit `624729cdb1263b96061b1fa14d4d1c5ba0b50239`. The ZIP SHA-256 is `5de068c4e77c82feb0bbe7cfcf1dacbca3d44aa94bde064f7f59f5ad6944e132`; the runtime SHA-256 is `33a460410b7f397187a12ac6984718c1716997936127f94f5dbcb2ef40315283`. Evidence reuses the Card B city log `%TEMP%\\lo-city-logs\\runtime-639250002081609096.log` plus a static inventory of `PrepareKnownShaders`, `PrepareKnownPipelines` and XMA `WorkerMain`.

**Validation:** Startup shader prepare already ran on that process: metadata snapshot 158 ms; bundle 2248 ms including 56 ms device-module creation; 28,547 records / 28,545 ready / 2 cached failures; 0 source reads, translations or DXC attempts. Pipeline preparation used 222 recipes, 4 workers and 21 ms. Per-frame city leftovers were `copy_ms` 0.0091, `shader_lookup_ms` 0.051, `pipeline_lookup_ms` 0.03, vertex `stage_sum` 0.3616 and `draw_ms` 3.82. `LO_TRACE_XMA` was unset. Dated evidence is [Card C prepare gate](notes/cpu-card-c-prepare-gate-2026-09-14.md); the ignored persist is `out/perf-ring/card-c-results.json`.

**Acceptance and interpretation:** `exhausted_resource_class=null`. `implement=false` for a new Parallel Prepare / Serial Commit path: startup shader/pipeline prepare already exists and already parallelizes; per-frame leftovers are below queue/copy overhead; a new per-frame task lacks snapshot, output ownership, join, stale cancel, serial fallback, independent task size and input-stability evidence. Do not add a third prepare pool, and do not parallelize guest scene traversal, descriptor mutation or `FSceneRenderer::Render`. The later Card D affinity experiment is recorded separately and remains default off. This is not 4K, Vulkan, a 3C6T-restricted run or whole-game validation, and it is not player acceptance.

**Publication:** This record does not change the published v0.5.11 release, source version or runtime package, and there is no new GitHub Release. No player or CPU-performance acceptance has been given.

### Card D 3C6T city measurement — experiment-only on published v0.5.11 — 2026-09-14

**Implementation:** No runtime source was changed. This is an experiment-only process-level affinity measurement of the already-published v0.5.11 Windows package on a 9800X3D + RTX 5080 host, using source version 0.5.11 at commit `624729cdb1263b96061b1fa14d4d1c5ba0b50239`. The ZIP SHA-256 is `5de068c4e77c82feb0bbe7cfcf1dacbca3d44aa94bde064f7f59f5ad6944e132`; the runtime SHA-256 is `33a460410b7f397187a12ac6984718c1716997936127f94f5dbcb2ef40315283`. The observed process affinity was `0x3F`, limiting the process to logical CPUs 0-5.

**Validation:** The bounded protocol used hidden 1280x720 D3D12, AA3, a 60 FPS cap and isolated `user01` city route. It collected 1,370 city frames; `over_budget` was 0%. `fence_wait_ms` mean/max was 0.0008/0.1336, with zero frames over 1 ms; nested flush remained zero; splits were zero. `gpu_queue` mean/max was 0.8162/1.8092 with zero samples over 16.67 ms, and `draw_ms` mean/p95/max was 3.805/4.556/13.65 with zero samples over 16.67 ms. Compared with the unconstrained Card A run, fence/queue remained in the same class while `draw_ms` increased from 3.44 to 3.81. Cold boot still missed the startup bundle; shader preparation requested 15 workers because `hardware_concurrency=16` ignores process affinity, and the bundle was published in 215.360 seconds. The player's `settings.ini` hash stayed `f41265ad611d2241cc5522f18ea1908156045856b43856730797e62849d9d8a1`, `original_saves_changed=false`, and `player_saves_changed=false`. Evidence is retained in the [Card D 3C6T city measurement](notes/cpu-card-d-3c6t-city-2026-09-14.md) and runtime log `%TEMP%\lo-city-logs\runtime-639250355668305659.log`.

**Acceptance and interpretation:** `exhausted_resource_class=null`. `implement=false` for default host pinning and for writing placement into `KeSetAffinityThread`: the constrained run did not expose a new exhausted CPU/GPU resource class, did not improve fence/queue behavior versus Card A, and slightly increased draw time. The UnleashedRecomp `KeSetAffinityThread` stub matches this project's no-op behavior and is not a pinning recipe. This is not Steam Deck, not 15 W, not a 4C8T measurement, not 1080p60 at 15 W, not Vulkan, not whole-game validation and not player CPU-performance acceptance.

**Publication:** This record does not change the published v0.5.11 release, source version or runtime package, and there is no new GitHub Release. Further development is deferred; no player or CPU-performance acceptance has been given.

### Current-scene TAA jitter coverage — local candidate — 2026-09-13

The local candidate extends the current-scene TAA jitter repair by seven vertex-shader paths: `8d3c80b318235b22` to c4, and `3eb16ad927f44289`, `83b23507725f85bf`, `6742ec1abe49589e`, `0f2b89c7eb1c409e`, `fecf2f9d9bef2702` and `2a7867b5eed37f8a` to c7. These paths were selected from the latest three-frame capture (frames 13429–13431): 39 draws per frame, 117 draws total. The strict audit matched two paths; the position fetch 95 versus auxiliary fetch 94 distinction recovered the other five. All seven original vertex-shader microcode hashes match the historical `f24842` family. The current PS `67b10` was not present in these frames and was not moved into the compensation policy. The capture recorded 2,229 submitted draws per frame with no reported drops; actual CPU-uploaded VS/PS constant banks and per-draw enabled/applied records were not instrumented.

The candidate CPU validation covered seven real current-scene world/VP draws at 1920×1080, frame 13429 and jitter phase 22, with three synthetic vertices per path. The maximum physical sample error was `0.000277985496` pixels and the canonical error was zero. An independent canonical oracle, Z/W and non-VP constants, and retained PS constant banks were checked; this does not verify PS execution behavior. Historical all-phase, resolution and guard fixtures were reused. This is implementation and bounded CPU evidence only: no game run, GPU replay or player visual acceptance is claimed. The Release build completed with clang 22.1.8 in 151.525 seconds and exit code 0; actual link provenance succeeded. PPC was reused under the verified equivalent input/compiler/Release contract without recompilation. Candidate delivery is `LostOdysseyRecomp-taa-scene-20260913.exe`, 83,437,568 bytes, SHA-256 `f47a894ec51f50b099f97f53c08717019eb02330aae943f8cdc33e1568d459b3`, source 0.5.7 with link identity `e6031efe0fe1da1effdcbb26670eb7463e6b20bb6a140c265a53b2bd82da5535`. It was copied into the existing installation without replacing the primary executable (`81705475dafb360c8c30a30acef14f1e365afc0368cd5aaf858889d54f675ac4`) and was not launched. The independent shader-discovery scan is complete; its conservative candidates and exclusions are recorded below and are not a bug count.

### Historical TAA path discovery — static coverage, no runtime mapping — 2026-09-13

The completed offline inventory covered 2,893 cached VS programs and 20,077 cached PS programs. Of the VS corpus, 47 reused an existing microcode SHA/proof and 2,846 were translated with the current production translator and analyzed by `position_evidence`; process/file failures were zero. Translation notes were present for 611 new programs, so generated HLSL does not establish complete microcode semantics. No DXC, GPU operation, cache writeback or production mapping was performed.

The conservative shortlist contains 81 static families: 76 cache-only candidates and five historical main-camera associations, all without translator notes. Eight historical main-camera associations remain separately recorded; three are held by conservative constant-predicate taint and require manual reuse of earlier derivations. The two unmapped matrices observed in the current capture were excluded as non-main-camera paths. Thirteen of the initial secondary-projection-bank families were downgraded; secondary projection candidates are not automatically TAA paths. The eight historical associations and 81-family shortlist are coverage leads, not bug counts. The runtime candidate still contains only the seven reviewed `PositionVPSlot` mappings. Evidence: `out/taa-live-20260913/discovery/DISCOVERY.md`, `out/taa-live-20260913/discovery/discovery.json` and `out/taa-live-20260913/discovery/SHORTLIST.json`.

The bug-fix implementation and its recorded local validation are committed as `2019cd017ab939d0b728cec340f7835c2082e615` and are included in the pushed `main` alongside its existing city-performance work. The 0.5.7 release is published; hosted CI and package verification passed.

### Published v0.5.7 — 2026-09-13

The v0.5.7 package is published at [GitHub](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.7), with Release CI [34743383193](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34743383193) succeeding for source commit `954d0e17dbe63e49189873db3949ebf5df2462aa` and publication at `2026-09-13T06:51:32Z`. The Windows ZIP is 44,262,080 bytes with SHA-256 `618a96aef78ac79f566d78bed48159e65b6275f1d02f741ed66eeb1c2856a5a7`; the standalone updater is 845,824 bytes with SHA-256 `2bc0bcceefaea3b23731ba939b04d3dd1762b9c3b80c75c85cdc74bb6187d6da`. All 50 manifest files passed hash and CRC checks with clean 0.5.7 source provenance; four public assets passed anonymous HTTP 200 and hash/size verification. Evidence: `out/release-v0.5.7/{public-downloads.json,published-release.json,ci-result.json,assets/verification.json,ppc/ci-consumption.json}`.

The release combines updater recovery/launch-consent work, the guarded shadow-loop and resolve-copy fixes, the HDR16 TAA bloom prefilter, and the material vertex-shader jitter repair. The user accepted the HDR-off/materials-on result for the reported lighting-flicker scene; other scenes and hardware remain unverified. Offline shader, cache and resolve checks are recorded separately; no new functional test, build launch or gameplay run is claimed here.

The matching PPC cache key `3260d975104d2cdf612e5858a7712a23f7264a8c4ebb857adf962b8c16c842eb` was consumed by Release CI from private commit `4894650b407255e0783026e4b73816f7640ec00f`. PPC inputs, 250 generated files, 471 headers and the compile contract match; the original four shards and library `ba3e4c4dff009d6d8e844c007186a6e5040266875bca6423f8fe26f8d27fb21b` were retained without compilation or relinking. Evidence: `out/release-v0.5.7/ppc/ci-consumption.json`.

### Standalone updater recovery and completion policy — v0.5.6-hotfix1 development record (included in v0.5.7; never released separately)

The `v0.5.6-hotfix1` development change, included in v0.5.7, removes local package-provenance and executable-hash gates from the standalone recovery path. It prefers a valid `source-version.txt`, falls back to a valid version-only `manifest.json`, and uses `0.0.0` when metadata is missing or unusable. An updater-only empty folder, or a missing game executable, therefore requests the latest release; stale or malformed metadata no longer blocks the check, although a selected valid version can still be up to date. `PrepareAtStartup` compares the selected version with the latest release without requiring an installed manifest/version match. Download SHA-256 verification, safe archive extraction, transaction rollback and update path-safety checks remain in force.

After a successful update, the local helper asks whether to launch the game, with **No** as the default. Silent mode does not launch the game; failed updates do not auto-restart it; and a requested launch failure leaves the installed update in place. The handoff runner uses the locally installed updater so this completion behavior remains active when the downloaded package contains an older helper.

The recorded checks and local helper for this historical development record were built before the suffix-only version metadata change, as source version 0.5.6: `LoUpdaterStandaloneTest` passed 44/44 (`out/updater-simple/standalone.log`), alongside the companion updater version/asset/integrity/staging/rollback/helper/preservation log and `LoUpdaterHelperContextTest` for a Unicode caller working directory and no unsolicited launch. The standalone count covers recovery and handoff behavior; transaction rollback remains covered by the companion updater fixture. These are hidden synthetic-process checks only. No real game or visible Yes/No dialog interaction was performed.

### GPU shadow-loop and resolve-copy fixes — included in published v0.5.7

The bounded GPU fixes are implemented in [`4715b60`](https://github.com/freefrank/LostOdysseyRecomp/commit/4715b60) and merged with `github/main` commit `b39c2c9` in [`1d139c9`](https://github.com/freefrank/LostOdysseyRecomp/commit/1d139c9). The translator exits only for the proven structured LoopEnd cases, and the renderer reuses identical adjacent color-resolve copies within one command batch while invalidating reuse across Draw, clear, transfer, allocation, submission and external access. Shader cache version 22 rejects older binaries and startup bundles. These changes are included in published v0.5.7.

Recorded validation includes 11 LoopEnd guard cases and 12 D3D12 cases across 64 mixed-lane inputs, nine related guest shaders compiling to both DXIL and SPIR-V with eight conservative optimizations and one original path, 99 cache checks, `LoResolveCopyPolicyTest` 30/30, and existing Vulkan/D3D12 resolve GPU checks covering three allocations and two passes per backend with zero mismatched pixels and correct copy/skipped/resolve behavior. The production PS SPIR-V matches the previously measured probe, so the single-frame ABBA and byte-identical PNG evidence is reused; the prior offline ABBA means were shadow 7.711424 → 1.244464 ms and total timed events 14.026832 → 7.1088 ms. These results do not establish hardware power, whole-frame gain, gameplay hit coverage, cross-scene behavior or player acceptance.

The built diagnostic executable is `D:/Mihoyo/LostOdysseyRecomp-windows-x64/LostOdysseyRecomp-gpu-perf.exe`, SHA-256 `313CDD34712154A88FEEAF9C25D8AB1409705D327D8B0FD15252A52562C1C211`, 83,422,720 bytes. It came from a dirty source-0.5.6-hotfix1 working tree with local extras and is not evidence for the clean published binary. A complete merged-binary game run remains pending. Delivery evidence: `out/gpu-profile-20260912/production-01/delivery.json`.

### TAA bloom prefilter and geometry diagnostics — included in published v0.5.7

The current candidate adds a guarded HDR16 area prefilter for the identified TAA bloom input, linear sampling after that filter, and an explicit `LoBloomPrefilterTest` target. D3D12 and Vulkan fixture runs passed the HDR negative-value, alpha, area-weight and subpixel checks. Captured inputs from frames 10170–10172 matched an independent 3x3 area-average reference within half-precision output error. The linear-only fixture also passed on both backends, covering 65 phase responses and the recorded red-step bound. These checks validate filter behavior and do not establish a live visual fix.

At the earlier bloom-only checkpoint, the user's same-scene result was partial: the upper large robot was stable while the lower enemy eyes still flickered. That candidate and its lower-eye diagnosis remain historical; the later material-jitter repair is accepted for the reproduced whole-lighting scene and is recorded in the published v0.5.7 section above. The follow-up diagnostic retained the AA3 condition while removing the transient `temporalJitter` gate and added process-scoped AA, jitter, history and bloom controls plus optional geometry/resolve tracing. An eight-frame trace showed history reuse without gaps, and the frame-7208 capture found four compared draw pairs with identical VP, world, active-bone, vertex-buffer and index data, excluding a CPU upload mismatch for those pairs. Cross-backend comparison covered four 84-index pairs; the later report includes additional 144-index eyes outside that capture, so complete scene coverage is not established. Detailed evidence is in [the TAA bloom prefilter note](notes/taa-bloom-prefilter.md); prior fixture results are reused.

### Local main 0.5.6 gameplay validation and PPC provenance

The merged local `main` at source version 0.5.6 built successfully with the normal CMake Release configuration in 173.782 seconds. The runtime executable SHA-256 is `d50c240d24bcd6cda7a1abc23107fa97f11d18dc5a68167310da6e6f892fe0ed`; the updater SHA-256 is `732681bf2a1c74106bb1ea90b32912b3269304dea3abe8ba351f6e1d5b94cc3e`. The build used no diagnostic object overlay or battle bridge. Evidence: `out/main-bugfix-0.5.6/REPORT.md`, `artifacts.json` and `build-result.json`.

The producer PPC key is `50b8ad415be405b302252558e0fd960913c3ce6a15d991ae3607142f1a3821a5`, uploaded at private commit `6a6ed03152431a232165e35b19b7f94f09bbbda9`. The CI-compatible key is `d89197759478260d7e135b654993d57cff30127f1d41cf30c410fd0ae4be27f4`, uploaded at private commit `a6cd91ea35261dd202b78e93b4acb65973369d07` for runner representation. Both use the byte-identical verified library SHA-256 `ba3e4c4dff009d6d8e844c007186a6e5040266875bca6423f8fe26f8d27fb21b`; no PPC recompilation occurred. The 19 key differences are fully explained by five line-ending differences and fourteen symlink placeholder/target-content representations; 250 generated outputs, 471 PPC headers, CMake fingerprints and the Release compile contract are identical. CI run `34726533463` completed Resolve, Record, Retrieve, Restore, Release build and packaging successfully. The resulting draft was published after package verification. `lo.ppcAutoSync=false` remains unchanged. Evidence: `out/main-bugfix-0.5.6/ppc-bundle/REPORT.md`, `upload-plan.md`, `out/v0.5.6/release/ppc-ci-upload.json` and `out/v0.5.6/release/ppc-ci-compatible-plan/compatibility-provenance.json`.

The main-binary target-scene replay passed on D3D12/local Asia Disc 3 using the new 0.5.6 executable (SHA-256 `d50c240d24bcd6cda7a1abc23107fa97f11d18dc5a68167310da6e6f892fe0ed`). It completed the unsuppressed freeze sequence, subsequent map229/menu progression and visible movement. This remains bounded scene validation, not whole-game, Vulkan, other-region or player acceptance; do not substitute the retained source-0.5.4 gameplay evidence for validation of this executable.

### Issue #14–#16 triage — 2026-09-12

GitHub Issues [#14](https://github.com/freefrank/LostOdysseyRecomp/issues/14), [#15](https://github.com/freefrank/LostOdysseyRecomp/issues/15) and [#16](https://github.com/freefrank/LostOdysseyRecomp/issues/16) were read as **OPEN** on 2026-09-12. Their tracker state remains separate from implementation, validation and reporter acceptance.

Live tracker reconciliation checked 2026-09-18T07:33:59Z: Issues [#14](https://github.com/freefrank/LostOdysseyRecomp/issues/14), [#15](https://github.com/freefrank/LostOdysseyRecomp/issues/15) and [#16](https://github.com/freefrank/LostOdysseyRecomp/issues/16) are **CLOSED / Done**. Closure updates the Issue state; it does not expand the bounded Asia Disc 3 D3D12 validation above into Vulkan, other-region coverage or player acceptance.

- **#14:** The two attachments describe different historical signatures from source 0.5.0 and 0.4.2. The current source already contains the earlier word-selector guard and render-flush mitigation, but neither attachment proves the current root cause or a 0.5.4 reproduction. A clean-package cage-scene reproduction and save are still needed.
- **#15:** The log shows a successful 206,000-byte first-save write and a later full read, then continued rendering until the window closed; the reported first-save progression hang therefore remains unresolved. The updater staging defect is fixed for subsequent transactions using the new `StageArchive`: the transaction now applies `manifest.json` itself and rolls back on failure. The focused `LoUpdaterTest --manifest-transaction` run passed three scenarios with zero failures: successful update and post-apply rollback, failure after manifest replacement, and tamper rejection, including plan serialization round-trip. This does not automatically repair an already mixed installation or remove old resources, and is updater transaction coverage only; no helper/game launch, network update or player acceptance is claimed. `PrepareAtStartup` compares manifest and executable source versions only; `up-to-date` does not prove every payload is current. The logged `dxcompiler.dll` and `dxil.dll` identities match the retained official v0.5.4 manifest hashes, and `DxcIdentity` hashes the actually loaded module paths; this supports those two DLLs only. Mixed installation is not established as the cause of the reported hang. Evidence: `out/bug-fix-evidence/updater-manifest-build/REPORT.md`.
- **#16:** The published v0.5.4 executable reproduces the King Train freeze/crash on local Asia Disc 3 D3D12 with the supplied `user08` path. The zero-count cooked and runtime `FParticleVF8336CA10` tables for `gt9_0_map.cs__frzShader1` / raw `xf_shd_aniflz.freeze` explain the null shader read at guest `0x823DFE14`. The narrow `particle_material_compat` fallback preserves sprite parameter updates and original logic, then uses the engine default material only for raw blend 2 when the normal particle shader is absent; its caller guard is limited to the ordinary sprite builder. `LoParticleMaterialCompatTest` compiled and ran with zero failures, covering the material compatibility policy only. The final branch native build in `build/branch-native-r1` succeeded, and `final-reload-01` successfully reread the new slot 11/`user10` checkpoint at the saved position and produced field shot `9326`. The r2 candidate separately demonstrated map 229 movement and a normal menu, but predates the final caller guard and is not final-branch build evidence. Final-branch `final-freeze-01` then completed the target sequence with visible frozen King/guards/carriage frames, later train animation, normal map229/menu progression and visible movement/camera change. Asia Disc 3 D3D12 target-scene validation is complete; Vulkan, other-region coverage and player acceptance remain pending.

Detailed evidence and boundaries are recorded in [Issue #14–#16 triage](notes/issues14-16-triage.md). The fixes and validation records above are included in published v0.5.6.

### Issue #6 startup allocation — current diagnosis — 2026-09-12

GitHub Issue [#6](https://github.com/freefrank/LostOdysseyRecomp/issues/6) is **OPEN** as of 2026-09-12. Three source-0.5.4 logs from 2026-09-11 consistently fail during the preferred and fallback `VirtualAlloc2` reservations with error 6 (`ERROR_INVALID_HANDLE`), before `CreateFileMapping` or `MapViewOfFile3` is reached: [log 1](https://github.com/user-attachments/files/32122228/runtime-1789145203661201.log), [log 2](https://github.com/user-attachments/files/32124989/runtime-1789146389573701.log) and [log 3](https://github.com/user-attachments/files/32125052/runtime-1789146749477451.log). The second log records `available_physical=14880874496` and `available_commit=33202167808` bytes at the delayed error report; those values do not support a RAM-exhaustion conclusion.

The 2026-09-12 follow-up reports that a clean extraction still fails ([comment](https://github.com/freefrank/LostOdysseyRecomp/issues/6#issuecomment-5647622343)). The current [`guest_address_space.cpp`](../LostOdysseyRecomp/kernel/guest_address_space.cpp#L54) passes `nullptr` for the process handle in both `VirtualAlloc2` calls, which is permitted by Microsoft's API contract; this is not a confirmed source defect. No code change, new local validation, recovery acceptance or root-cause determination is recorded. The historical v0.4.2 closure and its request to reopen on recurrence remain provenance, not evidence that the current path is resolved. The maintainer has requested the Windows version/build, architecture and compatibility environment, plus a complete current clean-extraction log ([comment](https://github.com/freefrank/LostOdysseyRecomp/issues/6#issuecomment-5648995559)); these remain pending.

Issue #6 is **CLOSED / Done** in the live tracker as checked at 2026-09-18T07:33:59Z. The technical diagnosis remains unresolved: the current evidence does not establish whether the failure is caused by the OS, compatibility environment, API behavior or another condition, and does not establish shared causation with Issue #5. Closure does not add reporter acceptance or runtime recovery evidence.

The local source-0.5.10 instrumentation above implements the previously pending observability items: OS/build/architecture identity, allocation-time API/flags/process-handle/memory context before logging initializes, and original terminal WinHTTP error codes. This changes the available evidence only; it does not determine the cause of Issue #6 or #22 and does not change guest mapping, arena size, slot/wait behavior or network request policy.

Issue [#22](https://github.com/freefrank/LostOdysseyRecomp/issues/22) is also **CLOSED / Done** in the live tracker as checked at 2026-09-18T07:33:59Z. The diagnostic instrumentation and the remaining runtime limits above retain their recorded scope; closure does not claim a confirmed root cause or additional compatibility coverage.

### PPC auto-sync and key-resolved prebuilt — current main synchronization, local hook off

The current synchronization update publishes an ordinary fast-forward to the private
`main` branch, preserves unrelated archive files and retains existing `ppc/<key>` branches
as historical build-selection references. Concurrent advances receive bounded retries;
the update does not create new PPC branches. Private `main` currently includes the merged
cache at `9e387adc045fe3b8ba4e6d1956812d11050bc9ed`. Release CI checks out that branch with
the SSH deploy key, validates the required fingerprint and compile contract before
restoring PPC, and records the immutable private HEAD in the identity artifact; a mismatch
fails with synchronization or `rebuild_ppc` guidance. The implementation remains
unreleased; real GitHub Release CI has not yet proved this new workflow. `lo.ppcAutoSync`
remains unset locally, so this does not claim automatic enablement. `actionlint`, the
23-test PPC sync suite (including six bare-Git integrations), and two embedded CI identity
fixtures passed. Runtime/gameplay validation remains separate.

Historical branch-based behavior was pushed to github/main as [`2c0456c`](https://github.com/freefrank/LostOdysseyRecomp/commit/2c0456c). The opt-in configuration remains `git config --local lo.ppcAutoSync true`; CMake `LO_PPC_AUTO_SYNC` reads that setting. After a successful PPC library build, the post-build hook runs `ppc_sync.py sync --already-built`. It is not a file watcher. At that historical checkpoint, a matching input/compiler key reused the immutable private `ppc/<key>` branch; a changed key published a new branch with shards of at most 40 MiB. CI, imported libraries and `LO_PPC_SYNC_ACTIVE` never upload. The historical release workflow resolved the library by key from `ppc/<key>` instead of a pinned private SHA; `rebuild_ppc: true` still compiles from source.

At the historical checkpoint, nineteen synthetic sync cases in `tools/tests/test_ppc_sync.py` passed. Built-library roundtrip and change-during-build checks passed. The real `LoPpcAutoSync` hook uploaded an existing library only to private commit `5e80263491b39dc0012146dd3a31cf5eea533225` on branch `ppc/4d21302a4eef224c82691878fbcb6cd2f427b60d676b3e692e78598257b5d1b4`; a subsequent same-key sync was unchanged and produced no PPC C++ compile or game run. The recorded local setting was `lo.ppcAutoSync=false`. The new 0.5.6 key was subsequently uploaded to private commit `6a6ed03152431a232165e35b19b7f94f09bbbda9`; hosted CI consumed the compatible retrieved artifact successfully. Sparse-clone restore/check and a simulated-CI Release contract key match passed. Evidence: `out/ppc-auto-sync-evidence/build-sync.log`, `github-output.txt`, `github-output-second.txt` and `out/ppc-sync/receipt.json`. The earlier manual prebuilt path remains historical at commit [`2b5b1d1`](https://github.com/freefrank/LostOdysseyRecomp/commit/2b5b1d1) and CI [34553414428](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34553414428). See [release packaging](notes/release-packaging.md).

The historical branch-based workflow is included in published v0.5.6 and was absent from published v0.5.4. Hosted [PPC prebuilt tests](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34565564964) passed for `2c0456c`; user gameplay acceptance remains separate.

### PPC generation guard — v0.5.4

PPC source generation now runs through `python -B tools/ppc_codegen.py generate`. `tools/build_tools.bat` records the generator binary and source receipt; generation and `check` validate the TOML/recompiler inputs, generated output hashes and the absence of obsolete 64-bit jump-table switches. The wrapper preserves the prior output tree if generation fails. The configured runtime build exposes `LoPpcCodegenCheck` as an order dependency before guest objects. The seven-case synthetic guard passed, and a fresh generation followed by `python -B tools/ppc_codegen.py check` completed successfully, producing 247 C++ files with 843 low-word (`u32`) and zero `u64` switch sites; 246 `ppc_recomp` instruction-comment stream hashes are unchanged. Existing 3,258 instruction checks, 109 word-switch checks, 843 recognized tables and 44,523 selector evaluations are reused from the earlier semantic evidence; no new gameplay acceptance is claimed. Package integrity and release provenance are recorded in the published v0.5.4 section above.

### TAA crowd coverage follow-up — local, unpublished

The current local executable hash `56e9e8d53f798a9ada10726c0141b0746b4dc3d0fbd763ae159eb8ee153b5d2d` matches the retained source-0.5.4 crowd candidate. The implementation adds seven local VS coverage paths: `fe3efe042c311110` to c4, and `1474db97dfc0afad`, `97b5d441419b5533`, `6742ec1abe49589e`, `3eb16ad927f44289`, `0f2b89c7eb1c409e` and `fecf2f9d9bef2702` to c7. VS `99c2` and PS `67b10` now share `IsSceneDepthReconstructionPair` in `temporal_jitter.h` and `renderer.cpp`, so resolve, compensation and diagnostic parsing use the same pair classification. Existing guards and the c4 algorithm are retained.

The audit baseline contains 63 observations: 20 cross-pass groups and 43 PS reconstruction observations. The postfix policy covers the first four VS paths; the 43 strategy candidates disappear from that policy output, while 20 older upload differences remain diagnostic evidence rather than visual acceptance. The audit is retained under `out/tools/taa-audit-latest/AUDIT.md`.


### F1 capture archive timeout adjustment

The v0.5.4 source change increases the local F1 menu ZIP archive wait from 60 seconds to 180 seconds for large captures. Optimal compression and background behavior are unchanged. The retained local evidence records two separate approximately 2.5 GB captures timing out at 60 seconds; it does not record a game run or F1 acceptance. The published package identity and clean-build evidence are recorded above.

The earlier local source-0.5.3 executable and installation matched SHA-256 `CF70EA663ED230334145E1135CA97A58DFE3A34C65ED7D43E9E428C41B53270B` (`out/f1-zip-180s/install.json`; previous EXE/metadata in `out/f1-zip-180s/backup`). That build linked successfully but required copying the existing installation's DXC DLLs after the source-directory DLL copy failed. This remains historical local-build provenance, separate from the v0.5.4 CI package above.

### Installer drag dispatch — v0.5.4

The installer drag-dispatch re-entrancy path now posts `WM_NCLBUTTONDOWN` with signed screen coordinates instead of synchronously calling the window procedure. `DragDispatch` passed 1/1, and the reporter confirmed the real installer drag fix. The installer-only local package is retained at `out/installer-drag-fix/dist/InstallGame.exe` (11,888,743 bytes; SHA-256 `707CD7D2E9F4AB3BF33363E172FAAD5CFCFA6B1A53161FEE0E7F53735B7C7FA7`). No game or importer regression validation is included; the fix is included in published v0.5.4.

### Optional assembly profiler

The standalone `tools/asm-profiler` utility is implemented for Win64 external attachment. It samples live thread RIPs by briefly suspending each target thread, reading its context, and resuming it before allocation or I/O; after collection it uses DbgHelp for a post-capture module, symbol, source-line and 32-byte code snapshot. `report.py` uses Capstone 5.0.6 to produce offline HTML and JSON with instruction hotspots, function self-sample rankings, a per-thread OS CPU-time table (not allocated to RIP), `--tid` filtering, HTML filtering and optional matching generated PPC comment context via `--source-root`. The HTML has no external resources. Existing output is rejected unless `--overwrite` is supplied, and the target executable is protected from replacement.

The Python reporting fixture passed 6 tests (`tools/asm-profiler/test_report.py`), followed by the independent thread CPU-time table check (1/1), for 7 covered checks. Coverage includes disassembly, aggregation, thread filtering, unknown/empty samples, changed code snapshots, HTML escaping, guest-comment boundaries and the thread table. MSVC 19.44 Release native build completed without warnings; a synthetic 2-second/10-ms background run collected 187 samples with zero failures, all code bytes, successful PDB/source resolution and a 1.84375-second busy-thread CPU delta. That synthetic capture remains historical fixture evidence. Samples include sleeping/waiting threads and are wall-clock shares; there is no call stack. Matching PDBs and generated sources are required, and PPC comments are source-line context rather than verified guest PCs.

Two isolated 2026-09-11 gameplay captures sampled the published v0.5.4 runtime (SHA-256 `3c3b4073f1b7abbcce38747dc08763335ac019edf560df849177176d0399f949`) with `lo_asm_profiler.exe` (SHA-256 `d6326a19c51e97ee1338364a98f36a7f2cb29aeb537c82f6b100bc3104b9d0f1`). There was no matching PDB beside the EXE, so every EXE sample is unresolved. Both runs used isolated working copies, `LO_AUDIO_MUTE=1` and `LO_TEST_INPUT_FILE` only; original install save timestamps were unchanged. Session 1 (`out/asm-profiler/play-2026-09-11/`) loaded user00 and walked `xenon_scr.fpd` for 15.036 s (5,092 samples, 0 failed, 38 threads). Hottest OS CPU TID 52396 used 8.11 s (that thread: EXE 38.8% + `NtWaitForSingleObject` 36.6% + AMD/D3D12). All-thread wall share was dominated by ntdll waits; EXE was about 2.18%. Heartbeats showed about 36 fps, 1,700–2,300 draws/frame and a 1280×720 frontbuffer; sampling dipped to 15.8 fps (suspend perturbation). Session 2 (`out/asm-profiler/play-2026-09-11-city/`) loaded user01 (02:15 Lv.10) for a city walk on `xenon_scr.fpd` for 15.017 s (10,736 samples, 0 failed, 44 threads). Hottest TID 5092 used 7.27 s (EXE 35.2% + `NtWaitForSingleObject` 39.8% + amdxc64/D3D12). After sampling: about 43–44 fps, about 1,913 draws/frame, 1280×720. The bottleneck picture matches session 1; without a PDB there is no new function-level hotspot. Wall-clock all-thread suspend snapshots include waits. There are no stacks, ETW, cycles or GPU pass timing, and no CPU-utilization, instruction-latency, cache or branch counters. This is diagnostic evidence, not a performance fix, player acceptance or a new Release. Session detail: [assembly profiler gameplay captures](notes/asm-profiler-gameplay.md). Fixture and capture evidence remains under `out/asm-profiler/`.

### GPU command-list ring and descriptor reuse — included in published v0.5.6

Branch `perf-gpu-ring` implements a 2-slot D3D12 command-list ring and raises the D3D12 descriptor-set limit to 1800 with 2D texture-set reuse (`b91d279`). A follow-up binds unused 2D/3D/cube banks to static dummy sets, uses BatchCache last-hit, and skips unchanged constant uploads (`ed90fe9`). The work is included in published v0.5.6; the retained measurements are diagnostic and are not player acceptance, Vulkan coverage or a 60 FPS claim.

Header fixtures pass: LoRenderBatchPolicyTest 22/22 and LoTextureDescriptorCacheTest 12/12, including a 2000 last-hit loop. An isolated Continue-sequence city walk loaded user01 (Lv.10) at 1280×720 D3D12 TAA=3 cap 60 with `LO_BACKGROUND=1`, `LO_AUDIO_MUTE=1` and `s@120,a@240,a@360,a@480,a@700,a@900`. Screenshots confirm an Uhra street walk. Original install save timestamps were unchanged (user00 2026-09-10 18:22:37, user01 18:28:28).

Ring EXE SHA-256 `5917F389F9FD9E88FDEC6DBD3437ADE76D415F1653FB6924575ACCF478C1B9AD`. Stable city (1664 frames, swap 1367–3030): about 57.7 fps (49.1–60), draws 1816, batches 2.00, splits 0, draw_ms 9.82, fence_wait_ms 1.67, gpu_queue 3.91, bind_ms 1.99, descriptor hits/misses 5289/160. Published v0.5.4 city diagnostic: 31–44 fps, about 5.2 batches, fence_wait_ms 15.40, draw_ms 21.19. Dummy EXE SHA-256 `02E303F1462546FB98236446E24B2397DF762179923DE1D7C02852317ED37BC4`. Stable city (1639 frames): about 56.0 fps (28–60), bind_ms 1.58, hits/misses 837/156. Dummy is bind-path only and shows no fps win versus the ring run. The two EXEs are sequential diagnostic captures, not a laboratory A/B.

These city numbers are diagnostic. They are not 60 fps acceptance, player acceptance, Vulkan coverage or a new Release. Compare note: [GPU ring measured comparison](notes/perf-gpu-ring-compare.md).

### City CPU conversion follow-up — included in published v0.5.6

The measured vertex-cache diagnosis and bounded-cache follow-up are now the
current city CPU result. With `LO_VERTEX_TIMING=1`, frame 1858 isolated a
41.8241 ms `unordered_map` insertion during a 262,144-to-524,288 bucket rehash;
the frame's 495 endian copies took 0.0254 ms in total. `gpu/vertex_cache.h` now
reserves a 65,536-entry metadata cache and, when full, examines at most 16
rotating candidates for eviction. It does not change GPU arena bytes, offsets,
slots or waits, and does not add a flush. `LoVertexCacheTest` passed 3,569,548
checks once; existing SIMD and arena results were reused.

Three single Hidden Uhra residential captures used the same 1280x720 D3D12,
AA=3, 60-cap, background and muted setup. The final same-EXE control moved only
the driver input/screenshot-request files to TEMP. The final all-city sample
had 1,790 frames at 59.651 FPS mean, 45.989 FPS 1% low and 43.0117 ms worst
accepted-present. In the paired render-frame 1600–2800 window, mean was 59.918
FPS, 1% low 54.495 FPS, draw max 13.162 ms, vertex max 2.2981 ms and there
were no draw over-budget samples or rehashes. The requested mean of at least
58 FPS is met on this route with the 60 cap. The strict present ratio remains
50.375% over 16.67 ms, so this is near-60 route evidence rather than a locked
60 FPS result or strict S4 pass.

The bounded run exposed a 412.9283 ms previous-swap post-present interval;
moving the polled control files to TEMP reduced that maximum to 0.4193 ms and
the long-stall class did not recur. This localizes the measurement-path I/O,
but does not establish a Syncthing filesystem or scheduler root cause. The
new previous-swap, post-present and command-processor-idle fields are
diagnostic only. `LO_VERTEX_TIMING` remains off by default. All captures were
Hidden and muted, every `original_saves_changed` result was false, and the
independent original-save SHA-256 inventories had zero differences. The final
image was visually checked as the expected Uhra residential area.

The final executable is `out/perf-ring/run/LostOdysseyRecomp.exe` with SHA-256
`9D9460248FEB72AC7239ABD40AC1DA6619847F176CF4AA38AD6F725C6923852B`; its PDB
is alongside it. Compilation, linking and provenance completed; the known
post-build `dxcompiler.dll` copy failure was reused because the run directory
already contained the DLL. This v0.5.6-targeted work is included in the published v0.5.6 package; its measured evidence remains tied to source version 0.5.4. It establishes neither whole-game behavior,
player acceptance nor a new release. Evidence: [city vertex-cache follow-up](notes/city-60fps-handoff.md),
`out/perf-ring/vertex-stage/{REPORT.md,comparison.json,identity.json,vertex-cache-test-evidence.json}`.

The current source and published release are v0.5.6. The executable and
performance evidence above remain from the measured source-0.5.4 runtime; no
rebuild or test rerun was performed for this version increment.

The implementation is recorded in code commit
`ae287f2a43a73c6f6bea61c40822c37f40afe052`. The measured executable was built
from the same runtime source before that commit; the commit did not trigger a
rebuild or rerun, so its hash and the reported performance figures are
unchanged.

The earlier SIMD conversion run remains historical context: its 41.736 ms
vertex hitch did not identify the cause. Current follow-up should focus on
broader-scene and longer-session coverage of bounded-cache eviction and on
remaining accepted-present timing variability; do not reclassify the TEMP
comparison as proof of a filesystem root cause.

### SIMD conversion diagnostic — included in published v0.5.6

The local renderer now uses `gpu::geometry_prepare::CopyDwordsSwapped` for the
two vertex-buffer conversion paths. Endian 0 uses `memcpy`; endian modes 1/2/3
use an SSSE3 four-dword loop where available, followed by scalar tails and the
portable fallback. The production object contains the expected `vpshufb`
instruction. The focused geometry fixture passed 16,685,865 checks, including
unaligned inputs and inaccessible-page boundary guards
(`out/perf-ring/simd-copy/geometry_prepare_test.log`).

A same-harness Hidden user01 city run used the local executable
`AFCC4BE89B42C033FB4041185E35F33CDDFCF7832FEACE2F6FA8328F10BFA7C2` and
completed in 57.5 seconds with 1,801 city frames, 884 menu frames and
`original_saves_changed=false`. The run kept 1280x720, window mode 0, backend 0,
AA 3 and frame-rate target 60, with the original configuration unchanged. City draw mean was 8.203 ms (p95 10.473 ms,
p99 11.547 ms, max 49.265 ms), vertex mean 1.467 ms, fence wait 0.003 ms and
`gpu_batches` 1.001. On the fixed render-frame 1600–2800 window, draw mean was
7.838 ms and vertex mean 1.396 ms; the matching same-harness no-rebuild baseline
was 7.766 ms and 1.436 ms. In the fixed 1600–2800 window, present mean was
16.779 ms, 51.457% of intervals exceeded 16.67 ms, and the 1% low was 36.539
fps; the baseline was 16.831 ms, 50.458% and 31.044 fps. The SIMD run still contained a 41.736 ms vertex hitch and a
separate 47.467 ms flush sample. Its 779.572 ms worst accepted-present interval
was a load-in `between_ms` interval, not a flush. Neither redirected-log run
reproduced the earlier 200–400 ms flush class, but the residual stalls remain
unexplained.

This is implementation and bounded diagnostic evidence only. The earlier
handoff attribution of the vertex hitch directly to `CopySwapped` is not
confirmed. `tVertex` covers the whole vertex-fetch loop
(`renderer.cpp:3339–3380`), including lookup, sample matching, capture/vector
resize, conversion and map insertion (`renderer.cpp:2818–2850`); rehash,
allocation, page fault and scheduling effects remain hypotheses. No SIMD
performance gain, S4 pass, player acceptance, Vulkan coverage or new release
is claimed. This local source remains version 0.5.4 and is absent from the
published v0.5.4 package. Full evidence is in the [city 60 FPS
handoff](notes/city-60fps-handoff.md) and
`out/perf-ring/city-simd-comparison.json`.

### TAA binding evidence — 0.5.2 historical build

The 0.5.2 development client now adds an opt-in schema 3 binding-evidence record for a bounded TAA draw. It preserves the VS/PS renderer hashes and draw classification, then records the consumer slot and phase, guest and uploaded VP values, raster viewport and jitter bits, selected PS `c0` values, and the referenced texture's format, extents, resolve rectangle, producer state, producer draw count, frame ages and resolve gap. The record uses an independent fixed 64-entry POD queue; collection is batched in groups of eight and is triggered by the existing 60-second background cadence or F1 request. It does not add GPU readback, a new wait, pointer or address data, or a jitter mapping.

The production build completed successfully with executable SHA-256 `c3711463fd4d21f17b68af27cecd2df35851af49e9567e956596b48a7ae1e259` and size 82,904,064 bytes. CPU queue and producer fixtures passed 82 and 32 checks with zero producer allocations. The updated 9,761-byte, eight-record serializer passed three real C++ → Worker → SQLite uploads with HTTP 200 responses; replay preserved eight deduplicated rows, 1,048 record leaf values and 640 IEEE bit words. Evidence: `out/v0.5.2/taa-binding-collection/production-build-resume.json`, `run.log` and `worker-integration.json`.

The earlier schema 3 deployment was Worker version `4db7e156-8a21-4f57-8f0d-bacd0c4576ab`; it advertised schemas 1, 2 and 3. The current Worker is `14e74c54-1213-4240-9877-047f35cdda75` and advertises schemas 1 through 4. No D1 schema migration was needed. There is still no new game run, F1 menu acceptance or visual acceptance for the `e810cfacc107fd3c` path, so schema 3 remains diagnostic evidence and does not authorize a jitter mapping.

For schema 3, `draws` and D1 `max_draws` count samples of the same recorded state, not all draws in a frame or a total frame draw count. `producerDraws` is a conservative CPU writer count; values above 65,535 become `0` with `Unknown` state. `Uniform` records consistency among CPU-recorded writer matrix and jitter values, without proving GPU completion, per-pixel use or recursive texture dependency. Detailed serializer and artifact evidence is in `out/v0.5.2/taa-binding-collection/REPORT.md`.

### Compact automatic diagnostics — v0.5.3

The new compact diagnostic contract uses the existing opt-in and records one fixed 32-frame CPU window at most every 180 seconds. It bounds the window at 24 VS/PS pairs and eight binding records, then includes final TAA history/rejection booleans, coverage reason counters, pending counts and delivery results. The request is capped at 32 KiB and uses a strict compact receipt; sparse GPU data is D3D12-only and remains unlinked to this window. No color preview, raw F1 ZIP, random/session/player/device identifier or new GPU completion claim is added.

The Worker validator passed 9/9 checks in `out/v0.5.2/compact-diagnostics/worker-tests.json`. The C++ fixture passed 68 checks with zero allocations; its corrected 18,905-byte request passed two loopback HTTP 200 uploads into the actual SQLite schema, preserving one deduplicated row and all window fields. The 0.5.2 client build completed at 2026-09-10 16:42:36 UTC with SHA-256 `59f039ea84404c1ab85095a95a10b32d435bf1d39b1ca610b38d15edb44ce62e` and size 82,946,560 bytes. Eight new ledger checks and one archive check passed; the installed wrapper's current archive sync remained unchanged across 18 cases with identical source hashes and modification times. The private archive schema 4 allowlist is published in commit `728d6030980a7be39feca493319f54bfb1a62e74`, and current Worker deployment `14e74c54-1213-4240-9877-047f35cdda75` advertises schemas 1 through 4 with temporal and shader-source capabilities enabled. Existing F1 force flush remains optional and is not a dependency. These are bounded diagnostic checks; no new game or visual acceptance is claimed.

Window completeness means 32 final CPU frame snapshots, not all draws or bindings and not GPU completion. Counters describe observation calls within the window; delivery counts are cumulative since the consent/device reset. Existing summary, source, binding and sparse streams use HTTP 200 acceptance, while compact delivery requires the validated compact receipt. The wire carries source version and protocol build information; an unknown `runtimeCommit` does not identify the exact executable.

The private [research archive repository](https://github.com/freefrank/LostOdysseyRecomp-build-inputs) is now enabled by a daily GitHub Action. Its first successful run is [Action 34493751731](https://github.com/freefrank/LostOdysseyRecomp-build-inputs/actions/runs/34493751731), with content-addressed deduplication and a data commit beginning `8da2b10e`. The first snapshot validated 3,401 diagnostics, 431 unique VS/PS programs, 909 GPU associations, 31 temporal records and 462 payloads. This archive is a private, long-term research copy; it has no D1-style automatic expiry.

### Updater validation

The v0.5.3 updater fixes ZIP staging for the explicit top-level directory entry emitted by Python `shutil.make_archive`; the entry's trailing slash is accepted while the archive must still contain exactly one package root. Standalone updater text and error dialogs are English, and native buttons are requested with `en-US`.

The focused archive runner passed 12/12 cases, including successful release-style staging, implicit and root-last directory handling, and rejection of multiple roots, top-level files, absolute or parent roots, traversal, duplicate and unlisted payloads, hash mismatch and missing `manifest.json`. At that earlier updater checkpoint, the updater-only `LoUpdaterTest` build succeeded while the game executable was not rebuilt; the retained embedded-updater evidence therefore belongs to that checkpoint. The later full game build includes the updated embedded updater source, but the updater transaction was not re-accepted. Render-only UI checks passed with English text and font assertions and no clipping; a live native dialog was not exercised. The v0.5.3 package is published, but no complete update transaction or player acceptance is claimed. Evidence: `out/updater-fix/REPORT.md`, `out/updater-fix/archive-test.log` and `out/updater-fix/ui/render-manifest.json`.

The retained D3D12 RTX 5080 background run used source-0.5.0 executable SHA256 `1beb8a50c5bef1ebf0fb147338b33d558a2193e87f82b0b8c579ecbcb856959d`, 1280×720 experimental TAA, 60 FPS target and Map 16 Main Street. Two periodic uploads added 23 then 32 programs, for 55 total (15 VS / 40 PS, 19,188 bytes). All received sources matched the local cache by bytes, SHA-256, renderer FNV and length; the final snapshot had 55 source rows and associations without duplicate keys, plus 28 matching structured diagnostics. Map 16 windows measured 59.67 FPS / p95 18.036 ms before the second upload and 59.84 FPS / p95 18.067 ms during it. This is bounded background acceptance, not F1 product export, opt-out A/B, universal zero-overhead proof, Vulkan/AMD coverage, flicker repair acceptance or whole-game validation. Evidence: runtime report (`out/v0.5.0/shader-source-collection/runtime/REPORT.md`, retained locally), D1 verification (`out/v0.5.0/shader-source-collection/runtime/D1-VERIFICATION.md`, retained locally) and shader-source report (`out/v0.5.0/shader-source-collection/REPORT.md`, retained locally).

Product F1 ZIP/manifest and immediate post-capture upload remain pending. The attempted `LO_CAPTURE_REQUEST` was a legacy trace request. The latest flicker diagnostic used older executable `ececed95`, before the four c7 paths; it is not a regression acceptance result. F1 frame costs were 1.690/3.156/1.561 seconds, and the later background ZIP attempt timed out at 325.117→385.166 seconds with the original directory retained. See flicker analysis (`out/v0.5.0/taa-flicker-20260910/analysis/REPORT.md`, retained locally).

The current Windows delivery scope is D3D12/Vulkan. DX11, Linux, macOS, experimental Switch work, other GPU coverage, DLC rewards/dungeons and full-game compatibility remain separate validation areas. Open work-item status and priorities are maintained in the [public Maintainer Project](https://github.com/users/freefrank/projects/3); completed change history is in the [CHANGELOG](../CHANGELOG.md).

<a id="live-issue-reconciliation"></a>

## Issue and acceptance boundary

GitHub Issue state, reporter acceptance and implementation evidence are separate facts. Use the [public Maintainer Project](https://github.com/users/freefrank/projects/3) for current work-item state and the [CHANGELOG](../CHANGELOG.md) for completed releases. Dated implementation and investigation evidence remains in `docs/notes/`.

Earlier Issue closure sources and their acceptance limits are preserved in the [archived reconciliation](archive/STATUS-2026-09-10.md#live-issue-reconciliation).

## Archived status snapshot

The former detailed status ledger, including historical candidate identities and dated validation narratives, is preserved in [STATUS-2026-09-10.md](archive/STATUS-2026-09-10.md). It is historical evidence and does not define the current source or release state.
