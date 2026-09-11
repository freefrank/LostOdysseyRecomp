# 外部汇编采样分析器

`lo_asm_profiler` 是可选的 Win64 诊断工具，用于挂接已运行的进程。它对所有活动线程进行墙钟时间快照：短暂挂起线程读取 RIP 后立即恢复。采集器写出 JSON；离线 Python 报告使用 Capstone 解码捕获的 x64 字节，并生成 HTML 和 JSON 摘要。

快照包含睡眠和等待线程。计数是样本占比，不是 CPU 利用率、周期数、指令延迟、缓存未命中、分支计数器、GPU 计时或 ETW CPU sampling。工具没有调用栈或 inclusive attribution，也不会推断 guest 指令地址或时序。挂起线程会扰动执行。

采集器每轮使用 Toolhelp 枚举系统线程。保留的 2 秒检查对 6 个线程执行了 31 轮并得到 187 个样本；`--interval-ms` 是请求的等待时间，每个样本另行保留实际 elapsed 时间。不能用请求间隔推导样本率或 CPU 时间，也不能把这次运行当作通用开销基准。

## 构建

在 **Visual Studio x64 Developer Command Prompt** 中运行。采集器不依赖游戏或生成的 guest 代码：

```powershell
cmake -S tools/asm-profiler -B out/asm-profiler/build
cmake --build out/asm-profiler/build --config Release
```

程序位于 `out/asm-profiler/build/Release/lo_asm_profiler.exe`，要求 Windows x64，并链接系统 DbgHelp。

## 采集

```powershell
out\asm-profiler\build\Release\lo_asm_profiler.exe --pid 1234 --seconds 10 --interval-ms 10 --output out\asm-profiler\capture.json
```

`--pid` 必填，不能指定采集器自身 PID。`--seconds` 默认 10，范围 `(0, 3600]`；`--interval-ms` 默认 10，范围 `[1, 10000]`。按 Ctrl+C 可提前结束。目标进程必须保持运行，账户需要打开、挂起和查询其线程的权限。已有输出会被拒绝；确认路径后可传 `--overwrite` 覆盖。目标可执行文件始终受保护。

JSON 包含采集元数据、每线程首次／最后一次观察到的操作系统 CPU 时间、线程 ID 与 RIP，以及采集结束后的模块／符号／源码／代码快照。DbgHelp 会使用目标可执行文件所在目录初始化；符号和 PDB 行号属于尽力解析。模块卸载后重新加载时，旧快照无法可靠追溯；PDB 必须匹配目标可执行文件，代码字节和模块映射标记为采集结束后的状态。

## 离线报告

```powershell
python -m venv out\asm-profiler\venv
out\asm-profiler\venv\Scripts\python.exe -m pip install -r tools\asm-profiler\requirements.txt
out\asm-profiler\venv\Scripts\python.exe tools\asm-profiler\report.py out\asm-profiler\capture.json --output out\asm-profiler\report.html
```

报告生成 `report.html` 及旁置的 `report.json`，包含指令热点和函数 self sample 排名。第一条反汇编指令是采样到的 RIP，后续指令只是捕获 32 字节中的上下文。使用 `--tid N` 筛选线程，使用 `--top 200` 调整显示行数。HTML 过滤器使用本地 JavaScript，不含外部资源，也不会联网。

```powershell
out\asm-profiler\venv\Scripts\python.exe tools\asm-profiler\report.py out\asm-profiler\capture.json --output out\asm-profiler\thread-7.html --tid 7 --top 100
out\asm-profiler\venv\Scripts\python.exe tools\asm-profiler\report.py out\asm-profiler\capture.json --output out\asm-profiler\with-ppc-context.html --source-root LostOdysseyRecompLib\ppc
```

`--source-root` 匹配生成的 `ppc_recomp.*.cpp` 文件，并可能显示附近生成的 `//` PPC 注释。这是源码行上下文，不是已验证的 host RIP 到精确 guest PC 映射；优化后的 PDB 行号可能只是近似值。未知及无法反汇编的样本仍计入分母。

## 验证边界

```powershell
out\asm-profiler\venv\Scripts\python.exe tools\asm-profiler\test_report.py
```

定向 fixture 覆盖反汇编、聚合、线程筛选、未知／空样本、代码快照变化、HTML 转义、guest 注释边界和线程 CPU 时间表。MSVC 19.44 原生 Release 构建无警告；2 秒／10 毫秒合成采样采集 187 个样本、失败数为 0，代码字节全部捕获，并成功解析 PDB／源码上下文。该工具不能证明游戏修复或性能结果。

[English](README.md)
