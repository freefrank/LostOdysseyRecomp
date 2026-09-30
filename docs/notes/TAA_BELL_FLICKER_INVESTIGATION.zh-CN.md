# TAA 铃铛架闪烁排查（Issue #46 相关）

状态：排查中。用户已确认 exact-stationary MV 加 stationary color-clip 候选
明显更稳定，但仍有残余闪烁；Vulkan 4K 铃铛问题尚未解决。
已验证的基线已提交并推送到 `origin/mv`，commit 为
`ae7df0fb051980a32410423284199c7cebd9dfc3`；这不是正式发布版本。独立的
`history_fp16` 实验已通过技术检查，但未通过画面验收。
台账：18 个 `taa-position` case 全部保持 `needs_review` /
`not_implemented` / `not_validated` / `not_accepted`，本文不把历史结论绑定到新证据。

## 既有诊断控制（仅 opt-in，默认行为不变）

文件：`LostOdysseyRecomp/gpu/renderer.cpp`。

- `LO_TEMPORAL_LOG_ALL=1`：记录每一帧的 `renderer temporal f...` 摘要和
  gates/相机/探针明细，不需要填起始帧。之前只记
  `LO_TEMPORAL_LOG_START_FRAME` 之后的前 256 帧，后期帧（f2349、f31390 等）
  完全看不见。
- `LO_TEMPORAL_DRAW_LOG_ALL=1`：按逐 draw 记录抖动，不需要起始帧，既有的
  VS/几何过滤仍然生效。
- `LO_TEMPORAL_DRAW_LOG_UNKNOWN=1`：额外记录落在场景视口内但没有位置矩阵槽
  （`temporalSlot<0`）的 draw，一次跑全就能收齐未白名单 draw，不用逐个猜 hash。

## 未提交的候选修复

当前工作树允许 cubic footprint 包含预测主深度表面和一个背景深度表面，
并加入受保护的 `stabilizeStationaryGeometry` 路径。该路径仅在几何稳定且
motion vector 可信时生效；对接近静止的几何（`0.002` 到 `0.125` 像素运动），
将 history weight 从 `31/33` 平滑返回 `0.85`。原有 rejection 和 clamp 规则
仍然生效。可设置 `LO_TAA_STATIONARY_HISTORY=0` 关闭该路径做对照。

设置 `LO_TAA_ACCEPTANCE=1` 会启用从候选 history 独立 resolve 到 display 的
诊断路径，不会给正常 history 路径染色或改值。本文记录的是当时未提交工作树
中的候选，不代表已发布或已获用户验收。

第二个候选增加通用 static coverage 处理。其 GPU 项目通过 811 项检查，runtime
构建成功。使用相同 `31/33` 权重的两表面 ownership 交换 fixture 中，峰峰值由
`128` 降至 `10`，均值由 `132` 变为 `135.875`；输入由 CPU 确定性上传，使用
生产 GPU consumer，不能等同于完整几何生产验证。另有 8 类安全拒绝通过。

## 已有证据

- `render-17897018090240431-f9876`（3 帧，AA=3）：相机稳定
  （`candidate_ready=true`，VP 相同，深度分配 14），但
  `temporal_history_verified=false`。同包 `shader.jsonl` 只覆盖 f0-f255，
  目标帧没有抖动计数。
- `render-17897637349644857-f2349`（铃铛场景）：UI 稳定，只有铃铛架闪，
  且只在 TAA 下出现。静态对照：45 个 VS 里 34 个在白名单，11 个不在。
- `render-17897643027668846-f31390`（AA=3）对
  `render-17897644282377816-f36731`（AA=0）：同一相机（VP 完全相同，988
  draw）。铃铛区域在两包里都动，TAA 是把本来就存在的位移放大了。
