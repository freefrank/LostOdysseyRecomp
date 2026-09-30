# FSR 超分与 DLSS 帧生成阶段性交接文档（P0 阶段）

> 本文保留 OpenCode 停止时的历史快照。2026-09-23 Codex 后续实现、验证与最新停止状态见 [Codex 停止交接](fsr-dlss-fg-codex-handoff.zh-CN.md)；以下旧基线和未启动状态不代表当前进度。

## 1. 任务状态与基线定义

当前任务响应用户明确指令“好，停止，写handoff”，所有实现、修复与审查任务均已停止，本交接文档为当前唯一交付物。

代码基线维持在初始提交 `39bbda6ff1cb8216582798bebab21a3a5395a36e`（main 分支 HEAD）。已有文件的修改尚未提交，新增文件尚未跟踪；没有提交、推送或发布。

在阶段进展方面，P0 阶段审查结论为未通过（Gate 1 attempt 1/3 NOT PASSED），P1 至 P4 阶段尚未启动。

## 2. 目标与锁定版本

本项目本轮实施目标为：保持现有原生 NGX DLSS/DLAA 运行路径，引入基于 Vulkan 的 FSR 3.1.4 作为额外时序超分方案，并在 Windows Vulkan 下通过 NVIDIA Streamline 接入 DLSS-G 固定 2× 帧生成独立探针。

相关 SDK 与依赖版本均严格锁定如下：
- FSR 超分：AMD FidelityFX SDK v1.1.4（Git commit `c6efa6bf7f2027b3ec94f28578bb5965eabb9e55`），抽取 Vulkan 后端与 FSR 3.1.4 算法实现，不包含 FSR 交换链或 FSR 帧生成。
- DLSS 帧生成：NVIDIA Streamline v2.14.1（Git commit `2122257e0fce486f91b385aa63b9a09b0a34b363`），官方发布包 ZIP 哈希 SHA256 为 `92c4d954631a1710da86ca3fa8d5034f2b9503838c95fc4ae977ae149319781b`，加载 DLSS_G、Reflex 与 PCL 功能。
- 本地 NGX 运行库：版本锁定为 310.9.1，作为既有原生 DLSS/DLAA 的执行主体。

## 3. 代码与文件变更总览

工作区现存所有变更均以只读方式核对，文件分组如下：

### 生产门面、配置、GPU 契约与菜单文件（已修改）
- `LostOdysseyRecomp/gpu/dlss_status_log.h`：增加 FSR 运行时诊断标签。
- `LostOdysseyRecomp/gpu/frame_plan.cpp`：支持 FSR 相关的帧规划解析与校验。
- `LostOdysseyRecomp/gpu/frame_plan.h`：升级 LOF2 v3 wire 协议，扩展 consumer 高位并保留 legacyAA 位。
- `LostOdysseyRecomp/gpu/renderer.cpp`：时序超分调用端适配通用门面，管理 token 与资源生命周期。
- `LostOdysseyRecomp/gpu/temporal_frame_inputs.h`：通用化输入完整性判定 `CompleteForConsumer`。
- `LostOdysseyRecomp/gpu/temporal_history.h`：时序历史管理辅助。
- `LostOdysseyRecomp/gpu/upscaling_plan.cpp`：超分规划逻辑拆分与 FSR 尺寸缓存。
- `LostOdysseyRecomp/gpu/upscaling_plan.h`：定义 `Upscaler::Fsr` 枚举与独立质量枚举。
- `LostOdysseyRecomp/gpu/video.cpp`：管理 `TemporalUpscaler` 门面实例的生命周期。
- `LostOdysseyRecomp/gpu/video.h`：声明通用门面成员。
- `LostOdysseyRecomp/settings/config.cpp`：持久化配置读写，保持 Off=0、Dlss=1，新增 Fsr=2。
- `LostOdysseyRecomp/settings/config.h`：配置结构体字段扩展。
- `LostOdysseyRecomp/settings/menu.cpp`：设置菜单将 FSR 显示为暂不可用，保持布局和行索引稳定。

