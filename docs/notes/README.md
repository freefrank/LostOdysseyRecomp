# 逆向与验证笔记

[文档总入口](../README.md) · [当前状态](../STATUS.md) · [路线图](../ROADMAP.zh-CN.md) · [历史归档](../archive/README.md)

2026-10-14 整理：已修复、已被现行文档取代或只对应原日期的 6 篇笔记移入[历史归档](../archive/README.md)，索引中不再列出。

2026-10-11 整理：已过期的交接、阶段记录、修复时间线和单次测量已移入[历史归档](../archive/README.md)的 `archive/notes/`，本目录现收录 **60 篇**专项笔记，按主题列出，每篇只出现一次。文件名和原始证据继续保留，目录不重复维护版本发布或任务进度。

## 如何使用

| 标记 | 阅读方式 |
|---|---|
| 参考 | 维护中的专题机制或操作说明；其中旧测试仍只覆盖原版本与条件。 |
| 历史 | 日期、版本或阶段绑定的测量、修复、交接与进度；其中“当前”“未发布”、PID 和后续步骤只属于当时。 |
| 调查 | 问题分析及其证据边界；此标记不等于对应 Issue 仍开放，现状以 STATUS / 路线图为准。 |
| 草案 | 设计、研究、需求或发布文案；不代表已实施，也不因年代久远自动取消。 |

分类合计：参考 33、历史 15、调查 10、草案 2。

先用当前状态确认实施、验证、验收和发布，再从下方进入证据。旧命令执行前核对源码、工具及输入；`out/`、本机绝对路径和截图编号是原调查的定位信息，不随仓库分发。已缺失的本地产物标为历史路径，不能视作本次复验。

## 常用专题入口

[FG 接入参考](reusable-fg-game-integration.md) · [设置菜单](settings-menu.md) · [渲染捕获](render-state-capture.md) · [Shader 准备](shader-preparation.md) · [首次使用管线卡顿方案](pipeline-first-use-stalls.md) · [CPU 优化指南](cpu-performance-optimization-guide.md) · [发布打包](release-packaging.md) · [随时存档限制](issue-74-save-anywhere-party-split.md)

## 帧生成与 SDK 接入

| 文档 | 类型 |
|---|---|
| [Vulkan DLSS/FSR FG、MetalFX 实现与 FSR 4 调查](vulkan-fg-fsr4-metalfx.md) | 调查 |
| [Reusable FG game integration](reusable-fg-game-integration.md) | 参考 |

## 超分与运动向量

| 文档 | 类型 |
|---|---|
| [Geometric motion replay: implementation and validation boundary](motion-vector-implementation.md) | 参考 |
| [Native DLSS Validation (P0 Foundation, P1 Temporal Inputs & P2 Checkpoint)](native-dlss-validation.md) | 历史 |
| [DLSS 5 NR 接入研究](dlss5-nr-research.zh-CN.md) | 调查 |

## TAA 与 jitter

