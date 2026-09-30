#!/usr/bin/env python3
"""Compare real FSR input MV with independent camera/depth reprojection.

Run separately on three-frame F1 captures made during horizontal camera
translation and yaw. Supply a rectangle containing static scene geometry;
the invalidity mask alone does not classify moving objects as static.
"""

import argparse
import json
import math
from pathlib import Path
import statistics
import struct


def inverse(matrix):
    rows = [[float(matrix[r * 4 + c]) for c in range(4)] +
            [float(r == c) for c in range(4)] for r in range(4)]
    for col in range(4):
        pivot = max(range(col, 4), key=lambda row: abs(rows[row][col]))
        if abs(rows[pivot][col]) < 1e-14:
            raise ValueError("singular current VP")
        rows[col], rows[pivot] = rows[pivot], rows[col]
        divisor = rows[col][col]
        rows[col] = [value / divisor for value in rows[col]]
        for row in range(4):
            if row == col:
                continue
            scale = rows[row][col]
            rows[row] = [a - scale * b for a, b in zip(rows[row], rows[col])]
    return [rows[r][c + 4] for r in range(4) for c in range(4)]


def transform(vector, matrix):
    return [sum(vector[i] * matrix[i * 4 + j] for i in range(4)) for j in range(4)]


def camera_motion(current, previous):
    def forward(vp):
        vector = [vp[i] for i in (3, 7, 11)]
        norm = math.sqrt(sum(value * value for value in vector))
        return [value / norm for value in vector] if norm > 1e-12 else None

    def center(vp):
        eye = transform([0, 0, 1, 0], inverse(vp))
        if not math.isfinite(eye[3]) or abs(eye[3]) < 1e-12:
            return None
        point = [eye[i] / eye[3] for i in range(3)]
        check = transform([*point, 1], vp)
        if not all(math.isfinite(value) for value in point + check):
            return None
        scale = max(1, max(abs(value) for value in check))
        if max(abs(check[i]) for i in (0, 1, 3)) > 1e-6 * scale:
            return None
        return point

    current_forward, previous_forward = forward(current["vp"]), forward(previous["vp"])
    current_center, previous_center = center(current["vp"]), center(previous["vp"])
    angle = None
    if current_forward and previous_forward:
        angle = math.degrees(math.acos(max(-1, min(1,
            sum(a * b for a, b in zip(current_forward, previous_forward))))))
    displacement = None
    if current_center and previous_center:
        displacement = [a - b for a, b in zip(current_center, previous_center)]
    return {"current_forward_world": current_forward, "previous_forward_world": previous_forward,
            "forward_angle_delta_deg": angle,
            "current_center_guest": current_center, "previous_center_guest": previous_center,
            "center_delta_guest": displacement,
            "center_displacement_guest": math.sqrt(sum(v * v for v in displacement)) if displacement else None,
            "classification": "metrics_only; intended motion must be checked against scene control"}


def project(pixel_x, pixel_y, raw_depth, current, previous, current_inverse):
    viewport = current["viewport"]
    half_x, half_y = current["half_pixel_ndc"]
    nx = 2 * (pixel_x - viewport[0]) / viewport[2] - 1 - half_x
    ny = (1 - 2 * (pixel_y - viewport[1]) / viewport[3] - half_y) / current["ndc_y_sign"]
    world = transform([nx, ny, 1 - raw_depth, 1], current_inverse)
    if not math.isfinite(world[3]) or world[3] <= 1e-12:
        return None
    world = [value / world[3] for value in world]
    clip = transform(world, previous["vp"])
    if not math.isfinite(clip[3]) or clip[3] <= 1e-12:
        return None
    viewport = previous["viewport"]
    half_x, half_y = previous["half_pixel_ndc"]
    previous_x = viewport[0] + (clip[0] / clip[3] + half_x + 1) * viewport[2] / 2
    previous_y = viewport[1] + (1 - (previous["ndc_y_sign"] * clip[1] / clip[3] + half_y)) * viewport[3] / 2
    previous_depth = 1 - clip[2] / clip[3]
    if not all(math.isfinite(value) for value in (previous_x, previous_y, previous_depth)):
        return None
    if not (viewport[0] + 0.5 <= previous_x <= viewport[0] + viewport[2] - 0.5 and
            viewport[1] + 0.5 <= previous_y <= viewport[1] + viewport[3] - 0.5 and
            0 < previous_depth <= 1):
        return None
    return previous_x, previous_y, previous_depth


def percentile(values, fraction):
    ordered = sorted(values)
    if not ordered:
        return None
    offset = (len(ordered) - 1) * fraction
    low = int(offset)
    return ordered[low] + (ordered[min(low + 1, len(ordered) - 1)] - ordered[low]) * (offset - low)


