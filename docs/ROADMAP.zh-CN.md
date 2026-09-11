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

- [x] **v0.5.4 发布：**已于 2026-09-11T01:36:30Z 从 `2ad94d418bb0478417ab9589109f1f685ed92eb3` 公开发布；CI 34550200618 通过。44,237,061-byte ZIP 的 SHA-256 为 `104ced8b60c16cd1b9013543a3940c9ed8d7cf904c3d05a6a8ef8d591f51d218`；包来源、版本、全部 50 个文件 hash/CRC 及四个匿名资源下载均通过。本条只记录发布交付；各项运行时和玩家验收边界仍见下文。
- [~] **发布流程复用 PPC 库：**本地 PPC 自动同步已实现并启用（`lo.ppcAutoSync=true`）：本地输入变化时发布不可变 `ppc/<key>` branch；远端同 key 则直接复用，不重建或上传。19 个 sync fixture、两项直接受影响的抽取 case、两份 workflow actionlint、真实 `LoPpcAutoSync` hook upload、sparse-clone/restore/hash 与 CI-contract 检查均通过。真实 hook 仅把既有库上传至私有 commit `5e80263491b39dc0012146dd3a31cf5eea533225`，同 key 随后保持 unchanged；没有编译 PPC C++ 或运行游戏。CI 现按 key 解析而不再更新 workflow SHA pin；CI/imported/recursive 路径不可上传，auth/network 失败不视为 cache miss。实现已本地提交，尚未推送。没有新的托管 CI；未包含在已发布的 v0.5.4 中，也无玩家验收。此前手动路径的 [`2b5b1d1`](https://github.com/freefrank/LostOdysseyRecomp/commit/2b5b1d1) 与[合成 CI](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34553414428)保留为历史证据。
- [x] **安装器拖动闪退：**v0.5.4 已公开包含本地验收的 `PostMessageW` 修复，针对 v0.5.3 安装器拖动卡顿／退出。message-only HWND DragDispatch case 通过 1/1；旧 fixture 仍受未变的前台 setup 断言限制。用户于 2026-09-10 验收报告的拖动路径；更广安装器交互覆盖仍为独立事项。
- [~] **更新器修复：**v0.5.3 包含 ZIP 根目录条目修复、单根目录校验和仅英文的更新器界面。保留的 updater target 和 12 个 archive case 已通过，v0.5.3 的安装包和公开下载核验也已通过。完整更新事务、游戏运行和用户验收仍待完成。
- [~] **可选 shader 收集：**v0.5.3 已发布紧凑的 opt-in 诊断和 schema 3 绑定证据；Worker schema 1–4 与私有归档、账本已集成。紧凑收集使用 32 帧／180 秒 CPU 窗口、最多 24 个配对和 8 个绑定，限制在 32 KiB 内。18 个账本案例仍待复核。schema 3 只覆盖 e810 的两对绑定；新候选仍需程序复核及足够的最终绑定、生产者时序和 jitter 证据。画面修复、玩家验收、更广 GPU 覆盖和全部提交绘制的覆盖审计仍待完成。
- [~] **Capture 导出超时：**两次 F1 ZIP 归档均在约 60 秒后超时。archive worker 现改为在把子进程视为超时前最多等待 180 秒（`60000` → `180000` ms）；既有进程、job object 和 `Optimal` 压缩行为保持不变。v0.5.3 EXE 已成功链接并核对 `build.json` 来源，安装替换后的 SHA256 为 `CF70EA663ED230334145E1135CA97A58DFE3A34C65ED7D43E9E428C41B53270B`；旧 EXE／metadata 已备份至 `out/f1-zip-180s/backup`。整体构建最终因源码依赖 DLL 缺失而在复制 DXC 时失败；未重编译，仅从安装目录复用 DLL 补齐 build 输出。尚无超时恢复测试、游戏运行或玩家验收记录；v0.5.4 安装包及公开下载核验已通过。
- [~] **4K TAA 与地面／阴影反馈：**17468–17470 capture 来自早于四路径 c7 修复的较早 EXE。它不重开后续已验收的四路径 Sol 修复，不定位新根因，也不建立更广画面覆盖。继续通过 Project 记录场景、硬件和报告者针对性的验证。
- [~] **PPC 生成源码一致性检查：**生成树 provenance 子范围已完成：`ppc_codegen generate` 产出 247 个 C++ 单元，含 843 个 `.u32`、零个 `.u64` selector，246 个指令流哈希保持不变。7 个 guard fixture 通过，覆盖旧输出、input/tool/context drift 和 return-0-without-output recovery；历史 3,258／109／843 表-44,523 证据已复用而未重跑。CMake 配置及编译前检查依赖已核验；更广 PPC 契约审计仍开放。Issue #14 的旧 v0.5.0 日志依旧不定位根因、已发布构建条件或牢笼场景修复；尚无游戏运行或玩家验收记录。
- [ ] **呈现与输入：**全屏、Alt+Enter、混合 DPI 和鼠标验收仍开放。判断用户报告的 underscan 时应保留正常宽高比黑边。
- [ ] **游戏流程与稳定性：**后续推进、存档读回、遇敌、长时间稳定性、其余渲染反馈和跨 GPU／正式包证据仍为待办。既有界定修复不代表可完整通关。
- [~] **汇编级性能分析：**外部 Win64 工具现已记录 wall-clock RIP 快照，解析 DbgHelp PDB 符号／源码位置，并生成 Capstone x64 HTML/JSON 热点、线程 CPU 时间／筛选和函数 self 样本排名。7 个定向报告用例、MSVC Release 构建和一次合成端到端采集均通过；实际游戏采样和玩家验收仍待完成。不支持 GPU 指令分析。其 PPC 注释是生成源码上下文，不是精确 guest PC；请求的采样间隔也不代表实际频率。
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
