# 原生90/120 FPS：实现与本地验收

> **专题参考。** 下文实现和测量只适用于各自注明的版本与范围；现行交付及验收状态见[项目状态](../STATUS.md)和[路线图](../ROADMAP.md)。

本分支沿用已经实现的30→60 FPS路径，增加90 FPS并将旧120 FPS隐藏开关接入正常设置。调整的是游戏提交真实帧的目标频率，与DLSS/FSR Frame Generation的生成帧倍数独立。默认仍为30 FPS，高帧率选项保留实验性标记。

## 时序与呈现

以下表格描述VRR Off时的原有路径；新增独立VRR开关、显示刷新率限帧和FG预算见[FreeSync / G-SYNC Compatible接入](vrr-freesync-gsync-compatible.md)。

| 原生目标 | 已识别游戏Present调用的interval | 宿主目标周期 | 宿主VSync策略 |
|---|---|---|---|
| 30 FPS | 保留2 | 33.333 ms | 保留该交换链原有策略 |
| 60 FPS | 2→0（immediate） | 16.667 ms | 保留该交换链原有策略 |
| 90 FPS | 2→0（immediate） | 11.111 ms | 请求关闭，由宿主deadline限帧 |
| 120 FPS | 2→0（immediate） | 8.333 ms | 请求关闭，由宿主deadline限帧 |

2026-10-10 起 60 FPS 档也改为 immediate。原来的 interval 1 下，单帧超过 16.667 ms 就要等下一个虚拟 VBlank，帧率直接掉到 30 FPS（Steam Deck 玩家看到帧率和 GPU 占用都只剩一半）。现在由宿主 deadline 限帧，慢帧只损失超出的那部分时间；宿主 VSync 策略不变，不会因此撕裂。30 FPS 档仍保留原版 interval 2。

同日新增 Graphics → VSync（关／开／FreeSync / G-SYNC Compatible，`vsync=0/1` 加原有的 `variable_refresh_rate`），上表的宿主 VSync 一列改为跟随该设置：开时每个原生档位都保留交换链原有的同步策略（包括 90/120），关时一律 immediate，30 FPS 的 guest interval 也改为 immediate。没有 `vsync` 键的旧配置按原行为迁移：`frame_rate` 不高于 60 为开，90/120 为关。macOS 的 Metal 始终同步，菜单只提供 ProMotion 开关。

间隔修改仍严格限定在调用者`0x827B4A4C`、原始interval=2的路径；其他调用者、其他interval及标志位不变，原函数仍只转发一次。游戏时间增量、PPC timebase、内核计时、音频时钟均不缩放，虚拟VBlank仍为60 Hz。原有deadline算法保持不变；慢帧不额外等待一个完整周期，长暂停后不集中补发过期帧。

新增`gpu/frame_rate.h`作为配置验证、菜单排序、宿主/guest呈现策略的共同定义。`settings.ini`保存实际帧率值，不保存菜单索引；旧`frame_rate=120`无需迁移，非法值仍回到30。

宿主VSync设置在共同的`PreparePresentation`入口、acquire之前应用，覆盖真实游戏帧和暂停后的host overlay。Vulkan模式变更沿用现有交换链重建流程：先处理FG资源交接、等待在途提交，再重建图像。降回30/60恢复该交换链的原始VSync策略；Vulkan DLSS-G自身要求immediate时继续保留其要求。交换链被替换后重新读取基线。

若平台无法提供immediate，记录警告并接受平台回退，避免反复请求同一失败模式导致逐帧重建。关闭VSync可能出现撕裂；实际显示还受显示器、窗口系统、驱动及VRR设置影响。本改动不保证任意机器或场景稳定达到目标频率，也不把FG生成帧算作新增原生游戏帧。

## 使用

在Graphics→Frame rate选择90 FPS或120 FPS并保存，使用现有设置应用流程，无需为帧率变更重启。`LO_EXPERIMENTAL_120`不再参与判断；即使环境中保留它也不影响新设置。

