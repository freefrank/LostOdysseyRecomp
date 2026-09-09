# Changelog / 更新日志

One record of completed changes, with unpublished work separated from verified releases. Dates below are UTC release dates. Planned work belongs in the [roadmap](docs/ROADMAP.md), not release entries.

本文统一记录已完成改动，并区分未发布内容与已确认发布版本；日期采用 UTC 发布日期。后续计划见[路线图](docs/ROADMAP.zh-CN.md)，不作为已发布功能记录。

## v0.5.0 — Unreleased / 未发布

Release scope is Windows D3D12 and Vulkan; DX11 is future work. Intermediate 0.4.xx versions remain internal and will be delivered together as v0.5.0. Installer, updater and Debug Menu UI passes are locally validated, and the 0.4.18 development build/package is recorded in the build report; publication remains pending.

发布范围为 Windows D3D12 与 Vulkan，DX11 属于未来工作。0.4.xx 保持内部开发编号，积累至 v0.5.0 一起发布。安装器、更新器与 Debug Menu UI 改进已在本地完成相称验证，0.4.18 开发构建与打包证据已记录在构建报告中；发布仍待进行。

Current source is **0.4.23**. The 13 historical feature batches through 0.4.15 and the three committed UI batches through 0.4.18 are listed below; local 0.4.19 DPI, 0.4.20 GC-validation, 0.4.21 DLC-filter, 0.4.22 menu-asset and 0.4.23 menu-UX source changes remain local and unpublished. The GC implementation itself was merged as `17e3ab7` and included in the GitHub backup. The delivery target remains **v0.5.0** and the published baseline remains **v0.4.2**. Earlier test binaries with matching version strings are separate artifacts and must be identified by their hashes.

当前源码为 **0.4.23**；下表保留截至 0.4.15 的 13 个历史功能提交，以及截至 0.4.18 的 3 个已提交 UI 批次；0.4.19 DPI、0.4.20 GC 验证记录、0.4.21 DLC 过滤、0.4.22 菜单素材和 0.4.23 菜单 UX 源码均为本地未发布内容。GC 实现已以 `17e3ab7` 合并并包含在 GitHub 备份中。交付目标仍是 **v0.5.0**，已发布基线仍为 **v0.4.2**。此前可能同名的测试程序属于其他产物，须以哈希区分。

| Source / 源码 | Commit / 提交 | Change / 改动 |
| --- | --- | --- |
| `0.4.3` | `03c8385` | resolve portable game paths / 便携游戏路径解析 |
| `0.4.4` | `a79588d` | add recomp executable identity / Recomp 程序图标 |
| `0.4.5` | `cc4a97c` | discover and import disc layouts / 光盘结构识别与安全导入 |
| `0.4.6` | `3f858f8` | modernize first-run setup / 独立首次启动设置 |
| `0.4.7` | `2c81dc1` | streamline the debug menu / 轻量 Debug Menu |
| `0.4.8` | `7569f9d` | restyle settings and offer safe restart / 设置菜单风格与安全重启 |
| `0.4.9` | `fc075dd` | add transactional updates and package provenance / 自动更新与分发包来源 |
| `0.4.10` | `3eb220d` | support Windows Vulkan and parallel shader preparation / Windows Vulkan 与并行 shader 准备 |
| `0.4.11` | `2dc7f57` | reduce guest polling and capture overhead / 降低 CPU polling 与调试开销 |
| `0.4.12` | `e2c8a48` | reuse startup shader bundles and known failures / 复用启动 bundle 与已知编译失败 |
| `0.4.13` | `84929bc` | validate backend capabilities and isolate caches / 后端能力检查、回退与缓存隔离 |
| `0.4.14` | `f513ec4` | balance backend window lifecycle resources / 后端窗口生命周期清理 |
| `0.4.15` | `f77d943` | import DLC and automatically recognize content / DLC 导入与自动内容识别 |
| `0.4.16` | `b094a1a` | installer window and interaction modernization / 安装器窗口与交互现代化 |
| `0.4.17` | `9c2dc76` | updater window and progress modernization / 更新器窗口与进度现代化 |
| `0.4.18` | `294df07` | Debug Menu window and diagnostics modernization / Debug Menu 窗口与诊断现代化 |
| `0.4.19` | local, uncommitted | physical-pixel game-window DPI lifecycle / 游戏主窗口物理像素 DPI 生命周期 |
| `0.4.20` | local record, unpublished | Issue #12 GC/render-thread flush production validation / Issue #12 GC/渲染线程排空生产验证 |
| `0.4.21` | local, uncommitted | real DLC directory-filter runtime validation / 真实 DLC 目录过滤运行验证 |
| `0.4.22` | local, uncommitted | native original menu asset loading and bounded fallback validation / 原版菜单素材读取与限定回退验证 |
| `0.4.23` | local, uncommitted | single-click graphics save, direct guest menu return and consistent original-font labels / 图形设置单击保存、直接返回与原版字体标签统一 |

