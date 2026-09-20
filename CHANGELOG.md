# Changelog / 更新日志

One record of completed changes, with unpublished work separated from verified releases. Dates below are UTC release dates. Planned work belongs in the [roadmap](docs/ROADMAP.md), not release entries.

本文统一记录已完成改动，并区分未发布内容与已确认发布版本；日期采用 UTC 发布日期。后续计划见[路线图](docs/ROADMAP.zh-CN.md)，不作为已发布功能记录。

## Unreleased / 未发布

### English

- Implemented P0 foundation for native NVIDIA DLSS Super Resolution (SR) on the development branch:
  - Added optional build support for pinned official NVIDIA DLSS SDK `310.9.1` (`374959484e79a640feaba44c93ac8cfb0a03f5b5`) using static CRT bootstrap libraries on Windows and static library on Linux.
  - Implemented Plume Vulkan bridge extension hooks and external command boundaries (`VulkanExtensionHooks`, `VulkanExtensionStatus`, `beginExternalCommands`, and `endExternalCommands`).
  - Added standalone and integrated capability probes (`LoNativeDlssProbe`, `LoNativeDlssReportTest`) verifying NGX feature discovery, Vulkan instance/device extension negotiation, capability parameters, and optimal input resolution queries.
  - P0 validation gate passed on local hardware (NVIDIA RTX 5080, driver 616.56, reporting optimal inputs for 1080p: Quality 1280×720, Balanced 1114×626, Performance 960×540). Controlled injection of missing runtime DLL verified expected standalone negative probe exit (exit 1).
  - Frame Generation (FG) remains deferred, and Super Resolution evaluation (`NGX_VULKAN_EVALUATE_DLSS_EXT`) and game output integration are not yet implemented (P1/P2). Proprietary licensing for SDK-on binary distribution remains unresolved, and no binary packages are released.

### 简体中文

- 在开发分支实现原生 NVIDIA DLSS 超分辨率（SR）P0 基础设施：
  - 增加对固定版本 NVIDIA 官方 DLSS SDK `310.9.1`（commit `374959484e79a640feaba44c93ac8cfb0a03f5b5`）的可选构建支持，Windows 采用静态 CRT 引导库，Linux 采用静态库。
  - 实现 Plume Vulkan 桥接扩展钩子与外部命令流边界（`VulkanExtensionHooks`、`VulkanExtensionStatus`、`beginExternalCommands` 与 `endExternalCommands`）。
  - 新增独立与集成能力探测程序（`LoNativeDlssProbe`、`LoNativeDlssReportTest`），验证 NGX 特征发现、Vulkan 实例与设备扩展协商、能力参数读取及推荐输入分辨率查询。
  - P0 验证门禁在本地设备通过（RTX 5080，驱动 616.56，1080p 目标下查询到 Quality 1280×720、Balanced 1114×626、Performance 960×540）。受控注入缺失运行库测试确认了独立探测程序的预期负向退出（exit 1）。
  - 帧生成（FG）保持暂缓，SR 执行求值（`NGX_VULKAN_EVALUATE_DLSS_EXT`）与游戏渲染链输出接回尚未实现（属于 P1/P2 阶段）。包含 SDK 的二进制分发许可尚待确定，不发布二进制包。

## v0.6.7 — 2026-09-20

### English

