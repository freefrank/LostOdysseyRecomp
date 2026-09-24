"""Small synthetic release ZIP checks; never inspect real release assets."""

import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile


SCRIPT = Path(__file__).resolve().parents[1] / "release/verify_package.py"
spec = importlib.util.spec_from_file_location("release_verify_package", SCRIPT)
verifier = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verifier)

VERSION = "v0.6.15"
COMMIT = "a" * 40


class ReleasePackageVerifyTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.assets = Path(self.temporary.name)
        self.package = self.assets / f"LostOdysseyRecomp-windows-x64-{VERSION}.zip"
        prefix = self.package.stem + "/"
        runtime = b"synthetic runtime only"
        runtime_hash = hashlib.sha256(runtime).hexdigest()
        manifest = {
            "version": VERSION, "source_version": VERSION[1:], "commit": COMMIT,
            "build_commit": COMMIT, "packaging_commit": COMMIT,
            "dirty": False, "development_build": False,
            "packaging_source": {"commit": COMMIT, "dirty": False},
            "build_provenance": [{"binary": "LostOdysseyRecomp.exe", "source_version": VERSION[1:],
                                  "binary_sha256": runtime_hash,
                                  "source": {"commit": COMMIT, "dirty": False}}],
            "files": {"LostOdysseyRecomp.exe": runtime_hash},
        }
        with zipfile.ZipFile(self.package, "w") as archive:
            archive.writestr(prefix + "LostOdysseyRecomp.exe", runtime)
            archive.writestr(prefix + "manifest.json", json.dumps(manifest))
        self.sidecar = Path(str(self.package) + ".sha256")
        self.sidecar.write_text(hashlib.sha256(self.package.read_bytes()).hexdigest() +
                                "  " + self.package.name + "\n", encoding="utf-8")

    def test_assets_entry_reports_provenance_without_extracting(self):
        output = self.assets / "verification.json"
        subprocess.run([sys.executable, str(SCRIPT), "--assets", str(self.assets),
                        "--version", VERSION, "--commit", COMMIT, "--output", str(output)],
                       check=True, capture_output=True, text=True)
        result = json.loads(output.read_text(encoding="utf-8"))
        self.assertEqual(result["commit"], COMMIT)
        self.assertEqual(result["manifest_files"], 1)
        self.assertFalse(result["payload_hashes_rechecked"])
        self.assertFalse((self.assets / "LostOdysseyUpdater.exe").exists())

    def test_wrong_version_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "Unexpected release ZIP name"):
            verifier.verify_package(self.package, "v0.6.16", COMMIT)

    def test_bad_sidecar_is_rejected(self):
        self.sidecar.write_text("0" * 64 + "  " + self.package.name + "\n", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "checksum mismatch"):
            verifier.verify_package(self.package, VERSION, COMMIT)


if __name__ == "__main__":
    unittest.main()
