"""Check a versioned Windows release ZIP against its sidecar and linked manifest.

This checks archive identity and the packaged provenance. Packaging already
hashes every payload; this reader does not rehash or run any payload.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import zipfile


def archive_digest(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def verify_package(package, version, commit):
    package = Path(package).resolve()
    if package.name != f"LostOdysseyRecomp-windows-x64-{version}.zip":
        raise ValueError(f"Unexpected release ZIP name: {package.name}")
    sidecar = Path(str(package) + ".sha256")
    fields = sidecar.read_text(encoding="utf-8").split()
    if len(fields) != 2 or fields[1] != package.name or not re.fullmatch(r"[0-9a-fA-F]{64}", fields[0]):
        raise ValueError(f"Invalid release checksum sidecar: {sidecar}")
    checksum = archive_digest(package)
    if checksum != fields[0].lower():
        raise ValueError(f"Release ZIP checksum mismatch: {package.name}")

    prefix = package.stem + "/"
    manifest_name = prefix + "manifest.json"
    with zipfile.ZipFile(package) as archive:
        entries = archive.infolist()
        names = [entry.filename for entry in entries if not entry.is_dir()]
        if len(names) != len(set(names)) or manifest_name not in names:
            raise ValueError("Release ZIP lacks a unique packaged manifest")
        if any(not entry.filename.startswith(prefix) or "\\" in entry.filename or
               any(part in ("", ".", "..") for part in entry.filename.rstrip("/").split("/"))
               for entry in entries):
            raise ValueError("Release ZIP has an unexpected archive path")
        manifest = json.loads(archive.read(manifest_name))

    if not isinstance(manifest, dict) or any(manifest.get(key) != commit for key in
                                               ("commit", "build_commit", "packaging_commit")):
        raise ValueError("Release manifest commit differs from requested build")
    if (manifest.get("version") != version or manifest.get("source_version") != version[1:] or
            manifest.get("dirty") is not False or manifest.get("development_build") is not False):
        raise ValueError("Release manifest version or formal source state differs")
    packaging_source = manifest.get("packaging_source")
    if (not isinstance(packaging_source, dict) or packaging_source.get("commit") != commit or
            packaging_source.get("dirty") is not False):
        raise ValueError("Packaging source does not match the release commit")
    files = manifest.get("files")
    if (not isinstance(files, dict) or "LostOdysseyRecomp.exe" not in files or
            any(not isinstance(name, str) or not name or name.startswith("/") or "\\" in name or
                any(part in ("", ".", "..") for part in name.split("/")) or
                not isinstance(digest, str) or not re.fullmatch(r"[0-9a-fA-F]{64}", digest)
                for name, digest in files.items())):
        raise ValueError("Release manifest has invalid file entries")
    if set(names) != {manifest_name, *(prefix + name for name in files)}:
        raise ValueError("Release ZIP entries differ from its manifest")
    stamps = manifest.get("build_provenance")
    if (not isinstance(stamps, list) or len(stamps) != 1 or not isinstance(stamps[0], dict) or
            stamps[0].get("binary") != "LostOdysseyRecomp.exe" or
            stamps[0].get("source_version") != version[1:] or
            stamps[0].get("binary_sha256") != files["LostOdysseyRecomp.exe"] or
            not isinstance(stamps[0].get("source"), dict) or
            stamps[0]["source"].get("commit") != commit or stamps[0]["source"].get("dirty") is not False):
        raise ValueError("Linked runtime provenance does not match the release manifest")

    return {"package": str(package), "version": version, "commit": commit,
            "size": package.stat().st_size, "sha256": checksum, "manifest_files": len(files),
            "payload_hashes_rechecked": False}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--assets", type=Path, help="directory containing the versioned Windows release ZIP")
    source.add_argument("--package", type=Path, help="versioned Windows release ZIP")
    parser.add_argument("--version", required=True, help="release tag, for example v0.6.15")
    parser.add_argument("--commit", required=True, help="full 40-character build/packaging Git commit")
    parser.add_argument("--output", type=Path, required=True, help="write the verification report JSON here")
    args = parser.parse_args(argv)
    if not re.fullmatch(r"v[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?", args.version):
        parser.error("--version must be a version tag such as v0.6.15")
    if not re.fullmatch(r"[0-9a-fA-F]{40}", args.commit):
        parser.error("--commit must be a full 40-character Git commit")
    package = (args.assets / f"LostOdysseyRecomp-windows-x64-{args.version}.zip"
               if args.assets else args.package)
    output = args.output.resolve()
    if output in (package.resolve(), Path(str(package.resolve()) + ".sha256")):
        parser.error("--output cannot overwrite a release asset or its checksum sidecar")
    try:
        report = verify_package(package, args.version, args.commit.lower())
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    except (OSError, ValueError, TypeError, AttributeError, zipfile.BadZipFile) as error:
        parser.exit(1, f"package verification failed: {error}\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
