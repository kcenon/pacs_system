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

- [아키텍처](ARCHITECTURE.kr.md)
- [API 참조](API_REFERENCE.kr.md)
- [기능](FEATURES.kr.md)
- [PRD](PRD.kr.md)
- [SRS](SRS.kr.md)
- [SDS](SDS.kr.md)

## Project Status

**Current Phase**: ✅ Phase 4 Complete - Advanced Services & Production Hardening

| Phase | Scope | Status |
|-------|-------|--------|
| **Phase 1**: Foundation | DICOM Core, Tag Dictionary, File I/O, Transfer Syntax | ✅ Complete |
| **Phase 2**: Network Protocol | PDU, Association State Machine, DIMSE-C, Compression Codecs | ✅ Complete |
| **Phase 3**: Core Services | Storage SCP/SCU, Index Database, Query/Retrieve, Logging | ✅ Complete |
| **Phase 4**: Advanced Services | REST API, DICOMweb, AI, Client Module, Cloud Storage, Security, Workflow | ✅ Complete |

**Test Coverage**: 1,980+ tests passing across 141+ test files | **Total**: 251,242 LOC (measured 2026-02-11)

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
┌───────────────────────────────────────────────────────────────────┐
│                          PACS System                              │
├───────────────────────────────────────────────────────────────────┤
│  REST API │ DICOMweb │ Web UI │ AI Service │ Workflow Scheduler   │
├───────────────────────────────────────────────────────────────────┤
│  Services (Storage/Query/Retrieve/Worklist/MPPS SCP/SCU)         │
│  Client Module (Job/Routing/Sync/Prefetch/RemoteNode)            │
├───────────────────────────────────────────────────────────────────┤
│  Network (PDU/Association/DIMSE) │ Security (RBAC/TLS/Anon)      │
├───────────────────────────────────────────────────────────────────┤
│  Core (Tag/Element/Dataset/File) │ Encoding (VR/Codecs/SIMD)     │
├───────────────────────────────────────────────────────────────────┤
│  Storage (DB/File/S3/Azure/HSM) │ Monitoring (Health/Metrics)    │
├───────────────────────────────────────────────────────────────────┤
│  Integration Adapters (container, network, thread, logger, etc.) │
├───────────────────────────────────────────────────────────────────┤
│  kcenon Ecosystem: common │ container │ thread │ network │ ...   │
└───────────────────────────────────────────────────────────────────┘
```

## License

See [LICENSE](https://github.com/kcenon/pacs_system/blob/main/LICENSE) for details.

## Contributing

Contributions are welcome! Please read our contributing guidelines before submitting PRs.

---

[View on GitHub](https://github.com/kcenon/pacs_system) | [Report Issues](https://github.com/kcenon/pacs_system/issues)
