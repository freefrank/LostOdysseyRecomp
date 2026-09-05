# 攻略路线与后台推进测试（2026-09-04）

## 来源与状态

用户提供的 [IGN Walkthrough](https://www.ign.com/wikis/lost-odyssey/Walkthrough) 已实际尝试访问，浏览工具返回不可重试错误；没有读取到正文，因此以下不能称为 IGN 原文摘要。

路线主要参照可访问的 [Freeola 攻略](https://walkthrough.freeola.com/game/9481/xbox-360/lost-odyssey.html)，并以 [Multiplayer.it 攻略](https://multiplayer.it/soluzioni/la-soluzione-di-lost-odyssey.html)、[BeastieGuides 的 Wohl 段落](https://www.beastieguides.de/lost-odyssey-komplettloesung/hochland-von-wohl/)交叉核对。不转载攻略全文，也不追求全收集。

当前本地进度以 [交接](handoff.md) 和 [遇敌修复](encounter-animation.md) 为准：已验证 Hypocenter 后续战斗正常攻击、自然胜利、覆盖保存和独立进程读回。后续区域在本笔记创建时均**尚未由本次测试验证**。本笔记的检查点是测试计划，不是通过报告。

## 第一目标：Hypocenter → Gorge 存档点

1. 从 Hypocenter 存档点沿主路前进。若再次遇到 Insane Khent Soldiers，正常攻击完成至少一战，检查主角进场、敌方受击和回到地图；记录战斗编号，不假定均为已兼容的 0x139。
2. 战斗后接近狭窄通路旁的大型圆筒/坦克残骸。出现 **Ram** 提示时按 A，清路并取得 **Bruiser Ring**，随后处理戒指教学。镜头变化会改变屏幕左右，按残骸和提示定位，不固定长时间向某一方向走。这是地图互动门槛，停在这里不等于游戏死锁。[路线与清路依据](https://www.beastieguides.de/lost-odyssey-komplettloesung/hochland-von-wohl/)
3. 沿窄路继续，经过其他可撞残骸；出口附近左侧地面发光物是可选 **Name Plate**。进入 **Edge of Wasteland** 后沿通道前进，路边木箱可取得补给。抵达 **Gorge** 后先保存，再接近士兵触发剧情。注意避开熔岩；不把环境扣血记为存档或战斗异常。[区域与存档顺序](https://multiplayer.it/soluzioni/la-soluzione-di-lost-odyssey.html)

| 检查点 | 最少证据 | 通过条件 |
|---|---|---|
| 新一场战斗 | 进场、攻击、结算截图及日志 | 动作执行，正常结算并返回地图 |
| Ram 与教学 | 提示、道具/教学、道路截图 | 残骸发生变化，教学可退出，角色能通过 |
| Edge of Wasteland | 区域名与游戏截图 | 场景装载完成，无持续白屏/停滞 |
| Gorge 保存 | 存档列表与内容写入日志 | 保存返回正常，位置显示 Gorge |
| 新进程读回 | 独立重启截图 | 回到 Gorge，剧情进度与角色状态保留 |

敌人 T 姿势与光影闪烁单列为未决问题：同一镜头保留多帧，分别观察地图敌人与战斗敌人；一次截图无法证明闪烁已经消失。戒指 Perfect 并非本段通行条件，RT 时机输入可后续单独测。

## 第二目标：营地 → Uhra → Central Station

Gorge 士兵剧情之后进入营地，处理 Ring Assembly 教学，向装甲车前进并上车。车内走向另一端的 Seth 推进剧情。到 **Great Gate of Uhra** 后进入塔楼，乘中央升降梯，上单轨列车；车内继续走向另一端触发抵达城市。到 **Central Station** 后乘梯下楼，可在该处保存。后续主线是经广场向东进入 Main Street、议会剧情，再去 Tolsan's Inn。[车内触发与城市路线](https://walkthrough.freeola.com/game/9481/xbox-360/lost-odyssey.html) [中央车站存档依据](https://multiplayer.it/soluzioni/la-soluzione-di-lost-odyssey.html)

推进时每次只验证一段：Gorge 剧情→营地、上车→Seth、Great Gate→塔楼升降梯、单轨列车→Central Station。观察新角色动作、分屏/实时演出、升降平台碰撞、字幕/菜单关闭、切图后的输入恢复；停在车厢里先检查人物位置和互动提示，不能只等待或连续确认。

## 后台执行约定

2026-09-05：以下为独立后台测试方式，不再是权限限制。用户已允许桌面/前台测试，仍保留原始存档。

- 只启动独立测试进程，复制 save/profile 到 out/ 下的测试目录；不写用户原始存档。
- 只用 `LO_TEST_INPUT_FILE` 向进程送输入，读取游戏自身截图与日志。不激活窗口，不注入系统键鼠，震动保持关闭。
- 输入文件格式：`serial hexButtonMask leftX leftY polls [LT RT]`。原有5字段兼容（扳机为0）；可选LT/RT必须成对提供，范围0..255，越界会钳制。序号递增；A=`1000`，B=`2000`，Y=`8000`。例如 `10 0 0 0 60 0 255` 持续RT共60次轮询，结束自动释放；`11 0 0 0 0 0 0` 在下一次文件采样时释放。持续时间上限6000次轮询，计数不是毫秒。短脉冲后等待界面完成，再读取截图决定下一步。
- 不用固定的长按脚本跨越未知切图。每次遇敌先放开移动；普通战斗至少自然完成一次，再考虑 debug 判胜加速其他重复战斗。
- 每个新存档点保留可重启副本，记录二进制版本、环境开关、区域、截图路径和发现的问题。未实际读回不标为存档回归通过。

## 本次执行记录

最初的攻略整理阶段仅检索资料；之后已追加实际后台执行。以下按时间保留，不以早期尚未到达的记录覆盖后续结果。

### 第一轮实际结果

- 从 `out/overwrite-final-reload/` 的 550G /00:11 副本出发，运行目录 `out/walkthrough-hypocenter/`。新增 `LO_BACKGROUND=1` 让 SDL 窗口从创建时即隐藏；保留 D3D12 渲染和游戏截图，进程 MainWindowHandle=0。输入仍通过 LO_TEST_INPUT_FILE。
- Hypocenter 地图移动与镜头切换通过；残骸出现 Ram 提示，A 触发动作、发光道具和 Target Ring System/What are rings 教学；教学可翻页并返回地图。没有用 debug 判胜推进。
- 接近出口的下一战编号 **4**，主角正常待机，未触发 0x139 遗留状态恢复。选择 Attack 并确认目标后，phase 2→4→5→6→7→8；503.205 秒触发 `dirty disc error UI requested`，503.215 秒 `launch title ''`，随后黑屏且仅2 draws/frame。
- 本次黑屏发生在客体致命退出路径，不标为普通动画或渲染白屏回归。未到达 Edge of Wasteland / Gorge，未完成下一存档点验证。
- 证据：`run.log`、`shot_14880.ppm`（攻击目标阶段）、`shot_15000.ppm`（黑屏）。完整读取日志与资产失败捕获复现在 `out/walkthrough-disc-trace/`。原始用户存档未改动。

### 推进中定位的修复

戒指首次攻击的资源名丢失语言后缀，已修正宽printf语义，44项离线回归通过。游戏确认int后缀恢复后，又暴露戒指逻辑缺失switch造成栈破坏；见 [戒指资源与重编译调查](battle-ring-resource.md)。当时尚未越过这一战；最终联合回归见下一节。

### 修复后的实际推进

`out/walkthrough-switch-fixed/` 使用最终跳转表修复版（SHA256 `F5E0312C7056312EB8CB1251A5504C5BFA962E4BCEA68132E008D0640456F7C1`），战斗编号4通过3次普通攻击与2轮敌方反击，在316.240秒进入胜利结算，获得75G。证据为该目录run.log、victory.png。没有使用debug判胜。结算后离开Hypocenter进入峡谷通路并继续行走；区域名尚未用菜单确认，不能据画面直接标记Gorge。

用户观察疑似卡住并提出人物传送需求，暂停路线推进以优先实现。最后一次移动实际上已触发下一场随机战斗；游戏仍能进入指令选择。Gorge保存及新进程读回仍未完成。

第二场峡谷随机战斗也通过3次普通攻击自然胜利（1137.823秒进入phase11）。结算与菜单明确显示 Highlands of Wohl - Wasteland，金币700、HP307/370，证据wasteland-victory.png。新进程传送测试需从最近存档副本重新加载；这段未到存档点的推进没有覆盖用户原始存档。

## 2026-09-05 最新路线范围

后续独立副本已从用户Gorge存档加载u12_0，沿普通路线触发士兵演出，进入营地并恢复移动/交互，详见[营地调查](third-map-hang.md)。上文“Gorge待验证”是当时记录；营地新存档读回、上车及后续城市流程仍未完成。
