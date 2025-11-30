# PACS System Implementation Analysis

> **Language:** **English** | [한국어](PACS_IMPLEMENTATION_ANALYSIS_KO.md)

## 1. Overview

This document presents an analysis and strategy for implementing a PACS (Picture Archiving and Communication System) without external DICOM libraries, utilizing the existing kcenon ecosystem (common_system, container_system, thread_system, logger_system, monitoring_system, network_system).

### 1.1 Goals

- **No External DICOM Libraries**: Pure implementation without DCMTK, GDCM, or other external libraries
- **Maximum Ecosystem Utilization**: Build upon already-proven systems
- **Production-Level Quality**: Apply the same quality standards as existing systems (CI/CD, testing, security)

---

## 2. Existing System Analysis

### 2.1 System Overview

| System | Main Features | PACS Utilization Area |
|--------|---------------|----------------------|
| **common_system** | IExecutor, Result<T>, Event Bus, Error Codes | Common interfaces, error handling |
| **container_system** | Type-safe serialization, SIMD acceleration, Binary/JSON/XML | DICOM data structures, serialization |
| **thread_system** | Thread Pool, Lock-free Queue, Hazard Pointers | Asynchronous task processing |
| **logger_system** | Async logging, 4.34M msg/s | System logging, audit logs |
| **monitoring_system** | Metrics, distributed tracing, Health Check | Performance monitoring, diagnostics |
| **network_system** | TCP, TLS, WebSocket, HTTP | DICOM network communication |

### 2.2 common_system Utilization

```
common_system (Foundation Layer)
├── IExecutor Interface → DICOM task async execution
├── Result<T> Pattern → DICOM error handling
├── Event Bus → DICOM event notification
└── Error Code Registry → DICOM-specific error codes (-800 ~ -899 allocated)
```

**Key Utilization Points:**
- `Result<T>` pattern for DICOM parsing/transmission error handling
- Event Bus for image reception, storage completion, and other event propagation
- Ecosystem error code system utilization (-800 ~ -899 range allocation)

### 2.3 container_system Utilization

```
container_system (Data Layer)
├── Value Types (15 types) → DICOM VR (Value Representation) mapping
├── Binary Serialization → DICOM file save/load
├── SIMD Acceleration → Large image processing acceleration
└── Thread-safe Container → Safe concurrent access
```

**DICOM VR to Value Type Mapping:**

| DICOM VR | Description | container_system Type |
|----------|-------------|----------------------|
| CS | Code String | string |
| DS | Decimal String | string → double conversion |
| IS | Integer String | string → int64 conversion |
| LO | Long String | string |
| PN | Person Name | string (struct extension) |
| DA | Date | string (YYYYMMDD) |
| TM | Time | string (HHMMSS) |
| UI | Unique Identifier | string |
| UL | Unsigned Long | uint32 |
| US | Unsigned Short | uint16 |
| OB | Other Byte | bytes |
| OW | Other Word | bytes |
| SQ | Sequence | container (nested) |
| AT | Attribute Tag | uint32 |

### 2.4 thread_system Utilization

```
thread_system (Concurrency Layer)
├── Thread Pool → Multi-client concurrent processing
├── Typed Thread Pool → Priority-based task scheduling
├── Lock-free Queue → High-performance task queue
└── Cancellation Token → Long task cancellation support
```

**Utilization Scenarios:**
- Parallel processing of C-STORE requests (image storage)
- Async processing of large Query/Retrieve operations
- Background processing of image compression/conversion tasks

### 2.5 logger_system Utilization

```
logger_system (Logging Layer)
├── Async Logging → Logging without performance degradation
├── Multiple Writers → Simultaneous output to file, console, network
├── Security Audit → HIPAA/GDPR compliance
└── Rotating Files → Long-term operation support
```

**PACS Logging Requirements:**
- Patient information access audit logs (HIPAA required)
- DICOM Association logging
- Image storage/retrieval history

### 2.6 monitoring_system Utilization

```
monitoring_system (Observability Layer)
├── Performance Monitor → Throughput, latency tracking
├── Distributed Tracing → Request flow tracking
├── Health Monitor → Service status check
└── Circuit Breaker → Failure isolation
```

**Monitoring Targets:**
- DICOM Association success/failure rate
- Image storage speed (MB/s)
- Query response time
- Storage usage

### 2.7 network_system Utilization

```
network_system (Network Layer)
├── TCP Server/Client → DICOM Upper Layer Protocol
├── TLS Support → Encrypted communication (DICOM TLS)
├── Session Management → DICOM Association management
└── Async I/O → High-performance network processing
```

