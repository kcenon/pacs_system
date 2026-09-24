# PACS Domain Release Gates

This document defines the **release gates** that must demonstrably pass before a
`pacs_system` version (starting with v1.0) is published. A generic green test
suite is not sufficient: because `pacs_system` is the medical-imaging tier of the
ecosystem, the release decision must explicitly show that the
medical/security-sensitive surfaces were verified.

Each gate below is followable **without reading test source code**. For every
gate you get: the exact command (or workflow) to run, where it runs in CI, and
how to interpret the result. The four mandatory domain gates are:

1. DICOM conformance
2. TLS / ATNA audit logging
3. Anonymization (de-identification)
4. Storage / index migration

> Scope note: this file is the gate **matrix**. It does not duplicate the
> conformance content itself — it points at the existing conformance documents
> (`DICOM_CONFORMANCE_STATEMENT.md`, `IHE_CONFORMANCE.md`, `SECURITY_AUDIT.md`,
> `SDS_SECURITY.md`) and at the tests/workflows that enforce them.

## How tests are selected

All C++ tests are registered with CTest through `cmake/testing.cmake` using
Catch2 (`catch_discover_tests`). Two selection mechanisms are available:

- **CTest labels** — coarse buckets: `unit`, `integration`, `stress`,
  `pacs_xds_integration`, `boundary`, `modules`. Select with `ctest -L <label>`.
- **Catch2 tags** — fine-grained per-domain selection embedded in each
  `TEST_CASE`, e.g. `[security][anonymization]`, `[security][atna]`,
  `[security][tls]`, `[migration]`, `[dcmtk][echo]`. With `catch_discover_tests`
  every Catch2 case becomes an individually named CTest test, so the simplest
  portable way to select a domain subset is the CTest name regex
  `ctest -R "<substring>"` (e.g. `ctest -R "atna"`). The compiled test binaries
  (`security_tests`, `storage_tests`, …) are emitted in the build root and also
  accept Catch2 tag expressions directly (e.g.
  `./security_tests "[security][atna]"`).

The medical-domain unit tests (security, storage, migration, anonymization) are
built into the `security_tests` and `storage_tests` binaries and carry the
`unit` CTest label, so they already run as part of the default `ctest`
invocation in **CI** and **Release**. The gate commands below additionally show
how to run *only* the domain subset for targeted local verification.

## Gate matrix

| # | Gate | Catch2 tag / CTest selector | Test source | Authoritative doc | CI-enforced by | Status |
|---|------|------------------------------|-------------|-------------------|----------------|--------|
| 1 | DICOM conformance | `[dcmtk][echo]`, `[dcmtk][store]`, `[dcmtk][find]`, `[dcmtk][move]` (via `./bin/pacs_integration_e2e`); `integration::` label | `tools/integration_tests/test_dcmtk_{echo,store,find,move}.cpp`, `tests/integration/dicom_workflow_integration_test.cpp` | `docs/DICOM_CONFORMANCE_STATEMENT.md`, `docs/IHE_CONFORMANCE.md` | `dcmtk-interop.yml` (**DCMTK Interoperability Tests**), `integration-tests.yml` (**Integration Tests**) | CI-enforced |
| 2 | TLS / ATNA audit logging | `[security][tls]`, `[security][atna]`, `[security][auditor]`, `[security][syslog]`, `[security][storage]` (audit cipher) | `tests/security/tls_policy_test.cpp`, `tests/security/atna_audit_logger_test.cpp`, `tests/security/atna_service_auditor_test.cpp`, `tests/security/atna_syslog_transport_test.cpp`, `tests/security/audit_log_cipher_test.cpp`; `tests/integration/ihe` ATNA suite (`pacs_xds_integration` label) | `docs/SECURITY_AUDIT.md`, `docs/SDS_SECURITY.md`, `docs/API_REFERENCE_SECURITY.md` | `ci.yml` (**CI**, full `ctest`), `dependency-security-scan.yml` (**Dependency Security Scan**, TLS-config report), `integration-tests.yml` (XDS ATNA bucket) | CI-enforced |
| 3 | Anonymization | `[security][anonymization]` | `tests/security/anonymizer_test.cpp`, `tests/security/uid_mapping_test.cpp`; CLI: `tools/dcm_anonymize` | `docs/SECURITY_AUDIT.md`, DICOM PS3.15 Annex E (referenced from `tools/dcm_anonymize/main.cpp`) | `ci.yml` (**CI**, full `ctest`) | CI-enforced |
| 4 | Storage / index migration | `[migration]`, `[migration][v1..v5]`, `[storage][database]`; plus `storage_boundary_guard` (`boundary` label) | `tests/storage/migration_runner_test.cpp`, `tests/storage/index_database_test.cpp`, `tests/storage/check_storage_boundary.py` | `docs/migration/0.x-to-1.0.md`, `docs/SDS_DATABASE.md` | `ci.yml` (**CI**, full `ctest` incl. `storage_boundary_guard`) | CI-enforced |

