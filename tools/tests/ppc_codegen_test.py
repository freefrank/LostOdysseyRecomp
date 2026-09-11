"""Synthetic temporary-tree tests; no game input or runtime required."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("ppc_codegen", Path(__file__).parents[1] / "ppc_codegen.py")
codegen = importlib.util.module_from_spec(spec)
spec.loader.exec_module(codegen)


class CodegenTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.output = self.root / "LostOdysseyRecompLib/ppc"
        files = {
            "LostOdysseyRecompLib/config/LostOdysseyRecomp.toml": '[main]\nfile_path="../private/default.xex"\nout_directory_path="../ppc"\n',
            "LostOdysseyRecompLib/private/default.xex": "synthetic input",
            "tools/XenonRecomp/XenonUtils/ppc_context.h": "context",
            "tools/ppc_codegen.py": "script",
            "tools/xexdump/CMakeLists.txt": "build configuration",
            "tools/build_tools.bat": "build script",
            "tool.exe": "synthetic binary",
        }
        for name in ("ppc_config.h", "ppc_recomp_shared.h", "ppc_recomp.0.cpp", "ppc_func_mapping.cpp"):
            files[f"LostOdysseyRecompLib/ppc/{name}"] = "synthetic output"
        files["LostOdysseyRecompLib/ppc/ppc_context.h"] = '#pragma once\n#include "ppc_config.h"\n\ncontext'
        for name, content in files.items():
            p = self.root / name
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text(content)
        _, _, inputs, _ = codegen.layout(self.root)
        receipt = {"schema": 1, "inputs": codegen.snapshot(self.root, inputs),
                   "outputs": codegen.outputs(self.root, self.output)}
        (self.output / codegen.MANIFEST).write_text(json.dumps(receipt))

    def test_matching_manifest(self):
        codegen.check(self.root)

    def test_input_drift(self):
        (self.root / "tools/XenonRecomp/new.cpp").write_text("new source")
        with self.assertRaisesRegex(ValueError, "inputs changed"):
            codegen.check(self.root)

    def test_output_drift(self):
        (self.output / "ppc_recomp.0.cpp").write_text("changed")
        with self.assertRaisesRegex(ValueError, "sources changed"):
            codegen.check(self.root)

    def test_stale_context(self):
        (self.output / "ppc_context.h").write_text("old context")
        with self.assertRaisesRegex(ValueError, "context"):
            codegen.check(self.root)

    def test_old_switch(self):
        (self.output / "ppc_recomp.0.cpp").write_text("switch (ctx.r11.u64) {}")
        with self.assertRaisesRegex(ValueError, "64-bit"):
            codegen.check(self.root)

    def test_tool_drift(self):
        executable = self.root / "tool.exe"
        codegen.stamp_tool(self.root, executable)
        executable.write_text("changed binary")
        with self.assertRaisesRegex(ValueError, "receipt"):
            codegen.generate(self.root, executable)

    def test_success_exit_without_generation(self):
        executable = self.root / "tool.exe"
        codegen.stamp_tool(self.root, executable)
        with patch.object(codegen.subprocess, "run"), self.assertRaisesRegex(ValueError, "Missing generated"):
            codegen.generate(self.root, executable)
        self.assertFalse((self.output / codegen.MANIFEST).exists())
        self.assertEqual((self.output / "ppc_recomp.0.cpp").read_text(), "synthetic output")


if __name__ == "__main__":
    unittest.main()
