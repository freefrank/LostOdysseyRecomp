"""Synthetic exact-program export and preflight-failure checks."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "scripts/export_programs.py"
SPEC = importlib.util.spec_from_file_location("export_programs", SCRIPT)
exporter = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(exporter)


class ExportProgramsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.archive = self.root / "archive"
        self.archive.mkdir()
        self.selection = self.root / "selection.json"
        self.output = self.root / "export"

    def source(self, stage="vs", data=b"1234", advertised=None):
        sha = exporter.ledger.digest(data)
        fnv = advertised or exporter.archive_feedback.renderer_hash(data)
        path = self.archive / f"feedback/data/shader_sources/{stage}/{sha}.json"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps({"stage": stage, "renderer_hash": fnv, "sha256": sha,
                                   "byte_length": len(data), "first_seen": 1, "last_seen": 2}))
        payload = self.archive / f"feedback/data/programs/{stage}/{sha}.bin"
        payload.parent.mkdir(parents=True, exist_ok=True)
        payload.write_bytes(data)
        return fnv, path, payload

    def select(self, value):
        self.selection.write_text(json.dumps(value))

    def test_verified_export_from_unrelated_cwd(self):
        vs, _, payload = self.source()
        ps, _, _ = self.source("ps", b"5678")
        self.select({"vs": [vs], "ps": [ps]})
        result = subprocess.run([sys.executable, "-B", str(SCRIPT), "--archive", str(self.archive),
            "--selection", str(self.selection), "--output", str(self.output)],
            cwd=self.archive, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.output / f"vs_{vs}.bin").read_bytes(), payload.read_bytes())
        provenance = json.loads((self.output / "provenance.json").read_text())
        self.assertEqual(len(provenance["programs"]), 2)
        self.assertEqual(provenance["programs"][0]["verified"], ["length", "sha256", "renderer-byte-fnv1a64"])

    def test_bad_selection_missing_and_output_protection(self):
        vs, _, _ = self.source()
        for value in ({"unknown": [vs]}, {"vs": "not-list"}, {"vs": ["../bad"]},
                      {"vs": [vs, vs]}, {"vs": []}, {"vs": ["0" * 16]}):
            self.select(value)
            with self.assertRaises(exporter.ledger.LedgerError):
                exporter.export(self.archive, self.selection, self.output)
            self.assertFalse(self.output.exists())
        self.select({"vs": [vs]})
        with self.assertRaises(exporter.ledger.LedgerError):
            exporter.export(self.archive, self.selection, self.archive / "export")
        self.output.mkdir()
        with self.assertRaises(exporter.ledger.LedgerError):
            exporter.export(self.archive, self.selection, self.output)
        self.assertEqual(list(self.output.iterdir()), [])

    def test_ambiguous_hash_and_corrupt_bytes_leave_no_partial_output(self):
        vs, _, payload = self.source()
        ps, _, bad = self.source("ps", b"5678")
        self.select({"vs": [vs], "ps": [ps]})
        bad.write_bytes(b"abcd")
        with self.assertRaisesRegex(exporter.ledger.LedgerError, "SHA-256"):
            exporter.export(self.archive, self.selection, self.output)
        self.assertFalse(self.output.exists())
        self.source(data=b"abcd", advertised=vs)
        self.select({"vs": [vs]})
        with self.assertRaisesRegex(exporter.ledger.LedgerError, "Ambiguous"):
            exporter.export(self.archive, self.selection, self.output)
        self.assertFalse(self.output.exists())
        self.assertEqual(payload.read_bytes(), b"1234")

    def test_fnv_length_stage_and_escape_rejection(self):
        vs, metadata, payload = self.source()
        original = json.loads(metadata.read_text())
        self.select({"vs": [vs]})
        for changes, message in (({"stage": "ps"}, "stage"), ({"byte_length": 8}, "length")):
            metadata.write_text(json.dumps({**original, **changes}))
            with self.assertRaisesRegex(exporter.ledger.LedgerError, message):
                exporter.export(self.archive, self.selection, self.output)
            self.assertFalse(self.output.exists())
        metadata.write_text(json.dumps({**original, "renderer_hash": "0" * 16}))
        self.select({"vs": ["0" * 16]})
        with self.assertRaisesRegex(exporter.ledger.LedgerError, "FNV"):
            exporter.export(self.archive, self.selection, self.output)
        with self.assertRaisesRegex(exporter.ledger.LedgerError, "escapes"):
            exporter.ledger.contained_file(self.archive, "../outside.bin")


if __name__ == "__main__":
    unittest.main()
