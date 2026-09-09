# Issue #12 跳零问题：给 Claude 的调查交接

请在以下范围内继续调查：找出葬礼交花流程使用的无效对象，是在哪里被释放、覆盖、错误复制或错误保留的。不要先通过跳过 null virtual call 来掩盖问题；需要先确认上游对象生命周期。

所有下列相对路径均相对于仓库目录：

```text
C:/Users/freefrank/ownCloud/Git/LostOdysseyRecomp
```

Claude 可为新增 writer/lifetime 证据进行必要的隔离、后台、静音诊断运行和最小 helper 适配，并复用已经完成的验证。本次交接本身没有启动新运行。生产修复、版本变更、push 和 release 仍未授权；不要重新运行已经完成的静态审计。

## 已确认的复现边界

Issue #12 仍是开放问题：
<https://github.com/freefrank/LostOdysseyRecomp/issues/12>

报告描述收集十朵花后与葬礼 NPC 对话时崩溃，但报告者的具体 EXE、后端、版本、游戏版本和驱动未知。以下是本机在相同剧情边界上的复现，不能直接证明与报告者机器的底层原因完全相同。

实际使用的官方程序是：

```text
out/issue12-triage/runtime/official-v0.4.2/LostOdysseyRecomp.exe
SHA256 13f1294bbb54efbf9a712a066441cb019ba5497cf1242bde6905e6c8bc1757b6
```

测试使用自有 Asia Disc 1、英文、D3D12、RTX 5080、标称 30 FPS 和正常速度。程序后台运行，不抢前台；`LO_AUDIO_MUTE=1` 只静音最终输出，保留资源和对白加载。原用户存档未改动。源码上下文是 branch `0.5.0`、source patch `0.4.15`、commit `bd47a04`；没有生产修复或版本递增。

## 复现过程与两个 run 的用途

诊断 bridge 只用于生成初始原生存档：它进入原始 event-debug map，选择 `RT_071_2C`，然后退出。bridge 不是待调查的官方程序。

run03 使用官方程序独立读取 slot 08 / `save/user07`，在 Ghost Town - Funeral Beach 让 Cooke 接到十朵白花任务。操作者用同图 POI / `SetLocation` 辅助定位交互点，但每朵花都通过正常站位和原始 `A` 采摘；随后按原始出口正常跨图，从 map 109 到 map 108，再返回 map 109。没有手动写入计数或完成 flag。官方程序写出 slot 09 / `save/user08` 的 10/10 交花前存档，Melvi 随后确认已收集花朵。run03 最后在下一次普通 `A` 后实际崩溃。

run04 不是重新证明 run03 的崩溃，而是读取同一个 slot 09，确认 10/10 任务状态和 Melvi 的完成对白已经能独立恢复；随后用外部 debugger 捕获 run03 没有记录到的对象字节和 owner 关系。run04 的 probe 没有写入 guest 内存、寄存器、代码或任务状态，捕获后解除暂停并让原始 crash handler 完成。

存档交付包：

```text
out/issue12-triage/delivery/LostOdyssey-Issue12-funeral-flowers-0-and-10-native-saves.zip
SHA256 3a00b68095886405601b804fad642c3c4eb22b45fed2c475fc1cab84307a419e
```

每个槽都必须保留三个文件：`save.bin`、`.lo-content`、`.lo-thumbnail.png`。目录名和 metadata 中的内嵌槽名必须保持原样：`user07` 对应 0/10，`user08` 对应 10/10；不要直接改名或覆盖玩家存档。ZIP 中的 README 曾写 slot 09 未独立读取，后续事实由 `out/issue12-triage/delivery/POST-PACKAGING-VALIDATION.md` 补充，不能据旧 README 下结论。

## 精确崩溃现场

官方程序在 guest `0x823CB538` 进行 virtual call，返回地址为 `LR=0x823CB53C`。第一次 run03 记录：

```text
ACCESS_VIOLATION (0xC0000005)
execute address: 0
host RIP: 0
guest CTR: 0
guest LR: 0x823CB53C
guest r3: 0x06559AE0
```

run04 的外部捕获在新的进程地址下得到：

```text
guest r3: 0x03B6A480
first word at r3: 0x03B6A560
next 0xE0 block: 0x03B6A640
next 0xE0 block: 0x03B6A720
second header word: 1
supposed vtable slot +0x124: 0
guest LR: 0x823CB53C
guest CTR: 0
```

这些 heap 地址每次运行都会变化，不能在新 run 中硬编码。当前捕获文件：

```text
out/issue12-triage/debug-probe/capture-run04/capture.json
out/issue12-triage/debug-probe/capture-run04/result.json
out/issue12-triage/runtime/run-04/crash-evidence/manifest.json
```

