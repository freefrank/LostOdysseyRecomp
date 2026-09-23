# FSR / DLSS FG Codex 当前进度

更新时间：2026-09-23。本文只记录当前 Codex 恢复执行的实现与验证状态；不替代历史 handoff，也不表示 Gate 1 已通过。

## 当前状态

`IMP-P0-FIX2` 的探针专用门禁修复已落盘，run03 已使用冻结 EXE（SHA256 `36665711B5258A234DC29AD7266BFD5A26243FD998B285178A516CDBF95EA9D2`）运行。G006 的完整 P0 Gate 仍不通过，Oracle 初审剩余 2 次材料复审；G002 的 P1 SDK 已构建，renderer/runtime 接线进行中，P2–P4 仍待验收，主 ultragoal 仍覆盖至 P4。

几何一致性小修 EXE SHA256 为 `C13BDC5E7ED8C73612388D79871770CCD4EB1ACE8232EE050FB74DC235C98398`，已用于 foreground01 和 background04；run03 使用的旧 `366657...` EXE 与证据保持不变。

P0 探针修复范围限制为 `tools/tests/streamline_fg` 与 `cmake/LoStreamline.cmake`；独立 P1 接线涉及生产渲染代码。已有 CPU 4/4 和生产回归证据继续复用，仅重测受新改动影响的项目。

用户已授权持续自主执行，native 工具现已确认 active，OMX 当前为 executing。Linux 依赖安装后，使用当时工作树 25 文件及 source manifest，在 psvita 隔离目录 `p0-cpu-01` 以现有 `CPU_ONLY` CMake 运行 4 项原 CPU 测试，4/4 通过；结果见 [`test-results.log`](../../out/streamline-fg-p0/linux-p0-cpu/test-results.log)、[`test-run.json`](../../out/streamline-fg-p0/linux-p0-cpu/test-run.json) 和 [`source-manifest.json`](../../out/streamline-fg-p0/linux-p0-cpu/source-manifest.json)。这不构成全量游戏构建、GPU 或 FSR 证据。

用户最新明确授权自主执行必要的前台运行，不再把前台许可列为等待项。foreground01 使用 C13 EXE 完成 48 帧，观察到 33 个 interval 的 `actual_presents=2`；release/shutdown API 成功，但 validation 仍为 46 条错误、exit1。background04 使用同一 C13 EXE，仍观察到 10 条 layout VUID。apidump01 使用 F474 EXE 完成 48 帧、52 条 validation、exit1；[`p0-api-layout-trace.md`](../../out/streamline-fg-p0/p0-api-layout-trace.md) 与 [`p0-api-input-trace.md`](../../out/streamline-fg-p0/p0-api-input-trace.md) 记录了 SDK 内部 WAW 及 pacer 缺失 transition。FG Gate 仍未通过，不能称为修复。

## 运行证据

| 运行 | 结果 | 当前解释 |
| --- | --- | --- |
| background-01 | EXE `3b639...`，exit 77，0 frames | 未启用 Vulkan `privateData` feature，触发 VUID；drawable 尺寸不足，不构成 Gate 证据。详见 [`manifest.json`](../../out/streamline-fg-p0/gate1-codex-background-01/manifest.json) 与 [`stdout`](../../out/streamline-fg-p0/gate1-codex-background-01/stdout)。 |
| background-02 | EXE `edba349...`，exit 1，48 frames、2 次 swapchain | 旧 `FeatureNotFound` 未出现；`SLfree=0`、`nativeRelease=1`、`nativeShutdown1=1`、`SLShutdown=0`。但所有 `actual_presents=1`，没有 active FG 退出证明；10 条 `VUID-vkCmdDraw-None-09600` 触及 SL fake swapchain buffer / pacer 期待 `TRANSFER_SRC` 而实际为 PRESENT。详见 [`manifest.json`](../../out/streamline-fg-p0/gate1-codex-background-02/manifest.json) 与 [`stdout`](../../out/streamline-fg-p0/gate1-codex-background-02/stdout)。 |
| background-03 | 同版 EXE `366657...`，exit 1，48 frames | validation 同类 10 条 VUID；swapchain epoch1 的三个输出 handles `ed00000000ed`、`f000000000f0`、`f300000000f3` 与观察到的 VUID 对应，证明为宿主代理图像而非四个输入 tag 的 image，但不证明具体 SDK 根因。每轮 SDK aggregate log 已保存；仍无 active FG 或 Gate 通过证据。详见 [`manifest.json`](../../out/streamline-fg-p0/gate1-codex-background-03/manifest.json) 与 [`stdout`](../../out/streamline-fg-p0/gate1-codex-background-03/stdout)。 |

