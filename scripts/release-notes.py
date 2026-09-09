#!/usr/bin/env python3
"""Extract one version section from CHANGELOG.md for a tagged release."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


class ReleaseNotesError(ValueError):
    """Raised when a release tag has no usable changelog section."""


_HEADING = re.compile(
    r"^##\s+(?:\[(?P<bracket>[^\]]+)\]|(?P<plain>\S+))(?:\s+-.*)?\s*$"
)


def normalize_tag(value: str) -> str:
    value = value.strip()
    if value.startswith("refs/tags/"):
        value = value[len("refs/tags/") :]
    return value[1:] if value.startswith("v") else value


def extract_release_notes(changelog: str, tag: str) -> str:
    expected = normalize_tag(tag)
    lines = changelog.splitlines()
    start = None
    end = len(lines)

    for index, line in enumerate(lines):
        match = _HEADING.match(line)
        if not match:
            continue
        heading = match.group("bracket") or match.group("plain")
        if normalize_tag(heading) == expected:
            start = index + 1
            break

    if start is None:
        raise ReleaseNotesError(f"CHANGELOG.md has no section for tag {tag!r}")

    for index in range(start, len(lines)):
        if lines[index].startswith("## "):
            end = index
            break

    notes = "\n".join(lines[start:end]).strip()
    if not notes:
        raise ReleaseNotesError(f"CHANGELOG.md section for tag {tag!r} is empty")
    return notes + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tag", help="release tag, for example v1.2.3")
    parser.add_argument("changelog", nargs="?", type=Path, default=Path("CHANGELOG.md"))
    args = parser.parse_args()
    try:
        notes = extract_release_notes(args.changelog.read_text(encoding="utf-8"), args.tag)
    except (OSError, ReleaseNotesError) as error:
        print(f"release-notes: {error}", file=sys.stderr)
        return 2
    sys.stdout.write(notes)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
