# FSR / DLSS FG Codex 停止交接

日期：2026-09-23。用户已明确停止开发并要求形成 checkpoint；当前 native goal 为 paused。本文记录可复用证据、状态和恢复边界，不表示发布或验收完成。

## 当前状态

- 本检查点之前的提交为 `283e5a1`（GPU timing，基于 `79ee317`）；包含本文的后续提交保存 postprocess 未完成代码。未发布 Release。
- G002（P1 FSR）已完成；G003（P2）进行中；P3/P4 尚待实施。
- P0 Gate 1 attempt 1/3 为 NOT PASSED，剩余 2 次材料复审。P0 的 validation、display、image、performance 要求仍保留。
- 用户已授权本次 checkpoint 所需的提交和推送范围；停止后恢复工作需要新的明确指令。本轮仅完成交接及已授权的提交／推送，不继续开发或追加运行测试。

本轮运行已收尾，相关进程 exit0 且已退出。GPU timing 代码审阅已通过；timing 结果属于完成的诊断切片，postprocess GPU 仍是未完成工作。`fsr-postprocess-windows-01/result.json` 已保存。

## 已确认的验证边界

新增四项 P2 设计/构建验证通过：R8 传播 CPU 检查、5 项 SPIR-V、静态审阅、Windows/Linux 双平台构建。证据见 [`out/fsr-postprocess-cpu/result.json`](../../out/fsr-postprocess-cpu/result.json)、[`fsr-p2-postprocess-build-result.json`](../../out/streamline-fg-p0/fsr-p2-postprocess-build-result.json) 和 [`fsr-p2-postprocess-static-review.json`](../../out/streamline-fg-p0/fsr-p2-postprocess-static-review.json)。Windows 构建 SHA256 为 `eb84f8b4b24eb10f01c5dd4e2895e57e611c2b33cc876693a986fee516cdd77d`，Linux 为 `7bdf80fa8683bfff51d2bd5dd873558694e272e756e3f757954fb2f12ca79323`。

Windows 实景 `fsr-postprocess-windows-01` 的 frame 12000 中，四种 PS 共六个 draw 均不可发布：三种 blur 对应五个 draw（2072、2075、2078、2081、2084）为 `quad_unavailable`，tone draw 2085 为 `sampler_unavailable`。这些 pass 均未 record/publish，final scene 为 unsupported；独立 loader 检查 exit1。该结果不能写成 postprocess GPU、SDK mask 或 P2 通过。证据见 `out/streamline-fg-p0/fsr-postprocess-windows-01` 和其中的 `postprocess-independent-check.json`。

GPU timing 已完成 10974 rows，parser 零错误；它是可选诊断记录，不是性能比较。capture 仅覆盖 frame 12000，reference/helper/checker 文件保存在对应 `out` 目录。当前缺口是解释 quad 条件（包括 cull）与 sampler 条件，保留现有 guard，不能只删除检查。

P1 的 Windows Quality/Native AA、motion translation/yaw 和相关接线已有独立验收结案；Linux 当前 slice 只有 build。设备为 psvita 上的 ONEXPLAYER APEX、Radeon 8060S、Bazzite 44、Distrobox `psbuild`，根目录为 `/var/home/freefrank/codex-fsr-linux-20260923-01a0ccfd`。Steam Deck、FG、物理显示、SDK 绑定、透明覆盖、完整质量和性能均未验收。

## P2 设计与未覆盖范围

P2 设计包含 alpha replay、resolve/fetch bridge 和后处理传播。已保存的 alpha 证据、shader 审计、AimRing 87 伤害入口以及 Hypocenter 路线只能支持局部场景入口与材质覆盖研究，不能代替 quality A/B。`fsr-p2-mask-design.md`、`fsr-p2-mask-implementation-plan.md` 及现有结果文件保留为后续依据。

旧 validated bridge 的 Windows 基线为 `e470f7c`，位于 `out/streamline-fg-p0/p2-bridge-validated-baseline/windows` 及对应 remote evidence；Hypocenter 路线确认 AimRing 与 87 伤害，见 `route-replay-timeline.json`，这不是画质验收。

P0 SDK 内部 layout 障碍的只读诊断保存在 [`p0-api-layout-trace.md`](../../out/streamline-fg-p0/p0-api-layout-trace.md)、[`p0-api-input-trace.md`](../../out/streamline-fg-p0/p0-api-input-trace.md) 和 [`p0-host-remediation-followup.md`](../../out/streamline-fg-p0/p0-host-remediation-followup.md)。不修改 SDK，也不把诊断写成根因修复。

接续资产包括原样复制到 `tools/tests/fsr/` 的 `postprocess_mask_reference.py`、`check-postprocess-capture.py`、`check-postprocess-host.py` 和 `check-fsr-isolated-timing.py`。四个脚本随本检查点提交。历史 dirty 的 ROADMAP 双语文件、`docs/project-management/items.json`、`sync-state.json`、`cmake/LoStreamline.cmake`、`tools/tests/streamline_fg` 和 `.omx` 保留本地，未纳入本次提交，也未同步远程看板。`out` 中的游戏截图、原始捕获和构建产物同样只保留本机；恢复时先检查这些文件的实际状态。

