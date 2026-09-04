# GPU 笔记

## 现状

`LostOdysseyRecomp/gpu/command_processor.cpp` 是一个最小的 Xenos 命令处理器（参照 Xenia `gpu/command_processor.cc`）：
消费主环形缓冲和间接缓冲，执行 CPU 会同步等待的包，跳过所有绘制/状态/着色器包。没有任何画面输出。

MMIO：重编译代码里 `eieio` 前缀的存储被 XenonRecomp 生成为 `PPC_MM_STORE_*`，
`gpu/ppc_mmio.h` 通过 `/FI` 强制包含到重编译库里，把它们转到 `LoMmioStore32`，
其中 CP_RB_WPTR（reg 0x01C5，地址 0x7FC80714）触发命令处理。
读取是普通 load，所以寄存器窗口内存里始终保持大端镜像；几个只读状态寄存器（0x1951 中断状态=1、
0x194C、0x0F00、0x0F01、0x1961）忽略写入，否则 D3D 的 ISR 应答后再也看不到 vblank。

## D3D 每帧协议（从包追踪得到）

```
IB:  REG_RMW, INVALIDATE_STATE, EVENT_WRITE_SHD(0xA004 <- fence), EVENT_WRITE_SHD(0xA000 <- frame)
ring: INDIRECT_BUFFER  →  IB: WAIT_REG_MEM reg COHER_STATUS_HOST & 0x80000000 == 0
... 绘制 ...
IB:  type0 SCRATCH_REG1 = 1（present interval）
IB:  WAIT mem[0xB004] == 1 ; WAIT mem[0xB000] == 4    ← CPU 的 Present 事先写好同步块
IB:  INTERRUPT cpu_mask=4                            ← ISR(source=1, cpu=2) 调回调 0x827B4780，清 [0] 与 [1]
IB:  WAIT mem[0xB000] == 0 ; WAIT mem[0xB004] == 0    ← GPU 等 ISR 处理完
IB:  ... XE_SWAP
```

同步块是 MmAllocatePhysicalMemoryEx(0x20) 分配的 0xA000B000：[0]=CPU 掩码 [1]=swap 挂起 [4]=回调 [5]=参数。
ISR 是 `sub_827B6C48(source, device)`：source 1 时若 block[4]==0x0BADF00D 就报 "Unanticipated CPU_INTERRUPT" 并 trap；
source 0 时检查 MMIO 0x1951 的 bit0 再调 vblank 处理 `sub_827B4680`（维护 pending swap 队列，
到期时写 D1GRPH_PRIMARY_SURFACE_ADDRESS 0x1844）。

中断派发：vblank 在独立客体线程（CPU 2，60 Hz）直接调 ISR；PM4_INTERRUPT 按掩码逐 CPU 入队到另一个线程，
调用前把 PCR+0x10C 写成对应 CPU 号，因为 ISR 会清掩码里"自己"的位。两者不能共用一个线程，否则回调里等 vblank 会自锁。

## 已修复的坑

- **环形缓冲大小**：`VdInitializeRingBuffer(ptr, log2)` 的 log2 以 8 字节为单位，真实大小是 `1 << (log2 + 3)`
  （Xenia `InitializeRingBuffer`）。按 `1 << log2` 处理时 D3D 的 WPTR 会跑到 0x400 以上，
  我们在 0x1000 处提前回绕，把第一圈的旧包（含早已被顶点数据覆盖的间接缓冲）重放一遍，
  表现为"WAIT_REG_MEM 操作数是浮点数"以及 CPU/GPU 互等死锁。第 167 帧必现。
- 定位手段：`LO_WATCH_PHYS=<物理地址> LO_WATCH_LEN=<字节数>` 在该页设 PAGE_GUARD + 单步，
  打印每次访问的宿主符号、写入值和客体回溯（`os/watchpoint.cpp`）；`LO_TRACE_IB=1` 打印每个环形缓冲
  间接包和 WPTR 更新。
- 命令处理器对 WAIT_REG_MEM 的非法操作数仍容错（重读 200 ms 后跳过），正常情况下不应触发。

## 标题流程的绘制统计（LO_GPU_STATS=1，run43）

