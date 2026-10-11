# Android ARM64 移植研究（2026-10-02）

研究基线：`origin/main` 的 `cf13e15a15e3d21e5c905de5408a8a01bbb17aa6`，本地分支 `trail/android-port-research`。

**初始研究结论（已保留为历史基线）：值得做 ARM64 / Vulkan 原型，但当时源码不能直接编译打包为可玩的 Android 版本。** 已有 ARM64 guest CPU、SDL 和 Vulkan 基础可复用；构建入口、16 KB 主机页、GPU 能力与资源预算、shader 编译和 Android 应用边界仍需适配。现阶段不承诺兼容机型、最低内存、帧率或交付日期。当前开发状态见文末的 checkpoint。

初始研究交付为源码与官方资料研究、平台判定探针和后续验证顺序；当时没有 Android 编译、APK、设备运行、玩家验收或发布。后续开发已加入受限的诊断 APK，但不改变完整游戏尚未接入的结论。

## 证据范围与来源

- 主仓库从最新拉取的 `origin/main` 建立研究分支；原 `trail/tall-ultrawide-layout` 分支保留。
- 本地依赖：SDL `0aef2c3`、plume `d890ac8`、XenonRecomp `ddd128b`、XenosRecomp `990d03b`。plume 与 XenonRecomp 有原有补丁工作树修改，主仓库设置了 `ignore = dirty`；本研究没有更改它们。
- `plume-lostodyssey.patch` 的只读 reverse-check 通过；XenonRecomp 的 reverse-check 在 `ppc_context.h` 失败。本次审查发现其 timebase 附近注释差异，相关 ARM64/timebase 结论同时参考仓库保存的补丁；该检查失败不能当作依赖完全同步的证据，后续构建前应重新核对。
- 已查询 Windows SDK 环境变量、默认 SDK 目录，以及 WSL Manjaro 的 SDK 环境变量和常用 NDK 目录，未发现可用 NDK。不是对所有磁盘或其他主机的完整搜索。
- `adb devices -l` 没有列出设备，因此下文 GPU、系统调用、性能和生命周期均无 Android 实测。
- 文中源码行号对应上述提交及本地依赖补丁状态。**证据**表示源码或命令结果；**判断**表示由证据推出的移植工作；**待验证**不构成支持声明。

## 按优先级排列的结论

| 优先级 | 结论 | 可信度与边界 |
|---|---|---|
| P0 | 构建系统不接受 Android，应用仍为桌面 executable；需要 NDK target 与 APK/SDLActivity 入口 | 高，直接源码与平台脚本探针 |
| P0 | Linux guest 内存的 E 别名偏移不符合 16 KB 页对齐；需新的映射方案与语义验证 | 高，直接源码；4 KB Android 也尚未实测 |
| P0 | Vulkan 支持标签不足以判定可运行；BDA、格式、storage-buffer 限制和资源预算必须逐项检查 | 高，当前渲染器要求明确；目标 GPU 能否满足未知 |
| P0 | 现有 portable shader pack 不等于完全无需 DXC；内建 shader 与 guest miss 仍有编译路径 | 高，直接源码；Android DXC 构建未做 |
| P1 | 存储、更新器、前后台/Surface、线程栈需要 Android 适配 | 高，桌面假设明确；实际改动量需原型验证 |
| P2 | ARM64 guest、SDL 音频和实体手柄可复用，持续性能与触控需另行验证/实现 | 中，代码基础存在；无手机运行证据 |

## 1. 构建与依赖

**证据。** [`cmake/LoPlatform.cmake`](../../cmake/LoPlatform.cmake) 第 9–16 行只接受 Windows、Apple、Linux；第 35–40 行已经识别 `aarch64`。在 WSL Manjaro 执行如下只读脚本探针：

```sh
cmake -DCMAKE_SYSTEM_NAME=Android -DCMAKE_SYSTEM_PROCESSOR=aarch64 -P cmake/LoPlatform.cmake
```

退出码 `1`，输出 `Unsupported target platform 'Android'`（第 16 行）。这只验证平台识别模块，不是 NDK configure 或编译结果。

[`CMakeLists.txt`](../../CMakeLists.txt) 第 23–28 行已将 x86 的 `-march=sandybridge` 限制在 x86；[`LostOdysseyRecompLib/CMakeLists.txt`](../../LostOdysseyRecompLib/CMakeLists.txt) 把生成的 PPC C++ 编为静态库。这些是 ARM64 复用基础，无须从 PowerPC 指令翻译重新开始。

