<div align="center">

# Lost Odyssey Recompiled

![Lost Odyssey — Press START](docs/images/title-screen.png)

**《失落的奥德赛》Xbox 360 版的实验性原生 PC 移植。**

PowerPC 静态重编译 · Xenos 着色器 · Windows / D3D12

[English](README.md) · [安装指南](docs/INSTALLING.md) · [项目状态](docs/STATUS.md) · [构建指南](docs/BUILDING.md) · [文档导航](docs/README.md)

</div>

---

> **仍处于早期开发。** 部分开场战斗和早期探索路线已能运行，但尚未通关，也未实现完整兼容。渲染、声音和流程仍有问题。仓库不包含游戏资产。

## 项目介绍

使用 **XenonRecomp** 将游戏 PowerPC 代码翻译为 C++，在宿主侧实现 Xbox 360 服务，并翻译 Xenos 着色器，通过 **plume** 渲染。目前实际验证的平台是 **Windows / Direct3D 12**；Linux 和 Vulkan 仍是开发目标。

当前优先保证游戏流程和原版渲染行为。本地[新设置菜单](docs/notes/settings-menu.md)提供英／日／韩／繁中／简中界面、游戏语言、FXAA 和输出分辨率缩放；DLSS、帧生成为禁用占位。提高内部渲染分辨率、帧率解锁、HDR 和现代超分辨率仍属于未来规划。全屏模式尚待桌面实测。

## 游戏版本与语言

开发基于项目所有者提供的**亚洲多语言版**。本地 Disc 1 XEX 的 Title ID 为 `4D5307FA`、Media ID 为 `39F7D748`、标题/基础版本为 `0.0.0.4`，区域掩码为 `0x00FFF900`。此掩码不是“亚洲独占”的零售版本标识，核对数据时应同时匹配可执行文件信息。

当前移植版实际使用**英语**测试。原版包含多语言不代表移植版已完整实现或验证每种语言；其他区域的可执行文件和 Title Update 也未验证。详见[版本证据](docs/notes/xex.md)。

## 当前进展

_核对日期：2026 年 9 月 5 日。_

| 范围 | 已有证据与限制 |
| :--- | :--- |
| 标题与输入 | 已验证动态背景、菜单、SDL 手柄和键盘输入。 |
| 流程 | 开场战斗和部分遇敌已运行；独立副本到达 Gorge 营地。尚未通关。 |
| 画面 | 几何、材质及战后白屏已有修复；阴影、火焰受击、Ring 外环和箱子破坏特效仍有问题。 |
| 声音 | 已实现 XMA 解码、双声道 PCM 输出及循环终点修正；背景音和部分对白仍会消失。 |
| 存档与调试 | 开发版手动保存已确认。F1 支持战斗判胜、坐标记录、同地图 POI 传送及当前地图 ID/名称。 |
| 稳定性 | 已有文件日志和 GPU 停帧诊断。营地卡死尚未明确修复。 |

部分遇敌和存档改动仍**仅在本地、尚未提交**。上述结果描述开发工作区，不是干净检出的完整保证，详见[状态页](docs/STATUS.md)。

## 构建与运行

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
| 调试菜单 | F1 |

默认关闭震动，`LO_CONTROLLER_RUMBLE=1` 可开启。Ring 使用手柄右扳机，肩键不等于扳机。

## 开发导航

| 目录 | 内容 |
| :--- | :--- |
| `LostOdysseyRecomp/` | 宿主内核、图形、音频、输入与调试 |
| `LostOdysseyRecompLib/` | 配置；Git 忽略的 `private/` 游戏数据和 `ppc/` 生成代码 |
| `tools/` | 重编译工具、依赖补丁、Ghidra 脚本 |
| `thirdparty/` | 渲染、音频及其他依赖 |
| `docs/` | 当前状态、指南、逆向记录与历史归档 |

[路线图](docs/ROADMAP.md) · [接手入口](docs/notes/handoff.md) · [渲染测试](docs/notes/rendering-validation.md) · [音频](docs/notes/audio-output.md) · [归档](docs/archive/README.md)

## 致谢与游戏数据

本项目参考 [UnleashedRecomp](https://github.com/hedge-dev/UnleashedRecomp)、[re:Blue](https://github.com/zolaware/reblue)、[XenonRecomp](https://github.com/hedge-dev/XenonRecomp)、[XenosRecomp](https://github.com/hedge-dev/XenosRecomp)、[plume](https://github.com/renderbag/plume) 和 [Xenia](https://github.com/xenia-project/xenia)。音频采用固定版本的 [Xenia FFmpeg 分支](https://github.com/xenia-project/FFmpeg)，已附[许可证](thirdparty/ffmpeg-LICENSE.txt)。

《失落的奥德赛》及其资产归各自权利人所有，本项目为非官方移植。请从自己拥有的光盘提取数据，不提交游戏程序、资源包、纹理、音视频、生成的游戏代码或捕获数据。依赖保留各自许可证。
