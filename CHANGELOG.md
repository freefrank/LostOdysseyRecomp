# Changelog / 更新日志

One record of completed changes, with unpublished work separated from verified releases. Dates below are UTC release dates. Planned work belongs in the [roadmap](docs/ROADMAP.md), not release entries.

本文统一记录已完成改动，并区分未发布内容与已确认发布版本；日期采用 UTC 发布日期。后续计划见[路线图](docs/ROADMAP.zh-CN.md)，不作为已发布功能记录。

## v0.5.2 — Release preparation

### English

- Include original VS/PS microcode in manual F1 render captures to support shader diagnosis.
- With the existing collection opt-in enabled, upload pending shader programs every three minutes and trigger an additional background attempt after F1 capture. Deduplicate content in D1 and associate GPU metadata; delete inactive records after 30 days.
- Keep automatic collection and upload work bounded, with no file/network I/O or waiting on the game thread.
- Add bilingual privacy documentation and simplify the README; move older release descriptions into the changelog and archive.

Focused checks and background D3D12 collection validation are retained in [current status](docs/STATUS.md). Manual F1 end-to-end validation and Vulkan/AMD collection coverage remain pending.

### 简体中文

- 在手动 F1 渲染捕获中附带 VS/PS 原始微码，补充着色器诊断依据。
- 沿用现有收集同意开关，每三分钟增量上传待处理着色器程序，并在 F1 捕获后额外触发一次后台上传尝试。D1 按内容去重并关联 GPU 元数据，30 天未更新后删除。
- 限制自动采集和上传的工作量，游戏线程不执行文件或网络 I/O，也不等待上传。
- 新增双语隐私说明并精简 README，将旧版描述移至 CHANGELOG 和归档。

已有定向检查及 D3D12 后台采集验证见[当前状态](docs/STATUS.md)。F1 菜单完整流程、Vulkan/AMD 采集仍待实机验收。

## v0.5.1 — 2026-09-10

### English

- Rename the updater release from `v0.5.1-updaterfix` to `v0.5.1`, with matching executable and package versions. Behavior and retained validation are unchanged.
- Double-click `LostOdysseyUpdater.exe` beside the installed game and `manifest.json` to check for updates without launching the game first. Close the game before updating; the updater starts it after a successful installation.
- Follow GitHub Latest when the numeric version is higher, or when the numeric version is equal but the suffix differs. Keep the full release suffix in the program and package manifest. Existing v0.5.0 clients can upgrade to this version through their numeric-version check.
- Validation: 43 standalone checks, 14 version-policy checks and 7 suffix-packaging checks passed.

### 简体中文

- 将更新器版本从 `v0.5.1-updaterfix` 统一为 `v0.5.1`，同步程序和安装包版本；功能与已有验证结果不变。
- 在游戏程序和 `manifest.json` 同目录双击 `LostOdysseyUpdater.exe`，无需先启动游戏即可检查更新。请先关闭游戏；更新成功后会自动启动游戏。
- GitHub Latest 数字版本更高，或数字版本相同但后缀不同时触发更新；程序与包清单保留完整发布后缀。现有 v0.5.0 客户端可通过数字版本检查升级到此版本。
- 验证：43 项独立启动检查、14 项版本规则检查和 7 项后缀打包检查通过。

## v0.5.0 — 2026-09-09

### English

- Add conservative position evidence for unknown vertex shaders, serialized in client schema 2 with independent temporal guards; the Worker accepts schema 1 and schema 2 without migration, and jitter classification is unchanged. Focused native, corpus, protocol and source-0.5.0 build checks passed; current game visual validation remains pending.
- Add four capture-confirmed c7 vertex paths (48 draws per captured frame); prioritize shader anomaly uploads every 10 seconds, cap routine resolution variants, reserve queue capacity, and defer MV archival uploads behind shader diagnostics. The user accepted the Ghost Town slot-02 same-scene fix after six spaced screenshots over approximately 11.37 seconds.
- Cover telemetry-confirmed vertex shader `0b786a899598ce18` with c7 TAA jitter while retaining camera/viewport/depth guards; the patch is included, while broader paths remain regression coverage. The Ghost Town slot-02 acceptance does not independently verify this shader.
- Extend optional TAA collection with compressed 32-frame, 32×18 sparse depth and camera-only motion samples, jitter and camera matrices. Reuse the existing renderer fence for GPU readback; bound collection to one pending sequence and at least five minutes between sequences. This does not supply object/skinned motion vectors or implement DLSS frame generation.
- Fix a settings-entry crash caused by mixed old/new translation-table definitions in an incremental build; rebuild all translation consumers together.
- Add opt-in TAA shader diagnostics with first-setup/existing-settings consent, a persistent off switch and background upload to lo.dotslash.pro. Worker/D1 deduplicates across clients; no raw logs or game assets are uploaded. Add ten capture-confirmed vertex projection paths. Windows build and service checks passed; game acceptance pending.

