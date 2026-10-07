# FreeSync / G-SYNC Compatible：应用侧VRR接入

> **专题参考。** 下文实现和测量只适用于各自注明的版本与范围；现行交付及验收状态见[项目状态](../STATUS.md)和[路线图](../ROADMAP.md)。

日期：2026-09-28。开发分支：`feature/native-90-120fps`。

## 使用与边界

Graphics中新增`FreeSync / G-SYNC Compatible`开关；默认Off，选择On后保存，运行中生效，无需重启。配置项为`variable_refresh_rate=0/1`。旧配置没有此项时保留原行为。选项控制游戏的呈现与限帧策略，不会代替用户开启显示器或驱动的VRR，也不会宣称检测到显示器正在变频。

先在显示器菜单启用相应自适应同步功能，并在显卡驱动中启用FreeSync或G-SYNC Compatible。NVIDIA的窗口化使用还需匹配控制面板的“窗口和全屏”范围。首次验证建议使用无边框全屏、FG Off。非VRR屏幕或不支持的窗口系统开启此项可能出现撕裂；关闭该选项可恢复本分支原有同步策略（高于60的原生模式仍使用原有immediate策略）。

## 实现

`gpu/vrr_policy.h`统一宿主VSync请求、原生限帧和FG输出预算。普通游戏swap经`video::GetFramePacingTarget`得到有效宿主上限，保存的30/60/90/120目标不变。`PreparePresentation`在acquire前应用异步呈现请求，继续使用既有交换链重建、在途提交等待及FG资源交接流程。暂停时的host overlay单独使用不含FG倍数的限帧。

已识别游戏Present调用点`0x827B4A4C`的interval=2在VRR On时改为immediate，包括30/60档，避免57fps这类上限被60Hz整数VBlank量化。其他调用者、其他interval及标志位保持不变。PPC时基、内核/音频时钟、engine delta和60Hz虚拟VBlank均不缩放。

刷新率由窗口线程读取并通过原子快照交给呈现线程：Windows使用窗口所在显示器的当前显示模式，其他平台通过SDL2当前显示模式查询。每500ms刷新，并在移动窗口、显示器/尺寸变化和重新获得焦点后请求重查。渲染线程不访问SDL窗口；无有效刷新率时记录0，保留既有目标，不猜测为60Hz。这是当前模式刷新率，**不是显示器VRR范围或实际扫描频率检测**。

