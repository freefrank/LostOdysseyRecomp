# 原生 Vulkan DLSS Super Resolution 开发与交接指南 (Native DLSS Handoff & Guide)

日期：2026-09-21
特性分支：`dlss`
基线提交：`main@5b765f617ec511a2f76aa9da7923a8c122679d2c`
当前最新代码提交：`origin/dlss` @ `c2f0602`
当前状态：2026-09-21 持续推进 P2 阶段开发与审阅。最新有界运行记录已在 RTX 5080 上确认真实游戏生产 NGX SR：Quality `1707x960 -> 2560x1440`、DisplayEncoded、reversed-Z；记录为 1 次成功 Create、24 条保留的成功 Evaluate（受 128 条上限限制，不代表总调用数），隔离 use `10684` 的提交序号 `23614` 已接受并提交，快照中仍在处理中；`completed through 23612` 是更早的 SR 完成水位。未记录失败。Gate 3 未通过，未进行视觉或玩家验收。历史 P0/P1、native5、composite、f11889 与 f2347 证据继续保留原有边界。

---

## 2026-09-21 续开发与只读审阅结果

### 最新代码演进与边界校正

在 `dlss` 分支上，代码已推进至 `c2f0602`：
- `59e9dce`：修复生产渲染器主路径，在 recording、submit 或 fence wait 失败时设置 fatal 闭锁、排空队列并阻止后续 GPU 任务提交。
- `19415e4`：测试 fixture 在 Vulkan 上真实执行生产 Prepare/Activate/Record/Flush/Restore 与 mock NGX 分发。
- `0a2af46`：将 reversed-Z 输入约定贯穿到 NGX feature 创建 flags。
- `4047d6f`：新增 `LoNativeDlssRendererTest`，在真实 `gpu::dlss::Controller` 与生产 `Renderer` 之间执行原生编排。
- `9375608`：在嵌入式渲染器测试 fixture 中隔离未使用的游戏入口符号依赖。
- `c2f0602`：在 Windows 构建中链接真实的 portable-pack zstd 解压缩依赖。

此前文档将“完整目标映射与致命错误处理”笼统称为“未实现”，该表述已过时。当前准确边界为：生产渲染器已实现了核心映射、槽位安全的晋升/恢复以及失败时的 fatal 闭锁与队列排空机制；仍有 `LO_NO_RENDERER` 关闭排空边界和旧执行 fixture 未检查 `void` fence wait 两个缺口。

### 已有 CI 验证结果

1. **主仓库 `dlss` CI (`c2f0602`)**：[Run 35629006599](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35629006599) 6 项 CPU 测试全部通过。
2. **Workbench Linux CI (检出候选 `c2f06023b7dfddc92d4413eae2ac7541492c1354`)**：[Run 35629333836](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35629333836) 7 项 CPU 测试通过；CTest 注册的两项测试分别执行：`LoNativeDlssRendererTest`（mock 路径）在 llvmpipe 下通过；`LoNativeDlssRendererNativeTest`（`--native`）在缺少 NVIDIA 硬件时返回跳过码 77。
3. **Workbench Windows CI (检出 `9375608` + workbench 链接修正)**：[Run 35628539860](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35628539860) 成功编译未执行。目前尚无针对 `c2f0602` exact HEAD 的 Windows 编译验证运行。

### Oracle 只读审阅发现的 3 个具体缺口与收口修复

1. **目标尺寸增长晋升丢失重叠区数据 (`renderer.cpp:1753–1773, 3158–3199`)**：
   在渲染目标晋升过程中，若发生尺寸增长（extent-growth），原实现恢复至 `parkedLow`，随后 `GetRenderTarget` 重新分配并退休旧目标导致重叠区像素数据未被保留。
   - *最终修复收口*：已限制在严格同 scale、同 width 的颜色高度扩容（`tex->guestHeight > oldTarget->guestHeight` 且 `tex->height >= oldTarget->height`），执行 1:1 的 `copyTextureRegion`。若 `Begin()` 失败则记录错误并返回 `nullptr`，安全退休资源。废弃的生产自检测试接口已被移除。
   - *独立硬件测试验证*：在独立测试工程中使用 `--extent-only`（`RunExtentGrowth`），针对真实 key 驱动客户机尺寸 1280×736 至 1280×768（内部分辨率 160×90 至输出 320×180，2× scale），对应物理尺寸从 160×92 扩容至 160×96（提升尺寸 320×184）。测试覆盖非均匀图案上传、提升目标局部清空、`GetRenderTarget` 自动恢复扩容、以及扩容后局部清空，验证了全部 14,720 个重叠区像素完全精确匹配（日志 `.cache/evidence/p2-native-run/extent_growth_regression_rev3.log`）。首版 142 checks 与 rev2 崩溃（根因为测试 fixture 解引用已退休的 `HostTexture` 野指针 `0xDDDDDDDD`）已不再作为证据。
