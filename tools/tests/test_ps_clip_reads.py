"""Focused offline tests for explicit copied-clip PS review."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from tools.shader_analysis import audit_ps


SCRIPT = Path(__file__).resolve().parents[1] / "shader_analysis" / "audit_ps.py"


def shader(instructions):
    return ("void main(in float4 i4 : TEXCOORD4, out float4 oC0 : SV_Target)\n"
            "{\n  float4 r4 = 0.0;\n  float4 r0 = 0.0;\n  float4 r1 = 0.0;\n"
            "  float4 r2 = 0.0;\n  float4 r3 = 0.0;\n" + instructions + "\n}\n")


class PsClipReadsTest(unittest.TestCase):
    def test_w_only_preserves_input_components(self):
        review = audit_ps.analyze_clip_reads(shader("  r4 = i4;\n  r0.w = max(r4.w, r4.w);\n  oC0 = r0;"), [4])
        self.assertTrue(review["candidate_no_clip_xy_reads"])
        self.assertFalse(review["clip_xy_reads"])
        self.assertEqual(review["clip_w_reads"][0]["inputs"], ["i4.w"])
        self.assertFalse(review["unsupported"])

    def test_clip_xy_and_w_reach_texture_coordinate(self):
        source = shader("  r4 = i4;\n  r1.x = rcp(r4.w);\n"
                        "  r2.xy = r4.xy * r1.xx;\n"
                        "  r3 = XeTex2D(tex2D_0, XeSampler(0u), r2.xy);\n  oC0 = r3;")
        review = audit_ps.analyze_clip_reads(source, [4])
        self.assertFalse(review["candidate_no_clip_xy_reads"])
        sample = next(row for row in review["clip_xy_reads"] if "XeTex2D" in row["text"])
        self.assertIn("i4.x", sample["inputs"])
        self.assertIn("i4.y", sample["inputs"])
        self.assertTrue(any("i4.w" in row["inputs"] for row in review["clip_w_reads"]))
        direct = audit_ps.analyze_clip_reads(shader("  oC0 = i4;"), [4])
        self.assertFalse(direct["candidate_no_clip_xy_reads"])
        self.assertEqual(direct["clip_xy_reads"][0]["inputs"], ["i4.x", "i4.y"])

    def test_overwritten_xyz_does_not_taint_later_w_read(self):
        source = shader("  r4 = i4;\n  r4.xyz = float3(0.0, 0.0, 0.0);\n"
                        "  r0.x = r4.w;\n  oC0 = r0;")
        review = audit_ps.analyze_clip_reads(source, [4])
        self.assertTrue(review["candidate_no_clip_xy_reads"])
        self.assertEqual(review["clip_xy_reads"], [])
        self.assertTrue(review["clip_w_reads"])

    def test_unknown_operation_and_control_flow_reject_candidate(self):
        for instructions, reason in (
            ("  r4 = i4;\n  r4 += float4(1.0, 1.0, 1.0, 1.0);\n  oC0 = r4.w;", "unparsed"),
            ("  r4 = i4;\n  if (r4.w > 0.0) r0.x = 1.0;\n  oC0 = r0;", "control flow"),
            ("  r4 = i4;\n  r0.x = mystery(r4.w);\n  oC0 = r0;", "unknown call"),
            ("  r4 = i4;\n  r0.x = r4.w ^ 1;\n  oC0 = r0;", "unknown expression operator"),
            ("  r4 = i4;\n  mystery();\n  oC0 = r4.w;", "unparsed assignment or operation"),
        ):
            with self.subTest(reason=reason):
                review = audit_ps.analyze_clip_reads(shader(instructions), [4])
                self.assertFalse(review["candidate_no_clip_xy_reads"])
                self.assertTrue(any(reason in item["reason"] for item in review["unsupported"]))

    def test_cli_from_other_cwd_is_opt_in_and_validates_index(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "ps_clip.hlsl"
            source.write_text(shader("  r4 = i4;\n  r0.x = r4.w;\n  oC0 = r0;"))
            output = root / "clip.json"
            command = [sys.executable, str(SCRIPT), "--hlsl", str(source),
                       "--output", str(output), "--clip-input", "4"]
            completed = subprocess.run(command, cwd=root, capture_output=True, text=True)
            self.assertEqual(completed.returncode, 0, completed.stderr)
            row = json.loads(output.read_text())["shaders"][0]
            self.assertTrue(row["clip_input_review"]["candidate_no_clip_xy_reads"])
            self.assertEqual(row["clip_input_review"]["clip_inputs"], [4])
            self.assertNotIn("clip_input_review", audit_ps.analyze(source))
            bad = subprocess.run(command[:-1] + ["16"], cwd=root, capture_output=True, text=True)
            self.assertNotEqual(bad.returncode, 0)
            self.assertIn("0..15", bad.stderr)


if __name__ == "__main__":
    unittest.main()
