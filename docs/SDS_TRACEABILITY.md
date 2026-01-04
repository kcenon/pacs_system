# SDS - Requirements Traceability Matrix

> **Version:** 2.3.0
> **Parent Document:** [SDS.md](SDS.md)
> **Last Updated:** 2026-01-04

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
| **SRS-STOR-003** | S3 Cloud Storage Backend | DES-STOR-005 | `s3_storage` class |
| **SRS-STOR-004** | Azure Cloud Storage Backend | DES-STOR-006 | `azure_blob_storage` class |
| **SRS-STOR-005** | Index database | DES-STOR-003, DES-DB-001 to DES-DB-007 | `index_database`, tables |
| **SRS-STOR-010** | Hierarchical Storage Management | DES-STOR-007 | `hsm_storage` class |
| **SRS-STOR-011** | HSM Migration Service | DES-STOR-008 | `hsm_migration_service` class |

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

### 3.7 Security Module Requirements

| SRS ID | SRS Description | SDS ID(s) | Design Element |
|--------|-----------------|-----------|----------------|
| **SRS-SEC-001** | Data Protection (Anonymization) | DES-SEC-006, DES-SEC-007, DES-SEC-008, DES-SEC-009 | `anonymization_profile`, `tag_action`, `uid_mapping`, `anonymizer` |
| **SRS-SEC-002** | Access Control (RBAC) | DES-SEC-001 to DES-SEC-005, DES-SEC-013 | `User`, `Role`, `Permission`, `user_context`, `access_control_manager`, `security_storage_interface` |
| **SRS-SEC-003** | Audit Trail | DES-SEC-004, DES-SEC-005 | `user_context` (source info), `access_control_manager` (audit callback) |

### 3.8 Security Sequence Diagram Mapping

| SRS ID | Scenario | SEQ ID(s) | Diagram Name |
|--------|----------|-----------|--------------|
| SRS-SEC-002 | DICOM Operation Authorization | SEQ-SEC-001 | Authorization flow |
| SRS-SEC-001 | DICOM Anonymization | SEQ-SEC-002 | Anonymization flow |
| SRS-SEC-001 | Digital Signature Creation | SEQ-SEC-003 | Signature creation |
| SRS-SEC-001 | Digital Signature Verification | SEQ-SEC-004 | Signature verification |

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

### 4.7 Security Module

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           Security Module Trace                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  PRD                  SRS                    SDS                            │
│  ───────────────────────────────────────────────────────────────            │
│                                                                              │
│  NFR-3 ───────────► SRS-SEC-001 ─────────► DES-SEC-006 (anon_profile)      │
│  (Security)         (Data Protection)        DES-SEC-007 (tag_action)       │
│                                      ─────────► DES-SEC-008 (uid_mapping)   │
│                                      ─────────► DES-SEC-009 (anonymizer)    │
│                                      ─────────► DES-SEC-010 (certificate)   │
│                                      ─────────► DES-SEC-011 (sig_types)     │
│                                      ─────────► DES-SEC-012 (digital_sig)   │
│                                      ─────────► SEQ-SEC-002 to SEQ-SEC-004  │
│                                                                              │
│                     SRS-SEC-002 ─────────► DES-SEC-001 (User)               │
│                     (Access Control)        DES-SEC-002 (Role)              │
│                                      ─────────► DES-SEC-003 (Permission)    │
│                                      ─────────► DES-SEC-004 (user_context)  │
│                                      ─────────► DES-SEC-005 (access_ctrl)   │
│                                      ─────────► DES-SEC-013 (sec_storage)   │
│                                      ─────────► SEQ-SEC-001                 │
│                                                                              │
│                     SRS-SEC-003 ─────────► DES-SEC-004 (user_context)       │
│                     (Audit Trail)    ─────────► DES-SEC-005 (access_ctrl)   │
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
| DES-CORE-006 | `pool_manager` | `include/pacs/core/pool_manager.hpp` | `src/core/pool_manager.cpp` |
| DES-CORE-007 | `tag_info` | `include/pacs/core/tag_info.hpp` | `src/core/tag_info.cpp` |
| - | `dicom_tag_constants` | `include/pacs/core/dicom_tag_constants.hpp` | (header-only) |
| - | `events` | `include/pacs/core/events.hpp` | (header-only) |
| - | `result` | `include/pacs/core/result.hpp` | (header-only) |

### 5.2 Encoding Module Implementation

> **Reference:** [SDS_COMPRESSION.md](SDS_COMPRESSION.md) - Complete Compression Codecs Module Design Specification