### 0.4.22 — Original menu assets / 原版菜单素材

The Settings renderer reads the selected installed `LO.fpi` language package and uses its native `Maru23` font data and `UI_MAIN_00` grey panel/gear assets. An optional same-package `Abc` whole-string fallback supplies the multiplication sign in resolution labels; missing packages, unsupported formats and uncovered glyphs retain GDI fallback. Direct run-05/run-06 checks passed the selected English/Simplified Chinese paths, three decoded-output byte comparisons, malformed/cache/LZO negative cases and the final 41-original-font/2-GDI coverage result. Menu geometry, input, save/restart transactions and presentation behavior were unchanged. The local 0.4.22 link/package completed successfully: `LostOdysseyRecomp-windows-x64-v0.4.22-df59dcab-dev.zip`, 44,082,161 bytes, SHA256 `cc21dc626862f829831e9568e0547899b8b5b293522580407ddc65ef67cd88f2`; runtime/helper/source identities and the build record are in `out/v0.5.0/final-preparation/build-0.4.22/execution-result.json`. User visual acceptance and publication remain separate; other editions and full language coverage are unverified. Evidence: `out/v0.5.0/final-preparation/menu-assets/INTEGRATION-REPORT.md`, `ABC-CHECKPOINT-MANIFEST.json` and [the public asset note](docs/notes/menu-original-assets.md).

设置渲染器读取用户所选已安装 `LO.fpi` 语言包，使用其中的原生 `Maru23` 字体数据及 `UI_MAIN_00` 灰色面板／齿轮素材。同包可选 `Abc` 整句回退为分辨率标签提供乘号；缺包、不支持的格式和未覆盖字形仍回退 GDI。run-05/run-06 的直接检查通过限定英文／简体中文路径、3 项解码输出字节对照、malformed/cache/LZO 负例，以及最终 41 次原字体／2 次 GDI 覆盖结果。菜单几何、输入、存档／重启事务和 presenter 行为未改变。本地 0.4.22 链接／打包已成功完成：`LostOdysseyRecomp-windows-x64-v0.4.22-df59dcab-dev.zip`，44,082,161 字节，SHA256 为 `cc21dc626862f829831e9568e0547899b8b5b293522580407ddc65ef67cd88f2`；runtime/helper/source 身份及构建记录见 `out/v0.5.0/final-preparation/build-0.4.22/execution-result.json`。用户视觉验收和公开发布仍分开；其他版本和完整语言覆盖尚未验证。证据见 `out/v0.5.0/final-preparation/menu-assets/INTEGRATION-REPORT.md`、`ABC-CHECKPOINT-MANIFEST.json` 和[公开素材说明](docs/notes/menu-original-assets.md)。

Completed behavior includes portable game discovery, the installer and desktop menus, Windows updater support, Vulkan rendering and shader preparation, measured CPU reductions, typed backend caches and failure fallback. The user accepted the bounded D3D12/Vulkan scenes and one-way Xenia-to-Recomp save compatibility. Existing proportionate checks and source equivalence were reused for the commit organization; intermediate commits were not separately built or run. The earlier source-backup checkpoint itself added no build, tests, package, tag or public release; the later 0.4.20 GC validation is recorded separately below. See [development status](docs/STATUS.md) and the [release preparation matrix](docs/RELEASE-v0.5.0.md) for evidence and remaining publication gates.

已完成便携游戏路径识别、导入器与桌面菜单、Windows updater、Vulkan 渲染和 shader 准备、实测 CPU 开销降低、后端缓存隔离及失败回退。用户已验收限定范围内的 D3D12/Vulkan 场景，并确认 Xenia 存档可单向复制到 Recomp 使用。提交整理复用已有适度验证并核对源码一致性，未逐提交构建或运行。此前源码备份步骤本身不新增构建、测试、分发包、tag 或公开 Release；后续 0.4.20 GC 验证另见下文。验证依据与剩余发布事项见[开发状态](docs/STATUS.md)及[发布准备矩阵](docs/RELEASE-v0.5.0.md)。

