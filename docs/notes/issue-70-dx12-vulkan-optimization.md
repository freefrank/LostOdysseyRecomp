# Issue #70 DX12/Vulkan optimization

状态：v0.7.3 已发布；首轮实现与限定验证完成，尚无用户验收，Issue #70 仍开放。

v0.7.3 已于 2026-09-27T18:01:05Z 发布到 [GitHub Release](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.7.3)，来源为 tag commit `fd139e3c0407309de0cd3d4e5724c59d4363e4ab`。Release CI [36335854660](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/36335854660) 全部通过；公开 Release 恰好包含 Windows ZIP、Linux AppImage 和 stable Linux Flatpak，三个链接均返回 HTTP 200。runtime archive、独立 checksum 和 `release-source.json` 未作为公开附件；大小、摘要、Windows 版本/53 项来源清单及 Flatpak stable 重导出记录保留在 `out/releases/v0.7.3/` 内部证据中。

本轮实现集中在 DX12 command list 的原生状态复用和渲染路径状态管理：Plume 对 graphics/compute 的原生 root signature 与 root descriptor table 请求进行去重，并在 descriptor heap、原生 root signature 变化、native `Reset`/`Close` 以及外部原生命令状态变化时统一失效缓存。video、NGX 和 FSR 的 native Reset/Close 生命周期同步进入该失效路径，避免沿用过期绑定。

计数与诊断保持按需启用：仅在 `LO_RENDER_TIMING` 打开时收集 palette hit/miss/replace、表创建成本与写入数，以及 texture-set/sampler-version 批次触发原因。`run-fg-game` 已增加 `Diagnostic`（默认）和 `Lightweight` 模式，并在 `run.json` 记录 `capture_mode`、`render_timing` 和 `mv_log`。

已完成的限定验证：runtime 与 `LoNativeDlssD3D12ExecutionTest` 构建 exit 0；无窗口 NGX GPU fixture exit 0，Quality 为 853×480→1280×720、DLAA 为 1280×720→1280×720，读回值为 `3400,3800,3000,3c00`；FSR D3D12 fixture 构建和运行均 exit 0，NativeAA 为 64×64→64×64、Quality 为 64×64→96×96，中心像素为 `102,51,25,191`。证据位于 `out/issue70-validation/runtime-build.log`、`ngx-gpu.log`、`fsr-build.log` 和 `fsr-gpu.log`。

本机替代场景运行：D3D12 与 Vulkan 均使用 2560×1440、DLAA、16× AF、120 FPS 上限、FG 关闭并保留 object motion，在隔离目录中静音运行约 120 秒，均以 exit 0 正常关闭且保留 baseline。截图确认进入 Uhra 市区/广场场景（D3D12：`out/issue70-runtime/dx12-dlaa-af16/scene_3195.png`；Vulkan：`out/issue70-runtime/vulkan-dlaa-af16/scene_2507.png`）；日志确认 DLAA 为 2560×1440→2560×1440，AF 实际为 16×。75–115 秒诊断窗口的 accepted-present API wall intervals：D3D12 2,400 次，mean/p95/p99 为 `16.666/17.977/18.792 ms`；Vulkan 2,323 次，mean/p95/p99 为 `17.219/20.471/23.045 ms`。两组均记录 AF miss/table create 为 0、sampler-version 拆批为 0、arena 拆批为 2，窗口内无 warning/error；这些是单次诊断采集窗口，不能作为优化前后收益或稳定 60 FPS 结论。

AF measurement fixture 在 `out/issue70-af-measurement` 通过 2/2，包含 AF palette `93,237` 项检查和 synthetic renderer fixture；`run-fg-game.ps1` 的 PowerShell AST 解析也已通过。`LoD3D12RootBindingTest` 构建和运行 exit 0，覆盖重复绑定、单 view slot、同 handle 不同 root index、graphics/compute 切换、A→B→A signature、heap 外部切换恢复、外部 signature 改写、Reset、独立 continuation 和 descriptor 地址复用。由于 `D3D12GetDebugInterface` 不可用，debug layer 未验证；该 fixture 只录制并 Close，不包含 draw/dispatch/readback，也未注入真实 isolated 失败。root fixture 日志见 `out/issue70-validation/root-binding.log`；干净 Plume 补丁应用一致性见 `out/issue70-validation/patch-validation.json`。本机运行属于替代 Uhra 场景，既不是 Issue #70 报告者的硬件/镜头，也没有优化前后 A/B 或最终 Lightweight 复测，因此不宣称 Issue #70 已解决，也不宣称 FPS 提升或稳定 60 FPS。GitHub Issue #70 于 2026-09-27 查询时仍为 OPEN（[链接](https://github.com/freefrank/LostOdysseyRecomp/issues/70)）。P2/P3 优化保持待测量后决定。
