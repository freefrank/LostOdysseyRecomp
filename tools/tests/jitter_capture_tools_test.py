#!/usr/bin/env python3
"""Focused offline candidate and reviewed fixture export checks."""

import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.capture_analysis import export_jitter_fixture, jitter_candidates, trace


DEPTH = "1111111111111111"
MATERIAL = "2222222222222222"
PIXEL = "3333333333333333"
VP = list(range(101, 117))


def reg(address, value):
    return f"{address:04x} {value:08x}\n"


class JitterCaptureToolsTest(unittest.TestCase):
    def make_capture(self, root):
        capture = root / "capture"
        frame = capture / "frame-01-f42"
        shaders = capture / "shaders"
        frame.mkdir(parents=True)
        shaders.mkdir()
        (capture / "capture-info.txt").write_text("status=complete\n")
        (frame / "temporal-scene.json").write_text(json.dumps({"vp_u32": VP}))
        (shaders / f"{MATERIAL}.hlsl").write_text(
            "xePV = XeConst(10);\nxePV = XeConst(9);\n"
            "xePV = XeConst(8);\nxePV = XeConst(7);\noPos.xyzw = xePV.xyzw;\n")
        mapping = root / "temporal_scene.h"
        mapping.write_text("inline int PositionVPSlot(uint64_t shader) { switch(shader) {\n"
                           f"case 0x{DEPTH}ull:return 4;\n"
                           "default:return -1; }}\n")
        state = {address: 0 for address in range(0x4000, 0x4400)}
        state.update({address: 0 for address in range(0x4400, 0x4440)})
        state.update({0x2000: 0x14000500, 0x2002: 0x10000, 0x2080: 0,
                      0x2081: 0, 0x2082: 0x02d00500, 0x2200: 0x700766,
                      0x2206: 0x43f, 0x2208: 5,
                      0x48be: 0x100, 0x48bf: 0x200})
        state.update({0x210f + i: i + 1 for i in range(6)})
        state.update({0x4000 + i: i + 10 for i in range(16)})
        state.update({0x4010 + i: value for i, value in enumerate(VP)})
        state.update({0x4000 + 230*4 + i: value for i, value in enumerate(VP)})
        lines = ["Frame 42\n", "draw 0 prim=4 indices=6 indexed=true base=0x80\n"]
        lines.extend(reg(address, value) for address, value in sorted(state.items()))
        lines.append(f"shaders vs={DEPTH} ps={'0'*16} ps_status=not_bound\n")
        lines.append("draw 1 prim=4 indices=6 indexed=true base=0x80\n")
        lines.extend(reg(0x401c + i, value) for i, value in enumerate(VP))
        lines.append(reg(0x2208, 4))
        lines.append(f"shaders vs={MATERIAL} ps={PIXEL} ps_status=bound\n")
        lines.append("draw 2 prim=8 indices=3 indexed=false base=0x0\n")
        lines.append("resolve f42_seq00.bin draw=2 address=0x100 width=2 height=2 plume_format=20 bpp=4 raw_ok=true\n")
        lines.append("draw 3 prim=4 indices=6 indexed=true base=0x80\n")
        lines.append(f"shaders vs={MATERIAL} ps={PIXEL} ps_status=bound\n")
        lines.append("end frame=42 submitted_draws=3\n")
        (frame / "render-state.txt").write_text("".join(lines))
        return capture, mapping

    def test_candidates_keep_stale_matches_unproven(self):
        with tempfile.TemporaryDirectory() as tmp:
            capture, mapping = self.make_capture(Path(tmp))
            result = jitter_candidates.analyze(capture, mapping, 42)
            frame = result["frames"][0]
            self.assertEqual((frame["before_draw"], frame["cutoff_source"]), (4, "full_frame"))
            hinted = [candidate for candidate in frame["candidates"] if candidate["position_chain_hint"]]
            self.assertEqual([(item["vs"], item["candidate_slot"]) for item in hinted], [(MATERIAL, 7)])
            self.assertEqual(hinted[0]["draws"][0]["matching_depth_draws"], [{"draw": 0, "vs": DEPTH}])
            self.assertTrue(any(candidate["candidate_slot"] == 230 and not candidate["position_chain_hint"]
                                for candidate in frame["candidates"]))
            later = jitter_candidates.analyze(capture, mapping, 42, before_draw=4)["frames"][0]
            self.assertEqual((later["before_draw"], later["cutoff_source"]), (4, "explicit"))
            self.assertEqual(len(next(item for item in later["candidates"] if item["candidate_slot"] == 7)["draws"]), 2)

    def test_late_light_geometry_survives_pass_changes_without_strict_pair(self):
        with tempfile.TemporaryDirectory() as tmp:
            capture, mapping = self.make_capture(Path(tmp))
            path = capture / "frame-01-f42" / "render-state.txt"
            text = path.read_text().replace("draw 3 prim=4 indices=6 indexed=true base=0x80\n",
                "draw 3 prim=4 indices=6 indexed=true base=0x80\n" + reg(0x2200, 0x00700263) +
                reg(0x2201, 0x01000101) + reg(0x2081, 0x00100010))
            path.write_text(text)
            frame = jitter_candidates.analyze(capture, mapping, 42)["frames"][0]
            late = next(c for c in frame["candidates"] if c["candidate_slot"] == 7)["draws"][1]
            self.assertEqual(late["matching_depth_draws"], [])
            self.assertEqual(late["matching_geometry_depth_draws"][0]["draw"], 0)
            self.assertIn("0x2200", late["matching_geometry_depth_draws"][0]["pass_differences"])
            with trace.load_capture(capture) as opened:
                with self.assertRaises(ValueError):
                    export_jitter_fixture.collect(opened, "frame-01-f42", [(3, MATERIAL, 7)], DEPTH, 4, before_draw=4)
            # Same registers with different geometry must not receive this association.
            path.write_text(text.replace("draw 3 prim=4 indices=6 indexed=true base=0x80",
                                         "draw 3 prim=4 indices=6 indexed=true base=0x90"))
            frame = jitter_candidates.analyze(capture, mapping, 42)["frames"][0]
            late = next(c for c in frame["candidates"] if c["candidate_slot"] == 7)["draws"][1]
            self.assertEqual(late["matching_geometry_depth_draws"], [])

    def test_default_scan_does_not_require_a_resolve(self):
        with tempfile.TemporaryDirectory() as tmp:
            capture, mapping = self.make_capture(Path(tmp))
            path = capture / "frame-01-f42" / "render-state.txt"
            path.write_text("\n".join(line for line in path.read_text().splitlines() if not line.startswith("resolve ")))
            frame = jitter_candidates.analyze(capture, mapping, 42)["frames"][0]
            self.assertEqual(frame["cutoff_source"], "full_frame")
            self.assertEqual(len(next(c for c in frame["candidates"] if c["candidate_slot"] == 7)["draws"]), 2)

    def test_fixture_needs_explicit_identity_and_exact_pair(self):
        with tempfile.TemporaryDirectory() as tmp:
            capture, _ = self.make_capture(Path(tmp))
            with trace.load_capture(capture) as opened:
                rows = export_jitter_fixture.collect(opened, "frame-01-f42", [(1, MATERIAL, 7)], DEPTH, 4)
                self.assertEqual((rows[0]["draw"], rows[0]["depth_draw"]), (1, 0))
                self.assertEqual(rows[0]["vertex"][28:44], tuple(VP))
                self.assertIn("0x2222222222222222ull", export_jitter_fixture.render_header(rows, "fixture_42", "f42"))
                with self.assertRaisesRegex(ValueError, "identity differs"):
                    export_jitter_fixture.collect(opened, "frame-01-f42", [(1, DEPTH, 7)], DEPTH, 4)
                with self.assertRaisesRegex(ValueError, "at/after draw boundary"):
                    export_jitter_fixture.collect(opened, "frame-01-f42", [(2, MATERIAL, 7)], DEPTH, 4)
                self.assertEqual(export_jitter_fixture.collect(opened, "frame-01-f42",
                    [(3, MATERIAL, 7)], DEPTH, 4, before_draw=4)[0]["draw"], 3)

    def test_missing_fetch_is_recorded_and_cannot_pair(self):
        with tempfile.TemporaryDirectory() as tmp:
            capture, mapping = self.make_capture(Path(tmp))
            path = capture / "frame-01-f42" / "render-state.txt"
            path.write_text(path.read_text().replace("48be 00000100\n", ""))
            frame = jitter_candidates.analyze(capture, mapping, 42)["frames"][0]
            hinted = next(item for item in frame["candidates"] if item["position_chain_hint"])
            self.assertEqual(hinted["draws"][0]["matching_depth_draws"], [])
            self.assertIn("0x48be", frame["incomplete_depth_draws"][0]["missing_evidence"])
            self.assertIn("0x48be", hinted["draws"][0]["missing_evidence"])
            with trace.load_capture(capture) as opened:
                with self.assertRaisesRegex(ValueError, "missing exact pair evidence"):
                    export_jitter_fixture.collect(opened, "frame-01-f42", [(1, MATERIAL, 7)], DEPTH, 4)

    def test_export_rejects_missing_header_invalid_ps_and_unsupported_slots(self):
        with tempfile.TemporaryDirectory() as tmp:
            capture, mapping = self.make_capture(Path(tmp))
            with trace.load_capture(capture) as opened:
                with self.assertRaisesRegex(ValueError, "material slots 0..12"):
                    export_jitter_fixture.collect(opened, "frame-01-f42", [(1, MATERIAL, 13)], DEPTH, 4)
                with self.assertRaisesRegex(ValueError, "depth slots 0..4"):
                    export_jitter_fixture.collect(opened, "frame-01-f42", [(1, MATERIAL, 7)], DEPTH, 5)
            path = capture / "frame-01-f42" / "render-state.txt"
            text = path.read_text()
            path.write_text(text.replace("draw 1 prim=4 indices=6 indexed=true base=0x80",
                                         "draw 1 prim=4 indices=6 indexed=true"))
            frame = jitter_candidates.analyze(capture, mapping, 42)["frames"][0]
            hinted = next(item for item in frame["candidates"] if item["position_chain_hint"])
            self.assertEqual(hinted["draws"][0]["matching_depth_draws"], [])
            self.assertIn("base", hinted["draws"][0]["missing_evidence"])
            with trace.load_capture(capture) as opened:
                with self.assertRaisesRegex(ValueError, "missing exact pair evidence"):
                    export_jitter_fixture.collect(opened, "frame-01-f42", [(1, MATERIAL, 7)], DEPTH, 4)
            path.write_text(text.replace(f"ps={PIXEL} ps_status=bound", "ps=0000000000000000 ps_status=bound"))
            with trace.load_capture(capture) as opened:
                with self.assertRaisesRegex(ValueError, "no valid bound material PS"):
                    export_jitter_fixture.collect(opened, "frame-01-f42", [(1, MATERIAL, 7)], DEPTH, 4)


if __name__ == "__main__":
    unittest.main()
