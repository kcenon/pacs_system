# SDS - Requirements Traceability Matrix

> **Version:** 1.0.0
> **Parent Document:** [SDS.md](SDS.md)
> **Last Updated:** 2025-12-01

---

## Table of Contents

- [1. Overview](#1-overview)
- [2. PRD to SRS Traceability](#2-prd-to-srs-traceability)
- [3. SRS to SDS Traceability](#3-srs-to-sds-traceability)
- [4. Complete Traceability Chain](#4-complete-traceability-chain)
- [5. SDS to Implementation Traceability](#5-sds-to-implementation-traceability)
- [6. SDS to Test Traceability](#6-sds-to-test-traceability)
- [7. Coverage Analysis](#7-coverage-analysis)

---

## 1. Overview

### 1.1 Purpose

This document provides complete traceability from Product Requirements (PRD) through Software Requirements (SRS) to Design Specifications (SDS). It ensures:

- All requirements are addressed by design
- No design elements exist without requirements
- Test planning can reference design elements
- Impact analysis for requirement changes

### 1.2 Traceability Chain (V-Model)

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                              V-Model Traceability                                │
├─────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  Requirements Phase                              Validation/Verification Phase   │
│  ─────────────────                              ─────────────────────────────    │
│                                                                                  │
│  ┌─────────┐                                              ┌─────────────────┐   │
│  │   PRD   │◄────────────────────────────────────────────►│ Acceptance Test │   │
│  │  FR-x   │                  Validation                  │   (Validation)  │   │
│  │  NFR-x  │                                              └─────────────────┘   │
│  │  IR-x   │                                                       ▲            │
│  └────┬────┘                                                       │            │
│       │                                                            │            │
│       ▼                                                            │            │
│  ┌─────────┐                                              ┌─────────────────┐   │
│  │   SRS   │◄────────────────────────────────────────────►│   System Test   │   │
│  │SRS-xxx  │                  Validation                  │   (Validation)  │   │
│  └────┬────┘                                              └─────────────────┘   │
│       │                                                            ▲            │
│       ▼                                                            │            │
│  ┌─────────┐                                              ┌─────────────────┐   │
│  │   SDS   │◄────────────────────────────────────────────►│Integration Test │   │
│  │DES-xxx  │                 Verification                 │  (Verification) │   │
│  │SEQ-xxx  │                                              └─────────────────┘   │
│  └────┬────┘                                                       ▲            │
│       │                                                            │            │
│       ▼                                                            │            │
│  ┌─────────┐                                              ┌─────────────────┐   │
│  │  Code   │◄────────────────────────────────────────────►│    Unit Test    │   │
│  │ .hpp    │                 Verification                 │  (Verification) │   │
│  │ .cpp    │                                              └─────────────────┘   │
│  └─────────┘                                                                    │
│                                                                                  │
└─────────────────────────────────────────────────────────────────────────────────┘

Legend:
  - Validation: "Are we building the RIGHT product?" (SRS 요구사항 충족 확인)
  - Verification: "Are we building the product RIGHT?" (SDS 설계대로 구현 확인)
```

### 1.3 Requirement ID Conventions

| Document | Format | Example |
|----------|--------|---------|
| PRD Functional | FR-X.Y | FR-1.1, FR-3.2 |
| PRD Non-Functional | NFR-X | NFR-1, NFR-3 |
| PRD Integration | IR-X | IR-1, IR-3 |
| SRS | SRS-MOD-XXX | SRS-CORE-001 |
| SDS Component | DES-MOD-XXX | DES-CORE-001 |
| SDS Sequence | SEQ-XXX | SEQ-004 |
| SDS Database | DES-DB-XXX | DES-DB-001 |

---

## 2. PRD to SRS Traceability

### 2.1 Functional Requirements

| PRD ID | PRD Description | SRS ID(s) | Coverage |
|--------|-----------------|-----------|----------|
| **FR-1.1** | DICOM Data Element (Tag, VR, Length, Value) | SRS-CORE-001, SRS-CORE-002 | Full |
| **FR-1.2** | 27 VR Types per PS3.5 | SRS-CORE-006 | Full |
| **FR-1.3** | DICOM Part 10 File Format | SRS-CORE-004 | Full |
| **FR-1.4** | Transfer Syntax Support | SRS-CORE-007, SRS-CORE-008 | Full |
| **FR-1.5** | Data Dictionary (PS3.6) | SRS-CORE-005 | Full |
| **FR-2.1** | PDU Types (A-ASSOCIATE, P-DATA, etc.) | SRS-NET-001 | Full |
| **FR-2.2** | Association State Machine (8 states) | SRS-NET-003 | Full |
| **FR-2.3** | DIMSE Message Types | SRS-NET-002 | Full |
| **FR-3.1** | Storage Service (C-STORE) | SRS-SVC-002, SRS-SVC-003 | Full |
| **FR-3.2** | Query/Retrieve Service (C-FIND, C-MOVE) | SRS-SVC-004, SRS-SVC-005 | Full |
| **FR-3.3** | Modality Worklist (MWL) | SRS-SVC-006 | Full |
| **FR-3.4** | MPPS Service (N-CREATE, N-SET) | SRS-SVC-007 | Full |
| **FR-4.1** | Hierarchical File Storage | SRS-STOR-002 | Full |
| **FR-4.2** | Index Database | SRS-STOR-003 | Full |

### 2.2 Non-Functional Requirements

| PRD ID | PRD Description | SRS ID(s) | Coverage |
|--------|-----------------|-----------|----------|
| **NFR-1** | Performance (throughput, latency) | SRS-PERF-001 to SRS-PERF-004 | Full |
| **NFR-2** | Reliability (uptime, data integrity) | SRS-REL-001 to SRS-REL-003 | Full |
| **NFR-3** | Security (TLS, access control, audit) | SRS-SEC-001 to SRS-SEC-003 | Full |
| **NFR-4** | Maintainability (test coverage, docs) | SRS-MAINT-001 to SRS-MAINT-003 | Full |

### 2.3 Integration Requirements

| PRD ID | PRD Description | SRS ID(s) | Coverage |
|--------|-----------------|-----------|----------|
| **IR-1** | container_system Integration | SRS-INT-001 | Full |
| **IR-2** | network_system Integration | SRS-INT-002 | Full |
| **IR-3** | thread_system Integration | SRS-INT-003 | Full |
| **IR-4** | logger_system Integration | SRS-INT-004 | Full |
| **IR-5** | monitoring_system Integration | SRS-INT-005 | Full |

---

## 3. SRS to SDS Traceability

### 3.1 Core Module Requirements

| SRS ID | SRS Description | SDS ID(s) | Design Element |
|--------|-----------------|-----------|----------------|
| **SRS-CORE-001** | dicom_tag implementation | DES-CORE-001 | `dicom_tag` class |
| **SRS-CORE-002** | dicom_element implementation | DES-CORE-002 | `dicom_element` class |
| **SRS-CORE-003** | dicom_dataset implementation | DES-CORE-003 | `dicom_dataset` class |
| **SRS-CORE-004** | dicom_file read/write | DES-CORE-004 | `dicom_file` class |
| **SRS-CORE-005** | dicom_dictionary lookup | DES-CORE-005 | `dicom_dictionary` class |
| **SRS-CORE-006** | VR type handling | DES-ENC-001, DES-ENC-002 | `vr_type`, `vr_info` |
| **SRS-CORE-007** | Transfer Syntax handling | DES-ENC-003 | `transfer_syntax` class |
| **SRS-CORE-008** | VR encoding/decoding | DES-ENC-004, DES-ENC-005 | Codecs |

### 3.2 Network Module Requirements

| SRS ID | SRS Description | SDS ID(s) | Design Element |
|--------|-----------------|-----------|----------------|
| **SRS-NET-001** | PDU encoding/decoding | DES-NET-001, DES-NET-002 | `pdu_encoder`, `pdu_decoder` |
| **SRS-NET-002** | DIMSE message handling | DES-NET-003 | `dimse_message` class |
| **SRS-NET-003** | Association management | DES-NET-004 | `association` class |
| **SRS-NET-004** | DICOM server | DES-NET-005 | `dicom_server` class |

### 3.3 Service Module Requirements

| SRS ID | SRS Description | SDS ID(s) | Design Element |
|--------|-----------------|-----------|----------------|
| **SRS-SVC-001** | Verification SCP | DES-SVC-001 | `verification_scp` class |
| **SRS-SVC-002** | Storage SCP | DES-SVC-002 | `storage_scp` class |
| **SRS-SVC-003** | Storage SCU | DES-SVC-003 | `storage_scu` class |
| **SRS-SVC-004** | Query SCP (C-FIND) | DES-SVC-004 | `query_scp` class |
| **SRS-SVC-005** | Retrieve SCP (C-MOVE/C-GET) | DES-SVC-005 | `retrieve_scp` class |
| **SRS-SVC-006** | Worklist SCP | DES-SVC-006 | `worklist_scp` class |
| **SRS-SVC-007** | MPPS SCP | DES-SVC-007 | `mpps_scp` class |

### 3.4 Storage Module Requirements

| SRS ID | SRS Description | SDS ID(s) | Design Element |
|--------|-----------------|-----------|----------------|
| **SRS-STOR-001** | Storage interface | DES-STOR-001 | `storage_interface` |
| **SRS-STOR-002** | File storage implementation | DES-STOR-002 | `file_storage` class |
| **SRS-STOR-003** | Index database | DES-STOR-003, DES-DB-001 to DES-DB-007 | `index_database`, tables |

### 3.5 Integration Module Requirements

| SRS ID | SRS Description | SDS ID(s) | Design Element |
|--------|-----------------|-----------|----------------|
| **SRS-INT-001** | container_system adapter | DES-INT-001 | `container_adapter` |
| **SRS-INT-002** | network_system adapter | DES-INT-002 | `network_adapter` |
| **SRS-INT-003** | thread_system adapter | DES-INT-003 | `thread_adapter` |
| **SRS-INT-004** | logger_system adapter | DES-INT-004 | `logger_adapter` |
| **SRS-INT-005** | monitoring_system adapter | DES-INT-005 | `monitoring_adapter` |

### 3.6 Sequence Diagram Mapping

| SRS ID | Scenario | SEQ ID(s) | Diagram Name |
|--------|----------|-----------|--------------|
| SRS-NET-003 | Association Establishment | SEQ-001, SEQ-002, SEQ-003 | Association flows |
| SRS-SVC-002 | C-STORE Reception | SEQ-004, SEQ-005 | Storage flows |
| SRS-SVC-004 | C-FIND Query | SEQ-006, SEQ-007 | Query flows |
| SRS-SVC-005 | C-MOVE Retrieve | SEQ-008 | Retrieve flow |
| SRS-SVC-006 | MWL Query | SEQ-009 | Worklist flow |
| SRS-SVC-007 | MPPS Status | SEQ-010, SEQ-011 | MPPS flows |
| SRS-REL-003 | Error Recovery | SEQ-012, SEQ-013, SEQ-014 | Error handling |

---

## 4. Complete Traceability Chain

### 4.1 DICOM Core Data Handling

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     DICOM Core Data Handling Trace                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  PRD                  SRS                    SDS                            │
│  ───────────────────────────────────────────────────────────────            │
│                                                                              │
│  FR-1.1 ──────────► SRS-CORE-001 ────────► DES-CORE-001 (dicom_tag)        │
│  (Data Element)     SRS-CORE-002 ────────► DES-CORE-002 (dicom_element)    │
│                                                                              │
│  FR-1.2 ──────────► SRS-CORE-006 ────────► DES-ENC-001 (vr_type)           │
│  (VR Types)                        ────────► DES-ENC-002 (vr_info)          │
│                                                                              │
│  FR-1.3 ──────────► SRS-CORE-004 ────────► DES-CORE-004 (dicom_file)       │
│  (Part 10 File)                                                              │
│                                                                              │
│  FR-1.4 ──────────► SRS-CORE-007 ────────► DES-ENC-003 (transfer_syntax)   │
│  (Transfer Syntax)  SRS-CORE-008 ────────► DES-ENC-004 (implicit_vr_codec) │
│                                    ────────► DES-ENC-005 (explicit_vr_codec) │
│                                                                              │
│  FR-1.5 ──────────► SRS-CORE-005 ────────► DES-CORE-005 (dicom_dictionary) │
│  (Data Dictionary)                                                           │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 4.2 Network Protocol

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                       Network Protocol Trace                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  PRD                  SRS                    SDS                            │
│  ───────────────────────────────────────────────────────────────            │
│                                                                              │
│  FR-2.1 ──────────► SRS-NET-001 ─────────► DES-NET-001 (pdu_encoder)       │
│  (PDU Types)                       ─────────► DES-NET-002 (pdu_decoder)      │
│                                                                              │
│  FR-2.2 ──────────► SRS-NET-003 ─────────► DES-NET-004 (association)        │
│  (State Machine)                   ─────────► SEQ-001 (Establishment)        │
│                                    ─────────► SEQ-002 (Rejection)            │
│                                    ─────────► SEQ-003 (Release)              │
│                                                                              │
│  FR-2.3 ──────────► SRS-NET-002 ─────────► DES-NET-003 (dimse_message)      │
│  (DIMSE Messages)                                                            │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 4.3 DICOM Services

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         DICOM Services Trace                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  PRD                  SRS                    SDS                            │
│  ───────────────────────────────────────────────────────────────            │
│                                                                              │
│  FR-3.1 ──────────► SRS-SVC-002 ─────────► DES-SVC-002 (storage_scp)       │
│  (Storage Service)  SRS-SVC-003 ─────────► DES-SVC-003 (storage_scu)       │
│                                  ─────────► SEQ-004 (C-STORE Success)       │
│                                  ─────────► SEQ-005 (Duplicate Handling)    │
│                                                                              │
│  FR-3.2 ──────────► SRS-SVC-004 ─────────► DES-SVC-004 (query_scp)         │
│  (Query/Retrieve)   SRS-SVC-005 ─────────► DES-SVC-005 (retrieve_scp)      │
│                                  ─────────► SEQ-006 (Patient Root Query)   │
│                                  ─────────► SEQ-007 (C-FIND Cancel)        │
│                                  ─────────► SEQ-008 (C-MOVE Retrieve)      │
│                                                                              │
│  FR-3.3 ──────────► SRS-SVC-006 ─────────► DES-SVC-006 (worklist_scp)      │
│  (Worklist)                       ─────────► SEQ-009 (MWL Query)            │
│                                  ─────────► DES-DB-006 (worklist table)    │
│                                                                              │
│  FR-3.4 ──────────► SRS-SVC-007 ─────────► DES-SVC-007 (mpps_scp)          │
│  (MPPS)                           ─────────► SEQ-010 (MPPS In Progress)     │
│                                  ─────────► SEQ-011 (MPPS Completed)       │
│                                  ─────────► DES-DB-005 (mpps table)        │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 4.4 Storage Backend

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        Storage Backend Trace                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  PRD                  SRS                    SDS                            │
│  ───────────────────────────────────────────────────────────────            │
│                                                                              │
│  FR-4.1 ──────────► SRS-STOR-001 ────────► DES-STOR-001 (storage_interface)│
│  (File Storage)     SRS-STOR-002 ────────► DES-STOR-002 (file_storage)     │
│                                                                              │
│  FR-4.2 ──────────► SRS-STOR-003 ────────► DES-STOR-003 (index_database)   │
│  (Index Database)                 ────────► DES-DB-001 (patients table)    │
│                                   ────────► DES-DB-002 (studies table)     │
│                                   ────────► DES-DB-003 (series table)      │
│                                   ────────► DES-DB-004 (instances table)   │
│                                   ────────► DES-DB-Q001 to DES-DB-Q006     │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 4.5 Integration

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         Integration Trace                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  PRD                  SRS                    SDS                            │
│  ───────────────────────────────────────────────────────────────            │
│                                                                              │
│  IR-1 ────────────► SRS-INT-001 ─────────► DES-INT-001 (container_adapter) │
│  (container_system)              VR ↔ value mapping                         │
│                                                                              │
│  IR-2 ────────────► SRS-INT-002 ─────────► DES-INT-002 (network_adapter)   │
│  (network_system)                TCP/TLS handling                           │
│                                                                              │
│  IR-3 ────────────► SRS-INT-003 ─────────► DES-INT-003 (thread_adapter)    │
│  (thread_system)                 Job queue, thread pool                     │
│                                                                              │
│  IR-4 ────────────► SRS-INT-004 ─────────► DES-INT-004 (logger_adapter)    │
│  (logger_system)                 Audit logging                              │
│                                                                              │
│  IR-5 ────────────► SRS-INT-005 ─────────► DES-INT-005 (monitoring_adapter)│
│  (monitoring_system)             Performance metrics                        │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 4.6 Non-Functional Requirements

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      Non-Functional Requirements Trace                       │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  PRD                  SRS                    SDS                            │
│  ───────────────────────────────────────────────────────────────            │
│                                                                              │
│  NFR-1 ───────────► SRS-PERF-001 ────────► Thread pool design              │
│  (Performance)      SRS-PERF-002 ────────► Async I/O design                │
│                     SRS-PERF-003 ────────► Index optimization              │
│                     SRS-PERF-004 ────────► Memory pooling                  │
│                                                                              │
│  NFR-2 ───────────► SRS-REL-001 ─────────► Atomic transactions             │
│  (Reliability)      SRS-REL-002 ─────────► RAII resource management        │
│                     SRS-REL-003 ─────────► SEQ-012 to SEQ-014 (Error)      │
│                                                                              │
│  NFR-3 ───────────► SRS-SEC-001 ─────────► TLS in network_adapter          │
│  (Security)         SRS-SEC-002 ─────────► AE Title whitelist              │
│                     SRS-SEC-003 ─────────► logger_adapter audit            │
│                                                                              │
│  NFR-4 ───────────► SRS-MAINT-001 ───────► Test coverage targets           │
│  (Maintainability)  SRS-MAINT-002 ───────► API documentation               │
│                     SRS-MAINT-003 ───────► Static analysis                 │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 5. SDS to Implementation Traceability

This section maps design elements to their corresponding source code files.

### 5.1 Core Module Implementation

| SDS ID | Design Element | Header File | Source File |
|--------|---------------|-------------|-------------|
| DES-CORE-001 | `dicom_tag` | `include/pacs/core/dicom_tag.hpp` | `src/core/dicom_tag.cpp` |
| DES-CORE-002 | `dicom_element` | `include/pacs/core/dicom_element.hpp` | `src/core/dicom_element.cpp` |
| DES-CORE-003 | `dicom_dataset` | `include/pacs/core/dicom_dataset.hpp` | `src/core/dicom_dataset.cpp` |
| DES-CORE-004 | `dicom_file` | `include/pacs/core/dicom_file.hpp` | `src/core/dicom_file.cpp` |
| DES-CORE-005 | `dicom_dictionary` | `include/pacs/core/dicom_dictionary.hpp` | `src/core/dicom_dictionary.cpp`, `src/core/standard_tags_data.cpp` |
| - | `tag_info` | `include/pacs/core/tag_info.hpp` | `src/core/tag_info.cpp` |
| - | `dicom_tag_constants` | `include/pacs/core/dicom_tag_constants.hpp` | (header-only) |

### 5.2 Encoding Module Implementation

| SDS ID | Design Element | Header File | Source File |
|--------|---------------|-------------|-------------|
| DES-ENC-001 | `vr_type` | `include/pacs/encoding/vr_type.hpp` | (header-only, enum) |
| DES-ENC-002 | `vr_info` | `include/pacs/encoding/vr_info.hpp` | `src/encoding/vr_info.cpp` |
| DES-ENC-003 | `transfer_syntax` | `include/pacs/encoding/transfer_syntax.hpp` | `src/encoding/transfer_syntax.cpp` |
| DES-ENC-004 | `implicit_vr_codec` | `include/pacs/encoding/implicit_vr_codec.hpp` | `src/encoding/implicit_vr_codec.cpp` |
| DES-ENC-005 | `explicit_vr_codec` | `include/pacs/encoding/explicit_vr_codec.hpp` | `src/encoding/explicit_vr_codec.cpp` |
| - | `byte_order` | `include/pacs/encoding/byte_order.hpp` | (header-only) |

### 5.3 Network Module Implementation

| SDS ID | Design Element | Header File | Source File |
|--------|---------------|-------------|-------------|
| DES-NET-001 | `pdu_encoder` | `include/pacs/network/pdu_encoder.hpp` | `src/network/pdu_encoder.cpp` |
| DES-NET-002 | `pdu_decoder` | `include/pacs/network/pdu_decoder.hpp` | `src/network/pdu_decoder.cpp` |
| DES-NET-003 | `dimse_message` | `include/pacs/network/dimse/dimse_message.hpp` | `src/network/dimse/dimse_message.cpp` |
| DES-NET-004 | `association` | `include/pacs/network/association.hpp` | `src/network/association.cpp` |
| DES-NET-005 | `dicom_server` | `include/pacs/network/dicom_server.hpp` | `src/network/dicom_server.cpp` |
| - | `pdu_types` | `include/pacs/network/pdu_types.hpp` | (header-only) |
| - | `server_config` | `include/pacs/network/server_config.hpp` | (header-only) |
| - | `command_field` | `include/pacs/network/dimse/command_field.hpp` | (header-only) |
| - | `status_codes` | `include/pacs/network/dimse/status_codes.hpp` | (header-only) |

### 5.4 Services Module Implementation

| SDS ID | Design Element | Header File | Source File |
|--------|---------------|-------------|-------------|
| DES-SVC-001 | `verification_scp` | `include/pacs/services/verification_scp.hpp` | `src/services/verification_scp.cpp` |
| DES-SVC-002 | `storage_scp` | `include/pacs/services/storage_scp.hpp` | `src/services/storage_scp.cpp` |
| DES-SVC-003 | `storage_scu` | `include/pacs/services/storage_scu.hpp` | `src/services/storage_scu.cpp` |
| DES-SVC-004 | `query_scp` | `include/pacs/services/query_scp.hpp` | `src/services/query_scp.cpp` |
| DES-SVC-005 | `retrieve_scp` | `include/pacs/services/retrieve_scp.hpp` | `src/services/retrieve_scp.cpp` |
| DES-SVC-006 | `worklist_scp` | `include/pacs/services/worklist_scp.hpp` | `src/services/worklist_scp.cpp` |
| DES-SVC-007 | `mpps_scp` | `include/pacs/services/mpps_scp.hpp` | `src/services/mpps_scp.cpp` |
| DES-SVC-008 | `sop_class_registry` | `include/pacs/services/sop_class_registry.hpp` | `src/services/sop_class_registry.cpp` |
| DES-SVC-009 | XA Storage SOP Classes | `include/pacs/services/sop_classes/xa_storage.hpp` | `src/services/sop_classes/xa_storage.cpp` |
| DES-SVC-010 | `xa_iod_validator` | `include/pacs/services/validation/xa_iod_validator.hpp` | `src/services/validation/xa_iod_validator.cpp` |
| - | US Storage SOP Classes | `include/pacs/services/sop_classes/us_storage.hpp` | `src/services/sop_classes/us_storage.cpp` |
| - | `scp_service` | `include/pacs/services/scp_service.hpp` | (header-only, interface) |
| - | `storage_status` | `include/pacs/services/storage_status.hpp` | (header-only) |

### 5.5 Storage Module Implementation

| SDS ID | Design Element | Header File | Source File |
|--------|---------------|-------------|-------------|
| DES-STOR-001 | `storage_interface` | `include/pacs/storage/storage_interface.hpp` | `src/storage/storage_interface.cpp` |
| DES-STOR-002 | `file_storage` | `include/pacs/storage/file_storage.hpp` | `src/storage/file_storage.cpp` |
| DES-STOR-003 | `index_database` | `include/pacs/storage/index_database.hpp` | `src/storage/index_database.cpp` |
| - | `migration_runner` | `include/pacs/storage/migration_runner.hpp` | `src/storage/migration_runner.cpp` |
| - | `patient_record` | `include/pacs/storage/patient_record.hpp` | (header-only, struct) |
| - | `study_record` | `include/pacs/storage/study_record.hpp` | (header-only, struct) |
| - | `series_record` | `include/pacs/storage/series_record.hpp` | (header-only, struct) |
| - | `instance_record` | `include/pacs/storage/instance_record.hpp` | (header-only, struct) |
| - | `worklist_record` | `include/pacs/storage/worklist_record.hpp` | (header-only, struct) |
| - | `mpps_record` | `include/pacs/storage/mpps_record.hpp` | (header-only, struct) |
| - | `migration_record` | `include/pacs/storage/migration_record.hpp` | (header-only, struct) |

### 5.6 Integration Module Implementation

| SDS ID | Design Element | Header File | Source File |
|--------|---------------|-------------|-------------|
| DES-INT-001 | `container_adapter` | `include/pacs/integration/container_adapter.hpp` | `src/integration/container_adapter.cpp` |
| DES-INT-002 | `network_adapter` | `include/pacs/integration/network_adapter.hpp` | `src/integration/network_adapter.cpp` |
| DES-INT-003 | `thread_adapter` | `include/pacs/integration/thread_adapter.hpp` | `src/integration/thread_adapter.cpp` |
| DES-INT-004 | `logger_adapter` | `include/pacs/integration/logger_adapter.hpp` | `src/integration/logger_adapter.cpp` |
| DES-INT-005 | `monitoring_adapter` | `include/pacs/integration/monitoring_adapter.hpp` | `src/integration/monitoring_adapter.cpp` |
| - | `dicom_session` | `include/pacs/integration/dicom_session.hpp` | `src/integration/dicom_session.cpp` |

### 5.7 Implementation Summary

| Module | Headers | Sources | Header-Only | Total Files |
|--------|---------|---------|-------------|-------------|
| Core | 7 | 7 | 1 | 14 |
| Encoding | 6 | 4 | 2 | 10 |
| Network | 9 | 5 | 4 | 14 |
| Services | 9 | 7 | 2 | 16 |
| Storage | 11 | 4 | 7 | 15 |
| Integration | 6 | 6 | 0 | 12 |
| **Total** | **48** | **33** | **16** | **81** |

---

## 6. SDS to Test Traceability (Verification)

This section maps design elements to their corresponding test files for **Verification** purposes.

> **Note on Verification vs Validation:**
> - **Verification** (This Section): Tests that confirm code matches SDS design specifications
>   - Unit Tests → Individual component design (DES-xxx)
>   - Integration Tests → Module interaction design (SEQ-xxx)
> - **Validation** (Separate): Tests that confirm implementation meets SRS requirements
>   - System Tests → SRS functional requirements
>   - Acceptance Tests → PRD user requirements

### 6.1 Core Module Tests

| SDS ID | Design Element | Test File | Test Count |
|--------|---------------|-----------|------------|
| DES-CORE-001 | `dicom_tag` | `tests/core/dicom_tag_test.cpp` | 12 |
| DES-CORE-002 | `dicom_element` | `tests/core/dicom_element_test.cpp` | 15 |
| DES-CORE-003 | `dicom_dataset` | `tests/core/dicom_dataset_test.cpp` | 18 |
| DES-CORE-004 | `dicom_file` | `tests/core/dicom_file_test.cpp` | 8 |
| DES-CORE-005 | `dicom_dictionary` | `tests/core/dicom_dictionary_test.cpp` | 6 |
| - | `tag_info` | `tests/core/tag_info_test.cpp` | 4 |

### 6.2 Encoding Module Tests

| SDS ID | Design Element | Test File | Test Count |
|--------|---------------|-----------|------------|
| DES-ENC-001 | `vr_type` | `tests/encoding/vr_type_test.cpp` | 10 |
| DES-ENC-002 | `vr_info` | `tests/encoding/vr_info_test.cpp` | 8 |
| DES-ENC-003 | `transfer_syntax` | `tests/encoding/transfer_syntax_test.cpp` | 7 |
| DES-ENC-004 | `implicit_vr_codec` | `tests/encoding/implicit_vr_codec_test.cpp` | 9 |
| DES-ENC-005 | `explicit_vr_codec` | `tests/encoding/explicit_vr_codec_test.cpp` | 9 |

### 6.3 Network Module Tests

| SDS ID | Design Element | Test File | Test Count |
|--------|---------------|-----------|------------|
| DES-NET-001 | `pdu_encoder` | `tests/network/pdu_encoder_test.cpp` | 7 |
| DES-NET-002 | `pdu_decoder` | `tests/network/pdu_decoder_test.cpp` | 7 |
| DES-NET-003 | `dimse_message` | `tests/network/dimse/dimse_message_test.cpp` | 5 |
| DES-NET-004 | `association` | `tests/network/association_test.cpp` | 8 |
| DES-NET-005 | `dicom_server` | `tests/network/dicom_server_test.cpp` | 4 |

### 6.4 Services Module Tests

| SDS ID | Design Element | Test File | Test Count |
|--------|---------------|-----------|------------|
| DES-SVC-001 | `verification_scp` | `tests/services/verification_scp_test.cpp` | - |
| DES-SVC-002 | `storage_scp` | `tests/services/storage_scp_test.cpp` | - |
| DES-SVC-003 | `storage_scu` | `tests/services/storage_scu_test.cpp` | - |
| DES-SVC-004 | `query_scp` | `tests/services/query_scp_test.cpp` | - |
| DES-SVC-005 | `retrieve_scp` | `tests/services/retrieve_scp_test.cpp` | - |
| DES-SVC-006 | `worklist_scp` | `tests/services/worklist_scp_test.cpp` | - |
| DES-SVC-007 | `mpps_scp` | `tests/services/mpps_scp_test.cpp` | - |
| DES-SVC-009 | XA Storage | `tests/services/xa_storage_test.cpp` | 15 |

### 6.5 Storage Module Tests

| SDS ID | Design Element | Test File | Test Count |
|--------|---------------|-----------|------------|
| DES-STOR-001 | `storage_interface` | `tests/storage/storage_interface_test.cpp` | - |
| DES-STOR-002 | `file_storage` | `tests/storage/file_storage_test.cpp` | - |
| DES-STOR-003 | `index_database` | `tests/storage/index_database_test.cpp` | - |
| - | `migration_runner` | `tests/storage/migration_runner_test.cpp` | - |
| DES-DB-005 | `mpps` records | `tests/storage/mpps_test.cpp` | - |
| DES-DB-006 | `worklist` records | `tests/storage/worklist_test.cpp` | - |

### 6.6 Integration Module Tests

| SDS ID | Design Element | Test File | Test Count |
|--------|---------------|-----------|------------|
| DES-INT-001 | `container_adapter` | `tests/integration/container_adapter_test.cpp` | - |
| DES-INT-002 | `network_adapter` | `tests/integration/network_adapter_test.cpp` | - |
| DES-INT-003 | `thread_adapter` | `tests/integration/thread_adapter_test.cpp` | - |
| DES-INT-004 | `logger_adapter` | `tests/integration/logger_adapter_test.cpp` | - |
| DES-INT-005 | `monitoring_adapter` | `tests/integration/monitoring_adapter_test.cpp` | - |

### 6.7 Verification Test Coverage Summary

| Module | Test Files | Design Elements Covered | Verification Coverage |
|--------|------------|------------------------|----------------------|
| Core | 6 | 6/6 | 100% |
| Encoding | 5 | 5/5 | 100% |
| Network | 5 | 5/5 | 100% |
| Services | 7 | 7/7 | 100% |
| Storage | 6 | 6/6 | 100% |
| Integration | 5 | 5/5 | 100% |
| **Total** | **34** | **34/34** | **100%** |

### 6.8 Validation Traceability (Separate Document)

**Validation** (SRS → System Test) is documented separately in [VALIDATION_REPORT.md](VALIDATION_REPORT.md).

| Document | Purpose | Traceability |
|----------|---------|--------------|
| **VERIFICATION_REPORT.md** | Confirms code matches SDS | SDS ↔ Unit/Integration Tests |
| **VALIDATION_REPORT.md** | Confirms implementation meets SRS | SRS ↔ System/Acceptance Tests |

> **Note:** This document (SDS_TRACEABILITY.md) focuses on **Verification** traceability only.

---

## 7. Coverage Analysis

### 7.1 Requirements Coverage Summary

| Category | Total PRD | Traced to SRS | Traced to SDS | Coverage |
|----------|-----------|---------------|---------------|----------|
| Functional (FR) | 14 | 14 | 14 | 100% |
| Non-Functional (NFR) | 4 | 4 | 4 | 100% |
| Integration (IR) | 5 | 5 | 5 | 100% |
| **Total** | **23** | **23** | **23** | **100%** |

### 7.2 SRS to SDS Coverage

| Module | SRS Count | SDS Count | Sequences | DB Tables | Coverage |
|--------|-----------|-----------|-----------|-----------|----------|
| Core | 8 | 8 | - | - | 100% |
| Network | 4 | 5 | 3 | - | 100% |
| Services | 7 | 7 | 8 | 2 | 100% |
| Storage | 3 | 3 | - | 4 | 100% |
| Integration | 5 | 5 | - | - | 100% |
| Performance | 4 | N/A | - | - | Addressed |
| Reliability | 3 | N/A | 3 | - | Addressed |
| Security | 3 | N/A | - | - | Addressed |
| Maintainability | 3 | N/A | - | - | Addressed |

### 7.3 Orphan Analysis

**Orphan Requirements (no design):** None

**Orphan Designs (no requirement):** None

### 7.4 Traceability Gaps

| Gap ID | Description | Status | Resolution |
|--------|-------------|--------|------------|
| - | None identified | - | - |

### 7.5 Impact Analysis Template

When a requirement changes, use this checklist:

```
┌─────────────────────────────────────────────────────────────────┐
│             Requirement Change Impact Analysis                   │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Changed Requirement: ________________________________________   │
│                                                                  │
│  1. Identify affected SRS items:                                │
│     □ SRS-CORE-xxx                                              │
│     □ SRS-NET-xxx                                               │
│     □ SRS-SVC-xxx                                               │
│     □ SRS-STOR-xxx                                              │
│     □ SRS-INT-xxx                                               │
│                                                                  │
│  2. Identify affected SDS elements:                             │
│     □ DES-xxx (Component designs)                               │
│     □ SEQ-xxx (Sequence diagrams)                               │
│     □ DES-DB-xxx (Database tables)                              │
│                                                                  │
│  3. Identify affected code:                                     │
│     □ include/pacs/xxx                                          │
│     □ src/xxx                                                   │
│                                                                  │
│  4. Identify affected tests:                                    │
│     □ tests/xxx                                                 │
│                                                                  │
│  5. Estimate effort:                                            │
│     □ Design change:    ___ hours                               │
│     □ Implementation:   ___ hours                               │
│     □ Testing:          ___ hours                               │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## Appendix A: Requirement ID Quick Reference

### PRD Requirements

| ID | Description |
|----|-------------|
| FR-1.1 | DICOM Data Element |
| FR-1.2 | VR Types |
| FR-1.3 | Part 10 File Format |
| FR-1.4 | Transfer Syntax |
| FR-1.5 | Data Dictionary |
| FR-2.1 | PDU Types |
| FR-2.2 | Association State Machine |
| FR-2.3 | DIMSE Messages |
| FR-3.1 | Storage Service |
| FR-3.2 | Query/Retrieve Service |
| FR-3.3 | Modality Worklist |
| FR-3.4 | MPPS Service |
| FR-4.1 | File Storage |
| FR-4.2 | Index Database |
| NFR-1 | Performance |
| NFR-2 | Reliability |
| NFR-3 | Security |
| NFR-4 | Maintainability |
| IR-1 | container_system |
| IR-2 | network_system |
| IR-3 | thread_system |
| IR-4 | logger_system |
| IR-5 | monitoring_system |

### SDS Design Elements

| ID | Element |
|----|---------|
| DES-CORE-001 | dicom_tag |
| DES-CORE-002 | dicom_element |
| DES-CORE-003 | dicom_dataset |
| DES-CORE-004 | dicom_file |
| DES-CORE-005 | dicom_dictionary |
| DES-ENC-001 | vr_type |
| DES-ENC-002 | vr_info |
| DES-ENC-003 | transfer_syntax |
| DES-ENC-004 | implicit_vr_codec |
| DES-ENC-005 | explicit_vr_codec |
| DES-NET-001 | pdu_encoder |
| DES-NET-002 | pdu_decoder |
| DES-NET-003 | dimse_message |
| DES-NET-004 | association |
| DES-NET-005 | dicom_server |
| DES-SVC-001 | verification_scp |
| DES-SVC-002 | storage_scp |
| DES-SVC-003 | storage_scu |
| DES-SVC-004 | query_scp |
| DES-SVC-005 | retrieve_scp |
| DES-SVC-006 | worklist_scp |
| DES-SVC-007 | mpps_scp |
| DES-SVC-008 | sop_class_registry |
| DES-SVC-009 | xa_storage (XA/XRF SOP Classes) |
| DES-SVC-010 | xa_iod_validator |
| DES-STOR-001 | storage_interface |
| DES-STOR-002 | file_storage |
| DES-STOR-003 | index_database |
| DES-INT-001 | container_adapter |
| DES-INT-002 | network_adapter |
| DES-INT-003 | thread_adapter |
| DES-INT-004 | logger_adapter |
| DES-INT-005 | monitoring_adapter |
| DES-DB-001 | patients table |
| DES-DB-002 | studies table |
| DES-DB-003 | series table |
| DES-DB-004 | instances table |
| DES-DB-005 | mpps table |
| DES-DB-006 | worklist table |
| DES-DB-007 | schema_version table |

---

## Appendix B: Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0.0 | 2025-11-30 | kcenon@naver.com | Initial traceability matrix |
| 1.1.0 | 2025-12-01 | kcenon@naver.com | Added SDS to Implementation and Test traceability (Sections 5, 6) |
| 1.2.0 | 2025-12-01 | kcenon@naver.com | Corrected V-Model diagram with proper Verification/Validation distinction |
| 1.3.0 | 2025-12-01 | kcenon@naver.com | Scoped to Verification only; Validation moved to separate VALIDATION_REPORT.md |
| 1.4.0 | 2025-12-01 | kcenon@naver.com | Added DES-SVC-008 (sop_class_registry), DES-SVC-009 (XA Storage), DES-SVC-010 (xa_iod_validator) |

---

*Document Version: 1.0.0*
*Last Updated: 2025-12-01*
*Author: kcenon@naver.com*
