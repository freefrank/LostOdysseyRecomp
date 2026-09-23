# FSR / DLSS FG Codex 当前进度

更新时间：2026-09-23。本文只记录当前 Codex 恢复执行的实现与验证状态；不替代历史 handoff，也不表示 Gate 1 已通过。

## 当前状态

用户此前曾要求暂停开发，现已重新授权恢复执行并推进 P2 后处理保护修复。G006 的完整 P0 Gate 仍为 attempt 1/3 NOT PASSED，初审剩余 2 次材料复审；G002 的 P1 已按原始通过条件验收完成；G003 的 P2 仍处于进行中（in progress），P3/P4 尚待实施。Windows 生产构建 build03（EXE SHA256 `8e513eb254bafe27d0373d1706f6afb314d49d2864d2d8afe57e7f56ab139f5d`，源码标识 `eac07c5c3cbf9154a62b490f2e144c0cb91835ea9f55a6ec87477444686788a7`，基于 dirty HEAD `ffc5399`，不可仅用 HEAD 标识）已完成 1280x720 FSR Quality 实景运行验证（`fsr-postprocess-windows-03`），6 个已审计 draw 全部成功 record/publish 且逐像素比对通过。Linux 仅有旧版构建，本次 C++ 改动未在 Linux 验证，不宣称双平台新通过。

`IMP-P0-FIX2` 的探针专用门禁修复已落盘，run03 已使用冻结 EXE（SHA256 `36665711B5258A234DC29AD7266BFD5A26243FD998B285178A516CDBF95EA9D2`）运行。G006 的完整 P0 Gate 仍不通过，Oracle 初审剩余 2 次材料复审；G002 的 FSR SDK、renderer/runtime 接线已实现并提交为 `87a1691`，Windows/Linux 基础运行已验证，P1 已按原始通过条件验收完成；G003 的 P2 历史暂停记录保留，P0 Gate 与 P2–P4 仍未完成。

几何一致性小修 EXE SHA256 为 `C13BDC5E7ED8C73612388D79871770CCD4EB1ACE8232EE050FB74DC235C98398`，已用于 foreground01 和 background04；run03 使用的旧 `366657...` EXE 与证据保持不变。

P0 探针修复范围限制为 `tools/tests/streamline_fg` 与 `cmake/LoStreamline.cmake`；独立 P1 接线涉及生产渲染代码。已有 CPU 4/4 和生产回归证据继续复用，仅重测受新改动影响的项目。

此前用户曾授权持续自主执行，native 工具当时确认 active、OMX 当时为 executing。Linux 依赖安装后，使用当时工作树 25 文件及 source manifest，在 psvita 隔离目录 `p0-cpu-01` 以现有 `CPU_ONLY` CMake 运行 4 项原 CPU 测试，4/4 通过；结果见 [`test-results.log`](../../out/streamline-fg-p0/linux-p0-cpu/test-results.log)、[`test-run.json`](../../out/streamline-fg-p0/linux-p0-cpu/test-run.json) 和 [`source-manifest.json`](../../out/streamline-fg-p0/linux-p0-cpu/source-manifest.json)。这不构成全量游戏构建、GPU 或 FSR 证据。

此前用户曾明确授权自主执行必要的前台运行。foreground01 使用 C13 EXE 完成 48 帧，观察到 33 个 interval 的 `actual_presents=2`；release/shutdown API 成功，但 validation 仍为 46 条错误、exit1。background04 使用同一 C13 EXE，仍观察到 10 条 layout VUID。apidump01 使用 F474 EXE 完成 48 帧、52 条 validation、exit1；[`p0-api-layout-trace.md`](../../out/streamline-fg-p0/p0-api-layout-trace.md) 与 [`p0-api-input-trace.md`](../../out/streamline-fg-p0/p0-api-input-trace.md) 记录了 SDK 内部 WAW 及 pacer 缺失 transition。FG Gate 仍未通过，不能称为修复；当前开发已停止，native goal 为 paused。

## 运行证据

