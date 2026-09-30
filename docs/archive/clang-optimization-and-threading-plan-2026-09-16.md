# LORecomp：Clang优化、线程拆分与对照测试指南

> **历史归档。** 本指南记录于 2026-09-16，为早期 Clang 优化、线程拆分与对照测试方案提案。它不是当前系统状态或执行指令；方案中未实施的优化提案不构成支持承诺，乌斯拉城实测基线与交付结果以 [乌斯拉城 CPU 优化实测报告](../notes/PERF_CITY_UHRA_RESULTS.zh-CN.md) 与 [STATUS](../STATUS.md) 为准。

日期：2026-09-16  
目标：Steam Deck原生Linux/Vulkan，1280×720内部渲染，APU功耗限制15W，争取稳定60FPS。  
核对快照：`freefrank/LostOdysseyRecomp`，`main`，`38550e3ffb7dd518b5ac78324b82932a8990c79a`。该快照的CMake源码版本为0.5.14，不等于发布包身份。[1]  
范围：只使用Clang/LLVM；共享运行时代码保留Windows/clang-cl回归。  
状态：开发与验证方案。已核对公开源码及官方文档；没有在本次工作中编译游戏、运行Steam Deck基准或实现这些优化。所有新增开关、阈值、任务接口均为提案。

## 1. 路线与判断原则

建议按下面的依赖顺序推进：

```text
冻结源码、工具链、游戏输入与测试环境
→ 识别当前关键线程的实际工作与等待
→ O2/O3、Zen 2目标、ThinLTO对照
→ IR-PGO训练、独立场景验证
→ 删除重复工作、优化被测热点
→ 串行Prepare/Commit接口
→ 有界并行：0 / 1 / 2个新增worker
→ 必要时评估guest热点替换或Vulkan并行录制
→ 代码稳定后重训PGO，最后评估BOLT与调度策略
```

目标是缩短决定帧率的关键路径，不以“吃满4核”作为验收指标。一个线程计算繁重、一个线程忙等、GPU背压，都可能伴随总CPU使用量不高；需要用时间线区分。单独的CPU占用截图不足以定因。

60FPS的帧间隔预算为约16.67ms。不同帧、不同线程和GPU阶段可以重叠，不能把各阶段的inclusive时间直接相加当作帧时间。给Game、guest Render、host GPU worker及GPU提交建立相互关联的frame/epoch标识，再分析吞吐与端到端延迟。

本指南的执行优先级是建议，不包含任何预先承诺的性能提升百分比。

## 2. 已确认的项目约束

| 项目现状 | 对开发计划的影响 |
|---|---|
| runtime和recompiled library已要求Clang | 不开展其他编译器迁移。[1][2] |
| 根CMake全局添加`-march=sandybridge` | 必须将其参数化；不能只在环境变量中追加`-march=znver2`后假定生效。[1] |
| `linux-clang` preset使用`RelWithDebInfo` | 不把配置名字等同于最终优化等级；查看实际编译、链接命令。[3] |
| runtime和PPC库保留`-fno-strict-aliasing`；PPC库另外关闭异常和RTTI | 保留这些边界，不将PPC库的选项全局复制给runtime。[2][4] |
| Linux直接编译生成的PPC源码 | 禁止复用Windows的预编译`.lib`；实验同时覆盖runtime和PPC库。[2][5] |
| guest线程通过`GuestThreadHandle`创建host线程 | 已有并发，不需要从零实现“六线程Xbox调度器”。[6] |
| renderer已有两槽GPU资源环；`Flush()`提交后切槽，`RecycleSlot()`等待复用 | 不把“去掉每次Flush立即等待”列为尚未实施的新优化；也不能删除资源回收所需fence。[7] |
| `geometry_prepare.h`已有SSE2比较、SSSE3换序和索引转换 | 先测剩余成本，不能把这些项目按未优化状态估算收益。[8] |
| 文档已记录shader/pipeline启动并行准备及XMA线程 | 新增帧内worker必须与现有工作共享CPU预算，不能无界叠加。[9] |

历史Card C在v0.5.11的一个Windows城市场景记录：vertex复制约0.0091ms、shader lookup约0.051ms、pipeline lookup约0.030ms，未批准新帧内并行路径。它提示“任务可能太小”，但不能代替本次Steam Deck/Linux测量，也不能证明所有场景都不值得并行。[9]

另一个必须保留的正确性边界：`SampledContent`对大于8192字节的缓冲区使用抽样比较，未覆盖的改动可能被漏掉。不得将其当作精确dirty tracking，也不得以“抽样一致”证明多线程输入安全。[8]

## 3. P0：先建立可重复的性能基线

### 3.1 冻结身份与环境

每份结果必须记录：

- 源码SHA、dirty diff、submodule SHA、游戏输入与PPC生成manifest哈希、二进制SHA-256及ELF build-id。
- Clang、LLD、llvm-ar、llvm-ranlib、llvm-profdata的完整版本；CMake、Ninja版本；实际编译及最终链接命令。
- Deck型号、SteamOS/kernel、Mesa/Vulkan驱动、运行方式、内部渲染分辨率、AA、VSync、刷新率、帧率限制、画质、shader cache状态。
- APU功耗上限、CPU/GPU时钟策略、SMT/affinity、温度和后台任务状态；电池充电状态固定。

Deck的官方CPU规格是Zen 2、4核8线程，APU功耗范围4–15W。[10] 15W应在报告中指明是APU限制，不能拿整机电池功耗或插座功耗混为一谈。无可靠功耗计数器时填`N/A`，不伪造每帧能耗。

使用固定sysroot/SDK构建，保持C++标准库及其他依赖版本不变。Clang不要求同时迁移到libc++；本轮不要把标准库变更混入比较。[11] WSL可做构建和可运行性检查，不作为Deck帧率验收设备。


