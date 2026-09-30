# CPU 性能优化指南：3C6T 预算、可行并行与反模式

日期：2026-09-14  
分支：`cpu-perf` / 实验分支 `cpu-perf-exp`  
状态：**指南与 R3 实现记录。** Card A / Card B / Card C / Card D（进程级 3C6T 信封）已用 published v0.5.11 测过（耗尽资源类 = null；B1/B2/B3、Parallel Prepare 与默认 pinning 均不实施）。2026-09-15 在本地 `deck` 分支已实现 R3 等待路径现代化（`notified_wait.h`、内核对象条件变量等待、CommandProcessor 写指针与关闭通知及 500 µs 上限等待），未提交、未改源版本、无发布。
正文：简体中文；符号、路径、函数名保持英文。

### 当前状态澄清（2026-09-15）

2026-09-15 本地 `deck` 分支已实现并验证 R3 CPU 等待路径优化，作为开发中未提交变更保留：
- **等待路径现代化**：引入 `LostOdysseyRecomp/notified_wait.h`，在 `kernel/imports.cpp` 中将 Event、Semaphore 和 Mutant 有限超时等待的 200 µs 轮询循环替换为条件变量谓词/截止期等待，保留消费、递归与超时语义；在 `gpu/command_processor.{h,cpp}` 中增加写指针更新与关闭通知，以 500 µs 有界等待替代原 200 次 yield 循环并保留 SDL 窗口事件泵送。
- **验证范围与边界**：Windows `LoNotifiedWaitTest`（预通知、提前唤醒、截止期超时、写指针唤醒）和 `LoPollWaitTest` 通过；Windows 完整目标编译及 diff 检查通过；依赖补丁应用后 Linux 原生构建与 codegen 通过。Radeon 8060S 上进行的 15 W 原生 Vulkan 实测（STAPM 15 W / Fast 25 W / Slow 20 W）创建/调整 1280x720 swapchain，启动 guest 并在 60 W 功耗下准备 28,484 个 shader（28,482 就绪，2 确定性失败），消费启动包后无错误运行 90 秒；目标 60 FPS，前期稳定窗口约 58.46–58.58 FPS，后期复杂场景约 37–40 FPS（**不宣称锁定 60 FPS**）。无玩家视觉验收，未提交、推送或发布。详见 [当前状态](../STATUS.md)。

配套记录：

- [Card A city measurement（2026-09-14；published v0.5.11，无耗尽资源）](cpu-card-a-city-2026-09-14.md)
- [Card B city measurement（2026-09-14；published v0.5.11，B1/B2/B3 不实施）](cpu-card-b-city-2026-09-14.md)
- [Card C prepare gate（2026-09-14；published v0.5.11，不实施 Parallel Prepare）](cpu-card-c-prepare-gate-2026-09-14.md)
- [Card D 3C6T city measurement（2026-09-14；published v0.5.11，默认不钉核）](cpu-card-d-3c6t-city-2026-09-14.md)
- [CPU 重编译深度诊断（2026-09-13；历史诊断）](cpu-recomp-deep-2026-09-13.md)
- [性能分析完整报告（2026-09-11；v0.5.4 诊断）](perf-complete-analysis.md)
- [GPU 环缓冲实测对比（2026-09-11；诊断，非验收）](perf-gpu-ring-compare.md)
- [Vulkan depth-clear performance（2026-09-13；已发布到 v0.5.9）](vulkan-depth-clear-performance-2026-09-13.md)
- 未进 notes 索引、但仍是有效历史证据：[CPU live profile（2026-09-13；`EqualSampleBlock64`）](cpu-live-profile-2026-09-13.md)

证据分级（全文通用）：

| 标记 | 含义 |
|---|---|
| **事实** | 2026-09-14 在 `cpu-perf` 核对过的当前代码契约 |
| **历史证据** | 指定版本/场景的测量；不得当作当前基线或 FPS 收益 |
| **假设** | 尚未用当前二进制复测 |
| **实施前 gate** | 改代码前必须先满足的条件 |

---

## 1. 结论与范围

### 目标

在最多 **3 个物理核心、6 个硬件线程** 的目标资源预算下，回答：

1. 现有 CPU 执行为什么仍然可能慢。
2. 哪些工作已经并发，哪些等待/冗余才是杠杆。
3. 哪些准备阶段可以有界并行，哪些绝对不能拆。
4. 下一位实施者从哪一项开始、怎样验证、何时停止。

### 非目标

- 不复刻 Xbox 360 Xenon 调度器。
- 不新建六 worker / fiber scheduler。
- 不把未同步的 guest 函数拆到多线程。
- 不默认 pinning。
- 不把历史毫秒数写成当前可回收帧时间。
- 本次不实现、不改版本、不跑整局回归。

### 3C6T 暂定定义

**整个游戏进程**（guest 逻辑线程 + host GPU/音频/VSync/中断/shader 准备）共用：

