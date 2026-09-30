"""Offline fixtures for persistent evidence identity and safe ledger updates only."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

MODULE = Path(__file__).resolve().parents[1] / "scripts" / "feedback_ledger.py"
SPEC = importlib.util.spec_from_file_location("feedback_ledger", MODULE)
ledger = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(ledger)

VS = "1111111111111111"
PS = "2222222222222222"
OTHER_PS = "3333333333333333"
CASE = ledger.case_name(VS)


class LedgerTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.path = self.root / "feedback" / "triage" / "ledger.json"
        self.seed = self.root / "seed.json"
        self.context = self.root / "context.json"
        self.review_file = self.root / "review.json"
        (self.root / "feedback/data/observations").mkdir(parents=True)
        (self.root / "feedback/data/shader_sources").mkdir(parents=True)
        self.dump(self.context, {"schema": 1, "analysis_version": "1",
                                "cases": {CASE: {"mapping_slot": None, "binding_pairs": []}},
                                "provenance": {"source_sha256": "initial"}})
        self.observation()

    def dump(self, path, value):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(ledger.encoded(value))

    def observation(self, ps=PS, fields=None, first=10, last=20, draws=5):
        diagnostic = {"schema": 2, "namespace": ledger.NAMESPACE, "vs": VS, "ps": ps,
                      "slot": -1, "flags": 19, "rejection": 2, "guards": 31,
                      "position": {"kind": 1, "issues": 0, "slot": 7}, "width": 1280}
        diagnostic.update(fields or {})
        raw = json.dumps(diagnostic, separators=(",", ":"))
        ident = ledger.digest(raw.encode())
        path = self.root / "feedback/data/observations" / (ident + ".json")
        self.dump(path, {"id": ident, "diagnostic": raw, "first_seen": first,
                         "last_seen": last, "max_draws": draws})
        return ident, path

    def source(self, stage="vs", fnv=VS, payload=b"1234", first=10, last=20):
        sha = ledger.digest(payload)
        path = self.root / "feedback/data/shader_sources" / stage / (sha + ".json")
        self.dump(path, {"stage": stage, "sha256": sha, "renderer_hash": fnv,
                         "byte_length": len(payload), "first_seen": first, "last_seen": last})
        binary = self.root / "feedback/data/programs" / stage / (sha + ".bin")
        binary.parent.mkdir(parents=True, exist_ok=True)
        binary.write_bytes(payload)
        return sha, path, binary

    def sync(self, seed=False):
        return ledger.sync(self.root, self.path, self.seed if seed else None, self.context)[0]

    def current_case(self):
        return ledger.load_ledger(self.path)["cases"][CASE]

    def record_review(self, **changes):
        review = {"decision": "needs_evidence", "summary": "Reviewed only the available exact pair",
                  "evidence": ["feedback/triage/evidence/review.md"], "gaps": ["visual acceptance"],
                  "scope": "VS/PS translation"}
        review.update(changes)
        self.dump(self.review_file, review)
        ledger.review(self.path, CASE, self.review_file)

    def test_idempotent_seed_sync_review_and_checkpoint_report(self):
        self.dump(self.root / "feedback/checkpoint.json", {"schema": 1, "through": 12345})
        self.dump(self.seed, {"schema": 1, "cases": {CASE: {"vs": VS, "title": "Historical VS",
            "historical_review": {"decision": "candidate", "summary": "Old subset only",
                                  "scope": "one older PS pair", "evidence": ["old.md"], "gaps": []}}}})
        self.sync(seed=True)
        original = self.path.read_bytes()
        original_mtime = self.path.stat().st_mtime_ns
        self.sync(seed=True)
        self.assertEqual(self.path.read_bytes(), original)
        self.assertEqual(self.path.stat().st_mtime_ns, original_mtime)
        case = self.current_case()
        self.assertEqual(case["review_status"], "needs_review")
        self.assertEqual(len(case["historical_reviews"]), 1)
        self.assertIsNone(case["review"])
        self.record_review()
        reviewed = self.path.read_bytes()
        self.record_review()
        self.sync(seed=True)
        self.assertEqual(self.path.read_bytes(), reviewed)
        report = ledger.report_markdown(ledger.load_ledger(self.path))
        self.assertIn("12345", report)
        self.assertIn("not a live D1 query", report)
        self.assertIn("unbound", report)

    def test_source_arrival_invalidates_but_preserves_review_and_three_states(self):
        self.sync()
        self.record_review(implementation="implemented", validation="cpu_only", acceptance="not_accepted")
        before = self.current_case()
        self.source()
        self.sync()
        after = self.current_case()
        self.assertNotEqual(before["fingerprint"], after["fingerprint"])
        self.assertEqual(after["review_status"], "needs_review")
        self.assertEqual(before["review"], after["review"])
        self.assertEqual(after["implementation"], "implemented")
        self.assertEqual(after["validation"], "cpu_only")
        self.assertEqual(after["acceptance"], "not_accepted")
        self.assertEqual(len(after["evidence"]["sources"]["vs:" + VS]["sha256"]), 1)

    def test_new_ps_and_new_diagnostic_each_invalidate(self):
        self.sync()
        self.record_review()
        before = self.current_case()["fingerprint"]
        self.observation(ps=OTHER_PS, fields={"flags": 0})  # Related non-strong rows still count.
        self.sync()
        case = self.current_case()
        self.assertNotEqual(before, case["fingerprint"])
        self.assertEqual(case["review_status"], "needs_review")
        self.assertEqual(case["evidence"]["pairs"], [PS, OTHER_PS])
        self.assertEqual(len(case["evidence"]["strong_ids"]), 1)
        self.record_review()
        before = self.current_case()["fingerprint"]
        self.observation(fields={"width": 1920})
        self.sync()
        self.assertNotEqual(before, self.current_case()["fingerprint"])
        self.assertEqual(self.current_case()["review_status"], "needs_review")

    def test_times_counts_gpu_associations_and_global_provenance_do_not_invalidate(self):
        self.source()
        self.sync()
        self.record_review()
        before = self.current_case()
        self.observation(first=5, last=30, draws=50)
        self.source(first=2, last=40)
        self.dump(self.root / "feedback/data/shader_source_observations/association.json",
                  {"gpu": "another GPU association", "last_seen": 99})
        context = ledger.read_json(self.context)
        context["provenance"]["source_sha256"] = "unrelated code changes"
        self.dump(self.context, context)
        self.sync()
        after = self.current_case()
        self.assertEqual(before["fingerprint"], after["fingerprint"])
        self.assertEqual(before["history"], after["history"])
        self.assertEqual(after["review_status"], "current")
        self.assertEqual((after["evidence"]["first_seen"], after["evidence"]["last_seen"],
                          after["evidence"]["max_draws"]), (5, 30, 50))

    def test_expiry_and_reappearance_preserve_evidence_without_duplicate_counts(self):
        sha, source_path, _ = self.source()
        self.sync()
        self.record_review()
        original = self.current_case()
        _, observation_path = self.observation()
        source_path.unlink()
        observation_path.unlink()
        self.sync()
        self.assertEqual(self.current_case(), original)
        self.source(first=30, last=40)
        self.observation(first=30, last=40, draws=3)
        self.sync()
        after = self.current_case()
        self.assertEqual(after["fingerprint"], original["fingerprint"])
        self.assertEqual(len(after["evidence"]["observations"]), 1)
        self.assertEqual(after["evidence"]["max_draws"], 5)
        self.assertEqual(after["evidence"]["first_seen"], 10)
        self.assertEqual(after["evidence"]["last_seen"], 40)
        self.assertEqual(after["evidence"]["sources"]["vs:" + VS]["sha256"], [sha])

    def test_seed_never_overwrites_existing_review_or_independent_states(self):
        self.sync()
        self.record_review(implementation={"state": "implemented", "commit": "local"})
        before = self.current_case()
        self.dump(self.seed, {"schema": 1, "cases": {CASE: {"vs": VS,
            "implementation": "not_implemented", "validation": "old_value",
            "historical_review": {"decision": "candidate", "summary": "Old local VS was available",
                                  "scope": "local source, absent from archive", "gaps": [], "evidence": []}}}})
        self.sync(seed=True)
        after = self.current_case()
        self.assertEqual(before["review"], after["review"])
        self.assertEqual(before["implementation"], after["implementation"])
        self.assertEqual(before["validation"], after["validation"])
        self.assertEqual(len(after["historical_reviews"]), 1)
        self.assertTrue(after["evidence"]["sources"]["vs:" + VS]["missing"])

    def test_bad_json_hash_namespace_and_missing_payload_leave_original_untouched(self):
        self.sync()
        original = self.path.read_bytes()
        _, observation_path = self.observation()
        valid = observation_path.read_bytes()
        for bad in (b"{", valid.replace(b'"max_draws": 5', b'"max_draws": NaN'),
                    valid.replace(b'1280', b'1920')):
            with self.subTest(bad=bad[:10]):
                observation_path.write_bytes(bad)
                with self.assertRaises(ledger.LedgerError):
                    self.sync()
                self.assertEqual(self.path.read_bytes(), original)
                self.assertFalse(Path(str(self.path) + ".lock").exists())
        observation_path.write_bytes(valid)
        _, bad_namespace = self.observation(fields={"namespace": "xenia-dword-fnv"})
        with self.assertRaisesRegex(ledger.LedgerError, "namespace"):
            self.sync()
        self.assertEqual(self.path.read_bytes(), original)
        bad_namespace.unlink()
        _, _, binary = self.source()
        binary.unlink()
        with self.assertRaisesRegex(ledger.LedgerError, "payload missing"):
            self.sync()
        self.assertEqual(self.path.read_bytes(), original)

    def test_fnv_collision_keeps_all_sha_variants_and_invalidates_review(self):
        sha1, _, _ = self.source(payload=b"1111")
        self.sync()
        self.record_review()
        sha2, _, _ = self.source(payload=b"2222")  # Trusted archive metadata fixture simulates collision.
        self.sync()
        case = self.current_case()
        bucket = case["evidence"]["sources"]["vs:" + VS]
        self.assertEqual(bucket["sha256"], sorted([sha1, sha2]))
        self.assertEqual(bucket["available"], sorted([sha1, sha2]))
        self.assertTrue(bucket["ambiguous"])
        self.assertEqual(case["review_status"], "needs_review")
        self.assertIn("FNV collision/ambiguity", ledger.report_markdown(ledger.load_ledger(self.path)))

    def test_lock_conflict_preserves_owner_lock_and_existing_ledger(self):
        self.sync()
        original = self.path.read_bytes()
        lock = Path(str(self.path) + ".lock")
        lock.write_text("another process\n", encoding="utf-8")
        with self.assertRaisesRegex(ledger.LedgerError, "lock exists"):
            self.sync()
        self.assertEqual(self.path.read_bytes(), original)
        self.assertEqual(lock.read_text(encoding="utf-8"), "another process\n")

    def test_context_change_only_invalidates_affected_case(self):
        other_vs = "4444444444444444"
        other_case = ledger.case_name(other_vs)
        self.observation(fields={"vs": other_vs})
        self.sync()
        before = ledger.load_ledger(self.path)
        context = ledger.read_json(self.context)
        context["cases"][CASE]["binding_pairs"] = [VS + ":" + PS]
        self.dump(self.context, context)
        after = self.sync()
        self.assertNotEqual(before["cases"][CASE]["fingerprint"], after["cases"][CASE]["fingerprint"])
        self.assertEqual(before["cases"][other_case]["fingerprint"], after["cases"][other_case]["fingerprint"])
        self.assertTrue(after["cases"][other_case]["evidence"]["context"]["mapping_unknown"])

    def test_zero_ps_is_no_ps_not_missing_microcode(self):
        self.observation(ps=ledger.ZERO_PS)
        self.sync()
        case = self.current_case()
        self.assertTrue(case["evidence"]["no_ps"])
        self.assertNotIn("ps:" + ledger.ZERO_PS, case["evidence"]["sources"])
        report = ledger.report_markdown(ledger.load_ledger(self.path))
        self.assertIn("no PS; not missing microcode", report)

    def test_report_separates_historical_gaps_and_informational_coverage(self):
        self.dump(self.seed, {"schema": 1, "cases": {CASE: {"vs": VS,
            "historical_review": {"decision": "candidate", "summary": "Old source review",
                                  "scope": "one old pair", "gaps": ["old_task"], "evidence": []}}}})
        self.sync(seed=True)
        self.record_review(decision="reviewed", gaps=[], evidence=["current-review.md"])
        report = ledger.report_markdown(ledger.load_ledger(self.path))
        self.assertIn("Historical gap (unbound, not a current task): old\\_task", report)
        active = report.split("\nGaps:\n", 1)[1]
        self.assertNotIn("old", active)
        self.assertNotIn("Binding collection", active)
        self.assertIn("Collection coverage (informational)", report)
        self.assertIn("current-review.md", report)
        context = ledger.read_json(self.context)
        context["cases"][CASE]["binding_evidence_required"] = True
        self.dump(self.context, context)
        self.sync(seed=True)
        report = ledger.report_markdown(ledger.load_ledger(self.path))
        self.assertIn("Binding collection", report.split("\nGaps:\n", 1)[1])

    def test_cli_report_review_and_invalid_inputs(self):
        self.assertEqual(ledger.main(["sync", "--archive", str(self.root), "--ledger", str(self.path)]), 0)
        output = self.root / "report.md"
        self.assertEqual(ledger.main(["report", "--ledger", str(self.path), "--output", str(output)]), 0)
        self.assertIn(VS, output.read_text(encoding="utf-8"))
        self.dump(self.review_file, {"decision": "needs_evidence", "summary": "<script>alert(1)</script> [run](cmd)",
                                     "gaps": ["a|b\nnew line"], "scope": "source only"})
        self.assertEqual(ledger.main(["review", "--ledger", str(self.path), "--case", CASE,
                                      "--file", str(self.review_file)]), 0)
        report = ledger.report_markdown(ledger.load_ledger(self.path))
        self.assertNotIn("<script>", report)
        self.assertNotIn("[run](cmd)", report)
        original = self.path.read_bytes()
        self.assertEqual(ledger.main(["report", "--ledger", str(self.path), "--output", str(self.path)]), 1)
        self.assertEqual(self.path.read_bytes(), original)


if __name__ == "__main__":
    unittest.main()
