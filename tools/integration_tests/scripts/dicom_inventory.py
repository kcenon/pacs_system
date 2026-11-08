#!/usr/bin/env python3
"""Discover valid Part 10 objects regardless of suffix and compare SOP UIDs."""
import argparse
from collections import Counter
from pathlib import Path
import re
import subprocess
import sys


def dicom_value(path, tag):
    result = subprocess.run(
        ["dcmdump", "+P", tag, str(path)], capture_output=True, text=True, timeout=10
    )
    if result.returncode:
        raise ValueError(f"dcmdump failed for {path}: {result.stderr}")
    matches = re.findall(r"^\(" + re.escape(tag) + r"\) \w+ \[([^\]]*)\]", result.stdout, re.M)
    if len(matches) != 1 or not matches[0]:
        raise ValueError(f"Expected one nonempty {tag} in {path}")
    return matches[0]


def inventory(directory):
    objects = []
    for path in sorted(Path(directory).rglob("*")):
        if not path.is_file() or path.is_symlink():
            continue
        with path.open("rb") as stream:
            stream.seek(128)
            if stream.read(4) != b"DICM":
                continue
        try:
            uid = dicom_value(path, "0008,0018")
            if not re.fullmatch(r"[0-9]+(?:\.[0-9]+)+", uid) or len(uid) > 64:
                raise ValueError(f"Invalid SOP Instance UID: {uid}")
        except ValueError as error:
            print(f"Ignoring invalid Part 10 candidate: {error}", file=sys.stderr)
            continue
        objects.append((path, uid))
    return objects


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["list", "count", "verify", "value", "query"])
    parser.add_argument("path")
    parser.add_argument("expected", nargs="*")
    args = parser.parse_args()
    if args.action == "value":
        print(dicom_value(args.path, args.expected[0]))
        return 0
    if args.action == "query":
        # A dedicated findscu -od directory contains only response datasets.
        # Parse every response, including one missing the required return key.
        actual = Counter(dicom_value(path, args.expected[0])
                         for path in Path(args.path).iterdir() if path.is_file())
        expected = Counter(args.expected[1:])
        if actual != expected:
            print(f"Query mismatch: expected {dict(expected)}, received {dict(actual)}", file=sys.stderr)
            return 1
        print(f"Verified exactly {sum(actual.values())} query responses")
        return 0
    objects = inventory(args.path)
    if args.action == "list":
        for path, _ in objects:
            print(path)
    elif args.action == "count":
        print(len(objects))
    else:
        expected = Counter(dicom_value(path, "0008,0018") for path in args.expected)
        actual = Counter(uid for _, uid in objects)
        if not expected or any(count != 1 for count in expected.values()):
            raise ValueError("Expected files must contain distinct SOP Instance UIDs")
        if actual != expected:
            print(f"SOP UID mismatch: expected {dict(expected)}, received {dict(actual)}", file=sys.stderr)
            return 1
        print(f"Verified exactly {len(objects)} Part 10 objects and their SOP Instance UIDs")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.TimeoutExpired, IndexError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