- 最多 3 个物理核心；
- 最多 6 个硬件线程（含 SMT sibling）。

这是 **资源预算 / 调度信封**，不是「进程只能创建 6 条软件线程」，也不是「必须启动 6 个等价 worker」。真正做 CPU 配额实验前，必须再确认测试机能否提供完整的 3 个物理核及其 SMT pair；给进程贴 6 个逻辑 CPU 不等于得到 3 个完整物理核。

### 一句话结论

静态重编译已经把 UE3 的 Game / Render / Async 以及 host GPU、音频线程映射成 host `std::thread`。当前杠杆首先是 **减少等待和冗余**，其次才是对已证明独立的准备阶段做 **Parallel Prepare / Serial Commit**。在 3C6T 预算里再堆 worker，多半会和现有提交路径抢同一把锁、同一道 fence。

---

## 2. 当前执行模型：已有多线程，不是单线程 JIT

### 2.1 静态重编译，不是解释器

`default.xex` 被 AOT 成约 62,800 个 C++ 函数。没有 guest CPU interpreter，也没有 JIT 调度器。`ExCreateThread` 经 `GuestThreadHandle` 1:1 落到 host `std::thread`（`LostOdysseyRecomp/cpu/guest_thread.cpp`）。guest 栈 `STACK_SIZE = 0x100000`（1 MiB）。

`GuestThread::Start` 从 `flags >> 24` 取 `procMask`，用 `7 - countl_zero(procMask)` 得到 `cpuNumber`（mask 为 0 则写 0），写入 PCR `+0x10C`。这只是 **guest 可见的硬件线程编号**，不是 host affinity。

### 2.2 Affinity stub

`KeSetAffinityThread` 在 `LostOdysseyRecomp/kernel/imports.cpp`（约 775–780 行）是 stub：把 previous affinity 写成 `2`，返回 `0`。没有 `SetThreadAffinityMask` / `pthread_setaffinity_np`。

因此：

- 补 stub **不等于** 还原 Xenon 性能；
- PCR 编号与 host placement 是两层；
- 现有线程已经在跑，只是漂在 OS 调度器上。

### 2.3 谁在跑、谁等谁

Guest 侧（UE3 原有并发，已 1:1 映射）：

| 角色 | 入口 | 生产 / 消费 |
|---|---|---|
| Game | 主 tick | 游戏状态；向 Render 投递场景工作 |
| Render | `sub_824856A0` `RenderingThreadMain` → `sub_825B2A20` `FSceneRenderer::Render` | 打包 draw；消费 Game 的场景；向 host GPU worker 投递 command buffer |
| Async / worker | `sub_82290D28` | 包/资源流式加载 |

Host 侧（进程共享同一 3C6T 预算）：

| 角色 | 入口 | 生产 / 消费 |
|---|---|---|
| GPU worker | `CommandProcessor::WorkerMain`（`command_processor.cpp` 约 394） | 解析 ring，生成 command list |
| VSync | `VsyncMain`（约 455；`GuestThreadContext(2)`） | 帧同步 |
| GPU interrupt | `InterruptMain` | 中断/完成通知 |
| APU | `DriverMain`（`audio.cpp`） | 音频驱动 |
| XMA | `WorkerMain`（`xma.cpp` 约 524） | XMA 解码（已是独立准备线程） |
| Shader prep | 异步准备 worker | 启动/首次遇到 shader 时的编译准备 |

依赖要点：

1. Game → Render 的投递顺序是 UE3 契约，不可重排。
2. Render / GPU worker 在 descriptor、upload ring、两槽 GPU slot 上汇合。
3. `Flush()` 提交后 **不等**；真正阻塞发生在 `RecycleSlot` 要复用即将被 GPU 占用的 slot 时。
4. 音频与 shader prep 已经占用预算；再加无界 worker 会挤占 Game/Render。

### 2.4 当前 GPU 提交契约（事实）

`Renderer` 使用 `kGpuSlots = 2`（`renderer.cpp`、`render_arena_policy.h`）。

- `Flush()`（约 1582–1598）：`executeCommandLists`，然后 `gpuSlot++`，**不等待 fence**。
- `RecycleSlot`（约 1505）：`waitForCommandFence`，然后回收 `queryResults`、`retiredTextures` / framebuffer、`textureSetCache.Clear`、`temporalHistory.ReleaseCompletedThrough`、`sceneAABusy = false`，并重置 `uploadOffset` / `setPoolUsed`。
- `Begin()` 对当前 `gpuSlot` 调用 `RecycleSlot`。
- `Upload()` 在 ring 满时会中途 `Flush` + `Begin`。
- `DrawImpl` 在 `poolsFull`、`ringLow` 或 arena wrap 时仍会 `Flush` + `Begin`。

Descriptor 上限（`render_batch_policy.h`，**事实，不是历史 500**）：

