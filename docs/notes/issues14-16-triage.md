# Issue #14–#16 triage — 2026-09-12

> **日期化分流快照。** 下文记录 2026-09-12 读取到的 Issue 状态和当时证据；当前 Issue 状态以 live tracker 和最新项目同步记录为准。

当前 tracker 核验时间为 2026-09-18T07:33:59Z：Issues #14, #15 and #16 are **CLOSED / Done**. Vulkan、其他地区覆盖和玩家验收仍是独立回归边界。

This note records the current investigation boundary. [GitHub Issue #14](https://github.com/freefrank/LostOdysseyRecomp/issues/14), [#15](https://github.com/freefrank/LostOdysseyRecomp/issues/15) and [#16](https://github.com/freefrank/LostOdysseyRecomp/issues/16) were read as OPEN on 2026-09-12. Issue state, implementation, validation and reporter acceptance remain separate.

## Issue #14

The two attachments have different historical signatures and source versions:

- The first is source 0.5.0, in the previously investigated `sub_829DFD58` switch/dispatch area. The current low-word selector generator fix and generated-source guard address the historical output risk, but the attachment does not prove its root cause or a current reproduction.
- The second is source 0.4.2, with a host read fault at the virtual call in `FSkeletalMeshSceneProxy::DrawDynamicElements`; the required object/free-list evidence for a lifetime race is absent. The existing render-flush mitigation is present in the current source.

No current 0.5.4 reproduction or player acceptance is recorded for these historical signatures. The existing generator guard and render-flush mitigation are included in published v0.5.6; a clean-package cage-scene run and save are still needed.

## Issue #15

The runtime log identifies source version 0.5.4. It records a successful 206,000-byte first-save write and a later full read, followed by continued rendering until the window closed. The reporter also says the resulting save loads and later progression succeeds. The exact stalled transition is therefore unresolved.

The updater audit separately confirmed a staging defect: `apply.cpp` replaces the files listed by `plan.files`, while staging did not extract or apply `manifest.json` itself. An older manifest could remain in an updated directory. The updater now applies the manifest transactionally for subsequent update transactions using the new `StageArchive`; it does not automatically repair an already mixed installation or remove old resources. Its focused `LoUpdaterTest --manifest-transaction out/updater-manifest-check` run passed three scenarios with zero failures: successful update and post-apply rollback, failure after manifest replacement, and tamper rejection, including plan serialization round-trip. This validates the updater transaction path only; it does not establish a network update, helper/game launch or player acceptance. `PrepareAtStartup` checks manifest and executable source versions, but does not verify all payload hashes; an `up-to-date` result does not establish complete installation provenance. Mixed installation is not proven to be the cause of the reported save-flow hang.

The log's compiler identity pair matches the retained official v0.5.4 manifest byte-for-byte: `dxcompiler.dll` is `1220478c87af97551d2342906c5a08d888ee9fa26f6f99b6072d674801f492d1`, and `dxil.dll` is `e038a9755387fc396a9a5f393d292b9318cf6ac7e520e9283bbc3088c5f8ac93`. `DxcIdentity` hashes the actually loaded module paths. This supports those two DLLs' provenance for the run, but does not establish the provenance of every DLL or resource.

## Issue #16

The official v0.5.4 executable (`SHA-256 3c3b4073f1b7abbcce38747dc08763335ac019edf560df849177176d0399f949`) reproduces the Issue #16 King Train freeze/crash on local Asia Disc 3 D3D12 with the supplied `user08` path. The baseline evidence is retained in `baseline-01` and `capture-baseline-06/findings.md`.

The material identity is `gt9_0_map.cs__frzShader1` → raw `xf_shd_aniflz.freeze`, GUID `0fd4ca6d4bb67581c9c9f5b8f0af62c0`. Both the cooked table and the actual `FParticleVF8336CA10` table have count zero. The original compatibility constant allowed a zero-size control sprite to reach guest `0x823DFE14`, producing the null shader read. The production `particle_material_compat.cpp/.h` change preserves parameter updates and original logic at the sprite compatibility gate, then limits the fallback to raw blend 2 when the ready shader map lacks a normal particle shader; it returns false to use the engine's existing default material. The caller guard is limited to the ordinary sprite builder (`entryLR=0x822C91CC`, builder `0x822C9150`), leaving shared callers on their original logic. This is a narrow compatibility path, not a global null guard or particle discard, and does not claim the Xbox root cause.

`LoParticleMaterialCompatTest` is registered `EXCLUDE_FROM_ALL`; its focused `/UNDEBUG` fixture compiled and ran with zero failures. It covers the material compatibility policy, not ABI or GPU behavior. A diagnostic candidate executable (`SHA-256 78d66966861ba854510ab4c95f005f1f7da901382bb3cda135463a1d175a7e89`) hit the fallback for raw `073BEAC0`; the associated result recorded `stable=true`, the same GUID and a 16-item source array with an empty particle child. In that r2 run, the freeze sequence completed through the supplied shots, then map 229 resumed actual movement from `540,0,97.85` to `485.55002,-81.072205,97.85` and reached normal menu shot `65291`. This candidate also contains unrelated pre-existing performance objects and an out-only battle-win bridge, and predates the final caller guard; it is not final-branch build evidence.

The final branch native build in `build/branch-native-r1` completed successfully with executable SHA-256 `f2015ad76edd48adf7603221c9618804645224c22fd5ca0bcee1471af55d483f` and updater SHA-256 `abb8daf32765c64a46e4fc005816e1611571b886b401852ee5765d4c8e6ace7b`. Its provenance records 58 current-branch runtime translation units, eight updater translation units and one resource, with no main native object/PCH or battle-win bridge; reused dependencies and PPC inputs were checked. In `final-reload-01`, the final executable successfully reread new `user10` at position `485.55002,-81.072205,97.899994` and produced field shot `9326`; that run did not perform the r2 movement or menu interaction. A new slot 11/user10 save was written and reread from `seed-after-freeze/checkpoint.json`; the checkpoint is marked reloaded. The final PPC library uses Release `/Ob2`, distinct from the earlier RelWithDebInfo `/Ob1` candidate.

The final-branch `final-freeze-01` validation passed on D3D12/local Asia Disc 3 using an actual `seed-before-boarding/user09` reload with the target sequence unsuppressed. At 1527.792 s the fallback hit raw `0A33A8C0`; the material result was stable for the same freeze GUID with 16 child entries and zero particle VF shaders. The captured frames show the King, guards and carriage visibly frozen, Jansen's frozen carriage, and later train animation. Automatic collection completed 180 frames. The run then reached map 229, a normal menu, and visible movement/camera change after 30 ticks. Its coordinate telemetry was unavailable, so those observations are not assigned the r2 or `final-reload-01` coordinates. This completes the final-branch target-scene validation; it is not whole-game acceptance.

The earlier native save in new slot `10`/`user09` was successfully reread from `seed-before-boarding/checkpoint.json` (SHA-256 `0fe47315ef942fae29b2f7216f80760dcc59d6039ded38784f64029d1dea621a`); the original `user08` was unchanged. These save checks do not establish recoverable in-memory save state. `current-package-mismatch` indicates a source/package or development-marker mismatch and does not prove stale DLLs or resources.

Issue #16 remains OPEN. Vulkan and other-region coverage and player acceptance remain pending; Asia Disc 3 D3D12 final-branch target-scene validation and recovery are covered above. The implementation and local validation are committed as `2019cd017ab939d0b728cec340f7835c2082e615` and are included in pushed `main` alongside its existing city-performance work. The validation executable and retained evidence remain source version 0.5.4; published v0.5.6 includes the implementation; publication does not change Issue #16 open state or player-acceptance boundaries.

## Main 0.5.6 build boundary

The merged local `main` built successfully with normal CMake Release configuration. Its runtime executable is SHA-256 `d50c240d24bcd6cda7a1abc23107fa97f11d18dc5a68167310da6e6f892fe0ed`; the updater is `732681bf2a1c74106bb1ea90b32912b3269304dea3abe8ba351f6e1d5b94cc3e`. This 0.5.6 build has no diagnostic object overlay or battle bridge. The retained Issue #16 runtime results above use the separate source-0.5.4 validation executable and must not be attributed to this new binary. The completed main-binary validation is recorded separately below.

The local producer PPC key is `50b8ad415be405b302252558e0fd960913c3ce6a15d991ae3607142f1a3821a5`, uploaded at private commit `6a6ed03152431a232165e35b19b7f94f09bbbda9`. Hosted CI `34726533463` consumed the CI-compatible key `d89197759478260d7e135b654993d57cff30127f1d41cf30c410fd0ae4be27f4` from private commit `a6cd91ea35261dd202b78e93b4acb65973369d07`. Five line-ending and fourteen symlink-representation differences explain the keys; all generated outputs, compile headers and the Release contract match. Both use the unchanged library SHA-256 `ba3e4c4dff009d6d8e844c007186a6e5040266875bca6423f8fe26f8d27fb21b`, without PPC recompilation. Hosted retrieval, restore, build and package verification passed; `lo.ppcAutoSync=false` remains unchanged. This records CI compatibility, not source-level key normalization.

The main-binary replay passed on D3D12/local Asia Disc 3 using the new 0.5.6 executable. The unsuppressed freeze sequence hit the production fallback for raw `0x0A54CE40`; the King, guards and carriage were visibly frozen, later train animation continued, and 360 frames were collected. The run reached map 229, a normal menu, and visible movement from `540,0,97.85` to `474.34607,-85.03167,97.850006` after 30 ticks, with before/after shots showing movement. This is the coordinate telemetry for this run and is separate from the older r2 and `final-reload-01` observations. The result is bounded scene validation, not whole-game, Vulkan, other-region or player acceptance. Evidence: `out/main-bugfix-0.5.6/REPORT.md` and `out/main-bugfix-0.5.6/main-freeze-01/validation.json`.

## Evidence

- [`out/bug-fix-evidence/issue14-analysis.md`](../../out/bug-fix-evidence/issue14-analysis.md)
- [`out/bug-fix-evidence/issue15-analysis.md`](../../out/bug-fix-evidence/issue15-analysis.md)
- [`out/issue16-investigation/analysis.md`](../../out/issue16-investigation/analysis.md)
- [`out/issue16-investigation/runtime-verification.md`](../../out/issue16-investigation/runtime-verification.md)
- [`out/bug-fix-evidence/updater-manifest-build/REPORT.md`](../../out/bug-fix-evidence/updater-manifest-build/REPORT.md)
- [`out/issue16-investigation/capture-baseline-06/findings.md`](../../out/issue16-investigation/capture-baseline-06/findings.md)
