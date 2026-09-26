# 路线图

[English](ROADMAP.md) · [开发状态](STATUS.md) · [更新日志](../CHANGELOG.md) · [维护者 Project](https://github.com/users/freefrank/projects/3)

2026-09-26 根据实时 Issue、Project 字段、已合并提交与发布记录核对。`[x]` 表示所述范围已交付；`[~]` 表示仍有明确余项；`[ ]` 表示规划工作。关闭跟踪项不代表新增游戏或硬件验证。

## 当前交付

- [x] **v0.7.0 已发布：**源码 `4142f23`，Release CI [36228746088](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/36228746088)。Windows 与 Linux 包见[发布页](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.7.0)。
- [x] **v0.7.1 源码已提交：**`b1cf166` 新增 Gameplay → 导入光盘与 DLC、安全重启至导入器、选择性替换与失败回滚。合成导入、菜单、controller、host 测试及 Windows 开发构建已通过；用户提供的 Asia GOD 和 USA/Europe ISO 来源均只读识别成功，未执行真实重新导入。打标签与发布尚待进行。

## 已完成功能与已核对跟踪项

- [x] **DLSS/DLAA 与 FSR 超分：**v0.7.0 范围已在记录的 Windows／原生 Linux 覆盖内通过用户验收。FSR P1/P2 完成，插帧见下方计划。
- [x] **PlayStation 按键提示：**宿主与游戏内面键、肩键、Start/Back 提示已验收并随 v0.7.0 发布。
- [x] **Mod API v1 与 Wiki：**`457ba24`／PR #68 已交付清单解析、禁用与回退、重载、LOTEX1/PNG 工具、原生菜单图集与字体替换及 Mod 指南。Windows/Linux Mod API 与 Wiki CI 已通过。任意游戏纹理／模型替换和真实 MO2 验收不属于此已完成范围。
- [x] **IME、闲置光标与手柄改进：**已随 v0.6.19 发布，旧 IME“未开始”条目已关闭。更广设备组合仍属回归覆盖。
- [x] **PortForge 清单：**`9abda30` 已提供 Windows/Linux 清单，Issue #37 已关闭，本地记录现与 Project 的 Done 一致。
- [x] **Issue 自动分析：**已部署 Issue-opened 与 `@codex` 回复，并核实[真实公开回复](https://github.com/freefrank/LostOdysseyRecomp/issues/21#issuecomment-5669773986)。后续缺陷修复和回复质量改进单独跟踪。
- [x] **音频跟踪项 #54/#55：**Issue 与 Project 均已关闭。此处记录维护者关闭状态，不将 PR #66 的诊断改动当作所有语言／缺失台词问题的修复证明；历史反馈仍保存在条目证据中。
- [x] **此前已交付：**实时 AF、退出桌面／标题菜单、随时存档偏好持久化、作弊导航、超宽屏控制、便携式 Vulkan shader 包、Linux AppImage、在线 PPC 编译及有界渲染／运行时修复。历史测量与版本细节保留在[开发状态](STATUS.md)和[更新日志](../CHANGELOG.md)，不再混在当前进行中队列。

## 当前工作

- [~] **未关闭报告：**#49 物理像素窗口坐标、#64 DLSS/FSR 行为及 #67 Grand Staff 天空闪烁仍开放。已有尺寸恢复和诊断不代表这些报告已解决。
- [~] **Issue #40 剩余 Mod 范围：**PS 提示、v1 框架与 Wiki 已交付；更广游戏纹理／模型接入、真实外部管理器集成仍待完成，Issue 保持开放。
- [ ] **功能请求：**景深控制（#30）和晕动症选项（#48）。
- [~] **原生运动与时序颜色：**几何／刚体／骨骼 replay 基础和已确认的 SDR 输入已实现，并有有界战斗与 Hybrid SR 证据。余项为未映射 draw、更广骨骼／场景覆盖、HDR／曝光和 D3D12 replay PSO 错误 `0x80070057`。
- [~] **Linux／Steam Deck：**Linux x64 运行时、菜单、导入器、更新器与 AppImage 已发布，并有原生 AMD 8060S RADV 证据。Steam Deck 实机、Steam runtime／Flathub 及更广流程仍待覆盖；APEX 15W 不能等同 Deck 实机。
- [~] **性能与 shader 启动：**有界城市场景／缓存优化和通知等待已进入 main。剩余停顿、两个保留的 shader 翻译失败、更广 15W／全游戏性能目标仍开放；旧“未发布分支”描述仅为历史。
- [ ] **图形后续：**D3D12 便携 shader 包、移除独立实验性 TAA、纯资源 PSO 覆盖，以及基于测量的缓存／运动优化。空间 AA 回退不等于移除 TAA 选项，也不证明所有闪烁已消除。
- [ ] **更广回归：**全流程、章节／换盘／存档兼容、音频／语言、手柄／震动、混合 DPI／全屏与多 GPU。不会仅因这些广泛目标尚未覆盖而把已完成的具体修复继续挂起。

## v0.8.0 计划

- [~] **P0：**共同时序契约已实现，Streamline／NGX 共存 Gate 1 尚未通过。
- [ ] **P3：**生产插帧呈现基础、资源租约与 UI 分离。
- [ ] **P4：**Windows Vulkan 固定 2× DLSS 插帧，包括 DLSS／DLAA／FSR 组合与安全暂停／恢复。
- [ ] **FSR 插帧：**独立目标，不预设 API、平台和倍率。
- [ ] **可选原生 120 FPS：**游戏独立帧呈现，不是生成帧；需验证节奏、Ring、音频与过场，默认保留 60 FPS。
- [ ] **Linux AArch64 与 macOS AArch64／Apple Silicon：**平台交付目标，尚不宣称官方包或实机验收。
- [ ] **移除 PM4 转换器：**已从 v0.9.0 提前至 v0.8.0，替代架构尚未决定。
- [ ] **实验性 Android：**探索目标，尚无 APK 或设备验证。

## 后续积压

D3D12 DLSS FG 与动态 MFG 保持延期，不作为 v0.8.0 必交目标。DX11、HDR 输出、高分辨率阴影、SSAO／深度访问、GI／反射、光追与暂停的 Switch 工作仍为独立计划。WMV 播放、临时主角伤害控制及其他未获完成证据的条目保留 Project 原范围，未直接标记完成。

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
