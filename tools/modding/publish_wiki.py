#!/usr/bin/env python3
"""Stage the managed mod pages in an already-cloned Wiki; never deletes pages."""
from __future__ import annotations

import argparse
import re
from pathlib import Path

PAGES = ("Modding", "Creating-Mods", "Modding-API", "Mod-Organizer-2",
         "Runtime-Texture-Replacement", "Modding-Validation")
BEGIN, END = "<!-- LO-MODS:BEGIN -->", "<!-- LO-MODS:END -->"


def navigation(existing: str, block: str) -> str:
    if BEGIN in existing or END in existing:
        if existing.count(BEGIN) != 1 or existing.count(END) != 1 or existing.index(END) < existing.index(BEGIN):
            raise ValueError("malformed managed Wiki navigation block; refusing to overwrite it")
        first, last = existing.index(BEGIN), existing.index(END) + len(END)
        return existing[:first] + block + existing[last:]
    return existing + ("\n" if existing and not existing.endswith("\n") else "") + "\n" + block + "\n"


def stage(source: Path, wiki: Path, repository: str, revision: str) -> list[str]:
    if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repository):
        raise ValueError("invalid repository name")
    if not re.fullmatch(r"[0-9a-f]{40}", revision):
        raise ValueError("revision must be the full source commit SHA")
    if not (wiki / ".git").is_dir():
        raise ValueError("destination must be an already-cloned Wiki Git repository")
    url = f"https://github.com/{repository}"
    plan = {}
    for name in PAGES:
        content = (source / (name + ".md")).read_text(encoding="utf-8")
        def link(match: re.Match) -> str:
            page, anchor = match.group(1), match.group(2) or ""
            return f"]({url}/wiki/{page}{anchor})" if page in PAGES else match.group(0)
        content = re.sub(r"\]\(([A-Za-z0-9_-]+)\.md(#[^)]+)?\)", link, content)
        banner = f"> Source: [{revision[:12]}]({url}/blob/{revision}/docs/wiki/{name}.md). Check the source page for feature scope.\n\n"
        plan[name + ".md"] = banner + content
    links = "\n".join(f"- [{name.replace('-', ' ')}]({url}/wiki/{name})" for name in PAGES)
    block = f"{BEGIN}\n## Modding\n\n{links}\n{END}"
    for filename in ("Home.md", "_Sidebar.md"):
        target = wiki / filename
        existing = target.read_text(encoding="utf-8") if target.exists() else ("# LostOdysseyRecomp\n" if filename == "Home.md" else "")
        plan[filename] = navigation(existing, block)
    # Validate every destination before writing anything.
    for name in plan:
        if (wiki / name).is_symlink():
            raise ValueError("refusing to replace a Wiki symlink: " + name)
    changed = []
    for name, content in plan.items():
        target = wiki / name
        if not target.exists() or target.read_text(encoding="utf-8") != content:
            with target.open("w", encoding="utf-8", newline="\n") as file:
                file.write(content)
            changed.append(name)
    return changed


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--wiki", type=Path, required=True)
    parser.add_argument("--repository", required=True)
    parser.add_argument("--revision", required=True)
    args = parser.parse_args()
    for page in stage(args.source, args.wiki, args.repository, args.revision):
        print("Updated " + page)


if __name__ == "__main__":
    main()
