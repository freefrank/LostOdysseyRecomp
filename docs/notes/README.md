# 逆向与验证笔记

先读[当前状态](../STATUS.md)和[路线图](../ROADMAP.zh-CN.md)。[旧交接](handoff.md)和日期化实验保留当时证据，不自动代表当前功能支持。部分过期 GPU、内核和交接记录已移到[归档](../archive/README.md)。

当前进度：[项目状态](../STATUS.md) · [图形后端路线图](../ROADMAP.zh-CN.md#v050-pc-graphics) · [Linux／Steam Deck 评估](linux-port-evaluation-2026-09-13.md)。历史汇总：[2026-09-05成果与交接](../WORK_REPORT_2026-09-05.md)。Issue #53 当前记录：[Disc 2 加载调查与防御性修正](ISSUE_53_DISC2_HANG_FIX_REPORT.md)。

## FSR / DLSS FG 记录

- [当前 P2 交接](fsr-dlss-fg-codex-handoff.zh-CN.md)：现行实现、验证边界、证据路径和接续步骤。
- [Codex 进度日志](fsr-dlss-fg-codex-progress.zh-CN.md)：保留日期化历史结果；其中旧“当前状态”不代表现状。
- [Codex 历史交接](fsr-dlss-fg-codex-history.zh-CN.md)：保留早期暂停 checkpoint 和后续历史过程。
- [OpenCode P0 handoff](fsr-dlss-fg-handoff.zh-CN.md)：早期阶段快照。
- [导入需求与阶段映射](fsr-dlss-fg-imported-tasks.zh-CN.md)：需求来源与历史项目映射；进度字段已过期。

## 现有文档

| 专项笔记 |
|---|
| [v0.6.0 发布前审计与修复状态（2026-09-18；未发布，运行时与真实更新验证仍有边界）](../audits/0.6.0-prerelease.md) |
| [当前场景 TAA jitter 覆盖与静态 shader discovery（2026-09-13；候选，未验收）](taa-current-scene-2026-09-13.md) |
| [Issue #7：议会崩溃、原生保存／读档与日志诊断](issue7-cutscene-crash.md) |
| [PowerPC 位宽／控制流审查与九类语义修复](recompiler-width-audit.md) |
| [Switch 评估与 v0.5.0 PC Vulkan／DX11 历史交接（2026-09-07；当前后端状态见 STATUS）](switch-vulkan-handoff.md) |
| [Linux／Steam Deck 首可玩评估与当前状态（2026-09-13—14；WSL 首可玩已验证，原生 GPU／Steam Deck 未验证）](linux-port-evaluation-2026-09-13.md) |
| [v0.5.0 QOL 历史需求与验收标准（当前进度见 STATUS）](v0.5.0-qol-requirements.md) |
| [v0.4.0 开发历史与验证边界](v0.4.0-development.md) |
| [v0.4.0 后续实现、正式包与验收范围](handoff-v0.4.0-followup.md) |
| [凯姆首战身体阴影 v0.4.0 复查（2026-09-07 用户挂起，未确认复现）](kaim-body-shadow-v040.md) |
| [DLSS/FSR 时域超分研究：官方契约、当前证据与后续接入](temporal-upscaling-feasibility.md) |
| [v0.7.0 DLSS-G 与 FSR FG 开发计划（D3D12／Vulkan，规划中）](v0.7.0-frame-generation-plan.md) |
| [多手柄与键盘输入（v0.2.1 已发布；IME 问题待修）](controller-input.md) |
| [Debug Menu 渲染状态捕获、后台 ZIP 与日志保留](render-state-capture.md) |
| [两套零售版本兼容、语言与发布验证](europe-support.md) |
| [自动读取已导入盘与原版管理器验证](disc-selection.md) |
| [Windows 发布打包与 CI](release-packaging.md) |
| [文本语言互补补丁研究（2026-09-06 用户暂停，无成品）](text-language-patch.md) |
| [Map12 海报黑斑与深度偏移](map12-poster-depth.md) |
| [窗口事件线程与无响应修正](window-event-pump.md) |
| [启动深度清除崩溃](startup-depth-clear-crash.md) |
| [设置菜单、语言与显示选项（2026-09-05）](settings-menu.md) |
| [首次启动着色器准备与覆盖边界（2026-09-05）](shader-preparation.md) |
| [音频输出与对白修复（2026-09-05）](audio-output.md) |
| [戒指战斗资源名缺少语言后缀（2026-09-04）](battle-ring-resource.md) |
| [临界区大小端与线程死锁（2026-09-05）](critical-section-endian.md) |
| [Debug 地图 ID 和本地化名称（2026-09-05）](debug-map-info.md) |
| [Debug 人物传送：逆向依据与验证边界（2026-09-04）](debug-teleport.md) |
| [随机遇敌动画与战斗停滞调查（2026-09-04，主角与战斗停滞已修复，敌人待查）](encounter-animation.md) |
| [GPU 说明（2026-09-05 快照）](gpu.md) |
| [接手入口（历史交接，当前状态以 STATUS 为准）](handoff.md) |
| [内核 HLE 说明（2026-09-05 快照）](kernel.md) |
| [光照 / 阴影续修（2026-09-04）](lighting-stencil-depth-clear.md) |
| [火焰受击亮暗帧与灯光参数（2026-09-05）](fire-hit-rendering.md) |
| [Polygon offset 接入与验证边界（2026-09-05）](polygon-offset.md) |
| [阴影纹理零 LOD 翻译（2026-09-05）](shadow-texture-lod.md) |
| [角色黑色剪影：物理地址别名与遮挡查询（2026-09-04）](physical-alias-rendering.md) |
| [战后演出白屏与 EDRAM 格式切换（2026-09-04）](post-battle-whiteout.md) |
| [重编译配置笔记](recomp.md) |
| [角色破面与后期重影调查（2026-09-04）](rendering-index-and-resolve.md) |
| [渲染验证（2026-09-04）](rendering-validation.md) |
| [手动存档失败（2026-09-04）](save-storage.md) |
| [随时存档开关与后台回归（2026-09-05）](save-anywhere.md) |
| [第三地图卡住调查（2026-09-04）](third-map-hang.md) |
| [标题动态背景恢复（2026-09-04）](title-packed-mips.md) |
| [攻略路线与后台推进测试（2026-09-04）](walkthrough-testing.md) |
| [Xenia 实机画面对照（2026-09-04）](xenia-render-comparison.md) |
| [性能分析完整报告（2026-09-11；诊断，未改运行时）](perf-complete-analysis.md) |
| [CPU 重编译深度诊断（2026-09-13；历史诊断，未改运行时）](cpu-recomp-deep-2026-09-13.md) |
| [CPU 性能优化指南：3C6T 预算、可行并行与反模式（2026-09-14；指南，未实施运行时；Card A/B/C/D 已测）](cpu-performance-optimization-guide.md) |
| [Card A city measurement（2026-09-14；published v0.5.11，无耗尽资源）](cpu-card-a-city-2026-09-14.md) |
| [Card B city measurement（2026-09-14；published v0.5.11，B1/B2/B3 不实施）](cpu-card-b-city-2026-09-14.md) |
| [Card C prepare gate（2026-09-14；published v0.5.11，不实施 Parallel Prepare）](cpu-card-c-prepare-gate-2026-09-14.md) |
| [Card D 3C6T city measurement（2026-09-14；published v0.5.11，默认不钉核）](cpu-card-d-3c6t-city-2026-09-14.md) |
| [GPU 环缓冲实测对比（2026-09-11；user01 城市，非验收）](perf-gpu-ring-compare.md) |
| [Vulkan depth-clear performance (2026-09-13; bounded 4K comparison)](vulkan-depth-clear-performance-2026-09-13.md) |
| [4K Vulkan shadow loop GPU 审查（2026-09-12；实施、测试与实景边界）](gpu-shadow-loop-audit-2026-09-12.md) |
| [TAA bloom prefilter 候选与 geometry trace（2026-09-12；部分改善，实景验证待完成）](taa-bloom-prefilter.md) |
| [ReBlue vs Lost Odyssey GPU 对照（2026-09-11）](reblue-gpu-comparison.md) |
| [游戏数据来源与 XEX](xex.md) |

记录地址范围、还原结构、可重复验证和未决问题；不要创建只有规划名称、没有实际内容的索引项。
