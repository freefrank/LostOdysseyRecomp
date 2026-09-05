# 随机遇敌动画与战斗停滞调查（2026-09-04，主角与战斗停滞已修复，敌人待查）

> 2026-09-05 同步：本文保留逐轮取证记录；相关新增代码仍有本地未提交部分，发布范围见[当前状态](../STATUS.md)。历史 PID 和测试中状态不代表进程仍在运行。

## 当前续修进度

- 新增 `debug/opening_state.cpp`：在已复现的战斗编号 0x139 中，将遗留的凯姆资源配置 11 修正为 0；其他战斗不做此兼容改写。debug 直接判胜开场编号 0–2 时也补齐这一资源转换。
- 正常开场对照在 `out/animation-normal-flow/`：不使用 debug 判胜，PlayData 的凯姆资源从 11 变为 0。`out/persistent-resource-value/run.log` 的值变化监视确认实际写入者是 82B00398 内的 82B003EC：读取脚本参数为 2 时执行 `stw 0,128(PlayData)`，11→0。829E6AF8 和 82A49640 不是这次观测到的写入者。
- `out/encounter-repair/` 使用原始存档的独立副本，未启用旧的 `LO_TEST_NORMAL_MODEL` 实验开关。约 swap 1470 兼容修正生效；正常攻击、敌方反击、下一回合和自然胜利（phase 9→11）均通过。`shot_1920.png` 为正常待机，`shot_2400.png` 为主角攻击，`shot_4080.png` 为结果界面。
- **地图名更正**：结果界面确认是 `Highlands of Wohl - Hypocenter`，此前笔记称为 Ipsilon 不准确。
- 返回存档点覆盖存档时发现额外问题：CREATE_ALWAYS 错报 disposition=2。已补齐 savedata 容器覆盖语义、metadata 更新；四个独立测试进程通过。游戏 `out/encounter-reload/` 在 swap 6780 附近覆盖写入 206000 字节、disposition=1，正常返回更新后的存档列表（550G，00:11）。原始用户存档未修改。
- 当轮按用户要求使用后台运行，不使用前台窗口/键鼠；2026-09-05 用户已允许前台操作。新增 opt-in `LO_TEST_INPUT_FILE`，文本为 `serial hexButtonMask leftX leftY polls`；新 serial 触发一次最多 6000 次轮询的输入，完全在测试进程内模拟，不注入系统输入。环境变量未设置时不启用。
- 地图敌人动作与光影闪烁仍未独立修复；不能把主角兼容修正当作渲染修复。

## 复现

用户确认手动存档成功后报告：遇敌时主角和敌人保持 T 姿势，选择攻击后战斗循环而不执行动作，敌人光影闪烁。开场战斗不在本次复现范围。

使用 `save/` 和 `profile/` 的独立副本。输入：
`LO_AUTO_BUTTONS=s@120,a@240,a@360,a@480,a@2200,a@2320,a@2500,a@2800`
`LO_AUTO_PULSE=6`，`LO_AUTO_STICK=0,32767,1000,1600`。
约 swap 700 进入存档地图，约 1470 遇敌，2320 选择攻击。日志入口 `out/animation-encounter/run.log`；进一步追踪在 `out/animation-selection/run.log`。

## 已确认

- 地图主角的局部骨骼数据在更新；并非整个动画时钟停止。
- 遇敌后战斗阶段依次 2、4、5、6、7、8，停在 8。
- 主角的 `FindAnimSequence`（8258E0E8）对 `bx09bin00_00`（进场）、`bx05atk00_01`、`bx05atk00_00`、`bx04mov00_01` 返回 0。
- 此时主角配置编号为 11，配置表位于 `*(0x832ca0d0)+0x90` 指向数组，每项 0x148 字节。其动画集合为 `pc_000d0_bx.pc_000d0_bx`，只有 11 项基础/受击等动作；普通配置 0 则包含 `pc_000a0_bx` 和 `pc_000a1_bx`。初查时不能判定配置选择是否合理；后续正常开场写入监视确认普通流程会转换为 0，见上方续修进度。
- 动画加载入口 82B26518 收到的配置已经为 11，调用方 82AB9E08（返回地址 82ABA9A0）从战斗角色对象 +0x48 读取配置编号。
- 战斗角色列表：`*(0x832ca0e8+0x14)` 为 TArray 描述符，元素指向战斗角色，+0x40 角色编号、+0x48 配置编号。主角编号 0，敌人编号 20、21。不要与骨骼组件或场景 Pawn 混淆。
- 敌人配置 35 使用 `en_031a0_bx`，包含进场动作；其局部骨骼哈希仍有变化。敌人 T 姿势/闪烁仍需单独检查姿态合成、GPU 蒙皮，不能直接断定与主角缺少动作同因。
- 未发现相关 FPD 读取失败。宽 printf 的默认 `%s/%S` 规则与 Xenia 不同，但本次战斗加载日志没有相应调用，尚无证据把它作为根因，未改。