对应 run02 的 PresentMon 输出有 44 行，均匹配目标 application PID；聚合日志仍记录 SDK 的 “window not focused” 提示。NVIDIA issue 84 存在相似 VUID 先例，但不能据此认定本项目根因。

## 工具与缺口

官方 PresentMon 2.6.0 ETW 启动已通过，便携 Khronos 1.4.328 已在实际运行中启用；当前不应再写成“没有 validation layer”。仍缺少物理显示侧生成帧证据和 Steam Deck 证据。`psvita` 的 Bazzite 44 / ONEXPLAYER APEX Linux 测试属于用户授权测试，不能写成 Deck 验证。用户已授权必要前台运行；物理显示采集仍是独立缺口。

FSR SDK 双平台 static build、backend-dispatch 符号 link 和 CPU 3.1.4 check 已 exit0，证据见 [`fsr-sdk-build-results.json`](../../out/streamline-fg-p0/fsr-sdk-build-results.json)；G002 的 renderer/runtime 接线仍在进行，尚无 FSR GPU 运行验收。生产 DLAA/Quality 背景回归已完成单场景证据，详见 [`RESULTS.md`](../../out/streamline-fg-p0/production-facade-regression/RESULTS.md)；不代表全游戏画质性能或 FG 验收。

P1 当前证据：Windows 最终生产构建完整哈希见 [`fsr-p1-integration-results.json`](../../out/streamline-fg-p0/fsr-p1-integration-results.json)，独立 adapter run04/run05-gap 全流程 exit0、0 warnings、0 errors，run05-gap 证明真实 `renderFrame 2→4` 会强制 SDK reset；真实 renderer failed-token-gpu-01 exit0、validation 0。两处实际缺陷已修复：SDK KHR 空 proc 使用 promoted core 等价回退；GLSL luma RGBA8 与 SDK RGBA16F 错配，生成期 overlay 仅重生 4 family，SDK cache 未改。quality02 已提交真实 FSR（`1706x960→2560x1440`），但 baseline classes/extent validation 共 144 条、exit0 且 baseline unchanged；quality03-normal 66 sampled non-reset median `16.7599 ms`，静止与短移动截图 exit0；NativeAA01 49 non-reset median `16.6743 ms`、`2560x1440→same`、截图 exit0，screenshot fix 已在 NativeAA 场景核实。fallbackgpu02 `6b00c780...` 通过实际 FSR record → 注入 post-record reject → renderer current green 4096 pixels（保留 alpha）→ 下一次 actual blue SDK reset，明确仅为 fallback/reset 范围证据，不是 SDK internal fault 或 full-facade 通过。上述结果仍不足以宣称 P1 完成；完整 P1 仍需 nonzero MV、depth translation 和 yaw 证据。

Linux Distrobox crash core 证明 `EffectContext` `alignas(32)` 实际 `mod32=24`；生成 backend alignment 修复后的全游戏 build 为 `76513e...`，main harness run02 exit0，5 项 readback 与 reset/resize 通过，但旧 run02 的 AMD coherent-memory `VUID-02790` 已由后续 Linux adapter run03 修复并以 0 errors/0 warnings 通过。Linux run03 已 clean，但这仍不是 Steam Deck 或完整 Linux 游戏画质验收；当前 Linux game 的完整实景结果仍待补充。

Linux 已将当前源码与 PPC 隔离传输到 psvita，使用 `FSR=ON` / `REQUIRE=ON` 完成完整 configure；证据见 [`linux-full-preparation.json`](../../out/streamline-fg-p0/linux-full-prepare/linux-full-preparation.json) 与 [`configure.log`](../../out/streamline-fg-p0/linux-full-prepare/configure.log)。Linux run03 clean 只证明 adapter harness 边界，不能据此宣称完整 Linux 游戏画质或 Steam Deck 验收；FG validation 与物理显示采集缺口仍在。

