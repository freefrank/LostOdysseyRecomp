# Cost of Graphics Enhancement Options: v0.9.22, RTX 5080, 2026-10-10

[简体中文](graphics-options-cost-2026-10-10.zh-CN.md)

This record gives the extra milliseconds each graphics enhancement option adds over the "original" settings. Each row changes only one setting. The rest stay at the original values.

## Conditions

- Version: v0.9.22 Windows release package (`d3888a0d`), Direct3D 12.
- Host: Windows 11 Pro (10.0.28000), AMD Ryzen 7 9800X3D, NVIDIA GeForce RTX 5080, driver 616.56.
- Display: the game runs in the foreground, borderless full screen at 3840x2160. The monitor is 144 Hz, with HDR on in Windows. Except for the HDR row, the game outputs SDR.
- GPU selection: an RX 9070 XT is also installed in the machine (no monitor connected). With `gpu_device` left empty, v0.9.22 chose the RX 9070 XT, so the tests set `NVIDIA GeForce RTX 5080`. The `video device:` line in each run's log confirms RTX 5080.
- Uncapped: `LO_FPS=0`. The log shows `native_target=0 host_cap=0`, and the requested vsync is false.
- Shaders: `portable_dx12-613e43cab7fde729` matched. The pipeline corpus `pipelines_corpus-1766361aa92f292b` (3974 entries) sits next to the package, as it does when players download it automatically. The number of new pipelines created in every measurement window was 0.
- Original definition:
  - Internal resolution 720.
  - Anti-aliasing, ambient occlusion and anisotropic filtering all off.
  - Shadows 1x, frustum culling 100%.
  - Super resolution, frame generation and HDR all off.
  - Depth of field 100, bloom, motion blur and dynamic shadows kept on (these are original effects).
  - FSR sharpening 0 (RCAS off, the config default).
  - DLSS model at the default M.
- Scenes:
  - Uhra residential area save (`u3b_0_scrw`), character standing still.
  - Battle: formation 3 opened with `LO_DEBUG_BATTLE_FILE` at the same spot, stopped at the first command. The camera faces Kaim. About 270 draws. The picture is much lighter than Uhra.
- Sampling: after entering the scene (for battle, until the first command), wait 10 seconds, then take the median of each frame's values in the 10-second window.
  - Frame interval: `present timing frame_ms`.
  - GPU: `gpu_queue_batches_elapsed_ms`, which counts only the render queue, not present or compositing.
  - Render thread: `draw_ms`.
- Runs: one run per row, with no reruns.
  - First batch (internal 720 baseline): Uhra 30 runs, battle 7 runs, 31.7 minutes total.
  - Second batch (internal 2160 baseline) and DLSS 5 neural rendering: Uhra 16 runs, battle 6 runs, 19.3 minutes total.
  - Three other trial runs on this machine are not counted in the results.
- RTSS runs in the background (only the OSD is on, global frame limit 0). The test did not change it.

## How to read this

**The Uhra original baseline is CPU-limited.** Frame interval 6.78 ms (146.5 FPS). The GPU takes only 2.33 ms, the render thread 5.06 ms. The frame interval is set by the CPU side, not the GPU.

- For options that add only GPU load, as long as the GPU time stays below about 6.8 ms, the frame interval change Δ stays close to 0. Internal resolution 2160 is an example: GPU +3.27 ms, but frame interval only +0.02 ms.
- So the main table gives two kinds of increase:
  - Frame interval Δ: the change the player actually sees on this machine and in this scene.
  - GPU Δ: the extra GPU time the option takes. On a GPU-limited machine (a weaker graphics card, or a baseline GPU time already close to the frame interval), it adds directly to the frame interval.
- Options with a large render-thread Δ lengthen the frame interval even on a fast graphics card. TAA, all super resolution modes and all three frame generation modes are in this group. Each adds 1.8–2.2 ms to the render thread, and `record_ms` rises from 1.45 ms to about 3.0 ms.
- Limit side:
  - GPU: GPU time ≥ 85% of the frame interval.
  - CPU: GPU time below 85% of the frame interval only means not GPU-limited. In Uhra, the render thread `draw_ms` of these rows is 74–81% of the frame interval, and no row reaches 85%. In battle it is about 54%.
- Drift:
  - The three Uhra baselines (start, middle, end) are frames 6.75 / 6.79 / 6.80 ms, GPU 2.32 / 2.33 / 2.33 ms, render thread 5.05 / 5.07 / 5.07 ms. The largest drift is 0.05 ms.
  - The two battle baselines are frames 2.22 / 2.20 ms.
  - All increments are relative to the baseline average. Differences within 0.1 ms are the same order of size as the difference between baselines.

