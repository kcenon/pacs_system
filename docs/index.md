---
layout: default
title: Home
nav_order: 1
---

# PACS System Documentation

[![CI](https://github.com/kcenon/pacs_system/actions/workflows/ci.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/ci.yml)
[![Integration Tests](https://github.com/kcenon/pacs_system/actions/workflows/integration-tests.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/integration-tests.yml)
[![Code Coverage](https://github.com/kcenon/pacs_system/actions/workflows/coverage.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/coverage.yml)

A modern C++20 PACS (Picture Archiving and Communication System) implementation built entirely on the kcenon ecosystem without external DICOM libraries.

## Key Features

- **Zero External DICOM Libraries**: Pure implementation using kcenon ecosystem
- **High Performance**: SIMD acceleration, lock-free queues, and async I/O
- **Production Grade**: Comprehensive CI/CD, sanitizers, and quality metrics
- **Modular Architecture**: Clean separation of concerns with interface-driven design
- **Cross-Platform**: Linux, macOS, Windows support

## Documentation

| Document | Description |
|----------|-------------|
| [Architecture](ARCHITECTURE.md) | System architecture and design patterns |
| [API Reference](API_REFERENCE.md) | Complete API documentation |
| [Features](FEATURES.md) | Feature list and capabilities |
| [PRD](PRD.md) | Product Requirements Document |
| [SRS](SRS.md) | Software Requirements Specification |
| [SDS](SDS.md) | Software Design Specification |

## Quick Links

### Specifications

- [SDS Components](SDS_COMPONENTS.md) - Component design details
- [SDS Interfaces](SDS_INTERFACES.md) - Interface specifications
- [SDS Sequences](SDS_SEQUENCES.md) - Sequence diagrams
- [SDS Database](SDS_DATABASE.md) - Database schema design
- [SDS Traceability](SDS_TRACEABILITY.md) - Requirements traceability

### Reports

- [Validation Report](VALIDATION_REPORT.md) - System validation results
- [Verification Report](VERIFICATION_REPORT.md) - Verification test results
- [Performance Results](PERFORMANCE_RESULTS.md) - Benchmark results

### Korean Documentation (한국어)

- [아키텍처](ARCHITECTURE_KO.md)
- [API 참조](API_REFERENCE_KO.md)
- [기능](FEATURES_KO.md)
- [PRD](PRD_KO.md)
- [SRS](SRS_KO.md)
- [SDS](SDS_KO.md)

## Project Status

**Current Phase**: Phase 2 Complete - Network & Services (100%)

| Milestone | Status |
|-----------|--------|
| Core DICOM Structures | ✅ Complete |
| Encoding Module | ✅ Complete |
| Storage Backend | ✅ Complete |
| Integration Adapters | ✅ Complete |
| Network Protocol (PDU) | ✅ Complete |
| DIMSE Services | ✅ Complete |
| Query/Retrieve | ✅ Complete |
| Worklist/MPPS | ✅ Complete |
| Advanced Compression | 🔜 Phase 3 |

**Test Coverage**: 143+ tests passing across 42 test files

## Getting Started

### Prerequisites

- C++20 compatible compiler (GCC 11+, Clang 14+, MSVC 2022+)
- CMake 3.20+
- SQLite3
- OpenSSL 1.1+

### Building

```bash
# Clone with submodules
git clone --recursive https://github.com/kcenon/pacs_system.git
cd pacs_system

# Configure and build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Run tests
ctest --test-dir build --output-on-failure
```

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                       PACS System                            │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │   Services  │  │   Network   │  │      Storage        │  │
│  │  (DIMSE)    │  │   (PDU)     │  │    (Database)       │  │
│  └──────┬──────┘  └──────┬──────┘  └──────────┬──────────┘  │
│         │                │                     │             │
│  ┌──────┴────────────────┴─────────────────────┴──────────┐ │
│  │                    Core Module                          │ │
│  │  (dicom_tag, dicom_element, dicom_dataset, dicom_file) │ │
│  └─────────────────────────┬───────────────────────────────┘│
│                            │                                 │
│  ┌─────────────────────────┴───────────────────────────────┐│
│  │                  Integration Adapters                    ││
│  │  (container, network, thread, logger, monitoring)        ││
│  └──────────────────────────────────────────────────────────┘│
├──────────────────────────────────────────────────────────────┤
│                     kcenon Ecosystem                         │
│  container_system │ network_system │ thread_system │ etc.   │
└──────────────────────────────────────────────────────────────┘
```

## License

See [LICENSE](https://github.com/kcenon/pacs_system/blob/main/LICENSE) for details.

## Contributing

Contributions are welcome! Please read our contributing guidelines before submitting PRs.

---

[View on GitHub](https://github.com/kcenon/pacs_system) | [Report Issues](https://github.com/kcenon/pacs_system/issues)
