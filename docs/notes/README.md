# 逆向与验证笔记

[文档总入口](../README.md) · [当前状态](../STATUS.md) · [路线图](../ROADMAP.zh-CN.md) · [历史归档](../archive/README.md)

2026-10-02 整理：本目录收录全部 **163 篇**专项笔记，按主题列出，每篇只出现一次。文件名和原始证据继续保留，目录不重复维护版本发布或任务进度。

## 如何使用

| 标记 | 阅读方式 |
|---|---|
| 参考 | 维护中的专题机制或操作说明；其中旧测试仍只覆盖原版本与条件。 |
| 历史 | 日期、版本或阶段绑定的测量、修复、交接与进度；其中“当前”“未发布”、PID 和后续步骤只属于当时。 |
| 调查 | 问题分析及其证据边界；此标记不等于对应 Issue 仍开放，现状以 STATUS / 路线图为准。 |
| 草案 | 设计、研究、需求或发布文案；不代表已实施，也不因年代久远自动取消。 |

分类合计：参考 30、历史 106、调查 17、草案 10。

先用当前状态确认实施、验证、验收和发布，再从下方进入证据。旧命令执行前核对源码、工具及输入；`out/`、本机绝对路径和截图编号是原调查的定位信息，不随仓库分发。已缺失的本地产物标为历史路径，不能视作本次复验。

## 常用专题入口

[FG 接入参考](reusable-fg-game-integration.md) · [Gate 1 验收依据](gate1-host-repair-20260927.md) · [设置菜单](settings-menu.md) · [渲染捕获](render-state-capture.md) · [Shader 准备](shader-preparation.md) · [CPU 优化指南](cpu-performance-optimization-guide.md) · [发布打包](release-packaging.md) · [随时存档限制](issue-74-save-anywhere-party-split.md)

## 帧生成与 SDK 接入

| 文档 | 类型 |
|---|---|
| [Vulkan DLSS/FSR FG、MetalFX 实现与 FSR 4 调查](vulkan-fg-fsr4-metalfx.md) | 调查 |
| [PR69 foreground validation — 2026-09-26](fg-pr69-validation-2026-09-26.md) | 历史 |
| [FSR / DLSS FG 历史交接（2026-09-24）](fsr-dlss-fg-codex-handoff.zh-CN.md) | 历史 |
| [FSR / DLSS FG Codex 历史交接记录](fsr-dlss-fg-codex-history.zh-CN.md) | 历史 |
| [FSR / DLSS FG Codex 历史进度日志](fsr-dlss-fg-codex-progress.zh-CN.md) | 历史 |
| [FSR 超分与 DLSS 帧生成阶段性交接文档（P0 阶段）](fsr-dlss-fg-handoff.zh-CN.md) | 历史 |
| [FSR / DLSS FG 未完成任务导入清单（历史）](fsr-dlss-fg-imported-tasks.zh-CN.md) | 历史 |
| [Gate 1 host repair follow-up — 2026-09-27](gate1-host-repair-20260927.md) | 历史 |
| [PR73：FSR+FG初始化与运行状态修正（待实机验证）](pr73-fsr-fg-recovery-20260927.md) | 历史 |
| [Reusable FG game integration](reusable-fg-game-integration.md) | 参考 |
| [v0.8.0 FG checked completion：第一批实现](v0.8.0-fg-checked-completion-20260927.md) | 历史 |
| [v0.8.0 游戏 DLSS FG 接入记录](v0.8.0-fg-game-integration-20260927.md) | 历史 |
| [P0 input-completion bootstrap and retirement fix](v0.8.0-fg-input-completion-fix.md) | 历史 |
| [v0.8.0 FG P3 外部 Agent 交接](v0.8.0-fg-p3-external-agent-handoff.md) | 历史 |
| [v0.8.0 P3 实机诊断记录](v0.8.0-fg-p3-hardware-20260926.md) | 历史 |
| [P3: exact producer / resolve / video diagnostic handoff](v0.8.0-fg-p3-resolve-handoff.md) | 历史 |
| [v0.8.0 P3 UI replay 诊断](v0.8.0-fg-p3-ui-replay-20260926.md) | 历史 |
| [v0.8.0 FG SDK 同步归因记录](v0.8.0-fg-sdk-sync-attribution-20260926.md) | 历史 |
| [v0.8.0 P0、P1、P4 开发交接](v0.8.0-p0-p1-p4-development.zh-CN.md) | 历史 |
| [v0.8.0 P0退出清理与P4组合连续性：代码补齐](v0.8.0-p0-p4-cleanup-continuity-20260927.md) | 历史 |
| [Reusable frame generation development](v0.8.0-reusable-fg-20260927.md) | 历史 |

