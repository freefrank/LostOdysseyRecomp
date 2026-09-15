# Win7 / VxKex legacy 交接（2026-09-14）

独立分支 `legacy`，基线 `main` @ `7bd608a`。不要在 `linux` 或 `main` 工作树上改这些文件。口径是 Win7 启动（Issue #6）加 DX11 图形路径（Issue #1）。Plume 没有 D3D11 后端；当前只有设备探测骨架，渲染器仍未实现。

## 环境（Issue #6，v0.5.11 日志）

不是 32 位系统。

- `RtlGetVersion` 6.1.7601（Win7 SP1 工作站）
- `pointer_bits=64`，PE `machine=0x8664`，约 8 TiB 用户 VA、16 GiB RAM
- 对方用 **VxKex + DXVK 1.10.3/Sarek + vkd3d 2.4**；从未成功启动
- `emulated=true` 是本仓库启发式误报：VxKex 的 `IsWow64Process2` 返回 `processMachine=0x8664` 而非 0
- 失败点：`VirtualAlloc2` `MEM_RESERVE|MEM_RESERVE_PLACEHOLDER` → Win32 **error 6**（`ERROR_INVALID_HANDLE`），尚未创建 section
- WinHTTP error 87 只是更新检查，与内存无关

VxKex（i486/VxKex）故意不实现 placeholder：`GetProcAddress` 会藏 `VirtualAlloc2`；若仍被调用，`MEM_REPLACE_PLACEHOLDER` 返回 `ERROR_INVALID_PARAMETER`。对方日志的 error 6 是残缺 stub，不是缺 RAM。

## 必须保持的映射契约

`LostOdysseyRecomp/kernel/guest_address_space.cpp`：

| view | VA | size | file offset |
|---|---|---|---|
| 0 | 0 | 0xA0000000 | 0 |
| A | 0xA0000000 | 0x20000000 | 0xA0000000 |
| C | 0xC0000000 | 0x20000000 | 0xA0000000 |
| E | 0xE0000000 | 0x20000000 | 0xA0001000 |

重编译代码走 `base + ea`（`memory.h`）。A/C 同页是遮挡查询修复的硬条件（`docs/notes/physical-alias-rendering.md`）。E 的 +4 KiB 只在 placeholder 路径出现。`MmGetPhysicalAddress` 仅在 `EWindowHasPageOffset()` 为真时加 `0x1000`。`memory_alias_test` 在 placeholder 下检查 E-4K，在 legacy 下检查 E 与 A/C 同页。

## Win7 做不到什么

`MapViewOfFileEx` / `NtMapViewOfSection` 对 pagefile section 强制 **64 KiB** `SectionOffset` 与基址对齐，4 KiB 的 `0xA0001000` 返回 `STATUS_MAPPED_ALIGNMENT`。AWE/`MapUserPhysicalPages` 需要 `SeLockMemoryPrivilege`，不能作为玩家默认路径。UnleashedRecomp 只有平坦 `VirtualAlloc`，没有物理别名。

## 已实现（2026-09-14）

1. `GetProcAddress(kernelbase/kernel32, VirtualAlloc2/MapViewOfFile3)`；`LO_GUEST_MEMORY_API_OVERRIDE` 供故障注入测试直调。
2. 仅在 Win32 6/87/50/120/127/1/126 时回退；注入码 `0xD00x` 不会误入。`LO_GUEST_MEMORY_LEGACY=1` 强制 legacy。
3. Legacy：`CreateFileMappingW` + `VirtualAlloc` 探 4 GiB 空洞 + `MapViewOfFileEx`。E 的 file offset 为 `0xA0000000`（与 A/C 同页）。静态重编译 `base+ea` 不能做 Xenia 式 +4K 修补，因此不假装有 page offset。
4. `MmGetPhysicalAddress` 与 `EWindowHasPageOffset()` 对齐。成功路径日志：`mapping=placeholder|legacy`。
5. DX11：`gpu/d3d11_probe.h` 的 `ProbeHardware` 在 `video::Init` 于请求 D3D11 时记录 feature level／HRESULT，然后 `backend_selection` 仍回退 D3D12/Vulkan。可执行文件链接 `d3d11`。没有 Plume D3D11 渲染器。

本地：`LoMemoryFailureTest` 260 checks；alias placeholder `mapping=placeholder`；`LO_GUEST_MEMORY_LEGACY=1` alias `mapping=legacy`；`LoBackendSelectionTest`；`LoD3D11DeviceTest` feature_level=0xb000。未在 Win7/VxKex 实机验证。

## 本分支范围

- 做：Win7 guest 地址空间回退、E 偏移与映射一致、失败诊断、DX11 设备探测骨架。
- 不做：完整 D3D11 渲染器、改源版本号、从本分支发布、在 `linux`/`main` worktree 改同一文件。
