<div align="center">

# Lost Odyssey Recomp

**《失落的奥德赛》Xbox 360 版的实验性原生 PC 移植。**

Windows x64 · Direct3D 12 · Vulkan · PowerPC 静态重编译

<img src="docs/images/title-screen.png" alt="失落的奥德赛标题画面 — Press START" width="960">

可选诊断默认关闭，也可在设置中关闭。详见[隐私说明](PRIVACY.zh-CN.md)。

### [下载最新版本](https://github.com/freefrank/LostOdysseyRecomp/releases/latest) · [安装指南](docs/INSTALLING.md) · [反馈问题](https://github.com/freefrank/LostOdysseyRecomp/issues)

[English](README.md) · [更新日志](CHANGELOG.md) · [开发工具](tools/README.md) · [Projects](https://github.com/users/freefrank/projects/3) · [从源码构建](docs/BUILDING.md)

</div>

> [!IMPORTANT]
> **本项目仍处于早期测试阶段。** 已测试开场区域和部分场景，尚未通关。渲染和稳定性仍有问题。请自行提供受支持版本的游戏文件。

### 实验性原生 DLSS SR 与 DLAA

原生 NVIDIA DLSS 超分辨率与 DLAA 属于实验性功能。
Windows 与 Linux 正式发布包中已内置官方 NGX 运行库；画质与稳定性仍在验证中。

### 实验性可选 FSR 超分

源码构建与发布打包工作流支持在 Windows 与原生 Linux 通过 `LO_ENABLE_FSR=ON` 和
`LO_REQUIRE_FSR=ON` 启用固定 FidelityFX SDK v1.1.4（FSR 3.1.4 实现）的实验性路径。
Quality、Balanced、Performance、Native AA 均有 Windows 运行证据；原生 Linux RADV 已有 Quality、Performance 和 Native AA 的运行证据。
实测的 Windows 渲染画面与 Linux 运行验证（AMD Radeon 8060S RADV STRIX_HALO）均已获用户验收。
全场景画质覆盖与 DLSS/FSR 帧生成仍待开展。低功耗硬件要求按用户授权通过 APEX 15W 代理设备验证，不代表 Steam Deck 硬件等价。

## v0.6.19 发布版

已发布 [v0.6.19](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.19)（于 2026-09-25T04:18:21Z 从 source `1b2ea6635c5ac4f7cf3c9186fda3cd05575db97d` 通过 Release CI [36092250520](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/36092250520) 发布为最新公开版本）。整合了游戏内实时各向异性过滤（Off / 2x / 4x / 8x / 16x，保存后生效无需重启）及基于不可变代际管理和生命周期同步的采样器表重构、图形设置菜单 DLSS/FSR 画质档位排序与整合抗锯齿交互、避免每次导航重复进行整屏背景软件滤波的静态菜单装饰缓存、经实机验收的原生系统菜单“退出到桌面”`SDL_QUIT` 路由、设置菜单“退出到主菜单”标题跳转、调试“随时存档”跨进程状态持久化、作弊菜单侧栏 LT/RT 手柄分类切换与底栏提示、鼠标闲置自动隐藏、游戏窗口输入法按键拦截修复，以及开发者工具与测试套件全景索引。独立 CPU 契约、Linux 软件 Vulkan 仿真以及限定的实体手柄与原生退出测试均已通过；全游戏实机 GPU 场景验证、其余语言逐项实机确认以及缺少 mip 链导致的远景闪烁修复仍待推进。详见[更新日志](CHANGELOG.md#v0619--2026-09-25)与[开发状态](docs/STATUS.md)。

## v0.6.15 发布版

已发布 [v0.6.15](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.15)（于 2026-09-24T19:47:35Z 从 source `6eef30d257f2e14ce30a546217574a0dc74fad69` 通过 Release CI [36044604844](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/36044604844) 发布为最新公开版本）。在既有官方 NVIDIA NGX DLSS 310.9.1 基础上，整合了官方 FidelityFX SDK v1.1.4（FSR 3.1.4 实现）的发布构建支持。包含原生 DLSS SR/DLAA 时序生命周期修复（BR-01、BR-02）、带状态校验反馈的图形菜单稳定性重构（BR-03、`GraphicsRow`）、F1 呈现前最终交换链截图与同步 NGX Evaluate 输入/输出捕获、FSR 暂态输入拒绝恢复与 UMA 显存分配支持、安装包内直接内置 28,527 项便携式 Vulkan 着色器整合（用户无需额外下载独立着色器包），以及包含许可证检查的发布与 CI 工作流。测试的 Windows 渲染画面与 Linux 运行验证均已获用户验收；广泛场景覆盖、确定性生产渲染器故障注入序列以及 DLSS/FSR 帧生成仍属于实验性未决阶段。详见[更新日志](CHANGELOG.md#v0615--2026-09-24)与[开发状态](docs/STATUS.md)。

## v0.6.11 发布版

已发布 [v0.6.11](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.11)（于 2026-09-22T06:44:37Z 从 source `3daba37` 通过 Release CI [35687931776](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35687931776) 发布）。引入实验性原生 NVIDIA DLSS 超分辨率（SR）与 DLAA 支持、游戏内图形设置缩放技术选项、长列表视口滚动以及 Start/Enter 聚焦“保存”且不立即保存的功能。Windows 与 Linux 发布包已内置官方 NVIDIA NGX 运行库。画质、运动响应及玩家验收均未宣称完成。详见[更新日志](CHANGELOG.md#v0611--2026-09-22)与[开发状态](docs/STATUS.md)。

## v0.6.7 发布版

已发布 [v0.6.7](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.7)（于 2026-09-20T20:09:28Z 从 source `f92c24d` 通过 Release CI [35533399325](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35533399325) 发布为最新公开版本）。在游戏内图形设置中增加“宽屏”开关并扩充 21:9 分辨率预设（1720×720、2560×1080、3440×1440、3840×1600、5120×2160），比例切换时按垂直高度最近匹配，自动推导识别旧配置，并在 5 种语言中同步更新首次启动设置向导。Issue #17 已解决并关闭。

> [!WARNING]
> **超宽屏支持在多样化硬件与多分辨率组合下仍处于实验性阶段（EXPERIMENTAL）。**

验证边界详见[更新日志](CHANGELOG.md#v067--2026-09-20)与[开发状态](docs/STATUS.md)。

## v0.6.6 发布版

已发布 [v0.6.6](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.6)（于 2026-09-20 从 source `c6cbd1f` 通过 Release CI [35527543573](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35527543573) 完成同版本重新发布）。包含原生超宽屏 (21:9) 初始支持（Issue #17）、针对所有比例与高内部分辨率的阴影贴图渲染修复，以及 Linux AppImage 更新器保留回滚的清理逻辑。

阴影修复修正了 effective-height 渲染目标缓存以及模式 4 与 5 的仅深度光栅化；受影响场景经用户实机测试确认阴影已恢复正常。重新发布的资产已核验并上传；初版 `c953bb5` 资产已被替代，已下载旧版本的用户需重新下载以获取修复。

验证边界详见[更新日志](CHANGELOG.md#v066--2026-09-20)与[开发状态](docs/STATUS.md)。

## v0.6.3 发布版

已于 2026-09-19T23:56:08Z 发布到 [GitHub Release v0.6.3](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.3)。大顶点缓存命中恢复为有界采样比较，以降低 CPU 比较成本；小顶点缓冲和 index cache 源数据校验继续保持精确。`LoVertexCacheTest` 通过 3,668,957 项定向检查；尚未宣称发布二进制性能或全游戏结果。本版本还包含 Issue #54 语言菜单安全修正、Issue #53 文件 I/O 锁范围修正和有界诊断、确定性的 I/O 生命周期回归覆盖，以及使用平台归档格式的异步 F1 渲染状态导出。Windows ZIP、Linux AppImage 及其 sidecar 已通过包 hash 和公开交付核验。Linux 原生 GPU、Steam Deck、AppImage 运行时及更广游戏流程仍待完成。详见[更新日志](CHANGELOG.md#v063--2026-09-19)。

## v0.6.2 发布版

已发布版本：[v0.6.2](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.2)。Windows ZIP 和 Linux AppImage 均内置 shader 集合；本版本没有单独 shader 发布包。v0.6.2 包含实验性几何运动矢量 replay，以及已接受的主路径 TAA 策略。正常 TAA 路径使用 0.5 抖动幅度、静止运动 snap、静止颜色裁剪和多表面 history，RGBA8 history 权重为 `31/33`；实验性 FP16 history 和 moving bilinear fallback 仍关闭。在 RTX 5080 的 Vulkan、Uhra 4K 同一场景中，用户以约 60 FPS 接受了画面质量。这是限定场景和本机的证据，不代表全游戏或跨平台验收。

隐藏静音、无 pacing 的 A-B-A-B 对照中，候选为 60.34/59.00 FPS，独立 Release 构建为 54.61 FPS，之前的 RelWithDebInfo 主程序为 54.57 FPS。1080p internal 到 4K output 的移动相机限制、更广场景覆盖、D3D12 replay PSO 后续工作、Linux 原生 GPU 和 Steam Deck 验证仍开放。

## v0.6.1 发布版

已发布版本：[v0.6.1](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.1)。Windows 和 Linux 现在会在导入游戏资料前检查更新。发现新版本时，带有应用品牌的提示会显示发布说明以及“安装”或“稍后”操作；接受后在导入前应用更新并重新启动。下载进度仍使用现有的更新器窗口。

[Release CI 35374267882](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35374267882) 的 Windows/Linux 发布任务及定向回归均通过。在线更新接受、实体手柄输入和网络下载仍未验证。

## v0.6.0 发布版

已发布版本：[v0.6.0](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.0)。提供 Windows x64 与原生 Linux x64 发布包。Windows ZIP 与 Linux AppImage 内置便携式 Vulkan 着色器包，让受支持的安装首次进入游戏时不再经历冗长的编译过程。着色器包覆盖当前测试过的集合，遗漏的着色器仍会按需编译，可能造成短暂卡顿。

- **大幅性能与稳定性改进**：修复 shader 与管线准备、等待与线程生命周期、呈现、时钟、更新器、几何缓存和 Linux 运行路径。此前 15W 测试使用了抽样缓存匹配，不能作为 0.6.0 最终 FPS 证据；Steam Deck 实机验收仍待完成。
- **原生 Linux 发布**：发布范围包含 Vulkan ELF 与 AppImage。当前 Linux 验证覆盖 WSL2 Mesa Dozen；原生 Linux GPU、AppImage 更新事务、Steam Deck 和全流程游戏仍未验证。
- **加固导入器**：资源只有在最终 `write`、`flush`、`close` 均成功后才会发布；XDVDFS 扫描按 2048 字节边界进行；目标目录页支持通过按钮、`F2` 或手柄 `Y` 创建并进入文件夹。
- **真实资料验证**：导入器识别了 `G:/ROMS/US` 下全部四张 USA/Europe 光盘镜像，并成功完成隔离的 Disc 1 导入。四盘完整安装、交互 UI 验收和游戏运行仍未验证。

下一阶段 **v0.7.0** 计划继续优化性能并加入 QOL 功能、DLSS/FSR Scaling、Frame Generation，同时发布 macOS 版本。

发布包和独立 shader pack 可从 [v0.6.0 发布页](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.0)下载。Release CI 已通过必要的审计和 Windows/Linux 打包门槛，公开资产已与 SHA-256 校验文件核对。原生 Linux GPU、Steam Deck、AppImage 更新事务和全流程游戏仍不在已验证范围内。

更早版本的改动见[更新日志](CHANGELOG.md)。

## 开始游戏

1. **下载并完整解压** Windows 发布包，放在可写入的文件夹中。
2. **运行 `LostOdysseyRecomp.exe` 并按提示导入游戏文件**。支持已提取文件夹、`default.xex`、XDVDFS ISO 或 GOD 容器。
3. **选择语言和图形设置**，设置与着色器预编译完成后继续进入游戏。

发布包不需要安装 Python 或 Visual Studio。后续启动会复用着色器缓存；更新程序时请保留存档和档案文件夹。

已发布的更新器会检查 GitHub 最新 Release：数字版本更高时更新，数字版本相同但 `-后缀` 不同时也会触发更新。v0.5.7 发布版另外允许从只有 updater 的空目录以及过期或损坏的本地 metadata 恢复。更新成功后，helper 会询问是否启动游戏，默认选择**否**；silent 运行会完成更新但不启动游戏。下载完整性校验、安全解压和回滚仍然保留。

| 要求 | 支持范围 |
| :--- | :--- |
| 系统 | Windows x64、支持 AVX 的 CPU、Direct3D 12 或 Vulkan 图形驱动 |
| 游戏数据 | 已核对的 Europe, Asia 或 USA, Europe 版；启动需要 Disc 1 |
| 其他光盘 | 通过 `LostOdysseyRecomp.exe` 内置导入器追加光盘或 DLC；后续光盘流程尚未完整验证 |

支持的光盘版本、文件位置和更新方式见[安装指南](docs/INSTALLING.md)。

## 实机画面

| Ring 战斗 | 城市探索 |
| :---: | :---: |
| ![凯姆攻击时的 Ring 判定界面](docs/images/ring-battle.png) | ![工业城市探索场景](docs/images/city-exploration.png) |

*截图来自 v0.1 发布前的开发构建，未经修图。*

## 当前功能

| 功能 | 说明 |
| :--- | :--- |
| 游戏导入器 | 支持文件夹、XEX、ISO 和 GOD；原始资源不改动，暂存复制会在发布前检查最终写入结果 |
| 首次启动设置 | 游戏初始化前选择语言和图形选项 |
| 语言设置 | 英语、日语、韩语、繁体中文、简体中文界面，以及游戏语言选择 |
| 图形设置 | Auto／手动内部分辨率（配置文件／兼容回退）、含宽屏开关的 16:9 / 21:9 分辨率预设、Off／FXAA／SMAA／实验性 TAA、缩放技术选项（关／DLSS，含质量／平衡／性能／DLAA）、标准／高质量滤波、30／60 FPS 及输出／显示控制；全屏和跨 DPI 仍需更多测试 |
| 设置菜单 | 原版字体、支持长列表滚动的菜单风格；图形设置单击保存并应用，支持按 Start/Enter 聚焦“保存”且不立即保存，需要重启时选择 Now/Later |
| 着色器预编译 | 内置便携式 Vulkan 着色器包（.lospv）、多线程自适应编译、即时跳过与缓存复用 |
| CPU 使用率 | 减少不必要的轮询，复用渲染计算 |
| 输入与调试 | 手柄和键盘输入；英文／简体中文游戏内浮层调试菜单（F1 或手柄 LB+RB）提供捕获、地图信息与同地图 POI 传送 |

发布包通过 `LostOdysseyRecomp.exe` 的 **Files** 或 **Folder** 导入游戏光盘和受支持的 DLC。资源缺失时可再次打开同一内置导入器。验证边界见[安装说明](docs/INSTALLING.md#automatic-content-import)和[开发状态](docs/STATUS.md)。

验证进展和剩余工作见[公开维护者 Project](https://github.com/users/freefrank/projects/3)。

<details>
<summary><strong>游戏版本与兼容性详情</strong></summary>

支持的两个版本对应 [Lost Odyssey (Europe, Asia) (En,Ja,Zh,Ko) (Disc 1)，Redump 39111](https://redump.info/disc/39111) 与 [Lost Odyssey (USA, Europe) (En,Ja,Fr,De,Es,It) (Disc 1)，Redump 11817](https://redump.info/disc/11817)。本文将前者简称亚洲版：Disc 1 的 Title ID 为 `4D5307FA`、Media ID 为 `39F7D748`、标题／基础版本为 `0.0.0.4`、XeMID 为 `MS204204H0X14`。USA, Europe 版 Disc 1 的 Media ID 为 `368DE6DD`、版本为 `0.0.0.3`、XeMID 为 `MS204203W0X14`。这些身份字段与已核对的两套数据一致；尚未进行整张 ISO 与 Redump 哈希的完整比对。导入器严格核对每盘受支持的 XEX 哈希，不能仅凭区域名称判断。

支持 **USA, Europe 0.0.0.3 四盘版本**，严格校验 XEX 并阻止不同版本混装。游戏语言按安装版本提供：USA, Europe 版为英／日／德／法／西／意，已核对的 Europe, Asia 资源保留英／日／韩／繁中／简中选项。详见[版本说明](docs/notes/europe-support.md)。

四盘全部导入后，游戏会自动读取所需光盘，无需手动换盘。详见[光盘处理说明](docs/notes/disc-selection.md)。

提供语言选项不代表每种语言都已通关验证。已核对集合之外的区域版本、Title Update 和修改后的 XEX 尚未验证，详见[版本证据](docs/notes/xex.md)。

</details>

<details>
<summary><strong>构建命令与仓库目录</strong></summary>

### 构建与运行

请按[构建指南](docs/BUILDING.md)准备自己的游戏数据、依赖及生成代码。辅助脚本自动查找工具，自定义安装位置可通过环境变量指定。

```powershell
.\tools\build_runtime.bat
$gameData = (Resolve-Path .\LostOdysseyRecompLib\private\disc1).Path
Push-Location .\out\build\windows-clang\LostOdysseyRecomp
.\LostOdysseyRecomp.exe --game $gameData --quiet-kernel
Pop-Location
```

保持启动工作目录一致，避免读到另一套存档或档案。

**启动与失败日志。** 正常启动会在工作目录写入 `logs/runtime-<timestamp>.log`，并同时输出到 `stderr`；设置 `LO_LOG_FILE=<path>` 可指定其他文件，设置 `LO_LOG_FILE=0` 可关闭重复文件输出。v0.5.11 还会记录 Windows build、进程／原生架构、source/build revision、PE 映像元数据、compiler、启动 memory baseline、GPU、原始 driver version、vendor/type 和 `reported_device_memory_bytes`。报告启动或渲染失败时，请附上当前 runtime log，并保留启动日志附近记录的 executable/source version、backend、GPU 和 driver 信息。诊断记录会保留原始 API code 及失败的资源或分配上下文，但这些记录本身不能确定根因。路径和保留规则见[构建与日志说明](docs/BUILDING.md)。

遇到画面问题时，请在问题出现时按 **F1**，选择**捕获渲染状态**。等待后台归档完成，并附上状态消息所示路径下的归档文件：Windows 生成 `.zip` 归档，Linux 生成 `.tar.gz` 归档。如果归档失败，原始捕获目录会保留，以便恢复。

| 动作 | 键盘 |
| :--- | :--- |
| Start / Back | Enter / Backspace |
| A / B / X / Y | Z / X / A / S |
| 十字键 / 左摇杆 | 方向键 / I、J、K、L |
| 左 / 右肩键 | Q / W |
| 左 / 右扳机 | E / R |
| 调试菜单 | F1 / 手柄 LB+RB |

SDL 已映射手柄与键盘可同时用于玩家 1。未映射摇杆需要 SDL 手柄映射。见[输入说明](docs/notes/controller-input.md)。

默认关闭震动，`LO_CONTROLLER_RUMBLE=1` 可开启。Ring 操作使用手柄右扳机或 R 键。

### 开发导航

| 目录 | 内容 |
| :--- | :--- |
| `LostOdysseyRecomp/` | 宿主内核、图形、音频、输入与调试 |
| `LostOdysseyRecompLib/` | 配置；Git 忽略的 `private/` 游戏数据和 `ppc/` 生成代码 |
| `tools/` | 重编译工具、依赖补丁、Ghidra 脚本，以及可选的[汇编采样分析器](tools/asm-profiler/README.zh-CN.md) |
| `thirdparty/` | 渲染、音频及其他依赖 |
| `docs/` | 当前状态、指南、逆向记录与历史归档 |

[路线图](docs/ROADMAP.zh-CN.md) · [接手入口](docs/notes/handoff.md) · [渲染测试](docs/notes/rendering-validation.md) · [TAA 实时调试](docs/TAA_LIVE_DEBUG.md) · [音频](docs/notes/audio-output.md) · [归档](docs/archive/README.md)

</details>

## 赞助者

感谢 **Cristian** 和 **Whitesun** 在 Ko-fi 上支持本项目。

## 致谢与游戏数据

本项目参考 [UnleashedRecomp](https://github.com/hedge-dev/UnleashedRecomp)、[re:Blue](https://github.com/zolaware/reblue)、[XenonRecomp](https://github.com/hedge-dev/XenonRecomp)、[XenosRecomp](https://github.com/hedge-dev/XenosRecomp)、[plume](https://github.com/renderbag/plume) 和 [Xenia](https://github.com/xenia-project/xenia)。音频采用固定版本的 [Xenia FFmpeg 分支](https://github.com/xenia-project/FFmpeg)，已附[许可证](thirdparty/ffmpeg-LICENSE.txt)。

《失落的奥德赛》及其资产归各自权利人所有，本项目为非官方移植。请从自己拥有的光盘提取数据，不提交游戏程序、资源包、纹理、音视频、生成的游戏代码或捕获数据。依赖保留各自许可证。
