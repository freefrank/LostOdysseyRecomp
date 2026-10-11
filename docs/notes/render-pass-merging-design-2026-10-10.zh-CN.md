# 合并 render pass：v1.0.0 设计调研

- 日期：2026-10-10。只读调研，没有改代码、没有构建、没有跑游戏。
- 代码基线：main `aaaf38fa`（worktree `li-1-0-0-progress-a93a50`）。`gpu/renderer.cpp` 和 `tools/patches/` 从 v0.9.22（`d3888a0d`）到 `aaaf38fa` 没有改动，所以平板 v0.9.22 的 trace 和这里引用的行号对得上。
- plume 引用：`tools/patches/plume-lostodyssey.patch` 的行号，加上已打补丁的 `C:\Users\freefrank\worktrees\LostOdysseyRecomp\apk-v100\thirdparty\plume\`。apk-v100 里的这份文件带着诊断插桩，行号会变，所以下文按函数名引用。
- 数据来源（除标"估计"的地方外都是实测）：
  - **平板 D1/D2**：TB321FU，Adreno 750，Turnip 26.0.0 R8，v0.9.22 加诊断补丁，原生 2560x1600，Uhra，热态 status 3。文件在 `apk-v100\out\perf\`。D1 是默认设置，D2 是高设置（FXAA + GTAO + 阴影 4x）。
  - **手机 T1**：OPPO PLP110，Adreno 840，0.8.53-dev，2616x1140，FXAA + GTAO + 阴影 4x，PR #302 之前。文件在 `apk-289\out\perf\T1-trace.runtime.log`。PurpleVK 的时间戳偏小，要乘约 1.65，各部分的相对比例不受影响。

---

## 0. 结论

### pass 为什么这么碎

一帧大约 305–320 个 Vulkan render pass。平板 D1 是 307 个，其中 253 个只做一次操作。按 pass 的起点分类：

| 类别 | D1 pass 数（单操作） | D1 GPU ms | 断开原因 |
|---|---|---|---|
| A. EDRAM 别名清除传播 | **145（138）** | 2.69 | 游戏用一个 pitch 清 EDRAM。渲染器把清除逐个复制到同一 tile base 的其他宿主纹理（别名），每个别名 `setFramebuffer` 一次，单独成一个 pass。 |
| B. 主视角 2560x1636 | **77（65）** | 25.8 | 约 67 个是"纯深度（stencil 标记）↔ 颜色+深度（光照）"逐光源交替，每个 pass 只有一个 draw。其余是 Z-prepass、两个大颜色 pass，以及被 resolve 和 transfer 打断的 pass。 |
| C. 阴影/离屏深度 | 57（24） | 0.99 | 阴影投射批次被 A 类清除切碎，48 个 pass 共 171 个 draw。 |
| E. 游戏小 RT（后处理链） | 11（10） | 2.73 | 真实的目标切换：半分辨率、bloom 这类。 |
| F. resolve blit（D32→R32F，FP16→RGBA8） | 8（8） | 2.22 | resolve 需要读完整的源，必须在源 pass 结束之后做。 |
| G. EDRAM 格式转换（transfer） | 8（7） | 3.17 | 同一 tile 换了颜色类别，先单独画一个转换 pass，再回到原来的 framebuffer。 |
| I. present | 1 | 1.42 | |
| pass 外的 resolve 拷贝（不算 pass） | 9 次 | 间隙 3.44 | 同格式 resolve 用 `vkCmdCopyImage`。D1 的 4.0 ms 间隙里，这部分占 3.44 ms。 |

pass 结束的方式：`setFramebuffer` 换目标约 260 次，barrier 约 46 次，copy 0 次。copy 不出现是因为 copy 前面的 barrier 已经把 pass 结束了。

手机 T1 的结构一样：324 个 pass 里，A 类 149 个（143 个单操作），主视角交替 73 个，阴影 40 个，blit 8 个，transfer 8 个。

### 机制：为什么 Turnip 全帧走 sysmem，LRZ 也被关掉

- plume 的每个 render pass 都是 `LOAD/STORE`，清除一律用 pass 内的 `vkCmdClearAttachments`。
- Turnip autotune 的带宽模型里：
  - GMEM 的代价是每个 LOAD 附件和每个 STORE 附件各算一份 cpp；
  - sysmem 的代价只计被清除的附件。

  所以只有一两个 draw 的 pass 几乎总是选 sysmem。强制 GMEM 时，每个 pass 都要把整张附件载入再写回，于是慢 3 倍。（来源见 §5，基于 Mesa 的 AOSP 镜像，2025-03 快照。）
- LRZ（Turnip 的低分辨率深度剔除）：
  - `vkCmdClearAttachments` 清深度会把当前 pass 的 LRZ 关掉；
  - `loadOp=CLEAR` 不会关；
  - A650 及以后有方向追踪，深度以 LOAD 进入下一个 pass 时可以沿用上一个 pass 的 LRZ 状态。

  平板上，主深度先在 D1 第 5 个 pass 用 `vkCmdClearAttachments` 清（A 类，单操作），第 6 个 pass 才是 1090 个 draw 的 Z-prepass。改成 loadOp=CLEAR 后，Z-prepass 一开始就有有效的 LRZ。这一点能省多少，要到平板上实测才能定。
- TU_DEBUG=perf 报的 LRZ 关闭原因（以下次数是 P3 运行约 22 秒的累计，不是每帧）里：
  - `CmdClearAttachments`（55k）和一部分 `Depth write + ALWAYS`（22k，游戏的清除矩形 draw）能靠本方案消掉；
  - `stencil write based on depth test`（37k）和 `FS writes depth`（25k）来自游戏自己的 shader 和状态，合并 pass 解决不了。

### 合并能带来什么

- **sysmem 下 pass 本身的固定开销不大。** 主视角交替 pass 之间的间隙约 1 µs，最小的单操作 pass 约 4–10 µs，300 个 pass 加起来大约 1.5–3 ms。（估计，来自 D1 中最小单操作 pass 的耗时分布。）实际能省下的，主要是不再执行的工作（A 类 2.7 ms），以及 LRZ。
- **最大的潜在收益是主视角走 GMEM**：25.8 ms，占 pass 时间 60%，其中大量是 FP16 的读写带宽。这需要先把 77 个 pass 合成个位数，再配合 CLEAR/DONT_CARE 的 load/store。但 2440 个 draw 的 binning 开销可能把收益吃掉。手机上 KEEP_COLOR 实验把 pass 从 318 降到 253，反而慢了 9%，所以这一项**没有验证过**，要按阶段 0 的实验来决定。
- **720p 新默认下，平板接近渲染线程上限**：GPU 17.1 ms，`draw_ms` 20.2 ms。GPU 省下的时间主要在 1080p 以上（GPU 27.0 ms 对 26.1 ms）和原生分辨率时才变成 FPS。720p 下只表现为 GPU 更闲、发热更少。
- **Apple 上收益应该更大**：每次切换 encoder 都要把 tile 写回内存。KEEP_COLOR 在 M1 Max 上只少了 60 个 encoder，就快了 15%。Mali 每个 pass 都必须 tiling，也会受益，但目前 Mali 被 int64 和描述符集数量的门槛挡着，进不了游戏。
- **桌面**：D3D12 没有 pass 的概念，阶段 1 只是把清除命令推迟、合并或丢掉，命令只会变少。桌面 Uhra 720p 受 CPU 限制，方案不能给每个 draw 加 CPU 开销，阶段 1 只在 bind 时多一次判断。

### 推荐第一阶段：EDRAM 清除延迟执行，plume Vulkan 支持 loadOp=CLEAR

- **怎么做**：
  - 别名清除传播（A1 的颜色 wipe、A2 的深度别名清除）、resolve 附带的清除（`RB_COPY_CONTROL` 0x100/0x200）和 `ClearDepthTarget`，都不再立即录命令，改成在纹理上记一条"待清除"。
  - 这张纹理下次被绑成附件时，在那个 pass 的开头执行：整张覆盖就用 loadOp=CLEAR，部分覆盖就用 pass 内的 `vkCmdClearAttachments`。
  - 下次是被读取（resolve、transfer、拷贝、读回）时，先执行掉。
  - 被新的整张清除覆盖时，直接丢掉旧的那条。
- **预期**（估计）：
  - pass 从约 305 降到约 170–185，少 120–135 个；
  - A 类 2.7 ms 的大部分不再执行；
  - 主深度和阴影深度从 loadOp=CLEAR 开始，LRZ 可能因此受益，但还没量化；
  - Metal 少掉对应的 encoder；
  - D3D12 少掉多余的清除。
- **为什么风险低**：清除值不变，每张纹理上的操作顺序也不变，所以结果逐字节一致。唯一真正的风险是漏掉了某个"执行点"（见 §3）。
- **工作量**：5–7 天。

### 分阶段

| 阶段 | 内容 | 预期减少的 pass（估计） | 工作量 |
|---|---|---|---|
| 0 | 测量和诊断：常驻的分类计数；`LO_VK_NO_END_DEP` A/B；KEEP_COLOR 分别在 `TU_DEBUG=sysmem` 和 `gmem` 下复测；记录每个 pass 选了 GMEM 还是 sysmem | 0 | 1.5–2 天 |
| 1 | 清除延迟执行，plume Vulkan 支持 loadOp=CLEAR | 120–135 | 5–7 天 |
| 1b | 游戏自己的深度清除矩形 draw 也改成延迟清除（条件保守）；颜色清除矩形的重放也推迟 | 30–45 | 3–4 天 |
| 2 | transfer 画进目标自己的 pass；主视角纯深度和颜色交替的合并（由阶段 0 的结果决定开不开）；渲染器自有的整屏 pass 用 loadOp=DONT_CARE | 70–75 | 3–5 天，另加测量 |
| 3（建议放到 1.0 之后，或看阶段 0–2 的数据再定） | 帧级延迟录制，决定 storeOp；resolve 拷贝写时复制；pass 内 resolve（`dynamic_rendering_local_read` 或 subpass）；按 Turnip 调 GMEM 和 autotune | 视设计而定 | 10 天以上 |

做完阶段 0–2，预计一帧剩 60–90 个 pass（估计）。

### 最大的风险

- **别名的读取先于待清除执行**：某条读取或写入路径漏掉了执行点，读到的就是清除前的 EDRAM 内容。对策：所有访问集中到三个入口；debug 模式下给漏掉的情况涂洋红；做 eager/lazy 的同二进制 A/B 和夹具读回对比。
- **Adreno autotune 翻转**：pass 变大、load op 变化之后，autotune 可能改选 GMEM，结果反而更慢。KEEP_COLOR 的 −9% 就是先例。每一步都要在平板热态下分 pass 实测。

### 另外：GTAO 在 Adreno 上很贵（与合并 pass 无关）

- D2 中 GTAO 两个 pass：可见性 1280x800 RG32F 2.79 ms，合成 2560x1600 RGBA8 7.99 ms，共 10.8 ms，占 pass 时间 18%。
- 平板 trace 里这两个 pass 被标成了 "TAA resolve/motion"，原因是 `ambient_occlusion.cpp` 没有站点插桩，沿用了上一个站点。D2 根本没开 TAA。
- 对照：RTX 5080 在 2160p 下 GTAO 只多 1.30 ms。手机上的合成 pass 在 PR #302 后是 3.30 ms（PurpleVK 单位）。
- 建议单独立项：减少合成 pass 在 Adreno 上的采样数，或改成半分辨率合成。Android 默认保持关闭（现在就是关的）。

---

## 1. 现状地图

### 1.1 plume Vulkan：render pass 什么时候开始、什么时候结束

| 行为 | 位置 | 说明 |
|---|---|---|
| 开始 pass | `VulkanCommandList::checkActiveRenderPass` | 只在 draw、clear、`beginOcclusionQuery` 时开始。renderArea 永远是整个 framebuffer，只有 1 个 subpass，`VK_SUBPASS_CONTENTS_INLINE`。 |
| 换 framebuffer 结束 pass | `setFramebuffer`（补丁 3275 起） | 同一个 framebuffer 重复绑定时直接返回，不结束 pass。 |
| barrier 结束 pass | `barriers`（补丁 3123–3150） | 只有真正录了 barrier 才结束。布局相同、只读、阶段已覆盖的条目会跳过。访问掩码一律是 `MEMORY_READ\|WRITE`。 |
| copy、resolve、dispatch、query reset、外部命令结束 pass | `copyTextureRegion`、`copyTexture`、`copyBuffer*`、`resolveTextureRegion`、`dispatch`、`resetQueryPool`、`beginExternalCommands`（补丁 3083） | |
| clear 不结束 pass | `clearColor`、`clearDepthStencil` | 都是 pass 内的 `vkCmdClearAttachments`。如果 pass 还没开始，会先开一个，所以一次单独的清除就成了一个单操作 pass。 |
| load/store | `VulkanFramebuffer` 构造函数；`VulkanGraphicsPipeline::createRenderPass` | 颜色和深度一律 `LOAD/STORE`，initial 和 final layout 固定。只读深度在支持时用 `NONE`。没有 DONT_CARE、transient 或 lazily allocated。 |
| subpass 依赖 | `setAttachmentPassDependencies`（补丁 2033–2081） | 开始依赖：EXTERNAL→0，src 含 TRANSFER 和 FRAGMENT_SHADER。结束依赖：0→EXTERNAL，dst 含 TRANSFER 和 FRAGMENT_SHADER。注释写明结束依赖不是 framebuffer-local，tiler 会在每个 pass 末尾等待并刷缓存。`LO_VK_NO_END_DEP=1` 可以去掉结束依赖，但一直没测过。 |
| 管线兼容 | `getCompatibleRenderPass` | 管线建在一个兼容的 LOAD/STORE render pass 上。兼容性不看 loadOp 和 storeOp，所以加 CLEAR/DONT_CARE 变体不用重建管线。 |

### 1.2 Metal 和 D3D12

- **Metal**：
  - `setFramebuffer` 会结束 encoder，`plume-macos.patch:566` 加了同一 framebuffer 不重复结束的判断。
  - `clearColor` 和 `clearDepthStencil` 遇到整张清除时，记成 pendingClears，下一个 encoder 用 `LoadActionClear`；部分清除画四边形。
  - storeAction 一律 Store。
  - 如果一次整张清除后面没有任何 draw，`handlePendingClears` 会为这次清除单独开、关一个 encoder，相当于一个只做清除的 pass，同样要把整张写回。
- **D3D12**：
  - 没有 render pass。`setFramebuffer` 是 `OMSetRenderTargets`，清除是 `Clear*View`，barrier 是 `ResourceBarrier`。
  - 合并 pass 对 D3D12 只意味着清除和 barrier 的命令变少。subpass 一类的方案在 D3D12 上没有对应物，只能按后端条件开启。

### 1.3 EDRAM 模拟模型（renderer.cpp）

- **渲染目标缓存**：
  - 键是 `(tile base, 存储类别, pitch, 是否深度)`，见 `RenderTargetKey` 486–492 和 `GetRenderTarget` 7083–7176。
  - 高度不进键。高度根据 scissor 猜，变高时重新分配，并把重叠部分拷过去（7146–7170）。
  - 每个 `(base, pitch, 类别)` 对应一张独立的宿主纹理。同一段 EDRAM 用不同 pitch 或不同类别访问，就是不同的纹理，也就是"别名"。
- **宿主格式**：
  - 8888、2_10_10_10、7e3、16_16_16_16 都存成 `RGBA16F`（`ClassHostFormat` 6909）；
  - 16_16 用 `RG16F`，32F 用 `R32F`/`RG32F`；
  - 深度一律 `D32_FLOAT_S8_UINT`（7130）。

  每像素 8 字节，对 sysmem 带宽和 GMEM 的 bin 数都是一项固定负担。改格式会改变混合精度，做不到逐字节一致，不在本方案范围内。
- **类别切换（transfer）**：
  - `tileOwner` 记录同一 `(base, pitch)` 当前属于哪个类别（`AcquireColorTarget` 7189–7214）；
  - 类别变了，就用 `TransferRegion`（2698–2745）画一个全屏三角形，按客体位表示重新解释。目标 framebuffer 是 `(dst, nullptr)`，单独成一个 pass。
- **清除**：
  - Xbox 360 的 Clear 是一个屏幕空间矩形 draw，经常用和场景不同的 pitch 或位深。
  - 渲染器先把这个 draw 照常画在它自己的目标上；
  - 再把效果传播到同一 tile base 的所有别名（12190–12350）：
    - **深度填充时擦除所有颜色视图**（12238–12269，trace 站点 12257，类别 A1）：遍历同一 base 的所有颜色纹理，每张 `setFramebuffer(tex, nullptr)` 加一次整张 `clearColor`，结束后在 12269 重新绑回原来的 framebuffer；
    - **深度别名清除**（12280–12312，站点 12292，类别 A2）：同 base、不同 pitch 的深度纹理，`MapDepthClear` 映射成 tile 矩形，Vulkan 上再合并矩形，然后 `clearDepthStencil(rects)`；
    - **颜色清除矩形重放**（12313–12345，站点 12332，类别 A3）：用拉伸的 viewport 把游戏的矩形 draw 在别名上重画一遍。它是 draw，不是 API 清除。
- **resolve**：
  - `ResolveImpl`（13100–13299）把 EDRAM 纹理的一个矩形拷到代表目标内存的 `ResolvedSurface`；
  - 同格式时，`ResolveOnGpu` 在 pass 外用 `copyTextureRegion`（12950–12968）；
  - 不同格式（FP16 到 RGBA8）用 `BlitRegion`（3855–3889），自成单 draw 的 pass；
  - 深度在 Vulkan 上同样用 `BlitRegion`，D32 写到 R32F（12848）；
  - resolve 附带的清除：颜色在 13280–13296，`setFramebuffer(color, nullptr)` 加矩形清除；深度走 `ClearDepthTarget` 13301–13312；两者各自成一个单操作 pass；
  - 新分配的 resolve 目标还会先整张清一次（12935）；
  - 采样逻辑尺寸比 resolve 存储小时，还要再拷一次，见 `fetchViews`（7746–7787）。
- **普通 draw**：
  - `DrawImpl`（8477 起）先取颜色和深度目标并做布局转换（8611–8650）；
  - 纯深度 draw（颜色写掩码为 0）默认绑 `(nullptr, depth)`，Apple 上保留颜色附件（8723–8746，`draw_attachment_policy.h`）；
  - 在 10469–10470 绑定 framebuffer，每个 draw 都会调一次，同一 framebuffer 不会打断 pass。
- **主路径之外会插 pass 的地方**：
  - AO：`ambient_occlusion.cpp:157–170`，可见性和合成两个 pass，前后都有 `barriers(ALL)`；
  - HDR sidecar：10825–10845、10959–10977，sidecar 自成 pass，之后再绑回游戏的 framebuffer；
  - FSR alpha 重放：11133–11158，深度在 DEPTH_READ 和 DEPTH_WRITE 之间来回转换；
  - 运动矢量重放：只有 Metal 排队批量画（8990），Vulkan 是逐 draw 插进来的，只在 TAA/SR 开启时出现；
  - present 和 FXAA：`presentation.cpp:820–918`，有黑边时在 pass 内清除（861）。
- **提交点**（也会结束 pass）：
  - 帧末 `command_processor.cpp:1041`；
  - 等待遮挡查询（renderer.cpp 4351–4358）；
  - 上传环或描述符池用完（8515–8529）；
  - 新命令列表开头的 query reset 在 pass 外执行（`Begin` 4514）。

### 1.4 一帧的结构（D1，pass 编号）

| 编号 | 内容 |
|---|---|
| 0 | present，1.42 ms |
| 1–17 | 游戏通过 640 pitch 的表面清 EDRAM。随后对 base 0 的各个颜色视图做 A1 擦除（2560x1636、1760x1728、960x1067、640x640、480x512……），再做 A2。 |
| 5 | 主深度的别名清除，单操作 |
| 6 | Z-prepass，1090 个 draw，3.15 ms，以 transfer 的 barrier 结束 |
| 18 | transfer 到主颜色，0.35 ms |
| 19 | 主颜色+深度，620 个 draw，7.05 ms，以 resolve 的 barrier 结束 |
| 20 | 主深度 resolve blit，0.35 ms；之前有约 1 ms 的 pass 外 resolve 拷贝间隙 |
| 34–126、169–207 | 阴影：880x896 共 48 个 draw pass、171 个 draw，被大量 A2（480x512、880x896）隔开；3 次阴影 resolve（864x864 R32F） |
| 55–62、127–168、208–224 | 主视角逐光源交替：约 67 个单 draw pass，共约 7 ms |
| 225–231 | resolve、两个 transfer、主颜色+深度 359 个 draw（3.84 ms）、颜色 pass |
| 232–306 | 后处理链（960x1067、480x512、1760x1728）、A1/A3 清除、4 次 RGBA8 resolve（0.25–0.5 ms）、UI 和合成 |

主视角 77 个 pass 的去向：

| 改动 | 涉及的 pass | 去向 |
|---|---|---|
| 阶段 1 | 第 5 个 pass | 并入第 6 个，变成 loadOp=CLEAR |
| 阶段 2a | 约 67 个交替 pass | 合成 2–4 个 |
| 阶段 2b | 第 18、228 个 transfer pass | 并进第 19、229 个 |
| 不能合并 | resolve（19→20、230/231）和 Z-prepass/颜色的边界 | 保留 |

改完后主视角大约剩 8–10 个 pass。

### 1.5 断开原因分类（三份 trace）

| 类别 | 手机 T1 | 平板 D1 | 平板 D2 | 能否消除 |
|---|---|---|---|---|
| A1 深度填充擦颜色视图 | 54 | 54（2.02 ms） | 54（2.26 ms） | 阶段 1 |
| A2 深度别名清除 | 78 | 74（0.49 ms） | 74（0.70 ms） | 阶段 1 |
| A3 颜色清除矩形重放 | 17 | 17（0.18 ms） | 17（0.20 ms） | 阶段 1b |
| B 主视角（全屏） | 73 个交替 + 10 个其他 | 77（25.8 ms） | 77（29.6 ms） | 交替部分：阶段 2a，前提是阶段 0 证明有收益 |
| C 阴影/离屏深度 | 40 | 57（0.99 ms） | 55（1.79 ms） | 阶段 1 和 1b 之后自然合并 |
| E 小 RT | 31 | 11（2.73 ms） | 11 | 真实切换，不动 |
| F resolve blit | 8 | 8（2.22 ms） | 8（4.94 ms） | pass 内 resolve，阶段 3 |
| G transfer | 8 | 8（3.17 ms） | 8（4.26 ms） | 阶段 2b |
| H AO / 后处理 | 4 | 0 | 3（12.1 ms） | 已经是 MRT，不能合并，见 GTAO 说明 |
| I present | 1 | 1 | 1 | 阶段 2 用 DONT_CARE |
| pass 间隙（主要是 pass 外拷贝） | 总间隙 1.95 ms | 总间隙 4.02 ms（其中 resolve 拷贝后 3.44） | 总间隙 5.09 ms | 写时复制，阶段 3 |
| **合计** | 324 | **307（253 个单操作）** | 308 | |

说明：
- 协调者转述的"resolve copy 1.4 ms"其实是第 0 个 pass，即 present（站点 13774 是 present 路径里的 `Transition`）。
- "TAA resolve/motion" 其实是 GTAO。
- 本表的分类按站点、尺寸和格式在脚本里核对过：站点等于当前代码行号加插桩偏移 5 或 6。

---

## 2. 各类的消除方案和影响

### 2.1 清除延迟执行（阶段 1，核心）

**渲染器侧**

- **数据结构**：`HostTexture` 加一条 `PendingClear`，记录颜色值，或深度值加模板值再加两个 aspect 掩码，以及一个宿主像素矩形列表。
  - 清除只记录，不录 GPU 命令；
  - 新的整张清除覆盖旧记录；
  - 部分清除按顺序追加，但这样会出现"不同值的矩形叠加"，实现时可以限制为每张纹理只保留一条：已经有一条时先执行掉，再记新的。
- **改成延迟的位置**：
  - 12251–12252（A1）；
  - 12286–12304（A2）；
  - 13280–13296，resolve 附带的颜色清除；
  - 13301–13312，`ClearDepthTarget`。
  - 不改：12935–12936 新 resolve 目标的初始化清除。它只在分配时执行，推迟它不会减少每帧的 pass；而 `rs.tex` 还会经过多条直接调用 `commandList->barriers` 的路径（例如 12654、9978、13016），推迟只增加风险。
- **执行点**：所有接触这张纹理的路径都必须先执行掉待清除。建议收成三个入口：
  - `BindForDraw(tex)`：DrawImpl 在 10469 前、TransferRegion 和 BlitRegion 的目标、HDR sidecar、清除重放的目标。执行方式见下面的 plume 侧。
  - `SettleForRead(tex)`：`Transition` 到 SHADER_READ、COPY_SOURCE、DEPTH_READ 的所有地方，即 4908 的 `Transition` 本身、`GetTexture` 绑定、`TransferRegion` 源 2733、`BlitRegion` 源 3864、`ResolveOnGpu` 的拷贝和 blit、深度 resolve、`fetchViews` 拷贝 7771、FSR alpha 重放的深度读取 11133、AO、TAA、SR、FG 的捕获。
  - `SettleForWrite(tex)`：pass 外的写入，即 resolve 目标的 COPY_DEST、`GetRenderTarget` 变高时的拷贝 7163、`TransferRegion` 部分覆盖的目标。
  - 另外，读回和调试路径（`DumpTexture`、`DumpRenderTargets`、`ReadbackTexture`、`LO_CLEAR_RT`、调试捕获、`present_capture`）开始前统一执行 `SettleAll()`。
  - 纹理被替换时分两种情况：
    - 新纹理继承旧内容时，先在旧纹理上执行待清除，再拷贝。包括 `GetRenderTarget` 变高时的重叠拷贝（7150–7170，读 `oldTarget`），以及超分时的 scene-copy promotion（`PrepareSceneCopyDestination` / `ActivateSceneCopyDestination` 会在帧中途替换 `renderTargets[key]`）。现有代码在这些位置已经调用 `SettleRestoreDebt()`（7123、7207、7212），照同样的位置加上即可，不需要新机制。
    - 不继承内容时（`retiredTextures`、改分辨率计划），直接丢掉记录。
- **跨 pitch 的语义为什么等价**：这个模型里，draw 不会在不同 pitch 的别名之间传播，只有清除会。纹理 B 上的待清除只受 B 自己的后续访问影响；只要每次访问 B 之前都先执行，结果就和立即清除完全相同。
- **哪些记账必须留在原来的时间点**：
  - `hdrValid=false`、`sdrProducerFrame`、`aaProvenance.Invalidate`、`sdrLoss`、`HandleFsrAlphaRgbWriter`、`fsrAlphaBridge->RecordFullColorClear`、`DropRestoreDebt`（12247）、`FlushMotionReplayQueue`；
  - 原因：这些描述的是逻辑内容，必须在逻辑上的清除时刻更新，只有 GPU 命令推迟。

**plume 侧**（Vulkan，照 Eden 和 Dolphin 的做法，Metal 已经有）

- `VulkanFramebuffer` 按"哪些附件用 CLEAR"缓存几个 `VkRenderPass` 变体：1 个颜色加深度/模板，最多 8 种。原来的 `VkFramebuffer` 与这些变体兼容。
- `setFramebuffer` 之后、第一个 draw 之前，如果来了整张的 `clearColor/clearDepthStencil`，就记成开始时的清除；`checkActiveRenderPass` 用对应变体，并传 `pClearValues`。
- 部分清除，或者 pass 已经开始，照旧用 `vkCmdClearAttachments`。
- 深度和模板可以分别设置，比如 A2 只清深度，模板仍然 LOAD。
- 如果一个 pass 只有开始时的清除、没有 draw 就结束，可以录一个空的 CLEAR pass，也可以改成 pass 外的 `vkCmdClearColorImage` / `vkCmdClearDepthStencilImage`；后者在 Turnip 上走快速清除 LRZ 的路径。

**主深度的部分矩形**

- 主深度纹理是 2560x1636，游戏的清除矩形映射过来可能不覆盖超出客体高度的那 36 行。不覆盖时只能用 `vkCmdClearAttachments`，LRZ 的好处就没有了。
- 可选做法：如果没覆盖的区域完全落在 `ScaleY(guestHeight)` 以外，就扩成整张清除。这部分像素没有任何 draw 或 resolve 能读到。
- 风险：纹理变高时的拷贝（7150–7170）会把这些行带进新纹理。所以只能在"变高时同样以清除值处理"成立的前提下做。先做计数统计，看这种情况有多少，再决定。

**各平台的影响**（估计）

| 平台 | 影响 |
|---|---|
| Adreno/Turnip（sysmem） | 少 120–135 个 pass。A 类 2.7 ms（原生）里，不再需要执行的部分能全部省掉；仍需执行的那部分改成 loadOp 或 pass 内清除。纯工作量预计省 1.5–2.5 ms（约 4–6%）。LRZ 的增益另算，还没量化。loadOp=CLEAR 会抬高 autotune 眼里的 sysmem 代价，个别 pass 可能被改判成 GMEM，要实测。 |
| Apple | 大部分只做清除的 encoder 消失，每个都是一次整张写回。预计收益比 Adreno 大。 |
| Mali | 同 Apple，不过目前进不了游戏。 |
| 桌面 D3D12/Vulkan | 清除命令只少不多，每个 draw 多一次判断。不会退化，但要用同二进制开关在 RX 9070 XT 和 RTX 上验证 `draw_ms` 和 GPU ms。 |

### 2.2 游戏自己的清除矩形 draw 也延迟（阶段 1b）

- 阴影段和后处理段里，每次清除都先在 640/880/480 pitch 的表面上画一个单 draw 的 pass，再做传播。
- 满足以下条件时，可以不画这个 draw，改成在本目标和所有别名上都记待清除：
  - 纯深度，`!colorWrites`；
  - zfunc 是 ALWAYS 并写深度（12199）；
  - 矩形覆盖整个目标；
  - 模板写入关闭，或者是 REPLACE 加全掩码，可以换算成模板清除值；
  - 没有活跃的客体遮挡查询（`occlusion.DrawSeen()`）；
  - 没有 polygon offset 或 PARA 偏移；
  - 各顶点 z 相同。
- 收益：
  - 阴影段大约从 57 个 pass 降到 3–6 个，每次阴影 resolve 一个 pass；
  - 主深度的 `Depth write + ALWAYS` 这类 LRZ 关闭会少掉一部分。
- A3（颜色清除矩形重放，17 个）是 draw，颜色值来自游戏的 PS，不能换算成清除值。可以参照运动矢量重放的 `QueuedDraw` 做法，把管线、常量偏移、描述符集排队，在别名下次被绑定时画到它的 pass 里。
- 逐字节一致的前提：光栅化出来的 z 要与渲染器算出的 `rectZ` 按位相同。A2 现在已经默认这一点成立，但仍要用夹具在每个后端上验证；条件不满足就照旧画 draw。

### 2.3 主视角纯深度和颜色交替（阶段 2a，由实验决定）

- 现状：约 67 个单 draw pass，是 stencil 标记（纯深度，`(nullptr, depth)`）和光照（`(color, depth)`）的交替，见 D1 第 55–62、127–168、208–224 个 pass。
- 现成手段：`LO_DEPTH_ONLY_KEEP_COLOR=1` 给纯深度 draw 保留颜色附件（写掩码 0），交替就能合成 2–4 个 pass。Apple 上已经默认开启，+15%。
- 手机 ABBA 实验里它慢了 9%，场景 pass 的耗时（按未受影响的 pass 归一）多了 29%。两种假设：
  1. 合并后 pass 变大，autotune 估出的 sysmem 代价超过了 LOAD 加 STORE 的 GMEM 代价，于是改用 GMEM；而这一帧几何很重，binning 更慢；
  2. sysmem 下纯深度 framebuffer 有更快的路径，例如 CCU 分配或 LRZ。

  区分方法（阶段 0）：在平板热态下，用 `KEEP_COLOR=1` 分别配 `TU_DEBUG=sysmem` 和 `TU_DEBUG=gmem` 跑 ABBA，每个 pass 记 GMEM/sysmem 的选择。如果打的 Turnip 有 `TU_AUTOTUNE_ALGO`，也一并对比。Mesa 2026 版加了 `prefer_sysmem`/`prefer_gmem`/`profiled`，Turnip 26.0.0 R8 是否带有还没确认。
- **约束**：不能把 KEEP_COLOR 原样再在 Adreno 上开一次。只有在阶段 1 改好 load op，并且实验证明有收益之后，才和 GMEM 的设置一起开。
- 逐字节一致：同一个 shader、同样的状态，颜色掩码为 0，Apple 已经在用。

### 2.4 transfer 画进目标自己的 pass（阶段 2b）

- `TransferRegion` 现在绑的是 `(dst, nullptr)`。改为：
  - 当紧接着的游戏 draw 要用 `(dst, depth)` 时，transfer 用一条"带深度格式、关闭深度测试和写入"的管线变体，直接画在 `(dst, depth)` 上；
  - 源纹理的 barrier 本来就在 `setFramebuffer` 之前。
- 每帧少 8 个 pass，其中主视角 2 个。shader 和像素都不变。
- 对 tiler 来说，转换就在 tile 里进行，省掉一次整张写回再读入。
- 另一个可能更大的收益（待计数确认）：
  - D1 里 transfer 共 3.17 ms，比整个 A 类还多；
  - D1 中 2560x1636 的 A1 整张擦除（第 9/12/16、25/28/32 个 pass）后面紧跟着往 2560x1636 的 transfer（第 18、22、54 个 pass）。
  - 如果 transfer 的源在本帧里只被整张清除过、之后没有 draw，那么阶段 1 之后这个源只带着一条整张待清除，transfer 读到的是一个常量。这时可以在 CPU 上算出 `UnpackGuest(PackGuest(v, 源类别), 目标类别)`，把 transfer 本身也变成目标上的待清除，省掉执行和 0.35–0.85 ms 的 transfer draw。
  - trace 里看不出类别对应关系，所以阶段 1 先加计数："transfer 的源在本帧内是否只被整张清除过"。收益上限按实测的 transfer 时间估计。

### 2.5 resolve

- **pass 外的同格式拷贝**：D1 中 9 次，间隙 3.44 ms，约占 8%。
  - 写时复制的思路：resolve 时让 `ResolvedSurface` 直接引用 EDRAM 纹理；EDRAM 纹理下次被写之前再真正拷贝。
  - base 0 一帧里会被反复写，能省下多少不确定。先统计"源在 resolve 结果被消费完之前没有再被写"的比例，再决定做不做（阶段 3）。
- **FP16→RGBA8 和深度→R32F 的 blit**：要读完整的源，只能在源 pass 结束后做。真正在 tile 内完成的 resolve，需要在 pass 开始时就把 resolve 目标作为第二个颜色附件绑上，再配合 `VK_KHR_dynamic_rendering_local_read` 或第二个 subpass 在 tile 内读源。
  - 问题一：需要预知后面会有哪次 resolve，只能按上一帧的结构来预测，预测错了退回现在的做法。
  - 问题二：subpass 数不同的 render pass 互不兼容，会让管线数量翻倍。
  - 结论：放到阶段 3。
- **resolve 附带的清除**：360 上的标准流程是"渲染 → resolve 并清除"。阶段 1 让这次清除变成下一个 pass 的 loadOp=CLEAR，在 GMEM 模式下等于省掉一次整张载入。

### 2.6 load/store op（阶段 2；storeOp 放到阶段 3）

- **loadOp=DONT_CARE**：渲染器自己画、全屏覆盖、不混合的 pass（AO 两个 pass、present 无黑边时、FXAA 中间结果、覆盖整张目标的 `BlitRegion`、全量拷贝前先清除的 HDR sidecar 10827）。
  - sysmem 下没有区别；GMEM 下省掉载入，也会降低 autotune 眼里的 GMEM 代价。
  - plume 现有的 `discardTexture` 在 Vulkan 上是空实现，可以改成"下一个 pass 对这个附件用 DONT_CARE"。
  - 前提：渲染区域内的每个像素都会被写到，即 scissor 必须是整张。
- **storeOp=DONT_CARE 和 transient 附件**：EDRAM 内容跨 pass 甚至跨帧都要保留，只有确定"下一次访问是整张清除"时才能丢弃。但 storeOp 必须在 pass 开始时就定下来，这就需要帧级延迟录制（阶段 3）。
- **AO 中间纹理**：只给合成 pass 读，可以设成 transient。但合成 pass 读它时会采样邻近像素，不能改成 subpass 输入附件。

### 2.7 barrier 和 pass 边界的同步（阶段 0）

- `LO_VK_NO_END_DEP=1` 已经实现，一直没测过。D1 中主视角交替 pass 之间的间隙只有约 1 µs，所以预期收益很小。阶段 0 做一次 A/B，并跑一遍 VVL 同步验证就够了。
- barrier 的访问掩码是 `MEMORY_*`，阶段集合也很宽。收窄掩码只影响缓存刷新，不影响 pass 数，优先级低。
- 把相邻 resolve 的 barrier 合成一次，最多省几个 pass 断点。旧笔记里"同目标在 barrier 后重开"只占 2.7%，不值得单独做。

### 2.8 后处理链和 AO 能不能合并

- 游戏的后处理每一步都换目标，还经常换分辨率（960x1067、480x512、1760x1728），而且要采样邻近像素。按 tile 处理的 subpass 只适用于"同一像素读写"，这里用不上。
- AO 的合成在 PR #302 时已经并成一个 MRT pass。
- 这一块不合并。

### 2.9 Vulkan 机制的取舍

| 机制 | 取舍 |
|---|---|
| 继续用 `VkRenderPass`，加 load-op 变体（推荐，阶段 1–2） | Vulkan 1.2 就够，管线不用改，Turnip 和高通官方驱动都支持。 |
| dynamic rendering（1.3，或 `VK_KHR_dynamic_rendering`） | 本身不会减少 pass，只是不再需要 render pass 对象，load/store 在 begin 时指定。配合 `dynamic_rendering_local_read` 才能做 tile 内读取。只在阶段 3 要做 pass 内 resolve 或 tile 内后处理时再考虑。 |
| 多 subpass | 管线兼容性要求 subpass 结构相同，会让管线数翻倍。不推荐。 |
| secondary command buffer 延迟录制 | 可以等 pass 录完再决定 load/store，但只能决定 load，因为 store 要知道以后的访问。会增加 CPU 开销，桌面不能用，属于阶段 3。 |

### 2.10 Metal

- 阶段 1 会直接减少 encoder：只做清除的 encoder 消失，或者并进下一个 encoder 的 `LoadActionClear`。
- KEEP_COLOR 在 Apple 上已经默认开启。
- 之后可以加：确定不再读取的深度用 `StoreActionDontCare`；中间纹理用 memoryless（阶段 3，前提同样是知道以后的访问）。
- 需要在 M1 Max 上通过 SSH 验证 encoder 数量和 FPS。

---

## 3. 风险

1. **漏掉执行点，读到清除前的 EDRAM 内容**（最大风险）。
   - 对策：所有访问收进 §2.1 的三个入口。
   - debug 模式 `LO_EDRAM_LAZY_CLEAR=poison`：执行待清除时用洋红色代替真实值，漏掉的路径会在画面上显形。思路同 `LO_CLEAR_RT=magenta`。
   - 保留同二进制开关 `=0`，回到立即清除。
   - transfer（`tileOwner`）既读旧类别的纹理，也写新类别的纹理，两边都要先执行。
2. **记账时间点**：见 §2.1 末尾，那些标记在逻辑清除时刻更新，不能跟着 GPU 命令推迟。`FlushMotionReplayQueue` 仍在逻辑清除点调用，保证排队的运动矢量重放先于深度清除录入。
3. **时域效果**：TAA、SR、运动矢量重放、FSR alpha 只读 resolve 结果和深度 resolve，以及在 draw 时读场景深度。清除的值和顺序都不变，所以输入不变。要确认的是：这些路径读深度时，都经过 `SettleForRead` 或 `Transition` 那个入口。
4. **HDR sidecar**：sidecar 不是 EDRAM 别名。它的 `hdrValid` 在清除时就失效，这个逻辑不能动。sidecar 的 DONT_CARE 只在随后的拷贝覆盖整张时才成立。
5. **截图和调试工具**：`DumpRenderTargets`、`DumpTexture`、调试捕获、`taa_debug` 读回、`present_capture` 在读回前都要先 `SettleAll()`。否则调试时看到的是还没清除的旧内容，会误导排查。
6. **哪些地方能证明逐字节一致，哪些不能**：

   | 改动 | 能否逐字节一致 |
   |---|---|
   | 阶段 1 | 能。`vkCmdClearAttachments` 和 loadOp=CLEAR 按同样的规则把清除值转换成附件格式。 |
   | 阶段 1b | 前提是光栅化的 z 与 `rectZ` 按位相同。要用夹具在每个后端上证明，不成立就退回画 draw。 |
   | 阶段 2a/2b | shader 和状态都不变。 |
   | GMEM 与 sysmem 之间 | 驱动不保证混合结果一致。要在 `TU_DEBUG=gmem` 和 `TU_DEBUG=sysmem` 下各截一张图对比。 |
   | 整个游戏 | 游戏每次运行的帧不完全相同，所以全游戏的一致性靠"构造上等价"加夹具来保证，不靠抽样截图证明。 |
7. **驱动问题**：
   - ANGLE 曾记录高通官方驱动在"loadOp=Clear 后接内联命令"时出错（2020，据说新驱动已修，只读到搜索摘要）；
   - Dolphin 对 ImgTec 有 `BUG_BROKEN_CLEAR_LOADOP_RENDERPASS`。

   对策：loadOp=CLEAR 留一个按驱动关闭的开关。默认的 Turnip 不受影响，但用户切到官方驱动时，要在 Adreno 750/840 上验证。
8. **autotune 翻转和 LRZ 快速清除的限制**：
   - loadOp=CLEAR 改变了 autotune 的代价估计，可能把个别 pass 改判成 GMEM；
   - LRZ 快速清除只对 0.0 和 1.0 有效。

   每个阶段都要在平板热态下分 pass 实测。
9. **遮挡查询**：查询必须在它所计数的 draw 所在的 pass 里。开始时的清除发生在查询开始之前，不受影响。阶段 1b 在有活跃查询时不做转换。
10. **桌面 CPU**：每个 draw 只多一次判断，清除记录只在清除事件时产生。Uhra 720p 是 CPU 受限的场景，需要用 `draw_ms` 确认没有退化。

---

## 4. 分阶段方案

### 阶段 0：测量与诊断（1.5–2 天，第一步做）

- 把 `apply_diag_v100.py` 的分 pass 计时整理成常驻的 debug 开关，只在开关打开时生效，不进发布路径：
  - 每个 pass 记录起点类别和结束原因；
  - 加上单操作 pass 的计数；
  - 给 `ambient_occlusion.cpp` 和 `presentation.cpp` 也加上站点标记，免得再出现"TAA resolve"这种错标；
  - Metal 上记录 encoder 数。
- 平板热态（沿用 `hot.sh` 的 NOGATE 热浸和前后括号），依次跑：
  1. `LO_VK_NO_END_DEP=0/1` 的 ABBA；
  2. `LO_DEPTH_ONLY_KEEP_COLOR=0/1` 分别配 `TU_DEBUG=sysmem` 和 `gmem`；
  3. `TU_DEBUG=perf` 统计每帧各类 LRZ 关闭原因的次数，作为后续阶段的基线；
  4. 如果 Turnip 支持，加上 `TU_AUTOTUNE_ALGO=prefer_sysmem/prefer_gmem`。
- 产出：知道 KEEP_COLOR 慢 9% 的原因，知道主视角在 GMEM 下有没有可能赢。这决定阶段 2a 做不做，以及阶段 3 值不值得投入。

### 阶段 1：清除延迟执行加 loadOp=CLEAR（5–7 天，推荐的第一阶段）

- **改动范围**：
  - renderer.cpp：A1、A2、resolve 附带的清除、`ClearDepthTarget`，加上三个执行入口；
  - 计数：transfer 的源在本帧内是否只被整张清除过（为 §2.4 的常量 transfer 做准备）；主深度的清除矩形是否覆盖整张纹理；
  - plume-lostodyssey.patch：Vulkan 的 load-op 变体，以及开始时的清除；
  - Metal 不用改。
- **预期**（估计）：
  - D1 从 307 个 pass 降到约 175–185 个，A 类 145 个基本消失，阴影段开始合并；
  - 原生分辨率下 GPU 少 1.5–2.5 ms，LRZ 的增益另计；
  - 720p 下 GPU 少约 1 ms，FPS 受渲染线程限制，基本不变；
  - Apple 的收益要另外测。
- **验证**：
  - CPU 单元测试：待清除的状态机，输入操作序列，检查执行点和最后录出的命令；
  - GPU 夹具，仿照 `LoDepthClearGpuTest`：同一份"清除、draw、resolve、transfer、读回"脚本，分别用立即清除和延迟清除跑，在 D3D12 和 Vulkan（本机）、Metal（Mac）上读回全部 resolve 目标，逐字节比对；
  - 游戏内同二进制开关：静态画面（标题、暂停菜单、Scenario Jump 的首帧）A/B，地图巡游和过场巡游抽样比对，出现差异逐个查；
  - 平板热态分 pass 计时，原生、1080、720 三档；`TU_DEBUG=perf` 中 CmdClearAttachments 的次数应明显下降；
  - 桌面 RX 9070 XT（D3D12 和 Vulkan）、RTX，加上 M1 Max：GPU ms 和 `draw_ms` 不能变差；
  - VVL 同步验证（psvita 原生 VVL 环境）。

### 阶段 1b：清除矩形 draw 也延迟（3–4 天）

- 内容见 §2.2。另外加一个夹具：比较光栅化写出的 z 和 `rectZ`，按位一致才开启转换。
- 预期少 30–45 个 pass，阴影段每次阴影 resolve 只剩一个 pass（估计）。

### 阶段 2：transfer 进 pass、主视角交替合并、DONT_CARE（3–5 天，另加测量）

- 2b transfer 进 pass：少 8 个。
- 2a 交替合并：少 60–65 个。只在阶段 0 证明有收益时开，可能只在 Apple 加上 Turnip 开、官方驱动不开。
- 渲染器自有的全屏 pass 用 DONT_CARE。
- 预期总共少 70–75 个 pass（估计）。Adreno 上 sysmem 部分的直接收益接近 0，真正的意义在于让 GMEM 和 LRZ 有机会起作用。
- 验证同阶段 1；KEEP_COLOR 必须做平板 ABBA。

### 阶段 3（建议放到 1.0 之后，或由数据决定）

- 帧级延迟录制，决定 storeOp 和 transient：只在 tiler 上启用，桌面不走这条路。
- resolve 拷贝的写时复制，先做统计。
- pass 内 resolve，用 `dynamic_rendering_local_read`，按上一帧的结构预测。
- 针对 Turnip 调 GMEM 的设置。
- 工作量 10 天以上，风险高。

---

## 5. 可借鉴的做法（只列有来源的）

- **Eden（yuzu 后继）**：
  - `vk_scheduler.cpp` 有 `DeferColorClear`、`DeferDepthStencilClear`、`RealizeDeferredClear`；
  - `vk_texture_cache.cpp` 按 `Framebuffer::RenderPassVariant` 选 CLEAR 变体，`vk_render_pass_cache.cpp` 把这些映射成 `LOAD_OP_CLEAR`；
  - 延迟的条件（`vk_rasterizer.cpp`）：覆盖整个渲染区域、当前没有 pass、第 0 层、所有通道都写。与本方案的条件相同。
  - DONT_CARE 只用于 MSAA 丢弃，也没有 subpass 合并。
  - <https://git.eden-emu.dev/eden-emu/eden/src/branch/master/src/video_core/renderer_vulkan>
- **Dolphin**：
  - `VKGfx::ClearRegion` 分三条路径：pass 外的整张清除用 `BeginClearRenderPass`（loadOp=CLEAR）；pass 内用 `vkCmdClearAttachments`；其余画四边形。
  - `DriverDetails.cpp` 对 ImgTec 标了 `BUG_BROKEN_CLEAR_LOADOP_RENDERPASS`。
  - EFB 拷贝也会先结束 pass，和这里的 resolve 一样。
  - <https://github.com/dolphin-emu/dolphin/tree/master/Source/Core/VideoBackends/Vulkan>
- **Xenia**：
  - `VulkanRenderTargetCache::PerformTransfersAndResolveClears` 为 transfer 单独进一个 render pass，draw 进 `last_update_render_pass()`；pass 或 framebuffer 不同就结束。
  - resolve 附带的清除在 transfer pass 里用 `vkCmdClearAttachments`；附件全是 LOAD/STORE。
  - 结构和本项目现在一样，不能当作改进的样板。它的 FSI/ROV 路径把 EDRAM 放在 buffer 里，没有 pass 断开，但选项说明里写着它"慢很多"。
  - <https://github.com/xenia-project/xenia/tree/master/src/xenia/gpu>
- **Mesa Turnip**（AOSP 镜像，2025-03 快照，约 Mesa 25.0/25.1；2026 年的变化来自 AlmaLinux 的 revert 补丁，没能读到当前 upstream main）：
  - `tu_autotune.cc` 和 `tu_pass.cc`：带宽代价模型，`fallback_use_bypass` 在 draw ≤5 时用 sysmem；
  - `tu_cmd_buffer.cc` 的 `use_sysmem_rendering`：强制 sysmem 的条件，包括 pass 内非 by-region 的 barrier 会设 `disable_gmem`；
  - `tu_lrz.cc`：`vkCmdClearAttachments` 清深度关闭 LRZ；loadOp=CLEAR 保持 LRZ 有效；A650+ LOAD 时沿用之前的状态；
  - 2026 年的 revert 补丁显示 upstream 加了 `TU_AUTOTUNE_ALGO`（`prefer_sysmem`/`prefer_gmem`/`profiled`）和 `TU_AUTOTUNE_FLAGS`；
  - "stencil write based on depth test" 在 2025 快照里找不到，是更新的规则，未核实。
  - <https://android.googlesource.com/platform/external/mesa3d/+/refs/heads/main/src/freedreno/vulkan/>
- **厂商建议**：
  - Arm 的 load/store 博客：CLEAR 加 DONT_CARE，读少 36%、写少 62%。<https://developer.arm.com/community/arm-community-blogs/b/mobile-graphics-and-gaming-blog/posts/picking-the-most-efficient-load-store-operations>
  - Khronos 的 render pass 示例：用 loadOp=CLEAR 代替 `vkCmdClear*`；只在一个 pass 里用的附件设成 transient。<https://docs.vulkan.org/samples/latest/samples/performance/render_passes/README.html>
  - Imagination：优先 DONT_CARE 或 CLEAR，store 不用就 DONT_CARE。<https://docs.imgtec.com/performance-guides/graphics-recommendations/html/topics/using-render-passes-and-sub-passes.html>
  - Qualcomm "Avoid GMEM loads"：<https://docs.qualcomm.com/bundle/publicresource/80-71528-1/topics/avoid_gmem_loads.md>。Qualcomm 的 Tiling 页和 Apple 的 TBDR 页都没能直接打开，这里不引用其中内容。
- **没有找到可靠来源的**：Skyline（源码已因下架删除）、Ryujinx 和 Ryubing（仓库已不可访问），以及 Triang3l 关于 tiler 的说法。

---

## 6. 附录：数据与推导

- **平板 D1/D2**：`apk-v100\out\perf\D1-trace.logcat`、`D2-trace.logcat`，trace 帧号 1292。
  - 分类按站点计算：站点等于当前代码行号，第 4908 行之前加 5，之后加 6。
  - 对应关系：10476 是 DrawImpl 的 `setFramebuffer`；12257/12292/12332 是 A1/A2/A3；2732 是 `TransferRegion`；3873 是 `BlitRegion`；9699 是 AO（无站点，沿用上一个）；10017 是 AO 之后的游戏 draw；13774 是 present。
  - 主视角按 2560 宽、高度 1600–1699 判断。D2 的阴影是 3456x3456，不能算成主视角。
- **平板帧时间**（热态）：
  - 默认设置 23 FPS，GPU 约 41.5 ms；高设置 17 FPS，GPU 约 56 ms；
  - 分辨率扫描：原生 GPU 42.6 ms，1080 是 27.0 ms，720 是 17.1 ms（`draw_ms` 20.2）；
  - 粗略拟合：GPU ≈ 7–8 ms 固定部分 + 每百万像素约 8.5 ms。
- **手机 T1**：`apk-289\out\perf\T1-trace.runtime.log`。站点映射用 `renderer.cpp.before-diag` 核对，偏移是 27 和 6。
- **旧结论（[OPTIMIZATION_PLAN.md](../archive/OPTIMIZATION_PLAN.md)）**：2026-09-29 在 M1 Pro 上测到，pass 结束的原因里颜色目标切换占 39%，barrier 22%，深度目标切换 21%。当时判断合并空间小。那次没有区分别名清除传播；本次按站点分类后，它占了将近一半的 pass。
- **待确认**：
  - Turnip 26.0.0 R8 是否带有 `TU_AUTOTUNE_ALGO`；
  - KEEP_COLOR 慢 9% 的原因；
  - 主深度的清除矩形能否覆盖整张纹理（要做计数）；
  - resolve 拷贝的源有多少比例在之后不被写。
