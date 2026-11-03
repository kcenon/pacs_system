---
doc_id: "PAC-INTR-001"
doc_title: "DICOM Conformance Statement"
doc_version: "0.1.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "pacs_system"
category: "INTR"
---

# DICOM Conformance Statement

> **SSOT**: This document is the single source of truth for **DICOM Conformance Statement**.

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
| 2026-11-03 | 1.11.0 | Declare DICOMweb as measured against independent clients and a reference server: media types, Transfer Syntaxes and frames (Section 8.1), statuses and what both protocols refuse (Section 8.2), matching and the maximum per response (Section 8.3), and that with no OAuth 2.0 there is no authorization, rather than a fallback (Section 8.5). |
| 2026-11-02 | 1.10.0 | Declare the normalized services as measured against independent peers: Worklist matching and return keys (Section 3.4), MPPS statuses (Section 3.5), commitment reports on a new association (Section 3.6), the Print contract and its spool (Section 3.7), and the UPS SOP Classes, which `pacs_server` now provides (Section 3.8). `pacs_server` also provides Storage Commitment and Print, which this statement already claimed for it. Implicit VR requests of these services are now readable: their attributes were missing from the data dictionary. |
| 2026-11-01 | 1.9.0 | Declare the Query/Retrieve contract: keys, matching, responses, retrieve rules and association limits (Section 3.3). The retired Patient/Study Only model is SCP only. |
| 2026-11-01 | 1.8.0 | Correct SOP Class UIDs and names against PS3.6: Legacy Converted Enhanced CT (`.2.2`), US Multi-frame (`.3.1`, retired `.3`), Label Map (`.66.7`) and Height Map (`.66.8`) Segmentation, Waveform Presentation State (`.9.100.1`), Waveform Annotation SR (`.88.77`), Wide Field Ophthalmic Photography (`.77.1.5.5`). Section 3.2 now lists every registered class, and `store_scp` and `pacs_server` accept exactly that list by default. |
| 2026-10-24 | 1.7.0 | Section 4 states PACS_SCP negotiation and codec support per transfer syntax: Explicit VR Big Endian, Deflated Explicit VR Little Endian, HEVC, JPEG XL, and Frame Deflate are not negotiated, and the last four have no working codec; Appendix B lists these and the JPEG Lossless decoder limitation |
| 2026-10-23 | 1.6.0 | Add JPEG-LS Lossless and Near-Lossless transfer syntaxes; list all optional codec libraries in Appendix B |
| 2026-03-05 | 1.5.0 | Add HEVC, JPEG XL, Frame Deflate transfer syntaxes; Heightmap/Label Map Segmentation, Waveform PS/Annotation SOP classes; TLS 1.3 BCP 195 profiles; OAuth 2.0 and DICOMweb Storage Commitment |
| 2026-03-04 | 1.4.0 | Add CT For Processing Image Storage SOP Class (multi-energy/spectral CT) |
| 2026-03-04 | 1.3.0 | Add HTJ2K Transfer Syntax support (Supplement 235) |
| 2026-02-20 | 1.2.0 | Add Print Management SCP/SCU (PS3.4 Annex H), update Cloud Storage to real SDK |
| 2026-02-18 | 1.1.0 | Add Storage Commitment Push Model SCP/SCU, update known limitations |
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
| PACS_SCP | Storage SCP, Query/Retrieve SCP, Worklist SCP, MPPS SCP, Storage Commitment SCP, Print SCP, UPS SCP, Verification SCP |
| PACS_SCU | Storage SCU, Query/Retrieve SCU, Worklist SCU, MPPS SCU, Storage Commitment SCU, Print SCU, UPS SCU, Verification SCU |

### 2.2 AE Specifications

#### 2.2.1 PACS_SCP Application Entity

**Description**: Accepts incoming associations for storage, query/retrieve,
worklist, MPPS, storage commitment, print management, Unified Procedure Step, and
verification services. It opens associations of its own to deliver C-MOVE objects and
N-EVENT-REPORT messages, to the peers named with `--peer AE:host:port`.

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
worklist, MPPS, storage commitment, print management, and Unified Procedure Step operations.

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
| Legacy Converted Enhanced CT Image Storage | 1.2.840.10008.5.1.4.1.1.2.2 | Yes | Yes |

#### MR

| SOP Class | UID | SCP | SCU |
|-----------|-----|-----|-----|
| MR Image Storage | 1.2.840.10008.5.1.4.1.1.4 | Yes | Yes |
| Enhanced MR Image Storage | 1.2.840.10008.5.1.4.1.1.4.1 | Yes | Yes |

#### US

| SOP Class | UID | SCP | SCU |
|-----------|-----|-----|-----|
| US Image Storage | 1.2.840.10008.5.1.4.1.1.6.1 | Yes | Yes |
| US Multi-frame Image Storage | 1.2.840.10008.5.1.4.1.1.3.1 | Yes | Yes |
| US Image Storage (Retired) | 1.2.840.10008.5.1.4.1.1.6 | Yes | Yes |
| US Multi-frame Image Storage (Retired) | 1.2.840.10008.5.1.4.1.1.3 | Yes | Yes |

#### XA / XRF

| SOP Class | UID | SCP | SCU |
|-----------|-----|-----|-----|
| X-Ray Angiographic Image Storage | 1.2.840.10008.5.1.4.1.1.12.1 | Yes | Yes |
| Enhanced XA Image Storage | 1.2.840.10008.5.1.4.1.1.12.1.1 | Yes | Yes |
| X-Ray Radiofluoroscopic Image Storage | 1.2.840.10008.5.1.4.1.1.12.2 | Yes | Yes |
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
| Multi-frame Single Bit Secondary Capture Image Storage | 1.2.840.10008.5.1.4.1.1.7.1 | Yes | Yes |
| Multi-frame Grayscale Byte Secondary Capture Image Storage | 1.2.840.10008.5.1.4.1.1.7.2 | Yes | Yes |
| Multi-frame Grayscale Word Secondary Capture Image Storage | 1.2.840.10008.5.1.4.1.1.7.3 | Yes | Yes |
| Multi-frame True Color Secondary Capture Image Storage | 1.2.840.10008.5.1.4.1.1.7.4 | Yes | Yes |

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
| Label Map Segmentation Storage | 1.2.840.10008.5.1.4.1.1.66.7 | Yes | Yes |
| Height Map Segmentation Storage | 1.2.840.10008.5.1.4.1.1.66.8 | Yes | Yes |

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

#### Waveform