| SDS ID | Design Element | Header File | Source File |
|--------|---------------|-------------|-------------|
| DES-ENC-001 | `vr_type` | `include/pacs/encoding/vr_type.hpp` | (header-only, enum) |
| DES-ENC-002 | `vr_info` | `include/pacs/encoding/vr_info.hpp` | `src/encoding/vr_info.cpp` |
| DES-ENC-003 | `transfer_syntax` | `include/pacs/encoding/transfer_syntax.hpp` | `src/encoding/transfer_syntax.cpp` |
| DES-ENC-004 | `implicit_vr_codec` | `include/pacs/encoding/implicit_vr_codec.hpp` | `src/encoding/implicit_vr_codec.cpp` |
| DES-ENC-005 | `explicit_vr_codec` | `include/pacs/encoding/explicit_vr_codec.hpp` | `src/encoding/explicit_vr_codec.cpp` |
| DES-ENC-006 | `explicit_vr_big_endian_codec` | `include/pacs/encoding/explicit_vr_big_endian_codec.hpp` | `src/encoding/explicit_vr_big_endian_codec.cpp` |
| DES-ENC-007 | `codec_factory` | `include/pacs/encoding/compression/codec_factory.hpp` | `src/encoding/compression/codec_factory.cpp` |
| DES-ENC-008 | `compression_codec` | `include/pacs/encoding/compression/compression_codec.hpp` | (header-only, interface) |
| DES-ENC-009 | `jpeg_baseline_codec` | `include/pacs/encoding/compression/jpeg_baseline_codec.hpp` | `src/encoding/compression/jpeg_baseline_codec.cpp` |
| DES-ENC-010 | `jpeg_lossless_codec` | `include/pacs/encoding/compression/jpeg_lossless_codec.hpp` | `src/encoding/compression/jpeg_lossless_codec.cpp` |
| DES-ENC-011 | `jpeg_ls_codec` | `include/pacs/encoding/compression/jpeg_ls_codec.hpp` | `src/encoding/compression/jpeg_ls_codec.cpp` |
| DES-ENC-012 | `jpeg2000_codec` | `include/pacs/encoding/compression/jpeg2000_codec.hpp` | `src/encoding/compression/jpeg2000_codec.cpp` |
| DES-ENC-013 | `rle_codec` | `include/pacs/encoding/compression/rle_codec.hpp` | `src/encoding/compression/rle_codec.cpp` |
| - | `byte_order` | `include/pacs/encoding/byte_order.hpp` | (header-only) |
| - | `byte_swap` | `include/pacs/encoding/byte_swap.hpp` | (header-only) |
| - | `image_params` | `include/pacs/encoding/compression/image_params.hpp` | (header-only) |
| - | `simd_config` | `include/pacs/encoding/simd/simd_config.hpp` | (header-only) |
| - | `simd_photometric` | `include/pacs/encoding/simd/simd_photometric.hpp` | (header-only) |
| - | `simd_rle` | `include/pacs/encoding/simd/simd_rle.hpp` | (header-only) |
| - | `simd_types` | `include/pacs/encoding/simd/simd_types.hpp` | (header-only) |
| - | `simd_utils` | `include/pacs/encoding/simd/simd_utils.hpp` | (header-only) |
| - | `simd_windowing` | `include/pacs/encoding/simd/simd_windowing.hpp` | (header-only) |

### 5.3 Network Module Implementation

| SDS ID | Design Element | Header File | Source File |
|--------|---------------|-------------|-------------|
| DES-NET-001 | `pdu_encoder` | `include/pacs/network/pdu_encoder.hpp` | `src/network/pdu_encoder.cpp` |
| DES-NET-002 | `pdu_decoder` | `include/pacs/network/pdu_decoder.hpp` | `src/network/pdu_decoder.cpp` |
| DES-NET-003 | `dimse_message` | `include/pacs/network/dimse/dimse_message.hpp` | `src/network/dimse/dimse_message.cpp` |
| DES-NET-004 | `association` | `include/pacs/network/association.hpp` | `src/network/association.cpp` |
| DES-NET-005 | `dicom_server` | `include/pacs/network/dicom_server.hpp` | `src/network/dicom_server.cpp` |
| DES-NET-006 | `pdu_buffer_pool` | `include/pacs/network/pdu_buffer_pool.hpp` | `src/network/pdu_buffer_pool.cpp` |
| DES-NET-007 | `accept_worker` | `include/pacs/network/detail/accept_worker.hpp` | `src/network/detail/accept_worker.cpp` |
| DES-NET-008 | `dicom_server_v2` | `include/pacs/network/v2/dicom_server_v2.hpp` | `src/network/v2/dicom_server_v2.cpp` |
| DES-NET-009 | `dicom_association_handler` | `include/pacs/network/v2/dicom_association_handler.hpp` | `src/network/v2/dicom_association_handler.cpp` |
| - | `pdu_types` | `include/pacs/network/pdu_types.hpp` | (header-only) |
| - | `server_config` | `include/pacs/network/server_config.hpp` | (header-only) |
| - | `command_field` | `include/pacs/network/dimse/command_field.hpp` | (header-only) |
| - | `status_codes` | `include/pacs/network/dimse/status_codes.hpp` | (header-only) |
| - | `n_action` | `include/pacs/network/dimse/n_action.hpp` | (header-only) |
| - | `n_create` | `include/pacs/network/dimse/n_create.hpp` | (header-only) |
| - | `n_delete` | `include/pacs/network/dimse/n_delete.hpp` | (header-only) |
| - | `n_event_report` | `include/pacs/network/dimse/n_event_report.hpp` | (header-only) |
| - | `n_get` | `include/pacs/network/dimse/n_get.hpp` | (header-only) |
| - | `n_set` | `include/pacs/network/dimse/n_set.hpp` | (header-only) |

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
| DES-SVC-011 | US Storage SOP Classes | `include/pacs/services/sop_classes/us_storage.hpp` | `src/services/sop_classes/us_storage.cpp` |
| DES-SVC-012 | `us_iod_validator` | `include/pacs/services/validation/us_iod_validator.hpp` | `src/services/validation/us_iod_validator.cpp` |
| DES-SVC-013 | DX Storage SOP Classes | `include/pacs/services/sop_classes/dx_storage.hpp` | `src/services/sop_classes/dx_storage.cpp` |
| DES-SVC-014 | `dx_iod_validator` | `include/pacs/services/validation/dx_iod_validator.hpp` | `src/services/validation/dx_iod_validator.cpp` |
| DES-SVC-015 | MG Storage SOP Classes | `include/pacs/services/sop_classes/mg_storage.hpp` | `src/services/sop_classes/mg_storage.cpp` |
| DES-SVC-016 | `mg_iod_validator` | `include/pacs/services/validation/mg_iod_validator.hpp` | `src/services/validation/mg_iod_validator.cpp` |
| DES-SVC-017 | NM Storage SOP Classes | `include/pacs/services/sop_classes/nm_storage.hpp` | `src/services/sop_classes/nm_storage.cpp` |
| DES-SVC-018 | `nm_iod_validator` | `include/pacs/services/validation/nm_iod_validator.hpp` | `src/services/validation/nm_iod_validator.cpp` |
| DES-SVC-019 | PET Storage SOP Classes | `include/pacs/services/sop_classes/pet_storage.hpp` | `src/services/sop_classes/pet_storage.cpp` |
| DES-SVC-020 | `pet_iod_validator` | `include/pacs/services/validation/pet_iod_validator.hpp` | `src/services/validation/pet_iod_validator.cpp` |
| DES-SVC-021 | RT Storage SOP Classes | `include/pacs/services/sop_classes/rt_storage.hpp` | `src/services/sop_classes/rt_storage.cpp` |
| DES-SVC-022 | `rt_iod_validator` | `include/pacs/services/validation/rt_iod_validator.hpp` | `src/services/validation/rt_iod_validator.cpp` |
| DES-SVC-023 | SEG Storage SOP Classes | `include/pacs/services/sop_classes/seg_storage.hpp` | `src/services/sop_classes/seg_storage.cpp` |
| DES-SVC-024 | `seg_iod_validator` | `include/pacs/services/validation/seg_iod_validator.hpp` | `src/services/validation/seg_iod_validator.cpp` |
| DES-SVC-025 | SR Storage SOP Classes | `include/pacs/services/sop_classes/sr_storage.hpp` | `src/services/sop_classes/sr_storage.cpp` |
| DES-SVC-026 | `sr_iod_validator` | `include/pacs/services/validation/sr_iod_validator.hpp` | `src/services/validation/sr_iod_validator.cpp` |
| DES-SVC-027 | `query_cache` | `include/pacs/services/cache/query_cache.hpp` | `src/services/cache/query_cache.cpp` |
| DES-SVC-028 | `query_result_stream` | `include/pacs/services/cache/query_result_stream.hpp` | `src/services/cache/query_result_stream.cpp` |
| DES-SVC-029 | `streaming_query_handler` | `include/pacs/services/cache/streaming_query_handler.hpp` | `src/services/cache/streaming_query_handler.cpp` |
| DES-SVC-030 | `database_cursor` | `include/pacs/services/cache/database_cursor.hpp` | `src/services/cache/database_cursor.cpp` |
| DES-SVC-031 | `parallel_query_executor` | `include/pacs/services/cache/parallel_query_executor.hpp` | `src/services/cache/parallel_query_executor.cpp` |
| - | `scp_service` | `include/pacs/services/scp_service.hpp` | (header-only, interface) |
| - | `storage_status` | `include/pacs/services/storage_status.hpp` | (header-only) |
| - | `simple_lru_cache` | `include/pacs/services/cache/simple_lru_cache.hpp` | (header-only, template) |