### 0.4.16 — Installer window and interaction / 安装器窗口与交互

Use a restrained navy/silver interface with a native borderless Windows frame, short action labels, a scrollable content review and full path details. Unknown totals show indeterminate progress; cancellation, retry and closing wait for the worker's safe completion. Unchanged polling values do not redraw the interface. Nine direct UI checks and two affected controller checks passed, with normal/minimum-size real renders reviewed on an inactive desktop. Import backends are unchanged. This is a local source change, not a published package.

安装器采用克制的深蓝／银色界面与原生无边框窗口，精简操作文案，支持内容表格滚动和完整路径查看。未知总量显示不定进度；取消、重试与关闭等待后台操作安全完成，未变化的轮询数值不触发重绘。9 项直接 UI 检查及 2 项受影响的控制器检查通过，已审阅未激活桌面中实绘的正常／最小尺寸界面。导入后端保持不变；本项为本地源码改动，尚未发布。

### 0.4.17 — Updater window and progress / 更新器窗口与进度

The updater now uses a native lightweight borderless frame with short localized phase labels, byte and percentage reporting, indeterminate unknown totals and safe cancellation boundaries. Verification, package checking and ready states disable cancellation; unchanged values do not invalidate controls, and late progress cannot replace `Cancelling…`. The public progress API and transaction/network behavior are unchanged. Focused fixture checks passed, and normal, unknown-total and narrow Chinese renders were reviewed on an inactive desktop. Physical monitor moves and live user-desktop gestures were not tested. This is a local source change, not a published package.

更新器采用原生轻量无边框窗口、精简本地化阶段文案、字节与百分比显示；未知总量使用不定进度，并在下载阶段保留安全取消边界。校验、检查和就绪状态禁用取消；未变化的数值不触发控件重绘，迟到的进度不会覆盖“正在取消…”。公开 progress API 及事务／网络行为保持不变。专项 fixture 检查通过，并已审阅未激活桌面中的正常、未知总量和窄中文实绘。未测试物理跨显示器移动和用户桌面实时手势；本项为本地源码改动，尚未发布。

### 0.4.18 — Debug Menu window and diagnostics / Debug Menu 窗口与诊断

The Debug Menu now uses a lightweight native borderless window with shorter status copy, paged Overview and Teleport content, a fixed title area, independent scrolling, and DPI-correct Toggle/Update layout. Long paths and result states remain readable, while F1/Escape/Gamepad-B and caption close behavior remain available. The focused fixture passed pagination, focus/close, bilingual busy/error, current-DPI metrics and cleanup checks; evidence is `out/v0.5.0/ui-modernization/native/debug-run.log`. The fixture remains source-local evidence; the separate 0.4.18 combined development build/package is recorded in `out/v0.5.0/ui-modernization/build/REPORT.md`, without establishing manual user acceptance or a public release.

Debug Menu 现在使用轻量原生无边框窗口，采用更短的状态文案，Overview 与 Teleport 支持分页，标题区域固定、内容独立滚动，Toggle／Update 在当前 DPI 下正确布局。长路径和结果状态保持可读，F1／Escape／手柄 B 及标题栏关闭行为仍可用。专项 fixture 已通过分页、焦点／关闭、双语忙碌／错误、当前 DPI 指标和清理检查；证据见 `out/v0.5.0/ui-modernization/native/debug-run.log`。该 fixture 仍是源码级证据；独立的 0.4.18 合并开发构建与打包已记录在 `out/v0.5.0/ui-modernization/build/REPORT.md`，不代表人工用户验收或公开发布。

### 0.4.19 — Physical-pixel game-window DPI lifecycle / 游戏主窗口物理像素 DPI 生命周期

The local 0.4.19 source change keeps the game window's client/presentation size in physical pixels across the video thread's full init, event and destruction lifecycle. `SDL_WINDOWS_DPI_SCALING=0` is fixed before window initialization and the thread awareness is restored at scope end; auxiliary UI behavior is unchanged. The production-validation build compiled the video, filesystem and GC translation units, reused the fixed guest object (`d553…`) and completed one real runtime link; it did not rebuild the guest library, PCH or helper. Window integration then ran Map 109 at 96 DPI with `SDL_app` PMv2, confirmed a 1280×720 physical client against configuration, remained hidden and non-foreground, and left the foreground window unchanged. The earlier hidden fixture also passed. 125/150/200 percent desktop and real cross-monitor coverage remain untested. This is local, uncommitted source evidence and is not published in v0.4.2.