| SOP Class | UID | SCP | SCU |
|-----------|-----|-----|-----|
| 12-lead ECG Waveform Storage | 1.2.840.10008.5.1.4.1.1.9.1.1 | Yes | Yes |
| General ECG Waveform Storage | 1.2.840.10008.5.1.4.1.1.9.1.2 | Yes | Yes |
| Ambulatory ECG Waveform Storage | 1.2.840.10008.5.1.4.1.1.9.1.3 | Yes | Yes |
| Hemodynamic Waveform Storage | 1.2.840.10008.5.1.4.1.1.9.2.1 | Yes | Yes |
| Basic Cardiac Electrophysiology Waveform Storage | 1.2.840.10008.5.1.4.1.1.9.3.1 | Yes | Yes |
| Basic Voice Audio Waveform Storage | 1.2.840.10008.5.1.4.1.1.9.4.1 | Yes | Yes |
| General Audio Waveform Storage | 1.2.840.10008.5.1.4.1.1.9.4.2 | Yes | Yes |
| Arterial Pulse Waveform Storage | 1.2.840.10008.5.1.4.1.1.9.5.1 | Yes | Yes |
| Respiratory Waveform Storage | 1.2.840.10008.5.1.4.1.1.9.6.1 | Yes | Yes |
| Multi-channel Respiratory Waveform Storage | 1.2.840.10008.5.1.4.1.1.9.6.2 | Yes | Yes |
| Routine Scalp EEG Waveform Storage | 1.2.840.10008.5.1.4.1.1.9.7.1 | Yes | Yes |
| Electromyogram Waveform Storage | 1.2.840.10008.5.1.4.1.1.9.7.2 | Yes | Yes |
| Electrooculogram Waveform Storage | 1.2.840.10008.5.1.4.1.1.9.7.3 | Yes | Yes |
| Sleep EEG Waveform Storage | 1.2.840.10008.5.1.4.1.1.9.7.4 | Yes | Yes |
| Body Position Waveform Storage | 1.2.840.10008.5.1.4.1.1.9.8.1 | Yes | Yes |
| Waveform Presentation State Storage | 1.2.840.10008.5.1.4.1.1.9.100.1 | Yes | Yes |
| Waveform Annotation SR Storage | 1.2.840.10008.5.1.4.1.1.88.77 | Yes | Yes |

#### Ophthalmic

| SOP Class | UID | SCP | SCU |
|-----------|-----|-----|-----|
| Ophthalmic Photography 8 Bit Image Storage | 1.2.840.10008.5.1.4.1.1.77.1.5.1 | Yes | Yes |
| Ophthalmic Photography 16 Bit Image Storage | 1.2.840.10008.5.1.4.1.1.77.1.5.2 | Yes | Yes |
| Ophthalmic Tomography Image Storage | 1.2.840.10008.5.1.4.1.1.77.1.5.4 | Yes | Yes |
| Wide Field Ophthalmic Photography Stereographic Projection Image Storage | 1.2.840.10008.5.1.4.1.1.77.1.5.5 | Yes | Yes |
| Ophthalmic OCT B-scan Volume Analysis Storage | 1.2.840.10008.5.1.4.1.1.77.1.5.8 | Yes | Yes |

#### Whole Slide Imaging and Parametric Maps

| SOP Class | UID | SCP | SCU |
|-----------|-----|-----|-----|
| VL Whole Slide Microscopy Image Storage | 1.2.840.10008.5.1.4.1.1.77.1.6 | Yes | Yes |
| Parametric Map Storage | 1.2.840.10008.5.1.4.1.1.30 | Yes | Yes |

#### Encapsulated Documents

| SOP Class | UID | SCP | SCU |
|-----------|-----|-----|-----|
| Encapsulated PDF Storage | 1.2.840.10008.5.1.4.1.1.104.1 | Yes | Yes |
| Encapsulated CDA Storage | 1.2.840.10008.5.1.4.1.1.104.2 | Yes | Yes |

### 3.3 Query/Retrieve

| SOP Class | UID | SCP | SCU |
|-----------|-----|-----|-----|
| Patient Root Q/R Information Model - FIND | 1.2.840.10008.5.1.4.1.2.1.1 | Yes | Yes |
| Patient Root Q/R Information Model - MOVE | 1.2.840.10008.5.1.4.1.2.1.2 | Yes | Yes |
| Patient Root Q/R Information Model - GET | 1.2.840.10008.5.1.4.1.2.1.3 | Yes | Yes |
| Study Root Q/R Information Model - FIND | 1.2.840.10008.5.1.4.1.2.2.1 | Yes | Yes |
| Study Root Q/R Information Model - MOVE | 1.2.840.10008.5.1.4.1.2.2.2 | Yes | Yes |
| Study Root Q/R Information Model - GET | 1.2.840.10008.5.1.4.1.2.2.3 | Yes | Yes |
| Patient/Study Only Q/R Information Model - FIND (Retired) | 1.2.840.10008.5.1.4.1.2.3.1 | Yes | No |

Query levels: PATIENT, STUDY, SERIES and IMAGE under Patient Root; STUDY, SERIES and
IMAGE under Study Root; PATIENT and STUDY under Patient/Study Only. `qr_scp` and
`pacs_server` answer from one implementation, the Hierarchical Search Method of PS3.4
C.4.1.3.1. Relational Queries are not negotiated.

**Keys held for existence and matching**

| Level | Unique Key | Required Keys | Optional Keys |
|-------|------------|---------------|---------------|
| PATIENT | Patient ID | Patient's Name | Patient's Birth Date, Patient's Sex |
| STUDY | Study Instance UID | Study Date, Study Time, Accession Number, Study ID; under Study Root also Patient's Name, Patient ID | Referring Physician's Name, Study Description, Modalities in Study; under Study Root also Patient's Birth Date, Patient's Sex |
| SERIES | Series Instance UID | Modality, Series Number | Series Description |
| IMAGE | SOP Instance UID | Instance Number | SOP Class UID |

**Matching**

- UIDs use Single Value or List of UID Matching, with no wild cards.
- AE, CS, LO, LT, PN, SH, ST, UC, UR and UT keys use Single Value or Wild Card Matching.
  `?` is one character, whatever its encoded length. Only PN ignores case, and only
  for ASCII letters (the implementation choice C.2.2.2.1.1 and C.2.2.2.4 leave open).
- DA and TM keys use Single Value or Range Matching by meaning. A TM value with less
  precision covers what it names, so `10` means 10:00:00 through 10:59:59.
- A stored attribute with no value matches any key, as C.2.2.1.2 requires. This
  applies to Optional Keys too, which C.2.2.1.3 says behave like Required Keys.
- A key with several values, outside List of UID Matching, matches when any value does.
  C.2.2.2.8 leaves this undefined without Extended Negotiation.
- Text keys are decoded from the Identifier's Specific Character Set and compare with
  the UTF-8 the index holds, so a key in one character set finds an object stored in
  another. An index written before this behavior needs a rescan to hold UTF-8.

**C-FIND responses**

Each response holds the requested keys held by the index, Query/Retrieve Level and
Retrieve AE Title, and declares ISO_IR 192 when its text is not ASCII. A key the index
does not hold is ignored and not returned, and matches then carry Pending status FF01.

| Request | Result |
|---------|--------|
| No Identifier, or no valid level for the model | A900 |
| A Unique Key above the level is missing or holds several values | A900 |
| The index cannot answer | C001, never an empty success |
| No entity matches | 0000 with no matches |
| C-CANCEL naming the operation | FE00, and no further matches |

**C-MOVE and C-GET**

The Identifier needs Query/Retrieve Level and the Unique Keys of that level and those
above, or the answer is A900. The Unique Key of the level may be a list of UIDs, except
Patient ID. A Unique Key below the level is refused, since it names less than the level
would deliver. Required and Optional Keys do not belong in the Identifier and are ignored,
so they neither narrow nor widen what the Unique Keys select. An object the index holds
but whose file cannot be read counts as a failed sub-operation and is named in Failed SOP
Instance UID List. A C-MOVE destination must be configured with `--peer`, or the answer
is A801.

