# Offline FSR Vulkan SDK build

This lane builds only FidelityFX SDK v1.1.4 (`c6efa6bf7f2027b3ec94f28578bb5965eabb9e55`), FSR upscaler 3.1.4 and its Vulkan backend. It excludes frame interpolation, optical flow and the SDK swapchain. Nothing is downloaded by CMake or at runtime.

Generate once on Windows using the SDK's pinned compiler binaries:

```powershell
python tools/fsr/generate_shaders.py --sdk .cache/deps/fidelityfx-sdk-v1.1.4 --output out/fsr-shaders-vk --threads 2
cmake -S tools/fsr -B out/fsr-sdk-build -DLO_ENABLE_FSR=ON -DLO_REQUIRE_FSR=ON -DLO_FSR_SDK_ROOT=<absolute-sdk-root> -DLO_FSR_SHADER_DIR=<absolute-shader-directory>
cmake --build out/fsr-sdk-build --config Release --target lo_fsr3upscaler_vk LoFsrSdkLinkCheck --parallel 2
python tools/fsr/prepare_adapter_shaders.py --glslang .cache/deps/fidelityfx-sdk-v1.1.4/sdk/tools/binary_store/glslangValidator.exe --output out/fsr-shaders-vk
```

The shader directory contains 40 permutation index headers, their SPIR-V byte-array headers, source/tool hashes, commands and the upstream MIT license. Copy that directory with the matching SDK sources to Linux; generation is not repeated there. Build configuration verifies prepared header/source hashes. Generated output belongs in the build/package inputs, not the runtime source checkout. Distribute `LICENSE-FidelityFX.txt` with binaries containing these shaders/SDK code. Enabled executable targets stage it in `licenses/LICENSE-FidelityFX.txt` beside the binary.

`LoFsr::Vulkan` exposes the SDK headers and links the upscaler/backend static library. `lo_enable_fsr(target)` links it and defines `LO_HAS_FSR=1`; disabled or missing optional inputs define `LO_HAS_FSR=0`. Both switches default off. `LO_REQUIRE_FSR=ON` makes requested missing/mismatched inputs fatal. The existing DLSS options are independent.

The generator reproduces the upstream GLSL flags, including all six Boolean permutation dimensions and FP16/FP32 families. Upstream GLSL wave64 families use identical compilation flags and a distinct symbol suffix; subgroup size is selected by the Vulkan backend's pipeline creation. HLSL-only WaveSize flags must not be injected into GLSL generation.

The two application color/depth conversion shaders have a separate preparation script and `adapter-manifest.json`; regenerate them when their GLSL changes, without rebuilding SDK shader permutations. Their headers expose `lo_fsr_prepare_spv`, `lo_fsr_present_spv` and byte-size constants through the same include directory.

The Vulkan backend is compiled from a build-directory copy that includes Plume's volk declarations and sets the unused frame-generation callback to null. This prevents a Vulkan function/function-pointer ABI mismatch and removes the link dependency on the excluded frame interpolation swapchain. The game supplies Plume's initialized volk symbols; the standalone link check compiles the same `volk.c` solely to resolve them. It does not initialize Vulkan.

## Linux compatibility

SDK v1.1.4 assumes MSVC string/array helpers and a Windows-sized public context. A target-private forced header provides bounded helpers, the missing standard includes and the float `abs` overload. No SDK files are edited.

Linux uses 32-bit `wchar_t`, making the private FSR context 837848 bytes with the tested Clang toolchain, beyond the upstream public 524288-byte storage. CMake creates a Linux-only public header overlay with 1048576-byte opaque storage. Its include directory is propagated **before** original SDK headers through `LoFsr::Vulkan`, so SDK and consumers share the same ABI. The SDK's existing `private <= public` static assertion remains active. Windows retains its original context. Do not use `-fshort-wchar` or manually put original SDK headers ahead of the target's overlay.

`LoFsrSdkLinkCheck` links actual backend and dispatch symbols and checks the effect version without creating a Vulkan instance/device. A library/link success is build evidence only; FSR rendering, GPU feature enablement, color/depth contracts and quality require separate runtime validation.

Windows SDK compilation explicitly includes `<bit>` because the pinned SDK utility header uses `std::popcount` under C++20 and newer without including it. This compatibility include is scoped to the SDK target; Linux receives it through its existing target-private compatibility header.

## Vulkan memory selection and source regression

The generated SDK backend requires **all** requested memory-property bits,
not any matching bit. Invisible device-local memory is preferred for ordinary
discrete-GPU allocations; a compatible host-visible local type remains eligible
when no invisible type exists (for example UMA). Types requiring disabled AMD
device-coherent memory remain excluded. The original SDK and shader manifest
are unchanged; reconfigure CMake and rebuild the SDK target to apply this fix.

`tools/tests/fsr/sdk_memory_selection_regression.py` compiles the original SDK
selector (with the prior project exclusion) and the function transformed by the
production CMake patch. Synthetic tables reproduce two baseline failures and
verify their repair without creating a Vulkan device. `LoFsrVulkanMemoryPolicyTest`
adds mask, required-property, preference, disabled-feature and type-31 cases.

The `FSR native compile contracts` workflow compiles both FSR-enabled and disabled
adapter objects plus the temporal provider wrapper against pinned public headers.
It compiles and validates the two existing conversion shaders, not SDK shader
permutations. This is not a full SDK link, game build, GPU execution, performance,
or image-quality test. CPU ownership tests require the project's patched Plume
headers; use `tools/patches/README.md` for dependency preparation.