## 恢复入口

恢复时先核对包含本文的 checkpoint 提交、当前工作区和上述证据路径，再从 G003 的 quad/cull/sampler 条件诊断开始。保留 `out` 中的结果、manifest、截图和参考文件；重新运行任何应用、GPU、构建或完整游戏流程都需要新的用户指令。不要以 clean CPU/SPIR-V/build、单场景截图、10974 timing rows 或 Hypocenter 路线替代 P2/P0 Gate。


接续命令示例（收到继续指令后，使用现有捕获，无需先重跑游戏）：

```powershell
python tools/tests/fsr/check-postprocess-capture.py out/streamline-fg-p0/fsr-postprocess-windows-01/alpha-capture/fsr-alpha-bridge-f12000.jsonl --output out/streamline-fg-p0/fsr-postprocess-windows-01/postprocess-independent-check.json
python tools/tests/fsr/check-fsr-isolated-timing.py out/streamline-fg-p0/fsr-postprocess-windows-01/console.log --output out/streamline-fg-p0/fsr-postprocess-windows-01/timing-independent-check.json --first-frame 4000 --exclude-frame 12000 --exclude-frame 12001
```

第一条当前预期 exit1，表示实景后处理尚不可用。第二条只核对计时记录格式和完成身份；额外截图帧仍需从实验元数据排除，统计值不能作为性能结论。

## 用户授权恢复开发与 P2 后处理验证更新（2026-09-23）

用户已提供明确指令恢复开发；当前状态脱离 paused。历史停止 checkpoint、01/02 运行记录与既有基线证据均保留。

### 当前阶段与门禁状态

- 总体阶段：G002（P1 FSR）已完成验收；G003（P2）仍处于进行中（in progress）；P3/P4 尚待实施。
- 门禁状态：P0 Gate 1 维持 attempt 1/3 NOT PASSED，剩余 2 次材料复审机会。P0 的 validation、display、image、performance 要求仍保留。
- 跨平台覆盖：Linux 平台当前仅有旧版本构建，本次 C++ 后处理修改尚未在 Linux 上进行验证，不宣称双平台新通过。
- 运行对比背景：此前的 02 运行分辨率为 1440p（与 01 的 720p 不一致），但其实际失败原因是 inset quad 缺少 clear 背景证明，并非 1440p 本身导致失败；03 运行恢复了 720p 分辨率条件，但因同时修改了 C++ 传播实现，不构成单一变量对比。01、02 与旧 baseline 结果全部保留。

### 本次修复与验证结论摘要

本次针对 P2 后处理传播补齐了保护与判定机制：
- 支持小数 viewport 的像素覆盖判定，并采用精确矩形角点检查，避免 SDR 宽松 epsilon 导致对角线像素缝隙。
- Resolve 阶段通过 validRect 交集裁剪，padding 保持无效。
- 接入真实 `depth_color_tile_clear` 追踪，对合格的 inset 几何以 clear 背景补齐，且在未知 RGB 写入或不支持后处理时使 clear 失效。
- 首次 clear 允许保留 raw 收集，并细化区分输入缺失（`input_unavailable`）与采样器不支持（`sampler_unavailable`）。
- Python 离线参考工具修正了 Vulkan Y 轴与项目 HLSL `FLT_MIN` 实际为 `-FLT_MAX` 的负极值处理，并增加独立 clear 检查套件。

相关 CPU 测试（`LoNativeDlssP2RoutingTest`、`LoFsrAlphaPropagationPolicyTest`）、Python clear 6 项测试与单像素 tonemap 回归测试全部通过。Windows build03（EXE `8e513eb2...`，源码标识 `eac07c5c...`，基于 dirty HEAD `ffc5399`，不可仅用 HEAD 标识）在 RTX 5080 Vulkan 1280x720 FSR Quality 实景（`fsr-postprocess-windows-03`，frame 12000，exit 0，配置存档基线未变）中完成验证：6 个已审计 draw 全部成功 record/publish，5 组 blur 与 1 组 tone 逐像素比对零差异（修正参考端 `FLT_MIN` 误解，原单像素失败报告原样保留），原始颜色零差异，且独立 oracle 传输链全量一致完成。

完整数据、逐 draw 像素明细及 oracle 传输记录详见 [Codex 进度记录](fsr-dlss-fg-codex-progress.zh-CN.md)。

### 保留缺口与未验证范围

- 深度不变性尚未由 originaldepth 对证明。
- 光栅化边缘规则未认证（`edge_check_status: not_covered`）。
- host prefilter 替换未覆盖（仅 host scene-only 通过）。
- SDK 遮罩绑定、完整 P2 画质及性能均未验收。
- Linux 实机运行未完成。
