# Software Requirements Specification - PACS System

> **Version:** 1.0.0
> **Last Updated:** 2025-11-30
> **Language:** **English** | [한국어](SRS_KO.md)
> **Standard:** IEEE 830-1998 based

---

## Document Information

| Item | Description |
|------|-------------|
| **Document ID** | PACS-SRS-001 |
| **Project** | PACS System |
| **Author** | kcenon@naver.com |
| **Status** | Draft |
| **Related Documents** | [PRD](PRD.md), [ARCHITECTURE](ARCHITECTURE.md), [API_REFERENCE](API_REFERENCE.md) |

---

## Table of Contents

- [1. Introduction](#1-introduction)
- [2. Overall Description](#2-overall-description)
- [3. Specific Requirements](#3-specific-requirements)
- [4. External Interface Requirements](#4-external-interface-requirements)
- [5. System Features](#5-system-features)
- [6. Non-Functional Requirements](#6-non-functional-requirements)
- [7. Requirements Traceability Matrix](#7-requirements-traceability-matrix)
- [8. Appendices](#8-appendices)

---

## 1. Introduction

### 1.1 Purpose

This Software Requirements Specification (SRS) document provides a complete description of all software requirements for the PACS System. It establishes the basis for agreement between stakeholders and the development team on what the software product will do.

### 1.2 Scope

The PACS System is a medical imaging storage and communication platform that:
- Implements DICOM protocol from scratch without external DICOM libraries
- Provides Storage, Query/Retrieve, Worklist, and MPPS services
- Integrates with the kcenon ecosystem (6 existing systems)
- Operates on Linux, macOS, and Windows platforms

### 1.3 Definitions, Acronyms, and Abbreviations

| Term | Definition |
|------|------------|
| **PACS** | Picture Archiving and Communication System |
| **DICOM** | Digital Imaging and Communications in Medicine |
| **SCP** | Service Class Provider (server role) |
| **SCU** | Service Class User (client role) |
| **PDU** | Protocol Data Unit |
| **DIMSE** | DICOM Message Service Element |
| **VR** | Value Representation |
| **SOP** | Service-Object Pair |
| **IOD** | Information Object Definition |
| **MWL** | Modality Worklist |
| **MPPS** | Modality Performed Procedure Step |
| **AE** | Application Entity |

### 1.4 References

| Reference | Description |
|-----------|-------------|
| DICOM PS3.1-PS3.20 | DICOM Standard 2024 |
| IEEE 830-1998 | IEEE Standard for SRS |
| PRD-PACS-001 | Product Requirements Document |
| ARCH-PACS-001 | Architecture Document |

### 1.5 Overview

This document is organized as follows:
- **Section 2**: Overall system description and constraints
- **Section 3**: Detailed functional requirements with traceability
- **Section 4**: External interface requirements
- **Section 5**: System feature specifications
- **Section 6**: Non-functional requirements
- **Section 7**: Requirements traceability matrix
- **Section 8**: Appendices

---

## 2. Overall Description

### 2.1 Product Perspective

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           PACS Ecosystem                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│    ┌──────────────┐    ┌──────────────┐    ┌──────────────┐             │
│    │   Modality   │    │   Modality   │    │   Modality   │             │
│    │  (CT/MR/CR)  │    │  (CT/MR/CR)  │    │  (CT/MR/CR)  │             │
│    └───────┬──────┘    └───────┬──────┘    └───────┬──────┘             │
│            │                   │                   │                     │
│            └───────────────────┼───────────────────┘                     │
│                                │ DICOM                                   │
│                                ▼                                         │
│    ┌─────────────────────────────────────────────────────────────────┐  │
│    │                        PACS System                               │  │
│    │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐   │  │
│    │  │Storage  │ │ Q/R     │ │Worklist │ │  MPPS   │ │Verify   │   │  │
│    │  │SCP/SCU  │ │SCP/SCU  │ │  SCP    │ │  SCP    │ │SCP/SCU  │   │  │
│    │  └────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘   │  │
│    │       └───────────┴───────────┴───────────┴───────────┘         │  │
│    │                              │                                   │  │
│    │  ┌───────────────────────────▼──────────────────────────────┐   │  │
│    │  │                    Storage Backend                        │   │  │
│    │  │    File Storage    │    Index Database    │    Cache     │   │  │
│    │  └──────────────────────────────────────────────────────────┘   │  │
│    └─────────────────────────────────────────────────────────────────┘  │
│                                │                                         │
│            ┌───────────────────┼───────────────────┐                     │
│            ▼                   ▼                   ▼                     │
│    ┌──────────────┐    ┌──────────────┐    ┌──────────────┐             │
│    │    Viewer    │    │   HIS/RIS    │    │   Archive    │             │
│    └──────────────┘    └──────────────┘    └──────────────┘             │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Product Functions

| Function | Description |
|----------|-------------|
| **DICOM Storage** | Receive and store medical images from modalities |
| **Image Query** | Search for patients, studies, series, and images |
| **Image Retrieve** | Transfer images to requesting systems |
| **Modality Worklist** | Provide scheduled procedure information |
| **MPPS Tracking** | Track performed procedure status |
| **Verification** | Test DICOM connectivity |

### 2.3 User Classes and Characteristics

| User Class | Characteristics | Technical Level |
|------------|-----------------|-----------------|
| Healthcare IT Developer | Builds applications using PACS API | Expert |
| System Integrator | Connects modalities to PACS | Advanced |
| System Administrator | Manages and monitors PACS | Intermediate |
| Radiologist | Views images via PACS viewer | Basic |

### 2.4 Operating Environment

| Component | Requirement |
|-----------|-------------|
| **Operating System** | Linux (Ubuntu 22.04+), macOS 14+, Windows 10/11 |
| **Compiler** | C++20 (GCC 11+, Clang 14+, MSVC 2022+) |
| **Memory** | Minimum 4 GB, Recommended 16 GB |
| **Storage** | Minimum 100 GB, Scalable |
| **Network** | 1 Gbps Ethernet minimum |

### 2.5 Design and Implementation Constraints

| Constraint | Description |
|------------|-------------|
| **C1** | No external DICOM libraries (DCMTK, GDCM, etc.) |
| **C2** | Must use kcenon ecosystem components |
| **C3** | Cross-platform compatibility required |
| **C4** | DICOM conformance required for supported SOP classes |
| **C5** | BSD 3-Clause license compatibility |

### 2.6 Assumptions and Dependencies

| ID | Assumption/Dependency |
|----|----------------------|
| **A1** | container_system provides stable binary serialization |
| **A2** | network_system TCP layer handles connection management |
| **A3** | thread_system provides lock-free queue implementation |
| **A4** | DICOM standard PS3.x specifications are stable |
| **D1** | common_system v1.0+ available |
| **D2** | container_system v1.0+ available |
| **D3** | network_system v1.0+ available |
| **D4** | thread_system v1.0+ available |
| **D5** | logger_system v1.0+ available |
| **D6** | monitoring_system v1.0+ available |

---

## 3. Specific Requirements

### 3.1 DICOM Core Module Requirements

#### SRS-CORE-001: DICOM Tag Structure
| Attribute | Value |
|-----------|-------|
| **ID** | SRS-CORE-001 |
| **Title** | DICOM Tag Representation |
| **Description** | The system shall represent DICOM tags as a 32-bit value composed of 16-bit group number and 16-bit element number. |
| **Priority** | Must Have |
| **Phase** | 1 |
| **Traces To** | FR-1.1.1 |

**Acceptance Criteria:**
1. Tag structure shall be memory-efficient (4 bytes)
2. Group and element extraction shall be O(1)
3. Tag comparison shall support standard ordering

**Implementation Notes:**
```cpp
struct dicom_tag {
    uint16_t group;
    uint16_t element;
    constexpr uint32_t to_uint32() const;
    constexpr bool operator<(const dicom_tag& other) const;
};
```

---

#### SRS-CORE-002: Value Representation Types
| Attribute | Value |
|-----------|-------|
| **ID** | SRS-CORE-002 |
| **Title** | VR Type System |
| **Description** | The system shall implement all 27 DICOM Value Representation types as defined in PS3.5. |
| **Priority** | Must Have |
| **Phase** | 1 |
| **Traces To** | FR-1.1.2 |

**Acceptance Criteria:**
1. All 27 VR types implemented
2. VR validation for value content
3. VR-specific length constraints enforced

**VR Type Mapping to container_system:**

| VR | Category | container_system Type | Validation |
|----|----------|----------------------|------------|
| AE | String | string | Max 16 chars, no backslash |
| AS | String | string | 4 chars, format nnnD/W/M/Y |
| AT | Binary | uint32 | Tag format |
| CS | String | string | Max 16 chars, uppercase |
| DA | String | string | YYYYMMDD format |
| DS | String | string | Decimal format, max 16 chars |
| DT | String | string | YYYYMMDDHHMMSS.FFFFFF format |
| FL | Numeric | float | 4 bytes IEEE 754 |
| FD | Numeric | double | 8 bytes IEEE 754 |
| IS | String | string | Integer format, max 12 chars |
| LO | String | string | Max 64 chars |
| LT | String | string | Max 10240 chars |
| OB | Binary | bytes | Variable length |
| OD | Binary | bytes | Variable length, 8-byte aligned |
| OF | Binary | bytes | Variable length, 4-byte aligned |
| OL | Binary | bytes | Variable length, 4-byte aligned |
| OW | Binary | bytes | Variable length, 2-byte aligned |
| PN | String | string | Max 64 chars, component groups |
| SH | String | string | Max 16 chars |
| SL | Numeric | int32 | 4 bytes signed |
| SQ | Sequence | container | Nested structure |
| SS | Numeric | int16 | 2 bytes signed |
| ST | String | string | Max 1024 chars |
| TM | String | string | HHMMSS.FFFFFF format |
| UC | String | string | Unlimited length |
| UI | String | string | UID format, max 64 chars |
| UL | Numeric | uint32 | 4 bytes unsigned |
| UN | Binary | bytes | Unknown VR |
| UR | String | string | URI format |
| US | Numeric | uint16 | 2 bytes unsigned |
| UT | String | string | Unlimited length |

---

#### SRS-CORE-003: Data Element Structure
| Attribute | Value |
|-----------|-------|
| **ID** | SRS-CORE-003 |
| **Title** | DICOM Data Element |
| **Description** | The system shall represent Data Elements with tag, VR, length, and value components supporting both explicit and implicit VR encoding. |
| **Priority** | Must Have |
| **Phase** | 1 |
| **Traces To** | FR-1.1.1, FR-1.3.3, FR-1.3.4 |

**Acceptance Criteria:**
1. Support Explicit VR encoding with 2-byte or 4-byte length fields
2. Support Implicit VR encoding with 4-byte length fields
3. Handle undefined length encoding for sequences
4. Support value multiplicity (VM)

**Implementation Notes:**
```cpp
class dicom_element {
public:
    dicom_tag tag() const;
    vr_type vr() const;
    uint32_t length() const;

    template<typename T>
    Result<T> get_value() const;

    template<typename T>
    Result<void> set_value(T value);

    bool is_sequence() const;
    Result<dicom_dataset> get_sequence_item(size_t index) const;
};
```

---

#### SRS-CORE-004: Sequence Element Handling
| Attribute | Value |
|-----------|-------|
| **ID** | SRS-CORE-004 |
| **Title** | Nested Sequence Support |
| **Description** | The system shall support nested Sequence (SQ) elements with unlimited depth, including undefined length sequences with delimitation items. |
| **Priority** | Must Have |
| **Phase** | 1 |
| **Traces To** | FR-1.1.3 |

**Acceptance Criteria:**
1. Parse sequences with defined length
2. Parse sequences with undefined length (0xFFFFFFFF)
3. Handle Item and Item Delimitation Tags correctly
4. Support nested sequences (SQ within SQ)

**Structure:**
```
Sequence (SQ)
├── Item (FFFE,E000)
│   ├── Data Element
│   ├── Data Element
│   └── Nested Sequence (SQ)
│       └── Item (FFFE,E000)
│           └── Data Element
├── Item (FFFE,E000)
│   └── Data Element
└── Sequence Delimitation Item (FFFE,E0DD) [if undefined length]
```

---

#### SRS-CORE-005: Data Set Container
| Attribute | Value |
|-----------|-------|
| **ID** | SRS-CORE-005 |
| **Title** | DICOM Data Set Operations |
| **Description** | The system shall provide a Data Set container that maintains elements in tag order and supports efficient lookup, insertion, and iteration. |
| **Priority** | Must Have |
| **Phase** | 1 |
| **Traces To** | FR-1.2.1, FR-1.2.2, FR-1.2.3, FR-1.2.4 |

**Acceptance Criteria:**
1. Elements stored in ascending tag order
2. O(log n) element lookup by tag
3. O(n) iteration in tag order
4. Deep copy and move semantics
5. Element insertion maintains order

**Required Operations:**

| Operation | Complexity | Description |
|-----------|------------|-------------|
| `get(tag)` | O(log n) | Get element by tag |
| `set(element)` | O(log n) | Insert or update element |
| `remove(tag)` | O(log n) | Remove element by tag |
| `contains(tag)` | O(log n) | Check element existence |
| `iterate()` | O(n) | Iterate in tag order |
| `size()` | O(1) | Element count |
| `clear()` | O(n) | Remove all elements |
| `copy()` | O(n) | Deep copy |
| `move()` | O(1) | Move semantics |

---

#### SRS-CORE-006: DICOM File Format (Part 10)
| Attribute | Value |
|-----------|-------|
| **ID** | SRS-CORE-006 |
| **Title** | DICOM File Read/Write |
| **Description** | The system shall read and write DICOM files conforming to PS3.10, including 128-byte preamble, "DICM" prefix, and File Meta Information. |
| **Priority** | Must Have |
| **Phase** | 1 |
| **Traces To** | FR-1.3.1, FR-1.3.2 |

**Acceptance Criteria:**
1. Read files with valid DICM prefix
2. Parse File Meta Information (Group 0002)
3. Detect and validate Transfer Syntax
4. Write files with proper File Meta Information
5. Support streaming for large files

**File Structure:**
```
┌────────────────────────────────────────┐
│ Preamble (128 bytes, typically 0x00)   │
├────────────────────────────────────────┤
│ DICOM Prefix "DICM" (4 bytes)          │
├────────────────────────────────────────┤
│ File Meta Information (Group 0002)     │
│ - (0002,0000) File Meta Length         │
│ - (0002,0001) File Meta Version        │
│ - (0002,0002) Media Storage SOP Class  │
│ - (0002,0003) Media Storage SOP Inst   │
│ - (0002,0010) Transfer Syntax UID      │
│ - (0002,0012) Implementation Class UID │
├────────────────────────────────────────┤
│ Data Set (encoded per Transfer Syntax) │
└────────────────────────────────────────┘
```

---

#### SRS-CORE-007: Transfer Syntax Encoding
| Attribute | Value |
|-----------|-------|
| **ID** | SRS-CORE-007 |
| **Title** | Transfer Syntax Support |
| **Description** | The system shall support encoding and decoding data according to Implicit VR Little Endian, Explicit VR Little Endian, and Explicit VR Big Endian transfer syntaxes. |
| **Priority** | Must Have |
| **Phase** | 1 |
| **Traces To** | FR-1.3.3, FR-1.3.4, FR-1.3.5, FR-1.3.6 |

**Acceptance Criteria:**
1. Detect transfer syntax from File Meta Information
2. Encode/decode Implicit VR Little Endian (1.2.840.10008.1.2)
3. Encode/decode Explicit VR Little Endian (1.2.840.10008.1.2.1)
4. Encode/decode Explicit VR Big Endian (1.2.840.10008.1.2.2)
5. Validate VR length field sizes

**Encoding Differences:**

| Transfer Syntax | VR Field | Length Field |
|-----------------|----------|--------------|
| Implicit VR LE | None (from dictionary) | 4 bytes |
| Explicit VR LE | 2 bytes | 2 or 4 bytes |
| Explicit VR BE | 2 bytes | 2 or 4 bytes |

---

#### SRS-CORE-008: Tag Dictionary
| Attribute | Value |
|-----------|-------|
| **ID** | SRS-CORE-008 |
| **Title** | DICOM Tag Dictionary |
| **Description** | The system shall provide a compile-time tag dictionary with all standard DICOM tags from PS3.6, including keyword, VR, VM, and retirement status. |
| **Priority** | Must Have |
| **Phase** | 1 |
| **Traces To** | FR-1.1.2 |

**Acceptance Criteria:**
1. All standard tags from PS3.6 defined
2. Compile-time constant tags
3. Tag-to-keyword lookup
4. Keyword-to-tag lookup
5. VR validation support

**Tag Entry Structure:**
```cpp
struct tag_entry {
    dicom_tag tag;
    const char* keyword;
    vr_type vr;
    const char* vm;  // "1", "1-n", "2-2n", etc.
    bool retired;
};
```

---

### 3.2 Network Protocol Requirements

#### SRS-NET-001: PDU Type Support
| Attribute | Value |
|-----------|-------|
| **ID** | SRS-NET-001 |
| **Title** | Protocol Data Unit Implementation |
| **Description** | The system shall implement all DICOM Upper Layer PDU types for association establishment, data transfer, and release. |
| **Priority** | Must Have |
| **Phase** | 2 |
| **Traces To** | FR-2.1.1 - FR-2.1.7 |

**Acceptance Criteria:**
1. Implement A-ASSOCIATE-RQ (0x01) encoding/decoding
2. Implement A-ASSOCIATE-AC (0x02) encoding/decoding
3. Implement A-ASSOCIATE-RJ (0x03) encoding/decoding
4. Implement P-DATA-TF (0x04) encoding/decoding
5. Implement A-RELEASE-RQ (0x05) encoding/decoding
6. Implement A-RELEASE-RP (0x06) encoding/decoding
7. Implement A-ABORT (0x07) encoding/decoding
8. Handle PDU fragment reassembly

**PDU Structure:**

| PDU Type | Code | Fields |
|----------|------|--------|
| A-ASSOCIATE-RQ | 0x01 | Protocol Version, Called AE, Calling AE, Variable Items |
| A-ASSOCIATE-AC | 0x02 | Protocol Version, Called AE, Calling AE, Variable Items |
| A-ASSOCIATE-RJ | 0x03 | Result, Source, Reason/Diag |
| P-DATA-TF | 0x04 | Presentation Data Value Items |
| A-RELEASE-RQ | 0x05 | (empty) |
| A-RELEASE-RP | 0x06 | (empty) |
| A-ABORT | 0x07 | Source, Reason/Diag |

---

#### SRS-NET-002: Association State Machine
| Attribute | Value |
|-----------|-------|
| **ID** | SRS-NET-002 |
| **Title** | Association State Management |
| **Description** | The system shall implement the DICOM Association State Machine with all 13 states and valid state transitions as defined in PS3.8. |
| **Priority** | Must Have |
| **Phase** | 2 |
| **Traces To** | FR-2.2.1 |

**Acceptance Criteria:**
1. Implement all 13 states
2. Validate state transitions
3. Handle timeout in each state
4. Log state changes for debugging

**State Machine:**

```
┌──────────────────────────────────────────────────────────────────┐
│                    Association State Machine                      │
├──────────────────────────────────────────────────────────────────┤
│                                                                   │
│    ┌─────────────┐                                               │
│    │  Sta1:Idle  │                                               │
│    └──────┬──────┘                                               │
│           │ A-ASSOCIATE req                                       │
│           ▼                                                       │
│    ┌─────────────────┐    ┌─────────────────┐                    │
│    │Sta4:Awaiting    │    │Sta2:Transport   │                    │
│    │Transport Conn   │    │Connection Open  │◄─── Incoming       │
│    └───────┬─────────┘    └────────┬────────┘                    │
│            │ Conn confirmed         │ A-ASSOCIATE-RQ recv         │
│            ▼                        ▼                             │
│    ┌─────────────────┐    ┌─────────────────┐                    │
│    │Sta5:Awaiting    │    │Sta3:Awaiting    │                    │
│    │A-ASSOCIATE-AC   │    │Local A-ASSOC rsp│                    │
│    └───────┬─────────┘    └────────┬────────┘                    │
│            │ A-ASSOCIATE-AC         │ Accept                      │
│            ▼                        ▼                             │
│    ┌─────────────────────────────────────────┐                   │
│    │            Sta6:Association             │                   │
│    │               Established               │◄───────┐          │
│    └─────────────────────┬───────────────────┘        │          │
│                          │ P-DATA                      │          │
│                          ▼                             │          │
│    ┌─────────────────────────────────────────┐        │          │
│    │            Data Transfer                 │────────┘          │
│    └─────────────────────┬───────────────────┘                   │
│                          │ A-RELEASE-RQ                           │
│                          ▼                                        │
│    ┌─────────────────────────────────────────┐                   │
│    │    Sta7/Sta8:Awaiting Release           │                   │
│    └─────────────────────┬───────────────────┘                   │
│                          │ A-RELEASE-RP                           │
│                          ▼                                        │
│    ┌─────────────────────────────────────────┐                   │
│    │            Sta13:Association            │                   │
│    │              Released/Closed            │                   │
│    └─────────────────────────────────────────┘                   │
│                                                                   │
└──────────────────────────────────────────────────────────────────┘
```

**States:**

| State | Name | Description |
|-------|------|-------------|
| Sta1 | Idle | No association, no transport connection |
| Sta2 | Transport Connection Open (Await A-ASSOC-RQ) | Incoming connection |
| Sta3 | Await local A-ASSOCIATE response | Processing association request |
| Sta4 | Await transport connection | Outgoing connection in progress |
| Sta5 | Await A-ASSOCIATE-AC or A-ASSOCIATE-RJ | Outgoing association pending |
| Sta6 | Association established | Ready for DIMSE |
| Sta7 | Await A-RELEASE-RP | Release requested |
| Sta8 | Await A-RELEASE-RP (collision) | Release collision |
| Sta9 | Release collision requestor side | Special case |
| Sta10 | Release collision acceptor side | Special case |
| Sta11 | Release collision requestor side | Special case |
| Sta12 | Release collision acceptor side | Special case |
| Sta13 | Await transport close | Cleanup pending |

---

#### SRS-NET-003: Presentation Context Negotiation
| Attribute | Value |
|-----------|-------|
| **ID** | SRS-NET-003 |
| **Title** | Presentation Context Management |
| **Description** | The system shall negotiate presentation contexts supporting multiple abstract syntaxes and transfer syntaxes per association. |
| **Priority** | Must Have |
| **Phase** | 2 |
| **Traces To** | FR-2.2.2 |

**Acceptance Criteria:**
1. Support up to 128 presentation contexts per association
2. Negotiate transfer syntax for each abstract syntax
3. Track accepted/rejected contexts
4. Select appropriate context for each DIMSE operation

**Negotiation Process:**
```
SCU                                    SCP
 │                                      │
 │  A-ASSOCIATE-RQ                      │
 │  ┌─────────────────────────────────┐ │
 │  │ Presentation Context 1          │ │
 │  │   Abstract Syntax: CT Storage   │ │
 │  │   Transfer Syntaxes:            │ │
 │  │     - Explicit VR LE            │ │
 │  │     - Implicit VR LE            │ │
 │  ├─────────────────────────────────┤ │
 │  │ Presentation Context 2          │ │
 │  │   Abstract Syntax: MR Storage   │ │
 │  │   Transfer Syntaxes:            │ │
 │  │     - Explicit VR LE            │ │
 │  └─────────────────────────────────┘ │
 │ ──────────────────────────────────────►
 │                                      │
 │                   A-ASSOCIATE-AC     │
 │  ┌─────────────────────────────────┐ │
 │  │ Presentation Context 1          │ │
 │  │   Result: Acceptance            │ │
 │  │   Transfer Syntax: Explicit LE  │ │
 │  ├─────────────────────────────────┤ │
 │  │ Presentation Context 2          │ │
 │  │   Result: Acceptance            │ │
 │  │   Transfer Syntax: Explicit LE  │ │
 │  └─────────────────────────────────┘ │
 │ ◄────────────────────────────────────
 │                                      │
```

---

#### SRS-NET-004: DIMSE Message Processing
| Attribute | Value |
|-----------|-------|
| **ID** | SRS-NET-004 |
| **Title** | DIMSE Command and Data Processing |
| **Description** | The system shall encode and decode DIMSE command and data sets, supporting all C-xxx and N-xxx message types. |
| **Priority** | Must Have |
| **Phase** | 2 |
| **Traces To** | FR-2.3.1 - FR-2.3.9 |

**Acceptance Criteria:**
1. Encode DIMSE commands with command elements
2. Decode DIMSE commands and extract fields
3. Handle command/data fragmentation
4. Support pending responses for C-FIND/C-MOVE

**DIMSE Message Structure:**

| Command | Command Field | Data Set |
|---------|---------------|----------|
| C-ECHO-RQ | 0x0030 | None |
| C-ECHO-RSP | 0x8030 | None |
| C-STORE-RQ | 0x0001 | Composite Object |
| C-STORE-RSP | 0x8001 | None |
| C-FIND-RQ | 0x0020 | Identifier |
| C-FIND-RSP | 0x8020 | Matching Dataset (pending) or None (final) |
| C-MOVE-RQ | 0x0021 | Identifier |
| C-MOVE-RSP | 0x8021 | None |
| C-GET-RQ | 0x0010 | Identifier |
| C-GET-RSP | 0x8010 | None |

---

#### SRS-NET-005: Concurrent Association Support
| Attribute | Value |
|-----------|-------|
| **ID** | SRS-NET-005 |
| **Title** | Multiple Simultaneous Associations |
| **Description** | The system shall support multiple concurrent associations with configurable limits, using thread pool for processing. |
| **Priority** | Must Have |
| **Phase** | 2 |
| **Traces To** | FR-2.2.3, NFR-1.2 |

**Acceptance Criteria:**
1. Support 100+ concurrent associations
2. Configurable maximum association limit
3. Independent state machine per association
4. Fair resource allocation

**Implementation Notes:**
- Use thread_system worker pool for DIMSE processing
- Lock-free queue for job distribution
- Per-association memory limit enforcement

---

### 3.3 DICOM Service Requirements

#### SRS-SVC-001: Verification Service
| Attribute | Value |
|-----------|-------|
| **ID** | SRS-SVC-001 |
| **Title** | C-ECHO Implementation |
| **Description** | The system shall implement Verification SCP and SCU roles for connection testing using C-ECHO. |
| **Priority** | Must Have |
| **Phase** | 2 |
| **Traces To** | FR-3.1.1, FR-3.1.2, FR-3.1.3 |

**Acceptance Criteria:**
1. SCP responds to C-ECHO-RQ with C-ECHO-RSP (status 0x0000)
2. SCU initiates C-ECHO and validates response
3. C-ECHO works in all association states where allowed

**Message Flow:**
```
SCU                              SCP
 │                                │
 │  C-ECHO-RQ                     │
 │  (0000,0100) CommandField=0030 │
 │  (0000,0110) MessageID=1       │
 │  (0000,0800) DataSetType=0101  │
 │ ────────────────────────────────►
 │                                │
 │  C-ECHO-RSP                    │
 │  (0000,0100) CommandField=8030 │
 │  (0000,0120) MessageIDRsp=1    │
 │  (0000,0800) DataSetType=0101  │
 │  (0000,0900) Status=0000       │
 │ ◄────────────────────────────────
 │                                │
```

---

#### SRS-SVC-002: Storage SCP
| Attribute | Value |
|-----------|-------|
| **ID** | SRS-SVC-002 |
| **Title** | Storage Service Class Provider |
| **Description** | The system shall accept and store DICOM composite objects via C-STORE, supporting CT, MR, CR, DX, and Secondary Capture SOP classes. |
| **Priority** | Must Have |
| **Phase** | 3 |
| **Traces To** | FR-3.2.1 - FR-3.2.4 |

**Acceptance Criteria:**
1. Accept C-STORE-RQ for supported SOP classes
2. Validate received dataset
3. Store to configured storage backend
4. Return appropriate status codes
5. Handle duplicate SOP Instance detection

**Status Codes:**

| Status | Code | Description |
|--------|------|-------------|
| Success | 0x0000 | Storage completed |
| Warning - Coercion | 0xB000 | Data modified |
| Warning - Data Set Mismatch | 0xB007 | Elements missing |
| Failure - Out of Resources | 0xA700 | Storage full |
| Failure - SOP Class Not Supported | 0x0122 | Unknown SOP class |
| Failure - Cannot Understand | 0xC000 | Invalid dataset |

---

#### SRS-SVC-003: Storage SCU
| Attribute | Value |
|-----------|-------|
| **ID** | SRS-SVC-003 |
| **Title** | Storage Service Class User |
| **Description** | The system shall transmit DICOM composite objects to remote SCP via C-STORE with progress tracking and retry support. |
| **Priority** | Must Have |
| **Phase** | 3 |
| **Traces To** | FR-3.2.5 |

**Acceptance Criteria:**
1. Initiate C-STORE-RQ with proper presentation context
2. Fragment large datasets per PDU size limit
3. Handle C-STORE-RSP status codes
4. Support cancellation during transfer
5. Provide progress callback

---

#### SRS-SVC-004: Query Service (C-FIND)
| Attribute | Value |
|-----------|-------|
| **ID** | SRS-SVC-004 |
| **Title** | Query/Retrieve Find SCP/SCU |
| **Description** | The system shall support Patient Root and Study Root Query/Retrieve information models at Patient, Study, Series, and Image levels. |
| **Priority** | Must Have |
| **Phase** | 3 |
| **Traces To** | FR-3.3.1 - FR-3.3.6 |

**Acceptance Criteria:**
1. Support wildcard matching (* and ?)
2. Support range matching for dates
3. Support case-insensitive matching where appropriate
4. Return matching results as pending responses
5. Return final response with status

**Query Levels:**

| Level | Unique Key | Return Keys |
|-------|-----------|-------------|
| PATIENT | PatientID | PatientName, PatientBirthDate, etc. |
| STUDY | StudyInstanceUID | StudyDate, AccessionNumber, etc. |
| SERIES | SeriesInstanceUID | Modality, SeriesNumber, etc. |
| IMAGE | SOPInstanceUID | InstanceNumber, etc. |

---

#### SRS-SVC-005: Retrieve Service (C-MOVE/C-GET)
| Attribute | Value |
|-----------|-------|
| **ID** | SRS-SVC-005 |
| **Title** | Query/Retrieve Move/Get SCP/SCU |
| **Description** | The system shall support image retrieval via C-MOVE (to third party) and C-GET (direct to requestor) operations. |
| **Priority** | Must Have |
| **Phase** | 3 |
| **Traces To** | FR-3.3.7, FR-3.3.8 |

**Acceptance Criteria:**
1. Parse C-MOVE-RQ with Move Destination AE
2. Establish sub-association to destination for C-MOVE
3. Handle C-GET on same association
4. Track and report progress (remaining, completed, failed, warning)
5. Support cancellation

**C-MOVE Flow:**
```
SCU                    SCP                    Destination
 │                      │                           │
 │  C-MOVE-RQ           │                           │
 │  Dest AE: "VIEWER"   │                           │
 │ ──────────────────────►                           │
 │                      │                           │
 │                      │  A-ASSOCIATE-RQ           │
 │                      │ ──────────────────────────►
 │                      │                           │
 │                      │  A-ASSOCIATE-AC           │
 │                      │ ◄──────────────────────────
 │                      │                           │
 │                      │  C-STORE-RQ (Image 1)     │
 │                      │ ──────────────────────────►
 │                      │                           │
 │  C-MOVE-RSP (Pending)│  C-STORE-RSP              │
 │  Remaining: 9        │ ◄──────────────────────────
 │ ◄──────────────────────                           │
 │                      │                           │
 │        ...           │         ...               │
 │                      │                           │
 │  C-MOVE-RSP (Final)  │                           │
 │  Completed: 10       │                           │
 │ ◄──────────────────────                           │
 │                      │                           │
```

---

#### SRS-SVC-006: Modality Worklist SCP
| Attribute | Value |
|-----------|-------|
| **ID** | SRS-SVC-006 |
| **Title** | Modality Worklist Service |
| **Description** | The system shall provide scheduled procedure step information to modalities via C-FIND on the Modality Worklist Information Model. |
| **Priority** | Should Have |
| **Phase** | 4 |
| **Traces To** | FR-3.4.1 - FR-3.4.3 |

**Acceptance Criteria:**
1. Support Scheduled Procedure Step matching
2. Return Patient, Visit, and Procedure information
3. Support date/time range queries
4. Support Modality filtering

**MWL Return Keys:**

| Module | Key Attributes |
|--------|---------------|
| Patient | PatientName, PatientID, PatientBirthDate, PatientSex |
| Visit | AdmissionID, CurrentPatientLocation |
| Imaging Service Request | AccessionNumber, RequestingPhysician |
| Scheduled Procedure Step | ScheduledStationAETitle, ScheduledProcedureStepStartDate/Time, Modality |
| Requested Procedure | RequestedProcedureID, RequestedProcedureDescription |

---

#### SRS-SVC-007: MPPS SCP
| Attribute | Value |
|-----------|-------|
| **ID** | SRS-SVC-007 |
| **Title** | Modality Performed Procedure Step Service |
| **Description** | The system shall track performed procedure status via N-CREATE and N-SET operations on MPPS SOP instances. |
| **Priority** | Should Have |
| **Phase** | 4 |
| **Traces To** | FR-3.5.1 - FR-3.5.4 |

**Acceptance Criteria:**
1. Accept N-CREATE with MPPS initial state (IN PROGRESS)
2. Accept N-SET to update MPPS state
3. Validate state transitions (IN PROGRESS → COMPLETED/DISCONTINUED)
4. Store MPPS information persistently

**MPPS State Machine:**
```
                    N-CREATE
                        │
                        ▼
              ┌─────────────────┐
              │   IN PROGRESS   │
              └────────┬────────┘
                       │
        ┌──────────────┴──────────────┐
        │ N-SET (COMPLETED)           │ N-SET (DISCONTINUED)
        ▼                             ▼
┌───────────────┐            ┌────────────────┐
│   COMPLETED   │            │  DISCONTINUED  │
└───────────────┘            └────────────────┘
```

---

### 3.4 Storage Backend Requirements

#### SRS-STOR-001: File System Storage
| Attribute | Value |
|-----------|-------|
| **ID** | SRS-STOR-001 |
| **Title** | Hierarchical File Storage |
| **Description** | The system shall store DICOM files in a configurable hierarchical directory structure with atomic write operations. |
| **Priority** | Must Have |
| **Phase** | 3 |
| **Traces To** | FR-4.1.1 - FR-4.1.4 |

**Acceptance Criteria:**
1. Configurable directory structure (Patient/Study/Series/Instance)
2. Atomic file write (write to temp, then rename)
3. Handle path length limits
4. Support configurable file naming

**Default Storage Structure:**
```
storage_root/
├── {PatientID}/
│   ├── {StudyInstanceUID}/
│   │   ├── {SeriesInstanceUID}/
│   │   │   ├── {SOPInstanceUID}.dcm
│   │   │   ├── {SOPInstanceUID}.dcm
│   │   │   └── ...
│   │   └── {SeriesInstanceUID}/
│   │       └── ...
│   └── {StudyInstanceUID}/
│       └── ...
└── {PatientID}/
    └── ...
```

---

#### SRS-STOR-002: Index Database
| Attribute | Value |
|-----------|-------|
| **ID** | SRS-STOR-002 |
| **Title** | DICOM Index Database |
| **Description** | The system shall maintain an index database for fast query operations on Patient/Study/Series/Instance hierarchy. |
| **Priority** | Must Have |
| **Phase** | 3 |
| **Traces To** | FR-4.2.1 - FR-4.2.4 |

**Acceptance Criteria:**
1. Index all standard query keys
2. Support concurrent read operations
3. Maintain referential integrity
4. Query response < 100ms for 10K studies

**Index Schema:**

```
┌────────────────────────────────────────────────────────────┐
│                     Index Database Schema                   │
├────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────────┐                                       │
│  │     Patient     │                                       │
│  ├─────────────────┤                                       │
│  │ patient_id (PK) │                                       │
│  │ patient_name    │                                       │
│  │ birth_date      │                                       │
│  │ sex             │                                       │
│  └────────┬────────┘                                       │
│           │ 1:N                                            │
│  ┌────────▼────────┐                                       │
│  │      Study      │                                       │
│  ├─────────────────┤                                       │
│  │ study_uid (PK)  │                                       │
│  │ patient_id (FK) │                                       │
│  │ study_date      │                                       │
│  │ study_time      │                                       │
│  │ accession_num   │                                       │
│  │ modalities      │                                       │
│  │ num_series      │                                       │
│  │ num_instances   │                                       │
│  └────────┬────────┘                                       │
│           │ 1:N                                            │
│  ┌────────▼────────┐                                       │
│  │     Series      │                                       │
│  ├─────────────────┤                                       │
│  │ series_uid (PK) │                                       │
│  │ study_uid (FK)  │                                       │
│  │ modality        │                                       │
│  │ series_number   │                                       │
│  │ series_desc     │                                       │
│  │ num_instances   │                                       │
│  └────────┬────────┘                                       │
│           │ 1:N                                            │
│  ┌────────▼────────┐                                       │
│  │    Instance     │                                       │
│  ├─────────────────┤                                       │
│  │ sop_uid (PK)    │                                       │
│  │ series_uid (FK) │                                       │
│  │ sop_class_uid   │                                       │
│  │ instance_number │                                       │
│  │ file_path       │                                       │
│  │ file_size       │                                       │
│  │ transfer_syntax │                                       │
│  └─────────────────┘                                       │
│                                                             │
└────────────────────────────────────────────────────────────┘
```

---

### 3.5 Integration Requirements

#### SRS-INT-001: common_system Integration
| Attribute | Value |
|-----------|-------|
| **ID** | SRS-INT-001 |
| **Title** | common_system Foundation Integration |
| **Description** | The system shall use common_system Result<T> for error handling and IExecutor for async operations. |
| **Priority** | Must Have |
| **Phase** | 1 |
| **Traces To** | IR-1 |

**Acceptance Criteria:**
1. All fallible operations return Result<T>
2. Error codes in -800 to -899 range
3. IExecutor for async task submission

---

#### SRS-INT-002: container_system Integration
| Attribute | Value |
|-----------|-------|
| **ID** | SRS-INT-002 |
| **Title** | container_system Data Serialization |
| **Description** | The system shall use container_system value types and binary serialization for DICOM data representation. |
| **Priority** | Must Have |
| **Phase** | 1 |
| **Traces To** | IR-1 |

**Acceptance Criteria:**
1. Map DICOM VR to container_system types
2. Use binary serialization for DICOM file I/O
3. Leverage SIMD for pixel data processing

---

#### SRS-INT-003: network_system Integration
| Attribute | Value |
|-----------|-------|
| **ID** | SRS-INT-003 |
| **Title** | network_system Network Layer |
| **Description** | The system shall use network_system for TCP connection management and TLS support. |
| **Priority** | Must Have |
| **Phase** | 2 |
| **Traces To** | IR-1 |

**Acceptance Criteria:**
1. messaging_server as SCP base
2. messaging_client as SCU base
3. messaging_session as Association wrapper
4. TLS 1.2+ for DICOM TLS

---

#### SRS-INT-004: thread_system Integration
| Attribute | Value |
|-----------|-------|
| **ID** | SRS-INT-004 |
| **Title** | thread_system Concurrency |
| **Description** | The system shall use thread_system worker pools for concurrent DIMSE processing and storage operations. |
| **Priority** | Must Have |
| **Phase** | 2 |
| **Traces To** | IR-1 |

**Acceptance Criteria:**
1. Worker pool for DIMSE processing
2. Lock-free queue for job distribution
3. Cancellation token support

---

#### SRS-INT-005: logger_system Integration
| Attribute | Value |
|-----------|-------|
| **ID** | SRS-INT-005 |
| **Title** | logger_system Audit Logging |
| **Description** | The system shall use logger_system for audit logs with PHI access tracking for HIPAA compliance. |
| **Priority** | Must Have |
| **Phase** | 3 |
| **Traces To** | IR-1, NFR-4.2, NFR-4.3 |

**Acceptance Criteria:**
1. Log all DICOM operations
2. Log PHI access with user identity
3. Async logging for performance
4. Rotating log files

---

#### SRS-INT-006: monitoring_system Integration
| Attribute | Value |
|-----------|-------|
| **ID** | SRS-INT-006 |
| **Title** | monitoring_system Observability |
| **Description** | The system shall use monitoring_system for performance metrics, health checks, and distributed tracing. |
| **Priority** | Should Have |
| **Phase** | 3 |
| **Traces To** | IR-1 |

**Acceptance Criteria:**
1. DICOM operation metrics (count, latency, throughput)
2. Health check endpoint
3. Storage usage metrics

---

## 4. External Interface Requirements

### 4.1 User Interfaces

The PACS System provides no direct user interface. All interaction is through:
- DICOM network protocol
- Configuration files
- Administrative CLI tools (future)

### 4.2 Hardware Interfaces

| Interface | Description |
|-----------|-------------|
| Network | 1 Gbps Ethernet or faster |
| Storage | Local filesystem or network storage (NFS, CIFS) |
| Memory | 4 GB minimum, 16 GB recommended |

### 4.3 Software Interfaces

| Interface | Protocol | Description |
|-----------|----------|-------------|
| Modalities | DICOM | CT, MR, CR, DX scanners |
| Viewers | DICOM | Image viewing workstations |
| HIS/RIS | HL7 (future) | Hospital/Radiology Information Systems |
| Archive | DICOM/WADO | Long-term storage systems |

### 4.4 Communications Interfaces

| Protocol | Port | Description |
|----------|------|-------------|
| DICOM | 104, 11112 | Standard DICOM ports |
| DICOM TLS | 2762 | Secure DICOM |
| HTTP (future) | 8080 | WADO-RS/DICOMweb |

---

## 5. System Features

### 5.1 Feature: DICOM Image Storage

**Description:** Receive and store medical images from modalities.

| Attribute | Value |
|-----------|-------|
| **Priority** | Must Have |
| **Phase** | 3 |
| **SRS Requirements** | SRS-SVC-002, SRS-STOR-001, SRS-STOR-002 |
| **PRD Requirements** | FR-3.2.1-FR-3.2.5, FR-4.1.1-FR-4.1.4 |

**Use Case:**
1. Modality initiates association with PACS
2. PACS accepts association and negotiates presentation context
3. Modality sends C-STORE with image data
4. PACS validates and stores image
5. PACS returns success status
6. Association is released

---

### 5.2 Feature: Image Query

**Description:** Search for patients, studies, series, and images.

| Attribute | Value |
|-----------|-------|
| **Priority** | Must Have |
| **Phase** | 3 |
| **SRS Requirements** | SRS-SVC-004, SRS-STOR-002 |
| **PRD Requirements** | FR-3.3.1-FR-3.3.6, FR-4.2.1-FR-4.2.4 |

**Use Case:**
1. Viewer initiates association with PACS
2. Viewer sends C-FIND with query criteria
3. PACS searches index database
4. PACS returns matching results (pending responses)
5. PACS returns final response
6. Association is released

---

### 5.3 Feature: Image Retrieval

**Description:** Transfer images to requesting systems.

| Attribute | Value |
|-----------|-------|
| **Priority** | Must Have |
| **Phase** | 3 |
| **SRS Requirements** | SRS-SVC-005 |
| **PRD Requirements** | FR-3.3.7, FR-3.3.8 |

**Use Case (C-MOVE):**
1. Viewer initiates association with PACS
2. Viewer sends C-MOVE with destination AE
3. PACS establishes association to destination
4. PACS sends images via C-STORE
5. PACS reports progress to viewer
6. Associations are released

---

### 5.4 Feature: Modality Worklist

**Description:** Provide scheduled procedure information.

| Attribute | Value |
|-----------|-------|
| **Priority** | Should Have |
| **Phase** | 4 |
| **SRS Requirements** | SRS-SVC-006 |
| **PRD Requirements** | FR-3.4.1-FR-3.4.3 |

**Use Case:**
1. Modality initiates association with PACS
2. Modality sends C-FIND with MWL query
3. PACS returns scheduled procedures
4. Modality displays worklist
5. Association is released

---

### 5.5 Feature: MPPS Tracking

**Description:** Track performed procedure status.

| Attribute | Value |
|-----------|-------|
| **Priority** | Should Have |
| **Phase** | 4 |
| **SRS Requirements** | SRS-SVC-007 |
| **PRD Requirements** | FR-3.5.1-FR-3.5.4 |

**Use Case:**
1. Modality initiates association with PACS
2. Modality sends N-CREATE (IN PROGRESS)
3. Examination is performed
4. Modality sends N-SET (COMPLETED)
5. Association is released

---

## 6. Non-Functional Requirements

### 6.1 Performance Requirements

| ID | Requirement | Target | Traces To |
|----|-------------|--------|-----------|
| SRS-PERF-001 | Image storage throughput | ≥100 MB/s | NFR-1.1 |
| SRS-PERF-002 | Concurrent associations | ≥100 | NFR-1.2 |
| SRS-PERF-003 | Query response time (P95) | <100 ms | NFR-1.3 |
| SRS-PERF-004 | Association establishment | <50 ms | NFR-1.4 |
| SRS-PERF-005 | Memory usage baseline | <500 MB | NFR-1.5 |
| SRS-PERF-006 | Memory per association | <10 MB | NFR-1.6 |

### 6.2 Reliability Requirements

| ID | Requirement | Target | Traces To |
|----|-------------|--------|-----------|
| SRS-REL-001 | System uptime | 99.9% | NFR-2.1 |
| SRS-REL-002 | Data integrity | 100% | NFR-2.2 |
| SRS-REL-003 | Graceful degradation | Under high load | NFR-2.3 |
| SRS-REL-004 | Error recovery | Automatic | NFR-2.4 |
| SRS-REL-005 | Transaction safety | ACID | NFR-2.5 |

### 6.3 Security Requirements

| ID | Requirement | Target | Traces To |
|----|-------------|--------|-----------|
| SRS-SEC-001 | TLS support | TLS 1.2/1.3 | NFR-4.1, SR-1 |
| SRS-SEC-002 | Access logging | Complete | NFR-4.2 |
| SRS-SEC-003 | Audit trail | HIPAA compliant | NFR-4.3, SR-2 |
| SRS-SEC-004 | Input validation | 100% | NFR-4.4 |
| SRS-SEC-005 | Memory safety | Zero leaks | NFR-4.5 |

### 6.4 Maintainability Requirements

| ID | Requirement | Target | Traces To |
|----|-------------|--------|-----------|
| SRS-MAINT-001 | Code coverage | ≥80% | NFR-5.1 |
| SRS-MAINT-002 | Documentation | Complete | NFR-5.2 |
| SRS-MAINT-003 | CI/CD pipeline | 100% green | NFR-5.3 |
| SRS-MAINT-004 | Thread safety | Verified | NFR-5.4 |
| SRS-MAINT-005 | Modular design | Low coupling | NFR-5.5 |

---

## 7. Requirements Traceability Matrix

### 7.1 PRD to SRS Traceability

| PRD Requirement | SRS Requirement(s) | Status |
|-----------------|-------------------|--------|
| FR-1.1.1 | SRS-CORE-001, SRS-CORE-003 | Specified |
| FR-1.1.2 | SRS-CORE-002, SRS-CORE-008 | Specified |
| FR-1.1.3 | SRS-CORE-004 | Specified |
| FR-1.1.4 | SRS-CORE-003 | Specified |
| FR-1.1.5 | SRS-CORE-003 | Specified |
| FR-1.2.1 | SRS-CORE-005 | Specified |
| FR-1.2.2 | SRS-CORE-005 | Specified |
| FR-1.2.3 | SRS-CORE-005 | Specified |
| FR-1.2.4 | SRS-CORE-005 | Specified |
| FR-1.2.5 | SRS-CORE-005 | Specified |
| FR-1.3.1 | SRS-CORE-006 | Specified |
| FR-1.3.2 | SRS-CORE-006 | Specified |
| FR-1.3.3 | SRS-CORE-003, SRS-CORE-007 | Specified |
| FR-1.3.4 | SRS-CORE-003, SRS-CORE-007 | Specified |
| FR-1.3.5 | SRS-CORE-007 | Specified |
| FR-1.3.6 | SRS-CORE-007 | Specified |
| FR-2.1.1-FR-2.1.7 | SRS-NET-001 | Specified |
| FR-2.2.1 | SRS-NET-002 | Specified |
| FR-2.2.2 | SRS-NET-003 | Specified |
| FR-2.2.3 | SRS-NET-005 | Specified |
| FR-2.2.4 | SRS-NET-002 | Specified |
| FR-2.3.1-FR-2.3.9 | SRS-NET-004 | Specified |
| FR-3.1.1-FR-3.1.3 | SRS-SVC-001 | Specified |
| FR-3.2.1-FR-3.2.4 | SRS-SVC-002 | Specified |
| FR-3.2.5 | SRS-SVC-003 | Specified |
| FR-3.3.1-FR-3.3.6 | SRS-SVC-004 | Specified |
| FR-3.3.7-FR-3.3.8 | SRS-SVC-005 | Specified |
| FR-3.4.1-FR-3.4.3 | SRS-SVC-006 | Specified |
| FR-3.5.1-FR-3.5.4 | SRS-SVC-007 | Specified |
| FR-4.1.1-FR-4.1.4 | SRS-STOR-001 | Specified |
| FR-4.2.1-FR-4.2.4 | SRS-STOR-002 | Specified |
| NFR-1.1-NFR-1.6 | SRS-PERF-001 - SRS-PERF-006 | Specified |
| NFR-2.1-NFR-2.5 | SRS-REL-001 - SRS-REL-005 | Specified |
| NFR-4.1-NFR-4.5 | SRS-SEC-001 - SRS-SEC-005 | Specified |
| NFR-5.1-NFR-5.5 | SRS-MAINT-001 - SRS-MAINT-005 | Specified |
| IR-1 | SRS-INT-001 - SRS-INT-006 | Specified |

### 7.2 SRS to Test Case Traceability (Template)

| SRS Requirement | Test Case ID | Test Type | Status |
|-----------------|-------------|-----------|--------|
| SRS-CORE-001 | TC-CORE-001 | Unit | Planned |
| SRS-CORE-002 | TC-CORE-002 | Unit | Planned |
| SRS-CORE-003 | TC-CORE-003 | Unit | Planned |
| SRS-CORE-004 | TC-CORE-004 | Unit | Planned |
| SRS-CORE-005 | TC-CORE-005 | Unit | Planned |
| SRS-CORE-006 | TC-CORE-006 | Integration | Planned |
| SRS-CORE-007 | TC-CORE-007 | Integration | Planned |
| SRS-CORE-008 | TC-CORE-008 | Unit | Planned |
| SRS-NET-001 | TC-NET-001 | Integration | Planned |
| SRS-NET-002 | TC-NET-002 | Integration | Planned |
| SRS-NET-003 | TC-NET-003 | Integration | Planned |
| SRS-NET-004 | TC-NET-004 | Integration | Planned |
| SRS-NET-005 | TC-NET-005 | Performance | Planned |
| SRS-SVC-001 | TC-SVC-001 | Conformance | Planned |
| SRS-SVC-002 | TC-SVC-002 | Conformance | Planned |
| SRS-SVC-003 | TC-SVC-003 | Conformance | Planned |
| SRS-SVC-004 | TC-SVC-004 | Conformance | Planned |
| SRS-SVC-005 | TC-SVC-005 | Conformance | Planned |
| SRS-SVC-006 | TC-SVC-006 | Conformance | Planned |
| SRS-SVC-007 | TC-SVC-007 | Conformance | Planned |
| SRS-STOR-001 | TC-STOR-001 | Integration | Planned |
| SRS-STOR-002 | TC-STOR-002 | Integration | Planned |

### 7.3 Cross-Reference Summary

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    Requirements Traceability Overview                    │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│   PRD                    SRS                      Test Cases            │
│   ───                    ───                      ──────────            │
│                                                                          │
│   FR-1.x  ────────────►  SRS-CORE-xxx  ────────►  TC-CORE-xxx          │
│   (DICOM Core)           (Core Module)            (Unit Tests)          │
│                                                                          │
│   FR-2.x  ────────────►  SRS-NET-xxx   ────────►  TC-NET-xxx           │
│   (Network)              (Network Module)         (Integration)         │
│                                                                          │
│   FR-3.x  ────────────►  SRS-SVC-xxx   ────────►  TC-SVC-xxx           │
│   (Services)             (Service Module)         (Conformance)         │
│                                                                          │
│   FR-4.x  ────────────►  SRS-STOR-xxx  ────────►  TC-STOR-xxx          │
│   (Storage)              (Storage Module)         (Integration)         │
│                                                                          │
│   NFR-x   ────────────►  SRS-PERF-xxx  ────────►  TC-PERF-xxx          │
│   (Performance)          SRS-REL-xxx              (Performance)         │
│                          SRS-SEC-xxx              (Load Tests)          │
│                          SRS-MAINT-xxx                                  │
│                                                                          │
│   IR-x    ────────────►  SRS-INT-xxx   ────────►  TC-INT-xxx           │
│   (Integration)          (Integration)            (Integration)         │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 8. Appendices

### Appendix A: Requirement ID Scheme

| Prefix | Category | Example |
|--------|----------|---------|
| SRS-CORE-xxx | Core DICOM module | SRS-CORE-001 |
| SRS-NET-xxx | Network protocol | SRS-NET-001 |
| SRS-SVC-xxx | DICOM services | SRS-SVC-001 |
| SRS-STOR-xxx | Storage backend | SRS-STOR-001 |
| SRS-INT-xxx | Integration | SRS-INT-001 |
| SRS-PERF-xxx | Performance | SRS-PERF-001 |
| SRS-REL-xxx | Reliability | SRS-REL-001 |
| SRS-SEC-xxx | Security | SRS-SEC-001 |
| SRS-MAINT-xxx | Maintainability | SRS-MAINT-001 |

### Appendix B: Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0.0 | 2025-11-30 | kcenon | Initial version |

### Appendix C: Glossary

| Term | Definition |
|------|------------|
| Association | A logical connection between two DICOM Application Entities |
| Command Set | DICOM data set containing DIMSE command information |
| Data Set | Collection of DICOM data elements |
| Presentation Context | Agreement on Abstract Syntax and Transfer Syntax |
| Transfer Syntax | Rules for encoding DICOM data |

---

*Document Version: 1.0.0*
*Created: 2025-11-30*
*Author: kcenon@naver.com*
