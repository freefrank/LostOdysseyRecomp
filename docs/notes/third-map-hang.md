# 第三地图卡住调查（2026-09-04）

当前汇总见[成果报告](../WORK_REPORT_2026-09-05.md)和[状态总表](../STATUS.md)。下文按实验时间保留证据，早期未完成状态不代表最新结果。

## 2026-09-05 诊断补齐与故障注入

`main.cpp` 默认将每次运行的日志写入工作目录 `logs/runtime-<时间戳>.log`，同时保留 stderr；`LO_LOG_FILE` 可指定路径，`0` 关闭重复文件输出。每行 flush，默认不覆盖旧运行。此前只写 stderr 的描述适用于旧版本。

GPU 看门狗改用完成 Present 和窗口事件处理后的计数，记录 submitted/completed、当前执行阶段和最后 PM4 opcode，再输出客体线程等待和回溯。它用于区分渲染、Present、事件处理与客体等待，不是营地卡死的修复。

验证副本 `out/diagnostics-01/` PID 32096，原始存档未修改。在独立副本读档稳定后，核实 GPU 线程属于此 PID，外部暂停该线程7秒并在 finally 中恢复。120.020秒实际记录：`completed=3417 submitted=3417 stage=WAIT_REG_MEM opcode=0x3c`；期间音频线程和文件读取日志仍继续。恢复后148秒重新达到30fps。这是主动故障注入，不是自然复现营地卡死，也不能据此认定用户的卡点是 WAIT_REG_MEM。

持久化证据：`out/diagnostics-01/logs/runtime-1788588297761813.log`，停滞报告第2639行。构建 `out/build-diagnostics.log` 通过。

## 范围和证据

用户报告进入第三地图卡死。本轮开始只读检查先前用户 PID 13344 时，进程已经不存在，不能取得或推断其卡住线程。没有终止用户进程或操作桌面。

独立隐藏副本 `out/third-map-repro-01/`，PID 40060，复制构建目录中的 save/profile 和 EXE；不写原始存档。EXE SHA256 `2D9A5696DDD48B77593792FF61D2BBBD89F566F319940D0E26D5D488BD492BD5`。控制仅通过 LO_TEST_INPUT_FILE / LO_TELEPORT_COMMAND_FILE。

## 初始发现

- 最新原始存档实际已到 Highlands of Wohl - Gorge。29秒截图 `loaded.png` 显示区域标题与存档点；地图包前缀为 `u12_0`。
- 读档后地图持续30fps，角色可行走，未复现载入本区域即卡死。不能据此否定用户报告；继续检查后续剧情和出口触发。
- 58.825秒原生POI传送到出口附近 `(2258,497,202.2508)` 成功，随后三段进程内方向输入已使角色接近士兵/车辆。截图 `exit-arrival.png`、`forward-01.png`、`forward-02.png`。

## 状态

调查进行中。尚未证明死锁、无限循环或资源等待，尚无针对性修复；不将普通站立、教学或等待互动视为卡死。


## 营地路径实际结果与限制

用户进一步澄清为“读取完成进入营地后窗口无响应，日志仍运行”。重新枚举进程时只有本轮隐藏副本，没有可读取的用户卡住现场。

先前 POI 传送跳过了中段士兵发现 Kaim 的触发区域。本轮返回存档点沿普通路径前进，取得 Healing Medicine（通知需 A 确认），约470秒正常触发实时演出。`route-04.png`、`camp-cutscene.png` 显示不同镜头；562.487秒截图 `camp-after.png` 回到营地，swap16800，812draws/frame，30fps。

营地后进程内方向输入仍使角色移动（`camp-move.png`、`cart-near.png`），730秒仍持续30fps。接近车辆的出口限制对话可以触发，`cart-gate.png` 显示士兵要求稍等，A对话在此前路径已确认可推进、关闭。箱子/戒指教学和上车仍未完成；不能将这一正常互动门槛称为死锁，也不能把本轮尚未复现等同于用户问题不存在。

地图包名 `u12_0` 的玩家显示名已用本轮进场标题直接确认是 **Highlands of Wohl - Gorge**（`loaded.png` / `shot_840.ppm`）。内部资源的 Wasteland 字样可能为开发命名，不能替代游戏显示名。营地与前段峡谷属于该次已加载区域。

日志实现 `os/logger.h` 只写 stderr 并flush，无默认落盘文件。GPU command processor在空闲和Present后调用 `video::PumpEvents`；因此其它线程日志仍动不能证明窗口线程或GPU正常。下次若有真实卡住现场，应同步记录swap/heartbeat是否停止、GPU线程等待点和客体线程栈。隐藏副本持续Present的结果未覆盖可见窗口专属消息问题。

本轮没有针对性代码修复；原始save/profile未修改，独立副本中的save/profile仍保留可重启的营地前Gorge存档。调查状态为**此路径未复现，根因未确定**。

## 临界区修复后的后台回归（2026-09-05）

