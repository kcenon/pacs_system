# Project Structure - PACS System

> **Version:** 1.0.0
> **Last Updated:** 2025-11-30
> **Language:** **English** | [한국어](PROJECT_STRUCTURE_KO.md)

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
├── README_KO.md                # Korean version
├── LICENSE                     # BSD 3-Clause License
├── CMakeLists.txt              # Root CMake configuration
├── .gitignore                  # Git ignore patterns
├── .clang-format               # Code formatting rules
├── .clang-tidy                 # Static analysis rules
│
├── include/                    # Public header files
│   └── pacs/
│       ├── core/               # Core DICOM structures
│       │   ├── dicom_element.h
│       │   ├── dicom_dataset.h
│       │   ├── dicom_file.h
│       │   ├── dicom_dictionary.h
│       │   └── dicom_tags.h
│       │
│       ├── encoding/           # VR encoding/decoding
│       │   ├── vr_types.h
│       │   ├── transfer_syntax.h
│       │   ├── implicit_vr_codec.h
│       │   ├── explicit_vr_codec.h
│       │   └── byte_order.h
│       │
│       ├── network/            # Network protocol
│       │   ├── pdu/
│       │   │   ├── pdu_types.h
│       │   │   ├── pdu_encoder.h
│       │   │   ├── pdu_decoder.h
│       │   │   ├── associate_rq.h
│       │   │   ├── associate_ac.h
│       │   │   ├── associate_rj.h
│       │   │   ├── p_data.h
│       │   │   ├── a_release.h
│       │   │   └── a_abort.h
│       │   │
│       │   ├── dimse/
│       │   │   ├── dimse_message.h
│       │   │   ├── dimse_command.h
│       │   │   ├── c_echo.h
│       │   │   ├── c_store.h
│       │   │   ├── c_find.h
│       │   │   ├── c_move.h
│       │   │   ├── c_get.h
│       │   │   ├── n_create.h
│       │   │   └── n_set.h
│       │   │
│       │   ├── association.h
│       │   ├── presentation_context.h
│       │   └── dicom_server.h
│       │
│       ├── services/           # DICOM services
│       │   ├── service_provider.h
│       │   ├── verification_scp.h
│       │   ├── verification_scu.h
│       │   ├── storage_scp.h
│       │   ├── storage_scu.h
│       │   ├── query_scp.h
│       │   ├── query_scu.h
│       │   ├── retrieve_scp.h
│       │   ├── worklist_scp.h
│       │   └── mpps_scp.h
│       │
│       ├── storage/            # Storage backend
│       │   ├── storage_interface.h
│       │   ├── file_storage.h
│       │   ├── storage_config.h
│       │   └── index_database.h
│       │
│       ├── integration/        # Ecosystem adapters
│       │   ├── container_adapter.h
│       │   ├── network_adapter.h
│       │   ├── thread_adapter.h
│       │   ├── logger_adapter.h
│       │   └── monitoring_adapter.h
│       │
│       └── common/             # Common utilities
│           ├── error_codes.h
│           ├── uid_generator.h
│           └── config.h
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
│   │   └── CMakeLists.txt
│   │
│   ├── storage/
│   │   ├── file_storage.cpp
│   │   ├── index_database.cpp
│   │   └── CMakeLists.txt
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
│   │   └── test_storage.cpp
│   │
│   ├── integration/
│   │   ├── test_interop.cpp
│   │   └── test_conformance.cpp
│   │
│   └── fixtures/
│       ├── sample_ct.dcm
│       ├── sample_mr.dcm
│       └── sample_cr.dcm
│
├── examples/                   # Example applications
│   ├── CMakeLists.txt
│   ├── echo_scu/
│   │   └── main.cpp
│   ├── store_scu/
│   │   └── main.cpp
│   ├── store_scp/
│   │   └── main.cpp
│   ├── query_scu/
│   │   └── main.cpp
│   └── pacs_server/
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
│   ├── PRD_KO.md               # Korean version
│   ├── FEATURES.md             # Feature documentation
│   ├── FEATURES_KO.md          # Korean version
│   ├── ARCHITECTURE.md         # Architecture guide
│   ├── ARCHITECTURE_KO.md      # Korean version
│   ├── PROJECT_STRUCTURE.md    # This file
│   ├── PROJECT_STRUCTURE_KO.md # Korean version
│   ├── API_REFERENCE.md        # API documentation
│   ├── API_REFERENCE_KO.md     # Korean version
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
    │   └── build-Doxygen.yaml  # API docs
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
| `vr_types.h` | 27 VR type definitions |
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

### Storage Module (`include/pacs/storage/`)

Storage backend abstraction:

| File | Description |
|------|-------------|
| `storage_interface.h` | Abstract storage interface |
| `file_storage.h` | Filesystem implementation |
| `index_database.h` | SQLite index |
| `storage_config.h` | Storage configuration |

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
#include <pacs/core/dicom_element.h>
#include <pacs/encoding/vr_types.h>
#include <pacs/network/association.h>
#include <pacs/services/storage_scp.h>
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

#include <pacs/core/dicom_element.h>  // Own header first

#include <pacs/encoding/vr_types.h>    // Internal headers
#include <pacs/common/error_codes.h>

#include <common/result.h>             // Ecosystem headers
#include <container/value.h>

#include <algorithm>                   // Standard library
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
find_package(NetworkSystem REQUIRED)
find_package(ThreadSystem REQUIRED)
find_package(LoggerSystem REQUIRED)
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
- Korean: `DOCUMENT_KO.md`

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

*Document Version: 1.0.0*
*Created: 2025-11-30*
*Author: kcenon@naver.com*