## Main table: Uhra residential area (first batch, internal 720 baseline)

Baseline (average of three runs): frame interval 6.78 ms, GPU 2.33 ms, render thread 5.06 ms, 146.5 FPS.

| Option | Value | Frame interval Δms | GPU Δms | Render thread Δms | Limit side | Notes |
|---|---|---|---|---|---|---|
| Internal resolution | 1080 | -0.06 | +0.48 | -0.03 | CPU | 148 FPS |
| Internal resolution | 1440 | +0.01 | +1.00 | +0.02 | CPU | 146 FPS |
| Internal resolution | 2160 | +0.02 | +3.27 | +0.01 | CPU | 146 FPS; GPU 5.60 ms |
| Anti-aliasing | FXAA | +0.04 | +0.01 | +0.04 | CPU | 145 FPS |
| Anti-aliasing | SMAA | +0.02 | +0.04 | +0.03 | CPU | 146 FPS |
| Anti-aliasing | TAA | +1.84 | +1.01 | +1.83 | CPU | 115 FPS; `record_ms` 1.45 → 2.87 |
| Ambient occlusion | SSAO | +0.09 | +0.10 | +0.10 | CPU | 145 FPS |
| Ambient occlusion | GTAO | -0.01 | +0.12 | -0.01 | CPU | 147 FPS |
| Anisotropic filtering | 4x | -0.03 | +0.01 | 0.00 | CPU | 147 FPS |
| Anisotropic filtering | 16x | 0.00 | +0.01 | +0.02 | CPU | 146 FPS |
| Shadow resolution | 2x | +0.11 | +0.15 | +0.08 | CPU | 140 FPS; frame interval average 7.16 ms, with spikes in the window |
| Shadow resolution | 4x | -0.03 | +0.59 | -0.02 | CPU | 147 FPS |
| Frustum culling | 0% | +0.60 | +0.19 | +0.57 | CPU | 134 FPS; draw count 2142 → 2425 |
| HDR | On | -0.02 | +0.05 | +0.02 | CPU | 143 FPS; log `display_active=true`. The number of HDR `direct` frames in the window kept rising, `gain=0` |
| All enhancements on | Internal 2160 + TAA + GTAO + AF 16x + shadow 4x + culling 0% | +4.32 | +8.20 | +2.92 | GPU | 90 FPS; frame 11.10 / GPU 10.53 / render thread 7.98 ms |
| Reference: original effects all off | Depth of field 0, bloom off, motion blur off | +0.03 | 0.00 | +0.03 | CPU | 146 FPS. This row is a reduction in load, not an enhancement |

At internal resolution 720, the GPU cost of AO, shadows and anisotropic filtering is small. Their cost at internal 2160 is in the second batch below.

## Super resolution

The input size of super resolution is set by the preset and is independent of the "internal resolution" setting. The log confirmed this. Each row gives two increments:

- Relative to the original baseline (internal 720, no super resolution).
- Relative to a run with the same input size and no super resolution, which is the cost of super resolution itself.

XeSS has no matching-size control, because internal resolution can only be 720/1080/1440/2160. The table shows its neighboring controls on both sides.

Each cell is the Δms of "frame interval / GPU / render thread".

| Super resolution | Preset | Input → output | Δ vs. original | Same-input control | Δ vs. same input without SR | Limit side | FPS |
|---|---|---|---|---|---|---|---|
| DLSS | DLAA | 3840x2160 → 3840x2160 | +6.61 / +10.70 / +3.88 | internal 2160 | +6.59 / +7.43 / +3.86 | GPU | 71 |
| DLSS | Quality | 2560x1440 → 3840x2160 | +1.96 / +5.44 / +1.95 | internal 1440 | +1.95 / +4.44 / +1.93 | GPU | 113 |
| DLSS | Performance | 1920x1080 → 3840x2160 | +2.16 / +3.82 / +2.15 | internal 1080 | +2.23 / +3.33 / +2.18 | CPU | 108 |
| FSR | Native AA | 3840x2160 → 3840x2160 | +3.04 / +6.92 / +2.09 | internal 2160 | +3.03 / +3.65 / +2.08 | GPU | 101 |
| FSR | Quality | 2560x1440 → 3840x2160 | +1.96 / +3.66 / +1.96 | internal 1440 | +1.95 / +2.65 / +1.94 | CPU | 113 |
| FSR | Performance | 1920x1080 → 3840x2160 | +1.96 / +2.70 / +1.98 | internal 1080 | +2.02 / +2.22 / +2.01 | CPU | 113 |
| XeSS | Quality | 2259x1271 → 3840x2160 | +2.04 / +4.12 / +2.05 | internal 1440 (larger); internal 1080 (smaller) | vs. 1440: +2.03 / +3.12 / +2.02; vs. 1080: +2.10 / +3.64 / +2.08 | CPU | 112 |
| XeSS | Performance | 1670x940 → 3840x2160 | +2.00 / +3.43 / +1.99 | internal 1080 (larger); internal 720, which is the original (smaller) | vs. 1080: +2.06 / +2.94 / +2.02; vs. 720: +2.00 / +3.43 / +1.99 | CPU | 113 |

