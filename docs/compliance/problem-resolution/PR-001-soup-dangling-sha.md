# PR-001 — SOUP provenance: dangling and divergent dependency SHAs

> Problem-resolution record per **IEC 62304 §9 (Software problem resolution)**.
> Record-control per **ISO 13485 §4.2.5** — the defective records were archived
> (not overwritten) before correction; see `archive/`.

| Field | Value |
|-------|-------|
| ID | PR-001 |
| Date opened | 2026-06-14 |
| Severity | Audit / configuration-management (no field/runtime impact) |
| Affected records | `dependency-manifest.json`, `docs/SOUP.md`, `.github/actions/checkout-kcenon-deps/action.yml` |
| Related issue | kcenon/pacs_system#1189 (gate: #1188) |
| Status | Corrected; **re-V&V of thread_system substitution required before v1.0 release (owner gate)** |

## 1. Problem description

pacs_system records the integrated commit of each internal kcenon ecosystem
dependency in three places (the SOUP/SBOM provenance surface). An ecosystem
analysis (2026-06-13) and the advisory drift gate added in #1188 found:

- **thread_system pinned to a dangling commit.** All three sources pinned
  `db8d36f15cf2dfb679474b20cb47160494a6c8be`. This object does not exist in
  `kcenon/thread_system` (`git cat-file -e` fails on every branch) — it was
  orphaned by an upstream history rewrite/squash. The integrated thread_system
  version is therefore **unrecoverable**: it cannot be checked out, rebuilt, or
  audited against any artifact (IEC 62304 §8.1.2 requires a uniquely
  identifiable integrated version).
- **database_system divergent across sources.** `dependency-manifest.json`
  pinned `b90b0f3b...` (also unresolvable), while `docs/SOUP.md` and the CI
  checkout action pinned `593a18a7...` (resolvable, the version CI actually
  built). The manifest disagreed with the as-built configuration.

## 2. Root cause

The three provenance sources are hand-maintained with no consistency gate, so
upstream history rewrites and partial manual edits drifted them apart and left
references to commits that no longer exist. (#1188 added the detecting gate.)

## 3. Correction

- **database_system** → re-pinned the manifest to `593a18a7158d5707b8f589c0dae75d6b2ddad931`,
  matching the CI checkout action and SOUP.md (the as-built, resolvable commit).
  All three sources now agree and resolve.
- **thread_system** → the unrecoverable `db8d36f1` is **substituted** with the
  `v1.0.0` release tag `6dd5c9e8c224fcd177560623cd7eccad6415f3e4` (resolvable,
  immutable) across all three sources. This is a *substitution of an
  unrecoverable baseline*, not a recovery of the original.
- The pre-correction `dependency-manifest.json` and `SOUP.md` are archived
  verbatim under `archive/` with the date suffix `2026-06-14.pre-PR-001`.
- The remaining five dependencies (common, container, logger, monitoring,
  network) were already resolvable and tri-source-consistent; they are
  unchanged here. (They are pinned to pre-release commits pacs was built and
  V&V'd against; alignment to their own v1.0.0 tags is deferred to the
  ecosystem v1.0 release, tracked separately.)

## 4. Verification & residual action

- **Now:** the #1188 drift gate is switched from advisory to **strict**
  (`--strict`); it fails CI if any of the three sources diverge or reference an
  unresolvable commit. With this correction it passes.
- **Owner gate (before v1.0 release):** because the originally-integrated
  thread_system commit is unrecoverable, the substituted `v1.0.0` baseline has
  not been through pacs's full V&V against the exact prior bytes. The pacs
  `RELEASE_GATES` matrix MUST be re-run against the substituted thread_system
  `v1.0.0` and the result recorded before pacs cuts its own v1.0.0. This record
  remains **open** until that re-V&V is signed off.

## 5. Change log

| Date | Action |
|------|--------|
| 2026-06-14 | Opened; archived defective records; corrected manifest/SOUP/action; switched #1188 gate to strict. Residual re-V&V action open. |