| 运行 | 结果 | 当前解释 |
| --- | --- | --- |
| background-01 | EXE `3b639...`，exit 77，0 frames | 未启用 Vulkan `privateData` feature，触发 VUID；drawable 尺寸不足，不构成 Gate 证据。详见 [`manifest.json`](../../out/streamline-fg-p0/gate1-codex-background-01/manifest.json) 与 [`stdout`](../../out/streamline-fg-p0/gate1-codex-background-01/stdout)。 |
| background-02 | EXE `edba349...`，exit 1，48 frames、2 次 swapchain | 旧 `FeatureNotFound` 未出现；`SLfree=0`、`nativeRelease=1`、`nativeShutdown1=1`、`SLShutdown=0`。但所有 `actual_presents=1`，没有 active FG 退出证明；10 条 `VUID-vkCmdDraw-None-09600` 触及 SL fake swapchain buffer / pacer 期待 `TRANSFER_SRC` 而实际为 PRESENT。详见 [`manifest.json`](../../out/streamline-fg-p0/gate1-codex-background-02/manifest.json) 与 [`stdout`](../../out/streamline-fg-p0/gate1-codex-background-02/stdout)。 |
| background-03 | 同版 EXE `366657...`，exit 1，48 frames | validation 同类 10 条 VUID；swapchain epoch1 的三个输出 handles `ed00000000ed`、`f000000000f0`、`f300000000f3` 与观察到的 VUID 对应，证明为宿主代理图像而非四个输入 tag 的 image，但不证明具体 SDK 根因。每轮 SDK aggregate log 已保存；仍无 active FG 或 Gate 通过证据。详见 [`manifest.json`](../../out/streamline-fg-p0/gate1-codex-background-03/manifest.json) 与 [`stdout`](../../out/streamline-fg-p0/gate1-codex-background-03/stdout)。 |

对应 run02 的 PresentMon 输出有 44 行，均匹配目标 application PID；聚合日志仍记录 SDK 的 “window not focused” 提示。NVIDIA issue 84 存在相似 VUID 先例，但不能据此认定本项目根因。

## 工具与缺口

官方 PresentMon 2.6.0 ETW 启动已通过，便携 Khronos 1.4.328 已在实际运行中启用；当前不应再写成“没有 validation layer”。仍缺少物理显示侧生成帧证据和 Steam Deck 证据。`psvita` 的 Bazzite 44 / ONEXPLAYER APEX Linux 测试属于用户授权测试，不能写成 Deck 验证。用户已授权必要前台运行；物理显示采集仍是独立缺口。

早期 SDK 阶段记录（后续状态见下文）：FSR SDK 双平台 static build、backend-dispatch 符号 link 和 CPU 3.1.4 check 已 exit0，证据见 [`fsr-sdk-build-results.json`](../../out/streamline-fg-p0/fsr-sdk-build-results.json)；当时 G002 的 renderer/runtime 接线仍在进行，尚无 FSR GPU 运行验收。生产 DLAA/Quality 背景回归已完成单场景证据，详见 [`RESULTS.md`](../../out/streamline-fg-p0/production-facade-regression/RESULTS.md)；不代表全游戏画质性能或 FG 验收。

P1 早期运行检查点（后续结案见文末）：Windows 当时生产构建完整哈希见 [`fsr-p1-integration-results.json`](../../out/streamline-fg-p0/fsr-p1-integration-results.json)，独立 adapter run04/run05-gap 全流程 exit0、0 warnings、0 errors，run05-gap 证明真实 `renderFrame 2→4` 会强制 SDK reset；真实 renderer failed-token-gpu-01 exit0、validation 0。两处实际缺陷已修复：SDK KHR 空 proc 使用 promoted core 等价回退；GLSL luma RGBA8 与 SDK RGBA16F 错配，生成期 overlay 仅重生 4 family，SDK cache 未改。quality02 已提交真实 FSR（`1706x960→2560x1440`），但 baseline classes/extent validation 共 144 条、exit0 且 baseline unchanged；quality03-normal 66 sampled non-reset median `16.7599 ms`，静止与短移动截图 exit0；NativeAA01 49 non-reset median `16.6743 ms`、`2560x1440→same`、截图 exit0，screenshot fix 已在 NativeAA 场景核实。fallbackgpu02 `6b00c780...` 通过实际 FSR record → 注入 post-record reject → renderer current green 4096 pixels（保留 alpha）→ 下一次 actual blue SDK reset，明确仅为 fallback/reset 范围证据，不是 SDK internal fault 或 full-facade 通过。上述结果仍不足以宣称 P1 完成；完整 P1 仍需 nonzero MV、depth translation 和 yaw 证据。