2. **`LO_NO_RENDERER` 无渲染器路径缺失最终排空 (`video.cpp:522–545`)**：
   在无 `g_renderer` 的环境或工具路径中，若 `WaitForPresentGpu` 失败，代码仍直接销毁 presentation 资源。当前渲染器的 `Shutdown` 排空仅在 `g_renderer` 存在时调用；`video.cpp` 自身必须提供独立的最终安全排空边界。
3. **旧测试 fixture 未检查 Plume 提交/等待结果 (`native_dlss_execution_test.cpp:156–159, 180–183, 199–202, 231–234`)**：
   旧的独立执行测试使用的是 Plume 的 `executeCommandLists()` 与 `waitForCommandFence()`，两者均返回 `void`，测试在调用后未检查底层原生 submit/wait 状态即无条件发布 serial 并调用 `ReleaseCompletedThrough`，因而无法覆盖和证明失败时的 fatal 闭锁与排空路径。该缺口依然保留在旧测试中。本轮优先采用新研发的 `LoNativeDlssRendererTest` 生产编排，以避免重复修补旧哨兵机制，但旧测试缺陷本身尚未消除。

### Sizing 会话持久化修复与 RTX 验证 (`dlss_ngx.cpp`, `video.cpp`, `native_dlss_probe.cpp`)

针对 `QueryOutputSizing` 因 `ProbeOnce` 退出调用 `Shutdown(true)` 导致后续查询缺少会话返回 `0xBAD00012` 的问题：
1. **Controller 会话持久化**：`gpu::dlss::Controller::QueryOutputSizing` 在当前接口/设备匹配时，若会话未初始化则显式调用 `EnsureSession(device)` 后再查询参数。错误分支使用 `std::nullopt` 代替猜测 raw 结果。
2. **Video 诊断日志**：`video.cpp` 在当前模式未处于 `Ready` 时输出阶段 `QueryOutputSizing` 详细日志（包含质量等级、状态、原生返回码、目标尺寸与设备周期）。
3. **Probe 安全清理**：`native_dlss_probe.cpp` 增加 `ScopeDrain` 确保在设备销毁前执行 `ShutdownAfterGpuDrain`，并返回真实 `ProbeExitCode`。
4. **RTX 硬件验证**：在本地 RTX 5080 上执行 `LoNativeDlssProbe --production-sizing`（`.cache/evidence/native-dlss-p2-sizing-fix.json`），验证生产调用顺序（`ProbeOnce` -> `Query 1280x720` -> `Query 2560x1440`）全部返回 `status: ok`，推荐 Quality 输入分别为 853×480 与 1707×960。上述微调只需编译确认，不重跑尺寸查询。

上述问题均不构成阻碍下一轮原生 RTX 测试或离线 `Unknown` 色彩捕获的 P0/P1 问题，但阻止授予 Gate 3 验收。

### 2026-09-21 前台实机帧捕获与色彩链路分析

