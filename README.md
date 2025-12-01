# PACS System

> **Language:** **English** | [한국어](README_KO.md)

## Overview

A production-ready C++20 PACS (Picture Archiving and Communication System) implementation built entirely on the kcenon ecosystem without external DICOM libraries. This project implements the DICOM standard from scratch, leveraging the existing high-performance infrastructure.

**Key Characteristics**:
- **Zero External DICOM Libraries**: Pure implementation using kcenon ecosystem
- **High Performance**: Leveraging SIMD acceleration, lock-free queues, and async I/O
- **Production Grade**: Comprehensive CI/CD, sanitizers, and quality metrics
- **Modular Architecture**: Clean separation of concerns with interface-driven design
- **Cross-Platform**: Linux, macOS, Windows support

---

## Project Status

**Current Phase**: 🎯 Phase 2 Near Complete - Network & Services (80%+)

| Milestone | Status | Target |
|-----------|--------|--------|
| Analysis & Documentation | ✅ Complete | Week 1 |
| Core DICOM Structures | ✅ Complete | Week 2-5 |
| Encoding Module | ✅ Complete | Week 2-5 |
| Storage Backend | ✅ Complete | Week 6-9 |
| Integration Adapters | ✅ Complete | Week 6-9 |
| Network Protocol (PDU) | ✅ Complete | Week 6-9 |
| DIMSE Services | ✅ Complete | Week 10-13 |
| Query/Retrieve | ✅ Complete | Week 14-17 |
| Worklist/MPPS | ✅ Complete | Week 18-20 |
| Advanced Compression | 🔜 Planned | Phase 3 |

**Test Coverage**: 113+ tests passing across 34 test files

### Phase 1 Achievements (Complete)

**Core Module** (57 tests):
- `dicom_tag` - DICOM Tag representation (Group, Element pairs)
- `dicom_element` - Data Element with tag, VR, and value
- `dicom_dataset` - Ordered collection of Data Elements
- `dicom_file` - DICOM Part 10 file read/write
- `dicom_dictionary` - Standard tag metadata lookup (5,000+ tags)

**Encoding Module** (41 tests):
- `vr_type` - All 27 DICOM Value Representation types
- `vr_info` - VR metadata and validation utilities
- `transfer_syntax` - Transfer Syntax management
- `implicit_vr_codec` - Implicit VR Little Endian codec
- `explicit_vr_codec` - Explicit VR Little Endian codec

**Storage Module**:
- `storage_interface` - Abstract storage backend interface
- `file_storage` - Filesystem-based hierarchical storage
- `index_database` - SQLite3 database indexing (~2,900 lines)
- `migration_runner` - Database schema migrations
- Patient/Study/Series/Instance/Worklist/MPPS record management

**Integration Adapters**:
- `container_adapter` - Serialization via container_system
- `network_adapter` - TCP/TLS via network_system
- `thread_adapter` - Concurrency via thread_system
- `logger_adapter` - Audit logging via logger_system
- `monitoring_adapter` - Metrics/tracing via monitoring_system
- `dicom_session` - High-level session management

### Phase 2 Achievements (Complete)

**Network Module** (15 tests):
- `pdu_types` - PDU type definitions (A-ASSOCIATE, P-DATA, etc.)
- `pdu_encoder/decoder` - Binary PDU encoding/decoding (~1,500 lines)
- `association` - Association state machine (~1,300 lines)
- `dicom_server` - TCP server for DICOM connections
- `dimse_message` - DIMSE message handling (~600 lines)

**Services Module** (7 test files):
- `verification_scp` - C-ECHO service (ping/pong)
- `storage_scp/scu` - C-STORE service (store/send)
- `query_scp` - C-FIND service (search)
- `retrieve_scp` - C-MOVE/C-GET service (retrieve)
- `worklist_scp` - Modality Worklist service (MWL)
- `mpps_scp` - Modality Performed Procedure Step

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                       PACS System                            │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │ Storage SCP │  │ Q/R SCP     │  │ Worklist/MPPS SCP   │  │
│  └──────┬──────┘  └──────┬──────┘  └──────────┬──────────┘  │
│         └────────────────┼────────────────────┘             │
│  ┌───────────────────────▼───────────────────────────────┐  │
│  │                  DIMSE Message Handler                 │  │
│  └───────────────────────┬───────────────────────────────┘  │
│  ┌───────────────────────▼───────────────────────────────┐  │
│  │              PDU / Association Manager                 │  │
│  └───────────────────────┬───────────────────────────────┘  │
└──────────────────────────┼──────────────────────────────────┘
                           │
         ┌─────────────────┼─────────────────┐
         │                 │                 │