### 生产门面新增文件（未跟踪）
- `LostOdysseyRecomp/gpu/temporal_upscaler.h`：时序超分通用门面接口声明，借用既有 Controller。
- `LostOdysseyRecomp/gpu/temporal_upscaler.cpp`：通用门面实现，提供尺寸查询、准备、隔离记录与提交通知。

### CPU 契约测试文件（已修改与未跟踪）
- `tools/tests/frame_plan_test.cpp`：新增 wire 编解码往返及非法协议拒绝测试。
- `tools/tests/native_dlaa_test.cpp`：适配门面调整。
- `tools/tests/native_dlss/CMakeLists.txt`：配置 CPU 测试套件编译目标。
- `tools/tests/native_dlss_p2_routing_test.cpp`：适配路由测试。
- `tools/tests/temporal_frame_inputs_test.cpp`：输入资格通用化单测。
- `tools/tests/temporal_upscaler_contract_test.cpp`（未跟踪）：通用门面契约测试。

### Streamline 帧生成共存探针文件（未跟踪）
- `cmake/LoStreamline.cmake`：探针构建配置。
- `tools/tests/streamline_fg/CMakeLists.txt`：探针独立工程定义。
- `tools/tests/streamline_fg/probe_scene.h` 与 `probe_scene.cpp`：探针模拟场景与资源生成。
- `tools/tests/streamline_fg/probe_vulkan_dispatch.h` 与 `probe_vulkan_dispatch.cpp`：Vulkan 拦截层与 8 个 WSI hook 转发。
- `tools/tests/streamline_fg/streamline_runtime.h` 与 `streamline_runtime.cpp`：Streamline 初始化、DLL 签名校验与函数导入。
- `tools/tests/streamline_fg/streamline_fg_probe.cpp`：探针主流程。
- `tools/tests/streamline_fg/sdk-manifest.json`：固定 SDK 资产校验清单。
- `tools/tests/streamline_fg/README.md`：探针使用说明与已知现象说明。

### 项目管理跟踪文件（已修改）
- `docs/ROADMAP.md`、`docs/ROADMAP.zh-CN.md`、`docs/project-management/items.json`、`docs/project-management/sync-state.json`：此前已同步 195 项，当前处于落后于最新探针及门面状态的暂存态。

## 4. 已完成门面、菜单与协议改造及验证证据

在 P0 阶段的前半部分，公共协议与超分通用门面已开发完成，并建立了严格的编译与测试证据。

协议层面，通信协议升级为 LOF2 v3，在寄存器 0x7F20 处占用 24 个字，支持宽 consumer 高位解析，同时保留了低位兼容与 legacyAA 标志位。解析逻辑保留了对旧版 LOFP 8 字布局及 v2 格式的向后兼容，对未识别的未知协议进行显式拒绝。

配置与架构层面，持久化配置保留了原有的 `Off=0` 与 `Dlss=1`，新增 `Fsr=2` 并配套独立的 FsrQuality 设置。新增的 `TemporalUpscaler` 门面借用现有的 `gpu::dlss::Controller`，插槽中通过 `SrUseToken` 记录 provider、设备 epoch、请求及几何标识，并在提交与丢弃时基于该 token 独立路由。尺寸查询建立了包含 provider 与输出有效区域的精确缓存，隔离了不同算法的尺寸上下文。在 P1 接入前，FSR 接口维持不可用状态并走已有回退路径。

设置菜单层面，`settings/menu.cpp` 进行了针对性适配，当用户载入保存为 FSR 的配置时，界面直接显示不可用，且查看操作不会误将数值修改或触发非法选择，整个菜单布局与行索引保持原样。

验证证据方面，实施过程执行了直接受影响的 CPU 测试与生产构建：
1. 初始 CPU 套件 8/8 项全部通过；
2. 修复 consumer 高位边界后，针对性重测 `LoP1FramePlanTest` 1/1 项通过；
3. 门面实现完成后，构建并执行直接受影响的 4 项测试：`LoP1FramePlanTest`、`LoNativeDlssP2RoutingTest`、`LoNativeDlaaTest`、`LoTemporalUpscalerContractTest`，全部通过（4/4 Passed）；
4. 生产目标执行编译命令 `cmake --build out/build/windows-clang --target LostOdysseyRecomp --parallel 2` 顺利通过，编译日志 `out/build/windows-clang/temporal-facade-build-retry-stable.log` 完整记录了包含设置菜单在内的全量链接结果，产物大小约为 93MB。

