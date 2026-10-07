# 首次使用管线的卡顿：方案（2026-10-05）

> 状态：P0、P1 已实现（2026-10-06，见文末“P0/P1 实现与测量”）；P2、P3、P4 已实现（2026-10-06，见文末“P2 实现与测量”“P3 实现与测量”“P4 实现与测量”）。影响所有平台：Windows（D3D12 / Vulkan）、Linux、macOS、Android。

## 现象

第一次进入一张没去过的地图、第一次和新敌人战斗、第一次看某段过场时，会卡一帧到一秒多。按[Shader 准备](shader-preparation.md)里的配方机制收集管线，再多也收不全。

## 证据

**管线键。** `gpu::pipeline_cache::Key` 由以下字段组成：

- 着色器对：VS、PS；
- 渲染状态：混合、深度控制（含 stencil 操作）、剔除与 polygon offset 模式、颜色写掩码、图元类型；
- 格式：RT 格式、深度格式；
- stencil 参考值与掩码（正面、背面）；
- depth bias、slope bias。

注释里写明“不做任何规范化”。

**配方统计（Windows 上的 `pipelines.bin`，统计方法见文末）：**

| 来源 | 配方 | 无关位规范化后 | 去掉所有可动态化状态后 | VS/PS 对 | VS | PS |
|---|---|---|---|---|---|---|
| i212-run（316 张地图巡游） | 1950 | 1903 | 1246 | 1241 | 183 | 1088 |
| 本机游玩安装（Vulkan） | 920 | 888 | 548 | 545 | 114 | 497 |

结论：
- 状态排列只占约三分之一。把所有能做成动态状态的字段都拿掉，配方也只少 36%。
- 漏掉的管线主要来自新的 VS/PS 组合。组合数随地图、敌人、特效增长，录制收不全。
- 不同的着色器本身少得多（183 个 VS、1088 个 PS），shader pack 里总共 28687 条记录，是有限且已知的集合。

**Android（TB321FU，Turnip Mesa 26，Vulkan，已装匹配的 shader pack）：**

| 情况 | 进场景首帧间隔 | 当帧新建管线 | `pipeline_ms` |
|---|---|---|---|
| 第一次进 Highlands of Wohl | 1.34 s | 37 | 1056 |
| 第二次启动、同一场景 | 0.40 s | 5 | — |

- 第二次启动时，启动阶段预建 213 个配方用了 2.5 s（7 个 worker），平均每个约 80 ms。说明驱动没有任何磁盘缓存命中，每次启动都在重编。
- 剩下的 0.4 s 主要是 `bind_ms` 200 ms 和 `taa_ms` 161 ms，不属于管线问题，单独处理。

**各后端现在都没有持久化驱动缓存：**

| 后端 | 现状 |
|---|---|
| plume Vulkan | `vkCreateGraphicsPipelines` 传 `VK_NULL_HANDLE`，没有 `VkPipelineCache`；Android 也没设置 `MESA_SHADER_CACHE_DIR` |
| plume D3D12 | 没有 `ID3D12PipelineLibrary`。NVIDIA/AMD 驱动自带隐式磁盘缓存，所以桌面上重复进入还好，第一次仍然卡 |
| plume Metal | 着色器用 `newLibraryWithSource` 编译 MSL，PSO 用 `newRenderPipelineState`，没有 `MTLBinaryArchive` |

**其他已知限制：**
- `SDL_QUIT` 路径用 `_Exit` 绕过 Shutdown，最后一次 60 帧 checkpoint 之后学到的配方会丢失（见[Shader 准备](shader-preparation.md)）。这会让“收集过的管线”实际上没存下来。
- 配方上限 16384 条。

**驱动能力：** 平板上的 Turnip 驱动二进制里有 `VK_EXT_graphics_pipeline_library`、`VK_EXT_extended_dynamic_state` 1/2/3、`VK_KHR_pipeline_binary` 和 `MESA_SHADER_CACHE_DIR`。实现时要用设备扩展查询确认。

## 目标与验收

- 平板和 PC：第一次进入没去过的地图时，管线造成的单帧耗时 `pipeline_ms` 小于 30 ms。
- 第一次和新敌人战斗：任何一帧的 `pipeline_ms` 都小于 16 ms。
- 启动准备时间不比现在长；持久化缓存的磁盘占用有上限，并且会被清理。
- 测量方法：`LO_RENDER_TIMING=1`，按帧间隔和 `pipelines=` / `pipeline_ms=` 统计。

## 方案

### P0　运行时未命中日志（半天，先做）

每次在绘制时新建管线都打一行日志，包含：

- 帧号、当前地图 ID 或战斗编号；
- VS/PS 哈希，耗时；
- 这个键在配方里有没有、两个着色器有没有见过。

再写一个汇总脚本，按场景列出未命中。后面每个阶段都用它验收。