Linux Distrobox crash core 证明 `EffectContext` `alignas(32)` 实际 `mod32=24`；生成 backend alignment 修复后的全游戏 build 为 `76513e...`，main harness run02 exit0，5 项 readback 与 reset/resize 通过，但旧 run02 的 AMD coherent-memory `VUID-02790` 已由后续 Linux adapter run03 修复并以 0 errors/0 warnings 通过。Linux run03 已 clean，但这仍不是 Steam Deck 或完整 Linux 游戏画质验收；当前 Linux game 的完整实景结果仍待补充。

Linux 已将当前源码与 PPC 隔离传输到 psvita，使用 `FSR=ON` / `REQUIRE=ON` 完成完整 configure；证据见 [`linux-full-preparation.json`](../../out/streamline-fg-p0/linux-full-prepare/linux-full-preparation.json) 与 [`configure.log`](../../out/streamline-fg-p0/linux-full-prepare/configure.log)。Linux run03 clean 只证明 adapter harness 边界，不能据此宣称完整 Linux 游戏画质或 Steam Deck 验收；FG validation 与物理显示采集缺口仍在。

## 下一步门禁

- [ ] 继续分析 foreground01、background04 与 apidump01 的 layout/API trace，解释 46 条 validation、10 条 layout VUID、SDK WAW 及 pacer transition 缺口；`actual_presents=2` 与 API cleanup 成功仍不足以关闭 P0。
- [ ] 解释或隔离 run02 的 fake swapchain / `TRANSFER_SRC` VUID，并补足 active FG 退出证据。
- [ ] 取得物理显示证据后再准备 Gate 1 材料复审；不因 48 frames 或旧错误消失而关闭 P0。
- [x] G002 的 P1 接线与运行结论已验收完成；G003 的 P2 传播诊断仍未完成，P3/P4 仍待实施和验收。

## 调度与设计证据

原始 brief 明确先做 FSR，并行推进 FG；此前“严格串行、P0 未过则不得做 FSR”的说法来自 OpenCode deepwork 旧文件，不是用户技术依赖。当前 G001 已被 superseded（不表示通过）；G006 逐字保留完整 P0 objective、attempt 1/3 NOT PASSED、剩余 2 次复审，以及 validation/display/image/performance 的完整要求。G002 的 P1 已完成 checkpoint，G003 的 P2 仍未完成，G003/G006/G004/G005 待验收；优先顺序只是排程，不新增依赖。用户已停止本轮，native goal 为 paused。

设计记录：[stage-dependency-audit.md](../../out/streamline-fg-p0/stage-dependency-audit.md)、[fsr-p1-input-contract.md](../../out/streamline-fg-p0/fsr-p1-input-contract.md)、[fsr-p2-mask-design.md](../../out/streamline-fg-p0/fsr-p2-mask-design.md)。P2 新 architect 计划还包括 alpha replay 与后处理传播；这些文档记录设计和证据边界，不表示 FSR/P2 已验收。

P2 mask 设计记录：[fsr-p2-mask-design.md](../../out/streamline-fg-p0/fsr-p2-mask-design.md)。该记录仅描述同帧 alpha/mask/final-color 对齐及覆盖缺口，不表示 P2 已启动或验收。

## Linux adapter 后续验证

AMD coherent-memory selector 修复后，Linux harness run03 exit0，同步校验 0 errors / 0 warnings，5 次 RGBA 读回、frame gap reset、discard/recreate 与 resize 均通过。证据见 out/streamline-fg-p0/linux-full-prepare/fsr-adapter-linux-run-03-result.json。完整游戏已链接相同 backend，Linux 实景仍在验证；不替代 Steam Deck 和 P2 画质验收。

P2 详细方案与新增 shader 审计已保存到 out/streamline-fg-p0/fsr-p2-mask-implementation-plan.md。真实 alpha 分类证据覆盖捕获的 38/48 个透明 draw；draw176 是 RG/BA 扰动向量累积，不能作为 opacity。方案尚未实施。

