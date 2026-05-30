---
doc_id: "PAC-GUID-032"
doc_title: "PACS System Integration Guide"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "pacs_system"
category: "GUID"
---

# PACS System Integration Guide

> **SSOT**: This document is the single source of truth for **PACS System Integration Guide**.

This directory contains integration documentation for using PACS System with the kcenon ecosystem and external systems.

## Ecosystem Dependencies

PACS System is built on the kcenon ecosystem with the following dependency chain:

| System | Tier | Role |
|--------|------|------|
| common_system | Tier 0 | Result<T> pattern, interfaces |
| container_system | Tier 1 | DICOM data serialization |
| thread_system | Tier 1 | Async operations, thread pooling (via network_system) |
| logger_system | Tier 2 | Structured logging (via network_system) |
| monitoring_system | Tier 3 | Metrics and observability (optional) |
| network_system | Tier 4 | DICOM PDU/Association, TCP/TLS |
| **pacs_system** | **Tier 5** | **DICOM PACS implementation** |

## IHE Integration Profiles

PACS System supports the following IHE integration profiles:

- **XDS-I.b** (Cross-Enterprise Document Sharing for Imaging) - Imaging Document Source, Consumer
- **AIRA** (AI Result Assessment) - Assessment Creator
- **PIR** (Patient Information Reconciliation) - Image Manager/Archive

See [IHE_INTEGRATION_STATEMENT.md](../IHE_INTEGRATION_STATEMENT.md) for full details.

## DICOM Conformance

For DICOM-level interoperability details, see:

- [DICOM_CONFORMANCE_STATEMENT.md](../DICOM_CONFORMANCE_STATEMENT.md) - Supported SOP Classes, Transfer Syntaxes
- [FEATURES.md](../FEATURES.md) - Complete feature listing including DICOMweb API

## Related Documentation

- [ARCHITECTURE.md](../ARCHITECTURE.md) - Integration architecture section
- [SOUP.md](../SOUP.md) - IEC 62304 dependency register
- [SDS_NETWORK_V2.md](../SDS_NETWORK_V2.md) - Network integration design
