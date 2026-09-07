# 启动着色器准备（2026-09-05）

## 2026-09-07 续：亚洲／美欧联合元数据（本地未发布）

内置裸 FPD 与 CPX 表已合并两版资源元数据，继续使用既有资源身份匹配代码，无需运行时区域选项。CPX 通过完整小型 FPI 摘要、FPD 名称／大小及 extent 位置选择布局；裸 FPD 使用既有名称／大小／采样签名匹配。联合表包含 104 个 archive 布局、12,857 个 CPX profile 与 275,186 个位置；原亚洲 CPX profile 保留。两生成器支持可重复的 `--additional-root`，维护者用不同根目录合并元数据，不改变导入器禁止单套安装混装版本的规则。

两版各 52／52 个布局匹配、52 个裸资源文件索引命中且零扫描；CPX 亚洲 10,060／美欧 10,198 个索引命中，均零回退。每版 20,686 个来源的名称／逐 SHA256 全等于此前保留的 strict 完整扫描基准，集合摘要为 `57cb834795fd99419f0a1980f86e38c7123317eed3d627576269f61fd03055fc`。从 Disc 2 入口自动发现同组兄弟盘，复用同一资源清单。两版读取分别为 142,093,208／142,289,816 字节，美欧版原为 20,770,329,949 字节；观测 12.537／12.631 秒可能与构建重叠，不作受控提速比例、完整启动或 FPS 结论。

