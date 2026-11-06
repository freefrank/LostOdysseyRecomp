# DLSS 5 NR Integration Research

[简体中文](dlss5-nr-research.zh-CN.md)

Date: 2026-10-08. Results of checking the source, SDK, driver and external implementations, and the basis for the Vulkan implementation.

## Conclusions

- NR is Neural Rendering. It sits after super resolution, in a generative image stage at the same resolution. It is not a super resolution preset, and it is not a denoiser.
- NVIDIA has no public header, but the official calling method is clear. The driver's NGX core creates NR with feature 18 (`NVSDK_NGX_Feature_Reserved18` in the SDK), and loads the signed `nvngx_dlssnr.dll` itself. Streamline's `sl.dlss_nr` plugin also takes this path.
- Only community modified versions are available. Their signatures are no longer valid, and the driver does not load them. So the Vulkan implementation loads the DLL itself and calls the `NVSDK_NGX_VULKAN_*` exports it provides, in the same way as Vapourkit.
- No D3D12 bridge is needed. In the existing NGX session, after the SR evaluate, feature 18 runs one more evaluate.
- `nvngx_dlssnr.dll` is not packaged. The user supplies it.

## NR itself

- NVIDIA research page: it takes the current frame, the engine motion vectors, the temporal state and style parameters, and outputs generated frames up to 4K. Officially it supports only RTX 50.
- OpenDLSS-NR reproduces the 310.8.0 version of the network: a U-net of 71 blocks (shifted-window transformer plus a global ViT at the bottom), FP8 activations, 141 MiB of weights. Input and output are at the same resolution. The network reads an sRGB-encoded LDR image and the previous frame's reprojected result.
- The engine side needs only color, depth and MV. Color must be display-encoded values from 0 to 1 with the sRGB curve. Linear HDR gives wrong results.
- The cost is high. OpenDLSS-NR on a 4070 SUPER: 1080p 7.8 ms, 1440p 12.6 ms, 4K 29.3 ms. This is a reproduction, not the NVIDIA runtime library.

## Official calling method

| Source | Content |
| --- | --- |
| Streamline 2.14.1 `sl_core_types.h:253` | `kFeatureDLSS_NR = 1004`, and three Uplift buffer types (70–72). The public package does not include `sl_dlss_nr.h` or `sl.dlss_nr.dll` |
| DLSS SDK 310.9.1 `nvsdk_ngx_defs.h:224` | `NVSDK_NGX_Feature_Reserved18 = 18` |
| NVIDIA-signed `sl.dlss_nr.dll` 2.13.0.0 (distributed with DLSS 5 Swapper) | Source file name `dlss_nrBackendNGX.cpp`. Goes through the NGX core via `sl.param.global.ngxContext`. Full `DLSSNR.*` parameter names. Error text: "check if you have a valid nvngx_dlssnr.dll or your driver supports DLSS-NR" |
| Driver 616.56 `_nvngx.dll` | Contains the "DLSSNR" feature name. The snippet loads with `nvLoadSignedLibraryW` and `WinVerifyTrust` |

Parameter names in `sl.dlss_nr.dll`: `DLSSNR.Width/Height/Hint.Render.Preset`. Resources `DLSSNR.Color/Output/Depth/MVec/ControlMask/Backbuffer/UI/UIAlpha/BidirectionalDistortionField`. Each resource has its own `SubrectBaseX/BaseY/Width/Height`. Also `DLSSNR.MVecScaleX/Y`, `DepthInverted`, `Intensity`, `LocalToneStrength`, `LocalStructureStrength`, `GlobalToneStrength`, `UseAutoMask`, `SkinStructureStrength`, `Style`, `Reset`, and `UICorrection`.

NGX stores parameters with the type they were set with. If the type differs on read, the default value is read. The types follow Vapourkit's Vulkan implementation. Width/Height and Style are unsigned. Enabled, Preset, DepthInverted, Reset, UseAutoMask, UICorrection and subrects are int. Intensity, each Strength, and MVecScale are float.

## Caller check

`nvngx_dlssnr.dll` accepts only a caller whose module name is `nvngx.dll`. It uses `RtlPcToFileHeader` to find the caller's module, then reads the name with `GetModuleFileNameW`. Both modified versions still import these two functions and carry the `nvngx.dll` string, so the check remains.

When the driver's core is used, the caller is the driver's own nvngx, so the check passes. A modified version can only be called by the program itself. The community has two approaches. Vapourkit changes the `GetModuleFileNameW` import in the snippet. Dagherbou makes a forwarding DLL whose name contains `nvngx.dll`. We use the first: only this one import slot is changed in memory, it reports `nvngx.dll` only for this exe module, all other queries go to the system as usual, and the DLL file is not changed.

## Four repositories

