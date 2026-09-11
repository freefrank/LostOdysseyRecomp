# 性能分析完整报告 — 2026-09-11

离线诊断。目标是已发布 v0.5.4 runtime。本文件合并 `perf` 分支已测数字与当前源码路径对照，列出所有能定位的优化点。这不是性能修复、玩家验收或新 Release。

索引与采集边界见 [perf-analysis-index](perf-analysis-index.md)（`perf` 分支）和 [assembly profiler gameplay captures](asm-profiler-gameplay.md)。测量语义见 `.agents/skills/lo-profiling/`。

## 1. 身份与能做的范围

| 项 | 值 |
|---|---|
| Runtime | 已发布 v0.5.4，SHA-256 `3c3b4073f1b7abbcce38747dc08763335ac019edf560df849177176d0399f949` |
| Profiler | `lo_asm_profiler.exe` `d6326a19…`；`wall_clock_all_threads_suspend_context`，15 s / 10 ms |
| PDB | 无匹配游戏 PDB；本机 `out/build/windows-clang` PDB 日期 2026-09-06，与此 EXE 不匹配 |
| 分辨率 | 心跳 frontbuffer 1280×720 |
| 游戏进程 | 本轮分析时未在运行；未再开游戏、未补采、未混用旧 PDB |
| 本 checkout | `out/asm-profiler/play-2026-09-11-*` 与 `RESEARCH.md` **不在磁盘**；数字从 `perf` 分支已跟踪索引抄录 |

本轮能做：复用已测帧计时 / asm 快照，对照 `Draw` / `Flush` / present / guest poll 源码，解释批次与未分项时间，给出可修方向。

本轮不能做（测量天花板，不是未完成项）：匹配 PDB 的 on-CPU 符号；瓦特 / ETW / cycles；逐 pass GPU busy；玩家验收；未授权的前台读档采集。

## 2. 已测结论（未另造数字）

### 标题 / 菜单

能稳住约 **60 fps**。限帧器仍有 pacer 睡眠（render-timing 均值 `sleep_requested` **6.85 ms**）。进程约 **1.2 核**（1.16–1.18）。最忙 TID 12716 在 15 s 窗口 **14.7 s CPU**，该线程样本 **100% 在 EXE（无符号）**。约 **114–119 draws/frame**，GPU **2 batches / 3.35 ms**，`fence_wait` **4.34 ms**。

菜单高功耗来自 60 Hz 下这条线程一直醒着干活并等 GPU，不是限帧失败。再睡久一点不能证明能降功耗。

### 城市走图

约 **31–44 fps**（无采样器心跳 42–44；render-timing 窗口 31.2）。每帧约 **1800–2300 draws**（计时均值 1833，心跳 1870–2300）。`draw_ms` **21.19 ms**，已超过 16.7 ms 预算：

| 分项 | ms | 计入 `tDraw` |
|---|---:|---|
| vertex | 4.08 | 是 |
| record | 2.91 | 是 |
| bind | 1.62 | 是（含嵌套 `tTexture`） |
| const | 0.70 | 是 |
| 其余 | ~12 | 是，先前未拆 |
| `fence_wait` | 15.40 | 独立计数；中途 Flush 时也嵌在 `tDraw` 里 |
| GPU batches | 13.21 / ~5.2 批 | 不含 Present |

`sleep_requested` **0**，`between` **18–25 ms**。进程约 **0.99 核**，并不比菜单占用百分比更高。掉帧是因为每帧 draw 录制和 GPU 队列 / fence 等待超过 16.7 ms。

不能把 `draw_ms` 与 `fence_wait_ms` 相加。不能点名单一 guest 函数。不能断言纯 GPU bound。

历史对照：source 0.5.0 在 Map16 / 4K TAA3 上做过 shader FNV 复用、顶点/索引准备和高精度限帧等待，当时固定视角约 59.8 fps。那是另一构建、另一分辨率，不能当作当前 v0.5.4 城市 1280×720 的验收。

