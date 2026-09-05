# Debug 人物传送：逆向依据与验证边界（2026-09-04）

> 2026-09-05 同步：本文保留逐轮取证记录；相关新增代码仍有本地未提交部分，发布范围见[当前状态](../STATUS.md)。历史 PID 和测试中状态不代表进程仍在运行。

## 状态

`debug/teleport.cpp` 已实现游戏线程原生移动；传送、返回、恢复行走、菜单拒绝及 POI 落点的后台验证结果见本文后半部分。F1 窗口前台交互及全地图兼容性未完成验证。本文各 PID 和动态对象地址仅作为当时取证，不能跨进程复用。

## 玩家对象获取

原生注册表的 `intAActorexecLocalPlayerControllers` 对应 `8250E158`。生成代码及只读反编译 `out/teleport-player.txt` 确认：

```text
engine = *(0x83315FB4)
players = *(engine + 0x2B8), count = *(engine + 0x2BC)
localPlayer = players[0]
controller = *(localPlayer + 0x40)
pawn = *(controller + 0x204)
```

`822A6020`（PlayerController GetViewTarget 路径）使用 `controller+0x204` 作为角色回退目标；参见 `out/teleport-target.txt`。地图现场补证了 Pawn `+0x210` 反指同一控制器。后端每次在游戏线程重新获取上述链，并验证双向关系，不缓存动态 Pawn 指针用于下一帧。

当时地图 Wasteland 的现场为：controller `0x4BED200`，pawn `0x3DA9000`，Pawn vtable `0x8201E4C8`。战斗时 controller `+0x204` 为零，控制器对象也不同。

## 坐标与原生移动

注册表字符串 `intAActorexecSetLocation`（`0x8219FC3C`）映射到 `82470DA8`。它调用 `822FA548`：

| 寄存器 | 值 |
|---|---|
| r3 | `*(0x83318744)`，world |
| r4 | Actor / Pawn |
| r5 | guest 内存中的 FVector 指针，三个大端 float |
| r6、r7、r8 | 0，与 execSetLocation 相同 |
| 返回 r3 | 非零表示成功 |

`822FA548` 读取并更新 Actor 的 `+0xF8/+0xFC/+0x100`，不是凭 UE3 常见偏移猜测。地图现场值为 `(-2868.7700, -3599.7004, 175.2687)`。反编译证据在 `out/teleport-native.txt`、`out/teleport-move.txt`。

调用该原生函数保留了碰撞检查、附着对象移动、组件与触碰更新；后端不直接写 Actor.Location，不强制无碰撞。目的地可能被拒绝或调整，状态/日志以原生返回和实际坐标为准。

后端使用一次分配的独立 guest 堆内存传递 FVector，在调用前后保存/恢复完整 PPCContext。原生返回后重新核对当前玩家身份，以处理 Touch 回调触发的场景变化。

## World / Level 身份

`822FA548` 的生成代码确实通过 world `+0x50` 取得另一个对象，再访问其 `+0x3C/+0x40` 数组数据/数量，并读取数组首 Actor 的世界属性。后端使用该 **Level 上下文指针** 与 world、pawn 一起识别场景变化。

将它称作 `PersistentLevel` 与该 UE3 对象结构吻合，但本轮没有用反射字段表独立确认这个字段名。实现只依赖已观察到的指针身份，不依赖名称。不能声称同一 world/level/pawn 内的所有剧情变化都会由此检测到。

## 游戏线程与禁用条件

两次初始 hook（`82388BE0` / `82384D78`）均未在地图现场收到调用。进一步读原生注册字符串确认 `82384CC0` 实为 `intAbhHUDexecBattleUpdate`，因此 `82384D78` 是战斗 HUD 路径，不能将静态战斗调用链称作公共游戏 tick。初版笔记的这一判断已撤回。

修正入口 `82290B60`：PID 21524 的 `*(0x83315FB4)` engine=`0x350E600`、vtable=`0x8200D128`，虚表 `+0x108` 实际指向该函数。该函数接收 f1，调用 `8231EE68`（world 更新、切图处理、全局计数递增），返回后检查延迟的地图 URL。证据为 `out/teleport-engine-tick3.txt` / `out/teleport-engine-tick4.txt`。hook 保存调用前 this 等于当前全局 engine 的判断，原生返回后才执行传送 Tick。

该入口构建通过 `out/teleport-engine-hook-build.log`。主代理后续现场确认 hook 实际执行：3.602 秒日志 this=`0x351E600`、caller=`0x822902A0`，命令文件开始得到响应。该证据确认进入了实际引擎路径；具体传送结果单独验证。

- `0x832CB6B4` 属于 bhHUD / battlecore 的状态字段（bhHUD+0x15D4、battlecore+0x15CC），不能称为普适地图类型：Hypocenter 可行走现场为 0，Wasteland 战后为 10。当前仅放行已观察的 0/10，并结合 Pawn/控制器和 PlayerWalking / PlayerIdling 判断；战斗路径使用 2。单独该字段不能证明玩家可控制。
- 玩家链或 Pawn.Controller 关系无效、角色待销毁标志出现时禁用。
- world/level/pawn 变化或 tick 超过 1 秒未更新，取消请求并令记录位置失效。
- UI 仅读快照和提交请求；不从窗口线程读写客体对象。