- `render-17897654353054983-f2111`（开 `LO_TEMPORAL_LOG_ALL` 后）：
  f2111/f2112 为 `reused=false gap=true`，抖动 0/0/0，gates 是
  `invalid_history|previous_incomplete|epoch_changed`。gap 的根因是抓帧
  readback 必然 stall 超过 250 毫秒阈值，每帧都重置历史并递增 epoch。
  因此抓包看不到稳态 TAA。
- 真实游玩日志（3491 个 temporal 帧）：2381 帧 `reused=true gap=false`，
  约 1800-1856 个 draw 被施加抖动，misses 全 0，unknowns 稳定 1-2 个。
  gates 干净（`mask=0x0`）。全局历史混合工作正常，问题是局部的。
- 未知 draw 排查：4 个未知（`6318`、`81217dc9`、`69349140`、`8bbd`）加上
  之前怀疑的（`760a`、`f95a`）全是 3-12 个 index 的微型 draw，都不是铃铛
  几何。铃铛的深度 draw 是 `b030`（前 25 个 draw，经 draw-step 预览和
  live draw 日志确认）。
- Bloom 隔离：TAA 开 + `LO_DISABLE_BLOOM_PREFILTER=1` 后闪得更厉害，说明
  只在 TAA 下走的 bloom prefilter 原本在 smoothing，闪的源头在它上游
  （被抖动的场景）。

### 候选的自动化证据

GPU 测试项目完成一次构建并通过 791 项检查。在 32 相位的静止几何循环中，
峰峰值变化由 `10` 降至 `4`，均值由 `127` 变为 `126`；运动恢复、深度拒绝、
alpha 以及 raw/camera-only 检查也通过。同一实例的 normal/no-jitter/
no-history 16 帧对照中，四个 Bell ROI 的 normal delta 为
`0.93/2.53/1.49/1.19`，no-jitter 约为 0。Acceptance 诊断持续接受
`97.6%`–`99.5%` 的帧，color rejection 低于 `0.13%`。

以上是 `out/bell-resume/gpu-test.log` 中的自动化和诊断结果。

### exact-stationary 与 color-clip 候选证据

当前 MV 源头候选使用 `exactStationary`：相同 geometry、raster 输入以及实际
vertex shader 读取的常量必须逐位一致。canonical 生成 HLSL literal 严格解析；
relative 或未知读取回退到 full bank。明确未使用的 shared 值和 pixel shader flags
会排除，零值也不会被当作速度阈值。14 项 CPU exactness 和 15 项 usage 检查通过。
限定的 GPU literal-zero 套件在 32 个 jitter 相位、rigid/skinned 路径和无效输入
保护下通过 543 项检查。该套件是新增候选证据，旧 826 项未重复运行。

live `stationary_color_clip` 默认值为 `0`。只有 stable、原始 MV 严格为零、深度
有效及其他 guards 满足时，才会把颜色越界的 hard reject 改为 clamp+blend；橙色
诊断像素表示裁剪后接受。color 组通过 25 项 GPU 检查，新 parser 字段单测通过。
当前 live 运行使用 `snap=1`、`motion_min=.002`、`colorclip=1`，仅作诊断。
`out/bell-resume/live-tuning/color-off|color-on/result.json` 的同 session 32 帧结果：
upper `0.60561→0.50872`（16%）、video `0.61189→0.52842`（13.6%）、ground
`0.47061→0.31962`（32%），不是性能数据。exact-source-MV 的第 877 帧证据中，
video 和 ground 为 100% zero，upper 为 99.412% zero。静止权重 `0.95` 独立对照仅
带来约 4% video 收益，已恢复 `31/33`。

用户现已实景查看 exact-stationary MV 加 stationary color-clip 候选，确认画面
明显更稳定，但仍有残余闪烁。这确认了当前 Bell 场景的部分视觉改善，但不代表
问题已完全解决，也不关闭本次排查。

### History FP16 精度候选

