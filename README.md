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
| **database_system** | Database abstraction | SQL injection prevention, multi-DB support (Optional) |

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
│   │   ├── tag_info.hpp         # Tag metadata
│   │   └── events.hpp           # Event Bus integration events
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
├── src/                         # Source files (~48,500 lines)
│   ├── core/                    # Core implementations (7 files)
│   ├── encoding/                # Encoding implementations (4 files)
│   ├── network/                 # Network implementations (8 files)
│   ├── services/                # Service implementations (7 files)
│   ├── storage/                 # Storage implementations (4 files)
│   ├── security/                # Security implementations (6 files)
│   ├── monitoring/              # Health check implementations (1 file)
│   └── integration/             # Adapter implementations (6 files)
│
├── tests/                       # Test suites (102 files, 170+ tests)
│   ├── core/                    # Core module tests (6 files)
│   ├── encoding/                # Encoding module tests (5 files)
│   ├── network/                 # Network module tests (5 files)
│   ├── services/                # Service tests (7 files)
│   ├── storage/                 # Storage tests (6 files)
│   ├── security/                # Security tests (5 files, 44 tests)
│   ├── monitoring/              # Health check tests (3 files, 50 tests)
│   └── integration/             # Adapter tests (5 files)
│
├── examples/                    # Example Applications (30 apps, ~35,600 lines)
│   ├── dcm_dump/                # DICOM file inspection utility
│   ├── dcm_info/                # DICOM file summary utility
│   ├── dcm_conv/                # Transfer Syntax conversion utility
│   ├── dcm_modify/              # DICOM tag modification utility
│   ├── dcm_anonymize/           # DICOM de-identification utility (PS3.15)
│   ├── dcm_dir/                 # DICOMDIR creation/management utility (PS3.10)
│   ├── dcm_to_json/             # DICOM to JSON conversion utility (PS3.18)
│   ├── json_to_dcm/             # JSON to DICOM conversion utility (PS3.18)
│   ├── dcm_to_xml/              # DICOM to XML conversion utility (PS3.19)
│   ├── xml_to_dcm/              # XML to DICOM conversion utility (PS3.19)
│   ├── img_to_dcm/              # Image to DICOM conversion utility
│   ├── dcm_extract/             # DICOM pixel data extraction utility
│   ├── db_browser/              # PACS index database browser
│   ├── echo_scp/                # DICOM Echo SCP server
│   ├── echo_scu/                # DICOM Echo SCU client
│   ├── secure_dicom/            # TLS-secured DICOM Echo SCU/SCP
│   ├── store_scp/               # DICOM Storage SCP server
│   ├── store_scu/               # DICOM Storage SCU client
│   ├── qr_scp/                  # Query/Retrieve SCP (C-FIND/C-MOVE/C-GET server)
│   ├── query_scu/               # DICOM Query SCU client (C-FIND)
│   ├── find_scu/                # dcmtk-compatible C-FIND SCU utility
│   ├── retrieve_scu/            # DICOM Retrieve SCU client (C-MOVE/C-GET)
│   ├── move_scu/                # dcmtk-compatible C-MOVE SCU utility
│   ├── get_scu/                 # dcmtk-compatible C-GET SCU utility
│   ├── worklist_scu/            # Modality Worklist Query client (MWL C-FIND)
│   ├── worklist_scp/            # Modality Worklist SCP server (MWL C-FIND)
│   ├── mpps_scu/                # MPPS client (N-CREATE/N-SET)
│   ├── mpps_scp/                # MPPS server (N-CREATE/N-SET)
│   ├── pacs_server/             # Full PACS server example
│   └── integration_tests/       # End-to-end integration test suite
│
├── docs/                        # Documentation (37 files)
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

**Database Integration:**
- 🗄️ [Migration Guide](docs/database/MIGRATION_GUIDE.md) - database_system integration guide
- 📚 [API Reference (Database)](docs/database/API_REFERENCE.md) - Query Builder API documentation
- 🏛️ [ADR-001](docs/adr/ADR-001-database-system-integration.md) - Architecture Decision Record
- ⚡ [Performance Guide](docs/database/PERFORMANCE_GUIDE.md) - Database optimization tips

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

### Windows Development Notes

When writing code for Windows compatibility, wrap `std::min` and `std::max` calls in parentheses to prevent conflicts with Windows.h macros:

```cpp
// Correct: Works on all platforms
size_t result = (std::min)(a, b);
auto value = (std::max)(x, y);

// Incorrect: Fails on Windows MSVC (error C2589)
size_t result = std::min(a, b);
auto value = std::max(x, y);
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

### DCM Info (File Summary Utility)

```bash
# Display DICOM file summary
./build/bin/dcm_info image.dcm

# Verbose mode with all available fields
./build/bin/dcm_info image.dcm --verbose

# JSON output for scripting
./build/bin/dcm_info image.dcm --format json

# Quick view of multiple files
./build/bin/dcm_info ./dicom_folder/ -r -q
```

### DCM Modify (Tag Modification Utility)

dcmtk-compatible DICOM tag modification utility supporting numeric tag format `(GGGG,EEEE)` and keyword format.

```bash
# Insert tag (creates if not exists) - supports both tag formats
./build/bin/dcm_modify -i "(0010,0010)=Anonymous" patient.dcm
./build/bin/dcm_modify -i PatientName=Anonymous -o modified.dcm patient.dcm

# Modify existing tag (error if not exists)
./build/bin/dcm_modify -m "(0010,0020)=NEW_ID" patient.dcm

# Delete tag
./build/bin/dcm_modify -e "(0010,1000)" patient.dcm
./build/bin/dcm_modify -e OtherPatientIDs patient.dcm

# Delete all matching tags (including in sequences)
./build/bin/dcm_modify -ea "(0010,1001)" patient.dcm

# Delete all private tags
./build/bin/dcm_modify -ep patient.dcm

# Regenerate UIDs
./build/bin/dcm_modify -gst -gse -gin -o anonymized.dcm patient.dcm

# Use script file for batch modifications
./build/bin/dcm_modify --script modify.txt *.dcm

# In-place modification (creates .bak backup)
./build/bin/dcm_modify -i PatientID=NEW_ID patient.dcm

# In-place without backup (DANGEROUS!)
./build/bin/dcm_modify -i PatientID=NEW_ID -nb patient.dcm

# Process directory recursively
./build/bin/dcm_modify -i PatientName=Anonymous -r ./dicom_folder/ -o ./output/
```

Script file format (`modify.txt`):
```
# Comments start with #
i (0010,0010)=Anonymous     # Insert/modify tag
m (0008,0050)=ACC001        # Modify existing tag
e (0010,1000)               # Erase tag
ea (0010,1001)              # Erase all matching tags
```

### DCM Conv (Transfer Syntax Converter)

```bash
# Convert to Explicit VR Little Endian (default)
./build/bin/dcm_conv image.dcm converted.dcm --explicit

# Convert to Implicit VR Little Endian
./build/bin/dcm_conv image.dcm output.dcm --implicit

# Convert to JPEG Baseline with quality setting
./build/bin/dcm_conv image.dcm compressed.dcm --jpeg-baseline -q 85

# Convert directory recursively with verification
./build/bin/dcm_conv ./input_dir/ ./output_dir/ --recursive --verify

# List all supported Transfer Syntaxes
./build/bin/dcm_conv --list-syntaxes

# Convert with explicit Transfer Syntax UID
./build/bin/dcm_conv image.dcm output.dcm -t 1.2.840.10008.1.2.4.50
```

### DCM Anonymize (De-identification Utility)

DICOM de-identification utility compliant with DICOM PS3.15 Security Profiles.

```bash
# Basic anonymization (removes direct identifiers)
./build/bin/dcm_anonymize patient.dcm anonymous.dcm

# HIPAA Safe Harbor compliance (18-identifier removal)
./build/bin/dcm_anonymize --profile hipaa_safe_harbor patient.dcm output.dcm

# GDPR-compliant pseudonymization
./build/bin/dcm_anonymize --profile gdpr_compliant patient.dcm output.dcm

# Keep specific tags unchanged
./build/bin/dcm_anonymize -k PatientSex -k PatientAge patient.dcm output.dcm

# Replace tags with custom values
./build/bin/dcm_anonymize -r "InstitutionName=Research Hospital" patient.dcm output.dcm

# Set new patient identifiers
./build/bin/dcm_anonymize --patient-id "STUDY001_001" --patient-name "Anonymous" patient.dcm