### P1　持久化驱动缓存（全平台，1~2 天）

- **Vulkan**：plume 为每个设备建一个 `VkPipelineCache`，所有 `vkCreate*Pipelines` 都传进去。
  - 缓存放在着色器缓存目录，比如 `pipeline_cache_vk.bin`。
  - 读入前校验 Vulkan 缓存头：`vendorID`、`deviceID`、`pipelineCacheUUID`。
  - 写盘随 `SavePipelineRecipes` 异步进行，并在正常退出时再写一次。
  - 以后可以换成 `VK_KHR_pipeline_binary`。
- **Android**：在加载 Turnip 之前，把 `MESA_SHADER_CACHE_DIR` 设到应用缓存目录。改动很小，能直接用上 Mesa 的磁盘缓存。
- **D3D12**：用 `ID3D12PipelineLibrary`，按键名 `StorePipeline` / `LoadGraphicsPipeline`。驱动已有隐式缓存，优先级较低。
- **Metal**：用 `MTLBinaryArchive` 存 PSO。
- **顺带修掉**：`_Exit` 退出路径前把配方和管线缓存 flush 到磁盘。

**效果**：第二次及以后几乎不卡，启动阶段的预建从秒级降到百毫秒级。第一次见到的组合仍然会卡，这一阶段不解决。

### P2　Vulkan graphics pipeline library（Vulkan 的主修复：Windows、Linux、Android）

思路：把编译成本从“每个组合一次”变成“每个着色器一次”。着色器集合有限而且已知（就是 pack 里的记录）。

管线拆成四部分：

| 部分 | 内容 | 粒度 |
|---|---|---|
| vertex input | 顶点输入（Xenos 的 vfetch 在着色器里做，这部分很小） | 几乎只有一个 |
| pre-raster | VS + 光栅状态 | 每个 VS 一个；剔除、正反面、depth bias 改成动态状态（EDS1 + Vulkan 1.0 核心） |
| fragment shader | PS + 深度/stencil 状态 | 每个 PS 一个；深度测试/写入/比较、stencil 操作用 EDS1，stencil 参考值/掩码用核心动态状态 |
| fragment output | RT/深度格式 + 混合 + 写掩码 | 不含着色器，创建很便宜，按需建并缓存 |

- **绘制时未命中**：fast-link 四个库（不开 LTO），预期亚毫秒级。同时把开了 `LINK_TIME_OPTIMIZATION` 的完整管线放到后台编译，编好后替换。
- **库的预编译**：
  - 启动时先为配方里出现过的着色器编库；
  - 之后在后台逐步为 pack 里的其余着色器编库；有 P1 的缓存，每个驱动版本只编一次；
  - 遇到新着色器（`GetShader` 第一次看到它）时立即编它的库。
- **plume 改动**：新增“创建库管线”和“链接管线”接口。改动量中等偏大。
- **回退**：设备不支持 GPL 或 EDS 时，走现在的完整管线路径。
- **风险**：某些驱动上 fast-link 出来的管线运行时偏慢，后台的优化编译可以弥补。28k 个库在 Android 上占多少内存需要实测，必要时只给配方和 pack 里的高频着色器常驻。

### P3　配方语料 + 按场景预取（全平台；D3D12 和 Metal 主要靠它）

- **无关位规范化**：stencil 关闭时清掉 stencil 字段；深度关闭时清掉比较函数；polygon offset 关闭时清掉 bias；写掩码为 0 时清掉混合。收益小（1950 → 1903），但没有成本，还能减少重复编译。要同时升级 `kPipelineRecipeVersion`。
- **分发语料**：用自动巡游收集配方，随 shader pack 一起发布。配方键里的格式是 `RenderFormat`，和后端无关。
  - 已有的地图巡游（316 张）和过场巡游（178 段）；
  - **新增战斗巡游**：通过调试入口逐个触发敌人编组。新敌人正是现在漏得最多的地方；
  - 玩家本地学到的配方继续合并进来。
- **按场景打标签和预取**：每条配方记录它出现在哪个地图 ID、战斗编组或过场里。游戏开始读某张地图、某场战斗时，在读盘画面期间用 worker 线程预建这个场景的配方。启动时只预建全局常用的那部分。

### P4　剩余未命中：并行化与上限

- 现在同一帧里的多个未命中是在渲染线程上串行创建的（37 × 约 28 ms）。遇到一个未命中时，可以在语料里找出共享同一个 VS 或 PS 的兄弟配方，并行预建。
- 可选开关：管线还没编好时跳过这次绘制（类似 DXVK async）。会导致短暂缺物体，默认关闭。

## 平台矩阵

