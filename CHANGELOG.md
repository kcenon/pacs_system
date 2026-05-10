# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Removed

- Audited public and internal source for `[[deprecated]]` markers ahead of v1.0 freeze; no deprecated APIs were found, so no symbols were removed in this audit. Inventory recorded in [`docs/v1.0-deprecation-inventory.md`](docs/v1.0-deprecation-inventory.md) ([#1160](https://github.com/kcenon/pacs_system/issues/1160))

### Added

- IHE XDS.b Document Source actor (ITI-41) at `src/ihe/xds/` with pugixml + libcurl transport stack ([#1128](https://github.com/kcenon/pacs_system/issues/1128))
- IHE XDS.b Document Consumer actor (ITI-43) at `src/ihe/xds/` with retrieve envelope builder and MTOM/XOP response parser ([#1129](https://github.com/kcenon/pacs_system/issues/1129))
- IHE XDS.b Registry Query actor (ITI-18) at `src/ihe/xds/` with FindDocuments and GetDocuments stored queries against a conformant XDS.b Document Registry ([#1130](https://github.com/kcenon/pacs_system/issues/1130))
- ATNA audit emission for IHE XDS.b transactions (ITI-18/41/43) with Gazelle-lite integration test harness, and published `docs/IHE_CONFORMANCE.md` ([#1131](https://github.com/kcenon/pacs_system/issues/1131))

### Changed

- API freeze for v1.0: enumerate the 290-header public surface under `include/kcenon/pacs/` in `docs/v1.0-api-surface.md`; confirm zero `[[deprecated]]` symbols and no experimental staging area; mark `network/detail/accept_worker.h` as the only implementation-detail header outside the v1.0 stability promise ([#1156](https://github.com/kcenon/pacs_system/issues/1156))
- Remove committed `.key` files from version control; regenerate via `generate_test_certs.sh` in CI and local development ([#1092](https://github.com/kcenon/pacs_system/issues/1092))
- Consolidate directory layout: `samples/` renamed to `examples/` (5 progressive tutorials) and `examples/` renamed to `tools/` (32 CLI utility binaries) so that the role split matches the ecosystem standard. CMake option names (`PACS_BUILD_EXAMPLES`, `PACS_BUILD_SAMPLES`) are preserved for backward compatibility ([#1139](https://github.com/kcenon/pacs_system/issues/1139))
- Align `cmake/*.cmake` modules with the canonical ecosystem template at `common_system/cmake/template`; pacs-specific modules (`pacs_system-config.cmake.in`, `summary.cmake`) and intentional design divergences are documented in `cmake/DEVIATIONS.md`, and the aligned template version is recorded in `cmake/VERSION` ([#1140](https://github.com/kcenon/pacs_system/issues/1140))

### BREAKING

- Downstream consumers that referenced the old paths (`samples/...` for tutorials or `examples/...` for the CLI utilities) must update to the new locations: tutorials are now under `examples/`, and CLI utility sources live under `tools/`. Build outputs for tutorials moved from `${CMAKE_BINARY_DIR}/samples` to `${CMAKE_BINARY_DIR}/examples` ([#1139](https://github.com/kcenon/pacs_system/issues/1139))

### Documentation

- Modernize Doxygen with doxygen-awesome-css theme, dark mode toggle, and standardized mainpage ([#1066](https://github.com/kcenon/pacs_system/issues/1066))
- Confirm canonical namespace is `kcenon::pacs::` across all source, with no remaining `pacs_system::` references; record ecosystem alignment ([#1138](https://github.com/kcenon/pacs_system/issues/1138))
- Document Catch2 test framework retention as an explicit ecosystem exception in README, `docs/ECOSYSTEM.md`, and the master EPIC ([#1141](https://github.com/kcenon/pacs_system/issues/1141))

### Performance

- Implement memory-mapped I/O for DICOM file loading ([#989](https://github.com/kcenon/pacs_system/issues/989))
- Replace global singleton `pool_manager` with thread-local instances to eliminate cross-thread contention ([#992](https://github.com/kcenon/pacs_system/issues/992))

### Security

- Implement AES-256-GCM encryption for anonymizer `encrypt_value()` ([#987](https://github.com/kcenon/pacs_system/issues/987))
- Replace non-cryptographic `std::hash` with SHA-256 in anonymizer `hash_value()` ([#988](https://github.com/kcenon/pacs_system/issues/988))
- Set `non_downgrading=true` in BCP195 basic TLS profile to prevent version downgrade attacks ([#991](https://github.com/kcenon/pacs_system/issues/991))
- Handle patient age in basic anonymization profile per HIPAA Safe Harbor requirements ([#993](https://github.com/kcenon/pacs_system/issues/993))

### Changed

- Replace POSIX iconv with ICU (ucnv_convert) for DICOM character set encoding/decoding ([#1012](https://github.com/kcenon/pacs_system/issues/1012))

### Fixed

- Replace thread-unsafe `std::localtime` with `localtime_r`/`localtime_s` ([#990](https://github.com/kcenon/pacs_system/issues/990))

## [0.1.0] - 2026-03-13

### Added
- DICOM protocol implementation (C-ECHO, C-STORE, C-FIND, C-MOVE, C-GET)
- Transfer syntax support (Implicit VR Little Endian, Explicit VR, JPEG, JPEG2000, HTJ2K, JPEG-LS, RLE)
- PACS storage with SQLite backend
- DICOMweb REST API via Crow HTTP framework
- Image codec support (JPEG, JPEG2000, HTJ2K, JPEG-LS, PNG)
- TLS/SSL support for secure DICOM associations
- AWS S3 and Azure Blob storage integration
- AI inference pipeline integration
- CLI tools (pacs_echo, pacs_store, pacs_find, pacs_move, pacs_get, pacs_server)
- Network layer migration to public tcp_facade API (#934)
- vcpkg overlay port with feature-based dependency declaration (#921)
- CMake install/export infrastructure (#920)
- Release workflow for version tagging (#925)

### Infrastructure
- GitHub Actions CI/CD with sanitizer testing
- Integration test workflow
- SBOM generation workflow
- Doxygen documentation with GitHub Pages deployment
- vcpkg manifest with feature-based codecs, storage, and cloud features
- Cross-platform support (Linux, macOS, Windows)