Linux 实景最新证据：Quality `30f3e390...` 已在 frame12356 提交 `853x480→1280x720`，41 条采样记录无 reset、exit0。旧截图红蓝交换在 Off 同场景也重现，已定位为 BGRA 交换链截图被按 RGBA 保存；修复不改变游戏呈现。新 Linux `034c22b4...` Native AA 在 frame10314 提交 `1280x720→same`，65 条无 reset 采样的中位帧间隔为 17.107218 ms，`linux-game-fsr-native-aa-01/shot_14399.png` 确认正确颜色，exit0。该帧间隔不是 GPU A/B 收益或全场景性能结论。

真实 FSR renderer fixture `fsr-fallback-gpu-02` 已通过：成功红色帧之后，在真实 SDK record 后注入拒绝，4096 个最终像素保留当前绿色和 alpha，token 被丢弃，恢复蓝色帧强制 SDK history reset。该证据不覆盖 SDK 内部失败或完整生产 facade。P1 仍待受控平移／旋转的同帧非零 MV/depth/jitter 独立比较；P2、Steam Deck 和 FG gate 保持未完成。

## 2026-09-23 运动输入捕获检查点

FSR 接入及已验证修复已本地提交为 `87a1691`，未推送或发布。新增诊断捕获构建在 Windows（`49cfd828…`）和 Linux（`357948f6…`）通过，见 [构建记录](../../out/streamline-fg-p0/fsr-capture-production-result.json)。

Windows `fsr-motion-windows-01` 在隔离状态运行并正常退出，存档／配置基线未变。真实 FSR 三帧均有 SDK 成功、checked submit 和 GPU completion 记录。首帧 12000 的人工静态地面 ROI 为 `[500,270,690,390)`，输入尺寸 `853×480`；1,440 个样本的独立深度回投 MV 误差中位数 `0.00049934 px`、P95 `0.00090686 px`，预期位移中位数 `1.28044 px`，见 [单帧结果](../../out/streamline-fg-p0/fsr-motion-windows-01/single-frame-preliminary.json)。这只证明该静态 ROI 的同帧几何合同，未捕获前一帧 raw depth，不能证明完整遮挡或连续 FSR history。

完整渲染捕获使后两帧间隔达到约 `677/603 ms`，正常重置逻辑随之变更 temporal epoch、清空 MV，并将 invalidity 置为无效。这两帧不能用于相邻运动验收。轻量 FSR-only 三帧捕获和可控右摇杆输入正在补充；尚无其实际运行结果。P1、P2、Steam Deck 与 FG Gate 保持未完成。

### 轻量连续帧试验

`LO_FSR_CAPTURE_REQUEST` 的轻量路径已在 Windows 实景通过：三帧仅保留五种 raw 输入／输出和元数据，第三帧提交完成后统一导出；不启动完整 F1 的逐 draw/resolve 捕获。测试构建 Windows SHA256 为 `784f2f4783e2e0e5ebc97ae940ffa23eece93b241b9998b7373080e5da7f0878`，Linux 同步构建为 `000a6d4c…`，见 [构建记录](../../out/streamline-fg-p0/fsr-lite-capture-production-result.json)。Linux 该诊断路径未实景重跑，已有原生 FSR 运行结果仍复用。

Windows 同一隔离进程完成左摇杆相机跟随移动和右摇杆旋转。各试验均为三个连续帧、同一 temporal epoch、无 SDK/input reset，五张 raw 均确认提交并完成。移动两对帧各有 1,440 个深度一致静态地面样本，P95 误差分别 `0.000919/0.003527 px`；旋转两对各有 900 个样本，P95 为 `0.000423/0.000494 px`。旋转时相机中心变化约 `0.00013` guest 单位，朝向变化约 `0.059/0.048°`，FOV 基本不变；控制脉冲和实际相机量共同证明旋转，未把人工标签当证据。X 方向共 4,680、Y 方向共 1,440 个显著分量样本，方向一致率均为 100%；旋转试验自身没有显著 Y 分量，该项由移动第二对覆盖。

