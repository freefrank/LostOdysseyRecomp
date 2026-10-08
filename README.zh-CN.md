<div align="center">

<img src="assets/lost-odyssey-recomp.png" alt="Lost Odyssey Recomp 标志" width="112">

# Lost Odyssey Recomp

**《失落的奥德赛》Xbox 360 版的实验性原生 PC 移植。**

[![最新版本](https://img.shields.io/github/v/release/freefrank/LostOdysseyRecomp?label=%E6%9C%80%E6%96%B0%E7%89%88%E6%9C%AC)](https://github.com/freefrank/LostOdysseyRecomp/releases/latest)
[![下载量](https://img.shields.io/github/downloads/freefrank/LostOdysseyRecomp/total?label=%E4%B8%8B%E8%BD%BD%E9%87%8F)](https://github.com/freefrank/LostOdysseyRecomp/releases)
[![Star](https://img.shields.io/github/stars/freefrank/LostOdysseyRecomp?style=flat)](https://github.com/freefrank/LostOdysseyRecomp/stargazers)
[![许可证：GPL-3.0](https://img.shields.io/badge/%E8%AE%B8%E5%8F%AF%E8%AF%81-GPL--3.0-blue)](LICENSE)
[![最近提交](https://img.shields.io/github/last-commit/freefrank/LostOdysseyRecomp?label=%E6%9C%80%E8%BF%91%E6%8F%90%E4%BA%A4)](https://github.com/freefrank/LostOdysseyRecomp/commits/main)
[![未关闭的问题](https://img.shields.io/github/issues/freefrank/LostOdysseyRecomp?label=%E9%97%AE%E9%A2%98)](https://github.com/freefrank/LostOdysseyRecomp/issues)
[![Ko-fi 赞助](https://img.shields.io/badge/Ko--fi-%E8%B5%9E%E5%8A%A9-FF5E5B?logo=kofi&logoColor=white)](https://ko-fi.com/dotslash)
[![Discord](https://img.shields.io/badge/Discord-%E5%8A%A0%E5%85%A5-5865F2?logo=discord&logoColor=white)](https://discord.gg/z2yPct6z2w)

![Windows x64](https://img.shields.io/badge/Windows-x64-0078D6)
![Linux x64](https://img.shields.io/badge/Linux-x64-FCC624?logo=linux&logoColor=black)
![macOS arm64（实验性）](https://img.shields.io/badge/macOS-arm64%EF%BC%88%E5%AE%9E%E9%AA%8C%E6%80%A7%EF%BC%89-000000?logo=apple&logoColor=white)
![Android arm64（实验性）](https://img.shields.io/badge/Android-arm64%EF%BC%88%E5%AE%9E%E9%AA%8C%E6%80%A7%EF%BC%89-3DDC84?logo=android&logoColor=white)
![Direct3D 12](https://img.shields.io/badge/Direct3D-12-5E5E5E)
![Vulkan](https://img.shields.io/badge/Vulkan-AC162C?logo=vulkan&logoColor=white)
![Metal](https://img.shields.io/badge/Metal-147EFB)

### [下载](https://github.com/freefrank/LostOdysseyRecomp/releases/latest) · [安装指南](docs/INSTALLING.zh-CN.md) · [English](README.md)

[更新日志](CHANGELOG.md) · [反馈问题](https://github.com/freefrank/LostOdysseyRecomp/issues) · [Discord](https://discord.gg/z2yPct6z2w) · [项目看板](https://github.com/users/freefrank/projects/3) · [从源码构建](docs/BUILDING.md)

</div>

> [!IMPORTANT]
> **本项目仍处于早期测试阶段。** 已测试开场区域和部分场景，尚未完整通关。渲染和稳定性仍有问题。请自行提供受支持版本的游戏文件。

## 目录

- [开始游戏](#开始游戏)
  - [macOS（实验性）](#macos实验性)
  - [Android（实验性）](#android实验性)
  - [HDR（实验性）](#hdr实验性)
  - [最新更新](#最新更新)
- [当前功能](#当前功能)
- [操作按键](#操作按键)
- [调试菜单](#调试菜单)
  - [Overview：捕获与游戏操作](#overview捕获与游戏操作)
  - [Teleport：当前地图内移动](#teleport当前地图内移动)
  - [Cheats：快进与游戏数据工具](#cheats快进与游戏数据工具)
- [文件与目录](#文件与目录)
- [命令行参数](#命令行参数)
- [反馈问题](#反馈问题)
- [实机画面](#实机画面)
- [开发导航](#开发导航)
- [赞助者](#赞助者)
- [致谢与游戏数据](#致谢与游戏数据)

## 开始游戏

从[最新发布页](https://github.com/freefrank/LostOdysseyRecomp/releases/latest)选择对应平台的安装包。当前已发布版本为 **v0.8.53**。

| 平台 | 安装包 | 首次启动 |
| :--- | :--- | :--- |
| Windows x64 | `LostOdysseyRecomp-windows-x64-v0.8.53.zip` | 把整个 ZIP 解压到可写目录，运行 `LostOdysseyRecomp.exe`。需要支持 AVX 的 CPU。 |
| Linux x64 | `LostOdysseyRecomp-linux-x64-v0.8.53.AppImage` | 用 `chmod +x` 加上执行权限后运行。 |
| Linux x64 | `LostOdysseyRecomp-linux-x64-v0.8.53.flatpak` | 先安装 Freedesktop 26.08 运行时，再安装这个 bundle（[安装命令](docs/INSTALLING.zh-CN.md#flatpak)）。 |
| macOS arm64（实验性） | `LostOdysseyRecomp-macos-arm64-v0.8.53.dmg` | 把 `LostOdysseyRecomp.app` 拖到“应用程序”。需要 macOS 15 或更高版本的 Apple Silicon Mac。首次启动见 [macOS](#macos实验性)。 |
| Android arm64（实验性） | `LostOdysseyRecomp-android-arm64-v0.8.53.apk` | 安装 APK 后先打开一次。需要支持 Vulkan 的 64 位 Android 8.0 及以上设备。见 [Android](#android实验性)。 |

1. **导入游戏数据。** 找不到游戏时会打开导入器。用 **Files** 或 **Folder** 选择已提取的游戏文件夹、`default.xex`、ISO 或 GOD 数据。
2. **选择语言和图形设置。** 首次启动时游戏会询问是否下载所选渲染器的预编译着色器；选择跳过则在本机编译一次。
3. **需要时再追加其他光盘和 DLC：** 设置中的 **Gameplay → Import discs & DLC（导入光盘与 DLC）**。四张光盘都导入后，游戏会自动换盘。

启动需要 Disc 1。请使用受支持的四盘套装（亚洲多语言版或 USA/Europe 版），不要混装不同版本；[安装指南](docs/INSTALLING.zh-CN.md)介绍如何核对。更新时请保留存档和个人配置。

### macOS（实验性）

应用未经公证，macOS 会拦截首次启动。先尝试打开一次，再到 **系统设置 → 隐私与安全性** 点击 **仍要打开**。更新检查只会打开发布页，更新时需要自己替换应用。目前只在一台 Mac 上运行过。[详细步骤](docs/INSTALLING.zh-CN.md#macos)。

### Android（实验性）

- **游戏数据：** 打开应用会建好 `Android/data/io.github.freefrank.lostodyssey/files/game/`。用 USB 把解出的 `disc1`–`disc4` 复制进去（约 20 GB），或者在设备上用 **Game folder → Import disc images…** 导入光盘镜像。**CTRL → Game folder** 也可以让游戏改用任意文件夹或 SD 卡。
- **高通设备：** 首次启动前会打开 **GPU driver** 页面，因为手机自带的驱动会让部分菜单文字不可见。在那里下载 Turnip 驱动。如果某个驱动无法运行游戏，页面会说明原因。
- **操作：** 连接手柄后触摸按键会自动隐藏。**CTRL** 可以调整触摸按键的大小、透明度和布局。CTRL 本身也能在布局编辑里移动，透明度跟随设置。几秒不碰它，它会缩到最近的屏幕边缘，点一下边缘的小块即可恢复。
- **更新：** 应用启动时检查更新；新 APK 直接覆盖安装，存档保留。
- **存档：** **CTRL → Saves** 可以把存档导出成 ZIP，也能导入来自电脑、Xenia 或 RGH 存档转换器的 ZIP。
- **反馈问题：** 附上 `Android/data/io.github.freefrank.lostodyssey/files/logs/` 里的文件（[方法](docs/INSTALLING.zh-CN.md#android-logs)）。

目前只在一台平板上测试过。[详细步骤](docs/INSTALLING.zh-CN.md#android)。

### HDR（实验性）

在图像设置中开启 **HDR** 并保存，立即生效（开启插帧时需要重启）。**HDR 最高亮度**会打开校准页：左边是 SDR 预览，右边是 HDR，按 **LB / RB** 在游戏画面和测试图案之间切换。**Auto（自动）** 使用显示器回报的亮度，没有回报时按 1000 nit。HDR 可以和任一抗锯齿模式及超分一起使用；开启插帧时只有 Vulkan 上的 DLSS 插帧保持 HDR。支持 Windows（Direct3D 12 和 Vulkan）、Linux 和 macOS。

### 最新更新

[v0.8.53](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.8.53) 减少第一次进地图和战斗时的卡顿：Vulkan 用预先建好的部件链接新管线，每个场景在读盘时建好已知会用到的管线，游戏还会下载一份预先录好的地图、过场和战斗管线列表。另外新增 5.1 环绕声输出（#174）、手柄震动强度（#198）、选择 GPU 和显示器（#202、#201），调试菜单的快进设置会保留（#104），并去掉了独占全屏。[v0.8.44](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.8.44) 让 Android 的状态栏和导航栏保持隐藏，并让 **CTRL** 按钮可以拖动。更早的版本见[更新日志](CHANGELOG.md)。

## 当前功能

| 功能 | 内容 |
| :--- | :--- |
| 导入 | 支持文件夹、XEX、ISO、GOD、DLC 和替换光盘。原始文件保持不变。 |
| 语言 | 菜单提供英语、日语、韩语、繁体中文和简体中文。游戏语言取决于你的版本。 |
| 显示 | 窗口或全屏；电脑有多台显示器或多张显卡时，可选择显示器和 GPU。16:9 和 21:9 分辨率；16:10、4:3 等更高的屏幕会被 3D 画面铺满。Off／FXAA／SMAA／TAA（实验性），DLSS、FSR 3.1、XeSS（Windows Direct3D 12）或 MetalFX 超分，以及 [HDR](#hdr实验性)。图像设置中的**亮度 / Gamma** 可以对照游戏默认画面调整，参考画面是最后的游戏场景或测试图案。 |
| 阴影与环境光遮蔽 | 阴影分辨率 1×／2×／4×，实验性 SSAO／GTAO。 |
| 帧率 | 30／60／90／120 FPS 目标，以及 FreeSync／G-SYNC Compatible VRR。 |
| 插帧 | Windows Direct3D 12：DLSS（显卡支持的倍率）、FSR 2× 或 XeSS 2×。Windows Vulkan：DLSS 2×–6×。切换插帧方案可能需要重启。 |
| 着色器 | 首次启动可下载预编译着色器；否则在本机编译一次并缓存。 |
| Mod | 纹理、菜单和字体替换，以及 PlayStation 按键提示。见 [Mod 指南](docs/wiki/Modding.md)。 |
| 声音 | 立体声或 5.1 环绕声输出；音频设备不足 6 声道时回到立体声。 |
| 输入 | 手柄、键盘和震动；Android 上有触摸按键。 |
| 调试菜单 | 渲染捕获、随时存档、不遇敌、一步一遇敌、传送、快进和修改。见[调试菜单](#调试菜单)。 |

完整通关、后续光盘、Linux 和 macOS 硬件、全屏和混合 DPI 显示器仍需测试。后续计划见[路线图](docs/ROADMAP.zh-CN.md)。

## 操作按键

手柄和键盘可以同时用于玩家 1。如果手柄无法识别，见[输入说明](docs/notes/controller-input.md)。

| 游戏操作 | 键盘 |
| :--- | :--- |
| Start／Back | Enter／Backspace |
| A／B／X／Y | Z／X／A／S |
| 十字键／左摇杆 | 方向键／I、J、K、L |
| 左／右肩键 | Q／W |
| 左／右扳机 | E／R |
| 调试菜单 | F1 |

按 **Back** 切换小地图缩放。按住约半秒可隐藏小地图，再按一次恢复显示。

Ring 操作用手柄**右扳机**或键盘 **R**。声音设置中的**震动**可调整震动强度，调到最小即关闭震动。

## 调试菜单

按 **F1**，或手柄 **LB+RB**（PlayStation 布局为 **L1+R1**）打开或关闭调试菜单。**菜单打开时游戏会暂停。** 菜单分为 **Overview（概览）**、**Teleport（传送）** 和 **Cheats（修改）** 三页，用键盘或手柄操作，不支持鼠标。

| 操作 | 键盘 | 手柄 |
| :--- | :--- | :--- |
| 选择项目 | ↑／↓ | 十字键上／下 |
| 修改数值 | ←／→ | 十字键左／右 |
| 确认 | Enter | A |
| 返回或关闭 | Esc | B |
| 上一页／下一页 | Q 或 Tab／E | LB／RB |
| 切换 Cheats 类别 | 选中类别行后按 ←／→ | LT／RT |
| 打开或关闭菜单 | F1 | LB+RB |

### Overview：捕获与游戏操作

**Overview** 显示当前地图，并提供菜单语言、**Capture render state（捕获渲染状态）**、**Save Anywhere（随时存档）**、**No Random Encounters（不遇敌）**、**Encounter Every Step（一步一遇敌）**，以及直接赢下当前战斗的操作。

遇到画面问题时，选择 **Capture render state** 并确认，然后**关闭菜单**让渲染继续。捕获结果会归档到 `captures/` 并显示路径，内含截图、渲染数据和日志，分享前请检查内容。

**Save Anywhere** 会开放原作的 **System → Save（系统 → 存档）**。关闭调试菜单后，打开游戏的 System 菜单存档。

**No Random Encounters** 会停止场景中的随机战斗。剧情战斗照常发生。

旁边的 **Encounter Every Step** 会让有随机战斗的区域每走一步都遇敌。重启游戏后自动关闭；这两个开关打开其中一个时，另一个会自动关闭。

> [!WARNING]
> **请另外保留正常存档。** 分队期间用随时存档保存的存档，读档后可能丢失 RB 换人功能（[#74](https://github.com/freefrank/LostOdysseyRecomp/issues/74)）。现在分队期间随时存档保持关闭；读取以前这类存档后，可用 F1 菜单中的 **Force RB Party Switch（强制开启 RB 换人）** 恢复。

### Teleport：当前地图内移动

**Teleport** 提供位置书签、可编辑的 X／Y／Z 坐标和当前地图的兴趣点。在坐标行按 **Enter** 选择 X、Y 或 Z，再按 **←／→** 调整。确认传送或某个兴趣点后，关闭菜单即可移动。

页面底部的 **Debug Event Room（调试事件房）** 会在关闭菜单后跳到游戏自带的事件调试图（z0g_9）。在那里按住 **LB** 再按**上**打开 Scenario Jump。只有在地图上操控角色时可用。

### Cheats：快进与游戏数据工具

| 类别 | 功能 |
| :--- | :--- |
| **Quick tools** | 快进、**Allow memory edits（允许内存修改）**、金币和 HP／MP。 |
| **Characters** | HP／MP、EXP（0–99，不是等级）和技能。 |
| **Inventory** | 把物品和素材设为 1、10、50 或 99，或填满整类。显示未刷新时，在游戏背包里整理一次。 |
| **Equipment** | 实验性的武器、指环和饰品修改。 |
| **Party** | 实验性的队伍成员、前后排和场景角色。部分修改需要重新读档。 |
| **Developer** | 实验性的原版 **EDIT MENU** 入口：开启后关闭 F1，按 **LT+RT**。用完后关闭此选项。 |

**快进**需要手柄或 Android 的触摸按键：按住 **LT**（**Hold**）或按一下切换（**Toggle**），倍率 2×–8×。菜单打开时暂停加速。快进的选择在重启后会保留。

**内存修改**默认关闭。先备份存档，并在战斗外能操控角色的地方使用。开启 **Allow memory edits**，选择操作并确认 **Yes**，然后关闭 F1 让它执行。修改后的数值可能随正常存档保存。

## 文件与目录

Windows ZIP 是**便携式**的，所有文件都留在解压目录里。AppImage、Flatpak 和 macOS 应用把文件放在当前用户的目录：

| 安装包 | 设置 | 存档、缓存和游戏数据 | 日志 |
| :--- | :--- | :--- | :--- |
| Windows ZIP | `LostOdysseyRecomp.exe` 所在目录 | 同左 | 同左 |
| Linux AppImage | `~/.config/lost-odyssey-recomp/` | `~/.local/share/lost-odyssey-recomp/` | `~/.local/state/lost-odyssey-recomp/` |
| Linux Flatpak | `~/.var/app/io.github.freefrank.LostOdysseyRecomp/config/lost-odyssey-recomp/` | `~/.var/app/io.github.freefrank.LostOdysseyRecomp/data/` | `~/.var/app/io.github.freefrank.LostOdysseyRecomp/.local/state/lost-odyssey-recomp/` |
| macOS 应用 | `~/Library/Application Support/LostOdysseyRecomp/` | 同左 | `~/Library/Logs/LostOdysseyRecomp/` |
| Android | 应用内部 | 应用内部；游戏数据在 `Android/data/io.github.freefrank.lostodyssey/files/game/` | `Android/data/io.github.freefrank.lostodyssey/files/logs/` |

| 内容 | 位置 | 说明 |
| :--- | :--- | :--- |
| 存档 | `save/` | 更新时保留。可以[导入](docs/INSTALLING.zh-CN.md#importing-saves) Xenia 和 Xbox 360 存档。 |
| 个人配置 | `profile/` | 更新时保留。 |
| 设置 | `settings.ini` | 删除后恢复默认设置。 |
| 导入的游戏 | `game/`，内含 `disc1/`–`disc4/` 和 `dlc/` | 导入器的默认位置；导入到别处时由 `game-path.txt` 记录。 |
| 着色器缓存 | `cache/shaders/` | 删除后会重新生成。 |
| 下载的着色器 | `shaders/` | |
| 日志 | `logs/runtime-*.log` | 保留最近三次运行。 |
| 渲染捕获 | `captures/` | |
| Mod | `mods/` | |

详见[文件位置](docs/INSTALLING.zh-CN.md#file-locations)。

## 命令行参数

| 参数 | 作用 |
| :--- | :--- |
| `--game <路径>` | 使用指定的游戏文件夹（含 `default.xex` 或 `disc1/`）或 `default.xex` 文件，跳过导入器。 |
| `--install` | 即使已经设置好游戏也打开导入器。 |
| `--setup` | Windows：重新运行首次启动设置，然后进入游戏。 |
| `--prepare-shaders-only` | 预先准备全部着色器，然后不启动游戏直接退出。 |

`--game <路径>` 要写成两个参数，`--game=<路径>` 会被忽略。程序不会向控制台输出内容，请查看日志。

```bash
LostOdysseyRecomp.exe --game "D:\Games\Lost Odyssey"
./LostOdysseyRecomp-linux-x64-v0.8.53.AppImage --game ~/Games/LostOdyssey
flatpak run io.github.freefrank.LostOdysseyRecomp --game ~/Games/LostOdyssey
```

| 环境变量 | 作用 |
| :--- | :--- |
| `LO_GRAPHICS_API` | Windows 上为 `d3d12` 或 `vulkan`。 |
| `LO_FPS` | 帧率上限；`0` 表示不限制。 |
| `LO_NO_UPDATE=1` | 跳过更新检查。 |
| `LO_MODS=0` | 关闭 Mod。 |
| `LO_AUDIO_MUTE=1` | 静音。 |
| `LO_CONTROLLER_RUMBLE=0` | 关闭震动。 |
| `LO_OPTISCALER_PATH` | 实验性：在 Windows 上加载自备的 `OptiScaler.dll`（[配置方法](docs/notes/vulkan-fg-fsr4-metalfx.md#optional-optiscaler-loading-on-windows)）。 |

## 反馈问题

[提交 issue](https://github.com/freefrank/LostOdysseyRecomp/issues) 时请写明版本、操作系统、图形后端、GPU 和驱动、游戏版本和光盘，以及出现问题的步骤或场景，并附上最新的 `logs/runtime-<timestamp>.log`。

画面问题请在问题出现时做一次[渲染捕获](#overview捕获与游戏操作)，检查归档内容后再分享。不要附上游戏文件、存档或个人数据。

可选诊断默认关闭，见[隐私说明](PRIVACY.zh-CN.md)。

## 实机画面

<img src="docs/images/title-screen.png" alt="失落的奥德赛标题画面 — Press START" width="960">

| Ring 战斗 | 城市探索 |
| :---: | :---: |
| ![凯姆攻击时的 Ring 判定界面](docs/images/ring-battle.png) | ![工业城市探索场景](docs/images/city-exploration.png) |

*截图来自 v0.1 发布前的开发构建，未经修图。*

## 开发导航

[构建指南](docs/BUILDING.md)介绍依赖和构建命令，[开发工具](tools/README.md)列出可用工具，[开发状态](docs/STATUS.md)记录验证情况，[文档索引](docs/README.md)汇总其他文档。

## 赞助者

感谢 **Frenzy Fresh**、**José Antonio Martínez Godoy**、**C_BAR**、**Arakon**、**Efren V**、**Torresmo**、**doc_haz**、**Whitesun** 和 **Cristian** 在 [Ko-fi](https://ko-fi.com/dotslash) 上支持本项目。

## 致谢与游戏数据

感谢所有提交过 PR 和补丁的人：[MikeRavenelle](https://github.com/MikeRavenelle)、[dj5927](https://github.com/dj5927)、[Xarishark](https://github.com/Xarishark)、[navjack](https://github.com/navjack)、[frankzzz](https://github.com/frankzzz) 和 [cngjd](https://github.com/cngjd)。macOS 版本最初来自 MikeRavenelle 在 Apple Silicon 上的工作。

本项目参考 [UnleashedRecomp](https://github.com/hedge-dev/UnleashedRecomp)、[re:Blue](https://github.com/zolaware/reblue)、[XenonRecomp](https://github.com/hedge-dev/XenonRecomp)、[XenosRecomp](https://github.com/hedge-dev/XenosRecomp)、[plume](https://github.com/renderbag/plume) 和 [Xenia](https://github.com/xenia-project/xenia)。音频采用 [Xenia FFmpeg 分支](https://github.com/xenia-project/FFmpeg)（[许可证](thirdparty/ffmpeg-LICENSE.txt)）。Android 上的 Turnip GPU 驱动来自 [Eden](https://git.eden-emu.dev/eden-emu/eden) 模拟器的驱动列表，通过 [libadrenotools](https://github.com/bylaws/libadrenotools) 加载。

《失落的奥德赛》及其资产归各自权利人所有，本项目为非官方移植。请从自己拥有的光盘提取数据，不要向仓库提交游戏程序、资源包、纹理、音视频、生成的游戏代码或捕获数据。依赖保留各自许可证。
