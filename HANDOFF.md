# 历史交接：v0.6.11 之后原生 DLSS、设置状态反馈与渲染捕获更新

> 本文记录 2026-09-22 的 DLSS 与诊断捕获工作，作为历史验证细节保留。当前代码与发布状态见[项目状态](docs/STATUS.md)；FSR P2 当前状态见[当前交接](docs/notes/fsr-dlss-fg-codex-handoff.zh-CN.md)。

日期：2026-09-22。本页所述构建与验证属于当时提交 `0625923` 及其工作区状态，不应作为当前 HEAD 或当前未提交状态的说明。相关功能未包含在 v0.6.11 公开包中；公开 Release 事实见项目状态与更新日志。

---

## 一、目标与已完成摘要

在 `v0.6.11` 发布之后，本轮工作集中解决在后续源码审计及真实硬件实测中发现的关键缺陷与稳定性问题：

1. **时序历史生命周期统一（BR-01）**：修复 DLSS SR 与 DLAA 在正常推进时未更新时钟导致超过 250 ms 异常重置历史并丢失抖动的问题（已随 commit `0625923` 提交）。
2. **设备能力快照互斥发布（BR-02）**：消除 CPU 帧规划器与 GPU 工作线程间在首次开启 DLSS 时的可变状态读取数据竞争（已随 commit `0625923` 提交）。
3. **图形菜单稳定性与实际执行反馈闭环（BR-03 & GraphicsRow）**：重构菜单行索引为共享枚举 `GraphicsRow`（0..10），消除了数字下标错位；菜单中 DLSS 的 `Active` 状态必须匹配真实渲染器采用 DLSS RGB 且经 checked Vulkan 检查成功提交，细化区分持久闭锁与动态回退原因。
4. **结构化诊断状态日志**：设置保存成功显式记录主要参数；运行时输出包含可读原因、尺寸与身份三元组的结构化状态日志，平抑重复刷新。
5. **最终交换链呈现截图与同步 NGX 捕获**：
   - F1 捕获中的 `screenshot.bmp` 更新为交由宿主呈现系统前的最终 pre-present 交换链图像，`guest-frontbuffer.bmp` 保留 resolved host 纹理，成对绑定相同 `XE_SWAP` ticket；
   - 实现 NGX Evaluate 同次输入色彩与成功后 scratch 输出的硬件级拷贝捕获（`dlss-evaluations.json`、输入输出 raw 与 preview）。
6. **DLAA 切换闪退修复（外部 depth 纹理 UAF）**：排查并修复从 Quality 切换到 DLAA 时外部 depth 纹理先销毁、已退休 motion stencil 视图后解构导致的非法内存访问（0x8），实机验证 Quality -> DLAA 切换在本次会话（19:35 至 19:41 日志）无复发。

---

## 二、现有改动与核心模块接口

- **`LostOdysseyRecomp/gpu/temporal_lifecycle.h`**：提供统一的 `gpu::temporal::TemporalConsumerActive`，用于帧首长间隔检测、帧尾重置判定（`!decision.complete || (decision.gap && !decision.gapAlreadyReset)`）。
- **`LostOdysseyRecomp/gpu/upscaling_plan.h`**：定义按值传递的 `BackendDeviceSnapshot`（`backend`、`deviceEpoch`、`deviceReady`、`dlssAvailable`），通过互斥锁进行 `PublishDeviceCapability` 与 `PublishedDeviceCapability`。
- **`LostOdysseyRecomp/settings/menu.h` / `menu.cpp`**：引入 `GraphicsRow` 稳定枚举，按行 ID 处理导航、动作、翻译帮助与自动化测试；帮助区动态展示二行状态文案。
- **`LostOdysseyRecomp/gpu/frame_plan.h` / `frame_plan.cpp`**：`CurrentDlssEffect` 接口实现三元组身份过滤（`deviceEpoch`、`requestSignature`、`geometryEpoch`），同帧 plan 切换保留最新失败原因，`SubmitVulkan` / `WaitForGpuFence` 失败发布 stopped。
- **`LostOdysseyRecomp/gpu/dlss_status_log.h`**：格式化状态输出（`Off`、`AwaitingExecution`、`Submitted`、`Fallback`、`GpuStopped` 等），实现抖动采样与语义去重。
- **`LostOdysseyRecomp/gpu/present_capture.h`** / **`video.cpp`** / **`renderer.cpp`**：
  - 呈现前捕获生命周期：`PrepareDebugCaptureFrame`、`CompleteDebugCaptureFrame`、`PollDebugCapture`；
  - 零捕获请求帧完全不分配、不拷贝、不映射新增捕获所需的 GPU 读回资源。