- 第 1–1140 帧（logo/加载）：每帧 1 个深度清屏（prim 8 rect，mode 5）+ 2 个 QuadList 四边形
  （auto-index 4 顶点，vs `ee5b1e880e971ce1` 30 dword / ps `31386e31cf9a84ed` 15 dword，mode 4）
  + 1 次 resolve（mode 6，rect 3 顶点，RB_COPY_DEST_BASE=0x70F000 即前缓冲，1280x720，
  RB_COPY_DEST_INFO=0x1000300 → 8_8_8_8）。顶点在 fetch[0]（物理 0x691C18，8in32），fetch[31] 后半是第二路顶点流。
- 第 1200 帧起进入标题场景：每帧 80–130 个绘制、50–80 次着色器加载、8–11 次 resolve；
  大量 prim 1 单点绘制（16x16 剪裁，写 EDRAM base 0）、prim 4 三角形列表（索引 DMA）；
  resolve 目标包括 0x9F90000 / 0x9BF0000 等纹理内存（copyCtl 0x4 = 深度拷贝）。
- 120 秒内共 25 个不同着色器，最大几十个 dword——标题画面着色器很简单。
- 微码转储在 `LostOdysseyRecompLib/private/shaders/{vs,ps}_<fnv1a64>.bin`（大端原始微码，无 D3D9 容器）。

## 渲染路线（已定：B，Xenos 模拟 + plume）

理由：绘制流已经完整经过命令处理器；D3D 库是静态链接的，路线 A 需要先在 Ghidra 里恢复上百个 D3D 入口。
分步：① SDL 窗口 + plume D3D12 设备/交换链，XE_SWAP 时呈现；② EDRAM 渲染目标缓存（按 base/format/pitch）
与 mode 6 resolve → 宿主纹理并回写客体内存；③ 绘制：fetch 常量→顶点缓冲、微码→HLSL（改造 XenosRecomp
的翻译器接受原始微码，顶点输入由 vfetch 指令推导）、DXC 运行期编译并缓存；④ 纹理 fetch 常量→解 tiling/字节序。


## 着色器翻译器（gpu/shader/，已完成并验证）

`xenos_translator.cpp`：原始微码 → HLSL（SM 6.0），`dxc_compiler.cpp` 运行期通过 dxcompiler.dll 编译。
`LoShaderTool <dir|bin> [--print] [--out dir]`（`tools/build_target.bat LoShaderTool`）离线验证：25 个标题流程着色器全部通过。
绑定约定（与绘制后端共享）：b0 = `float4 c[256]`（VS 用 0x4000 起、PS 用 0x4400 起的 ALU 常量），
b1 = XeShared（bool/loop 常量、NDC 缩放偏移、VTE 标志、alpha test），t0-95/space0 = 顶点 fetch 槽的
ByteAddressBuffer（CPU 先按 fetch 常量的 endian 做 32/16 位交换），t/space1..3 = 2D/3D/Cube 纹理，s0-31 采样器。
插值器固定 16 个 TEXCOORD；PS 的 r0..r15 直接由 i0..i15 初始化。VS 末尾按 PA_CL_VTE_CNTL 位 8-10 还原
（xy 已除 w、z 已除 w、w 为 1/w），再乘 NDC 缩放/偏移（对应 Xenia 的 CompleteVertexOrDomainShader）。
D3D9 顶点流 0 对应 fetch 槽 95（6 dword 组 31 的后两个 dword）。

## 绘制后端设计（下一步，gpu/renderer.cpp，尚未开始）

1. EDRAM 渲染目标缓存：键 (RB_COLOR_INFO.color_base 12 位 tile, color_format 4 位 @16, RB_SURFACE_INFO.surface_pitch 14 位, msaa @16)；
   深度键 (RB_DEPTH_INFO.depth_base, depth_format @16)。高度取剪裁 PA_SC_WINDOW_SCISSOR_BR（br_x:14 @0, br_y:14 @16）。
2. 管线缓存键：vs/ps 哈希 + RB_BLENDCONTROL0（src:5 @0, op:3 @5, dst:5 @8, alpha src @16, op @21, dst @24）
   + RB_COLOR_MASK（每 RT 4 位）+ RB_DEPTHCONTROL（stencil @0, z @1, zwrite @2, zfunc:3 @4）
   + PA_SU_SC_MODE_CNTL（cull_front @0, cull_back @1, face @2）+ 图元类型 + RT 格式。
