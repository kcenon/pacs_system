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

**Current Phase**: 📋 Analysis & Planning

| Milestone | Status | Target |
|-----------|--------|--------|
| Analysis & Documentation | ✅ Complete | Week 1 |
| Core DICOM Structures | 🔜 Planned | Week 2-5 |
| Network Protocol (PDU) | 🔜 Planned | Week 6-9 |
| DIMSE Services | 🔜 Planned | Week 10-13 |
| Storage SCP/SCU | 🔜 Planned | Week 14-17 |
| Query/Retrieve | 🔜 Planned | Week 18-20 |

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
├── core/                    # Core DICOM implementation
│   ├── dicom_element.h      # Data Element
│   ├── dicom_dataset.h      # Data Set
│   ├── dicom_file.h         # DICOM File (Part 10)
│   └── dicom_dictionary.h   # Tag Dictionary
│
├── encoding/                # Encoding/Decoding
│   ├── vr_types.h           # Value Representation
│   ├── transfer_syntax.h    # Transfer Syntax
│   └── codecs/              # Compression codecs
│
├── network/                 # Network Protocol
│   ├── pdu/                 # Protocol Data Units
│   ├── dimse/               # DIMSE Messages
│   └── association.h        # Association Manager
│
├── services/                # DICOM Services
│   ├── storage_scp.h        # Storage SCP
│   ├── qr_scp.h             # Query/Retrieve SCP
│   ├── worklist_scp.h       # Modality Worklist SCP
│   └── mpps_scp.h           # MPPS SCP
│
├── storage/                 # Storage Backend
│   ├── storage_interface.h  # Abstract interface
│   └── file_storage.h       # Filesystem storage
│
├── integration/             # Ecosystem Integration
│   ├── container_adapter.h  # container_system adapter
│   ├── network_adapter.h    # network_system adapter
│   └── thread_adapter.h     # thread_system adapter
│
├── tests/                   # Test suites
├── examples/                # Usage examples
├── scripts/                 # Build and utility scripts
└── docs/                    # Documentation
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

### Planned SOP Classes

| Service | SOP Class | Priority |
|---------|-----------|----------|
| **Verification** | 1.2.840.10008.1.1 | MVP |
| **CT Storage** | 1.2.840.10008.5.1.4.1.1.2 | MVP |
| **MR Storage** | 1.2.840.10008.5.1.4.1.1.4 | MVP |
| **X-Ray Storage** | 1.2.840.10008.5.1.4.1.1.1.1 | MVP |
| **Patient Root Q/R** | 1.2.840.10008.5.1.4.1.2.1.x | Phase 2 |
| **Study Root Q/R** | 1.2.840.10008.5.1.4.1.2.2.x | Phase 2 |
| **Modality Worklist** | 1.2.840.10008.5.1.4.31 | Phase 2 |
| **MPPS** | 1.2.840.10008.3.1.2.3.3 | Phase 2 |

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

### Build (Coming Soon)

```bash
# Clone repository
git clone https://github.com/kcenon/pacs_system.git
cd pacs_system

# Install dependencies
./scripts/dependency.sh

# Build
./scripts/build.sh

# Run tests
./scripts/test.sh
```

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