元数据审计确认旧 10,060 个 CPX profile 和旧裸资源内容全部保留；裸 FPD 联合表有 48 个完整资源身份 profile、40 个不同的有界查找 key，不能把 profile 数当作每版文件数。`shader-index` fixture 和主构建通过，证据为 `out/v0.4.0-followup/cpx-both-editions/{comparison.json,layout-audit.json,metadata-audit.json,fixtures.log}`。新 ZIP `ed8e0627…`／EXE `c75946a3…` 的 45 项 manifest 及安装器 self-test 已通过。两版均已实际运行包内 EXE，live 模块核实包内 DXC／DXIL；Map2 Auto 1080p／AA3 有效尺寸和已审阅静止截图均为 1920×1080，无分配回退。亚洲／美欧实际发现记录 13.482／13.367 秒，各 52 个文件索引命中、零裸扫描及 CPX 回退；每版实际 20,686 个期望来源哈希与完整扫描基准一致。7 项用户文件、任务 seed 和 EXE 保持，两个任务进程均已结束。完整身份和 `runtime-summary.json` 见[联合索引交付记录](../../out/v0.4.0-followup/cpx-both-editions/DELIVERY.md)。本次不扩大原报告战斗、全游戏画质或玩家验收范围。未知布局／内容与提取失败回退、未读内容边界及显式 strict 完整扫描合同保持。生成入口见[测试说明](../../tools/tests/README.md#regenerating-resource-metadata)。

## 2026-09-07 续：按已知布局直接提取（本地未发布）

本节保留 `79308a15…` 包和此前扫描器的证据；其中美欧版 0／52 及回退状态描述旧元数据，不描述上方联合表。

用户授权正常发现不再遍历已知 CPX 的整包 SHA256。新布局表通过完整的小型 FPI SHA256、FPD 文件名／大小及各 extent 的 offset／size 绑定到既有包内位置；已知空包、non-CPX extent 和已成功提取的重复 profile 直接跳过。含 shader 的首个包仅读取 header、block table 和覆盖所需微码的独立块，全部提取微码通过哈希检查后才发布该包来源。

这是已知布局的提取契约，不是资源完整性检查：未读块、已知空／non-CPX 内容和已跳过重复副本的同尺寸修改可能不被发现。未知布局退回整包读取与 SHA256 身份路径，仍匹配旧 profile 时可用已有索引；未知内容或提取失败则完整解码／容器搜索，不把失败的部分结果标作完成。

`LO_SHADER_FULL_SCAN=1` 是显式诊断入口：renderer 禁用裸资源、CPX package 和 archive 布局索引，并启用 strict，每次执行完整资源扫描，不复用上次 strict 清单。v5 的 `resource-scanner-v5-cpx-direct`／`resource-scanner-v5-cpx-strict` 清单区分两种模式并使旧 v4 清单失效。正常暖缓存仍使用资源路径／大小／mtime 及逐来源哈希，不能据此检测刻意保持大小和 mtime 的资源替换；DXIL v21 是另一缓存层。

版别核对（2026-09-07）确认当前 archive 元数据匹配亚洲版四盘 52／52 个布局、美欧版 0／52。下述直接读取性能仅适用于受测亚洲版。美欧版回到整包身份路径：裸资源 28 个索引／24 个扫描，CPX 7,401 个旧 SHA 索引／2,797 个完整回退，读取 20,770,329,949 字节。两版各自的正常发现与 strict 完整扫描均得到 20,686 个同名且逐 SHA256 一致的来源，集合摘要也一致；不是仅比较数量。此次版别扫描与运行检查并行，不用其耗时作性能对照。

当前包的 EXE `3aae46b8…` 及包内 DXC／DXIL 已在两版实际进程核实，两版均到达 Map2，普通 Auto 1080p 有效尺寸与截图均为 1920×1080，无分配回退；每版运行清单的 20,686 个来源逐哈希也与 scanner 一致，7 项用户文件保持，任务进程均已结束。两版仍有相同的已知 `ps_78af7d75d932c582`／`vs_291187f5ef8ba74a` 预编译失败，不属于新漏提取；不代表全游戏或新玩家验收。见[美欧版记录](europe-support.md)及 `out/v0.4.0-followup/edition-validation/{scanner/layout-audit.json,scanner/comparison.json,runtime-summary.json}`。

正式串行对照使用相同亚洲版资源和两个新的应用发现缓存，执行期间没有并行构建，未清空 Windows 文件缓存。两边 20,686 个来源名称和逐文件 SHA256 全部一致，集合摘要仍为 `57cb834795fd99419f0a1980f86e38c7123317eed3d627576269f61fd03055fc`。

| 测量 | 整包身份基线 | 直接提取候选 |
|---|---:|---:|
| 资源发现 | 30.015 秒 | 13.280 秒 |
| FPD + FPI 应用读取字节 | 9,781,918,469 | 142,093,208 |
| 解码字节 | 397,429,447 | 397,429,447 |
| 索引命中／完整扫描回退包数 | 10,060／0 | 10,060／0 |

本组发现耗时减少 55.75%，应用读取减少 98.55%；解码量未变，收益来自不再遍历整包。候选暖发现为 1.654 秒，读取来源校验 10,616,568 字节、资源读取为零。这是应用读取计数，不是物理磁盘 I/O；不包含 DXIL 编译、PSO、完整启动或 FPS。此前 49.578 秒基线和与构建并行的 13.264 秒初测只作诊断，不纳入正式对照。

`shader-index` fixture 已通过直接布局／所需块、strict 清单隔离、未知布局、提取失败、跨块来源、末项失败不部分发布及未读修改边界等检查；主程序构建通过。证据为 `out/v0.4.0-followup/cpx-direct/{baseline-final-cold.log,candidate-final-cold.log,candidate-final-warm.log,source-comparison-final.json,fixtures.log}`。实际新 EXE（SHA256 `3aae46b8…`）在亚洲版空发现缓存、复用任务自有 DXIL 缓存下记录 13.335 秒／142,093,208 字节／20,686 来源，52 个索引文件、零扫描／CPX 回退；Map2 Auto 1080p 静止检查通过，有效尺寸 1920×1080，无分配回退。运行缓存内全部 20,686 个清单来源逐哈希相同，额外运行来源不计入该集合；7 项用户文件保持。新开发 ZIP（SHA256 `79308a15…`）的 45 项 manifest、受测 EXE／DXC／DXIL 及安装器 self-test 通过，完整身份见[交付记录](../../out/v0.4.0-followup/cpx-direct/DELIVERY.md)。本次没有扩大既有画质或玩家验收范围。

下方 74.457 → 35.484 秒是此前整包 SHA256 方案的历史对照，不代表新直接提取路径。

## 2026-09-07：CPX 内置定位索引（本地未发布）

本节保留此前整包 SHA256 身份方案的实现与测量记录；正常发现的当前契约以上节为准。

v0.3.0 已发布 CPX／XEX 发现与已记录管线准备；以下定位优化是 `c548b48` 之后的本地工作，不属于现有发布包。本文后续“当前”小节保留各次实验的历史含义，最新版本与范围见 [STATUS](../STATUS.md)。

内置表包含 10,060 个完整压缩包 SHA256 身份、压缩／解码大小及 256,033 个包内 shader 位置记录，只分发身份和定位元数据，不包含游戏或 shader 字节。发现时仍读取并校验完整压缩包：8,910 个已知空包免解码，1,150 个含 shader 的包只解码与来源相交的独立 64 KiB 块，并在发布该包的任一来源前校验全部提取结果。未知、修改、索引越界或校验失败的包回退完整解码和容器搜索。SHA256 去重避免旧路径为比较重复包而再次读取内容。

四盘资源的两个独立应用冷缓存扫描得到完全一致的 20,686 个来源文件名及逐文件 SHA256；集合摘要为 `57cb834795fd99419f0a1980f86e38c7123317eed3d627576269f61fd03055fc`。

| 测量 | 原扫描器 | 索引候选 |
|---|---:|---:|
| 资源发现 | 74.457 秒 | 35.484 秒 |
| FPD + FPI 应用读取字节 | 15,202,218,361 | 9,781,918,469 |
| 解码字节 | 7,668,866,392 | 397,429,447 |
| 索引命中／完整扫描回退包数 | 0／10,060 | 10,060／0 |
| 暖资源缓存发现 | 1.187 秒 | 1.108 秒 |

该对照的发现时间减少 52.34%，应用读取减少 35.65%，解码字节减少 94.82%。基线旧计数漏记 1,671,168 字节 FPI，表内已补入；这不是物理磁盘 I/O。冷缓存指新的资源发现缓存，未清空 Windows 文件缓存，两组之前生成器均已读取同一资源。该测量不包含 shader／DXIL 编译、GPU、PSO、游戏 FPS 或完整启动；不能将这 35.484 秒称作首次启动时间。

暖缓存继续按路径／大小／mtime 判断资源身份，并逐来源验证内容；候选记录 10,616,568 字节来源校验读取，资源读取为零。它不检测刻意保持大小和 mtime 的资源修改；完整 CPX 身份校验在重新发现时生效。裸 FPD 索引原有的抽样布局合同也未升级为完整资源校验。v4 资源清单增加来源列表 SHA256 完成摘要，截断列表不能当作完整结果复用。

同轮内部分辨率改动增加了翻译 shader 的逻辑纹理尺寸常量，DXIL 缓存版本升为 v21，旧编译结果需要重建。它与资源发现清单 v4 是不同缓存层；CPX 发现变快不消除这次重新编译成本。

最终 `shader-index` fixture 通过完整／空／修改包、同大小新增 shader、跨块来源、末项损坏无部分发布、越界、损坏来源恢复、清单截断／迁移、SHA256 向量及进度单位检查。进度区分缓存验证、索引提取、回退扫描、编译和管线准备；单位分别为文件、条目、MiB、shader 或管线，不再把 FPI 条目数显示成 MB。53,091 个 FPI 条目不等于 30,399 个 CPX 包。

整合 `build-v1` 的原生 720p 启动也记录 20,686 来源、52 个裸文件索引／0 裸扫描、10,060 CPX 索引命中／0 回退、20,339 重复包和 9,781,918,469 资源读取字节，发现耗时 34,132 ms。此时 DXIL 首次编译仍在进行；该记录验证扫描器接入实际启动，不是总启动或画质验收。

证据位于原工作区 `out/v0.4.0-followup/cpx/`：`validation.md/json`、`source-comparison.json`、四份 cold／warm 日志、两组来源摘要及 `fixtures-final.log`。计时后只修正了进度单位、失败校验读取计数及 fixture 的全扫描开关；最终 fixture 重新通过，成功计时路径不变，未重复基准。可选生成器 [generate_cpx_shader_index.py](../../tools/generate_cpx_shader_index.py) 仅在显式运行时读取调用者提供的游戏资源；正常构建使用已检入的头文件。更多同场景画质和启动整合验证见[后续交接](handoff-v0.4.0-followup.md)。

## 多线程预编译

`7ABDBAFD` 默认检测逻辑线程数，使用 `max(1, logicalThreads - 1)` 个编译线程；任务数不足时不创建多余线程。检测返回0时回退到1个线程。当前机器检测16个逻辑线程，实际使用15个worker。`LO_SHADER_PREPARE_SERIAL=1` 仅供单线程对照诊断。

工作线程独立读取、校验微码、翻译并调用各自的DXC实例，写入各自的缓存文件。DXC库加载使用既有call_once。渲染器缓存表、统计和GPU对象创建仍在CP线程上执行；原子队列分发任务，主线程汇总进度，join后载入结果。文件名必须与内容哈希一致，避免多个别名同时写同一个缓存。资源扫描未并行化。

相同exe、相同2000份微码、两个独立空DXIL缓存的对照（复用相同资源清单，不含扫描）：单线程编译阶段52664ms、总准备53441ms；15线程编译阶段6030ms、总准备6746ms，总准备约7.9倍加速。两轮1998成功、2个相同的既有失败，1998份DXIL逐字节一致，也与上一版本串行产物一致。证据 `out/parallel-prepare-results.json`、`out/prepare-bench-serial`、`out/prepare-bench-parallel`。

首个并行性能副本遗漏profile，自动读档未执行，手动终止了该测试副本；它仅用于编译数据，不作为Map12回归证据。游戏回归使用另一个完整save/profile副本。此前全空缓存35秒扫描数据不变，不能把6.7秒当成完整首次启动耗时。

## 当前：首次启动资源预编译

本地 `8FD77179` 自动扫描 `--game` 目录的 FPD 资源，提取经容器边界、阶段和长度校验的 SDK 微码，去重后在首帧前编译。开发目录 `disc1..4` 会一起扫描已有的相邻盘目录；独立 `game` 目录只扫描自身。不需要先游玩对应地图。当前支持 `102a1100/102a1101` 容器，尚不解析压缩包内部或其他容器版本，不等于全游戏覆盖证明。

Windows 窗口内显示扫描/编译计数和进度条，完成后自动恢复游戏；标题也显示进度。窗口由独立线程处理事件，扫描期间关闭窗口验证正常。清单记录文件路径、大小、修改时间；下次校验来源和微码哈希后跳过资源扫描。微码损坏会重新提取，DXIL 缓存失效会重新编译。准备中断不写完成清单。相同大小和修改时间的资源替换无法由该指纹识别，需要移除 `resources.manifest` 强制扫描。

### 当前验证与限制

- 四盘提取 2000 个唯一微码（1761 PS / 239 VS）；盘1已有1905个，盘3累计1939，盘4累计2000。
- 空缓存独立启动：扫描35.4秒，编译54.1秒，1998成功、2失败，然后进入Map12。失败项 `ps_78af7d75d932c582`、`vs_291187f5ef8ba74a` 保留日志并在后续启动重试；未伪造成功或替换成空着色器。其翻译错误仍需专项诊断。
- 资源微码仅精确覆盖既有184个运行时样本中的66个PS。VS样本存在顶点提取指令差异，其他缺失项也未全部解释。Map12首次仍有104个shader补编译（2415ms）和178个管线创建（3603ms），不能声称首次加载已完全消除卡顿。
- 第二次启动复用清单；连同首次游玩收集的变体共准备2118个shader，耗时2075ms、2失败。Map12测试f100后无超过150ms的日志帧；不同运行驱动缓存状态未控制，不能只归因于预编译。
- `shader_resource_scan_test.cpp` 验证截断、偏移溢出、阶段错误、跨4MiB块容器、去重、清单复用、源文件损坏重提取。
- Windows后台游戏验证进度子窗口存在、标题更新、准备中关闭退出。隐藏窗口跨进程截图返回黑图，不能作为画面证据；同一绘制函数的独立进程内GDI绘制结果已目视核对，见 `out/preparation-ui/render.png`。可见桌面合成与多DPI仍未实测。

证据：`out/resource-first-start`、`out/resource-warm-start`、`out/resource-precompile.log`、`out/resource-shaders/inventory.json`、`out/test-resource-scanner.log`。最终只增加了WM_PRINTCLIENT绘制支持，重编译和独立绘制验证通过。未运行第二个并行测试游戏，未修改原始存档。

## 前一阶段：已知微码缓存

本地构建 `2BD8598E` 在启动阶段准备已收集的着色器。默认缓存为工作目录下的 `cache/shaders`，`LO_SHADER_CACHE_DIR` 可指定其他目录。运行时保存原始微码到 `source`，下次启动先翻译/编译或读取对应版本的 DXIL；窗口标题显示 `Preparing shaders X/Y`，完成后恢复标题。`LO_NO_SHADER_PREPARE=1` 可跳过启动准备用于诊断。

运行时与离线工具共用 v20 缓存命名和 DXBC 容器边界检查。截断、错误容器长度或越界块会触发重新编译；这不是完整 DXIL 语义验证，也不保证修复损坏的原始微码。

离线准备已捕获微码：

```text
LoShaderTool <microcode-directory> --cache <runtime-cache-directory>
```

工具输出包含 DXIL 和 source，可由运行时继续复用。缓存不包含在公开仓库中。

## 实测

所有游戏测试使用 Map12 存档副本，一次只运行一个测试进程并在结束后退出；原始 save/profile 未修改。

| 场景 | 结果 |
|---|---|
| 旧版无缓存，Map12 加载 | f718 耗时 4928 ms，其中 159 个 shader 耗时 4739 ms，178 个 pipeline 耗时 77 ms |
| 新版首次运行，没有已知微码 | f728 耗时 7440 ms，shader 3725 ms，pipeline 3475 ms；首次运行仍会卡顿 |
| 新版复用已知缓存 | 启动准备 184 个 shader，0 失败，174 ms；f100 后没有记录到超过 150 ms 的帧 |
| 仅保留已知微码，加入一份损坏 DXIL | 启动识别损坏缓存并重建，184 个 shader，0 失败，4500 ms；f100 后没有记录到超过 150 ms 的帧 |
| 离线工具 | 184 个 shader 首次编译成功；第二次全部命中缓存 |

窗口标题采样确认准备进度从 8/184 等中间值更新到完成。日志只记录超过 150 ms 或每 60 帧的统计，因此不能将采样最大值当作完整帧时间峰值。不同运行的驱动缓存状态和动画相位未严格控制，不能用这些数据量化全部性能提升。

证据位于 ignored `out/shader-prepare-results.json`、`out/load-cold-baseline`、`out/load-cache-first`、`out/load-cache-warm`、`out/load-cache-rebuild` 和 `out/precompile-known-*.log`。构建、链接和上述运行均通过。

## 后续边界

后续需扩大未覆盖资源和运行时变体的收集，修复两项翻译失败，并预创建实际使用的图形管线（PSO）。未知场景仍可能临时编译；冷状态管线创建也实测可达数秒。此功能不修复独立的 GPU query/wait 异常，不承诺消除资源读取或解压卡顿。

热缓存完整profile副本2118个shader准备1970ms（2116命中，2旧失败）；Map12载入后10秒运行完成，证据out/parallel-warm-start。7ABDBAFD已安装主目录并核对hash，未启动可见游戏。

本轮热缓存仍记录f101=167ms、Map12 f715=3767ms；成功进入地图不等于消除所有长帧。图形管线创建仍是独立未决项，详见该帧的shader/pipeline统计。

2026-09-05 用户可见扫描界面文字闪烁：video进度窗口原先直接清屏再画字且每8ms可能重绘。64FDB4ED改为离屏完整绘制后单次BitBlt、UI限10Hz且阶段切换立即更新、仅尺寸改变时MoveWindow。编译链接通过；独立GDI连续300次绘制和暖机后句柄计数检查通过，buffered.png目视核对。用户41308仍运行7ABDBAFD且响应正常，不中断，不替换主exe；修正版out/progress-buffered.exe/.pdb待下一次安装/启动。可见动态闪烁仍待重启实测。


当前安装状态：135DCA79已包含并行预编译与进度离屏绘制修正，原save/profile未改。上文64FDB4ED“待安装”为历史状态；可见扫描全过程的防闪烁仍待用户复查。

## 2026-09-07 UTC：首战冷暖对照与离线资源覆盖调查

本节为新的本地调查证据；上文安装 hash 和 Map12 数字保留为当时记录。本轮使用正式 v0.2.2（SHA256 `ccc63c3f95495ae4d0052ce6d4ee0f62c20897bb17f64bb79079b55a40a75c48`）、RTX 5080 和美／欧版盘1副本。阴影调查按用户十分钟止损条件继续挂起，本节不将其与卡顿合并归因。

### 已观察到的首战长帧

空应用 shader 缓存启动、正常预编译后，renderer f1270／1274／1275／1321 分别记录 700／1301／155／166 ms，其中 shader 处理为 617／1168／120／129 ms，pipeline 为 14／20／3／2 ms，GPU wait 为 1／3／1／2 ms。四帧均发生在首战引入阶段、第一次安排的玩家攻击之前，不能直接解释玩家报告中的每次攻击卡顿。

初始资源准备仍只有2000个来源、1998份成功 DXIL 和两项既有失败，发现后的准备阶段耗时8941 ms。游玩新增119个资源清单以外的来源（72 PS／47 VS），成功 DXIL 累计2117份。暖运行复用这些缓存，启动准备2119个来源、命中2117份、并行阶段没有新编译，仍有相同两项失败，耗时1808 ms；相同输入序列的 renderer f1–4000 未记录超过150 ms的帧，也未增加 DXIL 文件名。

新增微码／DXIL、冷运行的 shader 处理耗时和暖运行消失的对应长帧共同指向这些大停顿中的首次 shader 工作覆盖缺口。该计时包含翻译、缓存处理和对象创建，不是 DXC 独立耗时；日志只记录每60帧及超过150 ms的帧，不能计算完整帧耗时分位数或排除较小卡顿。驱动／操作系统缓存未清，不能推广到全冷机器或完整攻击动画报告。两轮主比较窗口没有连续截图、PS trace 或 resolve readback。

证据：`out/firstbattle-stutter-cold-01/report.md` 及两轮 `runtime.log`、`timing-rows.json`、`timing-summary.json`；暖运行目录 `out/firstbattle-stutter-warm-01`。

### 从 XEX 和资源推导额外来源

用户要求从 XEX 与全部资源离线推导，避免把2000个原始扫描结果视为完整清单。目前已确认：

- 24个去重后的命名 CPX 材质包解码为44,011,017字节，独立编译的原始 PPC decoder 与 Python 移植对全24包逐字节一致。提取9559个唯一微码（9139 PS／420 VS），其中8390个在旧2000清单之外，合并至少10390个来源。该解码验证只覆盖这24包。
- 提取先从资源独立生成，再与运行时缓存核对；52个首战新增 PS 为完整字节精确匹配。不是通过运行时缓存反填生成，也未用相似 hash 代替 shader。
- XEX 恢复4个唯一静态微码，其中3个精确匹配首战新增来源；另找到原始 VS fetch patch、输出链接及调用路径，为继续推导提供依据。
- 基于原始 SDK 元数据、8个固定 declaration 构造器和 fetch patch 逻辑，离线生成354个 VS 候选，其中17个精确匹配首战新增 VS。stride 使用紧凑大小和16字节对齐大小，仍是布局推断；其余337个候选未证明可达。后续编译结果见下节，不能称已覆盖所有运行时变体。
- 全部14,594个 CPX 条目的离线扫描已完成，最终提取和编译结果见下节；前述24包数字为较早的有限样本。

证据：`out/shader-resource-research/decoded-material-report.md`、`out/shader-xex-research/report.md`、`out/shader-vertex-research/report.md`。这些是 ignored 本地研究产物，未分发游戏微码。以上为早期有限样本，完整提取、编译及首战验证结果见下节；增加候选数量本身不是性能修复。本轮没有验收修复、新发布或提交／推送，也没有把离线研究接入运行时。

### 同日续：完整 CPX 扫描与隔离编译

旧2000不是配置上限，而是扫描裸露 SDK 容器的结果；正式启动扫描器尚未解压 CPX。按 archive SHA256／offset／size 去重得到14,594个 CPX extent，再按压缩内容去重得到10,198个 payload。C++ 离线 decoder 用55.19秒处理7,670,454,413个解码字节，报告0错误，只保留微码与来源信息，没有输出完整解压游戏。原 PPC、Python 和 C++ 对24个完整材质包的44,011,017字节逐字节一致；这是样本交叉验证，不能称全量均与原 PPC 比较过。

| 去重来源集合 | 唯一微码数 |
|---|---:|
| 全部解码 CPX | 20,482（19,874 PS／608 VS） |
| CPX 与旧裸 FPD 来源合并 | 20,686 |
| 再加入4个具有 PM4 发射调用证据的 XEX 静态 shader | 20,690 |
| 再加入354个固定 declaration VS 候选 | 21,044 |

21,044个来源经当前 LoShaderTool／DXC、15个 worker 检查，105.22秒内成功21,042个，仅 `ps_78af7d75d932c582` 与 `vs_291187f5ef8ba74a` 两项既有失败。612个来源带 translator notes，包括未 patch 的 VS fetch 模板；成功编译不证明实际 draw 正确或候选有用。

该21,044集合精确覆盖首战119个新增来源中的90个。进一步按原 SDK fetch／VS-PS 链接逻辑扩展到6534个 VS 候选后，精确覆盖提升到93个（71 PS／22 VS），仍缺1 PS／25 VS。较大的候选集并未全数包含在上述编译测试中，stride／binding 可达性仍未证明；生成输入不读取运行时缓存，运行时119个来源只用于独立事后比对。

随后只读核对 `out/firstbattle-stutter-offline-02/async-menu-ps.json` 与对应 `.bin`：XEX `AsyncMenuPixelShader` HLSL 运行时编译创建的对象，全局 `0x83262F00` 指向 `0x0060F560`，descriptor 为 `0x0060F60C`，60字节微码的 FNV 为 `f1cc033418e7b30f`，SHA256 为 `8b60f7619a04f737199f01da3bf26ecd3d1f5783d9d19c1baf21abff0ffcf8d9`，确认为唯一残余 PS。首战新增72个 PS 的来源因此均已解释，但这是运行时身份取证；未独立重放原 HLSL 编译器，也未把读取的 raw 反填候选，离线精确推导覆盖仍为93，另有25个 VS 待恢复布局等关系。

证据：`out/shader-offline/report.md`、`compile-summary.json`、`final-holdout-comparison.json`、`full-cpx/inventory.json` 与 `full-cpx/fast-reference-check.json`（后四项均位于 `out/shader-offline/` 下）。CPX 提取尚未接入正式 startup scanner，没有验收性能修复或新发布。

### 同日续：正式程序的 source-only 验证

`out/firstbattle-stutter-offline-01` 使用官方 v0.2.2、21,044个离线来源和0份初始 DXIL，由正式包自己的编译器启动准备：21,042个编译成功、两项既有失败，准备耗时103,616 ms。游玩只新增29个来源（1 PS／28 VS），全部属于原119个首战样本。这支持来源覆盖改善；未导入离线工具产生的 DXIL，因为其编译器环境与正式包不同，产物字节并不一致。

该轮战斗载入与另一项解包搜索重叠，407／357 ms两帧不能作为无干扰性能对照；240秒上限结束时场景截图尚未落盘，不算视觉验收。

无解包／编译重任务干扰的同序列复验 `out/firstbattle-stutter-offline-02` 已完成。官方程序从21,044个来源／0份 DXIL 启动，并行阶段21,042个编译、0命中、79,144 ms；总准备21,044个、两项既有失败、96,827 ms。最终21,073个来源／21,071份 DXIL，新增29个（1 PS／28 VS），初始来源未改，新增微码均与原冷运行 holdout 逐字节相同。

renderer f1–4000仅有两帧超过150 ms：f1269为301 ms（shader243／pipeline12／wait1 ms），f1273为261 ms（shader200／pipeline15／wait1 ms）。原冷运行同序列有700／1301／155／166 ms四帧，暖运行没有该阈值事件。这是该序列观测到的改善，仍有首次 shader 工作；驱动／OS缓存未清、日志不是完整帧分布，不能推广到所有攻击、场景或 AMD，也不能由此宣称卡顿已修复。

测量窗后的 `shot_4079.png` 已由父代理目视确认首战攻击菜单与凯姆。runner运行238.91秒，最后计时到f4140，最终主动结束已核实归属的PID42480；exit1来自清理，不是崩溃。开发EXE／PDB的原D584876D／E8244EEB完整hash断言保持不变。完整证据位于该目录的 `report.md`、计时和缓存对比记录。

本轮研究验证完成；下一步是将CPX提取正式集成到启动扫描器并保持来源／失效校验，恢复mesh声明与stream stride关系，独立重放原HLSL编译路径，以及单独验证PSO准备。尚无验收修复，未集成正式扫描器、未提交／推送，发布状态不变。

## 2026-09-07 UTC：按“接入，我来测试”完成本地接入

上节“未集成”是离线研究阶段的状态。当前本地 `PrepareKnownShaders` 已调用扩展发现流程，尚未提交／推送／发布，用户视觉和卡顿验收仍待进行。

`resource_fpi.h` 严格校验FPI范围并使用固定13个archive映射；`cpx_decode.h` 限制单包解码为128 MiB，分配前检查block table，失败清空输出。`resource_scan.h` 使用 `v3-cpx` 清单，把FPI与FPD身份共同纳入缓存失效；裸容器index命中后仍扫描CPX，payload去重先以hash筛选再比较原压缩字节。`resource_xex.h` 从已加载XEX读取并精确hash验证4个静态shader；`resource_variants.h` 从77个已知原VS及元数据生成354个有限候选，只按精确hash复用缓存，不替换draw所用shader，也不嵌入游戏微码。

`tools/test_shader_index.bat` 的旧用例及新增CPX／FPI／清单迁移／损坏输入fixture通过；decoder合成用例及24个原PPC参考包44,011,017字节比较通过。77个原始VS共40,536字节生成的354个候选与独立原型逐字节一致，`/W4 /WX` 和生成器 `--check` 通过。四盘全新scanner提取20,686个裸容器／CPX来源，全部与参考逐字节一致：84.600秒、28个indexed／24个fallback、读取26,143,351,797字节；这是完整扫描耗时，不能当成纯CPX解码耗时。

正式游戏构建通过，证据 `out/shader-integrated/build.log`、`scanner-tests.log`、`scanner-comparison.json` 与 `full-scan.log`（后三项同目录）。新EXE SHA256为 `deb313651223f6b084614e1abbb3fa62a974c7e8bbd02886f775d9987155b091`。

隔离启动检查已完成：初始缓存0文件，未导入source或DXIL；自动发现20,686个CPX／裸容器来源、4个XEX静态来源、354个VS候选，共21,044个，77个base校验通过、0 invalid。运行时扫描81,943 ms，准备94,979 ms，21,042个成功，仅两项既有失败。自动生成的全部21,044个来源与离线参考逐字节相同，另有4个菜单运行时来源；日志没有error级记录。脚本运行到1020帧后主动停止PID23280，exit1为主动停止。`shot_911.png`已查看，实际为原生Settings界面，不能写成首战通过或卡顿验收。

用户测试入口 `out/shader-integrated/play/Play.cmd` 与 `Launch.ps1` 已完成，并由父代理启动预览PID57112。使用原亚洲版盘、中文设置和save/profile独立副本，复制内容与原文件逐字节一致，独立cache并清除其它LO诊断开关。切换亚洲版会重新扫描一次；这是用户预览，不是冷缓存性能比较。主目录EXE／PDB已恢复D584876D／E8244EEB基线。证据 `out/shader-integrated/runtime-comparison.json`、`runtime-check/run.json`、`runtime-check/runtime.log`、`play/last-run.json`（后三项同根目录）。用户画面／卡顿验收仍待进行，未提交／推送／发布，阴影调查继续挂起。

## 2026-09-07 UTC：动态 VS 链接候选与已记录管线预创建

按用户后续要求，本地新增 `GenerateLinkedVariants`：从原资源77个VS与86个PS生成1891个净新增链接候选，连同旧354个共2245个，新增候选全部与独立Python结果逐字节一致。本次29个运行时缺失VS样本中匹配2个；来源总数从21,044增至22,935，启动新增编译1891个、缓存命中21,042个，共22,933成功，仍为两项既有失败。精确字节匹配不代表所有候选可达，也未覆盖全部动态布局。

`pipeline_cache.h` 保存完整pipeline Key，使用版本校验、校验和与原子写，最多16,384条。运行时每60帧异步checkpoint；启动最多4个worker沿用实际 `CreatePipeline` 路径，成功才加入缓存，缺shader则跳过并回到运行时创建，失败不污染缓存。界面显示 `Preparing pipelines`。`LO_NO_PIPELINE_PREPARE` 只跳过预创建；`LO_NO_PIPELINE_CACHE` 完全关闭记录和读入；`LO_PIPELINE_PREPARE_SERIAL` 用于串行对照。SDL_QUIT路径的 `_Exit` 绕过Shutdown，最后一次周期之后的新记录可能丢失，不能保证每次关闭都会flush。

fixture以 `/W4 /WX` 通过192个损坏、192个截断及边界／原子写失败检查。`out/shader-pipeline-preview/build-02.log` 构建通过；build-03的实际draw计数修正已构建通过（exit0），最终EXE SHA256为 `1c8d98d93b7a10db9252829a669801c664c8236cfc4e7167445cbf473afe3749`，主EXE已恢复原D584完整hash，证据 `out/shader-pipeline-preview/build-final.json`。首次record运行到7380帧，截图确认首战喷火，记录233个recipe；初始没有PSO，预创建数为0。seed仅导入独立21,044个原始来源及匹配DXIL，运行另有30个新source及1891个生成来源，证据 `out/shader-pipeline-preview/record`。control／warm最终验证见下段。

纯资源推导全部首用PSO尚未接入。虽已解析22个包、27,251个typed shader和821个material map，仍缺完整shader绑定与pass状态；当前预创建依赖之前实际记录的recipe。用户视觉／卡顿验收待做；以上均为本地未提交／推送／发布改动，不属于v0.2.2，首战闪烁调查继续挂起。

### 同日续：记录管线预创建 A/B 完成

control与warm均使用最终EXE `1c8d98d93b7a10db9252829a669801c664c8236cfc4e7167445cbf473afe3749`、相同初始pipeline文件SHA256 `40a5478afb5a4f657689f2ba43c68f7336799676a7a6645003737c3146403efb`、22,965个source／22,963份DXIL，按相同输入脚本分别运行到7380帧后主动停止。warm以4个worker在22 ms准备233个recipe，全部ready，0 missing／0 failed。

到f7200，control发生220次运行时pipeline创建；warm为0次，实际提交draw命中预创建pipeline共8,609,548次，使用205／233个预创建Key。该计数已修正为实际draw提交命中，不是仅查询cache。两组f1–7200均没有超过150 ms的日志帧，因此本轮证明已记录PSO能提前创建并被实际绘制使用，不声称测得卡顿或帧率改善。

父代理已查看control `shot_7237.png` 与warm `shot_7258.png`，均为首战凯姆；动画与火焰时序不同，相同输入脚本不是逐帧确定性录像，实际Key数量差异不能当作严格逐帧性能胜利。驱动／OS缓存未清，不证明全新场景覆盖或完全没有PSO记录的首轮性能。两轮无error／critical／device error，只有相同frame0 pitch24丢弃，pipeline drops为0，两项既有shader编译失败仍保留。证据为 `out/shader-pipeline-preview/control` 和 `warm` 下的 `runtime.log`、`run.json`；首轮record是从21,044个原来源及暖DXIL验证新增链接来源并学习233个recipe，与本次A/B分开。

新用户测试入口为 `out/shader-pipeline-preview/play/Play.cmd`，采用独立cache、上一轮shader-integrated玩家settings/profile副本和原亚洲盘；入口已准备完成，尚未启动本轮用户预览。用户视觉／卡顿验收待完成，未提交／推送／发布。

最终 `out/shader-pipeline-preview/summary.json` 对replay-seed的45,929个文件逐SHA256核对：control／warm均0 mismatch，aggregate为 `466fedfe339b4f8b0d084918bcc1048aa9e95d6dbcec0899c678ddba86603554`，两组新增source为0。`player-preview.json` 确认旧shader-integrated玩家副本的3个save、1个profile、1个settings共5文件复制后逐SHA一致；主EXE／PDB基线恢复已核对，本轮游戏进程均已退出。


## 2026-09-07：v0.3.0 正式发布

[v0.3.0](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.3.0) 已于UTC 2026-09-07 06:55:19正式发布，draft／prerelease均false，标签 `fba7ae4` 保持不变且已双推。后继工作流 `0a79e9e` 手动checkout该标签，run34091301175成功。范围为CPX/FPI扫描、4个XEX静态shader、354个固定与1891个链接VS候选，以及已记录PSO持久化／启动预创建；不含挂起的连续shadow trace诊断或闪烁修复。上文未提交／未发布陈述保留为各次实验历史。

正式ZIP为38,264,592字节，SHA256 `db11b3f275bde905ce4796cee886c6d4787e9030ab55728541c134e3e660f485`；44项manifest与CRC全通过，安装器自测exit0。正式EXE SHA256为 `9e9153e97a5caf1e107b1b9b36c9ed9c13a12d69dc6f6a6a0990f64aba201abe`。唯一隔离首战smoke到1860帧，233个recipe全部ready，0missing／0failed，4worker用21ms；f1800实际882,131次命中／171keys、0runtime creates。父代理查看截图确认凯姆首战菜单正常，无error／fatal／critical。此为短时及单帧场景验证，不代表阴影解决、FPS改善或新增玩家验收；本次没有重复启动8项／存储全套或A/B。

测试组织在标签之后独立演进，不属于v0.3.0源码包；importer／shaders／pipeline的独立CI34091299239／34091299255／34091299202均成功。原22,935来源／22,933编译成功与两个旧失败、A/B覆盖边界保持。原工作区证据为 `out/release-v0.3.0/published.json`、`package-validation.json`、`smoke-result.json`（后两项同目录）。