另一路首战自然死锁已定位到递归计数大小端错误并修正，见[专项记录](critical-section-endian.md)。新独立副本out/critical-section-camp-01（PID33632）从Gorge存档沿普通路线触发士兵演出，没有传送跳过触发区。7339帧回到营地，输入使坐标从(2660.9949,419.99747,147.81354)变到(2568.3096,184.4953,167.73782)；8609帧菜单可打开，关闭后继续行走，12213帧出现Open提示，13350帧宝箱实际打开。此流程未重现卡死。隐藏窗口仍未覆盖可见窗口消息问题，原报告与已修首战死锁是否同源未证实。

该路线在Healing Medicine拾取附近仍出现黑色特效（shot_2780.ppm）；此处是Gorge，不能冒称已经覆盖第二地图木箱破坏。营地新存档和重启读回仍待完成。

补充：shot_15071.ppm明确显示开箱取得4 Whetstones，验证了营地物品交互完成。

New background reload failure: out/ps-trace-runtime-01 terminated with guest null read at 827B6278 (caller 823B62A0; worker 8248AE80) after swap 661 during copied camp load. Its new PS trace had not accepted a request. Prior successful camp loads remain valid but do not exclude this intermittent failure; cause and relation to original camp hang are unproven. Preserve run.log for follow-up.

## GPU wait crash follow-up

Original-image decompilation (out/camp-null-chain.txt) identifies 823B62A0 as a GPU completion wait: it compares a requested timestamp to device +0x2A9C and the completed timestamp through device +0x2A90. It builds a stack wait state whose first word is the device pointer. 827B6278 reads this first word and dereferences device +0x2A90 while monitoring a 5000-tick timeout. In the failed 11504 process, captured r29=2 at that dereference implies the wait state's device pointer was invalid, not simply that a valid GPU fence had not completed. The original guest instructions were checked in ppc_recomp.74.cpp / ppc_recomp.12.cpp. The writer or clobber source is not identified; do not bypass the wait or null check as a fix.

Two controlled reload attempts used independent copies of the exact failed run's save/profile. The earlier 03722902 build (out/camp-null-baseline-01, PID39816) reached camp, screenshot1256. Current BC8FF36A build (out/camp-null-current-01, PID39484) also reached camp and opened the complete menu, screenshot3840. The current run needed one background A after the timed startup input left it at Last Saved Game, so startup input timing was not identical. These successes rule out a reliably reproducible failure in every load of this save/build; they do not exonerate a timing-sensitive regression. Both were configured with existing LO_CRASH_DUMP=r1,r1+0x80,r31,r28,r28+0x2a80 for another failure, but neither produced one. Baseline39816 was stopped after its exact executable path was verified; current39484 remains at the menu (input2,shots3, no PS trace request). Preserve failed11504 log and original saved evidence.

## Wait-state comparison instrumentation

Added optional debug/gpu_wait_trace.cpp hooks gated by LO_GPU_WAIT_TRACE. 823B62A0 retains its incoming device pointer in host thread-local storage while calling the original implementation; 827B6278 compares its stack-state first word with that incoming pointer. A mismatch logs the original/current device, stack state words, caller, SP, and PCR before calling the original poll. No timeout, completion value, pointer, or recovery path is modified. This narrows a future failure to a bad initial argument versus a changed stack value instead of masking it with a null guard. The diagnostic preserves nested wait tracking.

Build passed (out/gpu-wait-trace-build.log), SHA256 2FA43AC4BA9844B4DFD86099E1817BF21E664CF726FC4558A0AFE2930F2600DF. A new independent copied-save process out/gpu-wait-trace-01 (PID39260) logs real waits from multiple guest workers against device0055B680 and reaches camp, screenshot1069. No mismatch or crash was logged in that load. This validates hook integration and normal wait behavior only; it does not reproduce or fix the earlier invalid-device failure. Current input absent, shots1; LO_PS_TRACE_REQUEST also configured for later rendering checks. Prior failure11504 remains the only confirmed crash of this specific kind.

## 2026-09-05：Ring 回归途中新的 query 设备指针损坏

使用最新 2FA43AC4 构建及独立 Hypocenter save/profile，`out/ring-rt-hypocenter-01` PID34792，按 `out/replay-ring-regression.py` 已知路线进入战斗。shot4601 确认主角正常持剑入场，phase 到2；约178秒进程 ACCESS_VIOLATION 退出，已通过进程不存在核实。没有执行攻击/RT，因此不能作为外环通过或失败的证据。

故障调用链：823BAB50 → 823CE960 → 823CF390 → 823CF3F0。本次 query 对象 r5=0060EE00；823CF3F0 从对象+0读设备得到 r3=1，+4 type=9，+24 timestamp=00008B45；随后设备+2A90读出0，在823CF430 (`lwz r9,0(r9)`) 空读。原始寄存器/host栈保留于该目录run.log。与此前827B6278不同入口，但同样是用于时间戳查询的device指针成为小整数。不能据此断定同一个写坏来源，也不能加空指针跳过查询充当修复。下一步优先追 query 创建/生命周期与对象首字写入，而非重复确认 GPU fence 计数。