- **`LostOdysseyRecomp/gpu/dlss_evaluate_capture.h`**：在 `Controller::RecordIsolated` 内实现 Evaluate 前输入拷贝、vendor Evaluate、成功后输出拷贝，并在合成与 UI 前恢复 `VK_IMAGE_LAYOUT_GENERAL`。
- **`LostOdysseyRecomp/gpu/motion_replay_gpu.h`**：retired 视图绑定 `externalDepth` 身份，在 GPU 完成后且在纹理真正销毁前解绑深度视图。

---

## 三、真实证据、命令与测试覆盖边界

| 目标 / 模块 | 准确执行命令 / 目标名称 | 验证结果与覆盖范围 |
|---|---|---|
| BR-01 CPU 契约 | `LoTemporalLifecycleBr01Test.exe` | 154 项时钟推进检查通过；逻辑微调后 14 项 gap 检查通过。 |
| BR-01 GPU 所有权 | `LoTemporalLifecycleBr01OwnerTest.exe` | RTX 5080 D3D12 环境下 12 项检查通过（motion stub，无 NGX）。 |
| BR-02 能力快照 | `LoDlssCapabilitySnapshotTest.exe` | 43 项状态流转与并发一致性检查全部通过。 |
| BR-03 状态分类 | `LoDlssRuntimeStatusTest.exe` | 37 项回退与闭锁状态映射检查全部通过。 |
| 提交错误注入 | `LoVideoSubmissionStopTest.exe` | 14 项检查通过（模拟提交/等待失败时发布 stopped）。 |
| 状态格式化日志 | `LoDlssStatusLogTest.exe` | 400 项 logger 检查通过（真实日志汇、去重与格式化）。 |
| 菜单交互流转 | `LoMenuFlowTest.exe` | 18 张 BMP 截图与 notices 生成通过（按 ID 索引，排版对齐良好）。 |
| 渲染器状态硬件 | `LoNativeDlssRendererTest.exe --status-only` 与 `--plan-identity` | 真实 Vulkan 合成 vendor 扩展（RTX 5080 无 NGX）验证目标采用、checked 提交、输入选择与多用例 plan 切换通过。 |
| 呈现捕获硬件用例 | `LoPresentCaptureTest.exe --backend <d3d12或vulkan> --case <three-frame或failure>` | 两种后端与两种用例组合，共 4 项通过；覆盖 640×360 最终图与 320×240 guest 图、零请求无新增捕获开销及错误注入。 |
| 捕获收尾辅助逻辑 | `LoPresentCaptureTest.exe --case close` | 验证 3 帧完成计数 3、最后尝试帧 12、失败不打 ZIP、清理半写入 BMP。 |
| 深度退休 UAF 验证 | `motion_replay_gpu_test.exe --depth-retirement-only` | RTX 5080 Vulkan D32S8 下 26 项检查全部通过（先解绑视图后销毁纹理）。 |
| NGX 捕获契约 | `LoDlssEvaluateCaptureContractTest.exe --evaluate-capture-contract-only` | RGBA8/RGBA16F 契约、4 项/128 MiB 配额限制与截断、JSON 字段通过（合约测试，非全动态收尾覆盖）。 |
| NGX 渲染器合成 | `LoNativeDlssRendererTest.exe --evaluate-capture-only` | 真实 Vulkan 合成 vendor Evaluate 录制（主要覆盖 RGBA16F）、checked 提交与 Page 级 export 通过（真实 Controller SDK 参数接线经静态复核，公共 F1 完整收尾未在测试中全动态覆盖）。 |

**测试复用规则**：上述已有测试结论全部有效，后续开发或提交时**禁止无意义重跑**。

---

## 四、实机时间线、崩溃分析与最新构建产物

### 1. 实机崩溃与修复时间线
- **崩溃日志**：`build/LostOdysseyRecomp/logs/runtime-1790124441942149.log`。
  - 18:47:21 启动 `16:24:35` 版可执行文件；
  - 18:50:14 保存设置从 Quality 切换为 DLAA（尺寸 2560）；
  - 18:50:18 renderer 线程发生访问违规（AV 读取 `0x8`，RVA `C7F344`，`VulkanTextureView` 析构函数中）；
  - 崩溃基线产物备份于：`build/LostOdysseyRecomp/crash-baseline-6ab30022`。
- **修复与二次实测**：
  - 引入深度退休视图与外部 depth 身份解绑机制；
  - 运行日志：`build/LostOdysseyRecomp/logs/runtime-1790127353540651.log`；
  - 19:35:53 启动新版程序，成功在运行中完成 Quality -> DLAA 切换，成功导出帧 2961–2963，DLAA 在 2887 帧激活 `Submitted`，本次会话持续运行至 19:41:53 无重复崩溃。

