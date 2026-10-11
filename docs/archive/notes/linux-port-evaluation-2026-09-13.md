# Linux 移植评估与首可玩裁定（2026-09-13）

> **历史检查点。** 下文版本、Issue、进程和待办均是原时点记录；现行交付及验收状态见[项目状态](../STATUS.md)和[路线图](../ROADMAP.md)。

评估基线：公开 `main` / 已发布源码版本 **0.5.10**。本次只写入方案与裁定，**不实施移植、不改版本号、不承诺完成或发布日期**。Linux / Steam Deck 仍是独立未来平台工作。

## 当前状态澄清（2026-09-14）

以上结论保留为 2026-09-13 的规划检查点，不是后续结果的改写。之后的 `linux` 工作树已实现首可玩 Vulkan-only 未打包 ELF，并在 WSL2 Manjaro 的 Mesa Dozen Vulkan-on-D3D12 路径上由用户观看窗口后正常退出。该验证仅接受这条 WSL first-playable 路径；不代表原生 Linux GPU、全游戏、Steam Deck、安装包或 Linux GitHub Release。当前实现与验证边界见 [当前状态](../STATUS.md) 和 [版本变更记录](../../CHANGELOG.md)。

本分支范围：**首可玩 Vulkan-only 未打包 ELF**。用户明确排除 installer、F1 debug menu、updater，这些不进入工作分解。

## 结论

可行，但不是“已经大体可移植、解开几处 `#ifdef` 就能玩”。Windows Vulkan 存在，不等于 Linux 能出图。

第一刀目标：`./LostOdysseyRecomp --game <disc>` 在宿主 Mesa + SDL2 + X11/XWayland 上进入游戏画面。D3D12 不进 Linux。AppImage / Steam Deck / Flatpak **不进第一刀**。

推荐路径（排序，未实施）：

1. **图形：** CMake 打开 `PLUME_SDL_VULKAN_ENABLED`（当前默认 OFF，第一方从未设置）、把 `g_window` 而不是 HWND 传给 `createSwapChain`、解开 [`video.cpp`](../../LostOdysseyRecomp/gpu/video.cpp) `:449` 的 `_WIN32` GPU init、用 `#ifndef _WIN32` 编掉 [`backend_device.h`](../../LostOdysseyRecomp/gpu/backend_device.h) `:4` 的 `plume_d3d12.h`、Linux 默认 Vulkan。不要先写第一方 Xlib/Wayland surface。
2. **着色器：** 二选一，不可跳过。**(A)** `dlopen` 仓库已有的 `libdxcompiler.so`，让 `DxcIdentity()` 非空；**(B)** 离线 AOT SPIR-V（UnleashedRecomp / re:Blue 路线）并改 `ValidIdentity`。空 compiler 会让 `ReadBinary` 连缓存文件都不打开。
3. **PPC：** 独立 Linux 缓存；`/FI` 换成 `-include`；新增 `linux-clang` preset。根 [`CMakeLists.txt`](../../CMakeLists.txt) `:36-37` 的 `MATCHES Clang` 会让 Linux clang **configure 成功，然后在 `/FI` 处失败**。
4. **Host 路径：** `/proc/self/exe` 钉 CWD；`ResolvePath` 大小写折叠；`_stricmp` / `_fseeki64` 换成 POSIX；`--game` / `game-path.txt` 作为首次启动替代。
5. **mmap：** 校验返回基址 `== 0x100000000`，不要只把 hint 当真。
6. **包装：** 可玩 ELF 之后再选 Flatpak 或 AppImage。第一刀不选包装格式。

未在本机 Linux 上实际 configure / 编译 / 运行。PPC `/FI` 失败模式与 mmap 基址冲突是代码路径推论，不是实测。

## 逐项裁定

对照用户给出的 7 行工作表。installer / F1 / updater 已从本表范围剔除。

