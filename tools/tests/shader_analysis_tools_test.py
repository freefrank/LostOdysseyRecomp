"""Small synthetic, offline fixtures for tools/shader_analysis (no game data)."""

import importlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools/shader_analysis"
MODULES = ("collect_sources", "audit_vs", "audit_ps", "inspect_spirv_position", "cpx")


class ShaderAnalysisToolsTest(unittest.TestCase):
    def test_vs_current_constant_calls_and_legacy_array_agree(self):
        from tools.shader_analysis import audit_vs

        lines = ["r0.x = float(xeVertexId);", "r2 = XeVF_pos(r0);"]
        for slot in (0, 7):
            for offset, component in enumerate("wzyx"):
                tail = "" if offset == 0 else " + r0.xyzw"
                lines += [f"xePV.xyzw = r2.{component * 4} * XeConst({slot + 3 - offset}).xyzw{tail};",
                          "r0.xyzw = xePV.xyzw;"]
            lines.append("r2.xyzw = r0.xyzw;")
        lines += ["oPos.xyzw = r2.xyzw;", "if ((xeFlags & 8u) != 0) {}"]
        current = "\n".join(lines)
        legacy = audit_vs.normalize_constants(current)
        for source in (current, legacy):
            result = audit_vs.analyze(source)
            self.assertEqual(result["classification"], "matrix_position_candidate")
            self.assertEqual(result["slots"], [7])
            self.assertEqual(result["constants"], [0, 1, 2, 3, 7, 8, 9, 10])
            self.assertFalse(result["relative_position_constants"])
        self.assertTrue(any("XeConst(7)" in line["text"] for line in audit_vs.analyze(current)["slice"]))

    def test_vs_dynamic_constants_are_not_direct_position(self):
        from tools.shader_analysis import audit_vs

        for expression in ("XeConst(a0 + 7)", "c[a0 + 7]", "c[7 + a0]"):
            source = ("r0.x = float(xeVertexId);\nr1 = XeVF_pos(r0);\n"
                      f"r2 = r1 * {expression};\noPos = r2;\n")
            result = audit_vs.analyze(source)
            self.assertEqual(result["classification"], "unresolved_dynamic_constants")
            self.assertTrue(result["relative_position_constants"])

    def test_ps_current_constant_calls_reach_sample_coordinates(self):
        from tools.shader_analysis import audit_ps

        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "ps_projected.hlsl"
            for expression, dynamic in (("XeConst(7)", False), ("c[7]", False),
                                        ("XeConst(a0 + 7)", True)):
                path.write_text("void main(in float4 i4 : TEXCOORD4, out float4 oC0 : SV_Target) {\n"
                                "r0 = i4;\nr1 = rcp(r0.wwww);\n"
                                f"r2 = r0 * r1 * {expression};\n"
                                "r3 = XeTex2D(tex2D_0, XeSampler(0u), r2.xy);\noC0 = r3;\n}\n")
                result = audit_ps.analyze(path)
                self.assertEqual(result["dynamic_constants"], dynamic)
                dependencies = result["fetches"][0]["coordinate_dependencies"]
                self.assertIn("i4.w", dependencies)
                if not dynamic:
                    self.assertIn("c7", dependencies)

    def test_help_and_import_do_not_write(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            for name in MODULES:
                command = [sys.executable, str(TOOLS / f"{name}.py"), "--help"]
                self.assertEqual(subprocess.run(command, cwd=directory, capture_output=True).returncode, 0)
                importlib.import_module(f"tools.shader_analysis.{name}")
            self.assertEqual(list(directory.iterdir()), [])

    def test_collect_provenance_and_reject_existing_output(self):
        from tools.shader_analysis import collect_sources

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            name = f"vs_{collect_sources.fnv(bytes(12)):016x}.bin"
            for label in ("one", "two"):
                (root / label).mkdir()
                (root / label / name).write_bytes(bytes(12))
            output = root / "collected"
            self.assertEqual(collect_sources.main(["--source", f"first={root/'one'}",
                                                  "--source", f"second={root/'two'}",
                                                  "--output", str(output)]), 0)
            self.assertEqual((output / "source" / name).read_bytes(), bytes(12))
            report = json.loads((output / "provenance.json").read_text())
            self.assertEqual(report["summary"]["unique_sources"], 1)
            self.assertEqual([origin["label"] for origin in report["sources"][name]], ["first", "second"])
            with self.assertRaises(SystemExit) as failure:
                collect_sources.main(["--source", f"first={root/'one'}", "--output", str(output)])
            self.assertEqual(failure.exception.code, 1)

    def test_vs_ps_static_reports(self):
        from tools.shader_analysis import audit_ps, audit_vs

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            vs = root / "vs_a.hlsl"
            vs.write_text("void main() {\n  r0.x = float(xeVertexId);\n  r1 = XeVF_pos(r0);\n  oPos = r1;\n  if ((xeFlags & 8u) != 0) {}\n}\n")
            ps = root / "ps_a.hlsl"
            ps.write_text("void main(in float4 i0 : TEXCOORD0, out float4 oC0 : SV_Target) {\n"
                          "  r0 = i0;\n"
                          "  r1 = XeTex2D(tex2D_2, XeSampler(0u), r0.xy);\n"
                          "  r2 = rcp(r1);\n"
                          "  r3 = XeTex2D(tex2D_3, XeSampler(1u), r2.xy);\n"
                          "  oDepthVec = r3;\n}\n")
            self.assertEqual(audit_vs.main(["--hlsl", str(vs), "--output", str(root / "vs.json")]), 0)
            self.assertEqual(audit_ps.main(["--hlsl", str(ps), "--output", str(root / "ps.json")]), 0)
            self.assertEqual(json.loads((root / "vs.json").read_text())["shaders"][0]["classification"],
                             "direct_vertex_position")
            row = json.loads((root / "ps.json").read_text())["shaders"][0]
            self.assertEqual(row["texture_slots"], [2, 3])
            self.assertTrue(row["candidate_inverse_sample_projection"])
            self.assertTrue(row["depth_outputs"])

    def test_spirv_position_and_cpx_raw_block(self):
        from tools.shader_analysis import cpx, inspect_spirv_position

        grammar = {71: {"opname": "OpDecorate", "operands": []},
                   129: {"opname": "OpFAdd", "operands": [{"kind": "IdResultType"},
                                                          {"kind": "IdResult"}, {"kind": "IdRef"}, {"kind": "IdRef"}]},
                   62: {"opname": "OpStore", "operands": [{"kind": "IdRef"}, {"kind": "IdRef"}]}}
        words = [0x07230203, 0x10000, 0, 20, 0, 4 << 16 | 71, 4, 11, 0,
                 5 << 16 | 129, 1, 9, 5, 6, 3 << 16 | 62, 4, 9]
        raw = struct.pack(f"<{len(words)}I", *words)
        result = inspect_spirv_position.analyze(b"HEADER00" + raw, grammar)
        self.assertEqual(result["offset"], 8)
        self.assertEqual(result["positionArithmetic"][0]["op"], "OpFAdd")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "test.spv").write_bytes(raw)
            (root / "grammar.json").write_text(json.dumps({"instructions": [
                {"opcode": opcode, **entry} for opcode, entry in grammar.items()]}))
            self.assertEqual(inspect_spirv_position.main(["--spirv", str(root / "test.spv"),
                                                           "--grammar", str(root / "grammar.json"),
                                                           "--output", str(root / "report.json")]), 0)
            self.assertEqual(json.loads((root / "report.json").read_text())["shaders"][0]["position"], [4])
            block = b"\xff\0\x02\0abc"
            packed = b"cpx\0" + struct.pack("<III", 1 << 16, 20 + len(block), 3) + struct.pack("<I", 20) + block
            self.assertEqual(cpx.decode(packed), b"abc")
            (root / "input.cpx").write_bytes(packed)
            self.assertEqual(cpx.main(["--input", str(root / "input.cpx"),
                                       "--output", str(root / "decoded.bin")]), 0)
            self.assertEqual((root / "decoded.bin").read_bytes(), b"abc")
            with self.assertRaises(ValueError):
                cpx.decode(packed[:-1])


if __name__ == "__main__":
    unittest.main()
