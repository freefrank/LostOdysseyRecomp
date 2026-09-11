"""Synthetic bundle/contract tests; does not compile native PPC code."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location("ppc_prebuilt", Path(__file__).resolve().parents[1] / "release/ppc_prebuilt.py")
ppc = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(ppc)


class PpcPrebuiltTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name) / "source"
        self.build = self.root / "build"
        self.bundle = self.root / "bundle"
        self.restored = self.root / "restored"
        files = {
            "CMakeLists.txt": "project(test)\n",
            "LostOdysseyRecompLib/CMakeLists.txt": "add_library(test STATIC)\n",
            "LostOdysseyRecomp/gpu/ppc_mmio.h": "mmio\n",
            "tools/XenonRecomp/thirdparty/simde/simde.h": "simde\n",
            "LostOdysseyRecompLib/ppc/codegen-manifest.json": json.dumps({"inputs": {"input": "abc"}, "outputs": {"output": "def"}}),
            "build/CMakeCache.txt": "CMAKE_BUILD_TYPE:STRING=Release\n",
            "build/CMakeFiles/4.0/CMakeCXXCompiler.cmake": '\n'.join([
                'set(CMAKE_CXX_COMPILER_ID "Clang")', 'set(CMAKE_CXX_SIMULATE_ID "MSVC")',
                'set(CMAKE_CXX_COMPILER_ARCHITECTURE_ID x64)', 'set(CMAKE_CXX_COMPILER_VERSION "22.1")']),
            "build/build.ninja": self.ninja(),
        }
        for name, data in files.items():
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(data, encoding="utf-8")
        self.library = self.build / "LostOdysseyRecompLib" / ppc.LIBRARY
        self.library.parent.mkdir(parents=True)
        self.library.write_bytes(b"!<arch>\nsynthetic COFF fixture")
        # Source-generation receipt validation has its own tests. Isolate that
        # expensive dependency here; all bundle and compiler checks are real.
        self.codegen = patch.object(ppc.ppc_codegen, "check").start()
        self.addCleanup(patch.stopall)

    def ninja(self, flags="/O2 /MT -march=sandybridge -clang:-std=c++23", target="LostOdysseyRecompLib"):
        return (f"build obj: CXX_COMPILER__{target}_unscanned_Release {self.root}/LostOdysseyRecompLib/ppc/ppc_recomp.0.cpp\n"
                f"  FLAGS = {flags} /FI{self.root}/LostOdysseyRecomp/gpu/ppc_mmio.h\n"
                f"  INCLUDES = -I{self.root}/LostOdysseyRecompLib/ppc\n\n")

    def export(self):
        with patch.object(ppc.subprocess, "run") as run, patch.object(ppc, "CHUNK_SIZE", 10):
            ppc.export(self.root, self.build, self.bundle)
            run.assert_called_once_with(["cmake", "--build", str(self.build), "--target", "LostOdysseyRecompLib", "--parallel", "4"], check=True)
        return json.loads((self.bundle / "manifest.json").read_text())

    def ready(self):
        self.export()
        ppc.restore(self.bundle, self.restored)

    def test_round_trip_and_equivalent_contract_target(self):
        self.ready()
        self.assertEqual((self.restored / ppc.LIBRARY).read_bytes(), self.library.read_bytes())
        (self.build / "build.ninja").write_text(self.ninja(target="LoPpcCompileContract"))
        compiler = self.build / "CMakeFiles/4.0/CMakeCXXCompiler.cmake"
        compiler.write_text(compiler.read_text().replace("22.1", "23.0"))
        ppc.check(self.root, self.restored, self.build)
        self.assertGreaterEqual(self.codegen.call_count, 3)

    def test_input_header_drift(self):
        self.ready()
        (self.root / "LostOdysseyRecomp/gpu/ppc_mmio.h").write_text("changed")
        with self.assertRaisesRegex(ValueError, "inputs/outputs changed"):
            ppc.check(self.root, self.restored)

    def test_codegen_input_drift(self):
        self.ready()
        receipt = self.root / "LostOdysseyRecompLib/ppc/codegen-manifest.json"
        receipt.write_text(json.dumps({"inputs": {"input": "changed"}, "outputs": {"output": "def"}}))
        with self.assertRaisesRegex(ValueError, "inputs/outputs changed"):
            ppc.check(self.root, self.restored)

    def test_codegen_rejects_stale_sources(self):
        self.ready()
        self.codegen.side_effect = ValueError("Generated PPC sources changed")
        with self.assertRaisesRegex(ValueError, "Generated PPC"):
            ppc.check(self.root, self.restored)

    def test_flags_drift(self):
        self.ready()
        (self.build / "build.ninja").write_text(self.ninja(flags="/O1 /MT -march=sandybridge -clang:-std=c++23"))
        with self.assertRaisesRegex(ValueError, "compile contract changed"):
            ppc.check(self.root, self.restored, self.build)

    def test_cmake_line_endings_are_normalized(self):
        self.ready()
        (self.root / "CMakeLists.txt").write_bytes(b"project(test)\r\n")
        ppc.check(self.root, self.restored)

    def test_missing_chunk(self):
        manifest = self.export()
        (self.bundle / manifest["chunks"][0]["name"]).unlink()
        with self.assertRaises(FileNotFoundError):
            ppc.restore(self.bundle, self.restored)
        self.assertFalse((self.restored / ppc.LIBRARY).exists())

    def test_corrupt_chunk(self):
        manifest = self.export()
        (self.bundle / manifest["chunks"][0]["name"]).write_bytes(b"x" * 10)
        with self.assertRaisesRegex(ValueError, "hash/size mismatch"):
            ppc.restore(self.bundle, self.restored)

    def test_path_traversal_chunk(self):
        manifest = self.export()
        manifest["chunks"][0]["name"] = "../outside.bin"
        (self.bundle / "manifest.json").write_text(json.dumps(manifest))
        with self.assertRaisesRegex(ValueError, "Invalid PPC chunk"):
            ppc.restore(self.bundle, self.restored)

    def test_library_corruption(self):
        self.ready()
        (self.restored / ppc.LIBRARY).write_bytes(b"corrupt")
        with self.assertRaisesRegex(ValueError, "hash/size mismatch"):
            ppc.check(self.root, self.restored)

    def test_non_release_rejected(self):
        (self.build / "CMakeCache.txt").write_text("CMAKE_BUILD_TYPE:STRING=Debug\n")
        with self.assertRaisesRegex(ValueError, "requires Release"):
            self.export()

    def test_lto_and_dynamic_runtime_rejected(self):
        for flags in ("/MT -flto", "/MT /GL", "/MD", "/MTd"):
            with self.subTest(flags=flags):
                (self.build / "build.ninja").write_text(self.ninja(flags=flags))
                with self.assertRaisesRegex(ValueError, "non-LTO /MT"):
                    ppc.compile_contract(self.root, self.build)

    def test_change_during_build_rejected(self):
        def mutate(*args, **kwargs):
            (self.root / "LostOdysseyRecomp/gpu/ppc_mmio.h").write_text("changed during compilation")
        with patch.object(ppc.subprocess, "run", side_effect=mutate):
            with self.assertRaisesRegex(ValueError, "inputs changed during build"):
                ppc.export(self.root, self.build, self.bundle)
        self.assertFalse(self.bundle.exists())


if __name__ == "__main__":
    unittest.main()