| 平台 / 后端 | P1 | P2 | P3 | 预期 |
|---|---|---|---|---|
| Android / Vulkan（Turnip） | VkPipelineCache + Mesa 缓存 | GPL | 语料预取 | P1 后重复访问基本不卡；P2 后首次也基本不卡 |
| Windows / Vulkan、Linux / Vulkan | VkPipelineCache | GPL | 语料预取 | 同上 |
| Windows / D3D12 | ID3D12PipelineLibrary（驱动已有缓存） | 不适用 | 主要手段 | 首次卡顿取决于语料覆盖 |
| macOS / Metal | MTLBinaryArchive | MSL 已经按着色器编译，PSO 仍按组合 | 主要手段 | 同 D3D12 |

## 推荐顺序

P0 → P1 → P2 → P3 → P4。

- P1 改动小，立刻改善 Android 和所有平台的重复访问。
- P2 解决 Vulkan 平台的首次卡顿。
- P3 是 D3D12 和 Metal 的主要手段，同时提高所有后端的启动预建命中率。

## 统计方法

记录格式：48 字节文件头，之后每条 72 字节，前 64 字节是键（`Encode` 的顺序：vs、ps，再是 12 个 32 位字段）。

- 只用 Vulkan 核心动态状态和 EDS1 时，保留 (vs, ps, 有写掩码时的 blend, 写掩码, RT 格式, 深度格式, 是否 rect list)：i212-run 剩 1371 条。
- 再用 EDS3 把混合和写掩码也做成动态：剩 1246 条，即表中“去掉所有可动态化状态后”。

## P0/P1 实现与测量（2026-10-06）

**P0 未命中日志。** 绘制时新建的管线（`GetPipeline` 未命中）计入当帧；有未命中的帧在帧末打一行 `renderer: pipeline misses frame=… count=… ms=… map=… battle=… first_vs=… first_ps=… first_ms=… recipe=… vs_seen=… ps_seen=…`。`LO_PIPELINE_MISS_LOG=1` 再给每次创建打一行 `renderer: pipeline miss …`。

- `map`：`debug/map_info` 的地图定义 ID，切换中为 `-`；`battle`：本次运行第几场战斗（战斗核心最近 1 秒内有 tick），不在战斗中为 0。
- `recipe=1`：这个键已在配方集合里（读入的或本次学到的）但还没有建好，例如启动预建被跳过。
- `vs_seen` / `ps_seen`：此前有配方或已建管线用过这个着色器。两个都为 1 表示“已知着色器的新组合”，是 P2 要解决的那类。
- `tools/pipeline_misses.py <runtime log>` 按场景列出帧数、未命中数、总耗时、最差帧，有逐条日志时再列新 VS/PS 的数量。`LO_RENDER_TIMING=1` 的 `pipelines=` / `pipeline_ms=` 不变，两者数字一致。

**P1 驱动缓存。** 文件放在着色器缓存目录：`pipeline_cache_vk.bin`、`pipeline_cache_dx12.bin`、`pipeline_cache_metal.bin`。启动预建之前读入；预建结束后写一次，之后在写配方时（每 60 帧检查一次、有新配方才写）异步写回，正常退出时再写一次；只在数据变大时写，上限 256 MB（vkd3d-proton 下每条管线约 18 KB），超过上限时保留旧文件，经临时文件原子替换。

- Vulkan：plume 每个设备一个 `VkPipelineCache`，图形和计算管线都用它。读入前校验头部的 `headerSize`、`headerVersion`、`vendorID`、`deviceID`、`pipelineCacheUUID`，不匹配就从空缓存开始并覆盖。
- D3D12：`ID3D12PipelineLibrary`。只有 `CreatePipeline` 建的游戏管线带名字（键的哈希与 VS/PS/GS 字节码哈希），各种变体不入库。驱动拒绝旧数据时从空库开始。
- Metal：`MTLBinaryArchive` 已实现，但默认关闭，`LO_METAL_BINARY_ARCHIVE=1` 打开。理由见下表：命中并不比 Metal 自己的编译缓存快，载入 62 MB 文件却要 1.8 秒。PSO 的耗时主要不在后端编译，归档省不掉。
- Android：Java 侧设置 `MESA_SHADER_CACHE_DIR`（应用缓存目录下 `mesa_shader_cache`）和 `MESA_SHADER_CACHE_MAX_SIZE=256M`。
- `_Exit` 退出：渲染器初始化后在 `os::shaderlog` 登记一个退出回调，所有先调 `CloseForExit()` 的 `_Exit` 路径（窗口 `SDL_QUIT`、内核终止、只准备着色器）以及 `video.cpp` 关闭时 GPU 排空失败的路径，都会先写配方和驱动缓存，不等 GPU。
- `LO_NO_DRIVER_PIPELINE_CACHE=1` 关闭驱动缓存，用于对照。