**Associations**

Past the association limit a peer receives A-ASSOCIATE-RJ (rejected-transient, local limit
exceeded). An association silent for the idle timeout receives A-ABORT, unless a request is
being served, as a requester waiting for a long retrieve is silent but not idle.

**Cancellation**

Once C-CANCEL takes effect, no new match is sent and no new C-STORE sub-operation starts. A
store already in flight may still land at the receiver; it counts as failed unless its
acknowledgement arrived first. The final FE00 reports the sub-operations left undone.

**Known limitation**

Streams of C-FIND responses are unreliable for a peer that reads more slowly than the SCP
writes, once the responses outgrow the socket buffers. The network layer starts a write while
an earlier one is still incomplete, which breaks PDU framing. The query then fails, and a
damaged response may arrive before the failure (SOUP-006 in [SOUP.md](SOUP.md)). Retrieves are
not affected in this way, because each object waits for its acknowledgement.

### 3.4 Modality Worklist

| SOP Class | UID | SCP | SCU |
|-----------|-----|-----|-----|
| Modality Worklist Information Model - FIND | 1.2.840.10008.5.1.4.31 | Yes | Yes |

An identifier without the Scheduled Procedure Step Sequence, a required key (PS3.4 K.6.1.2.2), is
refused with A900H. `pacs_server` answers from its worklist table with the matching of Section 3.3,
and returns the keys asked for: Patient ID, Name, Birth Date and Sex, Accession Number, Study Instance
UID, Referring Physician, Requested Procedure ID, and in the sequence Modality, Scheduled Station AE
Title and Name, Start Date and Time, Scheduled Procedure Step ID and Description. A key it does not hold
matches nothing and comes back empty when it has no value; the table has no Requested Procedure
Description. A failure status from an SCP ends `worklist_scu` with exit 2, not with an empty worklist.

### 3.5 Modality Performed Procedure Step (MPPS)

| SOP Class | UID | SCP | SCU |
|-----------|-----|-----|-----|
| Modality Performed Procedure Step | 1.2.840.10008.3.1.2.3.3 | Yes | Yes |

MPPS status transitions: IN PROGRESS -> COMPLETED | DISCONTINUED

| Request | Status (PS3.4 F.7.2) |
|---------|----------------------|
| N-CREATE of an existing instance | 0111H |
| N-SET of an instance that does not exist | 0112H |
| N-SET after COMPLETED or DISCONTINUED | 0110H, and nothing changes |

`pacs_server` keeps instances in its database, so they can be completed after a restart. It reads the
study from the Scheduled Step Attributes Sequence, where the standard puts it. `mpps_scu` sends every
Type 1 and Type 2 attribute of Table F.7.2-1, so a peer that validates the request accepts it.

### 3.6 Storage Commitment Push Model

| SOP Class | UID | SCP | SCU |
|-----------|-----|-----|-----|
| Storage Commitment Push Model | 1.2.840.10008.1.20.1 | Yes | Yes |

The Storage Commitment Push Model service is implemented per DICOM PS3.4 Annex J.

#### Protocol Flow

```
SCU (Requestor)                    SCP (Provider)
     |                                  |
     |  N-ACTION-RQ (Commitment Req)    |
     |  Transaction UID + SOP list      |
     | -------------------------------->|
     |                                  |
     |  N-ACTION-RSP (Accepted)         |
     |<-------------------------------- |
     |                                  |
     |         [SCP verifies storage]   |
     |                                  |
     |  N-EVENT-REPORT-RQ (Result)      |
     |  Event Type: 1=Success, 2=Fail   |
     |<-------------------------------- |
     |                                  |
     |  N-EVENT-REPORT-RSP              |
     | -------------------------------->|
     |                                  |
```

#### Supported Features

| Feature | Support |
|---------|---------|
| N-ACTION Request (commit request) | Yes |
| N-EVENT-REPORT (commit result) | Yes |
| Event Type ID 1 (Storage Commitment Request - Successful) | Yes |
| Event Type ID 2 (Storage Commitment Request - Failure) | Yes |
| Per-instance failure reason codes | Yes |
| Transaction UID tracking | Yes |
| Asynchronous commit verification | Yes |
| Well-known SOP Instance UID (1.2.840.10008.1.20.1.1) | Yes |

#### Delivering the Result

A requestor named with `--peer` gets its report on an association `pacs_server` opens: the same AE Title as
the request, proposing the SCP role through Role Selection, as PS3.4 J.3.3 requires, since the SCU may
release as soon as it has the N-ACTION response. Without an address the report goes on the requesting
association, which only reaches a requestor that holds it open. Reports to one AE keep their order.

A report no attempt could deliver is tried `--event-attempts` times (default 3), `--event-retry-delay`
seconds apart (default 5), then logged and dropped. A status from the peer, even a failure, ends the
attempts, because the peer has the report. Waiting reports are not kept across a restart; the requestor
asks again with a new Transaction UID, and what is stored is still committed.

An instance is committed when the index holds it under that SOP Class and its file exists. An instance
the index names under another SOP Class fails with 0119H, and one whose file is gone with 0112H.

| Request | Status |
|---------|--------|
| Action Type other than 1 | 0123H |
| Requested SOP Instance other than the well-known one | 0117H |
| No Transaction UID, no Referenced SOP Sequence, or an item without both UIDs | 0120H |

#### Failure Reason Codes

| Code | Meaning |
|------|---------|
| 0110H | Processing failure |
| 0112H | No such object instance |
| 0213H | Resource limitation |
| 0122H | Referenced SOP Class not supported |
| 0119H | Class/Instance conflict |

### 3.7 Print Management (PS3.4 Annex H)

#### SOP Classes

| SOP Class | UID | SCP | SCU |
|-----------|-----|-----|-----|
| Basic Film Session | 1.2.840.10008.5.1.1.1 | Yes | Yes |
| Basic Film Box | 1.2.840.10008.5.1.1.2 | Yes | Yes |
| Basic Grayscale Image Box | 1.2.840.10008.5.1.1.4 | Yes | Yes |
| Basic Color Image Box | 1.2.840.10008.5.1.1.4.1 | Yes | Yes |
| Printer | 1.2.840.10008.5.1.1.16 | Yes | Yes |
| Basic Grayscale Print Meta SOP Class | 1.2.840.10008.5.1.1.9 | Yes | Yes |
| Basic Color Print Meta SOP Class | 1.2.840.10008.5.1.1.18 | Yes | Yes |

#### Protocol Flow

```
Workstation (Print SCU)                      Printer (Print SCP)
 |                                           |
 |  N-CREATE Film Session                    |
 |------------------------------------------>|
 |  N-CREATE-RSP (session UID)               |
 |<------------------------------------------|
 |                                           |
 |  N-CREATE Film Box                        |
 |------------------------------------------>|
 |  N-CREATE-RSP (box UID + image box UIDs)  |
 |<------------------------------------------|
 |                                           |
 |  N-SET Image Box (pixel data)             |
 |------------------------------------------>|
 |  N-SET-RSP (success)                      |
 |<------------------------------------------|
 |                                           |
 |  N-ACTION Film Box (Print)                |
 |------------------------------------------>|
 |  N-ACTION-RSP (success)                   |
 |<------------------------------------------|
 |                                           |
 |  N-DELETE Film Session                    |
 |------------------------------------------>|
 |  N-DELETE-RSP (success)                   |
 |<------------------------------------------|
```