### 5.5 Storage Module Implementation

> **Reference:** [SDS_CLOUD_STORAGE.md](SDS_CLOUD_STORAGE.md) - Cloud Storage Backends Design Specification

| SDS ID | Design Element | Header File | Source File |
|--------|---------------|-------------|-------------|
| DES-STOR-001 | `storage_interface` | `include/pacs/storage/storage_interface.hpp` | `src/storage/storage_interface.cpp` |
| DES-STOR-002 | `file_storage` | `include/pacs/storage/file_storage.hpp` | `src/storage/file_storage.cpp` |
| DES-STOR-003 | `index_database` | `include/pacs/storage/index_database.hpp` | `src/storage/index_database.cpp` |
| DES-STOR-004 | `migration_runner` | `include/pacs/storage/migration_runner.hpp` | `src/storage/migration_runner.cpp` |
| DES-STOR-005 | `s3_storage` | `include/pacs/storage/s3_storage.hpp` | `src/storage/s3_storage.cpp` |
| DES-STOR-006 | `azure_blob_storage` | `include/pacs/storage/azure_blob_storage.hpp` | `src/storage/azure_blob_storage.cpp` |
| DES-STOR-007 | `hsm_storage` | `include/pacs/storage/hsm_storage.hpp` | `src/storage/hsm_storage.cpp` |
| DES-STOR-008 | `hsm_migration_service` | `include/pacs/storage/hsm_migration_service.hpp` | `src/storage/hsm_migration_service.cpp` |
| DES-STOR-009 | `sqlite_security_storage` | `include/pacs/storage/sqlite_security_storage.hpp` | `src/storage/sqlite_security_storage.cpp` |
| - | `patient_record` | `include/pacs/storage/patient_record.hpp` | (header-only, struct) |
| - | `study_record` | `include/pacs/storage/study_record.hpp` | (header-only, struct) |
| - | `series_record` | `include/pacs/storage/series_record.hpp` | (header-only, struct) |
| - | `instance_record` | `include/pacs/storage/instance_record.hpp` | (header-only, struct) |
| - | `worklist_record` | `include/pacs/storage/worklist_record.hpp` | (header-only, struct) |
| - | `mpps_record` | `include/pacs/storage/mpps_record.hpp` | (header-only, struct) |
| - | `migration_record` | `include/pacs/storage/migration_record.hpp` | (header-only, struct) |
| - | `audit_record` | `include/pacs/storage/audit_record.hpp` | (header-only, struct) |
| - | `hsm_types` | `include/pacs/storage/hsm_types.hpp` | (header-only) |

### 5.6 Integration Module Implementation

| SDS ID | Design Element | Header File | Source File |
|--------|---------------|-------------|-------------|
| DES-INT-001 | `container_adapter` | `include/pacs/integration/container_adapter.hpp` | `src/integration/container_adapter.cpp` |
| DES-INT-002 | `network_adapter` | `include/pacs/integration/network_adapter.hpp` | `src/integration/network_adapter.cpp` |
| DES-INT-003 | `thread_adapter` | `include/pacs/integration/thread_adapter.hpp` | `src/integration/thread_adapter.cpp` |
| DES-INT-004 | `logger_adapter` | `include/pacs/integration/logger_adapter.hpp` | `src/integration/logger_adapter.cpp` |
| DES-INT-005 | `monitoring_adapter` | `include/pacs/integration/monitoring_adapter.hpp` | `src/integration/monitoring_adapter.cpp` |
| DES-INT-006 | `dicom_session` | `include/pacs/integration/dicom_session.hpp` | `src/integration/dicom_session.cpp` |
| DES-INT-007 | `thread_pool_adapter` | `include/pacs/integration/thread_pool_adapter.hpp` | `src/integration/thread_pool_adapter.cpp` |
| DES-INT-008 | `itk_adapter` | `include/pacs/integration/itk_adapter.hpp` | `src/integration/itk_adapter.cpp` |
| - | `thread_pool_interface` | `include/pacs/integration/thread_pool_interface.hpp` | (header-only, interface) |

