# FSR分支修复：录制恢复、设备丢失与Vulkan内存选择

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