# Use UID mapping for consistent anonymization across study files
./build/bin/dcm_anonymize -m mapping.json patient.dcm output.dcm

# Shift dates for longitudinal studies
./build/bin/dcm_anonymize --profile retain_longitudinal --date-offset -30 patient.dcm

# Batch processing with directory recursion
./build/bin/dcm_anonymize --recursive -o anonymized/ ./originals/

# Dry-run mode to preview changes
./build/bin/dcm_anonymize --dry-run --verbose patient.dcm

# Verify anonymization completeness
./build/bin/dcm_anonymize --verify patient.dcm anonymous.dcm
```

Available anonymization profiles:
- `basic` - Remove direct patient identifiers (default)
- `clean_pixel` - Remove burned-in annotations from pixel data
- `clean_descriptions` - Clean free-text fields that may contain PHI
- `retain_longitudinal` - Preserve temporal relationships with date shifting
- `retain_patient_characteristics` - Keep demographics (sex, age, size, weight)
- `hipaa_safe_harbor` - Full HIPAA 18-identifier removal
- `gdpr_compliant` - GDPR pseudonymization requirements

### DCM Dir (DICOMDIR Creation/Management Utility)

Create and manage DICOMDIR files for DICOM media storage following DICOM PS3.10 standard.

```bash
# Create DICOMDIR from directory
./build/bin/dcm_dir create ./patient_data/

# Create with custom output path and file-set ID
./build/bin/dcm_dir create -o DICOMDIR --file-set-id "STUDY001" ./patient_data/

# Create with verbose output
./build/bin/dcm_dir create -v ./patient_data/

# List DICOMDIR contents in tree format
./build/bin/dcm_dir list DICOMDIR

# List with detailed information
./build/bin/dcm_dir list -l DICOMDIR

# List as flat file list
./build/bin/dcm_dir list --flat DICOMDIR

# Verify DICOMDIR structure
./build/bin/dcm_dir verify DICOMDIR

# Verify with file existence check
./build/bin/dcm_dir verify --check-files DICOMDIR

# Verify with consistency check (duplicate SOP Instance UID detection)
./build/bin/dcm_dir verify --check-consistency DICOMDIR

# Update DICOMDIR by adding new files
./build/bin/dcm_dir update -a ./new_study/ DICOMDIR
```

DICOMDIR structure follows DICOM hierarchy:
- PATIENT → STUDY → SERIES → IMAGE (hierarchical record structure)
- Referenced File IDs use backslash separator for ISO 9660 compatibility
- Supports standard record types: PATIENT, STUDY, SERIES, IMAGE

### DCM to JSON (DICOM PS3.18 JSON Converter)

Convert DICOM files to JSON format following the DICOM PS3.18 JSON representation standard.

```bash
# Convert DICOM to JSON (stdout)
./build/bin/dcm_to_json image.dcm

# Convert to file with pretty formatting
./build/bin/dcm_to_json image.dcm output.json --pretty

# Compact output (no formatting)
./build/bin/dcm_to_json image.dcm output.json --compact

# Include binary data as Base64
./build/bin/dcm_to_json image.dcm output.json --bulk-data inline

# Save binary data to separate files with URI references
./build/bin/dcm_to_json image.dcm output.json --bulk-data uri --bulk-data-dir ./bulk/

# Exclude pixel data
./build/bin/dcm_to_json image.dcm output.json --no-pixel

# Filter specific tags
./build/bin/dcm_to_json image.dcm -t 0010,0010 -t 0010,0020

# Process directory recursively
./build/bin/dcm_to_json ./dicom_folder/ --recursive --no-pixel
```

Output format (DICOM PS3.18):
```json
{
  "00100010": {
    "vr": "PN",
    "Value": [{"Alphabetic": "DOE^JOHN"}]
  },
  "00100020": {
    "vr": "LO",
    "Value": ["12345678"]
  }
}
```

### JSON to DCM (JSON to DICOM Converter)

Convert JSON files (DICOM PS3.18 format) back to DICOM format.

```bash
# Convert JSON to DICOM
./build/bin/json_to_dcm metadata.json output.dcm

# Use template DICOM for pixel data and missing tags
./build/bin/json_to_dcm metadata.json output.dcm --template original.dcm

