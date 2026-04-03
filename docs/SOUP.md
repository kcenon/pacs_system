---
doc_id: "PAC-PROJ-006"
doc_title: "SOUP List &mdash; pacs_system"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "pacs_system"
category: "PROJ"
---

# SOUP List &mdash; pacs_system

> **SSOT**: This document is the single source of truth for **SOUP List &mdash; pacs_system**.

> **Software of Unknown Provenance (SOUP) Register per IEC 62304:2006+AMD1:2015 &sect;8.1.2**
>
> This document is the authoritative reference for all external software dependencies.
> Every entry must include: title, manufacturer, unique version identifier, license, and known anomalies.

| Document | Version |
|----------|---------|
| IEC 62304 Reference | &sect;8.1.2 Software items from SOUP |
| Last Reviewed | 2026-03-10 |
| pacs_system Version | 0.1.0 |

---

## C++ Backend Dependencies

### Tier 0&ndash;4: kcenon Ecosystem Libraries

| ID | Name | Manufacturer | Version (Commit SHA) | License | Usage in pacs_system | Safety Class | Known Anomalies |
|----|------|-------------|----------------------|---------|---------------------|-------------|-----------------|
| SOUP-001 | [common_system](https://github.com/kcenon/common_system) | kcenon | `47be5fd2` | BSD-3-Clause | Result&lt;T&gt; pattern, error handling primitives | B | None |
| SOUP-002 | [container_system](https://github.com/kcenon/container_system) | kcenon | `0b95a0c1` | BSD-3-Clause | DICOM data serialization containers | B | None |
| SOUP-003 | [thread_system](https://github.com/kcenon/thread_system) | kcenon | `db8d36f1` | BSD-3-Clause | Thread pool, async task scheduling | B | None |
| SOUP-004 | [logger_system](https://github.com/kcenon/logger_system) | kcenon | `f119ecd6` | BSD-3-Clause | Structured logging infrastructure | A | None |
| SOUP-005 | [monitoring_system](https://github.com/kcenon/monitoring_system) | kcenon | `221bf3d2` | BSD-3-Clause | Performance metrics collection | A | None |
| SOUP-006 | [network_system](https://github.com/kcenon/network_system) | kcenon | `093691d9` | BSD-3-Clause | TCP/UDP socket abstraction for DICOM PDU | B | None |
| SOUP-007 | [database_system](https://github.com/kcenon/database_system) | kcenon | `b90b0f3b` | BSD-3-Clause | SQL query builder, database abstraction | B | None |

> **Note**: kcenon libraries are pinned by commit SHA (no release tags available).
> CI clones are managed by `.github/actions/checkout-kcenon-deps/action.yml`.
> The root [dependency-manifest.json](../dependency-manifest.json) is the canonical provenance input for internal SHAs, FetchContent fallbacks, native/system resolution, and frontend lockfiles.

### Third-Party C++ Libraries

| ID | Name | Manufacturer | Version | License | Usage in pacs_system | Safety Class | Known Anomalies |
|----|------|-------------|---------|---------|---------------------|-------------|-----------------|
| SOUP-008 | [SQLite3](https://www.sqlite.org/) | SQLite Consortium | 3.45.1 | Public Domain | Local metadata database storage | B | None |
| SOUP-009 | [OpenSSL](https://www.openssl.org/) | OpenSSL Project | System-provided | Apache-2.0 | TLS encryption, certificate management | C | CVE tracking via OS vendor |
| SOUP-010 | [Crow](https://github.com/CrowCpp/Crow) | CrowCpp | v1.3.1 | BSD-3-Clause | REST API / DICOMweb HTTP server | B | None |
| SOUP-011 | [ASIO](https://github.com/chriskohlhoff/asio) | Christopher Kohlhoff | asio-1-30-2 | BSL-1.0 | Async I/O for network operations | B | None |
| SOUP-012 | [libjpeg-turbo](https://libjpeg-turbo.org/) | libjpeg-turbo Project | System-provided | IJG/BSD-3-Clause | JPEG image compression codec | B | None |
| SOUP-013 | [OpenJPEG](https://www.openjpeg.org/) | UCLouvain | System-provided | BSD-2-Clause | JPEG 2000 image compression codec | B | None |
| SOUP-014 | [CharLS](https://github.com/team-charls/charls) | Team CharLS | System-provided | BSD-3-Clause | JPEG-LS image compression codec | B | None |
| SOUP-015 | [OpenJPH](https://github.com/aous72/OpenJPH) | Aous Naman | 0.18.2 | BSD-2-Clause | HTJ2K (High Throughput JPEG 2000) codec | B | None |
| SOUP-016 | [fmt](https://github.com/fmtlib/fmt) | Victor Zverovich | 10.2.1 | MIT | String formatting library | A | None |
| SOUP-017 | [Catch2](https://github.com/catchorg/Catch2) | Catch2 Authors | v3.4.0 | BSL-1.0 | Unit test framework (test only) | A | None |
| SOUP-018 | [ICU](https://icu.unicode.org/) | Unicode Consortium | vcpkg baseline | Unicode-3.0 | Character set conversion (DICOM Specific Character Set) | B | None |
| SOUP-019 | [libpng](http://www.libpng.org/) | PNG Development Group | System-provided | libpng-2.0 | PNG image output | A | None |

### Optional Cloud SDKs (Disabled by Default)

| ID | Name | Manufacturer | Version | License | Usage in pacs_system | Safety Class | Known Anomalies |
|----|------|-------------|---------|---------|---------------------|-------------|-----------------|
| SOUP-020 | [AWS SDK for C++](https://github.com/aws/aws-sdk-cpp) | Amazon Web Services | N/A (opt-in) | Apache-2.0 | S3 cloud storage integration | A | Disabled by default |
| SOUP-021 | [Azure SDK for C++](https://github.com/Azure/azure-sdk-for-cpp) | Microsoft | N/A (opt-in) | MIT | Azure Blob storage integration | A | Disabled by default |

---

## Web Frontend Dependencies

> Frontend dependencies are managed via `web/package.json` and pinned via `web/package-lock.json`.

### Runtime Dependencies

| ID | Name | Manufacturer | Version | License | Usage in pacs_system | Safety Class | Known Anomalies |
|----|------|-------------|---------|---------|---------------------|-------------|-----------------|
| SOUP-022 | [React](https://react.dev/) | Meta | ^19.2.0 | MIT | UI rendering framework | A | None |
| SOUP-023 | [react-router-dom](https://reactrouter.com/) | Remix Software | ^7.12.0 | MIT | Client-side routing | A | None |
| SOUP-024 | [@tanstack/react-query](https://tanstack.com/query) | TanStack | ^5.90.12 | MIT | Server state management, data fetching | A | None |
| SOUP-025 | [axios](https://axios-http.com/) | Matt Zabriskie | ^1.13.5 | MIT | HTTP client for API communication | A | None |
| SOUP-026 | [zod](https://zod.dev/) | Colin McDonnell | ^4.1.13 | MIT | Runtime schema validation | A | None |
| SOUP-027 | [recharts](https://recharts.org/) | Recharts Group | ^3.5.1 | MIT | Data visualization charts | A | None |

### Build/Dev Dependencies (Not Shipped)

| ID | Name | Version | License | Purpose |
|----|------|---------|---------|---------|
| SOUP-028 | [TypeScript](https://www.typescriptlang.org/) | ~5.9.3 | Apache-2.0 | Static type checking |
| SOUP-029 | [Vite](https://vite.dev/) | ^7.2.4 | MIT | Build tool and dev server |
| SOUP-030 | [Tailwind CSS](https://tailwindcss.com/) | ^4.1.17 | MIT | Utility-first CSS framework |

---

## Safety Classification Key

| Class | Definition | Example |
|-------|-----------|---------|
| **A** | No contribution to hazardous situation | Logging, formatting, test frameworks |
| **B** | Non-serious injury possible | Data processing, network communication |
| **C** | Death or serious injury possible | Encryption, access control |

---

## Version Update Process

When updating any SOUP dependency:

1. Update the version in the source of truth (`CMakeLists.txt`, `package.json`, or `.github/actions/checkout-kcenon-deps/action.yml`)
2. Update the corresponding row in this document
3. Verify no new known anomalies (check CVE databases)
4. Run full CI/CD pipeline to confirm compatibility
5. Document the change in the PR description

---

## License Compliance Summary

| License | Copyleft | Primary obligation |
|---------|----------|--------------------|
| MIT | No | Include copyright notice |
| BSD-2-Clause | No | Include copyright notice |
| BSD-3-Clause | No | Include copyright notice and no-endorsement clause |
| BSL-1.0 | No | Include license text |
| Apache-2.0 | No | Include license text and NOTICE when required |
| Public Domain | No | Preserve provenance |
| Unicode-3.0 | No | Include copyright notice |
| libpng-2.0 | No | Include copyright notice |
| IJG | No | Include copyright notice |

> **GPL contamination**: None detected. All dependencies are permissively licensed.
> **Distribution note**: the product-level third-party inventory is maintained in [LICENSE-THIRD-PARTY](../LICENSE-THIRD-PARTY).