- Against the same-input control, apart from DLAA (+6.59) and FSR Native AA (+3.03), each preset adds 1.95–2.23 ms to the frame interval, and 1.93–2.18 ms to the render thread.
- DLAA's GPU time is 13.03 ms, the highest of all single options.

## Frame generation (2x, super resolution off, internal 720)

Frame interval, GPU and render thread are all counted on the frames the game renders itself. The output frame rate uses the SDK count (`generated_intervals` / `actual_presents` in the `D3D12 FG` log), not the frame rate measured on the monitor.

| Frame generation | Render frame rate | Frame interval Δms | GPU Δms | Render thread Δms | Output frame rate (SDK count) | Notes |
|---|---|---|---|---|---|---|
| DLSS-G | 117.5 | +1.65 | +1.41 | +1.82 | 235.8 (generated 117.9/s) | |
| FSR FG | 92.5 | +3.91 | +1.46 | +1.80 | Not available from the log | Log `active=true accepted=true`, but the SDK count stayed at `generated_intervals=0 actual_presents=0` |
| XeSS FG | 104.4 | +2.66 | +1.41 | +1.84 | 209.4 (generated 104.7/s) | |

All three frame generation modes were enabled successfully in the foreground. The GPU Δ only covers the render queue. The SDK frame generation work is not in this timing.

## Battle (first batch, internal 720 baseline; formation 3, first command)

Baseline (average of two runs): frame interval 2.21 ms, GPU 0.75 ms, render thread 1.19 ms, 445 FPS.

| Option | Value | Frame interval Δms | GPU Δms | Render thread Δms | Limit side | Notes |
|---|---|---|---|---|---|---|
| Internal resolution | 2160 | +2.14 | +3.42 | +0.14 | GPU | 234 FPS; GPU 4.17 ms |
| Shadow resolution | 4x | +0.04 | +0.16 | +0.01 | CPU | 430 FPS |
| Ambient occlusion | GTAO | +0.01 | +0.10 | 0.00 | CPU | 442 FPS |
| Anisotropic filtering | 16x | 0.00 | +0.01 | 0.00 | CPU | 446 FPS |
| Anti-aliasing | SMAA | +0.04 | +0.02 | +0.02 | CPU | 435 FPS |

The frame interval of this battle picture is only about 2.2 ms. GPU (0.75 ms) and render thread (1.19 ms) are both clearly below the frame interval. "CPU" in the table only means not GPU-limited. At internal 2160, it becomes GPU-limited, and the GPU increase shows directly in the frame interval.

## Second batch: internal 2160 as the baseline

Conditions are the same as the first batch. Only the baseline internal resolution is changed to 2160. Everything else stays at the original values (enhancements all off). Each row changes one item from this baseline.

### Uhra residential area (internal 2160 baseline)

Baseline (average of three runs): frame interval 6.85 ms, GPU 5.62 ms, render thread 5.14 ms, 144.7 FPS. The three baseline frames were 6.83 / 6.96 / 6.75 ms, GPU 5.58 / 5.68 / 5.61 ms, largest drift 0.21 ms.

The GPU time of this baseline is already 82% of the frame interval, close to GPU-limited. Most rows with more than 0.3 ms of extra GPU time become GPU-limited, and the frame interval Δ is part of the GPU Δ.