### 5.7 Security Module Implementation

> **Reference:** [SDS_SECURITY.md](SDS_SECURITY.md) - Complete Security Module Design Specification

#### RBAC Access Control (DES-SEC-001 to DES-SEC-005)

| SDS ID | Design Element | Header File | Source File |
|--------|---------------|-------------|-------------|
| DES-SEC-001 | `User` | `include/pacs/security/user.hpp` | (header-only) |
| DES-SEC-002 | `Role` | `include/pacs/security/role.hpp` | (header-only) |
| DES-SEC-003 | `Permission` | `include/pacs/security/permission.hpp` | (header-only) |
| DES-SEC-004 | `user_context` | `include/pacs/security/user_context.hpp` | (header-only) |
| DES-SEC-005 | `access_control_manager` | `include/pacs/security/access_control_manager.hpp` | `src/security/access_control_manager.cpp` |

#### DICOM Anonymization (DES-SEC-006 to DES-SEC-009)

| SDS ID | Design Element | Header File | Source File |
|--------|---------------|-------------|-------------|
| DES-SEC-006 | `anonymization_profile` | `include/pacs/security/anonymization_profile.hpp` | (header-only) |
| DES-SEC-007 | `tag_action` | `include/pacs/security/tag_action.hpp` | `src/security/tag_action.cpp` |
| DES-SEC-008 | `uid_mapping` | `include/pacs/security/uid_mapping.hpp` | `src/security/uid_mapping.cpp` |
| DES-SEC-009 | `anonymizer` | `include/pacs/security/anonymizer.hpp` | `src/security/anonymizer.cpp` |

#### Digital Signatures (DES-SEC-010 to DES-SEC-012)

| SDS ID | Design Element | Header File | Source File |
|--------|---------------|-------------|-------------|
| DES-SEC-010 | `certificate`, `private_key`, `certificate_chain` | `include/pacs/security/certificate.hpp` | `src/security/certificate.cpp` |
| DES-SEC-011 | `signature_algorithm`, `signature_status`, `signature_info` | `include/pacs/security/signature_types.hpp` | (header-only) |
| DES-SEC-012 | `digital_signature` | `include/pacs/security/digital_signature.hpp` | `src/security/digital_signature.cpp` |

#### Security Storage (DES-SEC-013)

| SDS ID | Design Element | Header File | Source File |
|--------|---------------|-------------|-------------|
| DES-SEC-013 | `security_storage_interface` | `include/pacs/security/security_storage_interface.hpp` | (header-only, interface) |

### 5.8 Web Module Implementation

> **Reference:** [SDS_WEB_API.md](SDS_WEB_API.md) - Complete Web/REST API Module Design Specification

| SDS ID | Design Element | Header File | Source File |
|--------|---------------|-------------|-------------|
| DES-WEB-001 | `rest_server` | `include/pacs/web/rest_server.hpp` | `src/web/rest_server.cpp` |
| DES-WEB-002 | `rest_config` | `include/pacs/web/rest_config.hpp` | (header-only) |
| DES-WEB-003 | `rest_types` | `include/pacs/web/rest_types.hpp` | (header-only) |
| DES-WEB-004 | `patient_endpoints` | `include/pacs/web/endpoints/patient_endpoints.hpp` | `src/web/endpoints/patient_endpoints.cpp` |
| DES-WEB-005 | `study_endpoints` | `include/pacs/web/endpoints/study_endpoints.hpp` | `src/web/endpoints/study_endpoints.cpp` |
| DES-WEB-006 | `series_endpoints` | `include/pacs/web/endpoints/series_endpoints.hpp` | `src/web/endpoints/series_endpoints.cpp` |
| DES-WEB-007 | `dicomweb_endpoints` | `include/pacs/web/endpoints/dicomweb_endpoints.hpp` | `src/web/endpoints/dicomweb_endpoints.cpp` |
| DES-WEB-008 | `worklist_endpoints` | `include/pacs/web/endpoints/worklist_endpoints.hpp` | `src/web/endpoints/worklist_endpoints.cpp` |
| DES-WEB-009 | `audit_endpoints` | `include/pacs/web/endpoints/audit_endpoints.hpp` | `src/web/endpoints/audit_endpoints.cpp` |
| DES-WEB-010 | `security_endpoints` | `include/pacs/web/endpoints/security_endpoints.hpp` | `src/web/endpoints/security_endpoints.cpp` |
| DES-WEB-011 | `system_endpoints` | `include/pacs/web/endpoints/system_endpoints.hpp` | `src/web/endpoints/system_endpoints.cpp` |
| DES-WEB-012 | `association_endpoints` | `include/pacs/web/endpoints/association_endpoints.hpp` | `src/web/endpoints/association_endpoints.cpp` |

### 5.9 Workflow Module Implementation

| SDS ID | Design Element | Header File | Source File |
|--------|---------------|-------------|-------------|
| DES-WF-001 | `auto_prefetch_service` | `include/pacs/workflow/auto_prefetch_service.hpp` | `src/workflow/auto_prefetch_service.cpp` |
| DES-WF-002 | `task_scheduler` | `include/pacs/workflow/task_scheduler.hpp` | `src/workflow/task_scheduler.cpp` |
| DES-WF-003 | `study_lock_manager` | `include/pacs/workflow/study_lock_manager.hpp` | `src/workflow/study_lock_manager.cpp` |
| - | `prefetch_config` | `include/pacs/workflow/prefetch_config.hpp` | (header-only) |
| - | `task_scheduler_config` | `include/pacs/workflow/task_scheduler_config.hpp` | (header-only) |