上述测试覆盖所列 CPU 合同，生产构建覆盖真实门面实现的编译与链接；尚未执行该生产路径的 GPU 运行回归，不能据此断言运行行为没有退化。

## 5. 硬件探针运行事实与失败区分

独立共存探针在真实硬件环境（NVIDIA GeForce RTX 5080，驱动版本 616.56，Windows 11）下完成了实机执行，证实了原生 NGX SR 与 Streamline 帧生成在 Vulkan 下能够共同运行，但也暴露了退出清理阶段的致命缺陷。

### 探针运行事实
探针以固定 2× 模式完整推进了 46 个真实渲染帧，覆盖了 FG 开启/关闭状态切换以及窗口从 1920×1080 到 2048×1152 的 resize 重建过程：
- 拦截层设置的 8 个 Vulkan WSI hook 均被实际命中，调用计数分别为：`present=46`、`createSwapchain=2`、`destroySwapchain=2`、`getSwapchainImages=4`、`acquire=46`、`deviceIdle=3`、`createSurface=1`、`destroySurface=1`。
- 场景平移过程中，落盘的主机端 HUDless 图像哈希（`fg_host_hudless_*.rgba16f`）发生变化。结合实际 NGX 调用记录可确认执行及输出变化，但不能单凭哈希判断时序重建或画质正确。
- 在 FG 开启区间内，SDK 状态结构体连续上报 `numFramesActuallyPresented = 2`，生成间隔计数达到预期。

### 失败特征与退出码
探针在运行结束后的清理阶段发生严重错误，最终退出码为 1：
- 错误日志明确记录：`commonEntry.cpp:923[releaseNGXFeature] [sl.dlss_g] NGX release feature failed 0xbad00004`。
- 该错误码对应 NGX 内部的 `FeatureNotFound`，表明 Streamline 在尝试释放 DLSS-G 特性句柄时，所依赖的底层 NGX 上下文已不存在。

### 产物与证据区分
工作区中现存的 `out/streamline-fg-p0/runtime.log` 文件修改时间为 23:30:16，早于最终探针源代码（23:31:16）及最终编译的可执行文件 `LoStreamlineFgProbe.exe`（23:31:32）。该日志属于早期构建的运行残留，绝对不能作为后续修正版二进制的运行凭证。

此外，当前运行环境未安装 Khronos Vulkan 验证层（VK_LAYER_KHRONOS_validation），运行日志仅能证明 API 级别的调用过程，无法排除潜在的 Vulkan 时序违规。同时，探针主机端截图无法证明显示器端物理接收到了插帧画面，外部物理显示证据仍未采集。内存记录仅采集了初始化与 resize 前后的粗粒度数值，无法得出长期运行无泄漏的结论。

## 6. Oracle Gate 1 评审结论与阻断规范

Oracle Gate 1 评审通过 OpenCode API 检索已保存在会话 `ses_f335eeb2dffexMz3oxg4dJ3qNj` 中的审查报告（消息 ID `msg_0ccd2c3ac001DW3biIC5DmW8as`，时间戳 2026-09-23 05:52:40–05:54:36 UTC，以及追随消息 `msg_0ccd49e13001iDJlOu7imMEmhl`）。

评审结论为 **Gate 1 暂不通过（attempt 1/3 NOT PASSED）**。根据规则，初始审查不可重跑，后续仍保留 2 次材料复审机会。

### 阻断问题根因分析
探针在退出时误认为调用 `slSetFeatureLoaded(sl::kFeatureDLSS_G, false)` 即可释放插件资源。然而在 Streamline 官方公开源码中，`slSetFeatureLoaded(false)` 仅将内部插件标记为禁用并重构 hook 表，并不会触发资源销毁或调用插件的 shutdown。