`LostOdysseyRecomp` 在隔离工作目录 `out/p2-game-cwd-c2f0602`（游戏资产位于 `LostOdysseyRecompLib/private/disc1`）以前台可见窗口（PID 42720）启动运行。用户进入 3D 场景并按下 `F1` 触发渲染状态捕获：
- **捕获触发机制修正**：文档之前所述 `LO_CAPTURE_REQUEST` 仅设置 `captureFrame` 触发传统的单帧 root dump（如 frame 2909），不会调用 `RequestDebugCapture`。本轮完整的连续 3 帧捕获是由游戏内 `F1` 键触发生成，产物为 `out/p2-game-cwd-c2f0602/captures/render-17900119851502951-f11889.zip`，`capture-info.txt` 确认包含完整的 11889–11891 三帧数据。后续无需用户针对相同色彩链路重复抓帧。
- **色彩链路确证**：通过只读审阅 `p2-oracle.jsonl`、render-state 与着色器，确证完整链路：
  1. HDR 场景缓冲：Allocation 9，地址 `0x09fa0000`，guest format 32。
  2. 后处理 Tonemap Draw 632：VS `9b81c55ca39bb529`，PS `b4b4d54a7a2d6b96`，目标为 EDRAM Allocation 3（guest base 十进制 720，即 `0x2d0`，format 0，FP16 host 10，1280×736，有效区域 1280×720），`colorMask = 7`（保留 alpha）。
   3. 常量确证：PS 常量 `c10.x = 0x3ee8ba2e` (`0.4545454383` ≈ `1.0 / 2.2`) 三帧严格一致，数学表达式为 `pow(saturate(M + 0.1725 * bloom), 1.0 / 2.2)`，输出确认为针对该捕获链路的**显示编码 SDR**（gamma 2.2 曲线），并非精确的标准 sRGB transfer、非已解码、亦非线性 HDR。该结论严格限定于本次分析链路，不向全游戏泛化。
  4. Resolve 与采样：Resolve 至 Allocation 20（`R8G8B8A8_UNORM`），带 RB 交换；Draw 659（VS `8bbd4da701845d16`，PS `cda578aef1724fdc`，`colorMask = 15`，copy blend `ONE / ZERO / ADD`）采样 `shared_texture_info = 0x00160a00`，`sign = 0`（fetch BGR）。RB 交换与 fetch BGR 净效果为恒等映射，无额外 gamma。Draw 632 与 659 之间的 24 次 Draw 均为 `(colorMask & 7) == 0`（深度/模板或仅 alpha 写）。Draw 660 开始绘制 UI，最终输出到前缓冲 Allocation 4（`0x00714000`）。写版本号逐帧递增：Alloc 9 为 207457/207475/207493，Alloc 20 为 207464/207482/207500，Frontbuffer 为 207465/207483/207501。
   5. 关键历史捕获观察：`candidate_ready = true`，但 `temporal_history_verified = false`；该旧三帧计划均为 `consumer = 0 (None)`，`input = 1280x720`，`output = 1280x720`，`epoch = 1`。这是启动阶段 sizing 会话尚未持久化时的捕获结果，不是当前实现状态；Sizing Session Persistence 修复已由 `.cache/evidence/native-dlss-p2-sizing-fix.json` 验证，后续 live-game 记录已确认 `1707x960 -> 2560x1440` 的 qualified SR 生产执行。该历史捕获不应被解读为当前低分辨率仍未验证。
- 完整的轻量化 SDR 色彩资格追踪与几何判定规范已记录在 [docs/notes/native-dlss-color-qualification-plan.md](native-dlss-color-qualification-plan.md)。

### 新原生测试 `LoNativeDlssRendererTest` 的能力与边界

- **可验证范围（执行后）**：Quality 模式复用、Balanced 模式切换、Performance 模式 1080p 重配置、非零有限浮点 RGB 与 Alpha 读回、不兼容目标恢复，以及 Evaluate 失败后的降级回退。
- **不可验证范围**：不能证明运动矢量响应、抖动消除、reversed-Z 深度视觉正确性（测试采用恒定 0.5 深度、零运动矢量与均匀纯色）、真实呈现队列同步、`video.cpp` 呈现边界、客户机 `DrawImpl` 真实绘制管线及性能开销。
- 软件 mock fixture 替代的是平台抽象边界，不能替代真实硬件执行。

---

## 1. 阶段状态总览与总体边界

原生 NVIDIA DLSS 超分辨率（Super Resolution, SR）工作流划分为四个阶段（P0–P3，P4 插帧 Frame Generation 暂缓）：

