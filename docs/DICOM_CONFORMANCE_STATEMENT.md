# DICOM Conformance Statement

## PACS System

**Implementation Name**: PACS_SYSTEM_001
**Implementation Class UID**: 1.2.826.0.1.3680043.2.1545.1
**Version**: 0.1.0
**Date**: 2026-02-17

This document describes the DICOM conformance of the PACS System application.
It specifies the Service Classes, SOP Classes, Transfer Syntaxes, and
communication profiles supported by this implementation.

This Conformance Statement is structured according to **DICOM PS3.2** (Conformance).

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Networking](#2-networking)
3. [Supported SOP Classes](#3-supported-sop-classes)
4. [Supported Transfer Syntaxes](#4-supported-transfer-syntaxes)
5. [Media Interchange](#5-media-interchange)
6. [Support of Character Sets](#6-support-of-character-sets)
7. [Security](#7-security)
8. [DICOMweb Support](#8-dicomweb-support)
9. [Configuration](#9-configuration)
10. [References](#10-references)

---

## 1. Introduction

### 1.1 Application Description

The PACS System is a modern C++20 Picture Archiving and Communication System
implementation providing DICOM network services for medical image storage,
retrieval, query, and workflow management. It is implemented from scratch
without external DICOM libraries (no DCMTK, GDCM) and built on the kcenon
ecosystem (common_system, container_system, thread_system, logger_system,
monitoring_system, network_system).

### 1.2 Revision History

| Date | Version | Description |
|------|---------|-------------|
| 2026-02-17 | 1.0.0 | Initial Conformance Statement |

### 1.3 Terminology

| Term | Definition |
|------|-----------|
| SCP | Service Class Provider (server role) |
| SCU | Service Class User (client role) |
| AE | Application Entity |
| PDU | Protocol Data Unit |
| DIMSE | DICOM Message Service Element |
| IOD | Information Object Definition |

---

## 2. Networking

### 2.1 Implementation Model

#### 2.1.1 Application Data Flow Diagram

```
+------------------+       DICOM         +------------------+
|                  |   Association(s)    |                  |
|    Modality      | =================> |    PACS System   |
|    (SCU)         |   C-STORE, MPPS    |    (SCP)         |
|                  | <================= |                  |
+------------------+   C-FIND results   +------------------+
                                              |     ^
                                              |     |
                                         WADO-RS  STOW-RS
                                         QIDO-RS  HTTP
                                              |     |
                                              v     |
                                        +------------------+
                                        |   DICOMweb       |
                                        |   Client         |
                                        +------------------+
```

#### 2.1.2 Functional Definitions of AEs

| AE | Function |
|----|----------|
| PACS_SCP | Storage SCP, Query/Retrieve SCP, Worklist SCP, MPPS SCP, Verification SCP |
| PACS_SCU | Storage SCU, Query/Retrieve SCU, Worklist SCU, MPPS SCU, Verification SCU |

### 2.2 AE Specifications

#### 2.2.1 PACS_SCP Application Entity

**Description**: Accepts incoming associations for storage, query/retrieve,
worklist, MPPS, and verification services.

##### Association Policies

| Parameter | Value |
|-----------|-------|
| Maximum simultaneous associations | 20 (configurable) |
| Maximum PDU size (receive) | 16384 bytes (configurable) |
| Association idle timeout | 300 seconds (configurable) |
| Association negotiation timeout | 30 seconds (configurable) |
| AE Title whitelist | Optional (empty = accept all) |
| Unknown calling AE | Rejected by default (configurable) |

##### Connection Policies

| Parameter | Value |
|-----------|-------|
| Default port | 11112 |
| Protocol version | 0x0001 |
| Application Context | 1.2.840.10008.3.1.1.1 (DICOM Application Context Name) |
| SCP/SCU Role Selection | Supported |
| User Identity Negotiation | Supported (item types 0x58/0x59) |
| Asynchronous Operations | Not supported (single operation per association) |

#### 2.2.2 PACS_SCU Application Entity

**Description**: Initiates outgoing associations for storage, query/retrieve,
worklist, and MPPS operations.

##### Association Policies

| Parameter | Value |
|-----------|-------|
| Maximum PDU size (proposed) | 16384 bytes (configurable) |
| Number of retries | Configurable per operation |

### 2.3 Upper Layer Protocol

The implementation supports the full DICOM Upper Layer Protocol as defined in PS3.8:

| PDU Type | Code | Support |
|----------|------|---------|
| A-ASSOCIATE-RQ | 0x01 | Full |
| A-ASSOCIATE-AC | 0x02 | Full |
| A-ASSOCIATE-RJ | 0x03 | Full |
| P-DATA-TF | 0x04 | Full |
| A-RELEASE-RQ | 0x05 | Full |
| A-RELEASE-RP | 0x06 | Full |
| A-ABORT | 0x07 | Full |

The association state machine implements all 13 states (Sta1-Sta13) per PS3.8 Section 9.2.

### 2.4 DIMSE Commands

All 12 DIMSE commands are implemented:

#### DIMSE-C (Composite)

| Command | Code | Support |
|---------|------|---------|
| C-STORE | 0x0001 | SCP + SCU |
| C-GET | 0x0010 | SCP + SCU |
| C-FIND | 0x0020 | SCP + SCU |
| C-MOVE | 0x0021 | SCP + SCU |
| C-ECHO | 0x0030 | SCP + SCU |
| C-CANCEL | 0x0FFF | SCP + SCU |

#### DIMSE-N (Normalized)

| Command | Code | Support |
|---------|------|---------|
| N-EVENT-REPORT | 0x0100 | SCP + SCU |
| N-GET | 0x0110 | SCP + SCU |
| N-SET | 0x0120 | SCP + SCU |
| N-ACTION | 0x0130 | SCP + SCU |
| N-CREATE | 0x0140 | SCP + SCU |
| N-DELETE | 0x0150 | SCP + SCU |

Response command pattern: `response = request | 0x8000`

---

## 3. Supported SOP Classes

### 3.1 Verification

| SOP Class | UID | SCP | SCU |
|-----------|-----|-----|-----|
| Verification | 1.2.840.10008.1.1 | Yes | Yes |

### 3.2 Storage

#### CT

| SOP Class | UID | SCP | SCU |
|-----------|-----|-----|-----|
| CT Image Storage | 1.2.840.10008.5.1.4.1.1.2 | Yes | Yes |
| Enhanced CT Image Storage | 1.2.840.10008.5.1.4.1.1.2.1 | Yes | Yes |

#### MR

| SOP Class | UID | SCP | SCU |
|-----------|-----|-----|-----|
| MR Image Storage | 1.2.840.10008.5.1.4.1.1.4 | Yes | Yes |
| Enhanced MR Image Storage | 1.2.840.10008.5.1.4.1.1.4.1 | Yes | Yes |

#### US

| SOP Class | UID | SCP | SCU |
|-----------|-----|-----|-----|
| US Image Storage | 1.2.840.10008.5.1.4.1.1.6.1 | Yes | Yes |
| US Multi-frame Image Storage | 1.2.840.10008.5.1.4.1.1.6.2 | Yes | Yes |
| US Image Storage (Retired) | 1.2.840.10008.5.1.4.1.1.6 | Yes | Yes |
| US Multi-frame Image Storage (Retired) | 1.2.840.10008.5.1.4.1.1.3.1 | Yes | Yes |

#### XA / XRF

| SOP Class | UID | SCP | SCU |
|-----------|-----|-----|-----|
| XA Image Storage | 1.2.840.10008.5.1.4.1.1.12.1 | Yes | Yes |
| Enhanced XA Image Storage | 1.2.840.10008.5.1.4.1.1.12.1.1 | Yes | Yes |
| XRF Image Storage | 1.2.840.10008.5.1.4.1.1.12.2 | Yes | Yes |
| X-Ray 3D Angiographic Image Storage | 1.2.840.10008.5.1.4.1.1.13.1.1 | Yes | Yes |
| X-Ray 3D Craniofacial Image Storage | 1.2.840.10008.5.1.4.1.1.13.1.2 | Yes | Yes |

#### DX / MG

| SOP Class | UID | SCP | SCU |
|-----------|-----|-----|-----|
| Digital X-Ray Image Storage - For Presentation | 1.2.840.10008.5.1.4.1.1.1.1 | Yes | Yes |
| Digital X-Ray Image Storage - For Processing | 1.2.840.10008.5.1.4.1.1.1.1.1 | Yes | Yes |
| Digital Mammography X-Ray Image Storage - For Presentation | 1.2.840.10008.5.1.4.1.1.1.2 | Yes | Yes |
| Digital Mammography X-Ray Image Storage - For Processing | 1.2.840.10008.5.1.4.1.1.1.2.1 | Yes | Yes |
| Digital Intra-Oral X-Ray Image Storage - For Presentation | 1.2.840.10008.5.1.4.1.1.1.3 | Yes | Yes |
| Digital Intra-Oral X-Ray Image Storage - For Processing | 1.2.840.10008.5.1.4.1.1.1.3.1 | Yes | Yes |

#### CR / SC

| SOP Class | UID | SCP | SCU |
|-----------|-----|-----|-----|
| CR Image Storage | 1.2.840.10008.5.1.4.1.1.1 | Yes | Yes |
| Secondary Capture Image Storage | 1.2.840.10008.5.1.4.1.1.7 | Yes | Yes |

#### NM

| SOP Class | UID | SCP | SCU |
|-----------|-----|-----|-----|
| NM Image Storage | 1.2.840.10008.5.1.4.1.1.20 | Yes | Yes |
| NM Image Storage (Retired) | 1.2.840.10008.5.1.4.1.1.5 | Yes | Yes |

#### PET

| SOP Class | UID | SCP | SCU |
|-----------|-----|-----|-----|
| PET Image Storage | 1.2.840.10008.5.1.4.1.1.128 | Yes | Yes |
| Enhanced PET Image Storage | 1.2.840.10008.5.1.4.1.1.130 | Yes | Yes |
| Legacy Converted Enhanced PET Image Storage | 1.2.840.10008.5.1.4.1.1.128.1 | Yes | Yes |

#### RT

| SOP Class | UID | SCP | SCU |
|-----------|-----|-----|-----|
| RT Plan Storage | 1.2.840.10008.5.1.4.1.1.481.5 | Yes | Yes |
| RT Dose Storage | 1.2.840.10008.5.1.4.1.1.481.2 | Yes | Yes |
| RT Structure Set Storage | 1.2.840.10008.5.1.4.1.1.481.3 | Yes | Yes |
| RT Image Storage | 1.2.840.10008.5.1.4.1.1.481.1 | Yes | Yes |
| RT Beams Treatment Record Storage | 1.2.840.10008.5.1.4.1.1.481.4 | Yes | Yes |
| RT Brachy Treatment Record Storage | 1.2.840.10008.5.1.4.1.1.481.6 | Yes | Yes |
| RT Treatment Summary Record Storage | 1.2.840.10008.5.1.4.1.1.481.7 | Yes | Yes |
| RT Ion Plan Storage | 1.2.840.10008.5.1.4.1.1.481.8 | Yes | Yes |
| RT Ion Beams Treatment Record Storage | 1.2.840.10008.5.1.4.1.1.481.9 | Yes | Yes |

#### SEG

| SOP Class | UID | SCP | SCU |
|-----------|-----|-----|-----|
| Segmentation Storage | 1.2.840.10008.5.1.4.1.1.66.4 | Yes | Yes |
| Surface Segmentation Storage | 1.2.840.10008.5.1.4.1.1.66.5 | Yes | Yes |

#### SR

| SOP Class | UID | SCP | SCU |
|-----------|-----|-----|-----|
| Basic Text SR Storage | 1.2.840.10008.5.1.4.1.1.88.11 | Yes | Yes |
| Enhanced SR Storage | 1.2.840.10008.5.1.4.1.1.88.22 | Yes | Yes |
| Comprehensive SR Storage | 1.2.840.10008.5.1.4.1.1.88.33 | Yes | Yes |
| Comprehensive 3D SR Storage | 1.2.840.10008.5.1.4.1.1.88.34 | Yes | Yes |
| Extensible SR Storage | 1.2.840.10008.5.1.4.1.1.88.35 | Yes | Yes |
| Key Object Selection Document Storage | 1.2.840.10008.5.1.4.1.1.88.59 | Yes | Yes |
| Mammography CAD SR Storage | 1.2.840.10008.5.1.4.1.1.88.50 | Yes | Yes |
| Chest CAD SR Storage | 1.2.840.10008.5.1.4.1.1.88.65 | Yes | Yes |
| Colon CAD SR Storage | 1.2.840.10008.5.1.4.1.1.88.69 | Yes | Yes |
| X-Ray Radiation Dose SR Storage | 1.2.840.10008.5.1.4.1.1.88.67 | Yes | Yes |

### 3.3 Query/Retrieve

| SOP Class | UID | SCP | SCU |
|-----------|-----|-----|-----|
| Patient Root Q/R Information Model - FIND | 1.2.840.10008.5.1.4.1.2.1.1 | Yes | Yes |
| Patient Root Q/R Information Model - MOVE | 1.2.840.10008.5.1.4.1.2.1.2 | Yes | Yes |
| Patient Root Q/R Information Model - GET | 1.2.840.10008.5.1.4.1.2.1.3 | Yes | Yes |
| Study Root Q/R Information Model - FIND | 1.2.840.10008.5.1.4.1.2.2.1 | Yes | Yes |
| Study Root Q/R Information Model - MOVE | 1.2.840.10008.5.1.4.1.2.2.2 | Yes | Yes |
| Study Root Q/R Information Model - GET | 1.2.840.10008.5.1.4.1.2.2.3 | Yes | Yes |
| Patient/Study Only Q/R Information Model - FIND (Retired) | 1.2.840.10008.5.1.4.1.2.3.1 | Yes | Yes |

Query levels supported: PATIENT, STUDY, SERIES, IMAGE.

### 3.4 Modality Worklist

| SOP Class | UID | SCP | SCU |
|-----------|-----|-----|-----|
| Modality Worklist Information Model - FIND | 1.2.840.10008.5.1.4.31 | Yes | Yes |

### 3.5 Modality Performed Procedure Step (MPPS)

| SOP Class | UID | SCP | SCU |
|-----------|-----|-----|-----|
| Modality Performed Procedure Step | 1.2.840.10008.3.1.2.3.3 | Yes | Yes |

MPPS status transitions: IN PROGRESS -> COMPLETED | DISCONTINUED

---

## 4. Supported Transfer Syntaxes

### 4.1 Uncompressed

| Transfer Syntax | UID | Support |
|----------------|-----|---------|
| Implicit VR Little Endian | 1.2.840.10008.1.2 | Full |
| Explicit VR Little Endian | 1.2.840.10008.1.2.1 | Full |
| Explicit VR Big Endian (Retired) | 1.2.840.10008.1.2.2 | Full |
| Deflated Explicit VR Little Endian | 1.2.840.10008.1.2.1.99 | Full |

### 4.2 Compressed (Encapsulated Pixel Data)

| Transfer Syntax | UID | Type | Support |
|----------------|-----|------|---------|
| JPEG Baseline (Process 1) | 1.2.840.10008.1.2.4.50 | Lossy | Full |
| JPEG Lossless, Non-Hierarchical, First-Order Prediction | 1.2.840.10008.1.2.4.70 | Lossless | Full |
| JPEG 2000 Image Compression (Lossless Only) | 1.2.840.10008.1.2.4.90 | Lossless | Full |
| JPEG 2000 Image Compression | 1.2.840.10008.1.2.4.91 | Lossy | Full |
| RLE Lossless | 1.2.840.10008.1.2.5 | Lossless | Full |

### 4.3 Default Transfer Syntax

The default Transfer Syntax for association negotiation is **Implicit VR Little Endian**
(1.2.840.10008.1.2) as required by DICOM PS3.5.

---

## 5. Media Interchange

### 5.1 DICOM File Format

The implementation supports the DICOM File Format as defined in PS3.10:

- 128-byte preamble
- "DICM" prefix
- File Meta Information (Group 0002)
- Dataset encoding per negotiated Transfer Syntax

### 5.2 DICOMDIR

DICOMDIR support is provided via the `dcm_dir` utility:

| Operation | Support |
|-----------|---------|
| Create DICOMDIR | Yes |
| List DICOMDIR contents | Yes |
| Verify DICOMDIR integrity | Yes |
| Update DICOMDIR | Yes |

Directory Record types: PATIENT, STUDY, SERIES, IMAGE.

Group 0004 directory record tags are fully supported.

---

## 6. Support of Character Sets

### 6.1 Supported Character Sets

| Character Set | Defined Term | Support |
|--------------|--------------|---------|
| Default (ASCII) | ISO_IR 6 | Full |
| Latin-1 | ISO_IR 100 | Full |
| Unicode UTF-8 | ISO_IR 192 | Full |

### 6.2 Limitations

The following character sets are **not currently supported**:

| Character Set | Defined Term | Status |
|--------------|--------------|--------|
| Japanese (JIS X 0208) | ISO_IR 87 | Planned (Issue #700) |
| Korean (KS X 1001) | ISO_IR 149 | Planned (Issue #700) |
| Simplified Chinese (GB 2312) | ISO_IR 58 | Planned (Issue #700) |
| ISO 2022 escape sequences | N/A | Planned (Issue #700) |

See [Issue #700](https://github.com/kcenon/pacs_system/issues/700) for CJK
character set extension plans.

---

## 7. Security

### 7.1 TLS Transport Connection Profile

The implementation supports TLS for secure DICOM communication.

### 7.2 Digital Signatures (PS3.15 Section C)

| Feature | Support |
|---------|---------|
| Sign dataset | Yes |
| Sign specific tags | Yes |
| Verify signatures | Yes |
| Verify with trust chain | Yes |
| Get signature info | Yes |
| Remove signatures | Yes |
| Default algorithm | RSA-SHA256 |
| Digital Signature Sequence | (0400,0561) |

### 7.3 De-identification Profiles (PS3.15 Annex E)

| Profile | Description | Support |
|---------|-------------|---------|
| Basic | Remove direct identifiers | Yes |
| Clean Pixel Data | Remove burned-in annotations | Yes |
| Clean Descriptions | Sanitize text fields | Yes |
| Retain Longitudinal | Preserve temporal relationships via date shifting | Yes |
| Retain Patient Characteristics | Preserve demographics for research | Yes |
| HIPAA Safe Harbor | Remove 18 identifier categories (45 CFR 164.514) | Yes |
| GDPR Compliant | European data protection pseudonymization | Yes |

### 7.4 Access Control

- AE Title-based whitelist filtering
- Role-Based Access Control (RBAC)
- User Identity Negotiation (item types 0x58/0x59)

---

## 8. DICOMweb Support

The implementation supports DICOMweb services as defined in PS3.18:

### 8.1 WADO-RS (Web Access to DICOM Objects - RESTful)

| Feature | Support |
|---------|---------|
| Retrieve Study | Yes |
| Retrieve Series | Yes |
| Retrieve Instance | Yes |
| Retrieve Frames | Yes (frame number parsing: single, range, comma-separated) |
| Retrieve Metadata (DicomJSON) | Yes |
| Retrieve Rendered (JPEG/PNG) | Yes |
| Window/Level transformation | Yes |
| Multipart/related responses | Yes |
| Bulk Data URI references | Yes |

### 8.2 STOW-RS (Store Over the Web)

| Feature | Support |
|---------|---------|
| Store instances | Yes |
| Multipart/related request parsing | Yes |
| Instance validation | Yes |
| Store response (DicomJSON) | Yes |
| Partial success reporting | Yes |

### 8.3 QIDO-RS (Query based on ID for DICOM Objects)

| Feature | Support |
|---------|---------|
| Search for Studies | Yes |
| Search for Series | Yes |
| Search for Instances | Yes |
| DicomJSON response format | Yes |
| Query parameter parsing | Yes |

### 8.4 Supported Media Types

| Media Type | Usage |
|-----------|-------|
| application/dicom | Native DICOM format |
| application/dicom+json | DicomJSON metadata |
| application/dicom+xml | DICOM XML metadata |
| application/octet-stream | Raw binary data |
| image/jpeg | Rendered images |
| image/png | Rendered images |
| multipart/related | Multi-part responses/requests |

---

## 9. Configuration

### 9.1 Default Configuration

| Parameter | Default Value | Description |
|-----------|---------------|-------------|
| AE Title | PACS_SCP | Application Entity Title (16 chars max) |
| Port | 11112 | DICOM network port |
| Max PDU Size | 16384 bytes | Maximum Protocol Data Unit size |
| Max Associations | 20 | Maximum concurrent associations |
| Idle Timeout | 300 seconds | Association idle timeout |
| Association Timeout | 30 seconds | Negotiation timeout |
| Implementation Class UID | 1.2.826.0.1.3680043.2.1545.1 | Unique implementation identifier |
| Implementation Version Name | PACS_SYSTEM_001 | Implementation version string |

### 9.2 Configurable Parameters

All parameters in `server_config` are configurable at runtime:

- AE Title and port
- Maximum concurrent associations
- Maximum PDU size
- Timeout values (idle, association)
- AE Title whitelist
- Unknown calling AE acceptance policy

---

## 10. References

| Standard | Title |
|----------|-------|
| DICOM PS3.2 | Conformance |
| DICOM PS3.3 | Information Object Definitions |
| DICOM PS3.4 | Service Class Specifications |
| DICOM PS3.5 | Data Structures and Encoding |
| DICOM PS3.6 | Data Dictionary |
| DICOM PS3.7 | Message Exchange |
| DICOM PS3.8 | Network Communication Support for Message Exchange |
| DICOM PS3.10 | Media Storage and File Format for Media Interchange |
| DICOM PS3.15 | Security and System Management Profiles |
| DICOM PS3.18 | Web Services |

---

## Appendix A: IOD Validation

The implementation includes modality-specific IOD validators for the following modalities:

- CT, MR, US, XA/XRF, DX, MG, NM, PET, RT, SEG, SR, SC

Each validator checks Mandatory (M), Conditional (C), and User-optional (U)
attributes as defined in the corresponding IOD specifications in PS3.3.

## Appendix B: Known Limitations

| Area | Limitation | Reference |
|------|-----------|-----------|
| Character Sets | CJK character sets (ISO_IR 87/149/58) not supported | Issue #700 |
| Storage Commitment | Service-level business logic not yet implemented | Issue #701 |
| Asynchronous Operations | Not supported (single operation per association) | N/A |
| Print Management | Not supported | N/A |

---

*DICOM is the registered trademark of the National Electrical Manufacturers Association (NEMA).*
