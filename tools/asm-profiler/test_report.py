import importlib.util
from pathlib import Path
import tempfile
import unittest

from capstone import Cs, CS_ARCH_X86, CS_MODE_64

spec = importlib.util.spec_from_file_location("report", Path(__file__).with_name("report.py"))
report = importlib.util.module_from_spec(spec)
spec.loader.exec_module(report)


class ReportTests(unittest.TestCase):
    def sample(self, **kwargs):
        return dict(dict(tid=1, ip="0x1010", module_base="0x1000", module="game.exe",
                         symbol="sub_82290008", displacement=16, source="", line=0,
                         bytes="4883c001c3"), **kwargs)

    def analyze(self, samples, **kwargs):
        return report.analyze(dict(schema_version=1, samples=samples), Cs(CS_ARCH_X86, CS_MODE_64), **kwargs)

    def test_hotspots_and_decode(self):
        result = self.analyze([self.sample(), self.sample(), self.sample(tid=2, ip="0x1020")])
        self.assertEqual(result["hotspots"][0]["count"], 2)
        self.assertEqual(result["hotspots"][0]["rva"], "0x10")
        self.assertEqual(result["hotspots"][0]["assembly"][0], "0x1010: add rax, 1")
        self.assertEqual(result["functions"][0]["count"], 3)
        self.assertAlmostEqual(sum(r["percent"] for r in result["hotspots"]), 100)

    def test_thread_filter(self):
        result = self.analyze([self.sample(), self.sample(tid=2)], tid=2)
        self.assertEqual(result["selected_samples"], 1)
        self.assertEqual(result["total_samples"], 2)
        self.assertEqual(result["hotspots"][0]["percent"], 100)

    def test_unknown_and_empty(self):
        result = self.analyze([self.sample(symbol="", module="", bytes="")])
        self.assertEqual(result["hotspots"][0]["symbol"], "<unresolved>")
        self.assertEqual(result["hotspots"][0]["assembly"], [])
        self.assertEqual(self.analyze([])["hotspots"], [])
        self.assertEqual(self.analyze([self.sample()], tid=99)["functions"], [])

    def test_changed_code_and_module_bases_stay_distinct(self):
        result = self.analyze([self.sample(), self.sample(bytes="90"), self.sample(module_base="0x900")])
        self.assertEqual(len(result["hotspots"]), 3)

    def test_html_escapes_symbols(self):
        result = self.analyze([self.sample(symbol="<script>alert(1)</script>")])
        output = report.render(result, dict(samples=[], executable="<img>"), 20)
        self.assertNotIn("<script>alert", output)
        self.assertIn("&lt;script&gt;", output)
        self.assertIn("&lt;img&gt;", output)

    def test_thread_cpu_table(self):
        result = self.analyze([self.sample(tid=17)])
        output = report.render(result, dict(thread_cpu_times=[dict(tid=17, observed_cpu_seconds=0.125)]), 20)
        self.assertIn("<td>17</td><td>1</td><td>0.125</td>", output)
        self.assertIn("do not attribute CPU time", output)

    def test_guest_context_bounded_and_labelled(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "ppc_recomp.0.cpp").write_text("PPC_FUNC_IMPL(f) {\n // addi r3,r3,1\n ctx.r3 += 1;\n}\nPPC_FUNC_IMPL(g) {\n", encoding="utf-8")
            self.assertIn("addi r3", report.guest_context("C:\\build\\ppc_recomp.0.cpp", 3, root))
            self.assertIn("unverified", report.guest_context("ppc_recomp.0.cpp", 3, root))
            self.assertEqual(report.guest_context("ppc_recomp.0.cpp", 5, root), "")
            self.assertEqual(report.guest_context("arbitrary.txt", 2, root), "")


if __name__ == "__main__":
    unittest.main()
