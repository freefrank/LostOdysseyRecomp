"""Verify a macOS DMG and optionally compare its payload with a release baseline.

Run on an Apple Silicon Mac. The only executable launch is a no-game-data,
headless startup check, which must stop at the missing-default.xex guard.
Raw tool output, a readable parity diff and JSON results remain in --output.
"""

import argparse
from contextlib import contextmanager
import difflib
import json
import os
from pathlib import Path
import platform
import plistlib
import re
import shutil
import subprocess
import sys
import tempfile
import time


BUNDLE_NAME = "LostOdysseyRecomp.app"
VERSION_KEYS = {"CFBundleShortVersionString", "CFBundleVersion"}
MINIMUM_MACOS = "15.0"


class VerificationError(RuntimeError):
    pass


class Evidence:
    def __init__(self, output):
        self.output = output
        self.commands = []
        output.mkdir(parents=True, exist_ok=True)

    def write_json(self, name, value):
        (self.output / name).write_text(
            json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    def run(self, label, command, *, cwd=None, env=None, timeout=120, check=True):
        command = [str(part) for part in command]
        started = time.monotonic()
        try:
            result = subprocess.run(command, cwd=cwd, env=env, capture_output=True,
                                    timeout=timeout)
        except subprocess.TimeoutExpired as error:
            self._record(label, command, started, None, error.stdout or b"",
                         error.stderr or b"", timed_out=True)
            raise VerificationError(f"{label} timed out after {timeout}s") from error
        self._record(label, command, started, result.returncode, result.stdout, result.stderr)
        if check and result.returncode:
            raise VerificationError(f"{label} exited {result.returncode}; see {label}.stderr.txt")
        return result

    def _record(self, label, command, started, returncode, stdout, stderr, timed_out=False):
        (self.output / f"{label}.stdout.txt").write_bytes(stdout)
        (self.output / f"{label}.stderr.txt").write_bytes(stderr)
        self.commands.append({"label": label, "command": command, "returncode": returncode,
                              "seconds": round(time.monotonic() - started, 3),
                              "timed_out": timed_out})


@contextmanager
def mounted_image(evidence, label, dmg, scratch):
    evidence.run(f"{label}-hdiutil-verify", ["hdiutil", "verify", dmg], timeout=300)
    mount = scratch / label
    mount.mkdir()
    attached = False
    try:
        result = evidence.run(f"{label}-attach", ["hdiutil", "attach", "-readonly", "-nobrowse",
                              "-noautoopen", "-mountpoint", mount, "-plist", dmg])
        attached = True
        entities = plistlib.loads(result.stdout).get("system-entities", [])
        # macOS reports /private/var even when TMPDIR uses the /var symlink.
        if not any(entity.get("mount-point") and
                   Path(entity["mount-point"]).resolve() == mount.resolve()
                   for entity in entities):
            raise VerificationError(f"{label}: hdiutil did not mount at {mount}")
        yield mount
    finally:
        if attached:
            result = evidence.run(f"{label}-detach", ["hdiutil", "detach", mount], check=False)
            if result.returncode:
                evidence.run(f"{label}-detach-force", ["hdiutil", "detach", "-force", mount])


def file_inventory(root):
    """List every payload file and symlink; never follow the /Applications link."""
    entries = []

    def failed(error):
        raise error

    for directory, directories, files in os.walk(root, followlinks=False, onerror=failed):
        base = Path(directory)
        for name in directories + files:
            path = base / name
            relative = path.relative_to(root).as_posix()
            if path.is_symlink():
                entries.append(f"symlink {relative} -> {os.readlink(path)}")
            elif path.is_file():
                entries.append(f"file {relative}")
    return sorted(entries)


def binary_metadata(evidence, label, binary):
    architectures = evidence.run(f"{label}-architectures", ["lipo", "-archs", binary])
    architectures = architectures.stdout.decode().strip().split()
    libraries = evidence.run(f"{label}-otool-libraries", ["otool", "-arch", "arm64", "-L", binary])
    dependencies = {}
    for line in libraries.stdout.decode().splitlines()[1:]:
        match = re.fullmatch(r"\s+(.+) \(compatibility version ([^,]+), current version ([^)]+)\)", line)
        if not match:
            raise VerificationError(f"{label}: unrecognized otool dependency: {line!r}")
        if match[1] in dependencies:
            raise VerificationError(f"{label}: duplicate linked library {match[1]}")
        dependencies[match[1]] = {"compatibility": match[2], "current": match[3]}

    commands = evidence.run(f"{label}-otool-load-commands", ["otool", "-arch", "arm64", "-l", binary])
    builds = []
    for block in re.split(r"Load command \d+\n", commands.stdout.decode()):
        if re.search(r"\bcmd LC_BUILD_VERSION\b", block):
            fields = dict(re.findall(r"^\s*(platform|minos|sdk)\s+(\S+)\s*$", block, re.MULTILINE))
            builds.append(fields)
        elif re.search(r"\bcmd LC_VERSION_MIN_MACOSX\b", block):
            fields = dict(re.findall(r"^\s*(version|sdk)\s+(\S+)\s*$", block, re.MULTILINE))
            builds.append({"platform": "macos", "minos": fields.get("version"), "sdk": fields.get("sdk")})
    if len(builds) != 1 or not builds[0].get("minos") or not builds[0].get("sdk"):
        raise VerificationError(f"{label}: expected exactly one macOS minimum-OS load command")
    if builds[0]["platform"].lower() not in ("1", "macos"):
        raise VerificationError(f"{label}: binary platform is not macOS: {builds[0]}")
    return {"architectures": architectures, "dependencies": dependencies, **builds[0]}


def inspect_image(evidence, label, mount):
    bundle = mount / BUNDLE_NAME
    plist_path = bundle / "Contents/Info.plist"
    if not plist_path.is_file():
        raise VerificationError(f"{label}: {BUNDLE_NAME}/Contents/Info.plist is missing")
    shutil.copyfile(plist_path, evidence.output / f"{label}-Info.plist")
    with plist_path.open("rb") as source:
        plist = plistlib.load(source)
    if plist.get("CFBundleExecutable") != "LostOdysseyRecomp":
        raise VerificationError(f"{label}: unexpected CFBundleExecutable")
    binary = bundle / "Contents/MacOS/LostOdysseyRecomp"
    inventory = file_inventory(mount)
    (evidence.output / f"{label}-files.txt").write_text("\n".join(inventory) + "\n", encoding="utf-8")
    metadata = binary_metadata(evidence, label, binary)
    report = {"files": inventory, "plist": plist, "binary": metadata}
    evidence.write_json(f"{label}.json", report)
    return report


def verify_signature(evidence, bundle):
    evidence.run("candidate-codesign-verify", ["codesign", "--verify", "--strict", "--deep", bundle])
    for label, code in (("app", bundle), ("dxc", bundle / "Contents/MacOS/libdxcompiler.dylib")):
        result = evidence.run(f"candidate-codesign-{label}", ["codesign", "-d", "--verbose=4", code])
        display = (result.stdout + result.stderr).decode()
        if not re.search(r"^Signature=adhoc$", display, re.MULTILINE):
            raise VerificationError(f"{label}: expected an ad-hoc code signature")
        if re.search(r"^Authority=", display, re.MULTILINE):
            raise VerificationError(f"{label}: unexpected signing authority in an ad-hoc package")


def headless_load(evidence, bundle, expected_version, scratch, timeout):
    home = scratch / "headless-home"
    working = scratch / "headless-cwd"
    home.mkdir()
    working.mkdir()
    log = evidence.output / "headless-runtime.log"
    # Startup logs all LO_* variables. Strip inherited switches (including any
    # workflow credentials), plus dyld overrides, before setting only this test's.
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(("LO_", "DYLD_"))}
    env.update({"HOME": str(home), "LO_HEADLESS": "1", "LO_LOG_FILE": str(log)})
    # Match settings/game_path.h's executable-relative fallbacks. Refuse the
    # launch if a malformed package contains or points at any usable game data.
    executables = bundle / "Contents/MacOS"
    for relative in ("game/disc1", "game", ".", "../game/disc1", "../game",
                     "../../../LostOdysseyRecompLib/private/disc1",
                     "LostOdysseyRecompLib/private/disc1"):
        if (executables / relative / "default.xex").exists():
            raise VerificationError("Headless load refused: package has reachable default.xex")
    result = evidence.run("headless-load", [bundle / "Contents/MacOS/LostOdysseyRecomp"],
                          cwd=working, env=env, timeout=timeout, check=False)
    text = log.read_text(encoding="utf-8", errors="replace") if log.is_file() else ""
    checks = {"exit_code_1": result.returncode == 1,
              "missing_game_guard": "missing default.xex in game root: headless or background environment prevents opening installer UI" in text,
              "source_version": f"source version: {expected_version}" in text,
              "macos_host": "host OS: macOS version=" in text}
    evidence.write_json("headless-load.json", {"returncode": result.returncode, "checks": checks})
    if not all(checks.values()):
        raise VerificationError(f"headless no-data load failed: {checks}")


