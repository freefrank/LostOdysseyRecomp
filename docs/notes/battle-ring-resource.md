# 戒指战斗资源名缺少语言后缀（2026-09-04）

当前汇总见[成果报告](../WORK_REPORT_2026-09-05.md)和[状态总表](../STATUS.md)。下文按实验时间保留证据，早期未完成状态不代表最新结果。

> 2026-09-05 同步：本文保留逐轮取证记录；相关新增代码仍有本地未提交部分，发布范围见[当前状态](../STATUS.md)。历史 PID 和测试中状态不代表进程仍在运行。

## 当前结论

已修复 `guest_printf.cpp` 对宽格式 `%s/%S` 的解释错误，并通过 44 项离线回归。故障是格式器生成错误资源名，随后游戏进入资源查找失败的致命处理；没有证据表明这次是光盘损坏或 SHA1 验证失败。

最终联合回归已完成普通攻击、反击和自然胜利，详见文末；Ring 外环渲染仍未修复。首轮现场目录为 `out/walkthrough-ring-fixed/`。本轮没有绕过资源查找错误或 DirtyDisc 分支。

## 两次修复前复现

1. `out/walkthrough-hypocenter/run.log`：从已修复的 Hypocenter 存档副本前进，Ram 和完整戒指教学通过；出口附近进入战斗编号 4。主角待机正常，确认攻击后 phase 2→4→5→6→7→8，503.205 秒出现 `dirty disc error UI requested`，503.215 秒 `launch title ''`。`shot_15000.ppm` 是随后的黑屏，GPU 仍交换帧但只剩约 2 draws/frame。
2. `out/walkthrough-disc-trace/run.log`：打开完整内核日志再次复现，404.621 秒 `guest disc failure caller=0x828204a4`。主线程栈包含 `82821294 → 8282140c → 8237d308`；报错线程是另起的线程，因此只抓报错线程不能看到原始资源名。

场景发生在获得 Bruiser Ring 后的后续战斗，不是先前已修复的开场资源配置 11→0 问题。此前 [遇敌笔记](encounter-animation.md) 中“未发现 FPD 读取失败”的判断仅适用于当时的场景。

## 现场对象与致命路径

主代理从 PPC 栈恢复调用者：保存的 LR `8237D308` 位于 guest `0x2000e8`，同帧保存的 r31 低字位于 LR 槽前 4 字节。失败对象为 `0x52d0c00`，FString 数据指针 `0x1464f50`、长度 17，内容是 **`RPMenuResBattle_`**（含末尾 NUL）。管理器为 `0xecdc00`，其虚表 `+8` 实际函数为 `823B0E98`。这些对象地址仅为该次进程现场，不能硬编码。

`8237D258` 在对象 `+16` 尚未初始化时，取 `*(0x83302A18)` 管理器，调用虚表 `+8`：输入名称来自对象 `+4/+8`，栈上空 FString 作为输出。返回 0 时，从 `8237D304` 调用 `828212E0`，返回地址为 `8237D308`。

`828212E0` 完成停止/淡出等处理，在 `828213F8` 创建入口为 `82820488` 的线程，主线程随后永远等待。该线程经 `82BE1C00` 请求 `XamShowDirtyDiscErrorUI`，然后调用 `XamLoaderLaunchTitle(0,0)`。`827CA048(33)` 是休眠调用，不能把 33 当作资源错误码。

另一个 DirtyDisc 调用者 `825F72C0` 比较 20 字节摘要，但本次明确没有走该分支。当前 `debug/asset_failure.cpp` 只记录 fatal 原始调用者及该资源查找分支的名称，不修改客体结果。

## 资源名生成与根因

镜像中的 UTF16BE 格式串：

| 地址 | 格式 |
|---|---|
| `0x820C7100` | `RPMenuResBattle_%s.feelring` |
| `0x820C7138` | `RPMenuResBattle_%s.BATTLE-LOG` |

