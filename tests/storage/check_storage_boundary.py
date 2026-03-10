#!/usr/bin/env python3
"""Enforce the PACS storage boundary for canonical repositories and service wiring."""

from __future__ import annotations

import sys
from pathlib import Path


DATABASE_MANAGER_TARGETS = (
    "include/pacs/storage/index_database.hpp",
    "src/storage/index_database.cpp",
    "include/pacs/client/sync_manager.hpp",
    "src/client/sync_manager.cpp",
    "include/pacs/client/prefetch_manager.hpp",
    "src/client/prefetch_manager.cpp",
    "include/pacs/storage/repository_factory.hpp",
    "src/storage/repository_factory.cpp",
)

SQLITE_TARGETS = (
    "include/pacs/client/sync_manager.hpp",
    "src/client/sync_manager.cpp",
    "include/pacs/client/prefetch_manager.hpp",
    "src/client/prefetch_manager.cpp",
    "include/pacs/storage/repository_factory.hpp",
    "src/storage/repository_factory.cpp",
    "include/pacs/storage/sync_config_repository.hpp",
    "src/storage/sync_config_repository.cpp",
    "include/pacs/storage/sync_conflict_repository.hpp",
    "src/storage/sync_conflict_repository.cpp",
    "include/pacs/storage/sync_history_repository.hpp",
    "src/storage/sync_history_repository.cpp",
    "include/pacs/storage/prefetch_rule_repository.hpp",
    "src/storage/prefetch_rule_repository.cpp",
    "include/pacs/storage/prefetch_history_repository.hpp",
    "src/storage/prefetch_history_repository.cpp",
    "include/pacs/storage/viewer_state_record_repository.hpp",
    "src/storage/viewer_state_record_repository.cpp",
    "include/pacs/storage/recent_study_repository.hpp",
    "src/storage/recent_study_repository.cpp",
    "src/web/endpoints/viewer_state_endpoints.cpp",
)


def scan(root: Path, files: tuple[str, ...], needle: str) -> list[str]:
    failures: list[str] = []
    for rel_path in files:
        path = root / rel_path
        if not path.exists():
            failures.append(f"missing file in boundary check: {rel_path}")
            continue
        text = path.read_text(encoding="utf-8")
        if needle in text:
            failures.append(f"{rel_path}: found forbidden token '{needle}'")
    return failures


def main() -> int:
    default_root = Path(__file__).resolve().parents[2]
    root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else default_root
    failures = []
    failures.extend(scan(root, DATABASE_MANAGER_TARGETS, "database_manager"))
    failures.extend(scan(root, SQLITE_TARGETS, "sqlite3_"))

    if failures:
        print("storage boundary check failed:")
        for failure in failures:
            print(f" - {failure}")
        return 1

    print("storage boundary check passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
