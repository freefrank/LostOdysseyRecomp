# 路线图

[English](ROADMAP.md) · [当前状态](STATUS.md) · [更新日志](../CHANGELOG.md) · [历史路线图快照](archive/ROADMAP.zh-CN-2026-09-10.md)

`[ ]` 待完成 · `[~]` 进行中 · `[x]` 在所述范围内已有证据。[公开维护者 Project](https://github.com/users/freefrank/projects/3) 是当前工作项的事实来源。本镜像只保留方向、未完成事项和验证边界；实现、玩家验收和发布状态彼此独立。

## 交付

[v0.5.3](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.3) 已于 2026-09-10 发布，为当前最新版本。构建、安装包和公开下载核验均通过；发布依据及验收范围见 [STATUS](STATUS.md)。

<a id="v050-pc-graphics"></a>
<a id="下一主版本v050--pc-vulkan-与-direct3d-11"></a>
## PC 图形方向

D3D12 仍是可用基线。Windows Vulkan 已有 RTX 5080 实机场景的界定验证，与更广 GPU 和全游戏覆盖分开。Direct3D 11、Linux/Steam Deck 和 Switch 是彼此独立的未来平台工作，未声明完成或发布日期。

<a id="近期优先事项"></a>
## 当前事项与验收边界

- [x] **安装器拖动闪退：**用户报告已发布的 v0.5.3 安装器在拖动时卡顿并退出。本机四次 `ucrtbase` 0xc0000409 事件及 41648 dump 指向 Tk binding 经同步 `SendMessageW` 进入 Python WNDPROC/Tk 重入，最终触发致命 GIL 状态错误。本地未发布修复把拖动分发改为带屏幕坐标的 `PostMessageW`；新的 message-only HWND DragDispatch case 通过 1/1。同一旧 fixture 两次都在 setup 阶段因未变的前台断言失败，尚未到达拖动行为。installer-only 修正版 EXE 已本地打包（11,888,743 bytes；SHA-256 `707CD7D2E9F4AB3BF33363E172FAAD5CFCFA6B1A53161FEE0E7F53735B7C7FA7`），只读 embedded-PYZ 检查确认目标分发／模块。用户于 2026-09-10 确认报告的拖动路径已解决；本修复仍未发布。
- [~] **更新器修复：**v0.5.3 包含 ZIP 根目录条目修复、单根目录校验和仅英文的更新器界面。保留的 updater target 和 12 个 archive case 已通过，v0.5.3 的安装包和公开下载核验也已通过。完整更新事务、游戏运行和用户验收仍待完成。
- [~] **可选 shader 收集：**v0.5.3 已发布紧凑的 opt-in 诊断和 schema 3 绑定证据；Worker schema 1–4 与私有归档、账本已集成。紧凑收集使用 32 帧／180 秒 CPU 窗口、最多 24 个配对和 8 个绑定，限制在 32 KiB 内。18 个账本案例仍待复核。schema 3 只覆盖 e810 的两对绑定；新候选仍需程序复核及足够的最终绑定、生产者时序和 jitter 证据。画面修复、玩家验收、更广 GPU 覆盖和全部提交绘制的覆盖审计仍待完成。
- [ ] **Capture 导出大小：**一次新的 F1 ZIP capture 在 60.049 秒后超时。这是失败记录；紧凑格式和 exporter 修复尚未实现。
- [~] **4K TAA 与地面／阴影反馈：**17468–17470 capture 来自早于四路径 c7 修复的较早 EXE。它不重开后续已验收的四路径 Sol 修复，不定位新根因，也不建立更广画面覆盖。继续通过 Project 记录场景、硬件和报告者针对性的验证。
- [ ] **呈现与输入：**全屏、Alt+Enter、混合 DPI 和鼠标验收仍开放。判断用户报告的 underscan 时应保留正常宽高比黑边。
- [ ] **游戏流程与稳定性：**后续推进、存档读回、遇敌、长时间稳定性、其余渲染反馈和跨 GPU／正式包证据仍为待办。既有界定修复不代表可完整通关。
- [ ] **性能与 shader 启动：**继续排查实际剩余卡顿和两个保留 compiler failure；固定场景数据不代表全游戏。

<a id="当前反馈与回归"></a>
## 保留的有效验证范围

用户于 2026-09-08 接受了有限的 D3D12/Vulkan 实机场景边界。四条 c7 路径在一个 Ghost Town 场景中约 11.37 秒内获得验收；这不是连续视频、跨场景、跨 GPU 或全游戏覆盖。此前已发布修复及其证据继续见[STATUS.md](STATUS.md)、[CHANGELOG.md](../CHANGELOG.md)和历史快照。

## 导航

以[Project](https://github.com/users/freefrank/projects/3)查看工作项状态、依赖和详细未完成需求；以[STATUS.md](STATUS.md)查看当前验证与发布证据；以[CHANGELOG.md](../CHANGELOG.md)查看已发布改动；以[历史路线图](archive/ROADMAP.zh-CN-2026-09-10.md)查看保留的详细历史。

<!-- 保留兼容锚点，供既有 Project 和 notes 链接使用。 -->
<a id="已发布里程碑v042--修复与验证"></a>
<a id="阶段-1产出可编译代码"></a>
<a id="阶段-2进入主菜单"></a>
<a id="阶段-3推进完整通关"></a>
<a id="阶段-4现代化"></a>
<a id="阶段-5可选探索"></a>
