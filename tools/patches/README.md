# 本地依赖补丁

补丁操作说明随源码维护；日期化核验记录只说明对应提交和当时结果，其中“未发布”不代表现行发布状态。当前发布与验收见[项目状态](../../docs/STATUS.md)。

主仓库固定上游子模块提交，并保存本项目补丁；不要把依赖修改推送到上游仓库。
在首次检出的干净子模块上，从仓库根目录执行：

```powershell
git submodule update --init --recursive
git -C tools/XenonRecomp apply --check ../patches/XenonRecomp-lostodyssey.patch
git -C tools/XenonRecomp apply ../patches/XenonRecomp-lostodyssey.patch
git -C thirdparty/plume apply --check ../../tools/patches/plume-lostodyssey.patch
git -C thirdparty/plume apply ../../tools/patches/plume-lostodyssey.patch
```

XenonRecomp 补丁保存本项目指令、分析与上下文适配。plume 补丁保留 D3D12 readback 安全检查、stencil reference 赋值及深度清除矩形分批提交（避免大量矩形触发驱动退出），并使 pool／device 两处 `createTexture` 在原生资源创建失败时返回 `nullptr`，让调用方的分配失败回退能够识别失败；同时补充 Windows Vulkan 所需的 swapchain 图像数量、MRT viewport、device-address、上传一致性／readback 失效、捕获路径和相关资源创建处理。该补丁只描述本项目已核对的 plume 集成范围，不代表所有平台或 GPU 均已通过验证。
补丁应用后，这两个子模块有本地修改是预期状态。`.gitmodules` 对它们设置 `ignore = dirty`，日常主仓库状态不再反复提示补丁造成的工作树修改；子模块提交指针发生变化时仍会提示。检查实际修改可运行 `git status --ignore-submodules=none --short` 或进入子模块运行 `git status --short`。此设置也会隐藏额外的意外修改，因此修改依赖时仍需检查子模块状态及补丁。`build_runtime.bat`不自动应用补丁；`build_tools.bat`会检查并尝试应用XenonRecomp补丁，plume仍需按上面的命令准备。
已有本地修改时不要重复应用；可以用以下只读检查确认补丁已应用：

```powershell
git -C tools/XenonRecomp apply --reverse --check ../patches/XenonRecomp-lostodyssey.patch
git -C thirdparty/plume apply --reverse --check ../../tools/patches/plume-lostodyssey.patch
```

继续修改依赖后需同步相应补丁，并重新构建验证。游戏数据、生成的 PPC 代码及工具缓存不属于补丁。

## Android plume patch

`plume-android.patch` contains the Android-specific plume integration needed by
the experimental runtime. Apply it after `plume-lostodyssey.patch` when building
the `LO_BUILD_ANDROID_RUNTIME=ON` target. The Android runtime also enables the
SDL Vulkan bridge from the parent CMake target; the patch does not establish
device compatibility, 16 KB support or gameplay acceptance. Recheck the
submodule state and regenerate the patch from the intended upstream-patch base
after changing either the dependent source or this patch.

拉取更新了受跟踪的 XenonRecomp 补丁时，应先检查 `tools/XenonRecomp/` 的**实际已修改工作树**，将其与更新后的补丁谨慎同步；不要在已有修改上盲目重复应用，也不要丢弃无关的本地改动。确认实际源码与预期补丁一致后，按仓库根目录的正常顺序执行 `.\tools\build_tools.bat`、`python -B tools/ppc_codegen.py generate`、`.\tools\build_runtime.bat`。`build_tools.bat` 会尝试自动应用补丁，遇到部分更新的工作树时应先理顺源码与补丁，而不是随意重盖工具收据或复用旧生成器。仅增量构建运行时或执行 `python -B tools/ppc_codegen.py check`，都不能证明实际依赖源码已跟上受跟踪补丁；例如本地头文件中残留旧 `PPCTimeBase` 时，可能继续生成使用旧时钟路径的 PPC 代码。

Vulkan 改动应从受跟踪的 plume 子模块状态和上方补丁应用；它们不替代驱动提供的 `vulkan-1.dll`／ICD，也不要求另行捆绑 SDK。保持 plume 源码与补丁处于兼容提交，并在修改过的依赖树上应用前先审阅补丁。

