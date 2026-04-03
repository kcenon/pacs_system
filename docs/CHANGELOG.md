---
doc_id: "PAC-PROJ-002"
doc_title: "Changelog - PACS System"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "pacs_system"
category: "PROJ"
---

# Changelog - PACS System

> **SSOT**: This document is the single source of truth for **Changelog - PACS System**.

> **Language:** **English** | [한국어](CHANGELOG.kr.md)

All notable changes to the PACS System project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Changed
- Documentation standardization (Doxyfile, README, ecosystem docs)

---

## [0.2.0] - 2026-02-09

### Added
- **IHE Integration Profiles**: XDS-I.b, AIRA, PIR actor support
- **DICOMweb API**: WADO-RS, STOW-RS, QIDO-RS endpoints
- **Monitoring and Observability**: Metrics collectors, health checks
- **Workflow Services**: Prefetch queue, intelligent caching, routing rules
- **Security Enhancements**: TLS for DICOM associations, input validation
- **Connection Pooling**: Remote node manager with pool-based connections
- **Error Handling**: Comprehensive Result<T> pattern integration

### Changed
- Phase 4 features marked complete (Security, Monitoring, Workflow, Ecosystem Integration)

---

## [0.1.0] - 2024-12-01

### Added
- Initial release of PACS System
- **DICOM Core**: Complete Data Element parser/encoder for all 34 Value Representations
- **DICOM Network**: SCP/SCU implementation (C-STORE, C-FIND, C-MOVE, C-GET, C-ECHO)
- **Transfer Syntax**: Implicit/Explicit VR, Little/Big Endian support
- **Multi-Codec Support**: JPEG, JPEG 2000, JPEG-LS, RLE, Uncompressed
- **Storage Backends**: Filesystem and cloud storage (S3/Azure Blob)
- **Database Integration**: DICOM metadata indexing
- **CLI Tools**: Command-line interface for DICOM operations
- **Ecosystem Integration**: Built on network_system, container_system, common_system
- **DICOM Conformance Statement**: Full PS3.x compliance documentation