## 下一步门禁

- [ ] 继续分析 foreground01、background04 与 apidump01 的 layout/API trace，解释 46 条 validation、10 条 layout VUID、SDK WAW 及 pacer transition 缺口；`actual_presents=2` 与 API cleanup 成功仍不足以关闭 P0。
- [ ] 解释或隔离 run02 的 fake swapchain / `TRANSFER_SRC` VUID，并补足 active FG 退出证据。
- [ ] 取得物理显示证据后再准备 Gate 1 材料复审；不因 48 frames 或旧错误消失而关闭 P0。
- [ ] 保持 G002 的 P1 接线与运行结论受证据约束；P2–P4 仍待验收，直到各自依赖和 Gate 结论满足。

## 调度与设计证据

原始 brief 明确先做 FSR，并行推进 FG；此前“严格串行、P0 未过则不得做 FSR”的说法来自 OpenCode deepwork 旧文件，不是用户技术依赖。当前 G001 已被 superseded（不表示通过）；G006 逐字保留完整 P0 objective、attempt 1/3 NOT PASSED、剩余 2 次复审，以及 validation/display/image/performance 的完整要求。G002 正在开发独立 FSR，G003/G006/G004/G005 待验收；优先顺序只是排程，不新增依赖。native 工具当前状态 active，但用户已授权持续自主执行；OMX 当前为 executing。

设计记录：[stage-dependency-audit.md](../../out/streamline-fg-p0/stage-dependency-audit.md)、[fsr-p1-input-contract.md](../../out/streamline-fg-p0/fsr-p1-input-contract.md)、[fsr-p2-mask-design.md](../../out/streamline-fg-p0/fsr-p2-mask-design.md)。P2 新 architect 计划还包括 alpha replay 与后处理传播；这些文档记录设计和证据边界，不表示 FSR/P2 已验收。

P2 mask 设计记录：[fsr-p2-mask-design.md](../../out/streamline-fg-p0/fsr-p2-mask-design.md)。该记录仅描述同帧 alpha/mask/final-color 对齐及覆盖缺口，不表示 P2 已启动或验收。

## Linux adapter 后续验证

AMD coherent-memory selector 修复后，Linux harness run03 exit0，同步校验 0 errors / 0 warnings，5 次 RGBA 读回、frame gap reset、discard/recreate 与 resize 均通过。证据见 out/streamline-fg-p0/linux-full-prepare/fsr-adapter-linux-run-03-result.json。完整游戏已链接相同 backend，Linux 实景仍在验证；不替代 Steam Deck 和 P2 画质验收。

P2 详细方案与新增 shader 审计已保存到 out/streamline-fg-p0/fsr-p2-mask-implementation-plan.md。真实 alpha 分类证据覆盖捕获的 38/48 个透明 draw；draw176 是 RG/BA 扰动向量累积，不能作为 opacity。方案尚未实施。

Linux 实景最新证据：Quality `30f3e390...` 已在 frame12356 提交 `853x480→1280x720`，41 条采样记录无 reset、exit0。旧截图红蓝交换在 Off 同场景也重现，已定位为 BGRA 交换链截图被按 RGBA 保存；修复不改变游戏呈现。新 Linux `034c22b4...` Native AA 在 frame10314 提交 `1280x720→same`，65 条无 reset 采样的中位帧间隔为 17.107218 ms，`linux-game-fsr-native-aa-01/shot_14399.png` 确认正确颜色，exit0。该帧间隔不是 GPU A/B 收益或全场景性能结论。

真实 FSR renderer fixture `fsr-fallback-gpu-02` 已通过：成功红色帧之后，在真实 SDK record 后注入拒绝，4096 个最终像素保留当前绿色和 alpha，token 被丢弃，恢复蓝色帧强制 SDK history reset。该证据不覆盖 SDK 内部失败或完整生产 facade。P1 仍待受控平移／旋转的同帧非零 MV/depth/jitter 独立比较；P2、Steam Deck 和 FG gate 保持未完成。