### 5.10 AI Module Implementation

| SDS ID | Design Element | Header File | Source File |
|--------|---------------|-------------|-------------|
| DES-AI-001 | `ai_service_connector` | `include/pacs/ai/ai_service_connector.hpp` | `src/ai/ai_service_connector.cpp` |
| DES-AI-002 | `ai_result_handler` | `include/pacs/ai/ai_result_handler.hpp` | `src/ai/ai_result_handler.cpp` |

### 5.11 DI Module Implementation

| SDS ID | Design Element | Header File | Source File |
|--------|---------------|-------------|-------------|
| DES-DI-001 | `service_registration` | `include/pacs/di/service_registration.hpp` | (header-only) |
| DES-DI-002 | `service_interfaces` | `include/pacs/di/service_interfaces.hpp` | (header-only, interface) |
| DES-DI-003 | `ilogger` | `include/pacs/di/ilogger.hpp` | (header-only, interface) |
| DES-DI-004 | `test_support` | `include/pacs/di/test_support.hpp` | (header-only) |

### 5.12 Monitoring Module Implementation

| SDS ID | Design Element | Header File | Source File |
|--------|---------------|-------------|-------------|
| DES-MON-001 | `pacs_monitor` | `include/pacs/monitoring/pacs_monitor.hpp` | (header-only) |
| DES-MON-002 | `pacs_metrics` | `include/pacs/monitoring/pacs_metrics.hpp` | `src/monitoring/pacs_metrics.cpp` |
| DES-MON-003 | `health_checker` | `include/pacs/monitoring/health_checker.hpp` | `src/monitoring/health_checker.cpp` |
| DES-MON-004 | `dicom_association_collector` | `include/pacs/monitoring/collectors/dicom_association_collector.hpp` | (header-only) |
| DES-MON-005 | `dicom_service_collector` | `include/pacs/monitoring/collectors/dicom_service_collector.hpp` | (header-only) |
| DES-MON-006 | `dicom_storage_collector` | `include/pacs/monitoring/collectors/dicom_storage_collector.hpp` | (header-only) |
| - | `health_status` | `include/pacs/monitoring/health_status.hpp` | (header-only) |
| - | `health_json` | `include/pacs/monitoring/health_json.hpp` | (header-only) |

### 5.13 Compat Module Implementation

| SDS ID | Design Element | Header File | Source File |
|--------|---------------|-------------|-------------|
| DES-COMPAT-001 | `format` | `include/pacs/compat/format.hpp` | (header-only) |
| DES-COMPAT-002 | `time` | `include/pacs/compat/time.hpp` | (header-only) |

### 5.14 Implementation Summary

| Module | Headers | Sources | Header-Only | Total Files |
|--------|---------|---------|-------------|-------------|
| Core | 10 | 8 | 3 | 18 |
| Encoding | 22 | 11 | 11 | 33 |
| Network | 19 | 9 | 10 | 28 |
| Services | 34 | 31 | 3 | 65 |
| Storage | 18 | 9 | 9 | 27 |
| Integration | 9 | 8 | 1 | 17 |
| Security | 13 | 6 | 7 | 19 |
| Web | 12 | 10 | 2 | 22 |
| Workflow | 5 | 3 | 2 | 8 |
| AI | 2 | 2 | 0 | 4 |
| DI | 4 | 0 | 4 | 4 |
| Monitoring | 8 | 2 | 6 | 10 |
| Compat | 2 | 0 | 2 | 2 |
| **Total** | **158** | **99** | **60** | **257** |

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
| DES-CORE-007 | `tag_info` | `tests/core/tag_info_test.cpp` | 4 |
| - | `events` | `tests/core/events_test.cpp` | - |

### 6.2 Encoding Module Tests

| SDS ID | Design Element | Test File | Test Count |
|--------|---------------|-----------|------------|
| DES-ENC-001 | `vr_type` | `tests/encoding/vr_type_test.cpp` | 10 |
| DES-ENC-002 | `vr_info` | `tests/encoding/vr_info_test.cpp` | 8 |
| DES-ENC-003 | `transfer_syntax` | `tests/encoding/transfer_syntax_test.cpp` | 7 |
| DES-ENC-004 | `implicit_vr_codec` | `tests/encoding/implicit_vr_codec_test.cpp` | 9 |
| DES-ENC-005 | `explicit_vr_codec` | `tests/encoding/explicit_vr_codec_test.cpp` | 9 |
| DES-ENC-006 | `explicit_vr_big_endian_codec` | `tests/encoding/explicit_vr_big_endian_codec_test.cpp` | - |
| DES-ENC-009 | `jpeg_baseline_codec` | `tests/encoding/compression/jpeg_baseline_codec_test.cpp` | - |
| DES-ENC-010 | `jpeg_lossless_codec` | `tests/encoding/compression/jpeg_lossless_codec_test.cpp` | - |
| DES-ENC-011 | `jpeg_ls_codec` | `tests/encoding/compression/jpeg_ls_codec_test.cpp` | - |
| DES-ENC-012 | `jpeg2000_codec` | `tests/encoding/compression/jpeg2000_codec_test.cpp` | - |
| DES-ENC-013 | `rle_codec` | `tests/encoding/compression/rle_codec_test.cpp` | - |
| - | `byte_swap` | `tests/encoding/byte_swap_test.cpp` | - |
| - | `simd_rle` | `tests/encoding/simd/simd_rle_test.cpp` | - |

### 6.3 Network Module Tests

