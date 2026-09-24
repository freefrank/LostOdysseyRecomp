# FSR / DLSS FG 未完成任务导入清单（历史）

> 本文保留需求来源和历史阶段映射。文中的“当前进度”、运行状态与待办不再作为现状依据；请查看[当前 P2 交接](fsr-dlss-fg-codex-handoff.zh-CN.md)与[项目状态](../STATUS.md)。

## 导入元数据

- 导入日期：2026-09-23
- 导入范围：把 OpenCode 主会话及 `oh-my-opencode-slim` 中与 FSR、DLSS FG、P0 Gate 1 相关的未完成工作整理为当前 Codex 可续接的本地清单。
- 任务性质：本地文档导入与状态整理；没有重建 OpenCode 任务执行器，也没有创建新的 Codex sidebar 任务。
- 导入时代码基线：`39bbda6ff1cb8216582798bebab21a3a5395a36e`；当时已有代码修改未提交。后续 FSR 实现已本地提交为 `87a1691`。
- 导入时验证边界：CPU 直接受影响测试 4/4 通过，生产链接构建通过；当时没有生产路径 GPU 回归。该历史事实保留；当前生产 facade 回归已有单场景证据，但不扩展为全游戏画质、性能、每个 SDK teardown 或 FG 验收。
- 交付状态：FSR 实现与运动捕获已本地提交为 `87a1691`、`c028962`；未推送、未发布。

## 2026-09-23 Codex 恢复执行

本次已由新建的 ultragoal 恢复开发清单并进入执行阶段。OpenCode 与 `oh-my-opencode-slim` 的 `STOPPED` 是历史来源状态，保留用于追溯；当前 `IMP-P0-FIX2` 仍未通过 Gate，G002 的 FSR SDK 和 renderer/runtime 接线已实现，Windows/Linux 基础运行已有证据；P1 可运行阶段已完成受控运动及四档检查并 checkpoint；P2–P4 与保留的 P0 Gate 仍待验收，其原导入检查框保持未完成。

持久计划索引：[`.omx/ultragoal/goals.json`](../../.omx/ultragoal/goals.json)、[`.omx/ultragoal/ledger.jsonl`](../../.omx/ultragoal/ledger.jsonl)。本次只同步当前清单状态，不改写上述持久计划文件。

已核实的硬件和环境边界：存在 NVIDIA RTX 5080 与 AMD 核显；`psvita` 已在线实测 Bazzite 44 / ONEXPLAYER APEX，且已有用户授权进行 Linux 测试；尚未找到已核实的 Steam Deck 证据，不能把该设备或 Bazzite 实测写成 Steam Deck 验证。官方 PresentMon 2.6.0 ETW 启动已通过，便携 Khronos 1.4.328 已在实际运行中启用；仍缺少外部显示采集设备证据，因此不能写成外部显示已验证。

## 状态覆盖规则

`oh-my-opencode-slim` 的最新顶部状态曾为 `STOPPED`：用户要求停止并写 handoff，fix-2 remediation 已取消，旧的 ora-1 tracking 已失效；此前 `RESUMED`、`running` 或 `active` 段落均是历史记录。当前 Codex 已按新 ultragoal 恢复 P0 修复，历史停止状态不覆盖当前恢复执行。

OpenCode 只读核对信息：数据库为 `C:/Users/freefrank/.local/share/opencode/opencode.db`，主会话 `ses_f336e4344ffdDzERcm4fl3Ohap`；该会话在 `session_v2/session_message` 有记录，但 `todo` 表没有对应任务项。最后停止确认消息为 `msg_0ccfa7170001fCXvXI00K5l493`。fix-2 取消事件为 `msg_0ccf44d7d001Rcils7G0Tax1P5`，文档完成事件为 `msg_0ccf577c8001eqL28KJt8NIcx9`。

## 来源会话与原始状态

