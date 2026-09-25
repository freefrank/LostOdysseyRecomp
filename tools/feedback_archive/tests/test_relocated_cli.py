"""Synthetic checks for explicit destinations and repository-owned CLI resolution."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
ARCHIVER = ROOT / "scripts/archive_feedback.py"
WRAPPER = ROOT / "skills/lo-feedback-triage/scripts/feedback.py"
SPEC = importlib.util.spec_from_file_location("archive_feedback_cli", ARCHIVER)
archive = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(archive)


class RelocatedCliTests(unittest.TestCase):
    def invoke(self, script, *args, cwd):
        return subprocess.run([sys.executable, "-B", str(script), *args],
                              cwd=cwd, capture_output=True, text=True)

    def test_required_arguments_fail_without_writing(self):
        with tempfile.TemporaryDirectory() as directory:
            for script, args in ((ARCHIVER, ()), (WRAPPER, ("sync",))):
                result = self.invoke(script, *args, cwd=directory)
                self.assertEqual(result.returncode, 2)
            self.assertEqual(list(Path(directory).iterdir()), [])

    def test_wrapper_uses_packaged_implementation_from_other_cwd(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "feedback/data/observations").mkdir(parents=True)
            (root / "feedback/data/shader_sources").mkdir(parents=True)
            (root / "scripts").mkdir()
            # Data archives must not supply executable code to this wrapper.
            (root / "scripts/feedback_ledger.py").write_text(
                "raise RuntimeError('archive code must never run')\n", encoding="utf-8")
            result = self.invoke(WRAPPER, "sync", "--archive", directory, cwd=directory)
            self.assertEqual(result.returncode, 0, result.stderr)
            ledger = root / "feedback/triage/ledger.json"
            self.assertEqual(json.loads(ledger.read_bytes())["cases"], {})
            before = ledger.read_bytes(), ledger.stat().st_mtime_ns
            result = self.invoke(WRAPPER, "report", "--archive", directory, cwd=directory)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual((ledger.read_bytes(), ledger.stat().st_mtime_ns), before)

    def test_explicit_d1_scope_and_invalid_identifier(self):
        account = "A" * 32
        database = "11111111-2222-3333-4444-555555555555"
        client = archive.D1("synthetic", account, database)
        self.assertIn(account.lower(), client.url)
        self.assertIn(database, client.url)
        with self.assertRaises(archive.ArchiveError):
            archive.D1("synthetic", "../other", database)


if __name__ == "__main__":
    unittest.main()