### 3.1A 实际目标机与目录约定

本计划的最终性能验收设备固定为：

```text
SSH:       freefrank@psvita
OS:        Bazzite
Power:     PowerStation 管理
Target:    Steam Deck / Zen 2 / Vulkan
Game data: ~/Downloads/game
Save data: ~/Downloads/save
Dev env:   优先复用现有 distrobox 开发环境
```

所有最终性能数据必须来自 `freefrank@psvita`。本地PC、WSL、CI或其他Linux机器只用于构建、静态分析和功能预检查，不能替代目标机验收。

连接后第一步记录实际环境，不根据上述描述猜测具体Bazzite版本、kernel、Mesa、PowerStation配置或distrobox名称：

```bash
ssh freefrank@psvita

uname -a
cat /etc/os-release
clang --version || true
ld.lld --version || true
vulkaninfo --summary 2>/dev/null || true
lscpu
```

同时保存PowerStation当前实际设置，包括APU/TDP限制、CPU频率/boost策略、GPU频率上下限、SMT状态、是否有手动core parking/affinity、Gamescope刷新率与FPS限制，以及任何额外性能profile。

PowerStation配置必须在同一组A/B测试中保持完全一致。若需要比较不同PowerStation参数，必须单独建立实验ID，不允许与Clang、PGO或线程数量变更混在同一对照里。

### 3.1B distrobox开发环境

优先使用目标机上已有的distrobox环境，不重新搭建另一套工具链。先枚举现有container：

```bash
distrobox list
```

选择已经包含本项目所需Clang/LLVM、CMake、Ninja、Python和开发依赖的container，并记录其名称。进入后再次记录：

```bash
distrobox enter <DEVBOX>

clang --version
clang++ --version
ld.lld --version
llvm-profdata --version
cmake --version
ninja --version
python3 --version
```

建议在测试记录中增加：

```text
host_os=
host_kernel=
distrobox_name=
container_image=
clang_version=
lld_version=
mesa_version=
vulkan_driver=
powerstation_profile=
```

代码checkout、build目录和profile数据建议放在用户home下，保证host与distrobox均可直接访问，例如：

```text
~/src/LostOdysseyRecomp
~/build/lo/<variant>
~/perf/lo/<date>/<variant>
```

不要把构建输出、PGO raw profile或perf数据写进 `~/Downloads/game` 和 `~/Downloads/save`。

### 3.1C 游戏资源和存档隔离

`~/Downloads/game`视为游戏资源源目录；`~/Downloads/save`视为用户真实存档源目录。性能自动化不得直接修改这两个目录。

每次测试建立独立工作副本。例如：

```bash
RUN_ROOT="$HOME/perf/lo/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$RUN_ROOT"

cp -a "$HOME/Downloads/save" "$RUN_ROOT/save-baseline"
```

游戏资源如果运行时本身不写入，可直接只读引用；若程序会创建缓存或修改资源目录，则先建立独立测试副本或使用项目支持的资源路径机制。禁止为了方便让测试脚本覆盖真实存档。

测试前后至少记录：

```bash
find "$HOME/Downloads/save" -type f -print0 | sort -z | xargs -0 sha256sum > "$RUN_ROOT/save-before.sha256"
# test...
find "$HOME/Downloads/save" -type f -print0 | sort -z | xargs -0 sha256sum > "$RUN_ROOT/save-after.sha256"
diff -u "$RUN_ROOT/save-before.sha256" "$RUN_ROOT/save-after.sha256"
```

预期结果是原始 `~/Downloads/save` 完全不变。若项目需要写存档，必须让运行实例使用测试副本。


### 3.2 测试场景

先选4–6段可重复路线，包含高draw城市场景、人物/动画较多场景、重粒子战斗、室内或遮挡明显场景、菜单及场景切换。另保留至少2–3段未用于PGO训练的路线。

每条路线固定起始存档、镜头、输入、画质与采样区间。存档使用隔离副本。首次shader编译、加载和稳定运行分别统计，不能将首次运行卡顿混入或从稳定运行中随意剔除。

建议初筛每场景3次；候选验收至少5组配对运行，按ABBA或随机顺序交错，预热至温度/频率稳定，每段有效采样120–180秒。时间是测试协议建议，不是性能事实。路线无法确定性复现时记录其限制，增加重复次数。

### 3.3 必须测的指标

| 层级 | 指标 | 回答的问题 |
|---|---|---|
| 显示结果 | 实际呈现帧间隔p50/p95/p99、慢帧比例、平均FPS、1% low | 是否真正改善流畅度？ |
| 关键线程 | Game、guest Render、host GPU worker的CPU时间、on-CPU热点、runnable等待、锁/事件等待 | 忙核在执行有效工作还是等待？ |
| 帧内准备 | snapshot、转换、缓存验证、排队、worker执行、join、ordered commit耗时 | 拆分后是否真正缩短关键路径？ |
| GPU | GPU执行耗时、fence等待、提交批次、frame/slot标识 | GPU是否才是限制，或出现资源背压？ |
| CPU计数器 | cycles、instructions、branch misses；可用时的I-cache/TLB等事件 | 是指令量、分支还是代码/数据局部性问题？ |
| 资源 | RSS、临时arena峰值、队列深度、复制字节、每帧总CPU时间、功耗/频率 | 是否以更多功耗和尾延迟换平均FPS？ |

计时使用线程本地缓冲，测试结束批量输出，避免每draw写日志或全局锁。GPU时间戳异步读取，不为了测量每帧加入`vkDeviceWaitIdle`。

`perf`示例，针对已经进入测量场景的进程；运行前设置`PID`、创建输出目录并确认权限：