| 原始角色 | 原任务 / 会话 | 导入时状态 | 说明 |
| --- | --- | --- | --- |
| Fixer 1：公共门面与契约 | `fix-1` / `ses_f3364202effe4tMyklOeMRhVe3` | 已完成 | FSR provider/wire/输入契约与 `TemporalUpscaler` 门面已落盘；CPU 4/4 与生产构建证据保留，尚无 GPU 回归。 |
| Fixer 2：Streamline 共存探针 | `fix-2` / `ses_f3357846dffe9PFYIvonMwasrU` | 已停止，未完成 | 取消前仅做基线日志备份，未落实修复；Gate 1 缺陷仍在代码中。 |
| Oracle：Gate 1 评审 | `ora-1` / `ses_f335eeb2dffexMz3oxg4dJ3qNj` | 初审未通过 | attempt 1/3 为 NOT PASSED；还剩 2 次材料复审，不得把复审当作已通过。 |
| Explorer：结构勘察 | `exp-1` / `ses_f336cbbbbffeJG3x3fgL3ny1PQ` | 已完成并覆盖旧 active | 公共门面结构扫描没有新增阻断。 |
| Librarian：SDK 与前置调研 | `lib-1` / `ses_f336c9142ffeCEHaiVsPffjXEc` | 已完成 | 研究结论可复用；不等于真实 FSR/FG 运行验收。 |
| Project Manager：项目同步 | `pro-1` / `ses_f3363e8b1ffe8h97Ce5BBCXtrX` | 历史同步已完成 | 195 项同步完成；下一阶段取得实质证据后再同步，不把停机状态写成运行中。 |

交接原文中的 Gate 1 报告来自 `msg_0ccd2c3ac001DW3biIC5DmW8as`，追随消息为 `msg_0ccd49e13001iDJlOu7imMEmhl`。fix-2 相关事实以交接文档第 5 至第 8 节为准；早于最终源代码和 EXE 的 `out/streamline-fg-p0/runtime.log` 只能作为历史失败运行残留，不能作为修正版凭证。

## 当前可续接待办

以下编号是本次整理编号，不是 OpenCode 原始 ID，也不表示任务已派发。

### P0：Gate 1 修复与证据

- [ ] `IMP-P0-FIX2`（进行中）：仅在 `tools/tests/streamline_fg/*` 与 `cmake/LoStreamline.cmake` 内修复 Gate 1 阻断。范围包括用真实新帧 token 完成一次 Off Present、动态导入并检查 `slFreeResources(DLSS_G, viewport)`、双方 session 关闭前的释放顺序、按交换链图像分配 render-finished semaphore、在 acquire 前等待上一帧完成、pending use 异常托管与 `OnBatchDiscarded()`、resize 后局部首帧 reset，以及清理返回值到最终退出码的传递。另需探针专用生命周期记录，避免原生 128 条环形日志截断退出证据；不修改生产 Controller。
- 当前进度：原门禁修复已落盘，新增 handle logging only；run03 使用同版 EXE 完成 48 帧、exit1，validation 层仍报告同类 10 条 VUID。输出的三个 swapchain proxy image handles 与观察到的 VUID 对应，证明返回的是宿主代理图像而非四个输入 tag 的 image，但不证明具体 SDK 根因。Gate 仍不通过，正式复审未花，剩 2 次。详见 [Codex 进度](fsr-dlss-fg-codex-progress.zh-CN.md)。
- 当前验证：用户 goal 已恢复为 native active。Linux 依赖安装后，在 psvita 隔离目录 `p0-cpu-01` 使用当前工作树 25 文件和现有 `CPU_ONLY` CMake 运行原 CPU 测试，4/4 通过；详见 [Codex 进度](fsr-dlss-fg-codex-progress.zh-CN.md)及其结果清单。该结果不代表全量游戏构建、GPU/FSR 验证，不重跑 Windows 测试。
- 当前验证：用户已授权自主执行必要前台运行。foreground01 使用 C13 EXE 完成 48 帧，观察到 33 个 interval 的 `actual_presents=2`，release/shutdown API 成功但 validation 46 条、exit1；background04 同版仍有 10 条 layout VUID；apidump01 使用 F474 EXE 完成 48 帧、52 条 validation、exit1，trace 显示 SDK 内部 WAW 与 pacer transition 缺口。FG Gate 仍未通过，不能称为修复。
- [ ] `IMP-P0-EVIDENCE`（依赖 `IMP-P0-FIX2`）：用修复后实际 EXE 做一次受控探针运行，记录 EXE 哈希、完整日志和退出码；确认清理阶段不再出现 `0xbad00004`。复用 CPU 4/4 与生产链接通过结果，不重测它们。
- 当前比较：foreground01 已观察 `actual_presents=2`，但 validation 和 API trace 仍未满足 Gate；background04 与 apidump01 仍需解释 layout、WAW 和 pacer transition 缺口。用户已授权自主前台运行，但物理显示证据仍独立缺失。
- [ ] `IMP-P0-EVIDENCE-DISPLAY`（依赖受控探针成功）：补充外部显示侧生成帧证据；主机 HUDless 截图、`numFramesActuallyPresented = 2` 或 SDK 状态不能单独证明显示器实际接收插帧。便携 Khronos validation layer 已在实际运行中启用，但仍缺少物理显示采集证据。
- [ ] `IMP-P0-REVIEW-1`（依赖上述修复证据）：整理 Gate 1 材料复审包。Oracle 初审为 NOT PASSED，剩余两次材料复审机会；不重跑初审，也不把 clean exit 自动等同于 Gate 通过。
- [ ] `IMP-P0-PM-SYNC`（依赖阶段性实质证据与复审结论）：更新 `docs/project-management/items.json`、中英文 ROADMAP 和同步状态。历史 195 项同步已完成；本次停止状态不触发新的 PM 同步。