3. 图元：RectList(8) 3 顶点→2 三角（v3 = v0 + v2 - v1）；QuadList(13)→索引三角；TriangleFan 转换；PointList 先按点画。
   索引：VGT_DMA_BASE + VGT_DMA_SIZE（num_words:24, swap_mode:2 @30），index_size 由 initiator bit 11。
4. 顶点流：fetch 常量 type 3：dword0 address:30（<<2 = 字节）, dword1 endian:2 + size:24（dword）；整块拷进上传环并交换字节序。
5. 纹理 fetch 常量（6 dword）：d0 type:2 sign:8 clamp:9 pitch:9@22(×32 texel) tiled@31；d1 format:6 endian:2 base_address:20@12(<<12)；
   d2 2D width:13/height:13（+1）；d3 num_format@0 swizzle:12@1 exp_adjust:6@13 滤波；d5 dimension:2@9 mip_address:20@12。
   格式枚举见 Xenia xenos.h TextureFormat（k_8_8_8_8=6, DXT1=18, DXT2_3=19, DXT4_5=20, k_5_6_5=4, k_8=2, k_4_4_4_4=15）。
   解 tiling 用 `video::TiledOffset2D`（压缩格式以 4x4 块为单位）。
6. Resolve（RB_MODECONTROL edram_mode 6 的绘制）：RB_COPY_CONTROL（copy_src_select:3 @0，4=深度；color_clear @8, depth_clear @9,
   copy_command:2 @20），RB_COPY_DEST_BASE 物理地址，RB_COPY_DEST_PITCH（pitch:14 @0, height:14 @16），
   RB_COPY_DEST_INFO（endian:3 @0, format:6 @7, swap @24）。做法：RT → 回读缓冲 → CPU tile 化写回客体内存；
   写到前缓冲后现有呈现层自然显示，这就是"第一帧"的验证路径。
7. Xenos 清屏就是深度模式/颜色模式的矩形绘制，无需特殊处理；alpha test 在 PS 末尾按 RB_COLORCONTROL（func:3 @0, enable @3）。

## 绘制后端现状（gpu/renderer.cpp，2026-09-03）

已实现：EDRAM 颜色/深度目标 → 宿主纹理（键 base/format/pitch/高度推断）；管线与着色器缓存（翻译器 + DXC，
`LO_SHADER_CACHE_DIR` 落盘）；顶点流按 fetch 常量整块拷入上传环并做字节序交换，着色器通过共享常量里的每槽偏移
读取同一个 ByteAddressBuffer；索引缓冲 16/32 位；QuadList/TriangleFan 转索引、RectList 用几何着色器补第四点；
纹理 8_8_8_8 / DXT1 / DXT3 / DXT5 / 5_6_5 / 4_4_4_4 / 1_5_5_5 / 8 / 8_8 / 16F / 32F 解 tiling 上传；采样器调色板
（64 个，D3D12 采样器堆上限 2048 所以不能每绘制 32 个）；resolve 回读到客体内存（目标 8_8_8_8 与 16_16_16_16_FLOAT，
源支持 RGBA8/RGBA16F/RG16F/R32F/RG32F），支持 copy 后清屏。标题画面 "Press START" 正确显示。

已知问题 / 待做：
- （已改，2026-09-04）resolve 留在 GPU 上：`ResolveOnGpu` 把渲染目标矩形拷进一张按目标地址键入的宿主纹理
  （`resolved[destBase]`，尺寸 destPitch × destHeight，矩形放在窗口坐标处），不再同步等待 GPU、不再 CPU tile 化、
  不再写客体内存。`GetTexture` 先查该表（格式兼容规则 `ResolveFormatMatches`：6↔14/50/62，7↔54），前缓冲由
  `video::PresentFrontbuffer` 通过 `renderer::AcquireResolvedSurface` 直接拷进交换链；截图走 `ReadbackResolvedSurface`。
  `LO_RESOLVE_READBACK=1` 回到旧的 CPU 回写路径，`LO_PRESENT_CPU=1` 强制旧的前缓冲 untile 路径。
- （已改，2026-09-04）顶点缓冲不再每个绘制整段拷贝：fetch 常量描述的缓冲可能有几 MB 而绘制只用几百个顶点，战斗场景
  550 个绘制曾要 12 s/帧。现在按（地址、大小、字节序）键入 256 MB 常驻 upload 缓冲（`vertexArena`），首次上传并字节交换，
  之后用抽样哈希（头尾各 512 B + 64 个 64 B 窗口）检测变化；arena 满了就 Flush 后清空重来。set0 的 vfetch 槽全部指向 arena。