验收原生高刷时先将Frame generation设为Off，超分/抗锯齿可维持原设置。检查启动脚本没有`LO_FPS`覆盖：它仍优先于设置，`0`为诊断性不限帧，并解除已识别调用点的guest间隔。`LO_GUEST_INTERVAL=0`仍是A/B诊断开关，会关闭guest间隔转换，不应在普通高刷验收中使用。

## 已做检查与证据边界

已用GCC C++20、`-Wall -Wextra -Werror`编译并运行`tools/tests/frame_pacer_test.cpp`。覆盖四档配置/菜单映射、每一对档位切换、超负载、长暂停恢复、不限帧切换、宿主VSync基线/FG策略及调用点/标志保留。该测试仅依赖生产头文件，不启动游戏或GPU。

同步更新菜单渲染样本、菜单fixture的头文件依赖，并在现有Windows配置fixture中增加四档SaveConfig→INI读回断言。这些Windows配置与菜单fixture在本次环境中未运行。尚未完成全游戏编译、Windows/Vulkan/D3D12实机呈现或玩法速度验收，不将源代码与纯CPU测试视为已验证稳定90/120 FPS。

可复现的轻量检查：

```sh
g++ -std=c++20 -Wall -Wextra -Werror -ILostOdysseyRecomp \
    tools/tests/frame_pacer_test.cpp -o /tmp/lo-frame-pacer-test
/tmp/lo-frame-pacer-test
```

已有构建环境也可使用`LoFramePacerTest`目标；Windows的`LoSaveAnywhereConfigTest`须在隔离的空工作目录执行，避免接触真实设置。

## 本地游戏验收

选用已有60 FPS正常的同一存档和场景，保持画质/分辨率不变，依次60→90→120→60→30。每档保存后确认重新打开菜单和重新启动程序都保留选项；另外覆盖暂停中保存、窗口缩放/最小化恢复及从120降档。D3D12和Vulkan应分别验证。

用相同现实时间比较行走距离、动画/特效持续时间、战斗与Aim Ring判定窗口、音频和过场同步。过场视频不应被要求产生超过源视频帧率的新内容。先完成FG Off的原生检查，再单独检查FG组合。

启动时设置`LO_FRAME_TIMING=1`可读取既有一秒窗口，新增`source=guest_swap`、`engine_rate`与`game_time_ratio`。`rate`是该窗口的游戏swap处理频率，`engine_rate`为engine tick频率，`game_time_ratio=delta_sum/window`在稳定、未暂停的普通游戏中应接近1；切换、加载、暂停的短窗口不能单独用于判断倍速。`LO_RENDER_TIMING`启用时会保留原有逐帧记录并抑制此汇总。

日志可帮助分辨原生处理频率与FG总显示帧率，但swap/tick计数本身不能证明每个图像都独特，也不能替代玩法比较。若出现原生rate仍约60、游戏加速/减速、Ring窗口异常或同步错误，应保留该场景日志继续定位，而非调慢全局时钟补偿。

## 已知的游戏逻辑高帧率问题

游戏按30 FPS编写，个别逻辑以帧为单位或假定帧时间下限，原生90/120下需要单独修复：

- Hungry Man差事计时按帧计数：`patches/hungry_man_timer.cpp`按30 FPS等效更新计数。
- 战斗相机旋转平滑器按12.5 ms步长重采样历史，要求每帧至少12.5 ms：`patches/battle_camera_smoother.cpp`只在累计满一步时更新它（#117，[详情](battle-camera-120fps-2026-10-01.md)）。
- 战斗台词等待曾把每次实际脚本更新的 60 Hz 步长截断并丢弃小数，导致原生90/120 FPS下等待可能没有进展：`patches/battle_script_timer.cpp`在更新之间保留 fractional tick，再在等待 consumer 前提供累计整数步长。原始 PPC fixture 与关闭修正的负对照均通过；完整运行时构建、真实场景运行和玩家验收仍待完成（#148，[详情](issue-148-battle-dialogue-timing.md)）。

排查同类问题时，先按帧记录相关对象的状态，再用硬件写断点找到写入者，并在该函数中查找固定步长或帧计数的假设。
