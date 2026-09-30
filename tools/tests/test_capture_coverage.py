"""Small synthetic multi-capture coverage checks; no game or real payloads."""
import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.capture_analysis import coverage, jitter_candidates

DEPTH = "1111111111111111"
VS = "2222222222222222"
PS = "3333333333333333"
VP = list(range(101, 117))
DROPS = "drops mode=0 shader=0 pitch=0 pipeline=0 upload=0 index=0 scissor=0"


class CoverageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.mapping = self.root / "mapping.h"
        self.mapping.write_text("inline int PositionVPSlot(uint64_t h) { switch(h) {"
                                f"case 0x{DEPTH}ull:return 4;default:return -1;" + "}}")

    def capture(self, name, frame_ids=(42,), camera=True):
        root = self.root / name
        root.mkdir()
        (root / "capture-info.txt").write_text(
            f"status=complete\nrequested_frames={len(frame_ids)}\ncompleted_frames={len(frame_ids)}\nSource version: synthetic\n")
        (root / "shaders").mkdir()
        (root / "shaders" / f"{VS}.hlsl").write_text(
            "\n".join(f"xePV = XeConst({i});" for i in range(7, 11)) + "\noPos.xyzw = xePV.xyzw;\n")
        state = {address: 0 for address in (*range(0x4000, 0x4010), *jitter_candidates.PAIR_REGS)}
        state.update({0x48be: 1, 0x48bf: 2, 0x2200: 1, 0x2208: 5})
        state.update({0x4010 + i: v for i, v in enumerate(VP)})
        for number in frame_ids:
            frame = root / f"frame-01-f{number}"
            frame.mkdir()
            if camera:
                (frame / "temporal-scene.json").write_text(json.dumps({"vp_u32": VP}))
            lines = [f"Frame {number}", "Source version: synthetic", "draw 0 indices=6 indexed=true base=0x80"]
            lines += [f"{a:04x} {v:08x}" for a, v in state.items()]
            lines += [f"shaders vs={DEPTH} ps={'0'*16} ps_status=not_bound",
                      "draw 1 indices=6 indexed=true base=0x80", "2208 00000004"]
            lines += [f"{0x401c+i:04x} {v:08x}" for i, v in enumerate(VP)]
            lines += [f"shaders vs={VS} ps={PS} ps_status=bound",
                      "resolve color.bin draw=2", "draw 3 indices=6 indexed=true base=0x80",
                      f"shaders vs={VS} ps={PS} ps_status=bound",
                      f"end frame={number} submitted_draws=3", DROPS]
            (frame / "render-state.txt").write_text("\n".join(lines) + "\n")
        return root

    def test_cross_capture_dedup_counts_all_frames_and_windows_zip(self):
        first = self.capture("first", (42, 43))
        second = self.capture("second")
        zipped = self.root / "second.zip"
        with zipfile.ZipFile(zipped, "w") as archive:
            for file in second.rglob("*"):
                if file.is_file():
                    archive.write(file, "enclosing\\" + str(file.relative_to(second)).replace("/", "\\"))
        result = coverage.analyze([first, zipped, first], self.mapping)
        self.assertEqual(result["summary"]["capture_count"], 2)
        entry, = result["unmapped_shaders"]
        self.assertEqual((entry["capture_count"], entry["frame_count"], entry["draw_count"]), (2, 3, 6))
        self.assertIn(7, entry["candidate_slots"])
        self.assertEqual(entry["before_resolve_position_and_depth_draw_count"], 3)
        self.assertEqual(entry["strict_depth_paired_draw_count"], 6)
        self.assertEqual(entry["stage_counts"]["at_or_after_first_resolve"], 3)
        self.assertTrue(all(frame["before_draw"] == 4 for cap in result["captures"] for frame in cap["frames"]))

    def test_missing_camera_keeps_unknown_and_missing_resolve_stage(self):
        root = self.capture("unknown", camera=False)
        trace = root / "frame-01-f42/render-state.txt"
        trace.write_text(trace.read_text().replace("resolve color.bin draw=2\n", ""))
        result = coverage.analyze([root], self.mapping)
        entry, = result["unmapped_shaders"]
        self.assertEqual(entry["candidate_slots"], [])
        self.assertEqual(entry["unknown_or_unmatched_draw_count"], 2)
        self.assertTrue(all(o["status"] == "camera_unknown" for o in entry["occurrences"]))
        self.assertEqual(entry["stage_counts"]["resolve_boundary_unknown"], 2)
        self.assertEqual(result["summary"]["coverage"], "insufficient_metadata")

    def test_frame_and_capture_errors_are_isolated_and_unknowns_retained(self):
        root = self.capture("partly-bad", (42, 43))
        (root / "frame-01-f42/temporal-scene.json").write_text('{"vp_u32": [1]}')
        (root / "frame-01-f44").mkdir()
        (root / "frame-01-f44/unused.bin").write_bytes(b"not a trace")
        result = coverage.analyze([self.root / "absent.zip", root], self.mapping)
        self.assertEqual(result["summary"]["captures_with_errors"], 1)
        self.assertEqual(result["summary"]["frames_with_errors"], 2)
        entry, = result["unmapped_shaders"]
        self.assertEqual(entry["draw_count"], 4)
        self.assertEqual(sum(o["status"] == "analysis_error" for o in entry["occurrences"]), 2)
        self.assertIn(7, entry["candidate_slots"])

    def test_late_only_evidence_is_not_early_scene_confirmation(self):
        root = self.capture("late")
        file = root / "frame-01-f42/render-state.txt"
        file.write_text(file.read_text().replace("resolve color.bin draw=2", "resolve color.bin draw=1"))
        entry, = coverage.analyze([root], self.mapping)["unmapped_shaders"]
        self.assertEqual(entry["before_resolve_position_and_depth_draw_count"], 0)
        self.assertEqual(entry["stage_counts"]["before_first_resolve"], 0)
        self.assertEqual(entry["stage_counts"]["at_or_after_first_resolve"], 2)

    def test_new_output_and_all_input_boundaries_are_protected(self):
        first, second = self.capture("one"), self.capture("two")
        def run(output):
            return coverage.main(["--input", str(first), "--input", str(second),
                                  "--mapping", str(self.mapping), "--output", str(output)])
        existing = self.root / "existing.json"
        existing.write_text("preserve")
        for output in (existing, second / "new.json"):
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                run(output)
        self.assertEqual(existing.read_text(), "preserve")
        self.assertFalse((second / "new.json").exists())
        output = self.root / "result.json"
        with contextlib.redirect_stdout(io.StringIO()) as stdout:
            run(output)
        self.assertEqual(json.loads(stdout.getvalue())["unique_unmapped_vs"], 1)
        self.assertEqual(json.loads(output.read_text())["summary"]["capture_count"], 2)

    def test_dummy_bindings_and_resolve_copy_are_not_missing_draw_evidence(self):
        root = self.capture("resolve-counters")
        file = root / "frame-01-f42/render-state.txt"
        value = file.read_text().replace("end frame=42", "draw 4 prim=8 indices=3 indexed=false base=0x0\n"
            "2208 0000000e\nresolve copy.bin draw=5\n"
            "draw 5 prim=8 indices=3 indexed=false base=0x0\nresolve copy2.bin draw=6\nend frame=42")
        value = value.replace(DROPS, DROPS + " dummy_bindings=143")
        file.write_text(value)
        result = coverage.analyze([root], self.mapping)
        frame = result["captures"][0]["frames"][0]
        self.assertEqual(result["summary"]["coverage"], "metadata_scanned")
        self.assertEqual(result["summary"]["non_shader_command_count"], 2)
        self.assertEqual(result["summary"]["unknown_execution_draw_count"], 0)
        self.assertEqual(frame["missing_shader_draws"], [])
        self.assertEqual(frame["diagnostic_counters"], {"dummy_bindings": "143"})
        self.assertEqual([r["draw"] for r in frame["non_shader_commands"]], [4, 5])

    def test_graphics_without_shader_and_real_drops_stay_unknown(self):
        root = self.capture("graphics-missing")
        file = root / "frame-01-f42/render-state.txt"
        value = file.read_text().replace("end frame=42", "draw 4 indices=3 indexed=false base=0x0\n"
            "2208 00000004\nresolve diagnostic.bin draw=5\nend frame=42")
        value = value.replace(DROPS, "drops mode=0 shader=1 dummy_bindings=143")
        file.write_text(value)
        result = coverage.analyze([root], self.mapping)
        frame = result["captures"][0]["frames"][0]
        self.assertEqual(result["summary"]["unknown_execution_draw_count"], 1)
        self.assertEqual(result["summary"]["non_shader_command_count"], 0)
        self.assertIn("missing_shader_identity", frame["gaps"])
        self.assertIn("trace_drops_or_unknown_drop_count", frame["gaps"])
        self.assertEqual(result["summary"]["coverage"], "insufficient_metadata")

    def test_missing_frame_and_nonconsecutive_range_remain_incomplete(self):
        root = self.capture("missing-frame")
        (root / "capture-info.txt").write_text("status=complete\nrequested_frames=2\ncompleted_frames=2\n")
        result = coverage.analyze([root], self.mapping)
        self.assertIn("capture_frame_directories_missing_or_extra", result["captures"][0]["gaps"])
        self.assertEqual(result["summary"]["decision"], "insufficient_evidence")
        info = dict(requested_frames="2", completed_frames="2", first_frame="42", last_attempted_frame="44")
        self.assertIn("consecutive_frame_range_mismatch", coverage.capture_count_gaps(
            info, ["frame-01-f42", "frame-02-f44"], consecutive=True))
        self.assertEqual(coverage.capture_count_gaps(info, ["frame-01-f42", "frame-02-f44"]), [])
        self.assertIn("capture_frame_counts_unknown", coverage.capture_count_gaps({}, []))

    def test_missing_drop_fields_and_broken_empty_input_are_not_success(self):
        root = self.capture("missing-drops")
        file = root / "frame-01-f42/render-state.txt"
        file.write_text(file.read_text().replace(DROPS, "drops mode=0"))
        result = coverage.analyze([root], self.mapping)
        self.assertIn("drop_counters_incomplete", result["captures"][0]["frames"][0]["gaps"])
        self.assertEqual(result["summary"]["decision"], "insufficient_evidence")
        broken = coverage.analyze([self.root / "missing.zip"], self.mapping)
        self.assertEqual(broken["unmapped_shaders"], [])
        self.assertEqual(broken["summary"]["decision"], "insufficient_evidence")


if __name__ == "__main__":
    unittest.main()
