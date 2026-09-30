"""Invoke the repository-owned ledger implementation on an explicit private archive."""
from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("sync", "report", "review"))
    parser.add_argument("--archive", type=Path, required=True, help="Private archive directory (data stays separate from this skill)")
    parser.add_argument("--ledger", type=Path)
    parser.add_argument("--seed", type=Path)
    parser.add_argument("--context", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--case")
    parser.add_argument("--file", type=Path)
    args = parser.parse_args()
    archive = args.archive.resolve()
    program = Path(__file__).resolve().parents[3] / "scripts" / "feedback_ledger.py"
    if not program.is_file():
        parser.error("Ledger implementation not found; keep this skill inside tools/feedback_archive/skills")
    ledger = (args.ledger or archive / "feedback" / "triage" / "ledger.json").resolve()
    command = [sys.executable, "-B", str(program), args.command, "--ledger", str(ledger)]
    if args.command == "sync":
        if args.output or args.case or args.file:
            parser.error("sync does not accept --output, --case or --file")
        command += ["--archive", str(archive)]
        for name, value in (("seed", args.seed), ("context", args.context)):
            selected = (value or archive / "feedback" / "triage" / (name + ".json")).resolve()
            if not selected.is_file():
                if value:
                    parser.error(f"Missing {name} file: {selected}")
                continue
            command += ["--" + name, str(selected)]
    elif args.command == "report":
        if args.seed or args.context or args.case or args.file:
            parser.error("report does not accept review or sync options")
        if args.output:
            command += ["--output", str(args.output.resolve())]
    else:
        if args.seed or args.context or args.output:
            parser.error("review does not accept --seed, --context or --output")
        if not args.case or not args.file:
            parser.error("review requires --case and --file")
        command += ["--case", args.case, "--file", str(args.file.resolve())]
    return subprocess.run(command, check=False).returncode


if __name__ == "__main__":
    raise SystemExit(main())