- D3D12：`kD3D12DescriptorLimit = 1800`
- Vulkan：`kVulkanDescriptorLimit = 2048`
- 下限：`kMin = 500`
- 未使用的 3D/cube bank 走静态 dummy set；真正吃额度的是 unique 2D combo。

历史笔记里的 `kMaxSetsPerKind = 500` 以及「`Flush` 立刻 `waitForCommandFence`」是 **v0.5.4 及更早源码契约**，不能当当前代码引用。

D3D12 的 fence wait 是 `WaitForSingleObjectEx(..., INFINITE)`；Vulkan 是 `vkWaitForFences(..., UINT64_MAX)` 后 `vkResetFences`。两槽已经允许「上一槽 GPU 执行、当前槽 CPU 录制」重叠；**不能声称 fence wait 可被完全消除**——两帧前的 slot 仍必须等 GPU 完成才能回收 descriptor / upload / query。

---

## 3. 证据台账与未知项

### 3.1 已落地、不要当未做项重做

| 项 | 状态 | 证据 |
|---|---|---|
| 两槽 command/descriptor ring | 已在当前代码 | `kGpuSlots = 2`；`Flush` 不等待 |
| Descriptor 默认上限 1800/2048 | 已在当前代码 | `render_batch_policy.h` |
| Dummy 3D/cube bank | 已在当前代码 | unique 2D 才消耗 limit |
| `EqualSampleBlock64` | 已发布到 v0.5.8 | `geometry_prepare.h` SSE2 4×16；[cpu-live-profile](cpu-live-profile-2026-09-13.md) |
| Texture-key avalanche mixing | 已发布到 v0.5.9 | `gpu/texture_key.h`：`>>16 * 0x7FEB352D >>15 * 0x846CA68B >>16` |
| Shader identity cache、Plume bind suppress、per-slot descriptor reuse | 已随 v0.5.9 候选发布 | [vulkan-depth-clear](vulkan-depth-clear-performance-2026-09-13.md) |
| `poll_wait` 自适应退避 | 已在当前代码 | 仅 `Query` / `SharedValue` / `GpuPoll`；32 次后 yield；GpuPoll 上限 50 µs |
| Query `getenv` / `LO_QUERY_TRACE` 缓存 | 已随 v0.5.8 | 同上 live profile 后续提交 |

### 3.2 历史测量（禁止当当前基线）

**v0.5.4 城市 1280×720**（[perf-complete-analysis.md](perf-complete-analysis.md)，已发布包）：

- 约 31–44 fps，1800–2300 draws，`draw_ms` 21.19，`fence_wait` 15.40 ms **嵌在 `tDraw` 里**。
- 当时源码：`Flush` 提交后立刻等 fence。该片段已过期。
- 标题画面约 60 fps、114 draws、2 batches、`fence_wait` 4.34 ms。

**两槽诊断、非验收**（[perf-gpu-ring-compare.md](perf-gpu-ring-compare.md)，同城市场景 vs v0.5.4）：

- RelWithDebInfo、两槽、limit=1800：FPS 57.7，batches 2.00，`descriptor_splits` 0，`fence_wait` 1.67 ms（范围 0.001–11.72）。
- 这是诊断对照，不是验收，也不是当前 `cpu-perf` 二进制的基线。

**source-0.5.8 WPR**（[cpu-recomp-deep-2026-09-13.md](cpu-recomp-deep-2026-09-13.md)，commit `6e6f11cf`，PID 38988，4K internal / 1920 window / AA3 / D3D12 / 8060S，包约 40W）：

- 心跳 31.9121 FPS，2284–2302 draws。
- Render TID 占 41.86% samples；guest render 24.0%，几乎全在 `sub_824856A0`（其中 79.93% 在 `FSceneRenderer::Render`）。
- `memcmp` **766 / 10848 = 7.0612% 的 Render 线程 self samples，不是整帧 7%**：vertex 376、`UploadUnchanged` 226、shader-identity 132、texture desc 32。
- Texture cache 快照 386 keys、33 buckets、最长链 84；hash hotspot 约 2.93% render self。证据目录 `out/cpu-recomp-deep-20260913/`。
- **链长 84 发生在 avalanche mixing 发布之前。** v0.5.9 之后不得再把「修复哈希」写成未做工作；剩余问题是：**当前包是否仍有长链、lookup 是否仍占可测 CPU。**

**`EqualSampleBlock64` live**（[cpu-live-profile-2026-09-13.md](cpu-live-profile-2026-09-13.md)；隔离 9.45–14.99×；固定 user00 40W 4K）：

- 均值 FPS 46.787 → 47.074（+0.61%）。
- vertex-match 2.279 → 0.488 ms。
- GPU queue 仍约 20.7 ms，所以局部 CPU 变快 **没有** 把 4K 送到 60。
- 同一候选 1080p 均值 59.981。已发布到 v0.5.8。

