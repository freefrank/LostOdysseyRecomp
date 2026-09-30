#!/usr/bin/env python3
"""Diagnose a dual-background FG overlay replay in attachment numeric RGB space.

Input is an explicit directory containing manifest.json and four tightly packed,
equal-sized RGBA raw images. black and white must replay the *same* overlay over
numeric RGB zero and one respectively. RGB alone enters the equation; input alpha
channels are ignored. This is an offline diagnostic, not an FG qualification gate.
"""

import argparse
import json
import sys
from pathlib import Path

# Direct execution prepends this directory to sys.path. Its inspect.py is a
# capture utility, not stdlib inspect; NumPy imports the latter during startup.
_tool_dir = Path(__file__).resolve().parent
sys.path[:] = [entry for entry in sys.path if Path(entry or ".").resolve() != _tool_dir]

import numpy as np
from PIL import Image


FORMATS = {"rgba8_unorm": (np.dtype("u1"), 1.0 / 255.0),
           "rgba16f_le": (np.dtype("<f2"), 1.0)}
IMAGE_NAMES = ("scene", "black", "white", "final")


def load_manifest(input_dir):
    manifest = json.loads((input_dir / "manifest.json").read_text(encoding="utf-8"))
    if not isinstance(manifest, dict):
        raise ValueError("manifest must be an object")
    width, height = manifest.get("width"), manifest.get("height")
    if type(width) is not int or type(height) is not int or width <= 0 or height <= 0:
        raise ValueError("width and height must be positive integers")
    if manifest.get("format") not in FORMATS:
        raise ValueError("format must be rgba8_unorm or rgba16f_le")
    if manifest.get("blend_domain") != "attachment_numeric":
        raise ValueError("blend_domain must be attachment_numeric")
    valid, reason = manifest.get("capture_valid"), manifest.get("rejection_reason")
    if type(valid) is not bool or (valid and reason is not None) or (
            not valid and (not isinstance(reason, str) or not reason.strip())):
        raise ValueError("capture_valid requires rejection_reason null when true, nonempty when false")
    draws = manifest.get("overlay_draw_count")
    if type(draws) is not int or draws < 0 or (valid and draws == 0):
        raise ValueError("overlay_draw_count must be nonnegative and positive for valid capture")
    images = manifest.get("images")
    if not isinstance(images, dict) or set(images) != set(IMAGE_NAMES):
        raise ValueError("images must contain exactly scene, black, white, final")
    for name in IMAGE_NAMES:
        value = images[name]
        if not isinstance(value, str) or not value:
            raise ValueError(f"images.{name} must be a relative file name")
        path = Path(value)
        if path.is_absolute() or not (input_dir / path).resolve().is_relative_to(input_dir):
            raise ValueError(f"images.{name} escapes input directory")
    return manifest


def read_images(input_dir, manifest):
    width, height = manifest["width"], manifest["height"]
    dtype, scale = FORMATS[manifest["format"]]
    expected = width * height * 4 * dtype.itemsize
    images = {}
    for name in IMAGE_NAMES:
        path = (input_dir / manifest["images"][name]).resolve()
        if not path.is_file() or path.stat().st_size != expected:
            raise ValueError(f"{name} must be a {expected}-byte tightly packed RGBA raw file")
        images[name] = np.fromfile(path, dtype=dtype).reshape(height, width, 4).astype(np.float32) * scale
    return images