`82B16310` 在 `82B1634C`、`82B16364` 调用 `822A9748` 格式化它们，`%s` 参数来自 `82341C40` 返回的 `0x83238A28`。镜像初值和现场读取均为 UTF16BE **`int`**。`822A9748` 将寄存器参数保存到 8 字节槽，直接调用 `__imp__vswprintf`（返回地址 `822A9780`）。因此正确名称为 `RPMenuResBattle_int.feelring` / `RPMenuResBattle_int.BATTLE-LOG`，对应包名是 `RPMenuResBattle_int`。

旧 `GuestFormatVaList(..., wideFormat=true)` 仅用 wideFormat 解码格式串，内部 `Format` 没有保留宽格式上下文，默认 `%s` 仍按 `char*` 读。UTF16BE `int` 第一个字节为零，后缀因此被读为空串。旧实现的独立测试实际生成了 `RPMenuResBattle_.feelring` 和 `RPMenuResBattle_.BATTLE-LOG`，与现场失败包名吻合。

语义已对照本地 Xenia `src/xenia/kernel/xboxkrnl/xboxkrnl_strings.cc` 的字符串分支：默认 `%s` 跟随格式函数宽度，`%S` 反转，`l/w` 强制宽串，`h` 强制窄串。修复将宽格式上下文传入公共 `Format` 并按上述规则选择字符串编码；不扩展 Unicode 支持或其他 printf 特性。

## 离线验证

新增 `tools/tests/guest_printf_test.cpp` 和 CMake 目标 `LoGuestPrintfTest`。测试分别经过真实格式器的 va_list 与寄存器入口，包含：两个实际战斗格式、窄/宽 `%s/%S`、`h/l/w` 与大小写组合、宽度/精度及空指针。

```powershell
.\tools\build_runtime.bat LoGuestPrintfTest
.\out\build\windows-clang\LostOdysseyRecomp\LoGuestPrintfTest.exe
```

- 改修复前先运行新测试：44 项检查、14 失败，包含两种实际资源名丢失 `int`。
- 应用修复后：`Guest printf: 44 checks, 0 failures`。
- 运行时及测试目标构建成功；`git diff --check` 通过。上述测试输出记录在本次工具调用中，没有另造旧日志文件路径。

## 游戏复测待补

`out/walkthrough-ring-fixed/` 使用后台模式及 `LO_TRACE_PRINTF`，仍使用独立存档副本。至少确认：资源格式化输出包含 `int`；得到戒指后战斗正常执行并自然结算；能离开 Hypocenter 到下一存档点，并独立重启读回。仅无错误日志不足以判定全部通过，需截图和状态变化共同确认。

## 修复后首轮游戏复测与第二处阻塞

`out/walkthrough-ring-fixed/` 从同一存档副本出发，此次随机遇敌编号为 3（先前两次为 4）。203.867 秒实际 vswprintf 输出已包含 `RPMenuResBattle_int.BATTLE-LOG` 和 `RPMenuResBattle_int.feelring`；不再走 DirtyDisc。随后205.370秒出现 `call to unrecompiled guest function ctr=0x82b14830 lr=0x82b146bc`，继而战斗更新读低地址0x240崩溃，不能把最后一张截图未变化误认为角色又卡住。

根因是82B146A8漏配switch：base82B14808、索引r10、default82B148BC、四目标82B14830/82B14878/82B14890/82B148A4；原表在镜像82B14820。旧生成代码把bctr当函数调用并return，绕过82B14A10的r1/f26-f31/r31恢复，破坏调用者栈。崩溃时r30=0x200，随后构造出错误battle core=0x208。82B145E0只是前一个switch留在CTR中的目标，不能误当函数根因。

此处修复必须补配置并重新生成，不能给battle hook加空指针保护掩盖。最终游戏回归结果另补。

### 跳转表构建核验

已在 switch_tables.toml 与 switch_tables_manual.toml 同步补入表项，通过现有生成流程重生成，未直接修改产出cpp。82B146A8中的2个bctr都生成本地switch、0个未解析间接调用；82B14A10的栈恢复保留。逐函数比较仅82B146A8改变，函数映射未变，其他249个生成文件哈希一致。