Every gate is CI-enforced. There is no manual-only gate in the v1.0 matrix; the
`storage_boundary_guard` (Python, no build required) is the only gate that can
also be run with zero compilation, which makes it the cheapest pre-flight check.

## Gate 1 — DICOM conformance

**What it proves:** the SCP/SCU services interoperate with a reference
toolkit (DCMTK) for the core DIMSE verbs (C-ECHO, C-STORE, C-FIND, C-MOVE) and
that the end-to-end store/query/retrieve workflow holds.

**Workflows (CI-enforced):**

- `DCMTK Interoperability Tests` (`.github/workflows/dcmtk-interop.yml`) — runs
  the Catch2 cases tagged `[dcmtk][echo|store|find|move]` against real DCMTK
  binaries (`echoscu`, `storescp`, `findscu`, `movescu`) and a shell-level
  interop suite. Triggers on push, pull_request, and `workflow_dispatch`.
- `Integration Tests` (`.github/workflows/integration-tests.yml`) — runs the
  `integration::`-labelled suite including
  `dicom_workflow_integration_test.cpp`.

**Local targeted run:**

The DCMTK interop cases live in the `pacs_integration_e2e` binary (sources under
`tools/integration_tests/`, built when DCMTK tools are available) and are
selected by Catch2 tag, exactly as the workflow does:

```bash
# Requires DCMTK tools (echoscu, storescp, findscu, movescu) on PATH:
cd build
./bin/pacs_integration_e2e "[dcmtk][echo]"
./bin/pacs_integration_e2e "[dcmtk][store]"
./bin/pacs_integration_e2e "[dcmtk][find]"
./bin/pacs_integration_e2e "[dcmtk][move]"
# DICOM end-to-end workflow (CTest integration bucket):
ctest --output-on-failure -R "integration::"
```

**Reference doc to cite in the release:** `docs/DICOM_CONFORMANCE_STATEMENT.md`.

## Gate 2 — TLS / ATNA audit logging

**What it proves:** the secure-transport policy is enforced (TLS profile,
cipher policy) and that auditable events are emitted as RFC 3881 / DICOM PS3.15
ATNA messages, transported (syslog/TLS), and protected at rest (AES-256-GCM
envelope + HMAC).

**Workflows (CI-enforced):**

- `CI` (`.github/workflows/ci.yml`) — the default `ctest` run builds and runs
  `security_tests`, which contains all `[security][tls]` and `[security][atna]`
  cases.
- `Dependency Security Scan` (`.github/workflows/dependency-security-scan.yml`)
  — emits a TLS-configuration report for the source tree and runs the
  dependency vulnerability scan (Trivy, OSV, npm audit). Triggers on push,
  pull_request, nightly schedule, and `workflow_dispatch`.
- `Integration Tests` — the XDS.b ATNA integration bucket
  (`pacs_xds_integration` CTest label).

**Local targeted run:**

