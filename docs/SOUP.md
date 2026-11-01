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
| SOUP-002 | [container_system](https://github.com/kcenon/container_system) | kcenon | `fa56dc5e` | BSD-3-Clause | DICOM data serialization containers | B | None |
| SOUP-003 | [thread_system](https://github.com/kcenon/thread_system) | kcenon | `6dd5c9e8` | BSD-3-Clause | Thread pool, async task scheduling | B | Original integrated commit (`db8d36f1`) unrecoverable (orphaned by upstream history rewrite); re-pinned to v1.0.0 tag — see [PR-001](compliance/problem-resolution/PR-001-soup-dangling-sha.md). Re-V&V required before release. |
| SOUP-004 | [logger_system](https://github.com/kcenon/logger_system) | kcenon | `f119ecd6` | BSD-3-Clause | Structured logging infrastructure | A | None |
| SOUP-005 | [monitoring_system](https://github.com/kcenon/monitoring_system) | kcenon | `da708abb` | BSD-3-Clause | Performance metrics collection | A | None |
| SOUP-006 | [network_system](https://github.com/kcenon/network_system) | kcenon | `093691d9` | BSD-3-Clause | TCP/UDP socket abstraction for DICOM PDU | B | Overlapping socket writes under backpressure; see [below](#network_system-overlapping-writes-1319) |
| SOUP-007 | [database_system](https://github.com/kcenon/database_system) | kcenon | `593a18a7` | BSD-3-Clause | SQL query builder, database abstraction | B | None |

> **Note**: kcenon libraries are pinned by commit SHA (no release tags available).
> CI clones are managed by `.github/actions/checkout-kcenon-deps/action.yml`.
> The root [dependency-manifest.json](../dependency-manifest.json) is the canonical provenance input for internal SHAs, FetchContent fallbacks, native/system resolution, and frontend lockfiles.

### Third-Party C++ Libraries

| ID | Name | Manufacturer | Version | License | Usage in pacs_system | Safety Class | Known Anomalies |
|----|------|-------------|---------|---------|---------------------|-------------|-----------------|
| SOUP-008 | [SQLite3](https://www.sqlite.org/) | SQLite Consortium | 3.45.1 | Public Domain | Local metadata database storage | B | None |
| SOUP-009 | [OpenSSL](https://www.openssl.org/) | OpenSSL Project | System-provided | Apache-2.0 | TLS encryption, certificate management | C | CVE tracking via OS vendor |
| SOUP-010 | [Crow](https://github.com/CrowCpp/Crow) | CrowCpp | v1.3.2 | BSD-3-Clause | REST API / DICOMweb HTTP server | B | None |
| SOUP-011 | [ASIO](https://github.com/chriskohlhoff/asio) | Christopher Kohlhoff | asio-1-30-2 | BSL-1.0 | Async I/O for network operations | B | None |
| SOUP-012 | [libjpeg-turbo](https://libjpeg-turbo.org/) | libjpeg-turbo Project | System-provided | IJG/BSD-3-Clause | JPEG image compression codec | B | None |
| SOUP-013 | [OpenJPEG](https://www.openjpeg.org/) | UCLouvain | System-provided | BSD-2-Clause | JPEG 2000 image compression codec | B | None |
| SOUP-014 | [CharLS](https://github.com/team-charls/charls) | Team CharLS | System-provided | BSD-3-Clause | JPEG-LS image compression codec | B | None |
| SOUP-015 | [OpenJPH](https://github.com/aous72/OpenJPH) | Aous Naman | 0.30.1 | BSD-2-Clause | HTJ2K (High Throughput JPEG 2000) codec | B | (1) The prior `coded_lists` allocator alignment defect is fixed by [OpenJPH#262](https://github.com/aous72/OpenJPH/pull/262); generic decoder unaligned loads seen in 0.27.2 are fixed by [OpenJPH#291](https://github.com/aous72/OpenJPH/pull/291). All 15 focused HTJ2K cases pass with alignment instrumentation on arm64; the old alignment suppressions are removed. (2) The generic reversible colour transform still left-shifts negative samples (`ojph_colour.cpp:467` and `:485`). The shift is undefined before C++20: that applies to OpenJPH's default C++14 build, which the vcpkg port uses, but not to a FetchContent build, which inherits C++20 from pacs_system. The defect is reported as [OpenJPH#406](https://github.com/aous72/OpenJPH/issues/406) and fixed upstream by [PR405](https://github.com/aous72/OpenJPH/pull/405), commit `c7a4d43942d2f92fdc385a8c4148a9b523063192`, but no published release contains that fix as checked on 2026-11-01. The pinned source remains unchanged; #1299 adds ARM coverage of both language modes, with a source-specific `shift-base` suppression for C++14 only (see [OpenJPH generic RCT tracking](#openjph-generic-rct-tracking-1299)). (3) Historical 0.21.0&ndash;0.21.4 AVX-512 encoder faults (#1257) require a system-provided OpenJPH on x86-64 to be 0.21.5 or later, or built with `OJPH_DISABLE_AVX512=ON`. |
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

For root vcpkg overrides, also run `scripts/check_vcpkg_overrides.py --vcpkg-root <checkout>`.
It reads version files from the configured builtin baseline commit, not the checkout's HEAD,
and checks exact port revisions. Crow 1.3.2 and OpenJPH 0.30.1 share baseline
`33f8517b96502c15c69b22512bbd7797195f24fc`; FetchContent uses the matching upstream releases
`v1.3.2` and `0.30.1` (both port revision 0). OpenJPH 0.27.2 was evaluated but its generic
decoder failed UBSan alignment checks; 0.30.1 includes the subsequent memcpy load fix
([OpenJPH#291](https://github.com/aous72/OpenJPH/pull/291)). Database availability and FetchContent version agreement
are required checks in SOUP Version Drift Detection; feature builds remain separate checks.

---

## network_system overlapping writes (#1319)

`tcp_socket::async_send` (`libs/network-tcp/src/tcp_socket.cpp`) and `secure_tcp_socket::async_send`
start `asio::async_write` for every call. Asio requires that no other write starts until that
operation completes. When a write completes only in part, because the peer reads slowly and the
socket buffers are full, the next send runs inside it and the byte stream is interleaved.
`i_session` reports neither completion nor pending bytes, so PACS cannot serialize its writes
without changing that library. The TLS path has the same code and was not measured.

**Effect on PACS.** C-FIND streams responses without acknowledgements, so a slow reader breaks
PDU framing. Against a slowed DCMTK `findscu`, four of four runs ended in `Find Failed` after
15,924 to 41,682 of 60,000 matches, and one of them stored one damaged response first. A retrieve
sends each object as one write and waits for its acknowledgement, so objects do not overlap each
other. `qr.protocol.find-scp.slow-reader-keeps-pdu-framing` reproduces the failure and fails on
this pin. It stays failing until the pin moves.

**Status.** Not reported upstream and not patched here. As checked on 2026-11-01, upstream `main`
has the same code and no matching issue. In a local experiment, a write queue in `tcp_socket` made
that case pass three of three runs and the slowed `findscu` receive all 60,000 matches exactly in
two of two runs. That patch is not part of any delivered build.

---

## OpenJPH generic RCT tracking (#1299)

No duplicate third-party report is needed: [OpenJPH#406](https://github.com/aous72/OpenJPH/issues/406)
already describes the failure, and [PR405](https://github.com/aous72/OpenJPH/pull/405) changes both
generic line-width branches from a negative signed left shift to multiplication. As checked on
2026-11-01, the latest published release is 0.32.0, which predates that fix. The aligned
FetchContent/vcpkg pin remains 0.30.1; the anomaly has not been removed or claimed fixed locally.

Whether the shift is undefined depends on the language mode. OpenJPH 0.30.1 selects C++14 only when
`CMAKE_CXX_STANDARD` is unset, so the same source behaves differently per delivery path (the original
report against 0.21.5 saw `-std=c++14` in this project's build):

| Delivery path | OpenJPH compiled as | Negative left shift | ARM UBSan result |
|---------------|---------------------|---------------------|------------------|
| vcpkg port, standalone OpenJPH | C++14 | Undefined | Reported; passes only with the tracked suppression |
| FetchContent (default) | C++20, inherited from pacs_system | Defined | Not reported; passes with no suppression |

The C++20 result is a language rule, not a repair: the expression is unchanged and the C++14 path
still executes it, so the anomaly stays open until a fixed release is adopted. Project builds do not
instrument the vcpkg port's libraries, so the job below compiles the fetched source in C++14 mode to
represent that path.

For the 32-bit transformed line, signed 30-bit component samples keep every intermediate and
`R + 2G + B` within int32; the weighted sum is in `[-2^31, 2^31-4]`. The 64-bit transformed line
consumes int32 samples and its sum is in `[-2^33, 2^33-4]`, well within int64. Multiplication by two
therefore adds no signed overflow for these domains, including PACS's 8/12/16-bit inputs. Tests
exercise all 512 combinations of eight boundary/negative/positive sample values per branch,
compare against an independent floor-division model, and require exact inverse reconstruction.
They call the generic functions directly and verify ARM dispatch selects the generic RCT.

The `HTJ2K arm64 UBSan` job in `.github/workflows/sanitizers.yml` is non-advisory and does not use
the general CTest quarantine. It compiles the dependency and PACS codec with UBSan, enables
`PACS_TEST_OPENJPH_INTERNALS`, and runs `scripts/check_openjph_arm_ubsan.py` once per mode. The script
first requires `compile_commands.json` to show the expected `-std` flag and `-fsanitize=undefined` for
`ojph_colour.cpp`, and its object to reference UBSan handlers. Then:

- With `PACS_TEST_OPENJPH_CXX_STANDARD=14`, four unsuppressed RGB/metadata/generic-width probes must
  report the shift, on separate source lines for the two widths. All HTJ2K cases then must pass with the
  source-specific RCT `shift-base` suppression; no other suppression may name OpenJPH or use a wildcard.
- With that option cleared, all HTJ2K cases must pass with no suppression file loaded.

In both modes a C++14 control must still report an unrelated negative shift while the suppression is
loaded, and the same source built as C++20 must report nothing for the shift yet still report a signed
overflow. That ties a quiet C++20 run to the language rule and not to a lost sanitizer. PR405 also
rewrites same-class shifts in `ojph_transform.cpp` and `ojph_resolution.cpp`; they are not suppressed,
so a C++14 report from them fails the job. Raw exits, UBSAN_OPTIONS, compile commands, logs and Catch2
XML are retained. The C++14 mode is tracked suppressed coverage and the C++20 mode is an unsuppressed
pass of unchanged source; neither is a fix. Local validation and hosted workflow execution are distinct
evidence.

When a released fix is adopted, synchronize both delivery paths and the registry baseline,
remove the `shift-base` suppression and require the unsuppressed pass in both modes. Retain the
boundary, reversibility and unrelated-failure checks.

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