**DICOM Network Mapping:**
- `messaging_server` → SCP (Service Class Provider)
- `messaging_client` → SCU (Service Class User)
- `messaging_session` → DICOM Association

---

## 3. DICOM Standard Requirements

### 3.1 DICOM Protocol Layers

```
┌─────────────────────────────────────────┐
│        Application (SOP Classes)        │
│   Storage, Query/Retrieve, Worklist     │
├─────────────────────────────────────────┤
│         DIMSE (Message Exchange)        │
│   C-STORE, C-FIND, C-MOVE, C-GET       │
├─────────────────────────────────────────┤
│     Upper Layer (Association Mgmt)      │
│   A-ASSOCIATE, A-RELEASE, P-DATA       │
├─────────────────────────────────────────┤
│         TCP/IP (Transport)              │
│          Port 104, 11112               │
└─────────────────────────────────────────┘
```

### 3.2 Required Implementation Items

#### 3.2.1 DICOM Data Structures
- **Data Element**: (Group, Element, VR, Length, Value)
- **Data Set**: Ordered collection of Data Elements
- **IOD (Information Object Definition)**: Standard data model
- **SOP Class**: Service-Object Pair

#### 3.2.2 DICOM Network Protocol
- **PDU (Protocol Data Unit)**: A-ASSOCIATE-RQ/AC/RJ, P-DATA-TF, A-RELEASE-RQ/RP, A-ABORT
- **DIMSE Messages**: C-STORE, C-FIND, C-MOVE, C-GET, C-ECHO, N-CREATE, N-SET, N-GET, N-EVENT-REPORT

#### 3.2.3 Transfer Syntax
- **Implicit VR Little Endian** (required)
- **Explicit VR Little Endian** (recommended)
- **Explicit VR Big Endian** (supported)
- Compression: JPEG, JPEG 2000, RLE (optional)

### 3.3 PACS Service Classes

| Service | SOP Class | Role |
|---------|-----------|------|
| **Storage** | Storage SCP | Image reception/storage |
| **Query/Retrieve** | Q/R SCP | Image search/transfer |
| **Worklist** | MWL SCP | Examination schedule query |
| **MPPS** | MPPS SCP | Examination performance status management |
| **Verification** | Verification SCP | Connection test (C-ECHO) |

---

## 4. Implementation Strategy

### 4.1 Module Architecture

```
pacs_system/
├── core/                    # Core DICOM implementation
│   ├── dicom_element.h      # Data Element
│   ├── dicom_dataset.h      # Data Set
│   ├── dicom_file.h         # DICOM File (Part 10)
│   └── dicom_dictionary.h   # Tag Dictionary
│
├── encoding/                # Encoding/Decoding
│   ├── vr_types.h           # Value Representation
│   ├── transfer_syntax.h    # Transfer Syntax
│   ├── implicit_vr.h        # Implicit VR Codec
│   └── explicit_vr.h        # Explicit VR Codec
│
├── network/                 # Network Protocol
│   ├── pdu/                 # Protocol Data Units
│   │   ├── pdu_types.h
│   │   ├── associate_rq.h
│   │   ├── associate_ac.h
│   │   └── p_data.h
│   ├── dimse/               # DIMSE Messages
│   │   ├── dimse_message.h
│   │   ├── c_store.h
│   │   ├── c_find.h
│   │   └── c_move.h
│   └── association.h        # Association Manager
│
├── services/                # DICOM Services
│   ├── storage_scp.h        # Storage SCP
│   ├── storage_scu.h        # Storage SCU
│   ├── qr_scp.h             # Query/Retrieve SCP
│   ├── worklist_scp.h       # Modality Worklist SCP
│   └── mpps_scp.h           # MPPS SCP
│
├── storage/                 # Storage Backend
│   ├── storage_interface.h  # Abstract interface
│   ├── file_storage.h       # File system storage
│   └── database_index.h     # Index DB
│
└── integration/             # Ecosystem Integration
    ├── container_adapter.h  # container_system adapter
    ├── network_adapter.h    # network_system adapter
    └── thread_adapter.h     # thread_system adapter
```

### 4.2 Development Roadmap

#### Phase 1: Foundation (4 weeks)
1. **DICOM Data Structures**
   - Data Element, VR types implementation
   - container_system value types utilization
   - Test: Basic DICOM file read/write

2. **Tag Dictionary**
   - DICOM standard tag definitions (PS3.6)
   - Compile-time constant implementation

#### Phase 2: Network Protocol (6 weeks)
1. **PDU Layer**
   - network_system TCP-based extension
   - A-ASSOCIATE, P-DATA, A-RELEASE implementation