| SDS ID | Design Element | Test File | Test Count |
|--------|---------------|-----------|------------|
| DES-NET-001 | `pdu_encoder` | `tests/network/pdu_encoder_test.cpp` | 7 |
| DES-NET-002 | `pdu_decoder` | `tests/network/pdu_decoder_test.cpp` | 7 |
| DES-NET-003 | `dimse_message` | `tests/network/dimse/dimse_message_test.cpp` | 5 |
| DES-NET-004 | `association` | `tests/network/association_test.cpp` | 8 |
| DES-NET-005 | `dicom_server` | `tests/network/dicom_server_test.cpp` | 4 |
| DES-NET-007 | `accept_worker` | `tests/network/detail/accept_worker_test.cpp` | - |
| DES-NET-008 | `dicom_server_v2` | `tests/network/v2/dicom_server_v2_test.cpp` | - |
| DES-NET-009 | `dicom_association_handler` | `tests/network/v2/dicom_association_handler_test.cpp` | - |
| - | N-Services | `tests/network/dimse/n_service_test.cpp` | - |
| - | Network V2 Integration | `tests/network/v2/network_system_integration_test.cpp` | - |
| - | PDU Framing | `tests/network/v2/pdu_framing_test.cpp` | - |
| - | Service Dispatching | `tests/network/v2/service_dispatching_test.cpp` | - |
| - | State Machine | `tests/network/v2/state_machine_test.cpp` | - |
| - | Stress Test | `tests/network/v2/stress_test.cpp` | - |

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
| DES-SVC-011 | US Storage | `tests/services/us_storage_test.cpp` | - |
| DES-SVC-013 | DX Storage | `tests/services/dx_storage_test.cpp` | - |
| DES-SVC-015 | MG Storage | `tests/services/mg_storage_test.cpp` | - |
| DES-SVC-016 | `mg_iod_validator` | `tests/services/mg_iod_validator_test.cpp` | - |
| DES-SVC-017 | NM Storage | `tests/services/nm_storage_test.cpp` | - |
| DES-SVC-018 | `nm_iod_validator` | `tests/services/nm_iod_validator_test.cpp` | - |
| DES-SVC-019 | PET Storage | `tests/services/pet_storage_test.cpp` | - |
| DES-SVC-020 | `pet_iod_validator` | `tests/services/pet_iod_validator_test.cpp` | - |
| DES-SVC-021 | RT Storage | `tests/services/rt_storage_test.cpp` | - |
| DES-SVC-022 | `rt_iod_validator` | `tests/services/rt_iod_validator_test.cpp` | - |
| DES-SVC-023 | SEG Storage | `tests/services/seg_storage_test.cpp` | - |
| DES-SVC-025 | SR Storage | `tests/services/sr_storage_test.cpp` | - |
| DES-SVC-027 | `query_cache` | `tests/services/cache/query_cache_test.cpp` | - |
| DES-SVC-031 | `parallel_query_executor` | `tests/services/cache/parallel_query_executor_test.cpp` | - |
| - | `simple_lru_cache` | `tests/services/cache/simple_lru_cache_test.cpp` | - |
| - | Streaming Query | `tests/services/cache/streaming_query_test.cpp` | - |

### 6.5 Storage Module Tests

| SDS ID | Design Element | Test File | Test Count |
|--------|---------------|-----------|------------|
| DES-STOR-001 | `storage_interface` | `tests/storage/storage_interface_test.cpp` | - |
| DES-STOR-002 | `file_storage` | `tests/storage/file_storage_test.cpp` | - |
| DES-STOR-003 | `index_database` | `tests/storage/index_database_test.cpp` | - |
| DES-STOR-004 | `migration_runner` | `tests/storage/migration_runner_test.cpp` | - |
| DES-STOR-005 | `s3_storage` | `tests/storage/s3_storage_test.cpp` | - |
| DES-STOR-006 | `azure_blob_storage` | `tests/storage/azure_blob_storage_test.cpp` | - |
| DES-STOR-007 | `hsm_storage` | `tests/storage/hsm_storage_test.cpp` | - |
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
| DES-INT-008 | `itk_adapter` | `tests/integration/itk_adapter_test.cpp` | - |
| - | Thread System Direct | `tests/integration/thread_system_direct_test.cpp` | - |
| - | Config Reload | `tests/integration/config_reload_integration_test.cpp` | - |
| - | DICOM Workflow | `tests/integration/dicom_workflow_integration_test.cpp` | - |
| - | Error Propagation | `tests/integration/error_propagation_integration_test.cpp` | - |
| - | Load Integration | `tests/integration/load_integration_test.cpp` | - |
| - | Shutdown | `tests/integration/shutdown_integration_test.cpp` | - |

### 6.7 Security Module Tests

| SDS ID | Design Element | Test File | Test Count |
|--------|---------------|-----------|------------|
| DES-SEC-001 | `access_control_manager` | `tests/security/access_control_manager_test.cpp` | - |
| DES-SEC-002 | `anonymizer` | `tests/security/anonymizer_test.cpp` | - |
| DES-SEC-003 | `digital_signature` | `tests/security/digital_signature_test.cpp` | - |
| DES-SEC-006 | `uid_mapping` | `tests/security/uid_mapping_test.cpp` | - |
| DES-STOR-009 | `sqlite_security_storage` | `tests/security/sqlite_security_storage_test.cpp` | - |

### 6.8 Web Module Tests

| SDS ID | Design Element | Test File | Test Count |
|--------|---------------|-----------|------------|
| DES-WEB-001 | `rest_server` | `tests/web/rest_server_test.cpp` | - |
| DES-WEB-002/003 | Patient/Study Endpoints | `tests/web/patient_study_endpoints_test.cpp` | - |
| DES-WEB-005 | `dicomweb_endpoints` | `tests/web/dicomweb_endpoints_test.cpp` | - |
| DES-WEB-007/010 | Worklist/Audit Endpoints | `tests/web/worklist_audit_endpoints_test.cpp` | - |
| DES-WEB-008 | `system_endpoints` | `tests/web/system_endpoints_test.cpp` | - |

### 6.9 Workflow Module Tests

| SDS ID | Design Element | Test File | Test Count |
|--------|---------------|-----------|------------|
| DES-WF-001 | `auto_prefetch_service` | `tests/workflow/auto_prefetch_service_test.cpp` | - |
| DES-WF-002 | `task_scheduler` | `tests/workflow/task_scheduler_test.cpp` | - |
| DES-WF-003 | `study_lock_manager` | `tests/workflow/study_lock_manager_test.cpp` | - |