| Option | Value | Frame interval Δms | GPU Δms | Render thread Δms | Limit side | Notes |
|---|---|---|---|---|---|---|
| Anti-aliasing | FXAA | -0.05 | +0.05 | -0.05 | CPU | 146 FPS |
| Anti-aliasing | SMAA | +0.03 | +0.27 | 0.00 | GPU | 144 FPS |
| Anti-aliasing | TAA | +1.99 | +2.31 | +1.90 | GPU | 111 FPS; `record_ms` 1.46 → 2.92 |
| Ambient occlusion | SSAO | +0.44 | +1.15 | +0.13 | GPU | 125 FPS; frame interval average 7.99 ms, with spikes in the window |
| Ambient occlusion | GTAO | +0.58 | +1.30 | +0.18 | GPU | 133 FPS |
| Anisotropic filtering | 4x | +0.08 | +0.11 | +0.07 | CPU | 140 FPS |
| Anisotropic filtering | 16x | +0.13 | +0.10 | +0.11 | CPU | 139 FPS |
| Shadow resolution | 2x | +0.08 | +0.18 | +0.09 | CPU | 142 FPS |
| Shadow resolution | 4x | +0.18 | +0.68 | +0.12 | GPU | 140 FPS |
| Frustum culling | 0% | +0.71 | +0.19 | +0.62 | CPU | 131 FPS; draw count about 2140 → 2423 |
| HDR | On | +0.17 | +0.66 | +0.03 | GPU | 137 FPS |

### Battle (internal 2160 baseline, formation 3, first command)

Baseline (average of two runs): frame interval 4.44 ms, GPU 4.26 ms, render thread 1.34 ms, 228 FPS. The two baseline frames were 4.49 / 4.39 ms, GPU 4.33 / 4.20 ms. This baseline is GPU-limited, and the frame interval Δ and GPU Δ are essentially the same.

| Option | Value | Frame interval Δms | GPU Δms | Render thread Δms | Limit side | Notes |
|---|---|---|---|---|---|---|
| Ambient occlusion | GTAO | +0.67 | +0.60 | +0.05 | GPU | 197 FPS |
| Shadow resolution | 4x | +0.19 | +0.19 | +0.01 | GPU | 220 FPS |
| Anisotropic filtering | 16x | +0.13 | +0.12 | +0.01 | GPU | 223 FPS |
| Anti-aliasing | SMAA | +0.06 | +0.07 | +0.01 | GPU | 225 FPS |

## DLSS 5 neural rendering (1 pass)

- DLL: the maintainer-supplied `nvngx_dlssnr.dll` (165,840,496 bytes), copied next to the exe in the test directory. It is used only in the two rows below. Both rows have this DLL in place. They differ only in `dlss_neural_rendering`.
- Settings: super resolution DLSS Quality (2560x1440 → 3840x2160), internal 720, everything else original. NR preset, style and intensity use the default values.
- Log confirms NR is running:
  - `DLSS NR: snippet initialized`
  - `DLSS NR: 1 feature(s) created for 3840x2160`
  - `DLSS NR: first evaluate recorded for 3840x2160, 1 pass(es)`
  - The whole run has no failed, off or skipped NR log lines.
- The control of DLSS Quality without NR was retested this time: frame 8.84 / GPU 7.82 / render thread 7.10 ms. Compared with DLSS Quality in the first batch, the difference is +0.10 / +0.05 / +0.08 ms.

| Row | FPS | Frame interval ms | GPU ms | Render thread ms | Limit side |
|---|---|---|---|---|---|
| DLSS Quality (control) | 111.8 | 8.84 | 7.82 | 7.10 | GPU |
| DLSS Quality + NR 1 pass | 44.0 | 22.74 | 22.40 | 10.35 | GPU |

| Increment | Frame interval Δms | GPU Δms | Render thread Δms |
|---|---|---|---|
| NR 1 pass vs. DLSS Quality | +13.91 | +14.58 | +3.25 |
| DLSS Quality + NR vs. first-batch original baseline (internal 720) | +15.97 | +20.07 | +5.29 |

With NR on, `record_ms` rose from 3.02 ms to 6.22 ms.

## Items not measured, and why

- MetalFX: only macOS has it.
- Vulkan backend: both batches measured only D3D12.
- Output frame rate of FSR FG: SDK count is 0, and the log has no output frame rate.
- psvita (AMD Radeon 8060S, Proton D3D12): before the test moved to this machine, only one startup trial was run. It did not reach a scene, so there is no usable data.
  - Reason: with uncapped settings, the keys that are counted per swap were all sent during startup.
  - The log is under `psvita\` in this machine's test directory.

Raw logs, settings for each run, and screenshots are on this machine at `C:\Users\freefrank\worktrees\LostOdysseyRecomp\gfx-cost\runs\<label>\`. The one-line record for each run is in `PERF_LOG.md` in the same directory. The test scripts are `gcw.py`, `batchw.py` (first batch), `batchw2.py` (second batch) and `gcanw.py`, in the same directory. They are not distributed with the repository.