在此之后，探针调用了原生 Controller 的 `ShutdownAfterGpuDrain()`，其内部执行了：
```cpp
NVSDK_NGX_VULKAN_Shutdown1(sessionDevice_->vk);
```
根据 NGX Vulkan 规范，该调用会直接关闭设备级别的 NGX 实例并释放该设备上的所有 NGX 状态。

随后探针才调用 `slShutdown()`。共享 session 提前关闭与后续释放 DLSS-G 句柄时的 `FeatureNotFound` 高度吻合，是 Oracle 判定的首要根因；仍需修复后的受控运行确认。SDK 指南还指出 options 配置在下一次 Present 才生效，单纯请求 Off 并排空 GPU 不能证明停用已生效。

### 严格的退出清理顺序规范
后续修复中，必须在探针中动态导入并检查 `slFreeResources(sl::kFeatureDLSS_G, viewport)`。必须满足的核心约束为：**双方的 feature 资源都必须在第一个 session shutdown 执行前完成释放。**

推荐的退出清理时序如下：
1. 发起 FG 停用请求（`Mode(false)`）；
2. 执行一次附带真实新帧 token 的 Off Present，确保停用配置在插件内部真正生效；
3. 执行 hooked 排空（Drain），确保原生命令列表完成并结清账本；
4. 在双方 session 仍处于存活状态时，调用并校验 `slFreeResources(sl::kFeatureDLSS_G, viewport)`；
5. 调用原生 Controller 的 `ReleaseFeatureAfterGpuDrain()` 释放 SR feature；
6. 清除 tags，销毁场景纹理、交换链及 surface（此时 WSI hooks 仍维持生效）；
7. 原生 Controller 释放全局参数并执行 session 关闭；
8. 校验并调用 `slShutdown()`；
9. 恢复原生 Vulkan 调度指针，销毁底层 Vulkan 设备与实例，卸载动态库。

严禁采取简单调换顺序（例如先全局执行 `slShutdown()` 再关 Controller）的错误做法，因为 Streamline 全局关闭内部传入的是 `nullptr`，同样会强行破坏原生句柄。任何情况下均不得压制或忽略 `0xbad00004` 报错。

### 其他必要修正规范
- 同步与信号量复用：当前探针在调用 `vkAcquireNextImageKHR` 时复用了同一个 acquire 信号量，且调用发生在等待上一帧 fence 之前，导致信号量处于未释放状态即被重用。修复时必须将上一帧 fence 等待与完成通知提前至下一次 acquire 之前。同时，必须针对交换链中的每个图像分别创建对应的 render-finished（present）信号量，严禁全链共用单个 present 信号量。
- 账本遗留处理：当 `RecordIsolated()` 返回失败但带有非零 `useId`，或在记录之后、提交之前的环节抛出异常时，探针必须立即托管该 pending use，并在提交失败时调用 `OnBatchDiscarded()` 予以注销，避免退出时因未结账本导致 `ShutdownAfterGpuDrain()` 直接返回。
- 清理错误向退出码传递：所有清理 API 的返回值均需纳入检查，若在清理过程中发生错误，即使此前主循环正常，最终退出码也必须置为 1（包括 `return 77` 分支在析构阶段暴露的错误）。
- 日志环形缓冲区溢出：原生 Controller 内部的调用记录环形缓冲区容量上限仅为 128 条，在 46 帧运行后已被填满，导致退出阶段的真实调用无法进入最终报告。探针应采用专用的生命周期记录机制捕获真实返回值，严禁修改生产环境的 Controller。
- 尺寸切换历史重置：当前探针仅在全局 `frame == 1` 时发送 reset。在 resize 重建场景后，首帧应被视为场景局部的首帧，必须同时向 NGX 和 Streamline 传递历史重置标志。

## 7. 终止修复会话的状态检查与未完成项

在收到用户停机指令前，fix-2 修复会话 `ses_f3357846dffe9PFYIvonMwasrU` 刚刚启动，并在派发后极短时间内被中断（消息 `msg_0ccf4b069001U3kAGk2kNbNZWh` 标记为 interrupted）。