1. **P0 基础与桥接（已通过并推送到远端）**：Vulkan 扩展协商、Plume 外部命令流桥接、NGX 官方能力探测（`LoNativeDlssProbe`）、缺失运行库受控退出（exit 1）。
2. **P1 时序输入与计划（已通过并推送到远端）**：CPU 帧计划器 24-word 快照、真实低内部分辨率光栅化、NGX 目标尺寸查询缓存、渲染器 pre-TAA 颜色、R32 深度与未抖动几何运动矢量采集、GPU fence 生命周期管理、`LO_DLSS_INPUT_PROBE=1` 诊断模式。
3. **P2 执行与目标提升（开发持续进行中，已确认有界生产执行）**：
   - Lane A 完成了持久化 NGX 会话控制器（`gpu::dlss::Controller`），支持 `NGX_VULKAN_CREATE_DLSS_EXT1` 与 `NGX_VULKAN_EVALUATE_DLSS_EXT` 录制，并对原生 Vulkan `vkResetCommandBuffer`、`vkBeginCommandBuffer`、`vkEndCommandBuffer` 实施校验。
   - Lane B 完成了渲染器侧 SR 路由调度、单调提交序号追踪、未知色彩编码旁路保护、基于停放低分辨率目标的目标提升架构以及诊断捕获钩子（`p2-oracle.jsonl`）。
   - 生产端重采样函数（`DrawPromotionResample`）在历史测试中完成了本地 RTX 5080 验证。
   - 历史暂停记录属于此前的检查点状态；当前 P2 已恢复持续开发与推进，但未达完成与验收标准。
4. **P3 与后续边界**：
   - 当前状态修正：`.cache/evidence/game-sr-runtime.json` 已确认有界的真实游戏 SR 调度；实际物理上传 quad、精确 resolve ordinal 与 net RGB view 的 SDR 路径已通过资格边界，其他候选路径仍为 `Unknown`。画质、运动响应、遮挡、UI、重置行为和玩家验收仍未确认。
   - 游戏默认色彩编码保持为未知状态（`ColorEncoding::Unknown`），受静态审查守卫保护，在游戏运行时完全绕过 SR 评估调用。
   - 已在真实游戏过程中确认有界的 DLSS SR 调度，但不能据此向玩家宣称画质、运动响应、遮挡、UI、重置行为或整体可用性已经验收。
   - Linux 原生运行跳过未跑；包含 SDK 的二进制分发许可尚待确定，不提供二进制安装包发布。

---

## 2. 源码模块所有权与文件映射

| 路径 | 核心职责与 API | 归属与修改内容 |
|---|---|---|
| `LostOdysseyRecomp/gpu/dlss_ngx.h` / `.cpp` | NGX SDK 桥接、参数块生命周期、持久化会话控制器 `gpu::dlss::Controller`、能力探测 | Lane A：实现持久会话管理、Create/Evaluate 录制与命令缓冲原生 reset/begin/end 结果检查 |
| `LostOdysseyRecomp/gpu/dlss_sr.h` | DLSS SR 执行参数契约、执行状态上报结构体 | Lane A/B 共享接口：定义 SR 录制输入与结果枚举 |
| `LostOdysseyRecomp/gpu/renderer.h` / `.cpp` | 渲染主循环、场景绘制 `DrawImpl`、帧缓冲区提升绑定、`DrawPromotionResample`、停放纹理置换 | Lane B：实现 SR 路由、单调提交序号管理、未知色彩旁路、目标提升重采样与恢复 |
| `LostOdysseyRecomp/gpu/frame_plan.h` / `upscaling_plan.h` | CPU 帧计划器、24-word 快照数据包、请求级 DLSS 失败闭锁 | Lane B：维护帧计划状态与降级闭锁 |
| `LostOdysseyRecomp/gpu/temporal_frame_inputs.h` / `temporal_history.h` | TAA 与 DLSS 共享的时序输入、R32 深度、未抖动像素运动矢量契约 | P1 冻结契约，P2 复用 |
| `LostOdysseyRecomp/gpu/video.h` / `.cpp` | Vulkan 上下文、设备扩展协商、呈现队列、渲染队列批次提交与栅栏通知 | Lane B：呈现与渲染共享单调提交序号，安全排空与关闭回调 |
| `LostOdysseyRecomp/main.cpp` | 程序入口、命令行解析、自检引导 | Lane B：受控宏 `LO_RENDERER_P2_SELFTEST` 下支持 `--self-test-scene-copy-promotion` |
| `tools/tests/native_dlss/native_dlss_execution_test.cpp` | 独立原生的 NGX Create/Evaluate 录制与隔离提交回归测试 | Lane A：验证隔离命令缓冲排除与首像素非零哨兵校验 |
| `tools/tests/native_dlss_p2_routing_test.cpp` | 路由决策与闭锁测试 | Lane B：验证请求闭锁与降级行为 |