**寄存器 SIMD 否决**（[vulkan-depth-clear-performance-2026-09-13.md](vulkan-depth-clear-performance-2026-09-13.md)）：

- const 路径 1.531 → 2.157 ms，FPS 42.72，已回退。
- 4K Vulkan 60W user01 静态：depth-clear coalesce 把均值 FPS 从 7.496 拉到 43.476——这是 **GPU** 收益，不能算进 CPU 多线程故事。
- 已发布 v0.5.9（`d26ee8b`，CI `34778434518`）。

### 3.3 当前未知（实施前 gate）

1. **当前 `cpu-perf` / 已发布 v0.5.11 包** 在城市场景的 `fence_wait`、`descriptor_splits`、mid-frame `Flush` 次数。两槽可能已经把多数等待重叠掉；没有新测量就不能排「再加 slot」。
2. avalanche 之后的 **真实 hash 链长与 lookup CPU**。
3. 剩余 `memcmp` 的长度分布：64B 已走 SIMD；其它长度是否仍热。
4. 3C6T 测试机是否真能提供 3 个物理核 + SMT pair（P/E、CCD、3D V-Cache 都会让「贴 6 个逻辑 CPU」失效）。
5. 任何新并行准备任务的：输入快照、输出所有权、取消/失效、与现有 XMA/shader worker 的预算叠加。

---

## 4. 3C6T 的正确使用方式

Xbox 360 Xenon 是 3 个 in-order 物理核 × SMT = 6 硬件线程。同一核上的两条硬件线程共享 L1；ATG 的经验是：两条重负载线程绑在同一核上，常常只有 +10–20%，不是 ×2。

本项目里「模仿 3C6T」只允许三件事：

1. **把 3C6T 当压力信封。** 优化必须在「进程级 3 物理核 / 6 硬件线程」下仍为正收益。默认调度已经在用更多核时，优化不得依赖「机器有 16 线程」才能变快。
2. **给已有线程留核。** Game、Render、GPU worker、音频至少要能跑；新增并行准备占用的逻辑线程必须计入预算，而不是无限加池。
3. **可选、可关的 topology-aware placement 实验。** 把现有线程放到合适的物理核 / SMT sibling。这是实验层，不是正确性修复，默认关闭。

明确不是：

- 不是 6 个软件 worker 模拟 6 条 Xenon 硬件线程。
- 不是把 guest 函数按 PPC 地址切开。
- 不是「进程总共 6 条 `std::thread`」。当前软件线程数已经多于 6；限制的是 **同时占用的硬件线程**。

现代主机差异：

- Intel P/E：把 Render/GPU worker 钉到 E-core 会拉高尾延迟。
- AMD 多 CCD / 3D V-Cache：跨 CCD 共享缓存很差；应优先让热线程留在有 V-Cache 的 CCD。
- 掌机 4C8T / 8C16T：3C6T 仍是 **预算**，不是必须关掉多出来的核；但验收场景应包含「被限制到 6 硬件线程」的一次对照。

默认保持 OS 调度。Affinity 只作为带开关的实验。

---

## 5. 优化优先级与路线表

顺序固定。前一项没有当前测量，不得跳到后一项「加线程」。

| 优先级 | 方向 | 杠杆类型 | 3C6T 含义 | 状态 |
|---|---|---|---|---|
| A | 测量剩余 fence / mid-frame split | 等待 | 不增加线程 | v0.5.11 城市已测；无耗尽资源 |
| B | 剩余冗余 CPU（非 64B `memcmp`、hash lookup、重复 identity） | 单线程开销 | 不增加线程 | 64B SIMD 与 avalanche **已做**；B1/B2/B3 不实施 |
| C | 有界 Parallel Prepare / Serial Commit | 并行准备 | 最多再占 1 个 SMT sibling，且可证明独立 | 启动 prepare 已存在；新 Parallel Prepare 不实施 |
| D | 可选 topology-aware affinity | placement | 只移动现有线程 | 进程级 3C6T 城市已测（affinity 0x3F）；**默认钉核不实施** |

Amdahl 警告：城市场景的历史主因是提交路径上的串行等待，不是「没用满 6 线程」。在 fence 仍主导时加 worker，只会让更多线程堵在 `RecycleSlot` / descriptor 分配上。

---

## 6. 候选实施卡片

### 卡片 A — 剩余 fence / split 测量（先做，且本身几乎不是代码改动）

**问题与证据**  
v0.5.4 城市 `fence_wait` 15.40 ms 来自「`Flush` 立刻等」。当前 `Flush` 已不等，两槽 + 1800/2048 limit 的诊断曾把同城市场景 `fence_wait` 降到 1.67 ms 均值，但范围仍到 11.72 ms，且那不是验收、也不是 v0.5.11 / `cpu-perf` 基线。