该结果通过试验前固定的静态几何判据（每对深度一致样本至少 100、预期运动中位数至少 `0.5 px`、误差中位数至多 `0.05 px`、P95 至多 `0.1 px`）。见 [结果与范围](../../out/streamline-fg-p0/fsr-motion-windows-02-lite/result.json) 和 [预设判据](../../out/streamline-fg-p0/fsr-motion-acceptance-plan.json)。游戏 exit0，存档／配置基线未变。这补齐 Windows P1 的有界运动输入证据，不代表 P2 画质、性能、动态物体、Steam Deck 或 FG 验收。

复现时在 FSR 启用的隔离进程设置 `LO_FSR_CAPTURE_REQUEST` 为请求文件的绝对路径，写入新的非零整数触发三帧；产物位于 `captures/fsr-motion-*/frame-*/`。运动测试文件保留原 5/7 字段，并支持 `serial hexButtonMask leftX leftY polls LT RT rightX rightY`；右轴限幅、取消和过期释放的实际 HID fixture 已通过。离线执行 `python tools/tests/fsr/compare_captured_motion.py <capture-directory> --motion translation --roi x0,y0,x1,y1 --step 4 --output result.json`，旋转则使用 `--motion yaw`。ROI 使用输入分辨率坐标，必须人工选静态几何。比较器输出诊断而不自动宣称验收，退出码 0 只表示成功生成结果；缺失方向、历史重置或覆盖不足必须单独判断。

## P1 阶段结案，转入 P2

独立复核完成后，G002 已通过受支持的 OMX checkpoint 标记 complete，G003 已进入执行；native aggregate goal 保持 active。验收对照见 [P1 audit](../../out/streamline-fg-p0/fsr-p1-acceptance-audit.md)。新增 Windows Balanced 为 `752×423 → 1280×720`，Performance 为 `640×360 → 1280×720`；两档均有真实 SDK 提交、连续三帧无 reset 的完整读回、正常最终场景／HUD、exit0 和未变基线，见 [Balanced](../../out/streamline-fg-p0/fsr-game-windows-balanced-01/result.json) 与 [Performance](../../out/streamline-fg-p0/fsr-game-windows-performance-01/result.json)。

原生 Linux RADV Performance 也通过相同 `640×360 → 1280×720` 提交与读回检查，三个实际帧 `8345–8347` 无 reset，正常退出且隔离配置、profile、存档 17 个文件不变，见 [Linux Performance](../../out/streamline-fg-p0/linux-game-fsr-performance-02/result.json)。首次较晚发送输入未能进入存档，随后进入待机影片，未计为 FSR 失败；重试只把 START 提前到约 28.5 秒，保持 12 tick 与同一 EXE，成功读档。

P1 的结案仅覆盖可运行路径及其有界输入／回退合同；P2 的透明遮罩、完整画质／性能场景和 Steam Deck，保留的 P0 Gate、P3/P4 FG 仍待完成。已有 clean adapter/fixture validation 不代表整个游戏 validation 清零。SDK 内部错误注入、动态物体画质与实际物理 FG 显示也未据此宣称通过。P2 原始 alpha 收集的后续检查点见下节；完整 P2 仍未验收。


## P2 原始材质 alpha 收集检查点

显式 `LO_FSR_ALPHA_REPLAY=1` 启用六组已审计 VS/PS 的原 alpha replay，保留原 PS 的 discard，以独立 R8 MAX 累积并只读原深度。每个 GPU 批次持有共享 lease，首 clear 与后续 LOAD、跨 Flush 累积具有显式依赖；实际深度分配与场景 anchor 决定归属，不固定捕获时的 eDRAM 地址。此时仅为 `PartialCoverage` 原始收集，未传播到最终场景，也未绑定 SDK reactive／T&C。

Windows 与原生 Linux 已完成首轮增量构建，见 [构建记录](../../out/streamline-fg-p0/fsr-p2-alpha-build-result.json)。Windows `d567693c…` 在隔离城镇实景的 frame12000 记录 14 次已审计 draw，实际分配为 `853×491`，raw mask 有 130 个非零像素。draw1979 的原 `R16G16B16A16_FLOAT` 颜色 3,350,584 字节、D32 的 R32 depth plane 1,675,292 字节在同一次 replay 前后完全一致，独立哈希与逐像素比较均吻合；读回所属 serial24006 已完成后才导出。进程 exit0，原 settings／save／profile 未变。见 [实景结果](../../out/streamline-fg-p0/fsr-alpha-windows-01/result.json)。该检查不覆盖 stencil，不表示完整透明覆盖或画质／性能收益。

