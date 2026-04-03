---
doc_id: "PAC-ADR-002"
doc_title: "ADR-002: Define PACS Storage Port Segmentation"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "pacs_system"
category: "ADR"
---

# ADR-002: Define PACS Storage Port Segmentation

> **SSOT**: This document is the single source of truth for **ADR-002: Define PACS Storage Port Segmentation**.

> **Status:** Accepted
> **Date:** 2026-03-10
> **Decision Makers:** pacs_system core team
> **Related Issues:** [#891](https://github.com/kcenon/pacs_system/issues/891), [#892](https://github.com/kcenon/pacs_system/issues/892), #893, #894, #896, #897

---

## Context

The PACS storage layer currently mixes three different shapes of persistence access:

1. Canonical split repositories already backed by `pacs_database_adapter`
2. Aggregate compatibility repositories that bundle multiple persistence concerns
3. Large mixed storage classes such as `index_database` that still own several PACS persistence ports

This makes it difficult for higher-level PACS services to depend on a stable repository-set contract and obscures which persistence responsibilities belong to PACS versus `database_system`.

---

## Decision

We will standardize PACS storage around two explicit contracts:

1. A **canonical repository-set contract** for higher-level PACS wiring
2. A **compatibility repository-set contract** used only during incremental migration away from aggregate repositories

The canonical contract is exposed through `repository_factory::canonical_repositories()` and is grouped by PACS-owned persistence ports. Aggregate repositories remain available through `repository_factory::compatibility_repositories()` until the migration phases complete.

`database_system` remains an infrastructure dependency only. PACS owns the port model, repository boundaries, and persistence semantics.

---

## Port Inventory

| Domain | PACS storage port | Primary contract | Current implementation | Compatibility adapter | Ownership |
|--------|-------------------|------------------|------------------------|-----------------------|-----------|
| Metadata | patient metadata | `index_database` | `index_database` patient APIs | None | PACS domain |
| Metadata | study metadata | `index_database` | `index_database` study APIs | None | PACS domain |
| Metadata | series metadata | `index_database` | `index_database` series APIs | None | PACS domain |
| Metadata | instance metadata | `index_database` | `index_database` instance APIs | None | PACS domain |
| Lifecycle | audit trail | `index_database` | `index_database` audit APIs | None | PACS domain |
| Lifecycle | modality worklist | `index_database` | `index_database` worklist APIs | None | PACS domain |
| Lifecycle | UPS workitems | `index_database` | `index_database` UPS APIs | None | PACS domain |
| Lifecycle | schema migration history | `index_database` / `migration_runner` | `index_database`, `migration_runner` | None | PACS domain |
| Lifecycle | storage commitment | `commitment_repository` | `commitment_repository` | None | PACS domain |
| Client state | sync configuration | `sync_config_repository` | `sync_config_repository` | `sync_repository` | PACS domain |
| Client state | sync conflicts | `sync_conflict_repository` | `sync_conflict_repository` | `sync_repository` | PACS domain |
| Client state | sync history | `sync_history_repository` | `sync_history_repository` | `sync_repository` | PACS domain |
| Client state | viewer state records | `viewer_state_record_repository` | `viewer_state_record_repository` | `viewer_state_repository` | PACS domain |
| Client state | recent studies | `recent_study_repository` | `recent_study_repository` | `viewer_state_repository` | PACS domain |
| Client state | prefetch rules | `prefetch_rule_repository` | `prefetch_rule_repository` | `prefetch_repository` | PACS domain |
| Client state | prefetch history | `prefetch_history_repository` | `prefetch_history_repository` | `prefetch_repository` | PACS domain |
| Client state | jobs | `job_repository` | `job_repository` | None | PACS domain |
| Client state | routing rules | `routing_repository` | `routing_repository` | None | PACS domain |
| Client state | remote nodes | `node_repository` | `node_repository` | None | PACS domain |
| Client state | annotations | `annotation_repository` | `annotation_repository` | None | PACS domain |
| Client state | key images | `key_image_repository` | `key_image_repository` | None | PACS domain |
| Client state | measurements | `measurement_repository` | `measurement_repository` | None | PACS domain |

---

## Canonical Repository Set

Higher-level PACS services should depend on the following canonical repository groups:

- `sync_repository_set`
  - `sync_config_repository`
  - `sync_conflict_repository`
  - `sync_history_repository`
- `viewer_state_repository_set`
  - `viewer_state_record_repository`
  - `recent_study_repository`
- `prefetch_repository_set`
  - `prefetch_rule_repository`
  - `prefetch_history_repository`
- standalone canonical repositories
  - `job_repository`
  - `routing_repository`
  - `node_repository`
  - `annotation_repository`
  - `key_image_repository`
  - `measurement_repository`
  - `commitment_repository`

Aggregate repositories are classified as compatibility-only:

- `sync_repository`
- `viewer_state_repository`
- `prefetch_repository`

These compatibility repositories may remain temporarily for migration windows, but new service wiring must not introduce new dependencies on them.

As of 2026-03-10, `sync_manager`, `prefetch_manager`, and viewer-state web endpoints have been rewired to the canonical split repositories. Aggregate repositories remain available only as compatibility shims.

---

## Ownership Boundary

### PACS owns

- Port names and repository segmentation
- Entity-specific persistence semantics
- Compatibility adapters required during PACS-internal migration
- Rules about which higher layers may depend on which repositories

### `database_system` owns

- Connection management
- Transactions and unit-of-work primitives
- Backend selection
- Query building and low-level execution helpers

PACS higher layers must not depend on raw SQL execution helpers or backend-specific APIs directly.

---

## Allowed Exceptions

Raw SQL or native-backend seams are allowed only for the following categories:

1. DDL and schema migrations in `migration_runner`
2. Explicitly documented metadata/lifecycle seams that still live inside `index_database` until later decomposition phases
3. Backend bootstrap and infrastructure-only code that does not encode PACS domain semantics

These exceptions do **not** permit higher-level PACS services or repositories to construct new ad hoc SQL strings outside documented storage boundaries.
The repository-scoped guard in `tests/storage/check_storage_boundary.py` enforces this rule for canonical repositories and service wiring in CI.

---

## Consequences

### Positive

1. Higher-level services can target a stable PACS-owned repository-set contract
2. Aggregate repositories are clearly marked as migration shims instead of primary contracts
3. Later phases can replace `index_database` internals without changing the port map
4. PACS/domain ownership is separated more cleanly from `database_system` infrastructure responsibilities

### Tradeoffs

1. The metadata and lifecycle domains still rely on `index_database` until later phases
2. Compatibility adapters remain temporarily to preserve incremental delivery

---

## Follow-up Work

1. #893 will replace raw adapter access with PACS session and unit-of-work boundaries
2. #894 will move more wiring to repository-set contracts
3. #896 and #897 will continue decomposing `index_database` while keeping runtime `database_manager` paths removed