经对工作区和磁盘状态的全面检查，fix-2 在中断前仅执行了基线日志备份操作，在 `out/streamline-fg-p0/gate1-baseline/` 目录下复制了历史日志并生成了哈希记录，**未对任何探针源代码进行任何修改**。

具体代码核查结果如下：
- `tools/tests/streamline_fg/` 目录下未发现 `slFreeResources` 的调用或动态导入；
- `streamline_fg_probe.cpp` 仍在使用单组 `acquire` 与 `present` 信号量，且等待 fence 仍位于 acquire 之后；
- 未实现未提交 use token 的异常捕获与丢弃逻辑；
- `c.reset` 依然硬编码为全局 `frame == 1`；
- 清理流程依然在使用已被证明无效的 `slSetFeatureLoaded(..., false)`。

因此，当前代码仍保留上述 Gate 1 缺陷，没有完成本轮修复。不能认定它与失败日志对应的二进制完全一致：第 5 节已记录源码、EXE 与日志的时间差。后续验证必须记录实际运行的 EXE 哈希。

## 8. 后续恢复执行与行动指南

当用户未来发出继续恢复指令时，后续接替者应遵循以下行动指南：

### 范围与权限约束
1. 修复范围严格限制在探针专用文件内（`tools/tests/streamline_fg/*`、`cmake/LoStreamline.cmake`），生产代码（`LostOdysseyRecomp/gpu/*`、Plume 及公共门面）不得改动。
2. 保持对既有测试结果的复用，严禁无故重复运行已经通过的 CPU 4/4 测试套件或生产链接构建。
3. 严格禁止在未经显式用户授权的情况下执行 Git commit、push 或发布操作。

### 探针修复与验证步骤
1. 在 `streamline_runtime.h/.cpp` 中补充 `slFreeResources` 函数指针导入与安全包装。
2. 在 `streamline_fg_probe.cpp` 中重构信号量管理，改为按交换链图像建立独立的 render-finished 信号量，并将 fence 等待提前至 acquire 前。
3. 调整退出顺序：通过带真实 token 的 Off Present 生效停用，排空后在两套 session 均存活的状态下调用 `slFreeResources`，再释放原生 feature，最后分别关闭 session 和卸载 Streamline。
4. 修复 pending use 异常清理与 resize 局部首帧 reset。
5. 重新编译探针，执行单次完整的受控测试序列，记录最终生成的二进制文件哈希及完整的运行日志。
6. 取得退出码 0 后，按 Gate 1 要求筹备外部显示侧生成帧证据。

### 项目看板与会话恢复信息
当前关联的 GitHub Project 为：`https://github.com/users/freefrank/projects/3`。
此前本地 `items.json` 与中英文 ROADMAP 虽完成过 195 项同步，但尚未反映本轮最终探针失败及停机状态。当前切勿擅自执行同步，待后续修复取得阶段性实质成果后再统一授权推进。

恢复工作时各专员会话 ID 如下。取消后保留的 Fixer 2、Oracle 使用 `task_revive`；已完成且可复用的会话通过 `subagent` 携带原 `sessionID` 继续，先核对当时的任务面板状态：
- 门面与契约实现（Fixer 1）：`ses_f3364202effe4tMyklOeMRhVe3`
- 探针修复（Fixer 2）：`ses_f3357846dffe9PFYIvonMwasrU`
- 架构与门禁评审（Oracle）：`ses_f335eeb2dffexMz3oxg4dJ3qNj`
- 结构勘察（Explorer）：`ses_f336cbbbbffeJG3x3fgL3ny1PQ`
- SDK 与前置调研（Librarian）：`ses_f336c9142ffeCEHaiVsPffjXEc`
- 项目管理同步（Project Manager）：`ses_f3363e8b1ffe8h97Ce5BBCXtrX`

## 当前 Codex 可续接任务

OpenCode 与 `oh-my-opencode-slim` 的未完成项已整理为本地清单：[fsr-dlss-fg-imported-tasks.zh-CN.md](fsr-dlss-fg-imported-tasks.zh-CN.md)。该清单的 `STOPPED` 状态覆盖历史 `running` 记录，并保留 Gate 1、P0 修复、受控证据、材料复审及 P1–P4 依赖边界。
