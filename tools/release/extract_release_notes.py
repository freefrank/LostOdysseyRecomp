"""Extract one version's ATX-heading section from the repository changelog."""

import argparse
from pathlib import Path
import re
import sys


VERSION = r'v[0-9][0-9A-Za-z.+-]*'
HEADING = re.compile(r'^ {0,3}(#{1,6})(?:[ \t]+(.*)|[ \t]*)$')
VERSION_HEADING = re.compile(
    rf'^(?:\[({VERSION})\]\([^)]*\)|({VERSION}))(?=\s|$)'
)
FENCE = re.compile(r'^ {0,3}(`{3,}|~{3,})(.*)$')


def extract_release_notes(changelog: str, version: str) -> str:
    """Return the body below exactly one matching version heading.

    Nested headings belong to the release body. A heading of the same or a
    higher level ends it. Fenced code is preserved without treating its sample
    headings as release boundaries. Only surrounding blank lines are removed.
    """
    if not re.fullmatch(VERSION, version):
        raise ValueError(f'Invalid version tag: {version!r}')

    lines = changelog.splitlines(keepends=True)
    headings = []
    fence_char = ''
    fence_length = 0
    for index, line in enumerate(lines):
        text = line.rstrip('\r\n')
        fence = FENCE.match(text)
        if fence_char:
            if (fence and fence[1][0] == fence_char
                    and len(fence[1]) >= fence_length and not fence[2].strip()):
                fence_char = ''
            continue
        if fence and (fence[1][0] != '`' or '`' not in fence[2]):
            fence_char, fence_length = fence[1][0], len(fence[1])
            continue
        heading = HEADING.match(text)
        if heading:
            title = re.sub(r'[ \t]+#+[ \t]*$', '', heading[2] or '').strip()
            match = VERSION_HEADING.match(title)
            heading_version = (match[1] or match[2]) if match else None
            headings.append((index, len(heading[1]), heading_version))

    matches = [heading for heading in headings if heading[2] == version]
    if not matches:
        raise ValueError(f'No changelog heading found for {version}')
    if len(matches) != 1:
        raise ValueError(f'Multiple changelog headings found for {version}')

    start, level, _ = matches[0]
    end = next((index for index, depth, _ in headings
                if index > start and depth <= level), len(lines))
    body = lines[start + 1:end]
    while body and not body[0].strip():
        body.pop(0)
    while body and not body[-1].strip():
        body.pop()
    if not body:
        raise ValueError(f'Changelog section for {version} is empty')
    return ''.join(body)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--changelog', type=Path, required=True)
    parser.add_argument('--version', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        with args.changelog.open(encoding='utf-8-sig', newline='') as source:
            notes = extract_release_notes(source.read(), args.version)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open('w', encoding='utf-8', newline='') as destination:
            destination.write(notes)
    except (OSError, UnicodeError, ValueError) as error:
        print(f'Release notes extraction failed: {error}', file=sys.stderr)
        return 1
    print(f'Extracted {args.version} release notes to {args.output}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
