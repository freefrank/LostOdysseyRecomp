# Issue #74：分队期间随时存档后无法换人（2026-09-29 调查）

[English](issue-74-save-anywhere-party-split.md)

> **调查记录。** 结论与待办只对应文中日期、版本和捕获；现行问题、交付与验收状态见[项目状态](../STATUS.md)和[路线图](../ROADMAP.md)。

[issue #74](https://github.com/freefrank/LostOdysseyRecomp/issues/74) 报告在 The Great Ancient Ruins（Astral Square，地图 244）分队期间用随时存档保存，读档后右下角的 "RB Change Character" 提示消失，RB 无反应。本轮用报告附带的 `user01` 存档和 USA/Europe 数据在隔离目录复现并做了逆向。随时存档的行为没有改动，只在 F1 菜单开关下方加了两行常驻警告；中英文布局用 `LoDebugOverlayTest` 离线渲染截图检查过。该测试在 main 上本来就会在后续步骤报 `concurrent menu actions closed overlay`，撤下本次改动后结果相同，与警告文字无关。

## 结论

换人功能由一段一次性剧情脚本开启，开启状态只存在于运行时，不进入存档。随时存档只是打开原生 System 菜单的 Save 项（见[随时存档笔记](save-anywhere.md)），存档内容与原生存档相同，所以问题出在"在原作不允许保存的时刻保存"，而不是存档写坏。读档后剧情标志和分队成员仍然正确：走到出口时 Cooke 会说要等队长那组，但 RB 换人不会回来。

已排除的恢复途径：

- 离开 Astral Square 到相邻的 Legacy of the Eastern Tribe 再走回来，提示仍不出现，RB 仍无反应。
- rpPlayData 的 `CmdDrawState` / `CmdEnableState` 位（`PlayData+0x31F7C..0x31F8B`，由 `SetCmdDrawState` 等原生接口经虚表 `+0x1C4..+0x1D8` 读写）确实不在 EasySave 复制进存档的区段（`+0x4C..+0x30BF8`），但把 64 位全部置 1 也不能让提示出现，说明换人开关不在这里。
- 当前关卡几乎没有 UE Kismet 逻辑（堆里只有 13 个 `SeqEvent_Touch` 实例，`fsSeqAct_DrawControl`、`fsSeqAct_KeyControl`、`fsSeqAct_CheckButtonInfo` 只有默认对象），换人逻辑应在游戏自带的事件脚本系统里。
- `rpBaseController.XPad_RB`（原生 `828173B0`）只在按下 RB 时被原生 C++ 以新建的 FFrame 调用，调用者不是 UnrealScript 函数，无法从脚本侧找到开关。

## 已确认的地址

| 项目 | 值 |
|---|---|
| rpPlayData 虚表 | `0x820108FC`，由 GEngine 虚表 `+0x160` 取得，与作弊功能同一路径 |
| EasySave 实现 | 虚表 `+0x220` → `82906E18`，复制 `+0x4C` 起五段数据 |
| MaskMember | 虚表 `+0x1EC` → `82908308`，位于 `PlayData+0x64`，会存档 |
| ChangeCtrlChara | 虚表 `+0x1DC` → `829087B0`，写 `PlayData+0x58/+0x5C`，会存档 |
| 提示文本 | "Change Character" 与 "Pick Up Firewood"、"Enter" 同在一张本地化 FString 表 |
| UFunction 反射 | UField `SuperField +0x3C`、`Next +0x40`；UStruct `Children +0x4C`、`PropertiesSize +0x50`、脚本字节码 `+0x54/+0x58`；UProperty `Offset +0x68` |

`PlayData+0x64` 的成员遮罩不能用来判断是否分队：报告存档为 `0x596`，Numara Palace 原生存档点的正常存档也有 `0x58C`。

## 后续方向

要么阻止随时存档在分队期间覆盖原生的 Save 禁用，要么在读档后重建换人状态，两者都需要先找到运行时开关。缺的是一个"可以换人"的活体状态：需要一份分队开始之前的存档，进入分队后与读档后的状态做内存对照。本轮临时探针（FName 检索、反射字段遍历、对象/字符串/指针扫描、`XPad_RB` 调用者记录）保存在 ignored 的 `out/issue74/issue74-probe.patch`，源码已还原。

## 2026-09-30 静态定位补充

上一节所说“缺的是一个可以换人的活体状态”是 2026-09-29 的运行时调查结论；本日已经在事件脚本中静态定位到开关和换组动作的候选位置。它缩小了后续对照范围，但没有证明读档后运行时状态为何没有恢复。

### 资产与解码证据

| 项目 | 静态证据 |
|---|---|
| 地图映射 | `worldinfo.xmb` 的 `MapID 244` → `ev5_0_scrw` |
| 脚本虚拟路径 | `bin\xenon\scr\scr\ev5_0_scrw.bin` |
| 盘内资产 | `D:/Mihoyo/LostOdysseyRecomp-windows-x64/game_us/disc1..4` 四盘均有相同资产 |
| FPD 记录 | `xenon_scr.fpd` offset `0xA35800`，stored `0x3588`；CPX 解码 `36828 (0x8FDC)` |
| 脚本 SHA-256 | `2e6c74bde83062fba5173482f65d6fe619e2e0ab84a8ee86a521e54935941561` |
| 可复查产物 | `out/issue74/script-locations.json`、`out/issue74/party-switch-locations.json`、`out/issue74/decoded-scripts/ev5_0_scrw.bin`（均 ignored，仅本地） |

脚本容器为 little-endian：每个 record 依次为 `id:u32`、`entrycount:u32`、`entries:u16[]`、`constcount:u32`、`constants:u32[]`、`codebytes:u32`、`code:u8[]`，共 148 个 record。record 0 的 codebase 为 `0x14D0`；record 1 的 header 为 `0x365C`，constantbase `0x3672`、count `63`，codebase `0x3772`、codebytes `0x362`。下表中的 PC 都是对应 record codebase 的相对位置，`fileoff` 是解码文件 offset，不是 FPD offset。

### 可复查的脚本位置

| 位置 | 静态内容 | 当前解释 |
|---|---|---|
| record 0, PC `0x1C65` / file `0x3135` | `03 34 10 6E 84` | `const[0x46E] = 1`，即写入 `ref 0x1034 = 1` |
| record 0, PC `0x1D5B` / file `0x322B` | `03 34 10 A1 84` | `const[0x4A1] = 0`，即清除 `ref 0x1034` |
| record 1, entry 1, PC `0x04` / file `0x3776` | 控制提示入口；PC `0x07` 比较 `ref 0x1034 == 1`，PC `0x0F` 比较 `ref 0x1036 == 0`，随后 `F1 0B` 检查控制器状态 | “Change Character” 提示的显示条件候选 |
| record 1, entry 1, PC `0x36` / file `0x37A8` | `F0 6D 00 05 80 06 80` | 创建 group 2，key 6，textID 24 |
| record 1, entry 1, PC `0x40` / file `0x37B2` | 创建 group 2，key 9，textID 24；两者由 `ref 0x0B07` 决定 | 两个提示变体；textID 24 已由 `xenon_loc.fpd` 的 `name_data.xml` 确认为 `Change Character` |
| record 1, entry 1, PC `0x47` / file `0x37B9`；PC `0x52` / file `0x37C4` | `F0 6E 01`；`F0 6E 00` | 显示 / 隐藏 group 2 |
| record 1, entry 4, PC `0x58` / file `0x37CA` | 换组动作入口；PC `0x82` / file `0x37F4` 写 `ref 0x1035 = 1`，PC `0x19E` / file `0x3910` 写 `ref 0x1035 = 0` | 换组状态候选，分别使用 `pc_010a0` 与 `pc_000a0` |
| record 1, entry 4, PC `0x2BF` / `0x30F` | 两处重排两组队员 | 换组动作的成员更新候选 |

原生 `core27` handler `828472F8` 调用 `PlayData` 虚表 `+0x1EC/+0x1F0` 等成员接口；opcode `ED`（`82A4E2C8`）以 32-byte 字符串切换角色模型。角色到人物名的映射尚未完成，不能据此猜测具体角色。

### 运行时边界

脚本 VM dispatcher 为 `829FDB40`（`context+0x494` 为 codebase、`+0x69C` 为 PC），handler table ctor 为 `829FF7C8`；`F0 6D` / `F0 6E` handler 分别为 `82A5FF28` / `82A5F650`。`F0 6D` 经 `823607A0` → `82360800`，其 `r4`、`r5`、`r6` 分别是 group、key、textID，说明 `2` 是分组而非固定槽。`ref 0x1034/0x1035/0x1036` 由 VM 读写器 `8229DDE8` / `8229E280` 映射到 `*(u32*)0x831F1B60` 指向的 VM manager：在 `scriptCtx+0 >= 0` 时，offset 分别为 `+0xF88/+0xF8C/+0xF90`。这证明它们是事件 VM 对象字段；该对象是否另有存档序列化路径仍未核实，因此此前“不进入存档”的判断还需要运行时或序列化证据。

同名对话资源为 `bin\xenon\scr\mes\int\ev5_0_scrw.jmd`（FPD offset `0x32B000`，length `0xE68`，解码 `11168` bytes）。zero-based row 10（`0x7C6`）描述 Seth 让 Kaim/Cooke 走下路、另一队走上路；row 20（`0xE4C`）有 Cooke 等 captain group 的提示，可作为场景脚本同源性的交叉线索。

本次只做了静态解包和定位，未修改 runtime、未 build、未实际读档验证恢复，也未复制游戏文件到 docs。下一次 runtime probe 应直接观察 `0x831F1B60` 指针目标的 `+0xF88/+0xF8C/+0xF90`，并记录 record 1 入口调度；不能再把泛查 `XPad_RB` 回调当作主要定位路径。

## 2026-09-30 运行时验证与恢复按钮

用上一节的静态定位做了运行时验证。探针读取 `*(u32*)0x831F1B60`，得到 VM manager `0x832C386C`，位于 XEX 镜像数据段，不在堆上。报告者的 `user01` 存档读档后，`+0xF88`（`ref 0x1034`）、`+0xF8C`（`ref 0x1035`）、`+0xF90`（`ref 0x1036`）都为 0。只把 `+0xF88` 写成 1 后，右下角立即出现 "RB Change Character"；按 RB 切到 Seth 所在的另一组，`ref 0x1035` 变为 1，再按一次切回 Kaim，`ref 0x1035` 回到 0。这确认了 `ref 0x1034` 就是换人开关，而且读档后不会恢复。

作为对照，把 rpPlayData 的 64 个 `CmdDrawState` / `CmdEnableState` 位全部置 1 后按 RB，`XPad_RB` 确实被读取，但角色不变、提示不出现，所以这组位与换人无关。

F1 常规页新增“强制开启 RB 换人”按钮，与随时存档并排。UI 只设置一个 atomic 请求，由引擎 tick（`82290B60` hook）在游戏线程把 `ref 0x1034` 写成 1，并记录原值。用临时文件触发调用同一个 `RequestPartySwitch()` 做过端到端验证：日志 `party switch armed (was 0)`，提示恢复，按 RB 换组成功；临时触发已删除。按钮本身的点击由 `LoDebugMenuInteractionTest` 覆盖，布局用 `LoDebugOverlayTest` 离线截图检查了中英文；两个测试都通过。

仍未覆盖的范围：只在 Astral Square 验证过。`ref 0x1034` 是全局脚本变量，在不提供换人的区域写入后，是否会被后续剧情读到尚未核实；分队结束时脚本会在 record 0 PC `0x1D5B` 把它清零。

### 分队期间停用随时存档

`save_anywhere.cpp` 的 `Apply` 在 `ref 0x1034 == 1` 时不再覆盖原生 Save 启用位，保留游戏自己的权限。玩家存档实测（随时存档开启）：读档后标志为 0，System 菜单 Save 可用；用同一按钮路径把标志置 1，退出菜单后重新进入 System，Save 变灰。这只防止新的分队存档；已经丢失换人的旧存档仍需上面的按钮恢复。原生存档点在分队期间的行为本轮没有单独测试。
