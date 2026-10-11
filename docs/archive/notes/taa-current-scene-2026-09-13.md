# Current-scene TAA coverage and static discovery — 2026-09-13

> **Historical checkpoint.** Version, issue, process and pending-work statements below describe their original date. Recheck current delivery and acceptance in [STATUS](../STATUS.md) and [ROADMAP](../ROADMAP.md).

This note records a local candidate and a bounded diagnostic capture. It does not record a release, a game acceptance result or a whole-game conclusion.

## Current-scene TAA candidate

The latest three-frame capture is frames 13429–13431 from v0.5.7, with 2,246 headers and 2,229 submitted draws per frame and no reported drops. Actual CPU-uploaded VS/PS constant banks and per-draw enabled/applied records were not instrumented. The target scene produced 39 relevant draws per frame, 117 across the three frames.

The candidate extends `temporal_scene.h` on top of the existing local changes. Path `8d3c80b318235b22` receives c4 coverage. Paths `3eb16ad927f44289`, `83b23507725f85bf`, `6742ec1abe49589e`, `0f2b89c7eb1c409e`, `fecf2f9d9bef2702` and `2a7867b5eed37f8a` receive c7 coverage. Strict audit matching directly identified two paths; separating position fetch 95 from auxiliary fetch 94 accounted for five additional current-scene paths. The seven original vertex-shader microcode hashes match the historical `f24842` family. PS `67b10` was not present in the current frames and was not moved into its compensation policy.

The static review compared the current HLSL and 11 PS variants. It preserves the existing camera, viewport, depth and policy guards, and does not modify PS bytecode or reconstruction compensation. The audit observation count is not a bug count: the 252 observations are coverage evidence, not 252 defects.

The smallest new CPU fixture used seven real current-scene world/VP draws at 1920×1080, frame 13429 and jitter phase 22, with three synthetic vertices per path. Maximum physical sample error was `0.000277985496` pixels; canonical error was zero. An independent canonical oracle, Z/W and non-VP constants, and retained PS constant banks were checked; this does not verify PS execution behavior. Historical all-phase, resolution and guard fixtures were reused without rerunning them. This fixture does not exercise the game, GPU replay or visual output.

The Release candidate build completed with clang 22.1.8 in 151.525 seconds and exit code 0, with actual link provenance successful. The PPC static library was reused after equivalent input/compiler/header/Release-contract checks; PPC was not recompiled. Delivery is `LostOdysseyRecomp-taa-scene-20260913.exe`, 83,437,568 bytes, SHA-256 `f47a894ec51f50b099f97f53c08717019eb02330aae943f8cdc33e1568d459b3`, source 0.5.7 and link identity `e6031efe0fe1da1effdcbb26670eb7463e6b20bb6a140c265a53b2bd82da5535`. The file was copied into the existing installation without replacing the primary executable (`81705475dafb360c8c30a30acef14f1e365afc0368cd5aaf858889d54f675ac4`) and was not launched. The candidate has no player visual acceptance.

## Historical discovery

The completed offline discovery inventoried 2,893 cached VS programs and 20,077 cached PS programs. Forty-seven VS programs reused existing microcode SHA/proof; 2,846 were translated with the production translator and analyzed by `position_evidence`, with zero process/file failures. Six hundred eleven new translations carried non-fatal notes, so generated HLSL does not prove complete microcode semantics. No DXC, GPU operation, cache writeback or production mapping was performed.

The conservative shortlist is 81 static families: 76 cache-only and five historical main-camera associations, all without translator notes. Eight historical main-camera associations remain separately recorded; three are conservatively tainted by constant predicates and require manual reuse of earlier derivations. The two currently observed unmapped matrices were excluded as non-main-camera paths. Thirteen secondary-projection-bank families were downgraded from the initial screen. These 81 families and eight historical associations are coverage leads, not bug counts. The runtime candidate still contains only the seven reviewed mappings. Evidence: `out/taa-live-20260913/discovery/DISCOVERY.md`, `out/taa-live-20260913/discovery/discovery.json` and `out/taa-live-20260913/discovery/SHORTLIST.json`.

## Evidence boundaries

Evidence pointers are repository-relative: `out/taa-live-20260913/current-capture-validation.log`, `out/taa-live-20260913/delivery.json`, `out/taa-live-20260913/discovery/DISCOVERY.md`, `out/taa-live-20260913/discovery/discovery.json` and `out/taa-live-20260913/discovery/SHORTLIST.json`. No private raw capture, absolute local path, network publication, player visual acceptance, PS execution claim or automatic runtime mapping is claimed.
