"""Point .forge.json (the PortForge manifest) at a published release.

    python tools/release/update_forge_manifest.py v0.9.27

Run it after the release is public: every download URL in the manifest is
checked with a HEAD request before the file is written.
"""
import argparse
import json
import re
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("tag", help="release tag, e.g. v0.9.27")
    parser.add_argument("--manifest", type=Path, default=ROOT / ".forge.json")
    args = parser.parse_args()
    if not re.fullmatch(r"v\d+\.\d+\.\d+", args.tag):
        parser.error("the tag must look like v0.9.27")
    new = args.tag[1:]
    text = args.manifest.read_text(encoding="utf-8")
    old = json.loads(text)["defaultVersion"]
    # Plain replacement keeps the file's hand-made layout.
    text = text.replace(f'"{old}"', f'"{new}"').replace(f"v{old}", args.tag)
    data = json.loads(text)
    for build in data["builds"]:
        if build["versions"] != [new]:
            raise SystemExit(f"{args.manifest}: a build still lists {build['versions']}")
        for step in build["steps"]:
            if step["step"] == "fetch":
                request = urllib.request.Request(step["url"], method="HEAD")
                with urllib.request.urlopen(request) as response:
                    print(f"{response.status} {step['url']}")
    args.manifest.write_text(text, encoding="utf-8", newline="\n")
    print(f"{args.manifest.name}: {old} -> {new}")


if __name__ == "__main__":
    main()
