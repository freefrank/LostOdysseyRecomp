"""Check the dual-background replay equation and input qualification boundary."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "capture_analysis"))
import fg_ui_recompose as tool


def rgba(rgb):
    rgb = np.asarray(rgb, dtype=np.float32).reshape(1, 1, 3)
    return np.concatenate((rgb, np.ones((1, 1, 1), dtype=np.float32)), axis=2)


def replay(scene_rgb, overlay_rgb, opacity):
    scene_rgb = np.asarray(scene_rgb, dtype=np.float32)
    overlay_rgb = np.asarray(overlay_rgb, dtype=np.float32)
    contribution = overlay_rgb * opacity
    transmission = 1 - opacity
    return {"scene": rgba(scene_rgb), "black": rgba(contribution),
            "white": rgba(contribution + transmission),
            "final": rgba(contribution + transmission * scene_rgb)}


class RecomposeMathTest(unittest.TestCase):
    def test_direct_cli_entrypoint_uses_stdlib_inspect(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, output = root / "source", root / "result"
            source.mkdir()
            names = {}
            for name, image in replay([0.2, 0.4, 0.6], [0.8, 0.1, 0.3], 1).items():
                filename = f"{name}.rgba"
                (source / filename).write_bytes(np.rint(image * 255).astype(np.uint8).tobytes())
                names[name] = filename
            (source / "manifest.json").write_text(json.dumps({
                "width": 1, "height": 1, "format": "rgba8_unorm",
                "blend_domain": "attachment_numeric", "capture_valid": True,
                "rejection_reason": None, "overlay_draw_count": 1, "images": names,
            }), encoding="utf-8")
            script = Path(__file__).resolve().parents[1] / "capture_analysis" / "fg_ui_recompose.py"
            result = subprocess.run([sys.executable, "-B", str(script), "--input", str(source),
                                     "--output", str(output)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue(json.loads((output / "report.json").read_text())["capture_equation_fit"])

    def test_opaque_overlay(self):
        report, reconstructed, alpha, _ = tool.analyze(
            replay([0.2, 0.4, 0.6], [0.8, 0.1, 0.3], 1), 0, 0, 1e-6)
        self.assertTrue(report["equation_fit"])
        np.testing.assert_allclose(reconstructed[0, 0], [0.8, 0.1, 0.3], atol=1e-6)
        self.assertAlmostEqual(float(alpha[0, 0]), 1)

    def test_translucent_overlay(self):
        images = replay([0.2, 0.4, 0.6], [0.8, 0.1, 0.3], 0.25)
        report, reconstructed, alpha, _ = tool.analyze(images, 0, 1e-6, 1e-6)
        self.assertTrue(report["equation_fit"])
        self.assertAlmostEqual(float(alpha[0, 0]), 0.25, places=6)
        np.testing.assert_allclose(reconstructed, images["final"][..., :3], atol=1e-6)

    def test_no_overlay(self):
        images = replay([0.2, 0.4, 0.6], [0.8, 0.1, 0.3], 0)
        report, reconstructed, alpha, _ = tool.analyze(images, 0, 0, 0)
        self.assertTrue(report["equation_fit"])
        np.testing.assert_array_equal(reconstructed, images["scene"][..., :3])
        self.assertEqual(float(alpha[0, 0]), 0)

    def test_chromatic_transmission_rejected(self):
        images = replay([0.2, 0.4, 0.6], [0.8, 0.1, 0.3], 0.25)
        images["white"][0, 0, 0] += 0.1
        report, _, _, _ = tool.analyze(images, 0, 0.01, 1)
        self.assertFalse(report["equation_fit"])
        self.assertEqual(report["transmission"]["channel_mismatch_pixels"], 1)

    def test_out_of_range_transmission_rejected(self):
        images = replay([0.2, 0.4, 0.6], [0.8, 0.1, 0.3], 0)
        images["white"][0, 0, :3] += 0.1
        report, _, _, _ = tool.analyze(images, 0.01, 0.01, 1)
        self.assertFalse(report["equation_fit"])
        self.assertEqual(report["transmission"]["out_of_bounds_pixels"], 1)

    def test_nonfinite_input_rejected(self):
        images = replay([0.2, 0.4, 0.6], [0.8, 0.1, 0.3], 0.25)
        images["black"][0, 0, 1] = np.nan
        with self.assertRaisesRegex(ValueError, "finite"):
            tool.analyze(images, 0.01, 0.01, 0.01)

    def test_invalid_capture_cannot_qualify_even_when_equation_fits(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, output = root / "source", root / "result"
            source.mkdir()
            images = replay([0.2, 0.4, 0.6], [0.8, 0.1, 0.3], 1)
            names = {}
            for name, image in images.items():
                filename = f"{name}.rgba"
                (source / filename).write_bytes(np.rint(image * 255).astype(np.uint8).tobytes())
                names[name] = filename
            (source / "manifest.json").write_text(json.dumps({
                "width": 1, "height": 1, "format": "rgba8_unorm",
                "blend_domain": "attachment_numeric", "capture_valid": False,
                "rejection_reason": "overlay replay lacked stable frame identity",
                "overlay_draw_count": 0, "images": names,
            }), encoding="utf-8")
            report = tool.run(source, output)
            self.assertTrue(report["equation_fit"])
            self.assertFalse(report["capture_equation_fit"])
            self.assertEqual(report["ui_separation"], "unavailable")
            self.assertFalse(report["provider_ready"])
            self.assertEqual(json.loads((output / "report.json").read_text())["rejection_reason"],
                             "overlay replay lacked stable frame identity")
            self.assertTrue((output / "recomposed.png").is_file())
            with self.assertRaisesRegex(ValueError, "new directory"):
                tool.run(source, output)


if __name__ == "__main__":
    unittest.main()
