# Offline F1 capture analysis

These commands inspect existing F1 render captures without launching the game. They do not import or execute any script from `out/`. Run from the repository root with Python 3.10+.

## Capture structure and render-state events

```sh
python -B tools/capture_analysis/inspect.py --input /path/to/render-capture.zip --output /path/to/new/summary.json
python -B tools/capture_analysis/inspect.py --input /path/to/extracted-capture --output /path/to/new/summary.json
```

The input is either an F1 ZIP containing exactly one `capture-info.txt` (at the archive root or under an enclosing directory), or an extracted directory containing `capture-info.txt` and `frame-*-f*/` directories. A parent directory containing `capture/` is also accepted. Only `capture-info.txt`, each `render-state.txt` and ZIP metadata / directory entries are inspected; no archive member is extracted. The JSON output contains the capture-info lines, per-frame header and footer (`end` / `drops`), draw and shader event counts, screenshot presence, and per-resolve draw, address, dimensions, format, provenance when present, expected/raw byte lengths and `raw_ok` marker. Draw count includes trace draw records, which can differ from the `submitted_draws` field in the footer. Resolve byte lengths come from file metadata; binary contents, hashes, CRCs, images and shaders are **not** validated. Input is read-only and `--output` must be a new path outside the input capture. The script creates output parent directories if needed. Standard library only.

## Two-image difference and ROI

```sh
python -B tools/capture_analysis/image_diff.py --first /path/to/frame-a/screenshot.bmp --second /path/to/frame-b/screenshot.bmp --roi 100,60,400,300 --output /path/to/new/metrics.json --diff-image /path/to/new/difference.png
```

`--roi x0,y0,x1,y1` uses half-open source-image coordinates; omit it for the full image. Both images must have the same dimensions. PNG, BMP and PPM are supported by Pillow. The JSON contains mean absolute RGB channel difference, pixels with any channel difference over 16, and maximum channel difference. `--diff-image` optionally writes an unamplified absolute RGB PNG of the selected region. Outputs must be new files outside any detected F1 input capture root; the command does not alter either input. It requires **Pillow and NumPy** already installed; no package installation is performed by these scripts.

For P6 PPM-to-PNG conversion without third-party dependencies, reuse [`../ppm2png.py`](../ppm2png.py). Camera/depth reprojection and motion-vector comparison on FSR captures are already implemented in [`../tests/fsr/compare_captured_motion.py`](../tests/fsr/compare_captured_motion.py); detailed postprocess snapshot comparison is in [`../tests/fsr/check-postprocess-capture.py`](../tests/fsr/check-postprocess-capture.py). These are separate, more specialized capture formats, not replacements for generic image differences.

The archived `out/battle-flicker-20260907-f2871/inspect_capture.py` and `compare_frames.py` were read-only references. Their machine/date-specific defaults, eager extraction, entire-archive validation, image montage layout, raw depth statistics and capture-specific ROI were not carried into the generic tools. HTTP capture, input control, RenderDoc, GPU replay, screenshots, microcode and binary payloads remain outside this offline migration: they require a running game/device or are capture data rather than reusable analysis code.