独立的 `history_fp16` 开关已通过 50 项 precision-only GPU 检查，覆盖 RGBA8
source 加 FP16 history 更新、最终 SDR/alpha、HistoryOwner 往返与重置，以及
`127/129` 格式。FP16 parser、Python half-trace、静止范围和 HTML syntax 检查也
通过，runtime 构建记录在 `build-history-precision-runtime.log`。同一 exe 的
32 帧数值为 RGBA8 `31/33`：upper/video/ground
`0.5168837/0.5271268/0.3196223`；FP16 `31/33`：
`0.5533177/0.5797869/0.3611376`，因此不宣称 FP16 修复，也不推荐作为默认。
FP16 `63/65` 为 `0.4818415/0.4930559/0.2660568`，`127/129` 为
`0.4366672/0.4451076/0.2160987`；RGBA8 `127/129` 为
`0.3996239/0.4103040/0.1550984`。更高静止权重仅作诊断，可能产生运动拖影。

用户对更高静止权重候选的最新观察是：似乎只稳定了一点，肉眼不容易分辨；
没有明显看到拖影，但反馈帧率变低。因此该候选未通过验收。当前控制已恢复为
baseline 的 RGBA8 history 和 `31/33` 权重。

### Uhra Main Street 后续观察

用户确认 Uhra Main Street 上方钢架是当前抖动区域。旧版同场景 8 帧 source/output
均值中，钢架由 `7.760` 降至 `0.356`，墙面由 `2.779` 降至 `0.034`，地面由
`2.004` 降至 `0.013`，说明 history 合成有效，残余已局部化。此前多层轮廓候选
让画面略有平稳感，但 60 FPS 降至约 55–58，关闭后恢复 60 FPS。短样本
PresentMon 中 TAA on 的 GPU/CPU busy 为 `7.098/16.877 ms`，AA off 为
`5.018/16.514 ms`，约增加 `2.08 ms` GPU 和 `0.36 ms` CPU；这不是广泛性能结论。

新 debug 候选增加 `jitter_scale`、`stationary_multi_surface` 和 `gpu_timing`，state
报告分阶段计时。当前运行的构建已记录累计 `total_ms`，运行候选同时报告 `last_ms`。
Uhra user02 中 `jitter_scale=.5` 基本消除了横向抖动；`.35` 没有进一
步改善，已恢复 `.5`。同时开启 `stationary_multi_surface=1` 后，用户确认斜线
更稳定，移动中未明显看到拖影，约保持 60 FPS。`.5`/multi-v2 短样本 PresentMon
为 CPU busy `16.8234/16.816 ms`、GPU busy `7.0976/7.0372 ms`、呈现间隔
`16.9901/16.9816 ms`，差异属于噪声范围。当前运行态为 AA3、jitter1、
`jitter_scale=.5`、multi-surface1、snap1、colorclip1、FP16 off、权重 `31/33`。
314 帧中每帧约 732.6 个 replay draw，replay `1.356 ms`、TAA `0.447 ms`、
mask `0.128 ms`；当前已关闭 gpu timing。这只是 Uhra 本地 debug 候选的画面验收，
不代表默认、全地图、跨平台或正式修复。FP16 仍为 opt-in，尚未验收。

### 2026-09-19 当前 Uhra 验收记录

当前已接受的本地 debug 候选使用 internal 3840x2160、用户输出 4K、
`jitter_scale=.5`、`stationary_multi_surface=1`、`snap_stationary=1`、
`stationary_color_clip=1`、`history_fp16=0`、`moving_bilinear_fallback=0`，
history 和 MV 输入均有效。在 `debug-moving-bilinear-opt.exe` 上（SHA-256 为
`00FC2575AD0D515EBC7A68CB2F26110981B271024974E4C870078A2F54E3E3A0`），用户反馈
“这一版已经不错了。可以接受”，实测约 60 FPS。这只是 Uhra 本地 debug 验收；
仍有轻微抖动，不涵盖全局、默认、跨平台或正式发布行为。