菜单 A/B 已确认：bhHUD 状态和 `PlayerWalking` 均可保持不变。因此后端另检查原生禁控标志（见下文），不能把 bhHUD 值或 Pawn 存在等同于接受输入。剧情、暂停及其它地图仍需实际回归。

`intAfcControllerexecSetPlayerControl` → `829EB0D8` → controller 虚表 `+0x49C`，参数为两个 bool。PID 40964 的地图控制器该虚函数为 `82A1C0C8`：bEnable 为真选 `PlayerWalking`，否则选 `PlayerNoControl` 并重置运动。证据：`out/teleport-control.txt`、`out/teleport-control-flags.txt`。

当前后端额外允许状态名精确为 `PlayerWalking` 或 `PlayerIdling`：`822A95C8` 的状态名 getter 提供 `controller+0x18 → frame+0x1C → state+0x2C` 的 FName，`822A9668` 提供名字表 `*(0x833690D0)`、数量 `*(0x833690D4)`、条目文本 `+16` 的 UTF-16BE。读取时检查数组界限，编号后缀必须为零。没有固定动态 FName 编号或调用状态切换函数。

PID 40964 现场为 controller `0x3096E00`、state `0x18D1D98`、FName `(0x32A5,0)`，表 `0x33E0000` 的对应条目实际读出 `PlayerWalking`。这些地址仅用于证据。此门禁明确拒绝 `PlayerNoControl`。随后 PID 19040 在 45.053 秒曾记录 available=true，站立后只读脚本读到 `PlayerIdling`：Walking-only 会导致可用性变化并反复清除记录坐标，因此实现扩为上述两个已确认状态的并集。游戏菜单仍可保持 `PlayerWalking`，所以控制状态名单不能替代下文的菜单禁控标志。

## 后台集成验证入口

设置 `LO_TELEPORT_COMMAND_FILE` 为独立测试目录中的文本文件，序号严格递增：

```text
1 status
2 save
3 offset 50 0 0
4 restore
5 absolute -2868.77 -3599.70 175.27
```

每次文件只保留一条命令；等日志完成后再写下一条。命令使用同一 UI 请求接口，不注入系统键鼠。`status` 输出可用状态、当前位置、是否有记录位置；`native result` 输出目标与实际坐标。上述坐标只是旧现场示例，不作为所有地图的默认目的地。

输入和偏移相加后的目标同时限制为有限数值、每轴绝对值不超过 `MaxTeleportCoordinate=1000000`；这是人为调试保护，避免极大输入破坏碰撞计算，不是逆向得到的引擎世界范围。

最低回归：地图记录→小位移→恢复→重启读档；战斗及游戏菜单拒绝；切图后旧记录不可恢复；不合法/非有限坐标拒绝；墙内目标由原生碰撞处理。完成后由实际运行者追加目录、截图和日志结果。


## 游戏菜单禁控现场与原生证据

PID 29764 的菜单关闭/打开只读快照保存为 `out/teleport-menu-baseline.json`、`out/teleport-menu-open.json`，差分 `out/teleport-menu-diff.txt`。菜单不改变 PlayerWalking，bhHUD 状态仍为 0，所以仅靠前述两者无法禁止菜单内传送。

关键变化为 controller `+0x5C8` 的 `0x20` 位由 0 变 1，controller `+0x639` 字节由 0 变 1。原生 `82A18080` 的确把 bool 参数写入该位；进入禁控分支时将 Pawn `+0x54` 保存到 controller `+0x638`，通过原生虚调用改变 Pawn 的物理状态，并记录 `+0x639` 模式；离开时恢复已保存的物理状态、清模式。`82A151D8` 查询该位及模式，`8235FCA0` 在查询为真或模式非零时跳过对应控制分支。完整只读反编译为 `out/teleport-menu-control.txt`。

当前实现已整合保守门禁：`controller+0x5C8 & 0x20` 或 `byte(controller+0x639)!=0` 时拒绝，并与 Walking/Idling 名单、Pawn 双向关系共同判断。主代理正在 `out/teleport-control-verified/` 复测；本节的静态与菜单差分证据不等同于整合版已通过菜单拒绝测试。

## 已完成的后台游戏验证

整合版目录 `out/teleport-control-verified/`，运行时和测试副本一致；使用隐藏SDL窗口、进程内输入与传送命令文件，没有桌面键鼠或窗口激活。

- 63.901秒记录 `(896,1008,41.921627)`，65.899秒原生传送到 `(1046,1008,41.921627)` 返回成功；87.096秒仍保持X/Y，Z由地面物理调整到44.07163，未被旧位置覆盖。
- 89.107秒返回记录点，原生实际坐标与记录完全一致。截图 before.png、offset.png、restored.png。
- 打开游戏菜单后出现 `game menu/control suspended`，96.638秒偏移请求accepted=false。关闭菜单后旧记录已失效，restore被拒绝。
- 超限坐标1000001被拒绝；137.442秒绝对坐标 `(896,1060,50)`传送成功。
- 随后通过游戏输入正常行走，143.442秒坐标变为 `(1274.8342,1086.3708,46.09128)`，截图walk-after.png，说明物理/移动继续执行。

