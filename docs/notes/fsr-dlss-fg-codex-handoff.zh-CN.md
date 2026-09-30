# FSR / DLSS FG 当前交接

更新时间：2026-09-24。本文是当前 FSR P2 状态与接续入口。详细历史测试过程见[历史交接记录](fsr-dlss-fg-codex-history.zh-CN.md)和[进度证据日志](fsr-dlss-fg-codex-progress.zh-CN.md)；已提交源码不等于公开发布或 P2 全面验收。

## 当前状态

G002/P1 FSR 已完成；G003/P2 仍在进行中，尚未完成最终用户画面验收。已推送检查点 `ebefab459515fcd629e9758c8726d5b9b6c4b04b`，其中包含本次三组战斗 shader 映射。手动战斗候选程序 SHA-256 为 `cb41c99cdc44456864ac0d44cfcd815c2d099f89595840f4e76df2a2db12c352`，源码身份为 `d78d80f3add32494c53f67702d27d52c5c5e23aa82d690339cba046768c6357e`。该候选身份只标识那次运行所用程序，不是当前发行版本。

## FSR分支代码修复（2026-09-24）

`FSR`分支在上述基线后修复了录制阶段暂态输入的请求闭锁、Prepare阶段设备丢失传播、反向深度准入，以及固定SDK的Vulkan内存属性匹配/统一内存回退；并修复独立CPU测试的Plume依赖和Linux类型推导问题。详见[修复范围与验证记录](fsr-repair-2026-09-24.zh-CN.md)。需要重新配置CMake并重建SDK目标。本段不宣称已有游戏候选二进制包含这些修复，也不扩大下方既有GPU和画面验收范围；P2仍未完成。

## 已实现与已验证

- Windows FSR Vulkan 路径有资格检查的默认桥接：R8 alpha resolve 后转为 R32 reactive mask，使用 `min(0.9, M)`；transparency-and-composition mask 为 null。既有环境开关可关闭桥接，非 FSR 路径不执行它。RCAS 默认关闭，菜单提供设置项。
- 暂态场景输入失败可在同一请求下恢复；history reset/epoch 更新不会永久闭锁。owned-depth 与 replay module shader 创建的资源错误可沿初始及缓存路径传播。失败注入不代表真实系统 OOM。
- Windows RTX 5080 Vulkan 的 FSR SDK 实际消费、颜色转换、跨提交 mask 传递和 raster edge 均有有界验证。参考记录：`out/streamline-fg-p0/fsr-p2-sdk-integration-01/`、`fsr-p2-color-roundtrip/`、`fsr-p2-cross-submit-01/`、`fsr-p2-raster-edge-01/`。Postprocess Windows03 在 720p Quality、853×480 输入下验证六个 draw 与参考像素链；深度全链不变性和通用光栅规则未由此证明。
- APEX 15W 是用户接受的本轮低功耗 Deck 代理验证，已完成 SDK 路径与受控性能检查；它不等同 Steam Deck 的硬件行为、性能或兼容性。证据：`out/streamline-fg-p0/fsr-p2-apex15w-sdk-02/`、`fsr-p2-apex15w-performance-01/`。
- Windows 有界画质矩阵与同进程 provider 切换已完成；case10 覆盖 Quality→Native AA→DLSS Quality→FSR Quality，原有结论可复用。证据：`out/streamline-fg-p0/fsr-p2-quality-windows-01/`、`fsr-p2-dynamic-windows-01/win-dynamic-10/`。既有 case10 截图审阅未证明快速转向、完整遮挡或 DLSS 画质优势。
- 战斗映射为 `8d97` slot 8、`4bd` slot 230、`f6` slot 8。CPU fixture 通过 768 clips；GPU fixture 编译六段映射 microcode 为 DXIL/SPIR-V，并通过 Vulkan alpha discard、palette motion 与 QuadList 双 quad 检查。Fixture 不代表三组映射都在游戏中实际出现。证据见 `out/streamline-fg-p0/fsr-p2-battle-cpu-01/`、`out/battle-motion-replay-build/`。
- 一次 RTX 5080 Vulkan 手动战斗运行，在 46.787–92.526 秒窗口记录 2,476 条完成的 FSR 使用，未记录 `mv_first_failure`；日志中有可恢复的暂态 fallback。它只证明该次战斗窗口，不是全游戏或视觉验收。证据：`out/streamline-fg-p0/fsr-p2-battle-mapped-01/manual-mapped-01/battle-log-audit.json` 和同目录的 `.md` 报告。

## 尚未覆盖与门禁

当前候选的视觉反馈仍待用户确认。完整 P2 画质、更多场景与最新 Linux 战斗映射尚未验证。`02d8` 的 screen-UV binding 未知，但没有证据证明它是实际 blocker，也不要求预先实现。`f6` 只在 fixture 中验证，未在上述手动战斗中观察到。APEX 15W 完成了用户批准的本轮低功耗代理范围，不能据此推断 Steam Deck 实机表现。

Gate B 第三次审查与一次额外授权审查均已用完。已定义的三组映射准入条件及 CPU/GPU fixture 已满足；这不表示最终 P2 验收通过，也没有剩余审查额度可用于新的重大 shader 映射审查。

## 接续步骤与数据保护

先等待用户对当前画面的反馈；不要重复自动 Ram 撞击交互路线，不要重新启动、控制或关闭用户当前游戏。PID 35952 仅是最后记录的进程号，不代表现在仍运行。根据用户画面反馈再决定是否需要 Linux 或更多场景覆盖；只在新证据显示 `02d8` 实际妨碍覆盖时再研究其 binding。

用户 `user01` SaveAnywhere 副本 SHA-256 为 `9b5407eb599917ddff02668e08f423ca522f7d7887f8dac3d61992fded2be0ba`。该副本已成功载入一次，原始存档已核实未被修改；不据此推断其他存档副本或场景。此前报告出现不同 hash，原因未查明，不能当作存档损坏证据。

用户已确认原版游戏也有角色经过雾效时的相同表现；该观察不作为本项目新增缺陷。已通过的 mask、color、edge、性能与 case10 检查按记录范围复用，不因交接、提交或文档整理重跑。

## 证据导航

- [历史 Codex 交接](fsr-dlss-fg-codex-history.zh-CN.md)：2026-09-23 前后当时状态与修复经过，保留历史判断。
- [Codex 进度证据日志](fsr-dlss-fg-codex-progress.zh-CN.md)：日期化过程记录；不能取代本页的当前状态。
- [旧 OpenCode handoff](fsr-dlss-fg-handoff.zh-CN.md)：P0 阶段早期快照，仅作历史来源。
- [导入任务记录](fsr-dlss-fg-imported-tasks.zh-CN.md)：原始需求与阶段映射的历史记录；其中旧的“当前”状态不再适用。
- [项目状态](../STATUS.md)：面向项目的英文实现、验证和发布状态摘要。
