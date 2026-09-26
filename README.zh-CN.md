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

### 超分辨率（DLSS 与 FSR）

Windows 与 Linux 支持实验性的 NVIDIA DLSS（超分辨率与 DLAA）和 AMD FSR 3.1 超分辨率选项，技术细节与验证边界详见[开发状态](docs/STATUS.md)。当 DLSS 或 DLAA 不可用或被禁用时，已保存的 TAA 选择自动回退至 SMAA，其余抗锯齿选择保持不变。

### 自动 PlayStation 手柄按键提示

项目基于 SDL 自动识别最近活动的手柄并动态切换按键提示：
- **按键提示切换**：覆盖 ABXY 动作键、肩键（LB/RB/LT/RT 对应 L1/R1/L2/R2）及暂停菜单 Start/Select（Options/Share 与 Options/Create），覆盖宿主界面（设置菜单、安装器、调试覆盖层）与客端游戏（暂停菜单及过场动画）。
- **技术文档**：客端纹理替换、图集内容哈希匹配与 GPU 上传生命周期细节归档于 [Issue #40 界面资源映射](docs/notes/issue-40-ui-resource-map.zh-CN.md)及[开发状态](docs/STATUS.md)。
- **状态与验收**：功能已通过用户在暂停菜单与过场动画的实机验收，已纳入 v0.7.0 发布版。
- **边界说明**：用户实机验收覆盖实测手柄与场景，不代表所有手柄硬件或全流程通关覆盖。Issue #40 同时提及的 Mod 支持本次未做，不视为该 Issue 整体完成。

## 路线图

**v0.7.0** 版本已发布，包含性能优化、易用性改进、PlayStation 手柄按键提示、v1 图像 Mod API 以及原生 DLSS/DLAA 和 FSR 超分辨率。已规划至 **v0.8.0** 的路线图目标包括 DLSS 插帧（Windows Vulkan 下固定 2× DLSS FG）、独立 FSR 插帧、原生独立 120 FPS 候选评估（`LO_EXPERIMENTAL_120=1`，评估原生呈现节奏）、移除既有 PM4 数据包转换层、Linux AArch64、macOS AArch64（Apple Silicon）以及实验性 Android 支持。上述内容均属于路线图规划目标，不代表当前已有实现或已验证覆盖；详情参见[路线图](docs/ROADMAP.zh-CN.md)。

历史版本发布说明与详细变更记录见[更新日志](CHANGELOG.md)。

## 开始游戏

1. **下载并完整解压** Windows 发布包，放在可写入的文件夹中。
2. **运行 `LostOdysseyRecomp.exe` 并按提示导入游戏文件**。支持已提取文件夹、`default.xex`、XDVDFS ISO 或 GOD 容器。
3. **选择语言和图形设置**，设置与着色器预编译完成后继续进入游戏。

发布包不需要安装 Python 或 Visual Studio。后续启动会复用着色器缓存；更新程序时请保留存档和档案文件夹。

已发布的更新器会检查 GitHub 最新 Release：数字版本更高时更新，数字版本相同但 `-后缀` 不同时也会触发更新。更新器另外支持从只有 updater 的空目录以及过期或损坏的本地 metadata 恢复。更新成功后，helper 会询问是否启动游戏，默认选择**否**；silent 运行会完成更新但不启动游戏。下载完整性校验、安全解压和回滚仍然保留。

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

发布包通过 `LostOdysseyRecomp.exe` 的 **Files** 或 **Folder** 导入游戏光盘和受支持的 DLC。当前开发版本新增 **Gameplay → 导入光盘与 DLC** 入口，可重新打开导入器并替换所选光盘与 DLC；此菜单入口尚未发布。验证边界见[安装说明](docs/INSTALLING.md#automatic-content-import)和[开发状态](docs/STATUS.md)。

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
