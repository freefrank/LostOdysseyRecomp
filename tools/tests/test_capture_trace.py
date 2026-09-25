"""Focused tests for offline F1 trace reading and comparison."""

import json
from pathlib import Path
import sys
import tempfile
import unittest
import zipfile


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "capture_analysis"))
import compare_traces
import trace


FRAME_A = """Frame 12
draw 0 prim=4 indices=3 indexed=true base=0x10 extra=kept
0001 00000001
0002 00000002
shaders vs=aaaa ps=bbbb ps_status=bound
draw 1 prim=4 indices=6 indexed=true base=0x20
0002 00000003
shaders vs=cccc ps=dddd ps_status=bound
draw 2 prim=4 indices=6 indexed=true base=0x20
0001 00000004
end frame=12 submitted_draws=2
"""

FRAME_B = """Frame 13
draw 0 prim=4 indices=3 indexed=true base=0x10 extra=kept
0001 00000001
0002 00000002
shaders vs=aaaa ps=bbbb ps_status=bound
draw 1 prim=4 indices=9 indexed=true base=0x20
0002 00000004
shaders vs=eeee ps=dddd ps_status=bound
draw 2 prim=4 indices=6 indexed=true base=0x20
0001 00000004
0002 00000003
shaders vs=cccc ps=dddd ps_status=bound
drops mode=0
"""


class TraceTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.archive = self.root / "capture.zip"
        with zipfile.ZipFile(self.archive, "w") as z:
            z.writestr("wrapped\\capture-info.txt", "F1 capture\n")
            z.writestr("wrapped\\frame-01-f12\\render-state.txt", FRAME_A)
            z.writestr("wrapped\\frame-02-f13\\render-state.txt", FRAME_B)

    def test_backslash_zip_and_cumulative_deltas(self):
        with trace.load_capture(self.archive) as capture:
            self.assertEqual(capture.frame_names(), ["frame-01-f12", "frame-02-f13"])
            self.assertEqual(capture.read_bytes("capture-info.txt"), b"F1 capture\n")
            frame = trace.parse_frame(capture, "frame-01-f12")
        snapshots = [dict(state) for _, state in trace.iter_draw_states(frame)]
        self.assertEqual(snapshots, [{1: 1, 2: 2}, {1: 1, 2: 3}, {1: 4, 2: 3}])
        self.assertEqual(frame["draws"][0]["header"]["extra"], "kept")
        self.assertIsNone(frame["draws"][2]["shader"])
        self.assertEqual(frame["end"]["submitted_draws"], "2")

    def test_comparison_preserves_unknown_and_counts(self):
        report = compare_traces.compare_capture(self.archive, ["12", "13"])
        pair = report["comparisons"][0]
        self.assertEqual(pair["draw_counts"], {"first": 3, "second": 3, "paired": 3})
        self.assertEqual([x["ordinal"] for x in pair["header_changes"]], [1])
        self.assertEqual([x["ordinal"] for x in pair["shader_changes"]], [1, 2])
        self.assertEqual(pair["shader_changes"][1]["first"], None)
        self.assertEqual(pair["register_change_counts"], {"0002": 1})
        self.assertIsNone(pair["metadata"]["second"]["end"])
        self.assertEqual(pair["metadata"]["first"]["missing_shader_draws"], [2])

    def test_actual_upload_evidence_is_distinct_from_guest_registers(self):
        text = FRAME_A.replace("shaders vs=aaaa ps=bbbb ps_status=bound\n",
            "shaders vs=aaaa ps=bbbb ps_status=bound\n"
            "jitter applied=true slot=7 rejection=0 phase=3 ndc_x=38000000 ndc_y=00000000\n"
            "jitter_uploaded_vp " + " ".join(f"{i:08x}" for i in range(16)) + "\n"
            "texture_binding slot=0 bank=0 kind=3 guest_width=1 guest_height=1 host_width=1 host_height=1\n")
        capture = trace.Capture(["frame-01-f12/render-state.txt"], lambda _: text.encode())
        frame = trace.parse_frame(capture, "frame-01-f12")
        first = frame["draws"][0]
        self.assertEqual(first["runtime_jitter"]["applied"], "true")
        self.assertEqual(first["uploaded_vp"], list(range(16)))
        self.assertEqual(first["texture_bindings"][0]["kind"], "3")
        self.assertEqual(first["register_changes"], {1: 1, 2: 2})
        self.assertNotIn("runtime_jitter", frame["draws"][1])
        self.assertNotIn("texture_bindings", frame["draws"][1])
        bad = trace.Capture(["frame-01-f12/render-state.txt"],
            lambda _: text.replace("jitter_uploaded_vp 00000000", "jitter_uploaded_vp").encode())
        with self.assertRaisesRegex(ValueError, "invalid uploaded VP"):
            trace.parse_frame(bad, "frame-01-f12")

    def test_new_output_guard_and_frame_boundary(self):
        capture_dir = self.root / "extracted"
        capture_dir.mkdir()
        (capture_dir / "capture-info.txt").write_text("capture\n", encoding="utf-8")
        frame_dir = capture_dir / "frame-01-f12"
        frame_dir.mkdir()
        (frame_dir / "render-state.txt").write_text(FRAME_A, encoding="utf-8")
        with trace.load_capture(capture_dir) as capture:
            self.assertEqual(len(trace.parse_frame(capture, "frame-01-f12")["draws"]), 3)
        with self.assertRaisesRegex(ValueError, "outside"):
            trace.check_new_output(capture_dir, capture_dir / "report.json")
        output = self.root / "report.json"
        output.write_text(json.dumps({}), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "new file"):
            trace.check_new_output(self.archive, output)
        with trace.load_capture(self.archive) as capture:
            with self.assertRaisesRegex(ValueError, "not found"):
                trace.select_frames(capture, ["14"])
            self.assertEqual(trace.select_frames(capture, ["13"]), ["frame-02-f13"])


if __name__ == "__main__":
    unittest.main()
