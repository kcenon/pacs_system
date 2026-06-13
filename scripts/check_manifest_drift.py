#!/usr/bin/env python3
# SOUP/SBOM provenance drift gate for pacs_system (IEC 62304 SaMD).
#
# Cross-checks the pinned commit SHA of every internal kcenon ecosystem
# dependency across the THREE places this repo records that provenance:
#
#   1. dependency-manifest.json
#        -> internal_ecosystem[].name + .version (full 40-char SHA per system)
#   2. .github/actions/checkout-kcenon-deps/action.yml
#        -> <SYSTEM>_SYSTEM_SHA: '...' env defaults (full 40-char SHA)
#   3. docs/SOUP.md
#        -> the per-system pinned SHA table (short SHA, treated as a prefix)
#
# Goal: catch re-pin churn that silently introduces a dangling or mismatched
# SHA in one source but not the others. This gate DETECTS and REPORTS; it does
# not repair (repair is tracked separately in #1189).
#
# ADVISORY until #1189 repairs the manifest (dangling thread SHA +
# database manifest/action mismatch); flip the CI step to required after #1189.
#
# Python 3, standard library only. No third-party deps (must run in a bare CI
# step before any pip install).

import json
import re
import subprocess
import sys
from pathlib import Path

# The seven internal ecosystem systems, in a stable display order.
SYSTEMS = [
    "common_system",
    "container_system",
    "thread_system",
    "logger_system",
    "monitoring_system",
    "network_system",
    "database_system",
]

# Repo root = parent of this script's directory (scripts/).
REPO_ROOT = Path(__file__).resolve().parent.parent

MANIFEST_PATH = REPO_ROOT / "dependency-manifest.json"
ACTION_PATH = REPO_ROOT / ".github" / "actions" / "checkout-kcenon-deps" / "action.yml"
SOUP_PATH = REPO_ROOT / "docs" / "SOUP.md"

# A SHA token: hex, 7..40 chars (SOUP uses short SHAs, others use full).
_SHA_RE = re.compile(r"\b([0-9a-f]{7,40})\b", re.IGNORECASE)


def _system_to_action_key(system: str) -> str:
    """thread_system -> THREAD_SYSTEM_SHA"""
    return system.upper() + "_SHA"


def _system_to_soup_base(system: str) -> str:
    """thread_system -> thread (the SOUP link/name uses the full system name,
    but matching is done on the full system name anyway)."""
    return system


def parse_manifest(path: Path) -> dict:
    """Return {system: sha} from dependency-manifest.json internal_ecosystem."""
    result = {}
    if not path.is_file():
        return result
    data = json.loads(path.read_text(encoding="utf-8"))
    for entry in data.get("internal_ecosystem", []):
        name = entry.get("name")
        version = entry.get("version")
        if name in SYSTEMS and isinstance(version, str):
            result[name] = version.strip().lower()
    return result


def parse_action(path: Path) -> dict:
    """Return {system: sha} from the <SYSTEM>_SYSTEM_SHA env defaults in the
    composite action. The same SHA appears in both the Unix and Windows env
    blocks; we take the first occurrence and (best effort) verify consistency."""
    result = {}
    if not path.is_file():
        return result
    text = path.read_text(encoding="utf-8")
    for system in SYSTEMS:
        key = _system_to_action_key(system)
        # Match e.g.  THREAD_SYSTEM_SHA: 'db8d36f1...'
        # Collect every occurrence so an internally inconsistent action
        # (Unix vs Windows block disagreeing) is itself surfaced as drift.
        found = re.findall(
            r"\b" + re.escape(key) + r"\s*:\s*['\"]([0-9a-fA-F]{7,40})['\"]",
            text,
        )
        if not found:
            continue
        shas = {s.strip().lower() for s in found}
        if len(shas) > 1:
            # Encode the internal disagreement so the report flags it.
            result[system] = "MULTIPLE:" + "/".join(sorted(shas))
        else:
            result[system] = found[0].strip().lower()
    return result


def parse_soup(path: Path) -> dict:
    """Return {system: sha} from the kcenon-ecosystem table in docs/SOUP.md.

    Each row links the system name (e.g. [thread_system](...)) and carries the
    pinned SHA in a backtick-quoted cell (e.g. `db8d36f1`). We anchor on the
    system name appearing in the same row, then take the first SHA-looking
    backtick token on that row."""
    result = {}
    if not path.is_file():
        return result
    for line in path.read_text(encoding="utf-8").splitlines():
        if "|" not in line:
            continue
        for system in SYSTEMS:
            base = _system_to_soup_base(system)
            # Row references the system by name (link text or plain).
            if base not in line:
                continue
            # SHA cells are backtick-wrapped, e.g. `593a18a7`.
            m = re.search(r"`([0-9a-fA-F]{7,40})`", line)
            if m:
                result[system] = m.group(1).strip().lower()
            break
    return result


def shas_agree(a: str, b: str) -> bool:
    """Two SHAs agree if one is a case-insensitive prefix of the other
    (SOUP short SHA vs full SHA). Sentinel MULTIPLE: values never agree."""
    if a is None or b is None:
        return True  # absence is handled separately, not a mismatch
    if a.startswith("MULTIPLE:") or b.startswith("MULTIPLE:"):
        return False
    lo, hi = sorted((a, b), key=len)
    return hi.startswith(lo)


def short(sha):
    if sha is None:
        return "(absent)"
    if sha.startswith("MULTIPLE:"):
        return sha
    return sha[:8]