**测量。** 启动预建用同一份 1950 条配方（i212-run 地图巡游，1933 条着色器齐全），`--prepare-shaders-only`，每台机器同一缓存目录连续运行。“冷”是没有缓存文件，“对照”是 `LO_NO_DRIVER_PIPELINE_CACHE=1`，“暖”是读入上一次写下的文件。

| 主机 / 后端 | 驱动自身磁盘缓存 | 冷 | 对照 | 暖 | 缓存文件 |
|---|---|---|---|---|---|
| psvita，Proton GE10-34 / vkd3d-proton D3D12，RADV | 关（`VKD3D_SHADER_CACHE_PATH=0`、`MESA_SHADER_CACHE_DISABLE=1`） | 2648 ms | 2631 ms | 888 / 894 ms（读入 18 ms） | 35.4 MB |
| psvita，原生 Linux Vulkan，RADV（Radeon 8060S） | 关（`MESA_SHADER_CACHE_DISABLE=1`） | 2124 ms | 2112 ms | 1022 / 955 ms（读入 4 ms） | 8.8 MB |
| M1 Max，Metal（macOS 26.6.2） | 系统缓存已热 | 13499 ms | 5245 ms | 5437 / 5265 ms（读入 1901 / 1764 ms） | 62.4 MB |
| M1 Max，Metal | 系统缓存清空 | — | 44847 ms | 44848 ms（1933 条全部命中） | 62.4 MB |

游玩：psvita D3D12，Uhra 存档开机进图（`skip_shader_prebuild=1`，所以全部管线在绘制时创建），两次运行之间只保留缓存目录：

| 运行 | 绘制时建管线 | `pipeline_ms` 合计 | 最差一帧 |
|---|---|---|---|
| 冷 | 243 | 3221 ms | 1893 ms（171 条） |
| 暖 | 244 | 50 ms | 26 ms（171 条） |

未测：Android 平板（TB321FU，Turnip）。0.8.38-dev-pipecache 调试包已装上，但平板锁屏，游戏停在启动阶段，没有得到预建数字；Mac 和原生 Linux 只测了启动预建，没有游玩。

## P2 实现与测量（2026-10-06）

**实现。** plume Vulkan 在设备支持 `VK_EXT_graphics_pipeline_library`（`graphicsPipelineLibraryFastLinking` 为真）和 `VK_EXT_extended_dynamic_state` 时报告 `fastLinkPipelines`。绘制时未命中（`GetPipeline`）的管线由四个库部件快速链接，不做链接期优化：

- 顶点输入：只有图元拓扑和 primitive restart；
- 光栅化前：VS（rect list 时加 GS），按 depth clamp、depth bias 是否开启区分；
- 片元着色：PS；
- 片元输出：混合、写掩码、格式。

部件按着色器模块和 render pass 兼容类（RT 格式、深度格式、采样数）缓存在设备上。剔除、正反面、depth bias、深度测试/写入/比较、模板测试/操作/掩码/参考值都是动态状态，每次绑定链接出的管线时重新设置。启动预建仍建完整管线。

预建结束后，一个后台线程按已知配方为它们的着色器建部件（`prepareGraphicsPipelineLibraries`），之后已知着色器的新组合只需链接。`LO_NO_PIPELINE_LIBRARY=1` 关闭，用于对照。D3D12 和 Metal 不受影响。

**测量。** psvita，Proton GE10-34 下的 Windows 版 Vulkan 后端（winevulkan → RADV，Radeon 8060S），Uhra 存档开机进图（Uhra 住宅区，map 20），`MESA_SHADER_CACHE_DISABLE=1`，空的驱动缓存。

| 情形 | 关（`LO_NO_PIPELINE_LIBRARY=1`） | 开 |
|---|---|---|
| 没有配方，`skip_shader_prebuild=1`：绘制时建管线合计 / 最差一帧 | 1807 ms / 1198 ms（171 条） | 1004 ms / 710 ms |
| 预建 1707 条配方（316 张地图巡游减去 Uhra 的 243 条），再进 Uhra：合计 / 最差一帧 | 1259 ms / 880 ms | 439 ms / 285 ms |

第二种情形里部件后台预建用了 6.0 s（1700 条配方），在进图之前完成。逐条看（`LO_PIPELINE_MISS_LOG=1`）：VS 和 PS 都见过的 172 条平均 0.24 ms；PS 是新的 62 条平均 5.2 ms，合计 324 ms，是剩下的主要部分。新着色器要靠 P3 的配方语料提前覆盖。

**验证。** psvita 原生 Linux 构建（RADV，Mesa 26.2.2），Vulkan validation layer 1.4.341，Uhra 开机进图，同样的设置下开/关各跑：

