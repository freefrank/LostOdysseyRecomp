#!/usr/bin/env python3
"""Upscale textures exported with LostOdysseyRecomp.exe --export-assets.

Each texture gets a class from its name, package and pixels (color, normal,
data, ui, vfx or a skip class) and goes through that class's model chain:
seam-safe padding, the models, a low-frequency color fix against the source,
then alpha and the final size. Output mirrors the export layout with an
index.csv of the new sizes, and a run can be resumed.

Needs Python 3.10+, PyTorch, spandrel and Pillow, for example ComfyUI's Python
environment. Models are spandrel-compatible files (OpenModelDB).
"""
from __future__ import annotations

import argparse
import csv
import json
import re
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from PIL import Image

# Class -> processing. "chain" lists model files, applied in order; an empty
# chain means plain Lanczos resizing. "pad" is how edges are extended so the
# models see context there: "wrap" for tiling world textures, "reflect" else.
DEFAULT_CONFIG = {
    "scale": 4,
    "max_size": 4096,
    "tile": 512,
    "color_fix": True,
    "classes": {
        "color": {"chain": ["1x_DEDXT.pth", "4x-PBRify_RPLKSRd_V3.pth"], "pad": "wrap"},
        "normal": {"chain": ["4x-Normal-RG0-BC1.pth"], "pad": "wrap", "normal": True, "detail": 0.5},
        "data": {"chain": [], "pad": "wrap"},
        "ui": {"chain": ["1x-BC1-smooth2.pth", "4xNomos2_realplksr_dysample.safetensors"], "pad": "reflect"},
        "vfx": {"chain": ["1x-BC1-smooth2.pth", "4xNomos2_realplksr_dysample.safetensors"], "pad": "reflect"},
    },
}
SKIP_CLASSES = ("skip-lightmap", "skip-engine", "skip-tiny")
DATA_SUFFIXES = {"S", "M", "MASK", "H", "HS", "A", "REF", "SP", "SPC", "SPEC"}
NORMAL_SUFFIXES = {"N", "NS", "NH", "2N", "TN", "NTS", "NORMAL", "NRM"}
PAD = 16  # Source pixels of context around each image.


def object_name(key: str) -> str:
    return key.rpartition(":")[2]


