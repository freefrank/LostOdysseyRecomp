#!/usr/bin/env python3
"""Small synthetic offline capture and image fixtures (no game or GPU)."""

import json
import io
from contextlib import redirect_stderr
from pathlib import Path
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.capture_analysis import image_diff, inspect


class CaptureAnalysisToolsTest(unittest.TestCase):
    def test_zip_and_directory_render_events(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            capture = root / "capture"
            frame = capture / "frame-01-f23"
            frame.mkdir(parents=True)
            (capture / "capture-info.txt").write_text("status=complete\n", encoding="utf-8")
            (frame / "render-state.txt").write_text(
                "Frame 23\nSource version: synthetic\ndraw 0 prim=4\n"
                "resolve f23_seq00.bin draw=0 address=0xab width=2 height=1 "
                "plume_format=20 bpp=4 raw_ok=true (packed rows, little endian)\n"
                "resolve_provenance f23_seq00.bin frame=23 write_ordinal=4 guest_format=6\n"
                "end frame=23 submitted_draws=1 screenshot=true\n",
                encoding="utf-8")
            (frame / "f23_seq00.bin").write_bytes(bytes(8))
            (frame / "screenshot.bmp").write_bytes(b"placeholder")
            expected = inspect.summarize(capture)
            self.assertEqual(expected["capture_info"], ["status=complete"])
            self.assertEqual(expected["frames"][0]["draw_count"], 1)
            self.assertEqual(expected["frames"][0]["resolves"][0]["raw_bytes"], 8)
            self.assertIn("guest_format=6", expected["frames"][0]["resolves"][0]["provenance"])
            self.assertEqual(expected["frames"][0]["footer"],
                             ["end frame=23 submitted_draws=1 screenshot=true"])
            archive = root / "capture.zip"
            with zipfile.ZipFile(archive, "w") as z:
                for path in capture.rglob("*"):
                    if path.is_file():
                        z.write(path, "render-123/" + path.relative_to(capture).as_posix())
            self.assertEqual(inspect.summarize(archive), expected)
            output = root / "new" / "summary.json"
            inspect.main(["--input", str(archive), "--output", str(output)])
            self.assertEqual(json.loads(output.read_text(encoding="utf-8")), expected)
            self.assertFalse((capture / "summary.json").exists())

    def test_zip_traversal_rejected_without_extraction(self):
        with tempfile.TemporaryDirectory() as temp:
            archive = Path(temp) / "unsafe.zip"
            with zipfile.ZipFile(archive, "w") as z:
                z.writestr("capture-info.txt", "status=complete\n")
                z.writestr("../escape.txt", "bad")
            with self.assertRaisesRegex(ValueError, "unsafe capture path"):
                inspect.summarize(archive)
            self.assertFalse((Path(temp).parent / "escape.txt").exists())

    def test_image_difference_full_and_roi(self):
        from PIL import Image

        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            first, second = root / "a.png", root / "b.bmp"
            Image.new("RGB", (2, 2), (0, 0, 0)).save(first)
            image = Image.new("RGB", (2, 2), (0, 0, 0))
            image.putpixel((1, 1), (20, 0, 0))
            image.save(second)
            full, _ = image_diff.compare(first, second)
            self.assertEqual((full["pixels"], full["pixels_gt16"], full["max_abs_channel"]),
                             (4, 1, 20))
            metrics, _ = image_diff.compare(first, second, (1, 1, 2, 2))
            self.assertAlmostEqual(metrics["mean_abs_rgb"], 20 / 3)
            json_file, png_file = root / "new" / "diff.json", root / "new" / "diff.png"
            image_diff.main(["--first", str(first), "--second", str(second), "--roi", "1,1,2,2",
                             "--output", str(json_file), "--diff-image", str(png_file)])
            self.assertEqual(json.loads(json_file.read_text())["pixels_gt16"], 1)
            with Image.open(png_file) as diff:
                self.assertEqual(diff.getpixel((0, 0)), (20, 0, 0))
            with self.assertRaisesRegex(ValueError, "ROI"):
                image_diff.compare(first, second, (0, 0, 3, 1))

    def test_image_outputs_outside_capture(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            capture = root / "capture"
            capture.mkdir()
            (capture / "capture-info.txt").write_text("status=complete\n")
            with redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                image_diff.main(["--first", str(capture / "a.bmp"),
                                 "--second", str(capture / "b.bmp"),
                                 "--output", str(capture / "diff.json")])
            self.assertFalse((capture / "diff.json").exists())


if __name__ == "__main__":
    unittest.main()
