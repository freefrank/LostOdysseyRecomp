"""Linux regression: compile the real apply entry point and exercise restart/rollback."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    repo = Path(__file__).resolve().parents[2]
    source = repo / "LostOdysseyRecomp"
    with tempfile.TemporaryDirectory(prefix="lo-appimage-cleanup-") as temporary:
        root = Path(temporary)
        probe_source = root / "probe.cpp"
        probe_source.write_text(
            '#include "updater/apply_mode.h"\n'
            'int main() { return updater::TryRunApplyMode().value_or(0); }\n'
        )
        probe = root / "probe"
        subprocess.run([
            "g++", "-std=c++20", "-I", str(source), str(probe_source),
            str(source / "updater/apply_mode.cpp"),
            str(source / "updater/update.cpp"),
            "-o", str(probe),
        ], check=True)
        app = root / "Lost Odyssey.AppImage"
        previous = Path(str(app) + ".previous")
        unrelated = root / "other.AppImage.previous"
        unrelated.write_bytes(b"unrelated")
        env = dict(os.environ, APPIMAGE=str(app), XDG_STATE_HOME=str(root / "state"))

        # Existing updater versions leave a backup without a completion marker.
        shutil.copyfile(probe, app)
        previous.write_bytes(b"legacy backup")
        subprocess.run([probe], env=env, check=True)
        assert not previous.exists() and unrelated.read_bytes() == b"unrelated"

        # Ordinary non-AppImage launches never touch adjacent files.
        previous.write_bytes(b"keep")
        plain_env = dict(env)
        plain_env.pop("APPIMAGE")
        subprocess.run([probe], env=plain_env, check=True)
        assert previous.read_bytes() == b"keep"
        previous.unlink()

        # Never traverse a symlink or remove a directory bearing the backup name.
        previous.symlink_to(unrelated)
        subprocess.run([probe], env=env, check=True)
        assert previous.is_symlink() and unrelated.read_bytes() == b"unrelated"
        previous.unlink()
        previous.mkdir()
        subprocess.run([probe], env=env, check=True)
        assert previous.is_dir()
        previous.rmdir()

        for succeeds in (True, False):
            old_bytes = b"previous installation"
            app.write_bytes(old_bytes)
            operation = root / "state/lost-odyssey-recomp/.update/operation-test"
            stage = operation / "stage"
            stage.mkdir(parents=True)
            payload = stage / "release.AppImage"
            payload.write_bytes(probe.read_bytes() if succeeds else b"invalid executable")
            plan = operation / "apply-plan.json"
            plan.write_text(json.dumps({
                "schema": 1, "version": "v9.9.9", "install_root": str(root),
                "stage_root": str(stage), "executable": str(app), "launch_arguments": [],
                "files": {payload.name: None},
            }))
            result = subprocess.run(
                [probe, "--apply-plan", plan, "--wait-process", "1"], env=env,
                capture_output=True, text=True,
            )
            assert result.returncode == (0 if succeeds else 1), result.stderr
            assert not previous.exists()
            assert app.read_bytes() == (probe.read_bytes() if succeeds else old_bytes)
            if not succeeds:
                assert "previous installation restored" in result.stderr
            assert unrelated.read_bytes() == b"unrelated"
        print("PASS: legacy cleanup, non-AppImage launch, symlink/directory preservation, successful restart, failed exec rollback")


if __name__ == "__main__":
    main()
