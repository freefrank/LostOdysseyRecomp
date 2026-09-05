# Debug 地图 ID 和本地化名称（2026-09-05）

F1 面板显示数字 ID、内部包名与当前语言的地图名称。没有匹配、加载期间存在多个不同 ID 或游戏线程超过两秒未更新时显示“加载中或尚未识别”。不会用保存预览中的地名冒充当前地图。实现为 `debug/map_info.cpp`，由现有 `82290B60` 引擎更新 hook 调用，窗口只读取加锁的字符串快照。

## 数据依据

- 原生定义插入 `82A0D648`：r4 是地图 ID，r5 是行结构，+4 为地图包名 FString。原函数始终继续执行；hook 仅记录 ID/包名对应关系。
- 当前 world 在 `0x83318744`，Levels 数组在 +0x44/+0x48。沿 Level 的 UObject Outer（+0x28）读取包名，使用原生定义匹配。FName 数组在 `0x833690D0`/+4，索引在 UObject+0x2C，文本在名字条目+16。
- 本地化地图名 FString 数组在 `0x832C9728`/+4，以原生 ID 索引，每项 12 字节。现场同时核对 ID 1 Battlefield、2 Hypocenter、3 Edge of Wasteland、4 Gorge。字体资源行号 10/11/12 不是这些地图的数字 ID，已排除该错误来源。
- 包名小写匹配；未匹配包不猜测 ID。读取在游戏线程原生 tick 返回后执行，每 250 ms 更新一次。未写客体地图状态。

## 实测

全部使用独立 save/profile 副本；不更改用户原始存档。

- `out/map-info-current-01/window.log`：24.643 秒读取 ID 4 / `u12_0_scrw` / Highlands of Wohl - Gorge，实际窗口文字与地图场景一致。
- `out/map-info-hypocenter-01/run.log`：标题阶段窗口显示未知；77.904 秒读入 ID 2 / Highlands of Wohl - Hypocenter。
- `out/map-info-hypocenter-01/final.log`：最终构建 24.965 秒显示 ID 2 / `u11_0_scrw`；实际点击出口 POI 并正常向前行走，111.691 秒更新到 ID 3 / `u13_0_scrw` / Highlands of Wohl - Edge of Wasteland。已检查切图后的窗口文字、可用控件和角色位置。
- 最终构建日志 `out/map-info-label-build.log` 通过。显示标签按需更新，避免每帧刷新同一字符串。

测试仅覆盖上述开场区域和当前英文运行语言，未宣称全四碟、全部语言或所有流式加载组合通过。该功能从游戏内表取名称，没有另建未经验证的翻译表。调试窗口交互与 F1 自动按键限制见[传送笔记](debug-teleport.md)。