2026-09-07 核验：保留原有 hunk 后，Plume 补丁从固定 HEAD `d890ac8` 应用到隔离 index／object store，所得 Git 规范化 blob 与当前源一致；XenonRecomp 补丁从固定 HEAD `ddd128b` 的同类核验也通过。现有工作树的 CRLF／混合换行导致部分原始文件字节不同，未重写换行或宣称 raw 字节一致；真实子模块源文件、index 和 HEAD 均未改变。证据：`out/v0.4.0-followup/patch-sync-validation.json`。

2026-09-14 精简核验：针对固定 HEAD `d890ac899e505fb30040e037a4037cdeca68f033`，Plume 补丁由 78,500 行／3,398,001 字节缩减为 1,849 行／92,265 字节，保留 6 个真实改动文件（含新增 `plume_log.h`），并消除 3 个同 SHA 的 `-dirty` gitlink 伪差异。正向 `apply --cached --check` 通过，应用后 tree 与旧补丁应用 tree 一致（133 个普通文件规范化内容及 gitlink 均核对）；当前 Plume 工作树 `reverse --check` 通过。未改动源码，未进行构建或游戏运行验证；该补丁尚未发布。

2026-09-29 D3D12 冗余状态过滤：D3D12 命令列表在管线与已绑定的相同时跳过 `SetPipelineState`，单个 viewport 或 scissor 与已绑定值逐位相同时跳过 `RSSetViewports`、`RSSetScissorRects`，再次绑定同一个 framebuffer 对象（按每个对象的创建序号识别）时跳过 `OMSetRenderTargets`；`invalidateCachedNativeState()` 会清除这些记录，命令列表开始录制以及 DLSS、插帧、FSR 和 `video.cpp` 的原生路径都已调用它。新补丁在固定 HEAD `d890ac8` 的临时干净 worktree 上依次应用原补丁和该过滤后生成，另一个干净 worktree 应用新补丁后的 index tree 与之相同，只有 `plume_d3d12.cpp`、`plume_d3d12.h` 两段变化。Vulkan 不受影响。

2026-10-01 Vulkan 插帧：补丁新增 `enableFrameInterpolationFeatures` 开关。只有打开时，Plume 才启用 FidelityFX 插帧依赖的 timeline semaphore、float16/int8、16-bit storage 和 subgroup size control 扩展及对应 feature；普通设备与 Streamline 路径的扩展列表不变。队列族加入 `reserve`/`release`（分配与释放加锁，预留队列不再分给虚拟队列，`createCommandQueue` 在无可用队列时返回空），`externalSwapchainSynchronization` 让替换 WSI 自行同步 present，`destroySwapchainBeforeResize` 让 resize 先销毁旧 swapchain 再创建新的，`resize()` 也会检查 `vkDeviceWaitIdle` 的结果。新补丁在固定 HEAD `d890ac8` 的临时干净 worktree 上重放，7 个文件按 LF 规范化后与本地依赖源码一致。

2026-10-01 遮挡查询（#118）：补丁新增 `RenderDevice::createOcclusionQueryPool`、`RenderCommandList::beginOcclusionQuery`/`endOcclusionQuery` 以及 `occlusionQueries`、`occlusionQueryPrecise` 两项能力；三个新方法在接口中都有默认实现（不支持时返回空池、空操作），因此 Metal 后端和 `plume-macos.patch` 无需改动。D3D12 使用 `OCCLUSION` 查询堆，每个查询结束时把结果解析进回读缓冲；Vulkan 使用 `VK_QUERY_TYPE_OCCLUSION`，开始查询前先确保 render pass 已开始，设备支持时使用 `PRECISE`，读取结果时带可用性标志（未使用的查询返回 `ResultUnavailable`）。新补丁同样在固定 HEAD `d890ac8` 的临时干净 worktree 上重放，7 个文件按 LF 规范化后与本地依赖源码一致，`plume-macos.patch` 仍可叠加应用。设计与验证见 [occlusion-queries.md](../../docs/notes/occlusion-queries.md)。