- 没有只在链接路径出现的消息。`VUID-vkCmdDraw-renderPass-02684`（render pass 的 dependency 数 2 与 0 不兼容）在 main 上就有：framebuffer 的 render pass 带两个外部依赖，管线的不带。用 `LO_VK_NO_PASS_DEPS=1` 去掉依赖后两种模式都是 0，说明部件用的 render pass 没有别的不兼容。
- 后台线程建完 1700 条配方的部件（0 失败），同时绘制时在链接，没有线程安全类消息；没有动态状态类消息。
- 截图与整体编译的版本对比，静态场景一致。

**稳态开销。** 最坏情况：`skip_shader_prebuild=1`、空缓存，所有游戏管线都是链接出来的，与全部整体编译对比（原生 Linux，ABBA 各两次，进图 10 s 后取 25 s）：

| 分辨率 | GPU 时间 | 录制 | 帧时间 |
|---|---|---|---|
| 1600x900 | 4.63 vs 4.31 ms（+0.32） | +0.21 ms | 9.91 vs 9.68 ms |
| 内部 3840x2160 | 16.04 vs 15.70 ms（+0.34） | +0.55 ms | 22.62 vs 22.20 ms |

GPU 多出的时间不随分辨率变，是每次绘制的固定开销（没有链接期优化的 VS→PS 接口），不是像素着色变慢。实际游玩中只有本次运行第一次见到的管线是链接出来的，下次启动由配方预建成整体管线，所以不做后台优化编译。正反面模板状态相同时只设一次（`VK_STENCIL_FACE_FRONT_AND_BACK`），减少绑定时的调用。

## P3 实现与测量（2026-10-06，分支 feat/pipeline-prefetch）

**规范化。** `gpu::pipeline_cache::Normalize` 在查找、记录和创建之前清掉 `DescribePipeline` 不读的位：Z 关闭或没有深度目标时的 Z 比较函数；stencil 关闭时的全部 stencil 字段和参考值/掩码；没开背面 stencil 时的背面字段；early-Z 位；polygon offset 开关（偏移量已经算进 bias）；深度测试关闭时的 bias；`-0.0` 斜率；写掩码为 0 时的混合（`DescribePipeline` 现在把这种绘制当作不混合）；点、线、线带、三角带、rect list 以外的图元都按三角形列表。绘制本身的启发式判断仍用原始键。

- 配方版本 2。版本 1 的文件读入时规范化，并在下次保存时改写；地图巡游的 1950 条变成 1902 条。
- 校验：用补丁版对 1950 条原始键逐条比较 `DescribePipeline(原键)` 和 `DescribePipeline(规范化键)`，只有 346 条斜率为 `-0.0` 的键按位不同，数值相等。

**文件版本 2。** 每条 104 字节：64 字节键、8 个场景标签、校验和。标签是 `类型 << 28 | ID`：地图为地图定义 ID，战斗为 loader 战斗槽里的战斗 ID；`0xF0000000` 表示在任何场景之外画的（标题、开机菜单）；超过 8 个场景记为“多场景”。`tools/pipeline_recipes.py` 读写两种版本（`info`、`merge`、`subtract`、`strip`），`merge OUT IN@battle:ID` 给不带标签的旧文件补标签。

**场景识别。** 引擎 tick 读 loader 的请求槽（`0x832631F0 + 3168`，槽 0 地图跳转、槽 1 战斗；相位在 +0，名字 FString 在 +4，ID 在 +16），也记下 world 里新出现的地图。当前场景：战斗中用战斗 ID；否则用当前地图；切换期间用正在加载的场景。

**预取。** 地图或战斗开始加载时，渲染线程在帧末把这个场景所有还没建的已知配方交给 worker 线程（线程数为逻辑核数一半）。建好的管线在渲染线程收进管线表；绘制碰到排队中的任务就自己建，碰到正在编译的任务就等它（Vulkan 有 pipeline library 时直接 fast-link）。`LO_NO_PIPELINE_PREFETCH=1` 关闭预取。