def candidate_dispatches(root):
    for path in sorted(root.glob("frame-*-f*/fsr-evaluations.json")):
        page = json.loads(path.read_text(encoding="utf-8"))
        for entry in page["dispatches"]:
            if (entry["sdk_success"] and entry["isolated_accepted"] and
                    entry["isolated_included"] and entry["checked_submit"] and
                    entry["gpu_completed"] and all(entry[name]["available"] for name in
                    ("input", "depth", "motion", "invalidity", "output"))):
                yield path.parent, entry


def load_image(directory, entry, name, expected_format):
    metadata = entry[name]
    if metadata["format"] != expected_format:
        raise ValueError(f"{name}: expected {expected_format}, got {metadata['format']}")
    suffix = f"{entry['dispatch_index']:03d}"
    path = directory / f"fsr-{name}-{suffix}.bin"
    data = path.read_bytes()
    if len(data) != metadata["raw_bytes"]:
        raise ValueError(f"{path}: expected {metadata['raw_bytes']} bytes, got {len(data)}")
    return data


def compare_pair(previous_item, current_item, roi, step):
    current_dir, current_entry = current_item
    previous_dir, previous_entry = previous_item if previous_item else (None, None)
    if previous_entry:
        if current_entry["render_frame"] != previous_entry["render_frame"] + 1:
            raise ValueError("selected FSR dispatches are not adjacent renderer frames")
        if current_entry["temporal_epoch"] != previous_entry["temporal_epoch"]:
            raise ValueError("temporal epoch changed across pair")
        if current_entry["depth_allocation"] != previous_entry["depth_allocation"]:
            raise ValueError("depth allocation changed across pair")
        if current_entry["sdk"]["reset"]:
            raise ValueError("current SDK dispatch reset history; choose a continuous pair")
    current = current_entry["current_camera"]
    previous = current_entry["previous_camera"]
    if current is None or previous is None:
        raise ValueError("current dispatch lacks actual current/previous camera pair")
    if previous_entry and (previous_entry["current_camera"] is None or
            max(abs(a - b) for a, b in zip(previous["vp"], previous_entry["current_camera"]["vp"])) > 1e-6):
        raise ValueError("previous VP differs from the captured preceding FSR frame")
    width, height = current_entry["sdk"]["render"]
    if previous_entry and previous_entry["sdk"]["render"] != [width, height]:
        raise ValueError("render size changed across pair")
    x0, y0, x1, y1 = roi
    if not (0 <= x0 < x1 <= width and 0 <= y0 < y1 <= height):
        raise ValueError("ROI is outside the render extent")
    depth = load_image(current_dir, current_entry, "depth", "R32_FLOAT")
    motion = load_image(current_dir, current_entry, "motion", "RG16_FLOAT")
    invalidity = load_image(current_dir, current_entry, "invalidity", "R8_UNORM")
    previous_depth = load_image(previous_dir, previous_entry, "depth", "R32_FLOAT") if previous_entry else None
    if (previous_entry and previous_entry["depth"]["content_rect"] != [0, 0, width, height]) or any(
            current_entry[name]["content_rect"] != [0, 0, width, height] for name in
           ("depth", "motion", "invalidity")):
        raise ValueError("subrect inputs need an explicit stride/origin conversion")
    matrix_inverse = inverse(current["vp"])
    jitter_x, jitter_y = current_entry["sdk"]["jitter_input_pixels"]
    previous_jitter = previous_entry["sdk"]["jitter_input_pixels"] if previous_entry else None
    all_errors, consistent_errors, expected_lengths = [], [], []
    signed_x = signed_y = signed_x_match = signed_y_match = 0
    counts = {"sampled": 0, "invalid_mask": 0, "invalid_depth": 0,
              "reprojection_rejected": 0, "invalid_mv": 0,
              "previous_jittered_outside": 0, "depth_consistent": 0}
    worst = []
    for y in range(y0, y1, step):
        for x in range(x0, x1, step):
            counts["sampled"] += 1
            offset = y * width + x
            if invalidity[offset] != 0:
                counts["invalid_mask"] += 1
                continue
            raw = struct.unpack_from("<f", depth, offset * 4)[0]
            if not math.isfinite(raw) or not (0 < raw <= 1):
                counts["invalid_depth"] += 1
                continue
            sample_x, sample_y = x + 0.5 - jitter_x, y + 0.5 - jitter_y
            projected = project(sample_x, sample_y, raw, current, previous, matrix_inverse)
            if projected is None:
                counts["reprojection_rejected"] += 1
                continue
            actual_x, actual_y = struct.unpack_from("<ee", motion, offset * 4)
            if not math.isfinite(actual_x) or not math.isfinite(actual_y):
                counts["invalid_mv"] += 1
                continue
            predicted_x, predicted_y = projected[0] - sample_x, projected[1] - sample_y
            error = math.hypot(predicted_x - actual_x, predicted_y - actual_y)
            all_errors.append(error)
            expected_lengths.append(math.hypot(predicted_x, predicted_y))
            if abs(predicted_x) >= 0.25:
                signed_x += 1
                signed_x_match += (predicted_x * actual_x > 0)
            if abs(predicted_y) >= 0.25:
                signed_y += 1
                signed_y_match += (predicted_y * actual_y > 0)
            if previous_depth is not None:
                # The previous raw depth was rasterized with its own jitter,
                # while the motion shader's previous clip is unjittered.
                previous_raster_x = projected[0] + previous_jitter[0]
                previous_raster_y = projected[1] + previous_jitter[1]
                if not (0 <= previous_raster_x < width and
                        0 <= previous_raster_y < height):
                    counts["previous_jittered_outside"] += 1
                else:
                    observed_previous_depth = struct.unpack_from("<f", previous_depth,
                        (int(previous_raster_y) * width + int(previous_raster_x)) * 4)[0]
                    tolerance = max(2e-4, 0.05 * projected[2])
                    if (math.isfinite(observed_previous_depth) and
                            abs(observed_previous_depth - projected[2]) <= tolerance):
                        counts["depth_consistent"] += 1
                        consistent_errors.append(error)
            worst.append((error, x, y, [predicted_x, predicted_y], [actual_x, actual_y]))
    worst.sort(reverse=True)
    if not all_errors:
        raise ValueError("no valid static-ROI samples after depth and invalidity filtering")
    summarize = lambda values: {"count": len(values), "median_px": statistics.median(values) if values else None,
                                "p95_px": percentile(values, 0.95), "p99_px": percentile(values, 0.99)}
    return {"frames": [previous_entry["render_frame"] if previous_entry else current_entry["render_frame"] - 1,
                       current_entry["render_frame"]],
            "captured_previous_fsr_dispatch": bool(previous_entry),
            "sdk_history_reset": current_entry["sdk"]["reset"],
            "depth_consistency_status": "checked" if previous_entry else "unavailable_previous_raw_depth",
            "roi": roi, "step": step, "jitter_pixels": [jitter_x, jitter_y],
            "previous_jitter_pixels": previous_jitter,
            "sdk_motion_vector_scale": current_entry["sdk"]["motion_vector_scale"],
            "vp_max_abs_delta": max(abs(a - b) for a, b in zip(current["vp"], previous["vp"])),
            "camera_change": camera_motion(current, previous),
            "fov_y_delta_deg": math.degrees(current_entry["sdk"]["fov_y_radians"] -
                previous_entry["sdk"]["fov_y_radians"]) if previous_entry else None,
            "counts": counts, "expected_motion": summarize(expected_lengths),
            "all_valid_error": summarize(all_errors),
            "depth_consistent_error": summarize(consistent_errors),
            "component_sign_agreement": {"x": signed_x_match / signed_x if signed_x else None,
                                         "y": signed_y_match / signed_y if signed_y else None},
            "component_sign_sample_counts": {"x": signed_x, "y": signed_y},
            "largest_errors": [{"error_px": e, "pixel": [x, y], "expected": expected, "actual": actual}
                               for e, x, y, expected, actual in worst[:12]],
            "scope": "Camera-only static ROI; excludes invalidity>0, not moving objects with invalidity=0."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture_root", type=Path, help="extracted captures/render-... directory")
    parser.add_argument("--motion", required=True, choices=("translation", "yaw"),
                        help="operator-performed camera movement; this label is not inferred")
    parser.add_argument("--roi", required=True, type=lambda text: [int(v) for v in text.split(",")],
                        help="x0,y0,x1,y1 known-static scene rectangle in render pixels")
    parser.add_argument("--step", type=int, default=8)
    parser.add_argument("--single-frame", type=int,
                        help="diagnose one FSR frame using its captured previous camera, without prior raw depth")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if len(args.roi) != 4 or args.step < 1:
        parser.error("--roi needs four integers and --step must be positive")
    entries = list(candidate_dispatches(args.capture_root))
    if args.single_frame is not None:
        chosen = [entry for entry in entries if entry[1]["render_frame"] == args.single_frame]
        if len(chosen) != 1:
            parser.error("--single-frame must select exactly one completed FSR dispatch")
        pairs = [(None, chosen[0])]
    else:
        pairs = [(a, b) for a, b in zip(entries, entries[1:])
                 if b[1]["render_frame"] == a[1]["render_frame"] + 1]
        if not pairs:
            parser.error("no adjacent, completed FSR dispatch pair with all five raw images")
    result = {"result_status": "diagnostic_only", "acceptance": "not_evaluated",
              "intended_motion_trial": args.motion,
              "comparisons": [compare_pair(a, b, args.roi, args.step) for a, b in pairs]}
    output = json.dumps(result, indent=2, allow_nan=False) + "\n"
    if args.output:
        args.output.write_text(output, encoding="utf-8")
    print(output, end="")


if __name__ == "__main__":
    main()