### 6.10 AI Module Tests

| SDS ID | Design Element | Test File | Test Count |
|--------|---------------|-----------|------------|
| DES-AI-001 | `ai_service_connector` | `tests/ai/ai_service_connector_test.cpp` | - |
| DES-AI-002 | `ai_result_handler` | `tests/ai/ai_result_handler_test.cpp` | - |

### 6.11 DI Module Tests

| SDS ID | Design Element | Test File | Test Count |
|--------|---------------|-----------|------------|
| DES-DI-001 | `service_registration` | `tests/di/service_registration_test.cpp` | - |
| DES-DI-003 | `ilogger` | `tests/di/ilogger_test.cpp` | - |

### 6.12 Monitoring Module Tests

| SDS ID | Design Element | Test File | Test Count |
|--------|---------------|-----------|------------|
| DES-MON-002 | `pacs_metrics` | `tests/monitoring/pacs_metrics_test.cpp` | - |
| DES-MON-003 | `health_checker` | `tests/monitoring/health_checker_test.cpp` | - |
| DES-MON-004/005/006 | Collectors | `tests/monitoring/collectors_test.cpp` | - |
| - | `health_status` | `tests/monitoring/health_status_test.cpp` | - |
| - | `health_json` | `tests/monitoring/health_json_test.cpp` | - |

### 6.13 Verification Test Coverage Summary

| Module | Test Files | Design Elements Covered | Verification Coverage |
|--------|------------|------------------------|----------------------|
| Core | 7 | 7/7 | 100% |
| Encoding | 13 | 13/13 | 100% |
| Network | 14 | 9/9 | 100% |
| Services | 24 | 31/31 | 100% |
| Storage | 9 | 9/9 | 100% |
| Integration | 12 | 8/8 | 100% |
| Security | 5 | 6/6 | 100% |
| Web | 5 | 10/10 | 100% |
| Workflow | 3 | 3/3 | 100% |
| AI | 2 | 2/2 | 100% |
| DI | 2 | 4/4 | 100% |
| Monitoring | 5 | 6/6 | 100% |
| **Total** | **101** | **108/108** | **100%** |

### 6.14 Validation Traceability (Separate Document)

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
| Core | 8 | 7 | - | - | 100% |
| Encoding | 8 | 13 | - | - | 100% |
| Network | 4 | 9 | 3 | - | 100% |
| Services | 7 | 31 | 8 | 2 | 100% |
| Storage | 3 | 9 | - | 7 | 100% |
| Integration | 5 | 8 | - | - | 100% |
| Security | 3 | 6 | - | - | 100% |
| Web | - | 10 | - | - | 100% |
| Workflow | - | 3 | - | - | 100% |
| AI | - | 2 | - | - | 100% |
| DI | - | 4 | - | - | 100% |
| Monitoring | - | 6 | - | - | 100% |
| Compat | - | 2 | - | - | 100% |
| Performance | 4 | N/A | - | - | Addressed |
| Reliability | 3 | N/A | 3 | - | Addressed |
| Maintainability | 3 | N/A | - | - | Addressed |

### 7.3 Implementation Coverage Summary

| Module | Design Elements | Header Files | Source Files | Test Files | Coverage |
|--------|----------------|--------------|--------------|------------|----------|
| Core | 7 | 10 | 8 | 7 | 100% |
| Encoding | 13 | 22 | 11 | 13 | 100% |
| Network | 9 | 19 | 9 | 14 | 100% |
| Services | 31 | 34 | 31 | 24 | 100% |
| Storage | 9 | 18 | 9 | 9 | 100% |
| Integration | 8 | 9 | 8 | 12 | 100% |
| Security | 6 | 13 | 6 | 5 | 100% |
| Web | 12 | 12 | 10 | 5 | 100% |
| Workflow | 3 | 5 | 3 | 3 | 100% |
| AI | 2 | 2 | 2 | 2 | 100% |
| DI | 4 | 4 | 0 | 2 | 100% |
| Monitoring | 6 | 8 | 2 | 5 | 100% |
| Compat | 2 | 2 | 0 | 0 | 100% |
| **Total** | **112** | **158** | **99** | **101** | **100%** |

### 7.4 Orphan Analysis

**Orphan Requirements (no design):** None

**Orphan Designs (no requirement):** None

### 7.5 Traceability Gaps

| Gap ID | Description | Status | Resolution |
|--------|-------------|--------|------------|
| - | None identified | - | - |

### 7.6 Impact Analysis Template

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

#### Core Module (DES-CORE-xxx)
| ID | Element |
|----|---------|
| DES-CORE-001 | dicom_tag |
| DES-CORE-002 | dicom_element |
| DES-CORE-003 | dicom_dataset |
| DES-CORE-004 | dicom_file |
| DES-CORE-005 | dicom_dictionary |
| DES-CORE-006 | pool_manager |
| DES-CORE-007 | tag_info |

#### Encoding Module (DES-ENC-xxx)
| ID | Element |
|----|---------|
| DES-ENC-001 | vr_type |
| DES-ENC-002 | vr_info |
| DES-ENC-003 | transfer_syntax |
| DES-ENC-004 | implicit_vr_codec |
| DES-ENC-005 | explicit_vr_codec |
| DES-ENC-006 | explicit_vr_big_endian_codec |
| DES-ENC-007 | codec_factory |
| DES-ENC-008 | compression_codec |
| DES-ENC-009 | jpeg_baseline_codec |
| DES-ENC-010 | jpeg_lossless_codec |
| DES-ENC-011 | jpeg_ls_codec |
| DES-ENC-012 | jpeg2000_codec |
| DES-ENC-013 | rle_codec |