- Fix Vulkan presentation DPI context and request swap-chain recreation after an out-of-date surface, including unchanged window sizes. Build passed; runtime confirmation is pending user testing.

- Keep source and release target at 0.5.0. Reuse index/primitive scratch, specialize endian conversion, compare vertex sample bytes directly and use precise Windows pacing waits. The final fixed Map16 4K comparison reached 59.76 RTSS FPS (16.72 ms internal mean); broader performance and player acceptance remain pending.

- Reuse content-checked shader identities across shader and pipeline lookup. The shader identity change passed 74 focused checks.

- Add Windows Vulkan alongside D3D12, with backend capability checks, failure fallback and separate caches.
- Automatically recognize game discs and DLC from files, folders or mixed selections. Start directly from the executable, with portable game-path discovery.
- Modernize the installer, updater, first-run setup and Debug Menu. Add a recomp icon and lighter window interactions.
- Use original game menu assets where available and consistent Simplified Chinese labels. Save graphics settings with one click; offer Now/Later for changes that need a restart. Closing Settings returns directly to the previous menu without the original confirmation dialog.
- Reuse valid startup shader caches, prepare shaders in parallel and reduce unnecessary CPU polling.
- Fix the reproduced Issue #12 GC/render-thread race and DLC directory filtering. Keep the game window sized in physical pixels and support direct Xenia-to-Recomp save copying.
- Add the v0.5.0 rendering diagnostics and focused repairs: Map16 TAA constant-path coverage, independent shader JSONL logs that follow runtime logging by default (with `LO_SHADER_LOG_FILE` customization or disable support), accepted-present/frame timing, GPU batch timestamps and bulk register snapshots.
- Correct Windows physical-pixel sizing across DPI-aware Plume/D3D12/Vulkan paths, preserve window placement through display changes, and add session-only Alt+Enter/fullscreen transitions.

These changes have passed their focused native and CPU checks. A fixed Map16 4K performance comparison reached **59.76 RTSS FPS** (16.72 ms internal mean) after the retained 48.01 FPS baseline; this is a bounded observation, not a whole-game benchmark. A separately retained Map16 temporal observation passed 32 consecutive 3840×2160 phases with normal ground output and matching paired material/depth uploads. The four capture-confirmed c7 paths were accepted by the user after the Sol scene check; broader scenes, whole-game coverage and fullscreen/Alt+Enter acceptance remain regression work. The enemy-death `c230` path and broader shader-family coverage remain follow-up work. Historical intermediate source identities are retained in the [release preparation record](docs/RELEASE-v0.5.0.md).

Windows D3D12/Vulkan is the delivery scope. DX11 is future work; broader GPU coverage awaits feedback. Three imported DLC packages were read successfully, but reward collection and dungeon gameplay remain unverified. The four c7 paths are accepted for the Ghost Town slot-02 scene; broader scene and whole-game coverage remain regression work.

### 简体中文