**当前测量（事实，2026-09-14）**：published v0.5.11 Hidden 1280×720 D3D12 AA3 60-cap 城市 `user01`，1836 city frames。`fence_wait_ms` 均值 0.0007、max 0.1371（0 帧 >1 ms）；`nested_flush_ms` 全 0；descriptor / upload / arena splits 全 0（D3D12 limit 1800）；`gpu_batches` 均值 1.0005；GPU queue 均值 0.8115 ms、max 1.5575（0 帧 >16.67 ms）。耗尽资源类 = null。历史 15.40 ms 与两槽诊断 1.67 ms **不得**替换本基线。详见 [Card A city measurement](cpu-card-a-city-2026-09-14.md)。

**代码入口**  
`LostOdysseyRecomp/gpu/renderer.cpp`：`Flush`、`RecycleSlot`、`Begin`、`DrawImpl`、`Upload`。  
`LostOdysseyRecomp/gpu/render_batch_policy.h`。  
计时：`LO_RENDER_TIMING`、`LO_FRAME_TIMING`、`LO_GPU_STATS`。  
城市场景驱动：`out/perf-ring/drive-city.ps1`（若仍可用；不可用则记 gate，不编造命令）。  
测试：`LoRenderBatchPolicyTest`、render-timing fixture。

**改动边界**  
本卡片默认 **只测量**。只有当当前包仍出现「每帧多次 `Flush` + `RecycleSlot` 阻塞」且能指出 **哪类资源**（descriptor set、upload ring、query、arena wrap）耗尽时，才允许单独提案：提高该类上限、推迟回收、或增加第 3 个 slot。禁止用一个笼统的「再加 ring」覆盖三类生命周期。

**数据 / 资源契约**  
Slot N 的 descriptor、upload、query、`textureSetCache`、temporal history 在 GPU 完成 N 之前不可复用。`RecycleSlot` 是该契约的汇合点。增加 slot 会增加常驻显存与 CPU 映射内存。

**3C6T 预算**  
测量本身不占额外硬件线程。若加 slot，CPU 录制仍在 Render/GPU worker 上，不新增并行度。

**验证**  
固定版本、场景、分辨率、API、功耗墙；warm-up 后重复。同时看：均值/尾帧 FPS、`fence_wait` 均值与 p95、`descriptor_splits`、每帧 `Flush` 次数、GPU queue 时间。局部 `fence_wait` 下降但 GPU queue 仍 >16.6 ms，不得宣称「CPU 优化成功」。

**回退**  
测量不改代码。若后续改 slot/limit：保留原 `kGpuSlots=2` 与 1800/2048 为默认，用开关或编译期常量回退。

**未决条件**  
当前包城市对照已完成（published v0.5.11）。没有耗尽的 descriptor / upload / arena / fence 资源；「再加 slot / 再抬 limit」标为低优先级。A 的实现子项全部停在 gate。下一步是卡片 B 的当前 profile，不是改 GPU 槽。

---

### 卡片 B1 — 剩余 `memcmp` / 脏检测（非 64 字节块）

**问题与证据**  
source-0.5.8：`memcmp` 占 Render **self samples** 的 7.0612%，不是整帧 7%。其中 vertex 376、`UploadUnchanged` 226、shader-identity 132、texture desc 32。v0.5.8 已对 64 字节块走 `EqualSampleBlock64`；隔离加速很大，4K 整帧只 +0.61%，因为 GPU queue 仍约 20.7 ms。寄存器 SIMD 在常量上传上变慢并已回退。

**代码入口**  
`LostOdysseyRecomp/gpu/geometry_prepare.h`：`EqualSampleBlock64`；`SampledContent` Visit（≤8192 全比，否则头尾 512 + 64 个 64B 窗口）。  
其它仍 `memcmp` 的调用点需在实施前用当前 profile 列出，不在指南里猜测。  
测试：`geometry_prepare_test.cpp`、`LoVertexCacheTest`、`shader_identity_test.cpp`。  
环境：`LO_VERTEX_TIMING`。

**改动边界**  
只碰 **已证明热、且长度不是 64B** 的比较。禁止再做一遍寄存器 SIMD。禁止把「手写 SIMD 一定更快」当默认。

**数据契约**  
比较必须保持与当前 `memcmp` 相同的相等语义（含提前退出）。输出仍由 Render 线程串行提交。

**3C6T 预算**  
单线程开销削减，不占新硬件线程。正收益时 Recycle 等待应间接下降，但不是并行。

**验证**  
先看该路径自身耗时（如 vertex-match ms），再看整帧。必须单变量：只改比较实现。若隔离变快、整帧不变，记录为「非帧率瓶颈」并停止，不扩大 SIMD 范围。

**回退**  
保留 `memcmp` 路径为 fallback；寄存器 SIMD 的否决仍然有效。