def analyze(images, bounds_tolerance, channel_tolerance, error_tolerance):
    if any(not np.isfinite(images[name]).all() for name in IMAGE_NAMES):
        raise ValueError("all input channels must be finite")
    scene, black, white, final = (images[name][..., :3] for name in IMAGE_NAMES)
    per_channel_t = white - black
    transmission = per_channel_t.mean(axis=2)
    opacity = 1.0 - transmission
    spread = per_channel_t.max(axis=2) - per_channel_t.min(axis=2)
    reconstructed = black + transmission[..., None] * scene
    difference = np.abs(reconstructed - final)
    pixel_error = difference.max(axis=2)
    bounds_bad = (per_channel_t < -bounds_tolerance) | (per_channel_t > 1 + bounds_tolerance)
    report = {
        "pixels": int(transmission.size),
        "transmission": {
            "min": float(per_channel_t.min()), "max": float(per_channel_t.max()),
            "out_of_bounds_pixels": int(np.count_nonzero(bounds_bad.any(axis=2))),
            "channel_spread_max": float(spread.max()),
            "channel_mismatch_pixels": int(np.count_nonzero(spread > channel_tolerance)),
        },
        "derived_opacity": {"min": float(opacity.min()), "max": float(opacity.max())},
        "final_rgb_error": {
            "mean_abs_channel": float(difference.mean()),
            "p95_abs_channel": float(np.percentile(difference, 95)),
            "p99_abs_channel": float(np.percentile(difference, 99)),
            "max_abs_channel": float(difference.max()),
            "pixels_over_tolerance": int(np.count_nonzero(pixel_error > error_tolerance)),
        },
        "tolerances": {"bounds": bounds_tolerance, "channel": channel_tolerance,
                       "final_rgb_error": error_tolerance},
    }
    report["equation_fit"] = not any((report["transmission"]["out_of_bounds_pixels"],
                                      report["transmission"]["channel_mismatch_pixels"],
                                      report["final_rgb_error"]["pixels_over_tolerance"]))
    return report, reconstructed, opacity, difference


def save_rgb(path, values):
    Image.fromarray(np.rint(np.clip(values, 0, 1) * 255).astype(np.uint8), "RGB").save(path)


def run(input_dir, output_dir, bounds_tolerance=None, channel_tolerance=None,
        error_tolerance=None):
    input_dir = input_dir.resolve()
    output_dir = output_dir.resolve()
    if not input_dir.is_dir():
        raise ValueError("--input must be a directory")
    if output_dir.exists() or output_dir.is_relative_to(input_dir):
        raise ValueError("--output must be a new directory outside --input")
    manifest = load_manifest(input_dir)
    images = read_images(input_dir, manifest)
    default = 3 / 255 if manifest["format"] == "rgba8_unorm" else 0.005
    tolerances = (default if value is None else value for value in
                  (bounds_tolerance, channel_tolerance, error_tolerance))
    bounds, channel, error = tolerances
    if any(not np.isfinite(value) or value < 0 for value in (bounds, channel, error)):
        raise ValueError("tolerances must be finite and nonnegative")
    report, reconstructed, opacity, difference = analyze(images, bounds, channel, error)
    report.update({"width": manifest["width"], "height": manifest["height"],
                   "format": manifest["format"], "blend_domain": manifest["blend_domain"],
                   "capture_valid": manifest["capture_valid"],
                   "rejection_reason": manifest["rejection_reason"],
                   "overlay_draw_count": manifest["overlay_draw_count"],
                   "capture_equation_fit": bool(manifest["capture_valid"] and report["equation_fit"]),
                   "ui_separation": "unavailable", "provider_ready": False,
                   "equation_scope": "this explicitly supplied replay only",
                   "capture_metadata": {key: manifest[key] for key in (
                       "source_allocation", "source_extent", "crop", "render_frame",
                       "temporal_epoch", "plan_serial", "plan_geometry_epoch",
                       "plan_device_epoch", "scene_copy_draw", "last_overlay_draw",
                       "resolve_ordinal", "destination_address", "submission_serial",
                       "resolve_destination_format", "resolve_destination_plume_format",
                       "resolve_destination_guest_format", "resolve_destination_extent",
                       "final_role") if key in manifest},
                   "input_alpha_used": False,
                   "previews": ["recomposed.png", "absolute_error_x8.png", "derived_opacity.png"]})
    output_dir.mkdir(parents=True)
    save_rgb(output_dir / "recomposed.png", reconstructed)
    save_rgb(output_dir / "absolute_error_x8.png", difference * 8)
    alpha_preview = np.repeat(opacity[..., None], 3, axis=2)
    save_rgb(output_dir / "derived_opacity.png", alpha_preview)
    (output_dir / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path, help="directory containing manifest.json and raw images")
    parser.add_argument("--output", required=True, type=Path, help="new output directory outside input")
    parser.add_argument("--bounds-tolerance", type=float, help="allowed RGB transmittance range slack")
    parser.add_argument("--channel-tolerance", type=float, help="allowed per-pixel RGB transmittance spread")
    parser.add_argument("--error-tolerance", type=float, help="allowed reconstructed RGB error per pixel")
    args = parser.parse_args(argv)
    try:
        run(args.input, args.output, args.bounds_tolerance, args.channel_tolerance,
            args.error_tolerance)
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as exc:
        parser.error(str(exc))


if __name__ == "__main__":
    main()