诊断可将 `LO_FSR_ALPHA_CAPTURE_FRAME`、`LO_FSR_ALPHA_COMPARE_FRAME` 设为同一个预定 renderer 帧，并设置绝对 `LO_FSR_ALPHA_CAPTURE_DIR`。正常未启用路径不分配这些读回。只读深度格式暂限 D32_FLOAT_S8_UINT；不支持格式明确输出状态，不能据此当作相等。首轮捕获的有效证据与后续 long-frame epoch 顺序修复分开记录，最终提交前完成该窄修与受影响构建。


### 原生 Linux alpha 实景与首切片提交

首切片已提交为 `c8975c2`。长帧 epoch 更新顺序已修正，168 项时钟检查和独立源码复核通过；随后双平台增量构建通过，见[最终构建记录](../../out/streamline-fg-p0/fsr-p2-alpha-final-build-result.json)。Windows 已取得的同 draw 证据不受无 hitch 路径未变的窄修影响，未重复运行。

psvita 的原生 Linux RADV 使用该最终构建 `8e8b2bd5…` 完成相同诊断：frame12000、serial24010、14 次 audited draw，`853×491` raw R8 有 148 个非零像素；draw1994 的颜色 3,350,584 字节和 R32 depth plane 1,675,292 字节前后完全相同。两个 readback 在 fence 完成后导出，画面、HUD 与颜色正常，进程 exit0，17 个原配置／profile／存档文件哈希未变。证据见[Linux alpha 结果](../../out/streamline-fg-p0/fsr-alpha-linux-02/result.json)。首轮代理导航延迟错过固定采集帧，保留为未取得证据；同一 EXE 改用定时菜单输入后重试成功，没有把首轮记为 SDK 失败。

这仍是 `PartialCoverage` 原始材质 alpha，不包含 resolve／fetch／后处理传播、SDK mask、stencil 不变、性能收益或 Steam Deck 验收。psvita 当前为 ONEXPLAYER APEX / Radeon 8060S。下一片版本桥接正在开发，未据此宣称通过。


### P2 resolve／fetch 版本桥接检查点

`LO_FSR_ALPHA_REPLAY=1` 与 `LO_FSR_ALPHA_BRIDGE=1` 同时启用有界桥接。R8 在实际颜色 resolve 点复制，按同帧／epoch、源与目标分配、写入序号和有效矩形关联；fetch 必须匹配最终 PS 绑定图像。未知 RGB 覆盖、clear／transfer 会使后续原始来源失效，已复制的独立快照仍可使用。已审计的局部混合只保留此前 alpha 贡献的保守上界，不增加透明覆盖声明。

CPU owner／policy 检查和 Windows／Linux 增量构建通过，见 [CPU 记录](../../out/fsr-alpha-p2-cpu/result.json) 与 [构建记录](../../out/streamline-fg-p0/fsr-p2-bridge-build-result.json)。Windows 实景构建 `e4e277da…` 在 frame12000／serial24009 产生写入序号 205034 的 `853×491 → 853×480` R8 copy，独立逐字节比较 409,440 个复制像素零差异，其中 150 个非零。随后 `copy_reused` 获得新序号 205035，downsample 与 tonemap 的实际 slot0 fetch 均匹配该版本和最终绑定图像。复用关系由代码与真实 trace 联合证明，没有声称执行第二次 GPU copy。

后续 draw2084 的未知 RGB 写入确实触发来源失效；先前独立复制的版本仍可供 fetch。六次后处理 fetch 的 guest 448→428 裁剪（本次实际物理尺寸 299→285）因尚未实现的后处理明确返回 `Unavailable`。见 [实景结果](../../out/streamline-fg-p0/fsr-alpha-bridge-windows-01/result.json)、[独立检查](../../out/streamline-fg-p0/fsr-alpha-bridge-windows-01/bridge-independent-check.json) 及目录内原始 JSONL／R8。GPU fence 完成后导出，进程 exit0，原配置／存档基线未变。独立复核支持本片提交，未重复既有测试。