1080p internal 到 4K output 时，钢架模型仍严重抖动。`.25` 能改善静态内容，
但移动相机抖动比 `.5` 更差，因此恢复 `.5`。钢架 ROI temporal byte delta 从
`10.15` 降至 `0.483`。移动诊断中钢架第三层拒绝为 `5.44%`、墙面为 `.075%`；
实验性的 `moving_bilinear_fallback=1` 将钢架拒绝降至 `.69%`，但约 60 FPS 下
没有明显感知收益，保持关闭。早期 fallback 虽改善画面却降至 55–58 FPS，仍未
验收。尚未定位到具体材质 shader。

### 主渲染路径接入（Uhra 本地画面验收）

在 Uhra 本地验收之后，已将接受的策略接入正常渲染路径。没有实时面板快照覆盖时，
使用 `jitter_scale=.5`、`snap_stationary=1`、`stationary_color_clip=1` 和
`stationary_multi_surface=1`，并继续使用 RGBA8 history、静止权重 `31/33`，关闭
`history_fp16=0` 与 `moving_bilinear_fallback=0`。TAA 默认启用几何运动矢量；
`LO_MV_ENABLE=0` 仍是明确的对照开关，非 TAA 模式不会运行 motion replay。

本地 CMake RelWithDebInfo 构建已通过，`LoMotionVectorTest` 在默认启用和
`LO_MV_ENABLE=0` 对照路径共通过 71 项检查，主 EXE/PDB 已部署且与构建输出哈希一致。
index-signature-cache 候选 SHA-256 为
`2BC90983FC5F866E16F52ACD0B7642347CAD1792FF67A687A2551817EA53707E`，旧程序保留为
`LostOdysseyRecomp.exe.pre-index-hash-20260919`。普通程序后台静音运行并通过原生 Continue 读档，Vulkan 日志
`runtime-1789846180585750.log` 记录第 960、1080、1200、1320、1440 帧的运动
重放已就绪且被 TAA 使用，运行中没有实时面板覆盖。同一 Uhra 存档、Vulkan、4K
internal/output、120 FPS 目标且无 pacing 的隐藏静音 A-B-A-B 采样中，旧版 heartbeat
均值分别为 54.61 FPS（独立 Release 构建）和 54.57 FPS（先前的 RelWithDebInfo
主程序），新版为 60.34/59.00 FPS。Release `/O2`/`/Ob2` 单独构建仍为 54.61 FPS；
关闭 TAA collector 为 54.78、开启为 55.68，不能据此认定 collector 是主因。
asm-profiler 热点指向 `memcmp` 和 Vulkan driver 最近符号，但采样不能精确归因。
index cache 复用 exact-content index fingerprint，同时保留完整源字节验证。
`LoVertexCacheTest` 已通过 3,668,948 项；MV audit 中 steady tracked/matched/replay
为 988，failed 为 0。用户在前台同一 Uhra 钢架位置、4K 内部／输出下观察，反馈持续帧率
约 60 FPS，画质保持、可以接受。另一次正常运行移动视角记录 1,882–2,605 draws/frame
和 38–55 FPS，条件不同，不能与固定 A-B-A-B 对照直接比较。已接受的 4K Uhra 场景结果
不代表全游戏、跨平台或正式发布验收，1080p internal 到 4K output 的移动相机限制仍未解决。

### Bell 候选的初步实景结果

当前 32 相位真实场景对照仅显示部分改善：red ROI delta 从 `0.9096` 降至
`0.5652`，cyan 从 `2.5112` 降至 `2.1208`，white 从 `1.4418` 降至
`1.0725`，green 从 `1.1751` 降至 `0.9403`，各区域约改善 16%–38%。
残余热图集中在轮廓，并与发生过 history rejection 的像素高度相关；约 17%
的区域像素贡献了 63% 的残余。Windows 运行时候选仍在继续修复，包括对
stable-grid 跨相位深度表面换位的排查。