2. **DIMSE Layer**
   - C-ECHO (Verification) implementation
   - C-STORE (Storage) implementation

#### Phase 3: Core Services (6 weeks)
1. **Storage SCP/SCU**
   - Image reception/transmission
   - File system storage

2. **C-FIND Query**
   - Patient/Study/Series/Image level search

#### Phase 4: Advanced Services (4 weeks)
1. **C-MOVE/C-GET**
   - Image transfer requests

2. **Worklist SCP**
   - Examination schedule query

3. **MPPS SCP**
   - Examination performance status management

### 4.3 Existing System Integration Design

```
┌─────────────────────────────────────────────────────────────┐
│                       PACS System                            │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │ Storage SCP │  │ Q/R SCP     │  │ Worklist/MPPS SCP   │  │
│  └──────┬──────┘  └──────┬──────┘  └──────────┬──────────┘  │
│         │                │                     │             │
│  ┌──────▼────────────────▼─────────────────────▼──────────┐ │
│  │                  DIMSE Message Handler                  │ │
│  └──────────────────────────┬──────────────────────────────┘ │
│                             │                                │
│  ┌──────────────────────────▼──────────────────────────────┐ │
│  │              PDU / Association Manager                   │ │
│  │           (extends network_system session)               │ │
│  └──────────────────────────┬──────────────────────────────┘ │
└─────────────────────────────┼───────────────────────────────┘
                              │
          ┌───────────────────┼───────────────────┐
          │                   │                   │
┌─────────▼─────────┐ ┌───────▼───────┐ ┌─────────▼─────────┐
│  network_system   │ │ thread_system │ │ container_system  │
│  (TCP/TLS)        │ │ (Thread Pool) │ │ (Serialization)   │
└───────────────────┘ └───────────────┘ └───────────────────┘
          │                   │                   │
          └───────────────────┼───────────────────┘
                              │
                    ┌─────────▼─────────┐
                    │   common_system   │
                    │   (Foundation)    │
                    └───────────────────┘
```

---

## 5. Technical Challenges

### 5.1 DICOM Complexity

| Challenge | Solution |
|-----------|----------|
| Various VR types (27 types) | container_system value types extension |
| Nested sequences (SQ) | Recursive container structure utilization |
| Transfer Syntax conversion | Separate codec module |
| Large images (several GB) | Streaming processing, memory mapping |

### 5.2 Network Protocol

| Challenge | Solution |
|-----------|----------|
| PDU parsing/generation | Byte stream parser implementation |
| Association state management | FSM (Finite State Machine) |
| Multiple presentation contexts | Negotiation table management |
| Concurrent multiple Associations | thread_system thread pool |

### 5.3 Performance Requirements

| Item | Target | Strategy |
|------|--------|----------|
| Image storage | 100 MB/s | Async I/O, buffering |
| Concurrent connections | 100+ | Connection pooling |
| Query response | <100ms | Index DB, caching |
| Memory usage | <2 GB | Streaming, chunk processing |

---

## 6. Pros and Cons Analysis

### 6.1 Advantages of Not Using External Libraries

| Advantage | Description |
|-----------|-------------|
| **Complete Control** | Manage all code directly, customization freedom |
| **License Freedom** | Remove GPL/LGPL library dependencies |
| **Ecosystem Integration** | Perfect integration with existing systems |
| **Lightweight** | Implement only necessary features |
| **Learning Value** | Deep understanding of DICOM standard |

### 6.2 Disadvantages of Not Using External Libraries

| Disadvantage | Description | Mitigation Strategy |
|--------------|-------------|---------------------|
| **Development Time** | 3-4x longer than using DCMTK | Phased development, MVP first |
| **Standard Compliance** | Risk of bugs, omissions | Thorough testing, Conformance verification |
| **Image Compression** | Complex JPEG/JPEG2000 implementation | Initially support only uncompressed |
| **Maintenance** | Direct response to standard changes | Modularization, documentation |

### 6.3 Comparison with DCMTK

| Item | DCMTK | Custom Implementation |
|------|-------|----------------------|
| Development time | Short | Long (20+ weeks) |
| Standard compliance | Verified | Self-verification required |
| License | BSD (modified distribution terms) | Free |
| Ecosystem integration | Adapter required | Native |
| Customization | Limited | Complete freedom |
| Performance optimization | General | Specializable |

---

## 7. Recommendations

### 7.1 MVP (Minimum Viable Product) Scope

**Stage 1 MVP (12 weeks):**
1. ✅ DICOM file read/write (Part 10)
2. ✅ C-ECHO (connection test)
3. ✅ C-STORE SCP (image reception)
4. ✅ Basic file storage
5. ✅ Basic logging/monitoring

