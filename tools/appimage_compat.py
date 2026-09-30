"""Validate the ABI of the actual AppDir, including dlopen-only vendor libraries.

This is a packaging check, not a GPU/game test. Never infer a glibc requirement
from version definitions: only the ELF version *needs* section describes it.
"""
import os
import re
import shlex
import subprocess
from pathlib import Path

BASELINE_GLIBC = "2.35"  # Ubuntu 22.04; build every dependency on this baseline.
GCC_LIBRARIES = ("libstdc++.so.6", "libgcc_s.so.1")


class CompatibilityError(RuntimeError):
    pass


def run(*command):
    result = subprocess.run(command, env={**os.environ, "LC_ALL": "C"},
                            text=True, capture_output=True, timeout=60)
    if result.returncode:
        raise CompatibilityError(
            f"Command failed: {shlex.join(map(str, command))}\n"
            f"{result.stdout}{result.stderr}")
    return result.stdout


def compiler_libraries(compiler):
    """Use the build compiler's runtime, not a hard-coded host library path."""
    command = shlex.split(compiler)
    if not command:
        raise CompatibilityError("An explicit C++ compiler is required")
    command += shlex.split(os.environ.get("CXXFLAGS", ""))
    libraries = []
    for name in GCC_LIBRARIES:
        path = Path(run(*command, f"-print-file-name={name}").strip())
        if not path.is_absolute() or not path.is_file():
            raise CompatibilityError(f"{compiler} could not locate {name}: {path}")
        libraries.append(path)
    return libraries


def version_sections(text):
    """Return {provider: required versions}, and locally defined versions."""
    needs, definitions = {}, set()
    section = provider = None
    for line in text.splitlines():
        if line.startswith("Version needs section"):
            section, provider = "needs", None
        elif line.startswith("Version definition section"):
            section, provider = "definitions", None
        elif line.startswith("Version "):
            section, provider = None, None
        if section == "needs":
            match = re.search(r"\bFile:\s+(\S+)", line)
            if match:
                provider = match[1]
            match = re.search(r"\bName:\s+(\S+)", line)
            if match and provider:
                needs.setdefault(provider, set()).add(match[1])
        elif section == "definitions":
            match = re.search(r"\bName:\s+(\S+)", line)
            if match:
                definitions.add(match[1])
    return needs, definitions


def check_glibc(needs, label):
    limit = tuple(map(int, BASELINE_GLIBC.split(".")))
    for versions in needs.values():
        for version in sorted(versions):
            if not version.startswith("GLIBC_"):
                continue
            number = version.removeprefix("GLIBC_")
            # Also reject GLIBC_PRIVATE / GLIBC_ABI_DT_RELR; these cannot be
            # silently accepted as if they were compatible numeric versions.
            if not re.fullmatch(r"[0-9]+(?:\.[0-9]+)+", number) or \
                    tuple(map(int, number.split("."))) > limit:
                raise CompatibilityError(
                    f"{label} requires {version}; AppImage baseline is GLIBC_{BASELINE_GLIBC}. "
                    "Rebuild this binary/dependency on Ubuntu 22.04; repacking is insufficient.")


def appdir_elfs(appdir):
    for path in sorted(appdir.rglob("*")):
        if path.is_symlink() or not path.is_file():
            continue
        with path.open("rb") as stream:
            header = stream.read(20)
        if not header.startswith(b"\x7fELF"):
            continue
        if len(header) != 20 or header[4:6] != b"\x02\x01" or header[18:20] != b"\x3e\x00":
            raise CompatibilityError(f"Expected an x86_64 ELF: {path}")
        yield path


def validate_abi(appdir):
    appdir = appdir.resolve()
    elfs = list(appdir_elfs(appdir))
    if not elfs:
        raise CompatibilityError("AppDir contains no ELF binaries")
    info = {}
    for path in elfs:
        needs, definitions = version_sections(run("readelf", "--wide", "--version-info", str(path)))
        check_glibc(needs, path.relative_to(appdir))
        info[path.resolve()] = (needs, definitions)
    for name in GCC_LIBRARIES:
        library = appdir / "usr/lib" / name
        if not library.is_file() or not library.resolve().is_relative_to(appdir):
            raise CompatibilityError(f"Missing bundled compiler runtime: {name}")
        if library.resolve() not in info:
            raise CompatibilityError(f"Bundled compiler runtime is not ELF: {name}")
        exports = info[library.resolve()][1]
        for path, (needs, _) in info.items():
            missing = needs.get(name, set()) - exports
            if missing:
                raise CompatibilityError(
                    f"{path.relative_to(appdir)} needs {sorted(missing)}, "
                    f"which the bundled {name} does not provide")
    print(f"AppImage ABI: {len(elfs)} ELF files checked; GLIBC <= {BASELINE_GLIBC}; GCC runtimes bundled")


def validate_loader(appdir):
    """Resolve dependencies without running the program or initializing a GPU.

    Run on the extracted final AppImage. Checking the build tree alone would
    miss missing libraries/RPATHs and permit an upgraded CI host to hide them.
    """
    appdir = appdir.resolve()
    main = appdir / "usr/bin/LostOdysseyRecomp"
    # Do not let a developer/CI LD_LIBRARY_PATH or LD_PRELOAD mask package bugs.
    env = {key: value for key, value in os.environ.items()
           if key not in ("LD_LIBRARY_PATH", "LD_PRELOAD", "LD_AUDIT")}
    env["LC_ALL"] = "C"
    for binary in appdir_elfs(appdir):
        dynamic = run("readelf", "--wide", "--dynamic", str(binary))
        if "(NEEDED)" not in dynamic:
            continue
        result = subprocess.run(["ldd", str(binary)], env=env, text=True,
                                capture_output=True, timeout=60)
        output = result.stdout + result.stderr
        if result.returncode or "not found" in output:
            raise CompatibilityError(f"Unresolved AppImage dependencies in {binary}:\n{output}")
        if binary == main:
            for name in GCC_LIBRARIES:
                match = re.search(r"^\s*" + re.escape(name) + r" => (.+?) \(0x[0-9a-f]+\)",
                                  output, re.MULTILINE)
                if not match or not Path(match[1]).resolve().is_relative_to(appdir):
                    raise CompatibilityError(f"AppImage does not load its bundled {name}:\n{output}")
    print("AppImage loader: all shipped ELF dependencies resolve without LD_LIBRARY_PATH")
