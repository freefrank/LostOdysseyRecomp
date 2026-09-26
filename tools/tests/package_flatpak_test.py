"""Offline tests for the explicit Flatpak staging/export boundary (no Flatpak CLI)."""
from __future__ import annotations

import argparse
from contextlib import contextmanager
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import package_flatpak as flatpak


ROOT = Path(__file__).resolve().parents[2]


def write(path: Path, content: str = "fixture") -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")
    return path


@contextmanager
def checkout():
    with tempfile.TemporaryDirectory() as temporary:
        repo = Path(temporary) / "repo"
        repo.mkdir()
        ppc = repo / flatpak.PPC_PATH
        manifest = write(ppc / "codegen-manifest.json", json.dumps({
            "schema": 1, "outputs": {"LostOdysseyRecompLib/ppc/ppc_recomp.0.cpp": "digest"}}))
        write(ppc / "ppc_recomp.0.cpp")
        write(ppc / "unselected.cpp")
        write(repo / "CMakeLists.txt", "project(LostOdysseyRecomp VERSION 0.7.1 LANGUAGES C CXX)\n")
        write(repo / "untracked-secret.txt", "not selected")
        write(repo / flatpak.SDL_PATCH, (ROOT / flatpak.SDL_PATCH).read_text(encoding="utf-8"))
        private = {name: write(repo / relative) for name, relative in flatpak.PRIVATE_INPUTS.items()}
        submodule = repo / "thirdparty/plume"
        write(submodule / ".git", "gitdir: elsewhere\n")
        write(submodule / "src/source.cpp")
        write(submodule / "plume_log.h", "// patched local header\n")
        write(submodule / "untracked-private.txt", "not selected")
        nested = submodule / "thirdparty/math"
        write(nested / ".git", "gitdir: elsewhere\n")
        write(nested / "include/math.h")

        def fake_git(directory, *args):
            if args == ("rev-parse", "HEAD"):
                if directory.resolve() == repo.resolve():
                    return b"1" * 40
                return (b"3" if directory.resolve() == nested.resolve() else b"2") * 40
            if args == ("ls-files", "--stage", "-z"):
                if directory.resolve() == repo.resolve():
                    return (b"100644 " + b"1" * 40 + b" 0\tCMakeLists.txt\0" +
                            b"160000 " + b"2" * 40 + b" 0\tthirdparty/plume\0")
                if directory.resolve() == submodule.resolve():
                    return (b"100644 " + b"3" * 40 + b" 0\tsrc/source.cpp\0" +
                            b"160000 " + b"4" * 40 + b" 0\tthirdparty/math\0")
                if directory.resolve() == nested.resolve():
                    return b"100644 " + b"5" * 40 + b" 0\tinclude/math.h\0"
            raise AssertionError((directory, args))

        yield repo, ppc, manifest, private, fake_git