## 3. 源码对照：未分项 ~12 ms 与 5 个 batch

### 3.1 每帧强制拆批

`DrawImpl` 在录制前检查描述符池、upload ring、vertex arena；任一不足就 `Flush()` 再 `Begin()`：

```2760:2773:LostOdysseyRecomp/gpu/renderer.cpp
                const bool poolsFull = setPoolUsed[1] >= kMaxSetsPerKind;
                const bool ringLow = uploadOffset + kUploadHeadroom > kUploadRingSize;
                const bool arenaLow = arenaOffset + kArenaHeadroom > kVertexArenaSize;
                if (poolsFull || ringLow || arenaLow)
                {
                    Flush();
                    Begin();
```

`kMaxSetsPerKind = 500`。每个 draw **无条件** `AcquireSet(1/2/3)`，三种各占 32 个 view。池满只看 kind 1，因此 500 次 draw 必拆一批。

城市 1833 draws / 500 ≈ **3.7** 次描述符拆批，加上帧末 `PM4_XE_SWAP` 的 Flush，与测到的 **~5.2 GPU batches** 同量级。标题 114 draws < 500，只需约 2 批（一趟录制 + 帧末），与测量一致。

Upload ring 96 MiB、headroom 24 MiB。每 draw 上传 VS+PS 常量各 256×vec4（8 KiB×2）再加 shared，约 16 KiB × 1800 ≈ **29 MiB/帧**，单独通常不够逼一次 ring Flush。Vertex arena 256 MiB 更不像每帧主因。**城市拆批的主开关是 500 套描述符上限，不是 GPU 硬件限制。**

plume 注释：shader-visible heap 65536 views。当前 500×3×32 = 48000。若未使用的 3D/cube 套改为静态 dummy，2D 一套 32 views，理论上可把单批容量抬到约 1800 量级（65536/32），一帧一次提交。

### 3.2 中途 fence 等待嵌在 `tDraw` 里

`Draw()` 整段包在 `tDraw` 中。`Flush()` 每次 `executeCommandLists` 后立刻 `waitForCommandFence`：

```1235:1271:LostOdysseyRecomp/gpu/renderer.cpp
            void Flush()
            {
                if (!listOpen)
                    return;
                ...
                queue->executeCommandLists(lists, 1, nullptr, 0, nullptr, 0, fence.get());
                {
                    ScopedTimer timer{ tFlush };
                    queue->waitForCommandFence(fence.get());
                }
                ...
                uploadOffset = 0;
                for (auto& used : setPoolUsed)
                    used = 0;
            }
```

D3D12 的 wait 是 `WaitForSingleObjectEx`。必须等，是因为只有一份 command list、一份 upload ring、一份描述符池，回收前 GPU 不能还在读。

若 5.2 批里约 4 次发生在 draw 中途，则嵌在 `tDraw` 里的 fence ≈ 4/5.2 × 15.40 ≈ **11.8 ms**，与未分项 **~12 ms** 对齐。这不是尚未计时的 vertex 内核，而是 **CPU 串行等 GPU 把当前批做完才能接着录下一批**。

`tSets` / `tIndex` / `tShader` / `tPipeline` / `tResolve` 以及 Draw 前半（RT 获取、pipeline key、TAA 分类）仍在 `tDraw` 里，但量级应远小于这 12 ms。`tIndex` 含 16 KiB 常量 memcpy 和索引/图元转换，先前减法未单列。

帧末还有第二次 CPU 等待：`PresentFrontbuffer` 在独立 command list 上 copy/scale 后再次 `g_queue->waitForCommandFence`（`video.cpp` 约 800–806 行）。同一 D3D12 queue 本来有序，这是又一次整帧排空。

### 3.3 计时器覆盖图