生成的ppc_context.h也同步为当前XenonUtils源码，对应仓库既有补丁的timebase/half实现；此前生成头没有匹配备份，无法逐行对比旧内容，不能声称头文件未变化。运行时构建成功，日志out/ring-switch-regenerate.log、out/ring-switch-build.log。游戏最终复测目录out/walkthrough-switch-fixed/。

## 两项修复联合游戏回归

最终 `out/walkthrough-switch-fixed/` 以隐藏窗口运行，编号4战斗完成3次正常攻击、2轮敌方反击，316.240秒phase9→11，奖励75G并返回地图。保留victory.png及run.log；没有debug判胜。随后离开Hypocenter进入峡谷通道，并再次正常遇敌。首次戒指攻击资源失败及遗漏switch栈破坏的复现路径均已通过。Gorge存档目标因用户新增传送需求暂缓，不能标为保存/读回通过。

Ring现场更新：82B15960负责RT输入驱动的feelring状态，82B146A8更新环参数；原始导出out/ring-update.txt与out/ring-state.txt。2026-09-05最新后台Hypocenter测试进入遇敌但在攻击前query设备指针损坏崩溃（third-map-hang.md），因此外环仍无实际RT覆盖。不要把旧replay的READY FOR ATTACK文字当成菜单截图或测试通过。

## 2026-09-05：RT 外环首次实际覆盖

构建E9174CB3A5A55C7A53F68C34AE8808F4E4F075135B941332856D354DD7A4118A，`out/query-watch-01` PID18768，Hypocenter独立存档按既有replay进入两名Insane Khent Soldier战斗。shot5249完整指令菜单，6827攻击目标菜单。后台input20确认目标，input21注入LT0/RT255共240polls，日志255.988秒确认接受。shots4捕获连续180帧7607..7786。

`rt-contact.png`及`ring-visible.png`(swap7687)明确显示中央判定环和外围装饰环；外围环在7679/7687/7703等帧呈不同半径，证明实际绘制及动画变化。长按至超时得到Bad，随后95点伤害。此前“未执行RT、外环仍未覆盖”已被本次证据更新：当前构建这条复现路径外环可见；尚未验证精准释放得到Good/Perfect，也未对比本次与更早未显示构建的唯一差异，不能将其归因于本轮query诊断。既有资源格式化/switch修复已包含在构建内。

本次未再次出现34792的query空指针崩溃，不能据单次成功宣称崩溃修复。查询watch独立启用，真实帧率截图期间最低记录约22.8fps；外环存在不依赖对帧间速度的推断。当前18768保留战斗现场，input21、shots4已完成，未提交。

## 2026-09-05：第二场遇敌 RT 释放获得 Good

继续同一后台PID18768，没有重启或修改存档/判定内存。Hypocenter第二次攻击自然胜利，shot18686奖励50G；关闭结果后普通步行进入Edge of Wasteland（22553），继续步行触发三名敌人，24726完整攻击菜单。

由原始82384Dxx→82388BE0→8238A5C0调用链确认管理器832CA0E0，+5580为feelring对象指针。只读采样脚本out/sample-ring-state.py记录642行：本场+0x44的101项表中0..87=Bad、88..97=Good、98..100=Perfect。+0x18是0..100往返进度，RT255时上升约118单位/秒，+2为流程状态，+4为输入状态；没有改动这些字段。

out/release-ring.py仅通过已有后台输入文件发送31/32确认攻击、33=RT255、34=RT0零时长释放。在进度91.6058请求释放，实际停在92.4529，状态进入6/2（Good），832CA0DC首字节=1。shots13连续25615..25704，ring-release-contact.png及shot25645明确显示Good，shot25700显示101伤害。外环可见、半径变化、释放判定与后续攻击均有实际画面，覆盖Hypocenter及Edge of Wasteland遇敌。Perfect未实测，但不再把释放是否可用列为未知；不将原本可工作的Good路径描述为本轮新增代码修复。

当前input34已释放RT，shots13已完成，进程保留在该战斗。未触发query写监视或崩溃；这是经过一次自然结算和再次遇敌的回归结果，不能宣称所有偶发GPU查询故障消失。
