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
- 每帧约 11 次 resolve 各带一次 GPU 同步 + CPU tile 化 1280x720，帧率只有约 16 fps；应改为懒回读（仅当目标被读时）或 GPU 端 tile。
- 深度 resolve 未实现（阴影/深度纹理）；纹理格式 23（k_24_8_FLOAT 深度作纹理）、29（k_16_16_16_16_EXPAND）未支持。
- 纹理缓存没有失效机制（只在 resolve 覆盖时清除），CPU 动态更新的纹理会显示旧内容。
- 纹理 swizzle（fetch 常量 dword3）、mip、立方体/3D 纹理上传、多渲染目标、模板、混合常量色未实现。
- 标题超时后进入开场影片（`xenon_mov.fpd` 内为 ASF/WMV，偏移 0x1000 起），播放器不出帧、画面黑 1–2 分钟后回到标题；
  期间绘制统计与标题完全一致，怀疑播放器卡在 `XMACreateContext` 桩返回失败上。
- plume 补丁：`copyTextureRegion` 目标为缓冲时 `setSamplePositions(nullptr)` 崩溃，见 `tools/patches/plume-lostodyssey.patch`。

## 渲染路线（原始备选）

A. UnleashedRecomp 路线：钩住游戏内静态链接的 D3D9 函数，用 plume 重写；需要在 Ghidra 里定位这些函数。
B. 扩展本命令处理器为完整 Xenos 模拟（解析 draw/状态/fetch 常量/着色器，XenosRecomp 翻译着色器），
   等价于重写 Xenia 的 GPU 后端，通用但工作量大。
