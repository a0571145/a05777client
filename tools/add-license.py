#!/usr/bin/env python3
"""Prepend the a05777Client GPL header to every first-party .h/.cpp file.

Usage (from H4 root):
    python3 raven-dll/tools/add-license.py

Idempotent: files already containing the marker are skipped.
Only touches raven-dll/src/** and raven-dll/tools/*.cpp (third_party vendored
code is deliberately excluded). Line endings and BOM (if any) are preserved.
"""
import sys
from pathlib import Path

HEADER_LINES = [
    "/*",
    " * Copyright (C) 2026 a05777",
    " *",
    " * This program is free software: you can redistribute it and/or modify",
    " * it under the terms of the GNU General Public License as published by",
    " * the Free Software Foundation, either version 3 of the License, or",
    " * (at your option) any later version.",
    " *",
    " * This program is distributed in the hope that it will be useful,",
    " * but WITHOUT ANY WARRANTY; without even the implied warranty of",
    " * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the",
    " * GNU General Public License for more details.",
    " */",
]

MARKER = b"Copyright (C) 2026 a05777"
BOM = b"\xef\xbb\xbf"


def targets(root: Path):
    files = sorted((root / "src").rglob("*.h")) + sorted((root / "src").rglob("*.cpp"))
    files += sorted((root / "tools").glob("*.cpp"))
    return files


def main() -> int:
    root = Path(__file__).resolve().parent.parent  # raven-dll/
    added, skipped = 0, 0
    for f in targets(root):
        raw = f.read_bytes()
        if MARKER in raw[:2048]:
            skipped += 1
            continue
        eol = b"\r\n" if b"\r\n" in raw else b"\n"
        header = eol.join(line.encode("ascii") for line in HEADER_LINES) + eol + eol
        if raw.startswith(BOM):
            out = BOM + header + raw[len(BOM):]
        else:
            # Normalize a leading blank line so we don't stack empty lines.
            body = raw.lstrip(b"\r\n") if raw.startswith((b"\r", b"\n")) else raw
            # Only strip when the file actually starts with blank lines.
            out = header + (body if body is not raw else raw)
        # ASCII check: our header is ASCII; file must stay decodable-safe.
        try:
            out.decode("utf-8")
        except UnicodeDecodeError:
            print(f"SKIP (non-utf8): {f}", file=sys.stderr)
            skipped += 1
            continue
        f.write_bytes(out)
        added += 1
    print(f"license: added={added} skipped={skipped}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