- 为未知顶点 shader 增加保守的位置证据，并以客户端 schema 2 搭配独立时序 guards 序列化；Worker 同时接受 schema 1 和 schema 2，无需迁移，且不改变 jitter 分类。定向 native、语料库、协议及 source-0.5.0 编译检查通过；当前候选仍未完成游戏画面验收。
- 补齐 capture 确认的四条 c7 顶点路径（每帧 48 次绘制）；异常 shader 每 10 秒优先上传，限制普通尺寸变体并预留队列空间，MV 资料上传让位于 shader 诊断。Sol 场景画面检查已由用户确认通过。
- 为真实采集确认的顶点着色器 `0b786a899598ce18` 补齐 c7 TAA 抖动覆盖；保留相机、视口和深度检查。该补丁已包含在候选中，但 Ghost Town slot-02 的验收不单独证明此 shader。
- 修复设置翻译表在增量构建中混用导致的闪退；扩展可选 TAA 收集，上传压缩的 32 帧稀疏深度、相机运动、抖动及相机矩阵。最多保留一组待上传序列，采集间隔至少五分钟；尚不包含物体／骨骼运动或 DLSS 帧生成。
- 新增可选 TAA 着色器诊断：首次设置或已有玩家打开设置时征求同意，可随时关闭；后台向 lo.dotslash.pro 上传摘要，Worker/D1 跨用户去重，不上传原始日志或游戏资源。补齐十条 capture 已确认的顶点投影路径。Windows 编译及服务检查通过，游戏验收待用户完成。

- 补齐 Vulkan 画面获取和提交的 DPI 上下文；交换链失效时，即使窗口尺寸未变也请求重建。编译通过，实机效果等待用户测试。

- 源码和发布目标固定为 0.5.0。复用索引／图元临时数组，将字节序判断移至循环外，直接比较顶点采样字节，并使用 Windows 精确限帧等待。固定 Map16 4K 对比从保留基线 48.01 FPS 改善至 59.76 RTSS FPS（内部均值 16.72 ms）；这是限定场景观察，不是全游戏 benchmark。

- shader 与 pipeline 查询共用经过内容校验的 shader 标识；shader 标识改动通过 74 项定向检查。

- 新增 Windows Vulkan，与 D3D12 并存，支持后端能力检查、失败回退和独立缓存。
- 从文件、文件夹或混合选择中自动识别游戏光盘与 DLC。可直接运行游戏程序，并自动查找便携目录中的游戏资源。
- 改进安装器、更新器、首次设置和 Debug Menu，加入 Recomp 图标及更轻量的窗口交互。
- 在可用时采用原版游戏菜单素材，统一简体中文标签。图形设置单击即可保存；需要重启时可选“现在”或“稍后”。关闭设置直接返回上一级菜单，不再显示原版确认框。
- 复用有效启动 shader cache，并行准备着色器，减少不必要的 CPU 轮询。
- 修复已复现的 Issue #12 GC／渲染线程竞争及 DLC 目录过滤问题。游戏窗口按物理像素确定大小，支持直接复制 Xenia 存档到 Recomp。
- 增加 v0.5.0 渲染诊断与限定修复：Map16 TAA 常量路径覆盖、默认随 runtime 日志启用的独立 shader JSONL 日志（支持通过 `LO_SHADER_LOG_FILE` 自定义或禁用）、成功 Present 帧时序、GPU batch 时间戳及批量寄存器快照。
- 修正 DPI 感知的 Plume／D3D12／Vulkan 路径中的 Windows 物理像素尺寸，保留显示器切换时的窗口位置，并加入仅会话生效的 Alt+Enter／全屏切换。

上述改动已通过对应的原生和 CPU 定向检查。固定 Map16 性能对比从保留基线 48.01 提升至 59.76 RTSS FPS，内部均值为 16.72 ms；这是限定场景观察，不是全游戏 benchmark。另有独立保留的 Map16 时序实跑连续通过 32 个 3840×2160 phase，地面输出正常，材质与深度的配对上传逐位一致。四条 capture 确认的 c7 路径已通过 Ghost Town slot-02 场景六张间隔截图的用户画面验收；该证据边界之外的场景和全游戏覆盖仍属回归工作。历史中间源码身份见[发布准备记录](docs/RELEASE-v0.5.0.md)。

本次面向 Windows D3D12／Vulkan；DX11 属于后续工作，其他 GPU 覆盖等待反馈。三个已导入 DLC 包均已成功读取，奖励领取及地下城游玩仍未验证。Issue #12 报告者确认和全游戏覆盖仍待完成。

