# PACS System

Status: active (ongoing development toward v1.0) | Latest release: [v0.1.0](https://github.com/kcenon/pacs_system/releases/tag/v0.1.0)

> **Language:** **English** | [한국어](README.kr.md)

[![CI](https://github.com/kcenon/pacs_system/actions/workflows/ci.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/ci.yml)
[![Integration Tests](https://github.com/kcenon/pacs_system/actions/workflows/integration-tests.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/integration-tests.yml)
[![Code Coverage](https://github.com/kcenon/pacs_system/actions/workflows/coverage.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/coverage.yml)
[![Static Analysis](https://github.com/kcenon/pacs_system/actions/workflows/static-analysis.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/static-analysis.yml)
[![Documentation](https://github.com/kcenon/pacs_system/actions/workflows/build-Doxygen.yaml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/build-Doxygen.yaml)
[![SBOM Generation](https://github.com/kcenon/pacs_system/actions/workflows/sbom.yml/badge.svg)](https://github.com/kcenon/pacs_system/actions/workflows/sbom.yml)

## Overview

A C++20 PACS (Picture Archiving and Communication System) built on the kcenon ecosystem. The
DICOM data model, encodings, network protocol, and services are implemented in this repository;
there is no DCMTK or GDCM dependency. DCMTK appears only as a test peer in the
[DCMTK interoperability workflow](.github/workflows/dcmtk-interop.yml).

## Features

- **DICOM services**: Storage, Query/Retrieve, Modality Worklist, MPPS, Storage Commitment, and Print Management SCP/SCU; DICOMweb (WADO-RS, STOW-RS, QIDO-RS) on a Crow REST server
- **Modular libraries**: core, encoding, network, services, storage, and security, plus optional AI, monitoring, workflow, web, and integration components that link separately
- **Building blocks**: thread pools and queues from thread_system, TCP/TLS transport from network_system, and SIMD code paths for byte swapping, RLE, photometric conversion, and window/level (`include/kcenon/pacs/encoding/simd/`)
- **Quality checks**: CI/CD with [sanitizers](.github/workflows/sanitizers.yml), [static analysis](.github/workflows/static-analysis.yml), and [fuzzing](.github/workflows/fuzzing.yml)
- **Platforms**: CI builds on Ubuntu (GCC and Clang), macOS, and Windows (MSVC) and runs CTest on each ([ci.yml](.github/workflows/ci.yml))

## Table of Contents

- [Project Status](#project-status)
- [DICOM Conformance](#dicom-conformance)
- [Getting Started](#getting-started)
- [CLI Tools & Examples](#cli-tools--examples)
- [Ecosystem Dependencies](#ecosystem-dependencies)
- [Architecture](#architecture)
- [Project Structure](#project-structure)
- [Performance](#performance)
- [Documentation](#documentation)
- [Code Statistics](#code-statistics)
- [Compliance](#compliance)
- [Contributing](#contributing)
- [License](#license)

## Project Status

**Latest Published Package**: 0.1.0 — [`v0.1.0` release](https://github.com/kcenon/pacs_system/releases/tag/v0.1.0), vcpkg port `kcenon-pacs-system` 0.1.0 (port-version 11)
**Planned Milestone**: v1.0 — unreleased API contract; no v1.0.0 tag, GitHub release, or registry version exists yet (gated by [#1095](https://github.com/kcenon/pacs_system/issues/1095) and [#1164](https://github.com/kcenon/pacs_system/issues/1164))
**Project Phase**: the components listed for Phases 1-4 below are in the `v0.1.0` tag; later work on the default branch (for example the IHE XDS.b actors, the `include/kcenon/pacs/` header layout, and the `pacs_system::pacs_system` CMake contract) is unreleased

The planned v1.0 contract covers the public header surface under `include/kcenon/pacs/`, the
`pacs_system::pacs_system` aggregate CMake target, and the documented Doxygen API. It is frozen for
review, not released: it ships only after the #1095 readiness checklist closes and every Tier 0-4
ecosystem dependency publishes v1.0 (#1164). See the [header inventory](docs/v1.0-api-surface.md),
the [planned exception/`Result<T>` contract](docs/v1.0-throw-policy.md), the
[pre-freeze deprecation audit](docs/v1.0-deprecation-inventory.md), and the
[0.x → 1.0 Migration Guide](docs/migration/0.x-to-1.0.md). The [CHANGELOG](CHANGELOG.md) records the
change set under `[Unreleased]`, including the `samples/` → `examples/` and `examples/` → `tools/`
relocation (#1139) and the CMake contract (#1158), and gets no v1.0.0 section until v1.0.0 is tagged.

| Phase | Scope | In `v0.1.0` |
|-------|-------|-------------|
| **Phase 1**: Foundation | DICOM Core, Tag Dictionary, File I/O (Part 10), Transfer Syntax | Yes |
| **Phase 2**: Network Protocol | Upper Layer Protocol (PDU), Association State Machine, DIMSE-C, Compression Codecs | Yes |
| **Phase 3**: Core Services | Storage SCP/SCU, File Storage, Index Database, Query/Retrieve, Logging, Monitoring | Yes |
| **Phase 4**: Advanced Services | REST API, DICOMweb, AI Integration, Client Module, Cloud Storage, Print Management, Security, Workflow, Annotation/Viewer | Yes |

CI runs the unit tests ([ci.yml](.github/workflows/ci.yml)) and uploads coverage reports to
[Codecov](https://codecov.io/gh/kcenon/pacs_system); per-phase feature lists are in [Features](docs/FEATURES.md).

## DICOM Conformance

The [DICOM Conformance Statement](docs/DICOM_CONFORMANCE_STATEMENT.md) is the source for the
supported SOP classes (section 3): Verification; CT, MR, US, XA, XRF, X-Ray, NM, PET, RT, SR, SEG,
MG, CR, and SC Storage; Patient and Study Root Query/Retrieve; Modality Worklist; MPPS; Storage
Commitment; and Basic Grayscale Print, Basic Color Print, and Printer. The
[DCMTK interoperability workflow](.github/workflows/dcmtk-interop.yml) exercises C-ECHO, C-STORE,
C-FIND, and C-MOVE against DCMTK tools on Ubuntu and macOS.

### Transfer Syntax Support

Codec-backed syntaxes need `PACS_BUILD_CODECS=ON` (the default) and the named library at configure
time; CMake disables a codec with a warning when its library is missing (`cmake/dependencies.cmake`).
Section 4 of the Conformance Statement lists further syntaxes (Deflated Explicit VR Little Endian,
HTJ2K, HEVC, JPEG XL, Frame Deflate).

| Transfer Syntax | UID | Status |
|----------------|-----|--------|
| Implicit VR Little Endian | 1.2.840.10008.1.2 | Built in |
| Explicit VR Little Endian | 1.2.840.10008.1.2.1 | Built in |
| Explicit VR Big Endian | 1.2.840.10008.1.2.2 | Built in |
| JPEG Baseline (Process 1) | 1.2.840.10008.1.2.4.50 | Requires libjpeg-turbo at build time |
| JPEG Lossless (Process 14) | 1.2.840.10008.1.2.4.70 | Built in |
| JPEG 2000 Lossless | 1.2.840.10008.1.2.4.90 | Requires OpenJPEG at build time |
| JPEG 2000 Lossy | 1.2.840.10008.1.2.4.91 | Requires OpenJPEG at build time |
| JPEG-LS Lossless | 1.2.840.10008.1.2.4.80 | Requires CharLS at build time |
| JPEG-LS Near-Lossless | 1.2.840.10008.1.2.4.81 | Requires CharLS at build time |
| RLE Lossless | 1.2.840.10008.1.2.5 | Built in |

## Getting Started

### Prerequisites

- C++20 compiler with Concepts support: GCC 10+ (GCC 13+ recommended for full `std::format` support), Clang 10+ (Clang 14+ recommended), or MSVC 2022 (19.30+)
- CMake 3.20+ and Ninja (recommended); kcenon ecosystem libraries are downloaded by CMake

| Platform | Dependencies |
|----------|--------------|
| Linux (Ubuntu 24.04+) | `sudo apt install cmake ninja-build libicu-dev libsqlite3-dev libssl-dev libfmt-dev` |
| macOS | `brew install cmake ninja icu4c sqlite3 openssl@3 fmt` |
| Windows | Visual Studio 2022 with the C++ workload and [vcpkg](https://vcpkg.io/): `icu`, `sqlite3`, `openssl`, `fmt` |

### Build

```bash
git clone https://github.com/kcenon/pacs_system.git
cd pacs_system
cmake -S . -B build
cmake --build build
cd build && ctest --output-on-failure
```

On Windows (PowerShell), run `vcpkg install icu:x64-windows sqlite3:x64-windows openssl:x64-windows fmt:x64-windows`
and add `-G Ninja -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"` to
the configure command.

Unit tests use **Catch2 v3.4.0**, which differs from the kcenon ecosystem default of GoogleTest;
see [`docs/ECOSYSTEM.md`](docs/ECOSYSTEM.md#per-system-conventions) and
[#1141](https://github.com/kcenon/pacs_system/issues/1141). Windows coding rules such as
`(std::min)(a, b)` are in the [contributing guide](docs/contributing/CONTRIBUTING.md#windows-compatibility).

### Build Options

```cmake
PACS_BUILD_TESTS (ON)              # Enable unit tests
PACS_BUILD_EXAMPLES (OFF)          # Build CLI tools (tools/)
PACS_BUILD_BENCHMARKS (OFF)        # Enable benchmarks
PACS_BUILD_STORAGE (ON)            # Build storage module
```

### Installation via vcpkg

```bash
vcpkg install kcenon-pacs-system
```

This installs the latest published package, 0.1.0 (port-version 11), built from the `v0.1.0`
source archive. The 0.1.0 package installs headers under `include/pacs/` and exports
`kcenon::pacs::<component>` targets (`core`, `encoding`, `network`, `client`, `services`,
`security`, `integration`, plus feature-dependent components):

```cmake
find_package(pacs_system 0.1 CONFIG REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE kcenon::pacs::core)
```

> The `usage` text shipped with port-version 11 lists `pacs_system::*` target names. Those names
> belong to the planned v1.0 contract below and are not exported by the 0.1.0 package (registry
> correction tracked in [kcenon/vcpkg-registry#104](https://github.com/kcenon/vcpkg-registry/issues/104)).
> Optional features and the full package record are in [Package State](docs/PACKAGE_STATE.md#vcpkg-features).

**Planned v1.0 CMake contract** (#1158): the aggregate target `pacs_system::pacs_system`; no
published package ships it yet. A source install of this branch (`cmake --install`) already provides
it but reports package version 0.1.0, so request `find_package(pacs_system 0.1 REQUIRED)` until
v1.0.0 is released. The consumer snippet, component targets, and the legacy alias
`kcenon::pacs_system` are in [Package State](docs/PACKAGE_STATE.md#cmake-targets-planned-v10-contract).

## CLI Tools & Examples

- `tools/` — one subdirectory per tool: the CLI utilities plus the `module_example` C++20 module demo (32 subdirectories); build with `-DPACS_BUILD_EXAMPLES=ON`
- `examples/` — 5 progressive tutorials under `examples/tutorials/` (`01_hello_dicom` through `05_production_pacs`); build with `-DPACS_BUILD_SAMPLES=ON`

The option names are kept for compatibility with existing CI and downstream consumers
(see [#1139](https://github.com/kcenon/pacs_system/issues/1139)). Quick examples after a build:

```bash
./build/bin/echo_scu localhost 11112                  # verify connectivity (C-ECHO)
./build/bin/store_scu localhost 11112 image.dcm       # send a file (C-STORE)
./build/bin/query_scu localhost 11112 PACS_SCP --level STUDY --patient-name "DOE^*"
./build/bin/dcm_dump image.dcm --format json          # inspect a file
```

The tools cover file utilities (dump, modify, convert, anonymize, DICOMDIR, JSON and XML),
network clients (echo, store, find, move, get, print), and servers (Query/Retrieve, worklist,
MPPS, `pacs_server`). Every tool and option: [CLI Reference](docs/CLI_REFERENCE.md).

## Ecosystem Dependencies

| System | Use in pacs_system | Required |
|--------|--------------------|----------|
| [common_system](https://github.com/kcenon/common_system) | `Result<T>`, `IExecutor`, event bus | Yes (Tier 0) |
| [container_system](https://github.com/kcenon/container_system) | DICOM data serialization | Yes (Tier 1) |
| [network_system](https://github.com/kcenon/network_system) | TCP/TLS transport for DICOM | Yes (Tier 4) |
| [thread_system](https://github.com/kcenon/thread_system) | Thread pools and queues | Via network_system (Tier 1) |
| [logger_system](https://github.com/kcenon/logger_system) | Logging infrastructure | Via network_system (Tier 2) |
| [monitoring_system](https://github.com/kcenon/monitoring_system) | Metrics and tracing | Optional (Tier 3) |
| [database_system](https://github.com/kcenon/database_system) | Query builder for the storage index repositories when found at configure time (`PACS_WITH_DATABASE_SYSTEM`) | Optional (Tier 3) |

The dependency graph is in [Architecture](docs/ARCHITECTURE.md#ecosystem-dependencies); versions,
licenses, and safety classifications are in the [IEC 62304 SOUP register](docs/SOUP.md).

## Architecture

The [layer diagram](docs/ARCHITECTURE.md#layer-overview) runs from the REST/DICOMweb, AI, and
workflow front ends through IHE actors, DICOM services, network and security, and core/encoding to
the kcenon ecosystem libraries. C++20 module interfaces for `kcenon.pacs` exist under `src/modules/`,
but the module build does not compile at this revision and no CI job builds with
`PACS_BUILD_MODULES=ON`; use the header-based interface ([status and the 12 partitions](docs/ARCHITECTURE.md#c20-module-architecture)).

## Project Structure

Public headers live under `include/kcenon/pacs/<module>/`. The legacy `include/pacs/...` path was
retired; all consumers should `#include <kcenon/pacs/...>`.

```
pacs_system/
├── include/kcenon/pacs/  # Public headers
├── src/                  # Source implementations
├── tests/                # Test suites
├── examples/             # Progressive tutorials (examples/tutorials/)
├── tools/                # CLI utilities, one subdirectory per tool
├── docs/                 # Documentation (Markdown)
└── CMakeLists.txt        # Build configuration (project VERSION 0.1.0)
```

The per-module table and the file-level tree are in [Project Structure](docs/PROJECT_STRUCTURE.md#public-header-modules);
file, line, and test counts are in [Code Statistics](#code-statistics), checked in CI by `scripts/check-stats-drift.sh`.

## Performance

[Benchmarks](docs/BENCHMARKS.md) lists the suites, targets, run commands, and recorded results with
their environment, date, command, and result file; [Migration Complete](docs/MIGRATION_COMPLETE.md) summarizes the thread system migration.

## Documentation

| Topic | Documents |
|-------|-----------|
| API | [API Reference (Doxygen)](https://kcenon.github.io/pacs_system/), generated from `include/kcenon/pacs/**`; [narrative API reference](docs/API_REFERENCE.md) |
| Design | [Architecture](docs/ARCHITECTURE.md), [Product Requirements](docs/PRD.md), [Implementation Analysis](docs/PACS_IMPLEMENTATION_ANALYSIS.md), [Features](docs/FEATURES.md), [Project Structure](docs/PROJECT_STRUCTURE.md) |
| Tools | [CLI Reference](docs/CLI_REFERENCE.md) |
| Conformance | [DICOM Conformance Statement](docs/DICOM_CONFORMANCE_STATEMENT.md); [IHE Integration Statement](docs/IHE_INTEGRATION_STATEMENT.md) — IHE profile self-declaration (XDS-I.b, AIRA, PIR); not validated at a Connectathon |
| Upgrading | [0.x to 1.0 Migration Guide](docs/migration/0.x-to-1.0.md): every breaking change between the 0.1.0 baseline and the planned 1.0.0 API freeze, with before/after snippets, a `Result<T>` migration walkthrough, and a self-audit checklist |
| Database | [Migration Guide](docs/database/MIGRATION_GUIDE.md), [Query Builder API](docs/database/API_REFERENCE.md), [Performance Guide](docs/database/PERFORMANCE_GUIDE.md), [ADR-001](docs/adr/ADR-001-database-system-integration.md), [ADR-002](docs/adr/ADR-002-pacs-storage-port-segmentation.md) (storage boundary and repository-set contract) |
| Packaging | [Package State](docs/PACKAGE_STATE.md), [Dependency Manifest](dependency-manifest.json) (native, fetched, internal, and frontend provenance), [Third-Party Licenses](LICENSE-THIRD-PARTY) |
| Index | [Documentation registry](docs/README.md) |

## Code Statistics

Generated from the tree by `scripts/update-readme-stats.sh` (counting rules in `scripts/calculate-stats.sh`);
the [Documentation Statistics workflow](.github/workflows/docs-stats.yml) fails when a value drifts beyond its threshold
or when the copy in `README.kr.md` differs.

<!-- STATS_START -->
<!-- source: generated by scripts/update-readme-stats.sh from scripts/calculate-stats.sh; checked by scripts/check-stats-drift.sh in the Documentation Statistics workflow (.github/workflows/docs-stats.yml) -->

| Metric | Value |
|--------|-------|
| **Header Files** | 291 files |
| **Source Files** | 217 files |
| **Header LOC** | ~78,700 lines |
| **Source LOC** | ~114,800 lines |
| **Example LOC** | ~4,100 lines |
| **Test LOC** | ~89,600 lines |
| **Total LOC** | ~287,300 lines |
| **Test Files** | 191 files |
| **Test Cases** | 2667+ tests |
| **Example Programs** | 5 apps |
| **Documentation** | 109 markdown files |
| **CI/CD Workflows** | 23 workflows |
| **Version** | 0.1.0 |

<!-- STATS_END -->

## Compliance

`pacs_system` provides technical primitives that healthcare organizations may use as part of an
Information Security Management System (ISMS). The library is not itself certified; adopters integrate
it and supply the organizational controls (policy, training, incident response, business-continuity planning).

- [SaMD certification readiness checklists](docs/checklists/README.md) — scope and applicability decisions, nine technical/QMS reviews, evidence records and proposed release gates
- [ISO 27799 Control Mapping](docs/compliance/iso-27799.md) — how ATNA audit trail, audit log encryption, TLS policy, access control, and anonymization map to ISO 27799:2016 clauses 7.4–7.7
- [DICOM Conformance Statement](docs/DICOM_CONFORMANCE_STATEMENT.md) — ISO 12052 / DICOM 2023e services, SOP classes, and transfer syntaxes implemented
- [common_system / ISO Standards Overview](https://github.com/kcenon/common_system/blob/main/docs/compliance/ISO_OVERVIEW.md) — the ecosystem-level index of all ISO standards the kcenon systems touch

## Contributing

Read the [contributing guide](docs/contributing/CONTRIBUTING.md) before submitting pull requests. README
changes follow the [README editing policy](docs/contributing/README_POLICY.md), checked by `python3 scripts/readme_lint.py`.

- **Project Owner**: kcenon (kcenon@naver.com)
- **Repository**: https://github.com/kcenon/pacs_system
- **Issues**: https://github.com/kcenon/pacs_system/issues

## License

This project is licensed under the BSD 3-Clause License; see the [LICENSE](LICENSE) file.
