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
- [Public Header Modules](#public-header-modules)
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

## Public Header Modules

Public headers live under `include/kcenon/pacs/<module>/`. The legacy `include/pacs/...`
path was retired; all consumers should `#include <kcenon/pacs/...>`. The published 0.1.0
package predates the relocation and still installs headers under `include/pacs/` (see
[Package State](PACKAGE_STATE.md)).

| Module | Location | Description |
|--------|----------|-------------|
| **Core** | `include/kcenon/pacs/core/` | DICOM tags, elements, datasets, Part 10 file I/O, tag dictionary |
| **Encoding** | `include/kcenon/pacs/encoding/` | VR types, transfer syntaxes, compression codecs (JPEG, JP2K, JPLS, RLE) |
| **Network** | `include/kcenon/pacs/network/` | PDU encoding/decoding, association state machine, DIMSE protocol |
| **Services** | `include/kcenon/pacs/services/` | SCP/SCU implementations, SOP class registry, IOD validators |
| **Storage** | `include/kcenon/pacs/storage/` | File storage, SQLite indexing, cloud storage (S3/Azure), HSM |
| **Security** | `include/kcenon/pacs/security/` | RBAC, anonymization (PS3.15), digital signatures, TLS |
| **Monitoring** | `include/kcenon/pacs/monitoring/` | Health checks, Prometheus metrics |
| **Web** | `include/kcenon/pacs/web/` | REST API (Crow), DICOMweb (WADO/STOW/QIDO) |
| **Client** | `include/kcenon/pacs/client/` | Job, routing, sync, prefetch, remote node management |
| **Workflow** | `include/kcenon/pacs/workflow/` | Auto prefetch, task scheduler, study lock manager |
| **AI** | `include/kcenon/pacs/ai/` | AI service connector, result handler (SR/SEG) |
| **Integration** | `include/kcenon/pacs/integration/` | Ecosystem adapters (container, network, thread, logger, monitoring) |
| **IHE** | `include/kcenon/pacs/ihe/` | IHE actors (XDS.b Document Source/Consumer, Registry Query, ATNA audit) |
| **DI** | `include/kcenon/pacs/di/` | Dependency injection utilities for service composition |
| **Compat** | `include/kcenon/pacs/compat/` | Compatibility shims for cross-platform and legacy include paths |

The directory layout and the module file lists below are abbreviated: they show each top-level
directory and selected files. `git ls-tree -r --name-only HEAD` lists every file.

---

## Directory Layout

```
pacs_system/
├── README.md                   # Project overview
├── README.kr.md                # Korean version
├── CHANGELOG.md                # Release notes
├── CONTRIBUTING.md             # Contribution workflow
├── LICENSE                     # BSD 3-Clause License
├── CMakeLists.txt              # Root CMake configuration
├── CMakePresets.json           # Configure and build presets
├── vcpkg.json                  # vcpkg manifest
├── Doxyfile                    # Doxygen configuration
├── .gitignore                  # Git ignore patterns
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
│   └── kcenon/
│       └── pacs/
│           ├── core/               # Core DICOM structures
│           │   ├── dicom_element.h
│           │   ├── dicom_dataset.h
│           │   ├── dicom_file.h
│           │   ├── dicom_dictionary.h
│           │   ├── dicom_tag.h
│           │   ├── dicom_tag_constants.h
│           │   └── tag_info.h
│           │
│           ├── encoding/           # VR encoding/decoding
│           │   ├── vr_type.h
│           │   ├── vr_info.h
│           │   ├── transfer_syntax.h
│           │   ├── implicit_vr_codec.h
│           │   ├── explicit_vr_codec.h
│           │   ├── byte_order.h
│           │   ├── compression/        # Compression codecs
│           │   └── simd/               # SIMD helpers
│           │
│           ├── network/            # Network protocol
│           │   ├── pdu_types.h         # PDU type definitions
│           │   ├── pdu_encoder.h       # PDU serialization
│           │   ├── pdu_decoder.h       # PDU deserialization
│           │   ├── association.h       # Association state machine
│           │   ├── dicom_server.h      # Multi-association server
│           │   ├── server_config.h     # Server configuration
│           │   │
│           │   ├── detail/             # Internal implementation
│           │   │   └── accept_worker.h     # Accept loop (thread_base)
│           │   │
│           │   ├── v2/                 # network_system V2
│           │   │   ├── dicom_server_v2.h           # messaging_server-based
│           │   │   └── dicom_association_handler.h # Per-session handler
│           │   │
│           │   ├── pipeline/           # I/O pipeline
│           │   │
│           │   └── dimse/              # DIMSE protocol
│           │       ├── dimse_message.h
│           │       ├── command_field.h
│           │       └── status_codes.h
│           │
│           ├── services/           # DICOM services
│           │   ├── scp_service.h             # Base SCP interface
│           │   ├── verification_scp.h        # C-ECHO SCP
│           │   ├── storage_scp.h             # C-STORE SCP
│           │   ├── storage_scu.h             # C-STORE SCU
│           │   ├── storage_status.h          # Storage status codes
│           │   ├── query_scp.h               # C-FIND SCP
│           │   ├── retrieve_scp.h            # C-MOVE/C-GET SCP
│           │   ├── worklist_scp.h            # MWL SCP
│           │   ├── mpps_scp.h                # MPPS SCP
│           │   ├── sop_class_registry.h      # Central SOP Class registry
│           │   │
│           │   ├── sop_classes/              # SOP Class definitions
│           │   │   ├── us_storage.h          # US Storage SOP Classes
│           │   │   └── xa_storage.h          # XA/XRF Storage SOP Classes
│           │   │
│           │   └── validation/               # IOD Validators
│           │       └── xa_iod_validator.h    # XA IOD validation
│           │
│           ├── storage/            # Storage backend
│           │   ├── storage_interface.h   # Abstract storage interface
│           │   ├── file_storage.h        # Filesystem storage
│           │   ├── index_database.h      # SQLite index
│           │   ├── migration_runner.h    # Database schema migrations
│           │   ├── migration_record.h    # Migration history record
│           │   ├── patient_record.h      # Patient data model
│           │   ├── study_record.h        # Study data model
│           │   ├── series_record.h       # Series data model
│           │   ├── instance_record.h     # Instance data model
│           │   ├── worklist_record.h     # Worklist data model
│           │   └── mpps_record.h         # MPPS data model
│           │
│           ├── ai/                 # AI integration
│           │   ├── ai_result_handler.h   # Handler for AI-generated DICOM objects (SR, SEG, PR)
│           │   └── ai_service_connector.h # Connector for external AI inference services
│           │
│           ├── integration/        # Ecosystem adapters
│           │   ├── container_adapter.h   # container_system integration
│           │   ├── network_adapter.h     # network_system integration
│           │   ├── thread_pool_adapter.h # thread_system integration
│           │   ├── logger_adapter.h      # logger_system integration
│           │   ├── monitoring_adapter.h  # monitoring_system integration
│           │   └── dicom_session.h       # High-level session management
│           │
│           ├── client/             # Job, routing, sync, prefetch, remote nodes
│           ├── compat/             # Compatibility shims
│           ├── di/                 # Dependency injection
│           ├── ihe/                # IHE actors (xds/)
│           ├── monitoring/         # Health checks, metrics
│           ├── security/           # RBAC, anonymization, signatures, TLS
│           ├── web/                # REST API, DICOMweb
│           └── workflow/           # Auto prefetch, task scheduler, study lock
│
├── src/                        # Implementation files, one directory per module
│   ├── core/
│   │   ├── dicom_element.cpp
│   │   ├── dicom_dataset.cpp
│   │   ├── dicom_file.cpp
│   │   └── dicom_dictionary.cpp
│   │
│   ├── network/
│   │   ├── dimse/
│   │   │   └── dimse_message.cpp
│   │   ├── pdu_encoder.cpp
│   │   ├── pdu_decoder.cpp
│   │   ├── association.cpp
│   │   └── dicom_server.cpp
│   │
│   ├── modules/                # C++20 module interface units (*.cppm)
│   ├── ai/
│   ├── client/
│   ├── encoding/
│   ├── ihe/
│   ├── integration/
│   ├── monitoring/
│   ├── security/
│   ├── services/
│   ├── storage/
│   ├── web/
│   └── workflow/
│
├── tests/                      # Catch2 test suites, one directory per module
│   ├── core/
│   │   ├── dicom_element_test.cpp
│   │   ├── dicom_dataset_test.cpp
│   │   └── dicom_file_test.cpp
│   │
│   ├── integration/            # Integration tests
│   ├── cmake_consumer/         # find_package(pacs_system) consumer check
│   ├── concurrency/            # Concurrency stress tests
│   ├── mocks/                  # Test doubles
│   ├── modules/                # C++20 module tests
│   ├── utilities/              # CLI utility tests
│   ├── ai/
│   ├── client/
│   ├── compat/
│   ├── di/
│   ├── encoding/
│   ├── ihe/
│   ├── monitoring/
│   ├── network/
│   ├── security/
│   ├── services/
│   ├── storage/
│   ├── web/
│   └── workflow/
│
├── examples/
│   └── tutorials/              # Tutorials (5 progressive learning steps)
│       ├── CMakeLists.txt
│       ├── common/             # Tutorial helpers
│       ├── 01_hello_dicom/     # Level 1: Basic DICOM file operations
│       ├── 02_echo_server/     # Level 2: C-ECHO SCP/SCU
│       ├── 03_storage_server/  # Level 3: C-STORE SCP/SCU
│       ├── 04_mini_pacs/       # Level 4: Full PACS with query/retrieve
│       └── 05_production_pacs/ # Level 5: Production-ready features
│
├── tools/                      # CLI utilities plus module_example (32 subdirectories)
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
│   │   └── main.cpp
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
│   ├── pacs_server/            # Full PACS Server
│   │   └── main.cpp
│   └── module_example/         # C++20 module demo
│       └── main.cpp
│
├── benchmarks/                 # Performance benchmarks
│   ├── thread_performance/     # Association, throughput, concurrency, shutdown
│   └── simd_performance/       # Byte swap, RLE helpers, photometric, windowing
│
├── fuzz/                       # libFuzzer targets (PACS_BUILD_FUZZ_TARGETS)
│
├── scripts/                    # Utility scripts
│   ├── calculate-stats.sh      # Code statistics
│   ├── update-readme-stats.sh  # README statistics block generator
│   ├── check-stats-drift.sh    # Statistics drift check
│   ├── readme_lint.py          # README policy lint
│   ├── verify_release_version.py # Version consistency check
│   └── tests/                  # Script unit tests
│
├── docs/                       # Documentation
│   ├── README.md               # Documentation registry
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
│   ├── DICOM_CONFORMANCE_STATEMENT.md   # DICOM conformance
│   ├── GETTING_STARTED.md      # Getting started guide
│   │
│   ├── adr/                    # Architecture decision records
│   ├── advanced/               # Advanced topics
│   ├── checklists/             # SaMD certification readiness checklists
│   ├── contributing/           # Contribution and README policies
│   ├── database/               # database_system guides
│   ├── integration/            # Integration guide
│   ├── migration/              # Migration guides
│   └── performance/            # Performance documentation
│
├── cmake/                      # CMake modules
│   ├── options.cmake
│   ├── compiler.cmake
│   ├── dependencies.cmake
│   ├── targets.cmake
│   ├── install.cmake
│   ├── testing.cmake
│   ├── examples.cmake
│   ├── warnings.cmake
│   ├── summary.cmake
│   └── pacs_system-config.cmake.in
│
├── vcpkg-ports/                # vcpkg overlay port (kcenon-pacs-system)
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
    └── pull_request_template.md
```

---

## Module Descriptions

### Core Module (`include/kcenon/pacs/core/`)

Provides fundamental DICOM data structures:

| File | Description |
|------|-------------|
| `dicom_element.h` | Data Element (Tag, VR, Value) |
| `dicom_dataset.h` | Ordered collection of Data Elements |
| `dicom_file.h` | DICOM Part 10 file handling |
| `dicom_dictionary.h` | Tag dictionary and metadata |
| `dicom_tag_constants.h` | Standard tag constants |

### Encoding Module (`include/kcenon/pacs/encoding/`)

Handles Value Representation encoding:

| File | Description |
|------|-------------|
| `vr_type.h` | VR type definitions |
| `transfer_syntax.h` | Transfer Syntax management |
| `implicit_vr_codec.h` | Implicit VR encoding |
| `explicit_vr_codec.h` | Explicit VR encoding |
| `byte_order.h` | Endianness handling |

### Network Module (`include/kcenon/pacs/network/`)

Implements DICOM network protocol:

| Subdirectory | Description |
|--------------|-------------|
| `dimse/` | DIMSE message handlers |
| `detail/` | Accept loop worker |
| `pipeline/` | I/O pipeline stages |
| `v2/` | network_system-based server |
| Root | PDU types, encoder, and decoder; association management |

### Services Module (`include/kcenon/pacs/services/`)

DICOM service implementations:

| File | Description |
|------|-------------|
| `verification_scp.h` | C-ECHO service |
| `storage_scp.h`, `storage_scu.h` | C-STORE service |
| `query_scp.h`, `query_scu.h` | C-FIND service |
| `retrieve_scp.h`, `retrieve_scu.h` | C-MOVE/C-GET service |
| `worklist_scp.h` | Modality Worklist |
| `mpps_scp.h` | MPPS service |
| `sop_class_registry.h` | Central SOP Class registry |
| `sop_classes/us_storage.h` | US Storage SOP Classes |
| `sop_classes/xa_storage.h` | XA/XRF Storage SOP Classes |
| `validation/xa_iod_validator.h` | XA IOD validation |

### Storage Module (`include/kcenon/pacs/storage/`)

Storage backend abstraction:

| File | Description |
|------|-------------|
| `storage_interface.h` | Abstract storage interface |
| `file_storage.h` | Filesystem implementation |
| `index_database.h` | SQLite index |
| `s3_storage.h`, `azure_blob_storage.h` | Cloud storage backends |
| `hsm_storage.h` | Hierarchical storage management |

### AI Module (`include/kcenon/pacs/ai/`)

Handler for AI-generated DICOM objects:

| File | Description |
|------|-------------|
| `ai_result_handler.h` | Handler for AI-generated DICOM objects (SR, SEG, PR) |
| `ai_service_connector.h` | Connector for external AI inference services |

Supports:
- Structured Reports (SR) with CAD findings
- Segmentation objects (SEG) with binary/fractional segments
- Presentation States (PR) with annotations and measurements

### Integration Module (`include/kcenon/pacs/integration/`)

Ecosystem integration adapters:

| File | Description |
|------|-------------|
| `container_adapter.h` | container_system integration |
| `network_adapter.h` | network_system integration |
| `thread_pool_adapter.h` | thread_system integration |
| `logger_adapter.h` | logger_system integration |
| `monitoring_adapter.h` | monitoring_system integration |

The other modules (`client/`, `compat/`, `di/`, `ihe/`, `monitoring/`, `security/`, `web/`,
`workflow/`) are summarized in [Public Header Modules](#public-header-modules).

---

## Header Files

### Include Path Structure

```cpp
// Standard include pattern
#include <kcenon/pacs/core/dicom_element.h>
#include <kcenon/pacs/encoding/vr_type.h>
#include <kcenon/pacs/network/association.h>
#include <kcenon/pacs/services/storage_scp.h>
```

### Header Guards

Headers use either `#pragma once` or an `#ifndef PACS_<MODULE>_<NAME>_HPP` include guard:

```cpp
#pragma once

namespace kcenon::pacs::core {
    // ...
}
```

### Namespace Structure

```cpp
namespace kcenon::pacs {
    namespace core { /* DICOM data structures */ }
    namespace encoding { /* VR encoding */ }
    namespace network {
        namespace dimse { /* DIMSE messages */ }
        namespace pipeline { /* I/O pipeline */ }
    }
    namespace services { /* DICOM services */ }
    namespace storage { /* Storage backend */ }
    namespace ai { /* AI result handling */ }
    namespace integration { /* Ecosystem adapters */ }
    // Also: client, compat, di, ihe, monitoring, security, web, workflow
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
// src/core/dicom_element.cpp

#include <kcenon/pacs/core/dicom_element.h>  // Own header first

#include <kcenon/pacs/core/dicom_dataset.h>  // Other pacs_system headers
#include <kcenon/pacs/core/result.h>

#include <algorithm>                         // Standard library
#include <charconv>

namespace kcenon::pacs::core {

// Implementation

} // namespace kcenon::pacs::core
```

---

## Build System

### CMake Structure

The root `CMakeLists.txt` declares the project (`VERSION 0.1.0`, C++20, executables in `bin/`
under the build directory) and includes these modules from `cmake/`:

| File | Purpose |
|------|---------|
| `options.cmake` | `PACS_BUILD_*` and `PACS_WITH_*` options |
| `compiler.cmake` | Compiler warnings and the `pacs_apply_warnings()` function |
| `dependencies.cmake` | `find_package`, FetchContent, ecosystem dependencies |
| `targets.cmake` | Library targets and source file lists |
| `install.cmake` | Install rules and the `pacs_system::pacs_system` export |
| `testing.cmake` | Test executables and CTest registration |
| `examples.cmake` | CLI tool binaries (under `tools/`), benchmarks, smoke tests |
| `warnings.cmake` | Applies compiler warnings to all PACS targets |
| `summary.cmake` | Build configuration summary output |

Main options: `PACS_BUILD_TESTS` (ON), `PACS_BUILD_EXAMPLES` (OFF, CLI tools under `tools/`),
`PACS_BUILD_SAMPLES` (OFF, tutorials under `examples/`), `PACS_BUILD_BENCHMARKS` (OFF),
`PACS_BUILD_STORAGE` (ON, requires SQLite3), and `PACS_BUILD_MODULES` (OFF).

### Library Targets

| Target | Built when | Description |
|--------|------------|-------------|
| `pacs_core` | Always | Core DICOM structures |
| `pacs_encoding` | Always | VR encoding/decoding |
| `pacs_network` | Always | Network protocol |
| `pacs_client` | Always | Remote node, job, routing, sync, and prefetch management |
| `pacs_services` | Always | DICOM services |
| `pacs_security` | Always | RBAC, anonymization, digital signatures, TLS |
| `pacs_storage` | `PACS_BUILD_STORAGE` and SQLite3 found | Storage backend |
| `pacs_ai` | `pacs_storage` built | AI result handling and service connector |
| `pacs_monitoring` | `pacs_storage` built | Health checks and metrics |
| `pacs_workflow` | `pacs_storage` built | Auto prefetch and task scheduler |
| `pacs_web` | Crow available | REST API and DICOMweb |
| `pacs_integration` | container_system, common_system, and network_system found | Ecosystem adapters |
| `pacs_ihe_xds` | pugixml, libcurl, and OpenSSL found | IHE XDS.b Document Source and Consumer |
| `pacs_system_all` | Always | INTERFACE target exported as `pacs_system::pacs_system`; links every built library above except `pacs_ihe_xds` |

---

## Documentation

### Document Types

| Type | Location | Purpose |
|------|----------|---------|
| PRD | `docs/PRD.md` | Requirements |
| Features | `docs/FEATURES.md` | Feature details |
| Architecture | `docs/ARCHITECTURE.md` | System design |
| API Reference | `docs/API_REFERENCE.md` | API docs |
| Getting Started | `docs/GETTING_STARTED.md` | Usage guide |
| Conformance | `docs/DICOM_CONFORMANCE_STATEMENT.md` | DICOM conformance |
| Registry | `docs/README.md` | Index of the documents |

### Language Support

Many documents have a Korean counterpart:
- English: `DOCUMENT.md`
- Korean: `DOCUMENT.kr.md`

Documents without a `.kr.md` file, such as `DICOM_CONFORMANCE_STATEMENT.md`, are English only.

---

## Testing Structure

### Test Organization

Tests use Catch2 v3.4.0 and live under `tests/<module>/` (see [Directory Layout](#directory-layout)).
`cmake/testing.cmake` builds one executable per module (for example `core_tests`,
`encoding_tests`, `network_tests`, `services_tests`) into `bin/` under the build directory and
registers each test case with CTest through `catch_discover_tests`.

```
tests/
├── core/           # Unit tests for core module
├── encoding/       # Unit tests for encoding module
├── network/        # Unit tests for network module
├── services/       # Unit tests for services module
├── storage/        # Unit tests for storage module
└── integration/    # Integration tests
```

### Test Naming Convention

```cpp
// Pattern: <component>_test.cpp
// Example: tests/core/dicom_element_test.cpp

TEST_CASE("dicom_element default construction", "[core][dicom_element]") {
    // ...
}

TEST_CASE("dicom_element string handling", "[core][dicom_element]") {
    // ...
}
```

### Test Data

Unit tests build their datasets in code and write temporary files when they need Part 10
files. The integration test tool uses the DICOM files in `tools/integration_tests/test_data/`:

| File | Description |
|------|-------------|
| `ct_minimal.dcm` | CT Image sample |
| `mr_minimal.dcm` | MR Image sample |
| `xa_minimal.dcm` | XA Image sample |

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
