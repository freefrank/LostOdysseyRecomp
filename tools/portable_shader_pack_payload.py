"""Stage only the portable distribution artifact, never local cache directories."""
from __future__ import annotations
import os
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]


def stage_portable_shader_pack(runtime_directory: Path, executable_directory: Path,
                               licenses: Path) -> dict | None:
    licenses.mkdir(parents=True, exist_ok=True)
    shutil.copy2(ROOT / "thirdparty/zstd-LICENSE.txt", licenses / "zstd-LICENSE.txt")
    env_override = os.environ.get("LO_PORTABLE_SHADER_PACK")
    source = Path(env_override) if env_override else runtime_directory / "shaders/portable_vk.lospv"
    if not source.exists():
        return None
    if not source.is_file() or not source.stat().st_size:
        raise SystemExit("Portable shader pack must be a nonempty regular file")
    destination = executable_directory / "shaders/portable_vk.lospv"
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)
    size = destination.stat().st_size
    print(f"Portable shaders: {size} distributed bytes")
    return {"file_bytes": size}