class PackageFlatpakTest(unittest.TestCase):
    def test_missing_explicit_inputs_fail(self):
        with checkout() as (repo, ppc, manifest, private, fake_git):
            with patch.object(flatpak, "git_output", fake_git), tempfile.TemporaryDirectory() as temp:
                private["default_xex"].unlink()
                with self.assertRaisesRegex(ValueError, "default_xex"):
                    flatpak.stage_checkout(repo, Path(temp) / "source", ppc, manifest, private)
                with self.assertRaisesRegex(ValueError, "portable shader pack"):
                    flatpak.required_file(Path(temp) / "missing.lospv", "portable shader pack")

    def test_stages_tracked_nested_submodule_and_recorded_ppc_only(self):
        with checkout() as (repo, ppc, manifest, private, fake_git):
            with patch.object(flatpak, "git_output", fake_git), tempfile.TemporaryDirectory() as temp:
                destination = Path(temp) / "stage"
                commits = flatpak.stage_checkout(repo, destination, ppc, manifest, private)
                self.assertEqual(commits, {".": "1" * 40, "thirdparty/plume": "2" * 40,
                                           "thirdparty/plume/thirdparty/math": "3" * 40})
                for name in ("CMakeLists.txt", "thirdparty/plume/src/source.cpp",
                             "thirdparty/plume/plume_log.h", flatpak.SDL_PATCH.as_posix(),
                             "thirdparty/plume/thirdparty/math/include/math.h",
                             "LostOdysseyRecompLib/ppc/ppc_recomp.0.cpp",
                             "LostOdysseyRecompLib/ppc/codegen-manifest.json", *flatpak.PRIVATE_INPUTS.values()):
                    self.assertTrue((destination / name).is_file(), name)
                self.assertEqual((destination / flatpak.PLUME_PATCH_HEADER).read_bytes(),
                                 (repo / flatpak.PLUME_PATCH_HEADER).read_bytes())
                self.assertEqual((destination / flatpak.SDL_PATCH).read_bytes(),
                                 (repo / flatpak.SDL_PATCH).read_bytes())
                for name in ("untracked-secret.txt", "LostOdysseyRecompLib/ppc/unselected.cpp",
                             "thirdparty/plume/.git", "thirdparty/plume/untracked-private.txt"):
                    self.assertFalse((destination / name).exists(), name)
                with self.assertRaisesRegex(ValueError, "PPC directory must be inside"):
                    flatpak.stage_checkout(repo, Path(temp) / "other", Path(temp), manifest, private)

    def test_missing_or_outside_generated_input_fails(self):
        with checkout() as (repo, ppc, manifest, private, fake_git):
            with patch.object(flatpak, "git_output", fake_git), tempfile.TemporaryDirectory() as temp:
                (ppc / "ppc_recomp.0.cpp").unlink()
                with self.assertRaisesRegex(ValueError, "generated PPC output"):
                    flatpak.stage_checkout(repo, Path(temp) / "stage", ppc, manifest, private)
                manifest.write_text(json.dumps({"schema": 1, "outputs": {"private/secret": "x"}}))
                with self.assertRaisesRegex(ValueError, "outside PPC"):
                    flatpak.stage_checkout(repo, Path(temp) / "stage", ppc, manifest, private)

    def test_missing_plume_patch_header_fails(self):
        with checkout() as (repo, ppc, manifest, private, fake_git):
            with patch.object(flatpak, "git_output", fake_git), tempfile.TemporaryDirectory() as temp:
                (repo / flatpak.PLUME_PATCH_HEADER).unlink()
                with self.assertRaisesRegex(ValueError, "Plume patch header"):
                    flatpak.stage_checkout(repo, Path(temp) / "stage", ppc, manifest, private)

    def test_missing_sdl_patch_fails(self):
        with checkout() as (repo, ppc, manifest, private, fake_git):
            with patch.object(flatpak, "git_output", fake_git), tempfile.TemporaryDirectory() as temp:
                (repo / flatpak.SDL_PATCH).unlink()
                with self.assertRaisesRegex(ValueError, "SDL PipeWire patch"):
                    flatpak.stage_checkout(repo, Path(temp) / "stage", ppc, manifest, private)

    def test_upstream_sdl_patch_changes_only_node_enumeration(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            target = root / "thirdparty/SDL/src/audio/pipewire/SDL_pipewire.c"
            target.parent.mkdir(parents=True)
            original = (ROOT / "thirdparty/SDL/src/audio/pipewire/SDL_pipewire.c").read_bytes().replace(b"\r\n", b"\n")
            write(root / flatpak.SDL_PATCH, (ROOT / flatpak.SDL_PATCH).read_text(encoding="utf-8"))
            command = flatpak.prepare_manifest(ROOT / flatpak.TEMPLATE, root / "stage", root)["modules"][0]["build-commands"][0]
            old = b"pw_node_enum_params(node->proxy, 0, info->params[i].id, 0, 0, NULL);"
            new = b"pw_node_enum_params((struct pw_node *)node->proxy, 0, info->params[i].id, 0, 0, NULL);"
            for eol in (b"\r\n", b"\n"):
                before = original.replace(b"\n", eol)
                self.assertEqual(before.count(old), 1)
                target.write_bytes(before)
                subprocess.run(["bash", "-c", command], cwd=root, check=True, capture_output=True)
                self.assertEqual(target.read_bytes(), before.replace(old, new, 1))

    def test_fsr_artifact_stages_only_manifest_sources_without_git(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            artifact = root / "artifact"
            prepared = root / "prepared"
            upscaler = flatpak.SDK_FILES["fsr"][0]
            header = "sdk/include/FidelityFX/host/ffx_interface.h"
            sources = {upscaler: "digest", header: "digest"}
            shader_manifest = prepared / "manifest.json"
            write(shader_manifest, json.dumps({"sdk_commit": flatpak.SDK_COMMITS["fsr"],
                                               "sdk_sources": sources}))
            for name in (*sources, "LICENSE.txt"):
                write(artifact / name, name)
            write(artifact / "sdk/unrelated-secret.txt", "must not stage")
            destination = root / "staged"
            self.assertEqual(flatpak.stage_fsr_sdk(artifact, destination, prepared), flatpak.SDK_COMMITS["fsr"])
            for name in (*sources, "LICENSE.txt"):
                self.assertEqual((destination / name).read_bytes(), (artifact / name).read_bytes())
            self.assertFalse((destination / "sdk/unrelated-secret.txt").exists())
            (artifact / header).unlink()
            with self.assertRaisesRegex(ValueError, "FSR SDK input"):
                flatpak.stage_fsr_sdk(artifact, root / "missing", prepared)
            sources["sdk/../unrelated-secret.txt"] = "digest"
            write(shader_manifest, json.dumps({"sdk_commit": flatpak.SDK_COMMITS["fsr"],
                                               "sdk_sources": sources}))
            with self.assertRaisesRegex(ValueError, "Invalid staged path"):
                flatpak.stage_fsr_sdk(artifact, root / "unsafe", prepared)
            write(shader_manifest, json.dumps({"sdk_commit": "0" * 40, "sdk_sources": sources}))
            with self.assertRaisesRegex(ValueError, "pinned SDK sources"):
                flatpak.stage_fsr_sdk(artifact, root / "wrong-commit", prepared)

    def test_prepared_shader_inputs_required_without_regeneration(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            prepared = root / "shaders"
            write(prepared / "manifest.json", json.dumps({"sdk_commit": flatpak.SDK_COMMITS["fsr"],
                                                         "headers": {"shader.h": "hash"}}))
            for name in (*flatpak.SHADER_INPUTS[1:], "shader.h"):
                write(prepared / name)
            flatpak.stage_shader_inputs(prepared, root / "stage")
            self.assertTrue((root / "stage/fsr_prepare_spv.h").is_file())
            (prepared / "LICENSE-FidelityFX.txt").unlink()
            with self.assertRaisesRegex(ValueError, "prepared FSR shader input"):
                flatpak.stage_shader_inputs(prepared, root / "other")

    def test_sdk_source_requires_pinned_commit_and_runtime(self):
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / "ngx"
            write(source / ".git/HEAD")
            with patch.object(flatpak, "tracked_paths", return_value=([], {".": "0" * 40})):
                with self.assertRaisesRegex(ValueError, "differs from pinned"):
                    flatpak.stage_git_sdk(source, Path(temp) / "staged", "ngx")
            with patch.object(flatpak, "tracked_paths", return_value=([], {".": flatpak.SDK_COMMITS["ngx"]})):
                with self.assertRaisesRegex(ValueError, "ngx SDK input"):
                    flatpak.stage_git_sdk(source, Path(temp) / "staged", "ngx")

    def test_ngx_staging_skips_uninitialized_unrelated_submodule(self):
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / "ngx"
            write(source / ".git/HEAD")
            for name in (*flatpak.SDK_FILES["ngx"], "include/nvsdk_ngx_helpers_vk.h"):
                write(source / name, name)
            def fake_git(directory, *args):
                if args == ("rev-parse", "HEAD"):
                    return flatpak.SDK_COMMITS["ngx"].encode()
                if args[:4] == ("ls-files", "--stage", "-z", "--"):
                    self.assertEqual(args[4:], ("include", *flatpak.SDK_FILES["ngx"]))
                    return b"".join(b"100644 " + b"1" * 40 + b" 0\t" + name.encode() + b"\0"
                                    for name in (*flatpak.SDK_FILES["ngx"], "include/nvsdk_ngx_helpers_vk.h"))
                raise AssertionError((directory, args))
            with patch.object(flatpak, "git_output", side_effect=fake_git):
                destination = Path(temp) / "staged"
                self.assertEqual(flatpak.stage_git_sdk(source, destination, "ngx"), flatpak.SDK_COMMITS["ngx"])
                self.assertFalse((destination / "NVIDIAImageScaling").exists())
                for name in (*flatpak.SDK_FILES["ngx"], "include/nvsdk_ngx_helpers_vk.h"):
                    self.assertEqual((destination / name).read_bytes(), (source / name).read_bytes())

    def test_manifest_only_staged_source_and_offline_required_providers(self):
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp)
            staged = output / "source"
            manifest = flatpak.prepare_manifest(ROOT / flatpak.TEMPLATE, staged, output)
            module, = manifest["modules"]
            self.assertEqual(module["sources"], [{"type": "dir", "path": str(staged.resolve())}])
            self.assertEqual(module["build-options"]["build-args"], ["--unshare=network"])
            self.assertTrue(module["build-options"]["strip"])
            self.assertIn("/usr/lib/sdk/llvm22/bin", module["build-options"]["env"]["PATH"])
            self.assertEqual(module["build-commands"][0], flatpak.SDL_PATCH_COMMAND)
            self.assertIn("-DLO_REQUIRE_DLSS=ON", module["build-commands"][1])
            self.assertIn("-DLO_REQUIRE_FSR=ON", module["build-commands"][1])
            self.assertIn("-DFETCHCONTENT_SOURCE_DIR_LO_FFMPEG=", module["build-commands"][1])
            self.assertIn("-DFETCHCONTENT_SOURCE_DIR_LO_PACK_ZSTD=", module["build-commands"][1])
            self.assertIn("-DCMAKE_DISABLE_FIND_PACKAGE_zstd=TRUE", module["build-commands"][1])
            self.assertIn("-DFMT_INSTALL=OFF", module["build-commands"][1])
            self.assertIn("-DSDL_PIPEWIRE=ON", module["build-commands"][1])
            self.assertNotIn("-DSDL_PIPEWIRE=OFF", module["build-commands"][1])
            self.assertIn("-DCMAKE_INSTALL_LIBDIR=lib", module["build-commands"][1])
            self.assertEqual(module["build-commands"][2:],
                              ["cmake --build _flatpak_build --target LostOdysseyRecomp --parallel 2",
                               "cmake --install _flatpak_build --prefix /app",
                               "rm -rf /app/include /app/lib/*.a /app/lib/cmake /app/lib/pkgconfig"])
            self.assertIn("--filesystem=host", manifest["finish-args"])
            self.assertIn("--share=ipc", manifest["finish-args"])
            self.assertEqual(flatpak.source_version(ROOT), "0.7.1")

    def test_only_expected_install_tree_is_exportable(self):
        with tempfile.TemporaryDirectory() as temp:
            tree = Path(temp) / "files"
            for name in flatpak.INSTALLED - {"bin/libnvidia-ngx-dlss.so", "bin/libnvidia-ngx-dlss.so.1"}:
                write(tree / name)
            for name in ("libnvidia-ngx-dlss.so", "libnvidia-ngx-dlss.so.1"):
                (tree / "bin" / name).symlink_to("libnvidia-ngx-dlss.so.310.9.1")
            flatpak.inspect_install_tree(tree)
            self.assertTrue((tree / "manifest.json").is_file())
            self.assertTrue((tree / "bin/shaders/portable_vk.lospv").is_file())
            self.assertTrue((tree / "share/licenses/lost-odyssey-recomp/NVIDIA-DLSS/NOTICE.txt").is_file())
            for name in flatpak.OPTIONAL_APPSTREAM:
                write(tree / name)
            flatpak.inspect_install_tree(tree)
            for name in (f"share/app-info/icons/flatpak/256x256/{flatpak.APP_ID}.png",
                         "share/app-info/icons/flatpak/64x64/another-app.jxl",
                         "share/app-info/xmls/another-app.xml.gz"):
                write(tree / name)
                with self.assertRaisesRegex(ValueError, "unexpected paths"):
                    flatpak.inspect_install_tree(tree)
                (tree / name).unlink()
            write(tree / "private/disc1/default.xex")
            with self.assertRaisesRegex(ValueError, "unexpected paths"):
                flatpak.inspect_install_tree(tree)
            (tree / "private/disc1/default.xex").unlink()
            (tree / "lib/libdxcompiler.so").unlink()
            (tree / "lib/libdxcompiler.so").symlink_to(write(Path(temp) / "external.so"))
            with self.assertRaisesRegex(ValueError, "symlink leaves export tree"):
                flatpak.inspect_install_tree(tree)
            (tree / "lib/libdxcompiler.so").unlink()
            write(tree / "lib/libdxcompiler.so")
            (tree / "bin/shaders/portable_vk.lospv").rename(tree / "bin/portable_vk.lospv")
            with self.assertRaisesRegex(ValueError, "missing required"):
                flatpak.inspect_install_tree(tree)

    def test_commands_and_source_metadata_without_flatpak(self):
        with checkout() as (repo, ppc, manifest, private, _), tempfile.TemporaryDirectory() as temporary:
            shutil.copytree(ROOT / "packaging/linux", repo / "packaging/linux", dirs_exist_ok=True)
            output = Path(temporary) / "exclusive"
            sdk_roots = {name: (repo / "sdk-inputs" / name) for name in ("ngx", "fsr", "ffmpeg", "zstd")}
            for directory in sdk_roots.values():
                directory.mkdir(parents=True)
            prepared = repo / "prepared-shaders"
            prepared.mkdir()
            shader_pack = write(repo / "pack.lospv")
            args = argparse.Namespace(source=repo, output=output, ppc=ppc, codegen_manifest=manifest,
                                      **private, ngx_sdk=sdk_roots["ngx"], fsr_sdk=sdk_roots["fsr"],
                                      ffmpeg_source=sdk_roots["ffmpeg"], zstd_source=sdk_roots["zstd"],
                                      fsr_shaders=prepared, shader_pack=shader_pack)
            commands = []
            leak_on_finish = False
            runtime_files = (
                "bin/LostOdysseyRecomp", "bin/libnvidia-ngx-dlss.so.310.9.1",
                "bin/shaders/portable_vk.lospv", "lib/libdxcompiler.so",
                "share/licenses/lost-odyssey-recomp/NVIDIA-DLSS/LICENSE.txt",
                "share/licenses/lost-odyssey-recomp/NVIDIA-DLSS/NOTICE.txt",
                "share/licenses/lost-odyssey-recomp/LICENSE-FidelityFX.txt",
                "share/licenses/lost-odyssey-recomp/zstd-LICENSE.txt",
                "share/applications/io.github.freefrank.LostOdysseyRecomp.desktop",
                "share/icons/hicolor/256x256/apps/io.github.freefrank.LostOdysseyRecomp.png",
                "share/metainfo/io.github.freefrank.LostOdysseyRecomp.metainfo.xml",
            )

            def fake_git(directory, *command):
                if directory.resolve() == sdk_roots["fsr"].resolve():
                    raise AssertionError("FSR artifact must not require Git metadata")
                return b"1" * 40

            def fake_run(command):
                commands.append(command)
                files = Path(command[-2]) / "files" if command[0] == "flatpak-builder" else None
                if "--build-only" in command:
                    for name in runtime_files:
                        write(files / name)
                    for alias in ("libnvidia-ngx-dlss.so", "libnvidia-ngx-dlss.so.1"):
                        (files / "bin" / alias).symlink_to("libnvidia-ngx-dlss.so.310.9.1")
                    for name in ("include/fmt/format.h", "lib/libzstd.a", "lib/cmake/fmt/fmt-config.cmake",
                                 "lib/pkgconfig/libzstd.pc"):
                        write(files / name)
                if "--finish-only" in command:
                    # Simulate the fourth build command's bounded cleanup before finish metadata.
                    shutil.rmtree(files / "include")
                    for archive in (files / "lib").glob("*.a"):
                        archive.unlink()
                    shutil.rmtree(files / "lib/cmake")
                    shutil.rmtree(files / "lib/pkgconfig")
                    self.assertFalse((files / "manifest.json").exists())
                    write(files / "manifest.json", "finish metadata")
                    if leak_on_finish:
                        write(files / "private/disc1/default.xex", "private input")
                if "--export-only" in command:
                    flatpak.inspect_install_tree(files)
                if command[:2] == ["flatpak", "build-bundle"]:
                    write(Path(command[3]), "bundle")

            with patch.object(flatpak, "git_output", side_effect=fake_git), \
                  patch.object(flatpak, "stage_checkout", return_value={".": "1" * 40}), \
                  patch.object(flatpak, "stage_git_sdk", side_effect=lambda root, dest, label: flatpak.SDK_COMMITS[label]), \
                   patch.object(flatpak, "stage_fsr_sdk", return_value=flatpak.SDK_COMMITS["fsr"]), \
                   patch.object(flatpak, "stage_shader_inputs"), patch.object(flatpak, "run", side_effect=fake_run):
                result = flatpak.package(args)
                args.output = Path(temporary) / "blocked"
                leak_on_finish = True
                with self.assertRaisesRegex(ValueError, "unexpected paths"):
                    flatpak.package(args)
                self.assertNotIn("--export-only", [part for cmd in commands[4:] for part in cmd])
                args.output = output
            self.assertEqual(["--build-only", "--finish-only", "--export-only"],
                             [next(x for x in cmd if x in ("--build-only", "--finish-only", "--export-only")) for cmd in commands[:3]])
            self.assertTrue(all(f"--state-dir={Path(cmd[-2]).parent / 'builder-state'}" in cmd
                                for cmd in commands[:3]))
            self.assertEqual(commands[3][:2], ["flatpak", "build-bundle"])
            self.assertEqual(commands[3][-2:], [flatpak.APP_ID, "dev"])
            self.assertEqual(result["version"], "0.7.1")
            self.assertTrue(result["dirty"])
            self.assertEqual(result["runtime_ref"], "org.freedesktop.Platform/x86_64/26.08")
            self.assertEqual(result["sdk_commits"], {key: value for key, value in flatpak.SDK_COMMITS.items() if key != "fsr"})
            self.assertEqual(result["fsr_manifest_commit"], flatpak.SDK_COMMITS["fsr"])
            self.assertEqual(result["sdl_patch"], {"path": flatpak.SDL_PATCH.as_posix(),
                                                  "upstream": flatpak.SDL_PATCH_UPSTREAM,
                                                  "sha256": flatpak.sha256(repo / flatpak.SDL_PATCH)})
            self.assertEqual(json.loads((output / "source.json").read_text())["fsr_manifest_commit"], flatpak.SDK_COMMITS["fsr"])
            self.assertEqual(result["sha256"], flatpak.sha256(output / result["bundle"]))
            with self.assertRaisesRegex(ValueError, "already exists"):
                flatpak.package(args)


if __name__ == "__main__":
    unittest.main()
