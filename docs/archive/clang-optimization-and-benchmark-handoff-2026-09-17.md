# LORecomp：Clang 优化与测试基准交接文档 (2026-09-17)

> **历史归档。** 本交接记录于 2026-09-17，保留当时的设备基线、构建产物与基准卡点诊断。它不是当前系统状态或执行指令；乌斯拉城后续实测与交付结果以 [乌斯拉城 CPU 优化实测报告](../notes/PERF_CITY_UHRA_RESULTS.zh-CN.md) 与 [STATUS](../STATUS.md) 为准。

本文档面向接手 LORecomp 优化计划后续基准测试与 PR 05–09 推进的 Agent。文档汇总当前设备状态、已完成产物、实测诊断证据与下一步具体操作路径。

---

## 1. 设备环境与硬件基线

- **测试目标设备**：`freefrank@psvita`
- **CPU / GPU**：AMD Ryzen AI Max+ 395 (Strix Halo，16 核 32 线程，Zen 5) + AMD Radeon 8060S Graphics (RADV STRIX_HALO)
- **操作系统**：Bazzite 44 (Linux 6.13.5-ba44.bazzite.fc41.x86_64)
- **图形后端**：Vulkan 1.3 / RADV 26.2.2（全局统一默认 Vulkan，不使用 D3D12/D3D11）
- **TDP 功耗限制**：严格锁定为 **15W**
  - 设置命令：`sudo ryzenadj -a 15000 -b 15000 -c 15000`
  - 状态核实：`sudo ryzenadj -i | grep -i -E 'stapm|fast|slow'` 确认 STAPM、FAST PPT、SLOW PPT 均为 15.000W
- **编译工具链容器**：`distrobox enter psbuild` (Clang 22.1.8, LLD 22.1.8, Ninja 1.12.1, CMake 3.31.6)
- **项目路径**：
  - 源码仓库：`/home/freefrank/src/LostOdysseyRecomp`
  - 预构建着色器包：`/home/freefrank/build/lo/shaders/portable_vk.lospv`
  - 游戏光盘根目录：`/var/home/freefrank/Games/LO/`（`disc1`）
  - 测试运行工作区：`/home/freefrank/perf/lo/run/`
  - 原始安全存档源（只读，切勿覆盖）：`/home/freefrank/Downloads/save`
    - 包含 `user00` (14:15:09) 与 `user01` (14:29:50，乌斯拉城街道进度)

---

## 2. 当前推进进度与已交付产物

| 阶段 / PR | 交付内容 | 当前状态 | 关键产物 / 验证依据 |
|---|---|---|---|
| **PR 01** | 构建配置开关支持 | 已完成 | [cmake/LoOptimization.cmake](../../cmake/LoOptimization.cmake) 整合至根构建配置 |
| **PR 02** | 线程命名与计时设施 | 已完成 | [LostOdysseyRecomp/os/thread_name.h](../../LostOdysseyRecomp/os/thread_name.h)；12+ 个线程入口命名（`/proc/<pid>/task/*/comm` 验证生效）；`render_timing` 打通 `swap` 帧序号 |
| **P0** | 基线环境全量留存 | 已完成 | 硬件、OS、内核、Mesa、LLVM 等标识保存于环境记录文件中 |
| **PR 03** | 优化矩阵全量构建 | 已完成 | 全部 5 组二进制构建完毕（见下方表），段大小缩减趋势符合预期 |
| **PR 04** | IR-PGO 构建与编译 | 已完成 | 生成采样 Profile：`/home/freefrank/perf/lo/pgo/game.profdata`；完成 C4 PGO Use 构建 |
| **测试准备** | 自动化测试脚手架 | 进行中 | 脚本 [tools/run_city_bench.sh](../../tools/run_city_bench.sh) 已部署至目标机，正调试读档进城逻辑 |

### 已完成构建矩阵二进制汇总

| 变体代号 | 配置参数 | 可执行文件绝对路径 | .text 段大小 | 相比 C0 变化 |
|---|---|---|---:|---:|
| **C0** | sandybridge, O2 | `/home/freefrank/build/lo/c0/LostOdysseyRecomp/LostOdysseyRecomp` | 60,682,514 B | 基准 (0) |
| **C1** | sandybridge, O3 | `/home/freefrank/build/lo/c1/LostOdysseyRecomp/LostOdysseyRecomp` | 61,364,402 B | +681 KB (+1.12%) |
| **C2a** | znver2, O2 | `/home/freefrank/build/lo/c2a/LostOdysseyRecomp/LostOdysseyRecomp` | 59,782,546 B | -899 KB (-1.48%) |
| **C2b** | znver2, O3 | `/home/freefrank/build/lo/c2b/LostOdysseyRecomp/LostOdysseyRecomp` | 60,419,794 B | -262 KB (-0.43%) |
| **C3** | znver2, O2, ThinLTO | `/home/freefrank/build/lo/c3/LostOdysseyRecomp/LostOdysseyRecomp` | 58,926,120 B | -1.75 MB (-2.89%) |
| **C4** | znver2, O2, ThinLTO, PGO Use | `/home/freefrank/build/lo/c4/LostOdysseyRecomp/LostOdysseyRecomp` | **58,160,061 B** | **-2.52 MB (-4.16%)** |

---

## 3. Benchmark 核心卡点诊断与取证分析