- （已改）plume 的 shader-visible 描述符堆只有 65536 项，每个绘制 3 个 32 槽的集合；池上限 500 个/种，超过就在绘制开始前
  Flush 拆帧。常量块的上传挪到顶点/贴图之后，避免中途 Flush 让已上传的偏移失效。
- `LO_GPU_STATS=1` 现在每 60 帧（或帧耗时 >150 ms 时）打印分阶段耗时：绘制、着色器编译、管线创建、贴图上传、顶点上传、resolve、GPU 等待。
- 待办：战斗场景 400–600 个绘制仍要 200–450 ms（每绘制约 0.5 ms 的 CPU 开销），需要进一步定位；每帧还有 100+ 个
  "新"顶点缓冲（动态缓冲每帧换地址）填满 arena。
- 深度 resolve 未实现（阴影/深度纹理）；纹理格式 22/23（k_24_8(_FLOAT) 深度作纹理）、29（k_16_16_16_16_EXPAND）未支持；
  7/54（k_2_10_10_10 及其 AS_16_16_16_16 别名）按 8 位截断上传，resolve 目标格式 7 已支持（HDR 场景缓冲）。
- 纹理缓存没有失效机制（只在 resolve 覆盖时清除），CPU 动态更新的纹理会显示旧内容。
- 纹理 swizzle（fetch 常量 dword3）、mip、立方体/3D 纹理上传、多渲染目标、模板、混合常量色未实现。
- 标题超时后进入开场影片（`xenon_mov.fpd` 内为 ASF/WMV，偏移 0x1000 起），播放器不出帧、画面黑 1–2 分钟后回到标题；
  期间绘制统计与标题完全一致，怀疑播放器卡在 `XMACreateContext` 桩返回失败上。
- plume 补丁：`copyTextureRegion` 目标为缓冲时 `setSamplePositions(nullptr)` 崩溃，见 `tools/patches/plume-lostodyssey.patch`。

## 渲染路线（原始备选）

A. UnleashedRecomp 路线：钩住游戏内静态链接的 D3D9 函数，用 plume 重写；需要在 Ghidra 里定位这些函数。
B. 扩展本命令处理器为完整 Xenos 模拟（解析 draw/状态/fetch 常量/着色器，XenosRecomp 翻译着色器），
   等价于重写 Xenia 的 GPU 后端，通用但工作量大。

## 战斗场景排查记录（2026-09-04 夜）

- **ALU 常量相对寻址**：`const_0_rel_abs/const_1_rel_abs` 按"第几个常量操作数"（src1..src3 顺序）编码，不按操作数位置
  （Xenia `AluInstruction::src_const_is_addressed`）。之前按位置取标志，蒙皮着色器的 `c[8+a0]` 全变成 `c[8]`。已修。
- **VTX_W0_FMT**：置 1 表示着色器输出的就是 w（Xenia `kSysFlag_WNotReciprocal`），为 0 才取倒数；之前反了。已修。
- **遮挡查询**：EVENT_WRITE_ZPD 现在往 RB_SAMPLE_COUNT_ADDR 指向的记录写递增计数（Xenia fake 模式），否则 UE3 把所有物体
  当作被遮挡剔除。
- **渲染目标 dump 会误导**：场景 RT 在 resolve 之后被清除，帧末 dump 只剩 UI；要看 `LO_SCREENSHOT_RESOLVED=1` 导出的
  resolve 结果。HDR 场景缓冲（resolve 目标 fmt 32，来自 EDRAM base 0x2d0 的 fmt 3/10/12 视图）里已能看到士兵队列，
  但最终合成输出为黑：8888 中间缓冲（0xaac000）几乎全白，怀疑后处理读了不支持的深度纹理（格式 22/23 → 哑纹理）或
  EDRAM 同一 tile 基址被以 fmt 0/3/10/12 交替解释（Xenia 用 ownership transfer 转换，我们按 (base,fmt,pitch) 分成不同宿主纹理）。