┌────────▼────────┐ ┌──────▼──────┐ ┌────────▼────────┐
│ network_system  │ │thread_system│ │container_system │
│    (TCP/TLS)    │ │(Thread Pool)│ │ (Serialization) │
└─────────────────┘ └─────────────┘ └─────────────────┘
         │                 │                 │
         └─────────────────┼─────────────────┘
                           │
                  ┌────────▼────────┐
                  │  common_system  │
                  │  (Foundation)   │
                  └─────────────────┘
```

---

## Ecosystem Dependencies

This project leverages the following kcenon ecosystem components:

| System | Purpose | Key Features |
|--------|---------|--------------|
| **common_system** | Foundation interfaces | IExecutor, Result<T>, Event Bus |
| **container_system** | Data serialization | Type-safe values, SIMD acceleration |
| **thread_system** | Concurrency | Thread pools, lock-free queues |
| **logger_system** | Logging | Async logging, 4.34M msg/s |
| **monitoring_system** | Observability | Metrics, distributed tracing |
| **network_system** | Network I/O | TCP/TLS, async operations |

---

## Project Structure

```
pacs_system/
├── include/pacs/
│   ├── core/                    # Core DICOM implementation (✅ Complete)
│   │   ├── dicom_tag.hpp        # Tag representation (Group, Element)
│   │   ├── dicom_tag_constants.hpp # Standard tag constants
│   │   ├── dicom_element.hpp    # Data Element
│   │   ├── dicom_dataset.hpp    # Data Set
│   │   ├── dicom_file.hpp       # DICOM File (Part 10)
│   │   ├── dicom_dictionary.hpp # Tag Dictionary
│   │   └── tag_info.hpp         # Tag metadata
│   │
│   ├── encoding/                # Encoding/Decoding (✅ Complete)
│   │   ├── vr_type.hpp          # Value Representation enum
│   │   ├── vr_info.hpp          # VR metadata and utilities
│   │   ├── transfer_syntax.hpp  # Transfer Syntax
│   │   ├── byte_order.hpp       # Byte order handling
│   │   ├── implicit_vr_codec.hpp # Implicit VR codec
│   │   └── explicit_vr_codec.hpp # Explicit VR codec
│   │
│   ├── network/                 # Network Protocol (✅ Complete)
│   │   ├── pdu_types.hpp        # PDU type definitions
│   │   ├── pdu_encoder.hpp      # PDU encoder
│   │   ├── pdu_decoder.hpp      # PDU decoder
│   │   ├── association.hpp      # Association management
│   │   ├── dicom_server.hpp     # TCP server
│   │   └── dimse/               # DIMSE protocol
│   │       ├── dimse_message.hpp
│   │       ├── command_field.hpp
│   │       └── status_codes.hpp
│   │
│   ├── services/                # DICOM Services (✅ Complete)
│   │   ├── scp_service.hpp      # Base SCP interface
│   │   ├── verification_scp.hpp # C-ECHO SCP
│   │   ├── storage_scp.hpp      # C-STORE SCP
│   │   ├── storage_scu.hpp      # C-STORE SCU
│   │   ├── query_scp.hpp        # C-FIND SCP
│   │   ├── retrieve_scp.hpp     # C-MOVE/GET SCP
│   │   ├── worklist_scp.hpp     # MWL SCP
│   │   └── mpps_scp.hpp         # MPPS SCP
│   │
│   ├── storage/                 # Storage Backend (✅ Complete)
│   │   ├── storage_interface.hpp # Abstract interface
│   │   ├── file_storage.hpp     # Filesystem storage
│   │   ├── index_database.hpp   # SQLite3 indexing
│   │   ├── patient_record.hpp   # Patient data model
│   │   ├── study_record.hpp     # Study data model
│   │   ├── series_record.hpp    # Series data model
│   │   ├── instance_record.hpp  # Instance data model
│   │   ├── worklist_record.hpp  # Worklist data model
│   │   └── mpps_record.hpp      # MPPS data model
│   │
│   └── integration/             # Ecosystem Adapters (✅ Complete)
│       ├── container_adapter.hpp # container_system integration
│       ├── network_adapter.hpp  # network_system integration
│       ├── thread_adapter.hpp   # thread_system integration
│       ├── logger_adapter.hpp   # logger_system integration
│       ├── monitoring_adapter.hpp # monitoring_system integration
│       └── dicom_session.hpp    # High-level session
│
├── src/                         # Source files (~13,500 lines)
│   ├── core/                    # Core implementations (7 files)
│   ├── encoding/                # Encoding implementations (4 files)
│   ├── network/                 # Network implementations (8 files)
│   ├── services/                # Service implementations (7 files)
│   ├── storage/                 # Storage implementations (4 files)
│   └── integration/             # Adapter implementations (6 files)
│
├── tests/                       # Test suites (34 files, 113+ tests)
│   ├── core/                    # Core module tests (6 files)
│   ├── encoding/                # Encoding module tests (5 files)
│   ├── network/                 # Network module tests (5 files)
│   ├── services/                # Service tests (7 files)
│   ├── storage/                 # Storage tests (6 files)
│   └── integration/             # Adapter tests (5 files)
│
├── examples/                    # Example Applications (5 apps, ~2,400 lines)
│   ├── echo_scp/                # DICOM Echo SCP server
│   ├── echo_scu/                # DICOM Echo SCU client
│   ├── store_scu/               # DICOM Storage SCU client
│   ├── query_scu/               # DICOM Query SCU client (C-FIND)
│   └── pacs_server/             # Full PACS server example
│
├── docs/                        # Documentation (30+ files)
└── CMakeLists.txt               # Build configuration (v0.2.0)
```

---

## Documentation

- 📋 [Implementation Analysis](docs/PACS_IMPLEMENTATION_ANALYSIS.md) - Detailed implementation strategy
- 📋 [Product Requirements](docs/PRD.md) - Product Requirements Document
- 🏗️ [Architecture Guide](docs/ARCHITECTURE.md) - System architecture
- ⚡ [Features](docs/FEATURES.md) - Feature specifications
- 📁 [Project Structure](docs/PROJECT_STRUCTURE.md) - Directory structure
- 🔧 [API Reference](docs/API_REFERENCE.md) - API documentation

---

## DICOM Conformance

### Supported SOP Classes

| Service | SOP Class | Status |
|---------|-----------|--------|
| **Verification** | 1.2.840.10008.1.1 | ✅ Complete |
| **CT Storage** | 1.2.840.10008.5.1.4.1.1.2 | ✅ Complete |
| **MR Storage** | 1.2.840.10008.5.1.4.1.1.4 | ✅ Complete |
| **X-Ray Storage** | 1.2.840.10008.5.1.4.1.1.1.1 | ✅ Complete |
| **Patient Root Q/R** | 1.2.840.10008.5.1.4.1.2.1.x | ✅ Complete |
| **Study Root Q/R** | 1.2.840.10008.5.1.4.1.2.2.x | ✅ Complete |
| **Modality Worklist** | 1.2.840.10008.5.1.4.31 | ✅ Complete |
| **MPPS** | 1.2.840.10008.3.1.2.3.3 | ✅ Complete |

### Transfer Syntax Support

| Transfer Syntax | UID | Priority |
|----------------|-----|----------|
| Implicit VR Little Endian | 1.2.840.10008.1.2 | Required |
| Explicit VR Little Endian | 1.2.840.10008.1.2.1 | MVP |
| Explicit VR Big Endian | 1.2.840.10008.1.2.2 | Optional |
| JPEG Baseline | 1.2.840.10008.1.2.4.50 | Future |
| JPEG 2000 | 1.2.840.10008.1.2.4.90 | Future |

---

## Getting Started

### Prerequisites

- C++20 compatible compiler (GCC 11+, Clang 14+, MSVC 2022+)
- CMake 3.20+
- kcenon ecosystem libraries

### Build

```bash
# Clone repository
git clone https://github.com/kcenon/pacs_system.git
cd pacs_system

