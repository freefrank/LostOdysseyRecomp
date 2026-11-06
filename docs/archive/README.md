# 历史归档 / Historical archive

保留原始地址、实验顺序和被推翻的假设。归档不是当前待办或支持承诺，先读[当前状态](../STATUS.md)。仍有效的专项修复证据继续保留在活动笔记中。

归档按内容是否仍承担当前指南职责判断，不按年龄自动删除。原文件名和历史标题保持稳定；文档移入归档时同步修复了指向它们的链接，旧路径不再保留跳转页。本地 `out/` 证据不随仓库分发。2026-09-30 检查中缺失的产物保留为明确标注的代码路径，不能用来证明本次已经复验。

## CI 历史

| 文档 | 归档原因 | 当前入口 |
|---|---|---|
| [Gitea CI](notes/ci-gitea.md) | 保留 2026-09-30 起的 runner 配置、发布记录和测量；T640 停用后由 GitHub-hosted CI 取代 | [GitHub Actions](../notes/ci.md) |

## 全局账本与早期快照

| 快照 | 归档原因 | 当前入口 |
|---|---|---|
| [GPU 调查](2026-09-04/gpu.md) | 旧缓存版本、后端未开始等阶段描述，以及已被推翻的根因假设 | [当前状态](../STATUS.md)；后续简版说明 [GPU](notes/gpu.md) 也已归档 |
| [内核调查](2026-09-04/kernel.md) | XMA 未实现、静音占位和旧存档状态 | [存档](notes/save-storage.md) / [音频](../notes/audio-output.md) / [XEX](../notes/xex.md)；后续简版说明 [内核](notes/kernel.md) 也已归档 |
| [旧交接](2026-09-04/handoff.md) | 旧攻略范围、音频状态及已撤销的前台操作限制 | [当前状态](../STATUS.md) / [路线图](../ROADMAP.zh-CN.md)；后续的 [接手入口](notes/handoff.md) 也已归档 |
| [STATUS-2026-09-10](STATUS-2026-09-10.md) | 详细状态账本已由当前精简状态取代 | [当前状态](../STATUS.md) |
| [STATUS-2026-09-29](STATUS-2026-09-29.md) | 保留整理前逾 1,100 行发布、实验和验收账本；旧“未发布”和失败门禁只对应当时 checkpoint | [当前状态](../STATUS.md) |
| [RELEASE-v0.5.0-preparation](RELEASE-v0.5.0-preparation.md) | 旧版本发布准备记录，保留历史 receipt | [更新日志](../../CHANGELOG.md) |
| [RELEASE-v0.2-historical](RELEASE-v0.2-historical.md) | 已发布的早期版本说明，保留历史兼容性边界 | [更新日志](../../CHANGELOG.md) |
| [RELEASE-v0.1-historical](RELEASE-v0.1-historical.md) | 首个 Windows 版本说明，保留历史兼容性边界 | [更新日志](../../CHANGELOG.md) |
| [WORK_REPORT_2026-09-05](WORK_REPORT_2026-09-05.md) | 日期工作报告，已由当前状态和专项笔记取代 | [当前状态](../STATUS.md) |
| [ROADMAP-2026-09-10](ROADMAP-2026-09-10.md) / [中文](ROADMAP.zh-CN-2026-09-10.md) | 旧路线图快照，保留当时需求和锚点 | [当前路线图](../ROADMAP.md) / [中文](../ROADMAP.zh-CN.md) |
| [Issue #12 Claude 交接](issue12-claude-handoff-2026-09-09.md) | 临时调查交接已由 Issue #12 调查记录和根因记录取代，保留原始复现边界与探针证据 | [Issue #12 调查记录](notes/issue12-funeral-crash.md) / [根因记录](notes/issue12-root-cause.md) |
| [Clang/PGO 与线程优化研究](clang-pgo-benchmark-2026-09-17.md) | 2026-09-16—17 的构建矩阵与基准准备记录；该次城市实景基准未完成，不能作为性能收益或用户验收依据 | [CPU 性能优化指南](../notes/cpu-performance-optimization-guide.md) / [当前状态](../STATUS.md) |
| [Clang 优化与测试基准交接](clang-optimization-and-benchmark-handoff-2026-09-17.md) | 2026-09-17 的 Strix Halo 设备基准与 PR 05—09 推进临时交接记录 | [CPU 性能优化指南](../notes/cpu-performance-optimization-guide.md) |
| [Clang 优化、线程拆分与测试指南](clang-optimization-and-threading-plan-2026-09-16.md) | 2026-09-16 的 Clang 优化与线程拆分对照测试提案（未实施） | [CPU 性能优化指南](../notes/cpu-performance-optimization-guide.md) |

## 2026-10-11 归档的文档

2026-10-11 整理：下列文档不再描述当前代码或当前工作（已完成的交接和进度日志、已修复问题的时间线、过往版本的计划和验证、被后续测量取代的单次快照），用 `git mv` 移入。活动目录里指向它们的链接和路径已改指这里。图片 [images/](images/) 只被归档文档引用。

### 根目录文档

docs/archive/ 下的根目录文档和 audits/。原先的 RELEASE-v0.1/v0.2/v0.5.0 与 WORK_REPORT_2026-09-05 跳转页已删除，内容见上表。

| 文档 | 归档原因 | 当前入口 |
|---|---|---|
| [Optimization plan](OPTIMIZATION_PLAN.md) | 2026-09-29 的优化排序与测量（来自上游分支），多数条目已实施或被推翻 | [路线图](../ROADMAP.md) / [render-pass-merging-design-2026-10-10.zh-CN.md](../notes/render-pass-merging-design-2026-10-10.zh-CN.md) / [CPU 性能优化指南](../notes/cpu-performance-optimization-guide.md) |
| [Menu boundary fixes (base 6e48a9c9)](MENU_FIX_VALIDATION.md) | 针对基线 6e48a9c9 的菜单边界修复验证记录 | [测试目录](../../tools/tests/README.md) |
| [Menu / shader prebuild audit — 2026-09-16 (America/Edmonton)](MENU_SHADER_PREBUILD_AUDIT.md) | 2026-09-16 的菜单／着色器预构建审计 | [当前状态](../STATUS.md) |
| [Debug menu 需求](debug-menu-requirements.md) | 2026-09-04 的 debug menu 需求清单，已实现部分见作弊菜单与传送笔记 | [作弊菜单](../notes/cheats.md) / [传送](../notes/debug-teleport.md) / [地图信息](../notes/debug-map-info.md) |
| [0.6.0 prerelease audit repairs](audits/0.6.0-prerelease.md) | 2026-09-18 的 0.6.0 预发布审计 | [当前状态](../STATUS.md) |
| [文档与知识库整理记录 — 2026-09-30](audits/documentation-2026-09-30.md) | 2026-09-30 文档整理记录 | [文档总入口](../README.md) |

### 帧生成与 SDK 接入（archive/notes/）

| 文档 | 归档原因 | 当前入口 |
|---|---|---|
| [PR69 foreground validation — 2026-09-26](notes/fg-pr69-validation-2026-09-26.md) | PR #69 单次前台验证，已被 Gate 1 验收取代 | [Vulkan FG / FSR 4 / MetalFX](../notes/vulkan-fg-fsr4-metalfx.md) |
| [FSR / DLSS FG 历史交接（2026-09-24）](notes/fsr-dlss-fg-codex-handoff.zh-CN.md) | Codex 交接，对应 FG 工作已完成 | [Vulkan FG / FSR 4 / MetalFX](../notes/vulkan-fg-fsr4-metalfx.md) |
| [FSR / DLSS FG Codex 历史交接记录](notes/fsr-dlss-fg-codex-history.zh-CN.md) | Codex 交接历史记录，对应 FG 工作已完成 | [Vulkan FG / FSR 4 / MetalFX](../notes/vulkan-fg-fsr4-metalfx.md) |
| [FSR / DLSS FG Codex 历史进度日志](notes/fsr-dlss-fg-codex-progress.zh-CN.md) | Codex 进度日志，对应 FG 工作已完成 | [Vulkan FG / FSR 4 / MetalFX](../notes/vulkan-fg-fsr4-metalfx.md) |
| [FSR 超分与 DLSS 帧生成阶段性交接文档（P0 阶段）](notes/fsr-dlss-fg-handoff.zh-CN.md) | P0 阶段交接，对应阶段已完成 | [Vulkan FG / FSR 4 / MetalFX](../notes/vulkan-fg-fsr4-metalfx.md) |
| [FSR / DLSS FG 未完成任务导入清单（历史）](notes/fsr-dlss-fg-imported-tasks.zh-CN.md) | 导入的未完成任务清单，条目已完成或取消 | [路线图](../ROADMAP.md) |
| [FSR分支修复：录制恢复、设备丢失与Vulkan内存选择](notes/fsr-repair-2026-09-24.zh-CN.md) | 2026-09-24 FSR 分支修复检查点 | [Vulkan FG / FSR 4 / MetalFX](../notes/vulkan-fg-fsr4-metalfx.md) |
| [FSR 性能对比：v0.8.0 到 main（#305）— 2026-10-08](notes/fsr-performance-v0.8.0-to-main-2026-10-08.zh-CN.md) | #172 单次性能对比，已被后续优化取代 | [画质选项开销](../notes/graphics-options-cost-2026-10-10.zh-CN.md) |
| [Gate 1 host repair follow-up — 2026-09-27](notes/gate1-host-repair-20260927.md) | Gate 1 宿主修复的一次性验证，已合并并验收 | [当前状态](../STATUS.md) |
| [PR73：FSR+FG初始化与运行状态修正（待实机验证）](notes/pr73-fsr-fg-recovery-20260927.md) | PR #73 专项恢复记录 | [Vulkan FG / FSR 4 / MetalFX](../notes/vulkan-fg-fsr4-metalfx.md) |
| [PR73：validation对象归因与深度-only附件修正](notes/pr73-validation-object-attribution-20260927.md) | PR #73 专项验证对象归因 | [Vulkan FG / FSR 4 / MetalFX](../notes/vulkan-fg-fsr4-metalfx.md) |
| [v0.7.0：DLSS-G 与 FSR FG 开发计划](notes/v0.7.0-frame-generation-plan.md) | v0.7.0 帧生成开发计划，版本已发布 | [Vulkan FG / FSR 4 / MetalFX](../notes/vulkan-fg-fsr4-metalfx.md) |
| [v0.8.0 FG checked completion：第一批实现](notes/v0.8.0-fg-checked-completion-20260927.md) | v0.8.0 FG 第一批实现记录 | [Vulkan FG / FSR 4 / MetalFX](../notes/vulkan-fg-fsr4-metalfx.md) |
| [v0.8.0 游戏 DLSS FG 接入记录](notes/v0.8.0-fg-game-integration-20260927.md) | v0.8.0 游戏内 DLSS FG 接入记录 | [Vulkan FG / FSR 4 / MetalFX](../notes/vulkan-fg-fsr4-metalfx.md) / [可复用 FG 接入](../notes/reusable-fg-game-integration.md) |
| [P0 input-completion bootstrap and retirement fix](notes/v0.8.0-fg-input-completion-fix.md) | v0.8.0 P0 输入完成修复及硬件跟进 | [Vulkan FG / FSR 4 / MetalFX](../notes/vulkan-fg-fsr4-metalfx.md) |
| [v0.8.0 FG P3 外部 Agent 交接](notes/v0.8.0-fg-p3-external-agent-handoff.md) | v0.8.0 P3 外部 Agent 交接 | [Vulkan FG / FSR 4 / MetalFX](../notes/vulkan-fg-fsr4-metalfx.md) |
| [v0.8.0 P3 实机诊断记录](notes/v0.8.0-fg-p3-hardware-20260926.md) | v0.8.0 P3 单次实机诊断 | [Vulkan FG / FSR 4 / MetalFX](../notes/vulkan-fg-fsr4-metalfx.md) |
| [P3: exact producer / resolve / video diagnostic handoff](notes/v0.8.0-fg-p3-resolve-handoff.md) | v0.8.0 P3 resolve 诊断交接 | [Vulkan FG / FSR 4 / MetalFX](../notes/vulkan-fg-fsr4-metalfx.md) |
| [v0.8.0 P3 UI replay 诊断](notes/v0.8.0-fg-p3-ui-replay-20260926.md) | v0.8.0 P3 UI replay 单次诊断 | [Vulkan FG / FSR 4 / MetalFX](../notes/vulkan-fg-fsr4-metalfx.md) |
| [v0.8.0 FG SDK 同步归因记录](notes/v0.8.0-fg-sdk-sync-attribution-20260926.md) | v0.8.0 SDK 同步归因记录，已知报告保留为待办 | [当前状态](../STATUS.md) |
| [v0.8.0 P0、P1、P4 开发交接](notes/v0.8.0-p0-p1-p4-development.zh-CN.md) | v0.8.0 P0/P1/P4 开发交接 | [Vulkan FG / FSR 4 / MetalFX](../notes/vulkan-fg-fsr4-metalfx.md) / [路线图](../ROADMAP.md) |
| [v0.8.0 P0退出清理与P4组合连续性：代码补齐](notes/v0.8.0-p0-p4-cleanup-continuity-20260927.md) | v0.8.0 P0 退出清理与 P4 连续性的实现记录 | [Vulkan FG / FSR 4 / MetalFX](../notes/vulkan-fg-fsr4-metalfx.md) |
| [Reusable frame generation development](notes/v0.8.0-reusable-fg-20260927.md) | 可复用 FG 开发的任务说明与验收记录 | [可复用 FG 接入](../notes/reusable-fg-game-integration.md) |

### 超分与运动向量（archive/notes/）

| 文档 | 归档原因 | 当前入口 |
|---|---|---|
| [Full-Game Motion Vector Audit (Phase 0)](notes/full-game-motion-vector-audit.md) | 2026-09 的全游戏运动向量 Phase 0 审计，已被几何回放实现取代 | [运动向量实现](../notes/motion-vector-implementation.md) |
| [Full-Game Motion Vector：持续开发交接与验收计划](notes/full-game-motion-vector-continuation-handoff.md) | 运动向量持续开发交接，工作已完成 | [运动向量实现](../notes/motion-vector-implementation.md) |
| [Full-Game Motion Vector Architecture & Design (Phase 1 & 2)](notes/full-game-motion-vector-design.md) | 运动向量 Phase 1/2 设计草案 | [运动向量实现](../notes/motion-vector-implementation.md) |
| [Full-Game Motion Vector — Next Development Handoff](notes/full-game-motion-vector-next-step-handoff.md) | 运动向量下一步交接，已被取代 | [运动向量实现](../notes/motion-vector-implementation.md) |
| [Issue #64：Hybrid MV修复与验证](notes/issue-64-hybrid-motion-validation.zh-CN.md) | Issue #64 Hybrid MV 修复的一次性验证 | [运动向量实现](../notes/motion-vector-implementation.md) |
| [Motion Vector Implementation Milestone Report (M0 - M4)](notes/motion-vector-milestone-report.md) | M0–M4 里程碑报告，自标“未通过完整验收” | [运动向量实现](../notes/motion-vector-implementation.md) |
| [Motion Vector Vertex Shader Position Dependency Analysis (M1 Report)](notes/motion-vector-vs-position-dependency-report.md) | M1 顶点着色器位置依赖数据报告 | [运动向量实现](../notes/motion-vector-implementation.md) |
| [MV audit repair on the geometric replay baseline](notes/mv-audit-repair.md) | MV 审计修复检查点 | [运动向量实现](../notes/motion-vector-implementation.md) |
| [原生 DLAA 初步实现](notes/native-dlaa-initial.zh-CN.md) | v0.6.11 原生 DLAA 初步实现检查点 | [原生 DLSS 验证](../notes/native-dlss-validation.md) |
| [Native Vulkan DLSS Color Qualification Plan & Implementation Specification](notes/native-dlss-color-qualification-plan.md) | 原生 DLSS 颜色资格规范，已实施 | [原生 DLSS 验证](../notes/native-dlss-validation.md) |
| [原生 Vulkan DLSS Super Resolution 开发与交接指南 (Native DLSS Handoff & Guide)](notes/native-dlss-handoff.zh-CN.md) | 2026-09-21 原生 DLSS 开发交接 | [原生 DLSS 验证](../notes/native-dlss-validation.md) |
| [P2续开发与下一次实机验收（2026-09-21）](notes/native-dlss-p2-next-run.zh-CN.md) | P2 下一次实机验收的准备说明，阶段已完成 | [原生 DLSS 验证](../notes/native-dlss-validation.md) |
| [Native DLSS P2 follow-up — 2026-09-21](notes/native-dlss-p2-progress-2026-09-21.md) | 2026-09-21 P2 进度日志 | [原生 DLSS 验证](../notes/native-dlss-validation.md) |
| [Temporal upscaling feasibility: DLSS, FSR and shared inputs](notes/temporal-upscaling-feasibility.md) | v0.4.0 时期的时域超分可行性研究，DLSS/FSR 已落地 | [原生 DLSS 验证](../notes/native-dlss-validation.md) / [Vulkan FG / FSR 4 / MetalFX](../notes/vulkan-fg-fsr4-metalfx.md) |

### TAA 与 jitter（archive/notes/）

其中单次捕获的映射和天空 jitter 调查已归档；仍被代码注释和过场巡游引用的 jitter 笔记留在活动目录。

| 文档 | 归档原因 | 当前入口 |
|---|---|---|
| [Battle depth writers and the tour material 8d66 — 2026-10-01](notes/jitter-battle-depth-2026-10-01.md) | 单次战斗深度写入 jitter 调查 | [过场巡游 jitter](../notes/jitter-cutscene-tour-2026-10-04.md) |
| [Burning Cave material jitter (f25276) — 2026-10-01](notes/jitter-cave-f25276-2026-10-01.md) | 单次捕获的洞窟材质 jitter 映射 | [jitter 覆盖审查](../notes/jitter-coverage-2026-09-25.md) |
| [Cutscene depth and floor jitter — 2026-10-01](notes/jitter-cutscene-f6814-2026-10-01.md) | 单帧过场深度与地面 jitter 调查 | [过场巡游 jitter](../notes/jitter-cutscene-tour-2026-10-04.md) |
| [Late-pass jitter follow-up — 2026-09-25](notes/jitter-late-pass-followup-2026-09-25.md) | jitter 覆盖审查的后续检查点 | [jitter 覆盖审查](../notes/jitter-coverage-2026-09-25.md) |
| [Old Sorceress' Mansion sky jitter (#121) — 2026-10-01](notes/jitter-sky-121-2026-10-01.md) | 单次天空 jitter 调查（#121） | — |
| [Sea of Baus battle sky jitter (#203) — 2026-10-04](notes/jitter-sky-203-2026-10-04.md) | 单次天空 jitter 调查（#203） | — |
| [Ice Canyon sky jitter (f12139) — 2026-10-01](notes/jitter-sky-f12139-2026-10-01.md) | 单次捕获的冰峡天空映射 | — |
| [Sky pairs and a depth VS from the map tour — 2026-10-01](notes/jitter-sky-tour-2026-10-01.md) | 地图巡游的天空配对快照 | [过场巡游 jitter](../notes/jitter-cutscene-tour-2026-10-04.md) |
| [TAA bloom prefilter candidate (2026-09-12)](notes/taa-bloom-prefilter.md) | v0.5.7 已修复的泛光闪烁，逐候选时间线 | [Bell 闪烁调查](../notes/TAA_BELL_FLICKER_INVESTIGATION.md) |
| [TAA shader 覆盖审查](notes/taa-coverage-audit.md) | 2026-09 的 TAA 着色器覆盖审查快照 | [jitter 覆盖审查](../notes/jitter-coverage-2026-09-25.md) |
| [Current-scene TAA coverage and static discovery — 2026-09-13](notes/taa-current-scene-2026-09-13.md) | 单场景 TAA 覆盖与静态发现 | [jitter 覆盖审查](../notes/jitter-coverage-2026-09-25.md) |
| [TAA jitter mapping for captures f5912 and f16385 — 2026-09-13](notes/taa-f5912-f16385-2026-09-13.md) | 单次捕获 f5912／f16385 的 jitter 映射 | [jitter 覆盖审查](../notes/jitter-coverage-2026-09-25.md) |
| [TAA jitter mapping for capture f5997 — 2026-09-13](notes/taa-f5997-2026-09-13.md) | 单次捕获 f5997 的 jitter 映射 | [jitter 覆盖审查](../notes/jitter-coverage-2026-09-25.md) |
| [从日志与 render state 定位 TAA 顶点路径遗漏](notes/taa-log-only-triage-2026-09-09.md) | 从日志定位顶点路径遗漏的一次性分诊 | [jitter 覆盖审查](../notes/jitter-coverage-2026-09-25.md) |
| [TAA exit under Proton on AMD (#200) — 2026-10-04](notes/taa-proton-exit-200-2026-10-04.md) | Issue #200 已修复，二分排查时间线 | — |

### GPU 渲染与画面问题（archive/notes/）

| 文档 | 归档原因 | 当前入口 |
|---|---|---|
| [Radeon 8060S 偏黑画面调试交接](notes/amd-8060s-dark-render-handoff.md) | Radeon 8060S 偏黑画面的调试交接，原因已定位并修复 | [amd-resolve-initialization.md（已归档）](notes/amd-resolve-initialization.md) |
| [AMD resolve initialization](notes/amd-resolve-initialization.md) | 已修复的 AMD resolve 初始化问题及验证日志 | — |
| [随机遇敌动画与战斗停滞调查（2026-09-04，主角与战斗停滞已修复，敌人待查）](notes/encounter-animation.md) | 随机遇敌动画与战斗停滞调查，问题已修复 | — |
| [4K Vulkan shadow-loop GPU audit — 2026-09-12](notes/gpu-shadow-loop-audit-2026-09-12.md) | 4K 阴影循环 GPU 审计，已修复 | — |
| [GPU 当前说明（2026-09-05）](notes/gpu.md) | 2026-09-05 GPU 说明：Vulkan 未验证、遗留问题清单均已过期 | [遮挡查询](../notes/occlusion-queries.md) / [当前状态](../STATUS.md) |
| [Kaim body-shadow flicker: v0.4.0 investigation](notes/kaim-body-shadow-v040.md) | v0.4.0 凯姆身体阴影闪烁报告（已搁置，无根因） | [阴影与 AO](../notes/shadow-ambient-occlusion.md) |
| [光照 / 阴影续修（2026-09-04）](notes/lighting-stencil-depth-clear.md) | 2026-09-04 模板测试与 D24 清除的修复叙述 | — |
| [Map12 poster black-patch repair — 2026-09-05](notes/map12-poster-depth.md) | 单张地图海报黑块修复 | — |
| [Polygon offset 接入与阴影回归（2026-09-05）](notes/polygon-offset.md) | 2026-09-05 polygon offset 接入的验证叙述 | — |
| [ReBlue vs Lost Odyssey GPU 对照 — 2026-09-11](notes/reblue-gpu-comparison.md) | 与 ReBlue 提交模型的对照，所推动的环缓冲工作已完成 | — |
| [角色破面与后期重影调查（2026-09-04）](notes/rendering-index-and-resolve.md) | 索引对齐、后期尺寸与 gamma/swizzle 的修复叙述 | — |
| [Startup shader coverage follow-up — 2026-09-13](notes/shader-startup-coverage-2026-09-13.md) | 单次启动着色器覆盖修复，已发布 | [Shader 准备](../notes/shader-preparation.md) |
| [阴影纹理采样 LOD（2026-09-05）](notes/shadow-texture-lod.md) | v0.4.x 阴影／TAA 报告的时间线 | [阴影与 AO](../notes/shadow-ambient-occlusion.md) |
| [启动时深度清除导致驱动退出（2026-09-05）](notes/startup-depth-clear-crash.md) | 启动深度清除导致驱动退出，已修复 | — |
| [Xenia 实机画面对照（2026-09-04）](notes/xenia-render-comparison.md) | 2026-09-04 与 Xenia 的一次性画面对照 | — |

### 性能与帧率（archive/notes/）

| 文档 | 归档原因 | 当前入口 |
|---|---|---|
| [Uhra City CPU Optimization Results — 2026-09-18](notes/PERF_CITY_UHRA_RESULTS.md) | 2026-09-18 乌斯拉城 CPU 优化单次实测，已被后续测量取代 | [CPU 性能优化指南](../notes/cpu-performance-optimization-guide.md) |
| [乌斯拉城 CPU 优化实测报告 — 2026-09-18](notes/PERF_CITY_UHRA_RESULTS.zh-CN.md) | 上篇的中文版 | [CPU 性能优化指南](../notes/cpu-performance-optimization-guide.md) |
| [Assembly profiler gameplay captures — 2026-09-11](notes/asm-profiler-gameplay.md) | v0.5.4 汇编采样快照 | [CPU 性能优化指南](../notes/cpu-performance-optimization-guide.md) |
| [City 60 FPS handoff — 2026-09-12](notes/city-60fps-handoff.md) | 0.5.4／0.5.6 的 60 FPS 交接 | [CPU 性能优化指南](../notes/cpu-performance-optimization-guide.md) |
| [Card A city measurement — published v0.5.11 — 2026-09-14](notes/cpu-card-a-city-2026-09-14.md) | v0.5.11 城市基线测量卡 A | [CPU 性能优化指南](../notes/cpu-performance-optimization-guide.md) |
| [Card B city measurement — published v0.5.11 — 2026-09-14](notes/cpu-card-b-city-2026-09-14.md) | v0.5.11 城市测量卡 B | [CPU 性能优化指南](../notes/cpu-performance-optimization-guide.md) |
| [Card C prepare gate — published v0.5.11 — 2026-09-14](notes/cpu-card-c-prepare-gate-2026-09-14.md) | v0.5.11 prepare gate 测量卡 C（结论：不实施） | [CPU 性能优化指南](../notes/cpu-performance-optimization-guide.md) |
| [Card D 3C6T city measurement — published v0.5.11 — 2026-09-14](notes/cpu-card-d-3c6t-city-2026-09-14.md) | v0.5.11 3C6T 亲和性实验卡 D | [CPU 性能优化指南](../notes/cpu-performance-optimization-guide.md) |
| [Live v0.5.7 CPU and 4K profiling — 2026-09-13](notes/cpu-live-profile-2026-09-13.md) | v0.5.7／0.5.8 采样快照，热点已修复 | [CPU 性能优化指南](../notes/cpu-performance-optimization-guide.md) |
| [CPU recompilation deep-dive — 2026-09-13](notes/cpu-recomp-deep-2026-09-13.md) | 0.5.8 CPU 重编译诊断 | [CPU 性能优化指南](../notes/cpu-performance-optimization-guide.md) |
| [性能分析完整报告 — 2026-09-11](notes/perf-complete-analysis.md) | v0.5.4 离线性能分析 | [CPU 性能优化指南](../notes/cpu-performance-optimization-guide.md) |
| [GPU 环缓冲实测对比 — 2026-09-11](notes/perf-gpu-ring-compare.md) | v0.5.4 与环缓冲分支的单次对比 | [CPU 性能优化指南](../notes/cpu-performance-optimization-guide.md) |
| [psvita 乌斯拉城市性能对比（发布素材草稿）](notes/psvita-performance-comparison-2026-09-18.md) | 发布素材草稿，所依据的两次快照已被取代 | [画质选项开销](../notes/graphics-options-cost-2026-10-10.zh-CN.md) |
| [v0.5.0 性能诊断记录](notes/v0.5.0-performance-diagnosis-2026-09-09.md) | v0.5.6／0.5.7 版本性能诊断 | [CPU 性能优化指南](../notes/cpu-performance-optimization-guide.md) |
| [v0.5.0 渲染、性能与发布交接 — 2026-09-09](notes/v0.5.0-rendering-handoff-2026-09-09.md) | v0.5.0 渲染、性能与发布交接 | [CPU 性能优化指南](../notes/cpu-performance-optimization-guide.md) |
| [Vulkan depth-clear performance — 2026-09-13](notes/vulkan-depth-clear-performance-2026-09-13.md) | 单次 4K Vulkan depth-clear A/B | [CPU 性能优化指南](../notes/cpu-performance-optimization-guide.md) |

### 运行时、存档与问题调查（archive/notes/）

| 文档 | 归档原因 | 当前入口 |
|---|---|---|
| [Issue #53 Disc 2 loading investigation](notes/ISSUE_53_DISC2_HANG_FIX_REPORT.md) | Issue #53 Disc 2 加载卡住的验证报告，根因未确认 | — |
| [戒指战斗资源名缺少语言后缀（2026-09-04）](notes/battle-ring-resource.md) | 戒指战斗资源名后缀问题的逐轮日志，已修复 | — |
| [DLC 目录枚举与 `FindNext` pattern 诊断](notes/dlc-directory-enumeration.md) | DLC 目录枚举 FindNext 问题，已修复 | — |
| [Issue #12: funeral flower hand-in investigation](notes/issue12-funeral-crash.md) | Issue #12 交花崩溃的调查日志 | [Issue #12 根因](notes/issue12-root-cause.md) |
| [Issue #12 交接（2026-09-09）：根因已定位，生产交花路径已通过](notes/issue12-handoff-2026-09-09.md) | Issue #12 的交接，修复已完成 | [Issue #12 根因](notes/issue12-root-cause.md) |
| [Issue #7: cutscene closure and missing crash diagnostics](notes/issue7-cutscene-crash.md) | Issue #7 过场崩溃及发布时间线 | [重编译宽度审计](../notes/recompiler-width-audit.md) |
| [Issues #54 / #55: audio follow-up and validation](notes/issue-54-55-audio-followup.md) | Issue #54／#55 音频诊断后续，自述未确立修复 | [音频输出](../notes/audio-output.md) |
| [Issue #70 DX12/Vulkan optimization](notes/issue-70-dx12-vulkan-optimization.md) | v0.7.3 版本的 DX12／Vulkan 优化快照 | — |
| [Issue #14–#16 triage — 2026-09-12](notes/issues14-16-triage.md) | Issue #14–#16 的分诊，均已关闭 | — |
| [内核 HLE 当前说明（2026-09-05）](notes/kernel.md) | 2026-09-05 内核 HLE 说明：语言、音频等描述已过期 | [存档](notes/save-storage.md) / [音频输出](../notes/audio-output.md) / [XEX](../notes/xex.md) |
| [Unicode startup and save paths (2026-09-06)](notes/save-path-unicode.md) | Issue #4 启动与存档路径的 Unicode 问题，已修复 | [存档](notes/save-storage.md) |
| [第三地图卡住调查（2026-09-04）](notes/third-map-hang.md) | 2026-09-04 第三地图卡住调查，看门狗与日志已进入代码 | — |
| [窗口消息停摆：独立事件线程](notes/window-event-pump.md) | 窗口消息停摆的修复，事件线程已落地 | — |
| [攻略路线与后台推进测试（2026-09-04）](notes/walkthrough-testing.md) | 2026-09-04 攻略路线测试计划 | — |
| [跨版本文本补充补丁（已挂起）](notes/text-language-patch.md) | 跨版本文本补丁提案（已挂起），被语言包研究取代 | [语言包研究](../notes/language-pack-research.zh-CN.md) |
| [Desktop UI modernization](notes/desktop-ui-modernization.md) | v0.5.0 的 Tk 安装器界面现代化，安装器已移除 | [发布打包](../notes/release-packaging.md) |

### 版本、计划、平台与交接（archive/notes/）

| 文档 | 归档原因 | 当前入口 |
|---|---|---|
| [v0.4.0 后续开发交接（2026-09-07）](notes/handoff-v0.4.0-followup.md) | v0.4.0 后续开发交接 | [当前状态](../STATUS.md) |
| [接手入口](notes/handoff.md) | 2026-09-05 至 09-07 的接手记录，页内自述不是当前指令 | [当前状态](../STATUS.md) / [路线图](../ROADMAP.md) |
| [Bug report：v0.6.7 之后的代码审计](notes/post-v0.6.7-bug-report.zh-CN.md) | v0.6.7 之后的代码审计 | [当前状态](../STATUS.md) |
| [改进建议：v0.6.7 之后的渲染与设置界面](notes/post-v0.6.7-improvement-proposals.zh-CN.md) | v0.6.7 之后的改进建议，条目已实施 | [设置菜单](../notes/settings-menu.md) |
| [PR #59、#60、#68 处理记录](notes/pr-59-60-68-triage.zh-CN.md) | PR #59／#60／#68 的处理记录 | — |
| [v0.2.2 release notes / 发布说明](notes/release-0.2.2.md) | v0.2.2 发布说明 | [更新日志](../../CHANGELOG.md) |
| [Lost Odyssey Recomp v0.6.0 — This one means a lot to me](notes/release-0.6.0-post-draft.md) | v0.6.0 发布帖草稿 | [更新日志](../../CHANGELOG.md) |
| [v0.4.0 development scope](notes/v0.4.0-development.md) | v0.4.0 范围与检查点 | [更新日志](../../CHANGELOG.md) |
| [v0.5.0 development history](notes/v0.5.0-development.md) | v0.5.0 开发历史 | [更新日志](../../CHANGELOG.md) |
| [v0.5.0 QOL 需求与未来验收标准](notes/v0.5.0-qol-requirements.md) | v0.5.0 QOL 需求，已交付 | [更新日志](../../CHANGELOG.md) |
| [Linux 移植评估与首可玩裁定（2026-09-13）](notes/linux-port-evaluation-2026-09-13.md) | Linux 移植评估与首可玩裁定，已被实际移植取代 | [构建指南](../BUILDING.md) |
| [Switch 移植评估与 PC Vulkan 后端交接（2026-09-07）](notes/switch-vulkan-handoff.md) | Switch 移植评估与 PC Vulkan 后端交接，后端已完成 | [当前状态](../STATUS.md) / [路线图](../ROADMAP.md) |
| [Android ARM64 移植研究（2026-10-02）](notes/android-port-research-2026-10-02.md) | Android 移植初始研究（自述历史基线），APK 已有 CI | [Android 打包说明](../../packaging/android/README.md) / [Android DXC 构建](../notes/android-dxc-build-2026-10-02.md) |
| [android-probe-tb321fu-2026-10-02.txt](notes/android-probe-tb321fu-2026-10-02.txt) | 上篇引用的设备探针原始数据 | [Android 打包说明](../../packaging/android/README.md) |
| [Developer-tool recovery record — 2026-09-24](notes/developer-tool-recovery-2026-09-24.md) | 2026-09-24 开发工具恢复清单，迁移已完成 | [工具目录](../../tools/README.md) |

## 2026-10-14 归档的文档

2026-10-14 整理（v0.9.52 之后）：下列文档的主题已完成、已被现行文档取代，或其中的说明只对应原日期。它们用 `git mv` 移入，文首加了归档提示；指向它们的链接已改指这里。

| 文档 | 归档原因 | 当前入口 |
|---|---|---|
| [MetalFX Temporal 设计](METALFX_TEMPORAL_DESIGN.md) | 2026-09-29 阶段的设计草稿，测量来自 MikeRavenelle 分支，不是本集成复测 | [Vulkan 帧生成、FSR 4 与 MetalFX](../notes/vulkan-fg-fsr4-metalfx.md) |
| [过场巡游报告](CUTSCENE_TOUR.md) / [中文](CUTSCENE_TOUR.zh-CN.md) | 2026-10-04 至 05 一次三机过场巡游的结果，不代表当前画面 | [巡游工具](../../tools/tours/README.md) / [过场 jitter 审查](../notes/jitter-cutscene-tour-2026-10-04.md) |
| [x86-64 与 AArch64 行为差异](ARM64_NOTES.md) | 早期 AArch64 差异清单，记录来自 MikeRavenelle 分支，不是本集成的复测 | [Linux AArch64 构建](../LINUX_ARM64.md) / [当前状态](../STATUS.md) |
| [Battle camera spin at 90/120 FPS (#117)](notes/battle-camera-120fps-2026-10-01.md) | #117 的调查记录，修复已随 v0.7.35 发布 | [原生 90/120 FPS](../notes/native-90-120fps.md) |
| [Battle dialogue timing at 90/120 FPS (#148)](notes/issue-148-battle-dialogue-timing.md) | #148 的调查记录，修复已随 v0.8.5 发布 | [原生 90/120 FPS](../notes/native-90-120fps.md) |
| [Screen-sampling candidate batch — 2026-09-25](notes/jitter-screen-batch-2026-09-25.md) | 屏幕采样候选批次的试验检查点 | [jitter 覆盖审查](../notes/jitter-coverage-2026-09-25.md) |
| [运行时纹理导入（含 4× 放大）：设计与可行性](notes/texture-import-research.zh-CN.md) | 2026-10-09 的设计与可行性研究，相关功能已有现行 Mod 文档 | [Mod 指南（4× 纹理包）](../wiki/Modding.md#4x-hd-texture-pack) / [运行时纹理替换](../wiki/Runtime-Texture-Replacement.md) |
| [Project review fixes — 2026-09-30](notes/PROJECT_REVIEW_FIXES_20260930.md) | 2026-09-30 项目审查的 13 项修复记录，修复已随 v0.7.25 发布 | [当前状态](../STATUS.md) / [测试目录](../../tools/tests/README.md) |
| [Settings menu interaction and redraw — 2026-09-24](notes/settings-menu-interaction.md) | 2026-09-24 设置菜单交互与重绘修复的检查点 | [设置菜单与显示选项](../notes/settings-menu.md) |
| [Issue #12 root cause: garbage-collected materials drawn by a stale scene proxy](notes/issue12-root-cause.md) | 已修复缺陷的根因报告（英文），保留原时点证据 | [当前状态](../STATUS.md) |
| [角色黑色剪影：物理地址别名与遮挡查询（2026-09-04）](notes/physical-alias-rendering.md) | 已修复缺陷的历史记录 | [当前状态](../STATUS.md) |
| [战后演出白屏与 EDRAM 格式切换（2026-09-04）](notes/post-battle-whiteout.md) | 已修复缺陷的历史记录 | [当前状态](../STATUS.md) |
| [手动存档失败（2026-09-04）](notes/save-storage.md) | 手动存档问题的逐轮取证记录，部分细节仍是原时点状态 | [当前状态](../STATUS.md) |
| [标题动态背景恢复（2026-09-04）](notes/title-packed-mips.md) | 已修复缺陷的历史记录 | [当前状态](../STATUS.md) |

各快照保留其记录日期和当时内容，归档提示与相对链接按当前位置调整；新结论写入当前文档或专项笔记。