| Repository | What it is | Use to us |
| --- | --- | --- |
| [OpenDLSS-NR](https://github.com/maanHimself/OpenDLSS-NR) | MIT. C++/Vulkan bit-exact reproduction of the network. Needs Ada or newer and NVIDIA proprietary extensions. Weights must be supplied by the user | Reference only |
| [dlss-unlocked](https://github.com/ShyVortex/dlss-unlocked) | Installer package: OptiScaler DLSSNR branch, a modified `nvngx_dlssnr.dll`, and Streamline 2.14.1, for 20/30/40 series | Not copied. Its release includes NVIDIA files |
| [DLSS5-Swapper](https://github.com/rakanki911/DLSS5-Swapper) | Electron launcher. Installs ReShade, DLSS5-Feeder, RenoDX, and OptiScaler DLSS-NR | The signed `sl.dlss_nr.dll` it ships gives the official parameter names |
| [DLSS-NR-on-AMD](https://github.com/danielblnc/DLSS-NR-on-AMD) | Closed-source installer. Hooks an FSR 3/4 path, with HIP inference written from scratch | Closed source, cannot be integrated |

The several `nvngx_dlssnr.dll` files on this machine (310.8.0.0) all have invalid signatures. They are of two modified kinds: `8270B350…` (Swapper 2.2.9, reblue, from September 2) and `4B8D19BC…` (ryujinx, Downloads, nds, August 30). The two differ in the GPU architecture dispatch code and in the compressed kernel data. Neither is the original NVIDIA version. The NGX OTA cache also has no DLSSNR. The test uses the newer `8270B350…`.

## Implementation

Settings → Graphics "DLSS 5 Neural Rendering" (`dlss_neural_rendering`, 0 is off, 1–4 are the number of passes) is shown only on Windows with DLSS on. The code is in `gpu/dlss_ngx.cpp` (shared part and Vulkan), `gpu/dlss_ngx_d3d12.cpp` (D3D12), and `gpu/dlss_nr.h`:

- The DLL is by default next to the SR runtime (the directory of `nvngx_dlss.dll`). The full path can be set with `LO_DLSS_NR_PATH`. It is loaded once per process and never unloaded.
- The pass count is part of `SrConfig`. When the setting changes, SR and NR are rebuilt together at a drain point. The feature is created in one frame, evaluate starts on the next frame, and the first evaluate has Reset. The same applies when the output size changes.
- Multiple passes: each frame runs N times in sequence. Each run takes the previous output as input. Each pass uses its own feature, so the temporal states do not interfere. Two FP16 images alternate. All passes in one frame use the same Reset. From the second pass on, `LocalToneStrength` is 0, following the defaults of wilsjo2's multi-pass implementation. These defaults were not tuned for this game. Community implementations allow up to 3 (30 unlocked) or 10 passes. NVIDIA has no matching parameter.
- MV uses render-resolution pixels with `MVecScale = 1`. The subrect uses the render size, the same as SR.
- The menu help line shows the status: running, DLL not found, GPU or driver not supported, or stopped after an error (`gpu/dlss_nr_state.h`). Any NR failure turns off only NR, not SR. The next drain rebuild tries again.

Vulkan:

- When the device is created, if the DLL is present, `VK_NVX_binary_import`, `VK_NVX_image_view_handle`, `VK_KHR_push_descriptor`, and `VK_KHR_buffer_device_address` are added to the SR extension list. If the DLL is absent, or the device lacks any of them, NR is not enabled, and SR behaves exactly as before. A DLL added while the game is running needs a restart.
- The snippet initializes with `Init_Ext2`. The parameter order follows the disassembly: `(…, GIPA, GDPA, Version, const NVSDK_NGX_Parameter*)`, which matches the SDK header. The first version and the community implementation passed the last two arguments in reverse (version 0, pointer 0x15). It happened to work. It is now `(NVSDK_NGX_Version_API, nullptr)`.
- After the SR evaluate, in the same isolated list: the SR output is blitted to an FP16 image, NR runs each pass's evaluate, and the result of the last pass is blitted back to the SR output. A global memory barrier is placed between steps. The images stay in GENERAL.

D3D12:

- Exports `NVSDK_NGX_D3D12_Init_Ext(appId, dataPath, device, Version, const NVSDK_NGX_Parameter*)` (confirmed by disassembly; the 4th parameter is read as 32 bits), `CreateFeature`, `EvaluateFeature`, `ReleaseFeature`, and `Shutdown1`. Resources are set with `SetD3d12Resource`.
- D3D12 has no blit that converts formats. The SR output (RGBA8) is first `CopyResource`'d to a same-format staging image. A small compute shader converts it to FP16 for NR. The result is converted back to the staging image, then copied to the SR output, keeping the original alpha. The shader is compiled at runtime by DXC (`SetComputeShaderCompiler`). The descriptor set is fixed to bind the staging image and the FP16 image.
- Copies, conversions and state transitions all go through plume, so the state tracking stays consistent. NR input is SHADER_READ and output is UAV. At the end, the SR output returns to UAV/GENERAL. After each NGX call, `invalidateCachedNativeState()` is called so plume rebinds its own descriptor heap.

HDR:

- With HDR output on, the DLSS input is still an 8-bit, display-encoded scene. In `renderer.cpp`, DLSS's `qualifiedEncoding` can only be `Sdr`. The highlights are added back in the presentation stage, using the pre-super-resolution FP16 scene scaled by a brightness gain. So NR runs as usual under HDR and needs no extra tone mapping.
- The `SrColorSpace::Linear` path does not occur in this game. If it is reached, NR is skipped and logged.

## Tuning page and live preview

Clicking the "DLSS 5 Neural Rendering" row on the Graphics page opens the tuning page. The controls follow RenoDX's ReShade plugin (`renodx-dlss5.addon64`, closed source). The control names and ranges come from DLSS5-Feeder and DLSS5-Swapper's mirror of it: count, model (Default / Natural / Cinematic, which is `DLSSNR.Style` 0–2), preset group (`Hint.Render.Preset` 0–3, effect not verified), intensity, overall tone, local tone, structure (0–200%, which is 0–2), skin structure (−100 to 100, where negative values follow structure), and character mask. Depth and motion vector overrides and Diffuse White are omitted, because our input is SDR. UI Correction is also omitted, because the HUD is drawn after super resolution. Values are saved with the Graphics page (`dlss_nr_*`).

- Parameter experiment (2026-10-08, RTX 5080, Vulkan, temporary `LO_DLSS_NR_SWEEP` switch, a new set every 5 seconds, no feature rebuild, one Reset per set change): Intensity 0, LocalTone 0 and Style 2 each changed the picture clearly (average difference in static areas 4–7, noise 1–2). GlobalTone 2 and Intensity 2 showed no change (310.8.0.0 does not read GlobalTone, and Intensity appears to cap at about 1). So style, intensity, tone, structure, skin and mask are read on each evaluate. Changing them in the game does not rebuild the feature. It sends one Reset only when a value changes. The preset group is still part of `SrConfig`, and it drains and rebuilds when it changes. Dagherbou's statement that "parameters are locked when the feature is created" does not hold on our path.
- Preview: in the game, every four frames DLSS captures the frame before NR (color at output size, depth and motion vectors at render size, offset 0). This is done only when the DLL is present. While the tuning page is open, the presentation path (the same thread as the renderer; menu frames already wait for the previous frame's presentation) runs a preview-only feature on this frame with the unsaved values on the page. The motion vector scale is 0. After each value change, it runs 8 times to let the static frame settle, then holds. The result is composed into one image with two halves, left DLSS and right DLSS + NR. The presentation side samples the two halves with the new flag 8192, and neither half applies the player's brightness curve. When the count or preset group changes, the preview feature is rebuilt. It is released when the page closes.
- Validation (2026-10-08, settings driver script `tools/settings_driver`, RTX 5080, 2560×1440, Uhra): Vulkan and D3D12 both open the page. The two halves differ by about 7. At intensity 0, the two halves are identical (difference 0.0). Cinematic is about 7.7. 2× is about 10.7. Cancel restores the values. Saving writes `dlss_nr_intensity=50`, and no rebuild is needed in game. On D3D12 at 120 frames, the driver script's consecutive key presses drop keys or land out of place. Wait 0.3 seconds between key presses. After entering settings, wait 3–4 seconds before pressing RB.

## Risks

- Officially only RTX 50 is supported. Whether 20/30/40 series work depends on the modified version the user has. We do not guarantee it.
- At 4K, the per-frame cost may be a few tens of milliseconds.
- No redistribution terms for `nvngx_dlssnr.dll` were found.

## Sources

- [NVIDIA DLSS 5 research page](https://research.nvidia.com/labs/adlr/DLSS5/)
- [Streamline](https://github.com/NVIDIA-RTX/Streamline) (`changelog.txt` 2.14.0: new `sl.dlss_nr` plugin)
- [DagorEngine streamline_adapter.cpp](https://github.com/GaijinEntertainment/DagorEngine/blob/master/prog/engine/drv/drv3d_commonCode/streamline_adapter.cpp)
- [Vapourkit vsdlssnr](https://github.com/Kim2091/vapourkit), [Dagherbou/OptiScaler_DLSSNR](https://github.com/Dagherbou/OptiScaler_DLSSNR) (parameter types, MV convention, caller check)
- [Guru3D: DLSS-NR configuration items in the 610.47 driver](https://guru3d.com/story/nvidia-geforce-61047-driver-quietly-adds-first-dlss-5-neural-rendering-profiles/)
- The four repositories in the table above
