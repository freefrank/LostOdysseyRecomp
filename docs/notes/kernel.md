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

## XAM 用户档案与 UI（2026-09-03，进入主菜单的关键）

- **登录状态靠通知，不靠 `XamUserGetSigninState`**：游戏按 Start 后并不查询登录状态，而是依赖启动时收到的
  `XN_SYS_SIGNINCHANGED`（MSGID(0,0xA)，参数 = 已登录用户位掩码）。Xenia 在第一个监听器注册时补发
  `XN_SYS_UI(0)`、`XN_SYS_SIGNINCHANGED(1)`、`XN_SYS_STORAGEDEVICESCHANGED(0)`，Live 区再发
  `XN_LIVE_CONNECTIONCHANGED(0x001510F1)`、`XN_LIVE_LINK_STATE_CHANGED(0)`。缺这些会弹 "not signed into a gamer profile"。
- **"档案存储空间不足"** = 写入 `XPROFILE_TITLE_SPECIFIC1`（0x63E83FFF，1000 字节二进制）后读回不是 TITLE 来源，
  或成就枚举器创建失败。`XUSER_PROFILE_SETTING` 为 40 字节（source @0，user_index/xuid @8，id @16，
  X_USER_DATA @24：type @24，size @32 / ptr @36），二进制载荷紧跟设置数组之后。设置持久化在
  `LO_PROFILE_DIR`（默认 exe 目录下 `profile/`）。`XamUserCreateAchievementEnumerator` 返回成功 + 空枚举器，
  条目 36 字节（带字符串标志再加缓冲）。
- **overlapped 完成约定**：带 overlapped 的 XAM 调用要返回 `ERROR_IO_PENDING`，把结果写进 Error / dwExtendedError /
  Length，触发 hEvent，若有 pCompletionRoutine 则以用户 APC 调用 `(result, length, overlapped)`。同步返回
  `ERROR_SUCCESS` 会被游戏当成"取消"（设备选择器弹 "If you don't select a storage device"）。
- 虚拟存储设备只有 ID 1（HDD，20 GiB / 10 GiB 空闲，同 Xenia）；`XMsgStartIORequest(0xFB, 0xB0006/0xB0007)` 是
  XGI 的 UserSetContext / UserSetPropertyEx，直接成功即可。
- 测试钩子：`LO_AUTO_START=<帧>` 之后每 240 帧按 20 次 Start，用于无人值守穿过标题；键盘映射见 hid.cpp。

## 新游戏进入战斗场景的崩溃（阶段 3 待办，2026-09-03 分析）

复现：`LO_AUTO_START=1100 LO_AUTO_BUTTONS=saaaaaaaa`（标题后每 240 帧按一次键：先 Start 再连按 A）→ 打开
`xenon_scr/event/obj.fpd` 后崩在 `sub_822B98C8+0xc43`（ppc_recomp.2.cpp:1118），调用栈
`sub_822B5938 ← sub_82AD9800 ← sub_82ABC400(RPBattle__Scene, bsSMsys_Idling) ← sub_82ABD958(bpPawn)`。

结构还原（对照 UE3 源码）：`sub_822B98C8(this=USkeletalMeshComponent, ..., &this[732])` 是 UpdateSkelPose/ComposeSkeleton
一类：`this+640` SkeletalMesh，`this+648` Animations → `sub_822B4B78` = Cast<UAnimTree>，AnimTree `+244/+212` =
`SkelControlLists`（FSkelControlListHead 16 字节 {FName, USkelControlBase* @8, INT}），`this+808/812/816` =
`TArray<BYTE> SkelControlIndex`（Data/Num/Max）。循环里 `Idx = SkelControlIndex(bone); if (Idx != 255)
Control = SkelControlLists(Idx).ControlHead; Control->flags@88`。

现场：Num=63、Max=118，但只有前 61 字节是 255/有效索引（FF 00 FF FF 01 02 03 04 FF…），第 61、62 字节是
分配块里的旧内容（每次运行不同：一次是残留的宽字符串 "Solider : bsSMsys_Idling"，一次是随机字节），骨骼 61 取到
0x20/0x26 → 越过 5 项的列表 → 垃圾指针 0xD → 读 0x65 崩溃。网格本身有 63 根骨骼（USkeletalMesh+0x78 与 +0x88 两个
Num=63 的数组）。结论：初始化 SkelControlIndex 的路径按 61 填充却把 Num 设成 63。尚未找到该初始化函数（搜索
`,808(r`+`stbx`/`li r4,255`/`li r4,-1`/`addi rN,rM,808` 均未命中，可能通过 TArray 辅助函数以结构指针操作）。
下一步用 Ghidra 反编译 sub_822B98C8 的调用者与 SkeletalMeshComponent 的 InitSkelControls 等价函数。

已排除：页分配器复用（`LO_PAGE_QUARANTINE=1` 延迟复用后仍崩）、宿主 `vswprintf` 钩子越界（`LO_TRACE_PRINTF=1`
未见该字符串）、stvlx/stvrx 语义、临界区/自旋锁原子性（实现检查正常）。
调试手段：`LO_CRASH_DUMP="r27+0x328*,r19+0xf4*"`（寄存器相对地址，`*` 跟随指针）在崩溃时转储客体内存；崩溃处理器
现在打印全部 32 个通用寄存器。