# Specify transfer syntax
./build/bin/json_to_dcm metadata.json output.dcm -t 1.2.840.10008.1.2.1

# Resolve BulkDataURI from specific directory
./build/bin/json_to_dcm metadata.json output.dcm --bulk-data-dir ./bulk/
```

### DCM to XML (DICOM to XML Conversion)

Convert DICOM files to DICOM Native XML format (PS3.19).

```bash
# Convert DICOM to XML (stdout)
./build/bin/dcm_to_xml image.dcm

# Convert to file with pretty formatting
./build/bin/dcm_to_xml image.dcm output.xml --pretty

# Include binary data as Base64
./build/bin/dcm_to_xml image.dcm output.xml --bulk-data inline

# Save binary data to separate files with URI references
./build/bin/dcm_to_xml image.dcm output.xml --bulk-data uri --bulk-data-dir ./bulk/

# Exclude pixel data
./build/bin/dcm_to_xml image.dcm output.xml --no-pixel

# Filter specific tags
./build/bin/dcm_to_xml image.dcm -t 0010,0010 -t 0010,0020

# Process directory recursively
./build/bin/dcm_to_xml ./dicom_folder/ --recursive --no-pixel
```

Output format (DICOM Native XML PS3.19):
```xml
<?xml version="1.0" encoding="UTF-8"?>
<NativeDicomModel xmlns="http://dicom.nema.org/PS3.19/models/NativeDICOM">
  <DicomAttribute tag="00100010" vr="PN" keyword="PatientName">
    <PersonName number="1">
      <Alphabetic>
        <FamilyName>DOE</FamilyName>
        <GivenName>JOHN</GivenName>
      </Alphabetic>
    </PersonName>
  </DicomAttribute>
</NativeDicomModel>
```

### XML to DCM (XML to DICOM Conversion)

Convert XML files (DICOM Native XML PS3.19 format) back to DICOM format.

```bash
# Convert XML to DICOM
./build/bin/xml_to_dcm metadata.xml output.dcm

# Use template DICOM for pixel data and missing tags
./build/bin/xml_to_dcm metadata.xml output.dcm --template original.dcm

# Specify transfer syntax
./build/bin/xml_to_dcm metadata.xml output.dcm -t 1.2.840.10008.1.2.1

# Resolve BulkData URI from specific directory
./build/bin/xml_to_dcm metadata.xml output.dcm --bulk-data-dir ./bulk/
```

### Img to DCM (Image to DICOM Conversion)

Convert standard image files (JPEG) to DICOM format using Secondary Capture SOP Class.

```bash
# Basic conversion
./build/bin/img_to_dcm photo.jpg output.dcm

# With patient metadata
./build/bin/img_to_dcm photo.jpg output.dcm \
  --patient-name "DOE^JOHN" \
  --patient-id "12345" \
  --study-description "Photograph"

# Convert directory of images
./build/bin/img_to_dcm ./photos/ ./dicom/ --recursive

# With verbose output
./build/bin/img_to_dcm photo.jpg output.dcm -v

# Overwrite existing files
./build/bin/img_to_dcm ./photos/ ./dicom/ --recursive --overwrite
```

Features:
- Converts JPEG images to DICOM Secondary Capture format
- Automatic UID generation for Study, Series, and Instance
- Customizable patient and study metadata
- Batch processing with recursive directory support
- Requires libjpeg-turbo for JPEG support

### DCM Extract (Pixel Data Extraction)

Extract pixel data from DICOM files to standard image formats.

```bash
# Show pixel data information
./build/bin/dcm_extract image.dcm --info

# Extract to raw binary format
./build/bin/dcm_extract image.dcm output.raw --raw

# Extract to JPEG (requires libjpeg)
./build/bin/dcm_extract image.dcm output.jpg --jpeg -q 90

# Extract to PNG (requires libpng)
./build/bin/dcm_extract image.dcm output.png --png

# Extract to PPM/PGM format
./build/bin/dcm_extract image.dcm output.ppm --ppm

# Apply window level transformation
./build/bin/dcm_extract image.dcm output.jpg --jpeg --window 40 400