F1菜单代码构建通过；本轮依用户约定没有打开前台菜单，测试命令走相同请求接口。战斗与教学边界实测见下一节；尚未验证所有跨区域切图。

### 战斗/教学边界与最终构建

同一进程在传送恢复后正常走过残骸、取得戒指并到下一场遇敌。301.051秒戒指教学期间available=false。400.270秒在地图重新记录位置；404.835秒进入战斗HUD状态，429.471秒offset被拒绝，431.470秒restore被拒绝，433.468秒status确认available=false、bookmark=false。证据battle-disabled.png及run.log，没有通过传送修改战斗角色。

实测后端二进制SHA256为 `B5772F7F22E337938A2FE808A776D0816548D1F0A0196ED60FE621681F983D9D`。最终仅将坐标输入框初值由0改为空，避免未填坐标就传送到原点；最终构建 `out/teleport-final-build.log` 通过，SHA256 `46D132782BA1E6615DC2995114A35FFE3F06760FEE94D06169E746E8328D2761`。F1界面没有在前台打开，后台命令通过同一请求路径测试。尚未验证全游戏地图、任意墙内目标和全部演出状态。

## 当前地图 POI 列表（2026-09-04）

在F1传送区加入POI下拉列表、落点坐标/距离预览和“传送到此POI”。仅枚举当前world已经加载的Level，不提供跨图加载；每秒刷新，传送执行时重新确认Actor仍在加载列表中。菜单线程只读取快照、提交不复用的POI编号。

数据依据来自后台只读枚举：UWorld+0x44/+0x48是已加载Level数组；每个对象的Class（UObject+0x34）名称为Level，其Actor数组在+0x3C/+0x40。Actor的Outer(+0x28)反指相应Level，FName在+0x2C/+0x30。Hypocenter实际加载Entry及u11_0_mapw/scrw/camw/sndw/colw/navw/lvdw八个Level；只枚举world+0x50会遗漏地图POI。

识别类：fgGimmickSavePoint（存档）、fnMapJumpPoint（出口）、PlayerStart（入口/落脚）、fgGimmickAttackPoint（机关）、fgGimmickTouchPoint及fiItem（拾取/触碰）。不把动态提示fpNoticeActor、灯光、体积和任意dummy点当POI。Entry的备用PlayerStart在Z=-14924，已排除；零坐标占位点、待删除对象、不合法坐标也排除。

存档标记使用效果/地面原点，例如(896,1008,-54.846)，不能直接当玩家落点；优先匹配附近fdDummyPoint。出口优先匹配附近PlayerStart，例如Hypocenter出口对应(9982.069,-3972.467,61.457)。其他标记上移100游戏单位作为候选。若原生SetLocation拒绝，最多检查128/256半径各8个候选，保留碰撞，不强制穿模；全部失败则提示拒绝。上移和搜索半径是debug落点策略，不是推断出的引擎字段。

数据不是全收集清单；尚未加载的区域、脚本尚未生成的道具和未识别类别不会出现。通用编号是调试标签，不冒充本地化道具名。POI到达仍可能触发遇敌/剧情/切图。

第一轮out/poi-verified列出20点，存档POI传送成功；机关标记被碰撞拒绝且角色保留原位，促成附近落点搜索。

### POI 整合版后台验证

目录 `out/poi-landing-verified/`，使用独立save/profile副本、隐藏窗口及进程内命令；没有桌面输入或窗口激活。`run.log`与截图保留以下证据：

- 38.653秒列出Hypocenter的20个POI。133.858秒传送到机关点，原坐标被碰撞拒绝后，在128半径找到落点 `(6211.241,150.47495,121.66301)`；截图 `interaction-poi.png` 出现原生 A Ram 提示。
- 149.857秒返回存档POI成功，截图 `save-poi.png` 出现 A Save 提示。原生落点使用执行时新读到的anchor，而非首次列表中的旧坐标。
- 167.291秒打开游戏菜单触发禁控；175.017秒POI请求被拒绝，185.416秒列表为空。关闭菜单后205.018秒旧编号33仍被拒绝；213.019秒新列表为20点，编号从41起，不复用之前的21–40。
- 228.220秒传送到出口附近成功，目标 `(9982.069,-3972.4673,61.456505)`，原生碰撞将实际Z调整到76.17852；截图 `exit-poi.png` 可见出口通道和站立角色。未把“到出口附近”等同于已完成切图。

最终构建 `out/poi-final-build.log` 成功，SHA256 `2D9A5696DDD48B77593792FF61D2BBBD89F566F319940D0E26D5D488BD492BD5`。相对实测版本只增加菜单窗口高度，避免底部说明裁切；后端一致。F1窗口未前台打开，后台命令走相同请求接口。尚未验证全游戏地图和全部类型落点；此列表只保证当前已加载、已识别的候选点，实际传送仍由原生碰撞与控制状态决定。
