#!/usr/bin/env python3
"""Install this repository's maintained OpenCode render workflow locally."""
import argparse
from pathlib import Path


def install(destination: Path, overwrite: bool = False) -> list[str]:
    source = Path(__file__).resolve().parent
    files = [Path("skills/lo-render-flicker/SKILL.md")]
    files += [Path("agents") / f"lo-render-{role}.md"
              for role in ("investigator", "fixer", "reviewer")]
    pending = []
    # Preflight every collision before writing any file. Never copy local configs.
    for relative in files:
        target = destination / relative
        content = (source / relative).read_bytes()
        if target.exists():
            if target.read_bytes() == content:
                continue
            if not overwrite:
                raise ValueError(f"Different file already exists: {target}; use --overwrite to update")
        pending.append((target, content))
    for target, content in pending:
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(content)
    return [str(target) for target, _ in pending]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True,
                        help="OpenCode directory, normally repository/.opencode")
    parser.add_argument("--overwrite", action="store_true", help="Update these four managed files")
    args = parser.parse_args()
    try:
        changed = install(args.output, args.overwrite)
    except (ValueError, OSError) as error:
        parser.exit(1, f"{error}\n")
    print("\n".join(changed) if changed else "Already current")


if __name__ == "__main__":
    main()