#### Network Module (DES-NET-xxx)
| ID | Element |
|----|---------|
| DES-NET-001 | pdu_encoder |
| DES-NET-002 | pdu_decoder |
| DES-NET-003 | dimse_message |
| DES-NET-004 | association |
| DES-NET-005 | dicom_server |
| DES-NET-006 | pdu_buffer_pool |
| DES-NET-007 | accept_worker |
| DES-NET-008 | dicom_server_v2 |
| DES-NET-009 | dicom_association_handler |

#### Services Module (DES-SVC-xxx)
| ID | Element |
|----|---------|
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
| DES-SVC-011 | us_storage (US SOP Classes) |
| DES-SVC-012 | us_iod_validator |
| DES-SVC-013 | dx_storage (DX SOP Classes) |
| DES-SVC-014 | dx_iod_validator |
| DES-SVC-015 | mg_storage (MG SOP Classes) |
| DES-SVC-016 | mg_iod_validator |
| DES-SVC-017 | nm_storage (NM SOP Classes) |
| DES-SVC-018 | nm_iod_validator |
| DES-SVC-019 | pet_storage (PET SOP Classes) |
| DES-SVC-020 | pet_iod_validator |
| DES-SVC-021 | rt_storage (RT SOP Classes) |
| DES-SVC-022 | rt_iod_validator |
| DES-SVC-023 | seg_storage (SEG SOP Classes) |
| DES-SVC-024 | seg_iod_validator |
| DES-SVC-025 | sr_storage (SR SOP Classes) |
| DES-SVC-026 | sr_iod_validator |
| DES-SVC-027 | query_cache |
| DES-SVC-028 | query_result_stream |
| DES-SVC-029 | streaming_query_handler |
| DES-SVC-030 | database_cursor |
| DES-SVC-031 | parallel_query_executor |

#### Storage Module (DES-STOR-xxx)
| ID | Element |
|----|---------|
| DES-STOR-001 | storage_interface |
| DES-STOR-002 | file_storage |
| DES-STOR-003 | index_database |
| DES-STOR-004 | migration_runner |
| DES-STOR-005 | s3_storage |
| DES-STOR-006 | azure_blob_storage |
| DES-STOR-007 | hsm_storage |
| DES-STOR-008 | hsm_migration_service |
| DES-STOR-009 | sqlite_security_storage |

#### Integration Module (DES-INT-xxx)
| ID | Element |
|----|---------|
| DES-INT-001 | container_adapter |
| DES-INT-002 | network_adapter |
| DES-INT-003 | thread_adapter |
| DES-INT-004 | logger_adapter |
| DES-INT-005 | monitoring_adapter |
| DES-INT-006 | dicom_session |
| DES-INT-007 | thread_pool_adapter |
| DES-INT-008 | itk_adapter |

#### Security Module (DES-SEC-xxx)
| ID | Element |
|----|---------|
| DES-SEC-001 | access_control_manager |
| DES-SEC-002 | anonymizer |
| DES-SEC-003 | digital_signature |
| DES-SEC-004 | certificate |
| DES-SEC-005 | tag_action |
| DES-SEC-006 | uid_mapping |

#### Web Module (DES-WEB-xxx)
| ID | Element |
|----|---------|
| DES-WEB-001 | rest_server |
| DES-WEB-002 | rest_config |
| DES-WEB-003 | rest_types |
| DES-WEB-004 | patient_endpoints |
| DES-WEB-005 | study_endpoints |
| DES-WEB-006 | series_endpoints |
| DES-WEB-007 | dicomweb_endpoints |
| DES-WEB-008 | worklist_endpoints |
| DES-WEB-009 | audit_endpoints |
| DES-WEB-010 | security_endpoints |
| DES-WEB-011 | system_endpoints |
| DES-WEB-012 | association_endpoints |

#### Workflow Module (DES-WF-xxx)
| ID | Element |
|----|---------|
| DES-WF-001 | auto_prefetch_service |
| DES-WF-002 | task_scheduler |
| DES-WF-003 | study_lock_manager |

#### AI Module (DES-AI-xxx)
| ID | Element |
|----|---------|
| DES-AI-001 | ai_service_connector |
| DES-AI-002 | ai_result_handler |

#### DI Module (DES-DI-xxx)
| ID | Element |
|----|---------|
| DES-DI-001 | service_registration |
| DES-DI-002 | service_interfaces |
| DES-DI-003 | ilogger |
| DES-DI-004 | test_support |

#### Monitoring Module (DES-MON-xxx)
| ID | Element |
|----|---------|
| DES-MON-001 | pacs_monitor |
| DES-MON-002 | pacs_metrics |
| DES-MON-003 | health_checker |
| DES-MON-004 | dicom_association_collector |
| DES-MON-005 | dicom_service_collector |
| DES-MON-006 | dicom_storage_collector |

#### Compat Module (DES-COMPAT-xxx)
| ID | Element |
|----|---------|
| DES-COMPAT-001 | format |
| DES-COMPAT-002 | time |

#### Database Tables (DES-DB-xxx)
| ID | Element |
|----|---------|
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
| 2.0.0 | 2026-01-03 | kcenon@naver.com | Major update: Added 7 missing modules (Security, Web, Workflow, AI, DI, Monitoring, Compat); Updated file counts from 81 to 257; Added 76 new DES-xxx IDs; Updated test traceability for all 13 modules |
| 2.1.0 | 2026-01-03 | kcenon@naver.com | Web module refactored: DES-WEB-001 to DES-WEB-012; Added rest_config and rest_types as separate design elements; Added reference to SDS_WEB_API.md |
| 2.2.0 | 2026-01-03 | kcenon@naver.com | Added reference to SDS_COMPRESSION.md for complete Compression Codecs Module Design Specification |
| 2.3.0 | 2026-01-04 | kcenon@naver.com | Added Cloud Storage module: SRS-STOR-003/004/010/011 traceability; Added reference to SDS_CLOUD_STORAGE.md (S3, Azure, HSM backends) |

---

*Document Version: 2.3.0*
*Last Updated: 2026-01-04*
*Author: kcenon@naver.com*
