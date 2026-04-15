---
doc_id: "PAC-PROJ-005"
doc_title: "Project Structure - PACS System"
doc_version: "0.1.3.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "pacs_system"
category: "PROJ"
---

# Project Structure - PACS System

> **SSOT**: This document is the single source of truth for **Project Structure - PACS System**.

> **Version:** 0.1.3.0
> **Last Updated:** 2025-12-13
> **Language:** **English** | [한국어](PROJECT_STRUCTURE.kr.md)

---

## Table of Contents

- [Overview](#overview)
- [Directory Layout](#directory-layout)
- [Module Descriptions](#module-descriptions)
- [Header Files](#header-files)
- [Source Files](#source-files)
- [Build System](#build-system)
- [Documentation](#documentation)
- [Testing Structure](#testing-structure)

---

## Overview

The PACS System follows a modular directory structure consistent with other kcenon ecosystem projects. Each module has a clear responsibility and interfaces are defined through header files in the `include/` directory.

---

## Directory Layout

```
pacs_system/
├── README.md                   # Project overview
├── README.kr.md                # Korean version
├── LICENSE                     # BSD 3-Clause License
├── CMakeLists.txt              # Root CMake configuration
├── .gitignore                  # Git ignore patterns
├── .clang-format               # Code formatting rules
├── .clang-tidy                 # Static analysis rules
│
├── web/                        # Web administration frontend
│   ├── src/                    # React TypeScript source
│   │   ├── api/                # API client
│   │   ├── components/         # React components
│   │   ├── hooks/              # Custom hooks
│   │   ├── lib/                # Utilities
│   │   ├── pages/              # Page components
│   │   └── types/              # TypeScript definitions
│   ├── package.json            # Node.js dependencies
│   ├── vite.config.ts          # Vite configuration
│   └── tailwind.config.js      # TailwindCSS configuration
│
├── include/                    # Public header files
│   └── pacs/
│       ├── core/               # Core DICOM structures
│       │   ├── dicom_element.hpp
│       │   ├── dicom_dataset.hpp
│       │   ├── dicom_file.hpp
│       │   ├── dicom_dictionary.hpp
│       │   ├── dicom_tag.hpp
│       │   ├── dicom_tag_constants.hpp
│       │   └── tag_info.hpp
│       │
│       ├── encoding/           # VR encoding/decoding
│       │   ├── vr_type.hpp
│       │   ├── vr_info.hpp
│       │   ├── transfer_syntax.hpp
│       │   ├── implicit_vr_codec.hpp
│       │   ├── explicit_vr_codec.hpp
│       │   └── byte_order.hpp
│       │
│       ├── network/            # Network protocol
│       │   ├── pdu_types.hpp       # PDU type definitions
│       │   ├── pdu_encoder.hpp     # PDU serialization
│       │   ├── pdu_decoder.hpp     # PDU deserialization
│       │   ├── association.hpp     # Association state machine
│       │   ├── dicom_server.hpp    # Multi-association server
│       │   ├── server_config.hpp   # Server configuration
│       │   │
│       │   ├── detail/             # Internal implementation
│       │   │   └── accept_worker.hpp   # Accept loop (thread_base) [NEW v1.1.0]
│       │   │
│       │   ├── v2/                 # network_system V2 [NEW v1.1.0]
│       │   │   ├── dicom_server_v2.hpp           # messaging_server-based
│       │   │   └── dicom_association_handler.hpp # Per-session handler
│       │   │
│       │   └── dimse/              # DIMSE protocol
│       │       ├── dimse_message.hpp
│       │       ├── command_field.hpp
│       │       └── status_codes.hpp
│       │
│       ├── services/           # DICOM services
│       │   ├── scp_service.hpp           # Base SCP interface
│       │   ├── verification_scp.hpp      # C-ECHO SCP
│       │   ├── storage_scp.hpp           # C-STORE SCP
│       │   ├── storage_scu.hpp           # C-STORE SCU
│       │   ├── storage_status.hpp        # Storage status codes
│       │   ├── query_scp.hpp             # C-FIND SCP
│       │   ├── retrieve_scp.hpp          # C-MOVE/C-GET SCP
│       │   ├── worklist_scp.hpp          # MWL SCP
│       │   ├── mpps_scp.hpp              # MPPS SCP
│       │   ├── sop_class_registry.hpp    # Central SOP Class registry
│       │   │
│       │   ├── sop_classes/              # SOP Class definitions
│       │   │   ├── us_storage.hpp        # US Storage SOP Classes
│       │   │   └── xa_storage.hpp        # XA/XRF Storage SOP Classes
│       │   │
│       │   └── validation/               # IOD Validators
│       │       └── xa_iod_validator.hpp  # XA IOD validation
│       │
│       ├── storage/            # Storage backend
│       │   ├── storage_interface.hpp # Abstract storage interface
│       │   ├── file_storage.hpp      # Filesystem storage
│       │   ├── index_database.hpp    # SQLite index (~2,900 lines)
│       │   ├── migration_runner.hpp  # Database schema migrations
│       │   ├── migration_record.hpp  # Migration history record
│       │   ├── patient_record.hpp    # Patient data model
│       │   ├── study_record.hpp      # Study data model
│       │   ├── series_record.hpp     # Series data model
│       │   ├── instance_record.hpp   # Instance data model
│       │   ├── worklist_record.hpp   # Worklist data model
│       │   └── mpps_record.hpp       # MPPS data model
│       │
│       ├── ai/                 # AI result handling [NEW v1.3.0]
│       │   └── ai_result_handler.hpp # Handler for AI-generated DICOM objects (SR, SEG, PR)
│       │
│       └── integration/        # Ecosystem adapters
│           ├── container_adapter.hpp # container_system integration
│           ├── network_adapter.hpp   # network_system integration
│           ├── thread_adapter.hpp    # thread_system integration
│           ├── logger_adapter.hpp    # logger_system integration
│           ├── monitoring_adapter.hpp # monitoring_system integration
│           └── dicom_session.hpp     # High-level session management
│
├── src/                        # Implementation files
│   ├── core/
│   │   ├── dicom_element.cpp
│   │   ├── dicom_dataset.cpp
│   │   ├── dicom_file.cpp
│   │   ├── dicom_dictionary.cpp
│   │   └── CMakeLists.txt
│   │
│   ├── encoding/
│   │   ├── vr_types.cpp
│   │   ├── transfer_syntax.cpp
│   │   ├── implicit_vr_codec.cpp
│   │   ├── explicit_vr_codec.cpp
│   │   └── CMakeLists.txt
│   │
│   ├── network/
│   │   ├── pdu/
│   │   │   ├── pdu_encoder.cpp
│   │   │   ├── pdu_decoder.cpp
│   │   │   └── CMakeLists.txt
│   │   │
│   │   ├── dimse/
│   │   │   ├── dimse_message.cpp
│   │   │   ├── c_echo.cpp
│   │   │   ├── c_store.cpp
│   │   │   ├── c_find.cpp
│   │   │   └── CMakeLists.txt
│   │   │
│   │   ├── association.cpp
│   │   ├── dicom_server.cpp
│   │   └── CMakeLists.txt
│   │
│   ├── services/
│   │   ├── verification_scp.cpp
│   │   ├── storage_scp.cpp
│   │   ├── query_scp.cpp
│   │   ├── sop_class_registry.cpp
│   │   ├── sop_classes/
│   │   │   ├── us_storage.cpp
│   │   │   └── xa_storage.cpp
│   │   ├── validation/
│   │   │   └── xa_iod_validator.cpp
│   │   └── CMakeLists.txt
│   │
│   ├── storage/
│   │   ├── file_storage.cpp
│   │   ├── index_database.cpp
│   │   └── CMakeLists.txt
│   │
│   ├── ai/                     # AI result handling [NEW v1.3.0]
│   │   └── ai_result_handler.cpp
│   │
│   └── integration/
│       ├── container_adapter.cpp
│       ├── network_adapter.cpp
│       └── CMakeLists.txt
│
├── tests/                      # Test suites
│   ├── CMakeLists.txt
│   ├── core/
│   │   ├── test_dicom_element.cpp
│   │   ├── test_dicom_dataset.cpp
│   │   └── test_dicom_file.cpp
│   │
│   ├── encoding/
│   │   ├── test_vr_types.cpp
│   │   └── test_transfer_syntax.cpp
│   │
│   ├── network/
│   │   ├── test_pdu.cpp
│   │   ├── test_dimse.cpp
│   │   └── test_association.cpp
│   │
│   ├── services/
│   │   ├── test_verification.cpp
│   │   ├── test_storage.cpp
│   │   └── xa_storage_test.cpp
│   │
│   ├── integration/
│   │   ├── test_interop.cpp
│   │   └── test_conformance.cpp
│   │
│   ├── ai/                     # AI module tests [NEW v1.3.0]
│   │   └── ai_result_handler_test.cpp
│   │
│   └── fixtures/
│       ├── sample_ct.dcm
│       ├── sample_mr.dcm
│       └── sample_cr.dcm
│
├── examples/                   # Example applications & CLI utilities
│   ├── CMakeLists.txt
│   ├── echo_scu/               # C-ECHO SCU (Verification)
│   │   └── main.cpp
│   ├── store_scu/              # C-STORE SCU (Storage)
│   │   └── main.cpp
│   ├── find_scu/               # C-FIND SCU (Query)
│   │   └── main.cpp
│   ├── move_scu/               # C-MOVE SCU (Retrieve)
│   │   └── main.cpp
│   ├── get_scu/                # C-GET SCU (Retrieve)
│   │   └── main.cpp
│   ├── worklist_scu/           # Modality Worklist SCU
│   │   ├── main.cpp
│   │   ├── worklist_query_builder.hpp
│   │   └── worklist_result_formatter.hpp
│   ├── mpps_scu/               # MPPS SCU
│   │   └── main.cpp
│   ├── echo_scp/               # C-ECHO SCP
│   │   └── main.cpp
│   ├── store_scp/              # C-STORE SCP
│   │   └── main.cpp
│   ├── qr_scp/                 # Query/Retrieve SCP (C-FIND, C-MOVE, C-GET)
│   │   ├── CMakeLists.txt
│   │   └── main.cpp
│   ├── query_scu/              # Query SCU (legacy)
│   │   └── main.cpp
│   └── pacs_server/            # Full PACS Server
│       └── main.cpp
│
├── benchmarks/                 # Performance benchmarks
│   ├── CMakeLists.txt
│   ├── README.md
│   ├── bench_serialization.cpp
│   ├── bench_storage.cpp
│   └── bench_network.cpp
│
├── scripts/                    # Utility scripts
│   ├── build.sh                # Unix build script
│   ├── build.bat               # Windows build script
│   ├── build.ps1               # PowerShell build script
│   ├── test.sh                 # Test runner
│   ├── test.bat                # Windows test runner
│   ├── dependency.sh           # Dependency installer
│   ├── dependency.bat          # Windows dependency installer
│   ├── clean.sh                # Clean build artifacts
│   └── format.sh               # Code formatter
│
├── docs/                       # Documentation
│   ├── README.md               # Documentation index
│   ├── PRD.md                  # Product Requirements
│   ├── PRD.kr.md               # Korean version
│   ├── FEATURES.md             # Feature documentation
│   ├── FEATURES.kr.md          # Korean version
│   ├── ARCHITECTURE.md         # Architecture guide
│   ├── ARCHITECTURE.kr.md      # Korean version
│   ├── PROJECT_STRUCTURE.md    # This file
│   ├── PROJECT_STRUCTURE.kr.md # Korean version
│   ├── API_REFERENCE.md        # API documentation
│   ├── API_REFERENCE.kr.md     # Korean version
│   ├── BENCHMARKS.md           # Performance benchmarks
│   ├── PRODUCTION_QUALITY.md   # Quality metrics
│   ├── CHANGELOG.md            # Version history
│   ├── PACS_IMPLEMENTATION_ANALYSIS.md  # Implementation analysis
│   │
│   ├── guides/                 # User guides
│   │   ├── QUICK_START.md
│   │   ├── BUILD_GUIDE.md
│   │   ├── INTEGRATION.md
│   │   ├── TROUBLESHOOTING.md
│   │   └── FAQ.md
│   │
│   ├── conformance/            # DICOM conformance
│   │   ├── CONFORMANCE_STATEMENT.md
│   │   └── SOP_CLASSES.md
│   │
│   └── advanced/               # Advanced topics
│       ├── CUSTOM_SOP.md
│       ├── TRANSFER_SYNTAX.md
│       └── PRIVATE_TAGS.md
│
├── cmake/                      # CMake modules
│   ├── FindCommonSystem.cmake
│   ├── FindContainerSystem.cmake
│   ├── FindNetworkSystem.cmake
│   ├── FindThreadSystem.cmake
│   ├── FindLoggerSystem.cmake
│   ├── FindMonitoringSystem.cmake
│   ├── CompilerOptions.cmake
│   └── Testing.cmake
│
└── .github/                    # GitHub configuration
    ├── workflows/
    │   ├── ci.yml              # CI pipeline
    │   ├── coverage.yml        # Code coverage
    │   ├── static-analysis.yml # Static analysis
    │   ├── integration-tests.yml # Integration test pipeline
    │   ├── sanitizers.yml      # Memory/UB sanitizers
    │   ├── sbom.yml            # Software Bill of Materials
    │   └── dependency-security-scan.yml # Security scanning
    │
    ├── ISSUE_TEMPLATE/
    │   ├── bug_report.md
    │   └── feature_request.md
    │
    └── PULL_REQUEST_TEMPLATE.md
```

---

## Module Descriptions

### Core Module (`include/pacs/core/`)

Provides fundamental DICOM data structures:

| File | Description |
|------|-------------|
| `dicom_element.h` | Data Element (Tag, VR, Value) |
| `dicom_dataset.h` | Ordered collection of Data Elements |
| `dicom_file.h` | DICOM Part 10 file handling |
| `dicom_dictionary.h` | Tag dictionary and metadata |
| `dicom_tags.h` | Standard tag constants |

### Encoding Module (`include/pacs/encoding/`)

Handles Value Representation encoding:

| File | Description |
|------|-------------|
| `vr_types.h` | 34 VR type definitions |
| `transfer_syntax.h` | Transfer Syntax management |
| `implicit_vr_codec.h` | Implicit VR encoding |
| `explicit_vr_codec.h` | Explicit VR encoding |
| `byte_order.h` | Endianness handling |

### Network Module (`include/pacs/network/`)

Implements DICOM network protocol:

| Subdirectory | Description |
|--------------|-------------|
| `pdu/` | Protocol Data Unit types |
| `dimse/` | DIMSE message handlers |
| Root | Association management |

### Services Module (`include/pacs/services/`)

DICOM service implementations:

| File | Description |
|------|-------------|
| `verification_scp/scu.h` | C-ECHO service |
| `storage_scp/scu.h` | C-STORE service |
| `query_scp/scu.h` | C-FIND service |
| `retrieve_scp.h` | C-MOVE/C-GET service |
| `worklist_scp.h` | Modality Worklist |
| `mpps_scp.h` | MPPS service |
| `sop_class_registry.hpp` | Central SOP Class registry |
| `sop_classes/us_storage.hpp` | US Storage SOP Classes |
| `sop_classes/xa_storage.hpp` | XA/XRF Storage SOP Classes |
| `validation/xa_iod_validator.hpp` | XA IOD validation |

### Storage Module (`include/pacs/storage/`)

Storage backend abstraction:

| File | Description |
|------|-------------|
| `storage_interface.h` | Abstract storage interface |
| `file_storage.h` | Filesystem implementation |
| `index_database.h` | SQLite index |
| `storage_config.h` | Storage configuration |

### AI Module (`include/pacs/ai/`) [NEW v1.3.0]

Handler for AI-generated DICOM objects:

| File | Description |
|------|-------------|
| `ai_result_handler.hpp` | Handler for AI-generated DICOM objects (SR, SEG, PR) |

Supports:
- Structured Reports (SR) with CAD findings
- Segmentation objects (SEG) with binary/fractional segments
- Presentation States (PR) with annotations and measurements

### Integration Module (`include/pacs/integration/`)

Ecosystem integration adapters:

| File | Description |
|------|-------------|
| `container_adapter.h` | container_system integration |
| `network_adapter.h` | network_system integration |
| `thread_adapter.h` | thread_system integration |
| `logger_adapter.h` | logger_system integration |
| `monitoring_adapter.h` | monitoring_system integration |

---

## Header Files

### Include Path Structure

```cpp
// Standard include pattern
#include <pacs/core/dicom_element.hpp>
#include <pacs/encoding/vr_type.hpp>
#include <pacs/network/association.hpp>
#include <pacs/services/storage_scp.hpp>
```

### Header Guards

All headers use `#pragma once` for include guards:

```cpp
#pragma once

namespace pacs::core {
    // ...
}
```

### Namespace Structure

```cpp
namespace pacs {
    namespace core { /* DICOM data structures */ }
    namespace encoding { /* VR encoding */ }
    namespace network {
        namespace pdu { /* PDU types */ }
        namespace dimse { /* DIMSE messages */ }
    }
    namespace services { /* DICOM services */ }
    namespace storage { /* Storage backend */ }
    namespace ai { /* AI result handling [NEW v1.3.0] */ }
    namespace integration { /* Ecosystem adapters */ }
}
```

---

## Source Files

### Implementation Conventions

- One class per `.cpp` file
- Match header file structure
- Include corresponding header first

### Source Organization

```cpp
// dicom_element.cpp

#include <pacs/core/dicom_element.hpp>  // Own header first

#include <pacs/encoding/vr_type.hpp>    // Internal headers
#include <pacs/core/dicom_tag.hpp>

#include <common/result.h>              // Ecosystem headers
#include <container/value.h>

#include <algorithm>                    // Standard library
#include <vector>

namespace pacs::core {

// Implementation

} // namespace pacs::core
```

---

## Build System

### CMake Structure

```cmake
# Root CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(pacs_system VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(BUILD_TESTING "Build tests" ON)
option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_BENCHMARKS "Build benchmarks" OFF)
option(BUILD_DOCS "Build documentation" OFF)

# Find ecosystem dependencies
find_package(CommonSystem REQUIRED)
find_package(ContainerSystem REQUIRED)
find_package(network_system REQUIRED)
find_package(thread_system REQUIRED)
find_package(logger_system REQUIRED)
find_package(MonitoringSystem REQUIRED)

# Add subdirectories
add_subdirectory(src)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
```

### Library Targets

| Target | Type | Description |
|--------|------|-------------|
| `pacs_core` | STATIC | Core DICOM structures |
| `pacs_encoding` | STATIC | VR encoding/decoding |
| `pacs_network` | STATIC | Network protocol |
| `pacs_services` | STATIC | DICOM services |
| `pacs_storage` | STATIC | Storage backend |
| `pacs_ai` | STATIC | AI result handling [NEW v1.3.0] |
| `pacs_system` | INTERFACE | All-in-one target |

---

## Documentation

### Document Types

| Type | Location | Purpose |
|------|----------|---------|
| PRD | `docs/PRD.md` | Requirements |
| Features | `docs/FEATURES.md` | Feature details |
| Architecture | `docs/ARCHITECTURE.md` | System design |
| API Reference | `docs/API_REFERENCE.md` | API docs |
| Guides | `docs/guides/` | User guides |
| Conformance | `docs/conformance/` | DICOM conformance |

### Language Support

All documents available in English and Korean:
- English: `DOCUMENT.md`
- Korean: `DOCUMENT.kr.md`

---

## Testing Structure

### Test Organization

```
tests/
├── core/           # Unit tests for core module
├── encoding/       # Unit tests for encoding module
├── network/        # Unit tests for network module
├── services/       # Unit tests for services module
├── integration/    # Integration tests
└── fixtures/       # Test data files
```

### Test Naming Convention

```cpp
// Pattern: test_<module>_<feature>.cpp
// Example: test_dicom_element.cpp

TEST(DicomElement, CreateWithTag) {
    // ...
}

TEST(DicomElement, SetValue) {
    // ...
}
```

### Test Data

Test DICOM files stored in `tests/fixtures/`:

| File | Description |
|------|-------------|
| `sample_ct.dcm` | CT Image sample |
| `sample_mr.dcm` | MR Image sample |
| `sample_cr.dcm` | CR Image sample |

---

## Document History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0.0 | 2025-11-30 | kcenon | Initial release |
| 1.1.0 | 2025-12-04 | kcenon | Added SOP class and validation directories |
| 1.2.0 | 2025-12-07 | kcenon | Added: network/detail/ (accept_worker), network/v2/ (dicom_server_v2, dicom_association_handler) |
| 1.3.0 | 2025-12-13 | kcenon | Added: ai/ module for AI-generated DICOM objects (SR, SEG, PR) handler |

---

*Document Version: 0.1.3.0*
*Created: 2025-11-30*
*Updated: 2025-12-13*
*Author: kcenon@naver.com*