| 部分 | 用户判断 | 裁定 | 要点 |
|---|---|---|---|
| CPU、内存、线程 | 中低；已有 Linux mmap、`std::thread`、TLS | **确认方向，补一条 latent。** [`guest_address_space.cpp`](../../LostOdysseyRecomp/kernel/guest_address_space.cpp) `:120-171` 已有 mmap+memfd+`MAP_FIXED`；[`stdafx.h`](../../LostOdysseyRecomp/stdafx.h) `:8-15` 已有 `__linux__`。affinity 已 stub。mmap(0x100000000) 只是 hint，未校验基址，后续 `MAP_FIXED` 可能打掉 host 映射。 | 中低 |
| 音频、手柄 | 较低；SDL + FFmpeg XMA | **确认。** `audio.cpp` / `hid.cpp` 零 `_WIN32`；XMA 走 `AV_CODEC_ID_XMAFRAMES`；`ffmpeg.cmake` 已有 gcc atomics `#else`。无 host XInput/WASAPI。 | 较低 |
| Vulkan 图形 | 较高，首要关卡；设备 init 仍 `_WIN32`；Linux DXC 不可用 | **确认方向，纠正做法，并加强 DXC。** GPU init 整段在 `#if LO_GPU_PLUME && _WIN32`。首选不是补 Xlib/Wayland，而是打开 Plume `PLUME_SDL_VULKAN_ENABLED` 并传 `g_window`。还要编掉无条件 `plume_d3d12.h`。Linux `DxcAvailable()=false` **且** `DxcIdentity()=""` → `ValidIdentity` 失败 → 已有 SPIR-V 缓存也不会被读。仓库里已有 `dxc-linux` + `libdxcompiler.so`，只链进 XenosRecomp。 | 较高，第一刀关卡 |
| 设置、首次启动、文件选择 | 中高；部分 Linux 空实现 | **纠正“空实现”。** [`folder_picker.h`](../../LostOdysseyRecomp/settings/folder_picker.h) 整文件无 `#else`；[`first_run.cpp`](../../LostOdysseyRecomp/settings/first_run.cpp) `:400-402` Linux 是跳过 UI 并 `SaveConfig`。首可玩用 `--game` / `game-path.txt`，不补 Win32 向导。`--game` 对后续 Deck/Gamescope 也更合适。 | 首可玩走 CLI；打包 UX 另议 |
| 文件、存档、缓存 | 中；残留 Win32 API；大小写与可写路径 | **低估。升为中高 / 首可玩。** 无 APPDATA；shader 缓存是相对路径 `cache/shaders`（[`renderer.cpp`](../../LostOdysseyRecomp/gpu/renderer.cpp) `:1097-1100`）。Linux **不** chdir 到 exe（[`main.cpp`](../../LostOdysseyRecomp/main.cpp) `:40-47,98-104`）。`ResolvePath` 无大小写折叠。`_stricmp` / `_fseeki64` 无 POSIX 映射，是**编译阻断**。部分 `#else` 已有（fopen、rename、startup_cache `directory_iterator`）。 | 中高 |
| 构建、发布 | 中；预设与 PPC 预编译面向 Windows | **确认预编译门；低估源码编译。** [`LostOdysseyRecompLib/CMakeLists.txt`](../../LostOdysseyRecompLib/CMakeLists.txt) `:23-26` 预编译 FATAL unless WIN32 x64 Release Clang；`:61-63` 源码路径 `/FI /EHs-c- /GR-`。无 `linux-clang` preset（仅 `windows-msvc` / `windows-clang`）。Windows 库链接已经 `if(WIN32)`。DXC DLL 拷贝也只在 WIN32。 | 中高 |
| AppImage | 较低 | **仅在可玩 ELF 之后才可能较低；并行则危险。** CWD 未钉会使包装体写到错误目录。Steam Deck native = sniper glibc 2.31 vs C++23。UnleashedRecomp 官方 Flatpak；re:Blue AppImage + AOT SPIR-V。第一刀不做包装。 | 不进第一刀 |

## 首可玩硬阻断

最小 Linux 进游戏画面必须先解这些。未解则不能出图、不能编过 PPC、不能稳定加载 Xbox 大小写路径。

