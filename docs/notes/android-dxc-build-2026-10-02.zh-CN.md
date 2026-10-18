# Android ARM64 DXC 构建（2026-10-02）

[English](android-dxc-build-2026-10-02.md)

运行时的 HLSL→SPIR-V 路径需要 `libdxcompiler.so`。仓库内
`tools/XenosRecomp/thirdparty/dxc-bin/lib/x64/libdxcompiler.so` 是桌面 x86-64
产物，不能装入 Android ARM64 进程。本脚本从 [微软 DXC v1.8.2407
源码](https://github.com/microsoft/DirectXShaderCompiler/releases/tag/v1.8.2407)
构建 Android 26 / ARM64 共享库；该标签提交为
`416fab6b5c4ba956a320d9131102304da995edfc`。现有 Windows
`dxc.exe --version` 报告 `1.8.2407.7 (416fab6b5)`，所以选用相同源码提交。

## 复现

在 Linux/WSL 中安装 Android SDK 的 NDK `28.2.13676358`、CMake `3.22.1`
和 Ninja，设置 `ANDROID_HOME` 后从仓库根目录运行：

```sh
export ANDROID_HOME=/path/to/Android/Sdk
bash tools/android/build-dxc.sh
```

源码、子模块与增量构建放在
`${XDG_CACHE_HOME:-$HOME/.cache}/lostodysseyrecomp/android-dxc`；可用
`LO_DXC_CACHE_DIR=/path/to/cache` 改变。默认成品是
`out/android-dxc/libdxcompiler.so`，与 `tools/android/build-runtime.sh` 的默认
输入一致；可用 `LO_DXC_OUTPUT=/path/to/libdxcompiler.so` 改变。可用
`LO_DXC_JOBS=4` 限制并行编译。已有符合版本、Android 26、ARM64 和导出
符号检查的成品会直接复用，不重新下载或构建。

脚本浅克隆 `v1.8.2407`，只深化 10 个提交，并验证上游
`utils/version/latest-release.json` 的基点
`737a12a663f1697d3755a522d8fbf30481ecd2f6` 到标签恰好有 7 个提交。
随后以 `HLSL_OFFICIAL_BUILD=ON` 生成 `1.8.2407.7 (416fab6b)`。若只有
深度 1 的历史，上游默认版本生成器会误报 `1.8.0.1`。脚本按标签固定的
提交初始化 `DirectX-Headers`、`SPIRV-Headers` 和 `SPIRV-Tools` 子模块，
使用 NDK CMake 工具链、`ANDROID_ABI=arm64-v8a`、
`ANDROID_PLATFORM=android-26`、`ENABLE_SPIRV_CODEGEN=ON`，只编译
`dxcompiler` 目标。[上游 CMake 构建说明](https://github.com/microsoft/DirectXShaderCompiler/blob/v1.8.2407/docs/BuildingAndTestingDXC.rst)
列出 `PredefinedParams.cmake` 预设；本脚本同时关闭测试及无关工具。

上游自动创建的 `NATIVE` 子构建负责在 x86-64 主机执行 tablegen。其默认
`LLVM_ENABLE_EH=OFF` 会使上游 LLVM Support 的 `try`/`throw` 编译失败；脚本
在编译 Android 目标前重新配置 `NATIVE`，设置 `LLVM_ENABLE_EH=ON` 和
`LLVM_ENABLE_RTTI=ON`。上游源码设置了 CMake `CMP0051=OLD`，本机 CMake
4.4 拒绝该设置，因此脚本固定使用 SDK CMake 3.22.1，无需修改上游源码。

## 本次构建证据与边界

- 源码提交和现有 Windows 二进制的提交缩写一致；`dxc-bin/inc/dxcapi.h`
  与上游该标签的 `include/dxc/dxcapi.h` 逐字相同。
- `dxcompiler` 目标完整编译链接；去除调试信息后成品约 34 MiB，本次
  SHA-256 为 `924fb3d8695676ed59ee1aaa2c73e1853f77972a8320824d93fe52bf77c2b6a8`。
- `file` 识别成品为 NDK `28.2.13676358` 构建的 Android 26 ARM64 ELF；
  动态符号表导出 `DxcCreateInstance` 和 `DxcCreateInstance2`；动态依赖仅
  `libdl.so`、`libm.so`、`libc.so`。嵌入版本字符串为
  `dxcoob 1.8.2407.7 (416fab6b)`。

这些是构建与 ELF 静态证据。仍需在设备上验证 `dlopen`、DXC COM 实例、
实际 HLSL→SPIR-V 编译，以及完整游戏运行；它们不由交叉编译成功推出。

## 许可证

此产物是源码构建。分发前应随 APK 或发布材料核对并保留所用源码中的
`LICENSE.TXT`（LLVM/University of Illinois/NCSA 条款）、
`external/DirectX-Headers/LICENSE`（MIT）、`external/SPIRV-Headers/LICENSE`
和 `external/SPIRV-Tools/LICENSE`（Apache-2.0）及其所要求的第三方声明。
仓库现有 `thirdparty/dxc-licenses/` 记录的是官方桌面二进制归档的来源
与许可证，不能直接假定覆盖此 Android 源码构建的全部子模块声明。