- 调试开关：`LO_DRAW_TRACE=<帧> LO_DRAW_TRACE_COUNT=<n>`（逐绘制/resolve 详情，含常量与顶点流头部）、`LO_DEBUG_VS=<hash>`
  限定到某个顶点着色器、`LO_PS_DEBUG=<n>`（≥n 索引的绘制输出品红）、`LO_VS_DEBUG=<n>`（替换成固定三角形并把原始 oPos
  经 TEXCOORD15 编码进颜色）、`LO_VS_RAW`（跳过 VTE 尾声）、`LO_NO_DEPTH/LO_NO_CULL/LO_NO_ALPHATEST`、`LO_DUMP_THREADS_AT=<帧>`。
  着色器磁盘缓存文件名带翻译器版本（当前 `_v8`），改 HLSL 生成后要递增。

### 后续修正（同夜）
- 深度目标按 (base, pitch) 共享 D24S8/D24FS8；视口 z 变换（PA_CL_VPORT_ZSCALE/ZOFFSET）改在顶点着色器里做（Xenia 方式），
  宿主视口固定 0..1，反向深度（scale -1, offset 1 + GREATER_EQUAL + 清 0）因此可用。
- **清屏就是画矩形**：360 D3D 的 Clear 是 vte=0x300 的屏幕空间 rect 绘制（深度 func ALWAYS + 写），而且常用另一个
  surface pitch/位深清同一片 EDRAM（1280 宽 32bpp 的主深度用 640 宽的矩形清）。我们按 (base,pitch) 分纹理，所以
  `DrawImpl` 末尾识别这类绘制：深度目标用矩形的 z 做 `clearDepthStencil`，颜色目标把矩形通过拉伸的视口重放到同基址的
  其他纹理。之前主深度一直残留 Loading 画面的 1.0，GE 测试把整个场景都挡掉了。
- 深度 resolve：把深度平面拷到 R32_FLOAT 的 resolved surface，fetch 格式 22/23 读它（`.x` 即写入的深度值）。
- fetch 格式 27/28/29（_EXPAND）按 Xenia 当作 float16，与 resolve 目标 30/31/32 匹配。
- 现状：HDR 场景缓冲里能看到士兵队列（很暗），合成后的最终画面只有微弱轮廓；地形（n=8994 的绘制）没出现。
  下一步：查材质贴图/光照（不支持的贴图格式、光照常量）与 EDRAM 同基址不同格式（fmt 0/3/10/12）的解释。

## EDRAM 视图统一与合成链路（2026-09-04 深夜）

**主因**：同一片 EDRAM（base 0x2d0，pitch 1280）在一帧内被以多种颜色格式绑定：56 个颜色绘制走 fmt 0（8_8_8_8）视图，
而第一次 resolve 却从 fmt 3（2_10_10_10_FLOAT）视图读。之前按 (base, 格式, pitch) 分成不同宿主纹理，于是 resolve 读到一张
从没被画过的纹理 —— HDR 场景缓冲全黑，最终画面只剩深度驱动的黑色剪影。
现在颜色目标只按 (base, pitch) 建一张 R16G16B16A16_FLOAT 纹理，所有格式视图共用。

配套改动：
- **格式转换 blit**：`BlitRegion` 用一个全屏三角形 + `Load()` 把渲染目标搬进另一种格式（SV_Position 在源和目标是同一像素，
  所以不需要常量）。resolve 目标的宿主格式由 destFormat 决定（6 → R8G8B8A8，7/32 → FP16），格式不同时走 blit，
  这样前缓冲仍是 8888、可以直接拷进交换链。
- **格式钳位**：EDRAM 统一成 FP16 后没有了硬件的 8_8_8_8 / 2_10_10_10 钳位，像素着色器尾声按绑定格式 clamp
  （定点 1.0，7e3 31.875，FP16 65504）。
- 贴图按内容抽样哈希每帧复验一次（流式贴图首次上传时内容可能还没写入）；cube 贴图按 6 个面上传并用 CUBE 标志创建；
  base 为 0 的 fetch 改从 mip 链读；哑贴图初始化成不透明黑（原来是未初始化显存）。
- 顶点着色器里的贴图采样改用 `SampleLevel`（`Sample` 在 VS 非法）。
- 描述符集是池化复用的，着色器声明但取不到贴图的槽位现在显式绑哑贴图，不再留着上一次绘制的内容。

**现状**：几何、深度、阴影图、UI 都正确，场景可见；但表面颜色仍然错误（大面积爆白 + 彩色条纹）。已排除：数值范围
（各 resolve 的原始浮点都在 [0,1]，无溢出）、贴图缺失（无槽位回退到哑贴图）、贴图采样本身（LO_PS_TEXDEBUG 输出有结构）。
下一个怀疑对象是深度的 20e4（D24FS8）编码：游戏以 fetch 格式 22/23 读回深度并在着色器里解码，而我们给的是线性 float32，
条纹的分布与深度相关。