本片没有正例裁剪、实际最终图像替换或游戏内 Off／epoch 切换证据；后两者仅有对应代码／CPU 边界检查。Linux 本片仅构建，首 alpha 实景结果继续复用。后处理 mask、SDK reactive／T&C、完整透明覆盖、画质／性能及 Steam Deck 仍待完成；该历史检查点当时未推送或发布，最新交付状态以停止交接及 Git 记录为准。


### P2 战斗／粒子场景入口

使用已保留的 bridge 构建 `e4e277da…`，从历史 Hypocenter 存档／profile 的隔离副本进入当前游戏，实际完成取得 Bruiser Ring、跳过可选教程、遇到 Insane Khent Soldier、选择 Attack／目标和 RT 输入。`shot_31698.ppm` 显示实际 Aim Ring 与 87 伤害；初始 Hypocenter 可见烟雾、火星和紫色发光。进程按请求正常退出 0，原安装状态及所选历史存档／profile 哈希均未变。见 [入口结果](../../out/streamline-fg-p0/fsr-battle-route-01/result.json) 和 [实际输入时间线](../../out/streamline-fg-p0/fsr-battle-route-01/route-replay-timeline.json)。

这补充了当前构建的场景入口，不证明画质 A/B、Good／Perfect 输入时机、随机遇敌稳定性或后处理 mask。旧 poll 脚本未直接作为成功依据；本次按实际画面调整，以 tick 记录输入。后续可在此场景比较战斗 UI、细环、粒子与遮挡；完整 P2 仍未验收。


### P2 后处理传播与可选 GPU 计时检查点

在显式 alpha replay／bridge 路径中加入四组已审计后处理 VS／PS 的 R8 传播，覆盖九采样 downsample、带 c10 限制的九采样、十六采样 bloom，以及读取实际深度分支的 tonemap。传播保留每个正权重采样足迹中的最大值，是已收集贡献的保守上界；仅发布经过几何／scissor 证明且未开启 cull 的有效矩形。实际 host bloom prefilter 使用面积重叠的最大值，并核对真正的输入与最终绑定输出图像。未知写入、无法匹配的 HDR／temporal 替换或覆盖范围仍明确不可用，未绑定 SDK reactive／T&C。

新增诊断按 fence 完成导出输入／输出 mask、实际常量与顶点、原颜色／深度快照；记录捕获失败并以事件序号区分文件，文件行距与 GPU readback 行距分开。独立静态审阅通过，CPU owner／shader fixture、四组原 VS wrapper 及 host area shader 的 SPIR-V 编译通过。Windows 与 psvita Distrobox 原生 Linux 生产增量构建通过，Windows SHA256 为 `eb84f8b4…`，Linux 为 `7bdf80fa…`。见 [静态审阅](../../out/streamline-fg-p0/fsr-p2-postprocess-static-review.json)、[CPU／着色器记录](../../out/fsr-postprocess-cpu/result.json) 和 [双平台构建](../../out/streamline-fg-p0/fsr-p2-postprocess-build-result.json)。已完成的 Windows 实景 `f12000` 中，三种 blur 对应五个 draw 均为 `quad_unavailable`，tone draw 2085 为 `sampler_unavailable`，没有 record/publish，final scene 为 unsupported；因此以上不构成完整 P2 验收。

`LO_FSR_GPU_TIMING=1` 可记录 prepare／SDK／encode／copy／同步范围的 GPU timestamp；查询只在所属提交 fence 完成后读取，未就绪明确为 unavailable。默认关闭，不分配查询池或录制计时命令。这是隔离 SR 段诊断，不是 SDK 单独耗时或整帧性能；截图、alpha capture、重置及预热帧需由实验记录排除，不能据此直接宣称性能收益。

此前开发曾按用户指令暂停，历史停止交接见 [Codex 停止交接](fsr-dlss-fg-codex-handoff.zh-CN.md)。

### P2 后处理保护修复与实景验证检查点 (windows-03)

用户已授权恢复开发并推进 P2 后处理保护修复。

#### 修复与机制实现

在 `CheckPostprocessQuadCoverage` 中引入支持小数 viewport 的像素覆盖判定与精确矩形角点检查，避免 SDR 宽松 epsilon 导致对角线像素缝隙。resolve 阶段通过 `IntersectCopyValidRect` 计算 validRect 交集，右侧和底部 padding 明确保留为无效。

