# Software Design Specification (SDS) - PACS System

> **Version:** 0.1.2.0
> **Last Updated:** 2025-12-07
> **Language:** **English** | [한국어](SDS_KO.md)
> **Status:** Complete

---

## Document Information

| Item | Description |
|------|-------------|
| Document ID | PACS-SDS-001 |
| Project | PACS System |
| Author | kcenon@naver.com |
| Reviewers | - |
| Approval Date | - |

### Related Documents

| Document | ID | Version |
|----------|-----|---------|
| Product Requirements Document | [PRD](PRD.md) | 1.0.0 |
| Software Requirements Specification | [SRS](SRS.md) | 1.0.0 |
| Architecture Documentation | [ARCHITECTURE](ARCHITECTURE.md) | 1.0.0 |
| API Reference | [API_REFERENCE](API_REFERENCE.md) | 1.0.0 |

### Document Structure

This SDS is organized into multiple files for maintainability:

| File | Description |
|------|-------------|
| **SDS.md** (this file) | Overview, design principles, module summary |
| [SDS_COMPONENTS.md](SDS_COMPONENTS.md) | Detailed component designs |
| [SDS_INTERFACES.md](SDS_INTERFACES.md) | Interface specifications |
| [SDS_DATABASE.md](SDS_DATABASE.md) | Database schema design |
| [SDS_SEQUENCES.md](SDS_SEQUENCES.md) | Sequence diagrams |
| [SDS_TRACEABILITY.md](SDS_TRACEABILITY.md) | Requirements traceability matrix |
| [SDS_SECURITY.md](SDS_SECURITY.md) | Security module design (RBAC, Anonymization, Digital Signatures) |

---

## Table of Contents

