"""Check the native installer release payload contract without building it."""
import hashlib
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
PACKAGE_SCRIPT = ROOT / 'tools/package_release.py'
REQUIREMENTS = ROOT / 'tools/release/requirements.txt'
FONT_PROVENANCE = ROOT / 'LostOdysseyRecomp/install/FONT-PROVENANCE.md'
FONT_LICENSE = ROOT / 'thirdparty/SDL/test/unifont-13.0.06-license.txt'


class PackagePayloadTests(unittest.TestCase):
    def test_release_payload_uses_native_importer_and_main_binary(self):
        source = PACKAGE_SCRIPT.read_text(encoding='utf-8')

        self.assertNotIn('PyInstaller', source)
        self.assertNotIn('InstallGame.exe', source)
        self.assertIn("runtime = build / 'LostOdysseyRecomp/LostOdysseyRecomp.exe'", source)
        self.assertNotIn('LostOdysseyUpdater.exe', source)

    def test_release_payload_contains_font_provenance_and_license(self):
        source = PACKAGE_SCRIPT.read_text(encoding='utf-8')
        license_hash = hashlib.sha256(FONT_LICENSE.read_bytes()).hexdigest()

        self.assertTrue(FONT_PROVENANCE.is_file())
        self.assertTrue(FONT_LICENSE.is_file())
        self.assertIn("FONT-PROVENANCE.md", source)
        self.assertIn("unifont-13.0.06-license.txt", source)
        self.assertIn(license_hash, FONT_PROVENANCE.read_text(encoding='utf-8'))

    def test_release_requirements_have_no_freezer_dependency(self):
        requirements = REQUIREMENTS.read_text(encoding='utf-8').lower()

        self.assertNotIn('pyinstaller', requirements)

    def test_release_payload_handles_dlss_runtime_and_license(self):
        source = PACKAGE_SCRIPT.read_text(encoding='utf-8')
        self.assertIn("dlss_runtime = runtime.parent / 'nvngx_dlss.dll'", source)
        self.assertIn("licenses / 'NVIDIA-DLSS'", source)
        self.assertIn("nvngx_dlss.dll is packaged but DLSS SDK license is missing.", source)


if __name__ == '__main__':
    unittest.main(verbosity=2)