### P1：FSR 可运行版本（G002 已完成有界验收）

- [x] `IMP-P1-FSR-RUNTIME`：固定 SDK 接入真实低分辨率渲染、dispatch 与输出目标，完成 Windows Vulkan 和原生 Linux 的实际构建/运行边界；先 Quality，再 Balanced、Performance 和 Native AA。
- [x] `IMP-P1-FSR-GATE`：提供真实帧中的 provider、输入/输出尺寸、jitter/reset、提交与失败回退证据，并证明平移/旋转下 depth/MV 契约正确。
- 当前进度：G002 已按原 P1 条件完成并 checkpoint；Windows 四档、Linux RADV 路径、受控运动、提交与回退证据见 [P1 audit](../../out/streamline-fg-p0/fsr-p1-acceptance-audit.md)。P2 画质／性能、Steam Deck 与 FG 不随之验收。
- 早期 P1 运行检查点：Windows 当时生产构建 hash 见 [`fsr-p1-integration-results.json`](../../out/streamline-fg-p0/fsr-p1-integration-results.json)；adapter run04/run05-gap 全流程通过，failed-token-gpu-01 exit0、validation 0。quality02 已提交真实 FSR（`1706x960→2560x1440`），但有 144 条 baseline classes/extent validation、exit0 且 baseline unchanged；quality03-normal 与 NativeAA01 均有 exit0 的非 reset median 和截图结果，NativeAA 已核实 screenshot fix。fallbackgpu02 `6b00c780...` 通过实际 FSR record、注入 post-record reject、renderer current green 4096 pixels（保留 alpha）和下一次 actual blue SDK reset，仅证明 fallback/reset 范围，不能解释为 SDK internal fault 或 full-facade 通过。Linux run03 clean；当时尚缺的 nonzero MV、depth translation/yaw 和新增两档实景证据现已补齐。Steam Deck 仍属于 P2 待验收范围。

### P2：FSR 画质与 Deck 验收（G003 执行中）

- [ ] `IMP-P2-QUALITY`：验证颜色域、透明/reactive mask、场景边界、锐化和 mip 策略；比较同输出尺寸的 Off、原 TAA、DLSS、FSR。
- [ ] `IMP-P2-HARDWARE`：取得 AMD 与 NVIDIA 实机证据，并分别验证原生 Linux 与 Steam Deck；记录样张、连续帧和基础渲染时间/GPU 收益。未测设备不能写成已验收。
- 依赖：P1 的真实 FSR 执行和输入契约证据。

P2 已据设计启动原始 alpha 收集切片，草稿尚未构建／验收。设计记录：[fsr-p2-mask-design.md](../../out/streamline-fg-p0/fsr-p2-mask-design.md)，新 architect 计划包括 alpha replay 与后处理传播。尚无完整 opaque-only/composited 对，P2 检查框保持未完成。

### P3：FG 生产输入与呈现基础设施（未启动）

- [ ] `IMP-P3-PRESENT`：把已证明的 hook 方案接入主程序，完成 HUDless/UI、present lease 与真实帧 Reflex token；可先保持 FG 关闭，先证明输入和资源回收。
- [ ] `IMP-P3-LIFETIME`：验证 resize、最小化、模式切换、退出和关闭 FG 时的资源所有权、同步与无额外每帧等待。
- 依赖：P0 共存和清理证据；不能让两套 SDK 同时接管 SR。