def resolvability(system: str, sha: str) -> str:
    """Best-effort, NON-FATAL: if a sibling repo exists at ../<system>, try to
    resolve the SHA as a commit. Returns 'resolvable', 'UNRESOLVABLE', or ''
    (siblings absent -> skip silently)."""
    if sha is None or sha.startswith("MULTIPLE:"):
        return ""
    sibling = REPO_ROOT.parent / system
    if not (sibling / ".git").exists() and not sibling.is_dir():
        return ""
    git_dir_present = (sibling / ".git").exists()
    if not git_dir_present:
        return ""
    try:
        rc = subprocess.run(
            ["git", "-C", str(sibling), "cat-file", "-e", sha + "^{commit}"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=15,
        ).returncode
    except (OSError, subprocess.SubprocessError):
        return ""
    return "resolvable" if rc == 0 else "UNRESOLVABLE"


def main() -> int:
    manifest = parse_manifest(MANIFEST_PATH)
    action = parse_action(ACTION_PATH)
    soup = parse_soup(SOUP_PATH)

    sources_present = {
        "manifest": MANIFEST_PATH.is_file(),
        "action": ACTION_PATH.is_file(),
        "soup": SOUP_PATH.is_file(),
    }

    print("=" * 78)
    print("SOUP/SBOM manifest drift gate -- internal kcenon ecosystem SHAs")
    print("ADVISORY (non-blocking) until #1189 repairs the manifest.")
    print("=" * 78)
    print("Sources:")
    print(f"  manifest : {MANIFEST_PATH.relative_to(REPO_ROOT)} "
          f"[{'found' if sources_present['manifest'] else 'MISSING'}]")
    print(f"  action   : {ACTION_PATH.relative_to(REPO_ROOT)} "
          f"[{'found' if sources_present['action'] else 'MISSING'}]")
    print(f"  soup     : {SOUP_PATH.relative_to(REPO_ROOT)} "
          f"[{'found' if sources_present['soup'] else 'MISSING'}]")
    print()

    # Per-system table.
    header = f"{'system':<20} {'manifest':<10} {'action':<10} {'soup':<10} {'status':<10} resolvability"
    print(header)
    print("-" * len(header))

    mismatches = []  # (system, [(srcname, sha), ...])
    any_sibling_checked = False

    for system in SYSTEMS:
        m = manifest.get(system)
        a = action.get(system)
        s = soup.get(system)

        present = {"manifest": m, "action": a, "soup": s}
        non_null = {k: v for k, v in present.items() if v is not None}

        # Pairwise agreement across all present sources.
        agree = True
        keys = list(non_null.keys())
        for i in range(len(keys)):
            for j in range(i + 1, len(keys)):
                if not shas_agree(non_null[keys[i]], non_null[keys[j]]):
                    agree = False
        status = "OK" if agree else "DRIFT"
        if not agree:
            mismatches.append((system, list(non_null.items())))

        # Resolvability: report against the manifest SHA (canonical provenance
        # input per SOUP.md); fall back to action SHA if manifest absent.
        probe_sha = m if m is not None else a
        res = resolvability(system, probe_sha)
        if res:
            any_sibling_checked = True

        print(f"{system:<20} {short(m):<10} {short(a):<10} {short(s):<10} "
              f"{status:<10} {res}")

    print()

    if mismatches:
        print("DRIFT REPORT -- tri-source SHA mismatches detected:")
        print("-" * 78)
        for system, items in mismatches:
            print(f"  * {system}:")
            for srcname, sha in items:
                print(f"      {srcname:<10} = {sha}")
            # Spell out which sources disagree.
            disagreeing = []
            for i in range(len(items)):
                for j in range(i + 1, len(items)):
                    if not shas_agree(items[i][1], items[j][1]):
                        disagreeing.append(f"{items[i][0]} != {items[j][0]}")
            if disagreeing:
                print(f"      -> disagreement: {', '.join(disagreeing)}")
        print()

    # Surface UNRESOLVABLE annotations explicitly (only meaningful if siblings
    # were checked). These do NOT change the exit code by themselves, but a
    # dangling SHA is almost always also a drift signal worth surfacing.
    if any_sibling_checked:
        unresolvable = []
        for system in SYSTEMS:
            probe = manifest.get(system) or action.get(system)
            if resolvability(system, probe) == "UNRESOLVABLE":
                unresolvable.append((system, probe))
        if unresolvable:
            print("UNRESOLVABLE SHAs (sibling repos checked locally):")
            print("-" * 78)
            for system, sha in unresolvable:
                print(f"  * {system}: {sha} cannot be resolved as a commit "
                      f"in ../{system}")
            print()
    else:
        print("(sibling repos not available locally -- resolvability skipped)")
        print()

    if mismatches:
        strict = "--strict" in sys.argv
        for system, _items in mismatches:
            print(f"::warning title=SOUP manifest drift::{system}: internal-dep "
                  f"SHA disagrees across manifest/action/SOUP (repair tracked in #1189)")
        print(f"RESULT: DRIFT -- {len(mismatches)} system(s) disagree across "
              f"sources. See report above. (Repair tracked in #1189.)")
        if strict:
            return 1
        print("ADVISORY MODE (default): drift surfaced as warnings, exit 0. "
              "Run with --strict and mark this check required after #1189 "
              "repairs the manifest.")
        return 0

    print("RESULT: OK -- all present sources agree on every internal SHA.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
