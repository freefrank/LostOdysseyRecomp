# Cost of graphics options (1080p output): main f7799aec, RTX 5080 and psvita, 2026-10-15

[简体中文](graphics-options-cost-1080p-2026-10-15.zh-CN.md)

This record gives, for 1920x1080 output, the extra milliseconds each graphics option costs compared with the original settings, measured on two machines: an RTX 5080 desktop and psvita (Strix Halo integrated GPU). Each row changes one setting only, and everything else stays at the original. The method is the same as the [4K report (2026-10-10)](graphics-options-cost-2026-10-10.zh-CN.md). Tests ran on the evening of 2026-10-14 (local time).

## Conditions

### Parts shared by both machines

- Version: origin/main `f7799aec` (source version 0.9.52).
  - Built with `tools/build_worktree.bat` in the windows-clang product configuration, with DLSS, Streamline (DLSS-G), FSR 3.1 (including FG) and XeSS 3.0.2 (including XeSS FG) compiled in.
  - Both machines ran the same exe. Each row's log has `DLSS SR feature created`, `FSR recorded`, `XeSS: ready`, `DLSS NR: … pass(es)` and `D3D12 FG: … active=true`. This shows the features were compiled in and were actually running in the rows that use them.
- Shaders:
  - This build's contract is D3D12 `05fd9431…` and Vulkan `03ef0787…`, matching the two packs on the `shader-packs` release page dated 2026-10-12.
  - So the published packs `portable_dx12-05fd9431d3febc7b` and `portable_vk-03ef078715ed8c8b` were used directly, with the pipeline corpus `pipelines_corpus-183df2d935260e33` (3974 entries) beside them.
  - Every run's log has `portable shader pack hit`. The number of newly created pipelines is 0 in every measurement window in the tables.
- Output: 1920x1080 window (`window_mode=0`), log `swapchain=1920x1080`.
- Uncapped: `LO_FPS=0`, log `native_target=0 host_cap=0`, `requested_vsync=false`.
- Original defined as:
  - Internal resolution 720.
  - Anti-aliasing, ambient occlusion and anisotropic filtering all off.
  - Shadows 1x, frustum culling 100%, dynamic shadows on.
  - Depth of field 100, bloom and motion blur on (these are original effects).
  - Scaling filter "High" (the default).
  - Upscaling, DLSS 5 neural rendering, frame generation, HDR and performance info all off.
  - VSync off, FreeSync / G-SYNC off.
  - FSR sharpening 0, DLSS model M.
- Scenes:
  - Uhra residential-area save (`u3b_0_scrw`), character standing still, about 2140 draws.
  - Battle: on the same spot, formation 3 was opened with `LO_DEBUG_BATTLE_FILE` and stopped at the first command. The camera faces Kaim. About 260 draws, much lighter than Uhra.
- Sampling: after entering the scene (for battle, up to the first command), wait 10 seconds, then take the median of each frame's values in that 10-second window.
  - Frame interval: `present timing frame_ms`.
  - GPU: `gpu_queue_batches_elapsed_ms`. Counts only the render queue submission batches. It excludes present and compositor work, and excludes the work of SDK-generated frames.
  - Render thread: `draw_ms`.

### RTX 5080 (this machine)

- Windows 11 Pro (10.0.28000), AMD Ryzen 7 9800X3D, NVIDIA GeForce RTX 5080, driver 616.56, Direct3D 12.
- Display:
  - The monitor is a 3840x2160 144 Hz display. Borderless fullscreen on this monitor gives a 4K swap chain, so a 1920x1080 window was used instead.
  - In window mode the frame rate is not limited by the refresh rate. The baseline is about 200 FPS.
  - HDR is on in Windows. Apart from the HDR row, the game outputs SDR.
