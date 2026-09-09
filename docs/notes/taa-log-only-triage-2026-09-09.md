# 从日志与 render state 定位 TAA 顶点路径遗漏

2026-09-09：用户接受本次 Ghost Town 二号档同场景修复。候选仍为 **0.5.0**，EXE SHA256 为 `1c9911a3ad6dc91acc5ac1a2cffd7503031b12a81b82b19dd526ce28345ca87a`。Sol 在实际场景取得六张间隔的 presented-frame 截图，覆盖约 11.37 秒，样本内未见闪烁；这不是连续录像。其他场景仍属回归覆盖。以下先保留实现前的修复复盘；本轮新增日志功能及其验证边界见文末。

## 本次修复如何得到证据

先核对实际 EXE、日志会话、shader hash 命名空间。`renderer-byte-fnv1a64` 才能直接对应本次 `PositionVPSlot` 与 capture shader 文件名；command-processor word hash、HLSL hash 不能混用。编译成功或缓存命中并不表示该 VS 已支持 jitter。

服务端摘要先发现 `0b786a899598ce18`：`slot=-1, candidates=4, flags=19, rejection=2`。这说明相机 c7 位模式匹配、视口兼容、jitter 开启、绑定相同深度，但分类未知且未应用 jitter。用本地缓存中的 microcode 验证身份，离线查看输出数据流，确认 c7–c10 实际参与 `oPos`，才加入精确 c7 分类。

用户再次报告闪烁时，新 capture 2813–2815 根本没有这个 VS。因此继续检查新的遗漏，不能把“还有闪烁”直接当成前一条补丁失效。重建 draw 顺序和原始寄存器状态，对齐相机、视口、深度及 shader 指令，得到四条额外的 c7 位置路径：

| VS | 每个捕获帧的 draw 数 | 写深度 | 实际位置矩阵 |
|---|---:|---|---|
| `e8ec18f1d3eac4df` | 44 | 是 | c7–c10 |
| `1ea46291cb1c7298` | 1 | 是 | c7–c10 |
| `7d403bdef896a97f` | 2 | 否 | c7–c10 |
| `45ed0948b6b701a7` | 1 | 否 | c7–c10 |

这些 draw 缺少 jitter，而同场景的其他材质／深度路径已有 jitter，形成不一致。修复只补分类，保留原有相机位模式、视口、深度以及特殊阴影采样检查。随后取得实景样本并由用户接受本次修复。该证据支持这次具体修复，不是对所有闪烁原因的排除。

## 当前日志能筛出什么

这里要区分普通 `shader-*.jsonl` 和 opt-in shader/state 摘要。前者默认主要记录 shader 创建、缓存和编译；详细 temporal draw trace 有开关、32 帧范围和已知 VS 筛选。后者包含 `slot/candidates/flags/rejection`，更适合发现未知路径。普通 shader log 没有异常记录，不能解释为覆盖完整。

当前字段来自 [renderer.cpp](../../LostOdysseyRecomp/gpu/renderer.cpp) 和 [temporal_jitter.h](../../LostOdysseyRecomp/gpu/temporal_jitter.h)：

| 字段 | 当前含义 | 分析方式 |
|---|---|---|
| `slot` | 精确 hash 分类返回的 VP 起始常量槽，未知为 -1 | -1 是未分类，不一定是错误 |
| `candidates` | 六个已知起点 `{0,4,7,8,230,233}` 的匹配位掩码 | bit 0–5 依次对应这些起点；不是候选数量 |
| `flags & 1` | 支持的 guest viewport/VTE 形式 | 不代表最终与 anchor 的 raster viewport 完全相等 |
| `flags & 2` | jitter 开启 | 关闭 TAA/jitter 的记录不能当遗漏 |
| `flags & 4` | 实际应用 jitter | 0 才是候选未应用 |
| `flags & 8` | 写深度 | 用于排序，不能作为必要条件 |
| `flags & 16` | 已绑定 depth 且 allocation 与 anchor 一致 | 强关联线索；仍不能证明位置数据流 |
| `rejection=2` | `UnknownShader` | 该判断早于后面的完整 guards，不能据此声称其他 guards 全通过 |

