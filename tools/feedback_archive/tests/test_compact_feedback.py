"""Schema4 archive-to-ledger behavior; no network, game or old suite rerun."""
from copy import deepcopy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location("feedback_ledger", Path(__file__).parents[1] / "scripts/feedback_ledger.py")
ledger = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(ledger)

VS = "e810cfacc107fd3c"
PS = "5b11f88a8bb293df"
PS2 = "78a5c96b2d7eaa91"
CASE = ledger.case_name(VS)


def counts(names):
    return dict.fromkeys(names.split(","), 0)


def record(ps=PS):
    return {"vs": VS, "ps": ps, "width": 1280, "height": 720, "slot": -1,
            "candidates": 8, "flags": 19, "rejection": 2, "draws": 2,
            "position": {"version": 1, "kind": 1, "slot": 7, "issues": 0, "outputs": 17}, "guards": 31}


def binding():
    def transform():
        return {"slot": 7, "phase": 0, "applied": False, "guestVP": [0] * 16,
                "uploadedVP": [0] * 16, "viewport": [0] * 4, "jitterNdc": [0, 0]}
    return {**record(), "consumer": transform(), "psC0": [0] * 4, "texture": {
        "slot": 0, "kind": 0, "bank": 0, "guestFormat": 0, "hostFormat": 0, "dimension": 0,
        "swizzle": 0, "sourceMip": 0, "sign": 0, "swapRedBlue": False, "sampler": [0] * 6,
        "guestExtent": [0, 0], "hostExtent": [0, 0], "parentExtent": [0, 0], "resolveRect": [0] * 4,
        "producerFrameAge": -1, "resolveFrameAge": -1, "resolveGap": -1,
        "producerState": 0, "producerDraws": 0, "producer": transform()}}


def compact():
    return {"schema": 4, "namespace": ledger.NAMESPACE, "build": "0.5.2-collection-diagnostics-1",
            "backend": "d3d12", "gpu": "Test GPU", "driver": "0",
            "capabilities": {"version": 1, "runtimeVersion": "0.5.2", "runtimeCommit": "d1bb801",
                "cpuWindowFrames": 32, "cpuCooldownSeconds": 180, "pairCapacity": 24, "bindingCapacity": 8,
                "bindingPairs": [VS + ":" + PS, VS + ":" + PS2], "bindingTextureSlot": 0,
                "bindingPerPairPerFrame": 1, "sourceMaxProgramBytes": 65536, "sparseSupported": True,
                "sparseFrames": 32, "sparseCooldownSeconds": 300, "sparseWindowLinked": False,
                "gpuCompletion": False, "colorImages": False, "sourceScope": "draw-program-observe",
                "summaryScope": "taa-draw-observe", "pendingWindowCapacity": 1, "strictAck": ["compact"]},
            "complete": False, "frameSpan": 1,
            "counters": {"source": counts("queued,known,invalid,full,disabled,busy"),
                "summary": counts("queued,counted,dropped,disabled,busy"),
                "binding": counts("queued,counted,full,busy,disabled,stale"),
                "sparse": counts("queued,duplicate,full,busy,disabled,stale,cooldown,discontinuous"),
                "compact": counts("pairDropped,bindingDropped,lockBusy,frameDiscontinuity")},
            "delivery": {"scope": "since-consent-reset", **{name: counts("accepted,transportFailed,httpRejected")
                for name in ("source", "summary", "binding", "sparse", "compact")}},
            "pending": {"summary": 0, "source": 0, "binding": 0, "sparseFrames": 0},
            "frames": [{"offset": 0, "taa": True, "ready": True, "completed": True, "reused": False,
                "sceneRejection": 0, "historyCaptured": True, "historyRejection": 1, "cameraChecks": True,
                "previousFrameDelta": 1, "sameEpoch": True, "resetAfterFrame": False, "sparseReady": True}],
            "pairs": [{"first": 0, "last": 0, "record": record()}], "bindings": []}


class CompactLedgerTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.path = self.root / "feedback/triage/ledger.json"
        (self.root / "feedback/data/observations").mkdir(parents=True)
        (self.root / "feedback/data/shader_sources").mkdir(parents=True)

    def add(self, diagnostic, first=10, last=20, max_draws=1):
        raw = json.dumps(diagnostic, separators=(",", ":"))
        ident = ledger.digest(raw.encode())
        path = self.root / f"feedback/data/observations/{ident}.json"
        path.write_bytes(ledger.encoded({"id": ident, "diagnostic": raw, "first_seen": first,
                                        "last_seen": last, "max_draws": max_draws}))
        return ident, path

    def sync(self):
        return ledger.sync(self.root, self.path)[0]

    def review(self):
        path = self.root / "review.json"
        path.write_bytes(ledger.encoded({"decision": "needs_binding_review", "summary": "Source review only",
            "scope": "exact pair", "gaps": [], "evidence": ["local-review.md"],
            "implementation": "implemented", "validation": "offline_only", "acceptance": "not_accepted"}))
        return ledger.review(self.path, CASE, path)

    def test_compact_only_discovery_retains_one_window_and_exact_child_references(self):
        diagnostic = compact()
        diagnostic["bindings"] = [{"offset": 0, "record": binding()}]
        ident, _ = self.add(diagnostic, max_draws=500)
        result = self.sync()
        case = result["cases"][CASE]
        self.assertEqual(set(result["compact_windows"]), {ident})
        self.assertEqual(result["compact_windows"][ident]["diagnostic"], diagnostic)
        self.assertEqual(case["evidence"]["compact_refs"][ident],
                         [{"group": "pairs", "index": 0}, {"group": "bindings", "index": 0}])
        self.assertEqual(case["evidence"]["observations"], {})
        self.assertEqual(case["evidence"]["pairs"], [PS])
        self.assertEqual(case["evidence"]["max_draws"], 2)  # Never use 500 window occurrences as shader draws.
        self.assertEqual(set(case["evidence"]["sources"]), {"vs:" + VS, "ps:" + PS})
        self.assertEqual(case["review_status"], "needs_review")
        self.assertEqual(ledger.load_ledger(self.path), result)

    def test_counter_metadata_and_frame_changes_do_not_invalidate_or_duplicate_semantics(self):
        original = compact()
        self.add(original)
        self.sync()
        before = self.review()["cases"][CASE]
        modified = deepcopy(original)
        modified["gpu"] = "Another GPU"
        modified["capabilities"]["runtimeCommit"] = "abcdef0"
        modified["counters"]["source"]["full"] = 4
        modified["delivery"]["source"]["transportFailed"] = 1
        modified["pending"]["source"] = 3
        modified["pairs"][0]["record"]["draws"] = 9
        modified["frames"][0]["historyRejection"] = 2
        modified["frames"][0]["reused"] = True
        self.add(modified, last=30)
        result = self.sync()
        after = result["cases"][CASE]
        self.assertEqual(before["fingerprint"], after["fingerprint"])
        self.assertEqual(before["history"], after["history"])
        self.assertEqual(after["review_status"], "current")
        self.assertEqual(after["implementation"], "implemented")
        self.assertEqual(after["validation"], "offline_only")
        self.assertEqual(len(after["evidence"]["compact_semantic_ids"]), 1)
        self.assertEqual(len(result["compact_windows"]), 2)
        before_bytes, before_time = self.path.read_bytes(), self.path.stat().st_mtime_ns
        self.sync()
        self.assertEqual((self.path.read_bytes(), self.path.stat().st_mtime_ns), (before_bytes, before_time))

    def test_new_pair_and_changed_binding_invalidate_and_preserve_manual_review(self):
        self.add(compact())
        self.sync()
        before = self.review()["cases"][CASE]
        second = compact()
        second["pairs"].append({"first": 0, "last": 0, "record": record(PS2)})
        self.add(second)
        after = self.sync()["cases"][CASE]
        self.assertNotEqual(before["fingerprint"], after["fingerprint"])
        self.assertEqual(after["review"], before["review"])
        self.assertEqual(after["review_status"], "needs_review")
        before = self.review()["cases"][CASE]
        second["bindings"] = [{"offset": 0, "record": binding()}]
        self.add(second)
        after = self.sync()["cases"][CASE]
        self.assertNotEqual(before["fingerprint"], after["fingerprint"])
        before = self.review()["cases"][CASE]
        second["bindings"][0]["record"]["texture"]["producerState"] = 1
        self.add(second)
        after = self.sync()["cases"][CASE]
        self.assertNotEqual(before["fingerprint"], after["fingerprint"])
        self.assertEqual(after["implementation"], "implemented")

    def test_repeat_existing_legacy_semantics_does_not_invalidate_review(self):
        legacy = {**record(), "schema": 2, "namespace": ledger.NAMESPACE,
                  "build": "older", "backend": "d3d12", "gpu": "Old GPU", "driver": "0"}
        legacy.pop("draws")
        self.add(legacy)
        self.sync()
        before = self.review()["cases"][CASE]
        self.add(compact())
        after = self.sync()["cases"][CASE]
        self.assertEqual(before["fingerprint"], after["fingerprint"])
        self.assertEqual(after["evidence"]["compact_semantic_ids"], [])
        self.assertEqual(after["review_status"], "current")

    def test_expiry_reappearance_keeps_window_references_and_counts_maxima(self):
        diagnostic = compact()
        ident, path = self.add(diagnostic, max_draws=3)
        self.sync()
        before = self.review()["cases"][CASE]
        path.unlink()
        retained = self.sync()
        self.assertEqual(retained["cases"][CASE], before)
        self.assertIn(ident, retained["compact_windows"])
        self.add(diagnostic, first=30, last=40, max_draws=2)
        after = self.sync()
        self.assertEqual(len(after["compact_windows"]), 1)
        window = after["compact_windows"][ident]
        self.assertEqual((window["first_seen"], window["last_seen"], window["max_draws"]), (10, 40, 3))
        self.assertEqual(after["cases"][CASE]["fingerprint"], before["fingerprint"])

    def test_bad_bounds_or_join_reference_leave_existing_ledger_unchanged(self):
        self.add(compact())
        self.sync()
        original = self.path.read_bytes()
        def duplicate_frame(d):
            d["frames"].append(deepcopy(d["frames"][0]))
        for change in (lambda d: d["pairs"][0].update(last=32), duplicate_frame,
                       lambda d: d.update(pairs=d["pairs"] * 25),
                       lambda d: d["pairs"][0]["record"].update(vs="bad"),
                       lambda d: d["counters"]["source"].update(full=-1),
                       lambda d: d["capabilities"].update(sparseWindowLinked=True)):
            broken = compact()
            change(broken)
            _, path = self.add(broken)
            with self.assertRaises(ledger.LedgerError):
                self.sync()
            self.assertEqual(self.path.read_bytes(), original)
            path.unlink()

    def test_report_separates_scope_absence_pending_failures_and_analysis(self):
        diagnostic = compact()
        diagnostic["backend"] = "vulkan"
        diagnostic["capabilities"]["sparseSupported"] = False
        diagnostic["pairs"][0]["record"]["ps"] = "3333333333333333"
        diagnostic["counters"]["source"]["busy"] = 1
        diagnostic["pending"]["source"] = 2
        diagnostic["delivery"]["source"]["transportFailed"] = 3
        ident, _ = self.add(diagnostic)
        report = ledger.report_markdown(self.sync())
        for expected in (ident, "offsets 0..0", "missing in archive", "Not collected by this window's binding allowlist",
                         "not collected on this backend", "Pending at serialization (aggregate)",
                         "affected shader identities are unknown", "not analyzed against all current evidence",
                         "no final pixel rejection or blend evidence"):
            self.assertIn(expected, report)

    def test_unrelated_weak_window_is_retained_without_inventing_candidate(self):
        diagnostic = compact()
        diagnostic["pairs"][0]["record"]["flags"] = 0
        ident, _ = self.add(diagnostic)
        result = self.sync()
        self.assertEqual(result["cases"], {})
        self.assertIn(ident, result["compact_windows"])
        self.assertIn("Compact CPU windows retained once by content ID: 1", ledger.report_markdown(result))


if __name__ == "__main__":
    unittest.main()
