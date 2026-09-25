#!/usr/bin/env python3
"""Export selected F1 screenshots/resolves as PNG and optional full-resolution ROI metrics."""
import argparse
from io import BytesIO
import json
from pathlib import Path
import re

from trace import load_capture


def numbers(value: str) -> list[int]:
    try:
        result = [int(part) for part in value.split(",")]
        if not result or any(x < 0 for x in result) or len(set(result)) != len(result):
            raise ValueError()
        return result
    except ValueError:
        raise argparse.ArgumentTypeError("Use distinct nonnegative integers separated by commas")


def roi_box(value: str) -> tuple[int, int, int, int]:
    try:
        x0, y0, x1, y1 = map(int, value.split(","))
        if min(x0, y0) < 0 or x1 <= x0 or y1 <= y0:
            raise ValueError()
        return x0, y0, x1, y1
    except ValueError:
        raise argparse.ArgumentTypeError("ROI must be x0,y0,x1,y1 with positive area")


def preview(source: Path, output: Path, frames=None, resolves=None,
            screenshot=True, max_width=1280, roi=None) -> dict:
    from PIL import Image, ImageChops, ImageStat

    source, output = source.resolve(), output.resolve()
    if output.exists():
        raise ValueError(f"Output must be a new directory: {output}")
    if source.is_dir() and output.is_relative_to(source):
        raise ValueError("Output must be outside the input capture directory")
    if max_width <= 0 or (not screenshot and not resolves):
        raise ValueError("Choose at least one image source and a positive max width")
    result = {"schema": 1, "input": str(source), "roi": roi, "images": [],
              "limits": "Metrics describe full-resolution selected pixels, not root cause or performance."}
    with load_capture(source) as capture:
        selected = []
        available = set()
        for frame_dir in capture.frame_names():
            match = re.search(r"-f(\d+)$", frame_dir)
            if not match:
                continue
            frame = int(match[1])
            available.add(frame)
            if frames is not None and frame not in frames:
                continue
            sources = []
            if screenshot:
                candidates = [f"{frame_dir}/screenshot.{ext}" for ext in ("bmp", "png", "ppm")]
                found = next((name for name in candidates if name in capture.names), None)
                if not found:
                    raise ValueError(f"No screenshot in {frame_dir}")
                sources.append(("screenshot", found))
            for sequence in resolves or []:
                matches = [name for name in capture.names
                           if name.startswith(f"{frame_dir}/f{frame}_seq{sequence:02d}_")
                           and name.endswith(".ppm")]
                if len(matches) != 1:
                    raise ValueError(f"Expected one PPM for {frame_dir} seq{sequence:02d}; got {len(matches)}")
                sources.append((f"seq{sequence:02d}", matches[0]))
            selected.extend((frame, label, name) for label, name in sources)
        if frames is not None and set(frames) - available:
            raise ValueError(f"Missing frames: {sorted(set(frames) - available)}")
        if not selected:
            raise ValueError("No matching frame images")
        output.mkdir(parents=True)
        previous = {}
        for frame, label, name in sorted(selected):
            with Image.open(BytesIO(capture.read_bytes(name))) as opened:
                image = opened.convert("RGB")
            row = {"frame": frame, "kind": label, "member": name,
                   "source_size": list(image.size)}
            if roi:
                if roi[2] > image.width or roi[3] > image.height:
                    raise ValueError(f"ROI is outside {name}: {image.size}")
                pixels = image.crop(roi)
                row["mean_rgb"] = ImageStat.Stat(pixels).mean
                red, green, blue = pixels.split()
                brightest = ImageChops.lighter(ImageChops.lighter(red, green), blue)
                row["all_channels_below_32_fraction"] = (
                    sum(brightest.histogram()[:32]) / (pixels.width * pixels.height))
                if label in previous:
                    prior_frame, prior_size, prior_pixels = previous[label]
                    if prior_size == image.size:
                        row["comparison_frame"] = prior_frame
                        row["mean_abs_rgb_difference"] = sum(
                            ImageStat.Stat(ImageChops.difference(pixels, prior_pixels)).mean) / 3
                    else:
                        row["comparison_omitted"] = "source_dimensions_changed"
                previous[label] = (frame, image.size, pixels)
            image.thumbnail((max_width, max(1, int(image.height * max_width / image.width))))
            filename = f"f{frame}-{label}.png"
            image.save(output / filename)
            row.update(file=filename, preview_size=list(image.size))
            result["images"].append(row)
    (output / "preview.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True, help="New directory outside the capture")
    parser.add_argument("--frames", type=numbers, help="Comma-separated renderer frame IDs; default all")
    parser.add_argument("--resolves", type=numbers, help="Comma-separated sequence IDs, e.g. 0,2,15")
    parser.add_argument("--no-screenshot", action="store_true")
    parser.add_argument("--max-width", type=int, default=1280)
    parser.add_argument("--roi", type=roi_box, help="Full-resolution half-open x0,y0,x1,y1")
    args = parser.parse_args()
    try:
        result = preview(args.input, args.output, args.frames, args.resolves,
                         not args.no_screenshot, args.max_width, args.roi)
    except (ValueError, OSError, KeyError, ImportError) as error:
        parser.exit(1, f"{error}\n")
    print(f"Wrote {len(result['images'])} images and preview.json to {args.output}")


if __name__ == "__main__":
    main()