**当前测量（事实，2026-09-14）**：同一 published v0.5.11 城市包加 `LO_VERTEX_TIMING=1`，1846 city frames。`match_ms` 均值 0.1535（阶段和 0.3616 的 42%）。`LO_VERTEX_TIMING` 是 vertex-cache 墙钟，**不是** `memcmp` 长度直方图。直方图仍缺。详见 [Card B city measurement](cpu-card-b-city-2026-09-14.md)。

**未决条件**  
当前包的长度直方图与调用点占比。没有直方图不得扩 SIMD。本轮 **不实施**。

---

### 卡片 B2 — Texture hash lookup（仅当 avalanche 后仍热）

**问题与证据**  
source-0.5.8 最长链 84、hash hotspot ~2.93% render self。v0.5.9 已加入 avalanche mixing。当前代码 `TextureKeyHash` 在 `gpu/texture_key.h`。历史链长 **不能** 再当缺陷清单。

**代码入口**  
`gpu/texture_key.h`、texture cache 实现、`LoTextureKeyTest`、`LoTextureDescriptorCacheTest`。

**改动边界**  
先对当前包做链长/lookup 计时。只有最长链与 lookup CPU 仍可观时，才允许改桶数或键布局。不要「再写一种 mix」。

**数据契约**  
Key 相等语义不变；并发查找若存在，必须仍是现有锁/无锁约定，不引入跨线程写 cache。

**3C6T 预算**  
查找仍在 Render/GPU worker 上。禁止为 hashing 单开 worker。

**验证**  
链长分布 + 该函数 self time + 整帧。只改善分布不是成功。

**回退**  
保持现有 avalanche。

**当前测量（事实，2026-09-14）**：城市 `shader_lookup_ms` 均值 0.051、`pipeline_lookup_ms` 0.03；vertex cache `rehashes` 合计 0（131072 buckets）。evictions 是 vertex-cache 容量周转，不是 texture hash chain。没有 texture 链长快照。lookup 在 avalanche 后不热。详见 [Card B city measurement](cpu-card-b-city-2026-09-14.md)。

**未决条件**  
v0.5.9+ 的 texture cache 链长快照。没有快照且 lookup 不热则 **不实施**、不要再写一种 mix。

---

### 卡片 B3 — `poll_wait`（默认不扩）

**问题与证据**  
`LostOdysseyRecomp/cpu/poll_wait.h` 只覆盖已知 guest 循环：`Query`、`SharedValue`、`GpuPoll`。32 次轮询后 yield；`GpuPoll` 延迟上限 `kGpuPollDelayCapUs = 50`。粗 `Sleep` 会打坏帧节奏。

**代码入口**  
`poll_wait.h` 及现有调用点；测试 `LoPollWaitTest`、`LoFramePacerTest`。

**改动边界**  
没有新的 guest 忙等证据，禁止扩大 Kind 集合、禁止加长 cap、禁止改成 OS sleep。

**数据契约**  
guest 可见的 query/shared value 语义不变；yield 不得变成「结果尚未就绪却继续」。

**3C6T 预算**  
忙等会浪费 SMT sibling。自适应 yield 是在 **减少** 对预算的浪费，不是加线程。

**验证**  
现有 `LoPollWaitTest`。运行时用 `LO_QUERY_TRACE`（已有缓存）看轮询次数，不抢前台。

**回退**  
保持 32 / 50 µs。

**当前测量（事实，2026-09-14）**：本次城市跑未设 `LO_QUERY_TRACE`，也没有新的 guest spin 证据。保持 32 / 50 µs。详见 [Card B city measurement](cpu-card-b-city-2026-09-14.md)。

**未决条件**  
新的 guest spin 调用点证据。没有就不做。本轮 **不实施**。

---

### 卡片 C — 有界 Parallel Prepare / Serial Commit

**问题与证据**  
可并行的只能是 **输入只读、输出有明确所有权、提交仍由 Render/GPU worker 串行完成** 的准备活。已经存在的例子：shader 异步准备、XMA `WorkerMain`。没有证据支持：把 `FSceneRenderer::Render`、guest 场景遍历、可变 descriptor 状态丢进线程池。

**代码入口**  
视具体任务而定。候选方向（均为 **假设**，未授权）：只读 texture key 预哈希、独立输出缓冲的顶点格式转换切片。必须先有「任务足够大、执行期间输入不变」的测量。

**改动边界**

- Prepare：worker 只写自己的输出缓冲。
- Commit：原 Render/GPU worker 按原顺序提交。
- 禁止 worker 碰 guest 内存、TLS/PCR、UE3 渲染可变状态。
- 新池大小上限：在 3C6T 下 **最多再占用 1 条硬件线程**；宁可复用已有 shader/XMA 线程，也不新建第三套池。

**数据契约**  
必须写明：快照时刻、谁拥有输出、谁在 Commit 前 join、任务取消时如何丢弃过期结果、失败时如何回退到串行 prepare。缺任何一项 = 不可实施。