### 3.1 现象与问题
在远程机上执行 `run_city_bench.sh` 时，程序稳定保持在 ~29.2–30 FPS，`draws=194` 或 `draws=210`，未成功进入乌斯拉城（Uhra City 漫游的标准 draws 范围在 800–1200+）。

### 3.2 诊断证据
1. **帧率锁 30 FPS 的成因**：
   - 游戏菜单阶段 Xbox 360 原生主线程逻辑本身锁定在 30 FPS，停留在菜单或选择界面时日志心跳均报告 29.2–30 FPS。只有成功读档进入乌斯拉城街道，渲染帧率才会放开至 60 FPS 上限。
2. **存档识别与目录结构**：
   - 游戏通过 `DiscoverSavedContent()` 扫描当前工作目录下的 `save/` 文件夹。
   - `save/user00` 和 `save/user01` 必须保留真实的时间戳（`user01` 晚于 `user00`，代表最新进度）。复制时必须使用 `cp -a`。若使用普通 `cp` 会导致二者修改时间相同。
   - 工作目录 `~/perf/lo/run/save` 当前已正确恢复 `cp -a /home/freefrank/Downloads/save ~/perf/lo/run/save`。
3. **抓帧（PPM/PNG）揭示的时序错位**：
   - 抓取第 250 帧无按键画面：标题画面在此阶段仍在渐显动画中，菜单项未完全展开，无光标激活。
   - 抓取第 350 帧画面：菜单中出现 “New Game”、“Continue” 和 “A Thousand Years of Dreams”，但未呈现激活光标。
   - 抓取第 400 帧画面：由于之前固定帧脚本 `LO_AUTO_BUTTONS="s@120,a@240,a@360,a@480,a@700,a@900"` 在菜单尚未响应时过早发送按键，误触发了“设置”界面（`Settings`），导致后续按键均在设置界面内空转，无法触发读档。

---

## 4. 参考实现与推荐解决方案

### 4.1 历史成熟脚本机制
参考工程内的成熟测试脚本：
- `out/perf-ring/drive-city.ps1`
- `docs/notes/city-60fps-handoff.md`
- `.omo/cpu-perf/drive-city-3c6t.ps1`

成熟脚本的核心机制**不是纯静态固定帧按键**，而是配合动态状态机：
1. 启动时注入基础按键引导（或针对 Linux 调整按键间隔，例如拉开 Start 与 A 键的时间差，等待菜单完全加载）。
2. 后台持续读取 `runtime-*.log` 的尾部：
   - 监听 `draws` 计数；
   - 一旦连续 3 帧满足 `draws >= 800`，判定为成功进城（`phase = 'city_hold'`）；
   - 进城后立即通过 `Send-Input` 或 `LO_AUTO_STICK=0,28000,1400,2800` 注入手柄摇杆让凯姆在街道漫游；
   - 保持 28 秒连续稳定采集后主动退出进程，规避进程卡死在菜单。

### 4.2 替代快捷方案（Python 驱动）
在目标机 `freefrank@psvita` 上，编写一个轻量 Python 驱动脚本 `drive_city.py`，复刻 `drive-city.ps1` 的核心逻辑：
- 启动子进程：`LostOdysseyRecomp --game /var/home/freefrank/Games/LO/`
- 环境变量：
  ```bash
  export DISPLAY=:0
  export WAYLAND_DISPLAY=wayland-0
  export XDG_RUNTIME_DIR=/run/user/1000
  export XAUTHORITY=/run/user/1000/.mutter-Xwaylandauth.IN3BV3
  export LO_BACKGROUND=1
  export LO_AUDIO_MUTE=1
  export LO_RENDER_TIMING=1
  export LO_GPU_STATS=1
  export LO_FRAME_TIMING=1
  export LO_AUTO_STICK="0,28000,1400,2800"
  ```
- 调整自动按键帧数以适配 Linux 启动延迟：通过试验校准能够稳定进入 `user01` 存档的序列（例如适当延后第 1 次与第 2 次按键间隔）。
- 轮询 tail 日志：当 `draws >= 800` 连续 3 帧时计时 28 秒，随后发送 `SIGINT` 或调用 `terminate()` 正常收尾，并直接提取帧时间统计。

---

## 5. 接手 Agent 的后续任务清单

1. **打通乌斯拉城自动化进城**：
   - 使用 Python 或改进的 Bash 驱动，使测试程序稳定进入乌斯拉城，达到 `draws >= 800`。
2. **在 15W TDP 下执行 C0 vs C4 对照基准测试**：
   - 运行 C0 二进制，采集 28 秒乌斯拉城街道数据（提取 mean FPS, 1% low FPS, draw_ms, vertex_ms, record_ms）。
   - 运行 C4 二进制，采集相同路线数据并产出对照表。
3. **推进 PR 05（热点消除与计算冗余削减）**：
   - 使用 `perf record -F 499 -e cycles:u` 分析 C4 运行下的主要 CPU 热点（`DrawImpl` 内部流程、描述符判定等）。
4. **推进 PR 06–08（线程解耦与多 Worker 扩展）**：
   - 串行 Prepare/Commit 抽象、1-worker 有界并行队列、2-worker 扩展与调度。
5. **推进 PR 09（BOLT 优化与全量交付）**：
   - 二进制布局重排、全量多变体验证交付。
