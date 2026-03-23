# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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
