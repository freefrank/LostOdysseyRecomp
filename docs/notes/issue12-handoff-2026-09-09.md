# Issue #12 交接（2026-09-09）：根因已定位，修复已在诊断构建中验证

接手人请先读本文，再按需读 [根因报告](issue12-root-cause.md)（英文，含完整证据链）。
本分支 `claude-issue12` 基于 `0.5.0` 的 `7919a9b`，只新增文件，不改动任何已有源码。

## 一句话现状

交花后"跳 0"的原因是**原版引擎的线程竞争**：剧情 VM 换主角网格后，同一 tick 里请求了完整 GC；
`UObject::CollectGarbage` 在 purge 之前不 flush 渲染线程，就把旧网格的 11 个
`MaterialInstanceConstant` 同步释放；渲染线程此时还在画上一帧，旧 proxy 仍引用这些对象，
`DrawDynamicElements` 通过已回收的池块读 vtable，跳到 0。主机上渲染线程够快所以不出事，
重编译里渲染线程滞后（崩溃时 ring 里积压 64 KB 命令）所以 v0.4.2 必崩；0.5.x 只是时序碰巧
错开，竞争仍在。宿主侧修复（GC 前先 flush 渲染线程）已在人为放大时序的诊断构建里验证有效。

## 已确认的事实（均有捕获或反汇编证据）

| 项 | 结论 | 证据位置 |
|---|---|---|
| 崩溃点 | `sub_823CB350` = `FSkeletalMeshSceneProxy::DrawDynamicElements`，对 `Material->vt[+0x124]` 做 `bctrl` | `out/issue12-triage/translation/crash-analysis/` |
| 悬空对象 | `MaterialInstanceConstant_2..12`，Outer=`SkeletalMeshComponent_0`（`fpPawn_0`），Parent 在 `chrpcMaterial` 包；flags `0x0000400200138020`（Transient/Unreachable/BeginDestroyed/FinishDestroyed）；`Resources[0..1]`=NULL | `out/issue12-triage/debug-probe/capture-run09/`、`capture-run10/` 的 `v2`/`v4` 段 |
| 组件状态 | 崩溃时组件已 attached、无待处理重挂，网格已是 `pc_000a0_m`，Materials 是 22 个新 MIC；被画的 proxy 仍是旧网格 `pc_050a0_m` | run-10 `v4` |
| 线程角色 | 崩溃线程 = UE3 渲染线程（`RenderingThreadMain 0x824856A0` → `FDrawSceneCommand::Execute`）；游戏线程停在 `CollectGarbage` purge 之后的 `FlushRenderingCommands` fence 等待 | run-09/10 全线程 guest 栈 |
| GC 触发 | `UWorld::Tick`（`sub_82299818`）末尾的"延迟完整 GC 请求"：`sub_825B32E8(delay)` 置位 `0x83318748`，场景切换 `sub_8231EE68` 传 delay 0 → `CollectGarbage(RF_Native, TRUE)`；11 个调用点仅 60 s 周期清理传 FALSE | 静态分析（opus lens） |
| 为何无保护 | `CollectGarbage`：前置回调→标记→逐对象 `BeginDestroy`→`IncrementalPurgeGarbage(FALSE)`→**之后**才 `sub_82485AF8` flush；MIC 的 `BeginDestroy` 只对非 NULL `Resources` 挂 fence | `sub_8249A568`、`sub_822FD0A8`、`sub_82702098`、`sub_827020F8` 反汇编 |
| 动态复现 | run-13：换网格后 79 ms GC，purge 释放后 **19.5 ms** 渲染线程绘制旧 proxy，13 条记录全部悬空 | `out/issue12-triage/runtime/run-13/probe.log` |
| 修复验证 | run-14：同样延迟下，GC 前 flush 等待 0.77 s，旧 proxy 先被移除，0 悬空 0 崩溃 | `run-14/probe.log`、`run-14/runtime.log`（`gc render flush` 行） |

