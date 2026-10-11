# 文档与知识库整理记录 — 2026-09-30

[文档入口](../README.md) · [当前状态](../STATUS.md) · [路线图](../ROADMAP.zh-CN.md) · [Project](https://github.com/users/freefrank/projects/3)

本次检查仓库原有的 223 份第一方 Markdown 文档，包括根目录、`docs/`、工具、打包与工作流说明；其中 `docs/notes/` 有 144 篇正文。第三方依赖和子模块文档由上游维护，生成的 `out/` 不进入文档索引。当前结论以源码、已记录的验证与验收、GitHub Release／Issue／Project 回读为依据。

## 入口与生命周期

`docs/README.md` 现在提供完整入口；`STATUS.md` 负责当前实现、验证、验收与发布摘要，中英文 roadmap 负责剩余工作，`CHANGELOG.md` 继续保存版本变化。原 STATUS 超过 1,100 行的结果与实验记录完整保存在 [2026-09-29 快照](../archive/STATUS-2026-09-29.md)，并调整相对链接。

144 篇 notes 均列入[主题索引](../notes/README.md)，保留原路径和证据。分类如下：

| 文档角色 | 篇数 | 处理方式 |
|---|---:|---|
| 技术参考 | 23 | 说明适用范围，当前状态指向统一入口 |
| 历史记录 | 98 | 明确检查点，保留当时失败、测量和交接内容 |
| 调查记录 | 13 | 保留未解决问题及证据缺口，不因年代自动关闭 |
| 方案／草稿 | 10 | 区分规划与实际实现，指向当前决策 |

旧 release、菜单需求、TAA 实验、工具验证以及初始 Project 导入记录也补上时间和用途。过期“当前”“未发布”“下一步”不再作为今天的执行依据。不存在于当前检出的本地实验产物改为带说明的代码路径，避免伪装成可用下载链接；两处历史 CI 引用固定到对应提交。

## 已纠正的状态漂移

| 主题 | 纠正后的记录 |
|---|---|
| 发布与安装 | v0.7.20 已发布，实际有 Windows ZIP、AppImage、Flatpak 三个资产；独立 DX12 `.lospd` 包见 v0.7.10 等较早发布。两种 README 保持一致。 |
| 发布后的源码 | #74 的随时存档警告在 v0.7.20 标签之后；警告不等于恢复分队换人权限，Issue 仍开放。 |
| Issue | #82／#87 已关闭并随 v0.7.20 交付；#54／#55 的维护者关闭不等于诊断代码证明所有语言问题已修复。当前九个开放 Issue 单独列出。 |
| FG 与 Gate 1 | 保留 2026-09-27 Gate 1 通过及 2026-09-29 v0.7.15 功能验收；已知 SDK 同步例外和更广硬件／实景覆盖继续明确跟踪。旧失败运行仍保留为历史证据。 |
| Linux | 原生 Linux 图形导入器和有限的 AMD 8060S／Flatpak 用户证据已存在；旧 Windows-only／仅首次启动的描述已纠正。 |
| 工具与打包 | 手柄震动默认开启；shader staging 与 native verification 的实际职责分开说明；已删除 workflow 和旧补丁脚本移至历史说明。 |
| Git remotes | 当前 `origin` 为 GitHub，`zkx` 为 Gitea；发布文档改为核对 remote 后显式推送目标 revision。旧 `tools/push_all.ps1` 仍硬编码历史 `origin`／`github` 名称，文档已标明适用限制。 |

## Project 对账

Project 从最初回读的 233 项归整至 237 项，本地清单从 218 项扩充为 236 项。初始 2026-09-08 的 145 项另属历史导入。最终远端状态为 Done 188、In Progress 14、Paused 16、Todo 19；Todo 从 32 项降至 19 项。

实际改动 26 条记录：新建 4 条、14 条既有事项迁移状态、8 条更新证据／Source key／正文。已发布的 D3D12 shader 包和 FSR／DLSS FG 移至 Done；明确延期的研究、UI 分离、SDK 例外及平台工作移至 Paused／Deferred；动态 MFG 保留已实现但尚有验证余项的状态。补入 #74、#85、#88 和 #90 的 save-state 事项，HDR 与 macOS 复用现有记录并关联 #90／#96。

远端多出的 1 条为无 Source key 的历史 Issue #17 重复链接，明确保留为外部项，不制造第二份本地任务。已有维护者 Done 的事项按其当前状态同步，不凭旧“未实现”说明重开。最终 2026-09-30T07:28:19Z 回读后，计划检查为 236 unchanged、0 create／update／conflict。完整迁移和边界见 [Project 对账](../project-management/reconciliation-2026-09-30.md)。本轮没有修改 Issue／PR 状态或评论。

## Basic Memory

在现有 Basic Memory `memory` 项目、`projects/lost-odyssey-recomp` 目录内先检索再写入，项目笔记从 11 篇增至 19 篇。新增八篇经验，分别覆盖 CPU 瓶颈归因、GPU／FG 时序证据、XMA 音频、发布打包、存档与跨平台 I/O、文档状态维护、shader 预编译和运动矢量／资源寿命。

同时更新六篇既有笔记：项目目标与协作约定、项目记忆索引、当前架构与发布快照、待跟进事项与证据缺口，以及 Issue 17／v0.6.6 和 v0.7.1／Flatpak 两篇发布接续记录。保留旧检查点并补充新的日期、源码、发布及跟踪器依据，避免旧快照继续充当当前结论；旧默认 SHA 复核要求、旧 Latest、早期 Gate 1 与 Flathub 状态均已加上适用日期或后续纠正。

每篇经验包含来源和适用边界；旧 sampled vertex-cache 性能实验明确标为历史，不能用于建议恢复已被完整字节比较替代的采样正确性检查。新增笔记均完成写后原文回读；既有笔记完成针对性编辑与回读。完整标题、Basic Memory permalink 和验证记录保存在本地 `out/docs-normalization-20260930/basic-memory-report.json`，该文件属于忽略的工作证据，不随仓库分发。

## 检查与边界

本轮只做文档、知识库与项目管理核对，没有新建运行时验证结论。已有构建、游戏、显示器和用户验收的范围沿用原证据。没有提交或推送仓库改动。

文档检查覆盖整理后的 226 份文档：本地 Markdown 链接、标题锚点、代码围栏、从总入口的可达性及 144 篇 notes 的索引完整性均通过，`git diff --check` 通过。可复核的本地证据保留在 `out/docs-normalization-20260930/`，包括整理前后的 Project 读回、逐项修改和文档检查结果。
