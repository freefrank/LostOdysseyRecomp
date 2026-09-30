"""Save isolation checks for the city benchmark helper."""

import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


SCRIPT = Path(__file__).resolve().parents[1] / "drive_city.py"
SPEC = importlib.util.spec_from_file_location("drive_city", SCRIPT)
drive_city = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(drive_city)


class DriveCitySaveTest(unittest.TestCase):
    def test_dry_run_does_not_change_any_files(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source"
            target = root / "run" / "save"
            source.mkdir()
            target.mkdir(parents=True)
            (source / "user01").write_bytes(b"baseline")
            (target / "user01").write_bytes(b"original")
            arguments = [str(SCRIPT), "--dry-run", "--save-src", str(source),
                         "--run-dir", str(target.parent), "--log-dir",
                         str(root / "logs")]
            with patch.object(sys, "argv", arguments):
                self.assertEqual(drive_city.main(), 0)
            self.assertEqual((target / "user01").read_bytes(), b"original")
            self.assertEqual((source / "user01").read_bytes(), b"baseline")
            self.assertFalse((root / "logs").exists())
            self.assertEqual(set(root.iterdir()), {source, target.parent})

    def test_overlapping_paths_are_rejected_without_mutation(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            target = root / "run" / "save"
            target.mkdir(parents=True)
            (target / "user01").write_bytes(b"original")
            for source in (target, target.parent, target / "nested"):
                arguments = [str(SCRIPT), "--dry-run", "--save-src", str(source),
                             "--run-dir", str(target.parent)]
                with patch.object(sys, "argv", arguments):
                    self.assertEqual(drive_city.main(), 2)
                self.assertEqual((target / "user01").read_bytes(), b"original")

    def test_copy_failure_preserves_target(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source"
            target = root / "run" / "save"
            source.mkdir()
            target.mkdir(parents=True)
            (source / "user01").write_bytes(b"baseline")
            (target / "user01").write_bytes(b"original")
            with patch.object(drive_city.shutil, "copytree", side_effect=OSError("copy failed")):
                with self.assertRaises(OSError):
                    drive_city.prepare_saves(source, target, "test")
            self.assertEqual((target / "user01").read_bytes(), b"original")

    def test_staging_keeps_previous_run_save(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source"
            target = root / "run" / "save"
            source.mkdir()
            target.mkdir(parents=True)
            (source / "user01").write_bytes(b"baseline")
            (target / "user01").write_bytes(b"original")
            backup = drive_city.prepare_saves(source, target, "test")
            self.assertEqual((target / "user01").read_bytes(), b"baseline")
            self.assertEqual((backup / "user01").read_bytes(), b"original")
            self.assertEqual((source / "user01").read_bytes(), b"baseline")

    def test_launch_failure_restores_previous_run_save(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source"
            target = root / "run" / "save"
            source.mkdir()
            target.mkdir(parents=True)
            (source / "user01").write_bytes(b"baseline")
            (target / "user01").write_bytes(b"original")
            control = root / "control"
            control.mkdir()
            arguments = [str(SCRIPT), "--save-src", str(source),
                         "--run-dir", str(target.parent), "--log-dir",
                         str(root / "logs")]
            with patch.object(sys, "argv", arguments), \
                 patch.object(drive_city.tempfile, "mkdtemp", return_value=str(control)), \
                 patch.object(drive_city.subprocess, "run"), \
                 patch.object(drive_city.subprocess, "Popen", side_effect=OSError("launch failed")):
                with self.assertRaises(OSError):
                    drive_city.main()
            self.assertEqual((target / "user01").read_bytes(), b"original")
            self.assertEqual((source / "user01").read_bytes(), b"baseline")
            self.assertFalse(list(target.parent.glob("save.pre-benchmark-*")))
            self.assertFalse(control.exists())


if __name__ == "__main__":
    unittest.main()