1. [`video.cpp`](../../LostOdysseyRecomp/gpu/video.cpp) `:449` + `:465` — 无 Windows 则无 plume device / swapchain；`g_nativeWindow` 是 HWND（`:391-407`）。Linux `#else` 目前只能 `createWindow` 和 `PumpEvents`。
2. [`backend_device.h`](../../LostOdysseyRecomp/gpu/backend_device.h) `:4` — 无条件 `#include <plume_d3d12.h>`。Plume 自身在 Linux 不编 D3D12 源，但第一方头仍会拉 D3D12 类型。
3. `PLUME_SDL_VULKAN_ENABLED` 默认 OFF，[`thirdparty/CMakeLists.txt`](../../thirdparty/CMakeLists.txt) `:16-20` 未打开。即使解开 `_WIN32`，swapchain 仍要 HWND。Plume 在该宏开启时走 `SDL_Vulkan_CreateSurface(SDL_Window*)`；未开启时 `__linux__` 仅 Xlib。
4. [`dxc_compiler.cpp`](../../LostOdysseyRecomp/gpu/shader/dxc_compiler.cpp) `:224-228` + [`cache.h`](../../LostOdysseyRecomp/gpu/shader/cache.h) `:44-50` + [`binary_cache.h`](../../LostOdysseyRecomp/gpu/shader/binary_cache.h) `:20` — 无 live DXC **且** 空 identity 拒绝缓存命中。TAA / SMAA / presentation 的 `CompileCachedHlsl` 同样空。`ArtifactKey` 含 compiler 字符串，伪造 identity 不能复用 Windows 缓存文件。磁盘上已有 `tools/XenosRecomp/thirdparty/dxc-bin/bin/x64/dxc-linux` 与 `lib/x64/libdxcompiler.so`，运行时未链接。
5. [`LostOdysseyRecompLib/CMakeLists.txt`](../../LostOdysseyRecompLib/CMakeLists.txt) `:23` + `:61` — 无 Linux PPC 库；`/FI` 让 clang++ 编不过。
6. [`file_system.cpp`](../../LostOdysseyRecomp/kernel/io/file_system.cpp) `:386` `_fseeki64` / `:728` `_stricmp` — Linux 编译失败。
7. 同文件 `ResolvePath`（`:212-278`）— 无大小写折叠，ext4 上 guest 资源 fopen 失败。
8. first-run 跳过选目录 + Linux 不 chdir — 无 `--game` / `game-path.txt` 则 `Load default.xex` 失败。相对 `cache/shaders` 会写到 shell CWD。

Linux `#else` 今天能做的：建 SDL 窗口、PumpEvents、mmap（带 latent 基址风险）、跳过安装 UI、SDL 音频/手柄。不能出图，不能编 PPC，不能编过 CRT 符号，不能稳定加载 Xbox 大小写路径。

本分支**不考虑** installer、F1 debug menu、updater，也不把它们列为第一刀事项。

## 工作分解 vs 假设

| 假设 | 实际 |
|---|---|
| “已经有 Windows Vulkan，Linux 主要是解开 `_WIN32` 设备 init” | 窗口句柄、surface 工厂、整段 renderer 启动都在同一 `#if` 里；Plume SDL surface 宏默认关 |
| “Linux DXC 不可用，也许能吃预编译 cache” | 空 `DxcIdentity` 让 `ValidIdentity` 失败，`ReadBinary` 连文件都不打开 |
| “PPC 有预编译库，Linux 另做缓存即可” | 源码路径 `/FI` 会让 Linux clang 直接失败；configure 不会 FATAL |
| “设置有 Linux 空实现，补 stub 就行” | picker 完全缺失；first-run 会静默成功并跳过选目录。首可玩用 `--game` |
| “clang 检查已经要求 clang-cl，Linux 会被 FATAL” | 检查是 `MATCHES Clang`，Linux clang 通过然后在 `/FI` 失败（更糟） |
| “文件工作量中，主要是可写路径” | 大小写折叠 + CRT 符号 + CWD 都是首可玩 |
| “音频手柄较低” | 成立 |
| “AppImage 较低、可并行” | 仅在可玩二进制之后成立；CWD 未钉会写坏 |
| “已经大体可移植” | 否定。真正漏掉的是 CRT 符号、identity 耦合、D3D12 头、SDL Vulkan 宏未开 |

## Steam Deck / AppImage

**不进第一刀。** [路线图](../ROADMAP.zh-CN.md) 已把 Linux/Steam Deck 标为独立未来平台、无日期。

第一刀：未沙箱 ELF + 宿主 Mesa + SDL2 + X11/XWayland + `--game`。不要强制 Wayland。

Deck 原生编译面对 sniper SDK glibc 2.31 与本仓库 C++23 的张力。Gamescope 下文件选择器会挂 — 当前 Linux first-run 跳过 UI + `--game` 反而是正确的后续路径，不要先补 portal picker。

