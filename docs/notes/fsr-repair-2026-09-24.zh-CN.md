# FSR分支修复：录制恢复、设备丢失与Vulkan内存选择

> [!NOTE]
> **2026-09-24 状态说明**：本篇记录为 FSR 分支独立修复时的阶段性验证记录。后续该分支修复已随提交 `6eef30d257f2e14ce30a546217574a0dc74fad69` 合并至 `main` 并包含于公开正式发布的 [v0.6.15](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.15)（Release CI [36044604844](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/36044604844)）。用户已验收实测的 Windows 画面与 Linux 运行验证；P2 全场景完整画质验收依然独立且保持 In Progress。

源码基线：`main`的`5f67b8c3d1f11ee7b1bea55fa3da7e4f00f943bc`。本轮只交付到`FSR`分支，不合并main、不发布版本。P2仍为In Progress；本页不取代[当前交接](fsr-dlss-fg-codex-handoff.zh-CN.md)中的既有画面证据与验收边界。

核心修复提交：`fae701043861719c26920ce5bdaab9745584e5ea`。提交前验证见[Actions运行35976343011](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35976343011)：Windows/MSVC及Linux/GCC的CPU测试组均13/13通过；Linux原生FSR开启/关闭适配器对象、超分调度器、两段转换GLSL/SPIR-V及SDK内存选择前后对照均通过。验证后才将同一SHA-256补丁提交到FSR。临时源码快照、补丁传输与自动提交工作流在收尾提交中移除；仅保留只读权限的正式回归工作流。

## 代码修复

1. FSR录制入口原来把无效帧间隔、相机元数据、抖动和曝光等输入拒绝与上下文失效统一返回Unavailable。渲染器会据此禁用当前请求。现在由生产路径与CPU测试共用的`CheckRecordGuard`区分InputUnavailable、NeedsReconfigure和硬性失败；原生纹理、格式、布局和资源错误仍保持严格拒绝。
2. 超分调度器保留InputUnavailable类型。渲染器在录制失败时先清理未提交token、恢复continuation布局，再记录单帧fallback；不把这一类失败写成长期请求故障。上下文失效走既有排空后的下一帧重建；SDK错误和资源错误没有改成无限重试。
3. Prepare阶段的DeviceLost现在进入StopGpuWork，不再只禁用超分请求后继续提交GPU工作。录制阶段已有的设备丢失处理保留。
4. FSR准备着色器使用反向深度转换，现在显式拒绝Forward深度，避免把“结构完整”的其他深度约定送入反向转换。
5. 固定版本SDK的Vulkan内存选择原来只要求任意一个属性位匹配，并跳过所有CPU可见的device-local内存。构建时生成的SDK副本改为要求全部属性匹配；独显优先不可见本地内存，统一内存设备允许可见本地内存回退；仍排除未启用的AMD device-coherent类型。未修改SDK源文件、固定提交或离线shader manifest。

## 测试与构建修复

原CPU测试组新增了依赖Plume接口的遮罩所有权测试，但CI没有准备子模块及项目补丁。本轮补全这两个步骤与Linux的Xlib开发头文件，并在CMake配置期提供明确的缺失依赖诊断，保留全部原测试。另外修正了混合`uint64_t`与`ull`列表导致的Linux类型推导错误。

新增59项录制/恢复策略检查、16项内存选择策略检查，并把已有投影测试接入CPU测试组。Linux本地完整CPU测试组13/13通过。对固定SDK函数执行真实CMake转换前后的编译对照：两种合成内存布局在旧函数中均失败，修复后均通过。它们证明的是选择算法，不代表这些设备已经实测，也不是OOM注入。

跨平台CPU工作流负责Linux/GCC及Windows/MSVC；原生FSR编译工作流编译开启/关闭FSR的adapter对象和超分调度器，并编译、验证两段现有转换GLSL。实际运行结果以本分支Actions为准；编译对象不等于完成SDK链接、完整游戏构建或GPU画面验收。

## 接续与边界

拉取本分支后，需要重新运行原有CMake配置并重建FSR SDK静态库/游戏目标，使生成的Vulkan backend更新。SDK shader permutations、mask上限、RCAS默认设置、战斗shader映射均未改动，不需要因本轮代码修复重新生成SDK shader permutations。

下一次有明确授权的运行应确认初始化、短暂fallback后恢复、provider切换与战斗画面，并观察是否存在真实内存分配或设备丢失错误。本轮没有启动、控制或关闭游戏，没有读取或改写存档，没有把既有雾效观察判作新增缺陷，也没有验证Steam Deck实机、最新Linux战斗映射或完整P2画质。

## 2026-09-24 合并准备检查

当前FSR分支HEAD为`23f0864`。此前的分支交付历史和验证范围保留；本地另有一处尚未提交的追加修复：审阅发现`tools/tests/native_dlss/CMakeLists.txt`中三项FSR测试重复`add_test`，CPU-only配置的提前`return()`遮住了第二处注册；现已删除第二处，保留GPU配置中的各一次注册。

