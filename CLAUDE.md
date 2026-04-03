# pacs_system

## Overview

Modern C++20 PACS (Picture Archiving and Communication System) implementation with
DICOM standard implemented from scratch — zero external DICOM library dependency.
Leverages the kcenon ecosystem for infrastructure (networking, threading, logging,
containers). Currently in Phase 4 (Advanced Services & Production Hardening).

## Architecture

12 library targets organized in layered dependency:

```
pacs_core        - Tags, elements, datasets, Part 10 file I/O, dictionary
pacs_encoding    - VR types, transfer syntaxes, compression codecs
pacs_security    - RBAC, anonymization (PS3.15), digital signatures, TLS
pacs_network     - PDU encode/decode, association state machine, DIMSE, pipeline
pacs_client      - Job/routing/sync/prefetch/remote node management
pacs_services    - SCP/SCU implementations, SOP class registry, IOD validators
pacs_storage     - File storage, SQLite indexing, cloud (S3/Azure), HSM
pacs_ai          - AI service connector, result handler (SR/SEG)
pacs_monitoring  - Health checks, Prometheus metrics
pacs_workflow    - Auto prefetch, task scheduler, study lock
pacs_web         - REST API (Crow), DICOMweb (WADO/STOW/QIDO)
pacs_integration - Adapters to kcenon ecosystem
```

Web frontend: `web/` — React + Vite + Tailwind + React Query (separate build).

## Build & Test

```bash
# Quick build
cmake -S . -B build && cmake --build build
cd build && ctest --output-on-failure --timeout 120

# Build script with options
./scripts/build.sh --debug --tests
```

Key CMake options:
- `PACS_BUILD_TESTS` (ON) — Catch2 v3.4.0 unit tests
- `PACS_BUILD_EXAMPLES` (OFF) — 32 example programs
- `PACS_BUILD_BENCHMARKS` (OFF), `PACS_BUILD_SAMPLES` (OFF)
- `PACS_BUILD_STORAGE` (ON) — Requires SQLite3
- `PACS_BUILD_CODECS` (ON) — JPEG, JPEG2000, JPEG-LS, HTJ2K, RLE
- `PACS_WITH_OPENSSL` (ON), `PACS_WITH_REST_API` (ON)
- `PACS_WITH_AWS_SDK` (OFF), `PACS_WITH_AZURE_SDK` (OFF)

Presets: `default`, `debug`, `release`, `asan`, `tsan`, `ubsan`, `ci`, `vcpkg`

CI: Multi-platform (Ubuntu GCC/Clang, macOS, Windows MSVC), coverage, static analysis,
sanitizers, fuzzing, DCMTK interop testing, CVE scan, SBOM.

## Key Patterns

- **DICOM from scratch** — Full SOP class implementations without dcmtk/GDCM dependency
- **SOP classes** — Verification (C-ECHO), CT/MR/US/XA/NM/PET/RT/SR/SEG/MG/CR/SC Storage,
  Patient/Study Root Q/R, Modality Worklist, MPPS, Storage Commitment, Print, UPS, PIR, XDS-I.b
- **Transfer syntaxes** — Implicit/Explicit VR, JPEG Baseline/Lossless, JPEG 2000,
  JPEG-LS, RLE, HTJ2K, HEVC, Frame Deflate, JPEG XL
- **Network pipeline** — PDU -> DIMSE -> Service execution -> Response encode pipeline architecture
- **Association state machine** — Full DICOM Upper Layer association management
- **Result type** — `kcenon::common::Result<T>` throughout

## Ecosystem Position

**Tier 5** — Highest-level application in the kcenon ecosystem.

```
common_system      (Tier 0) [required] — Result<T>, IExecutor
container_system   (Tier 1) [required] — DICOM serialization
thread_system      (Tier 1) [via network_system]
logger_system      (Tier 2) [via network_system]
monitoring_system  (Tier 2) [optional via network_system]
network_system     (Tier 4) [required] — TCP/TLS, async operations
database_system    (Tier 3) [optional]
pacs_system        (Tier 5)  <-- this project
```

## Dependencies

**Required ecosystem**: common_system, container_system, network_system (which pulls thread_system, logger_system)
**Required native**: ICU (charset encoding), SQLite3 >= 3.45.1 (storage)
**Optional codecs**: libjpeg-turbo, OpenJPEG, CharLS, libpng, OpenJPH (HTJ2K), OpenSSL >= 3.0
**Fetched**: Catch2 v3.4.0, Crow v1.3.1 (REST API), ASIO 1.30.2

## Known Constraints

- Windows `std::min`/`std::max`: Must use `(std::min)(a, b)` parenthesization (Windows.h macros)
- `std::jthread` unavailable on AppleClang; auto-fallback to `std::thread` with `USE_STD_JTHREAD`
- libjpeg-turbo cannot be built via FetchContent (must be system package)
- `PACS_USE_MOCK_S3` prohibited in Release builds (CMake FATAL_ERROR)
- Linux static linking: circular dependency requires `--start-group/--end-group` linker flags
- C++20 modules experimental (CMake 3.28+, Clang 16+/GCC 14+)
- UWP/Xbox unsupported
- Test framework: Catch2 (differs from ecosystem's Google Test convention)
