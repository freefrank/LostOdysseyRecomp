# Changelog / 更新日志

One record of completed changes, with unpublished work separated from verified releases. Dates below are UTC release dates. Planned work belongs in the [roadmap](docs/ROADMAP.md), not release entries.

本文统一记录已完成改动，并区分未发布内容与已确认发布版本；日期采用 UTC 发布日期。后续计划见[路线图](docs/ROADMAP.zh-CN.md)，不作为已发布功能记录。

## Unreleased / 未发布

### Local v0.4.0 development / 本地 v0.4.0 开发

- Rasterize host Settings text at output resolution and apply supported GPU scene AA before UI, skipping repeated final AA using processed-frame coverage. Actual 1080p Settings, same-real-frame Navigation comparisons, synthetic GPU controls and v7 processed-copy/final-skip logs verify these paths. Guest UI remains 720p; unsupported/CPU fallback and broader font quality remain outside this repair. / 宿主设置页按输出分辨率绘字；支持的 GPU 场景在 UI 前执行 AA，并根据已处理帧的覆盖记录跳过最终重复 AA。实际 1080p 设置页、同一真实 Navigation 帧对照、合成 GPU 控制及 v7 处理／跳过日志验证了这些路径。guest UI 仍为 720p；不支持场景／CPU 回退及更广字形质量不在此修复结论内。
- Add an independent English / Simplified Chinese Debug menu switch with immediate label/status updates and persistence that preserves unconfirmed display previews. Configuration/translation fixtures, startup switching and an actual Map2 tutorial-state EN → SC → EN cycle passed; map, no-battle and waiting-for-control statuses translated and English was restored. Capture render state remains first without scrolling or overlap. Other battle/status branches are regression coverage. / 增加独立英文／简体中文 Debug menu 切换，即时更新标签和状态，持久化时保留未确认的显示预览。配置／翻译 fixture、启动切换及实际 Map2 教学状态的 EN → SC → EN 通过；地图、无战斗和等待可控角色状态完成翻译并恢复英文。Capture render state 保持最上方，无需滚动且不重叠；其他战斗／状态分支列为回归。
- Add SMAA 1x HIGH using the upstream three passes and lookup textures, alongside Off/FXAA, with legacy `fxaa` settings migration. Targeted GPU identity, edge, flat-region, letterbox and resize checks passed. / 使用上游三阶段和查找纹理接入 SMAA 1x HIGH，与 Off/FXAA 并列并兼容旧 `fxaa` 设置；定向 GPU 原生恒等、边缘、平坦区域、黑边及尺寸切换检查通过。
- Match the actual output viewport with independent Standard/bilinear and High/bicubic spatial scaling, defaulting to High. Apply AA at input size before scaling; use area-weighted downsampling in both modes, with staged large reductions. GPU scaling and six configuration cases passed. A captured Navigation frame was replayed through all six AA/quality combinations at 1080p; High showed clearer edges and SMAA preserved this text's shape closer to Off than FXAA. Guest rendering resolution is unchanged; broader text/language and motion acceptance remain pending. / 自动匹配实际输出视口，提供独立的标准／双线性与高／双三次空间缩放，默认高质量；先在输入尺寸执行 AA，再缩放，两档缩小时均按覆盖面积取样，大幅缩小分级处理。GPU 缩放与 6 项配置测试通过；同一实际 Navigation 帧以六种 AA／质量组合重放至 1080p，高质量边缘更清楚，SMAA 在此文字样本的形状比 FXAA 更接近 Off。guest 渲染分辨率未变，更多文字／语言及运动场景验收仍待完成。
- Remove unimplemented DLSS/frame-generation placeholders from the local Settings page; keep all five interface languages and the research documentation. / 本地设置页移除尚未实现的 DLSS／帧生成占位，保留五种界面语言及研究文档。
- Add persisted 30/60/120 FPS selections, host deadline pacing and a narrowly scoped guest present-interval hook. The 60-FPS implementation passed bounded movement, dialogue, normal menu application and Ring core-timing checks. Natural 30/60 encounters showed matching Ring timer/progress rates and timeout sequences; precise release/Perfect, damage and whole-game locked 60 are not claimed. The optional unvalidated 120 candidate requires `LO_EXPERIMENTAL_120=1` and may be deferred. / 增加可保存的 30/60/120 FPS 选项、宿主截止时间节奏控制及限定调用点的 guest 呈现间隔钩子。60 FPS 实现通过限定移动、对白、正常菜单应用及 Ring 核心计时检查；自然遇敌的 30／60 对照显示 Ring 时间／进度速率及超时序列一致，不宣称精准释放／Perfect、伤害对照或全游戏锁 60。未验证的可选 120 候选需要 `LO_EXPERIMENTAL_120=1`，可延期。
- Correct host pacing so an already-late frame does not wait an extra period. The Map2 30/60 comparison averaged 30.00/59.34 completed presents per second, with engine delta/wall time near 1 and no observed 2× traversal acceleration. Input ACK uncertainty, other scenes, sustained pacing, animation, Ring timing, audio and cutscenes remain outside this result; it is not locked-60 or full gameplay acceptance. / 修正宿主节奏控制，已迟到的帧不再多等待一个周期；Map2 的 30／60 对照平均达到 30.00／59.34 次完成呈现每秒，引擎时间／墙钟时间接近 1，未观察到两倍移动加速。输入 ACK 误差、其他场景、持续节奏、动画、Ring 时机、音频和过场仍不在该结论内，不代表锁定 60 FPS 或完整游戏验收。
- Add persisted experimental TAA using camera reprojection, scene depth, projection jitter, stable-grid history and reactive rejection. Normal `antialiasing=3` on integrated v7 completed 64 static/moving trace groups with 55 history reuses/9 resets, finite depth, preserved alpha and 95 proven processed-copy/final-skip records. Unsupported paths use SMAA; a newly detected unsupported transition may contain one already-jittered frame. Native object motion and whole-game quality remain unverified. Temporal diagnostics and the earlier failed prototypes remain documented. / 增加可保存的实验性 TAA，使用相机重投影、场景深度、投影 jitter、稳定网格历史及响应变化的历史拒绝。v7 正常 `antialiasing=3` 完成静止／移动两段共 64 组记录，包含 55 次历史复用／9 次重置；深度值均有限、alpha 保留，并有 95 次已证实的处理／最终跳过。不支持路径使用 SMAA；新检测到的不支持切换可能包含一帧已经注入 jitter 的画面。原生对象运动和全游戏画质仍未验证，时域诊断及此前失败原型保留在文档中。