2026-09-27 Issue #70 状态缓存改动：Plume 的 D3D12 graphics/compute root signature 与 root descriptor table 去重，以及 descriptor heap、原生 root signature 变化、native `Reset`/`Close` 和外部状态失效路径已同步到本项目补丁。runtime、NGX/FSR D3D12 fixture、AF measurement fixture 和 root binding fixture 验证通过；独立临时 index 从固定干净 Plume 基线应用补丁并与本地依赖修改一致。未进行补丁发布或目标游戏性能验收。

2026-10-03 队列族传输位（#185）：Vulkan 规范规定图形和计算队列都支持传输操作，但单独声明 `VK_QUEUE_TRANSFER_BIT` 是可选的；高通 Adreno 专有驱动只声明图形和计算位，`pickFamilyQueue` 因此找不到队列族并报 "Required Vulkan queue family unavailable."。补丁在挑选队列族之前，为声明了图形或计算位的队列族补上传输位。新补丁在固定 HEAD `d890ac8` 的干净副本上先应用原补丁（重新生成与原补丁逐字节一致）再修改后生成，`plume-android.patch` 与 `plume-macos.patch` 仍可叠加应用。

2026-10-05 混合常量（#219）：`RenderCommandList::setBlendConstants` 设置 `BLEND_FACTOR`／`INV_BLEND_FACTOR` 读取的 RGBA 值，带默认空实现。D3D12 使用 `OMSetBlendFactor`；Vulkan 只在以 `dynamicBlendConstantsEnabled` 创建的管线上声明 `VK_DYNAMIC_STATE_BLEND_CONSTANTS`，命令列表记住该值，并在绑定这类管线时重新设置，因此中途绑定静态常量管线后也不会失效（D3D12 和 Metal 本来就跨管线保留）。新补丁在固定 HEAD `d890ac8` 的干净副本上生成，`plume-android.patch` 与 `plume-macos.patch` 仍可叠加应用。

2026-10-06 持久化管线缓存：`RenderDevice` 新增 `loadPipelineCache`、`getPipelineCacheData`、`getPipelineCacheSize`（默认实现表示不支持），`RenderGraphicsPipelineDesc` 新增 `cacheKey`。Vulkan 每个设备建一个 `VkPipelineCache`，图形和计算管线创建都传入它，`loadPipelineCache` 把保存的数据合并进去。D3D12 用 `ID3D12PipelineLibrary`：`cacheKey` 非零的管线按 `cacheKey` 与 VS/PS/GS 字节码的哈希命名，创建前 `LoadGraphicsPipeline`，未命中时创建后 `StorePipeline`；种子数据被拒（`D3D12_ERROR_ADAPTER_NOT_FOUND`、`D3D12_ERROR_DRIVER_VERSION_MISMATCH`、`E_INVALIDARG`）时换成空库。新补丁在固定 HEAD `d890ac8` 的干净副本上应用后与本地依赖源码一致，`plume-android.patch` 与 `plume-macos.patch` 仍可叠加应用。

2026-10-06 图形管线库（首次卡顿 P2）：Vulkan 设备支持 `VK_EXT_graphics_pipeline_library`（且 `graphicsPipelineLibraryFastLinking` 为真）和 `VK_EXT_extended_dynamic_state` 时启用两者，并报告 `capabilities.fastLinkPipelines`。`RenderGraphicsPipelineDesc::fastLink` 为真的管线由四个缓存的库部件快速链接（不做链接期优化，也不进 `VkPipelineCache`）：顶点输入（图元拓扑）、光栅化前（VS、GS）、片元着色（PS）、片元输出（混合和格式）。剔除、正反面、depth bias 和深度/模板状态改为动态状态，`setPipeline` 每次绑定这类管线时重新设置。部件以着色器模块句柄为键，所以这些着色器必须活到设备销毁。`RenderDevice::prepareGraphicsPipelineLibraries` 只建部件不链接，默认实现返回 false，Metal 后端和 `plume-macos.patch` 无需改动。新补丁在固定 HEAD `d890ac8` 的干净副本上应用后与本地依赖源码一致，`plume-android.patch` 与 `plume-macos.patch` 仍可叠加应用。

## macOS: plume Metal patch

`plume-macos.patch` applies on top of `plume-lostodyssey.patch` and changes `plume_metal.cpp`, `plume_metal.h`, `plume_apple.h`, `plume_apple.mm` and plume's `CMakeLists.txt` (the Apple files and the CMake change come with HDR output, PR #145):