# Batch extraction from directory
./build/bin/dcm_extract ./dicom/ ./images/ --recursive --jpeg
```

Features:
- Supports RAW, JPEG, PNG, and PPM/PGM output formats
- Window level (center/width) transformation
- Automatic 16-bit to 8-bit conversion
- MONOCHROME1/MONOCHROME2 handling
- Batch processing with recursive directory support

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
./build/bin/store_scu localhost 11112 image.dcm

# Send with custom AE Titles
./build/bin/store_scu -aet MYSCU -aec PACS localhost 11112 image.dcm

# Send all files in directory (recursive) with progress
./build/bin/store_scu -r --progress localhost 11112 ./dicom_folder/

# Specify transfer syntax preference
./build/bin/store_scu --prefer-lossless localhost 11112 *.dcm

# Verbose output with timeout
./build/bin/store_scu -v -to 60 localhost 11112 image.dcm

# Generate transfer report
./build/bin/store_scu --report-file transfer.log localhost 11112 ./data/

# Quiet mode (minimal output)
./build/bin/store_scu -q localhost 11112 image.dcm

# Show help
./build/bin/store_scu --help
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

### DCMTK-Compatible SCU Utilities

The following utilities provide dcmtk-compatible command-line interfaces for interoperability with existing DICOM toolchains:

#### find_scu (dcmtk-compatible C-FIND)

```bash
# Patient Root Query - find all studies for a patient
./build/bin/find_scu -P -L STUDY -k "0010,0010=Smith*" localhost 11112

# Study Root Query - find CT studies in date range
./build/bin/find_scu -S -L STUDY \
  -aec PACS_SCP \
  -k "0008,0060=CT" \
  -k "0008,0020=20240101-20241231" \
  localhost 11112

# Output as JSON
./build/bin/find_scu -S -L SERIES -k "0020,000D=1.2.840..." -o json localhost 11112

# Read query keys from file
./build/bin/find_scu -f query_keys.txt localhost 11112
```

#### move_scu (dcmtk-compatible C-MOVE)

```bash
# Move study to third-party workstation
./build/bin/move_scu -aem WORKSTATION \
  -L STUDY \
  -k "0020,000D=1.2.840..." \
  pacs.example.com 104

# Move series with progress display
./build/bin/move_scu -aem ARCHIVE \
  --progress \
  -L SERIES \
  -k "0020,000E=1.2.840..." \
  localhost 11112
```

#### get_scu (dcmtk-compatible C-GET)

```bash
# Get entire study directly (no separate storage SCP needed)
./build/bin/get_scu -L STUDY \
  -k "0020,000D=1.2.840..." \
  --progress \
  -od ./study_data/ \
  pacs.example.com 104

# Get single instance with lossless preference
./build/bin/get_scu --prefer-lossless \
  -L IMAGE \
  -k "0008,0018=1.2.840..." \
  -od ./retrieved/ \
  localhost 11112
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

### MPPS SCP (Modality Performed Procedure Step Server)

A standalone MPPS server for receiving procedure status updates from modality devices.

```bash
# Basic usage - listen for MPPS messages
./build/bin/mpps_scp 11112 MY_MPPS

# Store MPPS records to individual JSON files
./build/bin/mpps_scp 11112 MY_MPPS --output-dir ./mpps_records

# Append MPPS records to a single JSON file
./build/bin/mpps_scp 11112 MY_MPPS --output-file ./mpps.json

# With custom association limits
./build/bin/mpps_scp 11112 MY_MPPS --max-assoc 20 --timeout 600

# Show help
./build/bin/mpps_scp --help
```

Features:
- **N-CREATE**: Receive procedure start notifications (status = IN PROGRESS)
- **N-SET**: Receive procedure completion (COMPLETED) or cancellation (DISCONTINUED)
- **JSON Storage**: Store MPPS records to JSON files for integration
- **Statistics**: Display session statistics on shutdown
- **Graceful Shutdown**: Signal handling (SIGINT, SIGTERM)

### Query/Retrieve SCP (C-FIND/C-MOVE/C-GET Server)

Lightweight Query/Retrieve SCP server for serving DICOM files from a storage directory.