#### Supported Features

| Feature | Support |
|---------|---------|
| N-CREATE Film Session | Yes |
| N-CREATE Film Box (auto-creates Image Boxes) | Yes |
| N-SET Image Box (Grayscale and Color) | Yes |
| N-ACTION Film Box (initiate print) | Yes |
| N-DELETE Film Session/Box | Yes |
| N-GET Printer Status | Yes |
| Well-Known Printer SOP Instance UID (1.2.840.10008.5.1.1.17) | Yes |
| Meta SOP Class fallback negotiation | Yes |

A Film Session belongs to the association that created it (PS3.4 H.4.1): another association cannot
reach it, a second one on the same association is refused, and it ends with the association. A Film Box
joins the session it names. A Color Print Meta context prints Color Image Boxes.

Image Display Format is `STANDARD\C,R`, `ROW\R1,R2,...` or `COL\C1,C2,...`, up to 4096 Image Boxes.
SLIDE, SUPERSLIDE and CUSTOM are refused. The layout is fixed by the N-CREATE.

| Request | Status |
|---------|--------|
| Film Box with no Referenced Film Session Sequence or Image Display Format | 0120H |
| Film Box naming a session this association does not hold, or a format it cannot lay out | 0106H |
| Number of Copies that is not a positive number | 0106H |
| Second Film Session on an association | 0213H |
| Image Box with no position or no image sequence of its class | 0120H |
| Image Box with the position of another | 0106H |
| Film Session, Film Box or Image Box this association does not hold | 0112H |
| Printer instance other than the well-known one | 0112H |
| Action Type other than 1 | 0123H |
| Printing a Film Box with no image | B603H, and no job |
| Printing a Film Session with no Film Box | C600H |
| A job the spool cannot take | C602H |

`pacs_server` has no physical printer. Each printed Film Box becomes a directory under `--print-spool`
(default `<storage-dir>.print-spool`), holding `job.json` and the pixel bytes of each image. The manifest
appears last, so a job with a manifest has all its images. Print Job, Presentation LUT and Annotation Box
are not provided. `print_scu` sends one image with `--image`, and through Color Print Meta with `--color`;
without an image the printer answers B603H and the tool ends with exit 1.

### 3.8 Unified Procedure Step (PS3.4 Annex CC)

| SOP Class | UID | SCP | SCU |
|-----------|-----|-----|-----|
| Unified Procedure Step - Push | 1.2.840.10008.5.1.4.34.6.1 | Yes | Yes |
| Unified Procedure Step - Watch | 1.2.840.10008.5.1.4.34.6.2 | Yes | Yes |
| Unified Procedure Step - Pull | 1.2.840.10008.5.1.4.34.6.3 | Yes | Yes |
| Unified Procedure Step - Event | 1.2.840.10008.5.1.4.34.6.4 | Yes | No |
| Unified Procedure Step - Query | 1.2.840.10008.5.1.4.34.6.5 | Yes | No |

Each request carries the UPS Push SOP Class UID, whichever of these was negotiated (CC.2). A request a
negotiated class does not carry is answered 0211H: N-CREATE on Push; N-SET and Change UPS State on Pull;
Request UPS Cancel on Push and Watch; subscriptions on Watch; C-FIND on Pull, Watch and Query; N-GET on all
four. The SCU classes cover N-CREATE, N-GET, N-SET, the N-ACTIONs and subscriptions, and parse event reports;
no client sends C-FIND on Pull or Query, and no product listens for UPS events as an SCU.

Change UPS State follows Table CC.1.1-2:

| Request | SCHEDULED | IN PROGRESS | COMPLETED | CANCELED | No such UPS |
|---------|-----------|-------------|-----------|----------|-------------|
| To IN PROGRESS, correct Transaction UID | Success | C302H | C300H | C300H | C307H |
| To COMPLETED, correct Transaction UID | C310H | Success or C304H | B306H | C300H | C307H |
| To CANCELED, correct Transaction UID | C310H | Success or C304H | C300H | B304H | C307H |
| Any of these, incorrect Transaction UID | C301H | C301H | C301H | C301H | C307H |
| To SCHEDULED | C303H | C303H | C303H | C303H | C307H |
| Request UPS Cancel | Canceled | Reported, or C312H | C311H | B304H | C307H |
| N-CREATE of this UID | 0111H | 0111H | 0111H | 0111H | Created |

In SCHEDULED no Transaction UID is recorded, so "correct" means one was offered. COMPLETED needs, in the
Performed Procedure Sequence, the Station Name and Workitem Code Sequences, the Start and End DateTime, and
an Output Information Sequence, which may be empty. CANCELED needs a Procedure Step Cancellation DateTime.
Otherwise the answer is C304H. A step in progress is not canceled by a request: its subscribers hear
of it, and with none to tell the answer is C312H. A SCHEDULED step goes through IN PROGRESS to CANCELED.

| Other requests | Status |
|----------------|--------|
| N-CREATE with a state other than SCHEDULED | C309H |
| N-CREATE with a Transaction UID, or a UID or priority that is not valid | 0106H |
| N-CREATE without a Type 1 attribute of Table CC.2.5-3 | 0120H |
| N-SET in SCHEDULED, with a Transaction UID | C310H |
| N-SET in IN PROGRESS, without the recorded Transaction UID | C301H |
| N-SET in COMPLETED or CANCELED | C300H |
| N-SET of the Procedure Step State | 0106H |
| N-SET or N-GET of a UPS that does not exist | C307H |
| Subscribing without Receiving AE or Deletion Lock | 0120H |
| Subscribing a Receiving AE without an address | C308H |
| Subscribing to a UPS that does not exist, or a filtered global subscription | C307H |
| Suspending anything but the global subscription | C314H |

The SCP creates the UID when an N-CREATE has none, and the Worklist Label when it has none. N-GET and C-FIND
never return the Transaction UID, which is the lock. An N-SET may add attributes the N-CREATE did not have.
C-FIND matches the keys with values by VR, as in Section 3.3, and does not look inside sequence keys.

Reports go to the Receiving AE on an association `pacs_server` opens, as in Section 3.6, on the UPS Event
context with the UPS Push class: 1 State Report, with an initial one on subscribing (Table CC.2.3-2), 2 Cancel
Requested with the Requesting AE, 3 Progress when the progress sequence is set, 5 Assigned when the station or
performer is set. A restart sends 4 SCP Status Change, RESTARTED with both lists WARM START, under the global
subscription UID. A failed delivery changes neither the subscription nor the lock, and blocks no request.

The state, the lock and subscriptions are in the database and the whole instance in a Part 10 file under
`--ups-dir` (default `<storage-dir>.ups`), so they survive a restart. Nothing deletes a workitem, so deletion
locks hold trivially. `dimse_n_scu` hosts the SCU classes.

---

## 4. Supported Transfer Syntaxes