# Most precise: run the security binary with Catch2 tag expressions. Tags are not
# part of the CTest test names, so tag-based selection must go through the binary.
```bash
cd build
# TLS policy + ATNA emission/transport/at-rest (exact, by tag):
./security_tests "[security][tls]"
./security_tests "[security][atna],[security][auditor],[security][syslog]"
# Audit-at-rest cipher cases (tagged [security][storage] in audit_log_cipher_test):
./security_tests "[security][storage]"
# Approximate alternative via CTest test-name regex (titles, not tags):
ctest --output-on-failure -R "tls_p|atna|audit"
# XDS ATNA integration bucket (CTest label):
ctest --output-on-failure -L pacs_xds_integration
```

**Reference docs to cite:** `docs/SECURITY_AUDIT.md`, `docs/SDS_SECURITY.md`.

## Gate 3 — Anonymization (de-identification)

**What it proves:** de-identification honours the DICOM PS3.15 Annex E
confidentiality profiles (Basic, HIPAA Safe Harbor, GDPR, Retain-* variants),
including UID-mapping consistency and date shifting.

**Workflow (CI-enforced):**

- `CI` (`.github/workflows/ci.yml`) — the default `ctest` run executes the
  `[security][anonymization]` cases in `security_tests`.

**Local targeted run:**

```bash
cd build
# Most precise: by Catch2 tag (covers anonymizer_test + uid_mapping_test):
./security_tests "[security][anonymization]"
# Approximate alternative via CTest test-name regex (matches "Anonymizer: ..."):
ctest --output-on-failure -R "Anonymiz"
```

**CLI smoke check (after a build with `-DPACS_BUILD_EXAMPLES=ON`):**

```bash
# De-identify a sample using a named profile and inspect the result.
# The binary is emitted in the build tree; adjust the path to your build layout.
./dcm_anonymize --profile hipaa_safe_harbor input.dcm anonymized.dcm
./dcm_anonymize --help    # lists the supported profiles and tag options
```

**Reference doc to cite:** `docs/SECURITY_AUDIT.md` (and PS3.15 Annex E).

## Gate 4 — Storage / index migration

**What it proves:** schema migrations (v1→v5 tables/indexes/triggers) apply
forward correctly and the storage-boundary invariants (no cross-layer storage
access) hold.

**Workflow (CI-enforced):**

- `CI` (`.github/workflows/ci.yml`) — the default `ctest` run executes the
  `[migration]` and `[storage][database]` cases in `storage_tests`, plus the
  `storage_boundary_guard` CTest test.

**Local targeted run:**

```bash
# Cheapest pre-flight: storage-boundary guard (pure Python, NO build needed):
python3 tests/storage/check_storage_boundary.py "$(pwd)"
# After a build, the migration + index suite. Most precise is by Catch2 tag:
cd build
./storage_tests "[migration],[storage][database]"
# Approximate alternative via CTest test-name regex:
ctest --output-on-failure -R "migration|index"
```

**Reference docs to cite:** `docs/migration/0.x-to-1.0.md`, `docs/SDS_DATABASE.md`.

## Using the gates in a release

The `Release` workflow (`.github/workflows/release.yml`) runs the full `ctest`
suite in Release mode, which exercises gates 2, 3, and 4 (and the unit slice of
gate 1). The DCMTK-interop slice of gate 1 and the integration slice run on the
`develop` branch via `dcmtk-interop.yml` / `integration-tests.yml` and are
linked from the release notes.

The generated release notes include a **Release Gates** section (see the
`Build release notes` step in `release.yml`) that links back to this matrix so a
reviewer can confirm, from the GitHub Release page alone, that all four
medical-domain gates were verified for that version.

Before tagging a release, complete the checklist below and attach the four run
links (or the local command output) to the release PR or the release issue:

- [ ] **Gate 1 — DICOM conformance**: `DCMTK Interoperability Tests` + `Integration Tests` green on the release commit.
- [ ] **Gate 2 — TLS / ATNA audit**: `CI` green (security suite) + `Dependency Security Scan` green.
- [ ] **Gate 3 — Anonymization**: `CI` green (anonymization suite); optionally a `dcm_anonymize` CLI smoke run attached.
- [ ] **Gate 4 — Storage / index migration**: `CI` green (migration suite + `storage_boundary_guard`).

See [`CONTRIBUTING.md`](../CONTRIBUTING.md) for the contributor-facing summary of
these gates.