## 追加证据

- 用户再次确认：首场战斗动作正常，缺失发生在后续遇敌。不能把首场回归运行误报成故障复现。
- 写入监视确认战斗角色 +0x48 由 82AF5D18 写入，82AF6290 从持久 PlayData 的角色记录取值。`*(0x832ca0e8+0x20)` 在战斗初始化后指向 PlayData；地图阶段可能为 0，不能直接当作全程有效指针。PlayData +0x80 为凯姆配置。
- 初始角色表 `*(0x8326499c)` 每项 0xcc，+0xa4 的凯姆默认值也是 11。82907A68 初始化它，829E6AF8（SetBattleCharaResource）可按角色修改。初查时未能归因；后续已确认直接判胜绕过的正常开场流程包含 11→0 的持久资源转换。
- 隔离 A/B：`LO_TEST_NORMAL_MODEL=1` 仅在 82AF6290 构造战斗角色时把凯姆的 11 临时换成 0，调用后恢复持久值。`out/animation-model-ab/run.log` 确认资源请求变成 model=0；第一回合 8→9→10→1→2，第二回合 8→9→11，自然进入胜利。未使用 debug 判胜。`shot_1800.png` 显示普通待机，`shot_2520.png` 显示敌人攻击。这是诊断实验，尚非正式修复。
- 地图上的两名 fpPawn 使用 `en_031a0_bx`，初始化请求 `fx12turnl01_n_os`、`fx12turnr01_n_os`、`fx01walk01_f_lp`、`fx00idle01_n_lp` 均不存在。对照凯姆的地图集合为 `pc_000a0_f`。还需查地图敌人的动作覆盖配置，不能用任意同名或近似动作替代。
- 首场正常流程追踪在 `out/animation-opening-trace/`，至约 swap 6000 尚无 SetBattleCharaResource 调用。按用户要求关闭震动时停止了该测试，尚未跑完后续剧情。
- 手柄震动现默认关闭；`LO_CONTROLLER_RUMBLE=1` 显式恢复。此前隔离测试进程已停止。

## 诊断开关

`debug/animation_trace.cpp` 的观察默认关闭，`LO_TRACE_ANIMATION=1` 开启。记录 822B5938 局部骨骼哈希、8258E0E8 查找结果和加载入口。
`LO_TRACE_MODEL=1` 观察战斗角色构造 828BB378，在遇敌阶段对第一名新建角色 +0x48 安装写入监视；用来查实际配置写入者。
Ghidra 导出均为只读，位于 `out/actor-select.txt`、`out/actor-state-object.txt`、`out/animation-functions.txt` 等。

## 验证边界

已验证的正式兼容修复仅覆盖战斗 0x139；开场 debug 判胜补齐资源转换，不代表全部特殊战斗脚本兼容。所有测试使用独立 save/profile 副本，未修改用户原始存档。后台输入直接送入测试进程，不占用系统键鼠。震动保持默认关闭。

## 最终回归

- `out/debug-opening-regression/error.log`：swap 2400 记录 `debug victory: completed opening Kaim resource transition 11 -> 0`，随后进入原生胜利收尾；进程内 PlayData 读取确认 model=0。
- `out/overwrite-final-reload/`：从本轮覆盖后的副本启动新进程，成功加载回 Hypocenter 存档点，PlayData model=0，无需再次调用遗留状态恢复；`latest.png` 为游戏自身输出截图。
- 最终构建和 `git diff --check` 通过；存储四阶段测试在 `out/storage-overwrite-final/` 再次通过。未提交或推送本轮修改。