### P4：DLSS FG 2×（未启动）

- [ ] `IMP-P4-FG-2X`：依次验证 DLSS Quality+FG、DLAA+FG、FSR+FG，建立基于实际 feature probe 和输入状态的开关。
- [ ] `IMP-P4-ACCEPTANCE`：证明真实生成帧被显示，游戏速度与输入判定不变，HUD/边缘稳定，并覆盖切场景、电影、暂停、窗口变化的停用和恢复。
- 依赖：P3 的生产输入、present lease、UI 分离和同步证据；P4 不能因 P0 探针曾生成 host 侧变化图像而提前视为完成。

## 当前生产 facade 回归

`out/streamline-fg-p0/production-facade-regression/RESULTS.md` 已记录单场景 DLAA/Quality 回归：DLAA PID 45168、render 6749、serial 13503，`2560x1440 -> same`；Quality PID 48712、render 5241、serial 10486，`1707x960 -> 2560x1440`。两者均有铃铛广场后续截图，WM_CLOSE exit0，原 settings/save/profile 哈希不变。该结果只覆盖生产路由、后续出图和进程正常退出，不能推出全游戏画质/性能、每个 SDK teardown 或 FG 验收；结果已完成，后续不重测。

## 调度状态

原 brief 明确先做 FSR 并行推进 FG；旧 strict serial 结论来自 OpenCode deepwork，不是用户技术依赖。G001 已 superseded（不表示通过），G006 保留完整 P0 objective 与 attempt 1/3 NOT PASSED、剩 2 次复审及 validation/display/image/performance 要求；G002 P1 已完成，G003 P2 正在开发，G003/G006/G004/G005 待验收。native 工具当前已确认 active，但用户已授权自主持续执行；OMX 当前 executing。设计依据见 [stage-dependency-audit.md](../../out/streamline-fg-p0/stage-dependency-audit.md) 和 [fsr-p1-input-contract.md](../../out/streamline-fg-p0/fsr-p1-input-contract.md)。

## 恢复边界

当前 G002 P1 已完成，G003 P2 执行中；G006 完整保留 P0 未通过项，P2–P4 仍待验收。提交、推送和远程 PM 同步遵循各自授权边界。

## 来源

- 当前交接：[fsr-dlss-fg-handoff.zh-CN.md](fsr-dlss-fg-handoff.zh-CN.md)
- Codex 当前进度：[fsr-dlss-fg-codex-progress.zh-CN.md](fsr-dlss-fg-codex-progress.zh-CN.md)
- Slim 状态文件：`.slim/deepwork/fsr-dlss-fg.md`
- 原始计划：`C:/Users/freefrank/Downloads/LORecomp_FSR_DLSS_FG_implementation_plan.zh-CN.md`（P1–P4 位于第 226–248 行）
- OpenCode 数据库：`C:/Users/freefrank/.local/share/opencode/opencode.db`（只读核对）

最新运动输入诊断及其边界见[当前进度](fsr-dlss-fg-codex-progress.zh-CN.md#2026-09-23-运动输入捕获检查点)：首帧静态 ROI 的 MV 回投吻合；完整捕获触发后两帧重置，未据此关闭 P1。

后续轻量捕获已取得 Windows 连续三帧移动／旋转证据，见[轻量连续帧试验](fsr-dlss-fg-codex-progress.zh-CN.md#轻量连续帧试验)。它补齐该平台的 P1 静态几何运动输入检查，不改变 P2、Steam Deck 或 FG 待验收状态。

G002 P1 已完成独立证据复核并由 OMX 标记 complete；G003 P2 正在执行。完整要求及证据边界见[阶段结案](fsr-dlss-fg-codex-progress.zh-CN.md#p1-阶段结案转入-p2)，未将 P1 通过扩展到原 P0 Gate 或 FG。

P2 原始 alpha 收集已有 Windows 同一次绘制的颜色／深度不变与非零 mask 证据，见[收集检查点](fsr-dlss-fg-codex-progress.zh-CN.md#p2-原始材质-alpha-收集检查点)。后处理传播、SDK mask 绑定与完整 P2 验收保持待办。
