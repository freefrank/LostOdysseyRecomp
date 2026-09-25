# Offline F1 capture analysis

These commands inspect existing F1 render captures without launching the game. They do not import or execute any script from `out/`. Run from the repository root with Python 3.10+.

## Capture structure and render-state events

```sh
python -B tools/capture_analysis/inspect.py --input /path/to/render-capture.zip --output /path/to/new/summary.json
python -B tools/capture_analysis/inspect.py --input /path/to/extracted-capture --output /path/to/new/summary.json
```

The input is either an F1 ZIP containing exactly one `capture-info.txt` (at the archive root or under an enclosing directory), or an extracted directory containing `capture-info.txt` and `frame-*-f*/` directories. A parent directory containing `capture/` is also accepted. Only `capture-info.txt`, each `render-state.txt` and ZIP metadata / directory entries are inspected; no archive member is extracted. The JSON output contains the capture-info lines, per-frame header and footer (`end` / `drops`), draw and shader event counts, screenshot presence, and per-resolve draw, address, dimensions, format, provenance when present, expected/raw byte lengths and `raw_ok` marker. Draw count includes trace draw records, which can differ from the `submitted_draws` field in the footer. Resolve byte lengths come from file metadata; binary contents, hashes, CRCs, images and shaders are **not** validated. Input is read-only and `--output` must be a new path outside the input capture. The script creates output parent directories if needed. Standard library only.

## Preview, traces and jitter candidates

Export selected screenshots or resolve images as PNGs without modifying the capture:

```sh
python -B tools/capture_analysis/preview.py --input /path/to/render-capture.zip --output /path/to/new/preview --frames 2548,2549,2550 --resolves 0 --roi 2130,1095,2370,1260
```

`--output` must be a new directory outside the capture. `--frames` selects comma-separated renderer frame IDs, `--resolves` selects sequence IDs, `--roi` uses full-resolution half-open coordinates for optional metrics, and `--max-width` limits exported screenshots (default 1280). Use `--no-screenshot` for resolve-only exports. The preview tool requires Pillow; its inputs are read-only.

For register-level investigation, write a cumulative draw trace or compare selected frames:

```sh
python -B tools/capture_analysis/trace.py --input /path/to/render-capture.zip --output /path/to/new/trace.json --frames 2548,2549,2550
python -B tools/capture_analysis/compare_traces.py --input /path/to/render-capture.zip --output /path/to/new/comparison.json --frames 2548,2549,2550
```

For candidate triage and reviewed constant export, see [Jitter candidate triage and reviewed fixtures](#jitter-candidate-triage-and-reviewed-fixtures) below.

For bounded coverage across complete draw intervals in multiple captures, use the serial full-frame summarizer:

```sh
python -B tools/capture_analysis/coverage.py --input /path/to/capture-a.zip --input /path/to/capture-b.zip --mapping LostOdysseyRecomp/gpu/temporal_scene.h --output /path/to/new/coverage.json
```

It reads capture metadata and candidate HLSL, writes a new JSON file outside every input, and never edits the production map. The result ranks unmapped VS evidence; it does not establish a runtime mapping, pixel coverage, or player acceptance.

## Two-image difference and ROI

```sh
python -B tools/capture_analysis/image_diff.py --first /path/to/frame-a/screenshot.bmp --second /path/to/frame-b/screenshot.bmp --roi 100,60,400,300 --output /path/to/new/metrics.json --diff-image /path/to/new/difference.png
```

`--roi x0,y0,x1,y1` uses half-open source-image coordinates; omit it for the full image. Both images must have the same dimensions. PNG, BMP and PPM are supported by Pillow. The JSON contains mean absolute RGB channel difference, pixels with any channel difference over 16, and maximum channel difference. `--diff-image` optionally writes an unamplified absolute RGB PNG of the selected region. Outputs must be new files outside any detected F1 input capture root; the command does not alter either input. It requires **Pillow** already installed; no package installation is performed by these scripts.

For P6 PPM-to-PNG conversion without third-party dependencies, reuse [`../ppm2png.py`](../ppm2png.py). Camera/depth reprojection and motion-vector comparison on FSR captures are already implemented in [`../tests/fsr/compare_captured_motion.py`](../tests/fsr/compare_captured_motion.py); detailed postprocess snapshot comparison is in [`../tests/fsr/check-postprocess-capture.py`](../tests/fsr/check-postprocess-capture.py). These are separate, more specialized capture formats, not replacements for generic image differences.

The archived `out/battle-flicker-20260907-f2871/inspect_capture.py` and `compare_frames.py` were read-only references. Their machine/date-specific defaults, eager extraction, entire-archive validation, image montage layout, raw depth statistics and capture-specific ROI were not carried into the generic tools. HTTP capture, input control, RenderDoc, GPU replay, screenshots, microcode and binary payloads remain outside this offline migration: they require a running game/device or are capture data rather than reusable analysis code.

## Jitter candidate triage and reviewed fixtures

`trace.py` reads register deltas and shader IDs from an F1 ZIP or extracted capture. `iter_draw_states` reconstructs each draw's cumulative register state without retaining a full copy per draw. The following tools use that parser and read only capture metadata and shaders; they do not extract binary render surfaces or run the game.

```sh
python -B tools/capture_analysis/jitter_candidates.py --input /path/to/capture.zip --mapping LostOdysseyRecomp/gpu/temporal_scene.h --frame 2548 --output out/local-jitter-candidates.json
python -B tools/capture_analysis/export_jitter_fixture.py --input /path/to/capture.zip --frame 2548 --draw 722:a027ab99fa3e3b0d:7 --depth-vs f7fd88506d704a3d --depth-slot 4 --namespace reviewed_capture --output out/local-reviewed-capture.h
```

Candidate triage stops before the first resolve draw by default. `--before-draw N` sets an explicit exclusive boundary, including for later passes or traces without resolves. It reports unlisted VS hashes whose constant bank contains the exact selected scene VP, an HLSL text-window hint near `oPos`, and earlier depth-pass candidates with matching index geometry, world constants, fetch95, camera and pass registers. Stale constants can also match a VP; neither a text-window hint nor a paired draw authorizes a production mapping or proves which pixels a draw covered. Inspect the actual HLSL dependency and image stages before editing `PositionVPSlot`.

Fixture export requires each reviewed material draw as `ID:HASH:SLOT`, plus an explicit depth shader hash and slot. It rejects missing pair evidence, absent bound PS, mismatched VP, ambiguous depth companions and draws beyond the selected boundary. The compact header stores material c0–c15 and c254–c255, depth c0–c7 and PS c0–c15; supported material slots are 0–12 and depth slots 0–4. Both commands require a new output path outside an extracted capture. The exporter generates test data only; tests still need an independent transcription of the shader's position arithmetic and a negative control for the old defect.
