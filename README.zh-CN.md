<div align="center">

# Lost Odyssey Recomp

**《失落的奥德赛》Xbox 360 版的实验性原生 PC 移植。**

Windows x64 · Direct3D 12 · Vulkan · PowerPC 静态重编译

<img src="docs/images/title-screen.png" alt="失落的奥德赛标题画面 — Press START" width="960">

可选诊断默认关闭，也可在设置中关闭。详见[隐私说明](PRIVACY.zh-CN.md)。

### [下载最新版本](https://github.com/freefrank/LostOdysseyRecomp/releases/latest) · [安装指南](docs/INSTALLING.md) · [反馈问题](https://github.com/freefrank/LostOdysseyRecomp/issues)

[English](README.md) · [更新日志](CHANGELOG.md) · [Projects](https://github.com/users/freefrank/projects/3) · [从源码构建](docs/BUILDING.md)

</div>

> [!IMPORTANT]
> **本项目仍处于早期测试阶段。** 已测试开场区域和部分场景，尚未通关。渲染和稳定性仍有问题。请自行提供受支持版本的游戏文件。

## v0.5.4 新增

- 防止 PPC 源码生成使用过期输入、残缺输出或旧式 64 位跳转表 switch。
- 增加可选的 Win64 外部汇编分析器，支持离线报告。
- 延长大体积捕获的 F1 菜单 ZIP 归档等待时间，并修复安装器拖动分发。

更早版本的改动见[更新日志](CHANGELOG.md)。

## 开始游戏

1. **下载并完整解压** Windows 发布包，放在可写入的文件夹中。
2. **运行 `LostOdysseyRecomp.exe` 并按提示导入游戏文件**。支持已提取文件夹、`default.xex`、XDVDFS ISO 或 GOD 容器。
3. **选择语言和图形设置**，设置与着色器预编译完成后继续进入游戏。

发布包不需要安装 Python 或 Visual Studio。后续启动会复用着色器缓存；更新程序时请保留存档和档案文件夹。

更新器会检查 GitHub 最新 Release。数字版本更高时按通常规则更新；数字版本相同但 `-后缀` 不同（例如 `0.5.0-hotfix1`）时也会触发更新。已安装的程序必须先包含这项更新器逻辑，策略才会生效。

| 要求 | 支持范围 |
| :--- | :--- |
| 系统 | Windows x64、支持 AVX 的 CPU、Direct3D 12 或 Vulkan 图形驱动 |
| 游戏数据 | 已核对的 Europe, Asia 或 USA, Europe 版；启动需要 Disc 1 |
| 其他光盘 | 通过 `InstallGame.exe` 追加导入；后续光盘流程尚未完整验证 |

支持的光盘版本、文件位置和更新方式见[安装指南](docs/INSTALLING.md)。

## 实机画面

| Ring 战斗 | 城市探索 |
| :---: | :---: |
| ![凯姆攻击时的 Ring 判定界面](docs/images/ring-battle.png) | ![工业城市探索场景](docs/images/city-exploration.png) |

*截图来自 v0.1 发布前的开发构建，未经修图。*

## 当前功能

| 功能 | 说明 |
| :--- | :--- |
| 游戏导入器 | 支持文件夹、XEX、ISO 和 GOD；复制原始文件 |
| 首次启动设置 | 游戏初始化前选择语言和图形选项 |
| 语言设置 | 英语、日语、韩语、繁体中文、简体中文界面，以及游戏语言选择 |
| 图形设置 | 最高 4K 的 Auto／手动内部分辨率、Off／FXAA／SMAA／实验性 TAA、标准／高质量滤波、30／60 FPS 及输出／显示控制；全屏和跨 DPI 仍需更多测试 |
| 设置菜单 | 原版字体与菜单风格；图形设置单击保存并应用，需要重启时选择 Now/Later |
| 着色器预编译 | 内置资源索引、多线程编译、缓存复用 |
| CPU 使用率 | 减少不必要的轮询，复用渲染计算 |
| 输入与调试 | 手柄和键盘输入；英文／简体中文 F1 菜单提供捕获、地图信息与同地图 POI 传送 |

打开 `InstallGame.exe`，选择 **Files** 或 **Folder**，即可导入游戏光盘和受支持的 DLC。见[安装说明](docs/INSTALLING.md#automatic-content-import)。

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

| 动作 | 键盘 |
| :--- | :--- |
| Start / Back | Enter / Backspace |
| A / B / X / Y | Z / X / A / S |
| 十字键 / 左摇杆 | 方向键 / I、J、K、L |
| 左 / 右肩键 | Q / W |
| 左 / 右扳机 | E / R |
| 调试菜单 | F1 |

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

[路线图](docs/ROADMAP.zh-CN.md) · [接手入口](docs/notes/handoff.md) · [渲染测试](docs/notes/rendering-validation.md) · [音频](docs/notes/audio-output.md) · [归档](docs/archive/README.md)

</details>

## 致谢与游戏数据

本项目参考 [UnleashedRecomp](https://github.com/hedge-dev/UnleashedRecomp)、[re:Blue](https://github.com/zolaware/reblue)、[XenonRecomp](https://github.com/hedge-dev/XenonRecomp)、[XenosRecomp](https://github.com/hedge-dev/XenosRecomp)、[plume](https://github.com/renderbag/plume) 和 [Xenia](https://github.com/xenia-project/xenia)。音频采用固定版本的 [Xenia FFmpeg 分支](https://github.com/xenia-project/FFmpeg)，已附[许可证](thirdparty/ffmpeg-LICENSE.txt)。

《失落的奥德赛》及其资产归各自权利人所有，本项目为非官方移植。请从自己拥有的光盘提取数据，不提交游戏程序、资源包、纹理、音视频、生成的游戏代码或捕获数据。依赖保留各自许可证。