- `MetalShader` also accepts SPIR-V and translates it to MSL with SPIRV-Cross (`thirdparty/SPIRV-Cross`), using the options of plume's reference converter (`examples/cmake/tools/spirv_cross_msl.cpp`) so the output matches the backend's binding model. MSL 2.3 is used instead of 2.1 because the runtime's SPIR-V reads 64-bit device addresses. Fast math is disabled to match DXC.
- `MetalDevice::createShader` returns null when translation or compilation fails, as failed Vulkan and D3D12 shader creation does.
- The Metal device reports `RenderShaderFormat::METAL` in its device capabilities, as the patched Vulkan and D3D12 devices do.
- Translated vertex shaders may write point size for every topology, which Vulkan ignores outside point lists. Metal rejects that unless the pipeline's input topology class is point or unspecified, so such pipelines leave the class unspecified.
- `copyTextureRegion` supports texture-to-buffer copies (readback into a placed footprint), which the Metal backend lacked; the runtime uses them for screenshots and captures. The source may be a swap-chain drawable, and the `CAMetalLayer` is created with `framebufferOnly = false` so presented images can be read back.
- `setFramebuffer` keeps the active render pass when the framebuffer is unchanged, as `plume-lostodyssey.patch` already does for Vulkan. The renderer rebinds its target between draws, and on Apple's tile-based GPUs every pass break stores and reloads the attachments.
- Clears with more than `MAX_CLEAR_RECTS` rectangles are split into batches, matching the D3D12 and Vulkan changes in `plume-lostodyssey.patch`; the quad clear otherwise overruns fixed-size arrays.
- `plume::SetMetalMinimumPresentDuration` makes the swap chain present each drawable with `presentAfterMinimumDuration`, so ProMotion displays follow the game's frame rate (the runtime's "Adaptive sync (ProMotion)" setting and targets above 60 FPS).
- `setBlendConstants` stores the value and applies it with `setBlendColor` before the next draw, again on every new render encoder.
- `loadPipelineCache` / `getPipelineCacheData` keep an `MTLBinaryArchive` for pipelines with a `cacheKey`. Creation first looks the pipeline up with `MTLPipelineOptionFailOnBinaryArchiveMiss`; a miss compiles as before and queues the descriptor, and `getPipelineCacheData` adds the queued descriptors and serializes the archive through a temporary file. Lookups skip the archive while it is being written instead of waiting. The runtime only enables it with `LO_METAL_BINARY_ARCHIVE=1`.
- `plume::EncodeMetalFxSpatialScale` encodes MetalFX's spatial scaler into the command list's buffer (the "MetalFX" scaling filter). The runtime links `MetalFX.framework` to plume in `thirdparty/CMakeLists.txt`.
- Drawable slots advance only in `acquireTexture`, on the presentation thread, instead of in present completion handlers.
- Present completion is recorded in state shared with the handlers, so a handler that runs after teardown never touches the swap chain. `resize()` and the destructor wait up to 10 seconds for outstanding presents, because their command buffers wait on the caller's events without retaining them. A present command buffer that completes with an error is logged through `plume_log.h` and does not fail later resizes or teardown.

Apply it after the upstream patch, from the repository root:

```sh
git -C thirdparty/plume apply ../../tools/patches/plume-lostodyssey.patch
git -C thirdparty/plume apply ../../tools/patches/plume-macos.patch
```

After changing either file, regenerate it against a copy of plume with only the upstream patch applied, so it stays independent of that patch:

```sh
ref=$(mktemp -d)/plume
git -C thirdparty/plume worktree add --detach "$ref" HEAD
git -C "$ref" apply "$PWD/tools/patches/plume-lostodyssey.patch"
git -C "$ref" add -A && git -C "$ref" -c user.name=ref -c user.email=ref@local commit -qm ref
cp thirdparty/plume/plume_metal.cpp thirdparty/plume/plume_metal.h thirdparty/plume/plume_apple.h thirdparty/plume/plume_apple.mm thirdparty/plume/CMakeLists.txt "$ref/"
git -C "$ref" diff > tools/patches/plume-macos.patch
git -C thirdparty/plume worktree remove --force "$ref"
```
