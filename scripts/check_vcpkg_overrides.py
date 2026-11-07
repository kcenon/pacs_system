#!/usr/bin/env python3
"""Check every root override against the pinned builtin registry, entirely offline.

The registry checkout only needs the baseline commit and its version database.
This checks availability, not dependency resolution or successful compilation.
Exit codes: 0 = all available, 1 = missing versions, 2 = invalid input/registry.
"""

import argparse
import fnmatch
import json
import os
from pathlib import Path
import re
import subprocess
import sys

VERSION_FIELDS = ("version", "version-semver", "version-date", "version-string")
SHA = re.compile(r"[0-9a-fA-F]{40}\Z")
PORT = re.compile(r"[a-z0-9]+(?:-[a-z0-9]+)*\Z")


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def parse_json(text):
    return json.loads(text, object_pairs_hook=unique_object)


def load_inputs(root):
    manifest = parse_json((root / "vcpkg.json").read_text(encoding="utf-8"))
    if not isinstance(manifest, dict):
        raise ValueError("manifest must be a JSON object")
    config_path = root / "vcpkg-configuration.json"
    if config_path.exists() and "vcpkg-configuration" in manifest:
        raise ValueError("both embedded and standalone vcpkg configuration exist")
    config = (parse_json(config_path.read_text(encoding="utf-8")) if config_path.exists()
              else manifest.get("vcpkg-configuration", {}))
    if not isinstance(config, dict):
        raise ValueError("configuration must be a JSON object")
    registry = config.get("default-registry", {})
    if not isinstance(registry, dict) or registry.get("kind", "builtin") != "builtin":
        raise ValueError("this check requires a builtin default registry")
    baselines = [v for v in (registry.get("baseline"), manifest.get("builtin-baseline"))
                 if v is not None]
    if not baselines or any(not isinstance(v, str) or not SHA.fullmatch(v) for v in baselines):
        raise ValueError("a full 40-character builtin baseline commit is required")
    if len({v.lower() for v in baselines}) != 1:
        raise ValueError("conflicting builtin baseline commits")
    overrides = manifest.get("overrides")
    if not isinstance(overrides, list) or not overrides:
        raise ValueError("overrides must be a nonempty array")
    registries = config.get("registries", [])
    if not isinstance(registries, list):
        raise ValueError("registries must be an array")
    patterns = []
    for item in registries:
        if not isinstance(item, dict) or not isinstance(item.get("packages"), list):
            raise ValueError("each registry must list its packages")
        if any(not isinstance(p, str) for p in item["packages"]):
            raise ValueError("registry package patterns must be strings")
        patterns.extend(item["packages"])
    seen = set()
    for entry in overrides:
        name = entry.get("name") if isinstance(entry, dict) else None
        if not isinstance(name, str) or not PORT.fullmatch(name) or name in seen:
            raise ValueError(f"invalid or duplicate override name: {name!r}")
        seen.add(name)
        version_pair(entry)
        if any(fnmatch.fnmatchcase(name, pattern) for pattern in patterns):
            raise ValueError(f"{name}: custom-registry overrides require a separate registry check")
    return baselines[0].lower(), overrides


def version_pair(entry, historical=False):
    if not isinstance(entry, dict):
        raise ValueError("version entries must be objects")
    fields = [key for key in VERSION_FIELDS if key in entry]
    if len(fields) != 1 or not isinstance(entry[fields[0]], str):
        raise ValueError("each version entry must contain exactly one version string")
    # The builtin openjpeg history contains an empty legacy version-string.
    # It cannot satisfy a nonempty manifest override but is valid history.
    if not entry[fields[0]] and not (historical and fields[0] == "version-string"):
        raise ValueError("each version entry must contain exactly one nonempty version string")
    port_version = entry.get("port-version", 0)
    if type(port_version) is not int or port_version < 0:
        raise ValueError("port-version must be a nonnegative integer")
    return entry[fields[0]], port_version


def git(registry, *args):
    # Read the actual commit even if the local checkout has replacement refs.
    # Partial clones must not lazily fetch missing objects during this check.
    env = dict(os.environ, GIT_NO_REPLACE_OBJECTS="1", GIT_NO_LAZY_FETCH="1",
               GIT_ALLOW_PROTOCOL="")
    return subprocess.run(["git", "-C", str(registry), *args], capture_output=True,
                          text=True, encoding="utf-8", check=False, env=env)


def check(root, registry):
    baseline, overrides = load_inputs(root)
    commit = git(registry, "cat-file", "-t", baseline)
    if commit.returncode or commit.stdout.strip() != "commit":
        raise ValueError(f"baseline {baseline} is not a commit in {registry}; fetch it before checking")
    result = {"schema_version": 1, "baseline": baseline,
              "registry_checkout": str(registry.resolve()), "available": [], "missing": []}
    for override in overrides:
        name = override["name"]
        wanted = version_pair(override)
        path = f"versions/{name[0]}-/{name}.json"
        raw = git(registry, "show", f"{baseline}:{path}")
        record = {"name": name, "version": wanted[0], "port-version": wanted[1]}
        if raw.returncode:
            result["missing"].append({**record, "reason": "version database absent", "available": []})
            continue
        data = parse_json(raw.stdout)
        if not isinstance(data, dict) or not isinstance(data.get("versions"), list):
            raise ValueError(f"{path}: expected a versions array")
        versions = {}
        for entry in data["versions"]:
            pair = version_pair(entry, historical=True)
            tree = entry.get("git-tree")
            if not isinstance(tree, str) or not SHA.fullmatch(tree):
                raise ValueError(f"{path}: invalid git-tree for version {pair}")
            # Historical builtin databases contain duplicate version rows
            # (e.g. openssl 1.0.2j). Availability does not require uniqueness;
            # retain every matching tree instead of rejecting unrelated pins.
            versions.setdefault(pair, []).append(tree)
        if wanted in versions:
            result["available"].append({**record, "git-trees": versions[wanted]})
        else:
            result["missing"].append({**record, "reason": "version not registered at baseline",
                                      "available": [f"{v}#{p}" for v, p in versions]})
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--vcpkg-root", type=Path, default=os.environ.get("VCPKG_ROOT"))
    parser.add_argument("--json-output", type=Path)
    parser.add_argument("--print-baseline", action="store_true",
                        help="validate the manifest and print its builtin baseline for CI checkout")
    args = parser.parse_args(argv)
    try:
        if args.print_baseline:
            print(load_inputs(args.project_root)[0])
            return 0
        if args.vcpkg_root is None:
            raise ValueError("--vcpkg-root or VCPKG_ROOT is required (no network access is performed)")
        result = check(args.project_root, args.vcpkg_root)
        if args.json_output:
            args.json_output.parent.mkdir(parents=True, exist_ok=True)
            args.json_output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
        print(f"Builtin registry baseline: {result['baseline']}")
        for status, rows in (("AVAILABLE", result["available"]), ("MISSING", result["missing"])):
            for row in rows:
                print(f"{status} {row['name']}@{row['version']}#{row['port-version']}")
        print(f"Checked {len(result['available']) + len(result['missing'])} overrides; "
              f"{len(result['missing'])} missing")
        return int(bool(result["missing"]))
    except (ValueError, OSError, TypeError, AttributeError) as exc:
        print(f"vcpkg override check: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