```bash
perf list

perf stat -p "$PID" \
  -e task-clock,context-switches,cpu-migrations,page-faults \
  -o run-software.txt -- sleep 120

# 在独立、相同路线的运行中采集，避免与重型采样同时污染基准。
perf stat -p "$PID" \
  -e cycles,instructions,branches,branch-misses \
  -o run-hardware.txt -- sleep 120

perf record -F 499 -e cycles:u --call-graph dwarf \
  -p "$PID" -o run-cpu.data -- sleep 120
perf report -i run-cpu.data
```

硬件事件随CPU、kernel和权限而变化；`not supported`、采样丢失、计数器复用都要记录。采样只解释on-CPU热点，不能替代off-CPU/调度时间线；另开诊断运行采集sched事件或应用等待区间。[12]

保留两类构建：接近发布配置的性能构建，以及带更完整符号/必要frame pointer的诊断构建。后者的绝对帧率不直接用作发布收益；验证插桩及profiler自身开销。

### 3.4 决策分支

| 主要证据 | 优先动作 |
|---|---|
| guest Render有效计算占主导 | PGO、重编译热点/原生函数替换；不要先并行一个很短的host转换阶段。 |
| host `DrawImpl`内转换/验证占主导 | 减少重复工作，再验证纯CPU Prepare切片。 |
| command recording占主导 | 先去掉重复绑定；必要时评估独立池的并行录制。 |
| 锁、poll或fence主导 | 分清CPU自旋、线程阻塞和GPU背压；针对等待依赖优化。 |
| GPU本身超预算 | CPU优化不是充分条件；另立GPU质量/渲染优化实验，不混入编译器收益。 |

## 4. P1：整理Clang构建配置

### 4.1 建议新增的配置接口

下列为待开发选项，当前仓库没有这些优化开关；先实现、验证，再使用它们生成实验构建。

| 新选项 | 建议值 | 作用 |
|---|---|---|
| `LO_TARGET_ARCH` | `sandybridge` / `znver2` | 替换当前硬编码的architecture选项。 |
| `LO_OPT_LEVEL` | `2` / `3` | 显式控制项目代码优化等级。 |
| `LO_LTO_MODE` | `off` / `thin` / `full` | 首选测试ThinLTO，full留为后续对照。 |
| `LO_PGO_MODE` | `off` / `generate` / `use` | 互斥PGO状态。 |
| `LO_PGO_FILE` | 绝对路径 | use阶段的profile，缺失立即报错。 |
| `LO_BOLT_READY` | `OFF` / `ON` | 为独立BOLT实验保留relocations。 |
| `LO_PROFILE_DIAGNOSTIC` | `OFF` / `ON` | 诊断符号/frame pointer，不与发布成绩混用。 |

建议实现于`cmake/LoOptimization.cmake`，由根CMake初始化，再对runtime、实际PPC编译target和最终链接生效。根据热点决定是否将plume纳入LTO/PGO，不把预编译DXC或系统库算作已优化代码。

清理选项来源，避免根目录、target和`CMAKE_*_FLAGS_*`分别追加相互冲突的`-O`、`-march`、`-flto`。导出`compile_commands.json`，抽查runtime、至少一个PPC shard、需要参与优化的依赖以及最终链接命令。仅检查`CMakeCache.txt`不够。

Linux实验显式保持`LO_PREBUILT_PPC_DIR`为空；已有`LO_PPC_AUTO_SYNC`关闭，避免实验误触发上传流程。[5] 本指南不要求修改原有Windows预编译库发布契约。

### 4.2 编译选项策略

待测的核心组合：

```text
-O2 或 -O3
-march=znver2
-fno-strict-aliasing
-gline-tables-only
-flto=thin                         # 只在ThinLTO实验启用
```

最终链接使用对应版本Clang驱动和LLD；ThinLTO的`-flto=thin`同时出现在编译与最终链接。可配置独立的ThinLTO cache及`--thinlto-jobs=N`，控制构建资源，不能将其与游戏运行时worker数混淆。[13]

`-O3`可能生成更大的代码，不能直接认定比`-O2`快。[11] 生成代码量大也不能单独证明I-cache是瓶颈；要用热点、代码尺寸和可用计数器验证。后续可增加“runtime O3、PPC O2”的混合实验，检查LTO后的实际机器码。

`-march=znver2`会改变允许生成的指令，Deck专用产物必须单独标识，不覆盖原Sandy Bridge兼容包。不要在开发用的新CPU上以`-march=native`生成Deck发布包。`-mtune=znver2`通常无需在同名`-march`之外重复指定；需要隔离调度调优与ISA变化时，另建独立实验。

### 4.3 第一轮禁止混入的选项

不全局启用`-Ofast`、`-ffast-math`、盲目循环展开或激进inline阈值；不删除现有alias保护；不全局关闭异常/RTTI；不随意隐藏所有符号或使用`--icf=all`。

FP处理必须保留PPC/VMX语义。对guest代码中非融合运算，可在独立正确性实验中评估`-ffp-contract=off`；guest自身的FMA指令仍需显式保持融合语义。先核查生成代码及helper，再统一政策，不能把“所有FMA都关闭”当作准确性方案。NaN、±0、非规格化数、舍入模式与FPSCR相关路径都需测试。[14]

runtime涉及weak/strong替换及间接guest函数映射的地方要做链接后验证：保留预期hook，确认调用没有被错误地固定到未替换实现。LTO或更高优化暴露问题时，检查UB和链接语义；先回退，不能用“更快但偶尔坏”验收。

## 5. P2：编译器优化对照矩阵

所有行使用同一源码、工具链、依赖及画质。下表描述实验设计，不代表已经生成了这些构建。