```bash
# Basic usage - serve files from a directory
./build/bin/qr_scp 11112 MY_PACS --storage-dir ./dicom

# With persistent index database (faster restart)
./build/bin/qr_scp 11112 MY_PACS --storage-dir ./dicom --index-db ./pacs.db

# With known peers for C-MOVE destinations
./build/bin/qr_scp 11112 MY_PACS --storage-dir ./dicom --peer VIEWER:192.168.1.10:11113

# Multiple peers for multi-destination C-MOVE
./build/bin/qr_scp 11112 MY_PACS --storage-dir ./dicom \
  --peer WS1:10.0.0.1:104 --peer WS2:10.0.0.2:104

# Scan and index storage without starting server
./build/bin/qr_scp 11112 MY_PACS --storage-dir ./dicom --scan-only

# Customize association limits
./build/bin/qr_scp 11112 MY_PACS --storage-dir ./dicom --max-assoc 20 --timeout 600
```

Features:
- **C-FIND**: Query at Patient, Study, Series, and Image levels
- **C-MOVE**: Send images to configured destination AE titles
- **C-GET**: Direct image retrieval over the same association
- **Automatic Indexing**: SQLite-based fast query responses
- **Graceful Shutdown**: Signal handling (SIGINT, SIGTERM)

### Modality Worklist SCP

A standalone Modality Worklist (MWL) server for providing scheduled procedure information to modalities.

```bash
# Basic usage with JSON worklist file
./build/bin/worklist_scp 11112 MY_WORKLIST --worklist-file ./worklist.json

# Load worklist from directory
./build/bin/worklist_scp 11112 MY_WORKLIST --worklist-dir ./worklist_data

# With result limit
./build/bin/worklist_scp 11112 MY_WORKLIST --worklist-file ./worklist.json --max-results 100
```

**Sample Worklist JSON** (`worklist.json`):
```json
[
  {
    "patientId": "12345",
    "patientName": "DOE^JOHN",
    "patientBirthDate": "19800101",
    "patientSex": "M",
    "studyInstanceUid": "1.2.3.4.5.6.7.8.9",
    "accessionNumber": "ACC001",
    "scheduledStationAeTitle": "CT_01",
    "scheduledProcedureStepStartDate": "20241220",
    "scheduledProcedureStepStartTime": "100000",
    "modality": "CT",
    "scheduledProcedureStepId": "SPS001",
    "scheduledProcedureStepDescription": "CT Abdomen"
  }
]
```

Features:
- **MWL C-FIND**: Responds to Modality Worklist queries
- **JSON Data Source**: Load worklist items from JSON files
- **Filtering**: Patient ID, name, modality, station AE, scheduled date
- **Graceful Shutdown**: Signal handling (SIGINT, SIGTERM)

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

### Event Bus Integration

The PACS system integrates with `common_system` Event Bus for inter-module communication and event-driven workflows.

```cpp
#include "pacs/core/events.hpp"
#include <kcenon/common/patterns/event_bus.h>

// Subscribe to image storage events
auto& bus = kcenon::common::get_event_bus();
auto sub_id = bus.subscribe<pacs::events::image_received_event>(
    [](const pacs::events::image_received_event& evt) {
        std::cout << "Received image: " << evt.sop_instance_uid
                  << " from " << evt.calling_ae << std::endl;
        // Trigger workflow, update cache, send notification, etc.
    }
);

// Subscribe to association events
bus.subscribe<pacs::events::association_established_event>(
    [](const pacs::events::association_established_event& evt) {
        std::cout << "New association: " << evt.calling_ae
                  << " -> " << evt.called_ae << std::endl;
    }
);

// Cleanup when done
bus.unsubscribe(sub_id);
```

**Available Event Types**:
- `association_established_event` / `association_released_event` / `association_aborted_event`
- `image_received_event` / `storage_failed_event`
- `query_executed_event` / `query_failed_event`
- `retrieve_started_event` / `retrieve_completed_event`

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

<!-- STATS_START -->

| Metric | Value |
|--------|-------|
| **Header Files** | 157 files |
| **Source Files** | 98 files |
| **Header LOC** | ~44,800 lines |
| **Source LOC** | ~52,800 lines |
| **Example LOC** | ~34,700 lines |
| **Test LOC** | ~51,200 lines |
| **Total LOC** | ~183,500 lines |
| **Test Files** | 102 files |
| **Test Cases** | 1534+ tests |
| **Example Programs** | 30 apps |
| **Documentation** | 43 markdown files |
| **CI/CD Workflows** | 10 workflows |
| **Version** | 0.1.0 |
| **Last Updated** | 2026-01-01 |

<!-- STATS_END -->

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