- Complete DLSS/FSR feasibility documentation covering official input contracts, actual scene/depth/camera/jitter evidence, integration/distribution risks and the next implementation stages. Native object/skeletal motion, verified exposure/color-space semantics and vendor backends remain future work; research completion does not claim an available SDK feature. / 完成 DLSS/FSR 可行性文档，涵盖官方输入契约、实际场景／深度／相机／jitter 证据、接入／分发风险和后续实施阶段。原生对象／骨骼运动、已验证的曝光／色彩空间语义及供应商后端仍属后续工作；研究完成不代表 SDK 功能已可用。

The integrated builds, selected tests and bounded Map2 checks passed at their recorded scope. Actual v7 Graphics checks passed seven-row layout, unchanged settings during preview, 15-second quality rollback, Keep persistence and same-process close/reopen, including TAA and 60-FPS application without `LO_FPS`. The subsequent static 3D camp averaged 59.80 FPS across 29 complete windows; this is not a paired combat or whole-game locked-60 result. The separate final Ring comparison passed core timing and the long-hold timeout path; precise release and broader gameplay remain regression coverage. TAA is a camera-based experiment; native object motion, adjustable guest rendering resolution and DLSS/FSR backends are not implemented. These changes remain local, unpushed and unpublished, with no new user acceptance. See [development evidence and limits](docs/notes/v0.4.0-development.md). / 集成构建、选定测试及有界 Map2 检查在各自记录范围内通过。v7 实际图形页验证了七行布局、预览时设置文件不变、15 秒质量回退、Keep 持久化及同进程关闭／重开，包含无 `LO_FPS` 覆盖时应用 TAA 和 60 FPS。随后真实静止营地的 29 个完整窗口平均达到 59.80 FPS；不代表战斗对照或全游戏锁 60；另行完成的最终 Ring 对照通过核心计时与长按超时路径，精准释放和更广流程列为回归。TAA 属于相机重投影实验；原生对象运动、可调 guest 渲染尺寸及 DLSS/FSR 后端尚未实现。改动仍为本地开发，未推送／未发布，无新增用户验收。见[开发证据与边界](docs/notes/v0.4.0-development.md)。

### Development workflow after the v0.3.0 tag / v0.3.0 标签之后的开发流程

- Separate selected test suites and path-filtered CI from release packaging; provide one local test entry point without implicit full builds. / 将按需测试套件与按路径触发的CI从发布打包拆出，统一本地测试入口，不隐式全量构建。
- Keep checks proportionate and clean synthetic temporary inputs; default builds omit test targets. / 按改动选择相称检查并清理合成临时输入，默认构建不包含测试目标。

These workflow changes are outside the existing v0.3.0 tag. / 这些流程改动不属于现有v0.3.0标签内容。

## Published / 已发布

### [v0.3.0](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.3.0) — 2026-09-07

Published at 06:55:19 UTC from unchanged tag `fba7ae4`; official package and short first-battle smoke checks passed. / 于UTC 06:55:19从未改动标签 `fba7ae4` 正式发布，正式包与短时首战验证通过。