| 文档 | 类型 |
|---|---|
| [Cutscene tour and 45 reviewed jitter pairs — 2026-10-04](jitter-cutscene-tour-2026-10-04.md) | 历史 |
| [Mansion battle lights and a two-view cutscene (#212) — 2026-10-04](jitter-212-2026-10-04.md) | 历史 |
| [Temporal jitter coverage review — 2026-09-25](jitter-coverage-2026-09-25.md) | 历史 |
| [TAA Bell-Stand Flicker Investigation (Issue #46 adjacent)](TAA_BELL_FLICKER_INVESTIGATION.md) | 调查 |
| [TAA 铃铛架闪烁排查（Issue #46 相关）](TAA_BELL_FLICKER_INVESTIGATION.zh-CN.md) | 调查 |

## GPU 渲染与画面问题

| 文档 | 类型 |
|---|---|
| [Display gamma ramp (#78, #179) — 2026-10-04](display-gamma-179-2026-10-04.md) | 历史 |
| [Brightness / Gamma page — 2026-10-04](brightness-gamma-2026-10-04.md) | 参考 |
| [Live anisotropic filtering](anisotropic-filtering.md) | 参考 |
| [Fire-hit lighting investigation (2026-09-05)](fire-hit-rendering.md) | 调查 |
| [Experimental HDR output](hdr-output.md) | 参考 |
| [Linux/Vulkan HDR research — 2026-10-02](linux-vulkan-hdr.md) | 调查 |
| [Host GPU occlusion queries (#118)](occlusion-queries.md) | 参考 |
| [角色黑色剪影：物理地址别名与遮挡查询（2026-09-04）](../archive/notes/physical-alias-rendering.md) | 历史 |
| [战后演出白屏与 EDRAM 格式切换（2026-09-04）](../archive/notes/post-battle-whiteout.md) | 历史 |
| [Debug Menu render-state capture](render-state-capture.md) | 参考 |
| [渲染验证（2026-09-04）](rendering-validation.md) | 历史 |
| [Tall and non-standard aspect layout — 2026-10-01](tall-aspect-layout.md) | 参考 |
| [标题动态背景恢复（2026-09-04）](../archive/notes/title-packed-mips.md) | 历史 |
| [Shadow resolution and screen-space ambient occlusion](shadow-ambient-occlusion.md) | 参考 |

## 性能与帧率

| 文档 | 类型 |
|---|---|
| [CPU 性能优化指南：3C6T 预算、可行并行与反模式](cpu-performance-optimization-guide.md) | 参考 |
| [原生90/120 FPS：实现与本地验收](native-90-120fps.md) | 参考 |
| [FreeSync / G-SYNC Compatible：应用侧VRR接入](vrr-freesync-gsync-compatible.md) | 参考 |
| [画质增强选项的开销：v0.9.22，RTX 5080，2026-10-10](graphics-options-cost-2026-10-10.zh-CN.md) | 参考 |
| [画质选项的开销（1080p 输出）：main f7799aec，RTX 5080 与 psvita，2026-10-15](graphics-options-cost-1080p-2026-10-15.zh-CN.md) | 参考 |
| [合并 render pass：v1.0.0 设计调研](render-pass-merging-design-2026-10-10.zh-CN.md) | 草案 |
| [合并 render pass 阶段 0：平板实测](render-pass-merging-phase0-2026-10-10.zh-CN.md) | 调查 |

## 着色器准备与覆盖

| 文档 | 类型 |
|---|---|
| [启动着色器准备（2026-09-05）](shader-preparation.md) | 参考 |
| [One-file shader store and the 2026-10-02 packs](shader-store-2026-10-02.md) | 参考 |
| [Built-in shader resource index](shader-resource-index.md) | 参考 |
| [首次使用管线的卡顿：方案（2026-10-05）](pipeline-first-use-stalls.md) | 参考 |

## 界面、输入与调试

| 文档 | 类型 |
|---|---|
| [Built-in cheats and LT speed control](cheats.md) | 参考 |
| [Multiple controllers and keyboard input](controller-input.md) | 参考 |
| [Debug 地图 ID 和本地化名称（2026-09-05）](debug-map-info.md) | 参考 |
| [Debug 人物传送：逆向依据与验证边界（2026-09-04）](debug-teleport.md) | 历史 |
| [Issue #40 UI Resource Map and Asset Archive](issue-40-ui-resource-map.md) | 历史 |
| [Issue #40 UI 资源定位与资产映射归档](issue-40-ui-resource-map.zh-CN.md) | 历史 |
| [Original Settings menu assets](menu-original-assets.md) | 参考 |
| [设置菜单与显示选项](settings-menu.md) | 参考 |

## 运行时、存档与游戏流程

| 文档 | 类型 |
|---|---|
| [音频输出与对白修复（2026-09-05）](audio-output.md) | 历史 |
| [Guest critical-section byte order](critical-section-endian.md) | 参考 |
| [Automatic selection of imported discs](disc-selection.md) | 参考 |
| [USA/Europe edition support](europe-support.md) | 参考 |
| [Issue #74：分队期间随时存档后无法换人（2026-09-29 调查）](issue-74-save-anywhere-party-split.md) | 调查 |
| [Issue #12 root cause: garbage-collected materials drawn by a stale scene proxy](../archive/notes/issue12-root-cause.md) | 历史 |
| [Issue #114: field interaction during a pending battle request](ISSUE_114_FIELD_INTERACTION_20261001.md) | 调查 |
| [重编译配置笔记](recomp.md) | 参考 |
| [Recompiler width and control-flow audit](recompiler-width-audit.md) | 参考 |
| [随时存档：菜单权限与验证](save-anywhere.md) | 参考 |
| [手动存档失败（2026-09-04）](../archive/notes/save-storage.md) | 历史 |
| [游戏数据来源与 XEX](xex.md) | 参考 |

## Mod、语言包与资源替换

| 文档 | 类型 |
|---|---|
| [高分辨率 / 高帧率 CG 替换：可行性研究](hd-cutscene-replacement.zh-CN.md) | 调查 |
| [语言包（#369）：导出、导入、新增语言的设计与可行性](language-pack-research.zh-CN.md) | 草案 |

## 平台与开发工具

| 文档 | 类型 |
|---|---|
| [GitHub Actions checks and releases](ci.md) | 参考 |
| [Android ARM64 DXC 构建（2026-10-02）](android-dxc-build-2026-10-02.md) | 参考 |

## 分诊与发布

| 文档 | 类型 |
|---|---|
| [Automated issue triage](issue-triage.md) | 参考 |
| [Installer and Windows release pipeline](release-packaging.md) | 参考 |

## 相关记录

[文档整理记录](../archive/audits/documentation-2026-09-30.md)说明本次来源、状态修正和 Basic Memory 知识整理；[归档目录](../archive/README.md)保存已被替代的全局账本和已过期的笔记。过期笔记用 `git mv` 移到 `archive/notes/`，同时修复指向它的链接，不留跳转页。新增笔记应补入本索引，并给出适用版本、证据范围和现行入口。
