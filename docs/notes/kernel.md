# 内核 HLE 笔记

实现在 `LostOdysseyRecomp/kernel/`，语义以 Xenia（优先 xenia-canary）的 `kernel/xboxkrnl`、`kernel/xam` 为准，
结构照 UnleashedRecomp（GPLv3）。未实现的导入由 `tools/gen_import_stubs.py` 生成记录型桩。

## 内存布局（客体 4 GiB 空间）

| 范围 | 用途 |
|---|---|
| 0x00100000 - 0x7C000000 | 客体虚拟分配（NtAllocateVirtualMemory，页分配器）、线程块（PCR/TLS/TEB/1 MiB 栈） |
| 0x7C000000 - 0x7FC00000 | 宿主 o1heap，内核对象句柄就是这里的指针（IsKernelObject 按范围判定） |
| 0x7FC80000 | GPU 寄存器 MMIO 窗口（reg index = (addr & 0xFFFF)/4），必须保持空闲 |
| 0x82000000 - 0x833C0000 | XEX 镜像 |
| 0x833C0000 起 | XenonRecomp 函数查找表（PPC_LOOKUP_FUNC） |
| 0xA0000000 - 0xFFF00000 | 物理分配（MmAllocatePhysicalMemoryEx）；MmGetPhysicalAddress = addr & 0x1FFFFFFF |

游戏自己的堆（libcmt/xapi 的 RtlAllocateHeap）跑在 NtAllocateVirtualMemory 之上，没有钩它。
关键语义：对已保留区间内的地址做 MEM_COMMIT 必须原地返回（FindAllocation），否则游戏拿到的指针全错。

## 变量导入

11 个内核变量（KeTimeStampBundle、XboxKrnlVersion、XexExecutableModuleHandle、ExLoadedCommandLine、
VdGlobalDevice、VdGpuClockInMHz、VdHSIOCalibrationLock、ExEventObjectType、ExThreadObjectType、
KeDebugMonitorData、KeCertMonitorData）在 `xex_loader.cpp` 里分配并写入 IAT 槽。函数导入的 IAT 槽指向其桩。
`Image::ParseImage` 会原地把 thunk 首字节交换成小端，读序号时要按小端取。KeTimeStampBundle 由一个 1 ms 线程刷新。

## 线程

`GuestThreadContext`：PCR(0xAB0)+TLS(0x100)+TEB(0x2E0)+栈，r13 指向 PCR，PCR+0x10C 是 CPU 号。
ExCreateThread 的 xApiThreadStartup 非零时线程从它进入，r3=start, r4=context。
ExTerminateThread 用 setjmp/longjmp 回到线程入口（重编译代码不能异常展开）。
游戏大量创建短命线程（每个异步读一个线程），正常。

## 文件系统

Nt 级实现（`kernel/io/file_system.cpp`）。路径：`\??\`、`\Device\Cdrom0\`、`game:\`、`d:\` 映射到解包的 disc1 目录；
`\Device\Harddisk0\PartitionN` 映射到 cache 目录。RootDirectory 只在是我们的内核对象时才使用（游戏传过 0xFFFFFFFD）。
NtReadFile 带 Event 时读完直接置位事件。目录信息结构按 Xenia 的布局，文件名是 ANSI。

## 启动序列（观测）

1. 堆初始化（NtAllocateVirtualMemory 保留 1 MiB 后按 64 KiB 提交），3 个工作线程
2. 打开 game:\LO.FPI（资源索引），再起线程池
3. 打开 \Device\Harddisk0\partition0（探测硬盘缓存）
4. VdInitializeEngines → 设置中断回调 → 环形缓冲（0x2000，4 KiB）→ 读指针回写 → 两次（设备重建一次）
5. VdSetDisplayMode(0x40000000)，两个音频线程，XAudioRegisterRenderDriverClient
6. 读 lo.fpd、xenon_loc.fpd、xenon_sys.fpd、xenon_snd.fpd 的头部与资源块
7. 每帧轮询 XMP（app 0xFA，消息 0x70009/0x7001B），按 Xenia xmp_app 回写空闲状态

## 已知未实现

- XMACreateContext 返回 NOT_IMPLEMENTED，XMA 解码整体缺失
- XexGetModuleSection('HashSec') 返回 NOT_IMPLEMENTED
- 网络全部报无网络
- Xam 内容/存档只做了目录映射