def looks_like_normal_map(image: Image.Image, named_normal: bool) -> bool:
    """Tangent-space normals average to (0.5, 0.5, ~1) with unit-length vectors.

    Point samples keep per-pixel lengths (averaging shortens bumpy normals);
    near-black pixels are unused UV space and are left out.
    """
    small = image.convert("RGB").resize((64, 64), Image.Resampling.NEAREST)
    pixels = [p for p in small.get_flattened_data() if max(p) > 12]
    if len(pixels) < 64:
        return False
    n = len(pixels)
    mean = [sum(p[c] for p in pixels) / n / 255 for c in range(3)]
    lengths = sorted(((r / 127.5 - 1) ** 2 + (g / 127.5 - 1) ** 2 + (b / 127.5 - 1) ** 2) ** 0.5 for r, g, b in pixels)
    median = lengths[n // 2]
    if abs(mean[0] - 0.5) < 0.08 and abs(mean[1] - 0.5) < 0.08 and mean[2] > 0.75 and 0.85 < median < 1.15:
        return True
    # A normal-map name with a clearly blue-dominant image.
    return named_normal and mean[2] > mean[0] + 0.1 and mean[2] > mean[1] + 0.1 and 0.8 < median < 1.2


def classify(row: dict, image: Image.Image | None) -> str:
    key, name = row["key"], object_name(row["key"])
    package = key.partition("#")[0]
    width, height = int(row["width"]), int(row["height"])
    if name.startswith(("LightMapTexture2D", "ShadowMapTexture2D")):
        return "skip-lightmap"
    if package.startswith("bin/xenon/sys/"):
        return "skip-engine"
    if min(width, height) < 16:
        return "skip-tiny"
    tokens = name.upper().split("_")
    suffix = next((t for t in reversed(tokens) if not t.isdigit()), "")
    if "/loc/" in package or "UI" in tokens:
        return "ui"
    if image is not None and looks_like_normal_map(image, suffix in NORMAL_SUFFIXES):
        return "normal"
    if package.startswith("bin/xenon/vfx/"):
        return "vfx"
    if suffix in DATA_SUFFIXES or row.get("format") == "G8":
        return "data"
    return "color"


class Upscaler:
    def __init__(self, models_dir: Path, device: str, tile: int):
        import torch
        import spandrel
        self.torch, self.spandrel = torch, spandrel
        self.models_dir, self.device, self.tile = models_dir, device, tile
        self.cache: dict[str, object] = {}

    def model(self, name: str):
        if name not in self.cache:
            descriptor = self.spandrel.ModelLoader().load_from_file(str(self.models_dir / name))
            # fp16 where supported (ESRGAN); otherwise fp32. RealPLKSR claims
            # bf16 support but produces block artifacts in it.
            dtype = self.torch.float32
            if self.device.startswith("cuda") and descriptor.supports_half:
                dtype = self.torch.float16
            descriptor.to(self.device).eval()
            descriptor.model.to(dtype)
            self.cache[name] = (descriptor, dtype)
        return self.cache[name]

    def run(self, name: str, x):
        """x: 1x3xHxW float32 on device, 0..1. Tiles large inputs with overlap."""
        torch = self.torch
        descriptor, dtype = self.model(name)
        scale = descriptor.scale
        _, c, h, w = x.shape
        out = torch.empty((1, c, h * scale, w * scale), device=x.device, dtype=torch.float32)
        tile, margin = self.tile, 16
        with torch.inference_mode():
            for y0 in range(0, h, tile):
                for x0 in range(0, w, tile):
                    y1, x1 = min(y0 + tile, h), min(x0 + tile, w)
                    ya, xa = max(y0 - margin, 0), max(x0 - margin, 0)
                    yb, xb = min(y1 + margin, h), min(x1 + margin, w)
                    part = descriptor(x[:, :, ya:yb, xa:xb].to(dtype)).float()
                    oy, ox = (y0 - ya) * scale, (x0 - xa) * scale
                    out[:, :, y0 * scale:y1 * scale, x0 * scale:x1 * scale] = \
                        part[:, :, oy:oy + (y1 - y0) * scale, ox:ox + (x1 - x0) * scale].clamp(0, 1)
        return out, scale


def process(image: Image.Image, spec: dict, upscaler: Upscaler | None, config: dict) -> Image.Image:
    import numpy as np
    import torch
    import torch.nn.functional as F

    has_alpha = "A" in image.getbands()
    alpha = image.getchannel("A") if has_alpha else None
    grey = image.mode == "L"
    rgb = np.asarray(image.convert("RGB"), dtype=np.float32) / 255
    h, w = rgb.shape[:2]
    target = int(config["scale"])
    limit = int(config["max_size"])
    while max(w, h) * target > limit and target > 1:
        target //= 2
    out_w, out_h = w * target, h * target
    if target == 1:
        return image.copy()

    normal = spec.get("normal", False)
    src = rgb.copy()
    if normal:
        src[..., 2] = 0  # RG0 input, as the normal models expect.
    pad = min(PAD, h, w)
    mode = "wrap" if spec.get("pad") == "wrap" else "reflect"
    padded = np.pad(src, ((pad, pad), (pad, pad), (0, 0)), mode=mode)
    device = upscaler.device if upscaler else "cpu"
    x = torch.from_numpy(padded).permute(2, 0, 1)[None].to(device)
    total = 1
    for name in spec.get("chain", []):
        x, scale = upscaler.run(name, x)
        total *= scale
    if total != 1:
        x = x[:, :, pad * total:x.shape[2] - pad * total, pad * total:x.shape[3] - pad * total]
    else:
        x = x[:, :, pad:x.shape[2] - pad, pad:x.shape[3] - pad]
    # Resample to the target size (also does all the work for an empty chain).
    if x.shape[2:] != (out_h, out_w):
        x = F.interpolate(x, size=(out_h, out_w), mode="bicubic", antialias=True, align_corners=False).clamp(0, 1)
    source = torch.from_numpy(src).permute(2, 0, 1)[None].to(x.device)
    up = lambda t: F.interpolate(t, size=(out_h, out_w), mode="bicubic", align_corners=False)
    if spec.get("chain") and (config.get("color_fix", True) or normal):
        # Make the result average down to the source: removes color drift and
        # keeps the models from moving large-scale tones or slopes. "detail"
        # then scales what the models added on top of plain resampling.
        channels = slice(0, 2) if normal else slice(0, 3)
        base = up(source[:, channels])
        fixed = x[:, channels] + up(source[:, channels] - F.interpolate(x[:, channels], size=(h, w), mode="area"))
        detail = float(spec.get("detail", 0.5 if normal else 1.0))
        x = x.clone()
        x[:, channels] = (base + detail * (fixed - base)).clamp(0, 1)
    result = x[0].permute(1, 2, 0).float().cpu().numpy()
    if normal:
        # The normal models output RG only and no unit vectors: rebuild Z.
        xy = result[..., :2] * 2 - 1
        z = np.sqrt(np.clip(1 - (xy ** 2).sum(-1), 0, 1))
        vec = np.concatenate([xy, z[..., None]], -1)
        vec /= np.maximum(np.linalg.norm(vec, axis=-1, keepdims=True), 1e-6)
        result = vec * 0.5 + 0.5
        # Near-black areas are unused UV space, not vectors: keep them black.
        unused = np.repeat(np.repeat(rgb.max(-1) < 0.05, target, 0), target, 1)
        result[unused] = 0
    out = Image.fromarray((result * 255 + 0.5).clip(0, 255).astype("uint8"), "RGB")
    if grey:
        out = out.convert("L")
    if alpha is not None:
        a = alpha.resize((out_w, out_h), Image.Resampling.LANCZOS)
        histogram = alpha.histogram()
        if not any(histogram[1:255]):
            a = a.point(lambda v: 255 if v >= 128 else 0)  # Keep cutouts hard.
        out.putalpha(a)
    return out


def read_rows(export: Path) -> tuple[Path, list[dict]]:
    textures = export / "textures" if (export / "textures" / "index.csv").exists() else export
    index = textures / "index.csv"
    if not index.exists():
        raise SystemExit(f"{index} not found: pass the --export-assets output folder")
    with index.open(encoding="utf-8-sig", newline="") as file:
        return textures, list(csv.DictReader(file))


def write_csv(path: Path, header: list[str], rows: list[dict]) -> None:
    tmp = path.with_suffix(".tmp")
    with tmp.open("w", encoding="utf-8", newline="") as file:
        writer = csv.DictWriter(file, header, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)
    tmp.replace(path)


def review_sheet(pairs: list[tuple[Path, Path]], output: Path, crop: int = 192) -> None:
    """Side-by-side crops: source scaled with bicubic (left) and the result."""
    tiles = []
    for source_path, result_path in pairs:
        result = Image.open(result_path).convert("RGB")
        source = Image.open(source_path).convert("RGB").resize(result.size, Image.Resampling.BICUBIC)
        cx, cy = result.width // 2, result.height // 2
        box = (max(cx - crop // 2, 0), max(cy - crop // 2, 0), min(cx + crop // 2, result.width), min(cy + crop // 2, result.height))
        tiles.append((source.crop(box), result.crop(box)))
    if not tiles:
        return
    sheet = Image.new("RGB", (crop * 2 + 8, (crop + 8) * len(tiles)), (32, 32, 32))
    for i, (left, right) in enumerate(tiles):
        sheet.paste(left, (0, i * (crop + 8)))
        sheet.paste(right, (crop + 8, i * (crop + 8)))
    sheet.save(output)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--export", type=Path, required=True, help="--export-assets output folder")
    parser.add_argument("--output", type=Path, required=True, help="work folder for upscaled textures")
    parser.add_argument("--models", type=Path, help="folder with the model files")
    parser.add_argument("--config", type=Path, help="JSON overriding the default classes and chains")
    parser.add_argument("--write-config", type=Path, help="write the default configuration and exit")
    parser.add_argument("--classes", help="comma list of classes to process (default: all non-skip)")
    parser.add_argument("--filter", help="only keys containing this text (case-insensitive)")
    parser.add_argument("--limit", type=int, help="stop after this many textures")
    parser.add_argument("--dry-run", action="store_true", help="classify only: write plan.csv and print counts")
    parser.add_argument("--review", type=int, default=0, help="write review/<class>.png with this many samples")
    parser.add_argument("--device", default="cuda")
    args = parser.parse_args(argv)

    if args.write_config:
        args.write_config.write_text(json.dumps(DEFAULT_CONFIG, indent=2) + "\n", encoding="utf-8")
        print(f"Wrote {args.write_config}")
        return 0
    config = json.loads(json.dumps(DEFAULT_CONFIG))
    if args.config:
        user = json.loads(args.config.read_text(encoding="utf-8"))
        config.update({k: v for k, v in user.items() if k != "classes"})
        config["classes"].update(user.get("classes", {}))

    textures, rows = read_rows(args.export)
    if args.export.resolve() in args.output.resolve().parents or args.output.resolve() == args.export.resolve():
        raise SystemExit("--output must be outside the export folder")
    args.output.mkdir(parents=True, exist_ok=True)
    wanted = set(args.classes.split(",")) if args.classes else None
    text = args.filter.lower() if args.filter else None

    plan_path = args.output / "plan.csv"
    plan: dict[str, str] = {}
    if plan_path.exists():
        with plan_path.open(encoding="utf-8", newline="") as file:
            plan = {r["key"]: r["class"] for r in csv.DictReader(file)}
    started = time.time()
    for row in rows:
        if row["key"] not in plan:
            with Image.open(textures / row["file"]) as image:
                plan[row["key"]] = classify(row, image)
    for row in rows:
        row["class"] = plan[row["key"]]
    write_csv(plan_path, ["key", "file", "width", "height", "format", "class"], rows)
    counts: dict[str, int] = {}
    for row in rows:
        counts[row["class"]] = counts.get(row["class"], 0) + 1
    print("classes: " + ", ".join(f"{k} {v}" for k, v in sorted(counts.items())) + f" ({time.time() - started:.1f} s)")
    if args.dry_run:
        return 0

    selected = [r for r in rows if r["class"] not in SKIP_CLASSES and r["class"] in config["classes"]
                and (wanted is None or r["class"] in wanted) and (text is None or text in r["key"].lower())]
    if args.limit is not None:
        selected = selected[:args.limit]
    needs_models = any(config["classes"][r["class"]].get("chain") for r in selected)
    if needs_models and not args.models:
        raise SystemExit("--models is required for the configured chains")
    upscaler = Upscaler(args.models, args.device, int(config["tile"])) if needs_models else None
    if upscaler:
        for name in {m for r in selected for m in config["classes"][r["class"]].get("chain", [])}:
            if not (args.models / name).exists():
                raise SystemExit(f"model not found: {args.models / name}")

    out_root = args.output / "textures"
    done = skipped = failed = 0
    started = time.time()
    last = started
    review: dict[str, list[tuple[Path, Path]]] = {}
    with ThreadPoolExecutor(max_workers=2) as saver:
        pending = []
        for row in selected:
            target = out_root / row["file"]
            source = textures / row["file"]
            if target.exists():
                skipped += 1
            else:
                try:
                    with Image.open(source) as image:
                        image.load()
                        result = process(image, config["classes"][row["class"]], upscaler, config)
                    target.parent.mkdir(parents=True, exist_ok=True)
                    tmp = target.with_name(target.name + ".tmp.png")
                    pending.append(saver.submit(lambda r=result, t=tmp, f=target: (r.save(t, compress_level=1), t.replace(f))))
                    done += 1
                except Exception as exc:  # Report and continue with the next texture.
                    failed += 1
                    print(f"failed: {row['key']}: {exc}", file=sys.stderr)
                    continue
            if args.review and len(review.setdefault(row["class"], [])) < args.review:
                review[row["class"]].append((source, target))
            now = time.time()
            if now - last > 5:
                last = now
                n = done + skipped + failed
                print(f"progress {n}/{len(selected)} ({done / max(now - started, 1e-6):.2f} textures/s)", flush=True)
        for future in pending:
            future.result()

    index = []
    for row in rows:
        target = out_root / row["file"]
        if row["class"] in SKIP_CLASSES or not target.exists():
            continue
        with Image.open(target) as image:
            index.append({**row, "source_width": row["width"], "source_height": row["height"],
                          "width": image.width, "height": image.height})
    write_csv(args.output / "index.csv",
              ["key", "file", "width", "height", "format", "class", "source_width", "source_height"], index)
    if args.review:
        (args.output / "review").mkdir(exist_ok=True)
        for name, pairs in review.items():
            review_sheet(pairs, args.output / "review" / f"{name}.png")
    elapsed = time.time() - started
    print(f"upscaled {done}, already done {skipped}, failed {failed} in {elapsed:.1f} s; index: {args.output / 'index.csv'}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
