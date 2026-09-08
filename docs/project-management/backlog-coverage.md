# 未完成需求覆盖清单

已冻结 65 个去重工作项，供主代理导入 GitHub Projects。机器清单：[合并后清单 items.json](items.json)。本任务仅编辑这两份本地产物，没有修改 GitHub、构建或操作游戏。

## 字段与状态

- workflow_status：Todo 51；Awaiting validation 3；In Progress 7；Paused 4。
- delivery：Not started 39；Awaiting validation 17；In progress 5；Deferred 4。
- Status/Delivery 已归一到项目选项；详细阶段保留 stage_detail。等待报告者/玩家确认使用 Awaiting validation，不作为技术阻塞。
- 只有显式 PC Vulkan/DX11 目标使用 v0.5.0；其余版本、日期和无依据优先级为 null。
- 未发现明确已取消需求。跨版本语言补丁与 Switch 对齐为 Paused；120 FPS 可以延期，但不等于已取消。

## Issue 复用与独立报告

| Issue | 状态 | 归属与边界 |
|---|---|---|
| [#1](https://github.com/freefrank/LostOdysseyRecomp/issues/1) | OPEN | dx11-feasibility — Reuse open issue once. |
| [#2](https://github.com/freefrank/LostOdysseyRecomp/issues/2) | CLOSED | 独立已关闭报告，由历史清单收录 — Independent closed malware report in historical inventory; list state verified only here, comments not read by this agent. |
| [#3](https://github.com/freefrank/LostOdysseyRecomp/issues/3) | CLOSED | 独立已关闭报告，由历史清单收录 — Independent closed Save and Crash the game report, not RX shadow report; maintainer says should be fixed in 0.2.2, no reporter acceptance in comments. |
| [#4](https://github.com/freefrank/LostOdysseyRecomp/issues/4) | CLOSED | historical Unicode path repair — Reuse with historical repair; full in-game crash acceptance not implied. |
| [#5](https://github.com/freefrank/LostOdysseyRecomp/issues/5) | CLOSED | historical script entry-boundary repair — Reuse once; reporter accepted same saved encounter. |
| [#6](https://github.com/freefrank/LostOdysseyRecomp/issues/6) | CLOSED | independent historical allocation report — Do not equate report with startup-allocation-error-context diagnostic enhancement. Recovery draft links related_issue_url only. |
| [#7](https://github.com/freefrank/LostOdysseyRecomp/issues/7) | CLOSED | council-word-switch-crash — Historical repair Status Done; separate reporter-validation draft references issue but does not reuse its project item. |
| [#8](https://github.com/freefrank/LostOdysseyRecomp/issues/8) | OPEN | dlc-import-support — Reuse open issue once. |
| [#9](https://github.com/freefrank/LostOdysseyRecomp/issues/9) | OPEN | issue9-cpu-temperatures-taa-flicker — One fresh report, two symptoms, no assumed common root cause; state/body supplied by parent fresh API read. |

本清单只有 #1/#8/#9 使用 existing_issue/issue_url。#6/#7 验证 draft 只有 related_issue_url，已关闭报告或历史修复由主代理复用，避免同一 Issue 被重复显示为未修复。council-word-switch-crash 是唯一需跨历史清单解析的依赖。

#3 评论由 docs_sync 核验并共享：[维护者称 Should be fixed in 0.2.2](https://github.com/freefrank/LostOdysseyRecomp/issues/3#issuecomment-5564456493)，没有报告者新验收。closed_at=2026-09-07T03:06:01Z；events 为 02:18:19Z closed → 02:21:39Z reopened → 03:06:02Z closed。#2 本代理只核验 list 的关闭状态，未读评论，不宣称已核验最新评论。

#5 的[报告者存档与同场景通过确认](https://github.com/freefrank/LostOdysseyRecomp/issues/5#issuecomment-5576195692) 优先于旧文档缺存档描述，不新增未解决修复项。#6 是独立 allocation 报告，不能等同诊断增强；#7 是已关闭议会报告，原报告者确认另列。#9 在整理期间新增，由主代理最新 API 提供，其两个症状尚未验证。

## 冲突、验收与覆盖边界

- **Map 13 body/environment shadows**：Keep bounded controlled-regression item; ROADMAP retains open report while STATUS contains improvement/deferred observations. Do not infer identity with poster or accepted ground-shadow defects.
- **Issue 5**：Reporter supplied the USA/Europe save and confirmed the same encounter passed after the split fix; old missing-save wording is stale. No unresolved Issue 5 repair item.
- **Issues 6 and 7**：Both closed by maintainer. Keep closed report/repair historical items separate from awaiting-reporter-validation drafts; no reopening by this manifest.
- **AMD reports**：Earlier first-battle AMD report and RX 9060 XT f3182 report remain two paused investigations; Radeon 8060S bounded non-reproduction is not reporter acceptance.
- **Issue 9**：New open report added during collection; CPU temperature and TAA texture flicker are two unverified symptoms and not presumed same cause or identical to enemy disappearance.

shader-log-coverage-audit 保留独立 shader 日志/运行摘要、来源及 hash 命名空间、实际 draw/pass/拒绝原因、后台有界写入与丢弃计数、三组会话及版本累计覆盖、F1 快照等要求。静态未知候选只观察；不能据此自动扩生产白名单。

## 所有未完成 checkbox 的归并

双语 ROADMAP 共 94 个未完成/进行中 checkbox，全部映射。下表列英文位置；JSON 同时保留中英文原文和行号。

该表按初始冻结快照记录。最终同步又在两版路线图各加入 Issue #9（`issue9-cpu-temperatures-taa-flicker`），合计现为 96 个未完成/进行中标记；它已包含于上述 65 项，未增加重复工作项。新增条目之后的旧行号会后移一行。

| 来源 | 工作项 |
|---|---|
| [ROADMAP.md:18](../../docs/ROADMAP.md) | council-reporter-validation |
| [ROADMAP.md:19](../../docs/ROADMAP.md) | ppc-semantics-gameplay-regression |
| [ROADMAP.md:22](../../docs/ROADMAP.md) | enemy-disappearance-flicker |
| [ROADMAP.md:38](../../docs/ROADMAP.md) | pc-backend-selection |
| [ROADMAP.md:39](../../docs/ROADMAP.md) | vulkan-foundation |
| [ROADMAP.md:40](../../docs/ROADMAP.md) | vulkan-shader-compilation |
| [ROADMAP.md:41](../../docs/ROADMAP.md) | backend-cache-isolation |
| [ROADMAP.md:42](../../docs/ROADMAP.md) | vulkan-runtime-integration |
| [ROADMAP.md:43](../../docs/ROADMAP.md) | vulkan-temporal-presentation |
| [ROADMAP.md:44](../../docs/ROADMAP.md) | dx11-feasibility |
| [ROADMAP.md:45](../../docs/ROADMAP.md) | dx11-runtime-integration |
| [ROADMAP.md:46](../../docs/ROADMAP.md) | pc-backend-scene-validation |
| [ROADMAP.md:47](../../docs/ROADMAP.md) | pc-backend-lifecycle-validation |
| [ROADMAP.md:48](../../docs/ROADMAP.md) | pc-backend-hardware-validation |
| [ROADMAP.md:49](../../docs/ROADMAP.md) | pc-backend-release-readiness |
| [ROADMAP.md:103](../../docs/ROADMAP.md) | rx9060xt-kaim-shadow-report |
| [ROADMAP.md:104](../../docs/ROADMAP.md) | attack-animation-stutter |
| [ROADMAP.md:105](../../docs/ROADMAP.md) | text-clarity-and-garbled-glyphs |
| [ROADMAP.md:106](../../docs/ROADMAP.md) | fire-breath-shadow-regression |
| [ROADMAP.md:107](../../docs/ROADMAP.md) | dlc-import-support |
| [ROADMAP.md:108](../../docs/ROADMAP.md) | shader-log-coverage-audit |
| [ROADMAP.md:133](../../docs/ROADMAP.md) | shader-translation-failures |
| [ROADMAP.md:142](../../docs/ROADMAP.md) | ime-game-controls |
| [ROADMAP.md:148](../../docs/ROADMAP.md) | enemy-encounter-poses |
| [ROADMAP.md:150](../../docs/ROADMAP.md) | fire-hit-checker-effects |
| [ROADMAP.md:151](../../docs/ROADMAP.md) | debug-protagonist-damage |
| [ROADMAP.md:152](../../docs/ROADMAP.md) | save-content-compatibility |
| [ROADMAP.md:154](../../docs/ROADMAP.md) | chapter-disc-save-regression |
| [ROADMAP.md:155](../../docs/ROADMAP.md) | full-playthrough-regression |
| [ROADMAP.md:156](../../docs/ROADMAP.md) | full-playthrough-regression |
| [ROADMAP.md:161](../../docs/ROADMAP.md) | full-playthrough-regression |
| [ROADMAP.md:169](../../docs/ROADMAP.md) | map13-shadow-regression |
| [ROADMAP.md:170](../../docs/ROADMAP.md) | fire-hit-checker-effects |
| [ROADMAP.md:172](../../docs/ROADMAP.md) | crate-destruction-black-effects |
| [ROADMAP.md:174](../../docs/ROADMAP.md) | save-anywhere-acceptance |
| [ROADMAP.md:175](../../docs/ROADMAP.md) | gpu-query-pointer-corruption |
| [ROADMAP.md:187](../../docs/ROADMAP.md) | shader-translation-failures, resource-only-pso-preparation |
| [ROADMAP.md:189](../../docs/ROADMAP.md) | desktop-display-acceptance |
| [ROADMAP.md:192](../../docs/ROADMAP.md) | ultrawide-fov-layout |
| [ROADMAP.md:194](../../docs/ROADMAP.md) | frame-generation-research |
| [ROADMAP.md:196](../../docs/ROADMAP.md) | hdr-output-tonemapping |
| [ROADMAP.md:197](../../docs/ROADMAP.md) | higher-resolution-shadows |
| [ROADMAP.md:198](../../docs/ROADMAP.md) | ssao-and-reshade-depth |
| [ROADMAP.md:199](../../docs/ROADMAP.md) | pc-backend-release-readiness, linux-steamdeck-platform |
| [ROADMAP.md:204](../../docs/ROADMAP.md) | screen-space-gi-ssr |
| [ROADMAP.md:205](../../docs/ROADMAP.md) | hardware-raytracing |
| [ROADMAP.md:206](../../docs/ROADMAP.md) | texture-replacement-mod-loader |

另扫描 docs/notes 全部未完成 checkbox：

| 来源 | 处理 |
|---|---|
| docs/notes/recomp.md:118 | Tracked → embsec-segment-research |
| docs/notes/xex.md:66 | Tracked → title-update-research |
| docs/notes/xex.md:67 | Historical format-parser checkbox superseded for the implemented FPD/FPI/CPX discovery scope; remaining variant/PSO work tracked separately. → resource-only-pso-preparation |

FPD/FPI 格式解析旧 checkbox 在已发布发现/解析范围内已过时；已完成的 bare-FPD/CPX/FPI 发现与四盘资源扫描不重建为未实现。动态变体和仅凭资源准备首用 PSO 的剩余范围独立保留。

## 工作项总表

| stable_key | English title | Status | Delivery |
|---|---|---|---|
| pc-backend-selection | Add PC backend selection and capability checks | Todo | Not started |
| vulkan-foundation | Port reusable Vulkan foundations | Todo | Not started |
| vulkan-shader-compilation | Support SPIR-V shader compilation | Todo | Not started |
| backend-cache-isolation | Isolate caches by backend and compiler identity | Todo | Not started |
| vulkan-runtime-integration | Connect the Windows Vulkan renderer and WSI | Todo | Not started |
| vulkan-temporal-presentation | Adapt presentation and temporal rendering to Vulkan | Todo | Not started |
| dx11-feasibility | Establish Direct3D 11 feasibility | Todo | Not started |
| dx11-runtime-integration | Implement the Direct3D 11 backend | Todo | Not started |
| pc-backend-scene-validation | Validate PC backends in native-save scenes | Todo | Not started |
| pc-backend-lifecycle-validation | Validate backend lifecycle and cache recovery | Todo | Not started |
| pc-backend-hardware-validation | Validate PC backends across GPU vendors | Todo | Not started |
| pc-backend-release-readiness | Prepare v0.5.0 backend packages and documentation | Todo | Not started |
| council-reporter-validation | Confirm the Council repair on reporter systems | Awaiting validation | Awaiting validation |
| ppc-semantics-gameplay-regression | Regress PPC semantics across later chapters | Todo | Awaiting validation |
| battle-terrain-player-acceptance | Confirm the battle terrain TAA repair with the player | Awaiting validation | Awaiting validation |
| enemy-disappearance-flicker | Diagnose and repair enemy disappearance flicker | In Progress | In progress |
| shader-log-coverage-audit | Add independent shader logs and cumulative coverage auditing | Todo | Not started |
| amd-opening-shadow-report | Resume the earlier AMD opening-battle shadow report | Paused | Deferred |
| rx9060xt-kaim-shadow-report | Resume the RX 9060 XT Kaim shadow investigation | Paused | Deferred |
| attack-animation-stutter | Diagnose attack-animation stutter | Todo | In progress |
| text-clarity-and-garbled-glyphs | Investigate blurry and garbled game text | In Progress | In progress |
| fire-breath-shadow-regression | Regress shadows during fire-breath attacks | Todo | Awaiting validation |
| dlc-import-support | Assess and implement DLC import support | Todo | Not started |
| shader-translation-failures | Resolve the two remaining shader translation failures | Todo | Not started |
| resource-only-pso-preparation | Complete resource-only shader and first-use pipeline preparation | Todo | Not started |
| ime-game-controls | Prevent IME interference with gameplay controls | Todo | Not started |
| enemy-encounter-poses | Repair remaining enemy encounter poses | In Progress | In progress |
| fire-hit-checker-effects | Diagnose fire-hit checkerboard effects | Todo | Not started |
| debug-protagonist-damage | Add temporary protagonist damage controls | Todo | Not started |
| save-content-compatibility | Validate broader save and content compatibility | In Progress | Awaiting validation |
| chapter-disc-save-regression | Validate chapter and disc transitions on both editions | Todo | Not started |
| full-playthrough-regression | Complete native-save full-playthrough regression | In Progress | Awaiting validation |
| map13-shadow-regression | Recheck Map 13 body and environment shadows | Todo | Awaiting validation |
| crate-destruction-black-effects | Investigate black crate-destruction effects | Todo | Not started |
| save-anywhere-acceptance | Complete save-anywhere gameplay acceptance | In Progress | Awaiting validation |
| gpu-query-pointer-corruption | Diagnose GPU query and wait pointer corruption | In Progress | In progress |
| desktop-display-acceptance | Complete desktop display and input acceptance | Todo | Awaiting validation |
| ultrawide-fov-layout | Support ultrawide output and FOV changes | Todo | Not started |
| frame-generation-research | Evaluate frame generation integration | Todo | Not started |
| hdr-output-tonemapping | Add HDR output and tone mapping | Todo | Not started |
| higher-resolution-shadows | Add higher-resolution shadow rendering | Todo | Not started |
| ssao-and-reshade-depth | Evaluate SSAO and clean depth access | Todo | Not started |
| linux-steamdeck-platform | Port and validate Linux and Steam Deck | Todo | Not started |
| screen-space-gi-ssr | Explore screen-space GI and reflections | Todo | Not started |
| hardware-raytracing | Explore hardware ray-traced shadows and reflections | Todo | Not started |
| texture-replacement-mod-loader | Support texture replacement and a mod loader | Todo | Not started |
| switch-platform-alignment | Resume the Switch platform branch after PC Vulkan | Paused | Deferred |
| cross-edition-text-patch | Resume cross-edition text and font patch research | Paused | Deferred |
| native-object-motion | Develop native object and skeletal motion vectors | Todo | Not started |
| temporal-color-exposure | Define the temporal color and exposure contract | Todo | Not started |
| vendor-temporal-upscaling | Integrate vendor temporal upscaling after input validation | Todo | Not started |
| taa-motion-quality | Validate TAA motion and history quality across scenes | Todo | Awaiting validation |
| internal-resolution-acceptance | Complete internal-resolution visual acceptance | Todo | Awaiting validation |
| 60fps-gameplay-regression | Extend correct-speed 60 FPS gameplay validation | Todo | Awaiting validation |
| optional-120fps | Evaluate the optional 120 FPS candidate | Todo | Awaiting validation |
| audio-and-language-regression | Extend audio and edition-language regression | Todo | Awaiting validation |
| wmv-playback | Implement and validate WMV movie playback | Todo | Not started |
| debug-victory-special-battles | Validate debug victory in special battles | Todo | Awaiting validation |
| physical-controller-regression | Extend physical controller and rumble coverage | Todo | Awaiting validation |
| startup-allocation-reporter-validation | Confirm Issue 6 recovery on the original machine | Awaiting validation | Awaiting validation |
| recompiler-remaining-contracts | Audit remaining PPC translation contracts | Todo | Not started |
| embsec-segment-research | Determine the purpose of the embedded code sections | Todo | Not started |
| title-update-research | Investigate official title updates and XEXP data | Todo | Not started |
| edram-aliasing-completeness | Audit remaining EDRAM aliasing contracts | Todo | Not started |
| issue9-cpu-temperatures-taa-flicker | Investigate high CPU temperatures and TAA texture flicker | Todo | Not started |

## 不重新打开的历史范围

- Accepted original Map3 tire flicker, ground projection, poster, original AMD resolve and vehicle-dialogue repair stay in completed history, not reopened by broad regression.
- Completed capture background ZIP/raw-directory cleanup/latest-three-runtime-log behavior is separate from the unimplemented dedicated shader-log requirement.
- Old camera-history false-rejection and source-index follow-ups have implementations and bounded tests; only remaining explicit acceptance is listed.
- Removal of disabled DLSS/FG UI placeholders is completed presentation work, not cancellation of future engineering.
- Historical root task-plan checkboxes superseded by current STATUS/ROADMAP are not recreated as bugs.
- Generic absent Xbox Live/network HLE is not expanded into an invented feature commitment.

核验通过：65 个唯一键；状态和交付阶段全部有效；本地来源文件存在；所有依赖可解析；ROADMAP 与 notes 的全部未完成 checkbox 已映射或给出替代依据。行号来自冻结快照，后续文档变动可能移动。
