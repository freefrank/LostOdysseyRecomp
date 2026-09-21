# 原生 Vulkan DLSS Super Resolution 开发与交接指南 (Native DLSS Handoff & Guide)

日期：2026-09-21
特性分支：`dlss`
基线提交：`main@5b765f617ec511a2f76aa9da7923a8c122679d2c`
已推送到远端的阶段提交：`origin/dlss` @ `91bf37ea846189f5e7b1f4c12ca631e5ed510203`（P0: `089676f`，P1 尺寸: `c6bc50b`，P1 输入: `3f40030`，文档: `91bf37e`）
当前状态：2026-09-21 已恢复 P2 开发并分项推送；输入/提交生命周期、目标提升边界和合成测试已补强。P2 尚未完成，Gate 3 未通过，游戏内 SR 仍受未知颜色编码守卫保护。历史检查点与 RTX 记录保留，下方续开发结果优先于历史暂停状态。

---

## 2026-09-21 续开发结果

已新增 CPU-only CMake/CTest 与 Windows/Linux CI，修复晚期目标提升中的隐式 Flush、附件借用前的恢复顺序、深度/尺寸不匹配、NGX feature 安全重配置、提交 use 的 fence 生命周期、SR 有效区域外的填充像素，以及输入区域整数溢出。

详细提交、复现命令和证据范围见 [P2 续开发记录](native-dlss-p2-progress-2026-09-21.md)。新证据包括 5 组本地 CPU 测试、实际 renderer 编译单元、SDK 开/关构建与 report 测试，以及软件 Vulkan 执行生产像素 shader 的 256 个 FP16 RGBA 精确读回检查；本轮未运行 RTX NGX 或完整游戏。

仍需运行时颜色资格证据、完整目标映射路径验证，以及提交失败/设备丢失后的终止状态与 fence 等待处理。禁止据此删除 `Unknown` 守卫或标记 Gate 3 通过。下文暂停/取消复审文字描述的是此前检查点，不代表本次没有继续开发。

## 1. 阶段状态总览与总体边界

原生 NVIDIA DLSS 超分辨率（Super Resolution, SR）工作流划分为四个阶段（P0–P3，P4 插帧 Frame Generation 暂缓）：

1. **P0 基础与桥接（已通过并推送到远端）**：Vulkan 扩展协商、Plume 外部命令流桥接、NGX 官方能力探测（`LoNativeDlssProbe`）、缺失运行库受控退出（exit 1）。
2. **P1 时序输入与计划（已通过并推送到远端）**：CPU 帧计划器 24-word 快照、真实低内部分辨率光栅化、NGX 目标尺寸查询缓存、渲染器 pre-TAA 颜色、R32 深度与未抖动几何运动矢量采集、GPU fence 生命周期管理、`LO_DLSS_INPUT_PROBE=1` 诊断模式。
3. **P2 执行与目标提升（已提交未完成检查点，已暂停）**：
   - Lane A 完成了持久化 NGX 会话控制器（`gpu::dlss::Controller`），支持 `NGX_VULKAN_CREATE_DLSS_EXT1` 与 `NGX_VULKAN_EVALUATE_DLSS_EXT` 录制，并对原生 Vulkan `vkResetCommandBuffer`、`vkBeginCommandBuffer`、`vkEndCommandBuffer` 实施校验。独立求值测试 `LoNativeDlssExecutionTest` 通过。
   - Lane B 完成了渲染器侧 SR 路由调度、单调提交序号追踪、未知色彩编码旁路保护、基于停放低分辨率目标的目标提升架构以及诊断捕获钩子（`p2-oracle.jsonl`）。
   - 生产端重采样函数（`DrawPromotionResample`）通过入口自检参数 `--self-test-scene-copy-promotion`（源码门禁宏 `LO_RENDERER_P2_SELFTEST` 默认 `OFF`）在本地 RTX 5080 上完成验证（4×4 升至 8×8，64 个 RGBA 像素 0 不匹配，1 字节 alpha 容差）。
   - 初审提出的 4 项修复（SR 读回守卫、验证层宏显式开关、JSON 括号闭合、自检析构前等待 GPU 队列完成）已全部修改并通过针对性验证（包含自检资源清理与重采样验证 `evidence-cleanup/native-dlss-p2-resample.json`，mismatch 0）。依用户明确指示，取消形式化复审，不宣称 Gate 3 通过，工作暂停交接。