# Configure and build
cmake -S . -B build
cmake --build build

# Run tests
cd build && ctest --output-on-failure
```

**Test Results**: 113+ tests passing (Core: 57, Encoding: 41, Network: 15, Storage/Integration: 15+)

### Build Options

```cmake
PACS_BUILD_TESTS (ON)              # Enable unit tests
PACS_BUILD_EXAMPLES (OFF)          # Enable example builds
PACS_BUILD_BENCHMARKS (OFF)        # Enable benchmarks
PACS_BUILD_STORAGE (ON)            # Build storage module
```

---

## Examples

### Echo SCP (Verification Server)

```bash
# Build examples
cmake -S . -B build -DPACS_BUILD_EXAMPLES=ON
cmake --build build

# Run Echo SCP
./build/examples/echo_scp/echo_scp --port 11112 --ae-title MY_ECHO
```

### Echo SCU (Verification Client)

```bash
# Test connectivity
./build/examples/echo_scu/echo_scu --host localhost --port 11112 --ae-title TEST_SCU
```

### Storage SCU (Image Sender)

```bash
# Send single DICOM file
./build/examples/store_scu/store_scu localhost 11112 PACS_SCP image.dcm

# Send all files in directory (recursive)
./build/examples/store_scu/store_scu localhost 11112 PACS_SCP ./dicom_folder/ --recursive