---

## 3. 固定版本 SDK 依赖与构建配置

本项目使用固定版本的 NVIDIA DLSS SDK（Commit `374959484e79a640feaba44c93ac8cfb0a03f5b5`，版本 `310.9.1`）。SDK 属于专有软件，源码仓库外部维护，禁止提交到 Git 中。

### CMake 配置选项

- `LO_ENABLE_DLSS`（默认 `OFF`）：DLSS 基础设施与运行时总开关。
- `LO_DLSS_SDK_ROOT`：指向本地检出的 DLSS 仓库根目录。
- `LO_DLSS_STAGE_RUNTIME`（默认 `ON`）：构建时自动将 `nvngx_dlss.dll`（或 `.so`）复制到输出目录。
- `LO_NATIVE_DLSS_ENABLE_VALIDATION_LAYER`：独立测试工程提供，默认 `ON`；主工程 CMake 中默认为 `OFF`。
- `LO_RENDERER_P2_SELFTEST`：生产端重采样引导自检门禁，默认未定义（`OFF`）。

### 构建命令参考

推荐在现有已配置的运行时构建目录（如 `out/build/windows-clang`）中增量构建，避免强制重设生成器或编译类型。全新环境构建请先参照 `README.md` 与 `docs/BUILDING.md` 安装工具链并准备必要的代码生成前置条件。

#### 1. 现有已配置目录构建主程序
```bash
# 启用 DLSS 支持
cmake -S . -B out/build/windows-clang -DLO_ENABLE_DLSS=ON -DLO_DLSS_SDK_ROOT="<路径/至/nvidia-dlss-37495948>"
cmake --build out/build/windows-clang --target LostOdysseyRecomp
```

#### 2. 独立 NGX 原生执行测试 (Lane A 测试工程)
使用独立测试工程已配置的缓存与生成器（例如 Visual Studio 2022）：
```bash
cmake -S tools/tests/native_dlss -B out/build/native-dlss-p2-sdk -DLO_ENABLE_DLSS=ON -DLO_DLSS_SDK_ROOT="<路径/至/nvidia-dlss-37495948>"
cmake --build out/build/native-dlss-p2-sdk --config Release --target LoNativeDlssExecutionTest
ctest --test-dir out/build/native-dlss-p2-sdk -C Release -R ^LoNativeDlssExecutionTest$ --output-on-failure
```

#### 3. 生产端重采样引导自检命令 (Lane B 自检)
自检仅验证生产端 `DrawPromotionResample` 在 GPU 上的 4×4 到 8×8 RGBA 重采样与回读，无需启动完整游戏，也无需外部游戏资源：
```powershell
# 1. 在仓库根目录开启自检宏并编译目标
cmake -S . -B out/build/windows-clang -DLO_ENABLE_DLSS=ON -DLO_DLSS_SDK_ROOT="<路径/至/nvidia-dlss-37495948>" -DLO_RENDERER_P2_SELFTEST=ON
cmake --build out/build/windows-clang --target LostOdysseyRecomp

# 2. 在隔离的运行目录中执行自检，指定证据输出目录（例如 evidence-cleanup）
# PowerShell 下创建并切换至隔离工作目录：
New-Item -ItemType Directory -Force out/build/windows-clang/p2-bootstrap-cwd | Out-Null
Set-Location out/build/windows-clang/p2-bootstrap-cwd
& ../LostOdysseyRecomp/LostOdysseyRecomp.exe --self-test-scene-copy-promotion evidence-cleanup
```
实测证据输出到指定目录下的 `evidence-cleanup/native-dlss-p2-resample.json`，已确认 64 个 RGBA 像素 0 不匹配且退出码为 0。