## 超分与运动向量

| 文档 | 类型 |
|---|---|
| [FSR分支修复：录制恢复、设备丢失与Vulkan内存选择](fsr-repair-2026-09-24.zh-CN.md) | 历史 |
| [Full-Game Motion Vector Audit (Phase 0)](full-game-motion-vector-audit.md) | 调查 |
| [Full-Game Motion Vector：持续开发交接与验收计划](full-game-motion-vector-continuation-handoff.md) | 历史 |
| [Full-Game Motion Vector Architecture & Design (Phase 1 & 2)](full-game-motion-vector-design.md) | 草案 |
| [Full-Game Motion Vector — Next Development Handoff](full-game-motion-vector-next-step-handoff.md) | 历史 |
| [Issue #64：Hybrid MV修复与验证](issue-64-hybrid-motion-validation.zh-CN.md) | 历史 |
| [Geometric motion replay: implementation and validation boundary](motion-vector-implementation.md) | 参考 |
| [Motion Vector Implementation Milestone Report (M0 - M4)](motion-vector-milestone-report.md) | 历史 |
| [Motion Vector Vertex Shader Position Dependency Analysis (M1 Report)](motion-vector-vs-position-dependency-report.md) | 历史 |
| [MV audit repair on the geometric replay baseline](mv-audit-repair.md) | 历史 |
| [原生 DLAA 初步实现](native-dlaa-initial.zh-CN.md) | 历史 |
| [Native Vulkan DLSS Color Qualification Plan & Implementation Specification](native-dlss-color-qualification-plan.md) | 草案 |
| [原生 Vulkan DLSS Super Resolution 开发与交接指南 (Native DLSS Handoff & Guide)](native-dlss-handoff.zh-CN.md) | 历史 |
| [P2续开发与下一次实机验收（2026-09-21）](native-dlss-p2-next-run.zh-CN.md) | 历史 |
| [Native DLSS P2 follow-up — 2026-09-21](native-dlss-p2-progress-2026-09-21.md) | 历史 |
| [Native DLSS Validation (P0 Foundation, P1 Temporal Inputs & P2 Checkpoint)](native-dlss-validation.md) | 历史 |
| [Temporal upscaling feasibility: DLSS, FSR and shared inputs](temporal-upscaling-feasibility.md) | 草案 |

## TAA 与 jitter

| 文档 | 类型 |
|---|---|
| [Burning Cave material jitter (f25276) — 2026-10-01](jitter-cave-f25276-2026-10-01.md) | 历史 |
| [Ice Canyon sky jitter (f12139) — 2026-10-01](jitter-sky-f12139-2026-10-01.md) | 历史 |
| [Sky pairs and a depth VS from the map tour — 2026-10-01](jitter-sky-tour-2026-10-01.md) | 历史 |
| [Battle depth writers and the tour material 8d66 — 2026-10-01](jitter-battle-depth-2026-10-01.md) | 历史 |
| [Old Sorceress' Mansion sky jitter (#121) — 2026-10-01](jitter-sky-121-2026-10-01.md) | 历史 |
| [Cutscene depth and floor jitter — 2026-10-01](jitter-cutscene-f6814-2026-10-01.md) | 历史 |
| [Temporal jitter coverage review — 2026-09-25](jitter-coverage-2026-09-25.md) | 历史 |
| [Late-pass jitter follow-up — 2026-09-25](jitter-late-pass-followup-2026-09-25.md) | 历史 |
| [Screen-sampling candidate batch — 2026-09-25](jitter-screen-batch-2026-09-25.md) | 历史 |
| [TAA bloom prefilter candidate (2026-09-12)](taa-bloom-prefilter.md) | 调查 |
| [TAA shader 覆盖审查](taa-coverage-audit.md) | 调查 |
| [Current-scene TAA coverage and static discovery — 2026-09-13](taa-current-scene-2026-09-13.md) | 历史 |
| [TAA jitter mapping for captures f5912 and f16385 — 2026-09-13](taa-f5912-f16385-2026-09-13.md) | 历史 |
| [TAA jitter mapping for capture f5997 — 2026-09-13](taa-f5997-2026-09-13.md) | 历史 |
| [从日志与 render state 定位 TAA 顶点路径遗漏](taa-log-only-triage-2026-09-09.md) | 历史 |
| [TAA Bell-Stand Flicker Investigation (Issue #46 adjacent)](TAA_BELL_FLICKER_INVESTIGATION.md) | 调查 |
| [TAA 铃铛架闪烁排查（Issue #46 相关）](TAA_BELL_FLICKER_INVESTIGATION.zh-CN.md) | 调查 |

## GPU 渲染与画面问题

| 文档 | 类型 |
|---|---|
| [Radeon 8060S 偏黑画面调试交接](amd-8060s-dark-render-handoff.md) | 调查 |
| [AMD resolve initialization](amd-resolve-initialization.md) | 历史 |
| [Live anisotropic filtering](anisotropic-filtering.md) | 参考 |
| [Fire-hit lighting investigation (2026-09-05)](fire-hit-rendering.md) | 调查 |
| [GPU 当前说明（2026-09-05）](gpu.md) | 历史 |
| [Experimental HDR output](hdr-output.md) | 参考 |
| [Issue #70 DX12/Vulkan optimization](issue-70-dx12-vulkan-optimization.md) | 历史 |
| [Kaim body-shadow flicker: v0.4.0 investigation](kaim-body-shadow-v040.md) | 历史 |
| [光照 / 阴影续修（2026-09-04）](lighting-stencil-depth-clear.md) | 调查 |
| [Linux/Vulkan HDR research — 2026-10-02](linux-vulkan-hdr.md) | 调查 |
| [Map12 poster black-patch repair — 2026-09-05](map12-poster-depth.md) | 历史 |
| [Host GPU occlusion queries (#118)](occlusion-queries.md) | 参考 |
| [角色黑色剪影：物理地址别名与遮挡查询（2026-09-04）](physical-alias-rendering.md) | 历史 |
| [Polygon offset 接入与阴影回归（2026-09-05）](polygon-offset.md) | 调查 |
| [战后演出白屏与 EDRAM 格式切换（2026-09-04）](post-battle-whiteout.md) | 历史 |
| [Debug Menu render-state capture](render-state-capture.md) | 参考 |
| [角色破面与后期重影调查（2026-09-04）](rendering-index-and-resolve.md) | 历史 |
| [渲染验证（2026-09-04）](rendering-validation.md) | 历史 |
| [阴影纹理采样 LOD（2026-09-05）](shadow-texture-lod.md) | 调查 |
| [启动时深度清除导致驱动退出（2026-09-05）](startup-depth-clear-crash.md) | 历史 |
| [Tall and non-standard aspect layout — 2026-10-01](tall-aspect-layout.md) | 参考 |
| [标题动态背景恢复（2026-09-04）](title-packed-mips.md) | 历史 |
| [Xenia 实机画面对照（2026-09-04）](xenia-render-comparison.md) | 历史 |

## 性能与帧率

| 文档 | 类型 |
|---|---|
| [Assembly profiler gameplay captures — 2026-09-11](asm-profiler-gameplay.md) | 历史 |
| [Battle camera spin at 90/120 FPS (#117) — 2026-10-01](battle-camera-120fps-2026-10-01.md) | 历史 |
| [City 60 FPS handoff — 2026-09-12](city-60fps-handoff.md) | 历史 |
| [Card A city measurement — published v0.5.11 — 2026-09-14](cpu-card-a-city-2026-09-14.md) | 历史 |
| [Card B city measurement — published v0.5.11 — 2026-09-14](cpu-card-b-city-2026-09-14.md) | 历史 |
| [Card C prepare gate — published v0.5.11 — 2026-09-14](cpu-card-c-prepare-gate-2026-09-14.md) | 历史 |
| [Card D 3C6T city measurement — published v0.5.11 — 2026-09-14](cpu-card-d-3c6t-city-2026-09-14.md) | 历史 |
| [Live v0.5.7 CPU and 4K profiling — 2026-09-13](cpu-live-profile-2026-09-13.md) | 历史 |
| [CPU 性能优化指南：3C6T 预算、可行并行与反模式](cpu-performance-optimization-guide.md) | 参考 |
| [CPU recompilation deep-dive — 2026-09-13](cpu-recomp-deep-2026-09-13.md) | 历史 |
| [4K Vulkan shadow-loop GPU audit — 2026-09-12](gpu-shadow-loop-audit-2026-09-12.md) | 历史 |
| [原生90/120 FPS：实现与本地验收](native-90-120fps.md) | 参考 |
| [性能分析完整报告 — 2026-09-11](perf-complete-analysis.md) | 历史 |
| [GPU 环缓冲实测对比 — 2026-09-11](perf-gpu-ring-compare.md) | 历史 |
| [Uhra City CPU Optimization Results — 2026-09-18](PERF_CITY_UHRA_RESULTS.md) | 历史 |
| [乌斯拉城 CPU 优化实测报告 — 2026-09-18](PERF_CITY_UHRA_RESULTS.zh-CN.md) | 历史 |
| [psvita 乌斯拉城市性能对比（发布素材草稿）](psvita-performance-comparison-2026-09-18.md) | 草案 |
| [ReBlue vs Lost Odyssey GPU 对照 — 2026-09-11](reblue-gpu-comparison.md) | 历史 |
| [v0.5.0 性能诊断记录](v0.5.0-performance-diagnosis-2026-09-09.md) | 历史 |
| [FreeSync / G-SYNC Compatible：应用侧VRR接入](vrr-freesync-gsync-compatible.md) | 参考 |
| [Vulkan depth-clear performance — 2026-09-13](vulkan-depth-clear-performance-2026-09-13.md) | 历史 |

## 着色器准备与覆盖

| 文档 | 类型 |
|---|---|
| [启动着色器准备（2026-09-05）](shader-preparation.md) | 参考 |
| [One-file shader store and the 2026-10-02 packs](shader-store-2026-10-02.md) | 参考 |
| [Built-in shader resource index](shader-resource-index.md) | 参考 |
| [Startup shader coverage follow-up — 2026-09-13](shader-startup-coverage-2026-09-13.md) | 历史 |

## 界面、输入与调试

| 文档 | 类型 |
|---|---|
| [Built-in cheats and LT speed control](cheats.md) | 参考 |
| [Multiple controllers and keyboard input](controller-input.md) | 参考 |
| [Debug 地图 ID 和本地化名称（2026-09-05）](debug-map-info.md) | 参考 |
| [Debug 人物传送：逆向依据与验证边界（2026-09-04）](debug-teleport.md) | 历史 |
| [Desktop UI modernization](desktop-ui-modernization.md) | 历史 |
| [Issue #40 UI Resource Map and Asset Archive](issue-40-ui-resource-map.md) | 历史 |
| [Issue #40 UI 资源定位与资产映射归档](issue-40-ui-resource-map.zh-CN.md) | 历史 |
| [Original Settings menu assets](menu-original-assets.md) | 参考 |
| [Settings menu interaction and redraw — 2026-09-24](settings-menu-interaction.md) | 历史 |
| [设置菜单与显示选项](settings-menu.md) | 参考 |
| [窗口消息停摆：独立事件线程](window-event-pump.md) | 历史 |

## 运行时、存档与游戏流程

| 文档 | 类型 |
|---|---|
| [音频输出与对白修复（2026-09-05）](audio-output.md) | 历史 |
| [戒指战斗资源名缺少语言后缀（2026-09-04）](battle-ring-resource.md) | 历史 |
| [Guest critical-section byte order](critical-section-endian.md) | 参考 |
| [Guest function findings ledger (2026-10-09)](guest-function-findings.md) | 参考 |
| [Automatic selection of imported discs](disc-selection.md) | 参考 |
| [DLC 目录枚举与 `FindNext` pattern 诊断](dlc-directory-enumeration.md) | 历史 |
| [随机遇敌动画与战斗停滞调查（2026-09-04，主角与战斗停滞已修复，敌人待查）](encounter-animation.md) | 历史 |
| [USA/Europe edition support](europe-support.md) | 参考 |
| [Issues #54 / #55: audio follow-up and validation](issue-54-55-audio-followup.md) | 历史 |
| [Issue #74：分队期间随时存档后无法换人（2026-09-29 调查）](issue-74-save-anywhere-party-split.md) | 调查 |
| [Issue #12: funeral flower hand-in investigation](issue12-funeral-crash.md) | 历史 |
| [Issue #12 交接（2026-09-09）：根因已定位，生产交花路径已通过](issue12-handoff-2026-09-09.md) | 历史 |
| [Issue #12 root cause: garbage-collected materials drawn by a stale scene proxy](issue12-root-cause.md) | 历史 |
| [Issue #7: cutscene closure and missing crash diagnostics](issue7-cutscene-crash.md) | 历史 |
| [Issue #114: field interaction during a pending battle request](ISSUE_114_FIELD_INTERACTION_20261001.md) | 调查 |
| [Issue #53 Disc 2 loading investigation](ISSUE_53_DISC2_HANG_FIX_REPORT.md) | 历史 |
| [Project review fixes — 2026-09-30](PROJECT_REVIEW_FIXES_20260930.md) | 历史 |
| [内核 HLE 当前说明（2026-09-05）](kernel.md) | 历史 |
| [重编译配置笔记](recomp.md) | 参考 |
| [Recompiler width and control-flow audit](recompiler-width-audit.md) | 参考 |
| [随时存档：菜单权限与验证](save-anywhere.md) | 参考 |
| [Unicode startup and save paths (2026-09-06)](save-path-unicode.md) | 调查 |
| [手动存档失败（2026-09-04）](save-storage.md) | 历史 |
| [跨版本文本补充补丁（已挂起）](text-language-patch.md) | 草案 |
| [第三地图卡住调查（2026-09-04）](third-map-hang.md) | 历史 |
| [攻略路线与后台推进测试（2026-09-04）](walkthrough-testing.md) | 草案 |
| [游戏数据来源与 XEX](xex.md) | 参考 |

## 平台与开发工具

| 文档 | 类型 |
|---|---|
| [Pull request checks and releases on Gitea](ci-gitea.md) | 参考 |
| [Developer-tool recovery record — 2026-09-24](developer-tool-recovery-2026-09-24.md) | 历史 |
| [Android 移植研究（2026-10-02）](android-port-research-2026-10-02.md) | 调查 |
| [Android ARM64 DXC 构建（2026-10-02）](android-dxc-build-2026-10-02.md) | 参考 |
| [Linux 移植评估与首可玩裁定（2026-09-13）](linux-port-evaluation-2026-09-13.md) | 历史 |
| [Switch 移植评估与 PC Vulkan 后端交接（2026-09-07）](switch-vulkan-handoff.md) | 历史 |

## 版本、计划与历史交接

| 文档 | 类型 |
|---|---|
| [v0.4.0 后续开发交接（2026-09-07）](handoff-v0.4.0-followup.md) | 历史 |
| [接手入口](handoff.md) | 历史 |
| [Automated issue triage](issue-triage.md) | 参考 |
| [Issue #14–#16 triage — 2026-09-12](issues14-16-triage.md) | 历史 |
| [Bug report：v0.6.7 之后的代码审计](post-v0.6.7-bug-report.zh-CN.md) | 调查 |
| [改进建议：v0.6.7 之后的渲染与设置界面](post-v0.6.7-improvement-proposals.zh-CN.md) | 草案 |
| [PR #59、#60、#68 处理记录](pr-59-60-68-triage.zh-CN.md) | 历史 |
| [PR73：validation对象归因与深度-only附件修正](pr73-validation-object-attribution-20260927.md) | 历史 |
| [v0.2.2 release notes / 发布说明](release-0.2.2.md) | 历史 |
| [Lost Odyssey Recomp v0.6.0 — This one means a lot to me](release-0.6.0-post-draft.md) | 草案 |
| [Installer and Windows release pipeline](release-packaging.md) | 参考 |
| [v0.4.0 development scope](v0.4.0-development.md) | 历史 |
| [v0.5.0 development history](v0.5.0-development.md) | 历史 |
| [v0.5.0 QOL 需求与未来验收标准](v0.5.0-qol-requirements.md) | 草案 |
| [v0.5.0 渲染、性能与发布交接 — 2026-09-09](v0.5.0-rendering-handoff-2026-09-09.md) | 历史 |
| [v0.7.0：DLSS-G 与 FSR FG 开发计划](v0.7.0-frame-generation-plan.md) | 草案 |

## 相关记录

[文档整理记录](../audits/documentation-2026-09-30.md)说明本次来源、状态修正和 Basic Memory 知识整理；[归档目录](../archive/README.md)保存已被替代的全局账本。新增笔记应补入本索引，并给出适用版本、证据范围和现行入口。
