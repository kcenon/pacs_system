---
doc_id: "PAC-ADR-003"
doc_title: "ADR-003: DICOM Layer Separation Architecture"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Accepted"
project: "pacs_system"
category: "ADR"
---

# ADR-003: DICOM Layer Separation Architecture

> **SSOT**: This document is the single source of truth for **ADR-003: DICOM Layer Separation Architecture**.

| Field | Value |
|-------|-------|
| Status | Accepted |
| Date | 2025-09-01 |
| Decision Makers | kcenon ecosystem maintainers |

## Context

pacs_system implements the DICOM standard from scratch — no external DICOM library
dependency (dcmtk, GDCM). The DICOM standard spans encoding (Part 5), file format
(Part 10), network protocol (Part 7/8), security (Part 15), and numerous service
classes (Parts 3/4).

A monolithic DICOM implementation would create a single massive library where data
encoding, network protocol, and application services are tightly coupled. This prevents:
1. Independent testing of encoding vs. network vs. services.
2. Selective use (e.g., file I/O without networking).
3. Clear ownership boundaries for development.

## Decision

**Organize DICOM functionality into 12 layered library targets** with explicit
dependency relationships:

```
Layer 0 (Core):     pacs_core       - Tags, elements, datasets, Part 10 file I/O
Layer 1 (Encoding): pacs_encoding   - VR types, transfer syntaxes, compression codecs
Layer 2 (Security): pacs_security   - RBAC, anonymization, digital signatures, TLS
Layer 3 (Network):  pacs_network    - PDU encode/decode, association state machine, DIMSE
Layer 4 (Client):   pacs_client     - Job management, routing, sync, prefetch
Layer 5 (Services): pacs_services   - SCP/SCU implementations, SOP class registry
Layer 6 (Storage):  pacs_storage    - File storage, SQLite indexing, cloud, HSM
Layer 7 (AI):       pacs_ai         - AI service connector, SR/SEG result handling
Layer 8 (Monitor):  pacs_monitoring - Health checks, Prometheus metrics
Layer 9 (Workflow): pacs_workflow   - Auto prefetch, task scheduler, study lock
Layer 10 (Web):     pacs_web        - REST API, DICOMweb (WADO/STOW/QIDO)
Layer 11 (Bridge):  pacs_integration- Adapters to kcenon ecosystem
```

Dependencies flow strictly downward. `pacs_core` has no PACS-internal dependencies.
`pacs_services` depends on `pacs_network`, which depends on `pacs_encoding`, which
depends on `pacs_core`.

## Alternatives Considered

### Monolithic DICOM Library

- **Pros**: Simple build, no inter-library dependency management.
- **Cons**: Any change triggers full rebuild. Testing network code requires
  compiling encoding and service code. Binary size is maximal even for
  applications that only need file I/O.

### External DICOM Library (dcmtk/GDCM)

- **Pros**: Mature, battle-tested, extensive SOP class support.
- **Cons**: dcmtk uses C-style APIs incompatible with the ecosystem's C++20
  patterns (`Result<T>`, concepts, coroutines). GDCM has a complex build system.
  Both impose their own threading and memory models. Custom implementation
  enables full integration with the kcenon ecosystem's error handling, threading,
  and monitoring infrastructure.

### Micro-Libraries (One per SOP Class)

- **Pros**: Maximum granularity, minimal binary size.
- **Cons**: 50+ SOP classes would create 50+ libraries. Inter-SOP dependencies
  (Verification used by all services, QueryRetrieve used by many workflows)
  create a complex dependency web. Build system becomes unmanageable.

## Consequences

### Positive

- **Selective linking**: Applications link only the layers they need. A DICOM
  file viewer links `pacs_core` + `pacs_encoding` without network code.
- **Independent testing**: Each layer has its own test suite that compiles
  independently. `pacs_encoding` tests do not require a network stack.
- **Parallel development**: Teams can work on `pacs_web` and `pacs_storage`
  simultaneously without merge conflicts in shared code.
- **Clear boundaries**: Layer dependencies are enforced by CMake. Accidental
  upward dependencies cause build failures.

### Negative

- **12 library targets**: Build system complexity with 12 inter-dependent targets
  requires careful CMakeLists.txt management and documentation.
- **Circular dependency risk**: The strict layering must be maintained. Some
  features (e.g., storage notifications triggering network events) require
  callback interfaces to avoid upward dependencies.
- **Linux static linking**: Circular CMake dependencies on Linux require
  `--start-group`/`--end-group` linker flags, which are platform-specific.
