#!/usr/bin/env python3
"""Compare two extracted BMP/PPM/PNG images over the same optional ROI."""

import argparse
import json
from pathlib import Path


def compare(first, second, roi=None):
    from PIL import Image
    import numpy as np

    with Image.open(first) as source_a, Image.open(second) as source_b:
        if source_a.size != source_b.size:
            raise ValueError(f"image sizes differ: {source_a.size} vs {source_b.size}")
        width, height = source_a.size
        box = roi or (0, 0, width, height)
        x0, y0, x1, y1 = box
        if not (0 <= x0 < x1 <= width and 0 <= y0 < y1 <= height):
            raise ValueError("ROI must be within both images and nonempty")
        a = np.asarray(source_a.convert("RGB").crop(box), dtype=np.int16)
        b = np.asarray(source_b.convert("RGB").crop(box), dtype=np.int16)
    delta = np.abs(a - b)
    return ({"size": [width, height], "roi": list(box), "pixels": (x1 - x0) * (y1 - y0),
             "mean_abs_rgb": float(delta.mean()),
             "pixels_gt16": int(np.count_nonzero(np.max(delta, axis=2) > 16)),
             "max_abs_channel": int(delta.max())}, delta)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--first", required=True, type=Path, help="first extracted image")
    parser.add_argument("--second", required=True, type=Path, help="second extracted image")
    parser.add_argument("--roi", type=lambda text: tuple(int(v) for v in text.split(",")),
                        help="optional x0,y0,x1,y1 in source pixel coordinates")
    parser.add_argument("--output", required=True, type=Path, help="new JSON metrics file")
    parser.add_argument("--diff-image", type=Path, help="optional absolute RGB difference PNG")
    args = parser.parse_args(argv)
    if args.roi is not None and len(args.roi) != 4:
        parser.error("--roi requires x0,y0,x1,y1")
    inputs = {args.first.resolve(), args.second.resolve()}
    output = args.output.resolve()
    diff_path = args.diff_image.resolve() if args.diff_image else None
    if output in inputs or (diff_path and (diff_path in inputs or diff_path == output)):
        parser.error("output files must differ from both input images and each other")
    for path in inputs:
        for parent in path.parents:
            if (parent / "capture-info.txt").is_file():
                if output.is_relative_to(parent) or (diff_path and diff_path.is_relative_to(parent)):
                    parser.error("outputs must be outside the input capture")
                break
    if output.exists() or (diff_path and diff_path.exists()):
        parser.error("output files must be new")
    try:
        metrics, delta = compare(args.first, args.second, args.roi)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(metrics, indent=2) + "\n", encoding="utf-8")
        if diff_path:
            from PIL import Image
            import numpy as np
            diff_path.parent.mkdir(parents=True, exist_ok=True)
            Image.fromarray(np.minimum(delta, 255).astype("uint8"), "RGB").save(diff_path)
    except (OSError, ValueError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
