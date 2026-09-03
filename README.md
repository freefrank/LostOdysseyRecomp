# LostOdysseyRecomp

Lost Odyssey（失落的奥德赛，Xbox 360，2007）的非官方 PC 移植，采用静态重编译方式：
用 XenonRecomp 把原始 PowerPC 代码翻译成 C++，再配合重写的内核层与 D3D12/Vulkan 渲染后端，
让游戏作为原生 Windows/Linux 程序运行，而不是在模拟器里跑。

参照项目：
- [UnleashedRecomp](https://github.com/hedge-dev/UnleashedRecomp)（架构范本）
- [re:Blue](https://github.com/zolaware/reblue)（同为 Mistwalker 作品，2026-08 公开）
- [XenonRecomp](https://github.com/hedge-dev/XenonRecomp) / [XenosRecomp](https://github.com/hedge-dev/XenosRecomp)
- [plume](https://github.com/renderbag/plume)（D3D12 + Vulkan 抽象层）

## 目标

1. 游戏可以从头玩到尾，行为与原版逐帧一致
2. 任意分辨率、宽屏、高帧率，读盘等待消失
3. 现代光影：HDR、高分辨率阴影、TAA/DLSS/FSR、SSAO，后续视精力扩展
4. Steam Deck / Linux 通过 Vulkan 后端支持

## 法律说明

本仓库只包含移植代码，不包含任何游戏资产。使用者必须自行提供从自己拥有的正版光盘
提取的游戏数据。任何 XEX、UPK、音视频文件一律不得提交到仓库。

## 目录结构

```
LostOdysseyRecomp/        运行时：内核 HLE、渲染、音频、输入、补丁、UI、安装器
LostOdysseyRecompLib/     重编译产物与配置
  config/                 XenonRecomp 的 TOML 配置、跳转表
  private/                本地放置 default.xex 等原始数据（已 gitignore）
  ppc/                    XenonRecomp 生成的 C++（已 gitignore）
  shader/                 XenosRecomp 生成的 HLSL（已 gitignore）
tools/                    XenonRecomp、XenosRecomp 子模块，Ghidra 脚本
thirdparty/               plume 及其他第三方库
docs/                     路线图、逆向笔记、决策记录
```

## 构建

尚未可构建。见 [docs/ROADMAP.md](docs/ROADMAP.md)。
