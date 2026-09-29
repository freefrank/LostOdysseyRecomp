# 路线图

[English](ROADMAP.md) · [开发状态](STATUS.md) · [更新日志](../CHANGELOG.md) · [维护者 Project](https://github.com/users/freefrank/projects/3)

Issue、Project 字段和已合并提交于 2026-09-28 核对，发布记录于 2026-09-29 更新。`[x]` 表示所述范围已交付；`[~]` 表示仍有明确余项；`[ ]` 表示规划工作。关闭跟踪项不代表新增游戏或硬件验证。

## 当前交付

- [x] **v0.7.0 已发布：**源码 `4142f23`，Release CI [36228746088](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/36228746088)。Windows 与 Linux 包见[发布页](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.7.0)。
- [x] **v0.7.1 已发布：**源码提交 `c585ef820cb72993ad87a90a1a03c1c648fb654c`，打标 `v0.7.1`，[发布页](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.7.1)（2026-09-26T21:51:24Z），Release CI [36274702691](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/36274702691)。包含 Gameplay → 导入光盘与 DLC、安全重启至导入器、选择性替换与失败回滚。公开产物已核实 SHA256：Windows 包 `LostOdysseyRecomp-windows-x64-v0.7.1.zip`（243,762,106 字节，SHA256 `e53753a71b06ab39c41a3a5b327a8477523db4b006543c54e70834b183c5291f`），Linux AppImage `LostOdysseyRecomp-linux-x64-v0.7.1.AppImage`（251,038,200 字节，SHA256 `878d04f9a530771fc2ba752842c1c9b5ba1cfc3fea63555a401dd53b46dd6e65`），正式独立 Flatpak `LostOdysseyRecomp-linux-x64-v0.7.1.flatpak`（265,618,800 字节，SHA256 `2efe0a4ba556037f9118894b36cba4b7667132b708c9ec3ea325db9c16f71775`，stable 分支），以及 Flathub 输入 runtime `LostOdysseyRecomp-linux-x64-v0.7.1-flatpak-runtime.tar.xz`（SHA256 `661838345ca5e1590dce99e35a9dba2bc1138d073c1c76d947aec34ea4db931f`）。Flatpak 经验证获 psvita 用户验收（严格限制于该验证范围，不推断性能或多场景兼容性）。
- [x] **v0.7.2 已发布：**merge 源码 `e2fc909dc15757aa5180566cecfd1ef2ff25dd18`，打标 `v0.7.2`，[发布页](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.7.2)（2026-09-27T08:59:13Z），Release CI [36305268629](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/36305268629)。新增有界的 Windows D3D12 DLSS/FSR SR、DLAA 尺寸修正，以及原生物体运动不可用时的相机／深度 hybrid motion。10 个资产均与 GitHub SHA-256 和大小记录一致；Windows ZIP、AppImage sidecar 和 Flatpak CI 核验通过。更广场景、画质、性能和其他 GPU 覆盖不在本次发布证据范围内。
- [x] **v0.7.9 已发布：**打标 `v0.7.9`，tag commit 为 `99fdcfa232e4deff2a80989d217524e7eb4bb365`，[发布页](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.7.9)（2026-09-28T05:03:17Z），Release CI [36378342125](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/36378342125)。新增 Windows D3D12 图像页 FG 分区，支持关／DLSS／FSR 和同进程即时生效，并加入 Ubuntu 22.04 AppImage 基线及 AppDir 复用 Flatpak。公开发布仅包含 Windows ZIP、Linux AppImage 和 stable Flatpak；更广游戏、跨 GPU、画质及物理显示验证仍待完成。
- [x] **v0.7.15 已发布并通过功能验收：**tag 与 Release CI head 均为 [`b074b689`](https://github.com/freefrank/LostOdysseyRecomp/commit/b074b689a3d2ffdbebabc1e14aad524d87e8c3ae)，[发布页](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.7.15)（2026-09-29T00:26:14Z），Release CI [36500844014](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/36500844014)。5 个 job 均成功，4 个公开资产已上传；独立 DX12 shader 资产复用 v0.7.10 资产。维护者于 2026-09-29 确认本轮发布的全部功能通过验收。该验收限于已发布功能，不等于完整游戏、跨 GPU、实体显示或完整通关覆盖。
- [x] **PR #80 已随 v0.7.15 发布：**合并提交 [`e79a793`](https://github.com/freefrank/LostOdysseyRecomp/commit/e79a793530412633bc57b6fbd9b43097023deb3c) 新增原生 90／120 FPS 目标和 FreeSync／G-SYNC Compatible VRR 输出节奏控制。PR 最终 8 项 CI 检查通过；同场景用户证据确认输出节奏低于 144，且 G-SYNC／刷新率发生变化。v0.7.15 已发布功能已获维护者验收；更广游戏、退出生命周期、画质和实体显示覆盖仍作为后续回归工作。

## 已完成功能与已核对跟踪项

- [x] **Flatpak 独立发布：**原计划 v0.8.0，现提前随 v0.7.1 交付。独立包 `LostOdysseyRecomp-linux-x64-v0.7.1.flatpak` 已发布并通过 psvita 用户验收。独立 Flatpak 目标不以 Flathub 上架为前提。Flathub 商店提交流程单独跟踪（未创建 PR）：因 Flathub `requirements#generative-ai-policy` 严格禁止 AI 生成或协助编写 manifest 与 PR，且 PR 模板要求附带应用演示视频（application demonstration video），需由维护者本人人工另制 manifest、提供应用视频并提交 PR。

- [x] **DLSS/DLAA、FSR 超分与 D3D12 FG：**v0.7.0 范围已在记录的 Windows／原生 Linux 覆盖内通过用户验收；v0.7.9 新增有界的 Windows D3D12 DLSS／FSR FG 和游戏内同进程切换。更广 FG 硬件、场景、节奏、画质和物理显示覆盖仍待完成。
- [x] **PlayStation 按键提示：**宿主与游戏内面键、肩键、Start/Back 提示已验收并随 v0.7.0 发布。
- [x] **Mod API v1 与 Wiki：**`457ba24`／PR #68 已交付清单解析、禁用与回退、重载、LOTEX1/PNG 工具、原生菜单图集与字体替换及 Mod 指南。Windows/Linux Mod API 与 Wiki CI 已通过。任意游戏纹理／模型替换和真实 MO2 验收不属于此已完成范围。
- [x] **IME、闲置光标与手柄改进：**已随 v0.6.19 发布，旧 IME“未开始”条目已关闭。更广设备组合仍属回归覆盖。
- [x] **PortForge 清单：**`9abda30` 已提供 Windows/Linux 清单，Issue #37 已关闭，本地记录现与 Project 的 Done 一致。
- [x] **Issue 自动分析：**已部署 Issue-opened 与 `@codex` 回复，并核实[真实公开回复](https://github.com/freefrank/LostOdysseyRecomp/issues/21#issuecomment-5669773986)。后续缺陷修复和回复质量改进单独跟踪。
- [x] **音频跟踪项 #54/#55：**Issue 与 Project 均已关闭。此处记录维护者关闭状态，不将 PR #66 的诊断改动当作所有语言／缺失台词问题的修复证明；历史反馈仍保存在条目证据中。
- [x] **此前已交付：**实时 AF、退出桌面／标题菜单、随时存档偏好持久化、作弊导航、超宽屏控制、便携式 Vulkan shader 包、Linux AppImage、在线 PPC 编译及有界渲染／运行时修复。历史测量与版本细节保留在[开发状态](STATUS.md)和[更新日志](../CHANGELOG.md)，不再混在当前进行中队列。

## 当前工作

- [~] **未关闭报告：**[#49](https://github.com/freefrank/LostOdysseyRecomp/issues/49) 物理像素窗口坐标和 [#74](https://github.com/freefrank/LostOdysseyRecomp/issues/74) 多队伍迷宫中的 Debug Save Anywhere 行为仍开放。Issue [#64](https://github.com/freefrank/LostOdysseyRecomp/issues/64)、[#67](https://github.com/freefrank/LostOdysseyRecomp/issues/67) 和 [#77](https://github.com/freefrank/LostOdysseyRecomp/issues/77) 已在 GitHub 关闭（2026-09-29 核对）；关闭和 v0.7.15 功能验收不等于完整通关或更广硬件、场景覆盖。
- [~] **Issue #40 剩余 Mod 范围：**PS 提示、v1 框架与 Wiki 已交付；更广游戏纹理／模型接入、真实外部管理器集成仍待完成，Issue 保持开放。
- [ ] **功能请求：**景深控制（#30）和晕动症选项（#48）。
- [~] **原生运动与时序颜色：**几何／刚体／骨骼 replay 基础和已确认的 SDR 输入已实现，并有有界战斗与 Hybrid SR 证据。余项为未映射 draw、更广骨骼／场景覆盖、HDR／曝光和 D3D12 replay PSO 错误 `0x80070057`。
- [~] **Linux／Steam Deck：**Linux x64 运行时、菜单、导入器、更新器与 AppImage 已发布，并有原生 AMD 8060S RADV 证据。Steam Deck 实机、Steam runtime／Flathub 及更广流程仍待覆盖；APEX 15W 不能等同 Deck 实机。
- [~] **性能与 shader 启动：**有界城市场景／缓存优化和通知等待已进入 main。剩余停顿、两个保留的 shader 翻译失败、更广 15W／全游戏性能目标仍开放；旧“未发布分支”描述仅为历史。
- [ ] **图形后续：**移除独立实验性 TAA、纯资源 PSO 覆盖，以及基于测量的缓存／运动优化。D3D12 便携 shader 包已在 v0.7.10 作为独立 `.lospd` 资产交付。空间 AA 回退不等于移除 TAA 选项，也不证明所有闪烁已消除。
- [ ] **更广回归：**全流程、章节／换盘／存档兼容、音频／语言、手柄／震动、混合 DPI／全屏与多 GPU。不会仅因这些广泛目标尚未覆盖而把已完成的具体修复继续挂起。

## v0.8.0 计划

规划执行顺序（开发执行安排，不虚构后项对前项的必然技术依赖；P0/P3/P4 为阶段标识，不混同为优先级）：

1. [~] **P0：**共同时序契约已实现，Gate 1 宿主验证已由维护者于 2026-09-27 接受通过，依据为本地 `fe6f255` 加 Gate 1 宿主修复。原生构建和限定检查通过；FSR+FG 限定运行 exit 0、serial 为 820/820 且清理完整；获授权的 70 秒静音前台运行 exit 0、serial 为 2975/2975，生成区间 1,980 次、actual presents 4,955 次，采样 SDK／feature 创建错误为 0。已知 SDK 相关的 `PRESENT-AFTER-WRITE` 记录保留到 backlog，不再阻塞 Gate 1。`Application`／`ComposedFlip` 分类不足以证明生成帧到达物理显示；native failure injection 和 settings restart 仍属后续工作，native CPU 检查不等于 D3D12 GPU 验收。详见[Gate 1 宿主修复记录](notes/gate1-host-repair-20260927.md)。
2. [~] **P3：**Streamline 呈现接入、provider-neutral present／输入生命周期跟踪，以及最终合成 backbuffer 的 FG 路径已合入 [`81fe304`](https://github.com/freefrank/LostOdysseyRecomp/commit/81fe3048569f06bdeca4f1bd24c8fdc106428abc)／PR [#72](https://github.com/freefrank/LostOdysseyRecomp/pull/72)。v0.8.0 剩余工作是解决同步问题，并验证 resize、模式切换和退出生命周期。生产级 HUDless／UI 分离属于独立的 v1.0.0 目标，不再是 v0.8.0 完成条件。
3. [~] **P4：**Windows Vulkan 固定 2× DLSS 插帧已接入。经授权的 70 秒 Uhra 运行记录 48 个 enabled 周期、`actual_presents=2`、2,830 个生成区间和 0 个 SDK error；较早的同步 validation 运行失败，该历史结果仍保留在证据中。维护者之后已接受 Gate 1 通过，已知 SDK 例外转入 backlog。DLSS／DLAA／FSR 组合、生命周期恢复、节奏、画质和外部显示验证仍待完成。
4. [~] **独立 FSR 插帧后续：**独立 D3D12 FSR FG provider 已随 v0.7.9 交付；更广硬件、场景、节奏和画质验证仍待完成。
5. [~] **D3D12 DLSS 插帧后续：**D3D12 DLSS FG 和图像菜单即时切换已随 v0.7.9 交付；更广验证和 failure injection 覆盖仍待完成。
6. [~] **动态 MFG 后续：**D3D12 adapter 已包含受能力限制的诊断动态 MFG 路径，游戏内菜单仍只提供固定模式。更广 API、平台、倍率和硬件验证仍待完成。
7. [x] **原生 90／120 FPS 与 VRR：**原生游戏呈现和 FreeSync／G-SYNC Compatible 输出节奏已随 v0.7.15 发布，并获维护者验收。同场景输出节奏和硬件指示器变化已有有界用户证据；Ring、音频、过场、更广游戏、退出生命周期、FG 画质和独立 120 FPS 实体显示帧测量仍属后续覆盖，默认保留 30 FPS。
8. [ ] **移除 PM4 转换器：**替代架构可行性提前调查，执行排在插帧与呈现工作之后。[架构边界图](notes/native-migration-boundaries.md)已选出标题云雾背景的单个历史 draw，并定义有序回退与对照条件。当前版本的生产者归属、旁路实现和同场景验证仍待完成。 已实现[可选 SDK 旁路](notes/native-command-bypass.md)，完整移除 PM4 仍未完成。
9. [ ] **Linux AArch64：**平台交付目标，尚不宣称官方包或实机验收。
10. [ ] **macOS AArch64／Apple Silicon：**图形后端与依赖可行性提前调查，平台交付目标。
11. [ ] **实验性 Android：**探索目标，尚无 APK 或设备验证。

Gate 1 backlog：调查已知 SDK `PRESENT-AFTER-WRITE` 同步例外并补充显示分类，记录在[维护者 Project backlog](https://github.com/users/freefrank/projects/3?pane=issue&itemId=PVTI_lAHOAAsUY84Biy1azg9F10Q)。验证层记录的 10 条消息受 duplicate cap 限制，不是故障或帧数统计。

并行推进：

- [x] **Flatpak 发布：**已提前于 v0.7.1 交付完成（独立包已发布并经验证；Flathub 提交流程受 AI 政策限制，需人工另制 manifest、录制应用演示视频并提交，独立跟踪）。

## v1.0.0 延后计划

- [ ] **生产级 HUDless／UI 分离交接：**独立于 v0.8.0 的合成 backbuffer FG，建立并验证专用 scene／UI 合成契约。

## 后续积压

DX11、HDR 输出、高分辨率阴影、SSAO／深度访问、GI／反射、光追与暂停的 Switch 工作仍为独立计划。WMV 播放、临时主角伤害控制及其他未获完成证据的条目保留 Project 原范围，未直接标记完成。

逐项证据见 [Project](https://github.com/users/freefrank/projects/3)，早期细节见[历史路线图](archive/ROADMAP-2026-09-10.md)。本轮整理没有重跑构建、游戏或测试。

<!-- Historical link compatibility. -->
<a id="v070-frame-generation"></a>
<a id="v070-upscaling"></a>
<a id="v080-frame-generation-macos"></a>
<a id="v090-pm4-translator"></a>
<a id="v050-pc-graphics"></a>
<a id="下一主版本v050--pc-vulkan-与-direct3d-11"></a>
<a id="近期优先事项"></a>
<a id="当前反馈与回归"></a>
<a id="已发布里程碑v042--修复与验证"></a>
<a id="阶段-1产出可编译代码"></a>
<a id="阶段-2进入主菜单"></a>
<a id="阶段-3推进完整通关"></a>
<a id="阶段-4现代化"></a>
<a id="阶段-5可选探索"></a>
