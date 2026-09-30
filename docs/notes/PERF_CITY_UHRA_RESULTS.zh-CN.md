# 乌斯拉城 CPU 优化实测报告 — 2026-09-18

方法、基线、两次交付的优化（索引转换缓存、BOLT），以及所有数字背后的方差说明。

环境与构建标识、跑分流程见交接文档
`docs/archive/clang-optimization-and-benchmark-handoff-2026-09-17.md`。
英文版：`PERF_CITY_UHRA_RESULTS.md`。

## 1. 方法（引用数字前先读）

- 场景：乌斯拉住宅区行走，`tools/drive_city.py` 自动化（菜单序列
  `s@300,d@600,a@900,a@1200,a@1500,a@1800`，`draws>=800` 判定进城，
  120 秒开环摇杆行走，双跑同路线）。
- 主机：Ryzen AI Max+ 395（Strix Halo），RADV，Bazzite 44，
  **15W TDP 锁定**（`ryzenadj -a/-b/-c 15000`），分辨率改 `run/settings.ini`
 （游戏读运行目录的 ini，`~/.config` 那份会被忽略）。
- 指标：逐帧 `draw_ms`（`Renderer::Draw` 的 CPU 墙钟，帧内所有 draw 累加，
  约 7000 城市场景帧取均值）。
- **方差**：开环行走每跑略有分叉（卡墙点、视角不同）。同二进制重复跑差异
  **±9%**（12.706 对 13.784），各段（vertex/bind/record）同向摆动。
  基线均为单跑，5% 以内的差值只看方向，不看精确值。

## 2. 编译器基线（720p 与 1080p，15W）

C0 = sandybridge/O2 基准，C4 = znver2/O2/ThinLTO/PGO（`game.profdata`）。

| | 720p C0 | 720p C4 | 1080p C0 | 1080p C4 |
|---|---|---|---|---|
| draws 均值 | 1759 | 1760 | ~1760 | ~1760 |
| draw_ms | 9.448 | **8.906（-5.7%）** | 13.084 | 13.588 |
| vertex_ms | 1.202 | **0.966（-19.6%）** | 1.759 | 1.670 |

1080p 下帧时间由 GPU 主导，`draw_ms` 区分不出 CPU 构建差异。
720p 下 CPU 在关键路径上，C4 全面领先。

## 3. PR05 热点（C4，1080p，25 秒，24120 个 `cycles:u` 采样，0 丢失）

- 线程：GPU CmdProc **51%**，两条 Guest 线程 42%，其余约 7%。
- `DrawImpl` 内：索引转换 `Convert<false,1U>` **5.6%**，顶点缓冲内容比对
 （`EqualSampleBlock64` + `memcmp`）约 3.7%，寄存器快照拷贝 1.1%，
  `memmove` 2.3%。
- 管线/RenderTarget 三处 `unordered_map` 查找合计约 3.2%，
  `steady_clock::now` 约 0.7%，未解析 vdso（时钟）约 4.8%。

结论：一半以上 CPU 周期是 CmdProc 线程上 PM4→Vulkan 不可避免的翻译税，
但约 10 个点是每 draw 重复交的税（重转、重比对、重查找），缓存吃得掉。

## 4. 索引转换缓存（`renderer.cpp`、`vertex_cache.h`）

360 的索引缓冲是大端（16/32 位），Vulkan 要小端 32 位，所以每个带索引的
draw 都要全量字节交换+拓宽。缓存以
`(indexBase, count, primitiveType, wide, endian)` 为键存最终展开后的索引，
用 `SampledContent` 源采样校验（与顶点缓存同等保证）。

- **v1 诚实失败**：所有索引 draw 都进 `ankerl` 表，哈希平庸还配了
  `is_avalanching`，内容变化时 erase+重插。结果 +13%
 （`probe_simd` 13%，`erase` 4.8%）——查找比被干掉的 5.6% 转换还贵。
  教训：小缓冲上，固定查找开销永远赢不了紧凑循环。
- **v2**：只缓存 `count >= 256`（`IndexCache::kMinCount`），去掉
  `is_avalanching`，内容变化**原地刷新**（无 erase/emplace 抖动）。
  profile 确认两个病灶消除，`Convert` 归零，1283 项零逐出，99.98% 命中。
- **必须重训 PGO**：新代码使 `DrawImpl`/`GetVertexBuffer` 的旧 profile 失效；
  重训（`c4-gen` → 进城训练 → `game-new.profdata` → 重编 `c4`）后零 PGO 告警。

结果（15W）：

| | 720p | 1080p |
|---|---|---|
| C4 基线 | 8.906 | 13.588 |
| v2 + 新 PGO | 7.978 / 8.721（均值 8.35） | 12.505 |
| 变化 | **-6.2%** | **-8.0%** |

单测：`tools/tests/vertex_cache_test.cpp` 新增索引键参与度
（prim/width/endian）、源变更命中/失效、有界抖动（357 万检查，
Windows + Linux 全过）。所有基准跑存档无改动。

## 5. BOLT（PR09）

- 工具链：LLVM 22.1.8 `llvm-bolt` 解到 `~/bin/llvm-bolt-22`
 （该内核分支采样 `-j any,u` 可用；`perf2bolt` 需要 `perf` 在 PATH，
  用 shim 解决）。
- 仓库开关 `LO_BOLT_READY=ON` 加 `emit-relocs` + 行号表，保持开启，
  以后每次 `c4` 重编都自带 BOLT 就绪（基线代价约中性）。
- Profile：60 秒进城行走，452K 分支样本 → `bolt-profile.fdata`
 （3236/70532 函数，4.6% 覆盖；工具提示样本可再加 6 倍）。
  `llvm-bolt`（ext-tsp+hfsort+split）：taken branches -18.8%，输出 104MB。

结果（1080p，15W）：

| 二进制 | draw_ms |
|---|---|
| relocs 对照（同代码，无 BOLT） | 13.368 / 14.13 |
| **BOLT** | **12.436 / 12.509（±0.6%）** |

相对同二进制对照 -7~-9%。附带：跑间方差从 ±5% 收窄到 ±0.6%
（n=2，只记录，不展开 claim）。

## 6. 复现

1. 锁 15W：`sudo ryzenadj -a 15000 -b 15000 -c 15000`，
   `ryzenadj --info` 确认。
2. 分辨率：改 `run/settings.ini`（`internal_resolution`），日志确认
   `effective=WxH`（`~/.config` 那份会被忽略）。
3. 跑分：`~/perf/lo` 下 `python3 tools/drive_city.py --variant c4
   --deadline 400`，须包在 `systemd-run --user` unit 里
  （ssh 退出会回收前台进程）；看 `drive-status.json` +
   `drive-summary-c4.json`。
4. PGO：编 `c4-gen`，进城训练，`llvm-profdata merge`，
   替换 `game.profdata`，重编 `c4`。
5. BOLT：`perf record -j any,u` 采样，`perf2bolt`，
   `llvm-bolt`（ext-tsp+hfsort+split）；运行时目录须有可执行文件旁的
   `libdxcompiler.so` 软链。

产物（远端 `freefrank@psvita`）：`~/build/lo/c4-bolt/`（BOLT 二进制）、
`~/bolt-profile.fdata`、`~/bin/llvm-bolt-22/`、`~/perf-city-c4*.data`
（上文 profile 的原始 perf 捕获）。