- [1. Introduction](#1-introduction)
- [2. Design Overview](#2-design-overview)
- [3. Design Principles](#3-design-principles)
- [4. Module Summary](#4-module-summary)
- [5. Design Constraints](#5-design-constraints)
- [6. Design Decisions](#6-design-decisions)
- [7. Quality Attributes](#7-quality-attributes)

---

## 1. Introduction

### 1.1 Purpose

This Software Design Specification (SDS) document describes the detailed software design for the PACS (Picture Archiving and Communication System) System. It translates the requirements defined in the SRS into a comprehensive software architecture and component design that can be directly implemented.

### 1.2 Scope

This document covers:

- Detailed design of all software modules
- Component interfaces and interactions
- Data structures and database schema
- Sequence diagrams for key operations
- Design patterns and conventions
- Traceability to requirements

### 1.3 Definitions and Acronyms

| Term | Definition |
|------|------------|
| SDS | Software Design Specification |
| PRD | Product Requirements Document |
| SRS | Software Requirements Specification |
| DICOM | Digital Imaging and Communications in Medicine |
| PDU | Protocol Data Unit |
| DIMSE | DICOM Message Service Element |
| VR | Value Representation |
| SOP | Service-Object Pair |
| SCU | Service Class User |
| SCP | Service Class Provider |
| UID | Unique Identifier |
| RAII | Resource Acquisition Is Initialization |

### 1.4 Design Identifier Convention

Design specifications use the following ID format:

```
DES-<MODULE>-<NUMBER>

Where:
- DES: Design Specification prefix
- MODULE: Module identifier (CORE, ENC, NET, SVC, STOR, INT, DB, SEC)
- NUMBER: 3-digit sequential number
```

**Module Identifiers:**

| Module ID | Module Name | Description |
|-----------|-------------|-------------|
| CORE | Core Module | DICOM data structures |
| ENC | Encoding Module | VR encoding/decoding |
| NET | Network Module | DICOM network protocol |
| SVC | Services Module | DICOM service implementations |
| STOR | Storage Module | Storage backend |
| INT | Interface Module | Ecosystem integration |
| DB | Database Module | Index database design |
| SEC | Security Module | RBAC, anonymization, digital signatures |

---

## 2. Design Overview

### 2.1 System Context

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              External Systems                                │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────────┐ │
│  │  Modalities  │  │   Viewers    │  │     RIS      │  │  Other PACS     │ │
│  │  (CT, MR..)  │  │  (Workstns)  │  │   Systems    │  │    Servers      │ │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘  └────────┬─────────┘ │
│         │                 │                 │                    │          │
│         │    DICOM        │    DICOM        │    HL7/DICOM       │  DICOM   │
│         │    C-STORE      │    C-FIND       │    Worklist        │  C-MOVE  │
│         ▼                 ▼                 ▼                    ▼          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│                           ┌─────────────────────┐                           │
│                           │                     │                           │
│                           │    PACS System      │                           │
│                           │                     │                           │
│                           │  ┌───────────────┐  │                           │
│                           │  │  Storage SCP  │  │                           │
│                           │  │  Query SCP    │  │                           │
│                           │  │  Worklist SCP │  │                           │
│                           │  │  MPPS SCP     │  │                           │
│                           │  └───────────────┘  │                           │
│                           │                     │                           │
│                           └──────────┬──────────┘                           │
│                                      │                                      │
│                                      ▼                                      │
│                           ┌─────────────────────┐                           │
│                           │   Storage Backend   │                           │
│                           │  (Files + Index DB) │                           │
│                           └─────────────────────┘                           │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Architectural Layers

The PACS System follows a layered architecture:

```
┌─────────────────────────────────────────────────────────────────┐
│                    Layer 6: Application                          │
│            (PACS Server, CLI Tools, Examples)                    │
│                                                                  │
│  Traces to: FR-3.x (Services), NFR-1 (Performance)              │
└────────────────────────────────┬────────────────────────────────┘
                                 │
┌────────────────────────────────▼────────────────────────────────┐
│                    Layer 5: Services                             │
│       (Storage SCP/SCU, Query SCP/SCU, Worklist, MPPS)          │
│                                                                  │
│  Traces to: SRS-SVC-xxx (Service Requirements)                  │
└────────────────────────────────┬────────────────────────────────┘
                                 │
┌────────────────────────────────▼────────────────────────────────┐
│                    Layer 4: Protocol                             │
│              (DIMSE Messages, Association Manager)               │
│                                                                  │
│  Traces to: SRS-NET-xxx (Network Requirements)                  │
└────────────────────────────────┬────────────────────────────────┘
                                 │
┌────────────────────────────────▼────────────────────────────────┐
│                    Layer 3: Network                              │
│                    (PDU Encoder/Decoder)                         │
│                                                                  │
│  Traces to: FR-2.1, FR-2.2 (PDU Types, State Machine)           │
└────────────────────────────────┬────────────────────────────────┘
                                 │
┌────────────────────────────────▼────────────────────────────────┐
│                    Layer 2: Core                                 │
│         (DICOM Element, Dataset, File, Dictionary)               │
│                                                                  │
│  Traces to: SRS-CORE-xxx (Core Requirements)                    │
└────────────────────────────────┬────────────────────────────────┘
                                 │
┌────────────────────────────────▼────────────────────────────────┐
│                    Layer 1: Encoding                             │
│            (VR Types, Transfer Syntax, Codecs)                   │
│                                                                  │
│  Traces to: FR-1.2 (VR Types), FR-1.4 (Transfer Syntax)         │
└────────────────────────────────┬────────────────────────────────┘
                                 │
┌────────────────────────────────▼────────────────────────────────┐
│                    Layer 0: Integration                          │
│     (container_adapter, network_adapter, thread_adapter)         │
│                                                                  │
│  Traces to: IR-1.x through IR-5.x (Integration Requirements)    │
└────────────────────────────────┬────────────────────────────────┘
                                 │
┌────────────────────────────────▼────────────────────────────────┐
│                  Ecosystem Foundation                            │
│  (common_system, container_system, network_system, thread_system)│
└─────────────────────────────────────────────────────────────────┘
```

### 2.3 Module Dependencies

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           Module Dependencies                                │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│                    ┌───────────────┐                                        │
│                    │   services    │                                        │
│                    │               │                                        │
│                    │ • storage_scp │                                        │
│                    │ • query_scp   │                                        │
│                    │ • worklist    │                                        │
│                    │ • mpps        │                                        │
│                    └───────┬───────┘                                        │
│                            │                                                │
│              ┌─────────────┼─────────────┐                                  │
│              │             │             │                                  │
│              ▼             ▼             ▼                                  │
│       ┌──────────┐  ┌──────────┐  ┌──────────┐                             │
│       │ network  │  │ storage  │  │   core   │                             │
│       │          │  │          │  │          │                             │
│       │ • pdu    │  │ • file   │  │ • element│                             │
│       │ • dimse  │  │ • index  │  │ • dataset│                             │
│       │ • assoc  │  │          │  │ • file   │                             │
│       └────┬─────┘  └────┬─────┘  └────┬─────┘                             │
│            │             │             │                                    │
│            └─────────────┼─────────────┘                                    │
│                          │                                                  │
│                          ▼                                                  │
│                   ┌──────────┐                                              │
│                   │ encoding │                                              │
│                   │          │                                              │
│                   │ • vr     │                                              │
│                   │ • ts     │                                              │
│                   │ • codec  │                                              │
│                   └────┬─────┘                                              │
│                        │                                                    │
│                        ▼                                                    │
│                 ┌─────────────┐                                             │
│                 │ integration │                                             │
│                 │             │                                             │
│                 │ • container │                                             │
│                 │ • network   │                                             │
│                 │ • thread    │                                             │
│                 └─────────────┘                                             │
│                        │                                                    │
│                        ▼                                                    │
│         ┌──────────────────────────────┐                                    │
│         │     Ecosystem Libraries      │                                    │
│         │  common | container | network│                                    │
│         │  thread | logger | monitoring│                                    │
│         └──────────────────────────────┘                                    │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Design Principles

### 3.1 Ecosystem-First Design

**Principle:** Leverage existing kcenon ecosystem components rather than implementing from scratch.

| Ecosystem Component | PACS Usage | Design Rationale |
|---------------------|------------|------------------|
| `common_system::Result<T>` | Error handling | Consistent error propagation |
| `container_system::value` | VR value storage | Type-safe value handling |
| `network_system::messaging_server` | DICOM server | Proven async I/O |
| `thread_system::thread_pool` | Worker pool | Lock-free job processing |
| `logger_system` | Audit logging | HIPAA compliance |

**Traces to:** IR-1 through IR-5 (Integration Requirements)

### 3.2 Zero External DICOM Dependencies

**Principle:** Implement all DICOM functionality using ecosystem components only.

**Rationale:**
- No GPL/LGPL licensing concerns
- Complete implementation control
- Optimized ecosystem integration
- Full transparency and auditability

**Traces to:** PRD Design Philosophy §3

### 3.3 DICOM Standard Compliance

**Principle:** Follow DICOM PS3.x specifications precisely.

| Standard | Compliance Area |
|----------|-----------------|
| PS3.5 | Data structures and encoding |
| PS3.6 | Data dictionary |
| PS3.7 | Message exchange |
| PS3.8 | Network communication |

**Traces to:** PRD Design Philosophy §2

### 3.4 Production-Grade Quality

**Principle:** Maintain ecosystem-level quality standards.

| Quality Metric | Target | Verification |
|----------------|--------|--------------|
| RAII Grade | A | Code review, static analysis |
| Memory Leaks | Zero | AddressSanitizer |
| Data Races | Zero | ThreadSanitizer |
| Test Coverage | ≥80% | Coverage reports |
| API Documentation | Complete | Doxygen |

**Traces to:** NFR-2 (Reliability), NFR-4 (Maintainability)

---

## 4. Module Summary

### 4.1 Core Module (pacs_core)

**Purpose:** Fundamental DICOM data structures

**Key Components:**

| Component | Design ID | Description | Traces to |
|-----------|-----------|-------------|-----------|
| `dicom_tag` | DES-CORE-001 | Tag representation (Group, Element) | SRS-CORE-001 |
| `dicom_element` | DES-CORE-002 | Data Element (Tag, VR, Value) | SRS-CORE-002 |
| `dicom_dataset` | DES-CORE-003 | Ordered element collection | SRS-CORE-003 |
| `dicom_file` | DES-CORE-004 | Part 10 file handling | SRS-CORE-004 |
| `dicom_dictionary` | DES-CORE-005 | Tag metadata lookup | SRS-CORE-005 |

### 4.2 Encoding Module (pacs_encoding)

**Purpose:** VR encoding and Transfer Syntax handling

**Key Components:**

| Component | Design ID | Description | Traces to |
|-----------|-----------|-------------|-----------|
| `vr_type` | DES-ENC-001 | 27 VR type enumeration | SRS-CORE-006 |
| `vr_info` | DES-ENC-002 | VR metadata utilities | SRS-CORE-006 |
| `transfer_syntax` | DES-ENC-003 | Transfer Syntax handling | SRS-CORE-007 |
| `implicit_vr_codec` | DES-ENC-004 | Implicit VR encoder/decoder | SRS-CORE-008 |
| `explicit_vr_codec` | DES-ENC-005 | Explicit VR encoder/decoder | SRS-CORE-008 |

### 4.3 Network Module (pacs_network)

**Purpose:** DICOM network protocol implementation

**Key Components:**

| Component | Design ID | Description | Traces to |
|-----------|-----------|-------------|-----------|
| `pdu_encoder` | DES-NET-001 | PDU serialization | SRS-NET-001 |
| `pdu_decoder` | DES-NET-002 | PDU deserialization | SRS-NET-001 |
| `dimse_message` | DES-NET-003 | DIMSE message handling | SRS-NET-002 |
| `association` | DES-NET-004 | Association state machine | SRS-NET-003 |
| `dicom_server` | DES-NET-005 | Multi-association server | SRS-NET-004 |

### 4.4 Services Module (pacs_services)

**Purpose:** DICOM service implementations

**Key Components:**

| Component | Design ID | Description | Traces to | Status |
|-----------|-----------|-------------|-----------|--------|
| `verification_scp` | DES-SVC-001 | C-ECHO handler | SRS-SVC-001 | ✅ |
| `storage_scp` | DES-SVC-002 | C-STORE receiver | SRS-SVC-002 | ✅ |
| `storage_scu` | DES-SVC-003 | C-STORE sender | SRS-SVC-003 | ✅ |
| `query_scp` | DES-SVC-004 | C-FIND handler | SRS-SVC-004 | ✅ |
| `retrieve_scp` | DES-SVC-005 | C-MOVE/C-GET handler | SRS-SVC-005 | ✅ |
| `worklist_scp` | DES-SVC-006 | MWL C-FIND handler | SRS-SVC-006 | ✅ |
| `mpps_scp` | DES-SVC-007 | N-CREATE/N-SET handler | SRS-SVC-007 | ✅ |
| `dimse_n_encoder` | DES-SVC-008 | N-GET/N-ACTION/N-EVENT/N-DELETE | SRS-SVC-008 | ✅ |
| `ultrasound_storage` | DES-SVC-009 | US/US-MF SOP classes | SRS-SVC-009 | ✅ |
| `xa_storage` | DES-SVC-010 | XA/XRF SOP classes | SRS-SVC-010 | ✅ |

### 4.5 Storage Module (pacs_storage)

**Purpose:** Persistent storage backend

**Key Components:**

| Component | Design ID | Description | Traces to |
|-----------|-----------|-------------|-----------|
| `storage_interface` | DES-STOR-001 | Abstract storage API | SRS-STOR-001 |
| `file_storage` | DES-STOR-002 | Filesystem implementation | SRS-STOR-002 |
| `index_database` | DES-STOR-003 | SQLite index | SRS-STOR-003 |

### 4.6 Integration Module (pacs_integration)

**Purpose:** Ecosystem component adapters

**Key Components:**

| Component | Design ID | Description | Traces to | Status |
|-----------|-----------|-------------|-----------|--------|
| `container_adapter` | DES-INT-001 | VR to container mapping | IR-1 | ✅ |
| `network_adapter` | DES-INT-002 | TCP/TLS via network_system | IR-2 | ✅ |
| `thread_adapter` | DES-INT-003 | Job processing, thread pool | IR-3 | ✅ |
| `accept_worker` | DES-INT-003a | TCP socket bind/listen/accept (thread_base) | IR-3 | ✅ (v1.2.0) |
| `logger_adapter` | DES-INT-004 | Audit logging | IR-4 | ✅ |
| `monitoring_adapter` | DES-INT-005 | Performance metrics | IR-5 | ✅ |

### 4.7 Network V2 Module (pacs_network_v2) - Optional

**Purpose:** network_system-based DICOM server implementation

**Key Components:**

| Component | Design ID | Description | Traces to | Status |
|-----------|-----------|-------------|-----------|--------|
| `dicom_server_v2` | DES-NET-006 | messaging_server-based DICOM server | SRS-INT-003 | ✅ |
| `dicom_association_handler` | DES-NET-007 | Per-session PDU framing and dispatching | SRS-INT-003 | ✅ |

**Compile Flag:** `PACS_WITH_NETWORK_SYSTEM`

---

## 5. Design Constraints

### 5.1 Technical Constraints

| Constraint | Description | Impact |
|------------|-------------|--------|
| C++20 Required | Use of concepts, ranges, coroutines | Minimum compiler versions |
| No External DICOM | DCMTK, GDCM prohibited | Full internal implementation |
| Ecosystem Versions | Specific ecosystem library versions | Dependency management |
| DICOM Conformance | PS3.x compliance mandatory | Implementation complexity |

### 5.2 Resource Constraints

| Resource | Constraint | Design Decision |
|----------|------------|-----------------|
| Memory | Large images (up to 2GB) | Streaming processing, memory mapping |
| Threads | Limited thread count | Thread pool with configurable size |
| Storage | Potentially millions of files | Hierarchical directory structure |
| Network | Multiple concurrent associations | Async I/O, connection pooling |

### 5.3 Interface Constraints

| Interface | Constraint | Source |
|-----------|------------|--------|
| DICOM Port | Default 11112, configurable | DICOM standard |
| PDU Size | Max 131072 bytes default | PS3.8 |
| AE Title | Max 16 characters | PS3.5 |
| UID | Max 64 characters | PS3.5 |

---

## 6. Design Decisions

### 6.1 Key Design Decisions

| Decision ID | Decision | Rationale | Alternatives Considered |
|-------------|----------|-----------|------------------------|
| DD-001 | Use `std::map` for dataset elements | Tag ordering required by DICOM | `std::unordered_map` (faster but unordered) |
| DD-002 | SQLite for index database | Embedded, ACID, zero-config | PostgreSQL (overkill), custom (risky) |
| DD-003 | Hierarchical file storage | Natural DICOM hierarchy | Flat (UID naming collision risk) |
| DD-004 | Result<T> for error handling | Ecosystem consistency | Exceptions (performance overhead) |
| DD-005 | Async I/O via ASIO | Proven scalability | select/poll (less portable) |
| DD-006 | Thread pool for DIMSE | Decouple I/O from processing | Per-association threads (resource heavy) |

### 6.2 Technology Choices

| Technology | Version | Purpose |
|------------|---------|---------|
| C++ | 20 | Implementation language |
| CMake | 3.20+ | Build system |
| SQLite | 3.36+ | Index database |
| ASIO | (via network_system) | Async networking |
| Google Test | 1.11+ | Unit testing |
| Google Benchmark | 1.6+ | Performance testing |

---

## 7. Quality Attributes

### 7.1 Performance Targets

| Metric | Target | Design Solution | Traces to |
|--------|--------|-----------------|-----------|
| C-STORE throughput | ≥50 images/sec | Async I/O, thread pool | NFR-1.1 |
| C-FIND latency | <100ms (1000 studies) | Indexed queries | NFR-1.2 |
| Concurrent associations | ≥20 | Async multiplexing | NFR-1.3 |
| Memory per association | <10MB | Streaming, pooling | NFR-1.4 |

### 7.2 Reliability Targets

| Metric | Target | Design Solution | Traces to |
|--------|--------|-----------------|-----------|
| Uptime | 99.9% | Graceful degradation | NFR-2.1 |
| Data integrity | Zero loss | Atomic transactions | NFR-2.2 |
| Error recovery | Automatic | Retry mechanisms | NFR-2.3 |

### 7.3 Security Targets

| Metric | Target | Design Solution | Traces to |
|--------|--------|-----------------|-----------|
| Transport | TLS 1.2/1.3 | network_system TLS | NFR-3.1 |
| Access control | AE whitelist | Configuration-based | NFR-3.2 |
| Audit logging | Complete | logger_system | NFR-3.3 |

### 7.4 Maintainability Targets

| Metric | Target | Design Solution | Traces to |
|--------|--------|-----------------|-----------|
| Test coverage | ≥80% | Comprehensive test suite | NFR-4.1 |
| Documentation | Complete | Doxygen + guides | NFR-4.2 |
| Static analysis | Clean | clang-tidy integration | NFR-4.3 |

---

## References

1. DICOM Standard PS3.1-PS3.20
2. [PRD - Product Requirements Document](PRD.md)
3. [SRS - Software Requirements Specification](SRS.md)
4. [ARCHITECTURE - Architecture Documentation](ARCHITECTURE.md)
5. [API_REFERENCE - API Reference](API_REFERENCE.md)
6. kcenon Ecosystem Documentation

---

## Document History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0.0 | 2025-11-30 | kcenon | Initial release |
| 1.1.0 | 2025-12-04 | kcenon | Updated component status, added transfer syntax details |
| 1.2.0 | 2025-12-07 | kcenon | Added: DES-SVC-008~010 (DIMSE-N, Ultrasound, XA), DES-INT-003a (accept_worker), DES-NET-006~007 (Network V2); Updated thread_adapter design for thread_system migration |
| 1.3.0 | 2026-01-02 | kcenon | Updated accept_worker: Implemented TCP socket bind/listen/accept replacing placeholder implementation |

---

*Document Version: 0.1.3.0*
*Created: 2025-11-30*
*Updated: 2026-01-02*
*Author: kcenon@naver.com*