这项初步结果不能证明可感知修复、全游戏覆盖、跨硬件覆盖、发布就绪或用户验收。

第二个候选的 `LO_TAA_ACCEPTANCE=2` 诊断会用颜色标记拒绝原因，且不修改
history：蓝色为 reactive，黄色为 current/replay depth mismatch，黑色为
primary/support mismatch，洋红为第三层，青色为无效或边界，红色为 accepted，
绿色为 color rejected。当前 32 相位实景对照中，视频 ROI 从基线 `1.4946` 降至
`1.13965`，上方横梁 ROI 从 `1.30728` 降至 `0.82713`；仍有残余。第二个
候选也已被前台验收拒绝，用户仍能明显看到闪烁。候选 `607dd6f1…` 和
`3b942ee…` 均未通过画面验收。

### 用户实景验收结果

用户在 Bell 场景前台验收了 `607dd6f1…` 候选，反馈闪烁仍然明显，确认没有
好转。主要可见问题是上方横梁／支架的金属边缘，而不是之前记录的下方 Bell
ROI；远方地面缝隙也有轻微抖动。匹配的 4K ROI 为
`(x=1062, y=255, w=1326, h=188)`，视频 ROI 为
`(x=1166, y=299, w=98, h=90)`，匹配度 `0.966`。视频 ROI 的 delta 从
`1.4946` 变为 `1.2826`（约 14%），关闭 jitter 时仍为 `0`。
Acceptance 为 `95.45%`，颜色拒绝为 `0.73%`。该候选未通过用户画面验收。
第二个候选已包含通用 static coverage fallback，但两个候选仍未通过画面验收。
本地 live-debug 面板和 launcher 可用于限定诊断。Native Continue 已有实测证据：
成功读取 `save/user01/save.bin` 并进入 Bell 场景，3840x2160 状态中
`history_reused=true`、`motion_ready=true`、`motion_consumed=true`。工具本身
尚未获得单独的用户验收，底层闪烁也仍未验收。

## motion vector 和 coverage 候选之前的历史结论

- 混合 shader（`temporal_aa.cpp`）对全部深度 tap 做校验，历史颜色钳制在
  当前 3x3 邻域内（TAA 模式下生效），cubic 负瓣也有约束，混合本身没有缺陷。
- 铃铛场景 `f1848` 全量表：深度 `702c`/`b030`、材质 `4053`（slot 7，约 300
  draw，7 组 PS）、阴影 `99c2`/`d55` 已补偿，全部施加，misses 为 0，
  映射和补偿完整。
- 只关抖动（TAA 开，`1 3 0 -1 -1`）后不闪，但超采样收益也没了，符合预期，
  证明驱动是抖动本身，不是坏掉的混合。
- 历史权重压到 0.1 也没用：闪不是 ghosting 比例问题，这条路已排除。
- 稳态格点模式（跳过 display 重建、抖动照样开）还是闪：display 重建排除。
- 过去在没有 motion vector 时的结论是：细摆动物体在 Halton 相位下的当前帧
  混叠无解。现在已经有 motion-vector replay 和 consume，但当前两个候选仍未
  通过画面验收；这段历史结论不代表当前诊断或验收结果。`LO_TEMPORAL_HISTORY_WEIGHT`
  只保留为 opt-in 诊断，默认 0.85 不动，AA 档位仍是现成的用户侧开关。

## HDR 说明（Issue #46 原文）

交换链固定为 `R8G8B8A8_UNORM`（`video.cpp`），游戏不输出 HDR 信号，也不切换
DXGI 色彩空间。bloom 日志里的 `HDR_area linear` 是 tonemap 前的内部线性阶段。
关显示器 HDR 就恢复，更指向系统 SDR-to-HDR 映射，与上面 TAA 排查分开处理。
