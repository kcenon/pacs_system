# Product Requirements Document - PACS System

> **Version:** 0.2.0.0
> **Last Updated:** 2026-02-08
> **Language:** **English** | [한국어](PRD_KO.md)

---

## Table of Contents

- [Executive Summary](#executive-summary)
- [Product Vision](#product-vision)
- [Target Users](#target-users)
- [Functional Requirements](#functional-requirements)
- [Non-Functional Requirements](#non-functional-requirements)
- [System Architecture Requirements](#system-architecture-requirements)
- [DICOM Conformance Requirements](#dicom-conformance-requirements)
- [Integration Requirements](#integration-requirements)
- [Security Requirements](#security-requirements)
- [Performance Requirements](#performance-requirements)
- [Development Phases](#development-phases)
- [Success Metrics](#success-metrics)
- [Risks and Mitigations](#risks-and-mitigations)
- [Appendices](#appendices)

---

## Executive Summary

### Product Name
PACS System (Picture Archiving and Communication System)

### Product Description
A modern C++20 PACS implementation built entirely on the kcenon ecosystem without external DICOM libraries. This system implements the DICOM standard from scratch, providing complete control over the codebase while leveraging high-performance infrastructure from existing ecosystem components.

### Key Differentiators
- **Zero External DICOM Dependencies**: Pure implementation using kcenon ecosystem
- **High Performance**: SIMD acceleration, lock-free queues, async I/O, object pool memory management
- **Full Ecosystem Integration**: Native integration with 6 existing systems
- **Production Grade**: Comprehensive CI/CD, sanitizers, quality metrics
- **Complete DICOMweb Support**: WADO-RS, STOW-RS, QIDO-RS per PS3.18
- **19-Endpoint REST API**: Full web administration and monitoring via Crow framework
- **AI-Ready**: External AI service integration for medical image analysis
- **Multi-Node Federation**: Client module with routing, sync, prefetch, and remote node management

---

## Product Vision

### Vision Statement
To create a fully controllable, high-performance PACS solution that demonstrates the capability of the kcenon ecosystem while providing complete transparency and customizability for medical imaging workflows.

### Strategic Goals

| Goal | Description | Success Criteria |
|------|-------------|------------------|
| **Independence** | Eliminate dependency on external DICOM libraries | Zero GPL/LGPL dependencies |
| **Performance** | Match or exceed DCMTK performance | ≥100 MB/s image storage |
| **Ecosystem** | Maximize reuse of existing systems | ≥80% infrastructure reuse |
| **Compliance** | Meet DICOM conformance requirements | Pass conformance tests |
| **Quality** | Match ecosystem quality standards | CI/CD green, 80%+ coverage |

---

## Target Users

### Primary Users

#### 1. Healthcare IT Developers
- **Profile**: Software developers building medical imaging applications
- **Needs**:
  - Well-documented DICOM API
  - Easy integration with existing systems
  - Customizable DICOM services
- **Pain Points**:
  - Complex DCMTK learning curve
  - License concerns with GPL libraries
  - Limited customization options

#### 2. Medical Equipment Integrators
- **Profile**: Engineers connecting imaging devices to PACS
- **Needs**:
  - Reliable Storage SCP
  - Modality Worklist support
  - MPPS integration
- **Pain Points**:
  - Interoperability issues
  - Limited debugging tools
  - Complex protocol debugging

#### 3. Research Institutions
- **Profile**: Academic researchers in medical imaging
- **Needs**:
  - Source code access for modification
  - Custom SOP class support
  - Flexible data extraction
- **Pain Points**:
  - Black-box commercial solutions
  - License restrictions for research

### Secondary Users

#### System Administrators
- Monitor system health and performance
- Configure DICOM services
- Manage storage and retention policies

#### Radiologists (Indirect)
- End users of PACS viewer applications
- Require fast image retrieval
- Need reliable worklist integration

---

## Functional Requirements

### FR-1: DICOM Core Data Handling

#### FR-1.1: Data Element Processing
| ID | Requirement | Priority | Phase |
|----|-------------|----------|-------|
| FR-1.1.1 | Parse DICOM Data Elements (Tag, VR, Length, Value) | Must Have | 1 |
| FR-1.1.2 | Support all 34 VR types as per PS3.5 (including DICOM 2019b) | Must Have | 1 |
| FR-1.1.3 | Handle nested Sequence (SQ) elements recursively | Must Have | 1 |
| FR-1.1.4 | Support private tags and private creator elements | Should Have | 2 |
| FR-1.1.5 | Validate data element consistency | Should Have | 2 |

#### FR-1.2: Data Set Operations
| ID | Requirement | Priority | Phase |
|----|-------------|----------|-------|
| FR-1.2.1 | Create, modify, and delete data elements | Must Have | 1 |
| FR-1.2.2 | Iterate over data elements in tag order | Must Have | 1 |
| FR-1.2.3 | Search elements by tag, keyword, or path | Must Have | 1 |
| FR-1.2.4 | Deep copy and move data sets | Must Have | 1 |
| FR-1.2.5 | Merge data sets with conflict resolution | Should Have | 2 |

#### FR-1.3: DICOM File Handling (Part 10)
| ID | Requirement | Priority | Phase |
|----|-------------|----------|-------|
| FR-1.3.1 | Read DICOM files with File Meta Information | Must Have | 1 |
| FR-1.3.2 | Write DICOM files with proper preamble and prefix | Must Have | 1 |
| FR-1.3.3 | Support Implicit VR Little Endian | Must Have | 1 |
| FR-1.3.4 | Support Explicit VR Little Endian | Must Have | 1 |
| FR-1.3.5 | Support Explicit VR Big Endian | Should Have | 2 |
| FR-1.3.6 | Validate Transfer Syntax on read | Must Have | 1 |

---

### FR-2: Network Protocol

#### FR-2.1: Upper Layer Protocol (PDU)
| ID | Requirement | Priority | Phase |
|----|-------------|----------|-------|
| FR-2.1.1 | Implement A-ASSOCIATE-RQ PDU encoding/decoding | Must Have | 2 |
| FR-2.1.2 | Implement A-ASSOCIATE-AC PDU encoding/decoding | Must Have | 2 |
| FR-2.1.3 | Implement A-ASSOCIATE-RJ PDU encoding/decoding | Must Have | 2 |
| FR-2.1.4 | Implement P-DATA-TF PDU encoding/decoding | Must Have | 2 |
| FR-2.1.5 | Implement A-RELEASE-RQ/RP PDU encoding/decoding | Must Have | 2 |
| FR-2.1.6 | Implement A-ABORT PDU encoding/decoding | Must Have | 2 |
| FR-2.1.7 | Handle PDU fragment reassembly | Must Have | 2 |

#### FR-2.2: Association Management
| ID | Requirement | Priority | Phase |
|----|-------------|----------|-------|
| FR-2.2.1 | Manage association state machine (13 states per PS3.8 Sta1-Sta13) | Must Have | 2 |
| FR-2.2.2 | Negotiate presentation contexts | Must Have | 2 |
| FR-2.2.3 | Support multiple simultaneous associations | Must Have | 2 |
| FR-2.2.4 | Implement association timeout handling | Must Have | 2 |
| FR-2.2.5 | Support extended negotiation items | Should Have | 3 |
| FR-2.2.6 | Implement user identity negotiation | Could Have | 4 |

#### FR-2.3: DIMSE Message Exchange
| ID | Requirement | Priority | Phase |
|----|-------------|----------|-------|
| FR-2.3.1 | Implement C-ECHO request/response | Must Have | 2 |
| FR-2.3.2 | Implement C-STORE request/response | Must Have | 2 |
| FR-2.3.3 | Implement C-FIND request/response | Must Have | 3 |
| FR-2.3.4 | Implement C-MOVE request/response | Must Have | 3 |
| FR-2.3.5 | Implement C-GET request/response | Should Have | 3 |
| FR-2.3.6 | Implement N-CREATE request/response | Should Have | 4 | ✅ Implemented |
| FR-2.3.7 | Implement N-SET request/response | Should Have | 4 | ✅ Implemented |
| FR-2.3.8 | Implement N-GET request/response | Could Have | 4 | ✅ Implemented |
| FR-2.3.9 | Implement N-EVENT-REPORT request/response | Could Have | 4 | ✅ Implemented |
| FR-2.3.10 | Implement N-ACTION request/response | Could Have | 4 | ✅ Implemented |
| FR-2.3.11 | Implement N-DELETE request/response | Could Have | 4 | ✅ Implemented |

---

### FR-3: DICOM Services

#### FR-3.1: Verification Service
| ID | Requirement | Priority | Phase |
|----|-------------|----------|-------|
| FR-3.1.1 | Implement Verification SCP (C-ECHO responder) | Must Have | 2 |
| FR-3.1.2 | Implement Verification SCU (C-ECHO initiator) | Must Have | 2 |
| FR-3.1.3 | Support verification in all association states | Must Have | 2 |

#### FR-3.2: Storage Service
| ID | Requirement | Priority | Phase |
|----|-------------|----------|-------|
| FR-3.2.1 | Implement Storage SCP for CT images | Must Have | 3 |
| FR-3.2.2 | Implement Storage SCP for MR images | Must Have | 3 |
| FR-3.2.3 | Implement Storage SCP for CR/DX images | Must Have | 3 |
| FR-3.2.4 | Implement Storage SCP for secondary capture | Should Have | 3 |
| FR-3.2.5 | Implement Storage SCU for image transmission | Must Have | 3 |
| FR-3.2.6 | Handle storage commitment (optional) | Could Have | 4 |
| FR-3.2.7 | Support all standard storage SOP classes | Should Have | 4 |

#### FR-3.3: Query/Retrieve Service
| ID | Requirement | Priority | Phase |
|----|-------------|----------|-------|
| FR-3.3.1 | Implement Patient Root Q/R Information Model | Must Have | 3 |
| FR-3.3.2 | Implement Study Root Q/R Information Model | Must Have | 3 |
| FR-3.3.3 | Support Patient Level queries (C-FIND) | Must Have | 3 |
| FR-3.3.4 | Support Study Level queries (C-FIND) | Must Have | 3 |
| FR-3.3.5 | Support Series Level queries (C-FIND) | Must Have | 3 |
| FR-3.3.6 | Support Image Level queries (C-FIND) | Must Have | 3 |
| FR-3.3.7 | Implement C-MOVE for image retrieval | Must Have | 3 |
| FR-3.3.8 | Implement C-GET for image retrieval | Should Have | 3 |

#### FR-3.4: Modality Worklist Service
| ID | Requirement | Priority | Phase | Status |
|----|-------------|----------|-------|--------|
| FR-3.4.1 | Implement MWL SCP for scheduled procedures | Should Have | 4 | ✅ Implemented |
| FR-3.4.2 | Support scheduled procedure step matching | Should Have | 4 | ✅ Implemented |
| FR-3.4.3 | Support patient demographic queries | Should Have | 4 | ✅ Implemented |
| FR-3.4.4 | Implement MWL SCU for worklist query initiation | Should Have | 4 | ✅ Implemented |
| FR-3.4.5 | Provide REST API for worklist management (CRUD) | Should Have | 4 | ✅ Implemented |
| FR-3.4.6 | Integrate with HIS/RIS systems (interface) | Could Have | 5 | |

#### FR-3.5: MPPS Service
| ID | Requirement | Priority | Phase | Status |
|----|-------------|----------|-------|--------|
| FR-3.5.1 | Implement MPPS SCP for procedure tracking | Should Have | 4 | ✅ Implemented |
| FR-3.5.2 | Support N-CREATE for MPPS creation | Should Have | 4 | ✅ Implemented |
| FR-3.5.3 | Support N-SET for MPPS modification | Should Have | 4 | ✅ Implemented |
| FR-3.5.4 | Track procedure status (IN PROGRESS, COMPLETED, DISCONTINUED) | Should Have | 4 | ✅ Implemented |
| FR-3.5.5 | Implement MPPS SCU for procedure status reporting | Should Have | 4 | ✅ Implemented |

---

### FR-4: Storage Backend

#### FR-4.1: File System Storage
| ID | Requirement | Priority | Phase |
|----|-------------|----------|-------|
| FR-4.1.1 | Store DICOM files in hierarchical directory structure | Must Have | 3 |
| FR-4.1.2 | Support configurable file naming conventions | Must Have | 3 |
| FR-4.1.3 | Implement atomic file operations | Must Have | 3 |
| FR-4.1.4 | Handle duplicate SOP Instance detection | Must Have | 3 |
| FR-4.1.5 | Support file compression (optional) | Could Have | 4 |

#### FR-4.2: Index Database
| ID | Requirement | Priority | Phase |
|----|-------------|----------|-------|
| FR-4.2.1 | Index Patient/Study/Series/Instance hierarchy | Must Have | 3 |
| FR-4.2.2 | Support fast query by common attributes | Must Have | 3 |
| FR-4.2.3 | Maintain referential integrity | Must Have | 3 |
| FR-4.2.4 | Support concurrent read operations | Must Have | 3 |
| FR-4.2.5 | Implement database recovery mechanisms | Should Have | 4 |

### FR-5: Security Services

| ID | Requirement | Priority | Phase | Status |
|----|-------------|----------|-------|--------|
| FR-5.1 | Implement DICOM dataset anonymization per PS3.15 Annex E | Should Have | 4 | ✅ Implemented |
| FR-5.2 | Support digital signatures for DICOM datasets per PS3.15 | Could Have | 4 | ✅ Implemented |
| FR-5.3 | Implement role-based access control (RBAC) for DICOM operations | Should Have | 4 | ✅ Implemented |
| FR-5.4 | Support X.509 certificate management for TLS and signing | Could Have | 4 | ✅ Implemented |
| FR-5.5 | Implement tag-level security actions for selective anonymization | Should Have | 4 | ✅ Implemented |
| FR-5.6 | Provide UID mapping for privacy-preserving re-identification | Should Have | 4 | ✅ Implemented |
| FR-5.7 | Implement SQLite-based persistent security storage | Should Have | 4 | ✅ Implemented |
| FR-5.8 | Provide REST API endpoints for security configuration | Should Have | 4 | ✅ Implemented |

---

### FR-6: Web/REST API Services

#### FR-6.1: REST API Server
| ID | Requirement | Priority | Phase | Status |
|----|-------------|----------|-------|--------|
| FR-6.1.1 | Provide REST API server using Crow web framework | Should Have | 4 | ✅ Implemented |
| FR-6.1.2 | Support CORS for web browser access | Should Have | 4 | ✅ Implemented |
| FR-6.1.3 | Implement async/sync server modes | Should Have | 4 | ✅ Implemented |

#### FR-6.2: DICOMweb Services
| ID | Requirement | Priority | Phase | Status |
|----|-------------|----------|-------|--------|
| FR-6.2.1 | Implement DICOMweb WADO-RS for web image retrieval per PS3.18 | Should Have | 4 | ✅ Implemented |
| FR-6.2.2 | Implement DICOMweb STOW-RS for web image storage per PS3.18 | Should Have | 4 | ✅ Implemented |
| FR-6.2.3 | Implement DICOMweb QIDO-RS for web image query per PS3.18 | Should Have | 4 | ✅ Implemented |

#### FR-6.3: REST API Endpoints (19 Modules)
| ID | Endpoint Module | Description | Status |
|----|----------------|-------------|--------|
| FR-6.3.1 | System endpoints | System configuration, health, version | ✅ Implemented |
| FR-6.3.2 | Metrics endpoints | Performance metrics and monitoring | ✅ Implemented |
| FR-6.3.3 | Study endpoints | Study-level CRUD operations | ✅ Implemented |
| FR-6.3.4 | Series endpoints | Series-level operations | ✅ Implemented |
| FR-6.3.5 | Patient endpoints | Patient information management | ✅ Implemented |
| FR-6.3.6 | Metadata endpoints | DICOM metadata queries | ✅ Implemented |
| FR-6.3.7 | DICOMweb endpoints | WADO-RS, STOW-RS, QIDO-RS | ✅ Implemented |
| FR-6.3.8 | Jobs endpoints | Async job management and tracking | ✅ Implemented |
| FR-6.3.9 | Routing endpoints | Message routing configuration | ✅ Implemented |
| FR-6.3.10 | Remote nodes endpoints | Remote PACS node management | ✅ Implemented |
| FR-6.3.11 | Worklist endpoints | Worklist management | ✅ Implemented |
| FR-6.3.12 | Annotation endpoints | Annotation CRUD operations | ✅ Implemented |
| FR-6.3.13 | Viewer state endpoints | Viewer state persistence | ✅ Implemented |
| FR-6.3.14 | Measurement endpoints | Measurement data management | ✅ Implemented |
| FR-6.3.15 | Key image endpoints | Key image selection management | ✅ Implemented |
| FR-6.3.16 | Thumbnail endpoints | Image thumbnail generation | ✅ Implemented |
| FR-6.3.17 | Audit endpoints | Audit log access | ✅ Implemented |
| FR-6.3.18 | Security endpoints | Security configuration | ✅ Implemented |
| FR-6.3.19 | Association endpoints | DICOM association management | ✅ Implemented |

---

### FR-7: Workflow Services

#### FR-7.1: Auto Prefetch Service
| ID | Requirement | Priority | Phase | Status |
|----|-------------|----------|-------|--------|
| FR-7.1.1 | Implement worklist-triggered prior study prefetch from remote PACS | Should Have | 4 | ✅ Implemented |
| FR-7.1.2 | Support configurable prefetch criteria (modality, body part, lookback period) | Should Have | 4 | ✅ Implemented |
| FR-7.1.3 | Support multi-source prefetch from multiple remote PACS servers | Should Have | 4 | ✅ Implemented |
| FR-7.1.4 | Implement rate limiting and deduplication for prefetch requests | Should Have | 4 | ✅ Implemented |
| FR-7.1.5 | Provide retry logic for failed prefetch operations | Should Have | 4 | ✅ Implemented |

#### FR-7.2: Task Scheduler Service
| ID | Requirement | Priority | Phase | Status |
|----|-------------|----------|-------|--------|
| FR-7.2.1 | Implement interval-based, cron-like, and one-time task scheduling | Should Have | 4 | ✅ Implemented |
| FR-7.2.2 | Provide built-in task types: cleanup, archive, verification | Should Have | 4 | ✅ Implemented |
| FR-7.2.3 | Support task lifecycle management (pause, resume, cancel, trigger) | Should Have | 4 | ✅ Implemented |
| FR-7.2.4 | Track execution history with configurable retention | Should Have | 4 | ✅ Implemented |
| FR-7.2.5 | Implement retry mechanism with configurable attempts and delay | Should Have | 4 | ✅ Implemented |
| FR-7.2.6 | Support per-task execution timeout with async execution | Should Have | 4 | ✅ Implemented |

#### FR-7.3: Study Lock Manager
| ID | Requirement | Priority | Phase | Status |
|----|-------------|----------|-------|--------|
| FR-7.3.1 | Implement thread-safe exclusive and shared locks for study access control | Should Have | 4 | ✅ Implemented |
| FR-7.3.2 | Support migration locks with highest priority for HSM operations | Should Have | 4 | ✅ Implemented |
| FR-7.3.3 | Implement automatic lock expiration and token-based release | Should Have | 4 | ✅ Implemented |
| FR-7.3.4 | Provide force unlock capability for admin operations | Should Have | 4 | ✅ Implemented |

---

### FR-8: Cloud Storage

| ID | Requirement | Priority | Phase | Status |
|----|-------------|----------|-------|--------|
| FR-8.1 | Support AWS S3 as storage backend for DICOM files | Should Have | 4 | ✅ Implemented (Mock) |
| FR-8.2 | Support Azure Blob Storage as storage backend | Should Have | 4 | ✅ Implemented (Mock) |
| FR-8.3 | Implement hierarchical storage management (HSM) with three-tier storage | Should Have | 4 | ✅ Implemented |
| FR-8.4 | Implement automatic age-based migration between storage tiers | Should Have | 4 | ✅ Implemented |
| FR-8.5 | Provide transparent data retrieval across all storage tiers | Should Have | 4 | ✅ Implemented |
| FR-8.6 | Support progress callbacks for upload/download monitoring | Should Have | 4 | ✅ Implemented |
| FR-8.7 | Integrate full AWS SDK for production S3 operations | Could Have | 5 | |
| FR-8.8 | Integrate full Azure SDK for production Blob Storage operations | Could Have | 5 | |

*Note: FR-8.1 and FR-8.2 are currently mock implementations for API validation and testing. Full SDK integrations (FR-8.7, FR-8.8) are planned for Phase 5.*

---

### FR-9: AI Integration

| ID | Requirement | Priority | Phase | Status |
|----|-------------|----------|-------|--------|
| FR-9.1 | Implement AI service connector for external inference endpoints | Should Have | 4 | ✅ Implemented |
| FR-9.2 | Implement AI result handler for processing inference outputs | Should Have | 4 | ✅ Implemented |
| FR-9.3 | Support Segmentation (SEG) SOP class for AI/CAD outputs | Should Have | 4 | ✅ Implemented |
| FR-9.4 | Support Structured Report (SR) SOP classes for AI/CAD results | Should Have | 4 | ✅ Implemented |
| FR-9.5 | Provide IOD validation for AI-generated DICOM objects | Should Have | 4 | ✅ Implemented |

---

### FR-10: Client Module

#### FR-10.1: Job Manager
| ID | Requirement | Priority | Phase | Status |
|----|-------------|----------|-------|--------|
| FR-10.1.1 | Implement client-side job management for async DICOM operations | Should Have | 4 | ✅ Implemented |
| FR-10.1.2 | Track job status, progress, and completion | Should Have | 4 | ✅ Implemented |
| FR-10.1.3 | Provide REST API for job querying and management | Should Have | 4 | ✅ Implemented |

#### FR-10.2: Routing Manager
| ID | Requirement | Priority | Phase | Status |
|----|-------------|----------|-------|--------|
| FR-10.2.1 | Implement configurable routing rules for DICOM data distribution | Should Have | 4 | ✅ Implemented |
| FR-10.2.2 | Support rule-based routing by modality, body part, and AE Title | Should Have | 4 | ✅ Implemented |
| FR-10.2.3 | Provide REST API for routing rule management | Should Have | 4 | ✅ Implemented |

#### FR-10.3: Sync Manager
| ID | Requirement | Priority | Phase | Status |
|----|-------------|----------|-------|--------|
| FR-10.3.1 | Implement data synchronization across PACS nodes | Should Have | 4 | ✅ Implemented |
| FR-10.3.2 | Support sync configuration, history tracking, and conflict resolution | Should Have | 4 | ✅ Implemented |

#### FR-10.4: Remote Node Manager
| ID | Requirement | Priority | Phase | Status |
|----|-------------|----------|-------|--------|
| FR-10.4.1 | Implement remote PACS node discovery and management | Should Have | 4 | ✅ Implemented |
| FR-10.4.2 | Provide REST API for remote node registration and status monitoring | Should Have | 4 | ✅ Implemented |

#### FR-10.5: Prefetch Manager
| ID | Requirement | Priority | Phase | Status |
|----|-------------|----------|-------|--------|
| FR-10.5.1 | Implement intelligent prior study prefetch strategy management | Should Have | 4 | ✅ Implemented |
| FR-10.5.2 | Support prefetch rule configuration and history tracking | Should Have | 4 | ✅ Implemented |

---

### FR-11: Annotation, Viewer State, and Clinical Data

#### FR-11.1: Annotation Support
| ID | Requirement | Priority | Phase | Status |
|----|-------------|----------|-------|--------|
| FR-11.1.1 | Implement annotation repository for DICOM image annotations | Should Have | 4 | ✅ Implemented |
| FR-11.1.2 | Provide REST API for annotation CRUD operations | Should Have | 4 | ✅ Implemented |

#### FR-11.2: Viewer State Management
| ID | Requirement | Priority | Phase | Status |
|----|-------------|----------|-------|--------|
| FR-11.2.1 | Implement viewer state persistence for user display preferences | Should Have | 4 | ✅ Implemented |
| FR-11.2.2 | Support viewer state records for per-session state tracking | Should Have | 4 | ✅ Implemented |
| FR-11.2.3 | Provide REST API for viewer state management | Should Have | 4 | ✅ Implemented |

#### FR-11.3: Key Image Management
| ID | Requirement | Priority | Phase | Status |
|----|-------------|----------|-------|--------|
| FR-11.3.1 | Implement key image selection repository for significant findings | Should Have | 4 | ✅ Implemented |
| FR-11.3.2 | Provide REST API for key image selection management | Should Have | 4 | ✅ Implemented |

#### FR-11.4: Measurement Data
| ID | Requirement | Priority | Phase | Status |
|----|-------------|----------|-------|--------|
| FR-11.4.1 | Implement measurement repository for clinical measurements | Should Have | 4 | ✅ Implemented |
| FR-11.4.2 | Provide REST API for measurement data management | Should Have | 4 | ✅ Implemented |

---

### FR-12: Monitoring and Observability

#### FR-12.1: Database Monitoring
| ID | Requirement | Priority | Phase | Status |
|----|-------------|----------|-------|--------|
| FR-12.1.1 | Implement database metrics service for performance monitoring | Should Have | 4 | ✅ Implemented |
| FR-12.1.2 | Provide REST API for database metrics access | Should Have | 4 | ✅ Implemented |

#### FR-12.2: DICOM Metric Collectors
| ID | Requirement | Priority | Phase | Status |
|----|-------------|----------|-------|--------|
| FR-12.2.1 | Implement association lifecycle metric collector | Should Have | 4 | ✅ Implemented |
| FR-12.2.2 | Implement DIMSE operation metric collector | Should Have | 4 | ✅ Implemented |
| FR-12.2.3 | Implement storage and transfer metric collector | Should Have | 4 | ✅ Implemented |
| FR-12.2.4 | Support Prometheus text exposition format | Should Have | 4 | ✅ Implemented |
| FR-12.2.5 | Support JSON export for REST API integration | Should Have | 4 | ✅ Implemented |

#### FR-12.3: Object Pool Memory Management
| ID | Requirement | Priority | Phase | Status |
|----|-------------|----------|-------|--------|
| FR-12.3.1 | Implement object pooling for dicom_element, dicom_dataset, and PDU buffers | Should Have | 4 | ✅ Implemented |
| FR-12.3.2 | Provide RAII-based automatic pool return | Should Have | 4 | ✅ Implemented |
| FR-12.3.3 | Support pool hit ratio monitoring and statistics | Should Have | 4 | ✅ Implemented |

#### FR-12.4: Health Checks
| ID | Requirement | Priority | Phase | Status |
|----|-------------|----------|-------|--------|
| FR-12.4.1 | Implement health, readiness, and liveness probe endpoints | Should Have | 4 | ✅ Implemented |
| FR-12.4.2 | Support custom health check registration | Should Have | 4 | ✅ Implemented |

#### FR-12.5: Thumbnail and Metadata Services
| ID | Requirement | Priority | Phase | Status |
|----|-------------|----------|-------|--------|
| FR-12.5.1 | Implement image thumbnail generation service | Should Have | 4 | ✅ Implemented |
| FR-12.5.2 | Implement metadata query service for efficient DICOM attribute access | Should Have | 4 | ✅ Implemented |

---

## Non-Functional Requirements

### NFR-1: Performance

| ID | Requirement | Target | Measurement |
|----|-------------|--------|-------------|
| NFR-1.1 | Image storage throughput | ≥100 MB/s | Sustained C-STORE rate |
| NFR-1.2 | Concurrent associations | ≥100 | Simultaneous connections |
| NFR-1.3 | Query response time (P95) | <100 ms | C-FIND response latency |
| NFR-1.4 | Association establishment | <50 ms | A-ASSOCIATE round trip |
| NFR-1.5 | Memory usage baseline | <500 MB | Idle server memory |
| NFR-1.6 | Memory per association | <10 MB | Per-connection overhead |

### NFR-2: Reliability

| ID | Requirement | Target | Measurement |
|----|-------------|--------|-------------|
| NFR-2.1 | System uptime | 99.9% | Monthly availability |
| NFR-2.2 | Data integrity | 100% | Zero data corruption |
| NFR-2.3 | Graceful degradation | Required | Under high load |
| NFR-2.4 | Error recovery | Automatic | Connection failures |
| NFR-2.5 | Transaction safety | ACID | File operations |

### NFR-3: Scalability

| ID | Requirement | Target | Measurement |
|----|-------------|--------|-------------|
| NFR-3.1 | Horizontal scaling | Supported | Multiple instances |
| NFR-3.2 | Image capacity | ≥1M studies | Per storage instance |
| NFR-3.3 | Linear throughput scaling | ≥80% efficiency | With additional workers |
| NFR-3.4 | Queue capacity | ≥10K jobs | Pending operations |

### NFR-4: Security

| ID | Requirement | Target | Measurement |
|----|-------------|--------|-------------|
| NFR-4.1 | TLS support | TLS 1.2/1.3 | DICOM TLS |
| NFR-4.2 | Access logging | Complete | All DICOM operations |
| NFR-4.3 | Audit trail | HIPAA compliant | PHI access |
| NFR-4.4 | Input validation | 100% | All network input |
| NFR-4.5 | Memory safety | Zero leaks | AddressSanitizer clean |

### NFR-5: Maintainability

| ID | Requirement | Target | Measurement |
|----|-------------|--------|-------------|
| NFR-5.1 | Code coverage | ≥80% | Line coverage |
| NFR-5.2 | Documentation | Complete | All public APIs |
| NFR-5.3 | CI/CD pipeline | 100% green | All platforms |
| NFR-5.4 | Thread safety | Verified | ThreadSanitizer clean |
| NFR-5.5 | Modular design | Low coupling | Interface-driven |

---

## System Architecture Requirements

### SAR-1: Module Dependencies

```
┌──────────────────────────────────────────────────────────────────────────┐
│                              pacs_system                                  │
├──────────────────────────────────────────────────────────────────────────┤
│  ┌───────────┐  ┌──────────┐  ┌──────────┐  ┌────────────┐  ┌────────┐ │
│  │    web    │  │  client  │  │    ai    │  │  workflow  │  │security│ │
│  │           │  │          │  │          │  │            │  │        │ │
│  │ REST API  │  │ Job Mgr  │  │ AI Svc   │  │ Prefetch   │  │ RBAC   │ │
│  │ DICOMweb  │  │ Routing  │  │ AI Rslt  │  │ Scheduler  │  │ Anonym │ │
│  │ 19 Endpts │  │ Sync Mgr │  │ Handler  │  │ Study Lock │  │ DigSig │ │
│  │           │  │ Prefetch │  │          │  │            │  │        │ │
│  │           │  │ RemoteNd │  │          │  │            │  │        │ │
│  └─────┬─────┘  └────┬─────┘  └────┬─────┘  └─────┬──────┘  └───┬────┘ │
│        │              │             │              │             │      │
│  ┌─────▼──────────────▼─────────────▼──────────────▼─────────────▼────┐ │
│  │  ┌──────────────┐  ┌──────────────┐  ┌────────────────────────┐    │ │
│  │  │   services   │  │   network    │  │       storage          │    │ │
│  │  │              │  │              │  │                        │    │ │
│  │  │ Storage SCP  │  │ PDU Layer    │  │ File Storage           │    │ │
│  │  │ Q/R SCP      │◄─│ DIMSE Layer  │◄─│ Index Database         │    │ │
│  │  │ MWL SCP      │  │ Association  │  │ Cloud Storage (S3/Az)  │    │ │
│  │  │ MPPS SCP     │  │ Server V2    │  │ HSM Storage            │    │ │
│  │  │ Monitoring   │  │              │  │ Annotation/Viewer/Key  │    │ │
│  │  └──────┬───────┘  └──────┬───────┘  └───────────┬────────────┘    │ │
│  └─────────┼─────────────────┼──────────────────────┼─────────────────┘ │
│  ┌─────────▼─────────────────▼──────────────────────▼─────────────────┐ │
│  │                         core                                        │ │
│  │  dicom_element | dicom_dataset | dicom_file | dictionary | pool_mgr│ │
│  └─────────────────────────┬───────────────────────────────────────────┘ │
│                            │                                             │
│  ┌─────────────────────────▼───────────────────────────────────────────┐ │
│  │                      encoding                                        │ │
│  │  vr_types | transfer_syntax | implicit_vr | explicit_vr | SIMD      │ │
│  │  JPEG | JPEG2K | JPEG-LS | RLE | compression codecs                 │ │
│  └─────────────────────────┬───────────────────────────────────────────┘ │
│                            │                                             │
│  ┌─────────────────────────▼───────────────────────────────────────────┐ │
│  │                     integration                                      │ │
│  │  container_adapter | network_adapter | thread_adapter | itk_adapter │ │
│  │  logger_adapter | monitoring_adapter | executor_adapter              │ │
│  └─────────────────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────────────┘
                                │
        ┌───────────────────────┼───────────────────────┐
        │                       │                       │
┌───────▼───────┐    ┌──────────▼──────────┐    ┌───────▼───────┐
│network_system │    │   thread_system     │    │container_system│
│  (TCP/TLS)    │    │  (Thread Pool)      │    │(Serialization) │
└───────────────┘    └────────────────────┘    └───────────────┘
        │                       │                       │
        └───────────────────────┼───────────────────────┘
                                │
        ┌───────────────────────┼───────────────────────┐
        │                       │                       │
┌───────▼───────┐    ┌──────────▼──────────┐    ┌───────▼───────┐
│ logger_system │    │ monitoring_system   │    │ common_system │
│   (Logging)   │    │    (Metrics)        │    │ (Foundation)  │
└───────────────┘    └────────────────────┘    └───────────────┘
```

### SAR-2: Component Interface Requirements

| Component | Interface Type | Description |
|-----------|---------------|-------------|
| Storage Backend | Abstract Interface | Pluggable storage implementations |
| Association Handler | Observer Pattern | Event-driven association lifecycle |
| DIMSE Processor | Strategy Pattern | Interchangeable message handlers |
| PDU Codec | Template Interface | Type-safe encoding/decoding |
| Service Provider | Factory Pattern | Dynamic service instantiation |

### SAR-3: Thread Model Requirements

| Requirement | Description |
|-------------|-------------|
| Main IO Thread | ASIO event loop for network operations |
| Worker Pool | Thread pool for DIMSE message processing |
| Storage Workers | Dedicated threads for file I/O |
| Query Workers | Separate pool for database queries |
| Lock-free Queues | Inter-thread communication |

---

## DICOM Conformance Requirements

### DCR-1: SOP Class Support

#### Phase 1 (MVP) - Verification
| SOP Class | UID | Role |
|-----------|-----|------|
| Verification | 1.2.840.10008.1.1 | SCP/SCU |

#### Phase 3 - Storage
| SOP Class | UID | Role | Status |
|-----------|-----|------|--------|
| CT Image Storage | 1.2.840.10008.5.1.4.1.1.2 | SCP/SCU | ✅ Implemented |
| MR Image Storage | 1.2.840.10008.5.1.4.1.1.4 | SCP/SCU | ✅ Implemented |
| CR Image Storage | 1.2.840.10008.5.1.4.1.1.1 | SCP/SCU | ✅ Implemented |
| Digital X-Ray | 1.2.840.10008.5.1.4.1.1.1.1 | SCP/SCU | ✅ Implemented |
| Secondary Capture | 1.2.840.10008.5.1.4.1.1.7 | SCP/SCU | ✅ Implemented |
| Ultrasound Image Storage | 1.2.840.10008.5.1.4.1.1.6.1 | SCP/SCU | ✅ Implemented |
| Ultrasound Multi-frame | 1.2.840.10008.5.1.4.1.1.6.2 | SCP/SCU | ✅ Implemented |
| XA Image Storage | 1.2.840.10008.5.1.4.1.1.12.1 | SCP/SCU | ✅ Implemented |
| Enhanced XA Image Storage | 1.2.840.10008.5.1.4.1.1.12.1.1 | SCP/SCU | ✅ Implemented |
| XRF Image Storage | 1.2.840.10008.5.1.4.1.1.12.2 | SCP/SCU | ✅ Implemented |
| NM Image Storage | 1.2.840.10008.5.1.4.1.1.20 | SCP/SCU | ✅ Implemented |
| PET Image Storage | 1.2.840.10008.5.1.4.1.1.128 | SCP/SCU | ✅ Implemented |
| Enhanced PET Image Storage | 1.2.840.10008.5.1.4.1.1.130 | SCP/SCU | ✅ Implemented |
| RT Plan Storage | 1.2.840.10008.5.1.4.1.1.481.5 | SCP/SCU | ✅ Implemented |
| RT Dose Storage | 1.2.840.10008.5.1.4.1.1.481.2 | SCP/SCU | ✅ Implemented |
| RT Structure Set Storage | 1.2.840.10008.5.1.4.1.1.481.3 | SCP/SCU | ✅ Implemented |
| RT Image Storage | 1.2.840.10008.5.1.4.1.1.481.1 | SCP/SCU | ✅ Implemented |
| Segmentation Storage | 1.2.840.10008.5.1.4.1.1.66.4 | SCP/SCU | ✅ Implemented |
| Basic Text SR Storage | 1.2.840.10008.5.1.4.1.1.88.11 | SCP/SCU | ✅ Implemented |
| Enhanced SR Storage | 1.2.840.10008.5.1.4.1.1.88.22 | SCP/SCU | ✅ Implemented |
| Comprehensive SR Storage | 1.2.840.10008.5.1.4.1.1.88.33 | SCP/SCU | ✅ Implemented |
| Key Object Selection Document | 1.2.840.10008.5.1.4.1.1.88.59 | SCP/SCU | ✅ Implemented |
| Mammography CAD SR Storage | 1.2.840.10008.5.1.4.1.1.88.50 | SCP/SCU | ✅ Implemented |

#### Phase 3 - Query/Retrieve
| SOP Class | UID | Role |
|-----------|-----|------|
| Patient Root Q/R Find | 1.2.840.10008.5.1.4.1.2.1.1 | SCP/SCU |
| Patient Root Q/R Move | 1.2.840.10008.5.1.4.1.2.1.2 | SCP/SCU |
| Patient Root Q/R Get | 1.2.840.10008.5.1.4.1.2.1.3 | SCP/SCU |
| Study Root Q/R Find | 1.2.840.10008.5.1.4.1.2.2.1 | SCP/SCU |
| Study Root Q/R Move | 1.2.840.10008.5.1.4.1.2.2.2 | SCP/SCU |
| Study Root Q/R Get | 1.2.840.10008.5.1.4.1.2.2.3 | SCP/SCU |

#### Phase 4 - Workflow
| SOP Class | UID | Role |
|-----------|-----|------|
| Modality Worklist | 1.2.840.10008.5.1.4.31 | SCP |
| MPPS | 1.2.840.10008.3.1.2.3.3 | SCP |

### DCR-2: Transfer Syntax Support

| Transfer Syntax | UID | Priority | Status |
|-----------------|-----|----------|--------|
| Implicit VR Little Endian | 1.2.840.10008.1.2 | Required | ✅ Implemented |
| Explicit VR Little Endian | 1.2.840.10008.1.2.1 | Required | ✅ Implemented |
| Explicit VR Big Endian | 1.2.840.10008.1.2.2 | Optional | ✅ Implemented |
| JPEG Baseline | 1.2.840.10008.1.2.4.50 | Optional | ✅ Implemented |
| JPEG Lossless | 1.2.840.10008.1.2.4.70 | Optional | ✅ Implemented |
| JPEG 2000 Lossless | 1.2.840.10008.1.2.4.90 | Optional | ✅ Implemented |
| JPEG 2000 | 1.2.840.10008.1.2.4.91 | Optional | ✅ Implemented |
| JPEG-LS Lossless | 1.2.840.10008.1.2.4.80 | Optional | ✅ Implemented |
| JPEG-LS Near-Lossless | 1.2.840.10008.1.2.4.81 | Optional | ✅ Implemented |
| RLE Lossless | 1.2.840.10008.1.2.5 | Optional | ✅ Implemented |

---

## Integration Requirements

### IR-1: Ecosystem Integration

| System | Integration Type | Purpose | Status |
|--------|-----------------|---------|--------|
| **common_system** | Foundation | Result<T>, Error codes, IExecutor | ✅ Integrated |
| **container_system** | Data Layer | DICOM value serialization | ✅ Integrated |
| **thread_system** | Concurrency | Worker pools, async processing, jthread support, cancellation_token | ✅ Fully Migrated (v1.1.0) |
| **logger_system** | Logging | Audit logs, diagnostics | ✅ Integrated |
| **monitoring_system** | Observability | Metrics, health checks | ✅ Integrated |
| **network_system** | Network | TCP/TLS, session management, messaging_server | ✅ V2 Implementation (Optional) |

#### Thread System Migration (Completed 2025-12-07)

The system has been fully migrated from direct `std::thread` usage to `thread_system`:

| Component | Previous | Current | Benefit |
|-----------|----------|---------|---------|
| Accept Loop | `std::thread accept_thread_` | `accept_worker` (inherits `thread_base`) | jthread, cancellation_token |
| Worker Threads | Per-association `std::thread` | `thread_adapter` pool | Unified monitoring, load balancing |
| Shutdown | Manual thread management | Cooperative cancellation | 45x faster graceful shutdown |
| Statistics | None | Unified thread statistics | Full observability |

#### Network System V2 (Optional)

An optional V2 implementation using `network_system::messaging_server` is available:

| Component | Purpose |
|-----------|---------|
| `dicom_server_v2` | Uses `messaging_server` for TCP connection management |
| `dicom_association_handler` | PDU framing, state machine, service dispatching |
| Compile Flag | `PACS_WITH_NETWORK_SYSTEM` |

### IR-2: External System Integration

| System | Protocol | Purpose | Status |
|--------|----------|---------|--------|
| DICOMweb | WADO-RS/STOW-RS/QIDO-RS | Web image access (PS3.18) | ✅ Implemented |
| AI Services | REST/HTTP | External AI inference endpoints | ✅ Implemented |
| HIS/RIS | HL7 FHIR | Patient demographics | Future |

### IR-6: ITK/VTK Integration

| System | Integration Type | Purpose | Phase |
|--------|-----------------|---------|-------|
| **ITK/VTK** | Image Processing | Advanced medical image processing and 3D reconstruction | 5 |

### IR-7: Crow REST Framework Integration

| System | Integration Type | Purpose | Phase | Status |
|--------|-----------------|---------|-------|--------|
| **Crow** | HTTP Framework | REST API server implementation | 4 | ✅ Implemented |

### IR-8: AWS SDK Integration

| System | Integration Type | Purpose | Phase |
|--------|-----------------|---------|-------|
| **AWS SDK for C++** | Cloud Storage | S3 storage backend operations | 4 |

### IR-9: Azure SDK Integration

| System | Integration Type | Purpose | Phase |
|--------|-----------------|---------|-------|
| **Azure SDK for C++** | Cloud Storage | Blob Storage backend operations | 4 |

---

## Security Requirements

### SR-1: Data Protection

| Requirement | Implementation |
|-------------|----------------|
| PHI Encryption | TLS 1.2+ for network, AES for storage |
| Access Control | AE Title whitelisting, user authentication |
| Audit Logging | All DICOM operations logged with timestamps |
| Data Integrity | Checksum validation for stored images |

### SR-2: Compliance

| Standard | Requirements |
|----------|-------------|
| HIPAA | PHI access logging, encryption at rest/transit |
| GDPR | Data subject access rights, audit trail |
| IHE | ATNA (Audit Trail and Node Authentication) |

---

## Performance Requirements

### PR-1: Benchmarks

| Metric | Target | Measured | Status | Test Scenario |
|--------|--------|----------|--------|---------------|
| C-STORE throughput | ≥100 MB/s | 9,247 MB/s | ✅ PASS | 512KB images |
| C-ECHO throughput | ≥100 msg/s | 89,964 msg/s | ✅ PASS | Sustained traffic |
| Association latency | <100 ms | ~1 ms | ✅ PASS | Including negotiation |
| Graceful shutdown | <5,000 ms | 110 ms | ✅ PASS | With active connections |
| Memory per connection | <10 MB | ~5 MB | ✅ PASS | Active association |

*Performance measured after thread_system migration (2025-12-07). See [PERFORMANCE_RESULTS.md](PERFORMANCE_RESULTS.md) for details.*

### PR-2: Scalability Targets

| Metric | Target | Measured | Status |
|--------|--------|----------|--------|
| Concurrent associations | 100+ | 200+ | ✅ Exceeded |
| Concurrent C-ECHO ops | >50 ops/s | 124 ops/s | ✅ Exceeded |
| Concurrent C-STORE ops | >10 ops/s | 49.6 ops/s | ✅ Exceeded |
| Study database size | 1M+ studies | Tested 100K | ✅ On track |
| Daily ingestion | 50K+ images | Architecture validated | ✅ Validated |

---

## Development Phases

### Phase 1: Foundation (Weeks 1-4)

**Objective**: Establish core DICOM data handling

| Deliverable | Description | Acceptance Criteria |
|-------------|-------------|---------------------|
| DICOM Element | Data Element parser/encoder | Parse 100% of standard VRs |
| Data Set | Data Set container | All CRUD operations |
| DICOM File | Part 10 file reader/writer | Read/write DICOM files |
| Tag Dictionary | Standard tag definitions | All PS3.6 tags defined |
| Unit Tests | Core module tests | 80%+ coverage |

**Dependencies**: container_system, common_system

### Phase 2: Network Protocol (Weeks 5-10)

**Objective**: Implement DICOM Upper Layer and basic DIMSE

| Deliverable | Description | Acceptance Criteria |
|-------------|-------------|---------------------|
| PDU Layer | All PDU types | Encode/decode all PDUs |
| Association | State machine, negotiation | Pass association tests |
| C-ECHO | Verification service | Interop with DCMTK |
| C-STORE | Basic storage | Store images from modality |
| Integration Tests | Network tests | All scenarios pass |

**Dependencies**: network_system, thread_system

### Phase 3: Core Services (Weeks 11-16)

**Objective**: Implement Storage and Query/Retrieve

| Deliverable | Description | Acceptance Criteria |
|-------------|-------------|---------------------|
| Storage SCP | Full storage service | Store CT/MR/CR images |
| File Storage | Filesystem backend | Hierarchical storage |
| Index DB | Query index | Fast attribute lookup |
| C-FIND | Query service | Patient/Study/Series/Image |
| C-MOVE | Retrieve service | Image transmission |
| Conformance | Initial testing | Pass basic conformance |

**Dependencies**: All systems

### Phase 4: Advanced Services & Production Hardening (Weeks 17-24)

**Objective**: Add workflow services, web API, AI integration, client module, and production hardening

| Deliverable | Description | Acceptance Criteria | Status |
|-------------|-------------|---------------------|--------|
| Worklist SCP/SCU | MWL service | Query scheduled procedures | ✅ Complete |
| MPPS SCP/SCU | Procedure tracking | Track procedure status | ✅ Complete |
| C-GET | Alternative retrieve | Support C-GET model | ✅ Complete |
| TLS | Secure communication | DICOM TLS conformant | ✅ Complete |
| REST API Server | Crow-based web server | 19 endpoint modules | ✅ Complete |
| DICOMweb | WADO-RS, STOW-RS, QIDO-RS | PS3.18 conformant | ✅ Complete |
| AI Integration | AI service connector + result handler | External AI inference | ✅ Complete |
| Client Module | Job, Routing, Sync, Prefetch, Remote Node | Multi-node federation | ✅ Complete |
| Cloud Storage | S3 + Azure Blob (mock) + HSM | Three-tier storage | ✅ Complete |
| Security | RBAC, Anonymization, Digital Signatures | PS3.15 conformant | ✅ Complete |
| Workflow | Auto Prefetch, Task Scheduler, Study Lock | Automated operations | ✅ Complete |
| Monitoring | DICOM metrics, Object Pool, Health Checks | Production observability | ✅ Complete |
| Annotation/Viewer | Annotation, Viewer State, Key Image, Measurement | Clinical data management | ✅ Complete |

**Dependencies**: All systems

### Phase 5: Enterprise Features (Future)

**Objective**: Production cloud integration, advanced interoperability, and enterprise features

| Deliverable | Description | Acceptance Criteria |
|-------------|-------------|---------------------|
| Full AWS S3 SDK | Production S3 integration | Replace mock implementation |
| Full Azure SDK | Production Azure Blob integration | Replace mock implementation |
| ITK/VTK Integration | Advanced image processing and 3D reconstruction | Image analysis pipeline |
| FHIR Integration | HL7 FHIR interoperability | Healthcare data exchange |
| Clustering | Multi-node PACS deployment | Horizontal scaling |
| Connection Pooling | Reuse DICOM associations | Reduced latency |

**Dependencies**: All systems

---

## Success Metrics

### Technical Metrics

| Metric | Target | Measurement Method |
|--------|--------|-------------------|
| DICOM Conformance | 100% tested SOP classes | Conformance test suite |
| Code Coverage | ≥80% | CI coverage reports |
| CI/CD Success | 100% green | GitHub Actions |
| Sanitizer Clean | Zero issues | TSAN/ASAN/UBSAN |
| API Documentation | 100% public APIs | Doxygen generation |

### Performance Metrics

| Metric | Target | Measurement Method |
|--------|--------|-------------------|
| Storage Throughput | ≥100 MB/s | Benchmark suite |
| Query Latency P95 | <100 ms | Load testing |
| Memory Baseline | <500 MB | Profiling |
| Concurrent Connections | 100+ | Stress testing |

### Quality Metrics

| Metric | Target | Measurement Method |
|--------|--------|-------------------|
| RAII Grade | A | Static analysis |
| Thread Safety | Verified | ThreadSanitizer |
| Memory Safety | Zero leaks | AddressSanitizer |
| Static Analysis | Zero warnings | clang-tidy, cppcheck |

---

## Risks and Mitigations

### Technical Risks

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| DICOM complexity underestimated | High | Medium | MVP approach, incremental delivery |
| Performance targets not met | Medium | Low | Leverage ecosystem optimizations |
| Interoperability issues | High | Medium | Early conformance testing |
| Image compression complexity | Medium | High | Defer to future phase |

### Resource Risks

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| Extended development time | Medium | Medium | Clear phase boundaries |
| Scope creep | Medium | Medium | Strict priority management |
| Testing infrastructure | Low | Low | Reuse ecosystem patterns |

### External Risks

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| DICOM standard updates | Low | Low | Modular design for updates |
| Dependency vulnerabilities | Medium | Low | Regular security scans |

---

## Appendices

### Appendix A: DICOM VR Type Mapping

| VR | Full Name | container_system Type | Size |
|----|-----------|----------------------|------|
| AE | Application Entity | string | 16 max |
| AS | Age String | string | 4 fixed |
| AT | Attribute Tag | uint32 | 4 |
| CS | Code String | string | 16 max |
| DA | Date | string | 8 fixed |
| DS | Decimal String | string → double | 16 max |
| DT | Date Time | string | 26 max |
| FL | Floating Point Single | float | 4 |
| FD | Floating Point Double | double | 8 |
| IS | Integer String | string → int64 | 12 max |
| LO | Long String | string | 64 max |
| LT | Long Text | string | 10240 max |
| OB | Other Byte | bytes | variable |
| OD | Other Double | bytes | variable |
| OF | Other Float | bytes | variable |
| OL | Other Long | bytes | variable |
| OW | Other Word | bytes | variable |
| PN | Person Name | string | 64 max |
| SH | Short String | string | 16 max |
| SL | Signed Long | int32 | 4 |
| SQ | Sequence | container (nested) | variable |
| SS | Signed Short | int16 | 2 |
| ST | Short Text | string | 1024 max |
| TM | Time | string | 14 max |
| UC | Unlimited Characters | string | unlimited |
| UI | Unique Identifier | string | 64 max |
| UL | Unsigned Long | uint32 | 4 |
| UN | Unknown | bytes | variable |
| UR | Universal Resource Identifier | string | unlimited |
| US | Unsigned Short | uint16 | 2 |
| UT | Unlimited Text | string | unlimited |
| OV | Other 64-bit Very Long | bytes | variable |
| SV | Signed 64-bit Very Long | int64 | 8 |
| UV | Unsigned 64-bit Very Long | uint64 | 8 |

### Appendix B: Error Code Registry

```
pacs_system error codes: -700 to -899

DICOM File Errors (-700 to -719):
  -700: INVALID_DICOM_FILE
  -701: MISSING_DICM_PREFIX
  -702: INVALID_META_INFO
  -703: FILE_READ_ERROR
  -704: FILE_WRITE_ERROR

DICOM Element Errors (-720 to -739):
  -720: INVALID_DATA_ELEMENT
  -721: ELEMENT_NOT_FOUND
  -722: INVALID_VALUE_CONVERSION
  -723: MISSING_REQUIRED_TAG

Encoding Errors (-740 to -759):
  -740: INVALID_VR
  -741: INVALID_TRANSFER_SYNTAX
  -742: SEQUENCE_ENCODING_ERROR
  -743: COMPRESSION_ERROR
  -744: DECOMPRESSION_ERROR

Network Errors (-760 to -779):
  -760: ASSOCIATION_REJECTED
  -761: ASSOCIATION_ABORTED
  -762: NO_PRESENTATION_CONTEXT
  -763: INVALID_PDU
  -764: PDU_TOO_LARGE
  -765: ASSOCIATION_TIMEOUT
  -766: DIMSE_FAILURE
  -767: DIMSE_TIMEOUT
  -768: DIMSE_INVALID_RESPONSE
  -769: DIMSE_CANCELLED

Storage Errors (-780 to -799):
  -780: STORAGE_FAILED
  -781: DUPLICATE_SOP_INSTANCE
  -782: INVALID_SOP_CLASS
  -783: STORAGE_FULL
  -784: QUERY_FAILED
  -785: NO_MATCHES_FOUND
  -786: TOO_MANY_MATCHES
  -787: INVALID_QUERY_LEVEL
  -788: DATABASE_ERROR

Service Errors (-800 to -899):
  -800: C-STORE service errors (-800 to -819)
  -820: C-FIND service errors (-820 to -839)
  -840: C-MOVE/C-GET service errors (-840 to -859)
  -860: Verification service errors (-860 to -869)
  -870: MPPS service errors (-870 to -879)
  -880: Worklist service errors (-880 to -889)
  -890: General service errors (-890 to -899)
```

### Appendix C: References

- DICOM Standard: https://www.dicomstandard.org/
- PS3.5 - Data Structures and Encoding
- PS3.6 - Data Dictionary
- PS3.7 - Message Exchange
- PS3.8 - Network Communication Support
- PS3.4 - Service Class Specifications

---

## Document History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0.0 | 2025-11-30 | kcenon@naver.com | Initial PRD release |
| 1.1.0 | 2025-12-07 | kcenon@naver.com | Updated for completed features: Thread system migration (Epic #153), DIMSE-N services (#127), Ultrasound/XA storage (#128, #129), Explicit VR Big Endian (#126), Performance results |
| 2.0.0 | 2026-02-08 | raphaelshin | Major update: Added 15+ implemented features to PRD. New sections: FR-10 (Client Module), FR-11 (Annotation/Viewer State/Clinical Data), FR-12 (Monitoring/Observability). Updated status for FR-3.4 (Worklist), FR-3.5 (MPPS), FR-5 (Security), FR-6 (Web API - 19 endpoints), FR-7 (Workflow - 3 services), FR-8 (Cloud Storage + HSM), FR-9 (AI Integration). Updated architecture diagram and development phases. Reclassified Phase 5 items that are already implemented to Phase 4. |

---

*Document Version: 0.2.0.0*
*Created: 2025-11-30*
*Updated: 2026-02-08*
*Author: kcenon@naver.com*