def readable_diff(name, reference, candidate):
    return list(difflib.unified_diff(reference, candidate, fromfile=f"reference/{name}",
                                     tofile=f"candidate/{name}", lineterm=""))


def compare_images(evidence, reference, candidate):
    failures = []
    differences = []
    allowed = []
    if reference["files"] != candidate["files"]:
        failures.append("Package file/symlink lists differ; no payload files are ignored.")
        differences += readable_diff("files", reference["files"], candidate["files"])

    reference_plist = reference["plist"].copy()
    candidate_plist = candidate["plist"].copy()
    for key in sorted(VERSION_KEYS):
        before = reference_plist.pop(key, None)
        after = candidate_plist.pop(key, None)
        if before != after:
            allowed.append(f"Info.plist {key}: {before!r} -> {after!r}; source version differs from the baseline.")
    if reference_plist != candidate_plist:
        failures.append("Info.plist differs outside CFBundleShortVersionString/CFBundleVersion.")
        differences += readable_diff("Info.plist", json.dumps(reference_plist, indent=2, sort_keys=True).splitlines(),
                                     json.dumps(candidate_plist, indent=2, sort_keys=True).splitlines())

    before = reference["binary"]
    after = candidate["binary"]
    for key in ("architectures", "minos"):
        if before[key] != after[key]:
            failures.append(f"Binary {key} differs: {before[key]!r} -> {after[key]!r}.")
    if before["sdk"] != after["sdk"]:
        allowed.append(f"Binary SDK: {before['sdk']} -> {after['sdk']}; build hosts use different Xcode SDKs.")
    if before["dependencies"].keys() != after["dependencies"].keys():
        failures.append("Linked library paths differ; missing or additional libraries are not allowed.")
    for library in sorted(before["dependencies"].keys() & after["dependencies"].keys()):
        old = before["dependencies"][library]
        new = after["dependencies"][library]
        if old != new:
            if library.startswith(("/System/Library/", "/usr/lib/")):
                allowed.append(f"{library}: {old} -> {new}; system-library compatibility/current versions come from the linked SDK.")
            else:
                failures.append(f"Non-system library version differs: {library}: {old} -> {new}.")
    differences += readable_diff("binary", json.dumps(before, indent=2, sort_keys=True).splitlines(),
                                 json.dumps(after, indent=2, sort_keys=True).splitlines())
    report = {"passed": not failures, "failures": failures, "explained_differences": allowed}
    evidence.write_json("parity.json", report)
    lines = ["Parity: " + ("PASS" if not failures else "FAIL"), "",
             *failures, *allowed, "", *differences]
    (evidence.output / "parity.diff.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(lines))
    if failures:
        raise VerificationError("DMG parity failed; see parity.diff.txt")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dmg", type=Path, required=True)
    parser.add_argument("--expected-version", required=True, help="Exact source-version.txt / Info.plist version")
    parser.add_argument("--output", type=Path, required=True, help="Raw evidence and verification results directory")
    parser.add_argument("--compare", type=Path, help="Reference release DMG; read-only comparison")
    parser.add_argument("--headless-timeout", type=int, default=60)
    args = parser.parse_args()
    evidence = Evidence(args.output.resolve())
    summary = {"passed": False, "dmg": str(args.dmg.resolve()),
               "expected_version": args.expected_version, "minimum_macos": MINIMUM_MACOS,
               "reference": str(args.compare.resolve()) if args.compare else None}
    try:
        if sys.platform != "darwin" or platform.machine() != "arm64":
            raise VerificationError("Run this verifier on an Apple Silicon macOS runner")
        if args.headless_timeout <= 0:
            raise VerificationError("--headless-timeout must be positive")
        with tempfile.TemporaryDirectory(prefix="lo-macos-package-") as temporary:
            scratch = Path(temporary)
            with mounted_image(evidence, "candidate", args.dmg.resolve(), scratch) as mount:
                candidate = inspect_image(evidence, "candidate", mount)
                plist = candidate["plist"]
                for key in VERSION_KEYS:
                    if plist.get(key) != args.expected_version:
                        raise VerificationError(f"{key}: expected {args.expected_version!r}, got {plist.get(key)!r}")
                if plist.get("LSMinimumSystemVersion") != MINIMUM_MACOS:
                    raise VerificationError(f"Info.plist LSMinimumSystemVersion must be {MINIMUM_MACOS}")
                if candidate["binary"]["architectures"] != ["arm64"]:
                    raise VerificationError("Main binary must contain only the arm64 architecture")
                if candidate["binary"]["minos"] != MINIMUM_MACOS:
                    raise VerificationError(f"Main binary minos must be exactly {MINIMUM_MACOS}")
                bundle = mount / BUNDLE_NAME
                verify_signature(evidence, bundle)
                headless_load(evidence, bundle, args.expected_version, scratch, args.headless_timeout)
                if args.compare:
                    with mounted_image(evidence, "reference", args.compare.resolve(), scratch) as reference_mount:
                        reference = inspect_image(evidence, "reference", reference_mount)
                        compare_images(evidence, reference, candidate)
        summary["passed"] = True
        print("macOS package verification passed")
    except (VerificationError, OSError, ValueError, plistlib.InvalidFileException) as error:
        summary["error"] = str(error)
        print(f"macOS package verification failed: {error}", file=sys.stderr)
    finally:
        summary["commands"] = evidence.commands
        evidence.write_json("summary.json", summary)
    return 0 if summary["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