本地 0.4.19 源码改动使游戏窗口在 video 线程完整 init、event 与 destruction 生命周期内保持物理像素 client/presentation 尺寸。窗口初始化前固定 `SDL_WINDOWS_DPI_SCALING=0`，scope 结束时恢复线程 awareness；辅助 UI 行为不变。生产验证构建成功编译 video、filesystem 与 GC 三个 translation unit，复用固定 guest object（`d553…`）并完成一次实际 runtime link；未重编 guest library、PCH 或 helper。窗口集成随后在 96 DPI、`SDL_app` PMv2 的 Map 109 运行中通过，1280×720 物理 client 与配置一致，窗口保持 hidden/non-foreground，前台窗口未改变；此前 hidden fixture 也通过。125/150/200% 桌面及真实跨屏覆盖仍未测试。本项是本地未提交源码证据，未发布到 v0.4.2。

### Issue #12 diagnostic repair / Issue #12 诊断修复

The merged source repair drains the rendering thread before the relevant garbage-collection purge path, preventing the stale scene proxy from drawing freed material instances in the reproduced race. Run-13 reproduced five stale-material draws with an artificial 20 ms render delay; run-14 used the same diagnostic setup and observed zero stale draws and zero crashes after the flush. The GC repair, recorded at source 0.4.20 after validation, passed the bounded production hand-in path with `17e3ab7` and current `poll_wait`, without probe, overlay, delay or stale-skip controls. The frozen validation EXE retains source 0.4.18 identity (SHA prefix `7ccfdea7…`) and must not be relabeled 0.4.20. Native user08 independently reached Map 109, crossed the former crash point, completed the Kaim branch dialogue and regained movement during the approximately 148-second crash-free observation window after the critical A press; the owner then ended the process. This is not natural-shutdown coverage or an exact GC timing benchmark. Reporter acceptance, full-game coverage, additional candidate regression and release remain pending; Issue #12 remains OPEN. The 0.4.20 version record is local and unpublished; the merged GC implementation is included in the GitHub backup.

合并源码修复在相关垃圾回收 purge 路径前排空渲染线程，避免已复现竞争中的旧场景 proxy 绘制已释放的材质实例。run-13 使用人工 20 ms 渲染延迟复现 5 次悬空材质绘制；run-14 在相同诊断条件下加入 flush 后观察到 0 次悬空绘制、0 次崩溃。记为 0.4.20 功能增量的 GC 修复已用 `17e3ab7` 和现行 `poll_wait` 完成有界生产交花路径，未使用 probe、overlay、delay 或 stale-skip 控制。冻结验证 EXE 保留 0.4.18 源码身份（SHA 前缀 `7ccfdea7…`），不得改称 0.4.20。原生 user08 独立进入 Map 109，越过旧崩溃点，在关键 A 后约 148 秒的无崩溃观察窗口内完成 Kaim 分支对白并恢复移动；随后由 owner 结束进程。本次不覆盖自然退出，也不是精确 GC 耗时基准。报告者验收、全游戏覆盖、额外候选回归和发布仍待完成；Issue #12 仍为 OPEN。0.4.20 版本记录为本地未发布内容；GC 实现已合并并包含在 GitHub 备份中。

### 0.4.21 — DLC directory filtering / DLC 目录过滤

Retain each directory handle's search pattern when `FindNext` continues with a null or empty pattern, preventing `spa.bin` from being selected as `*.fpi`. The old-object control fails and the corrected native fixture passes 30 calls. The production runtime then reads all three real DLC packages successfully and reaches the main menu; rewards and dungeon gameplay remain untested.

`FindNext` 传入 null 或空 pattern 续查时保留该目录句柄的过滤条件，避免将 `spa.bin` 误选为 `*.fpi`。旧对象对照失败，修复后的原生 fixture 通过 30 次调用；随后生产程序成功读取三份真实 DLC 并进入主菜单。奖励领取及地下城游玩仍未测试。

### 0.4.23 — Menu UX correction / 菜单 UX 修正

