# ReBlue vs Lost Odyssey GPU 对照 — 2026-09-11

对照对象：[zolaware/reblue](https://github.com/zolaware/reblue)（Blue Dragon，`main`）与当前 Lost Odyssey `renderer.cpp` / `video.cpp`。两边都走 [plume](https://github.com/zolaware/reblue)。这不是性能验收，也不是 60 fps 声明。

ReBlue GPU 在 `src/gpu/`，没有顶层 `gpu/`。关键文件：`device.h`、`frame_ring.cpp`、`present.cpp`、`bindless_allocator.h`。

## 相同点

- 静态重编译 + Xenos 命令缓冲，而不是整机模拟。
- 主机图形层是 plume：`RenderCommandList` / `RenderCommandFence` / `executeCommandLists` / `waitForCommandFence`。
- Windows 可编 D3D12 或 Vulkan。
- 提交用每槽一把 fence；资源销毁要等 GPU 用完（ReBlue 的 `DrainSlot` / 我们的 `retiredTextures`）。
- 都有描述符上限：ReBlue bindless 65536 贴图槽；我们 D3D12 shader-visible heap 也是 65536 views。

## 提交模型（最大差异）

ReBlue `device.h` 固定 `kNumFrames = 2`。`frame_ring.cpp` 的顺序是：

1. `BeginCommandList`：所有录制的唯一入口，绑当前槽。
2. `SubmitOpenListLocked`：`end()` → `executeCommandLists(..., fences[cur])`，**不等**。
3. Present 后再 `AdvanceAndWaitReused`：只等 **即将复用、已经一帧前的槽**。

注释写明：`kNumFrames=1` 时复用的就是刚提交的那张 list，等于整帧 stall。

我们改之前就是这一种：单 list，Flush 后立刻 `waitForCommandFence`。城市约 5 批 × 串行等待，把未分项的 ~12 ms 嵌进 `tDraw`。

当前源码已开始往 ReBlue 靠：

| | ReBlue | LO 现在（未跑游戏） |
|---|---|---|
| in-flight | 2 套 list + fence + upload | `GpuSlot gpuSlots[2]` 已写，编包未完成 |
| Execute | Present 时 submit，不等刚踢出的活 | `Flush()` submit 后切槽，`Begin()` 只 `RecycleSlot(当前槽)` |
| 等谁 | 一帧前的槽 | 复用槽；arena 回绕才 `WaitForGpu()` 两槽 |
| Present | 不等刚提交的 graphics fence | `g_presentPending`，下次 present / upload / 退出再等 |
| 中途拆批 | bindless，很少因描述符切帧 | 池 / ring / arena 满仍 Flush；切到另一槽可继续录 |

## 描述符

ReBlue：`kBindlessTextureCount = 65536`，`kBindlessSamplerCount = 1024`，occupancy vector first-fit，graveyard 等槽 fence 后再释放。

LO：每 draw 三套 32-slot set。已做 D3D12 同绑定复用（未用 3D/cube 走 dummy key），上限 1800。Vulkan 仍 2048。`LO_DESCRIPTOR_REUSE=0` 可关。这是 bindless 的小改法，不是同一套 API。

## 还不该抄的

- `SetName` 当 pending 标记（ReBlue 早期/旁系实现有过；当前 `frame_ring` 用 `submitted` 旗标）。
- 只按帧回收描述符、不管 fence。
- 第一轮就上完整 bindless 64K。
- 把热点占比换算成可回收 FPS。

## 对 60 fps 的含义

城市基线（已发布 v0.5.4，无 PDB）：31–44 fps，~1800 draws，~5.2 batches，`draw_ms` 21.19，其中 ~12 ms 是中途 fence。菜单已 60 fps，约 1.2 核，最忙线程 14.7 s/15 s 在无符号 EXE。

ReBlue 能在弱 GPU 上稳住，靠的是 **2 槽重叠 + 少切批**，不是少解 PM4。我们描述符复用 + 1800 上限是为少切批；2 槽 ring 是为 CPU 录 N+1、GPU 跑 N。两件都落地并且 `LO_RENDER_TIMING` 再测之前，不能声称追上 ReBlue。

## 本仓库实现状态

已进树、header fixture 通过：`render_batch_policy` 22/22，`texture_descriptor_cache` 11/11。

2 槽 ring 与 Present 延迟 wait 已改 `renderer.cpp` / `video.cpp`。全量 clang 编包被 PPC 库/`ppc_sync` 和 CMake 路径打断，**没有新 runtime，没有新游戏采样**。