4. **P3 与后续边界**：
   - 游戏默认色彩编码保持为未知状态（`ColorEncoding::Unknown`），受静态审查守卫保护，在游戏运行时完全绕过 SR 评估调用。
   - 尚未在实际游戏过程中进行 DLSS 调度，不能向玩家宣称 DLSS 在游戏中可用。
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

后续接手代理开展工作时，原则上复用已通过的 P0/P1 测试（62 项 CPU、112 项 GPU 检查）与 P2 独立测试结论；仅当直接影响对应结论的行为或编译配置发生变更、或出现新的失败证据时，才重跑直接受影响的最小项目。首要行动如下：

### 第一步：获取真实像素着色器常量与生产管线资格
- 离线分析已知后处理像素着色器 `b4b4d54a7a2d6b96` 最终 RGB 计算为 `exp2(c10.x * clamped log2(v))`，指数依赖运行时常量 `c10.x`。已知 UNORM 存储格式不代表确定为 gamma 还是线性。
- 启动三帧诊断捕获，从 `p2-oracle.jsonl` 中采集实际运行时的像素着色器常量（`c0`–`c10`、`c255`）以及完整的 `producer -> resolve -> copy` 资源分配与版本链条。
- 在未完成上述捕获资格认定前，保持 `ColorEncoding::Unknown` 旁路保护不变，严禁盲目向实际游戏开启 SR 调度。

### 第二步：核验与修复生产端目标晋升映射 (Destination Promotion Mapping)
当前代码库中已包含目标晋升基础实现，接手代理不应从零重复实现，而应重点核验、修复并在生产测试中验证：
1. **核验已冻结的目标上下文**：确认 `DrawImpl` 中在 `fullSceneCopy`、`ObserveColor` 及输入尺寸校验后冻结的上下文正确性：
   - 存储键：`key = {colorInfo & 0xFFF, ColorClassOf((colorInfo >> 16) & 0xF), pitch, 0, false}`。
   - 原始颜色分配序号、客户机格式/尺寸（`guestWidth`/`guestHeight`）、物理分辨率及宿主格式。
   - 几何周期 `activePlan.geometryEpoch` 及输入/输出尺寸（`input.width/height`，`output.width/height`）。
   - 未缩放的客户机视口与窗口偏移剪裁区（注意 `output.x/y` 属于呈现阶段，不可在此叠加）。
   - 晋升目标分辨率与通过 `ScaleX/ScaleY` 推导对齐后的物理缓冲分配。
2. **核验激活与停放管理**：
   - 确认在最终帧缓冲区绑定前激活晋升目标：旧目标 RGBA 重采样至晋升目标，置换 `renderTargets[key]` 映射，原低分辨率目标保留在 `parked` 字段中供后续操作复用。
   - 保证在激活与后续绘制之间不发生因 Upload 引起的隐式 Flush。
3. **核验恢复与降级边界**：
   - 确认遇到不兼容操作（深度/模板网格不匹配、基址相同但 pitch/class 变化、尺寸增长、下一帧首次访问或目标转移）时，正确调用恢复流程：将当前晋升目标 RGBA 内容重采样回低分辨率停放目标，恢复原始映射并清除覆盖。
   - 确认中途 Flush 操作时保持现有的目标映射有效。
4. **验证 Alpha 保护与 UI 渲染顺序**：
   - 确认在合成 Pass 中使用独立的合成着色器将客户机 Alpha 正确回写，后续 UI/文本绘制直接绘制在高分辨率目标上，避免 UI 混入超分辨率。