owner 关系的正确解释如下：`r31=0x083E44C0`；`BE32[r31] = 0x82002900` 是静态表指针。对象自身的 owner 字段在 `[r31 + 0x134]`，当前 `r22 = 0x07D69D80`。`r25=1` 时，`r22 + 12*r25 = 0x07D69D8C` 指向 inner array `0x0920C810`，count 为 6、capacity 为 6。前六个 entry 中出现了相同形状的重复对象记录。这个关系说明 owner array 仍在提供记录，但不能单独证明 double free。

原始 allocator 的 free 路径位于 `0x82298990`，分支从 `0x82298A1C` 开始；它写入 `[block+4]=1`，把 `[block]` 接到旧的 `[pool+0x10]` 链首，再把该 block 设为新链首。现场对象头符合这种 free-list 节点形状。实际释放点、第一次破坏对象的 writer、以及最后一个有效 owner 尚未捕获。

## 已完成审计与明确限制

- `0x823CB538` 附近的 116 条原始指令切片、此前 7 个 VM 函数共 422 条指令、8 个 opcode 对照和 run04 的 9 项检查均没有证明直接 translation/emitter 错误；这不排除其他 guest 函数。
- 本机复现使用正常速度，没有 Cheat Engine 2x；报告者的加速说明是独立证据，不能当作本机失败的必要条件。
- 没有证据把问题归因于 Intel GPU、D3D12 或存档损坏。
- diagnostic bridge、旧 Issue #7 RVA、以及过期的 `LostOdysseyRecompLib/ppc` 文件不能作为官方 fault 的依据。
- 当前共享 guest-library link recipe 存在 stale-input provenance gap，但没有证据证明官方 v0.4.2 使用了它；不要据此归因。
- 不要通过跳过 null call 修复；这会隐藏损坏的对象契约并改变游戏行为。

## 可复用工具与下一步

官方 runtime 的 session 入口和约束见：

```text
out/issue12-triage/runtime/session.py
out/issue12-triage/debug-probe/README.md
out/issue12-triage/debug-probe/capture_execute_null.py
SHA256 1980265b382cd8ca9b08137c12c4aaf85477d1ab12b82ab5b9e41c5a3a0f3745
```

旧 session 和旧 PID 已失效。新 run 必须使用独立目录，并通过现有 helper 接口检查当前 EXE、guest memory base、LR/CTR 和 probe ABI；不要猜测 CLI。旧 probe 只匹配本次 `LR=0x823CB53C` / `CTR=0` 的捕获条件，不能直接当作通用写监视器；如果目标调用点变化，先做有边界的 helper 适配。

最有价值的下一步是在一次新的、受控的 10/10 读取中：

1. 对当前 run 重新识别的对象首字和随后 `0xE0` block 设置窄范围写监视，记录 guest PC、LR、thread 和 owner entry，找出首次变成 free-list header 的写入者。
2. 在 `0x82298990`、`0x82298A1C` 和 header write `0x82298A28` 附近记录 `r4` block 与 caller chain，确认是否释放了之后被 `0x823CB538` 使用的对象。
3. 追踪 hand-in lookup，解释六项 owner array 为什么仍返回该记录，以及记录是重复、陈旧还是被错误替换。
4. 若 writer 落在 translated guest function，再以当前 EXE 的实际 provenance 对照原始 PPC、生成源码和确切 link artifact。捕获对象源码与反汇编已在 `out/issue12-triage/translation/crash-analysis/REPORT.md` 和 `RUN04-OBJECT-REPORT.md`，无需重新复查已通过的静态切片、opcode 检查或存档校验。

保持范围收敛：不要做完整 4 GB heap dump、广泛 GC 扫描、null-call bypass、生产 patch、版本递增或无关剧情重测。如果写入者仍无法确认，请报告缺失的具体事件并保存捕获，不要宣称已经找到根因。

## 证据索引

- 主调查记录：`docs/notes/issue12-funeral-crash.md`
- 静态 / native 目标分析：`out/issue12-triage/translation/crash-analysis/REPORT.md`
- run04 对象细节：`out/issue12-triage/translation/crash-analysis/RUN04-OBJECT-REPORT.md`
- debugger 捕获：`out/issue12-triage/debug-probe/capture-run04/`
- run04 session 与日志：`out/issue12-triage/runtime/run-04/`
- 生成存档与验证：`out/issue12-triage/runtime/checkpoints/`、`out/issue12-triage/delivery/`

这些证据位于 Git 忽略的本地目录，调查期间必须保留。Issue 仍开放；本交接不代表生产修复、push、release 或玩家验收。