调试开关新增：`LO_DUMP_RESOLVE_SEQ=<帧> LO_DUMP_RESOLVE_DIR=<目录>` 按顺序导出该帧每次 resolve 的结果并打印原始浮点
min/max/mean，`LO_PS_TEXDEBUG=1` 让像素着色器输出最后一次贴图采样值，`LO_TEXTURE_STATIC=1` 关闭贴图复验。

## EDRAM 格式类与 Xenia 对照（2026-09-04 深夜二）

对照 Xenia canary（`d3d12_render_target_cache.cc` / `dxbc_shader_translator_om.cc`）核对后的结论：

- Xenia 按 `RenderTargetKey::resource_format` 建宿主纹理，`_AS_*` 变体由 `GetStorageColorFormat` 折叠掉
  （10→2，12→3），所以 2/10 一张、3/12 一张、0/1 一张。我们改成同样的"格式类"划分。
- **k_2_10_10_10_FLOAT（7e3）在 Xenia 里就是 `R16G16B16A16_FLOAT`，不做每次绘制的量化**；7e3 的打包只在
  ownership transfer 和 resolve 前的 dump 时发生。所以 HDR 通道必须用浮点目标 —— 之前把颜色目标降成 8888 时，
  7e3 通道的 0..31.875 被硬件钳到 1.0，整幅画面爆白，这就是"材质没加载"的直接原因。
- Xenia 在 RTV 路径**不**在像素着色器里钳位（靠宿主 UNORM 格式自然钳）；我们所有类都用 FP16，所以保留显式
  钳位（定点 1.0，7e3 31.875，浮点 65504）。
- `color_exp_bias` 本作全为 0，排除。深度 D24FS8 在 Xenia 是 float32 且把客体 [0,2) 映射到宿主 [0,1)；我们不做
  这个重映射，宿主深度即客体深度，客体以 fetch 格式 22/23 读回时拿到的正是客体值，一致。
- **Ownership transfer 已实现但默认关闭**（`LO_EDRAM_TRANSFER=1` 打开）：按 Xenia 的做法把源值打包成客体 32 位字
  再按目标类解包（含 7e3 的 `Float32To7e3`/`Float7e3To32`）。打开后画面出现品红/绿偏色，怀疑打包的通道顺序还需
  对照 Xenia 的 `XeResolveSwapRedBlue_8_8_8_8`。本作各通道是顺序覆盖同一片 tile，关掉后画面正确。

现状：战斗场景构图正确（天空渐变、山脉、士兵、UI），帧间差异 0.2 左右无闪烁。剩余问题是前景人物仍是纯黑剪影，
即光照没有落到角色上。

## 前景人物纯黑的根因：像素着色器常量只上传了 224 个 vec4（2026-09-04 深夜三）

ALU 常量文件两半各 256 个 vec4：顶点着色器 0x4000–0x43FF，像素着色器 0x4400–0x47FF（与 Xenia 的
`XE_GPU_REG_SHADER_CONSTANT_000_X = 0x4000` / `_256_X = 0x4400` 一致）。翻译出的 HLSL 两个阶段都声明
`cbuffer XeConstants { float4 c[256]; }`，但渲染器只把 224 个 vec4 拷进上传环：

```cpp
uint32_t vsConstants[256 * 4], psConstants[224 * 4];   // 错
for (uint32_t i = 0; i < 224 * 4; i++) psConstants[i] = Reg(REG_ALU_CONSTANTS + 256 * 4 + i);
```

角色材质的像素着色器（本作战斗场景是 `ps 2bcb2fea0078fc52`）恰好用 `c[253] / c[254] / c[255]` 存光照方向与
缩放，越界部分读回 0，于是

```
r7.y = saturate(dot(r6.wyz, c[254].xyz));   // → 0
r5.xyz = r6.xzw * c[255].xyz;               // → 0
oC0.xyz = Σ (r7.* * i1..i3 * 贴图)          // → 全 0
```

整条光照链乘成 0，人物输出纯黑；远处物体因为深度雾把黑色混向雾色，才呈现"近黑远白"。改成 256 个 vec4 后
角色材质、地面、云层全部出现。