**3C6T 预算**  
Game + Render + GPU worker + 音频已经占满或接近 3C6T。新 worker 默认与某条现有轻线程共享一颗物理核的 SMT sibling。若测量显示准备任务 < 排队/复制开销，取消该候选。

**验证**  
数据竞争（现有单测 + 必要时 TSAN/应用级重复播放）、提交顺序、过期结果不提交、3C6T 限制下的尾帧。平均 FPS 上升但 p95 变差 = 失败。

**回退**  
编译期或运行时开关回到串行 prepare；默认关。

**当前测量（事实，2026-09-14）**：复用 Card B 城市日志，无新城市跑。启动 shader bundle 2248 ms（设备模块 56 ms），pipeline prepare 21 ms / 4 workers / 222 recipes。城市场景 per-frame `copy_ms` 0.0091、`shader_lookup_ms` 0.051、`pipeline_lookup_ms` 0.03，小于排队/复制开销。新 Parallel Prepare 缺快照/所有权/join/取消/串行回退。详见 [Card C prepare gate](cpu-card-c-prepare-gate-2026-09-14.md)。

**未决条件**  
独立任务尺寸、输入稳定性、与已有异步工作的叠加。全是 gate。本轮 **不实施** 新 Parallel Prepare；不要再加第三套准备池。Card D 仍默认关。

---

### 卡片 D — 可选 topology-aware affinity（实验层）

**问题与证据**  
`KeSetAffinityThread` 仍是 stub。PCR `+0x10C` 已写 guest cpuNumber。现代拓扑与 Xenon 不同。没有证据表明默认 pinning 会更快。

**代码入口**  
`LostOdysseyRecomp/kernel/imports.cpp` `KeSetAffinityThread`；`guest_thread.cpp` 启动路径。正确性（guest 读到的 affinity）与性能 pinning 必须分开。

**改动边界**

- 正确性：若游戏依赖 previous affinity 返回值，stub 写死 `2` 可能已偏离；这是语义问题，单独修，不和性能绑在一起。
- 性能：可选映射表，默认关。必须识别 P/E、CCD、SMT sibling；识别失败则 fallback 到 OS 调度，不瞎钉。

**数据契约**  
不改变线程创建数量。只改变现有线程的 placement。Guest PCR 编号不必等于 host CPU id。

**3C6T 预算**  
实验时应把进程限制在 3 物理核 / 6 硬件线程，并保证 Game 与 Render 不挤在同一 SMT pair 上（它们是两条重负载）。GPU worker 与 Render 的共享/分离需要实测，不在指南里拍板。

**验证**  
同一场景、开关开/关对照；记录拓扑、哪些线程钉到哪。无拓扑信息的机器上必须走 fallback 且不明显变慢。

**回退**  
默认关；开关关即恢复 OS 调度。

**当前测量（事实，2026-09-14）**：published v0.5.11 Hidden 1280×720 D3D12 城市，进程 affinity `0x3F`（9800X3D 核 0–2 / 逻辑 0–5），1370 city frames。`fence_wait` 均值 0.0008、`gpu_queue` 0.816、`draw_ms` 3.805 / p95 4.556 / max 13.65，`over_budget=0%`，splits 全 0。冷启动 bundle 缺失时 prepare 仍按 `hardware_concurrency=16` 要 15 个 worker。UnleashedRecomp 的 `KeSetAffinityThread` 同为 stub，无 host pinning。详见 [Card D 3C6T city measurement](cpu-card-d-3c6t-city-2026-09-14.md)。

**未决条件**  
本机拓扑可读；3C6T 作用于整个进程的信封已测。默认 pinning 仍不实施。这不是 Steam Deck、15 W、4C8T 或 1080p60@15W。

---

## 7. 不可采用的方案与适用性限制

| 方案 | 为什么不行 |
|---|---|
| 六 worker / fiber 模拟 Xenon 6T | Xenon 6T 是硬件 SMT，不是软件池。进程里已经有 Game/Render/Async/GPU/音频/VSync。再造 6 池会破坏顺序并打满预算。 |
| 把未同步 guest 函数拆成多线程 | 破坏 PPC 弱内存、`lwarx`/`stwcx`、UE3 Game→Render 顺序、TLS/PCR/1 MiB guest 栈。静态重编译没有解释器可插 fiber。 |
| 「补上 `KeSetAffinityThread` 就能还原 360 性能」 | Stub 只影响 **已有线程的位置**。历史瓶颈是 fence/比较/哈希，不是没钉核。 |
| 默认 pinning | P/E、CCD、掌机都会钉错。无拓扑 = 禁止钉。 |
| 宣称多缓冲 ring 能消灭全部 fence wait | 两槽已存在。GPU 仍必须完成才能回收 slot。历史 15 ms 含旧契约。 |
| 把 `memcmp` 7% render samples 当成整帧 7% | 分母是 Render self samples。4K 上 vertex SIMD 只换来 +0.61% FPS。 |
| 默认手写 SIMD 快于运行库 | 寄存器 SIMD 已否决（const 1.531→2.157 ms）。 |
| 把 source-0.5.8 链长 84 写成当前缺陷 | avalanche 已随 v0.5.9 发布。 |
| 扩大 `poll_wait` 或改粗 Sleep | 无新证据；会打帧节奏。 |
| 并行化 `FSceneRenderer::Render` 或可变渲染状态 | 无同步证据。 |
| 用 16 核桌面收益证明 3C6T 方案 | 验收必须含预算受限对照。 |

