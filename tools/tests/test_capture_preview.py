"""Exercise ZIP preview selection and full-resolution ROI measurement."""
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile

from PIL import Image

TOOL = Path(__file__).resolve().parents[1] / "capture_analysis" / "preview.py"


class PreviewTest(unittest.TestCase):
    def test_windows_zip_selection_and_roi_before_resize(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive, output = root / "capture.zip", root / "preview"
            with zipfile.ZipFile(archive, "w") as capture:
                capture.writestr("capture-info.txt", "completed_frames=2\n")
                for frame, value in ((1, 0), (2, 100)):
                    image = Image.new("RGB", (8, 8), (value, value, value))
                    data = io.BytesIO()
                    image.save(data, "BMP")
                    capture.writestr(f"frame-0{frame}-f{frame}\\screenshot.bmp", data.getvalue())
                    capture.writestr(f"frame-0{frame}-f{frame}\\render-state.txt", f"Frame {frame}\n")
            run = subprocess.run([sys.executable, str(TOOL), "--input", str(archive),
                "--output", str(output), "--frames", "1,2", "--roi", "2,2,6,6",
                "--max-width", "2"], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stderr)
            report = json.loads((output / "preview.json").read_text())
            first, second = report["images"]
            self.assertEqual(first["preview_size"], [2, 2])
            self.assertEqual(first["source_size"], [8, 8])
            self.assertEqual(first["all_channels_below_32_fraction"], 1)
            self.assertEqual(second["mean_rgb"], [100, 100, 100])
            self.assertEqual(second["mean_abs_rgb_difference"], 100)
            self.assertEqual(second["comparison_frame"], 1)
            self.assertEqual(second["all_channels_below_32_fraction"], 0)

    def test_existing_output_is_not_modified(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            marker = root / "keep.txt"
            marker.write_text("preserve")
            run = subprocess.run([sys.executable, str(TOOL), "--input", str(root / "missing.zip"),
                "--output", str(root)], capture_output=True, text=True)
            self.assertNotEqual(run.returncode, 0)
            self.assertEqual(marker.read_text(), "preserve")
            self.assertFalse((root / "preview.json").exists())


if __name__ == "__main__":
    unittest.main()
