#!/usr/bin/env python3
"""Embed the standalone setup page into a PROGMEM C++ header."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path


NONCE_PLACEHOLDER = "__SETUP_NONCE__"


def choose_delimiter(page: str) -> str:
    """Return a raw-string delimiter that cannot terminate the embedded page."""
    # C++ limits a raw-string delimiter to 16 characters.
    digest = hashlib.sha256(page.encode("utf-8")).hexdigest()[:8]
    delimiter = f"ESP32SP_{digest}"
    if f"){delimiter}\"" in page:
        raise ValueError("setup page contains the generated raw-string delimiter")
    return delimiter


def embed(source: Path, destination: Path) -> None:
    page = source.read_text(encoding="utf-8")
    if "\x00" in page:
        raise ValueError("setup page contains a NUL byte")
    if NONCE_PLACEHOLDER not in page:
        raise ValueError(f"setup page must contain {NONCE_PLACEHOLDER!r}")

    delimiter = choose_delimiter(page)
    content = page if page.endswith("\n") else f"{page}\n"
    header = (
        "#pragma once\n"
        "#include <Arduino.h>\n"
        "\n"
        f"static const char kSetupPage[] PROGMEM = R\"{delimiter}(\n"
        f"{content}"
        f")" + delimiter + "\";\n"
    )
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(header, encoding="utf-8", newline="\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", nargs="?", type=Path, default=Path("web/setup.html"))
    parser.add_argument("destination", nargs="?", type=Path, default=Path("include/setup-page.h"))
    args = parser.parse_args()
    embed(args.source, args.destination)


if __name__ == "__main__":
    main()
