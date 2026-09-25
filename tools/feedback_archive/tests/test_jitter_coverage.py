"""Focused streaming archive coverage tests using only generated metadata."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from test_compact_feedback import compact, record, binding, VS, PS

SCRIPT = Path(__file__).resolve().parents[1] / "scripts/jitter_coverage.py"
SPEC = importlib.util.spec_from_file_location("jitter_coverage", SCRIPT)
coverage = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(coverage)
ledger = coverage.ledger


class StreamingCoverageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.archive = self.root / "archive"
        self.observations = self.archive / "feedback/data/observations"
        self.sources = self.archive / "feedback/data/shader_sources"
        self.observations.mkdir(parents=True)
        self.sources.mkdir(parents=True)
        self.mapping = self.root / "mapping.h"
        self.mapping.write_text("inline int PositionVPSlot(uint64_t h) {switch(h){"
                                f"case 0x{VS}ull:return 7;default:return -1;" + "}}")

    def observe(self, diagnostic, max_draws=20):
        raw = json.dumps(diagnostic, separators=(",", ":"))
        ident = ledger.digest(raw.encode())
        path = self.observations / (ident + ".json")
        path.write_text(json.dumps({"id": ident, "diagnostic": raw, "first_seen": 1,
                                    "last_seen": 2, "max_draws": max_draws}))
        return path

    def source(self, fnv=VS):
        sha = "1" * 64
        folder = self.sources / "vs"
        folder.mkdir(exist_ok=True)
        (folder / (sha + ".json")).write_text(json.dumps({"stage": "vs", "sha256": sha,
            "renderer_hash": fnv, "byte_length": 4, "first_seen": 1, "last_seen": 2}))
        payload = self.archive / f"feedback/data/programs/vs/{sha}.bin"
        payload.parent.mkdir(parents=True, exist_ok=True)
        payload.write_bytes(b"fake")

    def test_older_and_compact_pairs_bindings_dedup_and_stale_mapping(self):
        for schema in (1, 2, 3):
            self.observe({**record(), "schema": schema, "namespace": ledger.NAMESPACE})
        window = compact()
        window["bindings"] = [{"offset": 0, "record": binding()}]
        self.observe(window, 50)
        self.source()
        result = coverage.analyze(self.archive, self.mapping)
        pair, = result["pairs"]
        self.assertEqual((pair["observation_count"], pair["record_count"]), (4, 5))
        self.assertEqual(pair["max_draws_observed"], 50)
        self.assertEqual(pair["current_mapping_slot"], 7)
        self.assertEqual(pair["captured_mapping_relation"]["captured_unmapped_current_mapped"], 5)
        self.assertEqual(pair["groups"]["schema4.bindings"], 1)
        representative = next(e for e in pair["examples"] if "window" in e)
        self.assertEqual(len(representative["records"]), 2)
        self.assertFalse(representative["window"]["capabilities"]["gpuCompletion"])
        self.assertTrue(pair["sources"]["vs"][0]["payload_present"])

    def test_bad_metadata_unknown_schema_and_duplicate_ids_are_explicit(self):
        path = self.observe({**record(), "schema": 2, "namespace": ledger.NAMESPACE})
        duplicate = self.observations / "duplicate"
        duplicate.mkdir()
        (duplicate / path.name).write_bytes(path.read_bytes())
        self.observe({"schema": 99, "namespace": ledger.NAMESPACE})
        (self.observations / "broken.json").write_text('{"id":1,"id":2}')
        (self.sources / "bad.json").write_text("[]")
        result = coverage.analyze(self.archive, self.mapping)
        self.assertEqual(result["summary"]["errors"], 4)
        self.assertEqual(result["summary"]["duplicate_observation_ids"], 1)
        self.assertEqual(result["summary"]["unknown_schemas"], {"99": 1})
        self.assertEqual(result["pairs"][0]["observation_count"], 1)

    def test_samples_bounded_and_source_only_not_an_observed_miss(self):
        self.source(fnv="9" * 16)
        for index in range(12):
            self.observe({**record(), "schema": 2, "namespace": ledger.NAMESPACE, "build": str(index)})
        result = coverage.analyze(self.archive, self.mapping)
        self.assertEqual(len(result["pairs"]), 1)
        self.assertEqual(len(result["pairs"][0]["examples"]), 8)
        self.assertEqual(result["summary"]["unmapped_vs"], 0)
        self.assertEqual(result["summary"]["source_metadata_count"], 1)

    def test_output_protection_and_direct_invocation_from_archive_cwd(self):
        inside = self.archive / "report.json"
        args = ["--archive", str(self.archive), "--mapping", str(self.mapping)]
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            coverage.main([*args, "--output", str(inside)])
        self.assertFalse(inside.exists())
        output = self.root / "report.json"
        result = subprocess.run([sys.executable, "-B", str(SCRIPT), *args, "--output", str(output)],
                                cwd=self.archive, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        saved = output.read_bytes()
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            coverage.main([*args, "--output", str(output)])
        self.assertEqual(output.read_bytes(), saved)


if __name__ == "__main__":
    unittest.main()