| ID | 配置 | 对照目的 |
|---|---|---|
| R0 | 当前真实构建，完整记录实际flags | 保存用户现状。 |
| C0 | Clang O2、Sandy Bridge、无LTO/PGO、统一LLD | 建立显式、可复现控制组；与R0差异单独披露。 |
| C1 | C0仅改O3 | O2与O3比较。 |
| C2a | C0仅改znver2 | O2下目标CPU收益。 |
| C2b | C1仅改znver2 | O3下目标CPU收益及交互。 |
| C3 | 前述最佳正确配置加ThinLTO | 跨模块优化是否有实际收益。 |
| C4 | C3加IR-PGO | profile带来的增量。 |
| C5（可选） | 对C4的O等级或target组合单独变更并重训 | 检查PGO后最佳组合是否改变。 |
| C6（后置） | 最佳配置中ThinLTO改Full LTO | 收益是否值得额外链接成本；不预设胜负。 |

若某项退化，后续建立在最佳已验证配置上，不因“优化选项更多”保留退化项。每次保存构建时间、链接峰值内存、`.text`尺寸和运行结果；构建速度属于开发效率，单独报告。

## 6. P3：IR-PGO训练与回归

Clang提供IR instrumentation PGO；本轮使用`-fprofile-generate`和`-fprofile-use`，不引入另一套采样PGO链路。多线程训练启用atomic计数，代价是训练版本开销更高。训练数据必须覆盖代表性玩法，不能只停在标题画面。[14]

### 6.1 三阶段流程

```text
阶段A：编译和最终链接增加
    -fprofile-generate
编译阶段增加
    -fprofile-update=atomic

阶段B：在目标设备运行训练版，正常退出，合并.profraw

阶段C：重新干净构建，移除generate和训练专用选项，增加
    -fprofile-use=/absolute/path/game.profdata
```

generate/use阶段使用相同源码和目标配置，保持PPC库与runtime覆盖一致。不要使用已编译的非PGO静态库冒充完整PGO构建。工具链固定，记录profile来源及哈希。

运行/合并示例（Bash；`BIN`指向已完成构建和资源部署的训练版，不包含游戏特定启动参数）：

```bash
set -euo pipefail
: "${BIN:?Set BIN to the instrumented executable}"
PROFILE_DIR="$(pwd)/out/pgo/train"
SCENE="city-route-a"
mkdir -p "$PROFILE_DIR"
export LLVM_PROFILE_FILE="$PROFILE_DIR/${SCENE}-%p-%m.profraw"
"$BIN"

# 重复运行其他训练路线，并改变SCENE。
# 正常退出后检查profile，不能将强制杀进程视作成功采集。
shopt -s nullglob
raw=("$PROFILE_DIR"/*.profraw)
((${#raw[@]} > 0)) || { echo "No profile data produced" >&2; exit 1; }
llvm-profdata merge -o "$PROFILE_DIR/game.profdata" "${raw[@]}"
llvm-profdata show "$PROFILE_DIR/game.profdata"
```

训练构建可能改变运行速度和线程调度，不拿它的FPS与发布版比较。只有重新编译后的use构建参与性能A/B。

### 6.2 训练集与验收集分离

训练覆盖高draw、战斗、动画、资源流式读取、菜单和切换路径，控制各段占比。验收使用未参与训练的存档/镜头/战斗，另外检查首次启动、失败重试、设备重建及退出路径。

profile mismatch、关键热点缺失或大量函数过期应阻止接受该实验；局部未覆盖的冷函数允许存在，但必须登记。线程拆分、主要生成代码或工具链发生变化后重新采集PGO，不能长久沿用旧profile。

## 7. P4：先减少工作，再考虑并行

### 7.1 缓存与数据有效性

优先记录“每帧重复次数×单次成本”，检查重复常量比较、buffer转换、状态构造和绑定。使用能完整覆盖依赖的cache key；不要只按guest地址命中，因为同地址可被覆写或复用。

建议先对明确拥有写入入口的资源做generation/版本跟踪；全局dirty page是后续高风险实验。完整设计需覆盖guest CPU写入、host导入/解压写入、GPU resolve/MEMEXPORT等写回、别名映射、跨页数据以及资源释放重用。漏任一写入路径都会产生旧数据。

抽样hash或`SampledContent`不能替代这一契约。即便读前后版本号相同，也不能自动使无同步的普通C++内存读写成为合法并发。需要明确的只读阶段、所有权转移、锁或经过证明的内存协议。

验收增加专门用例：改动未抽样字节、相同地址新资源、跨页写入、保存/读档后旧缓存、GPU写回后CPU使用。减少检查不能以增加漏检换FPS。

### 7.2 SIMD与生成代码热点

`geometry_prepare.h`已经有显式128-bit intrinsics；换成znver2不会自动把所有手写128-bit操作变成256-bit。[8] 仅对实测热点比较现有路径与AVX2实现，覆盖尾部、非对齐、长度0、端序和合法边界，测整帧而非只测微基准。

对被证明确实昂贵的guest叶子函数，可评估原生HLE替换：输入/输出、guest内存副作用、返回寄存器、condition/status状态和调用约定均明确后，建立原实现与候选实现的差分测试。优先考虑纯数学/转换例程，不先动UE3对象生命周期、全局容器或完整`FSceneRenderer::Render`。

改动放在维护的hook、helper或生成器层，不能只修改会被重新生成覆盖的`ppc_recomp.*.cpp`。利用Clang optimization remarks核查实际inline/vectorize情况，不通过随意增加`always_inline`推进。[15]

## 8. P5：线程拆分设计

### 8.1 先证明可并行且值得并行

进入实现前同时满足：

1. 任务在目标设备当前关键路径上，累计成本超过可测噪声；建议初筛门槛为每帧约0.5ms以上，具体按采样调整。
2. 输入在任务期间稳定，且任务可独立执行；输出独占，不修改guest可变状态或renderer全局状态。
3. 把snapshot、排队、等待及回写计入后仍有空间；不会创建“主线程立刻join、worker串行做原工作”的假并行。
4. 有串行后备、取消/退出契约、数据生命周期和差分验证。

