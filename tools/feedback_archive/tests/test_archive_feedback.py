import importlib.util
import json
from pathlib import Path
import sqlite3
import struct
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location("archive_feedback", Path(__file__).parents[1] / "scripts/archive_feedback.py")
archive = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(archive)


class Database:
    account_id = "0" * 32
    database_id = "00000000-0000-0000-0000-000000000000"

    def __init__(self):
        self.connection = sqlite3.connect(":memory:")
        self.connection.row_factory = sqlite3.Row
        self.connection.executescript("""
            CREATE TABLE observations(id TEXT PRIMARY KEY, diagnostic TEXT, max_draws INTEGER, first_seen INTEGER, last_seen INTEGER);
            CREATE TABLE shader_sources(stage TEXT, sha256 TEXT, renderer_hash TEXT, byte_length INTEGER, payload BLOB, first_seen INTEGER, last_seen INTEGER, PRIMARY KEY(stage,sha256));
            CREATE TABLE shader_source_observations(stage TEXT, sha256 TEXT, build TEXT, backend TEXT, gpu_key TEXT, gpu TEXT, driver TEXT, first_seen INTEGER, last_seen INTEGER, PRIMARY KEY(stage,sha256,build,backend,gpu_key,driver));
            CREATE TABLE temporal_sequences(id TEXT PRIMARY KEY, metadata TEXT, summary TEXT, payload BLOB, first_seen INTEGER, last_seen INTEGER);
        """)
        self.sql_log = []
        self.now = 2000
        self.payload_reads = 0
        self.fail_table = None

    def query(self, sql, params=()):
        self.sql_log.append((sql, params))
        if " AS now" in sql:
            return [{"now": self.now}]
        if self.fail_table and f"FROM {self.fail_table}" in sql:
            raise archive.ArchiveError("Synthetic query failure")
        if "hex(payload)" in sql:
            self.payload_reads += 1
        # Match D1's documented string parameter representation.
        return [dict(row) for row in self.connection.execute(sql, [str(p) for p in params])]

    def diagnostic(self, number=0, last=1000, schema=2):
        value = json.dumps({"schema": schema, "synthetic": number}, separators=(",", ":"))
        key = archive.sha256(value.encode())
        self.connection.execute("INSERT INTO observations VALUES(?,?,?,?,?)", (key, value, 5, last, last))
        return key

    def source(self, stage="vs", data=b"\x01\x02\x03\x04", last=1000):
        key = archive.sha256(data)
        self.connection.execute("INSERT INTO shader_sources VALUES(?,?,?,?,?,?,?)", (stage, key, archive.renderer_hash(data), len(data), data, last, last))
        return key

    def association(self, key, gpu="Synthetic GPU", last=1000):
        self.connection.execute("INSERT INTO shader_source_observations VALUES(?,?,?,?,?,?,?,?,?)", ("vs", key, "synthetic", "d3d12", gpu.lower(), gpu, "0", last, last))

    def temporal(self, packed_codec="LOR1", last=1000):
        raw = bytearray(16 + 32 * 4824)
        struct.pack_into("<4sIII", raw, 0, b"LOMV", 1, 32, 4824)
        for frame in range(32):
            offset = 16 + 4824 * frame
            struct.pack_into("<QQdIII", raw, offset, frame + 100, 7, frame / 60, 1280, 720, 3)
            for sample in range(576):
                struct.pack_into("<eef", raw, offset + 216 + sample * 8, 1, -2, 0.5)
        packed = b"LOR1" + raw
        if packed_codec == "LOZ1":
            delta = bytearray(raw)
            for offset in range(len(raw) - 1, 16 + 4824 - 1, -1):
                delta[offset] ^= raw[offset - 4824]
            chunks = [b"LOZ1"]
            for offset in range(0, len(delta), 128):
                part = delta[offset:offset + 128]
                chunks.append(bytes([len(part) - 1]) + part)
            packed = b"".join(chunks)
        metadata = json.dumps({"build": "synthetic", "backend": "d3d12", "gpu": "Synthetic GPU", "driver": "0"}, separators=(",", ":"))
        decoded, summary = archive.temporal_raw_and_summary(packed)
        assert decoded == raw
        key = archive.sha256(metadata.encode() + raw)
        self.connection.execute("INSERT INTO temporal_sequences VALUES(?,?,?,?,?,?)", (key, metadata, json.dumps(summary), packed, last, last))
        return key


class ArchiveTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        self.db = Database()

    def tearDown(self):
        self.db.connection.close()
        self.directory.cleanup()

    def run_archive(self, page=2):
        return archive.Archive(self.root, self.db, page).run()

    def checkpoint(self):
        return (self.root / "feedback/checkpoint.json").read_bytes()

    def test_empty_initial_scan_advances_once(self):
        self.run_archive()
        self.assertEqual(json.loads(self.checkpoint())["through"], 1880)
        self.assertEqual(self.run_archive()["files_changed"], 0)

    def test_same_second_pagination_includes_every_primary_key(self):
        ids = [self.db.diagnostic(i) for i in range(9)]
        result = self.run_archive()
        self.assertEqual(result["rows_scanned"]["observations"], 9)
        self.assertEqual({p.stem for p in (self.root / "feedback/data/observations").glob("*.json")}, set(ids))

    def test_composite_primary_key_pagination(self):
        key = self.db.source()
        for i in range(7):
            self.db.association(key, f"GPU {i}")
        self.assertEqual(self.run_archive()["rows_scanned"]["shader_source_observations"], 7)
        self.assertEqual(len(list((self.root / "feedback/data/programs").rglob("*.bin"))), 1)

    def test_incremental_update_and_no_payload_redownload(self):
        key = self.db.source(last=1700)
        diag = self.db.diagnostic(last=1700)
        self.db.association(key, last=1700)
        self.run_archive()
        self.db.now = 2200
        self.db.connection.execute("UPDATE shader_sources SET last_seen=2000")
        self.db.connection.execute("UPDATE observations SET last_seen=2000,max_draws=20")
        self.db.connection.execute("UPDATE shader_source_observations SET last_seen=2000")
        result = self.run_archive()
        self.assertEqual(result["payloads_downloaded"], 0)
        self.assertEqual(self.db.payload_reads, 1)
        row = json.loads((self.root / f"feedback/data/observations/{diag}.json").read_bytes())
        self.assertEqual((row["first_seen"], row["last_seen"], row["max_draws"]), (1700, 2000, 20))

    def test_overlap_catches_late_same_second_record(self):
        self.run_archive()
        key = self.db.diagnostic(last=1880)
        self.db.now += 60
        self.run_archive()
        self.assertTrue((self.root / f"feedback/data/observations/{key}.json").is_file())

    def test_future_bound_defers_current_writes(self):
        key = self.db.diagnostic(last=1990)
        self.assertEqual(self.run_archive()["rows_scanned"]["observations"], 0)
        self.db.now += 120
        self.run_archive()
        self.assertTrue((self.root / f"feedback/data/observations/{key}.json").is_file())

    def test_schema_three_preserves_original_json(self):
        key = self.db.diagnostic(schema=3)
        self.run_archive()
        row = json.loads((self.root / f"feedback/data/observations/{key}.json").read_bytes())
        self.assertEqual(archive.sha256(row["diagnostic"].encode()), key)

    def test_schema_four_preserves_content_and_reappearance_identity(self):
        # The Worker validates the bounded wire contract. The archive retains its
        # canonical JSON without flattening frame references or rewriting IDs.
        raw = json.dumps({"schema": 4, "namespace": "renderer-byte-fnv1a64",
                          "window": {"synthetic": True, "frames": [0, 1]}}, separators=(",", ":"))
        key = archive.sha256(raw.encode())
        self.db.connection.execute("INSERT INTO observations VALUES(?,?,?,?,?)", (key, raw, 1, 1000, 1000))
        self.run_archive()
        path = self.root / f"feedback/data/observations/{key}.json"
        first = json.loads(path.read_bytes())
        self.assertEqual(first["diagnostic"], raw)
        self.db.connection.execute("UPDATE observations SET first_seen=1800,last_seen=1800,max_draws=2 WHERE id=?", (key,))
        self.db.now += 60
        self.run_archive()
        second = json.loads(path.read_bytes())
        self.assertEqual(second["diagnostic"], raw)
        self.assertEqual(archive.sha256(second["diagnostic"].encode()), key)
        self.assertEqual((second["first_seen"], second["last_seen"], second["max_draws"]), (1000, 1800, 2))
        self.assertEqual(len(list((self.root / "feedback/data/observations").glob("*.json"))), 1)

    def test_query_failure_does_not_promote_data_or_checkpoint(self):
        self.run_archive()
        baseline = self.checkpoint()
        key = self.db.diagnostic(last=1800)
        self.db.source(last=1800)
        self.db.fail_table = "temporal_sequences"
        self.db.now += 60
        with self.assertRaises(archive.ArchiveError):
            self.run_archive()
        self.assertEqual(self.checkpoint(), baseline)
        self.assertFalse((self.root / f"feedback/data/observations/{key}.json").exists())
        self.db.fail_table = None
        self.assertEqual(self.run_archive()["payloads_downloaded"], 0)
        self.assertNotEqual(self.checkpoint(), baseline)

    def test_bad_source_sha_or_fnv_or_length_fails_without_checkpoint(self):
        for column, value in (("sha256", "0" * 64), ("renderer_hash", "0" * 16), ("byte_length", 8)):
            with self.subTest(column=column):
                self.db.connection.execute("DELETE FROM shader_sources")
                self.db.source()
                self.db.connection.execute(f"UPDATE shader_sources SET {column}=?", (value,))
                with self.assertRaises(archive.ArchiveError):
                    self.run_archive()
                self.assertFalse((self.root / "feedback/checkpoint.json").exists())

    def test_bad_diagnostic_hash_fails(self):
        self.db.diagnostic()
        self.db.connection.execute("UPDATE observations SET diagnostic='{}'")
        with self.assertRaises(archive.ArchiveError):
            self.run_archive()

    def test_temporal_raw_and_delta_codec_round_trip_and_dedup(self):
        key = self.db.temporal()
        self.run_archive()
        self.db.now += 60
        self.assertEqual(self.run_archive()["payloads_downloaded"], 0)
        self.db.connection.execute("DELETE FROM temporal_sequences")
        same_key = self.db.temporal("LOZ1")
        self.assertEqual(key, same_key)
        self.assertEqual(self.run_archive()["payloads_downloaded"], 0)
        # A different wire encoding for the same content is valid in isolation.
        with tempfile.TemporaryDirectory() as other:
            archive.Archive(other, self.db).run()

    def test_temporal_tamper_or_summary_mismatch_fails(self):
        self.db.temporal()
        self.db.connection.execute("UPDATE temporal_sequences SET summary='{}'")
        with self.assertRaises(archive.ArchiveError):
            self.run_archive()
        self.assertFalse((self.root / "feedback/checkpoint.json").exists())

    def test_reappearing_diagnostic_preserves_archived_first_seen_and_max(self):
        key = self.db.diagnostic(last=1800)
        self.run_archive()
        self.db.connection.execute("UPDATE observations SET first_seen=2050,last_seen=2050,max_draws=1")
        self.db.now = 2200
        self.run_archive()
        row = json.loads((self.root / f"feedback/data/observations/{key}.json").read_bytes())
        self.assertEqual((row["first_seen"], row["last_seen"], row["max_draws"]), (1800, 2050, 5))

    def test_association_fetches_source_that_advanced_beyond_scan_bound(self):
        key = self.db.source(last=1990)
        self.db.association(key, last=1800)
        self.run_archive()
        self.assertTrue((self.root / f"feedback/data/programs/vs/{key}.bin").is_file())

    def test_missing_associated_source_fails(self):
        self.db.association("0" * 64)
        with self.assertRaises(archive.ArchiveError):
            self.run_archive()
        self.assertFalse((self.root / "feedback/checkpoint.json").exists())

    def test_wrong_database_checkpoint_refused(self):
        self.run_archive()
        value = json.loads(self.checkpoint())
        value["database_id"] = "other"
        (self.root / "feedback/checkpoint.json").write_text(json.dumps(value))
        with self.assertRaises(archive.ArchiveError):
            self.run_archive()

    def test_new_only_skips_existing_full_records_but_archives_new(self):
        self.db.source(last=1700)
        old = self.db.diagnostic(last=1700)
        self.run_archive()
        path = self.root / f"feedback/data/observations/{old}.json"
        baseline = path.read_bytes()
        self.db.connection.execute("UPDATE observations SET last_seen=2000,max_draws=99")
        new = self.db.diagnostic(1, last=2000)
        self.db.now = 2200
        self.db.sql_log.clear()
        result = archive.Archive(self.root, self.db, 2, new_only=True).run()
        self.assertEqual(path.read_bytes(), baseline)
        self.assertTrue((self.root / f"feedback/data/observations/{new}.json").exists())
        self.assertEqual(result["existing_records_skipped"], 2)
        self.assertEqual(result["rows_scanned"]["observations"], 1)
        full = [(sql, params) for sql, params in self.db.sql_log if "diagnostic" in sql]
        self.assertEqual(len(full), 1)
        self.assertEqual(full[0][1], [new])
        self.assertEqual(result["payloads_downloaded"], 0)

    def test_new_only_same_second_composite_keys_and_repeat(self):
        key = self.db.source(last=1700)
        for i in range(7):
            self.db.association(key, f"GPU {i}", last=1700)
        first = archive.Archive(self.root, self.db, 2, new_only=True).run()
        self.assertEqual(first["rows_scanned"]["shader_source_observations"], 7)
        second = archive.Archive(self.root, self.db, 2, new_only=True).run()
        self.assertEqual(second["existing_records_skipped"], 8)
        self.assertEqual(second["files_changed"], 0)

    def test_new_only_retry_promotes_metadata_left_in_staging(self):
        key = self.db.diagnostic(last=1700)
        self.db.fail_table = "temporal_sequences"
        with self.assertRaises(archive.ArchiveError):
            archive.Archive(self.root, self.db, 2, new_only=True).run()
        self.assertFalse((self.root / "feedback/checkpoint.json").exists())
        self.db.fail_table = None
        archive.Archive(self.root, self.db, 2, new_only=True).run()
        self.assertTrue((self.root / f"feedback/data/observations/{key}.json").exists())

    def test_indexed_pages_seek_across_large_timestamp_groups(self):
        self.db.connection.execute("CREATE INDEX observations_expiry ON observations(last_seen)")
        for i in range(2000):
            self.db.diagnostic(i, last=1000 + i // 700)
        steps = [0]
        def progress():
            steps[0] += 100
            return 0
        self.db.connection.set_progress_handler(progress, 100)
        rows = list(archive.rows_since(self.db, "observations", 0, 1880, 100, True))
        self.db.connection.set_progress_handler(None, 0)
        self.assertEqual(len({r["id"] for r in rows}), 2000)
        self.assertLess(steps[0], 100000)

    def test_source_primary_key_pages_with_time_filter(self):
        expected = []
        for i in range(30):
            key = self.db.source(data=struct.pack("<I", i), last=1700 if i % 2 else 1990)
            if i % 2:
                expected.append(key)
        rows = list(archive.rows_since(self.db, "shader_sources", 0, 1880, 2))
        self.assertEqual(sorted(row["sha256"] for row in rows), sorted(expected))

    def test_missing_token_and_mutation_are_rejected(self):
        with self.assertRaises(archive.ArchiveError):
            archive.D1("", self.db.account_id, self.db.database_id)
        with self.assertRaises(archive.ArchiveError):
            archive.D1("synthetic", self.db.account_id, self.db.database_id).query("DELETE FROM observations")


if __name__ == "__main__":
    unittest.main()