---

## 8. 分阶段验证与停止标准

### 可复用工具（不要发明命令）

- 单测：`LoPollWaitTest`、`LoRenderBatchPolicyTest`、`LoTextureDescriptorCacheTest`、`LoTextureKeyTest`、`LoVertexCacheTest`、`geometry_prepare_test.cpp`、`shader_identity_test.cpp`、`LoFramePacerTest`、render-timing fixture。套件说明见 `tools/tests/README.md`。
- 运行时环境变量：`LO_RENDER_TIMING`、`LO_FRAME_TIMING`、`LO_VERTEX_TIMING`、`LO_GPU_STATS`。
- 城市场景：`out/perf-ring/drive-city.ps1`（存在则用；不存在则记 gate）。
- 历史 WPR：`out/cpu-recomp-deep-20260913/`（source-0.5.8，不可当当前基线）。

Agent 运行时测试须后台、默认静音、不抢前台。需要前台交互时记为 gate，等用户安排。

### 实验纪律

1. 固定二进制版本、场景、分辨率、图形 API、功耗墙。
2. Warm-up 后重复；报告均值与尾帧，不只平均 FPS。
3. **单变量。** 禁止一次同时开 SIMD + 新 worker + affinity。
4. 同时看目标瓶颈和整帧。局部改善不是整体成功。
5. 正确性门槛：画面/提交顺序/异步过期结果；相关单测保持绿。
6. 3C6T 对照：至少一次进程被限制在 6 硬件线程；若机器给不出 3 物理核 SMT pair，标注实验无效，不把桌面 16T 结果写成 3C6T 结论。

### 停止 / 负收益

出现任一条就停该候选并回退：

- 目标计时下降，但 p95 帧时间上升。
- GPU queue 仍主导，继续削 CPU 无整帧收益（参见 4K +0.61%）。
- 新增等待、锁、拷贝超过准备任务本身。
- 数据竞争、提交乱序、资源未完成就复用。
- 3C6T 下变慢，只在宽机器上变快。

指南阶段不跑整套游戏回归。未来优化验证与本次文档验收分开。

### 本文档自身验收（本次）

- 九节齐全；卡片字段齐全。
- 历史数字带版本/场景/分母。
- 可行 vs 不可行可执行，不把 gate 写成已决策方案。
- 不改运行时、版本、ROADMAP。

---

## 9. 给下一位实施者的起步清单

1. **先做卡片 A 的测量**，不要加线程。用当前要优化的二进制（写明是 `cpu-perf` 本地构建还是已发布 v0.5.11 包）在固定城市场景打开 `LO_RENDER_TIMING` / `LO_GPU_STATS`，记录每帧 `Flush` 次数、`fence_wait`、`descriptor_splits`、GPU queue。2026-09-14 已对 **published v0.5.11** 城市包完成一次测量，见 [Card A city measurement](cpu-card-a-city-2026-09-14.md)。
2. 若 `fence_wait` 已接近诊断对照里的低个位数毫秒且 `descriptor_splits = 0`：把「再加 slot / 再抬 limit」标为低优先级，转卡片 B 的当前 profile。Card A 测量满足该条件（`fence_wait` 均值 0.0007 ms，splits 全 0）。Card B 当前 profile 已记录：B1 仍缺长度直方图故不扩 SIMD；B2 lookup 不热且无 texture 链长快照；B3 无新 guest spin。见 [Card B city measurement](cpu-card-b-city-2026-09-14.md)。
3. 若仍有中途 `Flush`：指出耗尽的是 descriptor、upload 还是 arena，再单独提案。禁止笼统「多缓冲」。
4. 任何并行准备：先写数据契约（快照、所有权、join、取消），再写代码。默认关。3C6T 下最多再占 1 条硬件线程。
5. Affinity 保持实验、默认关。进程级 3C6T 城市信封已测，见 [Card D](cpu-card-d-3c6t-city-2026-09-14.md)；不要把 pinning 写进 `KeSetAffinityThread`。修 stub 返回值若是正确性问题，单独提交，不和性能混。这不是 Steam Deck / 15 W 验收。
6. 没有新的授权不要改源版本，不要把本指南写进 Release notes。

**第一项工作就是测量，不是实现。**