0.5ms是本计划的筛选建议，不是项目实测值。不满足时记录原因，转向减少工作或更重要的热点。

### 8.2 第一步仅抽出串行接口

建议新增维护的`prepare_jobs.h/.cpp`与测试夹具，先保持所有工作在原线程执行：

```text
CaptureInputs → PrepareCPU → CommitOrdered
```

这个阶段的目标是定义边界，不求并行收益。若抽象本身明显增加复制/分配或关键路径时间，先改接口。

`CaptureInputs`必须在输入有效的同步点完成，捕获实际需要的寄存器、常量、格式以及资源内容/引用；不能因为复制了几个指针就宣称拥有snapshot。GPU命令流中的写内存、query、resolve、barrier及其他可观察副作用构成边界，不能跨越后再倒序补交。

### 8.3 第二步增加有界worker

```text
host命令解析/拥有者线程：保持原有顺序
    → 建立只读任务批次和独占输出槽
    → worker准备任务A、B……；拥有者执行其他独立工作
    → 按原packet序号依次Commit
    → 原有GPU提交与fence管理
```

上图是逻辑流程，不代表每个方框新增一个线程，也不自动改变guest Render线程的负载。

初版最多新增1个worker；验证后再比较2个worker。3个以上仅在新证据支持时实验，不把Game、guest Render、host GPU、音频和XMA当成没有成本的背景线程。

建议队列同时限制任务数与输入字节数，任务批量提交，使用预分配输出/arena。开始用容易验证的有界队列，避免“一draw一次`std::async`”。空闲时阻塞，worker数量与启动shader/pipeline准备共享预算，不能都按`hardware_concurrency()-1`各自占满。

初始可以按实测50–250µs级任务寻找粒度，再测更粗/更细；这些只是实验点。细小任务留在原线程执行。

### 8.4 严格的数据与生命周期契约

| 对象/动作 | 契约 |
|---|---|
| 输入 | 持有不可变数据或明确的只读生命周期；不得读取同时被guest写入的普通内存。 |
| 输出 | 每个任务独占输出槽；workers不resize共享容器、不并发写全局cache。 |
| 身份 | `epoch + packet sequence + resource generation`，拒绝过期结果。 |
| Commit | 由现有拥有者按原顺序提交；workers不碰guest寄存器、TLS、MMIO、全局descriptor/upload ring。 |
| 内存池 | 预分配；pool/slot在CPU任务与引用它的GPU工作都完成后才复用。 |
| 队列满 | 背压或在满足所有权的前提下由原线程执行尚未开始的任务；不能静默丢工作。 |
| 异常/失败 | 将失败状态发布给拥有者，保留正确的串行/失败处理；不能吞掉错误提交半成品。 |
| 取消 | epoch失效只防旧结果被提交，不代表输入/输出可以立即释放；等相关任务结束后回收。 |

首个版本只让worker做纯CPU准备，使用CPU staging输出，由拥有者处理上传和资源分配。多一次copy的成本计入实验，若不划算就拒绝该方案。进一步让worker写映射upload memory必须另立设计，证明范围不重叠、可见性和fence生命周期，不直接复用共享offset。

### 8.5 必须防止的死锁与重复提交

拥有者等待结果时不能持有worker需要的queue/cache/resource锁。worker不能同步调用拥有者处理RPC，也不能把子任务提交给同一耗尽的线程池后全部阻塞等待。

结果用明确的同步发布（mutex/condition variable或经过验证的release/acquire）；`volatile`不提供线程同步。唤醒必须带状态谓词，覆盖通知先于等待、虚假唤醒、停止和队列关闭。

退出顺序建议为：停止接收新任务→使待提交结果失效→唤醒等待者→结束/取消任务并join→等待相关GPU资源可回收→释放池和资源。禁止detach后让线程继续访问已销毁对象。

不能在等待超时后直接“再串行做一遍”而不管理原任务。fallback只能接管未开始的任务，或使用彼此隔离的输出并以一次性状态决定唯一提交者；已经运行的任务必须完成或安全取消。

### 8.6 两条后置、高风险路线

**guest Render拆分。** 当热点确实位于guest计算时，需要另做原生HLE边界/只读数据导出。不能把`PPCContext`、guest TLS、UE3全局状态或整个渲染函数复制给线程池后期待正确。先证明特定数学/可见性任务的无副作用与可分割性。

**Vulkan并行录制。** 仅在host command recording被证实昂贵、纯CPU准备无法提供足够收益时评估。每worker/在途slot拥有独立command pool及必要descriptor资源；合并顺序、barrier和资源状态由设计明确。Vulkan的command/descriptor pool有外部同步要求，不能让多个线程同时使用同一池。[16] 是否能通过现有plume接口实现，需要额外审查；本指南未确认其并行录制支持。

真实遮挡反馈可另立专项减少draw，但不能把“未返回query”当不可见，也不能随机丢draw追求帧率。它改变了工作量，收益独立于Clang/线程实验统计。

## 9. 线程与调度对照矩阵

先固定最佳正确编译配置，以`PGO=off`完成线程结构初筛，避免旧profile偏向旧结构。决赛时给串行和并行版本分别按同一训练协议重训PGO，再比较最终产物。

| ID | 变更 | 验证内容 |
|---|---|---|
| T0 | 原始串行路径 | 对照组。 |
| T1 | 新Prepare/Commit接口，0 worker | 抽象/额外复制本身的开销。 |
| T2 | T1 + 1 worker | 是否获得真实重叠。 |
| T3 | T1 + 2 workers | 是否值得增加争用与功耗。 |
| T4 | 最佳worker数，逐项调整批大小/阈值 | 减少排队与尾部join，不同时改变多个参数。 |
| T5 | 最佳结构 + 独立重训PGO | 与同样重训的串行候选做最终对照。 |
| A1（后置） | 默认OS调度对照topology-aware affinity | 是否改善迁移、争用与尾延迟。 |
| A2（独立） | SMT开启/关闭 | 仅实测，不预设关闭SMT更快。 |

