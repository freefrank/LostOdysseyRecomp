# External assembly profiler

`lo_asm_profiler` is an optional Win64 diagnostic for attaching to an already running process. It takes wall-clock snapshots of every live thread, briefly suspending a thread to read its RIP and then resuming it. The collector writes JSON; the offline Python report decodes captured x64 bytes with Capstone and writes HTML plus a JSON summary.

Snapshots include sleeping and waiting threads. Counts are sample shares, not CPU utilization, cycles, instruction latency, cache misses, branch counters, GPU timing or ETW CPU sampling. There is no call-stack or inclusive attribution, and the tool does not infer guest instruction addresses or timing. Suspending threads perturbs execution.

The collector enumerates system threads with Toolhelp on each pass. In the retained 2-second check, 31 passes over six threads produced 187 samples; `--interval-ms` is a requested delay, while each sample retains measured elapsed time. Do not infer a sample rate or CPU time from the requested interval, or treat this run as a general overhead benchmark.

## Build

Open a **Visual Studio x64 Developer Command Prompt**. The collector has no game or generated guest-code dependency:

```powershell
cmake -S tools/asm-profiler -B out/asm-profiler/build
cmake --build out/asm-profiler/build --config Release
```

The executable is `out/asm-profiler/build/Release/lo_asm_profiler.exe` and requires Windows x64. It links to system DbgHelp.

## Capture

```powershell
out\asm-profiler\build\Release\lo_asm_profiler.exe --pid 1234 --seconds 10 --interval-ms 10 --output out\asm-profiler\capture.json
```

`--pid` is required and cannot be the collector's own PID. `--seconds` defaults to 10 and accepts `(0, 3600]`; `--interval-ms` defaults to 10 and accepts `[1, 10000]`. Ctrl+C stops early. The target must remain alive and the account needs rights to open, suspend and query its threads. Existing output is rejected; pass `--overwrite` to replace it after checking the path. The target executable itself is always protected.

JSON contains capture metadata, each thread's first/last observed OS CPU times, sampled thread IDs and RIPs, and a post-capture module/symbol/source/code snapshot. DbgHelp is initialized with the target executable's directory. Symbols and PDB lines are best effort. An unloaded and reloaded module cannot be reliably identified from the old snapshot; PDBs must match the target executable, and code bytes/module mappings describe post-capture state.

## Offline report

```powershell
python -m venv out\asm-profiler\venv
out\asm-profiler\venv\Scripts\python.exe -m pip install -r tools\asm-profiler\requirements.txt
out\asm-profiler\venv\Scripts\python.exe tools\asm-profiler\report.py out\asm-profiler\capture.json --output out\asm-profiler\report.html
```

This writes `report.html` and a sibling `report.json`, with instruction hotspots and function self-sample rankings. The first disassembled instruction is the sampled RIP; following instructions are context from the captured 32 bytes. Use `--tid N` to select one thread and `--top 200` to change the displayed rows. The HTML filter is local JavaScript and has no external resources or network requests.

```powershell
out\asm-profiler\venv\Scripts\python.exe tools/asm-profiler/report.py out\asm-profiler\capture.json --output out\asm-profiler\thread-7.html --tid 7 --top 100
out\asm-profiler\venv\Scripts\python.exe tools\asm-profiler\report.py out\asm-profiler\capture.json --output out\asm-profiler\with-ppc-context.html --source-root LostOdysseyRecompLib\ppc
```

`--source-root` matches generated `ppc_recomp.*.cpp` files and may show a nearby emitted `//` PPC comment. This is source-line context, not a verified host RIP to exact guest PC mapping; optimized PDB lines can be approximate. Unknown and undecodable samples remain in the denominator.

## Validation boundary

```powershell
out\asm-profiler\venv\Scripts\python.exe tools\asm-profiler\test_report.py
```

The focused fixture covers disassembly, aggregation, thread filtering, unknown/empty samples, changed code snapshots, HTML escaping, guest-comment boundaries and the thread CPU-time table. The native Release build completed cleanly with MSVC 19.44; a synthetic 2-second/10-ms run collected 187 samples with zero failed samples, captured all code bytes, resolved PDB/source context, and observed the busy fixture's CPU delta. These checks do not establish a gameplay fix or performance result.

[简体中文](README.zh-CN.md)