- GPU selection: an RX 9070 XT is also installed in the machine, so `gpu_device` was set to `NVIDIA GeForce RTX 5080`. Every run's log `video device:` is the RTX 5080.
- Install directory: the maintainer's install `D:\Mihoyo\LostOdysseyRecomp-windows-x64` was used.
  - Only the exe was replaced, and the three XeSS DLLs (`libxess.dll`, `libxess_fg.dll`, `libxell.dll`, which that install did not originally have) were placed temporarily. The hashes of the other DLLs match the build output.
  - The game was started with `--game`, with a temporary working directory. `settings.ini`, `mod-list.ini`, `profile\` and the Uhra save are all there. The settings, mod list, saves and logs in the install directory were not read or written.
  - The temporary `mod-list.ini` set the install's three mods to off. The log reads `mod folders (highest first): none`.
  - After testing, the files were restored by hash (all 16 files matched), and the temporary DLLs were deleted.
- Foreground: DLSS-G only generates frames while the window has focus. The test script brought the game window to the foreground after entering the scene, and checked focus at the start and end of each measurement window. For runs that count in the results, both checks were in the foreground.
- Other load: other sessions were also using this machine. Before each run the script waited for other sessions' compiles (clang-cl, ninja) to finish. Runs where a compile process appeared in the measurement window, or focus was lost, were voided and re-run.

### psvita

- AMD Ryzen AI Max+ 395, Radeon 8060S integrated GPU (RADV, Mesa 26.2.2), Bazzite 44 (kernel 7.2.4), 2560x1600 120 Hz screen.
- The same Windows exe ran through umu-run + GE-Proton10-34. D3D12 used vkd3d-proton, and Vulkan used winevulkan. Visible 1920x1080 window, log `swapchain=1920x1080`.
- Power limit: `ryzenadj -i` was read at the start and end of each run. All 105 runs were at the defaults: STAPM 45 W, fast 62.5 W, slow 50 W.
  - Another session on the machine (low-end mode) runs power-limited tests. It shares the game lock with this test, so the two never ran at the same time.
- Temperature and "cooling":
  - Runs back to back ended at about 95 °C every time. The measurement window often had a few seconds of 90–100 ms stutter, and the median was also slower.
  - From 20:49, each run first waited for Tctl to drop below 84 °C (up to 90 seconds). The Vulkan, battle 720, and Uhra 1080 groups were measured after cooling. Most of the Uhra 720 group was measured before cooling, which is noted in that table's heading.
- Sharing: on psvita, other sessions also queue for the game lock. Each group held the lock once. The first run of each group was an unrecorded warm-up, and a baseline was inserted every 5 rows within the group.
- For each psvita row, the delta was interpolated in time between the two baselines before and after it in the same group, rather than using the group average.

### Run counts and time

- RTX 5080:
  - Round 1, 58 runs (18:42–19:52), were voided and kept as logs only. Reason: focus was not controlled. The DLSS-G 2x and 4x rows generated 0 frames, and the three baselines ranged from 5.24 to 6.73 ms.
  - Round 2, 102 rows, 120.6 minutes (19:53–21:54), one run per row. Another 3 runs were voided and re-run because a compile from another session appeared in the window or focus was lost.
  - Re-runs: 3 rows in round 2 differed from round 1 by more than the baseline noise, so each was re-run once (DLSS Performance, DLSS Quality model L, SSAO). The table uses the re-run values, and the notes give the first values. The FSR FG re-run and the rest of the re-run plan were not finished, because the maintainer needed the machine.
- psvita: 105 runs (19:14–22:01), including 3 warm-ups before start, 6 group warm-ups, and 11 re-runs after a "spike in the window". The lock was held 8 times, with 3–12 minutes waiting for the lock each time.
  - The planned re-runs of the two 1440 baseline groups, the 1080 battle group, and the full cooled Uhra 720 group were not run, in order to finish on time.

## How to read it

**The RTX 5080 Uhra baseline is not GPU-bound.** Frame interval 4.90 ms (201 FPS), GPU 2.29 ms, render thread 3.22 ms. The frame interval is set by the CPU side (game thread, render thread, present).

- Options that only add GPU load keep the frame interval Δ close to 0 as long as the GPU time stays below about 4.2 ms. Internal resolution 1440 is like this: GPU +0.96 ms, frame interval unchanged.
- So the tables give two deltas:
  - Frame interval Δ: what the player actually sees on this machine and in this scene.
  - GPU Δ: the extra GPU time the option costs. On a GPU-limited machine (such as psvita's battle), this adds directly to the frame interval.
- Options with a large render-thread Δ lengthen the frame interval even on a fast GPU. TAA, all upscalers, and all frame generation are in this group.
- Limit side:
  - GPU: GPU time ≥ 85% of the frame interval.
  - CPU: GPU time below 85% of the frame interval only means it is not GPU-limited.
- Noise:
  - The four RTX 5080 Uhra baselines were 5.00 / 4.85 / 4.89 / 4.85 ms, a spread of 0.15 ms. A single run occasionally runs slow overall (DLSS Performance first run 9.02 ms, re-run 6.03 ms). Differences within 0.15 ms are treated as noise.
  - psvita's CPU-side noise is much larger: for the same setting, baseline frame intervals range from 11.2 to 15.2 ms, the Vulkan baseline's render thread is 11.8–12.6 ms, and one row in the same group was as fast as 8.1 ms. On psvita, frame interval Δ and render-thread Δ only count when they clearly exceed this range. GPU Δ is much more stable, and is used as the reference.

## RTX 5080

### Uhra residential area, internal 720 baseline

Baseline (average of four runs): frame interval 4.90 ms, GPU 2.29 ms, render thread 3.22 ms, 201 FPS.

| Option | Value | Frame interval Δms | GPU Δms | Render thread Δms | Limit side | Notes |
|---|---|---|---|---|---|---|
| Internal resolution | 1080 | +0.08 | +0.46 | +0.06 | CPU | 193 FPS |
| Internal resolution | 1440 (supersampled to 1080) | -0.01 | +0.96 | -0.02 | CPU | 201 FPS; log `effective=2560x1440` |
| Anti-aliasing | FXAA | +0.01 | +0.01 | +0.02 | CPU | 201 FPS |
| Anti-aliasing | SMAA | -0.01 | +0.03 | +0.01 | CPU | 202 FPS |
| Anti-aliasing | TAA | +1.14 | +0.21 | +1.11 | CPU | 163 FPS; `record_ms` 1.02 → 1.63 |
| Ambient occlusion | SSAO | +0.11 | +0.11 | +0.10 | CPU | 193 FPS; re-run value, first run 5.31 ms |
| Ambient occlusion | GTAO | +0.12 | +0.12 | +0.10 | CPU | 189 FPS |
| Anisotropic filtering | 2x / 4x / 8x / 16x | -0.09 / -0.07 / -0.05 / -0.01 | 0.00 / 0.00 / 0.00 / +0.01 | -0.05 / -0.05 / -0.04 / 0.00 | CPU | 201–205 FPS |
| Shadow resolution | 2x | -0.03 | +0.09 | -0.02 | CPU | 202 FPS |
| Shadow resolution | 4x | +0.02 | +0.42 | 0.00 | CPU | 199 FPS |
| Depth of field | 0 (off) | 0.00 | 0.00 | -0.02 | CPU | 201 FPS |
| Bloom | Off | +0.10 | +0.01 | +0.07 | CPU | 195 FPS |
| Motion blur | Off | -0.04 | 0.00 | -0.04 | CPU | 203 FPS |
| Dynamic shadows | Off | -0.64 | -0.21 | -0.52 | CPU | 231 FPS; draws about 2140 → 1866 |
| Frustum culling | 0% | +0.42 | +0.17 | +0.32 | CPU | 185 FPS; draws about 2140 → 2428 |
| Frustum culling | 200% | -0.32 | -0.16 | -0.27 | CPU | 216 FPS; draws about 2140 → 1980 |
| Scaling filter | Standard | -0.03 | 0.00 | -0.04 | CPU | 202 FPS |
| HDR | On | -0.03 | +0.05 | 0.00 | CPU | 202 FPS; log `display_active=true`, in-window HDR `direct` frame count kept rising |
| Performance info | FPS | -0.07 | 0.00 | -0.05 | CPU | 205 FPS |
| Performance info | Detailed | -0.08 | 0.00 | -0.07 | CPU | 205 FPS |
| VSync | On | -0.04 | 0.00 | -0.05 | CPU | 204 FPS; no effect under `LO_FPS=0`, see below |
| VSync | FreeSync / G-SYNC | -0.06 | 0.00 | -0.05 | CPU | 205 FPS; same as above |
| Graphics API | Vulkan | -0.26 | -1.20 | -0.05 | CPU | 213 FPS; the basis of the GPU numbers is explained in "Notable findings" |

The VSync and FreeSync / G-SYNC rows only confirm that, with `LO_FPS=0`, the vsync the game requests is always false (log `requested_vsync=false`). These settings have no effect here, and the numbers match the baseline. In a normal capped run they limit the frame rate near the refresh rate. That was not measured here.

### Upscaling (Uhra, internal 720 baseline)

The upscaling input size is set by the quality tier, not by "internal resolution". The logs confirm this. Each cell is Δms relative to the original baseline.

| Upscaler | Tier | Input → output | Frame interval Δms | GPU Δms | Render thread Δms | Limit side | FPS | Notes |
|---|---|---|---|---|---|---|---|---|
| DLSS | Quality | 1280x720 → 1920x1080 | +1.11 | +0.83 | +1.14 | CPU | 164 | |
| DLSS | Balanced | 1114x626 → 1920x1080 | +1.22 | +0.71 | +1.22 | CPU | 159 | |
| DLSS | Performance | 960x540 → 1920x1080 | +1.13 | +0.53 | +1.17 | CPU | 163 | Re-run value, first run 9.02 ms |
| DLSS | DLAA | 1920x1080 → 1920x1080 | +1.12 | +2.13 | +1.16 | CPU | 163 | GPU 4.42 ms |
| DLSS | Quality, model L | 1280x720 → 1920x1080 | +1.00 | +1.01 | +1.03 | CPU | 167 | Re-run value, first run 6.85 ms; log `preset=L` |
| FSR | Quality | 1280x720 → 1920x1080 | +1.20 | +0.38 | +1.22 | CPU | 161 | |
| FSR | Balanced | 1129x635 → 1920x1080 | +1.34 | +0.36 | +1.33 | CPU | 158 | |
| FSR | Performance | 960x540 → 1920x1080 | +1.09 | +0.23 | +1.12 | CPU | 164 | |
| FSR | Native AA | 1920x1080 → 1920x1080 | +1.06 | +0.95 | +1.11 | CPU | 165 | |
| XeSS | Quality | 1130x636 → 1920x1080 | +1.04 | +0.75 | +1.10 | CPU | 165 | |
| XeSS | Balanced | 960x540 → 1920x1080 | +1.17 | +0.66 | +1.20 | CPU | 163 | |
| XeSS | Performance | 835x470 → 1920x1080 | +1.11 | +0.60 | +1.13 | CPU | 164 | |
| XeSS | Native AA | 1920x1080 → 1920x1080 | +1.02 | +1.49 | +1.07 | CPU | 167 | |

- Every tier adds 1.0–1.3 ms to the frame interval, almost all of it from the render thread (`record_ms` 1.02 → about 1.75). Lowering the input size saves only GPU time, not frame interval.
- At 1080p the Quality tier's input is the original 1280x720. So the GPU Δ of DLSS / FSR Quality is the upscaler's own GPU cost: DLSS 0.83 ms, FSR 0.38 ms.

### DLSS 5 neural rendering (Uhra, DLSS Quality, internal 720)

- DLL: the maintainer-supplied `nvngx_dlssnr.dll` (165,840,496 bytes), pointed to with `LO_DLSS_NR_PATH` at the copy in the download directory. It was not placed in the install directory. All five rows below set this environment variable, and differ only in `dlss_neural_rendering`.
- NR presets, style and strength use the default values.
- The log confirms NR is running: `DLSS NR: snippet initialized`, `DLSS NR: N feature(s) created for 1920x1080`, `first evaluate recorded for 1920x1080, N pass(es)`.

| Row | FPS | Frame interval ms | GPU ms | Render thread ms | Limit side | NR vs control: frame / GPU / render thread Δms |
|---|---|---|---|---|---|---|
| DLSS Quality (control, dll present, NR off) | 164 | 6.00 | 3.12 | 4.34 | CPU | |
| NR 1 pass | 123 | 8.04 | 7.90 | 5.48 | GPU | +2.04 / +4.78 / +1.14 |
| NR 2 pass | 79 | 12.69 | 12.56 | 5.86 | GPU | +6.69 / +9.44 / +1.52 |
| NR 3 pass | 59 | 16.95 | 16.79 | 6.60 | GPU | +10.95 / +13.67 / +2.26 |
| NR 4 pass | 47 | 21.18 | 21.02 | 7.19 | GPU | +15.18 / +17.90 / +2.85 |

Each additional pass costs about 4.3–4.8 ms more GPU. From 1 pass on, it becomes GPU-limited. In the 4K report, the GPU cost of 1 pass was +14.58 ms. At 1080p it is about one third of that.

### Frame generation (upscaling off, internal 720)

Frame interval, GPU and render thread are all counted on the frames the game renders itself. The output frame rate uses the SDK count (`actual_presents` in the `D3D12 FG` log). In window mode it is shown by the compositor, so it is not the frame rate measured on the monitor.

| Frame generation | Render frame rate | Frame interval Δms | GPU Δms | Render thread Δms | Output frame rate (SDK count) | Notes |
|---|---|---|---|---|---|---|
| DLSS-G 2x | 177 | +0.69 | +0.37 | +0.78 | 353/s | |
| DLSS-G 3x | 175 | +0.73 | +0.38 | +0.81 | 525/s | |
| DLSS-G 4x | 172 | +0.81 | +0.39 | +0.80 | 692/s | |
| FSR FG | 110 | +3.96 | +0.35 | +2.48 | Not available from the log | `active=true accepted=true`, SDK count stayed at 0; re-run not finished, and the voided first round (focus not controlled) was 7.89 ms |
| XeSS FG | 158 | +1.31 | +0.34 | +0.82 | 319/s | |

- The cost of the three DLSS-G multipliers is almost the same. The extra frames do not add to the render-side cost.
- GPU Δ includes only the render queue. The work of SDK-generated frames is not in this timing.

### Battle (formation 3, first command), internal 720 baseline

Baseline (average of two runs): frame interval 1.80 ms, GPU 0.70 ms, render thread 0.79 ms, 540 FPS.

| Option | Value | Frame interval Δms | GPU Δms | Render thread Δms | Limit side | Notes |
|---|---|---|---|---|---|---|
| Internal resolution | 1080 | -0.11 | +0.42 | -0.03 | CPU | 569 FPS |
| Internal resolution | 1440 (supersampled to 1080) | +0.17 | +1.05 | -0.02 | GPU | 502 FPS |
| Anti-aliasing | SMAA | -0.06 | +0.04 | 0.00 | CPU | 557 FPS |
| Anti-aliasing | TAA | +0.08 | +0.16 | +0.12 | CPU | 523 FPS |
| Ambient occlusion | GTAO | -0.06 | +0.12 | -0.01 | CPU | 553 FPS |
| Anisotropic filtering | 16x | -0.12 | +0.04 | -0.04 | CPU | 574 FPS |
| Shadow resolution | 4x | -0.11 | +0.14 | -0.03 | CPU | 571 FPS |
| DLSS | Quality | +0.24 | +0.79 | +0.21 | CPU | 490 FPS |
| FSR | Quality | +0.25 | +0.36 | +0.23 | CPU | 489 FPS |

The battle scene has only about 260 draws, so the render-thread cost of TAA and upscalers is much smaller (+0.1–0.2 ms). The frame interval is about 1.8 ms, and "CPU" only means it is not GPU-limited.

### Internal 1080 as the baseline

Conditions as above. Only the baseline internal resolution was changed to 1080.

Uhra (average of two runs): frame interval 4.92 ms, GPU 2.75 ms, render thread 3.23 ms, 199 FPS.

| Option | Value | Frame interval Δms | GPU Δms | Render thread Δms | Limit side | Notes |
|---|---|---|---|---|---|---|
| Anti-aliasing | FXAA | +0.28 | +0.04 | +0.24 | CPU | 186 FPS; run only once, frame interval difference exceeded baseline noise, not re-run; GPU Δ is consistent with the other baselines |
| Anti-aliasing | SMAA | -0.02 | +0.07 | +0.03 | CPU | 199 FPS |
| Anti-aliasing | TAA | +0.97 | +0.38 | +0.98 | CPU | 168 FPS |
| Ambient occlusion | SSAO | +0.03 | +0.22 | +0.06 | CPU | 198 FPS |
| Ambient occlusion | GTAO | +0.01 | +0.25 | +0.05 | CPU | 200 FPS |
| Anisotropic filtering | 16x | +0.07 | 0.00 | +0.07 | CPU | 195 FPS |
| Shadow resolution | 4x | +0.20 | +0.41 | +0.17 | CPU | 191 FPS |
| HDR | On | +0.08 | +0.10 | +0.09 | CPU | 196 FPS |

Battle (average of two runs): frame interval 1.76 ms, GPU 1.17 ms, render thread 0.78 ms, 552 FPS.

| Option | Value | Frame interval Δms | GPU Δms | Render thread Δms | Limit side | Notes |
|---|---|---|---|---|---|---|
| Anti-aliasing | SMAA | -0.03 | +0.05 | +0.01 | CPU | 557 FPS |
| Anti-aliasing | TAA | +0.21 | +0.29 | +0.16 | CPU | 507 FPS |
| Ambient occlusion | GTAO | 0.00 | +0.23 | +0.01 | CPU | 553 FPS |
| Anisotropic filtering | 16x | -0.08 | -0.04 | -0.02 | CPU | 575 FPS |
| Shadow resolution | 4x | -0.05 | +0.13 | -0.02 | CPU | 566 FPS |

### Internal 1440 → 1080 (supersampling) as the baseline

Conditions as above. Only the baseline internal resolution was changed to 1440. Without upscaling, the scene is rendered at 2560x1440 and presentation scales it to the 1080 window. With upscaling on (#332), 1440 becomes the upscaler's output size, and then it is scaled to the window.

Uhra (average of two runs): frame interval 4.90 ms, GPU 3.25 ms, render thread 3.23 ms, 201 FPS.

| Option | Value | Frame interval Δms | GPU Δms | Render thread Δms | Limit side | Notes |
|---|---|---|---|---|---|---|
| Anti-aliasing | FXAA | +0.03 | +0.04 | +0.03 | CPU | 200 FPS |
| Anti-aliasing | SMAA | -0.01 | +0.24 | +0.01 | CPU | 202 FPS |
| Anti-aliasing | TAA | +0.95 | +0.83 | +0.96 | CPU | 169 FPS |
| Ambient occlusion | SSAO | +0.11 | +0.69 | +0.11 | CPU | 197 FPS |
| Ambient occlusion | GTAO | -0.01 | +0.74 | 0.00 | CPU | 202 FPS |
| Anisotropic filtering | 16x | +0.08 | +0.04 | +0.04 | CPU | 196 FPS |
| Shadow resolution | 4x | -0.10 | +0.70 | -0.10 | CPU | 207 FPS |
| HDR | On | +0.01 | +0.29 | +0.02 | CPU | 200 FPS |
| DLSS | Quality (1707x960 → 2560x1440 → 1080) | +1.11 | +1.09 | +1.12 | CPU | 164 FPS |
| DLSS | DLAA (2560x1440 → 2560x1440 → 1080) | +1.78 | +3.15 | +1.23 | GPU | 148 FPS; GPU 6.40 ms |
| FSR | Quality (1706x960 → 2560x1440 → 1080) | +1.12 | 0.00 | +1.11 | CPU | 162 FPS; GPU same as baseline, the fewer pixels rendered offset FSR's own cost |
| FSR | Native AA (2560x1440 → 2560x1440 → 1080) | +1.11 | +1.25 | +1.12 | CPU | 164 FPS |
| XeSS | Quality (1506x848 → 2560x1440 → 1080) | +1.19 | +0.61 | +1.20 | CPU | 161 FPS |

Battle (average of two runs): frame interval 2.00 ms, GPU 1.72 ms, render thread 0.78 ms, 494 FPS. This baseline is already GPU-limited, and frame interval Δ is close to GPU Δ.

| Option | Value | Frame interval Δms | GPU Δms | Render thread Δms | Limit side | Notes |
|---|---|---|---|---|---|---|
| Anti-aliasing | SMAA | +0.12 | +0.20 | +0.01 | GPU | 464 FPS |
| Anti-aliasing | TAA | +0.55 | +0.74 | +0.19 | GPU | 387 FPS |
| Ambient occlusion | GTAO | +0.46 | +0.63 | +0.08 | GPU | 403 FPS |
| Anisotropic filtering | 16x | +0.02 | +0.08 | -0.02 | GPU | 490 FPS |
| Shadow resolution | 4x | +0.13 | +0.22 | 0.00 | GPU | 463 FPS |

## psvita

### Battle, internal 720 baseline (after cooling, D3D12)

This is the cleanest group on psvita: the scene is GPU-limited, and the three baselines differ by only 0.2 ms.

Baselines (three runs): frame interval 5.71 / 5.65 / 5.85 ms, GPU 5.18 / 5.13 / 5.26 ms, render thread about 3.75 ms, about 175 FPS.

| Option | Value | Frame interval Δms | GPU Δms | Render thread Δms | Limit side | Notes |
|---|---|---|---|---|---|---|
| Internal resolution | 1080 | +5.47 | +5.31 | -0.05 | GPU | 90 FPS |
| Internal resolution | 1440 (supersampled to 1080) | +13.64 | +13.44 | +1.23 | GPU | 51 FPS |
| Anti-aliasing | SMAA | -0.02 | -0.07 | +0.02 | GPU | 175 FPS |
| Anti-aliasing | TAA | +0.98 | +0.98 | +0.15 | GPU | 151 FPS |
| Ambient occlusion | GTAO | +1.35 | +1.29 | -0.16 | GPU | 140 FPS |
| Anisotropic filtering | 16x | -0.52 | -0.48 | +0.01 | GPU | 192 FPS; faster than baseline, cause not investigated |
| Shadow resolution | 4x | +1.02 | +0.96 | +0.28 | GPU | 152 FPS |
| FSR | Quality | +2.65 | +2.66 | +0.27 | GPU | 119 FPS |
| XeSS | Quality | +3.59 | +3.60 | +0.15 | GPU | 107 FPS |

### Uhra residential area, internal 1080 baseline (after cooling, D3D12)

Baselines (three runs): frame interval 11.29 / 12.30 / 12.53 ms, GPU 5.96 / 5.80 / 5.69 ms, render thread 9.83 / 10.73 / 10.95 ms, about 83 FPS. Render-thread limited, not GPU-limited.

| Option | Value | Frame interval Δms | GPU Δms | Render thread Δms | Limit side | Notes |
|---|---|---|---|---|---|---|
| Anti-aliasing | FXAA | +0.08 | +0.16 | +0.07 | CPU | 86 FPS |
| Anti-aliasing | SMAA | +0.12 | +0.36 | +0.09 | CPU | 85 FPS |
| Anti-aliasing | TAA | +2.12 | +0.48 | +2.06 | CPU | 72 FPS |
| Ambient occlusion | SSAO | +0.28 | +0.74 | +0.24 | CPU | 81 FPS |
| Ambient occlusion | GTAO | +0.24 | +0.78 | +0.23 | CPU | 81 FPS |
| Anisotropic filtering | 16x | +0.18 | +0.09 | +0.14 | CPU | 79 FPS |
| Shadow resolution | 4x | +0.47 | +1.54 | +0.39 | CPU | 77 FPS |

### Uhra residential area, internal 720 baseline (D3D12, mostly not cooled)

This group started before cooling was added. The frame intervals of five baselines range from 11.17 to 15.15 ms, and GPU from 3.82 to 4.63 ms. Frame interval Δ and render-thread Δ within ±2 ms are treated as noise. Only GPU Δ and large differences are considered.

| Option | Value | Frame interval Δms | GPU Δms | Render thread Δms | Limit side | Notes |
|---|---|---|---|---|---|---|
| Internal resolution | 1080 | +0.49 | +4.27 | +0.41 | CPU | 74 FPS; GPU 8.16 ms |
| Internal resolution | 1440 (supersampled to 1080) | +12.58 | +20.39 | +5.56 | GPU | 39 FPS; GPU 24.29 ms |
| Anti-aliasing | FXAA | +0.68 | +0.70 | +0.59 | CPU | within noise |
| Anti-aliasing | SMAA | +0.86 | +0.89 | +0.69 | CPU | within noise |
| Anti-aliasing | TAA | +2.44 | +0.63 | +2.37 | CPU | 62 FPS |
| Ambient occlusion | SSAO | +0.66 | +0.97 | +0.55 | CPU | |
| Ambient occlusion | GTAO | +0.32 | +0.84 | +0.28 | CPU | |
| Anisotropic filtering | 2x | | | | | Data conflict: the window had long stalls (mean 47 ms), not counted |
| Anisotropic filtering | 4x / 8x / 16x | +1.40 / +0.07 / +0.12 | +0.73 / +0.55 / +0.60 | +1.18 / +0.04 / +0.06 | CPU | within noise |
| Shadow resolution | 2x | +0.20 | +0.98 | +0.13 | CPU | |
| Shadow resolution | 4x | +0.20 | +4.68 | -0.02 | CPU | GPU 9.00 ms; paired re-run GPU +4.33 |
| Depth of field / Bloom / Motion blur off | | -0.60 / +0.29 / -0.55 | +0.28 / +0.25 / +0.25 | | CPU | within noise |
| Dynamic shadows | Off | -2.95 | -1.06 | -2.88 | CPU | Window had spikes; direction matches RTX 5080 |
| Frustum culling | 0% / 200% | -0.28 / -0.50 | +0.07 / +0.11 | | CPU | within noise |
| Scaling filter | Standard | +0.04 | +0.18 | +0.01 | CPU | within noise |
| HDR | On | | | | | Not available: under Proton `display_active=false`, `scene_enabled=false` |
| Performance info | FPS / Detailed | +0.49 / -0.09 | +0.15 / +0.11 | | CPU | within noise |
| VSync / FreeSync | On | | | | | No effect under `LO_FPS=0` (`requested_vsync=false`), same as RTX 5080 |
| FSR | Quality / Balanced / Performance | +2.09 / +2.80 / +1.97 | +2.77 / +1.97 / +0.16 | +1.99 / +2.59 / +1.91 | CPU | paired re-run FSR Quality GPU +2.80 |
| FSR | Native AA | +2.95 | +3.99 | +2.82 | CPU | GPU 7.89 ms |
| XeSS | Quality / Balanced / Performance | +2.29 / +2.36 / +2.35 | +1.16 / +0.79 / +0.64 | +2.21 / +2.29 / +2.27 | CPU | paired re-run XeSS Quality GPU +3.40, the two runs disagreed |
| XeSS | Native AA | +4.37 | +8.31 | +4.10 | CPU | GPU 12.31 ms |
| Frame generation | FSR FG | +6.79 | +0.55 | +2.87 | CPU | SDK count 0, output frame rate unknown |
| Frame generation | XeSS FG | +5.04 | +0.71 | +3.87 | CPU | Render 54 FPS, SDK count output 108/s |

Paired re-runs (baseline and option alternated, most windows still had stutter) GPU Δ: shadow 4x +4.33, GTAO +0.54, SSAO +0.33, TAA +0.27, internal 1080 +3.73, internal 1440 +17.95, FSR Quality +2.80, XeSS Quality +3.40.

### Vulkan (after cooling, Uhra, internal 720 baseline)

FSR was measured separately on Vulkan. Three Vulkan baselines: frame interval 15.47 / 14.51 / 14.39 ms, GPU 2.73 / 2.72 / 2.69 ms, render thread 12.55 / 11.85 / 11.76 ms.

The render-thread time in this group differs a lot from row to row (one internal 1080 row in the same group was only 8.09 ms). The frame interval Δ is not reliable, so the table gives only GPU Δ.

| Option | Value | GPU Δms | GPU ms | Notes |
|---|---|---|---|---|
| Internal resolution | 1080 | +1.39 | 4.11 | |
| FSR | Quality | +3.39 | 6.12 | |
| FSR | Balanced | +2.94 | 5.66 | |
| FSR | Performance | +2.33 | 5.05 | |
| FSR | Native AA | +5.93 | 8.65 | |
| Frame generation | FSR FG | +1.95 | 4.65 | Vulkan FSR FG gave no SDK count output |

## Notable findings

- **At 1080p, the RTX 5080 is GPU-limited only with DLSS 5 NR.** The original baseline of 4.90 ms (201 FPS) is set by the CPU side. Options that only add GPU load, such as internal resolution 1440, shadow 4x, AO and AF, leave the frame interval unchanged.
- **The cost of TAA, all upscalers and frame generation is on the render thread, about 1 ms.** TAA +1.14 ms, upscaler tiers +1.0–1.3 ms, DLSS-G +0.7–0.8 ms, XeSS FG +1.3 ms. Tier and input size only affect GPU. In the 4K report (v0.9.22), the render-thread cost of TAA was +1.83 ms and of upscalers +1.9–2.2 ms. The render-thread cost does not depend on output resolution, so this is a change in main relative to v0.9.22. Which commit caused it was not investigated here.
- **FSR FG is the most expensive.** On the RTX 5080 it is +3.96 ms (render thread +2.48 ms), much higher than DLSS-G and XeSS FG. The SDK count stayed at 0, so its output frame rate cannot be seen. It is also 0 on psvita.
- **The DLSS-G multiplier hardly affects the cost.** The frame interval Δ for 2x / 3x / 4x is +0.69 / +0.73 / +0.81 ms, and the SDK-counted output is 2 / 3 / 4 times the render frame rate.
- **Dynamic shadows off and frustum culling 200% are two options that save CPU.** Dynamic shadows off -0.64 ms (about 270 fewer draws), culling 200% -0.32 ms (about 160 fewer draws). Culling 0% adds +0.42 ms.
- **DLSS 5 NR costs about 4.3–4.8 ms of GPU per pass at 1080p.** One pass alone makes the RTX 5080 GPU-limited (123 FPS), and four passes give 47 FPS.
- **At 1080p output with internal 720, psvita is still render-thread limited.** Uhra is about 75–85 FPS, with the GPU at only 4–6 ms. Options with high GPU cost are clear on psvita:
  - Internal 1440 pushes the GPU to about 24 ms (about 40 FPS), the most expensive single option.
  - Shadow 4x adds +4.3–4.7 ms GPU, against +0.42 ms on the RTX 5080.
  - XeSS Native AA adds +8.3 ms GPU, FSR Native AA +4.0 ms.
  - The battle scene is GPU-limited on psvita, so GTAO +1.3 ms, TAA +1.0 ms and shadow 4x +1.0 ms show up directly in the frame interval.
- **Vulkan's GPU timing numbers are about half those of D3D12.** Both backends use the same timing method (a timestamp at the start and end of each submission batch: plume's Vulkan uses `BOTTOM_OF_PIPE`, and D3D12 uses `EndQuery`), but the batch division and driver queuing differ. This difference was not verified, so do not read it as "Vulkan does half the GPU work". On frame interval, Vulkan is 0.26 ms faster than D3D12, which is at the edge of noise.
- **Back-to-back psvita tests hit a temperature wall.** Every run climbed to 95 °C, and the measurement windows showed several seconds of 90–100 ms stutter. Waiting until below 84 °C before each run made the windows much cleaner. Future psvita performance tests should use this cooling by default.

## Items not measured and why

- Stopped at the end of this round and not run:
  - The RTX 5080 FSR FG re-run, and the second re-runs of DLSS Performance, model L and SSAO. These three rows use a single re-run value in the table.
  - The two psvita internal 1440 baseline groups (Uhra, battle), and the internal 1080 battle group. The 1080 battle group had only a warm-up and one baseline.
  - The full cooled Uhra 720 group re-run on psvita. Most of the data in that table group was collected before cooling.
- DLSS, DLSS 5 NR and DLSS-G on psvita: these need an NVIDIA GPU.
- HDR on psvita: under Proton `display_active=false`, so there is no measurable HDR output.
- The real effect of VSync and FreeSync / G-SYNC: they have no effect under `LO_FPS=0`. Measuring them needs a capped run, which is outside the method of this report.
- MetalFX: macOS only.
- Vulkan on this machine: only the baseline row was measured. The FSR Vulkan rows were measured on psvita only.

## Raw logs

The raw logs, per-run settings, screenshots and window statistics are on this machine at `C:\Users\freefrank\worktrees\LostOdysseyRecomp\_keep\options-1080p\`. They are not distributed with the repository:

- `runs\<label>\`: each RTX 5080 round-2 and re-run; `runs-pass1\`: the voided first round; `runs-trial\`: the three trial runs before the start.
- `psvita\runs\<label>\`: each psvita run (`*-disturbed` is a run where the window had a spike and was replaced by a re-run).
- `batch.out` (this machine), `psvita\batch.out`: one line per run, including focus, compile process, power and temperature checks.
- `results.json`, `psvita\results.json`: window statistics for each run.
- Scripts: `gc1080.py`, `batch1080.py`, `an1080.py`, `tables.py`, `install_guard.py` (this machine), `psvita\gcv.py`, `psvita\batchv.py`.
- Install directory restore records: `install-hashes-before.json`, `install-hashes-after.json`.
