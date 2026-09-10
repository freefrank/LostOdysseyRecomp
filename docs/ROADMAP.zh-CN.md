# 路线图

[English](ROADMAP.md) · [当前状态](STATUS.md) · [更新日志](../CHANGELOG.md) · [历史路线图快照](archive/ROADMAP.zh-CN-2026-09-10.md)

`[ ]` 待完成 · `[~]` 进行中 · `[x]` 在所述范围内已有证据。[公开维护者 Project](https://github.com/users/freefrank/projects/3) 是当前工作项的事实来源。本镜像只保留方向、未完成事项和验证边界；实现、玩家验收和发布状态彼此独立。

## 交付

[v0.5.2](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.2) 已于 2026-09-10 发布，为当前最新版本。构建、安装包和公开下载核验均通过；发布依据及验收范围见 [STATUS](STATUS.md)。

<a id="v050-pc-graphics"></a>
<a id="下一主版本v050--pc-vulkan-与-direct3d-11"></a>
## PC 图形方向

D3D12 仍是可用基线。Windows Vulkan 已有 RTX 5080 实机场景的界定验证，与更广 GPU 和全游戏覆盖分开。Direct3D 11、Linux/Steam Deck 和 Switch 是彼此独立的未来平台工作，未声明完成或发布日期。

<a id="近期优先事项"></a>
## 当前事项与验收边界

- [~] **更新器修复：**更新器修复以 v0.5.3 为目标；其本地修复接受 ZIP 的显式根目录条目，同时保留单根目录校验；更新器界面和错误现仅使用英文。updater target 构建成功，12 个归档用例通过，覆盖真实 `shutil.make_archive` 布局及畸形归档／安全拒绝。非激活桌面的仅渲染检查确认 language=7 的英文文案、控件适配和前台未变化。尚未运行游戏、网络更新或完整更新事务；尚无用户验收或发布证据。
- [~] **可选 shader 收集：**收集开发及受控 D3D12 Map16 上传路径已验证。source-0.5.0 EXE 在继续渲染时产生 55 个 cache source 和匹配的 D1 记录。v0.5.2 已发布；产品 F1 capture、新 ZIP manifest 和即时上传尚未验证。Vulkan、AMD、已报告画面问题和玩家验收仍开放；累计覆盖和全 submitted draw 审计仍进行中。用于最终纹理绑定、生产者时序、实际 jitter 和 VS/PS 配对的有界 opt-in 扩展已通过 queue、producer、Worker 和 HTTP-to-SQLite 检查，Worker 已部署；客户端仅完成本地构建，尚未发布或验收。e810 范围限制为每帧每个 PS 最多一次调用，不构成 mapping repair 结论。私有每日 03:17 UTC D1 增量归档首轮已成功：截至 2026-09-10T15:07:15Z，归档 3,401 条诊断、431 个唯一 VS/PS source、909 个 GPU association、31 条 temporal record 和 462 个已核验 payload。仓库仍为私有；下次运行是 2026-09-11。最新检查没有新增程序或诊断，仅增加 23 个 GPU association；三个 VS 候选仍待精确程序复核，两条 PS `i4.xy/w → c0 → texture0 → RGB` 路径只是诊断线索，尚无绑定、生产时序或 jitter 结论。当前 schema 3 只采集 e810 及其配对 PS，不能自动取得这两对新 VS/PS 的绑定证据。私有离线反馈账本通过 13 个新 fixture；已安装 skill 的首次导入更新 18 个 case，复跑后均未变化。16 条历史 review 仅作为未绑定历史保留，18 个 case 都待 review，当前没有人工结论；它是诊断账本，不是修复或验收。紧凑的 opt-in 自动遥测已完成本地实现和针对性验证：32 帧／180 秒 CPU 窗口、最多 24 个 pair 和 8 个 binding、18,905 B payload、零分配 C++ 检查、Worker／ledger／archive 检查以及实际 HTTP→SQLite→ledger 路径。本地 82,946,560 B build 尚未发布。私有 archive schema-4 兼容性已在 728d6030980a7be39feca493319f54bfb1a62e74 推送，Worker 14e74c54-1213-4240-9877-047f35cdda75 已上线并通过 schema 1–4 健康检查。客户端源代码仍未提交；现有玩家、游戏场景、画面修复和验收都不因此成立。当前紧凑诊断和 binding feedback 的交付目标为 v0.5.3；已授权的 commit、tag、release workflow build 和 GitHub 发布仍待完成。
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
