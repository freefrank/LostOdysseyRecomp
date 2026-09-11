"""Offline sync decisions and immutable-push races; no native build or network."""
import copy
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location("ppc_sync", Path(__file__).resolve().parents[1] / "release/ppc_sync.py")
sync = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(sync)
SHA = "a" * 40
OTHER_SHA = "b" * 40


class PpcSyncTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.build = self.root / "build"
        self.build.mkdir()
        (self.build / "CMakeCache.txt").write_text("CMAKE_BUILD_TYPE:STRING=Release\n")
        self.evidence = {"fingerprint": {"inputs": {"source": "abc"}}, "contract": {"FLAGS": "/MT /O2"}}
        self.environment = patch.dict(os.environ, {}, clear=True)
        self.environment.start()
        self.addCleanup(self.environment.stop)
        self.fp = patch.object(sync.ppc_prebuilt, "fingerprint", side_effect=lambda root: copy.deepcopy(self.evidence["fingerprint"])).start()
        self.contract = patch.object(sync.ppc_prebuilt, "compile_contract", side_effect=lambda *args: (copy.deepcopy(self.evidence["contract"]), "22.1")).start()
        self.addCleanup(patch.stopall)
        self.key, _ = sync.identity(self.root, self.build)
        self.manifest = {"schema": 1, **copy.deepcopy(self.evidence),
                         "library": {"name": sync.ppc_prebuilt.LIBRARY, "size": 10, "sha256": "c" * 64},
                         "chunks": [{"name": "ppc-0000.bin", "size": 10, "sha256": "d" * 64}]}

    def test_key_tracks_source_and_compile_contract(self):
        self.assertEqual(self.key, sync.identity(self.root, self.build)[0])
        self.evidence["fingerprint"]["inputs"]["source"] = "changed"
        source_key = sync.identity(self.root, self.build)[0]
        self.assertNotEqual(self.key, source_key)
        self.evidence["contract"]["FLAGS"] = "/MT /O1"
        self.assertNotEqual(source_key, sync.identity(self.root, self.build)[0])

    def test_key_has_no_network_build_or_config_side_effect(self):
        with patch.object(sync, "run") as run:
            sync.identity(self.root, self.build)
            run.assert_not_called()

    def test_commands_disable_interactive_authentication(self):
        with patch.object(sync.subprocess, "run") as run:
            sync.run(["git", "status"])
            self.assertEqual("0", run.call_args.kwargs["env"]["GIT_TERMINAL_PROMPT"])
            self.assertEqual("1", run.call_args.kwargs["env"]["GH_PROMPT_DISABLED"])

    def test_ci_and_recursive_sync_skip_even_force(self):
        for variable in ("CI", "GITHUB_ACTIONS", "LO_PPC_SYNC_ACTIVE"):
            with self.subTest(variable=variable), patch.dict(os.environ, {variable: "1"}), patch.object(sync, "run") as run:
                self.assertIsNone(sync.sync(self.root, self.build, force=True))
                run.assert_not_called()

    def test_disabled_sync_skips(self):
        with patch.object(sync, "enabled", return_value=False), patch.object(sync, "release_build") as build:
            self.assertIsNone(sync.sync(self.root, self.build))
            build.assert_not_called()

    def test_enabled_uses_git_boolean_parser(self):
        with patch.object(sync, "run", return_value=subprocess.CompletedProcess([], 0, "true\n", "")) as run:
            self.assertTrue(sync.enabled(self.root))
            self.assertIn("--bool", run.call_args.args[0])

    def test_imported_caller_skips_even_force(self):
        with (self.build / "CMakeCache.txt").open("a") as cache:
            cache.write("LO_PREBUILT_PPC_DIR:PATH=D:/prebuilt\n")
        with patch.object(sync, "remote_commit") as remote:
            self.assertIsNone(sync.sync(self.root, self.build, already_built=True, force=True))
            remote.assert_not_called()

    def test_freeze_built_library_does_not_build(self):
        library = self.build / "LostOdysseyRecompLib" / sync.ppc_prebuilt.LIBRARY
        library.parent.mkdir()
        library.write_bytes(b"synthetic existing archive")
        with patch.object(sync.ppc_prebuilt.subprocess, "run") as run:
            sync.ppc_prebuilt.write_bundle_from_built(self.root, self.build, self.root / "bundle")
            run.assert_not_called()
        manifest = sync.ppc_prebuilt.load_manifest(self.root / "bundle")
        self.assertEqual(library.stat().st_size, manifest["library"]["size"])

    def test_remote_missing_only_for_exit_two(self):
        with patch.object(sync, "run", return_value=subprocess.CompletedProcess([], 2, "", "")):
            self.assertIsNone(sync.remote_commit(self.key))
        for error in ("Authentication failed", "Could not resolve host"):
            with self.subTest(error=error), patch.object(sync, "run", return_value=subprocess.CompletedProcess([], 128, "", error)):
                with self.assertRaises(subprocess.CalledProcessError):
                    sync.remote_commit(self.key)

    def test_remote_ref_response_is_exact(self):
        result = subprocess.CompletedProcess([], 0, f"{SHA}\trefs/heads/ppc/{self.key}\n", "")
        with patch.object(sync, "run", return_value=result):
            self.assertEqual(SHA, sync.remote_commit(self.key))

    def test_unchanged_does_not_build_or_upload(self):
        with patch.object(sync, "remote_commit", return_value=SHA), patch.object(sync, "remote_manifest", return_value=self.manifest), patch.object(sync, "publish") as publish, patch.object(sync, "run") as run:
            self.assertEqual((self.key, SHA), sync.sync(self.root, self.build, force=True))
            publish.assert_not_called()
            run.assert_not_called()
        receipt = json.loads((self.root / "out/ppc-sync/receipt.json").read_text())
        self.assertEqual(SHA, receipt["commit"])
        self.assertNotIn("LO_PPC_SYNC_ACTIVE", os.environ)

    def test_existing_mismatch_is_not_overwritten(self):
        bad = copy.deepcopy(self.manifest)
        bad["contract"]["FLAGS"] = "different"
        with patch.object(sync, "remote_commit", return_value=SHA), patch.object(sync, "remote_manifest", return_value=bad), patch.object(sync, "publish") as publish:
            with self.assertRaisesRegex(ValueError, "does not match"):
                sync.sync(self.root, self.build, force=True)
            publish.assert_not_called()

    def test_deleted_ref_is_not_hidden_by_local_receipt(self):
        sync.receipt(self.root, self.key, SHA)
        with patch.object(sync, "remote_commit", side_effect=[None, OTHER_SHA]), patch.object(sync, "remote_manifest", return_value=self.manifest), patch.object(sync, "publish", return_value=(OTHER_SHA, self.manifest)) as publish:
            sync.sync(self.root, self.build, already_built=True, force=True)
            publish.assert_called_once_with(self.root, self.build, self.key, self.evidence, True)

    def test_failed_upload_readback_is_not_success(self):
        different = copy.deepcopy(self.manifest)
        different["library"]["sha256"] = "e" * 64
        with patch.object(sync, "remote_commit", side_effect=[None, SHA]), patch.object(sync, "remote_manifest", return_value=different), patch.object(sync, "publish", return_value=(SHA, self.manifest)):
            with self.assertRaisesRegex(ValueError, "readback metadata mismatch"):
                sync.sync(self.root, self.build, force=True)
        self.assertFalse((self.root / "out/ppc-sync/receipt.json").exists())

    def fake_writer(self, root, build, output):
        output.mkdir()
        (output / "manifest.json").write_text(json.dumps(self.manifest))

    def fake_git(self, *, push_error=False):
        self.git_commands = []
        def run(command, **kwargs):
            self.git_commands.append(command)
            operation = command[len(sync.GIT_AUTH):]
            result = SHA if operation == ["rev-parse", "HEAD"] else ""
            failed = push_error and operation[0] == "push"
            return subprocess.CompletedProcess(command, 1 if failed else 0, result, "rejected" if failed else "")
        return run

    def test_already_built_never_calls_export_or_build(self):
        with patch.object(sync, "run", side_effect=self.fake_git()), patch.object(sync, "remote_commit", return_value=None), patch.object(sync.ppc_prebuilt, "write_bundle_from_built", side_effect=self.fake_writer) as writer, patch.object(sync.ppc_prebuilt, "export") as export:
            commit, _ = sync.publish(self.root, self.build, self.key, self.evidence, True)
            self.assertEqual(SHA, commit)
            writer.assert_called_once()
            export.assert_not_called()
        self.assertTrue(any("push" in command for command in self.git_commands))
        self.assertFalse(any("--force" in arg or arg == "fetch" for command in self.git_commands for arg in command))
        self.assertEqual([], list((self.root / "out/ppc-sync").glob("upload-*")))

    def test_existing_ref_before_push_is_not_overwritten(self):
        with patch.object(sync, "run", side_effect=self.fake_git()), patch.object(sync, "remote_commit", return_value=OTHER_SHA), patch.object(sync, "remote_manifest", return_value=self.manifest), patch.object(sync.ppc_prebuilt, "write_bundle_from_built", side_effect=self.fake_writer):
            self.assertEqual(OTHER_SHA, sync.publish(self.root, self.build, self.key, self.evidence, True)[0])
        self.assertFalse(any("push" in command for command in self.git_commands))

    def test_push_race_accepts_matching_remote(self):
        with patch.object(sync, "run", side_effect=self.fake_git(push_error=True)), patch.object(sync, "remote_commit", side_effect=[None, OTHER_SHA]), patch.object(sync, "remote_manifest", return_value=self.manifest), patch.object(sync.ppc_prebuilt, "write_bundle_from_built", side_effect=self.fake_writer):
            self.assertEqual(OTHER_SHA, sync.publish(self.root, self.build, self.key, self.evidence, True)[0])

    def test_failed_push_with_no_remote_is_error(self):
        with patch.object(sync, "run", side_effect=self.fake_git(push_error=True)), patch.object(sync, "remote_commit", return_value=None), patch.object(sync.ppc_prebuilt, "write_bundle_from_built", side_effect=self.fake_writer):
            with self.assertRaises(subprocess.CalledProcessError):
                sync.publish(self.root, self.build, self.key, self.evidence, True)

    def test_non_release_configures_isolated_source_build(self):
        (self.build / "CMakeCache.txt").write_text("CMAKE_BUILD_TYPE:STRING=Debug\n")
        with patch.object(sync, "run") as run:
            self.assertEqual(self.root / "out/build/ppc-sync-Release", sync.release_build(self.root, self.build))
            command = run.call_args.args[0]
            self.assertIn("-DLO_PPC_AUTO_SYNC=OFF", command)
            self.assertIn("-DLO_BUILD_RUNTIME=OFF", command)
            self.assertIn("-DLO_PREBUILT_PPC_DIR=", command)
            self.assertNotIn("--build", command)


if __name__ == "__main__":
    unittest.main()