排查方法（可复用）：`LO_GPU_STATS=1` 现在会打印每帧被静默丢弃的绘制数（按 mode / shader / pitch / pipeline /
upload / index / scissor 分类，附图元类型掩码）——先用它排除"绘制根本没提交"，再用 `LO_DRAW_TRACE` 拿到该帧
每个绘制的 vs/ps 哈希与常量，用 `LO_SHADER_HLSL_DIR` 导出对应 HLSL 静态阅读，`LO_DUMP_DRAW_SEQ` 逐绘制导出
颜色目标定位是哪一笔画坏的。本作一帧约 570 个绘制，其中 ~430 个是 mode 5 深度预通道（含带遮罩变体
`ps fce57e1b46577a69`，只输出 0/1），真正的 3D 材质通道只有 4 笔大绘制（地形 1 笔 + 角色 3 笔）。

注：翻译器里的 `#define FLT_MIN asfloat(0xff7fffff)` 其实是 **-FLT_MAX**，所以
`clamp(log2(x), FLT_MIN, FLT_MAX)` 实现的是 Xenos 的 `LOGC`（log2 的 -INF 钳到 -FLT_MAX），语义正确，
只是宏名有误导性。

## 调试基础设施与战斗帧结构（2026-09-04 下午）

日志：每行前缀 `[运行秒数 t线程标签]`；每 60 个 swap 一条 `heartbeat`（fps、每帧 draw 数、frontbuffer、最近打开的游戏文件）；
游戏文件打开升为 info（`xenon_mov.fpd` = 影片、`xenon_battle.fpd` = 战斗加载）；每帧刷屏的 isr / scratch writeback 日志
降为 `LO_VERBOSE=1` 才输出。启动时打印时间、命令行和所有 `LO_*` 开关。配合 `--quiet-kernel` 一次运行约 1200 行。

自动测试序列（`LO_AUTO_PULSE=6 LO_AUTO_BUTTONS="s@300,a@600,b@900,u@1100,a@1300,s@2000,k@2100,a@2200"`）：
影片从 swap ~1440 开始，START@2000 弹出暂停菜单、BACK@2100 跳过，swap ~2160（约 72 秒）进入开场战斗；
在 1500/1560 按会因为影片还在加载而无效（之前每轮白等 3 分钟）。帧号在不同运行间会漂移（导出/追踪会拖慢游戏），
按帧号触发的导出改用 `LO_DUMP_DRAW_VS=<hash>`（在 `LO_DUMP_DRAW_SEQ` 之后第一次出现该 VS 的那一帧，每个该 VS 的
draw 后导出一次）。其他新增开关：`LO_CLEAR_RT=1|magenta`（每帧首次绑定时擦除颜色目标，品红能直接暴露"整帧没写过"的
像素）、`LO_EDRAM_TRANSFER=read|draw`（ownership transfer，默认关）、`LO_GPU_STATS=1` 现在也打印按原因分类的丢弃 draw 数。
draw trace 行新增 blend / mask / cull / colorctl / aref。

resolve 缓存改为按 (目标地址, 目标格式) 存多份：本作把同一片 EDRAM 先按 7e3 resolve 成 FP16、再按 fmt 10 resolve 成
2_10_10_10，都写到 0x9fa0000，单键缓存会让后者顶掉前者。

开场战斗一帧的结构（frame trace）：~430 个 mode 5 深度绘制（预通道 + 阴影；带遮罩变体用 ps fce57e1b，只输出 0/1），
3D 材质通道只有 4 笔：地形+天空 1 笔（vs 03184cec / ps 311b1400，正常）、角色 3 笔（vs 4053f2a2 / ps 2bcb2fea），
然后 4 张全屏雾面片、粒子四边形、resolve、后处理、UI。**角色 3 笔与地形那笔的 blend/mask/cull/alpha/depth 状态、
目标纹理完全相同，顶点流、索引、世界/视投矩阵与预通道逐位一致，但它们的像素从未被写入**（品红擦除、强制品红输出
两个实验都证明片元没到达目标）。当前最强假设：两个不同 HLSL 程序里同一串 MAD 被 DXC 以不同方式合并/重排，z 差
一个 ulp，`GEQUAL` 把整个角色拒掉；待对照 Xenia 的 dxbc_shader_translator 与我们翻译器里 `precise` 的作用范围验证。