默认保留OS调度，不写死CPU编号。需要affinity时先识别物理核/SMT拓扑和进程允许的CPU集合；不同logical CPU可能是同一物理核的siblings。绑定策略、GPU频率上限和电源策略各做独立实验。

APU 15W限制保持不变，记录时钟变化。若并行导致总CPU时间上升却缩短关键路径，可能仍值得；若挤压GPU而拖慢实际呈现，应拒绝。CPU占用升高本身不算收益。

## 10. P6：代码稳定后评估BOLT

BOLT是链接后的代码布局优化器，是可选后置实验，不能替代PGO和热点分析。准备未剥离符号的ELF并在最终链接加入`-Wl,--emit-relocs`；审查函数地址映射、weak alias、间接跳转及任何依赖代码布局的机制。[17]

对最终候选重新采样，保持输入二进制SHA与profile对应。不要假定Deck支持branch-stack采样；无此能力时可使用普通cycles采样及对应版本的basic-events模式，profile信息较少。[17]

示例（变量需预先设置，`BASE`是保留符号及relocations的候选ELF，`PID`是它的运行进程）：

```bash
perf record -e cycles:u -F 499 -p "$PID" \
  -o bolt.data -- sleep 180

# 当前上游文档使用-ba；先核对固定版本perf2bolt的--help。
perf2bolt -ba -p bolt.data -o bolt.fdata "$BASE"

llvm-bolt "$BASE" -o "$OUT" -data=bolt.fdata \
  -reorder-blocks=ext-tsp -reorder-functions=cdsort \
  -update-debug-sections -dyno-stats
```

首轮仅重排，cold splitting等选项另做实验。BOLT处理后的二进制重新跑完整正确性与性能回归，保留原ELF及新符号信息；重新计算包manifest/SHA，不能使用原地址映射解释新二进制崩溃。收益未超过噪声时不增加发布流程复杂度。

## 11. 正确性、性能门槛与回退

### 11.1 正确性是硬门槛

编译优化检查启动、导入、菜单、读档、战斗、场景切换、动画、音频、窗口/设备重建、失败处理和退出。weak/strong hooks、MMIO副作用及间接guest调用建立专门回归。

线程改动增加测试：随机worker延迟、队列满、结果逆序完成、资源复用、取消后旧结果到达、重复关闭、读档/设备重建时在途任务、连续启动退出和故障注入。检查不丢失、不重复提交，GPU slot不提前复用。

比较串行/并行Prepare的完整输出；允许不同的GPU资源地址或offset时做语义归一化，不直接用裸指针做digest。画面比较固定镜头并排除已知非确定性，验证粒子、透明、遮挡、UI和后处理；截图相似不能替代内存与顺序测试。

ASan/UBSan与TSan使用独立构建，TSan先覆盖可隔离的worker/queue/ownership夹具。TSan有显著开销且不支持所有静态链接方式；完整游戏还需检查其shadow memory与guest映射兼容性。测试未报告竞争不等于已证明线程安全。[18][19] sanitizer构建不参与性能打分，GPU对象同步另用Vulkan验证检查。

共享CPU代码须通过Windows/clang-cl与Linux/Clang构建及相关测试；Vulkan线程资源更改还需Linux运行验证，涉及共同renderer逻辑时加入Windows/D3D12回归。不要为此擅自改变Windows的预编译发布配置。

### 11.2 建议采用的性能接受门槛

以下是建议预先固定的实验门槛，可根据基线噪声调整；它们不是已达成结果。

- 主要受限场景：关键路径改善至少`max(3%, 0.3ms)`，且配对重复运行支持真实收益；或实际慢帧显著减少。
- 其他必测场景：p95/p99没有可重复的明显退化；可暂将超过3%设为需阻止合并并调查的阈值。
- 60FPS已封顶的场景：看关键路径余量、慢帧和每帧成本，不要求平均FPS继续上升。
- 资源和稳定性：不得出现新的错误、死锁、音频欠载、无界内存或队列增长；15W目标条件不变。

分别报告每次运行的分位数和配对差异；以“运行”为重复单位，避免把相邻帧当成大量独立样本。可以计算配对bootstrap区间，收益不能区分于噪声时标记为“未证实”。

“稳定720p60”应另设整体验收：固定内部720p及画质，目标设备热稳态长测；定义实际呈现慢帧容差及比例，并单列加载/首次编译。建议CPU/GPU各自关键工作保留约1–2ms余量，但不把这个建议写成已满足的保证。


### 11.2A 最终验收标准（固定）

以下标准用于决定某项Clang优化、PGO、线程拆分或调度改动是否可以进入默认Steam Deck/Linux路径。

#### A. 正确性：必须全部通过

任何一项失败都直接 `REVERT / REJECT`，不允许用性能收益抵消：

1. 游戏可从目标机正常启动并进入可玩状态。
2. 固定测试存档可正常载入。
3. 战斗、菜单、场景切换、动画、音频和退出路径无新回归。
4. 连续运行至少30分钟无崩溃、死锁或线程永久挂起。
5. 连续执行至少10次启动→读档→运行→退出循环，无随机hang。
6. 原始 `~/Downloads/save` 哈希前后一致。
7. worker路径不得出现丢任务、重复commit、过期epoch提交或GPU资源提前复用。
8. 若改动涉及线程同步，相关queue/worker夹具必须通过TSan或等价竞争检查；完整游戏无法使用TSan时必须记录原因，并以故障注入+长期运行补充。
9. Vulkan validation在可用的诊断构建中不得新增同步/生命周期错误。
10. Windows共享代码仍需通过clang-cl构建和相关测试；Steam Deck专用`znver2`产物不能破坏通用Windows产物。

