# PACS System

[![CI](https://github.com/kcenon/pacs_system/actions/workflows/ci.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/ci.yml)
[![Integration Tests](https://github.com/kcenon/pacs_system/actions/workflows/integration-tests.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/integration-tests.yml)
[![Code Coverage](https://github.com/kcenon/pacs_system/actions/workflows/coverage.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/coverage.yml)
[![Static Analysis](https://github.com/kcenon/pacs_system/actions/workflows/static-analysis.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/static-analysis.yml)
[![SBOM Generation](https://github.com/kcenon/pacs_system/actions/workflows/sbom.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/sbom.yml)

> **Language:** **English** | [한국어](README_KO.md)

## Overview

A modern C++20 PACS (Picture Archiving and Communication System) implementation built entirely on the kcenon ecosystem without external DICOM libraries. This project implements the DICOM standard from scratch, leveraging the existing high-performance infrastructure.

**Key Characteristics**:
- **Zero External DICOM Libraries**: Pure implementation using kcenon ecosystem
- **High Performance**: Leveraging SIMD acceleration, lock-free queues, and async I/O
- **Production Grade**: Comprehensive CI/CD, sanitizers, and quality metrics
- **Modular Architecture**: Clean separation of concerns with interface-driven design
- **Cross-Platform**: Linux, macOS, Windows support

---

## Project Status

**Current Phase**: ✅ Phase 2 Complete - Network & Services (100%)

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

**Test Coverage**: 120+ tests passing across 39 test files

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
- `parallel_query_executor` - Parallel batch query execution with timeout

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
│   │   ├── mpps_scp.hpp         # MPPS SCP
│   │   ├── sop_class_registry.hpp # SOP Class registry
│   │   ├── cache/               # Query caching and parallel execution
│   │   │   ├── query_cache.hpp  # LRU query result cache
│   │   │   ├── query_result_stream.hpp # Paginated query streaming
│   │   │   └── parallel_query_executor.hpp # Parallel batch queries
│   │   ├── sop_classes/         # Modality-specific SOP classes
│   │   │   ├── us_storage.hpp   # Ultrasound Storage
│   │   │   └── xa_storage.hpp   # X-Ray Angiographic Storage
│   │   └── validation/          # IOD Validators
│   │       ├── us_iod_validator.hpp # US IOD validation
│   │       └── xa_iod_validator.hpp # XA IOD validation
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
│   ├── monitoring/              # Health Monitoring (✅ Complete)
│   │   ├── health_status.hpp    # Health status structures
│   │   ├── health_checker.hpp   # Health check service
│   │   └── health_json.hpp      # JSON serialization
│   │
│   ├── security/                # Security Features (✅ Complete)
│   │   ├── access_control_manager.hpp # RBAC access control
│   │   ├── role.hpp             # User roles (Viewer, Admin, etc.)
│   │   ├── permission.hpp       # Resource permissions
│   │   ├── signature_types.hpp  # Digital signature types
│   │   ├── certificate.hpp      # X.509 certificate handling
│   │   ├── digital_signature.hpp # DICOM digital signatures
│   │   ├── anonymization_profile.hpp # De-identification profiles (PS3.15)
│   │   ├── tag_action.hpp       # Tag action definitions
│   │   ├── uid_mapping.hpp      # UID mapping for de-identification
│   │   └── anonymizer.hpp       # DICOM anonymization engine
│   │
│   └── integration/             # Ecosystem Adapters (✅ Complete)
│       ├── container_adapter.hpp # container_system integration
│       ├── network_adapter.hpp  # network_system integration
│       ├── thread_adapter.hpp   # thread_system integration
│       ├── logger_adapter.hpp   # logger_system integration
│       ├── monitoring_adapter.hpp # monitoring_system integration
│       └── dicom_session.hpp    # High-level session
│
├── src/                         # Source files (~15,500 lines)
│   ├── core/                    # Core implementations (7 files)
│   ├── encoding/                # Encoding implementations (4 files)
│   ├── network/                 # Network implementations (8 files)
│   ├── services/                # Service implementations (7 files)
│   ├── storage/                 # Storage implementations (4 files)
│   ├── security/                # Security implementations (6 files)
│   ├── monitoring/              # Health check implementations (1 file)
│   └── integration/             # Adapter implementations (6 files)
│
├── tests/                       # Test suites (43 files, 180+ tests)
│   ├── core/                    # Core module tests (6 files)
│   ├── encoding/                # Encoding module tests (5 files)
│   ├── network/                 # Network module tests (5 files)
│   ├── services/                # Service tests (7 files)
│   ├── storage/                 # Storage tests (6 files)
│   ├── security/                # Security tests (5 files, 44 tests)
│   ├── monitoring/              # Health check tests (3 files, 50 tests)
│   └── integration/             # Adapter tests (5 files)
│
├── examples/                    # Example Applications (15 apps, ~10,500 lines)
│   ├── dcm_dump/                # DICOM file inspection utility
│   ├── dcm_modify/              # DICOM tag modification & anonymization utility
│   ├── db_browser/              # PACS index database browser
│   ├── echo_scp/                # DICOM Echo SCP server
│   ├── echo_scu/                # DICOM Echo SCU client
│   ├── secure_dicom/            # TLS-secured DICOM Echo SCU/SCP
│   ├── store_scp/               # DICOM Storage SCP server
│   ├── store_scu/               # DICOM Storage SCU client
│   ├── query_scu/               # DICOM Query SCU client (C-FIND)
│   ├── retrieve_scu/            # DICOM Retrieve SCU client (C-MOVE/C-GET)
│   ├── worklist_scu/            # Modality Worklist Query client (MWL C-FIND)
│   ├── mpps_scu/                # MPPS client (N-CREATE/N-SET)
│   ├── pacs_server/             # Full PACS server example
│   └── integration_tests/       # End-to-end integration test suite
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
- 🚀 [Migration Complete](docs/MIGRATION_COMPLETE.md) - Thread system migration summary

---

## DICOM Conformance

### Supported SOP Classes

| Service | SOP Class | Status |
|---------|-----------|--------|
| **Verification** | 1.2.840.10008.1.1 | ✅ Complete |
| **CT Storage** | 1.2.840.10008.5.1.4.1.1.2 | ✅ Complete |
| **MR Storage** | 1.2.840.10008.5.1.4.1.1.4 | ✅ Complete |
| **US Storage** | 1.2.840.10008.5.1.4.1.1.6.x | ✅ Complete |
| **XA Storage** | 1.2.840.10008.5.1.4.1.1.12.x | ✅ Complete |
| **XRF Storage** | 1.2.840.10008.5.1.4.1.1.12.2 | ✅ Complete |
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

**All Platforms:**
- C++20 compatible compiler with Concepts support:
  - GCC 10+ (GCC 13+ recommended for full std::format support)
  - Clang 10+ (Clang 14+ recommended)
  - MSVC 2022 (19.30+)
- CMake 3.20+
- Ninja (recommended build system)
- kcenon ecosystem libraries (auto-downloaded by CMake)

**Linux (Ubuntu 24.04+):**
```bash
sudo apt install cmake ninja-build libsqlite3-dev libssl-dev libfmt-dev
```

**macOS:**
```bash
brew install cmake ninja sqlite3 openssl@3 fmt
```

**Windows:**
- Visual Studio 2022 with C++ workload
- [vcpkg](https://vcpkg.io/) for package management
- Dependencies: `sqlite3`, `openssl`, `fmt`, `gtest`

### Build

#### Linux/macOS

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

#### Windows

```powershell
# Prerequisites: Visual Studio 2022, vcpkg, CMake 3.20+

# Install dependencies via vcpkg
vcpkg install sqlite3:x64-windows openssl:x64-windows fmt:x64-windows gtest:x64-windows

# Clone repository
git clone https://github.com/kcenon/pacs_system.git
cd pacs_system

# Configure with vcpkg toolchain
cmake -S . -B build -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"

# Build
cmake --build build

# Run tests
cd build
ctest --output-on-failure
```

**Test Results**: 170+ tests passing (Core: 57, Encoding: 41, Network: 15, Services: 7+, Storage/Integration: 20+, Monitoring: 50)

### Build Options

```cmake
PACS_BUILD_TESTS (ON)              # Enable unit tests
PACS_BUILD_EXAMPLES (OFF)          # Enable example builds
PACS_BUILD_BENCHMARKS (OFF)        # Enable benchmarks
PACS_BUILD_STORAGE (ON)            # Build storage module
```

---

## Examples

### Build Examples

```bash
cmake -S . -B build -DPACS_BUILD_EXAMPLES=ON
cmake --build build
```

### DCM Dump (File Inspection Utility)

```bash
# Dump DICOM file metadata
./build/bin/dcm_dump image.dcm

# Filter specific tags
./build/bin/dcm_dump image.dcm --tags PatientName,PatientID,Modality

# Show pixel data information
./build/bin/dcm_dump image.dcm --pixel-info

# JSON output for integration
./build/bin/dcm_dump image.dcm --format json

# Scan directory recursively with summary
./build/bin/dcm_dump ./dicom_folder/ --recursive --summary
```

### DCM Modify (Tag Modification Utility)

```bash
# Modify single tag
./build/bin/dcm_modify image.dcm --set PatientName="Anonymous" -o modified.dcm

# Modify multiple tags
./build/bin/dcm_modify image.dcm \
  --set PatientName="Anonymous" \
  --set PatientID="ANON001" \
  --delete PatientBirthDate \
  -o anonymized.dcm

# Apply basic anonymization (DICOM PS3.15)
./build/bin/dcm_modify image.dcm --anonymize -o anonymized.dcm

# Convert transfer syntax
./build/bin/dcm_modify image.dcm --transfer-syntax explicit-le -o converted.dcm

# Batch anonymize directory
./build/bin/dcm_modify ./input/ --anonymize -o ./output/ --recursive
```

### DB Browser (Database Viewer)

```bash
# List all patients
./build/bin/db_browser pacs.db patients

# List studies for a specific patient
./build/bin/db_browser pacs.db studies --patient-id "12345"

# Filter studies by date range
./build/bin/db_browser pacs.db studies --from 20240101 --to 20241231

# List series for a study
./build/bin/db_browser pacs.db series --study-uid "1.2.3.4.5"

# Show database statistics
./build/bin/db_browser pacs.db stats

# Database maintenance
./build/bin/db_browser pacs.db vacuum
./build/bin/db_browser pacs.db verify
```

### Echo SCP (Verification Server)

```bash
# Run Echo SCP
./build/bin/echo_scp --port 11112 --ae-title MY_ECHO
```

### Echo SCU (Verification Client)

dcmtk-compatible DICOM connectivity verification tool.

```bash
# Basic connectivity test
./build/bin/echo_scu localhost 11112

# With custom AE Titles
./build/bin/echo_scu -aet MY_SCU -aec PACS_SCP localhost 11112

# Verbose output with custom timeout
./build/bin/echo_scu -v -to 60 localhost 11112

# Repeat test for connectivity monitoring
./build/bin/echo_scu -r 10 --repeat-delay 1000 localhost 11112

# Quiet mode (exit code only)
./build/bin/echo_scu -q localhost 11112

# Show all options
./build/bin/echo_scu --help
```

### Secure Echo SCU/SCP (TLS-Secured DICOM)

TLS-secured DICOM connectivity testing with support for TLS 1.2/1.3 and mutual TLS.

```bash
# Generate test certificates first
cd examples/secure_dicom
./generate_certs.sh

# Start secure server (TLS)
./build/bin/secure_echo_scp 2762 MY_PACS \
    --cert certs/server.crt \
    --key certs/server.key \
    --ca certs/ca.crt

# Test secure connectivity (server verification only)
./build/bin/secure_echo_scu localhost 2762 MY_PACS \
    --ca certs/ca.crt

# Test with mutual TLS (client certificate)
./build/bin/secure_echo_scu localhost 2762 MY_PACS \
    --cert certs/client.crt \
    --key certs/client.key \
    --ca certs/ca.crt

# Use TLS 1.3
./build/bin/secure_echo_scu localhost 2762 MY_PACS \
    --ca certs/ca.crt \
    --tls-version 1.3
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

### Retrieve SCU (C-MOVE/C-GET Client)

```bash
# C-GET: Retrieve study directly to local machine
./build/bin/retrieve_scu localhost 11112 PACS_SCP --mode get --study-uid "1.2.3.4.5" -o ./downloads

# C-MOVE: Transfer study to another PACS/workstation
./build/bin/retrieve_scu localhost 11112 PACS_SCP --mode move --dest-ae LOCAL_SCP --study-uid "1.2.3.4.5"

# Retrieve specific series
./build/bin/retrieve_scu localhost 11112 PACS_SCP --level SERIES --series-uid "1.2.3.4.5.6"

# Retrieve all studies for a patient
./build/bin/retrieve_scu localhost 11112 PACS_SCP --level PATIENT --patient-id "12345"

# Flat storage structure (all files in one directory)
./build/bin/retrieve_scu localhost 11112 PACS_SCP --study-uid "1.2.3.4.5" --structure flat
```

### Worklist SCU (Modality Worklist Query Client)

```bash
# Query worklist for CT modality
./build/bin/worklist_scu localhost 11112 RIS_SCP --modality CT

# Query worklist for today's scheduled procedures
./build/bin/worklist_scu localhost 11112 RIS_SCP --modality MR --date today

# Query by station AE title
./build/bin/worklist_scu localhost 11112 RIS_SCP --station "CT_SCANNER_01" --date 20241215

# Query with patient filter
./build/bin/worklist_scu localhost 11112 RIS_SCP --patient-name "DOE^*" --modality CT

# Output as JSON for integration
./build/bin/worklist_scu localhost 11112 RIS_SCP --modality CT --format json > worklist.json

# Export to CSV
./build/bin/worklist_scu localhost 11112 RIS_SCP --modality CT --format csv > worklist.csv
```

### MPPS SCU (Modality Performed Procedure Step Client)

```bash
# Create new MPPS instance (start procedure)
./build/bin/mpps_scu localhost 11112 RIS_SCP create \
  --patient-id "12345" \
  --patient-name "Doe^John" \
  --modality CT

# Complete the procedure
./build/bin/mpps_scu localhost 11112 RIS_SCP set \
  --mpps-uid "1.2.3.4.5.6.7.8" \
  --status COMPLETED \
  --series-uid "1.2.3.4.5.6.7.8.9"

# Discontinue (cancel) a procedure
./build/bin/mpps_scu localhost 11112 RIS_SCP set \
  --mpps-uid "1.2.3.4.5.6.7.8" \
  --status DISCONTINUED \
  --reason "Patient refused"

# Verbose output for debugging
./build/bin/mpps_scu localhost 11112 RIS_SCP create \
  --patient-id "12345" \
  --modality MR \
  --verbose
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

### Integration Tests (End-to-End Workflow Tests)

```bash
# Run all integration tests
./build/bin/pacs_integration_e2e

# Run specific test category
./build/bin/pacs_integration_e2e "[connectivity]"    # Basic C-ECHO tests
./build/bin/pacs_integration_e2e "[store_query]"     # Store and query workflow
./build/bin/pacs_integration_e2e "[worklist]"        # Worklist and MPPS workflow
./build/bin/pacs_integration_e2e "[workflow][multimodal]"  # Multi-modal clinical workflows
./build/bin/pacs_integration_e2e "[xa]"              # XA Storage tests
./build/bin/pacs_integration_e2e "[tls]"             # TLS integration tests
./build/bin/pacs_integration_e2e "[stability][smoke]"  # Quick stability smoke test
./build/bin/pacs_integration_e2e "[stress]"          # Multi-association stress tests

# List available tests
./build/bin/pacs_integration_e2e --list-tests

# Run with verbose output
./build/bin/pacs_integration_e2e --success

# Generate JUnit XML report for CI/CD
./build/bin/pacs_integration_e2e --reporter junit --out results.xml
```

**Test Scenarios**:
- **Connectivity**: C-ECHO, multiple associations, timeout handling
- **Store & Query**: Store files, query by patient/study/series, wildcard matching
- **XA Storage**: X-Ray Angiographic image storage and retrieval
- **Multi-Modal Workflow**: Complete patient journey with CT, MR, XA modalities
- **Worklist/MPPS**: Scheduled procedures, MPPS IN PROGRESS/COMPLETED workflow
- **TLS Security**: Certificate validation, mutual TLS, secure communication
- **Stability**: Memory leak detection, connection pool exhaustion, long-running operations
- **Stress**: Concurrent SCUs, rapid connections, large datasets
- **Error Recovery**: Invalid SOP class, server restart, abort handling

---

## Performance

The PACS system leverages the `thread_system` library for high-performance concurrent operations.
The thread system migration (Epic #153) has been successfully completed, replacing all direct `std::thread` usage with modern C++20 abstractions including jthread support and cancellation tokens.
See [PERFORMANCE_RESULTS.md](docs/PERFORMANCE_RESULTS.md) for detailed benchmark results and [MIGRATION_COMPLETE.md](docs/MIGRATION_COMPLETE.md) for the migration summary.

### Key Performance Metrics

| Metric | Result |
|--------|--------|
| **Association Latency** | < 1 ms |
| **C-ECHO Throughput** | 89,964 msg/s |
| **C-STORE Throughput** | 31,759 store/s |
| **Concurrent Operations** | 124 ops/s (10 workers) |
| **Graceful Shutdown** | 110 ms (with active connections) |
| **Data Rate (512x512)** | 9,247 MB/s |

### Running Benchmarks

```bash
# Build with benchmarks
cmake -B build -DPACS_BUILD_BENCHMARKS=ON
cmake --build build

# Run all benchmarks
cmake --build build --target run_full_benchmarks

# Run specific benchmark category
./build/bin/thread_performance_benchmarks "[benchmark][association]"
./build/bin/thread_performance_benchmarks "[benchmark][throughput]"
./build/bin/thread_performance_benchmarks "[benchmark][concurrent]"
./build/bin/thread_performance_benchmarks "[benchmark][shutdown]"
```

---

## Code Statistics

| Metric | Value |
|--------|-------|
| **Header Files** | 50+ files |
| **Source Files** | 35+ files |
| **Header LOC** | ~14,500 lines |
| **Source LOC** | ~15,000 lines |
| **Example LOC** | ~10,500 lines |
| **Total LOC** | ~40,000 lines |
| **Test Files** | 42 files |
| **Test Cases** | 170+ tests |
| **Documentation** | 30+ markdown files |
| **CI/CD Workflows** | 7 workflows |
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
