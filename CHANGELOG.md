# Changelog / 更新日志

One record of completed changes, with unpublished work separated from verified releases. Dates below are UTC release dates. Planned work belongs in the [roadmap](docs/ROADMAP.md), not release entries.

本文统一记录已完成改动，并区分未发布内容与已确认发布版本；日期采用 UTC 发布日期。后续计划见[路线图](docs/ROADMAP.zh-CN.md)，不作为已发布功能记录。

## Unreleased / 未发布

### English

- Deploy the GitHub Issue triage workflow update at `600b08e` with authorized human `@codex` comment requests, per-comment deduplication, recent human discussion and bounded first-party code retrieval. Nineteen triage/mention checks and seven retrieval checks pass; a real public `@codex` reply remains to be observed.
- On the `legacy` branch, fall back to `MapViewOfFileEx` when `VirtualAlloc2` placeholder reservation fails (Win7/VxKex). A/C aliases remain coherent; the E window is 64 KiB-aligned on that path. Direct3D 11 currently probes the adapter then falls back to D3D12/Vulkan.

### 简体中文

- 已在 `600b08e` 部署 GitHub Issue triage workflow 更新，支持授权协作者的 `@codex` 评论请求、按评论去重、最近人类讨论和有界的 first-party 代码检索。19 项 triage/mention 检查及 7 项 retrieval 检查已通过；首次公开 `@codex` 回复仍待观察。
- `legacy` 分支在 `VirtualAlloc2` placeholder 预留失败时回退到 `MapViewOfFileEx`（Win7/VxKex）。A/C 别名仍保持一致；该路径上 E 窗口按 64 KiB 对齐。Direct3D 11 目前只探测适配器，然后回退到 D3D12/Vulkan。

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
- A local USA/Europe Spanish opening-FMV check passed on 2026-09-14 with `game_language=5` and Vulkan at 3840×2160. German, French and Italian FMV visuals, complete event coverage and whole-game validation remain unverified.

Published at [GitHub Release v0.5.12](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.12) on 2026-09-14T21:34:13Z. Release CI [34895591364](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34895591364) succeeded for source/tag commit `36e574b64e0cbb11ddcdc7f68cbc36214b5c084f` after the PPC cache was synchronized. CI used the prebuilt PPC artifact from `LostOdysseyRecomp-build-inputs`, not `rebuild_ppc`; cache key `cb4a75c2d3fa6e3d5f92e3431e7006f77fd7927dd97f05d27eb488ea4cf87e44` came from private commit `5df5b6efb38537386f6c9522ae9126d800032559`. The Windows ZIP is 44,304,445 bytes with SHA-256 `7cc99618cee509bdea000b736772344de60279f4a439a4f876cc7d49d4c8e60a`; the runtime is 83,536,384 bytes with SHA-256 `d394c173cfa8ecc6ea57c1b9671a1a574ae02d2cd4bb76008392caca66686974`; the updater is 846,336 bytes with SHA-256 `36787c2314d9d554010193a4f020eb8d1a848292920e40af3bb7fb44f1a3893e`. The ZIP hash matches its sidecar, and both public ZIP and `.sha256` downloads returned HTTP 302 to GitHub release assets. Issue [#27](https://github.com/freefrank/LostOdysseyRecomp/issues/27) remains open pending reporter confirmation. Publication checks do not claim 50-file manifest re-verification, German/French/Italian FMV visuals, whole-game validation or reporter acceptance.

### 简体中文

- 修复 USA/Europe FMV／事件字幕语言映射。`82481BE8` hook 现在会为宿主 `GameLanguage()` ID 1–9 返回原可执行文件语言表指针，并保留既有简体中文修复。调用方以 `r4=0` 预筛选时，可解析 INT/JPN/DEU/FRA/SPA/ITA 后缀，而不再别名到 ID-0 英文记录。[#27](https://github.com/freefrank/LostOdysseyRecomp/issues/27)。
- 2026-09-14 本地 USA/Europe 西班牙语开场 FMV 检查通过，`game_language=5`，Vulkan 3840×2160。德语、法语、意大利语 FMV 画面、完整事件覆盖和全游戏验证仍未核验。

已于 2026-09-14T21:34:13Z 发布 [GitHub Release v0.5.12](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.12)。Release CI [34895591364](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34895591364) 使用源码／标签提交 `36e574b64e0cbb11ddcdc7f68cbc36214b5c084f` 成功；PPC 使用 `LostOdysseyRecomp-build-inputs` 中的预编译缓存，而非 `rebuild_ppc`。缓存 key 为 `cb4a75c2d3fa6e3d5f92e3431e7006f77fd7927dd97f05d27eb488ea4cf87e44`，来自私有提交 `5df5b6efb38537386f6c9522ae9126d800032559`。Windows ZIP 大小为 44,304,445 字节，SHA-256 为 `7cc99618cee509bdea000b736772344de60279f4a439a4f876cc7d49d4c8e60a`；runtime 为 83,536,384 字节，SHA-256 为 `d394c173cfa8ecc6ea57c1b9671a1a574ae02d2cd4bb76008392caca66686974`；updater 为 846,336 字节，SHA-256 为 `36787c2314d9d554010193a4f020eb8d1a848292920e40af3bb7fb44f1a3893e`。ZIP 哈希与 sidecar 一致，ZIP 和 `.sha256` 两个公开下载均返回 HTTP 302 到 GitHub release-assets。Issue [#27](https://github.com/freefrank/LostOdysseyRecomp/issues/27) 仍开放，等待报告者确认。发布检查不宣称重新核对 50 文件 manifest、德／法／意 FMV 画面、全游戏验证或报告者验收。

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