- Add in-game Graphics menu Widescreen switch and expanded 21:9 resolution presets (Issue #17):
  - In Settings -> Graphics, a new **Widescreen** toggle appears immediately above **Output resolution**.
  - When Widescreen is **Off**, Output resolution offers five standard 16:9 tiers: 1280×720, 1600×900, 1920×1080, 2560×1440, and 3840×2160.
  - When Widescreen is **On**, Output resolution offers five common 21:9 ultrawide tiers: 1720×720, 2560×1080, 3440×1440, 3840×1600, and 5120×2160.
  - Toggling aspect ratio maps to the nearest preset by vertical height, choosing the higher tier in equidistant ties (e.g. 900p maps to 1080p).
  - The switch state is derived dynamically from configured width and height without adding new INI keys; existing 3440×1440 configurations automatically open with Widescreen enabled.
  - Saving settings applies the chosen resolution and commits it to disk; exiting without saving preserves existing configuration. First-launch setup resolution choices are also updated to include matching presets. Localized terms and help descriptions are provided in English, Japanese, Korean, Traditional Chinese, and Simplified Chinese.
  - Validation includes `menu_flow_test` covering initial 3440×1440 auto-derivation, 5 ultrawide tiers cycling, height-preserving 2160p toggle back to 16:9, cancel discard, and Save display change state machine; `menu_render_test` layout verification (`out/snapshots/menu_1280x720_21_9.png`); and incremental runtime build verification.
  - In local runtime testing, the user confirmed functionality is working as expected and authorized closing Issue #17. Ultrawide support remains experimental across diverse hardware and aspect ratio combinations.

### 简体中文

- 游戏内图形设置增加“宽屏”开关并扩充 21:9 分辨率预设（Issue #17）：
  - 在“设置” -> “图形”中，“输出分辨率”上方新增**宽屏**切换开关。
  - 宽屏为**关**时，输出分辨率提供 5 档标准 16:9 选项：1280×720、1600×900、1920×1080、2560×1440 与 3840×2160。
  - 宽屏为**开**时，输出分辨率提供 5 档常见 21:9 超宽屏选项：1720×720、2560×1080、3440×1440、3840×1600 与 5120×2160。
  - 切换比例时按高度差最近匹配目标档位，等距时选取较高档位（例如 900p 切换至 1080p）。
  - 开关状态直接由当前配置的宽度与高度推导，不增加额外 INI 字段；原有 3440×1440 配置会自动识别并打开宽屏。
  - 保存图形设置将应用新分辨率并写入磁盘；取消或返回则不改动配置。首次启动设置向导的分辨率列表中也同步补齐对应预设。已适配英语、日语、韩语、繁体中文与简体中文 5 种语言的词条与提示文案。
  - 验证覆盖：`menu_flow_test` 验证了初始 3440×1440 自动推导、五档循环、关闭时保留 2160 高度、取消／重开恢复及 Save 显示状态机；`menu_render_test` 离线渲染（`out/snapshots/menu_1280x720_21_9.png`）验证了 10 行菜单布局；主程序增量构建通过。
  - 在本地构建实机测试中，用户已明确确认功能正常并授权关闭 Issue #17。超宽屏支持在多样化硬件与多分辨率组合下仍保持实验性。

## v0.6.6 — 2026-09-20

### English

- Add initial native ultrawide (21:9) support (Issue #17): render targets follow the native display aspect ratio using Hor+ projection adjustments applied before derived matrices and view-frustum culling, keeping 16:9 safe-region HUD and ordered left/right pillarbox bars during video playback. The frame plan manages queue epoch tracking and render-target catalog roles.
- Fix shadow-map rendering across all aspect ratios and high internal resolutions by correcting effective-height render target caching to avoid unnecessary 640×640 recreation and updating depth rasterization without color writes to cover modes 4 and 5 while preserving `SV_Depth` and alpha. In user testing of the affected scene, shadows were confirmed fixed.
- Note on same-version reissue: v0.6.6 was reissued on 2026-09-20 from source commit `c6cbd1f62414a46c00c6312559edcd8d217bc9ee` via Release CI [35527543573](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35527543573) to include this shadow fix. Initial published v0.6.6 packages (commit `c953bb5`) lacked this fix and are superseded; redownload the release packages to obtain the fix.
- **User note**: Ultrawide support remains EXPERIMENTAL and currently ONLY 3440×1440 is supported (2560×1080 is not currently advertised or supported despite appearing as an unverified UI option).
- Focused checks passed for `windows-clang` runtime build, `LoFramePlanTest` (18 checks), `LoTargetMappingTest` (7 checks), resolution (40 checks) and temporal math fixtures; in Vulkan testing with packed shaders, a native save loaded at 13.93s, captured two frames during scene transition at swaps 382–383, and verified 3440×1472 padded color/depth allocations with 3440×1440 resolve content. Comprehensive visual Hor+, HUD positioning, dynamic resize, broader scene shadow validation, failure injection, other backends, and full player acceptance remain pending.
- On Linux, the AppImage updater removes the exact temporary previous AppImage after the replacement reaches normal startup, while preserving it when replacement execution fails so rollback remains available. The focused WSL Manjaro Linux restart/rollback regression passed. Packaged AppImage, Fedora desktop and user acceptance remain pending.

### 简体中文

- 增加原生超宽屏 (21:9) 初始支持（Issue #17）：渲染目标依原生显示比例分配，在派生矩阵与视锥裁剪前应用 Hor+ 投影调整，并在视频播放期间保持 16:9 HUD 安全区与有序左右立柱黑边。FramePlan 负责队列周期跟踪与目标分类角色管理。
- 修复全显示比例与高内部分辨率下的阴影贴图（shadow map）渲染：修正 effective-height 渲染目标缓存以避免不必要的 640×640 重新创建，并在无颜色写入的深度光栅化中覆盖模式 4 与 5，同时保留 `SV_Depth` 与 alpha。受影响场景经用户实机测试确认阴影已恢复正常。
- 同版本重新发布说明：v0.6.6 已于 2026-09-20 从 source commit `c6cbd1f62414a46c00c6312559edcd8d217bc9ee` 通过 Release CI [35527543573](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35527543573) 完成同版本重新发布，以纳入该阴影修复。初版已发布的 v0.6.6 资产（commit `c953bb5`）不包含该修复并已被替代；请重新下载发布包以获取修复。
- **用户提示**：超宽屏支持仍处于**实验性（EXPERIMENTAL）**阶段，目前**仅支持 3440×1440**（界面虽有 2560×1080 选项但尚未验证支持，请勿作为受支持分辨率使用）。
- `windows-clang` 运行时构建、`LoFramePlanTest`（18 项检查）、`LoTargetMappingTest`（7 项检查）、分辨率（40 项）与时序数学测试均已通过；在 Vulkan 搭配打包着色器测试中，原生存档于 13.93 秒成功载入，在场景过渡期间捕获 swap 382–383 的两帧，日志确认 3440×1472 对齐的色彩／深度缓冲分配及 3440×1440 resolve 画面。完整实机 Hor+ 视觉呈现、HUD 排布、动态调整大小、更广场景阴影验证、故障注入、其他图形后端及完整玩家验收仍待完成。
- Linux AppImage 更新器会在替换版本正常启动后删除对应的临时旧版 AppImage；如果替换版本执行失败，则保留旧版 AppImage 以便回滚。定向 WSL Manjaro Linux 重启／回滚回归已通过；打包 AppImage、Fedora 桌面和用户验收仍待完成。

## v0.6.3 — 2026-09-19

### English

- Revert large vertex-cache hits to bounded sampled content comparison to prioritize draw-time CPU cost. Small vertex buffers remain exact; large buffers compare the head and tail plus strided samples, while index-cache hits retain exact source-byte verification. The focused `LoVertexCacheTest` passed 3,668,957 checks, including sampled changes, small-buffer exactness and the selected large-vertex blind-spot policy. The synthetic blind spot is known; no known game bug has been caused by sampling, and no game or release-binary performance result is claimed. A reliable low-cost vertex invalidation mechanism remains backlog work.
- Harden Issue #54 language-menu selection: invalid language-table counts and indices no longer read or rewrite a selection, and unavailable entries display as `—` instead of fabricating a language. The menu capacity follows the native parser limit (16 entries), while the existing USA/Europe `82481BE8` host-language mapping and independent text/voice handling remain unchanged. An opt-in, bounded `LO_TRACE_LANGUAGE=1` trace is available for future reports, including the menu apply/close boundary snapshot. The specific cause of the reported cutscene voice issue remains unconfirmed; no save was available and no real-game reproduction was performed.
- Apply a defensive file-I/O locking fix for Issue #53: read, write and scatter paths keep the per-file mutex only while updating seek, transfer, position and size state, then publish completion results through the retained handle reference. The original Disc 2 hang was not reproduced, so its root cause remains unconfirmed.
- Add opt-in, bounded I/O diagnostics for future Issue #53 reports, including file-handle lifecycle, mutex wait/acquisition, transfer, completion publication and API-return stages, plus manual JSONL snapshots.
- Add deterministic guest I/O lifetime, APC/event ordering, independent-file and duplicate-handle regression coverage, and reuse the multi-disc checks on Windows and Linux. These checks do not establish a story transition or player acceptance.
- Use the platform archive format for asynchronous F1 render-state exports: Windows produces `.zip`, while Linux uses the system `tar` and `gzip` tools to produce `.tar.gz`. Focused WSL Manjaro g++ C++20 `-Wall -Wextra -Werror` checks pass for byte-identical 8 MiB/log extraction, asynchronous preparation, archive collisions, missing `tar`, unreadable sources, symlink-root rejection and shutdown joining. AppImage runtime and in-game/user acceptance remain pending.

Published at [GitHub Release v0.6.3](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.3) on 2026-09-19T23:56:08Z from tag/source commit `93bdbc1ccae7652e38dc80db24a9d25a34a72a47`. Release CI [35476569158](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35476569158) passed Windows and Linux packaging on its first attempt. The four public assets are the Windows ZIP and Linux AppImage plus their `.sha256` sidecars; both package hashes match their sidecars and GitHub digests, and public sidecars returned HTTP 200. The Windows ZIP is 211,084,312 bytes with SHA-256 `9737befe09bc011c17327d5a83df8e437ab7e03a0f107b4acf40621dfd5fee08`; the Linux AppImage is 220,813,816 bytes with SHA-256 `b4f0448e6e661f51a2cab07209289188c786710929e4c4334f09786f125e7ed7`. The Windows manifest reports version/source version `0.6.3`, commit `93bdbc1` and `dirty=false`; all 49 payload hashes and the embedded shader were verified. Linux native GPU, Steam Deck, AppImage runtime and broader gameplay remain unverified.

### 简体中文

- 为优先降低绘制阶段 CPU 成本，将大顶点缓存命中恢复为有界 sampled-content 比较。小顶点缓冲仍逐字节精确比较；大缓冲比较头尾片段和跨区采样；index cache 命中继续保留完整源字节校验。定向 `LoVertexCacheTest` 通过 3,668,957 项检查，覆盖采样变化、小缓冲精确性和已选择的大顶点采样盲区策略。已知存在合成盲区；目前没有任何已知游戏 bug 由采样引起，也没有据此宣称实机或发布二进制性能结果。可靠且低成本的顶点失效机制仍列入 backlog。
- 加固 Issue #54 的语言菜单选择：语言表数量或索引无效时不再读取或改写选择，无效条目显示为 `—`，避免伪造语言；菜单容量遵循原生解析器的 16 项限制。既有 USA/Europe `82481BE8` 宿主语言映射以及文字／配音独立处理保持不变。新增默认关闭且有界的 `LO_TRACE_LANGUAGE=1` 追踪，包含 menu apply/close 边界快照，供后续报告使用。具体过场配音问题的根因尚未确认；没有存档，本次未做实机复现。
- 为 Issue #53 应用防御性文件 I/O 锁范围修正：读取、写入和 scatter 路径只在更新 seek、传输、位置和大小状态时持有文件互斥锁，随后通过保留的句柄引用发布完成结果。原 Disc 2 卡住未能复现，根因仍未确认。
- 增加默认关闭且有界的 I/O 诊断，记录文件句柄生命周期、互斥锁等待／取得、传输、完成发布和 API 返回阶段，并支持手动导出 JSONL 快照。
- 在 Windows 与 Linux 补充真实客户机 I/O 生命周期、APC／event 顺序、独立文件和复制句柄回归覆盖，并复用多盘测试。这些检查不代表已完成剧情换盘或玩家验收。
- 为异步 F1 渲染状态导出使用平台归档格式：Windows 生成 `.zip`，Linux 使用系统 `tar` 和 `gzip` 生成 `.tar.gz`。WSL Manjaro g++ C++20 `-Wall -Wextra -Werror` 定向检查已通过，覆盖 8 MiB／日志逐字节一致解包、异步 prepare、归档冲突、缺少 `tar`、源文件不可读、拒绝符号链接根目录和退出时 join。AppImage 运行时及游戏内／用户验收仍待完成。

v0.6.3 已于 2026-09-19T23:56:08Z 从 tag/source commit `93bdbc1ccae7652e38dc80db24a9d25a34a72a47` 发布到 [GitHub Release](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.3)。Release CI [35476569158](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35476569158) 首次通过 Windows/Linux 打包。四个公开资产为 Windows ZIP、Linux AppImage 及各自 `.sha256` sidecar；两个主体包 hash 与 sidecar 和 GitHub digest 一致，公开 sidecar 返回 HTTP 200。Windows ZIP 大小为 211,084,312 字节，SHA-256 为 `9737befe09bc011c17327d5a83df8e437ab7e03a0f107b4acf40621dfd5fee08`；Linux AppImage 大小为 220,813,816 字节，SHA-256 为 `b4f0448e6e661f51a2cab07209289188c786710929e4c4334f09786f125e7ed7`。Windows manifest 报告版本／source version 为 `0.6.3`、commit `93bdbc1`、`dirty=false`；49 个 payload hash 和内置 shader 均已核验。Linux 原生 GPU、Steam Deck、AppImage 运行时和更广游戏流程仍未验证。

## v0.6.2 — 2026-09-19

### English

- Apply the accepted Uhra TAA policy to the normal TAA path: 0.5 jitter scale, stationary motion snapping, stationary color clipping and multi-surface history, with RGBA8 history at `31/33`. Experimental FP16 history and moving bilinear fallback remain off.
- Enable experimental geometric motion-vector replay for TAA by default, while retaining `LO_MV_ENABLE=0` as a comparison switch. Repeated instances are paired by stable submission order so adjacent-frame Nth-to-Nth motion matching remains intact.
- Reuse exact-content index fingerprints in the index cache while retaining complete source-byte verification. In the same Uhra Vulkan 4K internal/output scene on an RTX 5080, hidden muted A-B-A-B captures without pacing measured 60.34/59.00 FPS for the candidate, versus 54.61 FPS for a separate Release build and 54.57 FPS for the former RelWithDebInfo main binary.
- Validation includes MV audit steady tracked/matched/replay 988 with failed 0, `LoMotionVectorTest` 71 checks and `LoVertexCacheTest` 3,668,948 checks. User foreground review of the same Uhra steel-frame scene found image quality acceptable at about 60 FPS.
- Scope remains bounded to the tested Uhra scene, Vulkan and the local RTX 5080. The 1080p-internal to 4K moving-camera limitation, broader scene coverage and D3D12 replay PSO creation follow-up remain open.

### 简体中文

- 将已接受的 Uhra TAA 策略应用到正常 TAA 路径：0.5 抖动幅度、静止运动 snap、静止颜色裁剪和多表面 history，使用 RGBA8 history 与 `31/33` 权重。实验性 FP16 history 和 moving bilinear fallback 仍关闭。
- TAA 默认启用实验性几何运动矢量 replay，保留 `LO_MV_ENABLE=0` 对照开关。重复实例继续按稳定提交顺序配对，保持相邻帧 Nth-to-Nth 运动匹配。
- index cache 复用 exact-content index fingerprint，同时保留完整源字节验证。在 RTX 5080 的 Vulkan、Uhra 4K 内部／输出同一场景中，隐藏静音、无 pacing 的 A-B-A-B 对照测得候选 60.34/59.00 FPS；独立 Release 构建为 54.61 FPS，之前的 RelWithDebInfo 主程序为 54.57 FPS。
- 验证包括 MV audit steady tracked/matched/replay 为 988、failed 为 0，`LoMotionVectorTest` 71 项和 `LoVertexCacheTest` 3,668,948 项。用户在同一 Uhra 钢架场景前台观察，确认画质可接受、约 60 FPS。
- 范围限定为已测试的 Uhra 场景、Vulkan 和本机 RTX 5080。1080p internal 到 4K output 的移动相机限制、更广场景覆盖和 D3D12 replay PSO 创建后续工作仍开放。

Published at [GitHub Release v0.6.2](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.2) on 2026-09-19T21:16:38Z from tag/source commit `7f99786f302b4ef3e5f672eacdba2b7a62972fda`. Release CI [35467796768](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35467796768) succeeded on its second attempt for Windows/Linux Release packaging. The final public delivery has four assets: the Windows ZIP, Linux AppImage and their `.sha256` sidecars; both platform packages include the shader set, with no separate shader package. The Windows ZIP SHA-256 is `99495f62315f44bfa1eb34ce294b8b08c9e173193e86482c2dfa4427b10963c7`; the Linux AppImage SHA-256 is `a4542b8eeee6b5ac27f4dc8e4e8f0ec184e1f0c7620e8846429a1455b3942ecd`. The Windows manifest reports version/source version `0.6.2`, commit `7f99786`, and `dirty=false`. The package hashes match their sidecars and public sidecars returned HTTP 200. Linux native GPU, Steam Deck and broader gameplay remain unverified.

## v0.6.1 — 2026-09-18 / Published / 已发布

### English

- Check for updates before importing game data on Windows and Linux. Up-to-date and offline checks continue normally; headless and background runs skip the automatic UI.
- When a newer release is available, an app-branded prompt shows its release notes with **Install** and **Later** actions. Keyboard and mouse input are supported, with controller input wired through the same prompt; accepting applies the update and relaunches before import. Download progress remains in the existing updater window.
- Windows runtime build, focused Windows/Linux prompt tests and changed Linux syntax checks pass. Live update acceptance, physical controller input and network downloading remain unverified.

### 简体中文

- Windows 和 Linux 现在会在导入游戏资料前检查更新。版本已是最新或无法联网时继续正常流程；无头和后台运行会跳过自动界面。
- 发现新版本时，带有应用品牌的提示会显示发布说明以及“安装”和“稍后”操作。提示支持键盘和鼠标，手柄输入也接入同一提示；接受后在导入前应用更新并重新启动。下载进度仍使用现有的更新器窗口。
- Windows 运行时构建、Windows/Linux 更新提示专项测试和修改后的 Linux 语法检查已通过。在线更新接受、实体手柄输入和网络下载仍未验证。

Published at [GitHub Release v0.6.1](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.1) from `ebd2ef13969a28fcabf26a4eb2dafa9c09ca965d`. Release CI `35374267882` passed the Windows and Linux release jobs and their focused regressions. The Windows ZIP, Linux AppImage and standalone shader pack SHA-256 values are `fb0fdfc823c53515eac300d7596c7bfe98127402ba90476dd093b8c82e80c689`, `5ef83615d4eb16922e51f74ebaf6002aedd51bbb1bb2eb3dd19964a79317b47c` and `387a23b9328b8136847d48b37b574fddd600a526eb837807fbb08b758c6de4d9`; public sidecars returned HTTP 200.

已从 `ebd2ef13969a28fcabf26a4eb2dafa9c09ca965d` 发布到 [GitHub Release v0.6.1](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.1)。Release CI `35374267882` 的 Windows/Linux 发布任务及两平台定向回归均通过。Windows ZIP、Linux AppImage 和独立 shader pack 的 SHA-256 分别为 `fb0fdfc823c53515eac300d7596c7bfe98127402ba90476dd093b8c82e80c689`、`5ef83615d4eb16922e51f74ebaf6002aedd51bbb1bb2eb3dd19964a79317b47c` 和 `387a23b9328b8136847d48b37b574fddd600a526eb837807fbb08b758c6de4d9`；公开 sidecar 均返回 HTTP 200。

## v0.6.0 — 2026-09-18 / Published / 已发布

### English

- 0.6.0 prerelease audit repairs: atomic multi-object waits, reference-counted
  kernel handles and non-blocking thread close with independently owned worker state.
- Retry transient shader/module failures with bounded backoff; preserve permanent
  negative compilation results. Cancel every eager shader/PSO preparation phase.
- Preserve presentation resources on allocation/map failure, resolve display
  transactions, and synchronize backend selection with window policy updates.
- Compare vertex cache content exactly with a bounded CPU snapshot budget; no
  per-draw cryptographic hashing. Honor serial overrides and Linux memory limits.
- Repair the 0.6.0 audit findings in the geometry, benchmark, clock and updater
  paths: large vertex/index cache hits compare complete source content; the
  index cache has a 64 MiB payload budget and evicts or bypasses entries under
  pressure; `tools/drive_city.py --dry-run` is read-only and protects overlapping
  save paths; guest PPC timebase now uses the same pause-aware high-resolution
  clock as the active game clock; Linux update apply removes completed staging
  data and restores the previous AppImage when the replacement cannot `execv`;
  standalone Windows recovery copies its runner from the helper itself.
- Harden importer publication: disc resources and `import-info.json` now require
  successful write, flush and close operations before staged data is published;
  any finalization failure aborts the transaction without rereading the full
  resource. Align XDVDFS signature scanning to 2048-byte boundaries and reuse
  the identity-verified reader during installation while retaining the final
  identity check. The destination browser can create a uniquely named folder
  with the button, `F2`, or destination-page controller `Y`, edit its name,
  enter and select it automatically, and report collisions, permission
  failures and read-only destinations without starting an import.
- Share the portable shader runtime contract with the release verifier; pin shader
  inputs and gate releases on native/portable pack regressions. Guard x86 compiler
  flags by target architecture. These changes do not certify gameplay or Deck FPS.
- GPU index conversion cache (`LostOdysseyRecomp/gpu/renderer.cpp`, `LostOdysseyRecomp/gpu/vertex_cache.h`): cache post-expansion indices for large buffers (`count >= 256`) keyed by extent plus conversion parameters and validated against exact source content. The cache now has a bounded 64 MiB payload budget and reports bytes, peaks and evictions. Previous 15W city measurements used sampled matching and do not carry over as performance evidence after the correctness repair; remeasure with the final release binary. Extended `tools/tests/vertex_cache_test.cpp` with key-participation, hit/miss, exact-mutation and bounded-churn checks.
- Build: `EXPORT_COMPILE_COMMANDS` enabled and `LO_BOLT_READY` retained so Clang builds stay BOLT-ready (`emit-relocs` + line tables, baseline-neutral).

### 简体中文

- 0.6.0预发布审计修复：原子化多对象等待、句柄引用管理及不阻塞的线程句柄关闭。
- shader暂时失败可退避重试，确定性失败保留负缓存；取消覆盖shader和PSO准备各阶段。
- 呈现资源分配或映射失败保留旧资源并结束显示事务，同步后端选择与窗口策略。
- 顶点缓存采用有内存上限的精确内容比较，不增加逐draw加密哈希；修正串行优先级及Linux内存检测。
- 修复0.6.0审计发现的几何、基准工具、时钟和更新器问题：大顶点／索引缓存命中完整比较源内容；索引缓存增加64 MiB有效载荷上限并在压力下淘汰或绕过；`tools/drive_city.py --dry-run` 改为只读并拒绝重叠存档路径；PPC timebase 与感知暂停的高精度游戏时钟统一；Linux 更新成功后清理暂存，替换后无法 `execv` 时恢复旧 AppImage；Windows 独立恢复使用 helper 自身作为 runner。
- 加固导入器发布流程：光盘资源与 `import-info.json` 只有在 `write`、`flush`、`close` 均成功后才发布暂存数据；最终写入失败会中止事务，不重复读取全部大文件。XDVDFS 签名扫描改为按 2048 字节边界前进，安装阶段复用已完成身份校验的读取器，同时保留最终身份复核。目标目录页支持通过按钮、`F2` 或目标页手柄 `Y` 创建唯一默认名称的文件夹，创建后自动进入并选中；支持改名，并明确报告重名、权限失败和只读目录，创建不会自动开始导入。
- 发布工具共享runtime的shader兼容契约，固定输入版本，发布前执行回归；按目标架构限定x86编译参数。
  本轮改动不代表已经通过游戏全流程或Steam Deck帧率验收。
- GPU索引转换缓存（`LostOdysseyRecomp/gpu/renderer.cpp`、`LostOdysseyRecomp/gpu/vertex_cache.h`）：对大缓冲（`count >= 256`）按范围和转换参数缓存展开后的索引，并对源内容进行完整比较。缓存增加64 MiB有效载荷上限，超限时淘汰或绕过，并记录字节数、峰值和淘汰次数。此前15W乌斯拉进城数据基于抽样比较，正确性修复后不再作为当前性能证据；应使用最终发布二进制重新测量。`tools/tests/vertex_cache_test.cpp`新增键参与度、命中/失效、完整变异和有界抖动检查。
- 构建：启用`EXPORT_COMPILE_COMMANDS`，保留`LO_BOLT_READY`使Clang构建持续BOLT就绪（`emit-relocs` + 行号表，基线代价中性）。

## v0.5.20 — 2026-09-17 / Published / 已发布

### English

- Fix Issue #38 inverted/black light fixtures ("anti-lights emitting darkness") in Numara Castle (Philosopher's Chamber):
  - In `LostOdysseyRecomp/gpu/renderer.cpp` and `LostOdysseyRecomp/gpu/shader/xenos_translator.cpp`, correct the host EDRAM epilogue clamping for unsigned formats (formats 0, 1, 2, 3, 10, 12, including 7e3 `COLOR_2_10_10_10_FLOAT`). Clamping lower bound is now strictly `0.0` for unsigned targets, preventing additive blending passes from accumulating negative light and tone-mapping `log2` from triggering NaNs / black voids.
  - Bump shader cache `Version` from 22 to 23 in `cache.h` to invalidate stale DXIL binaries.
- Fix TAA flicker on stairs and save point in `f2358`:
  - Register missing static scene and lighting vertex shaders (`0x69e9adcf2e1b6887`, `0x6a8c2c78737dc94c`, `0xa20d6099a44e2cd5` to Slot 7 and `0x6761469677f921c6` to Slot 8) in `PositionVPSlot` (`LostOdysseyRecomp/gpu/temporal_scene.h`), eliminating inter-frame phase jitter artifacts.
- Optimize shader and pipeline prebuilding concurrency and throughput:
  - Dynamically scale concurrent DXC and pipeline workers based on host physical memory and CPU thread count (`HostWorkerCap`). Hosts with >= 8 GB RAM automatically use `logicalThreads - 1` workers, while low-memory environments (< 8 GB RAM) retain a 4-worker safety cap to prevent OOM.
  - Support explicit worker concurrency overrides via environment variables `LO_SHADER_WORKERS` and `LO_PIPELINE_WORKERS` (accepting numeric counts, `0`, or `max`).
  - Eliminate `MOVEFILE_WRITE_THROUGH` forced disk flushes on Windows shader checkpoint saves, allowing OS write-caching to accelerate compilation throughput.
  - Prioritize primary game and executable shaders ahead of predictive expansion variants during preparation, ensuring essential scene shaders compile first.
  - Add interactive skip support to the shader preparation screen (press ESC, Space, or Controller B to skip remaining compilation and start the game immediately).
  - Add `skip_shader_prebuild` configuration setting in `settings.ini` to permanently bypass startup compilation on low-end systems.
  - Add `tools/release/package_shader_bundle.py` utility to package precompiled startup bundles into release archives for distribution without Git LFS.

- Add portable Vulkan shader pack distribution architecture (`.lospv`):
  - Introduce independent, read-only `.lospv` distribution artifact for successful SPIR-V binaries and essential `TranslatedShader` metadata, completely stripping HLSL sources, diagnostics, and failure records.
  - Implement SHA-256 deduplication of SPIR-V bytecode and chunked Zstandard block compression (~1 MiB blocks) with single-block streaming cache, significantly reducing distribution size and startup RSS (169.9 MB package for 28,482 shaders).
  - Runtime module creation is performed lazily upon first use in rendering, bypassing startup discovery/translation/DXC overhead when a valid pack is present (verified 1.2s startup on WSL Linux with 0 DXC calls).
  - Decouple compatibility contract from host install path and host DXC DLL/SO SHA-256, allowing portable pack reuse across platforms and CPU architectures (x86-64 / ARM64, Windows / Linux / Steam Deck).
  - Add `LoShaderPackTool` inspection/verification utility, `tools/build_linux.sh` and `tools/build_wsl.bat` for fast on-demand incremental builds, and update packaging scripts (`package_shader_bundle.py`, `package_release.py`, `package_appimage.py`) to verify and package `.lospv` release assets.
- Remove PowerPC prebuilt library cache and remote synchronization (`LO_PREBUILT_PPC_DIR`, `ppc_sync.py`, `ppc_prebuilt.py`):
  - Compile `LostOdysseyRecompLib` guest PowerPC code directly from recompiled source on all platforms (Windows and Linux) in CI and local release builds, eliminating platform-specific static library caching and providing architecture portability for future targets (such as ARM64).
  - Automatically bundle the portable Vulkan shader pack (`shaders/portable_vk.lospv`) into both Windows portable ZIP and Linux AppImage release artifacts via `tools/release/fetch_shader_pack.py`.
- Keep extraction and fixed/linked shader sources in bounded memory instead of
  exporting and re-reading temporary sources during prebuild; retain compiled
  checkpoints for interrupted-startup recovery.
- Bind startup bundles to the current runtime/compiler contract, restore event
  pumping and transactional error propagation, and fix cancellation wakeups.
- Cap concurrent DXC preparation at four workers, release retained HLSL, and
  decode only indexed CPX blocks. Preserve explicit diagnostic/full-scan controls.
- Add game-data-free CPU/sanitizer and production-function regression coverage.
  See [audit scope, evidence and remaining hardware checks](docs/MENU_SHADER_PREBUILD_AUDIT.md).


- Menu branch fixes improve the in-game debug overlay across output resolutions and protect shared overlay state during concurrent access. The software UI now uses the intended RGBA channel order, HID locking covers the complete device operation, and guest pause uses cooperative safe points so overlay interaction does not suspend worker threads indefinitely.
- Addressed code review report items R1 through R10:
  - Decoupled host controller input pumping (`PumpHostInput`) and overlay presentation (`PresentHostOverlay`) from guest pause, and added SDL event pumping to GPU `WAIT_REG_MEM` loops (R1, R2).
  - Replaced atomic wait in `InterruptMain` with `std::condition_variable` and integrated `host_ui::RequestStop()` to guarantee cancellable shutdown without lost wakeups (R5a, R5b).
  - Implemented pause-aware active game clock (`GetActiveGameTimeMs`) and scene generation tracking for teleport and battle commands, preserving bookmarks and pending states across pauses, and provided accurate asynchronous status feedback in the debug overlay (R3, R9).
  - Enabled custom settings on Linux via renderer capability check (`LO_GPU_PLUME`), isolated settings buffer caching from debug overlay compositing, guarded mouse click propagation under modal overlays, and corrected straight-alpha source-over blending math (R4, R7, R8, R10).
  - Migrated `LoDebugMenuInteractionTest` to the host overlay model, covering navigation, state synchronization, and error handling (R6).
- Addressed third-round code review report items R11 through R17:
  - Synchronized `g_pixels` buffer, dimensions, and added exact-size validation in `SaveScreenshot` CPU fallback to eliminate out-of-bounds reads during menu presentation (R11).
  - Deduplicated keyboard navigation between window event pump and host HID polling by removing raw key polling from `PumpHostInput` (R12).
  - Implemented button release quarantine for overlay close actions (B button and LB+RB chord) to prevent consumed buttons from leaking to underlying settings and game simulation (R13).
  - Added throttled 16ms host overlay presentation inside GPU `PM4_WAIT_REG_MEM` loops to ensure UI refreshes even when CP waits on paused guest conditions (R14).
  - Propagated active presentation tickets from `DisplayChangeTracker` in `PresentHostOverlay` to properly resolve display change transactions (R15).
  - Protected overlay status feedback with minimum display durations so immediate command rejections are not overwritten by stale service snapshots (R16).
  - Configured platform-appropriate graphics backend choices and graceful manual-restart guidance on non-Windows platforms (R17).
  - Relocated `LoDebugMenuInteractionTest` into the cross-platform test suite in CMake.
- Maintained and expanded test suite: `LoHidTest`, `LoHostUiCompositeTest`, `LoDebugOverlayTest`, `LoDebugMenuInteractionTest`, and `LoMenuRenderTest` all built and passed.

### 简体中文

- 修复 Issue #38 努玛拉城哲学者之间（Philosopher's Chamber）黑光灯具问题：
  - 在 `LostOdysseyRecomp/gpu/renderer.cpp` 与 `LostOdysseyRecomp/gpu/shader/xenos_translator.cpp` 中修正着色器尾声对无符号 EDRAM 格式（格式 0、1、2、3、10、12，含 7e3 格式 `COLOR_2_10_10_10_FLOAT`）的物理范围截断。无符号目标下限严格截断至 `0.0`，杜绝加法光照混合通道写入负数及后续色调映射 `log2(负数)` 扩散至全屏的巨大黑洞。
  - 将 `cache.h` 中的着色器缓存版本号 `Version` 由 22 提升至 23，自动使磁盘旧版 DXIL 缓存失效。
- 修复 `f2358` 场景阶梯与保存点光球处的 TAA 抖动闪烁：
  - 在 `LostOdysseyRecomp/gpu/temporal_scene.h` 的 `PositionVPSlot` 中补充注册遗漏的静态场景与光照顶点着色器（`0x69e9adcf2e1b6887`、`0x6a8c2c78737dc94c`、`0xa20d6099a44e2cd5` 映射至 Slot 7，`0x6761469677f921c6` 映射至 Slot 8），消除帧间相机抖动补偿相位不匹配导致的闪烁。
- 优化着色器与管线预构建并发编译效率与 I/O 吞吐：
  - 基于宿主物理内存容量与 CPU 逻辑线程数动态调整 DXC 与管线并发工作线程数（`HostWorkerCap`）。宿主物理内存 >= 8 GB 时自动使用 `logicalThreads - 1` 线程充分发挥多核并发性能，仅在物理内存 < 8 GB 的低内存环境中保留 4 线程安全上限以防 OOM。
  - 支持通过环境变量 `LO_SHADER_WORKERS` 与 `LO_PIPELINE_WORKERS` 显式指定工作线程数（支持数值、`0` 或 `max` 全核）。
  - 移除 Windows 单文件着色器落盘时的 `MOVEFILE_WRITE_THROUGH` 物理强行刷盘标志，利用系统写缓冲消除小文件磁头寻道阻塞，显著提升写入吞吐。
  - 引入着色器预构建优先级排序：核心 XEX 与游戏包体真实着色器（优先级 0）优先编译，预测性展开变体（优先级 1）靠后编译，确保基础画面即时就绪。
  - 启动预构建界面增加按键跳过支持：按 ESC、空格键或手柄 B 键可随时安全跳过剩余预编译直接进入游戏，已编译内容自动持久化保存。
  - `settings.ini` 增加 `skip_shader_prebuild` 配置项，方便低配电脑与掌机用户永久绕过全量预编译。
  - 新增 `tools/release/package_shader_bundle.py` 打包脚本，支持将预编译启动包打入独立 Release 附件分发，避免消耗 Git LFS 配额。

- 新增可分发便携式 Vulkan 着色器包架构（`.lospv`）：
  - 引入独立、只读的 `.lospv` 分发资产，仅封装成功编译的 SPIR-V 字节码与必要 `TranslatedShader` 元数据，彻底剥离 HLSL 源码、诊断日志与编译失败条目。
  - 实现 SPIR-V 字节码 SHA-256 精确去重与分块 Zstandard 压缩（约 1 MiB 数据块），采用单块流式解码缓存，大幅压缩分发体积并削减启动 RSS 内存占用（28,482 个着色器压缩至 169.9 MB）。
  - 运行时着色器模块创建改为按需惰性加载（Lazy loading），存在有效便携包时直接秒级进入游戏并跳过启动阶段全量预编译（WSL Linux 实测 1.2 秒启动，0 次 DXC 调用与重翻译）；遇到未覆盖着色器时保留本地按需编译回退。
  - 兼容性契约与宿主绝对路径、宿主 DXC 动态库哈希完全解耦，支持跨系统与跨 CPU 架构（Windows / Linux / Steam Deck，x86-64 / ARM64）通用复用。
  - 提供 `LoShaderPackTool` 检查与完整性校验工具，新增 `tools/build_linux.sh` 与 `tools/build_wsl.bat` 便捷增量构建脚本，并更新打包流程（`package_shader_bundle.py`、`package_release.py`、`package_appimage.py`），支持在发布时按需校验并打包独立的 `.lospv` Release 附件。
- 移除 PowerPC 预编译静态库缓存与远程同步机制（`LO_PREBUILT_PPC_DIR`、`ppc_sync.py`、`ppc_prebuilt.py`）：
  - 所有平台（Windows 与 Linux）在 CI 及本地 Release 构建中均统一从重编译源码直接在线编译 `LostOdysseyRecompLib`，彻底消除特定平台的静态库依赖契约，为未来扩展更多硬件架构（如 ARM64）铺平道路。
  - 发布流程中通过 `tools/release/fetch_shader_pack.py` 自动获取便携式 Vulkan 着色器包，直接内置到 Windows 便携 ZIP 与 Linux AppImage 发布产物中（`shaders/portable_vk.lospv`）。
- 将解包及固定/链接着色器源码保存在有界内存中，而非在预构建期间导出并重新读取临时源码；保留已编译检查点以供启动中断后恢复。
- 将启动包绑定到当前运行时/编译器契约，恢复事件泵送与事务性错误传递，并修复取消唤醒机制。
- 低内存环境下限制 DXC 预处理并发工作线程，释放保留的 HLSL，且仅解码索引后的 CPX 数据块。保留显式诊断/全量扫描控制。
- 增加脱离游戏数据的 CPU/Sanitizer 以及生产功能回归测试覆盖。详见[审计范围、证据及剩余硬件检查](docs/MENU_SHADER_PREBUILD_AUDIT.md)。
- menu 分支修复了游戏内调试浮层在不同输出分辨率下的合成，并保护并发访问中的共享浮层状态。软件 UI 现使用正确的 RGBA 通道顺序，HID 锁覆盖完整设备操作，客户机暂停通过协作安全点完成，避免浮层交互无限期挂起工作线程。
- 完整修复代码审查报告 R1 至 R10 缺陷项：
  - 将手柄宿主输入泵（`PumpHostInput`）与浮层呈现（`PresentHostOverlay`）与客户机暂停解耦，并在 GPU `WAIT_REG_MEM` 循环中注入事件泵，防止暂停期间卡死（R1, R2）。
  - 使用 `std::condition_variable` 替换 `InterruptMain` 中的原子变量等待，并在退出流程接入 `host_ui::RequestStop()`，消除漏唤醒并确保可取消销毁（R5a, R5b）。
  - 引入感知暂停的主动游戏时钟（`GetActiveGameTimeMs`）与场景代数（`sceneGeneration`），防止暂停超时误判失效并保留传送标记与胜负请求；浮层 UI 准确反馈异步命令状态，杜绝虚假成功提示（R3, R9）。
  - 移除 Linux 设置菜单入口的平台宏限制，改为按渲染器能力启用（`LO_GPU_PLUME`）；拆分独立呈现缓冲隔离设置底图缓存，拦截模态下底层鼠标点击穿透，并修正 `ColorBlend` 的 straight-alpha source-over 混合算法（R4, R7, R8, R10）。
  - 将废弃的原生窗口测试迁移为浮层交互测试 `LoDebugMenuInteractionTest`，覆盖模态切换、导航、错误反馈及光栅化渲染（R6）。
- 完整修复第三轮代码审查报告 R11 至 R17 缺陷项：
  - 同步菜单呈现时的 `g_pixels` 缓冲与元数据，并在 `SaveScreenshot` 的 CPU 回退分支增加严格尺寸一致性校验，根除截图越界读取隐患（R11）。
  - 去除 `PumpHostInput` 中的按键重复轮询，统一由窗口事件泵处理键盘导航，解决方向与切页键双重触发问题（R12）。
  - 引入按键释放隔离（Release Quarantine）机制，关闭浮层的 B 键及肩键组合（LB+RB）在物理松开前持续过滤，防止泄漏到底层设置及游戏（R13）。
  - 在 GPU `PM4_WAIT_REG_MEM` 循环中增加 16ms 节拍的浮层独立呈现，解决等待停滞期间的菜单重绘饥饿问题（R14）。
  - 浮层呈现接入 `DisplayChangeTracker` 的有效展示凭证（Presentation Ticket），修复暂停期间全屏切换事务挂起缺陷（R15）。
  - 为浮层命令反馈增加最短保留时间，防止即时拒绝提示被过期的业务快照无条件覆盖（R16）。
  - 在 Linux 平台上按实际能力展示唯一的 Vulkan 图形后端，并将重启对话框调整为友好的手动重启提示（R17）。
  - 将 `LoDebugMenuInteractionTest` 测试目标移出 Windows 独占条件，加入跨平台测试套件。
- 维持并扩充测试套件：`LoHidTest`、`LoHostUiCompositeTest`、`LoDebugOverlayTest`、`LoDebugMenuInteractionTest` 与 `LoMenuRenderTest` 全部重新编译并测试通过。

Published at [GitHub Release v0.5.20](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.20) on 2026-09-17. / 已于 2026-09-17 发布于 [GitHub Release v0.5.20](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.20)。

## Historical development checkpoints / 历史开发检查点

### English

- In-game debug overlay and cross-platform settings rasterizer (unpublished development):
  - Replace standalone Win32 debug dialog window with cross-platform in-game UI overlay rendered directly via swapchain blending, toggled with keyboard F1 or gamepad shoulder chord LB+RB.
  - Automatically pause guest simulation while overlay is visible: freeze `KeTimeStampBundle` timestamp advances and pause audio streaming (`apu::SetPaused`), avoiding GPU CP synchronization deadlocks caused by suspended waiting threads.
  - Full keyboard and gamepad navigation support: D-pad / left stick, A/Enter confirm, B/Escape return, LB/RB/Tab page switching between Overview and Teleport tabs.
  - Settings menu completely removes Windows GDI dependencies, implementing a pure software 1280x720 cross-platform rasterizer rendered via the GPU swapchain, eliminating high-resolution (4K+) CPU rasterization slowdown and frame drops.
  - Extract `host_ui` shared module providing software rasterization primitives, pixel buffer blending, and Unifont bitmap font decoding.
  - Bounded verification: verified on Windows and Linux (RADV Vulkan) native builds with screenshot evidence retained.

- Linux installer, importer, updater, and packaging support (unpublished development):
  - Retain embedded SDL installer UI (`ShowInstallerUI`) and built-in file browser on Linux without requiring desktop document portal integrations; missing `default.xex` or `--install` invokes host installer.
  - Add writable user paths via `os/user_paths.h` following XDG conventions (`XDG_CONFIG_HOME`, `XDG_DATA_HOME`, `XDG_STATE_HOME`) when running in non-portable mode. Support read-only installation layouts, suppress `chdir` into read-only executable directories on Linux, and map Flatpak data directory to `/var/data`. Resolution of `game-path.txt` and default directories uses `ConfigDir`/`DataDir` when `!UsePortableLayout()`.
  - Add POSIX updater support: POSIX SHA256 (`import_crypto`), libcurl HTTP transport (`posix_http.cpp`), and POSIX startup check (`posix_startup.cpp`) targeting GitHub asset `LostOdysseyRecomp-linux-x64-<tag>.AppImage`. Apply mode executes atomic rename on `$APPIMAGE` followed by `execv`.
  - In Flatpak environments (`FLATPAK_ID` or `/.flatpak-info`), the updater reports `StartupStatus::ExternalUpdateAvailable`, never writes to `/app`, and prompts users to run `flatpak update io.github.freefrank.LostOdysseyRecomp`.
  - Add Linux SDL update confirmation and progress UI (`posix_ui.cpp`, `progress_posix.inl`), launching the updater child process via `posix_spawn` with `--apply-plan` and `--wait-process`.
  - Add Linux packaging specifications under `packaging/linux/`: desktop entry, 256x256 application icon, AppStream metainfo, and source-build Flatpak manifest (`io.github.freefrank.LostOdysseyRecomp.json`, freedesktop 24.08 platform with `filesystem=host`). Add `tools/package_appimage.py` for AppDir layout generation via `linuxdeploy`. CMake configures UNIX install targets with `$ORIGIN` RPATH, linking libcurl on UNIX only.
  - Add `release-linux` GitHub Actions job on `ubuntu-24.04` in `.github/workflows/release.yml`: compiles PowerPC code from source (`LO_PREBUILT_PPC_DIR` empty, as the Windows prebuilt library is `clang-cl /MT` only) and packages the AppImage. The Windows ZIP release job remains unchanged.
  - Retained boundaries: first-run HWND settings wizard remains a Win32 dialog with a stubbed `SaveConfig` on Linux; in-game F1 debug menu remains a Win32 stub. No Flathub package submission or Steam Deck sniper container build is claimed.
  - Focused verification: `LoGamePathTest` PASS, `LoUserPathsTest` PASS, `LoUpdaterPosixSha256Test` compiled cleanly, `LoUpdaterSdlUiTest` covered by worker, `package_appimage.py --dry-layout` ran on Windows. No hosted Linux CI run has completed yet.

- Modernize CPU waiting paths (R3 implementation): introduce `LostOdysseyRecomp/notified_wait.h` providing condition-variable predicate and deadline waits (`notified_wait::For`, `notified_wait::Until`). In `kernel/imports.cpp`, replace 200 µs polling loops for finite Event, Semaphore, and Mutant waits with condition-variable waits while preserving consume, recursive ownership, and timeout semantics. In `gpu/command_processor.{h,cpp}`, notify on write-pointer updates and shutdown, replacing the 200-iteration yield loop in `WorkerMain` with a bounded 500 µs condition-variable wait while preserving SDL event pumping. Add direct fixture `LoNotifiedWaitTest` (`tools/tests/notified_wait_test.cpp`).
- CPU waiting-path verification: Windows `LoNotifiedWaitTest` passed pre-notify, early wake, deadline, and CP-pointer wake cases; `LoPollWaitTest` passed; full Windows target compiled; diff check passed; native Linux build and codegen passed with maintained dependency patches. A 15 W native Vulkan run on AMD Radeon 8060S (STAPM 15 W / Fast 25 W / Slow 20 W) created/resized a 1280x720 swapchain, started guest execution, completed 28,484 startup shaders at allowed 60 W (28,482 ready, 2 deterministic failures), consumed the startup bundle, and ran 90 seconds without error. Target was 60 FPS: earlier stable windows observed around 58.46–58.58 FPS, and a later heavier scene ran around 37–40 FPS (do not claim locked 60 FPS). No player visual acceptance, push, or release exists.
- Unify content importer and updater into the single `LostOdysseyRecomp.exe` runtime binary; exclude separate `InstallGame.exe` and `LostOdysseyUpdater.exe` helper executables from the release payload while retaining the legacy updater build target for fixtures. Packaging now distributes only `LostOdysseyRecomp.exe`, DXC libraries, licenses and manifests.
- Remove the Python/Tk `tools/installer` sources, their fixture tests and `test-importer.yml`. Release CI now reads the Disc 1 XEX SHA-256 from `LostOdysseyRecomp/install/import_game.cpp`. Current importer coverage is `LoImportGameTest` and `LoInstallerControllerTest`.
- Update embedded SDL installer controller and UI to support mixed disc and DLC discovery and import (`ScanContent`/`InstallContent`), transactional staging, cancellable background scanning and import, preserved game paths for DLC-only runs, and partial-disc preservation on DLC failure. Add game-style brushed steel UI styling, graphic folder/file icons, centered selection bar, and precise UTF-8 width-measured text truncation with CJK glyph rendering.
- Installer navigation and D-pad icon updates: Review screen action buttons now navigate Left/Right only, with Up/Down leaving selection unchanged; a cached 32x32 transparent dark-cross D-pad bitmap icon with pale triangular arrows replaces the text PAD label in source, destination, and review footers; controller left stick maps SDL `LEFTX`/`LEFTY` motion to directional navigation using strongest-axis resolution with deadzone 16000 and held-threshold 10000, initial 350 ms delay and 120 ms repeat rate, window focus and worker-busy guards, and controller device hotplug handling.
- Installer UI follow-up fixes: read import errors from destination `InstallResult` objects, allow Escape/B during scan to request cancellation and immediately return to the source browser (late successful scans intentionally not auto-transitioned), convert and width-clip review paths via UTF-8, and clip DLC names to avoid overwriting the Files column. Real keyboard navigation in the source browser, a scan of 4 Asia GOD discs and 3 DLC packages (`out/installer-god-review-fixed.png`), and custom destination picker opening (`out/installer-destination.png`) were verified on local sources without copy/import or source modifications; read-only Oracle review confirmed no regressions under the cancellation contract.
- Navigation verification: Windows main build and installer controller tests passed (`LoInstallerControllerTest`), with 14 explicit axis mapper checks. Keyboard navigation verified Left/Right clamping between Start import and Change source, no Up/Down selection changes, and Right + Enter from Start import opening Change destination. Six `out/navigation-*.png` captures record the states; independent visual review of five captures passed within that scope. Real stick hardware and full UI acceptance remain unverified.
- Fixed discovery and transactional import of extracted DLC directories containing `.lo-content`, `.lo-dlc-header`, and `.lo-dlc.json`. Validate identity, safe manifest paths, sizes and payload hashes, preserve sidecars, reject overlapping destinations, and keep identical imports unchanged. The extracted-DLC-only fixture imported and byte-compared all three real packages in temporary storage and passed duplicate, invalid-source and mid-copy cancellation checks. Live installer review now shows four discs and three Ready DLC entries for the supplied extracted game directory (`out/extracted-dlc-review.png`); originals were not modified.
- Update main executable startup to check and prompt for updates before game initialization using lightweight settings preferences (`automatic_updates` and `ui_language` without altering edition game language), parse English/Chinese release notes from GitHub Release bodies with consent dialog before download, run updates from a private copy of the main binary in `--apply-plan` mode, and auto-restart after update when requested.
- 2026-09-16 installer/updater audit fixes (unpublished development): require successful STFS DLC payload and sidecar I/O before publishing an import; clear stale scan selections and block failed or empty scans, and clear cancellation state on retry; parse Windows `--apply-plan` as an independent argument; resolve Linux `ProfileDir` under the XDG data directory while retaining `LO_PROFILE_DIR` and handling directory-creation failure without throwing; exclude `libwayland*` during linuxdeploy dependency deployment before AppImage output generation; and fix extracted-DLC trailing-separator ancestor traversal with cancellation support.
- The user accepted this development batch for commit. Repeated focused verification passed with exit 0: `LoInstallerControllerTest`, `LoUpdaterApplyArgumentsTest` (8 checks), `LoImportGameTest --dlc-io`, and the AppImage script checks (3/3). The Windows `LoUserPathsTest` cannot cover Linux behavior; the separate WSL Manjaro fixture already passed portable, XDG, changed-CWD, `LO_PROFILE_DIR` override and Flatpak paths. The Windows main target incremental build also completed successfully; existing deprecated compiler warnings remain.
- Non-blocking follow-up: add a root guard for the `ExistingDlcPayloadMatches` ancestor walk (the only current caller generates `dest/dlc/<hexID>`, so trailing-slash reachability is unconfirmed), and add explicit close-result coverage for extracted DLC. No repeated-import hang is established.
- Acceptance is limited to this development batch and its focused checks: no real AppImage package, live network or in-place update, complete interactive game import, or full-game playthrough has been performed; no release has been published.
- Importer POSIX lock and process-id compatibility fixes pass Linux WSL synthetic tests (`LoImportGameTest`). Windows updater startup/restart tests pass (`LoUpdaterTest --startup-restart`, `LoUpdaterStartupOrderTest`). Configured Windows main build passed after final changes; prior tests were reused.
- Bounded verification only: real network update downloads, in-place installed-game binary replacement, full live import completion in UI, and full-game play remain unverified. Game visual fidelity, actual controller hardware acceptance, and full UI visual acceptance are NOT established. AppImage is not packaged, macOS is untested, and the POSIX updater path has not had a live update run. Importer POSIX file locking is fixed, but general Linux game compatibility is not established.

- Prior installer migration checkpoint: Folder/XEX, XDVDFS ISO, GOD and STFS DLC discovery, portable hashing, edition-aware identity checks, transactional disc staging/rollback, and self-drawn SDL2 host UI. Focused synthetic transaction/cancellation/mixed-edition/DLC checks passed; prior audited read-only scans (USA ISO, Asia GOD+DLC, Asia folders) are retained as bounded evidence, not release or cross-platform acceptance.
- The first playable Linux Vulkan-only, unbundled ELF path remains a source-built path (`linux-clang` preset on WSL2 Manjaro with Mesa Dozen Vulkan-on-D3D12); no Linux package is published.
- Deploy the GitHub Issue triage workflow update at `600b08e` with authorized human `@codex` comment requests, per-comment deduplication, recent human discussion and bounded first-party code retrieval. Nineteen triage/mention checks and seven retrieval checks pass; a real public `@codex` reply remains to be observed.

### 简体中文

- 游戏内调试浮层与跨平台设置菜单光栅化（未发布开发内容）：
  - 调试菜单从独立 Win32 窗口改为通过交换链混合渲染的跨平台游戏内 UI 浮层（Overlay），支持键盘 F1 与手柄双肩键组合（LB+RB 同时按下）随时呼出与隐藏。
  - 菜单呼出时自动暂停客户机模拟：冻结 `KeTimeStampBundle` 时间戳递增并暂停音频流（`apu::SetPaused`），避免挂起等待线程导致的 GPU CP 同步死锁。
  - 支持全功能键盘及手柄导航：十字键／左摇杆导航、A/Enter 确认、B/Esc 返回、LB/RB/Tab 切换 Overview 与 Teleport 分页。
  - 设置菜单彻底去除 Windows GDI 依赖，实现纯软件 1280x720 跨平台光栅化，并经由 GPU 交换链呈现，解决 4K 等高分辨率 CPU 渲染导致的菜单卡顿与掉帧。
  - 提取 `host_ui` 共享模块，共用纯 CPU 光栅化基础原语、像素缓冲区混合与 Unifont 点阵字体解码。
  - 定向验证：在 Windows 与 Linux（RADV Vulkan）原生构建下验证通过，并保留实机截图证据。

- Linux 安装器、导入器、更新器与打包支持（未发布开发内容）：
  - Linux 保留内置 SDL 安装器界面（`ShowInstallerUI`）和内建文件浏览器，无需依赖桌面文档门户（portal）；缺少 `default.xex` 或指定 `--install` 时启动宿主安装器。
  - 增加通过 `os/user_paths.h` 管理的写入路径：在非便携模式下遵循 XDG 规范（`XDG_CONFIG_HOME`、`XDG_DATA_HOME`、`XDG_STATE_HOME`）。识别只读安装目录并在 Linux 下避免切换工作目录至只读可执行文件路径；Flatpak 数据目录映射到 `/var/data`。在 `!UsePortableLayout()` 时，`game-path.txt` 与默认游戏路径解析改用 `ConfigDir`/`DataDir`。
  - 增加 POSIX 更新器支持：实现 POSIX SHA256（`import_crypto`）、libcurl HTTP 传输（`posix_http.cpp`）与 POSIX 启动检查（`posix_startup.cpp`），对应 GitHub 资源 `LostOdysseyRecomp-linux-x64-<tag>.AppImage`。更新应用模式在 Linux 下重命名 `$APPIMAGE` 后调用 `execv`。
  - 在 Flatpak 环境下（检测 `FLATPAK_ID` 或 `/.flatpak-info`），更新器返回 `StartupStatus::ExternalUpdateAvailable`，不写入 `/app`，并提示运行 `flatpak update io.github.freefrank.LostOdysseyRecomp`。
  - 增加 Linux SDL 更新确认及下载进度界面（`posix_ui.cpp`、`progress_posix.inl`）；在 `main.cpp` 中通过 `posix_spawn` 携带 `--apply-plan` 与 `--wait-process` 启动更新器子进程。
  - 添加 `packaging/linux/` 下的 Linux 打包定义：桌面启动文件、256x256 图标、AppStream metainfo 与源码构建 Flatpak 清单（`io.github.freefrank.LostOdysseyRecomp.json`，基于 freedesktop 24.08 运行时，赋予 `filesystem=host`）。增加 `tools/package_appimage.py`，使用 `linuxdeploy` 生成 AppDir 布局。CMake 增加 UNIX 安装目标及 `$ORIGIN` RPATH，且仅在 UNIX 下链接 libcurl。
  - 在 `.github/workflows/release.yml` 中新增 `ubuntu-24.04` 上的 `release-linux` GitHub Actions 任务：从源码直接生成并编译 PowerPC 代码（`LO_PREBUILT_PPC_DIR` 留空，因 Windows 预编译 `.lib` 仅支持 `clang-cl /MT`），并打包 AppImage。原 Windows ZIP 发布流程保持不变。
  - 保留的平台边界：首次运行 HWND 设置向导在 Linux 下仍为 stub 化的 `SaveConfig`；游戏内 F1 调试菜单在 Linux 下仍为 Win32 stub。未提交至 Flathub，未打包 Steam Deck 原生 sniper 运行时版本。
  - 定向验证：`LoGamePathTest` 通过，`LoUserPathsTest` 通过，`LoUpdaterPosixSha256Test` 编译通过，`LoUpdaterSdlUiTest` 覆盖通过，Windows 上成功执行 `package_appimage.py --dry-layout`。新增的托管 Linux CI 任务尚未实际运行。

- 现代化 CPU 等待路径（R3 实现）：引入 `LostOdysseyRecomp/notified_wait.h`，提供条件变量谓词与截止期等待工具（`notified_wait::For`、`notified_wait::Until`）。在 `kernel/imports.cpp` 中，将 Event、Semaphore 和 Mutant 有限超时等待中的 200 µs 轮询循环替换为条件变量等待，同时保留原有的消费、递归所有权与超时语义。在 `gpu/command_processor.{h,cpp}` 中，在写指针更新与关闭时触发通知，将 `WorkerMain` 中的 200 次 yield 循环替换为带 500 µs 上限的条件变量等待，并保留 SDL 窗口事件轮询（`video::PumpEvents()`）。增加直接测试夹具 `LoNotifiedWaitTest`（`tools/tests/notified_wait_test.cpp`）。
- CPU 等待路径验证：Windows `LoNotifiedWaitTest` 预通知、提前唤醒、截止期超时和 CP 写指针唤醒用例全部通过；`LoPollWaitTest` 回归测试通过；完整 Windows 目标成功编译；diff 校验通过；依赖补丁应用后原生 Linux 构建与代码生成通过。AMD Radeon 8060S 上进行的 15 W 原生 Vulkan 实机运行（STAPM 15 W / Fast 25 W / Slow 20 W）创建并调整 1280x720 swapchain，启动 guest 运行，在允许的 60 W 功耗下完成 28,484 个已知 shader 的准备（28,482 就绪，2 个确定性失败），消费启动包，并持续运行 90 秒无错误或致命诊断。目标帧率为 60 FPS：前期稳定区间约 58.46–58.58 FPS，后期复杂场景约 37–40 FPS（不宣称锁定 60 FPS）。未进行玩家视觉验收，未执行提交、推送或发布。
- 将内容导入器与更新器整合至单个 `LostOdysseyRecomp.exe` 运行时主二进制中；发布打包中排除独立的 `InstallGame.exe` 与 `LostOdysseyUpdater.exe` 辅助可执行文件，同时保留旧版更新器编译目标供测试夹具使用。发布打包现仅分发 `LostOdysseyRecomp.exe`、DXC 运行库、许可证及校验清单。
- 删除 Python/Tk `tools/installer` 源码、对应 fixture 测试和 `test-importer.yml`。Release CI 现从 `LostOdysseyRecomp/install/import_game.cpp` 读取 Disc 1 XEX SHA-256。当前导入器覆盖为 `LoImportGameTest` 与 `LoInstallerControllerTest`。
- 更新内置 SDL 安装器控制器与界面：支持光盘与 DLC 混合发现及安装（`ScanContent`/`InstallContent`）、事务式暂存、后台可取消的扫描与导入、纯 DLC 导入时不覆盖已有游戏路径、DLC 出错时保留已完成的光盘路径。增加游戏风格的拉丝钢质界面样式、图形化文件夹／文件图标、垂直居中选择条，以及基于 UTF-8 字符宽度精确测量的文本省略截断与 CJK 字符渲染。
- 安装器导航与十字键图标更新：确认界面操作按钮现仅支持左／右键切换选择，上／下键保持当前选择不变；源路径、目标路径及确认界面底部以 32x32 缓存的透明黑十字带浅色三角箭头 D-pad 位图图标替代文本 PAD 提示；手柄左摇杆通过 SDL `LEFTX`/`LEFTY` 轴移动映射到方向导航，采用主轴强度判定，死区为 16000、持续触发阈值为 10000，初始延迟 350 ms、重复间隔 120 ms，并具备窗口焦点与工作线程忙碌保护及手柄热插拔支持。
- 安装器 UI 后续修复：从目标 `InstallResult` 对象读取导入错误；扫描中按 Escape/B 请求取消并立即返回源路径浏览（延迟扫描完成故意不自动跳转）；确认界面路径经 UTF-8 转换并按宽度裁剪；DLC 显示名称按 168 像素宽度裁剪以避免覆盖文件数列。在本地真实源上验证了真实键盘在源路径浏览中的导航、4 张亚洲版 GOD 光盘与 3 个 DLC 的扫描（`out/installer-god-review-fixed.png`）以及自定义目标选择器的打开（`out/installer-destination.png`），未执行实际复制／导入或修改原始源文件；只读 Oracle 审阅确认取消契约下无回归问题。
- 导航验证：Windows 主程序编译与 `LoInstallerControllerTest` 通过，包含 14 项显式轴映射检查。实机键盘验证左／右键在 Start import 与 Change source 间切换并限制边界，上／下键不改变选择，从 Start import 按右键加 Enter 打开 Change destination。六张 `out/navigation-*.png` 记录状态，独立视觉审阅五张截图在限定范围内通过。真实摇杆硬件与完整 UI 验收仍未完成。
- 修复带 `.lo-content`、`.lo-dlc-header`、`.lo-dlc.json` 的已解包 DLC 目录识别及事务式导入。校验身份、清单路径、大小与内容哈希，保留 sidecar，拒绝源目标重叠，相同内容重复导入保持不变。独立 DLC 检查在临时目录导入并逐字节比对三个真实包，重复导入、无效源和中途取消检查通过。主程序扫描所提供的解包游戏目录现显示四张光盘与三个 Ready DLC（`out/extracted-dlc-review.png`），原目录未修改。
- 更新主程序启动流程：在游戏初始化前使用轻量设置项（`automatic_updates` 和 `ui_language`，不改变版本游戏语言）检查并提示更新；从 GitHub Release 正文解析中英文更新日志并在下载前弹出确认框；使用主二进制私有副本以 `--apply-plan`模式执行更新，并在应用后按计划自动重启。
- 2026-09-16 安装器／更新器审计修复（未发布开发内容）：要求 STFS DLC payload 和 sidecar 的 I/O 成功后才发布导入；清理过期扫描选择并阻止失败或空扫描导入，重试时清除取消状态；将 Windows `--apply-plan` 按独立参数解析；在保留 `LO_PROFILE_DIR` 的同时将 Linux `ProfileDir` 固定到 XDG data 目录，并处理目录创建失败而不抛出异常；在 linuxdeploy 依赖部署阶段排除 `libwayland*`，在生成 AppImage 输出前完成；修复已解包 DLC 尾部分隔符导致的祖先遍历死循环，并支持取消。
- 用户已接受本开发批次进入 commit 流程。定向验证复跑均以 exit 0 通过：`LoInstallerControllerTest`、`LoUpdaterApplyArgumentsTest`（8 项）、`LoImportGameTest --dlc-io` 和 AppImage 脚本检查（3/3）。Windows `LoUserPathsTest` 无法覆盖 Linux 行为；独立的 WSL Manjaro fixture 已通过 portable、XDG、切换 CWD、`LO_PROFILE_DIR` override 和 Flatpak 路径。Windows 主目标增量构建也已成功完成，仍有既存弃用编译警告。
- 非阻塞后续：为 `ExistingDlcPayloadMatches` 祖先遍历增加 root guard（当前唯一调用方生成 `dest/dlc/<hexID>`，尾斜杠是否可达尚未证实），并增加 extracted DLC 的显式 close 结果覆盖。没有证据证明重复导入会死循环。
- 验收仅覆盖本开发批次及定向检查：未执行真实 AppImage 封包、在线或原地更新、UI 完整真实导入或全游戏流程；没有发布版本。
- 导入器 POSIX 文件锁与进程 ID 兼容性修复已通过 Linux WSL 合成测试（`LoImportGameTest`）。Windows 更新器启动与重启测试已通过（`LoUpdaterTest --startup-restart`、`LoUpdaterStartupOrderTest`）。配置的 Windows 主程序在最终改动后已通过编译；复用既有测试，未重复运行。
- 仅限有界验证：真实网络更新下载、现场替换运行中的游戏二进制、UI 中完整执行真实导入以及全游戏游玩仍未验证。游戏画面还原度、真实手柄硬件验收以及整套 UI 视觉验收尚未确立。未打包 AppImage，macOS 未测试，POSIX 更新器路径尚未进行真实更新运行。导入器 POSIX 文件锁已修复，但不代表全平台游戏可用。

- 先前安装器迁移检查点：Folder/XEX、XDVDFS ISO、GOD 和 STFS DLC 扫描、可移植哈希、版本对应的身份校验、事务式暂存／回滚，以及自绘 SDL2 宿主界面。定向合成事务／取消／混合版本／DLC 检查已通过；先前已审计的只读扫描（美版 ISO、亚洲 GOD+DLC、亚洲文件夹）作为有界证据保留，不代表发布或跨平台验收。
- 首个可玩的 Linux Vulkan-only 未打包 ELF 路径仍为源码构建路径（WSL2 Manjaro + Mesa Dozen Vulkan-on-D3D12）；目前没有发布 Linux 包。
- 已在 `600b08e` 部署 GitHub Issue triage workflow 更新，支持授权协作者的 `@codex` 评论请求、按评论去重、最近人类讨论和有界的 first-party 代码检索。19 项 triage/mention 检查及 7 项 retrieval 检查已通过；首次公开 `@codex` 回复仍待观察。

## v0.5.14 — 2026-09-16 / Published / 已发布

### English

- The main binary now provides the installer and updater in one place, with safer DLC import failure handling, reliable scan and retry behavior, exact apply-mode argument handling, Linux XDG profile paths, extracted-DLC trailing-separator scanning, and AppImage dependency filtering.
- Linux x64 AppImage is available alongside the Windows x64 ZIP, with the embedded installer/updater and XDG user data paths. AppImage gameplay and in-place updating remain unverified; complete Steam Deck compatibility is not established.
- CPU waiting paths now use condition-variable notifications in place of the former polling loops. Focused verification is retained in the historical development record.
- Real network updates, AppImage gameplay or update runs, complete UI import, and full-game validation remain unverified.

The published Linux AppImage had a packaging correction after a user launch exposed `execv` `ENOENT`: the original published package lacked `AppRun` because its desktop entry was placed at the AppDir root. `tools/package_appimage.py` now deploys metadata explicitly, uses two packaging stages, and checks the internal executable before output. The corrected Linux asset and checksum are published (SHA-256 `0991df9aca8e930fa8eacbd8afe99b7a3a81e940dfa740fcfbdba34cd54c4d0a`), with an anonymous download verified. The runtime and library bytes are unchanged; this correction does not claim gameplay or live-update validation.

### 简体中文

- 已替换缺少 `AppRun` 入口的 Linux AppImage 和校验文件，修复启动时的 `execv` 报错。原主程序与库内容不变；5/5 打包回归通过，WSL 已验证进入主程序，未追加游戏或在线更新验证。

- 主二进制现已统一提供安装器与更新器，并改进 DLC 导入失败处理、扫描与重试行为、apply 模式参数解析、Linux XDG profile 路径、已解包 DLC 尾部分隔符扫描和 AppImage 依赖过滤。
- 同时提供 Windows x64 ZIP 和 Linux x64 AppImage，包含内嵌安装器／更新器与 XDG 用户数据路径。AppImage 游戏运行和原地更新仍未验证，完整 Steam Deck 兼容性尚未确立。
- CPU 等待路径使用条件变量通知替代原有轮询循环，定向验证保留在历史开发记录中。
- 真实在线更新、AppImage 游戏或更新运行、UI 完整导入和全游戏验证仍未核验。

Published at [GitHub Release v0.5.14](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.14) on 2026-09-16T07:18:53Z from source/tag commit `caf8060d99e5f1e52aec9d545331efe59e0c01e8`. Release CI `35065717899` succeeded. The Windows ZIP is 32,915,456 bytes with SHA-256 `c21224ed985ada3502e25b42dd9e9379cb95749b0f06cea6d838f4d60843c09d`; the Linux AppImage is 43,162,104 bytes with SHA-256 `a9912d2a258f17a2fea1a4d7f99c9589b538e66efd25225b1ade74af606e6196`. The 47-file Windows manifest and Linux format/sidecar checks passed; anonymous downloads of all four public assets matched their recorded bytes and hashes. Publication does not claim packaged gameplay or live update validation.

## v0.5.13 — 2026-09-14 / Published / 已发布

### English

- Add an accepted **Alt+Enter** shortcut that toggles **Windowed** and **Borderless** presentation modes without selecting DXGI exclusive fullscreen. The chord accepts left Alt, right Alt and AltGr while retaining Shift/GUI rejection, placement handling and debounce; `DXGI_MWA_NO_ALT_ENTER` remains set.
- The focused `LoGameWindowPixelsTest --rendering-fixes-only` fixture passed after the SYSKEY, `windowID=0`, right-Alt and AltGr/`VK_MENU=18` updates. Live local acceptance confirmed both left-Alt+Enter and right-Alt+Enter in the actual game window on 2026-09-14. Whole-game, exclusive-fullscreen, mixed-DPI and mouse validation remain unverified; the published package was not launched for gameplay validation.

### 简体中文

- 增加已验收的 **Alt+Enter** 快捷键，在 **Windowed** 与 **Borderless** 显示模式之间切换，不选择 DXGI exclusive fullscreen。快捷键支持左 Alt、右 Alt 和 AltGr，同时保留 Shift／GUI 拒绝、窗口位置处理和去抖；`DXGI_MWA_NO_ALT_ENTER` 仍保持设置。
- 更新 SYSKEY、`windowID=0`、右 Alt 和 AltGr／`VK_MENU=18` 覆盖后的定向 `LoGameWindowPixelsTest --rendering-fixes-only` 夹具已通过。2026-09-14 在实际游戏窗口中现场验收了左 Alt+Enter 和右 Alt+Enter。全游戏、exclusive fullscreen、混合 DPI 和鼠标验证仍未核验；未启动发布包进行游戏实测验证。

Published at [GitHub Release v0.5.13](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.13) on 2026-09-14T23:36:59Z. Release CI [34908617463](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34908617463) succeeded after the synchronized PPC identity was retargeted; source/tag commit `545af9be26ebafed364f68c1b2725c10b5b03206` and annotated tag `v0.5.13` are published. CI used the prebuilt PPC artifact from `LostOdysseyRecomp-build-inputs`, not `rebuild_ppc`; cache key `bdf6dce2af1112a1dd0e1ed161bd66a2d7959ed1f174ad049a684b74a4367fc7` came from private commit `d4feb17917189a5b9f051b48b783f5fc62081658`. Only the root `CMakeLists.txt` hash changed versus v0.5.12; the PPC contract and library chunks were unchanged. The Windows ZIP is 44,304,038 bytes with SHA-256 `a993071c24f7324a3ae3a0dbe24688afc60450d3da9f1a78fbf532c69f65bacd`; the runtime is 83,536,896 bytes with SHA-256 `d144762040db8918e34282f429044833266fdd3f5b9c48bf221c27752fbc6e58`; the updater is 846,336 bytes with SHA-256 `5e5c7957947969431ae499a3acc18536dc5940aaef0598fd7ba2faa2f6c76b96`. The ZIP hash matches its sidecar, and both public ZIP and `.sha256` downloads returned HTTP 302 to GitHub release assets; followed downloads returned HTTP 200 and matched the recorded hash and size. Publication checks do not claim 50-file manifest re-verification or gameplay validation of the published package.

已于 2026-09-14T23:36:59Z 发布 [GitHub Release v0.5.13](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.13)。Release CI [34908617463](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34908617463) 在同步 PPC identity 重新指向后成功；源码／标签提交为 `545af9be26ebafed364f68c1b2725c10b5b03206`，annotated tag 为 `v0.5.13`。CI 使用 `LostOdysseyRecomp-build-inputs` 中的预编译 PPC artifact，而非 `rebuild_ppc`；缓存 key 为 `bdf6dce2af1112a1dd0e1ed161bd66a2d7959ed1f174ad049a684b74a4367fc7`，来自私有提交 `d4feb17917189a5b9f051b48b783f5fc62081658`。相较 v0.5.12 仅根目录 `CMakeLists.txt` hash 改变，PPC contract 和 library chunks 未改变。Windows ZIP 大小为 44,304,038 字节，SHA-256 为 `a993071c24f7324a3ae3a0dbe24688afc60450d3da9f1a78fbf532c69f65bacd`；runtime 为 83,536,896 字节，SHA-256 为 `d144762040db8918e34282f429044833266fdd3f5b9c48bf221c27752fbc6e58`；updater 为 846,336 字节，SHA-256 为 `5e5c7957947969431ae499a3acc18536dc5940aaef0598fd7ba2faa2f6c76b96`。ZIP hash 与 sidecar 一致，公开 ZIP 和 `.sha256` 下载均返回 HTTP 302 到 GitHub release-assets；跟随下载返回 HTTP 200，且 hash 与大小一致。发布检查不宣称重新核对 50 文件 manifest，也不宣称发布包游戏实测验证。

## v0.5.12 — 2026-09-14 / Published / 已发布

### English

- Fix USA/Europe FMV and event subtitle language mapping. The `82481BE8` hook now returns the original executable language-table pointer for host `GameLanguage()` IDs 1–9, preserving the existing Simplified Chinese repair. Callers that pre-filter with `r4=0` can resolve INT/JPN/DEU/FRA/SPA/ITA suffixes instead of aliasing to the ID-0 English record. Issue [#27](https://github.com/freefrank/LostOdysseyRecomp/issues/27).
- A local USA/Europe Spanish opening-FMV check passed on 2026-09-14 with `game_language=5` and Vulkan at 3840×2160. On 2026-09-15 the original reporter confirmed Spanish opening FMV subtitles on v0.5.12 (Vulkan and Direct3D 12), and DE/FR/IT FMV coverage was confirmed. Complete event coverage and whole-game validation remain unverified. Issue [#27](https://github.com/freefrank/LostOdysseyRecomp/issues/27) is closed.

Published at [GitHub Release v0.5.12](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.12) on 2026-09-14T21:34:13Z. Release CI [34895591364](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34895591364) succeeded for source/tag commit `36e574b64e0cbb11ddcdc7f68cbc36214b5c084f` after the PPC cache was synchronized. CI used the prebuilt PPC artifact from `LostOdysseyRecomp-build-inputs`, not `rebuild_ppc`; cache key `cb4a75c2d3fa6e3d5f92e3431e7006f77fd7927dd97f05d27eb488ea4cf87e44` came from private commit `5df5b6efb38537386f6c9522ae9126d800032559`. The Windows ZIP is 44,304,445 bytes with SHA-256 `7cc99618cee509bdea000b736772344de60279f4a439a4f876cc7d49d4c8e60a`; the runtime is 83,536,384 bytes with SHA-256 `d394c173cfa8ecc6ea57c1b9671a1a574ae02d2cd4bb76008392caca66686974`; the updater is 846,336 bytes with SHA-256 `36787c2314d9d554010193a4f020eb8d1a848292920e40af3bb7fb44f1a3893e`. The ZIP hash matches its sidecar, and both public ZIP and `.sha256` downloads returned HTTP 302 to GitHub release assets. Issue [#27](https://github.com/freefrank/LostOdysseyRecomp/issues/27) is closed after reporter Spanish confirmation and lead DE/FR/IT FMV confirmation on 2026-09-15. Publication checks do not claim 50-file manifest re-verification or whole-game validation.

### 简体中文

- 修复 USA/Europe FMV／事件字幕语言映射。`82481BE8` hook 现在会为宿主 `GameLanguage()` ID 1–9 返回原可执行文件语言表指针，并保留既有简体中文修复。调用方以 `r4=0` 预筛选时，可解析 INT/JPN/DEU/FRA/SPA/ITA 后缀，而不再别名到 ID-0 英文记录。[#27](https://github.com/freefrank/LostOdysseyRecomp/issues/27)。
- 2026-09-14 本地 USA/Europe 西班牙语开场 FMV 检查通过，`game_language=5`，Vulkan 3840×2160。2026-09-15 原报告者确认 v0.5.12 西班牙语开场 FMV 字幕（Vulkan 与 Direct3D 12），德语、法语、意大利语 FMV 覆盖亦已确认。完整事件覆盖和全游戏验证仍未核验。Issue [#27](https://github.com/freefrank/LostOdysseyRecomp/issues/27) 已关闭。

已于 2026-09-14T21:34:13Z 发布 [GitHub Release v0.5.12](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.12)。Release CI [34895591364](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34895591364) 使用源码／标签提交 `36e574b64e0cbb11ddcdc7f68cbc36214b5c084f` 成功；PPC 使用 `LostOdysseyRecomp-build-inputs` 中的预编译缓存，而非 `rebuild_ppc`。缓存 key 为 `cb4a75c2d3fa6e3d5f92e3431e7006f77fd7927dd97f05d27eb488ea4cf87e44`，来自私有提交 `5df5b6efb38537386f6c9522ae9126d800032559`。Windows ZIP 大小为 44,304,445 字节，SHA-256 为 `7cc99618cee509bdea000b736772344de60279f4a439a4f876cc7d49d4c8e60a`；runtime 为 83,536,384 字节，SHA-256 为 `d394c173cfa8ecc6ea57c1b9671a1a574ae02d2cd4bb76008392caca66686974`；updater 为 846,336 字节，SHA-256 为 `36787c2314d9d554010193a4f020eb8d1a848292920e40af3bb7fb44f1a3893e`。ZIP 哈希与 sidecar 一致，ZIP 和 `.sha256` 两个公开下载均返回 HTTP 302 到 GitHub release-assets。Issue [#27](https://github.com/freefrank/LostOdysseyRecomp/issues/27) 已于 2026-09-15 关闭（报告者确认西班牙语，维护者确认德／法／意 FMV）。发布检查不宣称重新核对 50 文件 manifest 或全游戏验证。

## v0.5.11 — 2026-09-14 / Published / 已发布

### English

- Add startup and lower-layer failure diagnostics. D3D12/Vulkan GPU failures record the API, raw code and resource context; early allocation failures retain their original error and memory/context snapshot; startup records environment and build identity; WinHTTP terminal failures retain raw errors. Repeated failures are rate-limited, and GPU adapter/renderer formatting has an emergency fallback. Issues [#6](https://github.com/freefrank/LostOdysseyRecomp/issues/6) and [#22](https://github.com/freefrank/LostOdysseyRecomp/issues/22) remain open; this instrumentation does not claim either report is fixed.
- Focused Plume, allocation, updater-HTTP and native-GPU diagnostic checks passed. Independent D3D12 and Vulkan runs also confirmed the new startup fields are written to runtime logs; these are bounded logging checks, not complete game/backend acceptance. The build provenance check now accepts patch-added files while still verifying the complete expected tree.

### 简体中文

- 增加启动和底层失败诊断。D3D12/Vulkan GPU 失败记录 API、原始 code 和资源上下文；早期客体地址空间分配失败保留原始错误及内存／现场快照；启动记录环境与构建身份；WinHTTP 终止失败保留原始错误。重复失败会限频，GPU adapter/renderer 格式化失败有应急兜底。[#6](https://github.com/freefrank/LostOdysseyRecomp/issues/6) 与 [#22](https://github.com/freefrank/LostOdysseyRecomp/issues/22) 仍开放；此诊断增强不宣称修复任一报告。

- Plume、分配、updater HTTP 和 native GPU diagnostic 定向检查均已通过。独立 D3D12 与 Vulkan 进程也确认新增启动字段写入 runtime log；这些是有界日志检查，不是完整游戏／后端验收。构建 provenance 检查现支持补丁新增文件，同时仍核对完整的预期源码树。

## v0.5.10 — 2026-09-13 / Published / 已发布

### English

- Fix 11 additional Vulkan TAA vertex-shader paths observed in captures f5997, f5912 and f16385, keeping depth, material and light layers aligned. The user accepted the reported flicker scenes; other scenes and whole-game coverage remain unverified.
- Extend startup shader preparation with verified original CPX metadata and the captured packed static-mesh declaration, including `1474db97dfc0afad`. Discovery changes refresh the startup bundle while reusing valid individual compiled shaders. Gameplay frame-time validation remains pending.
- Reuse passing captured-layer, original-source variant and startup-cache checks, including byte-identical preservation of 2,245 historical fixed/linked outputs. See the [TAA records](docs/notes/taa-f5912-f16385-2026-09-13.md) and [startup coverage note](docs/notes/shader-startup-coverage-2026-09-13.md).

### 简体中文

- 修复 f5997、f5912、f16385 捕获中的另外 11 条 Vulkan TAA 顶点 shader 路径，使深度、材质和光照层保持对齐。用户已验收报告中的闪烁场景；其他场景和全游戏覆盖仍未验证。
- 补充启动 shader 准备的原始 CPX 元数据及捕获到的 packed 静态网格声明，覆盖 `1474db97dfc0afad`。覆盖变化会更新启动包，同时复用有效的单个已编译 shader；实景帧时间仍待验证。
- 复用已通过的捕获层、原始资源变体和启动缓存检查，包括 2,245 个历史 fixed/linked 输出的逐字节兼容性。详见 [TAA 记录](docs/notes/taa-f5912-f16385-2026-09-13.md)及[启动覆盖记录](docs/notes/shader-startup-coverage-2026-09-13.md)。

Published at [GitHub Release v0.5.10](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.10) on 2026-09-13T21:20:10Z. Release CI [34783107248](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34783107248) passed for source/tag commit `db63ebaf50fed9612ca66a498f162ad6be69a54e`. The clean ZIP is 44,288,884 bytes with SHA-256 `e1b6b9a84bcf0360f104db2e001ca5e740552834b554a4bf5812dd9c8f52f6eb`; runtime SHA-256 is `25c3c83366143b5f74943ee0cd88789cca0042b3d4a06a7ef986fa8d8de94995`. All 50 manifest payload hashes and ZIP CRCs passed; all four public assets matched anonymous HTTP downloads, sizes, SHA-256 and API digests. CI consumed PPC key `481e10e3e18a083fbc8f3207422a2065c5ba548c39b2588336b02f3da75effdf` from private commit `eb883038c92fdfc0e154b5e3f8b61e346a98034e`. Existing functional checks were reused; no new gameplay or frame-time acceptance is claimed.

## v0.5.9 — 2026-09-13 / Published / 已发布

### English

- Add conservative Vulkan depth-clear coalescing for 720 EDRAM tile rectangles, preserving D3D12 mapping output, holes and uncleared regions. A matched 45-second static 4K Vulkan observation at 60 W improved mean FPS from 7.49638 to 43.47614 and GPU time from 132.91221 ms to 21.06364 ms; this remains bounded candidate evidence rather than a clean release-build benchmark or 4K60/1080p60/15 W acceptance.
- Add texture-key avalanche mixing, captured-shader identity caching, same-handle graphics descriptor binding suppression, same-framebuffer Plume rebind suppression and per-`GpuSlot` immutable descriptor reuse while retaining the two-slot/fence contract. The binding-cache fixture passed duplicate, replacement, incompatible-prefix and post-fence cases.
- Retain the v0.5.8 TAA behavior without changing its mappings. Player acceptance and 1080p60 at 15 W remain pending. See the [Vulkan depth-clear performance note](docs/notes/vulkan-depth-clear-performance-2026-09-13.md).

### 简体中文

- 增加保守的 Vulkan 深度清除合并，将 720 个 EDRAM tile rectangle 合并为一个，同时保留 D3D12 映射输出、空洞和未清除区域。在 60 W、固定视角、4K Vulkan 的配对 45 秒观测中，平均 FPS 从 7.49638 提升到 43.47614，GPU 时间从 132.91221 ms 降至 21.06364 ms；这仍是有界候选证据，不是干净 Release 构建基准，也不代表 4K60、1080p60 或 15 W 验收。
- 增加 texture-key avalanche、captured-shader identity 缓存、同 handle graphics descriptor 绑定抑制、同 framebuffer 的 Plume rebind 抑制和每个 `GpuSlot` 的 immutable descriptor 复用，同时保留双 slot/fence 契约。binding-cache fixture 已通过重复、替换、不兼容前缀和 fence 后场景。
- 保留 v0.5.8 的 TAA 行为，不改变现有映射。玩家验收和 15 W 下 1080p60 仍待完成。详见 [Vulkan 深度清除性能记录](docs/notes/vulkan-depth-clear-performance-2026-09-13.md)。

Published at [GitHub Release v0.5.9](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.9) on 2026-09-13 19:50:00 UTC. Release CI [34778434518](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34778434518) succeeded for source/tag commit `d26ee8b021784d7232b5319d816227867f98050d`. The clean package is 44,269,995 bytes with SHA-256 `fd71bf65f92b242f81a350b5b6e97ea7f1991107a2c30e91ece2dd261bac0591`; all 50 manifest payload hashes and CRCs passed, and the runtime hash is `ef93c01db40433fb6d463357ea3e6979b3a1f45cf0d2d3f81be2fcf3f8885faa`. Four public assets matched anonymous HTTP, size, hash and API-digest checks. CI consumed PPC key `ab194913725bd44df7ea9e248d4e60c561ac4d73f7b87d4c5bb080613bc5567a` from private commit `6433064e547a9460249b1162b938ac0b2c332688`; no PPC recompile was performed.

## v0.5.8 — 2026-09-13 / Published / 已发布

### English

- Extend current-scene TAA jitter coverage with seven reviewed main-camera vertex-shader paths; bounded CPU evidence and static discovery are retained, while original-scene visual acceptance remains pending.
- Add the bounded 64-byte sampled-content SIMD comparison and cache `LO_QUERY_TRACE` presence across query hooks. Focused fixtures passed on Clang 19.1.5 and 22.1.8; limited 40 W comparisons remain around 47 FPS at 4K and do not establish stable whole-game performance or power gains.
- Synchronize the private PPC cache `main` branch with its verified input and compile contract; Release CI and package provenance passed for v0.5.8.

### 简体中文

- 扩展当前场景 TAA jitter 覆盖，加入七条已审阅的主相机顶点 shader 路径；保留限定 CPU 证据和静态扫描结果，原场景画面验收仍待完成。
- 增加有界的 64 字节 sampled-content SIMD 比较，并在 query hook 间缓存 `LO_QUERY_TRACE` presence。Clang 19.1.5 和 22.1.8 的定向夹具均通过；有限 40 W 对照在 4K 仍约 47 FPS，不能证明稳定的全游戏性能或功耗收益。
- 同步带有已核验输入和编译契约的私有 PPC cache `main` 分支；v0.5.8 的 Release CI 与包 provenance 已通过。

Published at [GitHub Release v0.5.8](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.8) on 2026-09-13. Release CI `34764115203` passed for source/tag commit `6e6f11cf56ef69082f5be5b049e5d48d58154415`; ZIP verification covered all 50 manifest files, CRCs, version 0.5.8 and clean commit provenance. PPC key `ec7f34708ff870d6ec940a7a4fe83686d4ec5802344934c9084b85e2cf113c3d` matched private commit `65a6869ea7ec36987f1ca7026c0588d4fa6c40d7`. Anonymous downloads of all four published assets matched bytes, hashes, sidecars and API digests. The 40 W benchmark and TAA visual-acceptance limits remain separate from package validation.

## v0.5.7 — 2026-09-13 / Published / 已发布

### English

- Allow an updater-only installation, stale or malformed local metadata, or a missing game executable to use the latest-release recovery path, then ask whether to launch the game with **No** as the default. Download integrity checks, safe extraction and rollback remain enabled.
- Remove proven inactive shadow-loop work and reuse identical adjacent color-resolve copies within a command batch; shader cache version 22 rejects older binaries and startup bundles. Offline shader, cache and resolve checks passed, while gameplay, hardware-power and cross-scene validation remain separate.
- Include the guarded HDR16 TAA bloom prefilter and the material vertex-shader jitter repair. The user accepted the HDR-off/materials-on fix for the reported lighting-flicker scene; other scenes and hardware remain unverified.

Published at [GitHub Release v0.5.7](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.7) on 2026-09-13 06:51:32 UTC. Release CI run `34743383193` produced the verified package; package hashes and provenance are recorded in `docs/STATUS.md` and `out/release-v0.5.7/`.

### 简体中文

- 允许 updater-only 安装、过期或损坏的本地 metadata 以及缺少游戏可执行文件的安装使用最新 Release 恢复路径，随后询问是否启动游戏并默认选择**否**。下载完整性校验、安全解压和回滚仍然保留。
- 消除已证明安全的阴影循环空转，并在同一 command batch 内复用相邻且完全相同的 color-resolve 复制；shader cache 版本 22 拒绝旧二进制和启动 bundle。离线 shader、cache 与 resolve 检查已通过，游戏、硬件功耗和跨场景验证仍需单独判断。
- 纳入有界的 HDR16 TAA bloom prefilter 与材质顶点 shader jitter 修复。用户已在报告的光影闪烁场景接受 HDR 关闭、materials 开启的修复；其他场景和硬件仍未覆盖。

已于 2026-09-13 06:51:32 UTC 发布 [GitHub Release v0.5.7](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.7)。Release CI `34743383193` 生成的包已完成校验；包哈希和来源记录见 `docs/STATUS.md` 与 `out/release-v0.5.7/`。

## v0.5.6-hotfix1 development record / 开发记录

This development record was never released separately; its accepted scope was included in v0.5.7. / 本开发记录从未单独发布；其中已接受的范围已纳入 v0.5.7。

### English

- Simplify standalone updater recovery: an empty updater-only folder, stale or malformed metadata, a development package, or a modified executable can use the latest-release update path. `source-version.txt` is preferred; a valid version-only `manifest.json` is a fallback, and unknown metadata uses `0.0.0`. Download SHA-256 verification, safe archive extraction, transaction rollback and update path-safety checks remain enabled.
- After a successful update, the local helper asks whether to launch the game and defaults to **No**. Silent mode performs the update without launching; failed updates do not restart the game, and a requested launch failure preserves the installed update. The updater copies the locally installed helper into the handoff runner so this completion policy remains active even when the downloaded package contains an older helper.
- Focused checks recorded before the suffix-only version metadata change passed on source version 0.5.6: `LoUpdaterStandaloneTest` 44/44, updater version/asset/integrity/staging/rollback/helper/preservation checks, and the Unicode caller-CWD helper-context check. These are synthetic hidden-process checks; no real game, public download or visible Yes/No dialog interaction was performed.
- Remove inactive shadow-loop iterations where safety can be proved, and skip identical adjacent color-resolve copies within one command batch. Shader cache version 22 rejects older binaries and startup bundles. Local shader and GPU pixel checks passed; the historical validation limits are retained in the audit note.
- Add a guarded TAA bloom prefilter candidate for the identified HDR16 bloom inputs larger than 1280x720 and within the supported 8x extent, with linear reconstruction after the area filter and a `LoBloomPrefilterTest` fixture. The measured 3840x2160-to-1280x720 case is conditionally enabled for AA3 and the matching scene draw, with `LO_DISABLE_BLOOM_PREFILTER=1` available for comparison. D3D12 and Vulkan fixture runs, captured-input checks and the linear response checks passed. The bloom-only candidates stabilized the upper large robots; the remaining lower-enemy lighting flicker was addressed by the material jitter repair below. Bounded AA3/jitter/history/bloom controls and geometry tracing supported the diagnosis. The candidate history and its validation limits are retained in the audit note.
- Fix three material vertex shader paths that omitted TAA jitter, leaving depth and material positions misaligned; add slot 7 and pass the 131,457-check `LoTemporalJitterTest --captured-static-layers` selector across 32 phases, two worlds and 720p/4K. The user confirmed the HDR-off/materials-on candidate removes the flicker from the whole lighting target in the reproduced scene. This scene acceptance does not establish whole-game or cross-hardware coverage. The accepted scene scope is included in v0.5.7; broader coverage remains unverified.

### 简体中文

- 简化 standalone 更新器的恢复路径：只有 updater 的空目录、过期或损坏的 metadata、开发包或被修改的可执行文件，都可以使用最新 Release 更新。优先读取 `source-version.txt`；否则回退到只含有效版本号的 `manifest.json`，未知 metadata 使用 `0.0.0`。下载 SHA-256 校验、安全 ZIP 解压、事务回滚和更新路径安全检查仍然保留。
- 更新成功后，当前本地 helper 会询问是否启动游戏，默认选择**否**。silent 模式只执行更新而不启动游戏；更新失败不会自动重启游戏；用户请求启动但启动失败时保留已安装的更新。交接 runner 使用本地已安装的 helper，因此即使下载包内含较旧 helper，也会保留当前完成策略。
- 后缀版本 metadata 改动前，以 source version 0.5.6 记录的定向检查已通过：`LoUpdaterStandaloneTest` 44/44、更新器版本／asset／完整性／staging／回滚／helper／保留行为检查，以及 Unicode 调用方工作目录的 helper context 检查。这些是隐藏的合成进程检查，未运行真实游戏、公开下载或可见 Yes/No 对话框交互。
- 在可证明安全的条件下消除阴影循环空转，并跳过同一提交批次内相邻、完全相同的颜色 resolve 复制。shader cache 版本 22 拒绝旧二进制和启动 bundle。本地 shader 与 GPU 像素检查已通过；历史验证边界保留在审查笔记中。
- 为已定位的、大于 1280x720 且不超过 8 倍尺寸的 HDR16 bloom 输入增加有条件启用的 TAA bloom prefilter 候选，并在面积滤波后使用线性重建，同时增加 `LoBloomPrefilterTest` 夹具。实测的 3840x2160 到 1280x720 场景仅在 AA3 和匹配的 scene draw 命中时启用，并可用 `LO_DISABLE_BLOOM_PREFILTER=1` 做对照。D3D12／Vulkan 夹具、捕获输入检查和线性响应检查均已通过。仅包含 bloom 修补的候选稳定了上方大型机器人；下方敌人剩余的光影闪烁由下述材质 jitter 修补解决。有限范围的 AA3／jitter／history／bloom 控制和 geometry trace 用于本次诊断。候选历史和验证边界保留在审查笔记中。
- 修复三条材质顶点 shader 路径漏加 TAA jitter 导致的 depth/material 错位，补充 slot 7，并通过 `LoTemporalJitterTest --captured-static-layers` 的 131,457 项、32 相位、双 world、720p/4K 检查。用户确认 HDR 关闭、materials 开启的候选在复现场景中使整个光影目标不再闪烁。该场景验收不代表全游戏或跨硬件覆盖。已接受的场景范围已纳入 v0.5.7；更广覆盖仍未验证。

## v0.5.6 — 2026-09-13 / 已发布

### English

- Fix updater manifest staging for subsequent transactions using the new `StageArchive`; the focused manifest transaction check passed three scenarios with zero failures. This does not repair already mixed installations, remove old resources or establish the unresolved Issue #15 save-flow hang. See [Issue #14–#16 triage](docs/notes/issues14-16-triage.md) for the #14–#16 investigation boundaries.
- For Issue #16, add a narrow particle-material compatibility fallback for the zero-entry `xf_shd_aniflz.freeze` shader case; `LoParticleMaterialCompatTest` compiled and ran with zero failures. Final-branch D3D12/local Asia Disc 3 validation completed the target freeze sequence and subsequent map229/menu progression. Vulkan, other-region coverage and player acceptance remain pending. See the [triage record](docs/notes/issues14-16-triage.md).
- Build and validate the merged local `main` at source version 0.5.6 with normal CMake Release configuration: the D3D12/local Asia Disc 3 target freeze sequence, map229/menu progression and visible movement all passed on the local binary. Release CI completed the formal build with the matching PPC artifact. The downloaded package passed hash/CRC checks for all 50 manifest files and clean source-version provenance; gameplay validation of the local binary is retained separately. Detailed hashes and boundaries are in [current status](docs/STATUS.md).
- Bound the renderer's vertex metadata cache to 65,536 reserved entries. Full
  caches evict from at most 16 rotating candidates, eliminating the old
  `unordered_map` growth path: the diagnostic capture measured a 41.8241 ms
  insertion during a 262,144-to-524,288 bucket rehash, while all 495 endian
  copies in that frame took 0.0254 ms. `LoVertexCacheTest` passed 3,569,548
  checks once. Three single Hidden Uhra captures (old map, bounded cache and
  the same executable with input/shot controls moved to TEMP) retained
  `original_saves_changed=false`; the final all-city sample reached mean 59.651
  FPS, 1% low 45.989 FPS and worst accepted-present 43.0117 ms. The fixed
  1600–2800 window reached 59.918 FPS mean and 54.495 FPS 1% low, with zero
  draw over-budget samples and zero rehashes. This meets the requested mean
  threshold on the route, but is not a locked 60 FPS result, strict S4 pass,
  whole-game validation or player acceptance. These changes are included in
  v0.5.6; the measurement build is source version
  0.5.4. See [city vertex-cache follow-up](docs/notes/city-60fps-handoff.md).

- Keep `LO_VERTEX_TIMING` disabled by default; the added previous-swap,
  post-present and command-processor-idle fields are diagnostic measurements,
  not optimization savings. The bounded run's 412.9283 ms post-present sample
  fell to 0.4193 ms after moving the driver input/screenshot controls to TEMP;
  this does not establish a Syncthing filesystem or scheduler root cause.

- Use a PPC prebuilt library by default in the release workflow. `release.yml` restores a
  sharded library bundle from the immutable private `ppc/<key>` branch selected by
  the computed inputs/compiler key, while
  manual `rebuild_ppc: true` retains the source-compilation path. Local builds can
  set `LO_PREBUILT_PPC_DIR` to import `LostOdysseyRecompLib.lib` and skip PPC C++
  compilation; clearing it restores the normal source build. The bundle is kept
  in the private input repository and is not a public release artifact.
- Add `tools/release/ppc_prebuilt.py` for incremental PPC export, bundle restore
  and receipt/hash checks. The 13 synthetic bundle checks, local Release/x64
  clang-cl PPC export and isolated prebuilt CMake checks pass. The four-shard
  library is 138,454,798 bytes with SHA256
  `ba3e4c4dff009d6d8e844c007186a6e5040266875bca6423f8fe26f8d27fb21b`; private
  commit `a6cd91ea35261dd202b78e93b4acb65973369d07` was read back and consumed by Release CI.
  The CI-compatible cache preserves the original library and records five line-ending
  and fourteen symlink-representation differences; all 250 generated outputs,
  471 PPC headers and the Release compile contract are identical. Hosted Release CI
  and package verification passed; user acceptance remains separate.

- Add opt-in local PPC auto-sync. Enable it with local Git config
  `git config --local lo.ppcAutoSync true`; the CMake option reads that setting,
  and an existing cached `OFF` value may be reconfigured with
  `-DLO_PPC_AUTO_SYNC=ON`. This does not bypass the script's local opt-in. The
  post-build hook invokes `ppc_sync.py sync --already-built`; ordinary contributors remain off by default. Matching input/compiler
  hashes reuse an existing immutable private branch, while changes publish a
  new `ppc/<key>` branch with dynamically sized shards of at most 40 MiB. CI, imported libraries and
  `LO_PPC_SYNC_ACTIVE` never upload. The auto-sync source is pushed to
  github/main as [`2c0456c`](https://github.com/freefrank/LostOdysseyRecomp/commit/2c0456c).
  Hosted [PPC prebuilt tests](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34565564964)
  passed; hosted Release CI and package verification also passed. Nineteen
  synthetic sync cases pass. Separately, the built-library roundtrip and
  change-during-build checks pass, and the real local auto-sync branch/upload
  plus same-key unchanged check pass. This workflow is included in v0.5.6 and was absent
  from published v0.5.4.

- Add a 2-slot D3D12 command-list ring, raise the D3D12 descriptor-set
  limit to 1800, reuse 2D texture descriptor sets, bind unused 2D/3D/cube
  banks to static dummy sets, use BatchCache last-hit for texture sets,
  and skip unchanged constant uploads. Header fixtures pass: LoRenderBatchPolicyTest
  22/22 and LoTextureDescriptorCacheTest 12/12 (including a 2000 last-hit
  loop). Isolated user01 Uhra city walks on two local RelWithDebInfo
  EXEs are diagnostic only (ring SHA-256
  `5917F389F9FD9E88FDEC6DBD3437ADE76D415F1653FB6924575ACCF478C1B9AD`
  stable city about 57.7 fps, 49.1–60; dummy SHA-256
  `02E303F1462546FB98236446E24B2397DF762179923DE1D7C02852317ED37BC4`
  about 56.0 fps, 28–60, bind-path only with no fps win versus the ring
  run). Published v0.5.4 city diagnostic was 31–44 fps with about 5.2
  batches. The two EXEs are not a laboratory A/B. Commits `b91d279` and `ed90fe9`
  are included in v0.5.6; the cited measurements remain historical diagnostics,
  with no player acceptance or 60 fps claim. See
  [GPU ring compare](docs/notes/perf-gpu-ring-compare.md).

- Move the vertex dword endian copy helper into `geometry_prepare.h` and add an
  SSSE3 four-dword path for endian modes 1/2/3, with scalar tails/fallbacks and
  `memcpy` for endian 0. The focused fixture passed 16,685,865 checks,
  including unaligned and inaccessible-page boundary cases. A same-harness
  Hidden city run completed with `original_saves_changed=false`; the SIMD run
  still had a 41.736 ms vertex hitch, a separate 47.467 ms flush sample and a
  779.572 ms load-in present interval, so the change does not establish a
  performance gain or 60 fps acceptance. Both redirected-log runs avoided the
  earlier 200–400 ms flush class, but residual stalls remain. The implementation
  is included in v0.5.6; the cited local measurement build retains source version
  0.5.4. See [city 60 FPS handoff](docs/notes/city-60fps-handoff.md).

### 简体中文

- 修复使用新版 `StageArchive` 的后续更新事务中的 manifest staging；manifest 事务定向检查 3 个场景零失败。该修复不会自动修复已经混装的安装、清理旧资源，也不能证明 #15 存档流程卡顿的原因。#14–#16 调查边界见[分流记录](docs/notes/issues14-16-triage.md)。
- 针对 Issue #16 的零项 `xf_shd_aniflz.freeze` shader 情况增加窄范围 particle-material 兼容回退；`LoParticleMaterialCompatTest` 已编译并运行且零失败。最终分支 D3D12／亚洲 Disc 3 验证已完成目标冻结过场及后续 map229／菜单流程。Vulkan、其他地区覆盖和玩家验收仍待完成，见[分流记录](docs/notes/issues14-16-triage.md)。
- 使用普通 CMake Release 配置成功构建并验证合入本地 `main` 的 0.5.6 源码：本地二进制已通过 D3D12／亚洲 Disc 3 目标冻结过场、map229／菜单流程和可见移动。Release CI 已使用匹配的 PPC artifact 完成正式构建；下载的安装包通过全部 50 个 manifest 文件的 hash／CRC 及干净源码版本 provenance 检查。本地二进制的游戏实测记录保持独立。详细 hash 和边界见[当前状态](docs/STATUS.md)。
- 将渲染器顶点 metadata cache 限制为预留 65,536 项。缓存满时最多检查 16 个轮转候选并淘汰，消除了旧
  `unordered_map` 扩容路径：诊断捕获中一次 262,144 到 524,288 bucket 的 rehash 插入耗时 41.8241 ms，
  而该帧全部 495 次 endian copy 合计仅 0.0254 ms。`LoVertexCacheTest` 一次通过 3,569,548 项检查。
  三次单独 Hidden 乌拉住宅区捕获（旧 map、有界缓存、以及将输入／截图控制移到 TEMP 的同一 EXE）均为
  `original_saves_changed=false`；最终全城市样本平均 59.651 FPS、1% low 45.989 FPS，最差 accepted-present
  间隔 43.0117 ms。固定 1600–2800 窗口平均 59.918 FPS、1% low 54.495 FPS，draw 超预算为 0、rehash 为 0。
  这满足该路线请求的平均帧率门槛，但不能称为全程锁 60 FPS、严格 S4 通过、全游戏验证或玩家验收。
  这些改动包含在 0.5.6 中；测量构建仍为 source version 0.5.4。见[城市 vertex-cache 后续记录](docs/notes/city-60fps-handoff.md)。

- `LO_VERTEX_TIMING` 默认关闭；新增的 previous-swap、post-present 和 command-processor-idle 字段是诊断测量，
  不是优化收益。有界缓存运行中的 412.9283 ms post-present 样本，在将驱动输入／截图控制移到 TEMP 后降至
  0.4193 ms；这不能证明 Syncthing 文件系统或调度是根因。

- 发布流程默认使用 PPC 预编译库。`release.yml` 根据输入／编译参数 key 从私有
  不可变的 `ppc/<key>` branch 恢复分片库；手动设置 `rebuild_ppc: true` 仍使用源码编译
  路径。本地构建可设置 `LO_PREBUILT_PPC_DIR` 导入 `LostOdysseyRecompLib.lib`
  并跳过 PPC C++ 编译；清除该变量即可恢复普通源码构建。分片库保存在私有输入
  仓库中，不作为公共发布产物。
- 增加 `tools/release/ppc_prebuilt.py`，支持增量导出 PPC 库、恢复 bundle 以及
  receipt／hash 校验。13 项合成 bundle 检查、本地 Release／x64 clang-cl PPC
  导出和隔离 prebuilt CMake 检查均已通过。四片库大小为 138,454,798 字节，SHA256
  为 `ba3e4c4dff009d6d8e844c007186a6e5040266875bca6423f8fe26f8d27fb21b`；私有
  commit `a6cd91ea35261dd202b78e93b4acb65973369d07` 已远端读回，并由 Release CI 实际使用。
  CI 兼容缓存保留原库，记录五项行尾及十四项符号链接表示差异；全部 250 个生成文件、471 个 PPC 头文件和
  Release 编译契约完全相同。托管 Release CI 和包检查已通过；用户验收保持独立。

- 增加可选的本地 PPC 自动同步。必须先用 Git 本地配置
  `git config --local lo.ppcAutoSync true` 启用；CMake 选项读取该设置，已有缓存
  为 OFF 时需重新配置并传入 `-DLO_PPC_AUTO_SYNC=ON`，不能绕过脚本授权。post-build
  hook 调用 `ppc_sync.py sync --already-built`；普通贡献者默认关闭。输入与编译参数
  hash 相同则复用已有不可变私有 branch，变化时创建新的 `ppc/<key>` branch 并上传每片
  不超过 40 MiB 的动态分片。CI、导入库和
  `LO_PPC_SYNC_ACTIVE` 不会上传。auto-sync 源码已推送到 github/main，提交为
  [`2c0456c`](https://github.com/freefrank/LostOdysseyRecomp/commit/2c0456c)。托管
  [PPC prebuilt tests](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34565564964)
  已通过；托管 Release CI 和包检查也已通过。19 项合成同步用例通过；另外，built-library
  roundtrip、change-during-build、真实本地自动同步 branch／上传及同 key unchanged
  检查均已通过。该流程包含在 v0.5.6 中，此前未包含在已发布的 v0.5.4 中。

- 增加 D3D12 双槽 command-list 环缓冲，将 D3D12 描述符集上限提到 1800，
  复用 2D 纹理描述符集，未使用的 2D／3D／cube bank 绑定静态 dummy 集，
  纹理集使用 BatchCache last-hit，并跳过未变化的常量上传。头文件夹具通过：
  LoRenderBatchPolicyTest 22/22、LoTextureDescriptorCacheTest 12/12
  （含 2000 次 last-hit 循环）。两份本地 RelWithDebInfo EXE 的隔离
  user01 乌拉城市走图仅为诊断（环缓冲 SHA-256
  `5917F389F9FD9E88FDEC6DBD3437ADE76D415F1653FB6924575ACCF478C1B9AD`
  稳定段约 57.7 fps，49.1–60；dummy SHA-256
  `02E303F1462546FB98236446E24B2397DF762179923DE1D7C02852317ED37BC4`
  约 56.0 fps，28–60，仅 bind 路径，相对环缓冲一轮没有帧率收益）。
  已发布 v0.5.4 城市诊断为 31–44 fps、约 5.2 个 batch。两份 EXE 不是
  实验室 A/B。提交 `b91d279`、`ed90fe9` 包含在 v0.5.6 中；上述测量仍为历史诊断，
  不构成玩家验收，也不宣称 60 fps。
  见[GPU 环缓冲实测对比](docs/notes/perf-gpu-ring-compare.md)。

- 将顶点 dword endian copy helper 移到 `geometry_prepare.h`，为 endian 1／2／3
  增加每次处理四个 dword 的 SSSE3 路径，并保留 scalar 尾部／fallback，endian 0
  使用 `memcpy`。专项夹具通过 16,685,865 项检查，包括非对齐和不可访问页边界。
  同一 Hidden 城市脚本的复测为 `original_saves_changed=false`；SIMD 运行仍有
  41.736 ms 顶点卡顿、另一个 47.467 ms flush 样本以及 779.572 ms 的载入期
  present 间隔，因此不能据此宣称性能提升或 60 fps 验收。两次重定向日志运行
  都未复现之前 200–400 ms 的 flush 类别，但残余卡顿仍在。该实现包含在 v0.5.6 中，
  上述本地测量构建仍保留 source version 0.5.4。见[城市 60 FPS handoff](docs/notes/city-60fps-handoff.md)。

## v0.5.4 — 2026-09-11

### English

- Guard PPC source generation with binary/source receipts and generated-output manifests; preserve prior output on failure and reject stale inputs or 64-bit jump-table switches.
- Add an optional Win64 external assembly profiler with bounded sampling and offline Capstone HTML/JSON reports.
- Increase the F1 menu ZIP archive wait from 60 to 180 seconds for large captures.
- Fix installer drag dispatch by posting signed-coordinate `WM_NCLBUTTONDOWN`; the reporter confirmed the fix.

The release retains the documented validation boundaries in [current status](docs/STATUS.md); no whole-game, visual or complete F1 acceptance is implied.

### 简体中文

- 为 PPC 源码生成增加二进制／源码 receipt 和生成输出 manifest；失败时保留旧输出，并拒绝过期输入或 64 位跳转表 switch。
- 增加可选的 Win64 外部汇编分析器，支持有界采样和离线 Capstone HTML／JSON 报告。
- 将大体积 F1 菜单 ZIP 归档等待时间从 60 秒延长至 180 秒。
- 通过发送带符号坐标的 `WM_NCLBUTTONDOWN` 修复安装器拖动分发；报告者已确认修复。

本版本保留[当前状态](docs/STATUS.md)中的验证边界；不代表全游戏、画面或完整 F1 流程验收完成。

## v0.5.3 — 2026-09-10

### English

- Add compact opt-in TAA diagnostics using bounded 32-frame CPU windows, requests capped at 32 KiB and a 180-second cadence, with delivery receipts and no upload wait on the game thread.
- Add bounded TAA consumer and texture-producer binding evidence for shader review, while keeping jitter mapping and visual acceptance separate.
- Archive opted-in feedback daily with content deduplication and maintain a research analysis ledger; D1's 30-day inactive-record expiry remains separate from the long-term Git archive.
- Fix updater staging for ZIP packages with an explicit root-directory entry.

TAA remains experimental; this release does not claim a new player visual acceptance or a flicker fix.

### 简体中文

- 增加有界 32 帧 CPU 窗口的 opt-in TAA compact 诊断，载荷上限 32 KiB、采集间隔 180 秒，提供投递回执且上传无需等待游戏线程。
- 增加供 shader 审阅使用的 TAA 消费者与纹理生产者绑定证据；jitter 映射和画面验收仍单独判断。
- 每日归档已同意的反馈并按内容去重，同时维护研发分析账本；D1 的 30 天未活跃记录过期规则与长期 Git 归档分开执行。
- 修复带显式根目录条目的 ZIP 包暂存处理。

TAA 仍处于实验阶段；本版本不宣称新的玩家画面验收结果，也不宣称修复闪烁问题。

## v0.5.2 — 2026-09-10

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