已排除：PPC 翻译错误（崩溃序列与分配器逐指令核对）、fence 计数丢失（lwarx/stwcx 为 CAS 模拟）、
`poll_wait` 时序 hook（去掉后仍不崩）、事件/临界区实现、Intel GPU、存档损坏、Cheat Engine 加速。

## 分支内容

| 路径 | 作用 |
|---|---|
| `LostOdysseyRecomp/cpu/gc_render_flush.cpp` | **修复**。hook `sub_8249A568`（CollectGarbage）和 `sub_822FD0A8`（IncrementalPurgeGarbage，仅当 `0x83315F40` 有待清理时），在 `GIsThreadedRendering`（`0x83318040`）为 1 时先调用 guest `FlushRenderingCommands`（`sub_82485C18`）。保存/恢复 r3–r7、f1、LR、CTR。`LO_GC_RENDER_FLUSH=0` 关闭。CMake 用 GLOB 收集运行时源码，加入即生效。 |
| `docs/notes/issue12-root-cause.md` | 根因报告：证据链、线程角色、GC 触发链、主机差异分析、修复说明、运行索引 |
| `tools/diagnostics/issue12/README.md` | 工具说明与关键地址速查 |
| `tools/diagnostics/issue12/issue12_probe.cpp` | 宿主探针 TU：记录 0xE0 分配/释放、GC/BeginDestroy/fence/重挂/proxy 事件；绘制前校验每条材质记录；`LO_ISSUE12_SKIP_STALE=1` 跳过悬空绘制；`LO_ISSUE12_RT_DELAY_US`/`LO_ISSUE12_RT_DELAY_ARMED_US`+`LO_ISSUE12_ARM_ON_SWAP` 放大渲染线程时序 |
| `tools/diagnostics/issue12/gc_render_flush.cpp` | 修复文件的副本；`-DISSUE12_EXTERNAL_GC_HOOKS` 下与探针链式配合 |
| `tools/diagnostics/issue12/build_probe.py` | 用冻结的 bridge 编译参数 + 冻结宿主对象 + `guest-fixed.lib` 链接诊断 EXE；变体 `default`/`nopoll`/`gcflush` |
| `tools/diagnostics/issue12/capture_execute_null*.py` | 只读外部调试器 v1–v4：官方 EXE 上捕获首次匹配异常，全线程 guest 回溯、FName 解析、组件材质数组 |
| `tools/diagnostics/issue12/drive_probe_run.py`、`session_probe.py` | 脚本化走到交花对白（标题→Continue→Last Saved Game→走位→对话），`--step final-a` 触发 |

## 未完成事项（按优先级）

1. **正式构建验证**：修复目前只和冻结的 0.5.1 宿主对象一起链接过（`bin-gcflush`，SHA `6d0b33d1…`）。需要用当前工作树正式构建（`tools/build_runtime.bat` 或现有 release 流程），再用官方时序（不加延迟）跑一次交花：`drive_probe_run.py --run run-NN --exe <正式 EXE> --sha <sha>`，`--step final-a` 后确认无 `[crash]`，并抽查运行日志里 `gc render flush` 只在真实 GC 时出现。
2. **性能与回归**：每次 GC 多一次渲染线程排空（关卡加载、60 s 周期清理、场景切换）。建议在几个场景切换点（含战斗进出）观察帧时间与 `gc render flush` 频率；不需要额外测试的话至少跑一遍 `tools/tests` 现有套件。
3. **版本与文档同步**：本分支未改 `CHANGELOG.md`、版本号、`docs/STATUS.md`、`docs/ROADMAP*.md`、`docs/project-management/`；主工作树上这些文件正有未提交改动，合并时按项目流程补齐（建议作为 v0.4.16 或 v0.5.0 的 fix 条目）。
4. **Issue 回复**：GitHub Issue #12 仍 OPEN，尚未回复报告者；报告者提到的"火把位置错误"是另一现象，未调查。
5. **同类隐患排查**（可选）：探针的材质校验 + 事件日志可复用到其他场景切换点（换装、战斗、地图切换）；打开 `LO_ISSUE12_RT_DELAY_ARMED_US` 放大时序即可暴露同类竞争。另一个没深究的问题：官方 v0.4.2 渲染线程在交花瞬间为何滞后 ≥100 ms（怀疑是新角色材质的 pipeline 首次创建），修复后已不影响正确性。
6. `out/issue12-triage/` 里 run-05…14、`probe-build/`、`branch-delivery/`、`capture-run09/10` 都是本次证据，被 git 忽略；调查未结束前请保留。主工作树里有一份未跟踪的 `docs/notes/issue12-root-cause.md` 副本（与分支内容相同），可删。