接入真实 `depth_color_tile_clear` 追踪全量清屏事件，当检测到合规的内缩（inset）几何时以 clear 背景补齐未覆盖边框，并在未知 RGB 写入或不支持后处理时使该 clear 失效。首次 clear 允许保留后续 raw 收集；若该 clear 替换了已有源，则保留此前写入者拒绝记录，旧 raw 禁用策略维持不变。

细化区分输入纹理缺失（`input_unavailable`）与采样器不支持（`sampler_unavailable`）。在 `FsrAlphaBridgeTraceEnabled` 下输出包含 `draw_written_rect`、`inset_geometry_ok`、`clear_background_available`、`geometry_supported` 等详尽诊断字段。

Python 离线参考工具修正了 Vulkan Y 轴方向，以及项目 HLSL 中 `FLT_MIN` 实际为 `-FLT_MAX` 的负极值下界逻辑（恢复 scene 采样分支激活权重）；新增 6 项 clear 背景合成独立检查用例与单像素 tonemap 回归测试。

#### 定向验证结果

- CPU 与 Python 测试：CPU 单元测试 `LoNativeDlssP2RoutingTest` 与 `LoFsrAlphaPropagationPolicyTest` 全部通过，独立 oracle 阻塞均已关闭。Python 脚本 `test_postprocess_clear_background.py`（6 项 clear 背景用例）与 `test_postprocess_tonemap_flt_min.py`（单像素 tonemap 回归）全部通过。
- Windows build03 生产构建：可执行文件 SHA256 为 `8e513eb254bafe27d0373d1706f6afb314d49d2864d2d8afe57e7f56ab139f5d`，源码标识为 `eac07c5c3cbf9154a62b490f2e144c0cb91835ea9f55a6ec87477444686788a7`，基于 dirty HEAD `ffc5399`（禁止单独使用 HEAD 标识）。
- Windows 实景运行（`out/streamline-fg-p0/fsr-postprocess-windows-03`）：在 RTX 5080 Vulkan、1280x720 FSR Quality（输入 853x480）、frame 12000 条件下运行，进程 exit 0 且配置与存档基线未变（`baseline_unchanged`）。6 个已审计 draw（2091、2094、2097、2100、2103、2104）全部成功 record 并 publish。
- 逐像素比对结果：5 组 blur draw（2091、2094、2097、2100、2103）各包含 45,440 个 interior 像素与 445 个 clearborder 像素，比对 0 mismatch（非零像素依次为 33、124、277、385、2322）。tone draw（2104）包含 409,440 个 interior 像素，修正参考端 `FLT_MIN` 误解后比对 0 mismatch（非零像素 22,672）。原报告单像素失败记录原样保留，修正后报告见 `alpha-capture/postprocess-independent-check-corrected.json`（exit 0，`bounded_interior_checks_passed`，同目录保存 provenance）。6 个 draw 的原始颜色字节比对均为 0 差异。
- Oracle 独立传输链：6 次 blur 传输（每次 45,885 字节）、2 次 raw 传输（每次 409,440 字节）与 tone 至 final 传输（409,440 字节）全量比对 0 差异，stage 4 rev 2104 ordinal 205234 submission 24009 完成；285 valid、288 resolve、299 parent 裁剪正确，padding 未认证为有效。

#### 运行条件与保留限制

- 运行条件澄清：此前 02 运行采用 1440p 设置（与 01 的 720p 不一致），但失败实际是由于 inset quad 缺少 clear 背景证明，并非 1440p 导致失败；03 运行恢复了 720p 分辨率条件，但同时修改了 C++ 传播实现，不构成单一变量对比。01、02 及旧 baseline 证据全部保留。
- 门禁与阶段状态：P0 Gate 1 维持 attempt 1/3 NOT PASSED，初审剩余 2 次材料复审；P1 已完成，P2 仍在进行中（in progress）。
- 未验证范围：未捕获 originaldepth 成对读回，不宣称深度不变；光栅化边缘规则未认证（`edge_check_status: not_covered`）；host prefilter 未覆盖（仅 host scene-only 检查通过）；SDK 遮罩绑定、完整 P2 画质及性能未验收；Linux 平台当前仅有旧版本构建，本次 C++ 修改未在 Linux 实机验证，不宣称双平台新通过。