包装：UnleashedRecomp 官方 Flatpak（弃官方 AppImage）；re:Blue / Zelda64Recomp 走 AppImage 并剥离 `libwayland*`。第一刀不选。

## 证据

核对日期 2026-09-13，相对当时 `main` HEAD。

| 路径 | 要点 |
|---|---|
| `LostOdysseyRecomp/kernel/guest_address_space.cpp:120-171` | Linux mmap(0x100000000)+memfd+MAP_FIXED；hint 未校验基址 |
| `LostOdysseyRecomp/gpu/video.cpp:449-486` | `#if LO_GPU_PLUME && _WIN32` 包死 GPU init |
| `LostOdysseyRecomp/gpu/video.cpp:391-407,445-446` | `g_nativeWindow` 仅 HWND；Linux 只 createWindow |
| `LostOdysseyRecomp/gpu/shader/dxc_compiler.cpp:10,224-231` | 非 Windows `DxcAvailable()=false`，`DxcIdentity=""` |
| `LostOdysseyRecomp/gpu/shader/cache.h:44-50` | `ValidIdentity` 要求 `!compiler.empty()` |
| `LostOdysseyRecomp/gpu/shader/binary_cache.h:20` | `!ValidIdentity` 则 `ReadBinary` 直接 `{}` |
| `LostOdysseyRecomp/gpu/backend_device.h:4` | 无条件 `plume_d3d12.h` |
| `LostOdysseyRecomp/gpu/renderer.cpp:1097-1100` | shader 缓存 = `LO_SHADER_CACHE_DIR` 否则 `"cache/shaders"`，非 APPDATA |
| `LostOdysseyRecomp/settings/folder_picker.h` | 整文件 `#ifdef _WIN32`，无 `#else` |
| `LostOdysseyRecomp/settings/first_run.cpp:400-402` | Linux 跳过 UI，`SaveConfig(GetConfig())` |
| `LostOdysseyRecomp/main.cpp:40-47,98-104` | `ExecutableDirectory` Linux 回落到 `current_path()`；chdir 仅 `_WIN32` |
| `LostOdysseyRecomp/kernel/io/file_system.cpp:212-278,386,728` | `ResolvePath` 无大小写折叠；`_fseeki64` / `_stricmp` 无 POSIX `#else` |
| `LostOdysseyRecompLib/CMakeLists.txt:23-26,61-63` | 预编译 FATAL unless WIN32；`/FI` `/EHs-c-` `/GR-` |
| `CMakeLists.txt:36-37` | `MATCHES Clang` 却提示 clang-cl |
| `CMakePresets.json` | 仅 `windows-msvc` / `windows-clang` |
| `LostOdysseyRecomp/CMakeLists.txt:54-98,552-565` | d3d12 / winmm / RC / DXC 拷贝均 `if(WIN32)` |
| `LostOdysseyRecomp/stdafx.h:8-15` | 已有 `__linux__` unistd/mman |
| `thirdparty/plume/CMakeLists.txt:20` + `thirdparty/CMakeLists.txt:16-20` | `PLUME_SDL_VULKAN_ENABLED` 默认 OFF；第一方未打开 |
| `tools/XenosRecomp/thirdparty/dxc-bin/bin/x64/dxc-linux` 与 `lib/x64/libdxcompiler.so` | 磁盘上存在；运行时未链接 |
| `docs/ROADMAP.md` PC graphics | Linux/Steam Deck 独立未来工作，无日期 |

外部对照（2026-09-13 访问，非本仓库运行时证据）：Microsoft 官方 `linux_dxc_*.x86_64.tar.gz`；UnleashedRecomp 官方 Flatpak + issue #1444 case-fold VFS；re:Blue AppImage，Vulkan Linux 无 runtime DXC linker。

## 未决

- 未在 Linux 上 configure / 编译 / 运行。
- mmap 实际返回基址未探测。
- Steam Deck sniper SDK 与 C++23 是否可共存未测。
- 着色器路径 A（dlopen DXC）与路径 B（AOT SPIR-V）尚未选型；第一刀必须二选一。
- 包装格式（Flatpak vs AppImage）未决，且不应在第一刀决定。