本次没有启用 LO_GPU_WAIT_TRACE（该测试进程由单独启动脚本设置环境）；诊断构建包含hook但走原始分支。没有全量draw capture，只有3次单帧截图；故障不能归为此前全量capture造成的0.7秒帧间隔。

## Query creation and lifetime probe

827B7408 creates the 0x9c-byte type-9 query: +0 device, +4 type9, +0xc refcount1; 823CCCE8 maintains a reuse array through global83235AB8, increments references when adding a query, and calls823CDCA8 to release its local reference. 823CDCA8 decrements+0xc and frees only on zero, marking+8 as78787878 before827C9DB0. Original decompilation in out/query-create.txt and generated instructions checked. No incorrect initialization or confirmed early release found.

Added opt-in debug/query_trace.cpp: LO_QUERY_TRACE logs creations and final releases; optional LO_QUERY_WATCH_INDEX arms the existing write watch for the selected creation's first word. It never changes query results or bypasses the original path. BuildE9174CB3 passed (out/query-trace-build.log). Independent Hypocenter out/query-watch-01 PID18768 selected index25=609AD0, initializeddevice55B680; up to index94 initialized correctly. No watched-word changes or final releases recorded through first attack. This selection is a probe, not established as the earlier corrupted object; fixed guest addresses vary across processes. PAGE_GUARD single stepping is diagnostic and can perturb timing/miss concurrent accesses, so lack of a hit does not exonerate corruption. This run reached the menu and completed a Ring attack without the prior crash; no crash fix is claimed.

2026-09-05补充：音频StoreDecoded版D58526BC，独立out/xma-read-cursor-01 PID35404在823CF3F0+0x520写guest0退出。r5=0060EDF0、r4/r31=0，SP02334480；链823CF390→823CE960→823BAB50。这次是空写，不应直接归为前次设备指针空读。完整寄存器/host栈在run.log末尾，未开query trace；截图1因进程退出未完成。下一步检查ppc_recomp.13.cpp:1575对应PPC写入和调用者输出指针。

## Query result pointer / callee preservation probe (2026-09-05)

35404空写对应823CF454 `stw r11,0(r31)`：type9查询尚未完成时先`li r3,1`再写结果。因此该崩溃的r3=1是返回状态，不能拿来证明设备指针为1。823CEEE4调用823CF390前以r1+88传输出，823CF390用r30保存；823CF3F0用r31保存输出。需要定位哪一层破坏保存寄存器/栈，不能仅凭崩溃末尾的易失寄存器推回入口对象内容。

新增LO_QUERY_CALL_TRACE：823CF3F0原调用前后比较SP及r27–r31，前4次/空输出记录query/output/device，异常保存寄存器单独报告，不恢复或改变查询结果。构建out/build-query-call-trace.log通过。独立out/query-call-trace-01 PID11736，LO_QUERY_TRACE也开启；最初自动输入漏掉最后确认，shot1755证实停Last Saved Game，文件input1 A后实际读入Hypocenter。110.279秒query60EDF0、device55B680、output23345D8、SP2334510正确；至172秒没有空输出、保存寄存器异常或崩溃，shot5411确认已进入存档点地图。该结果只验证诊断调用路径，不证明偶发崩溃修复。现场input1/shots2，下一步沿此进程后续遇敌继续追，必要时在823CDEF8内部调用处缩小保存状态破坏范围。

2026-09-05后台遇敌扩展：11736沿已有正常步行路线input2–19到Hypocenter后续战斗，shot12549显示战斗菜单/正常持剑，input20/21实际攻击后shot14665回菜单且HP340→335。期间LO_QUERY_CALL_TRACE未报告空输出或保存寄存器异常，进程仍存活；不能以未复现证明修好。保留input21/shots4，replay-query-route.py会话49544已结束。

检查I/O路径：NtReadFile本地fread及IOSB赋值均在返回前同步完成，未发现该函数后台延迟写IOSB。User APC排队到发起线程的alertable wait，GuestToHostFunction设置/恢复TLS PPCContext；当前未证实其破坏query。另FileHandle上的seek+read/position没有组合锁，若同一handle并发访问可能串位，尚未取得同handle并发证据，不能直接当音频根因。

2026-09-05 F15118B4新证据：xma-command-camp-01 PID7860在车内剧情约556秒退出，823CF430读取guest0，query r5=0060EF40、r3=1（此分支仍为query首字device）、r4/r31=023445D8输出正常、SP02344480，ppc_recomp.13.cpp:1552。547.866秒此前LO_QUERY_CALL_TRACE首次报告callee changed saved registers，但打印前后值全相同；代码比较和日志读取分开，尚不知是否PPCContext被并发瞬时修改，不能依据该条确认某个寄存器变了。当前日志已经按u64输出，非简单低32位截断。应把返回后值一次性快照，再比较与记录。query/wait问题仍未修复；与新增XMA解码错误尚无已证实因果。run.log全部保留，截图6未完成，PID已无，不重启掩盖退出。