- Discover shaders inside CPX resources and the loaded XEX, and derive bounded vertex-fetch/output-link variants before gameplay. / 游戏开始前扫描CPX资源与已加载XEX中的shader，并推导有限顶点提取／输出链接变体。
- Persist previously used pipeline recipes and prepare them in parallel on later launches, with validated cache files and runtime fallback. / 持久化实际使用过的管线记录，在后续启动并行预创建，校验缓存并保留运行时回退。

Local build, fixtures and first-battle pipeline reuse checks passed. Two known shader failures remain; no measured FPS/stutter improvement, complete first-use PSO coverage, shadow-flicker fix or new player visual acceptance is claimed. / 本地构建、fixture和首战管线复用验证通过；两个已知shader失败仍在，不宣称测得帧率／卡顿改善、覆盖全部首用PSO、修复阴影闪烁或新增玩家视觉验收。

### [v0.2.2](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.2.2) — 2026-09-07

Published from `f03efe370d444db1a8a9c1213c240da697f58504`. / 发布提交为 `f03efe370d444db1a8a9c1213c240da697f58504`。

- Fix initialization before partial copies into placed resolve render targets, addressing tested AMD black/dark title, background and depth-of-field output; retire cached framebuffer views with their textures. AMD tests and NVIDIA RTX 5080 regression/user acceptance passed. / 修复 placed resolve 渲染目标局部复制前的初始化及缓存视图退役，解决已验证 AMD 标题、背景与景深全黑／偏黑；AMD 测试与 NVIDIA RTX 5080 回归／用户验收通过。
- Preserve Unicode Windows startup and save paths; eight startup cases and eight storage runs passed. The complete Issue #4 gameplay crash remains unreproduced. / 保留 Windows Unicode 启动与存档路径，8 项启动及 8 组存储测试通过；Issue #4 完整游戏崩溃仍未复现。

See [v0.2.2 notes](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.2.2). Two known shader-preparation failures remain; these checks do not establish full-playthrough compatibility. / 见 [v0.2.2 说明](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.2.2)。两个已知着色器预编译失败仍保留，验证不代表完整通关兼容性。

### [v0.2.1](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.2.1) — 2026-09-06

- Add F1 next-frame render-state capture with automatic ZIP, progress and output path. Raw files are retained; this is a diagnostic export, not replayable GPU capture. / 增加 F1 下一完整帧渲染状态捕获、自动 ZIP、进度与路径提示，保留原始文件；属于诊断导出，不是可重放 GPU 捕获。
- Combine SDL-mapped controllers and keyboard into player 1, support hotplug, add E/R triggers and clear keyboard state on focus loss. / SDL 已映射手柄与键盘合并到玩家 1，支持热插拔，增加 E/R 扳机并在失焦时清除按键状态。
- Clarify supported editions using Redump entries. AMD rendering repair and the paused text-language patch are not part of this version. / 按 Redump 条目明确支持版本；本版不含 AMD 渲染修复及已暂停的文本语言补丁。

### [v0.2](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.2) — 2026-09-06

- Add audited USA/Europe 0.0.0.3 support alongside Asian 0.0.0.4, with strict XEX validation and mixed-edition rejection. / 增加已核对欧美 0.0.0.3 支持，保留亚洲 0.0.0.4，严格核对 XEX 并拒绝版本混装。
- Select game text and voice choices from the installed edition; import missing data before first-launch setup. / 按安装版本提供游戏文本与语音选项，首次启动缺数据时先导入再设置。
- Automatically select the requested imported disc and reload its index; controlled four-disc manager tests passed, with chapter-boundary story progression still unverified. / 自动读取原游戏请求的已导入盘并重载索引，四盘管理器受控测试通过，章节交界剧情尚未验证。

### [v0.1](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.1) — 2026-09-06

- First experimental portable Windows x64 release with DXC dependencies, graphical importer and first-launch setup for the supported four-disc Asian edition. / 首个实验性便携 Windows x64 版本，含 DXC 依赖、图形导入器及受支持亚洲四盘版的首次设置。
- Add five interface/game text choices, FXAA and display settings, shader location index and parallel preparation. DLSS and frame generation remain placeholders. / 提供五种界面／游戏文本选项、FXAA 与显示设置、着色器位置索引及并行预编译；DLSS 与帧生成仍为占位。
- Include dialogue playback, shadow rendering and window responsiveness repairs. Early-area/selected-scene coverage does not establish full-game compatibility. / 包含对白播放、阴影渲染及窗口响应修复；早期区域与选定场景验证不代表全游戏兼容。

Release history checked against GitHub release records through 2026-09-07 UTC. No v0.1.1 release record was found, so no entry is inferred. / 已核对截至 UTC 2026-09-07 的 GitHub 发布记录；未找到 v0.1.1 发布记录，因此不推定该版本已发布。