The tables list every transfer syntax in the registry (`src/encoding/transfer_syntax.cpp`).
The **PACS_SCP negotiation** column states whether PACS_SCP accepts the syntax in a presentation
context during association negotiation, for every SOP Class in Section 3. Both server
implementations take that list from `include/kcenon/pacs/network/detail/scp_transfer_syntaxes.h`,
and `tests/network/scp_transfer_syntaxes_test.cpp` fails when the list and this section disagree.
`transfer_syntax::is_supported()` is false exactly for the rows whose last column is "Not
supported" or "Not implemented".

### 4.1 Uncompressed

| Transfer Syntax | UID | PACS_SCP negotiation | Part 10 files |
|----------------|-----|----------------------|---------------|
| Implicit VR Little Endian | 1.2.840.10008.1.2 | Accepted | Read and write |
| Explicit VR Little Endian | 1.2.840.10008.1.2.1 | Accepted | Read and write |
| Explicit VR Big Endian (Retired) | 1.2.840.10008.1.2.2 | Not accepted | Read and write |
| Deflated Explicit VR Little Endian | 1.2.840.10008.1.2.1.99 | Not accepted | Not supported |

DIMSE messages carry datasets only in Implicit and Explicit VR Little Endian, so PACS_SCP does
not accept Explicit VR Big Endian; the `dicom_file` Part 10 reader and writer handle it.
File byte-order conversion applies recursively to sequence items: US/SS/OW/AT
use 16-bit units, UL/SL/FL/OL/OF use 32-bit units, and FD/SV/UV/OD/OV use
64-bit units. AT preserves group/element order; OW uses 16-bit words even with
Bits Allocated 8. OB and text are unchanged. Incomplete binary values are rejected
during Big Endian decoding or writing. Independent intermediate-byte and DCMTK
pixel checks are in `tools/integration_tests/scripts/test_big_endian_values.py`;
this does not expand retired Big Endian network negotiation.
pacs_system has no inflate or deflate code, so it does not support Deflated Explicit VR Little
Endian at all.

### 4.2 Compressed (Encapsulated Pixel Data)

| Transfer Syntax | UID | Type | PACS_SCP negotiation | Pixel data encoding and decoding |
|----------------|-----|------|----------------------|----------------------------------|
| JPEG Baseline (Process 1) | 1.2.840.10008.1.2.4.50 | Lossy | Accepted | Requires libjpeg-turbo |
| JPEG Lossless, Non-Hierarchical, First-Order Prediction | 1.2.840.10008.1.2.4.70 | Lossless | Accepted | Built in |
| JPEG-LS Lossless Image Compression | 1.2.840.10008.1.2.4.80 | Lossless | Accepted | Requires CharLS |
| JPEG-LS Lossy (Near-Lossless) Image Compression | 1.2.840.10008.1.2.4.81 | Lossy (near-lossless) | Accepted | Requires CharLS |
| JPEG 2000 Image Compression (Lossless Only) | 1.2.840.10008.1.2.4.90 | Lossless | Accepted | Requires OpenJPEG |
| JPEG 2000 Image Compression | 1.2.840.10008.1.2.4.91 | Lossy | Accepted | Requires OpenJPEG |
| RLE Lossless | 1.2.840.10008.1.2.5 | Lossless | Accepted | Built in |
| High-Throughput JPEG 2000 (Lossless Only) | 1.2.840.10008.1.2.4.201 | Lossless | Accepted | Requires OpenJPH |
| High-Throughput JPEG 2000 with RPCL (Lossless Only) | 1.2.840.10008.1.2.4.202 | Lossless | Accepted | Requires OpenJPH |
| High-Throughput JPEG 2000 | 1.2.840.10008.1.2.4.203 | Lossless/Lossy | Accepted | Requires OpenJPH |
| HEVC/H.265 Main Profile / Level 5.1 | 1.2.840.10008.1.2.4.107 | Lossy | Not accepted | Not implemented (stub codec) |
| HEVC/H.265 Main 10 Profile / Level 5.1 | 1.2.840.10008.1.2.4.108 | Lossy | Not accepted | Not implemented (stub codec) |
| JPEG XL Lossless | 1.2.840.10008.1.2.4.110 | Lossless | Not accepted | Not implemented (stub codec) |
| JPEG XL JPEG Recompression | 1.2.840.10008.1.2.4.111 | Lossless | Not accepted | Not implemented (stub codec) |
| JPEG XL | 1.2.840.10008.1.2.4.112 | Lossless/Lossy | Not accepted | Not implemented (stub codec) |
| Frame Deflate | 1.2.840.10008.1.2.11 | Lossless | Not accepted | Not implemented (stub codec) |