对追加修复运行`cmake -S tools/tests/native_dlss -B C:/Users/freefrank/AppData/Local/Temp/opencode/lo-native-dlss-reg-Ndlv1o -DLO_NATIVE_DLSS_CPU_ONLY=OFF -DBUILD_TESTING=ON -DLO_NATIVE_DLSS_ENABLE_VALIDATION_LAYER=OFF`，配置生成成功；`ctest --show-only=json-v1 -C Debug`列出16项测试，三项FSR均只注册一次，未执行测试。复用`23f0864`的CI记录：[35976658348](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35976658348)中Windows/Linux CPU组各13/13通过；[35976658408](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35976658408)中Linux原生FSR开／关对象、SPIR-V及selector检查通过。这些结果不覆盖本地追加修复的运行测试。

新增测试用例并在 RTX 5080 硬件上实测可执行文件 `LoFsrAdapterGpuTest --transient-only` 一次通过（PASS）。N 帧完成真实 SDK dispatch 提交并等待 fence 读回；N+1 帧保持相同配置、请求签名、各周期与有效输入纹理，传入非有限 dt（NaN），生产 `Controller::RecordIsolated` 正确返回 `InputUnavailable` 与 `useId 0`，不执行 GPU dispatch 与命令提交，`lastDispatchRenderFrameId` 保持为 N，未重新创建 context，`EnsureSession` 保持 `Ready`；N+2 帧恢复有效 dt（16.6ms）且 `input_reset=false`，成功 dispatch 并触发 SDK gap reset（`gap_reset=true`），输出图像中心蓝色通道由 128 变为 96 证实采用了新输入；N+3 帧无 gap reset 持续 dispatch（`gap_reset=false`）。本次测试仅覆盖 adapter 与 SDK 层的暂态隔离与恢复能力，不覆盖生产 renderer 完整回退链调度，也不代表游戏实机画面通过，不能作为完整 P2 验收或 UMA 硬件验证。

发布与 CI 工作流同步进行了支持更新。`.github/workflows/native-dlss-cpu.yml` 新增双平台独立的 `out/native-dlss-config` 非 CPU-only CMake 生成及 CTest GPU 单次注册校验（Windows 初始化 Plume `D3D12MemoryAllocator` 子模块），原有 CPU 路径保持不变。`.github/workflows/release.yml` 新增 Windows `prepare-fsr` 任务，检出固定 commit `c6efa6bf7f2027b3ec94f28578bb5965eabb9e55` 的 FidelityFX SDK 并生成 Vulkan shader，打包为 `fsr-vulkan-build-inputs` artifact；`release-windows` 与 `release-linux` 下载并消费该 artifact，配置并强制 `LO_ENABLE_FSR=ON` 与 `LO_REQUIRE_FSR=ON`，并在打包步骤中增加 `LICENSE-FidelityFX.txt` 存在性断言；`tools/build_release.bat` 负责转发 `LO_ENABLE_FSR`、`LO_REQUIRE_FSR`、`LO_FSR_SDK_ROOT` 与 `LO_FSR_SHADER_DIR` 四项参数。工作流已完成静态结构校验，尚未在 Actions 中运行，不代表发布产物打包已获验证。

本地 Windows Clang 增量构建（`out/build/windows-clang`）同一次 configure 与 build 均以 exit 0 成功完成，FSR 静态库、`LoShaderPackTool`（SHA-256 `830b314da541b280d9816f069f6069d095a247fcdcc9929a35ff01571f0fe93a`）与完整游戏可执行文件 `LostOdysseyRecomp.exe`（生成时间 `2026-09-24 11:17:48 -0600`，SHA-256 `3689f20b3eab2558de60ba8143822c5fa4aa116fb9bad4ed08e6facecbf5765c`）均已链接成功。构建生成的 `ffx_vk.cpp` 包含 `vulkan_memory_policy.h`，链接目标包含 FSR 库，暂存许可证与 SDK/shader 一致（SHA-256 `c93d509cd69d50698796c9c3274247352736ef122a3197ace2010f0978e21aa6`），旧可执行文件与 PDB 已备份。过程记录保留于 `out/fsr-repair-build-20260924/windows-clang-configure.log`、`windows-clang-build.log` 与 `final-identity.txt`。

在 Windows 本地构建完成后，用户已明确确认当前画面无问题。针对 Linux 端验证，Linux 执行环境（`psvita distrobox psbuild`）报告游戏链接与 `LoFsrAdapterGpuTest --transient-only` 在 AMD Radeon 8060S RADV STRIX_HALO 上通过（复用 SDK 与 shader，未回传详细 exit code 与产物 hash），用户据此明确确认 Linux 验证通过，补证任务已取消收尾。上述结论覆盖当前画面验收与双平台测试结果，但不代表确定性生产渲染器故障注入序列或完整 P2 门禁全部通过。工作流更新（`native-dlss-cpu.yml` 与 `release.yml`）尚未在 Actions 中触发运行。截至 2026-09-24，用户已授权进行 v0.6.15 版本发布准备；分支合并、tag 打标与 Release CI 正在进行中，未公开前不作已发布断言，P2 状态保持 In Progress。