#### B. 性能：目标场景

最终目标是目标机 `freefrank@psvita`、Bazzite、Vulkan、内部1280×720、APU 15W条件下稳定60FPS。

对每个候选使用相同PowerStation配置、相同存档、相同路线、相同画质和相同shader-cache状态，至少进行5组交错A/B运行。每组有效采样不少于120秒。

候选满足以下任一“主要收益条件”，才有资格KEEP：

- CPU受限场景关键线程关键路径缩短至少 `max(3%, 0.3ms)`；
- 或p95 frame time改善至少3%；
- 或p99 frame time改善至少3%且慢帧比例明显下降；
- 或原本低于60FPS的固定路线达到并维持60FPS，同时GPU/CPU功耗条件不变。

同时必须满足全部“无退化条件”：

- 所有主要测试路线p95不得可重复退化超过3%；
- p99不得可重复退化超过5%；
- 1% low不得可重复下降超过3%；
- 每帧总CPU时间不得无解释地增加超过10%；
- RSS不得持续增长；10分钟稳定区间内不得出现无界队列或内存爬升；
- 不得通过提高TDP、提高GPU频率上限、降低分辨率或降低画质取得收益。

若结果处于噪声范围，标记 `INCONCLUSIVE`，不合并为默认优化。

#### C. 60FPS整机验收

当最终候选声称“Steam Deck 720p60”时，额外执行长测：

```text
设备：freefrank@psvita
系统：Bazzite
渲染：native Linux + Vulkan
内部分辨率：1280×720
APU限制：15W
PowerStation：固定并记录
时长：每条主要路线至少10分钟
```

建议整机门槛：

- 稳态平均FPS ≥ 59.0；
- p95 frame time ≤ 16.67ms；
- p99 frame time ≤ 20.0ms；
- >20ms帧比例 < 1%；
- 不允许持续性的30FPS/40FPS节奏锁定；
- 不允许音频爆音/欠载、输入明显延迟增长、温控降频造成后半程退化。

加载画面、首次shader编译和明确的场景切换可单独统计，不和稳定游玩区间混为一谈，但必须单独报告其峰值和频率。

如果某个真实重场景无法满足上述60FPS门槛，不得把项目整体标记为“稳定720p60”；应写明具体失败场景及当前数据。

#### D. Clang构建优化验收

每个编译优化阶段单独验收：

| 阶段 | KEEP条件 |
|---|---|
| `-O3` | 相对O2有稳定收益，且代码尺寸/尾延迟没有抵消收益 |
| `-march=znver2` | Deck上有稳定收益；通用包仍保留兼容target |
| ThinLTO | 全量链接正确，目标路线有可重复收益，构建成本可接受 |
| IR-PGO | 未参与训练的验证路线仍有收益，无严重profile mismatch |
| BOLT | 在PGO最终候选上仍提供额外可重复收益，否则不进入发布链 |

不得因为理论上“应该更快”而保留任何一项。

#### E. 线程拆分验收

T1/T2/T3必须逐级通过，不能直接跳到多worker：

```text
T0 原始路径
→ T1 Prepare/Commit抽象，0 worker
→ T2 1 worker
→ T3 2 workers
```

要求：

- T1相对T0关键路径退化 <1%；否则先优化抽象。
- T2必须证明存在实际重叠，而不是dispatch后立即join。
- T2/T3的 `snapshot + queue + join + commit` 总开销必须低于被转移工作的收益。
- worker数量增加后，p95/p99不能恶化。
- 如果T3相对T2收益 <1%或落入噪声，则默认使用T2，不为“更多核利用率”保留第二worker。
- 若并行后平均FPS提高但p99、1% low或音频稳定性下降，则判失败。
- 线程拆分最终必须与重新训练后的PGO版本比较，不能直接复用串行结构训练出的profile作为最终结论。

#### F. 自动判定标签

每次实验报告最后必须明确写一个：

```text
KEEP          = 正确性全部通过，性能达到预设门槛
REVERT        = 出现正确性/稳定性问题，或明确性能退化
INCONCLUSIVE  = 差异在噪声范围，证据不足
RETEST        = 环境、PowerStation、cache、温度或路线不一致导致结果无效
```

禁止使用“看起来更流畅”“CPU占用更高”“理论上更快”作为KEEP依据。


### 11.3 回退原则

编译选项有清晰开关；线程路径保留串行实现；缓存优化保留正确验证路径。任何数据竞争、显示错误、丢任务、退出挂起或无法解释的尾延迟回归，先回退该项。

不通过降低分辨率、关特效、换更大功耗或更改场景掩盖回归。必须改变这些条件时另开实验，不能归功于Clang或线程拆分。

## 12. 建议PR与交付物

| PR | 内容 | 合并条件 |
|---|---|---|
| 01 | 构建开关、flags审计、工具链及产物manifest | 不改变默认行为；每个选项实际生效且可回退。 |
| 02 | 线程命名、frame/epoch关联、低开销计时与测试协议 | 计时开销已量化，完成Deck基线。 |
| 03 | O2/O3、znver2、ThinLTO实验 | 正确性通过，按矩阵提交结果。 |
| 04 | IR-PGO训练/合并/use流程 | 独立验收集、profile身份完整、无关键mismatch。 |
| 05 | 一个被测热点的去重/缓存/SIMD或原生替换 | 一次只改一类工作，差分测试和整帧收益成立。 |
| 06 | 串行Prepare/Commit边界及夹具 | 所有权成立，抽象开销可接受。 |
| 07 | 有界1/2 worker及故障注入 | 真正重叠、无竞争/死锁、尾延迟不过线。 |
| 08 | 对串行/并行决赛版本分别重训PGO | 公平对照后选择最终配置。 |
| 09（可选） | BOLT、affinity或更高风险渲染拆分 | 各自独立实验与回归，不捆绑提交。 |