| 计时器 | 位点 | 含义 |
|---|---|---|
| `tDraw` | `Draw()` 整函数 | 含中途 Flush 等待、TAA、RT、shader/pipeline 查找 |
| `tConst` | 常量读 + viewport + jitter 分类，到 descriptor 之前 | 0.70 ms；`ReadRegisters` 已替代逐寄存器 |
| `tSets` | 三次 `AcquireSet` | 未从 21.19 中单列 |
| `tVertex` | fetch 槽 + `GetVertexBuffer` | 4.08 ms，已分项最大 CPU 块 |
| `tBind` | VS/PS 贴图；内含 `tTexture`、偶发 TAA `ResolveColor` | 1.62 ms |
| `tIndex` | 常量 **Upload** + 索引转换 + fan/quad 展开 | 未从 21.19 中单列 |
| `tRecord` | framebuffer / scissor / drawIndexed | 2.91 ms |
| `tFlush` | 仅 `waitForCommandFence` | 15.40 ms；中途部分与 `tDraw` 重叠 |
| `tShader` / `tPipeline` | 仅 cache miss | 走图稳态应接近 0 |
| `tResolve` | `modeControl==6` 的 copy | 计入 `tDraw`；GPU resolve 默认不 readback |

`render_timing.h` 写明 CPU 分段可重叠，GPU 只计 renderer queue batches，不含 Present / compositor。

### 3.4 菜单 1.2 核

已有 `poll_wait`：仅两个已知 guest 循环（`sub_823CF390` 查询、`sub_82322478` + `KeDelayExecutionThread(0)`）。菜单最忙线程 14.7 s/15 s 且 100% EXE，说明 **不是** `NtWaitForSingleObject`（那会落在 ntdll）。更像未覆盖的 guest 忙等，或渲染线程在 EXE 内干活。无 PDB 不能点名。标题仍提交 100+ draws 并每帧 fence，会维持唤醒，但解释不了 14.7 s 全 EXE。

## 4. 优化点（按证据强度）

建议落在已测拆分内，当作待验证方向。不把热点占比换算成可回收毫秒或 FPS。

### A. 减少城市每帧 GPU batch — 最强

**现象：** 标题 ~100 draws / 2 batches 能 60；城市 ~1800 / ~5 掉到 31–44。

**可修（源码级，不需要新游戏数字就能论证机制）：**

1. **不要每个 draw 都分配 3 套描述符。** 未用的 3D/cube 绑静态 dummy set（与 dummy 贴图同一语义）。heap 压力降到约 1/3。
2. **把 `kMaxSetsPerKind` 从 500 提高到 heap 实际能装下的 2D 套数**（静态 set0 + dummy 之后，2D 可接近 1800）。目标是城市一帧 1 次 submit。
3. **双缓冲 command list / upload ring / 描述符池。** Flush 只 submit；等到要复用某一槽时再 wait。中途 12 ms 的 CPU 空等可以和 GPU 重叠。TAA `ReleaseCompleted`、`retiredTextures`、`sceneAABusy` 必须挂到对应槽的 fence 上，不能提前回收。

**不要先做：** 改 FramePacer、加长睡眠、改 TAA/阴影特效来“降 GPU”。

未测：1800 次 guest PM4 本身是否含重复阴影 / TAA / resolve。那是 draw **数量** 问题，与 host 拆批是两件事。砍 guest draw 需要场景捕获，本轮没有。

### B. Draw CPU — 中等

`draw_ms` 21 ms 在扣掉嵌套 fence 后，剩余大约是 vertex 4.08 + record 2.91 + bind 1.62 + const 0.70 + index/sets/前半。若 A 去掉嵌套等待，CPU 录制可能落到 ~9 ms 量级（推断，不是新测量）。

仍值得做、但收益小于拆批：

