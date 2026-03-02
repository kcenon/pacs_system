# PACS System

[![CI](https://github.com/kcenon/pacs_system/actions/workflows/ci.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/ci.yml)
[![Integration Tests](https://github.com/kcenon/pacs_system/actions/workflows/integration-tests.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/integration-tests.yml)
[![Code Coverage](https://github.com/kcenon/pacs_system/actions/workflows/coverage.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/coverage.yml)
[![Static Analysis](https://github.com/kcenon/pacs_system/actions/workflows/static-analysis.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/static-analysis.yml)
[![SBOM Generation](https://github.com/kcenon/pacs_system/actions/workflows/sbom.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/sbom.yml)

> **Language:** **English** | [한국어](README.kr.md)

## Overview

A modern C++20 PACS (Picture Archiving and Communication System) implementation built entirely on the kcenon ecosystem without external DICOM libraries. This project implements the DICOM standard from scratch, leveraging the existing high-performance infrastructure.

**Key Characteristics**:
- **Zero External DICOM Libraries**: Pure implementation using kcenon ecosystem
- **High Performance**: Leveraging SIMD acceleration, lock-free queues, and async I/O
- **Production Grade**: Comprehensive CI/CD, sanitizers, and quality metrics
- **Modular Architecture**: Clean separation of concerns with interface-driven design
- **Cross-Platform**: Linux, macOS, Windows support

---

## Table of Contents

- [Project Status](#project-status)
- [Architecture](#architecture)
- [DICOM Conformance](#dicom-conformance)
- [Getting Started](#getting-started)
- [CLI Tools & Examples](#cli-tools--examples)
- [Ecosystem Dependencies](#ecosystem-dependencies)
- [Project Structure](#project-structure)
- [Documentation](#documentation)
- [Performance](#performance)
- [Code Statistics](#code-statistics)
- [Contributing](#contributing)
- [License](#license)
- [Contact](#contact)

---

## Project Status

**Current Phase**: ✅ Phase 4 Complete - Advanced Services & Production Hardening

| Phase | Scope | Status |
|-------|-------|--------|
| **Phase 1**: Foundation | DICOM Core, Tag Dictionary, File I/O (Part 10), Transfer Syntax | ✅ Complete |
| **Phase 2**: Network Protocol | Upper Layer Protocol (PDU), Association State Machine, DIMSE-C, Compression Codecs | ✅ Complete |
| **Phase 3**: Core Services | Storage SCP/SCU, File Storage, Index Database, Query/Retrieve, Logging, Monitoring | ✅ Complete |
| **Phase 4**: Advanced Services | REST API, DICOMweb, AI Integration, Client Module, Cloud Storage, Print Management, Security, Workflow, Annotation/Viewer | ✅ Complete |

**Test Coverage**: 1,980+ tests passing across 141+ test files

> For detailed feature lists per phase, see [Features](docs/FEATURES.md).

---

## Architecture

```
┌──────────────────────────────────────────────────────────────────────┐
│                            PACS System                               │
├──────────────────────────────────────────────────────────────────────┤
│  ┌──────────┐ ┌──────────┐ ┌─────────┐ ┌──────────┐ ┌───────────┐  │
│  │ REST API │ │ DICOMweb │ │ Web UI  │ │ AI Svc   │ │ Workflow  │  │
│  │ (Crow)   │ │ WADO/STOW│ │ (React) │ │Connector │ │ Scheduler │  │
│  └────┬─────┘ └────┬─────┘ └────┬────┘ └────┬─────┘ └─────┬─────┘  │
│       └─────────────┼───────────┼───────────┼──────────────┘        │
│  ┌──────────────────▼───────────▼───────────▼────────────────────┐  │
│  │  Services (Storage/Query/Retrieve/Worklist/MPPS/Commit/Print) │  │
│  └──────────────────────────────┬────────────────────────────────┘  │
│  ┌──────────────────────────────▼────────────────────────────────┐  │
│  │  Network (PDU/Association/DIMSE) + Security (RBAC/TLS/Anon)   │  │
│  └──────────────────────────────┬────────────────────────────────┘  │
│  ┌──────────────────────────────▼────────────────────────────────┐  │
│  │  Core (Tag/Element/Dataset/File) + Encoding (VR/Codecs/SIMD)  │  │
│  └──────────────────────────────┬────────────────────────────────┘  │
│  ┌─────────────┐  ┌─────────────┴──────────┐  ┌─────────────────┐  │
│  │  Storage    │  │  Client Module         │  │  Monitoring     │  │
│  │  (DB/File/  │  │  (Job/Route/Sync/      │  │  (Health/       │  │
│  │   S3/Azure) │  │   Prefetch/RemoteNode) │  │   Metrics)      │  │
│  └──────┬──────┘  └────────────────────────┘  └─────────────────┘  │
├─────────┼────────────────────────────────────────────────────────────┤
│         │             Integration Adapters                           │
│  container │ network │ thread │ logger │ monitoring                  │
├─────────┼────────────────────────────────────────────────────────────┤
│         │              kcenon Ecosystem                               │
│  common_system │ container_system │ thread_system │ network_system   │
│  logger_system │ monitoring_system │ database_system (opt)           │
└──────────────────────────────────────────────────────────────────────┘
```

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
| **Storage Commitment** | 1.2.840.10008.1.20.1 | ✅ Complete |
| **NM Storage** | 1.2.840.10008.5.1.4.1.1.20 | ✅ Complete |
| **PET Storage** | 1.2.840.10008.5.1.4.1.1.128 | ✅ Complete |
| **RT Storage** | 1.2.840.10008.5.1.4.1.1.481.x | ✅ Complete |
| **SR Storage** | 1.2.840.10008.5.1.4.1.1.88.x | ✅ Complete |
| **SEG Storage** | 1.2.840.10008.5.1.4.1.1.66.4 | ✅ Complete |
| **MG Storage** | 1.2.840.10008.5.1.4.1.1.1.2.x | ✅ Complete |
| **CR Storage** | 1.2.840.10008.5.1.4.1.1.1 | ✅ Complete |
| **SC Storage** | 1.2.840.10008.5.1.4.1.1.7 | ✅ Complete |
| **Basic Grayscale Print** | 1.2.840.10008.5.1.1.9 | ✅ Complete |
| **Basic Color Print** | 1.2.840.10008.5.1.1.18 | ✅ Complete |
| **Printer** | 1.2.840.10008.5.1.1.16 | ✅ Complete |

### Transfer Syntax Support

| Transfer Syntax | UID | Status |
|----------------|-----|--------|
| Implicit VR Little Endian | 1.2.840.10008.1.2 | ✅ Complete |
| Explicit VR Little Endian | 1.2.840.10008.1.2.1 | ✅ Complete |
| Explicit VR Big Endian | 1.2.840.10008.1.2.2 | ✅ Complete |
| JPEG Baseline (Process 1) | 1.2.840.10008.1.2.4.50 | ✅ Complete |
| JPEG Lossless (Process 14) | 1.2.840.10008.1.2.4.70 | ✅ Complete |
| JPEG 2000 Lossless | 1.2.840.10008.1.2.4.90 | ✅ Complete |
| JPEG 2000 Lossy | 1.2.840.10008.1.2.4.91 | ✅ Complete |
| JPEG-LS Lossless | 1.2.840.10008.1.2.4.80 | ✅ Complete |
| JPEG-LS Near-Lossless | 1.2.840.10008.1.2.4.81 | ✅ Complete |
| RLE Lossless | 1.2.840.10008.1.2.5 | ✅ Complete |

> For the full DICOM Conformance Statement, see [DICOM Conformance Statement](docs/DICOM_CONFORMANCE_STATEMENT.md).

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

**Test Results**: 1,980+ tests passing across 141+ test files (Core, Encoding, Network, Services, Storage, Security, Web, Workflow, Client, AI, Monitoring, Integration)

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

## CLI Tools & Examples

The project includes 32 example applications. Build them with:

```bash
cmake -S . -B build -DPACS_BUILD_EXAMPLES=ON
cmake --build build
```

### Tool Summary

| Category | Tool | Description |
|----------|------|-------------|
| **File Utilities** | `dcm_dump` | Inspect DICOM file metadata |
| | `dcm_info` | Display DICOM file summary |
| | `dcm_modify` | Modify DICOM tags (dcmtk-compatible) |
| | `dcm_conv` | Convert Transfer Syntax |
| | `dcm_anonymize` | De-identify DICOM files (PS3.15) |
| | `dcm_dir` | Create/manage DICOMDIR (PS3.10) |
| | `dcm_to_json` | Convert DICOM to JSON (PS3.18) |
| | `json_to_dcm` | Convert JSON to DICOM (PS3.18) |
| | `dcm_to_xml` | Convert DICOM to XML (PS3.19) |
| | `xml_to_dcm` | Convert XML to DICOM (PS3.19) |
| | `img_to_dcm` | Convert JPEG images to DICOM |
| | `dcm_extract` | Extract pixel data to image formats |
| | `db_browser` | Browse PACS index database |
| **Network** | `echo_scu` / `echo_scp` | DICOM connectivity verification |
| | `secure_dicom` | TLS-secured DICOM Echo SCU/SCP |
| | `store_scu` / `store_scp` | DICOM Storage (C-STORE) |
| | `query_scu` | Query client (C-FIND) |
| | `find_scu` | dcmtk-compatible C-FIND SCU |
| | `retrieve_scu` | Retrieve client (C-MOVE/C-GET) |
| | `move_scu` | dcmtk-compatible C-MOVE SCU |
| | `get_scu` | dcmtk-compatible C-GET SCU |
| | `print_scu` | Print Management client (PS3.4 Annex H) |
| **Servers** | `qr_scp` | Query/Retrieve SCP (C-FIND/C-MOVE/C-GET) |
| | `worklist_scu` / `worklist_scp` | Modality Worklist (MWL) |
| | `mpps_scu` / `mpps_scp` | Modality Performed Procedure Step |
| | `pacs_server` | Full PACS server with configuration |
| **Testing** | `integration_tests` | End-to-end workflow tests |

### Quick Examples

**Verify DICOM connectivity:**
```bash
./build/bin/echo_scu localhost 11112
```

**Send a DICOM file:**
```bash
./build/bin/store_scu localhost 11112 image.dcm
```

**Query studies by patient name:**
```bash
./build/bin/query_scu localhost 11112 PACS_SCP --level STUDY --patient-name "DOE^*"
```

**Inspect a DICOM file:**
```bash
./build/bin/dcm_dump image.dcm --format json
```

> For complete CLI documentation with all options, see [CLI Reference](docs/CLI_REFERENCE.md).

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

| Module | Location | Description |
|--------|----------|-------------|
| **Core** | `include/pacs/core/` | DICOM tags, elements, datasets, Part 10 file I/O, tag dictionary |
| **Encoding** | `include/pacs/encoding/` | VR types, transfer syntaxes, compression codecs (JPEG, JP2K, JPLS, RLE) |
| **Network** | `include/pacs/network/` | PDU encoding/decoding, association state machine, DIMSE protocol |
| **Services** | `include/pacs/services/` | SCP/SCU implementations, SOP class registry, IOD validators |
| **Storage** | `include/pacs/storage/` | File storage, SQLite indexing, cloud storage (S3/Azure), HSM |
| **Security** | `include/pacs/security/` | RBAC, anonymization (PS3.15), digital signatures, TLS |
| **Monitoring** | `include/pacs/monitoring/` | Health checks, Prometheus metrics |
| **Web** | `include/pacs/web/` | REST API (Crow), DICOMweb (WADO/STOW/QIDO) |
| **Client** | `include/pacs/client/` | Job, routing, sync, prefetch, remote node management |
| **Workflow** | `include/pacs/workflow/` | Auto prefetch, task scheduler, study lock manager |
| **AI** | `include/pacs/ai/` | AI service connector, result handler (SR/SEG) |
| **Integration** | `include/pacs/integration/` | Ecosystem adapters (container, network, thread, logger, monitoring) |

```
pacs_system/
├── include/pacs/         # Public headers (222 files)
├── src/                  # Source implementations (148 files)
├── tests/                # Test suites (141+ files, 1,980+ tests)
├── examples/             # Example applications (32 apps)
├── docs/                 # Documentation (57 files)
└── CMakeLists.txt        # Build configuration (v0.2.0)
```

> For the full file-level directory tree, see [Project Structure](docs/PROJECT_STRUCTURE.md).

---

## Documentation

- 📋 [Implementation Analysis](docs/PACS_IMPLEMENTATION_ANALYSIS.md) - Detailed implementation strategy
- 📋 [Product Requirements](docs/PRD.md) - Product Requirements Document
- 🏗️ [Architecture Guide](docs/ARCHITECTURE.md) - System architecture
- ⚡ [Features](docs/FEATURES.md) - Feature specifications
- 📁 [Project Structure](docs/PROJECT_STRUCTURE.md) - Directory structure
- 🔧 [API Reference](docs/API_REFERENCE.md) - API documentation
- 🖥️ [CLI Reference](docs/CLI_REFERENCE.md) - CLI tools documentation
- 📄 [DICOM Conformance Statement](docs/DICOM_CONFORMANCE_STATEMENT.md) - DICOM conformance
- 🚀 [Migration Complete](docs/MIGRATION_COMPLETE.md) - Thread system migration summary

**Database Integration:**
- 🗄️ [Migration Guide](docs/database/MIGRATION_GUIDE.md) - database_system integration guide
- 📚 [API Reference (Database)](docs/database/API_REFERENCE.md) - Query Builder API documentation
- 🏛️ [ADR-001](docs/adr/ADR-001-database-system-integration.md) - Architecture Decision Record
- ⚡ [Performance Guide](docs/database/PERFORMANCE_GUIDE.md) - Database optimization tips

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
| **Header Files** | 248 files |
| **Source Files** | 172 files |
| **Header LOC** | ~75,500 lines |
| **Source LOC** | ~100,900 lines |
| **Example LOC** | ~35,600 lines |
| **Test LOC** | ~73,400 lines |
| **Total LOC** | ~285,400 lines |
| **Test Files** | 153 files |
| **Test Cases** | 2261+ tests |
| **Example Programs** | 32 apps |
| **Documentation** | 59 markdown files |
| **CI/CD Workflows** | 10 workflows |
| **Version** | 0.1.0 |
| **Last Updated** | 2026-03-02 |

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
