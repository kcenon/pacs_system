# SDS - Requirements Traceability Matrix

> **Version:** 1.0.0
> **Parent Document:** [SDS.md](SDS.md)
> **Last Updated:** 2025-11-30

---

## Table of Contents

- [1. Overview](#1-overview)
- [2. PRD to SRS Traceability](#2-prd-to-srs-traceability)
- [3. SRS to SDS Traceability](#3-srs-to-sds-traceability)
- [4. Complete Traceability Chain](#4-complete-traceability-chain)
- [5. Coverage Analysis](#5-coverage-analysis)

---

## 1. Overview

### 1.1 Purpose

This document provides complete traceability from Product Requirements (PRD) through Software Requirements (SRS) to Design Specifications (SDS). It ensures:

- All requirements are addressed by design
- No design elements exist without requirements
- Test planning can reference design elements
- Impact analysis for requirement changes

### 1.2 Traceability Chain

```
┌─────────┐       ┌─────────┐       ┌─────────┐       ┌─────────┐
│   PRD   │──────►│   SRS   │──────►│   SDS   │──────►│  Tests  │
│         │       │         │       │         │       │         │
│  FR-x   │       │SRS-xxx  │       │DES-xxx  │       │ TC-xxx  │
│  NFR-x  │       │         │       │SEQ-xxx  │       │         │
│  IR-x   │       │         │       │DES-DB-x │       │         │
└─────────┘       └─────────┘       └─────────┘       └─────────┘
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

## 5. Coverage Analysis

### 5.1 Requirements Coverage Summary

| Category | Total PRD | Traced to SRS | Traced to SDS | Coverage |
|----------|-----------|---------------|---------------|----------|
| Functional (FR) | 14 | 14 | 14 | 100% |
| Non-Functional (NFR) | 4 | 4 | 4 | 100% |
| Integration (IR) | 5 | 5 | 5 | 100% |
| **Total** | **23** | **23** | **23** | **100%** |

### 5.2 SRS to SDS Coverage

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

### 5.3 Orphan Analysis

**Orphan Requirements (no design):** None

**Orphan Designs (no requirement):** None

### 5.4 Traceability Gaps

| Gap ID | Description | Status | Resolution |
|--------|-------------|--------|------------|
| - | None identified | - | - |

### 5.5 Impact Analysis Template

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

*Document Version: 1.0.0*
*Created: 2025-11-30*
*Author: kcenon@naver.com*
