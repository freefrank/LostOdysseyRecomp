# Switch 移植评估与 PC Vulkan 后端交接（2026-09-07）

## 2026-09-07 补充：v0.5.0 PC Vulkan 与 DX11 目标

用户确定下一主版本 **v0.5.0** 的目标为 Windows PC Vulkan 与 Direct3D 11（DX11）支持。实施与验收见[中文 TODO](../ROADMAP.zh-CN.md#v050-pc-graphics)／[English TODO](../ROADMAP.md#v050-pc-graphics)，目前全部待完成。本次仅补充规划，不代表新后端已实现、构建通过或可玩；保留可用 D3D12 基线，Switch 和 Linux／Steam Deck 分别另做平台适配与验收。

下方保留原评估：合并基线 `5c371fc`、缓存版本 21／22、1,998／2,000 着色器结果及冲突数量均为当时快照，开发前须按实际 `main`、资源／变体集合与依赖重查。原文“跑到标题画面”是启动门槛；v0.5.0 还须完成各后端独立 GPU 检查、原生存档场景对照、实际包和硬件验收。不能把标题通过作为完整后端验收，也不能把旧缓存版本递增建议直接用于新基线。

本次代码核对补充：当前 [video.cpp](../../LostOdysseyRecomp/gpu/video.cpp) 仍创建 D3D12 接口；[renderer.cpp](../../LostOdysseyRecomp/gpu/renderer.cpp)、[presentation.cpp](../../LostOdysseyRecomp/gpu/presentation.cpp)、[temporal_aa.cpp](../../LostOdysseyRecomp/gpu/temporal_aa.cpp) 与 [smaa_pipeline.h](../../LostOdysseyRecomp/gpu/shader/smaa_pipeline.h) 的着色器创建仍使用 DXIL。现有 [plume 构建](../../thirdparty/plume/CMakeLists.txt) 已纳入 Vulkan 编译单元，但 SDL Vulkan 选项受 Linux 条件限制：Windows 需要核查并接通 SDL／WSI、loader／volk 和运行时选择，不能按旧快照判断为缺少整个 Vulkan 编译单元。plume 当前后端为 D3D12／Vulkan／Metal，着色器格式接口没有 DXBC；DX11 需要独立后端和编译／绑定适配，不能仅增加环境变量开关。

### 当前渲染层的额外 TODO

- 在原三批 Vulkan 工作基础上，纳入较新的 SMAA 三阶段／LUT、TAA 深度／jitter／历史重置、内部分辨率和独立 UI 路径；按当前实现适配，保留已验收的 TAA 修正。
- 统一内置与微码着色器创建、格式及分后端缓存身份；按实际编译选项和变体校验损坏回退。描述符／常量布局、资源屏障、同步、resolve、回读和三帧 F1 捕获均要覆盖。
- 先独立 GPU fixture 和实际资源编译，再标题与 Map2／Map3／Map12 原生存档同站位对照 D3D12，检查 AA Off／SMAA／TAA、内部分辨率、窗口变化、重开及冷／暖缓存；在可获得的 AMD／NVIDIA／Intel 硬件上记录实际包证据，缺样本项保留待验。

## 结论与决定

朋友在 v0.1 时期（上游提交 `2d9ce9f`，2026-09-06）开始了 Nintendo Switch 移植，快照位于 `../LostOdysseyRecomp-main`（非 git，独立作者）。该快照能编译出 NRO，但默认处于硬件点亮探针模式，尚不可玩，且基于旧版本，未同步 v0.2 至 v0.4 的渲染层改动。

2026-09-07 决定：**Switch 代码暂不进入主库。先在主库实现 PC Vulkan 后端，再让 Switch 分支 rebase 到新渲染层。** 本文记录评估证据、可复用的部分和 PC Vulkan 的工作拆分，供下一次会话或朋友接手。

## Switch 快照的构成

分叉点通过逐文件 blob 哈希比对确认：218 个非生成文件中 172 个与 `2d9ce9f` 完全一致，其余 46 个为作者改动（约 2500 行），无删除文件。三个子模块 `thirdparty/plume`、`tools/XenonRecomp`、`tools/XenosRecomp` 被替换成 vendored 副本，`tools/build_tools.bat` 中应用补丁的步骤已删除。

| 层次 | 文件 | 内容 |
|---|---|---|
| 构建 | `toolchains/switch-devkitA64.cmake`、`tools/build_switch.ps1`、`thirdparty/CMakeLists.txt`、`thirdparty/ffmpeg.cmake`、`thirdparty/ffmpeg_switch_config.h` | devkitA64 交叉工具链；Windows 端先跑 XenonRecomp 和 LoShaderTool 离线编译 SPIR-V，再交叉编译并打包 NRO；SDL 用 portlibs 静态库；FFmpeg 借 Android AArch64 配置并关闭 NEON |
| 内存 | `kernel/guest_address_space.{cpp,h}`、`kernel/heap.cpp`、`kernel/memory.cpp`、`kernel/xex_loader.cpp`、`apu/xma.cpp`、`gpu/command_processor.cpp` | 新增 `GuestAddressSpace::Commit()`：Switch 预留 4 GiB 虚拟空间，按需用 `svcMapProcessMemory` 提交页面并维持 A/C/E 三段别名；桌面实现为空操作 |
| 渲染 | `gpu/renderer.{cpp,h}`、`gpu/presentation.{cpp,h}`、`gpu/video.{cpp,h}`、`gpu/shader/xenos_translator.cpp`、`gpu/shader/dxc_compiler.{cpp,h}`、`gpu/shader/cache.h`、`gpu/shader/builtin_shaders.h` | Vulkan 路径：root CBV 改为 push constant 内的三个 buffer device address，采样器独立到 descriptor set 4，内置着色器抽成头文件并可缓存到磁盘，翻译器用 `__spirv__` 宏切换常量访问 |
| 线程 | `apu/audio.cpp`、`apu/xma.cpp`、`kernel/xex_loader.cpp`、`main.cpp` | devkitA64 的 `pthread_detach` 返回 ENOSYS，音频、XMA、时间戳线程改为可 join 并在退出时回收 |
| 平台 | `os/switch/runtime_switch.cpp`、`os/switch/nvk_switch_stubs.c` | applet 主循环、内存日志、异常转储；给 Mesa NVK 的 Rust 部分补 glibc 桩 |
| plume | `plume_vulkan.cpp`、`plume_vulkan.h`、`plume_render_interface_types.h`、`CMakeLists.txt`、`contrib/volk/volk.c` | `VK_NN_vi_surface`、无 loader 的 ICD 直连、image 到 buffer 拷贝、VMA 大块降到 32 MiB、`maxImageCount` 为零的处理 |
| XenonRecomp | `XenonUtils/ppc_context.h`、`XenonRecomp/recompiler.cpp`、`XenonAnalyse/function.cpp`、`XenonUtils/{file.h,memory_mapped_file.*,xbox.h,xdbf.h}` | ARM 真实内存屏障、GCC builtin 兼容宏、`PPC_MUL_HI`、CR 位运算、一批 VMX 指令；无 mmap 的文件读取 |

外部依赖：`../switch-nvk`（HayatoG/switch-nvk，Mesa 25.0.7 NVK 移植到 Tegra X1），链接 71 MB 的 `nvk-switch/lib/libvulkan.a`。授权 GPL-2.0-or-later，附带要求衍生品完全开源的 fork 政策。

完整 diff（作者改动相对 `2d9ce9f`，含 plume 与 XenonRecomp 副本差异）保存在 `../LostOdysseyRecomp-main/out/switch-author-vs-2d9ce9f.patch`。

## 与当前 main 的合并干跑

将作者改动作为一个提交放在 `2d9ce9f` 上，合并 `main`（`5c371fc`，v0.4.1-dev）：

| 项目 | 数值 |
|---|---|
| 作者改动文件 | 46 |
| 上游此后也改过的文件 | 19（约 1900 行） |
| 自动合并成功 | 37 |
| 冲突文件 | 9 |
| 冲突块 | 21 |

冲突文件：`gpu/renderer.cpp`（5 块）、`gpu/presentation.cpp`（4）、`gpu/video.cpp`（2）、`gpu/shader/cache.h`、`gpu/presentation.h`、`gpu/command_processor.cpp`、`main.cpp`、`LostOdysseyRecomp/CMakeLists.txt`、`tools/tests/presentation_test.cpp`。内核、线程和构建层全部自动合并。上游 v0.4.0 重写了 `Presentation` 接口（SMAA、TAA、内部分辨率），作者又改了同一个 `Init` 签名，这部分必须按新代码重做。

## 直接合入会带来的问题

1. **默认是探针模式。** `main.cpp` 在 `__SWITCH__` 下硬编码 `LO_SWITCH_GUEST_PRESENT_PROBE_ONLY=1`、`LO_SWITCH_GUEST_PRESENT_PROBE_FRAMES=120`、`LO_DRAW_LIMIT=4`。dist 里 2026-09-07 打包的 83 MB NRO 只提交 4 个 draw、呈现 120 帧后停住。
2. **vendored XenonRecomp 比它自己的补丁文件还旧。** 快照的 `tools/patches/XenonRecomp-lostodyssey.patch` 含 `PPCTimeBase`，但 vendored 副本没有：mftb 回到 `__rdtsc()`（Switch 下映射到 19.2 MHz 的 `cntpct_el0`，而客户机时基应为 49.875 MHz），缺少 bdzlr/bdnzlr 分支分析修正和 `PPCHalfToFloat`。作者独立补的 VMX 指令与上游补丁重叠但实现位置不同。
3. **plume 的 D3D12 修正丢失。** 上游补丁里的深度清除分批和空指针检查在 vendored 副本中不存在。
4. **着色器缓存版本号冲突。** 作者把 `Version` 提到 22，上游此后提到 21。
5. **依赖仓库外的私有静态库**，CI 无法复现。
6. **无文档。** 快照的 docs 与 README 未提及 Nintendo、libnx 或 NRO。

## PC Vulkan 后端：工作拆分

目标：在 Windows 桌面驱动上让 `LO_GRAPHICS_API=vulkan` 跑到标题画面，D3D12 路径行为不变。完成后 Switch 分支只剩平台代码。

### 第一批：平台无关修正，D3D12 同样受益

- `gpu/shader/xenos_translator.cpp`：KILL 系列强制读四分量（`op(VECTOR_n, 0b1111)`），MAXA 和 SETP 推入读 W 分量，未知向量 opcode 输出 `0.0` 并记录 note 而非产生非法 HLSL。
- `apu/audio.cpp`、`apu/xma.cpp`、`kernel/xex_loader.cpp`：线程改为 join 回收，新增 `apu::Shutdown()` 与 `XexLoader::StopTimeStampThread()`，`main.cpp` 退出时调用。
- `kernel/guest_address_space.h`：加入 `Commit()` 声明与桌面空实现，供 Switch 分支后续使用。
- `gpu/mmio.cpp` 与 `command_processor.h`：用 `MMIO_SIZE` 替换 `0x10000` 硬编码。
- `tools/patches/plume-lostodyssey.patch`：加入 `maxImageCount == 0` 处理和 `PLACED_FOOTPRINT ← SUBRESOURCE` 的 `vkCmdCopyImageToBuffer` 路径。

### 第二批：着色器二进制格式

- `gpu/shader/dxc_compiler.{cpp,h}`：`ShaderBinaryFormat { Dxil, Spirv }`，`CompiledShader::dxil` 改名 `bytecode`；SPIR-V 参数 `-spirv -fspv-target-env=vulkan1.1 -fvk-use-dx-layout`，顶点阶段加 `-fvk-invert-y`，SPIR-V 不传 `-Qstrip_reflect`。新增 `CompileCachedHlsl()` 按名字缓存内置着色器。
- `gpu/shader/cache.h`：`FileName(pixel, hash, spirv)` 生成 `.spv`，`BuiltinFileName()`，`CompleteSpirv()` 校验魔数 `0x07230203`，`Directory()` 统一缓存目录。`Version` 在上游 21 基础上递增，不沿用作者的 22。
- `gpu/shader/builtin_shaders.h`：把 presentation、blit、transfer、rect list GS 的 HLSL 源抽出，`__spirv__` 分支用 `vk::RawBufferLoad` 读常量。注意上游 v0.4.0 的 presentation 着色器已加入 Catmull-Rom 与 `filter` 参数，要以上游版本为准重新抽取。
- `tools/xenos_shader_tool/main.cpp`：`--vulkan` 开关与 `PrepareBuiltins()`。
- 4 个 `tools/tests/*.cpp` 的 `dxil` 改名。

### 第三批：渲染器与呈现

- `gpu/video.{cpp,h}`：`IsVulkan()`、`LO_GRAPHICS_API` 解析、`SDL_WINDOW_VULKAN`、`PLUME_SDL_VULKAN_ENABLED` 下用 `CreateVulkanInterface(window)`；Vulkan 时跳过 D3D12 独占全屏逻辑。上游 v0.4.0 已在此处加入 `ScaleResolvedSize`，合并时保留。
- `gpu/renderer.cpp`：`vulkan` 标志；pipeline layout 在 Vulkan 下用 24 字节 push constant 存三个 device address，`setBuilders` 扩到 5 个，set 4 放 64 项采样器调色板；`SetConstantBuffer(offset, index)` 封装两种路径；所有 `setGraphicsDescriptorSet` 调用点补 set 4；上传环用 `RenderBufferFlag::CONSTANT | DEVICE_ADDRESSABLE`；Vulkan 下 blit/transfer 管线的 `renderTargetBlend[0] = RenderBlendDesc::Copy()`（plume 的 UNKNOWN 哨兵会变成 `VK_*_MAX_ENUM`）。`SharedConstants` 的 5 个 `static_assert` 偏移（160、240、256、640、768）是布局检查点。
- `gpu/presentation.{cpp,h}`：`Init(device, vulkan)`；Vulkan 下纹理与采样器分成两个 set。必须按上游 v0.4.0 的 `Pass`、`ProcessSceneColor`、`DrawComposited` 结构重做，作者的版本不能直接套。
- `thirdparty/CMakeLists.txt`：非 Windows 时 `PLUME_SDL_VULKAN_ENABLED=ON`；Windows 下需要让 plume 的 Vulkan 后端参与编译并链接 volk。
- `LostOdysseyRecomp/CMakeLists.txt`：`lo_copy_dxc()` 把支持 SPIR-V 的 `dxcompiler.dll` 拷到输出目录；`LoPresentationTest --vulkan` 进 CI。

### 验证顺序

1. `LoPresentationTest --vulkan` 通过（作者已把该测试改成双后端）。
2. `LoShaderTool --scan <game> --vulkan --cache <dir>` 对四盘微码编译成功率不低于 DXIL 路径（DXIL 当前 2000 个中 1998 成功）。
3. `LO_GRAPHICS_API=vulkan` 跑到标题画面，再进 Map12 与 F1 捕获对比 D3D12。
4. D3D12 路径回归：现有 GPU 测试与捕获 ZIP 对比不变。

## v0.5.0 DX11：独立工作拆分（2026-09-07 新增 TODO）

下列均为待开发、待验证事项，DX11 与 Vulkan 不共用完成标记；详细顺序以[路线图](../ROADMAP.zh-CN.md#v050-pc-graphics)为准。

1. **先验证契约与最低能力。** 核对现有 render 抽象需要的格式、资源、绘制与同步操作。评估 DXBC／Shader Model 5 编译路线、逐 draw 资源／采样器重映射、slot／UAV 限制、几何着色器与同步回读。当前翻译器使用 Shader Model 6 profiles 和 64 项采样器数组，须验证转换方案；候选路线不视为定案，不支持项及回退策略要明确记录。
2. **实现真正的后端。** 接通设备／交换链、资源与管线状态、常量和资源绑定、resolve／拷贝／同步／回读，验证最低硬件能力及错误处理；保留 D3D12 可用入口。
3. **接入完整游戏路径。** 内置与微码 shader 编译、DX11 缓存、presentation、SMAA／TAA、内部分辨率、独立 UI 和捕获分别适配，不以 Vulkan 已通过代替 DX11 完成。
4. **独立验证和交付。** 采用与 Vulkan 相同层次的独立 GPU、固定场景与 D3D12 对照、三帧 F1 导出、缓存与窗口生命周期检查；单独记录硬件、实际包、玩家验收和能力限制，发布前同步构建依赖及中英文文档／UI。

## Switch 分支后续对齐（PC Vulkan 完成后）

- 在主库开 `switch` 分支，把快照中 `os/switch/`、工具链、`build_switch.ps1`、`Commit()` 的 libnx 实现、devkitA64 的 CMake 分支、FFmpeg 配置、`nvk_switch_stubs.c` 移过来；`main.cpp` 的探针环境变量改为命令行或配置项。
- vendored 三件套恢复为子模块加补丁：作者对 XenonRecomp 的增补合进 `XenonRecomp-lostodyssey.patch`，保留屏障、GCC 宏、`PPC_MUL_HI`、CR 位运算，去掉 `__rdtsc` 回退；plume 的 Switch 部分合进 `plume-lostodyssey.patch`。
- 把作者建的 `LoRuntimeCompileCheck` 目标接进 CI：它交叉编译全部运行时源码但不链接私有 NVK 和生成代码，用 devkitPro 官方 Docker 镜像即可，防止主库改动再次破坏 Switch 编译。
- 真机点亮沿用作者的探针阶梯：`LO_SWITCH_VIDEO_PROBE_ONLY` → `RENDERER` → `RECT_SHADER` → `BUILTIN_SHADER` → `DUMMY_RECORD` → `DUMMY_SUBMIT` → `GUEST_RECORD` → `GUEST_SUBMIT` → `GUEST_PRESENT`，作者停在最后一级 120 帧 4 draw。
- 已知风险与预案：rect list 几何着色器在 switch-nvk 路线图中未验证，备选是索引重写阶段拆成两个三角形；Cortex-A57 1 GHz 对 Xenon 3.2 GHz，考虑超频、LTO、lwsync 只用 acquire/release；必须以 Application 模式启动才有约 3 GB 内存；SD 卡需 exFAT；写 `docs/SWITCH.md` 记录工具链、NVK commit 与授权。

## 证据与位置

- 快照：`../LostOdysseyRecomp-main`（文件时间 2026-09-06 00:20 为快照基线，作者改动至 2026-09-07 15:33）。
- 作者构建产物：`../LostOdysseyRecomp-main/dist/switch/LostOdysseyRecomp/LostOdysseyRecomp.nro`（83,622,085 字节）及 2021 个 `.spv`；`out/switch-shader-cache` 4025 个 SPIR-V。
- 作者 diff：`../LostOdysseyRecomp-main/out/switch-author-vs-2d9ce9f.patch`。
- NVK：`../switch-nvk`，`README.md`、`ROADMAP.md`、`RESUME_NVK.md` 记录驱动已验证到索引绘制、混合、mipmap、零拷贝 WSI。
- 本文不代表任何 Switch 功能可用；快照未在真机上进入游戏画面。
