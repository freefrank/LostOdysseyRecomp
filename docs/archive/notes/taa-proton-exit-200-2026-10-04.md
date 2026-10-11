# TAA exit under Proton on AMD (#200) — 2026-10-04

Issue #200: with TAA (`antialiasing=3`, no upscaler) the Windows build closed without a message about 16–19 s after loading a save on psvita (Radeon 8060S, RADV from Mesa 26.2.2, GE-Proton10-34, Direct3D 12 through vkd3d-proton). Anti-aliasing off or FSR worked.

## Cause

- The process exit code was 3, the C runtime's `abort()` code. With `WINEDEBUG=err+all,fixme-all` Wine printed `_wassert (L"!status && \"vkCreateGraphicsPipelines\"", L"../src-wine/dlls/winevulkan/loader_thunks.c", 3350)`: the Linux side of `vkCreateGraphicsPipelines` faulted, and winevulkan's assert called Wine's own `abort()`. That is a different C runtime from the game's, so the game's SIGABRT and terminate handlers never ran and no crash report was written.
- `trace+seh` showed the fault: a write to address `0x40` at `libvulkan_radeon.so+0x149e82`, inside `radv_shader_spirv_to_nir`.
- `MESA_SPIRV_FAIL_DUMP_PATH` dumped the module Mesa failed to parse: the TAA `pixel` shader as converted by vkd3d-proton's dxil-spirv. `spirv-val` rejects it: "block exits the selection ... but not via a structured exit". Mesa's SPIR-V parser fails, and RADV dereferences the null result.
- Bisecting the shader with `LoTemporalAATest` under Proton (seconds per variant, source read from a file) pinned it to one block in `motionPixel`: `[branch]if(!primaryDepthFound)` with a `return` and an inlined `stationaryMultiSurfaceSupport` / `stationaryCoverageSupport` call followed by another `return`. Without the helpers, or with any one-exit form of that block, the converted module is valid.

## Change

`temporal_aa.cpp`: the block computes one `unsupported` flag and returns once after it. HLSL 2021 `||` short-circuits, so the helpers still run only when the earlier conditions pass, and both returns had the same reason.

`LoTemporalAATest` no longer built on main (`fmt` include for `dxc_compiler.cpp`, telemetry symbols from `temporal_aa.cpp`); it now links `fmt::fmt` and `tools/tests/motion_replay/platform_stub.cpp`, like `LoTemporalLifecycleBr01OwnerTest`.

## Validation

- psvita, game, Uhra save, TAA: main `3afdc4e` exits with code 3 at the first 3D frame; with the change it loads the map and runs for 60 s.
- psvita, `LoTemporalAATest` under Proton: 74 checks pass; before the change it exits with code 3 at pipeline creation. The converted SPIR-V of the changed shader passes `spirv-val`.
- Windows, RTX 5080: `LoTemporalAATest` on D3D12 and Vulkan passes 74 checks with output identical to the unchanged shader. The test does not reach the stationary-support path; a variant without those helpers also passes all 74.
- Not checked: native Linux Vulkan with TAA, and the image of the stationary path in the game.

Upstream: the invalid structured exit is a dxil-spirv problem, and RADV does not check `spirv_to_nir`'s result. Neither is reported yet.
