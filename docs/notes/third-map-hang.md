# 第三地图卡住调查（2026-09-04）

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
