# 角色黑色剪影：物理地址别名与遮挡查询（2026-09-04）

## 根因

角色在深度预通道中出现，却没有提交材质 base pass。问题发生在 guest 的可见性判定，
不是像素着色器把已提交角色画成黑色。

旧 `Memory::Memory()` 把整个 4 GiB 当作互不相关的匿名内存。GPU 命令处理器把物理地址
映射至 `base + 0xA0000000 + physical`，但游戏的 D3D 遮挡查询从 `0xC0000000 + physical`
读取。两者本应是同一块物理内存，旧实现却让 CPU 永远读不到 GPU 更新的结果。
CPU 因而把角色标记为不可见，跳过材质 pass，只留下深度/后处理形成的剪影。

此前 `gpu.md` 中“遮挡查询已排除”的结论失效：调整 `LO_ZPD_MODE` 只改变 A 区写入，
对独立的 C 区读值没有影响；draw 数不变不能排除遮挡查询。

## 逆向证据

使用现有 Ghidra 项目，以 `-readOnly` 和 `tools/ghidra/ExportFunctions.java` 导出以下函数，
并对照生成的 PPC C++ 核实直接内存访问：

| 地址 | 行为 |
|---|---|
| `0x823BAB50` | 主渲染流程：InitViews、深度预通道、查询结果、base pass |
| `0x823CE960` | 全局 `0x83235AB4` 非零时处理遮挡查询；结果为零清除 view 可见性位 |
| `0x823CF390` | 轮询查询 GetData |
| `0x823CF3F0` | type 9 遮挡查询；通过 C 别名读取 END/BEGIN，字节交换后求差 |
| `0x823D0500` | base pass；检查 view 的可见性 bitset，不可见则不提交 primitive |

查询记录的 END 在 `+0x10/+0x14`，BEGIN 在 `+0x30/+0x34`。
view `+0x25C` 是可见性位图，`+0x274` 是 relevance 数组，`+0x2A4/+0x2A8` 是动态 primitive 列表/数量。
第 2400 帧，大量 skeletal primitive 的可见性位为零，而刚性地形仍可见。

诊断实验：仅在一次测试进程中将 `0x83235AB4` 从 1 改为 0，立即恢复角色材质提交和画面。
该修改不进入正式代码；临时 guest hook、`LO_NO_PRED` 实验分支均已移除。

本地 ignored 证据：`out/render-query-data.c`、`out/render-visibility.c`、
`out/render-relevance/run.log`、`out/render-relevance/shot_7500.png`。

## 修复

`kernel/guest_address_space.cpp` 建立连续 4 GiB host 地址范围，保持生成代码的 `base + address` 访问：

| guest 区间 | backing offset | 长度 |
|---|---:|---:|
| `0x00000000..0x9FFFFFFF` | `0` | `0xA0000000` |
| `0xA0000000..0xBFFFFFFF` | `0xA0000000` | `0x20000000` |
| `0xC0000000..0xDFFFFFFF` | `0xA0000000` | `0x20000000` |
| `0xE0000000..0xFFFFFFFF` | `0xA0001000` | `0x20000000` |

A/C 共用 512 MiB，E 向后偏移一页，与本地 Xenia `src/xenia/memory.cc` 的视图映射、
现有 HLE `MmGetPhysicalAddress` 一致。E 视图尾部需要额外一页 backing。
物理分配器只从 A 区分配，避免将同一物理页经多个别名重复分配。

Windows 使用页文件 section、placeholder reservation 和 `MapViewOfFile3`。
保留整个地址区间后拆分/替换，避免释放地址空间后再映射造成的竞争；失败时清理部分映射。
placeholder replacement 支持 E 视图的 4 KiB offset，需要 Windows 10 1803 或更新版本。
Linux 使用 memfd 和 `MAP_SHARED | MAP_FIXED` 覆盖预留范围。
两平台均保留 guest 第零页不可访问。

API 对照：[MapViewOfFile3](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-mapviewoffile3)、
[VirtualAlloc2](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc2)。

## 验证

无游戏资产测试：`tools/build_target.bat LoMemoryAliasTest`，运行
`out/build/windows-clang/LostOdysseyRecomp/LoMemoryAliasTest.exe`。
覆盖 A→C、C→A 清零、E 的一页偏移、虚拟内存隔离、跨页及物理区末端、
CPU 初始化/GPU 写回/CPU 读回的查询往返，以及释放后重新分配。

正常游戏复跑使用 `handoff.md` 的自动按键序列，记录在 `out/render-alias-fixed/`。
运行时不设置遮挡禁用、ZPD 覆盖、强制颜色/深度调试开关。


验证结果：Windows clang-cl 构建及测试通过，WSL Manjaro 使用 `c++ -std=c++20 -O2 -Wall -Wextra -Werror`
编译同一测试和实现并运行通过。Linux 完整游戏/渲染后端未在本轮验证。

`render-alias-fixed` 第 2400 帧与第 9300 帧截图已检查：主角、士兵材质、红眼与蓝色选中光圈恢复。
第 9301 swap 日志 30.0 fps、1353 draws/frame；此记录覆盖约五分钟的运行。
额外攻击复跑 `out/render-alias-attack/` 中读回 `0x83235AB4=1`，确认保持正常遮挡开关。
已有 ZPD counter 仍是近似实现，本次修复其 CPU/GPU 共享路径，不等于完成真实硬件遮挡查询。

第 2400 帧主场景 `full-width 7e3` 的相对常量材质绘制由 0 恢复到 387 笔、1,343,967 个索引；
此统计用于佐证提交恢复，网格身份仍以实际画面/guest proxy 为准。


攻击复跑已到第 3961 swap，日志 30.0 fps；第 3000/3300/3600 帧截图已导出，
第 3000/3300 帧已目视检查，攻击后的菜单及镜头切换仍保持角色材质。
此时再次读回遮挡开关仍为 1。验证完成后主动停止本轮测试进程。
近景第 3000 帧仍可见局部重影/细长条纹，其来源尚未定位；本次仅确认黑色剪影问题修复，
不宣称后处理、阴影和蒙皮的全部细节已经正确。