**语料。** 可选的 `shaders/pipelines_corpus.bin`（打开的 shader pack 旁边或 pack 的安装位置，`LO_PIPELINE_CORPUS` 可指定）并入已知配方，不写进玩家的文件，也不检查翻译器版本。语料作为 `shader-packs` 预发布里的单独文件发布，`index.json` 里以 `pipeline-corpus` 条目列出；运行时在 shader pack 检查之后用后台线程把它下载到安装目录的 `shaders/pipelines_corpus.bin`，缺失时下载，已有时开着自动更新每天最多检查一次，哈希不同就替换，下次启动生效（见 [PORTABLE_SHADER_PACK.md](../PORTABLE_SHADER_PACK.md#pipeline-recipe-corpus)）。第一版语料 3934 条：地图、过场和战斗巡游合并，去掉了 4 条启动预建时永远建不出来的 rect list 配方（rect list 的 VS 变体要到第一次这种绘制时才生成）。过场的 646 条没有场景标签，会在启动时全部预建；以后可以用带场景标签的构建重录过场，让它们改走场景预取。

**启动预建。** 默认预建玩家学到的全部配方，加上语料里没有标签、在场景外画过、或出现在至少 4 个场景（`LO_PIPELINE_PREBUILD_SCENES`）的配方；其余交给场景预取。`LO_PIPELINE_PREBUILD=all` 全部预建，`=common` 对学到的配方也用同一规则。Vulkan 的 pipeline library 预编译覆盖全部已知配方。

**测量。** 场景：Uhra 存档开机进 u3b（地图 20），15 秒后调试跳到第一次去的 u36（地图 16），各停 20 秒。语料就是同一场景上一次运行学到的配方（411–416 条），所以这是“语料完整”时的上限。每次运行都从空的着色器缓存目录开始；psvita 关掉了 vkd3d 和 Mesa 的磁盘缓存。下表“最长帧”取 `LO_RENDER_TIMING` 的 `draw_ms`，它包括绘制时创建着色器模块的时间（Metal 上这部分比管线本身贵）。

psvita，Proton GE10-34 / vkd3d-proton D3D12，RADV：

| 运行 | 启动预建 | 开机地图最长帧 | 跳图后最长帧 | 绘制时建管线 |
|---|---|---|---|---|
| P2 基线（cea0413a，无配方） | — | 2046 ms | （基线没有跳图命令） | 243 条，3189 ms |
| 有语料，不预建不预取 | — | 2072 ms | 1662 ms | 415 条，5387 ms |
| 全部预建（`LO_PIPELINE_PREBUILD=all`） | 412 条，1036 ms | 53 ms | 56 ms | 2 条，10 ms |
| 默认（学到的 + 场景外 + 常用） | 13 条，56 ms | 66 ms | 176 ms | 15 条，188 ms（另有 16 条等预取任务，共 38 ms） |
| 只预取（不预建） | — | 66 ms（标题 121 ms） | 122 ms | 25 条，293 ms |

- 开机：读档请求到地图出现约 3 秒；默认模式下 218 条配方（含 178 个着色器模块）在 0.9 秒内建完。
- 跳图：请求到地图出现只有 0.3–0.5 秒；172 条在 0.3 秒内建完，最后几帧仍有少量未命中或等待。这是提前量的上限，P4 可以继续处理。

psvita，Proton 下的 Vulkan（RADV，pipeline library 开）：不预取时 220 次绘制时创建全部 fast-link，合计 17 ms；默认模式 9 次、14 ms。两者地图里最长帧都在 56 ms 以内，差别是预取给出的是完整优化的管线。pipeline library 预编译覆盖语料，407 条在后台 1.5 秒。

M1 Max，Metal（同一二进制第二次以后运行，系统着色器缓存已热）：

| 运行 | 启动预建 | 开机地图最长帧 | 跳图后最长帧 |
|---|---|---|---|
| 无语料（新二进制第一次运行，冷） | — | 5148 ms | 1849 ms + 1229 ms |
| 有语料，不预建不预取 | — | 734 ms | 300 ms |
| 全部预建 | 412 条，382 ms | 61 ms | 46 ms |
| 默认 | 13 条，50 ms | 59 ms | 49 ms |
| 只预取 | — | 60 ms | 48 ms |

- Metal 上贵的是着色器模块（SPIR-V 转 MSL 再编译），原来在渲染线程上逐个创建：一次场景预取的准备花了 7.8 秒，全部预建 10.3 秒。现在模块也在 worker 线程上建：预取 390 条配方和 267 个模块 0.4 秒，全部预建 0.38 秒。

**启动预建的取舍。** 默认只预建玩家学到的配方和语料里的场景外/常用配方，其余靠场景预取：在测过的场景里首次进入的结果和全部预建相同，启动时间不随语料大小增长（D3D12 冷启动全部预建约 2.5 ms/条，Android 约 80 ms/条）。玩家自己学到的配方仍然全部预建，和 P3 之前一样。

**未测：** Android；战斗（战斗 ID 的请求槽读法来自 `patches/encounter_defer.cpp`，`scene load: battle` 日志还没有在真实战斗里核对过）；冷 Metal 系统缓存下的预取。

## P4 实现与测量（2026-10-06，分支 feat/pipeline-miss-parallel）

**一个 worker 池。** 场景预取（P3）、兄弟配方和跳过的绘制共用 P3 的 worker 线程（逻辑核数一半）。三个队列按优先级取任务：跳过的绘制要的管线 > 正在加载的场景 > 兄弟配方（最新一次未命中的在前，超过 128 个时丢最旧的）。场景任务可以占满所有线程；绘制时的任务（兄弟配方、跳过的绘制）同时最多 `clamp(逻辑核数 / 4, 2, 6)` 个，`LO_PIPELINE_MISS_WORKERS` 可改，0 两者都关。

**兄弟配方。** 绘制时未命中，渲染线程先把共用这个 PS 的已知配方、再把共用这个 VS 的（RT/深度格式和图元相同的在前）交给 worker，每次最多 16 条；每组里当前场景画过的配方在前。然后才自己建这条管线或等它，worker 和渲染线程同时编译。索引在第一次未命中时用还没建的已知配方（规范化键）建一次，之后新学到的配方都已经建好，不需要更新。`LO_NO_PIPELINE_SIBLINGS=1` 关闭。

- 兄弟配方的另一个着色器还没加载时，worker 用自己的 pack reader 读出、建模块，再建管线；渲染线程收任务时把模块放进着色器表。渲染线程自己要加载一个有任务的着色器时：排队中就自己执行这个任务，正在建就等它，所以一个着色器只有一个模块。同一个哈希出现第二个模块时也不释放，因为可能已经有管线用过它，而 pipeline library 按模块句柄缓存。
- 先排着色器任务、收进着色器表后再排管线任务的做法测过（33aab39c）：大部分兄弟配方来不及开始，被渲染线程自己建了，最长帧间隔比现在多约 200 ms。
- **Metal 默认关闭**（`LO_PIPELINE_SIBLINGS=1` 打开）：Metal 上管线很便宜，贵的是着色器模块（MSL 编译）。worker 上建的兄弟模块会挡住渲染线程自己的模块，M1 Max 上两次运行里有一次开机首帧从 0.75 秒变成 4.9 秒，另一次没有收益。

**等待规则。** 绘制碰到排队中的任务就自己建（`src=claim`）。碰到正在建的任务：如果它建的是同一种管线（有 pipeline library 时都是 fast-link，没有时都是完整编译）就等；有 pipeline library 而任务是场景预取的完整编译时，自己 fast-link（`src=inline`），任务结果丢掉。同一种管线不会建两次。

**跳过绘制（`LO_PIPELINE_ASYNC=1`，默认关）。** 主绘制路径上，管线还没建好（包括场景预取正在建的）时，这次绘制直接跳过，管线交给 worker（`src=defer`），物体会短暂缺失；场景拷贝提升路径从不跳过。跳过的绘制不计入 `drops.pipeline`，记在 `pipeline workers ... skipped=` 里。被跳过的也可能是深度、模板或之后要 resolve 的绘制，所以不只是少画一个物体。

**读包。** worker 线程各开一个 pack reader：`Reader::Get` 在锁内解压整块，共用一个 reader 会挡住渲染线程自己的着色器加载。启动时 `LoadPackShaders` 的线程也各用一个（psvita 上最多 32 个，每个读 2.5 MB 索引），启动反而更快：Vulkan 为 1270 个已知着色器建模块 1026 → 124 ms，B 的启动预建 D3D12 2000 → 1654 ms、Vulkan 2093 → 1099 ms。

**日志。** 每次未命中多了 `src=create|claim|inline|defer`；每帧多一行 `renderer: pipeline workers frame=… queued= built= hits= skipped= shaders=`（`hits` 需要 `LO_PIPELINE_MISS_LOG=1`）。pack 着色器的加载现在计入 `shader_ms`，和之前的版本比较时要注意。`tools/pipeline_misses.py` 多了兄弟配方的列。

**测量。** psvita，Uhra 存档开机，每次运行都从空的着色器缓存开始，vkd3d 和 Mesa 的磁盘缓存关闭；基线是 P3 分支头 0cbc7d6c。“合计/最差一帧”是绘制时建管线和等任务的时间；“最长帧间隔”和“超过 50 ms 的帧合计”取地图出现之后的 `between_ms`。三个场景：

- A：地图巡游的 1950 条配方（无场景标签）作为玩家文件，不预建。地图的配方都已知但没建，是兄弟配方要处理的情况（跳过启动预建的玩家、预取没赶上）。
- B：去掉 Uhra 配方的 1707 条，启动预建。地图的管线都是新的，没有兄弟配方可建，用来确认不变慢。
- C：P3 的语料（同一场景学到的 411 条，带标签），默认预建，15 秒后跳到 u36。

Proton / vkd3d-proton D3D12：

| 场景 | 运行 | 合计 | 最差一帧 | 最长帧间隔 | 超过 50 ms 的帧合计 |
|---|---|---|---|---|---|
| A | P3 基线（两次） | 3049 / 3148 ms | 1831 / 1962 ms | 1995 / 2140 ms | 3129 / 3239 ms |
| A | P4 | 1672 / 1630 ms | 1285 / 1329 ms | 1443 / 1490 ms | 1823 / 1769 ms |
| A | P4 + 跳过绘制 | 13 ms | 9 ms | 129 ms | 184 ms（跳过 5691 次绘制） |
| B | P3 基线 / P4 | 2921 / 2928 ms | 1768 / 1779 ms | 1878 / 1881 ms | 3186 / 3151 ms |
| C | P3 基线 | 164 ms（15 条） | 100 ms | 162 ms | 503 ms |
| C | P4 | 61 ms（6 条） | 43 ms | 61 ms | 336 ms |
| C | P4 + 跳过绘制 | 13 ms | 13 ms | 61 ms | 280 ms |

Proton 下的 Vulkan（RADV，pipeline library 开）：P3 起启动时为全部已知配方建着色器模块和 library，A 里的未命中都是 fast-link，P4 没有可省的。三个场景的差别都在两次基线之间的波动内（A 的合计 20 / 17 ms、最长帧间隔 57–58 / 54–57 ms；B 410 / 417 ms；C 13 / 9 ms）。跳过绘制时 A 的合计 4 ms。

M1 Max，Metal（系统着色器缓存已热）：没有可测出的差别。A 的最长帧间隔：基线 750 / 742 ms（主要是渲染线程建着色器模块），P4 默认（兄弟配方关）738 / 729 ms，兄弟配方打开时 4881 / 780 ms；B 341 / 343 ms；C 的超过 50 ms 帧合计 298 / 300 / 250 ms（第三个也是兄弟配方关）和打开时的 242 ms 都在同一波动范围里。4.9 秒那一帧里有 282 个兄弟着色器模块，渲染线程的 `shader_ms` 是 4.8 秒；可能是它在等 worker 上的模块，也可能是 MSL 编译之间的争用，日志分不出来。跳过绘制对 Metal 帮助不大（A 713 ms），因为贵的是着色器模块，不是管线。

**兄弟配方的浪费。** D3D12 的 A 里开机进图时排了约 1400 条兄弟配方，建了约 800 条，这张地图用到的不到 100 条。多建的都是别的地图的真实配方，以后会用到，但每次这样的爆发会让 P1 的驱动缓存多出约 14 MB，也多占显存。

**未测：** Android（8 核上绘制时任务只有 2 个线程，和游戏线程的争用没有测）；原生 Linux Vulkan；冷 Metal 系统缓存。

## 语料巡游（2026-10-06，分支 feat/battle-tour）

**战斗入口。** `LO_DEBUG_BATTLE_FILE` 指向一个命令文件，序号变大时执行一次：`序号 battle <编组> [战斗地图]`、`序号 victory`（调试菜单的判胜）、`序号 list`（把编组表写进日志）。不设这个变量时整个功能不工作。编组走游戏自己的随机遇敌路径：行走更新 `sub_829E3048`（角色 vtable +1632）调用的抽签 `sub_829E3268`（+1636）直接返回 1，并把编组写进它的输出槽；行走更新随后照常调用 `sub_82B53AD8`、`RequestBattle 0x828278A0(GEngine, 编组)`、`sub_8285E380`。站着不动也会触发。

**编组表。** `0x83264978 + 128` 处的 TArray，399 项，每项 160 字节：+0 战斗地图（空串时用区域的战斗地图 `0x832631F0 + 1604`，再空用 `u10_b3_scrw`），+12 音乐，+60 AI 脚本，+72/+84 开场/结束序列，+148/+152 敌人槽数组（每槽 32 字节：槽号、敌人配置即模型、敌人参数）。共 170 种敌人模型、56 个固定战斗地图；另有 33 个区域战斗地图没有编组直接引用。

**工具。** `tools/recipe_tour.py`（用法见文件头）：
- `list` 导出编组表；`battles` 按“先覆盖全部敌人模型和固定地图、再覆盖模型参数组合”的顺序打编组，模型都打过的编组跳过，可用 `--part K/N` 分给多台机器；`stages` 用 3 号编组（两只弱敌）在给定的战斗地图上各打一场；`maps` 逐图跳转并转动镜头，需要带地图跳转、换盘请求和包探测命令文件的诊断构建（不进 main）。
- 每段最多 15 分钟；共享测试机上另有任务等锁时立即让出。
- 战斗里每 2 秒按 A，第一次指令后 45 秒请求判胜；失败、强制判胜或卡住时重启游戏，自然胜利或逃跑回到地图后继续下一场。

**结果。** psvita（D3D12）和 M1 Max（Metal）并行：94 个编组打完，覆盖 163/170 种敌人模型（未覆盖的 7 种只出现在会卡住或资源不在光盘上的 4 个编组里）；33 个区域战斗地图；250 张地图（另有 2 张报资源缺失、5 张不在任何光盘上）。合并成场景标签版语料：地图 3005 条，过场再加 646 条（没有场景标签），战斗 271 条，战斗地图 16 条，共 3938 条、285 个 VS、2075 个 PS。存档里只有 10 级的凯姆一人，多数战斗在敌人第一轮行动后失败，所以每种敌人只看到一轮技能。
