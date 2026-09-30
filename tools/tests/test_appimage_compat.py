"""Small, dependency-free regressions for the AppImage packaging ABI gate."""
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from appimage_compat import CompatibilityError, check_glibc, compiler_libraries, version_sections


class AppImageCompatibilityTests(unittest.TestCase):
    def test_only_needs_set_the_glibc_floor(self):
        needs, exports = version_sections("""
Version definition section '.gnu.version_d' contains 2 entries:
  0x0000: Rev: 1 Flags: none Index: 2 Cnt: 1 Name: GLIBC_99.0
  0x0010: Rev: 1 Flags: none Index: 3 Cnt: 1 Name: GLIBCXX_3.4.32
Version needs section '.gnu.version_r' contains 1 entry:
  0x0000: Version: 1 File: libc.so.6 Cnt: 1
  0x0010: Name: GLIBC_2.35 Flags: none Version: 4
""")
        self.assertEqual(needs, {"libc.so.6": {"GLIBC_2.35"}})
        self.assertIn("GLIBCXX_3.4.32", exports)
        check_glibc(needs, "libstdc++.so.6")

    def test_new_glibc_is_rejected_for_any_provider(self):
        for library in ("libc.so.6", "libm.so.6"):
            with self.subTest(library=library), self.assertRaises(CompatibilityError):
                check_glibc({library: {"GLIBC_2.38"}}, "vendor.so")

    def test_numeric_comparison_and_non_glibc_versions(self):
        check_glibc({"libc.so.6": {"GLIBC_2.9", "GLIBC_2.2.5"},
                     "libstdc++.so.6": {"GLIBCXX_3.4.32"}}, "runtime")
        with self.assertRaises(CompatibilityError):
            check_glibc({"libc.so.6": {"GLIBC_2.100"}}, "runtime")

    def test_private_and_relr_requirements_are_not_ignored(self):
        for version in ("GLIBC_PRIVATE", "GLIBC_ABI_DT_RELR"):
            with self.subTest(version=version), self.assertRaises(CompatibilityError):
                check_glibc({"libc.so.6": {version}}, "vendor.so")

    def test_missing_compiler_library_fails_closed(self):
        with patch("appimage_compat.run", return_value="libstdc++.so.6\n"):
            with self.assertRaises(CompatibilityError):
                compiler_libraries("clang++-18")


if __name__ == "__main__":
    unittest.main()
