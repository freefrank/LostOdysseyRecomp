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

## 渲染路线（原始备选）

A. UnleashedRecomp 路线：钩住游戏内静态链接的 D3D9 函数，用 plume 重写；需要在 Ghidra 里定位这些函数。
B. 扩展本命令处理器为完整 Xenos 模拟（解析 draw/状态/fetch 常量/着色器，XenosRecomp 翻译着色器），
   等价于重写 Xenia 的 GPU 后端，通用但工作量大。
