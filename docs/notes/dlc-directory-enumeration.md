# DLC 目录枚举与 `FindNext` pattern 诊断

## 当前状态（2026-09-09）

三份真实 DLC 包的首次导入和重复选择／保留已通过；源文件与安装文件的字节、哈希和修改时间保持不变。历史运行路径曾在绑定三根内容并部分读取后于 guest `0x1A` 处发生访问异常；该路径已由目录过滤修复后的 runtime02 复核。runtime02 分别读取 XDC0/1/2 的 LODLC002/001/003，三个包均完成 header、完整索引和 payload 读取，合计 24 次，spa.bin 零匹配、零崩溃，主菜单正常。奖励和地下城玩法仍未验收。该结果验证了三包的导入、枚举与读取，未覆盖奖励领取及地下城游玩。

离线诊断使用 exact EXE SHA256 `28415cb4b1d5a2b218dbf293e0871926fcf19c8e46262b463b26da3d340e5d46`，并以其 guest-host table 与 unwind 信息定位 fault RVA `0x1C10BE8` = guest `0x82853DD0`。对应指令为 `lhz r25,26(r31)`，当时 `r31=0`、读取地址为 `0x1A`，不是 GC 结论。该 EXE 的 `MAP` timestamp `6AA02E81` 与 EXE timestamp `6AA07DE7` 不同，因此不能用不匹配的映像或旧地址替代本次定位。

## 已确认的枚举契约

guest 实际调用 `FindFirst` 时向 `XDCn:\*.fpi` 传入 pattern，后续 `FindNext` 传入 null。修复前宿主 `NtQueryDirectoryFile` 路径在空 pattern 情况下重新计算过滤条件，使 `spa.bin`（XDBF）混入 FPI 解析；离线证据将其与后续空索引异常关联，但没有直接追踪空索引创建函数的每个内部返回。该链路解释了为什么导入和目录发现可以先成功、而部分读取仍会在 guest 端失败。

原始 Xbox 代码与历史 EXE 静态对照证明首个与后续参数形式：guest 首次传 `*.fpi`、续查传 null。Xenia 固定 commit [`95a5c3ee`](https://github.com/xenia-project/xenia/blob/95a5c3ee250f80c3b9d139658649d9ffb6db3eec/src/xenia/kernel/xfile.cc#L43-L71) 的 `xfile.cc:42–65` 显示：非空 pattern 会更新／重置搜索状态，null 或空 pattern 会保留既有 pattern，`RestartScan` 只重置 cursor。Microsoft 的 [`NtQueryDirectoryFile` 文档](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-ntquerydirectoryfile) 描述了 Windows NT 的每句柄 pattern 规则：首次调用捕获表达式，后续非 null 表达式会被忽略；这与 Xbox/Xenia 的替换行为不同。项目 fixture 覆盖了该差异，但没有把它宣称为新测得的 Xbox 实机行为。

## 修复与验证边界

`dlc_directory_filter` 已改为为每个句柄持久化 `searchPattern`；直接 native 回归 fixture 已通过。该回归只编译 `storage_test`／`file_system` 两个 translation unit，复用 51 个 host 输入并运行两次 fixture link，覆盖真实 `NtQueryDirectoryFile` guest import 的大小写 `*.fpi` 首次／后续／null／empty、`RestartScan`、多句柄隔离、非空 pattern 更换、fresh null 全文件、no-match 以及 buffer 尾部和 IOSB 边界。旧对象对照 fixture 仍为 exit 1，修复 fixture 为 exit 0。该结果不是生产链接或实际游戏运行，因此不能宣称实际 DLC 玩法修复、奖励或迷宫区域已验收。冻结离线报告为 `out/v0.5.0/dlc-validation/offline-diagnosis/REPORT.md`，SHA256 `fa05352c18c28221de1e276bca3d2b7ba5695813b8dac62c934dda3017b4a07d`；`ARTIFACTS.json` 区分 exact-host 与 original-PPC 证据。Issue #12 的生产 GC 修复已有独立的无 probe 正常时序验证；本 note 不替代其证据链。

证据目录：`out/v0.5.0/dlc-validation/offline-diagnosis/`，包括 `REPORT.md`、`original-ppc-selected.txt` 和 `exact-host-selected.txt`；目录过滤回归报告为 `out/v0.5.0/dlc-validation/directory-filter/REPORT.md`。唯一运行 owner：`issue12_guest_path`。runtime02 的游戏验证由 owner 完成；本次文档同步不额外运行构建或测试。版本记录为本地未发布，奖励和地下城仍待后续验收。