每个PR随附：假设、源码身份、基线/候选配置、原始日志、逐场景结果、正确性结果、负面结果及回退方式。即使“没有收益”也保留报告，避免下一轮重复做相同无效优化。

建议结果CSV表头：

```csv
run_id,variant,source_sha,binary_sha256,profile_sha256,scene,repeat,internal_resolution,power_limit_w,workers,avg_fps,frame_p50_ms,frame_p95_ms,frame_p99_ms,one_percent_low_fps,slow_frame_rate,guest_render_cpu_ms,host_gpu_cpu_ms,snapshot_ms,prepare_ms,queue_ms,join_ms,commit_ms,gpu_ms,fence_wait_ms,cpu_ms_per_frame,rss_mb,correctness,notes
```

明确`1% low`的计算方式，例如取最慢1%帧的平均帧时间后计算`1000/平均ms`；同时保留p99，不能混用不同工具的定义。未知指标填`N/A`，不要填0。

## 13. 可交给开发代理的任务摘要

> 基于固定的LORecomp源码快照，只做Clang/Linux优化。最终目标机固定为`ssh freefrank@psvita`，Bazzite + PowerStation，游戏资源位于`~/Downloads/game`，真实存档位于`~/Downloads/save`且不得被测试直接修改；优先复用现有distrobox开发环境。先记录host/distrobox/Clang/LLD/Mesa/Vulkan/PowerStation实际身份，再添加可回退构建开关、审计最终flags并建立15W/内部720p/Vulkan基线。依次比较O2/O3、znver2、ThinLTO、IR-PGO，不预设收益。保留现有alias/FP/hook契约，不使用Windows预编译PPC库。先依据目标设备profile减少重复工作，再将一个明确独立且足够昂贵的阶段提取为串行Prepare/Commit接口。只在所有权、同步、取消和输出等价验证完成后启用有界1/2 worker；不直接并行完整guest renderer或共享GPU状态。每项修改独立提交，必须按KEEP/REVERT/INCONCLUSIVE/RETEST标准验收。最终“720p60”声明只允许来自`freefrank@psvita`上固定PowerStation配置、15W、Vulkan、长时间实测。结构稳定后分别重训PGO，再单独评估BOLT。没有运行得到的结果一律标记未验证，不填预计FPS或伪造基准。

## 参考资料

以下仓库链接固定到本次核对的commit；官方文档为本次访问的上游页面。仓库历史测量仅代表文档标注的版本与场景。

[1]: https://github.com/freefrank/LostOdysseyRecomp/blob/38550e3ffb7dd518b5ac78324b82932a8990c79a/CMakeLists.txt "根CMake及源码版本"
[2]: https://github.com/freefrank/LostOdysseyRecomp/blob/38550e3ffb7dd518b5ac78324b82932a8990c79a/LostOdysseyRecompLib/CMakeLists.txt "PPC库编译与预编译限制"
[3]: https://github.com/freefrank/LostOdysseyRecomp/blob/38550e3ffb7dd518b5ac78324b82932a8990c79a/CMakePresets.json "Linux preset"
[4]: https://github.com/freefrank/LostOdysseyRecomp/blob/38550e3ffb7dd518b5ac78324b82932a8990c79a/LostOdysseyRecomp/CMakeLists.txt "runtime编译配置"
[5]: https://github.com/freefrank/LostOdysseyRecomp/blob/38550e3ffb7dd518b5ac78324b82932a8990c79a/docs/BUILDING.md "构建、生成代码、预编译及auto-sync"
[6]: https://github.com/freefrank/LostOdysseyRecomp/blob/38550e3ffb7dd518b5ac78324b82932a8990c79a/LostOdysseyRecomp/cpu/guest_thread.cpp "guest线程映射"
[7]: https://github.com/freefrank/LostOdysseyRecomp/blob/38550e3ffb7dd518b5ac78324b82932a8990c79a/LostOdysseyRecomp/gpu/renderer.cpp#L1503-L1620 "GPU资源环与提交"
[8]: https://github.com/freefrank/LostOdysseyRecomp/blob/38550e3ffb7dd518b5ac78324b82932a8990c79a/LostOdysseyRecomp/gpu/geometry_prepare.h "已有SIMD、索引转换及抽样比较"
[9]: https://github.com/freefrank/LostOdysseyRecomp/blob/38550e3ffb7dd518b5ac78324b82932a8990c79a/docs/notes/cpu-card-c-prepare-gate-2026-09-14.md "历史准备阶段测量及边界"
[10]: https://www.steamdeck.com/en/tech "Valve：Steam Deck规格"
[11]: https://clang.llvm.org/docs/CommandGuide/clang.html "Clang选项、优化等级及标准库"
[12]: https://perfwiki.github.io/main/tutorial/ "Linux perf官方项目教程"
[13]: https://clang.llvm.org/docs/ThinLTO.html "ThinLTO使用与链接资源控制"
[14]: https://clang.llvm.org/docs/UsersManual.html "Clang：PGO、浮点及相关选项"
[15]: https://llvm.org/docs/Remarks.html "LLVM optimization remarks"
[16]: https://docs.vulkan.org/guide/latest/threading.html "Khronos：Vulkan线程及资源池"
[17]: https://github.com/llvm/llvm-project/blob/main/bolt/README.md "LLVM BOLT上游指南"
[18]: https://clang.llvm.org/docs/ThreadSanitizer.html "ThreadSanitizer能力及限制"
[19]: https://clang.llvm.org/docs/AddressSanitizer.html "AddressSanitizer"