可在现有 opt-in 摘要上使用以下规则，输出“高优先级覆盖候选”：

```text
slot < 0
AND rejection == UnknownShader
AND (flags & 3) == 3
AND (flags & 4) == 0
AND candidates != 0
```

相同 depth allocation、稳定跨帧出现和较多 draw 可提高排序。写深度的路径优先；不写深度的光照／透明路径也要保留。按 VS 聚合发现目标，同时保留 PS 和状态分组，不能把跨版本、跨时刻、不同视口的摘要合成一条不存在的 draw。

`candidates=4` 表示 c7；`20` 表示 c7 与 c230；`36` 表示 c7 与 c233。多处都匹配时不能选最小槽或直接选 c7。当前检测只扫描上述六个起点；`candidates=0` 也可能是尚无 anchor、矩阵存于其他槽、矩阵表示不同或采样条件未满足，不能证明 shader 无需 jitter。

另一个重要盲点：当前 `jitter_misses` 只累加 `temporalSlot >= 0` 的拒绝 draw，**未知 VS 不计入**。所以 `jitter_misses=0` 不是“没有未覆盖路径”的证据。已有分类但 `CameraMismatch/DepthMismatch` 的记录应进入 guard／场景关联排查，不与新增 shader 分类混为同一问题。

## 为什么现在不能直接按候选批量修补

本次对保留的 capture 审计 JSON 再统计，三个帧分别都有 464 次 draw；每帧的“未知分类、兼容视口、带匹配相机常量”集合相同：

| 集合 | 不同 VS 数 | 每帧 draw 数 |
|---|---:|---:|
| 仅依据状态筛出的候选 | 9 | 70 |
| 指令证实需要补 c7 的路径 | 4 | 48 |
| 残留矩阵造成的非目标候选 | 5 | 22 |

这是同一场景三个捕获帧的复盘，不是全游戏准确率评估。只保留写深度候选，在这组样本里会留下 2 个 VS、45 次正确 draw，但同时漏掉另外 2 个合法路径、3 次 draw。

反例 `eb5f611c4321708e` 占 18 次 draw，c7 与 c233 都留有相机矩阵，但实际位置计算不是这两个槽。`8bbd4da701845d16` 的画面拷贝路径也保留相机常量。其余三个非目标 VS 为 `6318b7b358aa8b45`、`73a203360434358c`、`9b81c55ca39bb529`。它们说明：寄存器里“存在相机值”，不等于当前 shader 会读取它，更不等于读取结果影响 `oPos`。

## 怎样让以后只上传日志就能更可靠定位

关键是让日志携带客户端已计算的位置数据流证据。以下为实现前的设计；本轮已落实的部分见文末，观察帧数、提交计数和积压统计等仍未实现：

1. **每个新 VS 只分析一次。** 在译码／编译或缓存准备阶段对位置输出做反向依赖分析，识别实际矩阵起点、变换顺序和可能修改的其他输出。结果按 shader 身份和分析器版本缓存；不要在每次 draw 扫描指令。
2. **记录位置用途，而非全部常量读集合。** 建议小型字段包括 `position_vp_slot`、`position_pattern`、`position_proof`、`relative_addressing/ambiguous`、相关插值输出用途。仅判断 c7 被读取仍会把光照、雾和其他用途误归为位置投影。未知分支、动态索引或复杂位置写入保留为不确定。
3. **独立记录遗漏路径的 guards。** 即使 `UnknownShader` 提前返回，也给出候选槽相机匹配、完整 raster viewport 匹配、深度关联和 scene epoch 的证据；位置模式明确且这些条件成立，才提升为“已证实的覆盖缺口”。相机一致性按 draw 的原始上传前状态计算，不能用 jitter 修改后的常量反推。
4. **按新异常聚合，保留覆盖范围。** 输出首次／末次帧、观察帧数、尝试与提交计数、是否截断以及上传积压／丢弃计数，避免把没有收到记录当成没有异常。跨用户比较按版本和硬件分组，不能把 `max_draws` 当用户数或全局发生次数。

