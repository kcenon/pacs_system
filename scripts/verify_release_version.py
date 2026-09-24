#!/usr/bin/env python3
"""Verify that every pacs_system version source matches an expected release version.

Checked sources:
  * CMakeLists.txt       project(pacs_system ... VERSION x.y.z ...), single- or multi-line
  * vcpkg.json           "version-semver" (or another vcpkg version key), parsed as JSON
  * Doxyfile             PROJECT_NUMBER
  * vcpkg-ports/<port>/vcpkg.json  the local overlay manifest

Exit codes: 0 all sources match, 1 at least one mismatch, 2 a source is missing or malformed
(or the expected version itself is invalid).
"""

import argparse
import json
from pathlib import Path
import re
import sys

SEMVER = re.compile(r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$")
VCPKG_VERSION_KEYS = ("version", "version-semver", "version-string", "version-date")
DEFAULT_PORT = "kcenon-pacs-system"
DEFAULT_PROJECT = "pacs_system"


class SourceError(Exception):
    """A version source is missing or cannot be parsed."""


def _strip_cmake_comments(text):
    """Remove `#` line comments that are outside double-quoted strings."""
    out = []
    in_quote = False
    i = 0
    while i < len(text):
        ch = text[i]
        if ch == "\\" and in_quote and i + 1 < len(text):
            out.append(text[i:i + 2])
            i += 2
            continue
        if ch == '"':
            in_quote = not in_quote
        elif ch == "#" and not in_quote:
            while i < len(text) and text[i] != "\n":
                i += 1
            continue
        out.append(ch)
        i += 1
    return "".join(out)


def _cmake_arguments(text, start):
    """Return the argument text of a command whose `(` is at `start`, honoring quotes."""
    depth = 0
    in_quote = False
    i = start
    while i < len(text):
        ch = text[i]
        if ch == "\\" and in_quote:
            i += 2
            continue
        if ch == '"':
            in_quote = not in_quote
        elif not in_quote:
            if ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
                if depth == 0:
                    return text[start + 1:i]
        i += 1
    raise SourceError("unterminated project() command")


def _cmake_tokens(args):
    return [m.group(1) if m.group(1) is not None else m.group(2)
            for m in re.finditer(r'"((?:[^"\\]|\\.)*)"|([^\s"]+)', args)]


def cmake_version(path, project=DEFAULT_PROJECT):
    if not path.is_file():
        raise SourceError(f"{path} not found")
    text = _strip_cmake_comments(path.read_text(encoding="utf-8"))
    for match in re.finditer(r"\bproject\s*\(", text, re.IGNORECASE):
        tokens = _cmake_tokens(_cmake_arguments(text, match.end() - 1))
        if not tokens or tokens[0] != project:
            continue
        if "VERSION" not in tokens:
            raise SourceError(f"{path}: project({project}) has no VERSION argument")
        index = tokens.index("VERSION")
        if index + 1 >= len(tokens):
            raise SourceError(f"{path}: project({project}) VERSION has no value")
        return tokens[index + 1]
    raise SourceError(f"{path}: no project({project} ...) command found")


def vcpkg_version(path):
    if not path.is_file():
        raise SourceError(f"{path} not found")
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise SourceError(f"{path}: invalid JSON ({exc})") from exc
    if not isinstance(data, dict):
        raise SourceError(f"{path}: top-level value is not an object")
    keys = [key for key in VCPKG_VERSION_KEYS if key in data]
    if len(keys) != 1:
        found = ", ".join(keys) if keys else "none"
        raise SourceError(f"{path}: expected exactly one of {', '.join(VCPKG_VERSION_KEYS)} (found {found})")
    value = data[keys[0]]
    if not isinstance(value, str) or not value:
        raise SourceError(f"{path}: {keys[0]} is not a non-empty string")
    return value


def doxygen_version(path):
    if not path.is_file():
        raise SourceError(f"{path} not found")
    values = []
    for line in path.read_text(encoding="utf-8").splitlines():
        match = re.match(r"^\s*PROJECT_NUMBER\s*=\s*(.*?)\s*$", line)
        if match:
            values.append(match.group(1).strip('"').strip())
    if len(values) != 1 or not values[0]:
        raise SourceError(f"{path}: expected one non-empty PROJECT_NUMBER (found {len(values)})")
    return values[0]


def collect(root, port=DEFAULT_PORT, project=DEFAULT_PROJECT):
    """Return [(label, version or None, error or None)] for every checked source."""
    sources = [
        ("CMakeLists.txt project() VERSION", lambda: cmake_version(root / "CMakeLists.txt", project)),
        ("vcpkg.json", lambda: vcpkg_version(root / "vcpkg.json")),
        ("Doxyfile PROJECT_NUMBER", lambda: doxygen_version(root / "Doxyfile")),
        (f"vcpkg-ports/{port}/vcpkg.json", lambda: vcpkg_version(root / "vcpkg-ports" / port / "vcpkg.json")),
    ]
    results = []
    for label, read in sources:
        try:
            results.append((label, read(), None))
        except (SourceError, OSError) as exc:
            results.append((label, None, str(exc)))
    return results


def verify(expected, root, port=DEFAULT_PORT, project=DEFAULT_PROJECT, out=sys.stdout):
    if not SEMVER.match(expected):
        print(f"ERROR: expected version '{expected}' is not MAJOR.MINOR.PATCH", file=out)
        return 2
    status = 0
    for label, version, error in collect(root, port, project):
        if error:
            print(f"ERROR   {label}: {error}", file=out)
            status = 2
        elif version != expected:
            print(f"MISMATCH {label}: {version} (expected {expected})", file=out)
            status = max(status, 1)
        else:
            print(f"OK      {label}: {version}", file=out)
    if status == 0:
        print(f"All version sources match {expected}", file=out)
    return status


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("expected", help="expected release version, e.g. 0.1.0")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--port", default=DEFAULT_PORT, help="overlay port directory name")
    parser.add_argument("--project", default=DEFAULT_PROJECT, help="CMake project() name")
    args = parser.parse_args(argv)
    return verify(args.expected, args.root, args.port, args.project)


if __name__ == "__main__":
    sys.exit(main())
