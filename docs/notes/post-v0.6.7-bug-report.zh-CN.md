# Bug report：v0.6.7 之后的代码审计

审计日期：2026-09-22。基线为 `v0.6.7`（`f92c24da03816b4c0c7664fbc8589169a205b555`），终点为 `main` 的 `83655ac29d5aba85adc575c77ddf3269c8f40b6c`，即仓库记录 v0.6.11 发布后的状态。本报告中的行号均对应这个终点。改进方向另见[改进建议](post-v0.6.7-improvement-proposals.zh-CN.md)。

## 结论

本轮确认三项新增问题：**一项 P1，会破坏正常 DLSS/DLAA 的连续时序历史；两项 P2，分别是首次开启 DLSS 时的数据竞争，以及设置界面缺少实际回退状态反馈。** 另复核了一项已有记录的诊断路径资源销毁问题，单独列出。未发现证据充分的 P0，也没有在本次变更范围内发现直接破坏存档的路径。

这些是源码和调用链审计结论。本轮没有修改运行时代码、启动游戏、构建或重复执行现有测试；没有声称已经复现崩溃或完成画质验收。优先级按正常玩家路径、触发频率和实际影响划分，范围不包括企业安全加固、恶意输入攻防或全量资源完整性认证。

| 编号 | 级别 | 问题 | 适用范围 | 证据状态 |
| --- | --- | --- | --- | --- |
| BR-01 | P1 | DLSS/DLAA 帧尾没有更新时钟，导致持续重置历史并关闭 jitter | 普通 DLSS/DLAA 游戏路径 | 当前源码确认；画质影响未实测 |
| BR-02 | P2 | CPU 规划器直接读取 GPU 线程修改的能力报告 | 以缩放关闭启动，再首次开启 DLSS | 当前源码确认竞争；未复现崩溃 |
| BR-03 | P2 | 保存 DLSS 后，菜单没有显示实际未启用及回退原因 | D3D12、非 RTX 或 NGX/档位不可用 | 设置与生产 fallback 调用链确认 |
| K-01 | P2，诊断路径 | 禁用 Renderer 后，等待失败仍进入 GPU 资源销毁 | `LO_NO_RENDERER` 与原生等待失败同时出现 | 已有记录，本轮复核仍在 |

P1 应优先修复；BR-02 和 BR-03 可在对应条件下处理；K-01 仍是较低优先级的诊断路径事项。以下建议是待办，尚未实施、验收或发布。

## BR-01：正常 DLSS/DLAA 持续重置历史

