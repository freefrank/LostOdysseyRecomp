# GPU 环缓冲实测对比 — 2026-09-11

`perf-gpu-ring` 相对已发布 v0.5.4 的城市走图计时。这是诊断采集，不是玩家验收或新 Release。

## 身份

| 项 | v0.5.4 基线 | 本轮 |
|---|---|---|
| EXE | 已发布，SHA-256 `3c3b4073f1b7abbcce38747dc08763335ac019edf560df849177176d0399f949` | RelWithDebInfo clang 增量链接，`5917F389F9FD9E88FDEC6DBD3437ADE76D415F1653FB6924575ACCF478C1B9AD` |
| 场景 | user01 城市走图 `xenon_scr.fpd`，1280×720 D3D12 TAA=3 cap 60 | 同：隔离副本读 user01（Lv.10），截图确认 Uhra 街道行走 |
| 采集 | `perf` 索引 / asm-profiler gameplay | `LO_RENDER_TIMING` + `LO_FRAME_TIMING` + `LO_GPU_STATS`，后台 `LO_BACKGROUND=1` `LO_AUDIO_MUTE=1` |
| 输入 | — | `LO_AUTO_BUTTONS=s@120,a@240,a@360,a@480,a@700,a@900`（Continue，不按 Down） |
| 窗口 | 城市稳定段 | 1664 帧，swap 1367–3030，丢弃读档后前 60 个高 draw 帧 |
| 原始安装存档 | — | 未改时间戳：user00 `2026-09-10 18:22:37`，user01 `18:28:28` |

本轮日志：`out/perf-ring/run/logs/runtime-1789157012865755.log`。启动 `descriptor reuse=true backend=D3D12 limit=1800 gpu_slots=2`。截图 `out/perf-ring/run/shots2/`（swap 1816 / 2210 为城市行走）。

基线数字抄自 [perf-complete-analysis](perf-complete-analysis.md)，不是同一次进程、也不是同一份 EXE。分辨率、后端、存档槽位对齐；不能当成 A/B 实验室对照。

## 城市走图

| 指标 | v0.5.4 | 本轮均值（min–max） |
|---|---:|---:|
| 帧率 | 31–44（计时窗 31.2） | **57.7**（49.1–60.0，swap≥1400 的 1 s 心跳） |
| draws | 1833（1800–2300） | 1816（1729–2368） |
| GPU batches | ~5.2 | **2.00**（1–4） |
| `descriptor_splits` | ~3.7（500 套上限） | **0** |
| `draw_ms` | 21.19 | **9.82**（6.59–78.89） |
| vertex_ms | 4.08 | 1.36 |
| record_ms | 2.91 | 2.59 |
| bind_ms | 1.62 | 1.99 |
| constants_ms | 0.70 | 0.63 |
| `fence_wait_ms` | 15.40 | **1.67**（0.001–11.72） |
| gpu_queue_ms | 13.21 | **3.91**（1.29–10.38） |
| `sleep_requested` | 0 | 约 0–3.9 ms（多数秒有 pacer 睡眠） |
| descriptor hits / misses | — | 5289 / 160 |
| upload / arena splits | — | 0 / ~0 |

Draw 数同量级，所以收益来自提交模型，不是场景变简单。`kMaxSetsPerKind` 从 500 提到 1800，加上 2D 描述符复用，1816 draws 不再中途拆批；2-slot `GpuSlot` 在回收下一槽时才 `waitForCommandFence`，Present 也不再立刻等第二道 fence。

`draw_ms` 与 `fence_wait_ms` 仍不可相加。`draw_ms` 里曾经嵌着中途 Flush 的等待，所以 21→9.8 与 15.4→1.7 描述的是同一类停顿被拆掉，不是两笔独立回收。

## 标题 / 菜单（同一次进程，swap 850–969）

| 指标 | v0.5.4 | 本轮 |
|---|---:|---:|
| draws | 114–119 | 104 |
| batches | 2 | 2 |
| `draw_ms` | — | 0.91 |
| `fence_wait_ms` | 4.34 | **0.002** |
| gpu_queue_ms | 3.35 | 1.48 |

菜单本来就不拆批。fence 从 4.34 ms 掉到几乎为零，符合「提交后不等当前槽、只等上一帧槽」：菜单 GPU 很短，等旧槽时它已经做完。

## 不能从这次数字得出的结论

- 不是 60 fps 验收。心跳均值 57.7，最低 49；`draw_ms` 仍有到 79 ms 的尖峰。
- 不是功耗结论。没有瓦特 / ETW。菜单线程是否还 1.2 核醒着，这次没采。
- 不是与 ReBlue bindless 的对比。本轮是 1800 套 + 2D key 复用，不是 64K bindless。
- 顶点 4.08→1.36 没有单独对照实验，不能全部记在环缓冲账上（缓存已热、截图回读不在稳定段）。
- bind 略升（1.62→1.99）可能是复用查找；未单独测。
- 未打包、未覆盖 Vulkan。环缓冲已提交 `b91d279`；dummy/last-hit/常量复用尚未提交。

## 第二轮：静态 dummy + last-hit + 常量跳过（本 EXE）

EXE SHA-256 `02E303F1462546FB98236446E24B2397DF762179923DE1D7C02852317ED37BC4`（86804480 bytes）。日志 `runtime-1789158471640626.log`。同一 Continue 序列、同一隔离 user01、截图仍是 Uhra 街道行走。稳定段 1639 帧，swap 1387–3025。原始安装存档时间戳仍未改。

| 指标 | v0.5.4 | 环缓冲（5917F389） | 本轮（02E303F1） |
|---|---:|---:|---:|
| 帧率均值（swap≥1400） | 31–44 | 57.7（49.1–60.0） | 56.0（28.0–60.0） |
| draws | 1833 | 1816 | 1804 |
| batches | ~5.2 | 2.00 | 2.00 |
| splits | ~3.7 | 0 | 0 |
| draw_ms | 21.19 | 9.82 | 10.52 |
| bind_ms | 1.62 | 1.99 | **1.58** |
| vertex_ms | 4.08 | 1.36 | 1.46 |
| record_ms | 2.91 | 2.59 | 2.80 |
| fence_wait_ms | 15.40 | 1.67 | 2.09 |
| gpu_queue_ms | 13.21 | 3.91 | 4.04 |
| descriptor hits/misses | — | 5289 / 160 | **837 / 156** |

hits 从 5289 降到 837：未用的 3D/cube（以及无 2D 槽的 draw）不再进 hashmap，改绑静态 dummy。misses 仍约 156，说明 2D 独特组合数没变。`bind_ms` 回到略低于 v0.5.4 的 1.58。

城市 fps / `draw_ms` 没有比环缓冲那一轮更好（均值还略差，最低心跳 28）。不能把 dummy/常量复用记成帧率收益；两轮也不是实验室 A/B（走位、截图回读、机器状态不同）。常量跳过作用在 `tIndex` 的 Upload，不表现在 `constants_ms`（那是寄存器读取）。


## 与实现的对应

`LostOdysseyRecomp/gpu/renderer.cpp`：`GpuSlot gpuSlots[2]`，`Flush` 只 `executeCommandLists` 并切槽；`RecycleSlot` 在 `Begin` 里等即将复用的那一槽。`render_batch_policy.h` D3D12 上限 1800。`texture_descriptor_cache.h` 按 32 槽纹理 key 复用。`video.cpp` Present 后不立刻 `WaitForPresentGpu`。