### 2. 最新完整构建信息
- **构建命令**：`cmake --build build --target LostOdysseyRecomp`
- **可执行文件路径**：`build/LostOdysseyRecomp/LostOdysseyRecomp.exe`
- **文件大小**：93,635,072 字节
- **时间戳**：UTC 2026-09-23 02:32:58 / 当地 2026-09-22 20:32:58 -0600
- **哈希**：SHA-256 `08d50e3774d18a02d4f6eaf2267472e9fab75db36e3ee970980aa96faf641e9d`
- **源码标识**：`bdd9539ee4f176bdda0d9660bb5621b8a90a09acf8f8faa8427c10f2075c2688`
- **运行依赖**：同目录下配备 `nvngx_dlss.dll`、`dxcompiler.dll`、`dxil.dll`。

---

## 五、捕获文件判读规范与判读要点

在新导出的渲染状态归档包中，文件判读须严格遵守以下技术事实：

1. **`screenshot.bmp`**：呈现前（pre-present）提交给宿主交换链的最终画面。在启用了超分辨率、黑边裁切或后处理时，该图包含这些效果，但不包含桌面混成器或物理显示器的额外缩放。
2. **`guest-frontbuffer.bmp`**：渲染器解析的 host 前缓冲纹理。由于架构设计，在开启 DLSS/AA 时该表面**可能已经包含了后处理或超分结果**，它不是“DLSS 前的纯净输入”，也不保证维持 720p 或 guest RAM 原始布局。
3. **`dlss-evaluations.json` 及 `dlss-input-NNN` / `dlss-output-NNN`**：
   - 记录同次调用中真实的 Evaluate color 输入与 scratch 输出（SR 模式下输入尺寸较小，DLAA 模式下输入与输出同尺寸；不承诺为唯一原始前级输入）；
   - 包含原生二进制 `.bin` 与夹取预览 `.bmp`（RGBA8 或 RGBA16F）；
   - 记录该次调用的真实 subpixel jitter、历史重置原因、执行周期等元数据；
   - 只有在 vendor `EVALUATE_DLSS_EXT` 真正返回成功后才会抓取 output，非无条件执行；
   - 未执行 Evaluate 的帧会给出明确的 `zero_evaluate_reason`，绝不伪造图片。
4. **状态与停顿语义**：
   - 状态中的 `Submitted` 严格代表渲染器采用了 DLSS 目标并经由 checked 检查成功提交 Vulkan 命令列表，**不代表 GPU 执行已经完成或显示屏已呈现**；
   - F1 捕获会执行 GPU 读回和磁盘 I/O，可能会造成渲染停顿；停顿引发的帧间隔重置（gap reset）是预期内的保护机制，不能当作普通运行下的稳定性缺陷。

---

## 六、本地证据文件（被忽略且不分发）

本地保留了用于复核的真实样本，未包含在 git 分发中：
- 崩溃日志：`build/LostOdysseyRecomp/logs/runtime-1790124441942149.log`
- 修复后会话日志：`build/LostOdysseyRecomp/logs/runtime-1790127353540651.log`
- Off 基线捕获：`build/LostOdysseyRecomp/captures/render-17901245452315165-f4879.zip`
- 崩溃前 Quality 捕获源文件：`build/LostOdysseyRecomp/captures/render-17901245966387624-f6603/` 及未完成的 `.partial`
- 修复后 DLAA 捕获：`build/LostOdysseyRecomp/captures/render-17901274240213977-f2961.zip`
- 对照截图目录：`out/capture-compare-20260922/`

---

## 七、后续操作指引与剩余待办

### 1. 用户下一步操作
- 用户仅需启动最新构建的 `LostOdysseyRecomp.exe`（20:32:58 版），分别在 **Quality** 与 **DLAA** 模式下按 F1 导出渲染状态，以获取真正包含 `dlss-evaluations.json` 与前后输入输出图片的完整归档；
- **无需重新跑 Off 模式导出**，已有的 Off 捕获完全有效。

### 2. 剩余待办事项（严禁越权标注为已完成）
- 诊断路径 K-01（`LO_NO_RENDERER` 与原生等待失败时的资源释放顺序）本轮未动，维持待办；
- 设置界面改进建议中第 3 至 6 项（理顺传统 AA/滤镜关系、设置交互打磨、帧首帧尾实景回归、纹理分配优化）维持待办；
- 真实游戏内画质表现、细线与遮挡稳定性、运动响应及玩家端整体验收仍待用户完成；
- 版本尚未发布至公开 Release。