**Stage 2 Extension (8 weeks):**
1. ✅ C-FIND (Query)
2. ✅ C-MOVE (Retrieve)
3. ✅ Index DB
4. ✅ Web viewer integration (WADO)

### 7.2 Alternative Approaches

If you really want to avoid external libraries, consider the following compromises:

1. **Header-only Libraries Only**
   - Use only DICOM parsing without dcmdata

2. **Minimal Feature Libraries**
   - Lightweight implementation supporting only specific SOP classes

3. **Hybrid Approach**
   - Custom implementation for network protocol
   - Separate libraries for image compression (libjpeg, openjpeg)

---

## 8. Conclusion

The existing kcenon ecosystem provides most of the infrastructure needed for PACS implementation:

- **network_system**: DICOM network foundation
- **container_system**: DICOM data serialization
- **thread_system**: Async task processing
- **logger_system**: Audit logging
- **monitoring_system**: Operational monitoring
- **common_system**: Common patterns and error handling

Implementing without external DICOM libraries is **technically feasible** and offers advantages of complete control and ecosystem integration. However, **development time will be significantly longer**, and thorough testing for standard compliance is required.

**Recommended Approach:**
1. Start with MVP scope, prioritizing core functionality
2. Verify standard compliance with Conformance Statement testing
3. Gradually expand functionality
4. Consider external libraries for image compression when needed

---

## Appendix A: DICOM Tag Examples

```cpp
// DICOM standard tag definitions (PS3.6)
namespace dicom::tags {
    // Patient Module
    constexpr Tag PatientName{0x0010, 0x0010};        // PN
    constexpr Tag PatientID{0x0010, 0x0020};          // LO
    constexpr Tag PatientBirthDate{0x0010, 0x0030};   // DA
    constexpr Tag PatientSex{0x0010, 0x0040};         // CS

    // Study Module
    constexpr Tag StudyInstanceUID{0x0020, 0x000D};   // UI
    constexpr Tag StudyDate{0x0008, 0x0020};          // DA
    constexpr Tag StudyTime{0x0008, 0x0030};          // TM
    constexpr Tag AccessionNumber{0x0008, 0x0050};    // SH

    // Series Module
    constexpr Tag SeriesInstanceUID{0x0020, 0x000E};  // UI
    constexpr Tag Modality{0x0008, 0x0060};           // CS
    constexpr Tag SeriesNumber{0x0020, 0x0011};       // IS

    // Image Module
    constexpr Tag SOPInstanceUID{0x0008, 0x0018};     // UI
    constexpr Tag SOPClassUID{0x0008, 0x0016};        // UI
    constexpr Tag InstanceNumber{0x0020, 0x0013};     // IS
    constexpr Tag Rows{0x0028, 0x0010};               // US
    constexpr Tag Columns{0x0028, 0x0011};            // US
    constexpr Tag PixelData{0x7FE0, 0x0010};          // OW/OB
}
```

## Appendix B: Error Code Allocation

```cpp
// common_system error code system extension
// pacs_system: -800 ~ -899

namespace pacs::error_codes {
    // DICOM Parsing Errors (-800 ~ -819)
    constexpr int INVALID_DICOM_FILE = -800;
    constexpr int INVALID_VR = -801;
    constexpr int MISSING_REQUIRED_TAG = -802;
    constexpr int INVALID_TRANSFER_SYNTAX = -803;

    // Association Errors (-820 ~ -839)
    constexpr int ASSOCIATION_REJECTED = -820;
    constexpr int ASSOCIATION_ABORTED = -821;
    constexpr int NO_PRESENTATION_CONTEXT = -822;
    constexpr int INVALID_PDU = -823;

    // DIMSE Errors (-840 ~ -859)
    constexpr int DIMSE_FAILURE = -840;
    constexpr int DIMSE_TIMEOUT = -841;
    constexpr int DIMSE_INVALID_RESPONSE = -842;

    // Storage Errors (-860 ~ -879)
    constexpr int STORAGE_FAILED = -860;
    constexpr int DUPLICATE_SOP_INSTANCE = -861;
    constexpr int INVALID_SOP_CLASS = -862;

    // Query Errors (-880 ~ -899)
    constexpr int QUERY_FAILED = -880;
    constexpr int NO_MATCHES_FOUND = -881;
    constexpr int TOO_MANY_MATCHES = -882;
}
```

---

*Document Version: 1.0.0*
*Created: 2025-01-30*
*Author: kcenon@naver.com*