但 [`LostOdysseyRecomp/CMakeLists.txt`](../../LostOdysseyRecomp/CMakeLists.txt) 第 136 行仍是 `add_executable`，第 186–191 行对所有 UNIX 使用桌面 RPATH、`pthread dl` 和 `find_package(CURL)`；第 843–858 行的非 Apple DXC 路径指向 `lib/x64/libdxcompiler.so`。Android Bionic 将 pthread 实现放在 libc 中，桌面依赖和 `.so` 不能直接沿用。[Bionic 官方说明](https://android.googlesource.com/platform/bionic/+/main/README.md)

[`thirdparty/ffmpeg.cmake`](../../thirdparty/ffmpeg.cmake) 第 10–35 行已有按平台/ISA 选择配置和 AArch64 NEON 源码的机制；仓库仅有 `thirdparty/ffmpeg-config/macos-aarch64/config.h`。Android 需针对固定的 Xenia FFmpeg fork，用 NDK 生成独立配置并核对 XMA decoder，不能换成没有相同 XMA 支持的普通发行库。

**判断。** 首次实现需要：

1. 新增明确的 `android` 平台分支和 `arm64-v8a` NDK 配置；初期不包含 32-bit ABI。
2. 建立 SDL2 `SDLActivity` / Gradle 壳，提供 native shared library 入口，处理静态依赖 PIC、JNI、日志与 APK 打包。当前强制静态 SDL 的选项也需与采用的 Android 装载方式一起调整。[SDL2 Android 入口](https://wiki.libsdl.org/SDL2/README-android)
3. host 与 target 分开：XenonRecomp、Python、shader/pack 工具在构建主机运行；PPC 产物、runtime、SDL、plume、FFmpeg、zstd 等使用 NDK 编译。若保留 libcurl，也需 Android 版本及其 TLS 依赖。生成器不能在构建机上误执行 Android ELF。[NDK CMake](https://developer.android.com/ndk/guides/cmake)、[其他构建系统交叉编译](https://developer.android.com/ndk/guides/other_build_systems)
4. 起步关闭 DLSS、FG、MetalFX 等不相关路径；FSR/其他画质扩展等首帧稳定后再评估。host 离线 shader 工作与设备运行依赖要显式分开。

建议使用支持 16 KB 对齐的 NDK r28 或更新版本、AGP 8.5.1 或更新版本作为原型工具链起点；具体版本在实现时固定。版本满足要求并不自动修复页粒度语义。[Android 16 KB 指南](https://developer.android.com/guide/practices/page-sizes)

## 2. Guest 内存与 ARM64 运行时

**证据。** [`kernel/guest_address_space_layout.h`](../../LostOdysseyRecomp/kernel/guest_address_space_layout.h) 第 10–16 行定义的 E 视图 backing offset 为 `0xA0001000`。[`kernel/guest_address_space.cpp`](../../LostOdysseyRecomp/kernel/guest_address_space.cpp) 第 199–245 行的非 Mac POSIX 路径在 `0x100000000` 固定预留 4 GiB 虚拟地址，使用 `SYS_memfd_create`，以 `MAP_SHARED | MAP_FIXED` 建立全部四个别名。该 E offset 只对齐到 4 KB，在 16 KB 页设备上不能原样传给 `mmap`。

这里的 **4 GiB 是 guest 虚拟地址预留，不能据此宣称消耗 4 GiB 物理内存**；实际 backing、常驻页和 GPU 资源占用应分别测量。

[`kernel/guest_address_space_macos.cpp`](../../LostOdysseyRecomp/kernel/guest_address_space_macos.cpp) 第 11–18、43–82、107–130 行已针对大页问题跳过 E 视图并设置访问探针，但采用 Mach VM。Android 不能直接复制这一 backend，也不能仅因 Mac 的有限运行成功就宣布所有 E 访问都不存在。

**判断。** Android 应有单独的 guest memory 适配：区分 guest 页与 host 页，探测 `sysconf(_SC_PAGESIZE)`，对 reserve、alias、commit/protect/unmap 和 E 访问建立可观测验证。优先研究保留别名语义的方案；若原型暂时省略 E，必须让实际访问被记录或明确失败，并以覆盖证据限制支持范围。

**待验证。** 4 KB 与 16 KB 环境中的固定地址预留可行性、Bionic/API 对应系统调用、内存保护粒度、压力下的失败处理、长期 guest 内存正确性。

ARM64 其他基础与风险：

- [`cpu/ppc_context.h`](../../LostOdysseyRecomp/cpu/ppc_context.h) 第 3–12 行、[`cpu/guest_thread.cpp`](../../LostOdysseyRecomp/cpu/guest_thread.cpp) 第 29–53、98–118 行使用 guest TLS/context；[`XenonRecomp-lostodyssey.patch`](../../tools/patches/XenonRecomp-lostodyssey.patch) 第 687–698 行的 timebase 使用平台中立的 active game clock。
- [`os/guest_code_thread.h`](../../LostOdysseyRecomp/os/guest_code_thread.h) 第 46–49 行依赖 Linux 默认大线程栈的假设；Android 必须测量或显式设置 guest worker 栈，不能沿用“Linux 默认 8 MiB”的判断。
- [`os/platform.h`](../../LostOdysseyRecomp/os/platform.h) 第 7–23 行没有 Android 分类；`__linux__` 会进入 Linux 分支。与 CMake 一样，需要把 Android 的系统边界明确出来。
- [`os/crash_handler.cpp`](../../LostOdysseyRecomp/os/crash_handler.cpp) 第 364–374 行的非 Windows handler 为空；需要可用的 native crash/logcat 证据，Mac 的 Mach 探针不能代替 Android 诊断。

## 3. Vulkan：版本、Surface、格式与资源预算

**证据。** 当前 plume 请求 Vulkan **1.2**（[`plume_vulkan.cpp`](../../thirdparty/plume/plume_vulkan.cpp) 第 4910–4915 行），DXC SPIR-V 编译也使用 `vulkan1.2` target（[`dxc_compiler.cpp`](../../LostOdysseyRecomp/gpu/shader/dxc_compiler.cpp) 第 234–237 行）。应用已有设备能力门槛：[`gpu/backend_selection.h`](../../LostOdysseyRecomp/gpu/backend_selection.h) 第 39–64 行要求 Vulkan 1.2、BDA、`shaderInt64`、`scalarBlockLayout` 与部分 descriptor limits，并在 `video.cpp` 第 1749 行执行。不能把“手机支持 Vulkan”或某个 Android 版本当作这些条件已满足。[Android Vulkan native engine 指南](https://developer.android.com/games/develop/vulkan/native-engine-support)

### Surface 接入是已知接口缺口

[`thirdparty/plume/CMakeLists.txt`](../../thirdparty/plume/CMakeLists.txt) 第 16–20 行将 SDL Vulkan 选项限制到系统名精确为 `Linux`。Android 会关闭该选项；其 `RenderWindow` 成为 `ANativeWindow*`（[`plume_render_interface_types.h`](../../thirdparty/plume/plume_render_interface_types.h) 第 63–66 行），但应用的通用非 Windows/macOS 分支传入 `SDL_Window*` 并调用 SDL 版本的接口（[`gpu/video.cpp`](../../LostOdysseyRecomp/gpu/video.cpp) 第 1725–1728、1828–1829 行）。

plume 已有 Android native surface 分支（`plume_vulkan.cpp` 第 2163–2174 行），所以方向明确：选择完善 Android SDL Vulkan 集成，或显式取得 `ANativeWindow` 并使用 native 接口；同时处理 Surface 的创建、销毁和重建。

### 设备探针必须覆盖的渲染要求

| 项目 | 当前源码证据 | 移植判断 |
|---|---|---|
| WSI | plume 第 45–74 行要求 surface / Android surface 或 SDL WSI，以及 `VK_KHR_swapchain` | 验证实例、设备、present queue 和 swapchain，不能只枚举 GPU |
| BDA / shaderInt64 / scalarBlockLayout | plume 把 BDA 作为可选能力查询，但应用的 backend gate 已要求这三项；renderer 第 8050–8055 行使用 device address，`common_hlsl.h` 第 10–24 行使用 64 位地址 | 复用已有启动检查并在设备报告中记录结果；不可把 Vulkan 1.2 版本号等同于可选 feature 全部可用 |
| 已有 descriptor 门槛 | `backend_selection.h` 第 39–64 行要求至少 5 个 descriptor sets、32 个 samplers、96 个 sampled images、1 个 storage buffer、24 字节 push constants | 按 `backend_device.h` 第 27–41 行的实际 Vulkan limit 字段查询；这些数量门槛之外仍要检查下述 buffer range |
| Storage buffer range | renderer 第 1915–1918 行创建 1 GiB vertex arena，第 1980–1983 行将整段绑定；plume 第 2003–2012 行使用该完整 descriptor range | 查询 `maxStorageBufferRange`；不满足时需要分段绑定/arena 改造，降低分辨率无法解决该限制 |
| 映射内存预算 | renderer 第 243、428、1886–1906 行有两个 96 MiB upload ring，另有上述 1 GiB mapped arena | 已知请求容量合计约 1.1875 GiB，尚不含纹理、目标、guest backing；不等同于测得 RSS。需按设备预算设计容量与回收 |
| BC1 / BC2 / BC3 | renderer 第 5475–5477 行将 guest DXT 格式直接映射到 BC；第 5873–5881 行创建失败直接返回 | 检查 sampled format 支持；该路径没有 BC→RGBA 回退，缺失时需解压/转码策略及其内存预算 |
| 深度与浮点目标 | renderer 第 5352–5364 行使用 `D32_FLOAT_S8_UINT`，第 5175–5189 行使用 RGBA16F 目标 | 按实际用途查询 attachment、sampled、transfer 等 format feature 位，必要时适配格式 |

上述 renderer 引用均指 [`gpu/renderer.cpp`](../../LostOdysseyRecomp/gpu/renderer.cpp)；arena 容量常量位于 [`gpu/render_arena_policy.h`](../../LostOdysseyRecomp/gpu/render_arena_policy.h) 第 7–9 行。

设备探针还应完整记录 Vulkan features（包括 descriptor indexing、scalar block layout 等）、limits、驱动、内存堆/类型，以及一个代表性 shader + pipeline + draw 的结果。仅列出扩展不足以验证功能。没有证据支持现阶段宣称某个 Adreno/Mali 型号可玩，或对两家驱动兼容性作排名。

## 4. Shader 编译与分发

**证据。** portable pack 可以减少 guest shader 编译，但 [`gpu/renderer.cpp`](../../LostOdysseyRecomp/gpu/renderer.cpp) 第 2030–2038、2139、2241–2261 行的内建 shader 仍调用 `CompileCachedHlsl`；[`dxc_compiler.cpp`](../../LostOdysseyRecomp/gpu/shader/dxc_compiler.cpp) 第 333–363 行在 miss 时编译。guest shader miss 在 renderer 第 4733–4750、5000–5011 行也进入编译路径。

缓存身份依赖 `DxcIdentity`（renderer 第 1841 行）；DXC 不能装载时 identity 为空（dxc 第 175–190 行）。非 Mac `.so` 查找路径和现有打包指向桌面/x64，未提供 Android ARM64 DXC。因此“复制 `portable_vk.lospv` 后就能去掉 DXC”不成立。

**两个待选择的实现方向：**

- 构建 Android ARM64/Bionic DXC，保留 miss 编译路径；需验证产物大小、装载、编译峰值内存和线程栈。
- 将启动内建 shader 与需要的变体一起离线化，定义不依赖设备端 DXC 装载的稳定缓存身份；guest miss 必须有明确的失败/补包机制和可观测日志。此方案需先证明 shader 覆盖，不能用黑屏或静默跳过掩盖缺失。

建议原型先把**最小首帧**所需 shader 离线固定，以快速验证 GPU；完整游戏采用哪种方案，由实际编译成本与覆盖结果决定。SPIR-V 是输入，设备 pipeline cache 仍需按驱动/设备隔离。

## 5. Android 应用边界

| 领域 | 已有行为 | 所需工作 |
|---|---|---|
| 入口、路径、更新 | [`main.cpp`](../../LostOdysseyRecomp/main.cpp) 第 65–72 行读 `/proc/self/exe`，第 267–315 行运行桌面 updater / `posix_spawn`；[`os/user_paths.h`](../../LostOdysseyRecomp/os/user_paths.h) 第 19–85 行采用可执行目录或 HOME/XDG | 使用 app-specific files/cache/save 路径；原型禁用桌面自更新；APK 升级交给 Android 安装流程 |
| 游戏数据 | 当前游戏定位/加载依赖普通目录和文件路径（main 第 318–402 行、[`settings/game_path.h`](../../LostOdysseyRecomp/settings/game_path.h) 第 142–184 行） | 选择 SAF 导入到应用可访问目录，或实现 URI/FD 后端。前者涉及空间和中断恢复，后者涉及 seek/mmap/大量小文件；需明确取舍 |
| 生命周期 | 当前窗口/输入事件处理主要覆盖桌面事件，未发现应用自己的 `SDL_APP_*` 处理；退出有 `std::_Exit`（main 第 116–122 行、[`hid/hid.cpp`](../../LostOdysseyRecomp/hid/hid.cpp) 第 234–249 行） | 处理前后台、音频暂停、guest 时间/线程、Surface 重建、状态落盘和进程被回收后的重启 |
| 输入 | SDL_GameController 已有；未发现 runtime 的 `SDL_FINGER*` 处理 | 第一轮用实体手柄验证；触控 UI/虚拟按键为独立后续工作 |
| 音频 | [`apu/audio.cpp`](../../LostOdysseyRecomp/apu/audio.cpp) 第 96–111、127–175 行使用 SDL float stereo 队列，SDL 子模块自带 Android backend | 可复用，但 XMA、暂停恢复、设备切换与延迟仍需真机验证 |

SAF 返回 URI，不能当作普通路径；provider 给出的 FD 也可能不可 seek。应验证授权持久化、随机读取、目录移动/删除后的错误处理。[SAF 官方指南](https://developer.android.com/training/data-storage/shared/documents-files)、[ContentResolver FD 契约](https://developer.android.com/reference/android/content/ContentResolver#openFileDescriptor(android.net.Uri,%20java.lang.String))

Activity/进程可被系统重建或终止，不能把桌面退出语义直接当成移动生命周期；持续性能应包含热状态与降频。[Activity lifecycle](https://developer.android.com/guide/components/activities/activity-lifecycle)、[Android thermal 指南](https://developer.android.com/games/optimize/adpf/thermal)

## 6. 建议的原型顺序与停止条件

研究基线建议：**ARM64、原厂驱动 Vulkan 1.2 + 经查询满足的功能集、实体手柄、固定横屏、SDR、关闭 FG/高成本画质扩展**。Android 最低 API 尚未定案，由所用 NDK API 和设备范围确定。这是工程提案，不是最低系统/设备支持声明。

| 阶段 | 产物与通过条件 | 不通过时 |
|---|---|---|
| A：工具链与空壳 | SDL Android APK 在选定设备启动，native 日志、窗口、音频、手柄可用；ELF/APK 对齐正确 | 修复入口和依赖；不接完整游戏 |
| B：内存语义 | 分别在 4 KB、16 KB 环境验证真实 guest reserve/alias/protect/unmap、E 访问与 worker 栈 | 调整 backend 或明确限制原型环境；不宣称 16 KB 支持 |
| C：GPU 首帧 | 记录完整 capabilities，正确创建 Surface、pipeline、代表性纹理和 draw；完成前后台 Surface 重建 | 为缺失功能设计适配；单纯降低分辨率不解决 feature/descriptor 限制 |
| D：完整运行链接 | NDK 编译 runtime/PPC/依赖，完成 built-in shader 路径；记录所有 shader miss；加载用户提供的数据 | 修复编译/装载/路径与 shader 缺口；不发布可玩声明 |
| E：有限玩法 | 新游戏、开场、首战、菜单、保存/读取、音频、手柄、前后台/锁屏恢复，截图和日志留证 | 分别记录实现、运行验证和玩家验收，不以启动成功替代玩法验收 |
| F：持续运行 | 同一设备/驱动/设置下固定场景 20–30 分钟，记录 frame time、RSS、内存峰值、热状态；另测低内存回收/重启后数据 | 优化资源容量/带宽和帧节奏，扩大设备覆盖前保留单机限制 |

A/B/C 可用不含游戏数据的小型探针推进；B/C 是进入完整游戏工作的关键决策门槛。20–30 分钟为研究建议，不是已完成测试或官方验收标准。

后续设备取证至少记录：

```sh
adb shell getprop ro.product.model
adb shell getprop ro.build.version.sdk
adb shell getprop ro.product.cpu.abilist
adb shell getconf PAGE_SIZE
```

初始研究时尚未在设备执行上述命令；后续执行结果见文末真机 checkpoint。GPU 探针另行输出 `VkPhysicalDeviceProperties`、features、extensions、limits、format properties 与 memory heaps。APK 检查 `zipalign -c -P 16 -v 4 app.apk`，逐个 native `.so` 核对 ELF LOAD 对齐；同样需要实际 16 KB 运行验证。[官方验证步骤](https://developer.android.com/guide/practices/page-sizes)

## 初始研究快照（截至研究阶段）

已完成：建立研究分支、源码与上游文档审查、平台判定失败复现、依赖来源/工具链可用性检查、分阶段研究记录。

未完成：Android 工具链安装、NDK configure/build、APK、内存/GPU 探针实现、任何 Android 真机或模拟器运行、性能数据、用户验收、commit/push/PR 或发布。现有桌面构建与 Mac 测试均不能补足这些证据。

## 开发 checkpoint（2026-10-02）

在初始研究之后，`trail/android-port-research` 已开始实现一个受限的 Android ARM64 诊断路径。根 CMake 当时只在 `LO_BUILD_ANDROID_PROBE` 路径提前构建探针，并拒绝完整游戏目标；后续完整 runtime 已改为独立的 opt-in 路径。探针位于 [`tools/android_probe`](../../tools/android_probe/)，APK 工程位于 [`packaging/android`](../../packaging/android/)。它使用 SDL2 的 `SDLActivity` 和 native report UI，提供 **Run checks**、**Test audio** 与 **Copy report** 操作，并将报告写入 `files/probe-report.txt`，native 日志使用 `LOAndroidProbe` 标签。

当前工具链固定为 Gradle 8.11.1（wrapper 含官方 SHA-256 校验）、Android Gradle Plugin 8.9.3、NDK 28.2.13676358、SDK 35、Build Tools 35.0.0、CMake 3.22.1、minSdk 26，且只生成 `arm64-v8a`。WSL 中已确认可复用 `/home/freefrank/Android/Sdk`；不需要另装一套 SDK。构建命令和安装/取证命令见 [`packaging/android/README.md`](../../packaging/android/README.md)。

探针的内存实验使用固定 4 GiB 虚拟地址保留和小别名；当前只对 16 KB 主机页的 E alias 路径明确报告不支持，A/C 检查仍可执行。它不证明完整 guest 映射、保护、取消映射或游戏运行语义。Vulkan 目前只记录能力、限制、格式和内存堆，并在每次检查运行一个 clear/present frame；不含 shader、pipeline 或游戏渲染。探针不请求游戏文件或存储权限。

已完成的主机侧证据：WSL 4 KB memory sentinel 与 cleanup 检查通过，原生 host memory test 覆盖地址冲突安全检查并通过；`assembleDebug` 和 `lintDebug` 均为 `BUILD SUCCESSFUL`。最终修正版 arm64 APK 为 5,757,171 bytes，签名验证通过，`zipalign -c -P 16 -v 4` 通过；三个 native 库均为 ARM64 且满足 16 KB ELF 对齐。CMake 真实 NDK 默认 full-runtime 路径也会按预期拒绝，并提示 `LO_BUILD_ANDROID_PROBE=ON`。SDL Java 使用生成副本应用 USB intent 防护和 AndroidX receiver shim；lint baseline 仅包含 26 条上游 SDL `MissingPermission`，不屏蔽应用或 receiver 新错误。

以上主机侧检查仅证明探针工具链和静态包装正确，不能替代完整游戏验证。设备安装、运行与恢复证据见下方真机 checkpoint；未取得该设备的 logcat 输出，因此设备结论以应用报告与界面取证为依据。

## 首个真机诊断 checkpoint（2026-10-02）

首个设备报告来自 Lenovo TB321FU（Android 16/API 36、arm64，主机页面 4096 字节）；报告原文保存在 [`android-probe-tb321fu-2026-10-02.txt`](android-probe-tb321fu-2026-10-02.txt)。4 GiB 虚拟地址保留、memfd 和小型 A/C/E alias sentinel 检查均通过，结果仍只覆盖小映射可行性，不证明生产 guest 映射和保护语义。该 4 KB 设备不能提供 16 KB 真机支持证据。

设备为 Adreno 750，Vulkan API 1.3.128，驱动版本 `0x802fa028`；`shaderInt64`、buffer device address 和 scalar block layout 均报告可用。BC1/2/3、D32S8、RGBA16F 格式探针和 renderer layout gate 通过，clear submit/present 在 2560×1600 成功。`maxStorageBufferRange` 只有 128 MiB，而当前 vertex arena 需要 1 GiB，因此 vertex arena range gate 失败；这是完整游戏移植的当前 GPU 阻塞项，下一步应适配 render arena，不能用降低分辨率规避。

音频队列 API 提交成功，但只执行了 API/队列检查，未证明人耳可听；本次没有 SDL controller。Android 16 inset 修复后的最终 APK 已重新构建，按钮避开状态栏；三次 Run checks 均在同一进程产生新报告并通过 memory/clear/present，Home 触发 background 后恢复的新 probe 成功，Back 退出并重新启动后的新 probe 也成功。最终 APK SHA-256 为 `3c55eeb2020049d7ad775fdbd768e0adc85b3e338fd24b32f8477a8706be828e`，v2 签名和 16 KB ZIP 对齐通过，`assembleDebug`/`lintDebug` 通过。此次 checkpoint 没有游戏 renderer、shader、资源加载、性能或玩法证据；探针本身不使用游戏资源，16 KB 真机仍未测。

## 第二阶段 checkpoint：Android ARM64 重编译库（2026-10-02）

第一提交已完成探针交付；本次增量处理 host/target 分离：主机运行 XenonRecomp 生成 PPC C++，Android NDK 以 ARM64/PIC 编译 `LostOdysseyRecompLib` 静态库，同时关闭 runtime、GPU 和 tools。246 个生成的 PPC 源文件及 function mapping 来自私有构建输入，不能提交到仓库。对应说明和命令见 [`packaging/android/README.md`](../../packaging/android/README.md)。

该切片要求 WSL system CMake ≥3.28；Gradle APK 的 CMake 3.22.1 只适用于探针。推荐配置命令为：

```sh
cmake -S . -B out/build/android-ppc -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 \
  -DLO_BUILD_RUNTIME=OFF -DLO_BUILD_GPU=OFF \
  -DLO_BUILD_TOOLS=OFF -DLO_BUILD_RECOMP_LIB=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/android-ppc --target LostOdysseyRecompLib -j 4
```

根 CMake 已开放这个 library-only 路径，强制 Android ARM64 和 PIC，拒绝将 XenonRecomp/XenosRecomp 编为 Android 工具；这段验证当时仍使用完整 runtime 拒绝检查。NDK 28.2 / Clang 19 的 Release configure/build 通过，247 条编译命令均使用 `aarch64-none-linux-android26` 与 `-fPIC`，归档中的全部 247 个对象均为 ELF64 little-endian AArch64 relocatable。静态库为 438,257,042 bytes，SHA-256 `4f9a3a304033527fbccc70457c5071a9c1540b1da593e3c1cec8bd1ef7d47bbd`；本地证据为 `out/android-device-probe/ppc-library-validation.json`。默认 runtime 和 tools=ON 的 configure 拒绝检查，以及原有 probe 根目录 configure 均通过。之后的完整 runtime opt-in 链接结果见第三阶段 checkpoint。

### 生产内存实现的设备 shell 检查

使用 NDK 28.2 将现有 `kernel/guest_address_space.cpp` 与 `tools/tests/memory_alias_test.cpp` 编译为 Android 26 ARM64/PIE 可执行文件，在同一 TB321FU 的 ADB shell 执行成功。两轮测试覆盖完整生产地址空间的 A/C 别名、E 偏移、虚拟页隔离、查询数据往返及释放后重新分配；实际只触碰少量哨兵页。证据保存在 `out/android-device-probe/production-memory-shell-test.json`，测试程序已从设备临时目录移除。

这次执行使用 ADB shell 身份，不等于 SDLActivity 应用沙箱内验证；现有测试没有主动触发 null guard fault，也没有完整 guest 程序、16 KB 页或压力测试。下一步仍需把生产内存检查接入应用，并补保护语义和生命周期覆盖。

### 下一 GPU 验证计划

host 侧已用 `motion_replay_fixture.h` 经真实 translator 和 host DXC 生成 SPIR-V：VS 6992 bytes、PS 4224 bytes；fixture 记录了 `Shader`、`Int64`、`PhysicalStorageBufferAddresses`、`PhysicalStorageBuffer64 GLSL450`，以及 VS 的 descriptor set 0 / binding 0 顶点 arena 合约。证据保存在 `out/android-device-probe/next-gpu-shader/fixture-compile.json`。这只是 host shader 编译证据，尚无新设备 draw/readback。

这段记录属于 BDA 适配前的计划：当时建议在 native probe 中使用这些 SPIR-V 完成真实 draw + readback，并评估 BDA vertex fetch 和 shader-pack ABI 变化。之后 runtime 已采用 BDA vertex fetch，保持 24-byte push constants 和 3 个 BDA 常量 contract；真实设备 draw/readback 与完整 shader 覆盖仍未验证。

## 第三阶段 checkpoint：完整运行时开发切片（2026-10-02，未完成）

后续开发已把完整运行时改为明确的 opt-in 路径：配置 Android runtime 时设置
`LO_BUILD_ANDROID_RUNTIME=ON`；诊断 APK 仍使用 `LO_BUILD_ANDROID_PROBE=ON`，两者不能混为同一个验证结论。Android 壳和 native runtime 分在 Gradle 的 `:runtime` 模块，诊断 APK 保留在 `:app` 模块。`libmain.so` 已使用 NDK 28.2、API 26、ARM64 完成链接，运行时的 focused host 检查也已通过；完整 APK 已构建并安装，但资源加载和可玩流程仍未验收。

随后 `tools/android/build-runtime.sh`、`:runtime:assembleDebug` 和 `:runtime:lintDebug` 均成功；运行版 APK 通过 v2 签名及 16 KB zip 对齐检查，并已用 ADB 安装成功。最初从共享 Download 目录迁移资源时，shell 所有者和 `0660` 权限导致应用首次读取 `default.xex` 失败；为应用专属 external files 目录补充可读权限后，运行版成功加载 XEX。当前仍没有完整可玩流程证据。

本切片加入固定版本的 Android FFmpeg 配置与隐藏符号边界、Android ARM64 DXC 构建，以及 `libc++` 的 `atomic_ref`／`jthread` 兼容处理。vertex fetch 改为 BDA 路径，避免把完整 1 GiB vertex arena 声明成单一 storage-buffer descriptor，同时保持桌面 HLSL 原文不变。开发平板报告的 128 MiB `maxStorageBufferRange` 因此不再是原先的直接 descriptor 阻塞项；真实 shader draw/readback、设备性能和完整 shader 覆盖仍需验证。Android runtime 的 FFmpeg/DXC 构建细节见 [Android DXC 构建记录](android-dxc-build-2026-10-02.md)。

运行时保留 SDL 的实体手柄路径，并加入屏幕触摸手柄。触摸控件可以在 Android UI 中关闭，开关保存在 `SharedPreferences`；这只证明实现范围，尚未证明在游戏内菜单、战斗和恢复流程中可用。资源开发测试将使用应用专属 external files 目录；设备上已有的约 20.27 GiB 四张光盘资源会迁移到该目录，文档不记录设备序列号或游戏私有数据。

2026-10-02 的 runtime session 已在 Lenovo TB321FU 上加载 XEX，创建 Adreno 750 Vulkan 设备和 2560×1600 swapchain，并进入真实 DXC 编译的 28,484 个 shader 准备阶段；屏幕显示 `Preparing shaders` 和触摸控件。通过 Android UI 关闭触摸控件后，控件消失而 OFF 入口保留，`SharedPreferences` 记录 `enabled=false`。尚未验证重启后的持久化、游戏内输入、实体手柄硬件、音频或完整可玩流程；shader 准备期间的结果也不等于游戏验收。共享 SDL 重构后的 `:app:assembleDebug`／`:app:lintDebug` 仍已通过。

后续 session 跳过首轮预编译后报告 4,325 个游戏 shader ready、0 个失败，4,344 次 DXC 调用成功；不能把这组数字扩展为完整约 28K shader 已完成。guest entry `0x827ca440` 已运行，SDL OpenSL ES 48,000 Hz stereo 与 320 个 FFmpeg XMA context 已初始化。新版 APK 验证每帧 resize 修复只发生一次 resize，但游戏画面仍为黑屏。host debug UI 可以正常显示。触摸 D-pad 下、A、B 已在真机驱动 host debug menu 导航、确认并触发 capture、关闭菜单，截图证据为 `touch-menu-down.png`、`touch-menu-confirm.png` 和 `capture first-black-capture`；这些是 host UI 操作证据，不是游戏玩法验收。

在 session `1790936548149152` 中，触摸关闭设置跨 force-stop、APK 覆盖更新和冷启动保持；OFF 入口的新位置也避开状态栏。L3/R3 通过 guest input trace 分别读到 `0x0040`／`0x0080`，释放后回到 `0`。Home 后回到同一 task，host menu 可以恢复显示。实体手柄未连接，尚未验证实体硬件；新版 APK 中 L3/R3、mouse 过滤和 CTRL 位置的静态检查已通过。`RuntimeActivity` 新增仅 debuggable 的 ADB extras（`LO_VS_DEBUG`、`LO_PS_DEBUG`、`LO_NO_ALPHATEST`、`LO_DEBUG_CAPTURE_SWAP`、`LO_TRACE_INPUT`），需用 `--es key value` 并 force-stop 后切换；固定 VS/PS diagnostic 仍为黑屏，GPU debug 继续进行。

## BDA SPIR-V 对齐核验（2026-10-02）

Android `common_hlsl.h` 的 BDA `uint64` `RawBufferLoad` 曾以默认 4 字节对齐生成 SPIR-V，触发校验层 `VUID-StandaloneSpirv-PhysicalStorageBuffer64-06314`。显式指定 8 字节对齐后，真实 DXC 编译、`spirv-val` 和真机校验层均不再报告该错误。独立 Android compute probe 从 `shared + 1024` 读取两级 BDA 并读回 4 个 float 通过；旧的 4 字节对齐版本在这个小型硬件 probe 上也通过，因此该修复不能被认定为当前黑屏根因。游戏仍黑屏，host UI 和音频保持活动，完整可玩性尚未验证。

回归命令 `wsl -d Manjaro -- python3 tools/tests/android_bda_spirv_contract.py --dxc tools/XenosRecomp/thirdparty/dxc-bin/bin/x64/dxc.exe` 当前通过；早期 alignment test 产物为 2,508 bytes，旧 header 的负向检查按预期失败并报告 `06314`。`RuntimeActivity` 新增仅 debuggable 的 `LO_CLEAR_RT` 和 `LO_NO_SHADER_PREPARE` extras，Java 构建与 lint 通过；前者可将 clear 设为洋红色，后者取值 `1` 可跳过启动 shader 准备，便于诊断。洋红色 clear 能显示整屏，说明被测 EDRAM clear→resolve→present 路径连通；guest draws 仍没有颜色，黑屏原因仍在排查。固定 VS/PS 的临时 early return 诊断没有改善画面，不属于交付功能。

## Vertex-stage BDA 地址核验（2026-10-02）

Adreno 750 真机上的独立 Plume vertex-stage probe 使用 24-byte push constants 和三个不同地址，发现 shader 读取 `SharedConstants`（第二个 `uint64`）时实际取到了第三个 PS 地址：预期 marker 为 `.125, 1, .875`，实际为 `.125, .875, .875`。固定 VS quad probe 在 32×32 framebuffer（共 1,024 像素）下呈洋红色；将 push 字段改为三个 `uint2`，并显式用 `uint64(high) << 32 | low` 重组后，`1024/1024` 像素地址 probe 通过。其余 arena `uint64 RawBufferLoad` 未改；Android-only 修复已进入正式构建，cache option 更新为 `v2-u32-push`，新 debug whitelist `LO_DRAW_TRACE`／`LO_DRAW_TRACE_COUNT` 的 Java/lint 检查通过。

早先 index smoke 使用 `%3`，且 quad indices `0,1,3` 退化，曾造成误导；修正 quad 后 index probe 正常。该隔离硬件 probe 证明了 push 地址映射问题和修复方向，但单独不足以证明游戏 renderer；后续正式运行已在标题和首战画面确认该修复路径有效。

## 标题与菜单真机 checkpoint（2026-10-02，限定范围）

正式 APK 已构建、通过 lint、签名和 16 KB zip 对齐检查并安装。无 VS/PS diagnostic flags、仅使用 `LO_NO_SHADER_PREPARE=1` 跳过首轮预编译时，真机已恢复标题画面；触摸 `START` 进入 New Game/Continue 菜单，按 A 选择 New Game 后进入游戏 Settings 屏。该 checkpoint 当时只确认标题和菜单路径；首战证据见下一节。

## 首战真机 checkpoint（2026-10-02，限定范围）

正式黑屏修复 APK 在无 VS/PS diagnostic flags、仅跳过首轮 shader 准备的条件下，播放了开场视频并进入首战。`START` 暂停、`BACK` 跳过视频；触摸 A 选择 Attack 和目标，D-pad 下切换到另一敌人，A 执行攻击，随后完成第二回合的再次攻击，截图显示伤害 `96`、`95`、`142`。3D 角色、敌人、场景和 UI 均显示，说明基本触摸输入和首战攻击路径已通过；这不代表整场战斗、完整流程、帧率或实体手柄验收。

首轮 shader 编译期间曾观测到一次约 40 秒的 GPU PM4 stage `0x22` 停顿，随后编译和 frame 继续推进并进入首战；这不是每次启动的性能结论。音频 native 计数持续非零且 `queue_errors=0`，目前只证明软件队列提交。证据截图为 `runtime-first-battle-live.png`、`runtime-battle-target.png` 和 `runtime-battle-second-attack.png`。补充的 `motion_replay_hlsl` 第四 push 地址兼容检查中，现有 BDA 0/1 的 6 个 VS/PS/depthPS fixture 均通过 DXC 与 `spirv-val`，桌面 HLSL 保持逐字不变。

最终 u32-push APK 的 native build、`:runtime:assembleDebug` 和 `:runtime:lintDebug` 已通过（`runtime-final-u32-push-build.log`）。

## 最终 APK 启动 checkpoint（2026-10-02，限定范围）

最终 APK 已安装，默认启动（无 ADB extras）显示标题画面；SHA-256 为 `d3435c6a6301398ff65575849041ec59969205cc6588082873aa697d50e4043d`。触摸 B 取消启动 shader batch 时报告 1,970 个已准备、0 个失败，实际 DXC 调用 1,926 次；这不是完整 28,504 项 batch 的完成声明。证据截图为 `runtime-final-title.png`。

替换 APK 前，战斗中关闭触摸控件和 Home/恢复已通过；关闭状态跨 APK 更新与冷启动保留。最终应用重新启用控件后，`SharedPreferences` 的 `enabled=true` 已确认。设备上的临时 probe（26 个文件、2 个目录）及两处合计约 1.8 GiB 的 capture 目录已清理，主机侧证据保留。以上仍不等于完整流程、性能、其他 GPU、16 KB 设备或实体手柄验收。

当前明确的 Android 边界是：桌面在线 updater、桌面自动 restart 和自动 tar capture 打包暂不支持。`app:assembleDebug` 与 `:runtime` 的 Gradle 构建是独立目标；native link、APK 打包、资源加载、shader 准备或首战通过都不能代替实体／触摸输入的更广覆盖、音频确认、前后台恢复、长时间游玩和完整流程验证。未发布、未 push、无用户验收。


## 虚拟手柄布局实现 checkpoint（2026-10-02，设备 UI 已限定验证）

Android 源码新增了可配置的十五个虚拟手柄控件，并保留 SDL 实体手柄路径。`Controller settings` 提供 `Show touch controls`、`Control size`（60–140%）、`Opacity`（25–100%）、`Apply`、`Cancel` 和 `Edit layout`；编辑器提供 `SAVE`、`CANCEL`、`RESET` 及按当前选择切换的 `HIDE`／`SHOW`。控件可独立拖动，隐藏控件在编辑器中仍以淡化状态可选；重置位置和可见性时保留全局大小与透明度，顶部中央的 `CTRL` 入口保留可达空间。

`TouchControlLayout` 的模型检查已通过，覆盖默认参考布局、草稿隔离、缩放坐标和边缘限制；最终 APK 的 `assembleDebug`／`lintDebug` 也通过。真机已验证默认参考布局、拖动 A 并保存、隐藏／显示 Y、取消拖动、布局跨 APK 更新与冷启动保留、全局隐藏、尺寸 80%、透明度 59% 及 RESET+SAVE 恢复参考布局；A、START 的 guest input trace 也分别读到 `0x1000`、`0x0010`，释放回零。完整多点触控边缘覆盖、实体手柄和长时间游玩仍待验证。

## 手柄、Surface 与预编译包增量（2026-10-02）

开发在独立 worktree 的 `trail/android-port-research` 继续，保留原 checkout、私有资源和用户配置。同步 [PR #97](https://github.com/freefrank/LostOdysseyRecomp/pull/97) 的 SDL2 pin `b7502f1a884c055f8535cf8d2be3f44c41669a43`（2.33.0），接入 [issue #103](https://github.com/freefrank/LostOdysseyRecomp/issues/103) 请求的上游手柄更新。该版本有 8BitDo `2dc8:6012` 的 HIDAPI 驱动；Android 默认的 framework 输入路径和不同连接模式仍需硬件验证，不能把源码更新等同于该型号已在 Android 验收。

USB／蓝牙使用同一自动隐藏策略：实体手柄连接时隐藏虚拟控件并保留 `CTRL`，用户可重新显示并同时操作；最后一只实体手柄断开时恢复保存的触摸偏好。Android InputManager 事件与每秒一次的 SDL controller 状态补查共同检测连接，补查在后台停止。自动隐藏不把临时状态写成用户关闭偏好。Java 可见性模型检查通过；新 SDL 下 `LoHidTest`、`LoAndroidTouchInputTest` 均通过，覆盖按键合并、扳机取强、左右摇杆独立取活动来源，以及清空触摸不清空 SDL 输入。虚拟扳机测试使用 SDL 映射后的实际轴值，避免假设原始 `[-32768,32767]` 等于 GameController 的 `[0,32767]`。实体 USB／蓝牙热插拔按用户安排暂未验收。真机手动隐藏至仅 `CTRL`、再重新显示已通过。

Android shader bundle 在隔离的宿主构建和缓存目录生成：28,484 条记录、27,674 个唯一二进制、240,411,592 bytes、失败 0 条，完整 payload 和 Android runtime 合同验证通过。SHA-256 为 `d8ac36aab5a697af75836155788745f13be3efc1b390361116ebaf6cbfbe18f3`。复制到平板 `files/shaders/portable_vk.lospv` 后，设备 hash 一致。默认启动无跳过编译的 ADB extras，日志确认 bundle 命中、startup DXC actual calls 0，标题画面正常。一次启动的 197 条 pipeline recipe 中 67 ready、130 missing shaders、0 failed；driver pipeline 准备与未覆盖运行期 shader 仍可能需要处理。包目前手动安装，未内置 APK，也未接入 Android 在线下载。构建与部署方法见 [Android README](../../packaging/android/README.md)。

后台返回问题分为两处：默认 Activity launch mode 允许桌面图标创建第二个 SDLActivity，导致同一进程重复运行 SDL_main；`singleTop` 已避免这条重复入口。其后仍可复现相同 PID／Activity 下黑屏，logcat 明确记录 Surface 已销毁并创建新实例，而 Vulkan 继续向旧 BufferQueue 提交。运行时现按前后台与窗口变化重建完整 Vulkan surface／swapchain，等待 GPU 后释放旧图像信号量，并让 shader 准备画面复用同一路径。SDL Android 局部补丁在 Activity mutex 下读取 native window，并持有额外 ANativeWindow 引用完成 Vulkan surface 创建，避免与 UI 销毁回调并发释放。

功耗排查读取了平板实际配置：60 FPS、720p、FXAA、2× AF，保留这些用户设置。较早前台快照中 GPU command thread 与 guest thread 合计约一个 CPU 核心量级，设备有严重热状态，但 USB 充电和缺少固定场景基线使其不能作为瓦数对比。Home 后进程 CPU 为 0.0%、线程休眠，未发现后台持续运行；不能据此宣称前台功耗已降低。预编译包消除了本次启动的 DXC 工作，持续游戏耗电仍需固定场景的 30／60 FPS、分辨率与温度控制对比。

最终 APK 的 SHA-256 为 `1fdaa30c67c8e29ca7ae84e327f29641681b5ee11a5411b5099f8f23e6b8ab7b`，NDK runtime 构建、runtime／probe 两个模块的 assembleDebug 与 lintDebug、v2 签名、16 KB ZIP 对齐均通过；已覆盖安装到平板。Graphics 真机页面现仅显示 Render resolution、四种受支持 AA、AF、Scaling filter、RGB Range、Frame rate、Brightness 和 Save，导航落在第一个可见行；此前只跳过光标而仍绘制隐藏行的遗漏已修正。

同一最终 APK 上，无 bundle 的准备画面恢复检查通过：测试临时改名 bundle，Home 前显示 indexed shader extraction，HOT 返回后继续显示 Preparing shaders，PID 17893 未变且交换链重建成功。测试结束已恢复原 bundle。随后默认冷启动再次记录 28,484 条 pack 命中和 startup DXC actual calls 0。设置界面连续两次 Home／launcher 往返保持 PID 18223、Activity `e2af09d` 和原页面；两次都记录新 Surface 与成功重建交换链，截取的 logcat 未出现 abandoned BufferQueue 或崩溃。最终 `settings.ini` 与测试前逐行一致，仍为用户的 60 FPS／720p。以上未覆盖系统杀进程后的游戏状态恢复。

本轮本地证据集中于 `out/android-followup/`：`shader-pack-build.log`、`shader-pack/android-pack-report.json`、`shader-pack-test-results.txt`、`build-final-locked.log`、`probe-sdl233-build.log`、`runtime-prepare-resume.log`、`runtime-final-delivery.log`、`graphics-final.png`、`graphics-after-resume.png`。构建、自动测试、有限真机流程与实体手柄／长时间玩法验收分开记录；未 push 或发布。
