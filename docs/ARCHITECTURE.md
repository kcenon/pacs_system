# Architecture Documentation - PACS System

> **Version:** 1.0.0
> **Last Updated:** 2025-01-30
> **Language:** **English** | [한국어](ARCHITECTURE_KO.md)

---

## Table of Contents

- [Design Philosophy](#design-philosophy)
- [Core Principles](#core-principles)
- [System Architecture](#system-architecture)
- [Component Architecture](#component-architecture)
- [Network Architecture](#network-architecture)
- [Data Flow Architecture](#data-flow-architecture)
- [Integration Architecture](#integration-architecture)
- [Thread Model](#thread-model)
- [Error Handling Strategy](#error-handling-strategy)
- [Security Architecture](#security-architecture)

---

## Design Philosophy

The PACS System is designed around four fundamental principles:

### 1. Ecosystem-First Design

Build upon the proven kcenon ecosystem infrastructure rather than reinventing core components. This maximizes code reuse, ensures consistency, and leverages battle-tested implementations.

**Key Design Decisions:**
- Use network_system for all TCP/TLS operations
- Leverage container_system for data serialization
- Apply thread_system patterns for concurrency
- Adopt Result<T> pattern from common_system

### 2. DICOM Standard Compliance

Implement DICOM protocols according to the official standard (PS3.x) to ensure interoperability with all DICOM-compliant systems.

**Compliance Strategy:**
- Follow PS3.5 (Data Structures and Encoding)
- Follow PS3.7 (Message Exchange)
- Follow PS3.8 (Network Communication)
- Regular conformance testing

### 3. Zero External DICOM Dependencies

Implement all DICOM functionality from scratch using ecosystem components, eliminating dependency on external DICOM libraries (DCMTK, GDCM).

**Benefits:**
- Complete control over implementation
- No GPL/LGPL license concerns
- Optimized for ecosystem integration
- Full transparency and customizability

### 4. Production-Grade Quality

Maintain the same quality standards as other ecosystem projects:

**Quality Characteristics:**
- RAII Grade A (perfect resource management)
- Zero memory leaks (AddressSanitizer verified)
- Zero data races (ThreadSanitizer verified)
- 80%+ test coverage
- Complete API documentation

---

## Core Principles

### Modularity

The system is organized into loosely coupled modules with clear responsibilities:

```
┌─────────────────────────────────────────────────────────────────┐
│                        Services Layer                            │
│     (Storage SCP, Query/Retrieve SCP, Worklist SCP, MPPS SCP)   │
└────────────────────────────────┬────────────────────────────────┘
                                 │ uses
┌────────────────────────────────▼────────────────────────────────┐
│                        Protocol Layer                            │
│              (DIMSE Messages, PDU, Association)                  │
└────────────────────────────────┬────────────────────────────────┘
                                 │ uses
┌────────────────────────────────▼────────────────────────────────┐
│                          Core Layer                              │
│        (Data Element, Data Set, DICOM File, Dictionary)         │
└────────────────────────────────┬────────────────────────────────┘
                                 │ uses
┌────────────────────────────────▼────────────────────────────────┐
│                       Encoding Layer                             │
│        (VR Types, Transfer Syntax, Implicit/Explicit VR)        │
└────────────────────────────────┬────────────────────────────────┘
                                 │ uses
┌────────────────────────────────▼────────────────────────────────┐
│                      Integration Layer                           │
│    (Container Adapter, Network Adapter, Thread Adapter)         │
└────────────────────────────────┬────────────────────────────────┘
                                 │ uses
┌────────────────────────────────▼────────────────────────────────┐
│                     Ecosystem Foundation                         │
│  (common_system, container_system, network_system, thread_system)│
└─────────────────────────────────────────────────────────────────┘
```

### Extensibility

New DICOM services and SOP classes can be added without modifying existing code:

- Service handlers implement a common interface
- SOP classes registered in dictionary
- Transfer syntaxes pluggable as codecs
- Storage backends implement abstract interface

### Performance

Optimizations are applied at multiple levels:

1. **Network**: Async I/O via ASIO (network_system)
2. **Concurrency**: Lock-free queues, thread pools (thread_system)
3. **Serialization**: Binary encoding, zero-copy where possible
4. **Memory**: RAII, smart pointers, memory pooling for PDUs

### Safety

Memory and thread safety enforced through modern C++ idioms:

- RAII for all resources (file handles, sockets, associations)
- Smart pointers (`std::shared_ptr`, `std::unique_ptr`)
- Thread-safe queues from thread_system
- Result<T> for error propagation

---

## System Architecture

### High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              PACS System                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌────────────────────────────────────────────────────────────────────────┐ │
│  │                         Service Providers                               │ │
│  │                                                                         │ │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────────┐│ │
│  │  │   Storage   │  │   Query/    │  │  Worklist   │  │      MPPS       ││ │
│  │  │     SCP     │  │  Retrieve   │  │     SCP     │  │      SCP        ││ │
│  │  │             │  │     SCP     │  │             │  │                 ││ │
│  │  │ • C-STORE   │  │ • C-FIND    │  │ • C-FIND    │  │ • N-CREATE      ││ │
│  │  │ • Store     │  │ • C-MOVE    │  │ • Scheduled │  │ • N-SET         ││ │
│  │  │   Commit    │  │ • C-GET     │  │   Procedure │  │ • Status Track  ││ │
│  │  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └────────┬────────┘│ │
│  │         │                │                │                  │         │ │
│  └─────────┼────────────────┼────────────────┼──────────────────┼─────────┘ │
│            │                │                │                  │           │
│  ┌─────────▼────────────────▼────────────────▼──────────────────▼─────────┐ │
│  │                      DIMSE Message Handler                              │ │
│  │                                                                         │ │
│  │  ┌─────────────────────────────────────────────────────────────────┐   │ │
│  │  │                    Message Dispatcher                            │   │ │
│  │  │  • C-ECHO Handler      • C-STORE Handler    • C-FIND Handler    │   │ │
│  │  │  • C-MOVE Handler      • C-GET Handler      • N-CREATE Handler  │   │ │
│  │  │  • N-SET Handler       • N-GET Handler      • N-EVENT Handler   │   │ │
│  │  └─────────────────────────────────────────────────────────────────┘   │ │
│  └─────────────────────────────────┬───────────────────────────────────────┘ │
│                                    │                                         │
│  ┌─────────────────────────────────▼───────────────────────────────────────┐ │
│  │                    Association Manager                                   │ │
│  │                                                                          │ │
│  │  ┌────────────────┐  ┌────────────────┐  ┌──────────────────────────┐  │ │
│  │  │  State Machine │  │  Presentation  │  │   PDU Encoder/Decoder   │  │ │
│  │  │                │  │    Context     │  │                          │  │ │
│  │  │  8-state FSM   │  │   Negotiation  │  │  • A-ASSOCIATE-RQ/AC/RJ │  │ │
│  │  │  per PS3.8     │  │                │  │  • P-DATA-TF             │  │ │
│  │  │                │  │                │  │  • A-RELEASE-RQ/RP       │  │ │
│  │  │                │  │                │  │  • A-ABORT               │  │ │
│  │  └────────────────┘  └────────────────┘  └──────────────────────────┘  │ │
│  └─────────────────────────────────┬───────────────────────────────────────┘ │
│                                    │                                         │
│  ┌─────────────────────────────────▼───────────────────────────────────────┐ │
│  │                      Network Transport Layer                             │ │
│  │                    (network_system integration)                          │ │
│  │                                                                          │ │
│  │  ┌────────────────┐  ┌────────────────┐  ┌──────────────────────────┐  │ │
│  │  │ TCP Server     │  │ TCP Client     │  │   Session Manager        │  │ │
│  │  │ (Port 11112)   │  │ (SCU Mode)     │  │                          │  │ │
│  │  └────────────────┘  └────────────────┘  └──────────────────────────┘  │ │
│  └─────────────────────────────────────────────────────────────────────────┘ │
│                                                                              │
├──────────────────────────────────────────────────────────────────────────────┤
│                            Storage Subsystem                                 │
│                                                                              │
│  ┌────────────────────────────┐  ┌────────────────────────────────────────┐ │
│  │      File Storage          │  │           Index Database               │ │
│  │                            │  │                                        │ │
│  │  • Hierarchical structure  │  │  • Patient/Study/Series/Image index   │ │
│  │  • Atomic operations       │  │  • Fast attribute lookup              │ │
│  │  • Duplicate detection     │  │  • SQLite backend                     │ │
│  └────────────────────────────┘  └────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────────────────┘
```

### Deployment Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           Deployment Options                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  Option 1: Single Instance                                                   │
│  ┌────────────────────────────────────────────────────────────────────────┐ │
│  │                         PACS Server                                     │ │
│  │  ┌─────────────────┐  ┌─────────────────┐  ┌──────────────────────────┐│ │
│  │  │  Storage SCP    │  │  Q/R SCP        │  │  Storage Backend         ││ │
│  │  │  Port 11112     │  │  Port 11112     │  │  /data/dicom             ││ │
│  │  └─────────────────┘  └─────────────────┘  └──────────────────────────┘│ │
│  └────────────────────────────────────────────────────────────────────────┘ │
│                                                                              │
│  Option 2: Distributed (Future)                                              │
│  ┌────────────────────────────────────────────────────────────────────────┐ │
│  │                                                                         │ │
│  │  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────────┐ │ │
│  │  │  Storage Node 1 │  │  Storage Node 2 │  │  Query Node             │ │ │
│  │  │  Port 11112     │  │  Port 11112     │  │  Port 11113             │ │ │
│  │  └────────┬────────┘  └────────┬────────┘  └────────────┬────────────┘ │ │
│  │           │                    │                        │              │ │
│  │           └────────────────────┼────────────────────────┘              │ │
│  │                                │                                        │ │
│  │                    ┌───────────▼───────────┐                           │ │
│  │                    │   Shared Storage      │                           │ │
│  │                    │   (NFS/S3)            │                           │ │
│  │                    └───────────────────────┘                           │ │
│  └────────────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Component Architecture

### Core Module

The core module provides fundamental DICOM data structures:

```
┌─────────────────────────────────────────────────────────────────┐
│                          Core Module                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │                      dicom_element                           ││
│  │                                                              ││
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────┐ ││
│  │  │   Tag    │  │    VR    │  │  Length  │  │    Value     │ ││
│  │  │ (4 bytes)│  │ (2 bytes)│  │ (2/4 b)  │  │  (variable)  │ ││
│  │  └──────────┘  └──────────┘  └──────────┘  └──────────────┘ ││
│  └─────────────────────────────────────────────────────────────┘│
│                                                                  │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │                      dicom_dataset                           ││
│  │                                                              ││
│  │  • Ordered map of Tag → DicomElement                        ││
│  │  • O(log n) lookup by tag                                   ││
│  │  • Iterator support (tag order)                             ││
│  │  • Sequence (SQ) nesting                                    ││
│  └─────────────────────────────────────────────────────────────┘│
│                                                                  │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │                       dicom_file                             ││
│  │                                                              ││
│  │  ┌──────────────────┐  ┌─────────────────┐  ┌─────────────┐ ││
│  │  │  Preamble (128)  │  │  DICM Prefix    │  │  Meta Info  │ ││
│  │  └──────────────────┘  └─────────────────┘  └─────────────┘ ││
│  │                                                              ││
│  │  ┌─────────────────────────────────────────────────────────┐││
│  │  │                    Data Set                              │││
│  │  └─────────────────────────────────────────────────────────┘││
│  └─────────────────────────────────────────────────────────────┘│
│                                                                  │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │                    dicom_dictionary                          ││
│  │                                                              ││
│  │  • Compile-time tag constants                               ││
│  │  • Tag → (VR, Name, VM) lookup                              ││
│  │  • Private tag registration                                 ││
│  └─────────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────────┘
```

### Encoding Module

The encoding module handles VR encoding and Transfer Syntax:

```
┌─────────────────────────────────────────────────────────────────┐
│                        Encoding Module                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │                        vr_types                              ││
│  │                                                              ││
│  │  enum class vr_type {                                       ││
│  │      AE, AS, AT, CS, DA, DS, DT, FL, FD, IS, LO, LT,       ││
│  │      OB, OD, OF, OL, OW, PN, SH, SL, SQ, SS, ST, TM,       ││
│  │      UI, UL, UN, US, UT                                     ││
│  │  };                                                         ││
│  │                                                              ││
│  │  • VR classification (string, numeric, binary, sequence)   ││
│  │  • VR constraints (max length, padding character)          ││
│  │  • VR to container type mapping                            ││
│  └─────────────────────────────────────────────────────────────┘│
│                                                                  │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │                    transfer_syntax                           ││
│  │                                                              ││
│  │  class transfer_syntax {                                    ││
│  │      uid: string                                            ││
│  │      is_implicit_vr: bool                                   ││
│  │      is_little_endian: bool                                 ││
│  │      is_encapsulated: bool                                  ││
│  │  };                                                         ││
│  │                                                              ││
│  │  Static factory methods:                                    ││
│  │  • implicit_vr_little_endian()                              ││
│  │  • explicit_vr_little_endian()                              ││
│  │  • explicit_vr_big_endian()                                 ││
│  └─────────────────────────────────────────────────────────────┘│
│                                                                  │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │                      VR Codecs                               ││
│  │                                                              ││
│  │  ┌──────────────────┐  ┌──────────────────────────────────┐ ││
│  │  │ implicit_vr_codec│  │      explicit_vr_codec           │ ││
│  │  │                  │  │                                  │ ││
│  │  │ • No VR field    │  │ • 2-byte VR field                │ ││
│  │  │ • 4-byte length  │  │ • 2 or 4-byte length             │ ││
│  │  │ • Lookup VR      │  │ • VR in stream                   │ ││
│  │  └──────────────────┘  └──────────────────────────────────┘ ││
│  └─────────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────────┘
```

### Network Module

The network module implements DICOM Upper Layer Protocol:

```
┌─────────────────────────────────────────────────────────────────┐
│                        Network Module                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │                         PDU Layer                            ││
│  │                                                              ││
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────────┐││
│  │  │ pdu_encoder │ │ pdu_decoder │ │     PDU Types           │││
│  │  │             │ │             │ │                         │││
│  │  │ • serialize │ │ • parse     │ │ • A-ASSOCIATE-RQ (0x01) │││
│  │  │   to bytes  │ │   from      │ │ • A-ASSOCIATE-AC (0x02) │││
│  │  │             │ │   bytes     │ │ • A-ASSOCIATE-RJ (0x03) │││
│  │  │             │ │             │ │ • P-DATA-TF (0x04)      │││
│  │  │             │ │             │ │ • A-RELEASE-RQ (0x05)   │││
│  │  │             │ │             │ │ • A-RELEASE-RP (0x06)   │││
│  │  │             │ │             │ │ • A-ABORT (0x07)        │││
│  │  └─────────────┘ └─────────────┘ └─────────────────────────┘││
│  └─────────────────────────────────────────────────────────────┘│
│                                                                  │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │                      DIMSE Layer                             ││
│  │                                                              ││
│  │  ┌──────────────────────────────────────────────────────────┐││
│  │  │                 dimse_message                             │││
│  │  │                                                           │││
│  │  │  ┌────────────┐  ┌────────────┐  ┌──────────────────────┐│││
│  │  │  │ Command Set│  │   Data Set │  │   Message Types      ││││
│  │  │  │            │  │ (optional) │  │                      ││││
│  │  │  │ • Command  │  │            │  │ • C-ECHO-RQ/RSP      ││││
│  │  │  │ • Message ID│  │            │  │ • C-STORE-RQ/RSP    ││││
│  │  │  │ • SOP Class │  │            │  │ • C-FIND-RQ/RSP     ││││
│  │  │  │ • Status    │  │            │  │ • C-MOVE-RQ/RSP     ││││
│  │  │  └────────────┘  └────────────┘  │ • C-GET-RQ/RSP       ││││
│  │  │                                  │ • N-CREATE-RQ/RSP    ││││
│  │  │                                  │ • N-SET-RQ/RSP       ││││
│  │  │                                  └──────────────────────┘│││
│  │  └──────────────────────────────────────────────────────────┘││
│  └─────────────────────────────────────────────────────────────┘│
│                                                                  │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │                   Association Manager                        ││
│  │                                                              ││
│  │  ┌──────────────────────────────────────────────────────────┐││
│  │  │                  association                              │││
│  │  │                                                           │││
│  │  │  State Machine (8 states per PS3.8):                     │││
│  │  │                                                           │││
│  │  │  Sta1 ─► Sta2 ─► Sta3 ─► Sta5 ─► Sta6 ─► Sta7 ─► Sta1   │││
│  │  │  (Idle)  (TCP)  (Await)  (Await) (Est.)  (Await) (Idle)  │││
│  │  │                                                           │││
│  │  │  Methods:                                                 │││
│  │  │  • connect()      - Initiate association (SCU)           │││
│  │  │  • accept()       - Accept association (SCP)             │││
│  │  │  • release()      - Graceful release                     │││
│  │  │  • abort()        - Immediate abort                      │││
│  │  │  • send_dimse()   - Send DIMSE message                   │││
│  │  │  • receive_dimse()- Receive DIMSE message                │││
│  │  └──────────────────────────────────────────────────────────┘││
│  └─────────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────────┘
```

---

## Network Architecture

### DICOM Communication Flow

```
┌────────────────┐                              ┌────────────────┐
│      SCU       │                              │      SCP       │
│  (Modality)    │                              │    (PACS)      │
└───────┬────────┘                              └───────┬────────┘
        │                                               │
        │  1. TCP Connection                            │
        │──────────────────────────────────────────────►│
        │                                               │
        │  2. A-ASSOCIATE-RQ                            │
        │  [Called AE, Calling AE, Contexts]           │
        │──────────────────────────────────────────────►│
        │                                               │
        │  3. A-ASSOCIATE-AC                            │
        │  [Accepted Contexts]                          │
        │◄──────────────────────────────────────────────│
        │                                               │
        │  4. P-DATA-TF (C-STORE-RQ Command)           │
        │──────────────────────────────────────────────►│
        │                                               │
        │  5. P-DATA-TF (C-STORE-RQ Data Set)          │
        │──────────────────────────────────────────────►│
        │                                               │
        │  6. P-DATA-TF (C-STORE-RSP)                  │
        │  [Status: Success]                            │
        │◄──────────────────────────────────────────────│
        │                                               │
        │  7. A-RELEASE-RQ                              │
        │──────────────────────────────────────────────►│
        │                                               │
        │  8. A-RELEASE-RP                              │
        │◄──────────────────────────────────────────────│
        │                                               │
        │  9. TCP Close                                 │
        │──────────────────────────────────────────────►│
        │                                               │
```

### Multi-Association Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                      PACS Server                                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                  TCP Acceptor (Port 11112)                 │  │
│  │                  (network_system::messaging_server)        │  │
│  └────────────────────────────┬──────────────────────────────┘  │
│                               │                                  │
│                               │ New Connection                   │
│                               ▼                                  │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                  Session Manager                           │  │
│  │                                                            │  │
│  │  ┌────────────┐  ┌────────────┐  ┌────────────┐           │  │
│  │  │ Session 1  │  │ Session 2  │  │ Session N  │  ...      │  │
│  │  │            │  │            │  │            │           │  │
│  │  │ Assoc ID:1 │  │ Assoc ID:2 │  │ Assoc ID:N │           │  │
│  │  │ AE: CT_01  │  │ AE: MR_02  │  │ AE: CR_03  │           │  │
│  │  │ State: Est │  │ State: Est │  │ State: Rel │           │  │
│  │  └────────────┘  └────────────┘  └────────────┘           │  │
│  └────────────────────────────┬──────────────────────────────┘  │
│                               │                                  │
│                               │ DIMSE Messages                   │
│                               ▼                                  │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                  Worker Thread Pool                        │  │
│  │                  (thread_system::thread_pool)              │  │
│  │                                                            │  │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  │  │
│  │  │ Worker 1 │  │ Worker 2 │  │ Worker 3 │  │ Worker N │  │  │
│  │  └──────────┘  └──────────┘  └──────────┘  └──────────┘  │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

---

## Data Flow Architecture

### Storage Flow (C-STORE)

```
┌─────────────────────────────────────────────────────────────────┐
│                     C-STORE Data Flow                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Modality                                                        │
│     │                                                            │
│     │ C-STORE-RQ                                                 │
│     ▼                                                            │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │  1. PDU Decode                                               ││
│  │     • Parse P-DATA-TF PDUs                                   ││
│  │     • Reassemble DIMSE fragments                             ││
│  └────────────────────────────┬────────────────────────────────┘│
│                               ▼                                  │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │  2. DIMSE Parse                                              ││
│  │     • Parse Command Set                                      ││
│  │     • Parse Data Set (image data)                            ││
│  └────────────────────────────┬────────────────────────────────┘│
│                               ▼                                  │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │  3. Validation                                               ││
│  │     • Verify SOP Class                                       ││
│  │     • Check mandatory attributes                             ││
│  │     • Validate Transfer Syntax                               ││
│  └────────────────────────────┬────────────────────────────────┘│
│                               ▼                                  │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │  4. Duplicate Check                                          ││
│  │     • Query index for SOP Instance UID                       ││
│  │     • Handle duplicate policy (reject/replace)               ││
│  └────────────────────────────┬────────────────────────────────┘│
│                               ▼                                  │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │  5. Storage                                                  ││
│  │     • Write DICOM file to storage                            ││
│  │     • Update index database                                  ││
│  │     • (Atomic transaction)                                   ││
│  └────────────────────────────┬────────────────────────────────┘│
│                               ▼                                  │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │  6. Response                                                 ││
│  │     • Build C-STORE-RSP                                      ││
│  │     • Status: Success (0x0000)                               ││
│  │     • Send via PDU Encoder                                   ││
│  └─────────────────────────────────────────────────────────────┘│
│                               │                                  │
│                               ▼                                  │
│                           Modality                               │
└─────────────────────────────────────────────────────────────────┘
```

### Query Flow (C-FIND)

```
┌─────────────────────────────────────────────────────────────────┐
│                      C-FIND Data Flow                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Viewer                                                          │
│     │                                                            │
│     │ C-FIND-RQ (Query: PatientName=Doe*)                       │
│     ▼                                                            │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │  1. Parse Query                                              ││
│  │     • Extract query level (PATIENT/STUDY/SERIES/IMAGE)      ││
│  │     • Parse matching keys                                    ││
│  │     • Parse return keys                                      ││
│  └────────────────────────────┬────────────────────────────────┘│
│                               ▼                                  │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │  2. Build SQL Query                                          ││
│  │     • Map DICOM attributes to database columns              ││
│  │     • Handle wildcards (*, ?)                                ││
│  │     • Apply range queries (dates)                            ││
│  └────────────────────────────┬────────────────────────────────┘│
│                               ▼                                  │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │  3. Execute Query                                            ││
│  │     • Run optimized SQL                                      ││
│  │     • Apply limit (max matches)                              ││
│  │     • Stream results                                         ││
│  └────────────────────────────┬────────────────────────────────┘│
│                               ▼                                  │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │  4. Send Results                                             ││
│  │     • For each match:                                        ││
│  │         • Build C-FIND-RSP (Status: Pending)                ││
│  │         • Include matching attributes                        ││
│  │         • Send to viewer                                     ││
│  │     • Final C-FIND-RSP (Status: Success)                    ││
│  └─────────────────────────────────────────────────────────────┘│
│                               │                                  │
│                               ▼                                  │
│                            Viewer                                │
└─────────────────────────────────────────────────────────────────┘
```

---

## Integration Architecture

### Ecosystem Integration Map

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        Ecosystem Integration                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌────────────────────────────────────────────────────────────────────────┐ │
│  │                           pacs_system                                   │ │
│  │                                                                         │ │
│  │  ┌─────────────────────────────────────────────────────────────────┐   │ │
│  │  │                    Integration Layer                             │   │ │
│  │  │                                                                  │   │ │
│  │  │  ┌──────────────────┐  ┌──────────────────┐  ┌────────────────┐ │   │ │
│  │  │  │ container_adapter │  │ network_adapter  │  │ thread_adapter │ │   │ │
│  │  │  │                   │  │                  │  │                │ │   │ │
│  │  │  │ • VR to value    │  │ • TCP server     │  │ • Job queues   │ │   │ │
│  │  │  │ • Serialization  │  │ • TLS support    │  │ • Async tasks  │ │   │ │
│  │  │  │ • SQ handling    │  │ • Sessions       │  │ • Cancellation │ │   │ │
│  │  │  └────────┬─────────┘  └────────┬─────────┘  └───────┬────────┘ │   │ │
│  │  │           │                     │                     │         │   │ │
│  │  └───────────┼─────────────────────┼─────────────────────┼─────────┘   │ │
│  └──────────────┼─────────────────────┼─────────────────────┼─────────────┘ │
│                 │                     │                     │               │
│                 ▼                     ▼                     ▼               │
│  ┌──────────────────────┐ ┌──────────────────────┐ ┌──────────────────────┐ │
│  │   container_system   │ │    network_system    │ │    thread_system     │ │
│  │                      │ │                      │ │                      │ │
│  │  • value_container   │ │  • messaging_server  │ │  • thread_pool       │ │
│  │  • Binary serialize  │ │  • messaging_client  │ │  • job_queue         │ │
│  │  • Type-safe values  │ │  • messaging_session │ │  • cancellation      │ │
│  │  • SIMD acceleration │ │  • TLS/SSL           │ │  • lock-free         │ │
│  └──────────────────────┘ └──────────────────────┘ └──────────────────────┘ │
│                 │                     │                     │               │
│                 ▼                     ▼                     ▼               │
│  ┌──────────────────────────────────────────────────────────────────────┐  │
│  │                          common_system                                │  │
│  │                                                                       │  │
│  │  • Result<T> pattern           • Error code registry                  │  │
│  │  • IExecutor interface         • Event bus                            │  │
│  └──────────────────────────────────────────────────────────────────────┘  │
│                                                                              │
│  ┌──────────────────────┐                         ┌──────────────────────┐  │
│  │    logger_system     │                         │  monitoring_system   │  │
│  │                      │                         │                      │  │
│  │  • Async logging     │                         │  • Performance       │  │
│  │  • Audit trail       │                         │    metrics           │  │
│  │  • Multiple writers  │                         │  • Health checks     │  │
│  │  • HIPAA compliance  │                         │  • Distributed trace │  │
│  └──────────────────────┘                         └──────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Thread Model

### Threading Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          Thread Model                                        │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌────────────────────────────────────────────────────────────────────────┐ │
│  │                         Main Thread                                     │ │
│  │                                                                         │ │
│  │  • Configuration loading                                               │ │
│  │  • Service initialization                                              │ │
│  │  • Signal handling                                                     │ │
│  │  • Graceful shutdown orchestration                                     │ │
│  └────────────────────────────────────────────────────────────────────────┘ │
│                                                                              │
│  ┌────────────────────────────────────────────────────────────────────────┐ │
│  │                      I/O Thread Pool                                    │ │
│  │                   (network_system ASIO)                                 │ │
│  │                                                                         │ │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐                  │ │
│  │  │  IO Thread 1 │  │  IO Thread 2 │  │  IO Thread N │                  │ │
│  │  │              │  │              │  │              │                  │ │
│  │  │ • Accept     │  │ • PDU Read   │  │ • PDU Write  │                  │ │
│  │  │ • Read       │  │ • PDU Write  │  │ • Read       │                  │ │
│  │  │ • Write      │  │              │  │              │                  │ │
│  │  └──────────────┘  └──────────────┘  └──────────────┘                  │ │
│  └────────────────────────────────────────────────────────────────────────┘ │
│                                      │                                       │
│                                      │ Jobs                                  │
│                                      ▼                                       │
│  ┌────────────────────────────────────────────────────────────────────────┐ │
│  │                     Worker Thread Pool                                  │ │
│  │                   (thread_system::thread_pool)                          │ │
│  │                                                                         │ │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐                  │ │
│  │  │  Worker 1    │  │  Worker 2    │  │  Worker N    │                  │ │
│  │  │              │  │              │  │              │                  │ │
│  │  │ • C-STORE    │  │ • C-FIND     │  │ • C-MOVE     │                  │ │
│  │  │   handling   │  │   handling   │  │   handling   │                  │ │
│  │  └──────────────┘  └──────────────┘  └──────────────┘                  │ │
│  │                                                                         │ │
│  │  ┌──────────────────────────────────────────────────────────────────┐  │ │
│  │  │                   Lock-Free Job Queue                             │  │ │
│  │  │              (thread_system::job_queue)                           │  │ │
│  │  └──────────────────────────────────────────────────────────────────┘  │ │
│  └────────────────────────────────────────────────────────────────────────┘ │
│                                                                              │
│  ┌────────────────────────────────────────────────────────────────────────┐ │
│  │                      Storage Thread Pool                                │ │
│  │                                                                         │ │
│  │  ┌──────────────┐  ┌──────────────┐                                    │ │
│  │  │  Storage 1   │  │  Storage 2   │                                    │ │
│  │  │              │  │              │                                    │ │
│  │  │ • File I/O   │  │ • DB Index   │                                    │ │
│  │  └──────────────┘  └──────────────┘                                    │ │
│  └────────────────────────────────────────────────────────────────────────┘ │
│                                                                              │
│  ┌────────────────────────────────────────────────────────────────────────┐ │
│  │                       Logger Thread                                     │ │
│  │                  (logger_system async)                                  │ │
│  └────────────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Error Handling Strategy

### Error Propagation

All errors are propagated using the Result<T> pattern from common_system:

```cpp
// Error code registration (-800 to -899)
namespace pacs::error_codes {
    // DICOM Parsing Errors (-800 to -819)
    constexpr int INVALID_DICOM_FILE = -800;
    constexpr int INVALID_VR = -801;
    // ... (see PRD for complete list)
}

// Error handling example
auto result = dicom_file::read("/path/to/file.dcm");
if (result.is_err()) {
    const auto& error = result.error();
    log_error("Read failed: {} (code: {})", error.message, error.code);
    return result; // Propagate error
}

// Success path
auto& file = result.value();
```

### Error Categories

| Category | Code Range | Description |
|----------|-----------|-------------|
| Parsing | -800 to -819 | DICOM data parsing errors |
| Association | -820 to -839 | Network association errors |
| DIMSE | -840 to -859 | Message exchange errors |
| Storage | -860 to -879 | File/database storage errors |
| Query | -880 to -899 | Query processing errors |

---

## Security Architecture

### Authentication and Authorization

```
┌─────────────────────────────────────────────────────────────────┐
│                     Security Architecture                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                    Transport Security                      │  │
│  │                                                            │  │
│  │  • TLS 1.2/1.3 encryption                                 │  │
│  │  • Certificate validation                                 │  │
│  │  • Mutual authentication (optional)                       │  │
│  │  • Modern cipher suites                                   │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                                                  │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                    Access Control                          │  │
│  │                                                            │  │
│  │  • AE Title whitelist                                     │  │
│  │  • IP-based filtering                                     │  │
│  │  • SOP Class restrictions per AE                          │  │
│  │  • Operation-level permissions                            │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                                                  │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                    Audit Logging                           │  │
│  │                                                            │  │
│  │  • All DICOM operations logged                            │  │
│  │  • PHI access tracking                                    │  │
│  │  • Tamper-evident log storage                             │  │
│  │  • Compliance: HIPAA, GDPR, IHE ATNA                      │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

---

*Document Version: 1.0.0*
*Created: 2025-01-30*
*Author: kcenon@naver.com*