**定位：** [renderer.cpp:6990](../../LostOdysseyRecomp/gpu/renderer.cpp#L6990)、[帧时间更新:7046](../../LostOdysseyRecomp/gpu/renderer.cpp#L7046)、[长间隔处理:4181](../../LostOdysseyRecomp/gpu/renderer.cpp#L4181)。新增性：`v0.6.7` 没有普通 SR 路径；`744ab91` 加入 `dlssSrRequested` 后，旧帧尾判定未相应扩展，DLAA 继承同一路径。

### 触发与后果

在支持 DLSS 的配置上选择任意 SR 档位或 DLAA，以默认诊断设置正常运行，不设置 `LO_DLSS_INPUT_PROBE=1`。距离初始化或最近一次成功更新 `temporalFrameTime` 超过 250 ms 后，就满足问题条件；不需要真的出现 250 ms 卡顿。

普通 SR 路由得到 `temporalExperiment=false`、`temporalInputProbe=false`、`dlssSrRequested=true`。初始化逻辑在 [4155–4158](../../LostOdysseyRecomp/gpu/renderer.cpp#L4155) 正确开启 jitter，但帧尾只检查前两个布尔值：

```cpp
if (auto& owner = g_renderer->temporalHistory;
    owner && (g_renderer->temporalExperiment || g_renderer->temporalInputProbe)) {
    auto& r = *g_renderer;
    const auto now = std::chrono::steady_clock::now();
    // ...
    r.temporalFrameTime = now;
}
```

这里省略了无关语句；上述条件和赋值对应生产帧尾。普通 SR 永远跳过这个时间更新。下一帧在 `4181` 比较的仍是过期时间，于是每帧都执行 `Reset()` 并增加 `temporalEpoch`。同一分支的 jitter 选择也遗漏 `dlssSrRequested`，把前面开启的 jitter 又关掉。

[HistoryOwner::Reset:282](../../LostOdysseyRecomp/gpu/temporal_history.h#L282) 清除两个帧槽的 `inputsComplete`；[BeginFrame:299](../../LostOdysseyRecomp/gpu/temporal_history.h#L299) 又处理不断变化的 epoch；[CaptureColorInputs:355](../../LostOdysseyRecomp/gpu/temporal_history.h#L355) 因前帧不完整持续要求重置。因此正常 DLSS/DLAA 无法维持预期的帧间累积。NGX 的 Create/Evaluate 仍可能返回成功。

**可以确认的是历史和 jitter 生命周期错误。** 细线、稳定性和运动画质会受多大影响，需要修复前后的固定场景对照；本轮没有测量这些结果，也没有把该问题描述为已发生的黑屏或崩溃。

### 为什么现有通过记录没有排除它

[native_dlss_renderer_gpu_test.cpp:3](../../tools/tests/native_dlss_renderer_gpu_test.cpp#L3) 定义 `LO_RENDERER_P2_EMBEDDED_TEST`，生产源在 [6621](../../LostOdysseyRecomp/gpu/renderer.cpp#L6621) 排除公共游戏入口。fixture 直接驱动目标提升、NGX、提交与恢复，手动推进帧编号，不经过完整 `DrawImpl` 与公共帧尾。CPU planner 中“稳定 DLAA 不改变 geometry epoch”的检查也不覆盖这里的 renderer `temporalEpoch`。

本地已有 `game-sr-runtime.json` 和它指向的日志没有连续成功帧的 temporal epoch、reset、jitter 序列；日志标记为历史 `c2f06023b7df-dirty` 构建。这些记录证明过有界 SR 执行，不能支持或反证当前问题。

### 最小修复与回归

统一当前帧是否存在 temporal consumer 的判定，用于帧首、长间隔处理和帧尾；同时修正长间隔后的 jitter 选择。不要仅修改时间戳条件而遗留第二处遗漏。

增加一个经过生产帧生命周期、使用可控时钟的有界回归：分别以 SR 和 DLAA 连续推进至少 20 个 16 ms 帧，确认运行总时长超过 250 ms 不会引发 epoch 增长，完整输入建立后 reset 结束，jitter 持续。再插入一次 300 ms 间隔，确认只发生一次预期重置，后续正常恢复。最后在同一场景对照修复前后画面，不需要重跑无关的打包或 shader 测试。

## BR-02：首次开启 DLSS 时的能力状态数据竞争

**定位：** [video.cpp:481–488](../../LostOdysseyRecomp/gpu/video.cpp#L481) 读取可变报告；[dlss_ngx.cpp:638](../../LostOdysseyRecomp/gpu/dlss_ngx.cpp#L638) 写入 `report_.state`。读路径由 `3f400309` 引入，运行时写路径由 `d018be72` 引入。

### 触发与因果

以 Upscaler 关闭的配置启动，在游戏设置中首次开启 DLSS。启动阶段的尺寸预热只在 [video.cpp:713](../../LostOdysseyRecomp/gpu/video.cpp#L713) 检查到 DLSS 配置时执行，其余情况会在运行中建立持久 NGX session。

两端调用链如下：

- 游戏 CPU：[`teleport.cpp:417`](../../LostOdysseyRecomp/debug/teleport.cpp#L417) → [`BeginCpuFrame:55`](../../LostOdysseyRecomp/gpu/frame_plan.cpp#L55) → `BackendDeviceState()` → `g_dlssController->Report().state`。
- GPU worker：[`PresentFrontbuffer:1259`](../../LostOdysseyRecomp/gpu/video.cpp#L1259) → [`ServicePendingDlssSizing:497`](../../LostOdysseyRecomp/gpu/video.cpp#L497) → `QueryOutputSizing()` → `EnsureSession()` → 修改 `report_.state`。该 worker 在 [`command_processor.cpp:236`](../../LostOdysseyRecomp/gpu/command_processor.cpp#L236) 独立创建。

报告字段不是 atomic，读取端也没有锁。仅对 `g_deviceEpoch` 使用 acquire/release 不能同步同一 epoch 下随后发生的报告修改，即使写入值仍然是 `Available`，也存在 C++ 数据竞争。

这是确定的并发契约缺陷，可能使能力快照判断不可靠；**没有证据证明它已导致某次崩溃或存档损坏**，因此列为 P2。

### 最小修复与回归

由 GPU 线程发布一个小型、按值且线程安全的设备能力快照，CPU planner 仅读快照。用现有 mutex 保护完整快照，或按明确生命周期发布 atomic 状态即可；不需要围绕整个 NGX controller 添加复杂锁层。

定向覆盖“关闭启动 → 首次开启 DLSS → 返回关闭 → 再开启”。检查 CPU 快照不会直接访问可变报告且配置转换正确；若需要动态竞争证据，使用隔离的线程检查即可，无需重复已有 NGX 图像套件。

## BR-03：DLSS 请求被保存，实际回退没有界面反馈

**定位：** [menu.cpp:202–209](../../LostOdysseyRecomp/settings/menu.cpp#L202)、[帮助文案:275](../../LostOdysseyRecomp/settings/menu.cpp#L275)、[生产计划器:176](../../LostOdysseyRecomp/gpu/frame_plan.h#L176)。由 `943062f` 对外暴露 DLSS 菜单后形成普通玩家路径。

### 触发与因果

使用 D3D12 后端，进入图形设置，选择 DLSS 和任意质量档，保存。菜单无条件提供并启用这些选项，帮助只介绍重建和画质/性能，没有说明当前后端不支持。保存后显示设置已保存，重开仍展示请求的 DLSS 值。

生产 planner 则要求 Vulkan、设备和 DLSS 能力可用、对应档位尺寸就绪，才进入 `DlssSr`；条件不满足时保留 legacy 路径。这一回退本身是正确的保活行为，缺陷在于**设置页面没有显示请求与实际运行的差异和原因**。用户无法在界面判断 DLSS 是否起效。

这个场景不需要特殊环境：Windows 的默认配置是 [`D3D12`](../../LostOdysseyRecomp/settings/config.h#L42)，[后端选择](../../LostOdysseyRecomp/gpu/backend_selection.h#L91) 会优先使用请求后端，成功后不会为了 DLSS 自动改成 Vulkan。Vulkan 下 NGX 不可用也有相同反馈缺口；能力/尺寸错误仅写日志。

### 最小修复与回归

使用 BR-02 的线程安全状态快照，至少显示“需切换 Vulkan 并重启”“当前设备不可用”“临时回退”“实际生效”。保留用户偏好可以，但不能只显示请求值。编辑中的后端设置尚未应用时，区分当前能力与重启后的待检测状态，避免把选择 Vulkan 的用户永久锁在禁用选项上。

定向检查 D3D12、Vulkan 不支持设备、支持设备但尺寸查询暂未完成、DLSS 成功四种状态，核对菜单文字和实际 consumer。后续状态测试可以注入值快照，无需为了文案重新运行所有 GPU fixtures。

## K-01：诊断路径等待失败后仍释放 GPU 资源

**定位：** [video.cpp:527–545](../../LostOdysseyRecomp/gpu/video.cpp#L527)，关联 [renderer.cpp:6640](../../LostOdysseyRecomp/gpu/renderer.cpp#L6640)。此项已登记在[既有验证文档](native-dlss-validation.md)，不是本轮新发现。

设置 `LO_NO_RENDERER` 时，正常 Renderer 的独立 drain 不会执行；`ResetGpu()` 忽略 `WaitForPresentGpu()` 的失败结果后继续释放 presentation、command list 和 device。在原生等待失败且无法证明 GPU 已结束使用资源的情况下，会破坏资源销毁顺序。

该触发需要诊断变量与等待失败同时出现，优先级低于正常游戏路径。建议让 video 自己完成可判定的退出排空，并根据 device-lost 或未确认完成的状态选择一致的释放策略；不要把“尝试过等待”当作“已完成”。本轮未注入设备故障或运行这条路径。

## 审计范围与证据边界

`git diff v0.6.7..83655ac` 共 **28 个提交、81 个变更文件、9,471 行新增、571 行删除**。本轮对新增/修改的运行时代码、构建工具和相关测试进行审查，并沿调用链检查必要的旧代码。没有声称逐行重审整个上游依赖或生成的 PPC 代码。

| 范围 | 审查内容 | 结果/边界 |
| --- | --- | --- |
| Renderer、frame plan、command processor | 24-word 编解码、请求签名、epoch、迟到失败、回退闭锁、CPU/PM4 交接、promotion/restore、跨 Flush、生存期、颜色资格 | BR-01；其余未确认新恶性缺陷 |
| NGX、video、Plume 补丁 | 能力/尺寸查询、Create/Evaluate、feature 重建、提交序号、fence、错误停止、退出、扩展协商 | BR-02；复核 K-01 |
| Temporal/MV 与尺寸工具 | 当前深度/颜色采集、reset、lazy TAA、replay pending 恢复、资源退役、输出区域、sizing cache | 联合确认 BR-01 后果；未把合法 fallback 当作 bug |
| 设置与入口 | 配置保存、DLSS 档位、隐藏行、滚动、导航、语言、入口自测开关 | BR-03；发现测试截图行号过时，见配套建议 |
| 构建、CI、分发 | CMake SDK 接入、Windows batch、SDK 获取、ZIP/AppImage、运行库和许可文件布局 | 未确认正常发布路径的新增恶性缺陷；自定义 SDK 路径和测试覆盖可改善 |
| 23 个测试/测试支撑文件 | planner、DLAA、输入、故障/退役、renderer/composite、菜单、打包、oracle | 审查测试的实际覆盖，不把合成 fixture 当作完整游戏 |
| 15 个文档/状态文件 | 基线、历史测试、当前实验边界、发布记载 | 用于证据核对；若历史章节与当前状态冲突，以明确版本为界 |

前四项合计覆盖 31 个运行时变更文件；构建/补丁/CI/忽略规则合计 12 个文件，其余为上述 23 个测试文件和 15 个文档/状态文件。

本轮复用仓库记录的 CPU DLSS/DLAA、菜单、合成 Vulkan、RTX NGX 和打包验证。源码与预期不一致之处已按独立缺陷列出；历史测试的通过范围没有被扩大。未对当前二进制执行全游戏、Linux RTX、设备丢失或存档回归，也未提交、推送或修改既有验收状态。

## 未提升为确认 bug 的重点

- `motionInvalidity` 已产生，但 [NGX Evaluate 参数:766](../../LostOdysseyRecomp/gpu/dlss_ngx.cpp#L766) 没有消费该纹理。SDK 的 bias-current-color mask 与现有 invalidity 语义不能直接等同。需对新出现物体、被拒绝的 replay draw 和遮挡边缘做固定场景验证，再决定 reset/fallback/mask 策略，不能只凭“有一个 mask 没绑定”断言画面已经错误。
- 原有 execution fixture 使用返回 `void` 的 submit/wait，不能证明提交或等待失败时的安全退役。此项是验证缺口，已有文档记录；正常 Renderer 的 checked 路径另有覆盖，不据此宣称正常游戏必然释放过早。

建议先处理 BR-01、BR-02，并补齐 BR-03 的状态反馈，再进行针对性的真实画面验收；设置界面其余改进可按配套文档独立安排。

## 2026-09-22 修复进展与验证状态

本节记录 2026-09-22 对 BR-01、BR-02、BR-03 及相关菜单稳定性的实现与验证进展。前面的审计章节保留作为原始历史基线，不修改当时发现的问题描述与边界。诊断路径项 K-01 本轮未做改动。本轮功能实现、定向验证与全程序可执行文件构建均已完成；真实游戏画面验证、玩家验收及版本发布仍待完成。

### 1. BR-01：时序生命周期统一与帧间隔时钟推进（已随 commit 0625923 提交并合入 main）

- **实现**：引入 `LostOdysseyRecomp/gpu/temporal_lifecycle.h`，提供统一的 `gpu::temporal::TemporalConsumerActive`（涵盖 legacy TAA、input probe 与 DLSS SR/DLAA）。在帧开始、长间隔检查（`ApplyTemporalLongInterval`）与帧尾判定（`EvaluateTemporalFrameEnd`）中统一应用此规则，确保普通 DLSS SR 与 DLAA 在正常推进时正确更新 `temporalFrameTime`，并在超过 250 ms 间隔后保留 subpixel jitter。
- **边界修正**：在独立评审中发现，如果帧首已因长间隔执行了 reset 并记录 `gapResetFrame == frame`，原逻辑的 `gapAlreadyReset` 可能会误屏蔽帧尾因场景未就绪或未提交引起的重置需求。逻辑已修正为 `decision.reset = !decision.complete || (decision.gap && !decision.gapAlreadyReset)`，确保任何未完成帧均会执行历史清除。
- **验证**：
  - CPU 跨平台契约测试 `tools/tests/temporal_lifecycle_br01_test.cpp`（CMake 目标 `LoTemporalLifecycleBr01Test`）：原 154 项时钟推进与生命周期检查通过；在上述逻辑修正后，相关的 14 项 gap 检查全部通过。
  - GPU 真实所有权测试 `tools/tests/temporal_lifecycle_br01_owner_test.cpp`（CMake 目标 `LoTemporalLifecycleBr01OwnerTest`，条件为 `WIN32 AND LO_BUILD_GPU`）：在 RTX 5080 D3D12 环境下使用 motion stub 纹理执行真实 `HistoryOwner` 验证（无 NGX 依赖、不启动游戏），12 项检查通过，覆盖了初始化、连续推进、300 ms 长间隔（验证 >250 ms 阈值）、恢复序列及重复颜色失效判定。相关测试结果直接复用，本轮未重跑。

### 2. BR-02：设备能力快照与互斥保护发布（已随 commit 0625923 提交并合入 main）

- **实现**：在 `gpu::upscaling` 中定义按值传递的 `BackendDeviceSnapshot`（包含 `backend`、`deviceEpoch`、`deviceReady`、`dlssAvailable`）。设备拥有方在初始化、尺寸查询与退出清理时持有互斥锁发布完整快照（`PublishDeviceCapability`）；CPU 规划器只调用互斥锁保护的 `PublishedDeviceCapability()` 复制该快照，彻底解耦对可变 NGX 控制器 `report_` 的直接无锁读取。
- **验证**：
  - CPU 跨平台测试 `tools/tests/dlss_capability_snapshot_test.cpp`（CMake 目标 `LoDlssCapabilitySnapshotTest`）：在 commit 0625923 中 43 项检查全部通过。在本轮 Active 状态匹配执行层观察之后，测试按新断言更新并重新运行，43 项检查再次全部通过，后续未再重跑。

### 3. BR-03 与设置行索引重构：执行反馈闭环与 GraphicsRow（本轮已实现，待提交）

- **GraphicsRow 稳定枚举**：将图形菜单行号抽象为共享的 `GraphicsRow` 枚举（`0..10`），帮助文本、按键导航、变更响应与自动化测试统一根据行 ID 索引，消除增减选项引起的下标错位。修正了既有测试用例中关于 AA 与帧率行的截图索引断言。
- **执行层状态闭环**：
  - 菜单中 DLSS 的 `Active` 状态不再仅依赖 CPU 帧计划，而是要求同时匹配到来自底层渲染器的真实观察——实际采用 DLSS RGB 输出且经由 checked 检查成功提交 Vulkan 命令。
  - 细化执行层回退与失败原因：区分持久失败闭锁（如缺少 Vulkan、设备不支持、尺寸未就绪、显式失败停止）与动态回退（如运动矢量管线就绪中 motion pending、未确认色彩编码 Unknown、feature 重建中、目标提升 promotion 失败、无合格绘制目标、等待首个结果、输入探针模式及底层停止等）。
  - 运行时状态来自设备周期、请求签名与几何周期三元组身份过滤；同帧内若发生计划切换，保留最新失败原因；在 `SubmitVulkan` 与 `WaitForGpuFence` 出现错误时发布 stopped 状态。
  - 菜单展示的渲染尺寸与输出尺寸直接取自实际执行计划 `execution.plan`；未保存的菜单编辑项继续作为附加说明显示在末尾。
- **验证**：
  - 新增 CPU 测试 `tools/tests/dlss_runtime_status_test.cpp`（CMake 目标 `LoDlssRuntimeStatusTest`）：37 项状态迁移与回退分类检查全部通过。
  - 新增原生调用前错误注入测试 `tools/tests/video_submission_stop_test.cpp`（CMake 目标 `LoVideoSubmissionStopTest`）：14 项检查全部通过，验证底层提交与等待失败时发布 stopped 状态。
  - 真实硬件 Vulkan 测试（`native_dlss_renderer_gpu_test.exe`）：在 RTX 5080 上使用合成 vendor 扩展运行，`--status-only` 验证通过（目标采用、checked 提交、输入选择与帧尾发布）；`--plan-identity` 验证通过（覆盖 fallback->fallback、success->fallback、activePlan 在提交前变更但已排队命令仍保留原 plan、SR 后常规非空批次不降级等边界场景）。
  - 菜单交互测试 `tools/tests/menu_flow_test.cpp`（CMake 目标 `LoMenuFlowTest`）：结合 GraphicsRow 与最新执行状态反馈一次运行通过，在 `out/br03-dlss-menu/` 生成 18 张 BMP 截图与 `notices.txt`。经对齐检查，最短提示 18、最长提示 05、中文提示 06 及未保存质量提示 08 均完整展示且排版良好，未对底层 `menu_render` 额外做视觉侵入修改。
  - 生产受影响对象（`frame_plan.cpp`、`renderer.cpp`、`video.cpp`、`menu.cpp`）增量编译通过。

### 4. 边界说明

- 功能实现与定向测试已完成。BR-01 CPU 与 owner 测试结论严格复用；BR-02 capability 测试因 Active 语义变化更新断言后重新运行 43 项通过并复用；其他既有 GPU 测试套件无重复运行。
- 状态反馈仅代表渲染器采用目标与 checked 成功提交，不宣称 GPU 执行完成、最终呈现或整帧 DLSS 画面。
- 修复 Quality->DLAA 切换时外部 depth 纹理与 motion stencil 视图解构顺序导致的崩溃，新增深度退休测试（`--depth-retirement-only`，26 项检查通过）。
- 完整程序目标 `LostOdysseyRecomp` 成功完成最新本地增量构建与链接（产物 `build/LostOdysseyRecomp/LostOdysseyRecomp.exe`，93,599,744 字节，SHA-256 `eb37e8349775ec343032a7cdeadcfbb53e74ffe915f1b3c0890d938fb5cdbb41`，2026-09-22 19:33:37 -0600，基于 `0625923` 叠加未提交改动；崩溃基线备份于 `crash-baseline-6ab30022`）。用户实机验证成功完成 Quality->DLAA 切换并导出帧 2961–2963 无崩溃（DLAA 在 2887 帧激活）；同次 NGX 真实输入输出与 jitter 捕获需求仍待后续实施。
- 真实游戏内画质与视觉表现、用户体验验收以及版本发布仍待后续开展。