PACS_SCP accepts the codec-backed syntaxes whether or not the build includes the codec: it
stores the pixel data without decoding it. Encoding and decoding need the library named in the
last column at build time ([Appendix B](#appendix-b-known-limitations)), and
`codec_factory::is_available()` reports which codecs a given build includes. The built-in JPEG
Lossless codec has limitations listed in Appendix B. HEVC, JPEG XL, and Frame Deflate are in
the registry, but their codecs are stubs whose encoding and decoding always fail, and PACS_SCP
does not accept them.

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

Undefined-length `UN` values in Explicit VR Little or Big Endian files are decoded
as sequences whose Items and contents use Implicit VR Little Endian (PS3.5 6.2.2).
They are retained as `SQ` elements on re-encoding, preserving nested items and
following elements. Defined-length `UN` remains opaque. Malformed UN item streams,
invalid/missing delimiters and more than 64 nested sequence levels within that
path return errors. A dataset that ends before its last element is complete is
rejected instead of being read as far as it goes. Little Endian files may have
only zero bytes after the last element; Explicit VR Big Endian files also refuse
trailing bytes too short for an element header, even when they are zero. This
does not assert strict validation of every unrelated legacy parser path.

### 5.2 DICOMDIR

DICOMDIR support is provided via the `dcm_dir` utility:

| Operation | Support |
|-----------|---------|
| Create DICOMDIR | Yes |
| List DICOMDIR contents | Yes |
| Verify DICOMDIR integrity | Yes |
| Update DICOMDIR | Add/rebuild; deletion is not implemented |

Directory Record types: PATIENT, STUDY, SERIES, IMAGE.

The writer emits records in deterministic patient/study/series/instance order and
sets first-root, last-root, next-sibling and lower-level UL offsets to the absolute
Item-tag positions, including the 128-byte preamble and `DICM` prefix. File Meta
Information carries the Directory Storage SOP identifiers and group length; the
Basic Directory dataset does not carry SOP Class/Instance UID attributes.

`list`, `verify` and the update reader follow stored links, including when physical
record order differs. They reject missing or malformed links, offsets that do not
point to record Item tags, cycles, shared children, unreachable records and invalid
hierarchy types. Empty directories and multiple roots/siblings are supported.
Explicit VR Little Endian directories with defined or undefined sequence/item
lengths are supported, with a 32-bit file-offset limit and a nesting limit of 32.
Inactive records, MRDR/private record types and other directory hierarchies are
not supported. Add/rebuild updates preserve the File-set UID and ID.

Independent pydicom `FileSet` loading, DCMTK-created directories, literal offset
checks and corrupt-link rejection are exercised by
`tools/integration_tests/scripts/test_dicomdir_offsets.py`. This establishes the
listed directory behavior, not full media application-profile or source-IOD
validation. Referenced paths retain their actual spelling and resolve from the
DICOMDIR parent; callers prepare compliant File ID names and media layout. The
tool does not rename/copy files or validate every media filename/character-set rule.

---

## 6. Support of Character Sets

### 6.1 Supported Character Sets

| Character Set | Defined Term | ISO-IR | Support |
|--------------|--------------|--------|---------|
| Default (ASCII) | ISO_IR 6 | IR 6 | Full |
| Latin-1 (Western European) | ISO_IR 100 | IR 100 | Full |
| Unicode UTF-8 | ISO_IR 192 | IR 192 | Full |
| Korean (KS X 1001) | ISO 2022 IR 149 | IR 149 | Full |
| Japanese Kanji (JIS X 0208) | ISO 2022 IR 87 | IR 87 | Full |
| Japanese Katakana (JIS X 0201) | ISO_IR 13 | IR 13 | Full |
| Japanese Katakana (JIS X 0201) | ISO 2022 IR 13 | IR 13 | Full |
| Simplified Chinese (GB 2312) | ISO 2022 IR 58 | IR 58 | Full |

### 6.2 ISO 2022 Code Extension Support

The implementation supports ISO 2022 escape sequence-based code extensions
as specified in DICOM PS3.5 Section 6.1. Multi-valued Specific Character Set
(0008,0005) attributes are parsed, and text segments are decoded/encoded
per their associated character set.

Features:
- Automatic detection and splitting by ISO 2022 escape sequences
- Per-segment encoding conversion using platform iconv (POSIX) or
  MultiByteToWideChar/WideCharToMultiByte (Windows)
- Person Name (PN) component group decoding (Alphabetic=Ideographic=Phonetic)
- Round-trip encode/decode support for CJK character sets
- Dataset-level `get_decoded_string()` / `set_encoded_string()` convenience
  methods that auto-detect Specific Character Set from the dataset

---

## 7. Security

### 7.1 TLS Transport Connection Profile

The implementation supports TLS for secure DICOM communication per DICOM PS3.15
and BCP 195 (RFC 9325).

#### TLS Protocol Versions

| Protocol | Support |
|----------|---------|
| TLS 1.3 | Full (preferred) |
| TLS 1.2 | Full (minimum required) |
| TLS 1.1 and below | Rejected |

#### BCP 195 Security Profiles

| Profile | Minimum TLS | Key Features |
|---------|-------------|-------------|
| BCP 195 Basic | 1.2 | Standard cipher suites, RSA >= 2048, ECDSA >= P-256 |
| BCP 195 Non-Downgrading | 1.2 (prefers 1.3) | No protocol downgrade allowed, DICOM PS3.15 recommended profile |
| BCP 195 Extended | 1.3 only | Strictest cipher suites, RSA >= 3072, ECDSA >= P-256 |

The **BCP 195 Non-Downgrading** profile is the default and recommended configuration
per DICOM PS3.15.

#### TLS 1.3 Cipher Suites

| Cipher Suite | Profile |
|-------------|---------|
| TLS_AES_256_GCM_SHA384 | Basic, Non-Downgrading, Extended |
| TLS_AES_128_GCM_SHA256 | Basic, Non-Downgrading |
| TLS_CHACHA20_POLY1305_SHA256 | Basic, Non-Downgrading, Extended |

#### TLS 1.2 Cipher Suites (BCP 195 Recommended)

| Cipher Suite | Profile |
|-------------|---------|
| ECDHE-ECDSA-AES256-GCM-SHA384 | Basic, Non-Downgrading |
| ECDHE-RSA-AES256-GCM-SHA384 | Basic, Non-Downgrading |
| ECDHE-ECDSA-AES128-GCM-SHA256 | Basic, Non-Downgrading |
| ECDHE-RSA-AES128-GCM-SHA256 | Basic, Non-Downgrading |
| ECDHE-ECDSA-CHACHA20-POLY1305 | Basic, Non-Downgrading |
| ECDHE-RSA-CHACHA20-POLY1305 | Basic, Non-Downgrading |

#### Certificate Constraints

| Parameter | Basic | Non-Downgrading | Extended |
|-----------|-------|-----------------|----------|
| RSA minimum key size | 2048 bits | 2048 bits | 3072 bits |
| ECDSA minimum curve | P-256 | P-256 | P-256 |

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

The named presets implement a subset of Annex E actions. They do not establish
complete PS3.15, HIPAA, or GDPR compliance. Applications must evaluate the IOD,
remaining identifying content, and intended disclosure before releasing data.

| Preset | Implemented behavior | Limits |
|--------|----------------------|--------|
| Basic | Configured identifier actions, recursive sequences and consistent instance UID mappings | Partial attribute table; no complete IOD validation |
| Clean Pixel Data | Same metadata actions as Basic | Pixel cleaning and OCR are **not implemented** |
| Clean Descriptions | Empty configured description fields; recursively clean selected sequence items | No semantic cleaning of arbitrary text or codes |
| Retain Longitudinal | Shift configured dates with a shared offset | Caller chooses offset; not all date/time attributes are covered |
| Retain Patient Characteristics | Keep configured demographic fields | Retained values may identify a patient |
| HIPAA Safe Harbor | Additional configured identifier removals | Preset name is not a compliance guarantee |
| GDPR Compliant | Hash configured patient identifiers | Pseudonymization is not anonymization or a compliance guarantee |

Sequence actions follow the current [PS3.15 Table E.1-1](https://dicom.nema.org/medical/dicom/current/output/chtml/part15/chapter_E.html)
subset in `src/security/anonymization_sequence_rules.h`:

- X removes the entire sequence. Z retains an empty sequence. D replaces the
  contents with one empty dummy item; required IOD-specific item attributes are
  not synthesized, so the result may require further IOD validation.
- K, C and U retain items and recursively apply configured attribute actions at
  every depth. U remaps instance UID values with the same mapping as the root.
  SOP Class and Transfer Syntax UIDs remain unchanged. C does not perform semantic
  text, code, or pixel recognition.
- Without IOD type information, X/Z and X/Z/D choose Z; X/D chooses X. Image
  reference X/Z/U* uses U. Referenced Study Sequence defaults to Z; an explicit
  custom K action retains and cleans its items. Content Sequence uses D, rather
  than claiming support for the Clean Structured Content option.
- Anatomic Region and selected clinical/reference containers have explicit K
  policies. Unknown sequences are emptied at all depths. Unparsed retained
  sequence payloads, unsupported sequence transforms, and nesting beyond 64 items
  produce report errors and clear the affected sequence. Unknown opaque UN
  values are also emptied, since they can hide implicit-VR sequences missing
  from the dictionary; retaining or transforming non-empty UN values fails. Callers must check both
  the Result and `report.is_successful()` before saving.
- Private-tag policy applies in every retained item. The library keeps private
  elements by default for API compatibility; callers should select `remove_all`
  when appropriate. `dcm_anonymize` removes all private elements, including their
  creators, at every depth. A custom keep action does not override this policy.

`dcm_anonymize` shares one UID mapping across all files in an invocation, rebuilds
file metadata and the preamble, and refuses to save failed reports. Mapping files
can preserve linkage between invocations and contain original identifiers. The
`dcm_modify` compatibility anonymizer delegates to the same engine; its options
apply recursively. The `dcm_modify` executable itself exposes tag editing, not an
anonymization command.

Custom retention may preserve identifying data. `--verify` only checks common
identifiers recursively and reports tag names/depths without values; it is not a
compliance check. The library's opt-in detailed action records can contain
original values and must be treated as sensitive data.

### 7.4 Access Control

- AE Title-based whitelist filtering
- Role-Based Access Control (RBAC)
- User Identity Negotiation (item types 0x58/0x59)

---

## 8. DICOMweb Support

The implementation plays the role of origin server, natively: `pacs_server --http-port` serves these resources under
`/dicomweb` and WADO-URI under `/wado`, and not the REST API under `/api`, on the same database and storage as its DICOM services, so an
object stored over either protocol is found, unchanged, over both. `validation/dcmtk/tests/dicomweb_probe.py` measures this with curl, dcm4che and DCMTK, after
the same tables hold on another implementation.

Every request needs an `Accept` header: without one the answer is 406 (PS3.18 8.7.5). DICOM and rendered types together
are 400. A type that does not apply to the resource is passed over, in the client's order of preference.

### 8.1 WADO-RS (Web Access to DICOM Objects - RESTful)

| Resource | Media types | Notes |
|----------|-------------|-------|
| Study, Series | `multipart/related; type="application/dicom"` (default) | Every object, each once, even when there is one. `application/dicom` alone is 406 |
| Instance | The same (default), and `application/dicom` | |
| Study, Series, Instance | `multipart/related; type="application/octet-stream"` | The Bulk Data of each instance, as Table 10.4.4-1 Note 2 defines |
| Metadata | `application/dicom+json` (default), `multipart/related; type="application/dicom+xml"` | Every top level attribute. Pixel Data by Bulk Data URI, anything else binary inline, nested values inline |
| Bulk Data | `multipart/related; type="application/octet-stream"` (default), `application/octet-stream` | At the URI the metadata gives, per instance. Pixel Data comes decoded |
| Frames | `multipart/related; type="application/octet-stream"` (default); `application/octet-stream` for one frame | Decoded when the object is compressed; each part has its Content-Location |
| Frames, as held | `multipart/related; type="image/jpeg"`, `"image/jls"`, `"image/jp2"`, `"image/jphc"`, `"image/jxl"`, `"image/dicom-rle"` | Only for the form the object is held in, with the fragment delimiters removed |
| Rendered Instance, Rendered Frame | `image/jpeg` (default), `image/png`, `image/jphc` | |

**Transfer Syntax.** With no `transfer-syntax` parameter, objects go out in Explicit VR Little Endian, decoded if need
be, unless only lossy pixels are held, or no codec for them is built: those go out as held. `transfer-syntax=*` asks for what is held. Implicit
VR and Big Endian objects are converted, since Web Services shall not use them. A Transfer Syntax that is neither what
is held nor Explicit VR Little Endian is 406: there is no encoder here for the others.

**All or nothing.** 200 means every representation of the resource (PS3.18 10.4.3.1). An indexed file that cannot be read fails
the whole retrieve with 500. A resource named under the wrong study or series is 404, like one that does not exist.

**Frames** are numbers in ascending order, each once, as PS3.18 10.1 defines them; `2-4` is taken for `2,3,4`. Anything else
is 400, and a frame past the end fails the whole request with 404.

**Rendering.** `window=center,width,function` takes `linear`, `linear-exact` or `sigmoid`, by PS3.3 C.11.2.1.2 and C.11.2.1.3. With none, the
first stored window applies, or else the frame's own range. MONOCHROME1, MONOCHROME2, RGB and YBR_FULL are rendered, with
compressed objects decoded first. `viewport=width,height` scales to fit and keeps the proportions. These are refused
with 400: a viewport with a source rectangle, PALETTE COLOR, and uncompressed YBR_FULL_422.

### 8.2 STOW-RS (Store Over the Web)

| Status | Meaning |
|--------|---------|
| 200 | Every part stored |
| 202 | Some parts stored; the others are in Failed SOP Sequence with a Failure Reason |
| 400 | No boundary, no parts, no closing delimiter, or a part without the blank line ending its headers |
| 406 | The response cannot be given in an acceptable type, or there is no `Accept` header. Nothing is stored |
| 409 | No part stored; each is in Failed SOP Sequence with a Failure Reason |
| 413 | The request exceeds `max_body_size` (`--http-max-body-mb` for `pacs_server`). Nothing is stored |
| 415 | Not `multipart/related` of `application/dicom` |

The response is `application/dicom+json` or `application/dicom+xml`, with a Retrieve URL for each stored object. A part
that is not `application/dicom` is listed as a failure rather than skipped. Failure Reason (0008,1197) is 0xC000 for what cannot be read or
understood, 0xA900 for a missing or malformed identifying attribute or another study's object in a study's target resource,
0x0111 for a duplicate and 0xA700 when it cannot be written.

STOW-RS and C-STORE store through one function, `storage::archive_instance`, so both keep an object in the Transfer Syntax it came in,
index it in UTF-8, apply the configured duplicate policy, and refuse the same things. An object is refused, with nothing written, when
its Patient ID is empty, because no query could reach it, or when a UID is not digits and dots of at most 64 characters, because UIDs
name files and appear in URLs. C-STORE answers those with A900H, and a duplicate with 0111H. If the index cannot
take an object, its file is removed again and the sender is told. Metadata and Bulk Data parts are not supported: send Part 10 objects.

### 8.3 QIDO-RS (Query based on ID for DICOM Objects)

| Feature | Support |
|---------|---------|
| Search for Studies, Series and Instances, at the top level and within a study or series | Yes |
| Media types | `application/dicom+json` (default), `multipart/related; type="application/dicom+xml"` |
| Matching | The Hierarchical Search Method, by the same evaluation as C-FIND (Section 3.3): `*` and `?` are the only wildcards, date and time ranges, UID lists with commas |
| Maximum matches | 100 per response unless `limit` asks otherwise, and at most 1000 in one response |
| More results than returned | `Warning: 299 <service>: There are <n> additional results that can be requested` |
| Fuzzy Matching, Empty Value Matching, Multiple Value Matching | Not supported: asking for them gets the Warning of PS3.18 8.3.4 and a literal search |
| `includefield` | Any attribute of the first object of the result, by Keyword or Tag. `all` at the instance level means every attribute but Bulk Data, and adds nothing at the study and series levels |
| Attributes the index does not hold | Not used for matching, and named in a Warning |

A query parameter that is not a Keyword, a Tag or one of the above is 400, like a repeated attribute or a `limit` or `offset` that is not an
unsigned integer. A search with no matches is 200 with an empty list. Under a study or series that does not exist it is 404. Identical
requests return the same order while the archive does not change.

### 8.4 DICOMweb Storage Commitment (Supplement 234)

The implementation provides a RESTful equivalent of the DIMSE Storage Commitment
Push Model, as defined in DICOM Supplement 234.

| Feature | Support |
|---------|---------|
| Request storage commitment (POST) | Yes |
| Query commitment status (GET) | Yes |
| Per-instance commitment results | Yes |
| Transaction UID tracking | Yes |

This supplements the DIMSE-based Storage Commitment (Section 3.6) with a RESTful
interface for DICOMweb-native workflows.

### 8.5 OAuth 2.0 Authorization

DICOMweb endpoints support OAuth 2.0 authorization per RFC 6749.

| Feature | Support |
|---------|---------|
| JWT Bearer token validation | Yes |
| JWKS (JSON Web Key Set) key retrieval | Yes |
| RS256 and ES256 signatures | Yes |
| Token issuer, audience and validity period validation | Yes |
| Scope-based access control | Yes, fixed per route |

When OAuth 2.0 is enabled, every DICOMweb route, including `/wado`, requires a valid JWT Bearer token: without one the
answer is 401 with `WWW-Authenticate: Bearer`, and with the wrong scope it is 403.

| Scope | Opens |
|-------|-------|
| `dicomweb.read` | Retrieve, metadata, Bulk Data, frames, rendered, WADO-URI, commitment status, and search |
| `dicomweb.search` | Search only |
| `dicomweb.write` | Store and commitment requests |

**When it is not enabled, DICOMweb has no authorization at all**: everyone who can reach the port can read and store objects.
`pacs_server` therefore listens on loopback unless told otherwise, says so when it starts, allows no web origin unless told one (and answers no CORS preflight, so a browser can only make requests that need none), and refuses
to start with only some of its OAuth settings. These tokens protect DICOMweb and WADO-URI. Most of the REST API under `/api` checks no credentials at all, which is why `pacs_server` does not serve it; a host of `rest_server` that keeps it must protect it some other way. Keys come from a JWKS file. Fetching a JWKS from a URL needs an embedder to set a fetcher.

### 8.6 Supported Media Types

| Media Type | Usage |
|-----------|-------|
| application/dicom | One instance in a single part response |
| application/dicom+json | Metadata, search results and the store response |
| application/dicom+xml | The same in the Native DICOM Model; multipart for metadata and search |
| application/octet-stream | Uncompressed Bulk Data and frames |
| image/jpeg, image/png, image/jphc | Rendered images |
| image/jpeg, image/jls, image/jp2, image/jphc, image/jxl, image/dicom-rle | Frames as held |
| multipart/related | Multi-part responses and store requests |

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
| Supplement 232 | JPEG XL Transfer Syntaxes |
| Supplement 234 | DICOMweb Storage Commitment |
| Supplement 235 | HTJ2K Transfer Syntaxes |
| Supplement 240 | Heightmap Segmentation IOD |
| Supplement 243 | Label Map Segmentation IOD |
| Supplement 244 | Frame Deflate Transfer Syntax |
| RFC 6749 | OAuth 2.0 Authorization Framework |
| RFC 9325 | Recommendations for Secure Use of TLS and DTLS (BCP 195) |

---

## Appendix A: IOD Validation

The implementation includes modality-specific IOD validators for the following modalities:

- CT, MR, US, XA/XRF, DX, MG, NM, PET, RT, SEG, SR, SC
- CT For Processing (multi-energy/spectral CT)
- Heightmap Segmentation (Supplement 240)
- Label Map Segmentation (Supplement 243)
- Waveform Presentation State and Annotation

Each validator checks Mandatory (M), Conditional (C), and User-optional (U)
attributes as defined in the corresponding IOD specifications in PS3.3.

## Appendix B: Known Limitations

| Area | Limitation | Reference |
|------|-----------|-----------|
| Character Sets | CJK character sets (ISO 2022 IR 87/149/58/13) fully supported | Issue #700 (resolved) |
| Storage Commitment | Push Model SCP/SCU implemented (N-ACTION, N-EVENT-REPORT). Reports waiting for a retry do not survive a restart | Section 3.6 |
| Unified Procedure Step | Filtered global subscriptions are refused with C307H. No client for C-FIND on Pull or Query, and no product listens for events as an SCU | Section 3.8 |
| JPEG Baseline Codec | Requires libjpeg-turbo at build time; built when `PACS_BUILD_CODECS=ON` (default) and CMake finds the library (`PACS_WITH_JPEG_CODEC`) | Optional dependency |
| JPEG 2000 Codec | Requires OpenJPEG at build time; built when `PACS_BUILD_CODECS=ON` (default) and CMake finds the library (`PACS_WITH_JPEG2000_CODEC`) | Optional dependency |
| JPEG-LS Codec | Requires CharLS at build time; built when `PACS_BUILD_CODECS=ON` (default) and CMake finds the library (`PACS_WITH_JPEGLS_CODEC`) | Optional dependency |
| HTJ2K Codec | Requires OpenJPH at build time; built when `PACS_BUILD_CODECS=ON` (default) and CMake finds the library or, with `PACS_FETCH_OPENJPH=ON` (default), fetches it from GitHub (`PACS_WITH_HTJ2K_CODEC`) | Optional dependency |
| JPEG Lossless Codec | Grayscale only. The decoder uses the Huffman tables in the stream, and refuses color frames and restart intervals instead of guessing | Section 4.2 |
| HEVC, JPEG XL, Frame Deflate | Stub codecs: encoding and decoding always fail, and PACS_SCP does not negotiate these transfer syntaxes | Section 4.2 |
| Explicit VR Big Endian | Not negotiated by PACS_SCP; Part 10 files only | Section 4.1 |
| Deflated Explicit VR Little Endian | Not implemented and not negotiated | Section 4.1 |
| Asynchronous Operations | Not supported (single operation per association) | N/A |
| Print Management | Film Session, Film Box, Image Box and Printer (SCP + SCU). No Print Job, Presentation LUT or Annotation Box; `pacs_server` writes jobs to a spool directory | Section 3.7 |
| Patient Information Reconciliation | A local library service with no DIMSE exchange, so not part of this statement | N/A |
| Generic N-GET SCP | `n_get_scp` has no product host. N-GET is provided by the UPS and Printer services | Section 3.8 |

`PACS_BUILD_CODECS=OFF` disables all four optional codecs. With it on, CMake looks for each library at
configure time and disables only the codec whose library is missing, with a warning
(`cmake/dependencies.cmake`), unless `PACS_REQUIRE_CODECS=ON` (default OFF) turns a missing library
into a configure error; the compile definition in parentheses is set when the codec is
built, and `codec_factory::is_available()` reports it at run time.

---

## See Also

The following non-normative references may help integrators locate upstream requirements and implementation details. They do not modify any conformance claim in this statement.

- [PRD.md](PRD.md) — FR-1 DICOM Services, FR-3 Storage, FR-8 Query/Retrieve conformance requirements
- [ARCHITECTURE.md](ARCHITECTURE.md) — Overall system architecture context
- [SDS_NETWORK_V2.md](SDS_NETWORK_V2.md) — DIMSE implementation details for the SOP classes above
- [SDS_SEQUENCES.md](SDS_SEQUENCES.md) — C-STORE/C-FIND/C-MOVE sequence diagrams
- [IHE_INTEGRATION_STATEMENT.md](IHE_INTEGRATION_STATEMENT.md) — IHE profile conformance (companion document)

---

*DICOM is the registered trademark of the National Electrical Manufacturers Association (NEMA).*
