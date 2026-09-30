"""Focused checks for exporting the AppImage runtime into a Flatpak."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import package_flatpak as flatpak  # noqa: E402


def write(path: Path, data: bytes = b"payload") -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    return path


def appdir(root: Path) -> Path:
    directory = root / "LostOdysseyRecomp.AppDir"
    files = directory / "usr"
    for name in flatpak.REQUIRED - {"bin/libnvidia-ngx-dlss.so", "bin/libnvidia-ngx-dlss.so.1"}:
        write(files / name)
    for name in ("libnvidia-ngx-dlss.so", "libnvidia-ngx-dlss.so.1"):
        (files / "bin" / name).symlink_to("libnvidia-ngx-dlss.so.310.9.1")
    write(files / "lib/libcurl.so.4", b"runtime dependency")
    write(files / "share/doc/libcurl4/copyright", b"license")
    return directory


class PackageFlatpakTest(unittest.TestCase):
    def test_payload_preserves_runtime_libraries_and_rejects_leaks(self):
        with tempfile.TemporaryDirectory() as temporary:
            files = appdir(Path(temporary)) / "usr"
            flatpak.inspect_payload_tree(files)
            write(files / "private/disc1/default.xex")
            with self.assertRaisesRegex(ValueError, "Unexpected payload path"):
                flatpak.inspect_payload_tree(files)
            (files / "private/disc1/default.xex").unlink()
            write(files / "lib/libunexpected.a")
            with self.assertRaisesRegex(ValueError, "Unexpected payload path"):
                flatpak.inspect_payload_tree(files)
            (files / "lib/libunexpected.a").unlink()
            (files / "bin/shaders/portable_vk.lospv").unlink()
            with self.assertRaisesRegex(ValueError, "Missing required"):
                flatpak.inspect_payload_tree(files)

    def test_escaping_symlink_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            files = appdir(Path(temporary)) / "usr"
            (files / "lib/libdxcompiler.so").unlink()
            (files / "lib/libdxcompiler.so").symlink_to(write(Path(temporary) / "outside.so"))
            with self.assertRaisesRegex(ValueError, "Symlink leaves payload tree"):
                flatpak.inspect_payload_tree(files)

    def test_manifest_is_metadata_only_and_permissions_are_reused(self):
        metadata = flatpak.manifest_config()
        self.assertEqual(metadata["app-id"], flatpak.APP_ID)
        self.assertNotIn("modules", metadata)
        self.assertNotIn("sdk-extensions", metadata)
        self.assertIn("--share=ipc", metadata["finish-args"])
        self.assertIn("--filesystem=host", metadata["finish-args"])

    def test_appdir_runtime_is_copied_without_compilation_for_both_branches(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = appdir(root)
            commands = []

            def fake_run(command):
                commands.append(command)
                if command[:2] == ["flatpak", "build-init"]:
                    (Path(command[3]) / "files").mkdir(parents=True)
                if command[:2] == ["flatpak", "build-bundle"]:
                    write(Path(command[3]), b"bundle")

            for tag, branch in (("v0.7.3-updaterfix", "stable"), ("", "dev")):
                output = root / branch
                args = argparse.Namespace(appdir=source, output=output, version=tag)
                with patch.object(flatpak, "source_version", return_value="0.7.3"), \
                     patch.object(flatpak.subprocess, "check_output", return_value="a" * 40), \
                     patch.object(flatpak, "run", side_effect=fake_run):
                    result = flatpak.package(args)
                copied = output / "builder/files/bin/LostOdysseyRecomp"
                self.assertEqual(copied.read_bytes(), (source / "usr/bin/LostOdysseyRecomp").read_bytes())
                self.assertEqual((output / "builder/files/lib/libcurl.so.4").read_bytes(), b"runtime dependency")
                self.assertTrue((output / "builder/files/bin/libnvidia-ngx-dlss.so").is_symlink())
                self.assertEqual(result["size"], (output / result["bundle"]).stat().st_size)
                self.assertEqual(result["packaging_commit"], "a" * 40)
                self.assertEqual(result["branch"], branch)
                self.assertEqual(json.loads((output / "source.json").read_text())["bundle"], result["bundle"])
                self.assertTrue(result["bundle"].endswith((f"{tag}.flatpak" if tag else "-dev.flatpak")))
            self.assertEqual([command[1] for command in commands],
                             ["build-init", "build", "build-finish", "build-export", "build-bundle"] * 2)
            for index, branch in ((0, "stable"), (5, "dev")):
                self.assertEqual(commands[index + 3][-1], branch)
                self.assertEqual(commands[index + 4][-1], branch)
                self.assertEqual(commands[index + 1][1:3], ["build", "--runtime"])
                self.assertIn("--arch=x86_64", commands[index])
                self.assertIn("--share=ipc", commands[index + 2])
                self.assertIn("--command=LostOdysseyRecomp", commands[index + 2])

    def test_release_version_must_match_checkout_and_output_is_exclusive(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = appdir(root)
            args = argparse.Namespace(appdir=source, output=root / "output", version="v0.7.2")
            with patch.object(flatpak, "source_version", return_value="0.7.3"):
                with self.assertRaisesRegex(ValueError, "must match"):
                    flatpak.package(args)
            args.output.mkdir()
            with self.assertRaisesRegex(ValueError, "already exists"):
                flatpak.package(args)

    def test_failed_runtime_probe_prevents_export_and_bundle(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            args = argparse.Namespace(appdir=appdir(root), output=root / "output", version="v0.7.3")
            commands = []

            def fake_run(command):
                commands.append(command)
                if command[:2] == ["flatpak", "build-init"]:
                    (Path(command[3]) / "files").mkdir(parents=True)
                elif command[:2] == ["flatpak", "build"]:
                    raise subprocess.CalledProcessError(1, command)

            with patch.object(flatpak, "source_version", return_value="0.7.3"), \
                 patch.object(flatpak.subprocess, "check_output", return_value="a" * 40), \
                 patch.object(flatpak, "run", side_effect=fake_run):
                with self.assertRaises(subprocess.CalledProcessError):
                    flatpak.package(args)
            self.assertEqual([command[1] for command in commands], ["build-init", "build"])
            self.assertEqual(list(args.output.glob("*.flatpak")), [])


if __name__ == "__main__":
    unittest.main()