2026-10-07 更新（[PR #289](https://github.com/freefrank/LostOdysseyRecomp/pull/289)已合并到 main，尚未发布；最新发布 v0.8.53）：运行时已从SDL2迁移到SDL 3.4.18，上一段“通过SDL2当前显示模式查询”描述的是 v0.8.53 及更早的SDL2版本。现行源码在非Windows平台调用`SDL_GetCurrentDisplayMode`；SDL3报告小数刷新率（例如120Hz显示为119.99），`gpu/video.cpp`的`PollDisplayRefresh`四舍五入为整数Hz后再交给`vrr::OutputLimit`，所以“当前模式刷新率减3”的预算规则不变。Windows仍读取窗口所在显示器的当前显示模式。迁移的验证记录中没有VRR或限帧测量，本文的测量范围不变。

D3D12复用Plume已有的flip-discard、能力查询、创建/ResizeBuffers时的`ALLOW_TEARING`。此次补上Present时的实际全屏状态检查：只在窗口/无边框、允许tearing且SyncInterval=0时传入`DXGI_PRESENT_ALLOW_TEARING`，独占全屏不传该标志。Vulkan沿用Plume的异步模式选择；不支持immediate时接受现有后端回退并发出警告，不循环重建交换链。窗口系统或驱动仍可限制呈现节奏。

## 限帧与FG

本项目采用当前模式刷新率减3的输出预算，为整数刷新率报告、分数刷新率和调度误差留余量。这是工程策略，不是AMD/NVIDIA规定，也不是绝对不越界保证。

| 配置 | 有效宿主上限 |
|---|---:|
| 90fps，90Hz，FG Off | 87fps |
| 120fps，120Hz，FG Off | 117fps |
| 120fps，144Hz，FG Off | 120fps |
| 120fps，144Hz，固定2×FG | 70fps，名义总输出140fps |
| 120fps，144Hz，固定4×FG | 35fps，名义总输出140fps |

固定FG按包含原始帧的倍数分配预算，D3D12使用已应用且运行时支持的配置，Vulkan沿用当前固定2×会话。D3D12在FG已就绪但场景临时不能插帧时仍保守预留倍数预算，因此此类场景原生帧率也可能降低。Vulkan根据上一帧的会话可用性决定倍数；模式/场景过渡可能短暂采用上一次预算，不构成每帧物理显示上限保证。

动态DLSS MFG将有效SDK目标限制在输出预算以内，0（自动目标）变为该预算；较低显式目标保留。原生上限也不高于该有效动态目标，但不按最大MFG倍数机械相除。切屏、刷新率变化、VRR On/Off经既有FG reconfigure更新有效请求；VRR会调整宿主帧率上限和动态MFG的有效输出目标，固定FG倍数仍取已应用的配置。单独切换并保存VRR不会覆盖磁盘中的FG provider、mode、multiplier或target；实际切换FG provider或multiplier时，原有菜单逻辑会重置mode和target。SDK请求失败沿用原有失败/回退路径，不假装支持。

DLSS FG与FSR FG不能在保留当前Streamline/NGX会话的同一进程中替换。保存时会阻止该切换，并明确提示“DLSS FG切到FSR FG需要重启；现在重启/稍后”；选择稍后会保留FSR配置、让当前FG保持Off，状态行也会说明需要重启。前台运行已观察到该切换被阻止；`LoMenuFlowTest`和`LoRestartTest`验证了提示与重启状态路径，但本次没有重新启动游戏，也不据此宣称FSR FG启动后的运行效果。

正常退出还曾在FG Off/On运行后于`NGX D3D12 ReleaseFeature`的execute路径发生访问冲突。当前两阶段修复在renderer/GPU drain之后，由`D3D12Bridge::ReleaseFeatureAfterGpuDrain`在FG和SR会话都仍存活时调用经过检查且可重复的FG feature release（`slFreeResources`）；随后执行SR的ReleaseFeature、parameters和Shutdown1，最后才由`g_d3dFg.reset()`执行Streamline shutdown/unload，此时device/queue仍保持有效。这样保留FG资源释放所需的NGX会话；历史共存探针曾显示先关闭NGX会话会使`slFreeResources`返回`FeatureNotFound`。完整Windows Clang游戏构建已在两阶段修复后再次通过，但尚未重新进行运行时退出验证，因此不视为已验收。DLSS FG初始化中途失败、以及新建DLSS bridge后队列／交换链创建失败时的卸载回退路径仍需单独处理。

`LO_FPS=0`仍保持诊断性不限帧，会绕过VRR自动上限。普通验收应清除`LO_FPS`、`LO_GUEST_INTERVAL=0`和非必要FG环境覆盖。游戏帧率不足时不会用修改时钟补偿，显示器低帧率补偿由外部显示系统处理。

## 日志与验证

日志`VRR pacing`记录`requested`、`refresh_hz`、`native_target`、`host_cap`、`fg_multiplier`和`dynamic`。每秒至多报告一次变化；`hardware_vrr=unverified`始终明确真实VRR没有被检测。`native presentation`另记录请求/报告的VSync状态；`frame timing`中的target是有效宿主限帧值，不能当成显示器实际扫描率或插帧后FPS。

本次本地通过：三项独立CPU测试（VRR策略、真实配置解析/写入与preview隔离、FG请求选择）；菜单交互fixture的396项检查，其中包括仅切换VRR On和Off并保存后保留Dynamic MFG的provider、mode、multiplier和target；菜单光栅fixture的21项检查；原生frame-pacer测试。菜单/配置fixture隔离平台和游戏服务，不代表运行了真实游戏。Plume补丁通过固定上游源码的apply检查。

```sh
cmake -S tools/tests/vrr -B out/vrr-tests
cmake --build out/vrr-tests --config Release
ctest --test-dir out/vrr-tests -C Release --output-on-failure
```

Windows Clang `RelWithDebInfo` 的完整 `LostOdysseyRecomp` 目标构建、`LoFramePacerTest` 和 `LoReusableFgCoreTest` 已通过。隔离目录 `out/pr80-runtime-smoke` 使用现有游戏数据和 shader packs 运行 Vulkan 与 D3D12 的 `--prepare-shaders-only`，两者均正常退出，命中 shader pack 并报告 renderer ready；两后端各有一次后台窗口启动片段，使用 `LO_BACKGROUND=1`、VRR On、native target 120、FG Off 和 1280×720，游戏线程运行约20秒且日志无 warning/error。该后台片段的日志显示 `refresh_hz=144`、`host_cap=120`、`requested/reported_vsync=false`、`guest_refresh=60` 和 `clocks=unchanged`；稳定片段约为60 presents/s、`game_time_ratio`约为1，因此不能据此证明120 FPS或完整实景体验。用户在同一场景确认RTSS输出稳定低于144，source约47 FPS、output约141/s；FG Off 前台运行超过20秒无崩溃且DLSS SR继续提交，随后切换到Fixed 2×也运行正常，但之后的正常退出曾触发上述NGX访问冲突，修复后的退出路径尚未重新运行验证。用户确认FG Off时视觉异常消失；Fixed 2×下VRR On/Off的运动物体边缘重影/碎裂程度相近，该画质问题记录为独立的FG画质待验收项，不能归因或宣称为VRR tearing已通过。用户还通过显示器/驱动指示器确认刷新率变化和G-SYNC启用；软件日志中的`hardware_vrr=unverified`仍表示程序没有自行检测硬件状态。上述运行使用隔离的安装/存档副本，未修改D盘安装。仍需覆盖60/90/120档、设置重新打开、动态MFG、跨显示器移动、Alt+Enter、最小化/恢复与暂停设置，并比较等现实时间行走、Aim Ring及音画同步；本提交不宣称AMD或NVIDIA认证或所有目标FPS均已稳定达到。

## 对照接口资料

- [Microsoft：Variable refresh rate displays](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/variable-refresh-rate-displays)
- [Microsoft：DXGI_PRESENT，独占全屏与ALLOW_TEARING限制](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-present)
- [SDL2：SDL_GetCurrentDisplayMode](https://wiki.libsdl.org/SDL2/SDL_GetCurrentDisplayMode)
- [Khronos：VkPresentModeKHR](https://docs.vulkan.org/refpages/latest/refpages/source/VkPresentModeKHR.html)
- [AMD：Enable AMD FreeSync](https://www.amd.com/en/resources/support-articles/faqs/DH3-013.html)
- [NVIDIA：Set up G-SYNC](https://www.nvidia.com/content/Control-Panel-Help/vLatest/en-us/mergedProjects/nvdsp/To_use_variable_refresh_rates.htm)
