---
doc_id: "PAC-INTR-003"
doc_title: "IHE XDS.b Completion Status"
doc_version: "0.1.0"
doc_date: "2026-04-20"
doc_status: "Draft"
project: "pacs_system"
category: "INTR"
---

# IHE XDS.b Completion Status

> **Purpose**: Concrete, audit-friendly tracking of issue [#1101](https://github.com/kcenon/pacs_system/issues/1101)
> (`feat: complete IHE XDS.b profile implementation`).
> For the broader integration surface and profile list, see the
> [IHE Integration Statement](IHE_INTEGRATION_STATEMENT.md).

## Summary

| Area | Status | Evidence |
|---|---|---|
| Imaging Document **Source** actor | **Implemented** | `src/services/xds/imaging_document_source.cpp` (346 LOC), `include/kcenon/pacs/services/xds/imaging_document_source.h` |
| Document **Source** actor (ITI-41, pure XDS.b peer path) | **Implemented** | `src/ihe/xds/document_source.cpp`, `include/kcenon/pacs/ihe/xds/document_source.h`, transport stack in `src/ihe/xds/common/` (pugixml + libcurl); see [#1128](https://github.com/kcenon/pacs_system/issues/1128) |
| Document **Consumer** actor (ITI-43, pure XDS.b peer path) | **Implemented** | `src/ihe/xds/document_consumer.cpp`, `include/kcenon/pacs/ihe/xds/document_consumer.h`, retrieve helpers in `src/ihe/xds/consumer/` (`retrieve_envelope`, `retrieve_response_parser`); reuses `src/ihe/xds/common/` transport stack; see [#1129](https://github.com/kcenon/pacs_system/issues/1129) |
| Imaging Document **Consumer** actor (incl. Registry Stored Query) | **Implemented** | `src/services/xds/imaging_document_consumer.cpp` (174 LOC), `include/kcenon/pacs/services/xds/imaging_document_consumer.h` |
| ATNA audit records for XDS transactions | **Missing** | `grep -l "atna\|audit" src/services/xds/*.cpp` returns nothing; ATNA infrastructure exists in `src/security/atna_*` but is not wired into XDS transactions |
| Gazelle-style registry integration test | **Missing** | No integration fixture against a test registry; existing `tests/services/xds/xds_imaging_test.cpp` is unit-level (Catch2, mocked registry) |
| Published Conformance Statement | **Partial** | `docs/IHE_INTEGRATION_STATEMENT.md` is released at v0.1.0 and documents XDS-I.b actors + transactions (ITI-41, RAD-68, ITI-43, RAD-69); lacks a dedicated XDS.b (ITI-only) section |

## Scope interpretation note

Issue #1101 phrases the target as "XDS.b". The existing implementation is for
**XDS-I.b** (the imaging specialization that cross-references images via WADO-RS /
C-MOVE). For a DICOM-focused system, XDS-I.b is the correct profile — XDS.b alone
would lose the image-retrieval half. The Source and Consumer in the code today
therefore satisfy the spirit of #1101 for the DICOM/imaging use case.

The remaining work is the **cross-cutting integration** (ATNA) and
**conformance evidence** (Gazelle testing, documentation), not new actor
implementations.

## Remaining Work (tracked as sub-issues)

| # | Sub-issue | Acceptance |
|---|---|---|
| 1 | Wire ATNA audit emitter into XDS Source / Consumer | Each completed ITI-41, RAD-68, ITI-43, RAD-69 transaction emits a DICOM-compatible ATNA message via the existing `atna_service_auditor`; covered by unit test |
| 2 | Add Gazelle-style integration test harness | A CTest-registered integration test that stands up a minimal mock XDS registry in-process, runs one ITI-41 submit + ITI-43 retrieve round trip, and verifies metadata parity |

The "publish conformance statement" AC item from #1101 is already satisfied by
`docs/IHE_INTEGRATION_STATEMENT.md`; no additional document is planned unless the
IHE Connectathon process specifically requires a different format.

## Out of Scope for #1101

The following are explicit non-goals for this issue and are tracked separately
if/when they become requirements:

- Full WSDL-based SOAP envelope validation (current transport layer is pragmatic, not strict WS-I)
- ebXML Registry Filter Query (SQL) grammar support beyond Stored Query
- XCA (Cross-Community Access) — a separate IHE profile

## Verification

1. Confirm the inventory above:
   ```bash
   find src/services/xds -type f
   find include/kcenon/pacs/services/xds -type f
   ```
2. Confirm ATNA gap:
   ```bash
   grep -rn "atna\|audit" src/services/xds/ || echo "no audit wiring"
   ```
3. Confirm test style (Catch2, not GTest):
   ```bash
   grep -n "catch2\|TEST_CASE" tests/services/xds/xds_imaging_test.cpp | head
   ```