这样，服务器的确定性规则可以完成“发现未分类 VS → 核实位置用途和场景 guards → 输出精确修复目标”，LLM 再负责阅读相关源码、解释证据与提出补丁。无需把完整画面、存档或 shader 源码持续上传；但客户端需要先实现保守的位置分析，复杂模式继续留给本地 microcode 或 render state 复核。位置依赖本身也不自动证明修改矩阵没有其他输出副作用，特殊 VS/PS 路径仍需检查。

现阶段已有的折中方法是 **日志中的 hash + 本地已验证 shader 语料库**：按 hash 取回对应 microcode，离线确认位置模式，不必每次重新导出画面。没有对应 shader 的 hash 不能靠数值相似性推断用途。完整 render state 主要用于证明某次 draw 的实际上下文；画面用于确定视觉问题与修复结果。日志发现遗漏与证明画面不再闪烁，属于不同证据层。

## 本次容易误判的地方

- 上传队列曾按 hash 排序，动态分辨率变体挤占正常批次；较早 105 条服务端记录中 92 条属于同一 VS 的变体，新四条 VS 当时尚未入库。现已改为异常优先和普通变体限额，但队列仍在内存中，不能把服务端缺记录当成客户端未遇到。
- 原审计脚本的 switch 正则被代码注释打断，曾把已支持 shader 误算为未知；修正解析后才得到最终 9 个候选／4 个真实缺口。自动分析应绑定准确源码版本并使用可靠解析。
- 三帧重型 capture 分别耗时约 1.75、1.58、3.76 秒，超过 250ms 历史 reset 阈值；不能把捕获造成的 reset 归因于正常运行。
- 服务端稀疏 MV/jitter 序列与 capture 的相机／帧号并不一致，不能强行按“同一地点”拼成同帧证据。稀疏 MV 正常也不能排除某个 draw 未 jitter。

## 可复核的本地证据

- [第一次日志定位及 microcode 证据](../../out/v0.5.0/temporal-diagnosis/REPORT.md)
- [capture 2813–2815 复核](../../out/v0.5.0/taa-capture-followup/REPORT.md)；逐 draw 数据：`out/v0.5.0/taa-capture-followup/frame-01-f2813.json` 至 `frame-03-f2815.json`
- [精确 c7 补丁与上传优先级产物](../../out/v0.5.0/performance-fix/shader-priority-0.5.0/REPORT.md)
- [二号档实景观察](../../out/v0.5.0/slot2-sol-check/REPORT.md)
- [采集协议与边界](../../tools/taa-collector/README.md)、[已支持位置分类](../../LostOdysseyRecomp/gpu/temporal_scene.h)

`out/` 中的原始诊断产物只在维护者工作区保留，不随仓库公开。文末列出本轮已部署的字段及验证边界。

## 2026-09-09 当前实现澄清

本轮已实现保守的位置证据：native renderer 对每个 VS 至多分析一次直线 HLSL 符号依赖，记录真实位置矩阵、直传或未证实结果，并保留 issues、可能关联的 interpolator mask 和每个未知 VS 最多 8 种 flag/guard 状态的 coverage-candidate 事件；`UnknownShader` 返回前仍独立记录 anchor、matrix、viewport、depth 和 finite guards。客户端实际序列化 schema 2 的嵌套 `position` 与 guards，不放宽 jitter 分类。Worker 当前部署同时接受 schema 1 和 schema 2，schema 1 canonical/hash 保持不变。

定向验证包括 27 条 C++ rule/guard 检查、保留 capture corpus 的分类结果、Worker 协议检查、343B native JSON 接收和 source-0.5.0 增量编译；完整证据见 [position-evidence report](../../out/v0.5.0/performance-fix/position-evidence-0.5.0/REPORT.md)。本轮没有运行游戏，因此不能把旧 Ghost Town 候选的实景验收迁移到当前二进制；全游戏覆盖和用户验收仍待后续实测。