Source 0.4.23 maps the Simplified Chinese labels to `反走样` and `画面速率` using existing original glyphs. Graphics settings save/apply on one click; restart-required changes ask only Now/Later after a successful save. The bounded replacement-flow fixture passed, and one hidden Windowed D3D12 runtime path saved 1600×900 at 144 DPI, returned directly to System Settings after Back and preserved settings bytes. The development package is local and unpublished; no new user visual acceptance or public release is claimed. Evidence: `out/v0.5.0/settings-replacement-flow/{fixture-result.json,runtime-01/result.json}` and the package under `out/v0.5.0/settings-replacement-flow/packages/`.

简体中文标签改为使用原版字形的“反走样”和“画面速率”。图形设置单击直接保存并应用；需要重启的修改在保存成功后仅询问 Now/Later。窄 replacement-flow fixture 通过；一次隐藏 Windowed D3D12 流程在 144 DPI 保存 1600×900，Back 后直接回到 System Settings 且设置字节保持一致。开发包为本地未发布产物；不宣称新一轮用户视觉验收或公开发布。

### DLC import and automatic content recognition

- Add Lost Odyssey STFS DLC import to the v0.5.0 milestone. InstallGame now uses one **Files** or **Folder** flow to recognize game discs, DLC, and mixed selections from content headers and structure, presents one review, and imports discs before saving the shared path and importing DLC. A failed or cancelled DLC stage preserves completed discs and offers only remaining DLC on retry; a path-save failure warns without rolling back completed imports, and DLC-only imports leave `game-path.txt` unchanged. Nested unknown extensions receive a bounded ISO descriptor probe, while manually selected files and `.iso` inputs retain the bounded padded-image search. / 在 v0.5.0 里程碑中加入失落的奥德赛 STFS DLC 导入。InstallGame 现在通过统一的 **Files** 或 **Folder** 流程，根据内容 header 和结构自动识别游戏光盘、DLC 及混合输入，统一显示审查结果，并按光盘、共享路径保存、DLC 的顺序导入。DLC 阶段失败或取消时保留已完成光盘，只提供剩余 DLC 重试；路径保存失败只警告、不回滚已完成导入；纯 DLC 导入不修改 `game-path.txt`。嵌套未知扩展名执行有界 ISO descriptor 探测，手选文件和 `.iso` 输入保留有界填充镜像搜索。
- Validation: 13 new automatic-import cases and 2 directly affected GUI cases passed on the first run in 0.934 seconds. Twenty unchanged DLC importer cases, two native modes and the independent STFS review were reused. No Tk, game, audio, build or package run was needed for this UX change. Real DLC rewards, areas and edition compatibility remain unverified. / 验证：13 项自动导入新增检查及 2 项直接受影响的 GUI 检查首次运行均通过，用时 0.934 秒；复用 20 项未变的 DLC 导入器检查、2 个原生模式和独立 STFS 审查。本次 UX 改动未启动 Tk、游戏、音频、构建或打包。真实 DLC 奖励、区域及版本兼容性仍待验证。

### Validation boundaries / 验证边界

Vulkan coverage remains Windows/RTX 5080 Maps 2, 3 and 12; other GPUs, full-game compatibility and future macOS/Linux work remain open. Earlier candidate Settings validation used system Trebuchet/CJK fonts and procedural textures, without a pixel-identical art claim; the current 0.4.23 menu-UX implementation is documented above; the 0.4.22 menu-asset package remains historical. DX11 is unsupported at runtime. The preserved candidates stamped v0.5.0 and v0.5.1 remain historical package evidence; their filenames and hashes do not change the v0.5.0 milestone or identify this newly committed source. Hosted CI, public download and release gates remain pending.

Vulkan 证据仍限定为 Windows/RTX 5080 的 Maps 2、3、12；其他 GPU、全游戏兼容性和未来 macOS/Linux 工作仍待完成。此前候选设置验证使用系统 Trebuchet/CJK 字体及程序纹理，未宣称像素级原版美术一致；当前 0.4.23 菜单 UX 实现见上文；0.4.22 菜单素材包保持历史身份。DX11 运行时尚不支持。保留的 v0.5.0、v0.5.1 候选包属于历史分发证据，其文件名和哈希不改变 v0.5.0 里程碑归属，也不代表本次新提交源码。Hosted CI、公开下载与发布检查仍待完成。

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