- **常量脏检测 / 跳过未改 bank。** 每 draw 16 KiB memcpy × 1800 ≈ 29 MiB/帧，目前算在 `tIndex`。`tConst` 读寄存器只有 0.70 ms；上传更大。光材质用 c[253..255]，不能改成少传却不校验尾部。
- **顶点缓存。** 已有 sampled exact compare；4.08 ms 是已分项最大块。再优化需要 PDB 或更细 `GetVertexBuffer` 计时（命中 vs 拷贝 vs 校验）。
- **索引/quad/fan 转换** 已在 0.5.0 把宽度和字节序移出循环；再挖需要 `tIndex` 单独对照。

### C. 菜单功耗 — 证据不足以下手 guest

方向仍然对：减少标题 100+ draws、减少每帧 `waitForCommandFence`、让 guest 空闲真正让出 CPU。A 的双缓冲会减少渲染线程唤醒，但 14.7 s EXE 热点要匹配 PDB 或再钩一层已知忙等，不能猜 guest PC。

已有 `poll_wait` 不要无根据地扩到更多 PPC 函数。

### D. Present 路径 — 次要但真实

`video.cpp` 在 copy 到 swap chain 之后 CPU wait fence，再 `present`。同一 queue 上 GPU 已经有序。把 wait 挪到下一次复用 `g_commandList` 之前，能让 pacing 睡眠与 GPU copy 重叠。菜单已有 ~7 ms 睡眠，这里对功耗有意义；对城市帧率小于 A。

### E. 明确不要做

- 不要把 `NtWaitForSingleObject` / fence wait 当成纯 GPU 瓶颈去改特效。城市最忙线程是未解析 EXE + 等待 + AMD/D3D12 的混合。GPU batch 13 ms 不含 Present。
- 不要动限帧器来“救”城市 fps；`sleep_requested=0` 是结果。
- 不要无栈采样再跑一轮全线程 wall 快照；下一轮应是 Draw 细计时或同源 PDB。
- 不要声称本报告授权 60 fps 或降功耗已经发生。

## 5. 若实施，怎样才算打到 60

城市现在墙钟 ≈ CPU 录制（含中途等 GPU）超过 16.7 ms。机制上：

- 单批提交：CPU 不再在中途等，但帧末仍可能 `9 ms 录制 + 13 ms GPU wait` ≈ 22 ms（**推断**），仍可能不到 60。
- 跨帧双缓冲：录下一帧时 GPU 跑上一帧，墙钟 ≈ max(CPU, GPU) + present。GPU 13 ms、CPU ~9 ms 时有机会进入 16.7 ms 预算。Present 的第二次 CPU wait 也要挪走，否则仍串行。
- 若 13 ms GPU 里含批次之间的空转，合并提交后 GPU 墙钟可能低于 13 ms；这需要 `LO_RENDER_TIMING` 再测，不能用旧 5 批之和当新 GPU 时间。

菜单已 60 fps。功耗要降 1.2 核，主缺口仍是无符号 EXE 忙线程。

## 6. 验证边界

| 已有 | 没有 |
|---|---|
| 菜单 vs 城市的 fps / draws / batches / 分段 ms | 匹配 PDB 的函数名 |
| Flush 拆批与 500-set 上限的源码对应 | 城市 1800 draws 的 pass 构成（阴影/TAA/resolve） |
| 中途 wait 嵌套进 `tDraw` 的机制解释 | 实施后的 A/B；本轮未改代码、未跑游戏 |
| 0.5.0 Map16 4K 历史修复（另一 EXE） | 瓦特、ETW、cycles、逐 pass GPU、玩家验收 |

工具：`LO_RENDER_TIMING`（每帧 CPU 分段 + GPU batch）、`LO_GPU_STATS`（约每 60 帧汇总）、`LO_FRAME_TIMING`（1 秒聚合）、`tools/asm-profiler`（wall 快照，含等待）。合成 fixture 只证明采集通路。

后台测试不得抢焦点；非音频实验用测试静音。前台读档采集需要单独安排。