## 复现 / 验证步骤

前提：`out/issue12-triage/` 的私有布局仍在（官方 EXE、`checkpoints/flowers-ten` 存档、`run-01/shader-cache`、`bridge/` 冻结输入、`../issue7-semantics-fix/build/guest-fixed.lib`）。所有命令在 `out/issue12-triage/runtime` 下执行。

```bash
# 1) 构建诊断 EXE（从 tools/diagnostics/issue12 复制到 out/issue12-triage/probe-build 后）
python build_probe.py nopoll     # 复现用
python build_probe.py gcflush    # 修复验证用
```

```bash
# 2) 复现（换网格后 3 秒内每次骨骼绘制自旋 20 ms）
python drive_probe_run.py --run run-NN --exe ../probe-build/bin-nopoll/LostOdysseyRecomp.exe --sha <artifact.json 里的 sha> --armed-delay-us 20000 --arm-on-swap 2 --arm-seconds 3 --skip-stale 1
# 看截图：没出对白就 --step forward-talk；出了对白就
python drive_probe_run.py --run run-NN --step final-a
# 期望：run-NN/probe.log 出现 "### STALE MATERIAL"，游戏因 skip 存活
```

```bash
# 3) 修复验证：同样参数换 bin-gcflush，期望 0 个 STALE、无 [crash]，runtime.log 有 "gc render flush before CollectGarbage"
```

```bash
# 4) 官方 EXE 上抓完整现场（只读调试器）
python drive_probe_run.py --run run-NN --official        # 走到对白
python ../debug-probe/capture_execute_null_v4.py --session run-NN/session.json --pid <pid> --output ../debug-probe/capture-run-NN --timeout-seconds 1500
# 等 armed.json 出现后 --step final-a；结果在 capture-run-NN/capture.json（v2/v4 段）
```

## 关键地址速查

`CollectGarbage 0x8249A568` · `IncrementalPurgeGarbage 0x822FD0A8` · `FlushRenderingCommands 0x82485C18` ·
`sub_82485AF8`（flush+延迟释放）· `FRenderCommandFence::Wait 0x82322478` · `RenderingThreadMain 0x824856A0` ·
`GIsThreadedRendering 0x83318040` · 待清理标志 `0x83315F40` · 完整 GC 请求字节 `0x83318748` ·
`FName::Names 0x833690D0` · `GObjObjects 0x833690F4` · 渲染命令 ring `0x8336A7A4` ·
骨骼 proxy vtable `0x82002900`（+0x0C=DrawDynamicElements）· 组件 vtable `0x8200CF08`（+0x1B4 CreateSceneProxy，+0x1E8 GetMaterial）·
MIC vtable `0x82005A30`（+32 BeginDestroy `0x82702098`，+36 IsReadyForFinishDestroy `0x827020F8`）·
`SetSkeletalMesh` 类函数 `0x8258D648` · `BeginDeferredReattach 0x822D6810` · 剧情 VM 调度 `0x829FDB40`。

## 工作方式备注

- 官方 EXE 与存档、cache 始终未改动（每个 run 的 `session.json` 有 `seed_unchanged`/`exe_unchanged`）。
- 子 agent 请显式指定 `opus`/`sonnet`，不要用默认模型做大规模 fan-out（本次曾因此耗尽额度）。