# Specify transfer syntax
./build/examples/store_scu/store_scu localhost 11112 PACS_SCP image.dcm --transfer-syntax explicit-le
```

### Query SCU (C-FIND Client)

```bash
# Query studies by patient name (wildcards supported)
./build/bin/query_scu localhost 11112 PACS_SCP --level STUDY --patient-name "DOE^*"

# Query by date range
./build/bin/query_scu localhost 11112 PACS_SCP --level STUDY --study-date "20240101-20241231"

# Query series for a specific study
./build/bin/query_scu localhost 11112 PACS_SCP --level SERIES --study-uid "1.2.3.4.5"

# Output as JSON for integration
./build/bin/query_scu localhost 11112 PACS_SCP --patient-id "12345" --format json

# Export to CSV
./build/bin/query_scu localhost 11112 PACS_SCP --modality CT --format csv > results.csv
```

### Full PACS Server

```bash
# Run with configuration
./build/examples/pacs_server/pacs_server --config pacs_server.yaml
```

**Sample Configuration** (`pacs_server.yaml`):
```yaml
server:
  ae_title: MY_PACS
  port: 11112
  max_associations: 50

storage:
  directory: ./archive
  naming: hierarchical

database:
  path: ./pacs.db
  wal_mode: true
```

---

## Code Statistics

| Metric | Value |
|--------|-------|
| **Header Files** | 48 files |
| **Source Files** | 33 files |
| **Header LOC** | ~13,500 lines |
| **Source LOC** | ~13,800 lines |
| **Example LOC** | ~1,200 lines |
| **Total LOC** | ~27,300 lines |
| **Test Files** | 34 files |
| **Test Cases** | 113+ tests |
| **Documentation** | 26+ markdown files |
| **Version** | 0.2.0 |

---

## Contributing

Contributions are welcome! Please read our contributing guidelines before submitting pull requests.

---

## License

This project is licensed under the BSD 3-Clause License - see the [LICENSE](LICENSE) file for details.

---

## Contact

- **Project Owner**: kcenon (kcenon@naver.com)
- **Repository**: https://github.com/kcenon/pacs_system
- **Issues**: https://github.com/kcenon/pacs_system/issues

---

<p align="center">
  Made with ❤️ by 🍀☀🌕🌥 🌊
</p>
