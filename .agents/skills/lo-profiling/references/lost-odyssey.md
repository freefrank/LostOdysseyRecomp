# LostOdysseyRecomp assembly profiling

## 定位与准备

当前已知工作区为 `D:/syncthing/Git/LostOdysseyRecomp`；优先使用当前会话明确的 checkout，不盲目切换到这个路径。核实 `tools/asm-profiler/collector.cpp`、`report.py` 和 `README.md` 存在，读取仓库当前 README／`--help` 解决选项差异。工具属于仓库，skill 不复制实现或固定历史 EXE 哈希。

可复用产物通常为 `out/asm-profiler/build/Release/lo_asm_profiler.exe`，Python 为 `out/asm-profiler/venv/Scripts/python.exe`。核实当前文件；缺失或采集器实现改变时才需要构建。此工具独立于游戏，不需要全量重编游戏。

从仓库根目录，在可用的 Visual Studio x64 开发环境中构建：

```powershell
cmake -S tools/asm-profiler -B out/asm-profiler/build
cmake --build out/asm-profiler/build --config Release
```

需要初始化报告环境时：

```powershell
python -m venv out/asm-profiler/venv
out/asm-profiler/venv/Scripts/python.exe -m pip install -r tools/asm-profiler/requirements.txt
```

使用仓库固定的依赖，不为一次分析擅自升级。Windows 工具链发现可复用 `tools/setup_windows.bat`，其环境需在同一 shell 中传递。

## 采集与生成报告

以下 `1234` 是占位 PID，必须替换成已核实的目标；`session-001` 应换成本次新的目录。不要使用 PowerShell 保留变量 `$PID` 存目标 PID。

```powershell
New-Item -ItemType Directory -Force out/asm-profiler/session-001 | Out-Null
out/asm-profiler/build/Release/lo_asm_profiler.exe --pid 1234 --seconds 10 --interval-ms 10 --output out/asm-profiler/session-001/capture.json
out/asm-profiler/venv/Scripts/python.exe -B tools/asm-profiler/report.py out/asm-profiler/session-001/capture.json --output out/asm-profiler/session-001/report.html
```

采集器默认拒绝现有输出。保留基线并优先创建新目录，不常规使用 `--overwrite`。采集器要求 x64 Windows 目标及相应进程访问权限；不会自动启动或激活游戏。Ctrl+C 会请求停止采集并写出已采样本，不能把强制杀死采集器当成等价操作。

报告同时生成同名 `.json` 摘要。先看线程表中的 OS CPU 时间和样本数，再针对实际 TID 重用原 capture：

```powershell
out/asm-profiler/venv/Scripts/python.exe -B tools/asm-profiler/report.py out/asm-profiler/session-001/capture.json --output out/asm-profiler/session-001/thread.html --tid 5678 --top 100
```

`5678` 也必须替换。仅在生成源码匹配被测构建时添加 `--source-root LostOdysseyRecompLib/ppc`。PDB 放在目标 EXE 旁供解析；无匹配符号时明确降级为 module／RVA／原始汇编，不承诺源码定位。

## 必查语义

- `sampling_method=wall_clock_all_threads_suspend_context`：短暂挂起每条线程读取 RIP 后恢复，结束后才解析符号／读代码。等待线程也进入样本。
- `interval_ms` 是请求周期，`elapsed_ms` 和 `duration_seconds` 才记录实际时间。全系统 Toolhelp 枚举可能使实际频率明显更低；不靠减小参数假定能提高采样率。
- 检查 `attempted_samples`、`failed_samples`、`snapshot_failures`、`target_exited`、`sample_limit_reached`、`resume_failed`、`symbols_initialized`，不要只看程序成功返回或热点表。
- `thread_cpu_times.observed_cpu_seconds` 是各线程首次至最后观察的 CPU 增量，不是完整进程窗口的精确归一化 CPU 利用率。
- 代码字节／模块信息在采集后读取。目标退出、模块卸载重载或代码修改会使映射缺失或失真；`code_snapshot_phase`／时间元数据不能消除这个限制。
- 第一条反汇编指令对应采样 RIP；后面最多约 32 字节是上下文。unknown 和 undecodable 样本仍在所选样本分母中。
- PPC 注释来自生成 C++ 源码附近的注释，`guest PC unverified`／`source_context_verified=false` 必须保留。不按注释行数推算原始 PPC 地址，不称为逐条 guest 指令计时。
- 工具没有调用栈、ETW on-CPU 事件、硬件性能计数器或 GPU 指令分析。对这些需求，选择另一个适用工具，不从本报告制造对应指标。

## 验证与证据

对纯分析工作不自动跑工具测试。修改报告逻辑时按受影响行为选择 `tools/asm-profiler/test_report.py` 用例；修改 native 采集时可使用 CMake 的 `LO_ASM_PROFILER_BUILD_FIXTURE=ON` 合成目标，按仓库后台测试规则运行。合成目标证明采集／符号／报告通路，不证明游戏采集开销或实际性能改善。

历史 2026-09-10 合成结果为 2 秒、187 samples、0 failed、约 31 轮，7 项 Python 定向检查通过。它只说明那次版本与机器的测试；不要重跑作为仪式，也不要把该记录冒充当前构建或真实游戏证据。当前证据以实际文件和本任务记录为准。