---

## 4. 运行时架构契约与关键不变量

### 4.1 隔离命令缓冲与三段式提交
为防止 NGX 专有分发发生内部异常破坏主渲染流，采用三段式主命令缓冲隔离：
1. **前缀命令（Prefix）**：执行低分辨率场景光栅化、时序输入捕获、空间降级备份绘制，并将颜色、R32 深度、运动矢量和目标转换到 `VK_IMAGE_LAYOUT_GENERAL` 布局。结束前缀时不执行 Flush、不轮换插槽、不重置描述符池。
2. **独立 NGX 主缓冲（Isolated NGX Primary Buffer）**：专用 `VkCommandBuffer` 调用 `NGX_VULKAN_EVALUATE_DLSS_EXT`。如果录制返回非 `NVSDK_NGX_Result_Success`，则将该缓冲从队列提交列表中排除，保留前缀空间升采样作为安全后备，并闭锁下一帧的 DLSS 请求。排除的缓冲等待安全栅栏后回收，禁止直接复位命令池。
3. **后序命令（Continuation）**：在 NGX 成功时执行目标合成；随后执行后处理、UI 与交换链呈现。

### 4.2 资源格式与时序输入契约
- **颜色输入（`pInColor`）**：低分辨率场景颜色缓冲，需与经确认的 SDR 边界完全匹配。
- **深度输入（`pInDepth`）**：单通道 R32 浮点深度图，subresource aspect mask 必须严格遵循 `VK_IMAGE_ASPECT_COLOR_BIT`。
- **运动矢量（`pInMotionVectors`）**：未抖动几何运动矢量，单位为输入像素单位（`previousPixel - currentPixel`），与光栅化抖动采样保持完全相同的坐标系与符号定义。
- **图像布局**：在通过实测的运行配置中，颜色、R32 深度、运动矢量与输出目标在 RecordIsolated 前均转换为 `VK_IMAGE_LAYOUT_GENERAL`（实测通过，但因 Vulkan 验证层不可用，不作为官方规范保证）。
- **Alpha 保护机制**：DLSS 超分辨率仅评估 RGB 数据。当前实现通过独立的后序合成 Pass 将客户机原本的 Alpha 通道与高分辨率 RGB 结合输出，不依赖单一的 colorMask 限制。

### 4.3 提交序号与生命周期管理
- 渲染器与呈现系统统一使用单调递增的 Vulkan 提交序号（`submission serial`）。
- 仅当队列提交成功后序号才递增。若批次被丢弃，禁止虚报完成。
- 持久化会话（Session）与能力参数在设备初始化时分配，重置或尺寸变更时仅重建特征（Feature），严禁每帧反复销毁重建整个 NGX 会话。

---

## 5. 环境隔离与调试捕获规则

接手代理在运行、测试或捕获游戏环境时，必须严格遵守以下环境隔离策略：

1. **工作目录与存档保护**：
   - 调试运行时必须使用独立的测试工作目录（如专用的 build 子目录），严禁直接在用户原始安装目录或存档目录下启动。
   - `user_paths.h` 在 Windows 便携模式下会将存档和缓存重定向至当前工作目录（CWD）。使用明确的 `--game` 路径启动，并在独立的 CWD 中执行，以绝对避免覆盖或修改玩家原始存档。
   - `LO_PROFILE_DIR` 仅重定向客户机用户配置文件（guest profile settings），**不会**重定向宿主应用全部配置或游戏存档文件。
2. **环境变量与运行参数**：
   - 保持 `LO_GRAPHICS_API=vulkan` 与 `LO_NO_UPDATE=1` 处于受控状态。
   - 注意：不存在 `LO_OFFLINE` 环境变量，不能假设游戏离线模式生效。
   - 严格禁止全资源扫描或不必要的内容 Hash 校验，遵循精简原则。