Development evidence / 开发证据：[v0.5.0 release preparation](https://github.com/freefrank/LostOdysseyRecomp/blob/v0.5.0/docs/RELEASE-v0.5.0.md).

## [v0.4.2](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.4.2) — 2026-09-08

### English

Repairs the reproduced Uhra Council cutscene crash, expands PowerPC correctness and battle TAA coverage, and improves crash reports and F1 exports.

- Use the guest's low 32 bits for word-switch dispatch, preventing a high-word carry from indexing beyond the host table.
- Correct nine further PPC translation defects in scalar results/flags, update and atomic/absolute addresses, and indirect/conditional branches. Keep the tracked dependency patch synchronized.
- Write essential native crash details to automatic runtime logs through an independent append sink, including faults while normal logging locks are held.
- Add six verified battle terrain/object/skinned TAA paths while retaining the existing guards. Enemy-disappearance flicker remains unresolved.
- Compress completed F1 captures in the background; remove only the matching raw folder after success and preserve it on archive failure. Readbacks/file writes can still pause rendering.
- Retain the current default runtime log plus the two newest earlier logs. Active/undeletable files may remain; custom log paths are excluded.

Fix validation covers 3,258 passing instruction regressions (the old generator fails 1,533 matching cases), 109 switch checks, 14 isolated crash cases and the full Council scene, restored movement, native save and independent restart/reload. TAA validation covers 17,287 CPU checks and bounded 32-phase Map3 battle/tire comparisons; capture/log fixtures and an actual background-export run also pass. Original-reporter acceptance, later chapters and whole-game compatibility remain unverified.

See [Council and semantics evidence](https://github.com/freefrank/LostOdysseyRecomp/blob/v0.4.2/docs/notes/issue7-cutscene-crash.md), [TAA scope](https://github.com/freefrank/LostOdysseyRecomp/blob/v0.4.2/docs/notes/shadow-texture-lod.md#battle-taa-runtime-dev) and [capture behavior](https://github.com/freefrank/LostOdysseyRecomp/blob/v0.4.2/docs/notes/render-state-capture.md).

### 简体中文

修复已复现的乌拉议会过场崩溃，完善 PowerPC 指令语义与战斗 TAA 覆盖，并改进崩溃记录和 F1 导出。

- 字宽 switch 分派使用客体低 32 位，避免高位进位导致宿主跳转表越界。
- 修正另外九类 PPC 翻译错误，覆盖标量结果／标志、更新式与原子／绝对寻址、间接／条件分支，并同步受跟踪的依赖补丁。
- 通过独立追加通道将必要的原生崩溃信息写入自动运行日志，常规日志锁被持有时仍可记录。
- 补齐六条已核对的战斗地形／物件／蒙皮 TAA 路径，保留现有限制；敌人消散闪烁仍未修复。
- F1 捕获完成后在后台压缩，仅成功后清理对应原始目录，归档失败保留源文件；读回和文件写入仍可能暂停渲染。
- 默认保留当前运行日志及最新两份旧日志；活动或无法删除的文件可能暂留，自定义日志路径不参与轮转。

修复验证覆盖 3,258 项指令回归全部通过（旧生成器在相同输入中有 1,533 项失败）、109 项 switch 检查、14 项独立崩溃用例，以及完整议会剧情、恢复移动、原生保存和独立重启读档。TAA 验证覆盖 17,287 项 CPU 检查和限定的 Map3 战斗／轮胎 32 相位对照；导出／日志用例及实际后台导出也通过。原报告者验收、后续章节及全游戏兼容性仍待确认。

详见[议会与语义证据](https://github.com/freefrank/LostOdysseyRecomp/blob/v0.4.2/docs/notes/issue7-cutscene-crash.md)、[TAA 范围](https://github.com/freefrank/LostOdysseyRecomp/blob/v0.4.2/docs/notes/shadow-texture-lod.md#battle-taa-runtime-dev)和[捕获行为](https://github.com/freefrank/LostOdysseyRecomp/blob/v0.4.2/docs/notes/render-state-capture.md)。

## Published / 已发布

### [v0.4.1](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.4.1) — 2026-09-08

#### English

Fixes the reported Map3 tire-shadow flicker with TAA enabled and improves F1 render exports for visual-bug reports.

##### Changes

- Align TAA jitter across the verified scene-depth, opaque-material and lighting passes, and correct shadow reconstruction while preserving depth sampling.
- Capture three consecutive frames in one ZIP, retaining per-frame render diagnostics and raw depth while sharing shaders. Default exports omit draw-step previews and duplicate screenshot PPM/depth `.f32` files; `LO_DEBUG_CAPTURE_DRAW_STEPS=1` restores draw previews.
- Include the current process log snapshot, logging availability, source version and graphics settings in render exports. Unavailable logging does not discard the render data.
- Add focused CPU jitter regression checks and optional submitted-draw diagnostics.

##### Validation and limits

The r2 development candidate passed its integrated build, 8,192 CPU jitter checks, scene checks and same-position Map3 TAA/Off runs. The user confirmed that the original tires no longer flicker with TAA enabled. Three-frame export checks verified archive contents and continued rendering in a separate title/menu run.

TAA remains experimental; other maps, motion and hardware remain regression coverage. The independent AMD reports remain suspended. Capture improvements do not establish an AMD rendering fix or a gameplay-performance improvement.

See [shadow-fix evidence](https://github.com/freefrank/LostOdysseyRecomp/blob/v0.4.1/docs/notes/shadow-texture-lod.md) and [capture format](https://github.com/freefrank/LostOdysseyRecomp/blob/v0.4.1/docs/notes/render-state-capture.md).

#### 简体中文

修复已报告的 Map3 轮胎在启用 TAA 时阴影闪烁的问题，并改进用于画面问题反馈的 F1 渲染导出。

##### 改动

- 对齐已核对的场景深度、不透明材质和补光层的 TAA 偏移，修正阴影位置重建并保留深度采样。
- 连续捕获三帧并合并为一个 ZIP，各帧保留渲染诊断和原始深度，共享着色器。默认省去逐绘制预览及重复的截图 PPM／深度 `.f32`；`LO_DEBUG_CAPTURE_DRAW_STEPS=1` 可恢复逐绘制预览。
- 渲染导出附带当前进程日志快照、日志可用状态、源码版本及图形配置；日志不可用时仍保留渲染数据。
- 增加针对性 CPU jitter 回归和可选的已提交绘制诊断。

##### 验证与边界

r2 开发候选通过整合构建、8,192 项 CPU jitter 检查、场景检查和同位置 Map3 TAA／Off 实跑。用户确认原轮胎位置启用 TAA 后不再闪烁。独立标题／菜单实跑验证了三帧导出内容及导出后继续渲染。

TAA 仍为实验功能，其他地图、运动场景和硬件列为回归；独立 AMD 报告继续挂起。捕获改进不代表修复 AMD 画面问题或改善游戏运行性能。

详见[阴影修复证据](https://github.com/freefrank/LostOdysseyRecomp/blob/v0.4.1/docs/notes/shadow-texture-lod.md)和[捕获格式](https://github.com/freefrank/LostOdysseyRecomp/blob/v0.4.1/docs/notes/render-state-capture.md)。

### [v0.4.0](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.4.0) — 2026-09-07

#### English

Real internal resolution up to 4K, new anti-aliasing options and faster indexed shader discovery for both supported editions.

##### Changes

- **Internal resolution:** Auto follows output up to 3840×2160; manual 720p/1080p/1440p/2160p settings are independent of output resolution. Graphics changes support preview, timeout rollback and Keep.
- **AA and text:** add SMAA 1x and experimental camera-based TAA, with Standard/High spatial filtering. Settings text renders at output resolution; supported scenes receive AA before UI without repeating it afterward. Correct false TAA history rejection at the camera projection boundary.
- **Frame rate:** add saved 30/60 FPS controls and correct host pacing. Selected movement, dialogue, menu and Ring core-timing checks passed.
- **Shader discovery:** automatically match Asian and USA/Europe resource indexes, read only required CPX blocks for known layouts and show clearer preparation progress. Both editions preserve the full-scan source set; unknown layouts retain fallback and `LO_SHADER_FULL_SCAN=1` enables strict rescanning.
- **Debug and diagnostics:** add independent English/Simplified Chinese Debug menu switching, restore two missing indirect-call entries found during Issue #5 investigation, and report the failing operation, original OS error and memory context for startup allocation failures.
- **Development tools:** separate selected test suites and path-filtered CI from release packaging, and document DLSS/FSR feasibility. Remove unimplemented DLSS/frame-generation controls from Settings.

##### Validation and limits

Local builds, selected CPU/GPU checks and bounded Map2 runs on both audited editions passed. These checks do not establish complete-playthrough compatibility or new player visual acceptance.

- TAA remains experimental and lacks native object-motion vectors; unsupported paths use SMAA. DLSS, FSR and frame generation are not implemented.
- 60 FPS is not guaranteed throughout the game; precise Ring release/Perfect and broader gameplay need more coverage. The unvalidated 120 FPS option requires `LO_EXPERIMENTAL_120=1`; otherwise it runs at an effective 60 FPS.
- Existing translated-shader caches rebuild after updating. Two known shader failures and first-use stalls remain; fast discovery does not check all unread resource content. Some render targets and the legacy CPU readback path retain native sizing.
- Issue #5's original battle and Issue #6's reporting machine have not been retested. Their recovery is unconfirmed; these changes do not close either issue.

See [development evidence](https://github.com/freefrank/LostOdysseyRecomp/blob/v0.4.0/docs/notes/v0.4.0-development.md) and [follow-up validation](https://github.com/freefrank/LostOdysseyRecomp/blob/v0.4.0/docs/notes/handoff-v0.4.0-followup.md). Report problems through [GitHub Issues](https://github.com/freefrank/LostOdysseyRecomp/issues).

#### 简体中文

新增最高 4K 的真实内部分辨率、抗锯齿选项，以及支持两种版本的快速索引着色器发现。

##### 改动

- **内部分辨率：**Auto 跟随输出，最高 3840×2160；手动 720p／1080p／1440p／2160p 与输出分辨率独立，支持图形设置预览、超时回退和 Keep 保存。
- **抗锯齿与文字：**增加 SMAA 1x、实验性相机重投影 TAA，以及标准／高质量空间滤波。设置页文字按输出分辨率绘制，支持的场景在 UI 前抗锯齿并跳过后续重复处理；修正相机投影边界导致的 TAA 历史误拒绝。
- **帧率：**增加可保存的 30／60 FPS 控制并修正宿主帧节奏；已通过选定移动、对白、菜单和 Ring 核心计时检查。
- **着色器发现：**自动匹配亚洲／美欧资源索引，已知布局的 CPX 仅读取所需块，并明确显示准备阶段。两版来源集合均与完整扫描一致；未知布局保留回退，`LO_SHADER_FULL_SCAN=1` 可启用严格重扫。
- **调试与诊断：**增加独立英文／简体中文 Debug 菜单切换，恢复 Issue #5 调查中发现的两个缺失间接调用入口，并为启动分配失败记录失败操作、原始 OS 错误和内存上下文。
- **开发工具：**将按需测试套件及按路径触发的 CI 与发布打包分离，记录 DLSS/FSR 可行性研究；从设置中移除尚未实现的 DLSS／帧生成控件。

##### 验证与限制

本地构建、选定 CPU／GPU 检查及两个已核对版本的限定 Map2 实跑通过；这些结果不代表完整通关兼容性或新增玩家画质验收。

- TAA 仍为实验功能，缺少原生对象运动矢量，不支持的路径使用 SMAA；DLSS、FSR 和帧生成尚未实现。
- 不保证全游戏锁定 60 FPS；精准 Ring 释放／Perfect 和更广流程仍待覆盖。未验证的 120 FPS 选项需要 `LO_EXPERIMENTAL_120=1`，否则实际按 60 FPS 运行。
- 更新后旧翻译着色器缓存会重建。两个已知着色器失败及首次使用卡顿仍可能存在；快速发现不会校验全部未读取资源内容，部分渲染目标及旧 CPU 回读路径保留原生尺寸。
- Issue #5 原报告战斗和 Issue #6 原报告机器尚未复测，未确认故障恢复，本次改动不代表关闭这两个问题。

详见[开发证据](https://github.com/freefrank/LostOdysseyRecomp/blob/v0.4.0/docs/notes/v0.4.0-development.md)与[后续验证](https://github.com/freefrank/LostOdysseyRecomp/blob/v0.4.0/docs/notes/handoff-v0.4.0-followup.md)，请通过 [GitHub Issues](https://github.com/freefrank/LostOdysseyRecomp/issues) 反馈问题。

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