3. **帧捕获触发**：
   - `LO_CAPTURE_REQUEST` 指定包含非零 uint64 序号的文件，序号改变将触发连续 3 帧的状态捕获。
   - `LO_DRAW_TRACE` 接收指定帧号，非布尔值。
   - 捕获完成后状态记录在 `captures/render-*/capture-info.txt`，目标诊断元数据输出为 `p2-oracle.jsonl`。

---

## 6. 接手代理的首要行动项 (Next Agent Action Items)

后续接手代理开展工作时，原则上复用已通过的 P0/P1 测试（62 项 CPU、112 项 GPU 检查）与 P2 独立测试结论；仅当直接影响对应结论的行为或编译配置发生变更、或出现新的失败证据时，才重跑直接受影响的最小项目。执行顺序如下：

### 第一步：本机原生 RTX 5080 执行验证证据（已完成实测）

针对 commit `c2f0602`，独立测试工程 `tools/tests/motion_replay` 已在现有 `out/build/motion-replay-p1`（Ninja, clang-cl, Debug 缓存，`LO_ENABLE_DLSS=ON`, `LO_DLSS_STAGE_RUNTIME=ON`）下成功完成构建。依赖的 DXC 工具位于 `tools/XenosRecomp/thirdparty/dxc-bin/bin/x64/dxcompiler.dll` 且已确认可用，未修改任何受版本追踪的源码。
- 目标程序：`out/build/motion-replay-p1/LoNativeDlssRendererTest.exe`（SHA-256 `7a00626f12a2215f24943d6fceaf91c07e8022e609267f2b3f7a8f847bba58fd`）。
- 运行时库：`nvngx_dlss.dll`（SHA-256 `3975567b8943c53acce397f2b72380092f84f162d00b0d2c7d08a1025c563983`）。
- 运行环境与结果：在隔离工作目录 `out/tmp/isolated-native-p2` 中执行 `./LoNativeDlssRendererTest.exe --native`，退出码为 0，日志记录在 `.cache/evidence/p2-native-run/test_run.log`（共 15 行，硬件为 NVIDIA GeForce RTX 5080，驱动 616.56，`MODE=native_ngx`，无验证层）。
- 实测通过的 5 项用例（全部 `alpha_mismatches=0`）：Quality 模式（853×480->1280×720）、Quality 模式复用、Balanced 模式切换（742×418->1280×720）、Performance 模式 1080p 重配置（960×540->1920×1080）以及 Performance 模式注入 evaluation 失败（`failure=1`）后的安全降级回退。基于未变行为复用，不重复测试。
- 扩容修复通过验证：在 `LoNativeDlssRendererTest --extent-only` 中真实驱动 1280×736 至 1280×768（物理 160×92 扩容至 160×96），全部 14,720 个重叠区像素通过匹配验证（`.cache/evidence/p2-native-run/extent_growth_regression_rev3.log`）。
- Sizing 持久化通过验证：在 `LoNativeDlssProbe --production-sizing` 中验证 `ProbeOnce` -> `Query 1280x720` -> `Query 2560x1440` 序列全部通过（`.cache/evidence/native-dlss-p2-sizing-fix.json`）。

### 第二步：定向视觉与行为验收（进行中）

SDR 色彩资格跟踪及几何判定代码已实现，并通过 CPU qualification boundary；当前前台旧程序（PID 42720）不包含这些新修改。最新 live-game 记录使用更新后的生产路径。

后续计划流程：
1. 针对视觉、motion、遮挡、UI 与 reset 进行前台验收，复用已确认的 live-game 生产执行基线。
2. 继续记录实际下发尺寸与 runtime qualification 结果，不重跑已完成的 sizing、color-qualification、extent-growth 或 cold-start evidence。

### 第三步：补充无渲染器路径的呈现排空边界

在 `LO_NO_RENDERER` 条件下，当 `WaitForPresentGpu` 失败时，在销毁 presentation 资源前增加 `video.cpp` 自身的排空保护。

### 第四步：色彩资格认定与缺陷收口后的真实 SR 验证

真实 SR 调度已经确认；下一步安排运动响应、抖动、遮挡、UI 叠加、重置处理、窗口缩放及性能的定向视觉验收。当前 Gate 3 未通过，未进行用户验收。
