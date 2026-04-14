---
doc_id: "PAC-INTR-002"
doc_title: "IHE Integration Statement"
doc_version: "0.1.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "pacs_system"
category: "INTR"
---

# IHE Integration Statement

> **SSOT**: This document is the single source of truth for **IHE Integration Statement**.

## PACS System

**Product Name**: PACS System
**Version**: 0.1.0
**Vendor**: kcenon
**Date**: 2026-03-05

This document describes the IHE (Integrating the Healthcare Enterprise) integration
profiles supported by the PACS System. It is structured according to the
[IHE Integration Statement template](https://wiki.ihe.net/index.php/IHE_Integration_Statement).

For DICOM-level conformance details, see the
[DICOM Conformance Statement](DICOM_CONFORMANCE_STATEMENT.md).

---

## 1. Integration Profiles Implemented

| IHE Domain | Profile | Actor(s) | Options |
|-----------|---------|----------|---------|
| RAD | XDS-I.b (Cross-Enterprise Document Sharing for Imaging) | Imaging Document Source, Imaging Document Consumer | None |
| RAD | AIRA (AI Result Assessment) | Assessment Creator | Structured Report output |
| RAD | PIR (Patient Information Reconciliation) | Image Manager/Archive | Demographics Update, Patient Merge, Patient Link |

---

## 2. Profile Details

### 2.1 XDS-I.b -- Cross-Enterprise Document Sharing for Imaging

#### Overview

Enables cross-enterprise sharing of imaging documents through an XDS infrastructure.
The PACS System implements both the Imaging Document Source and Imaging Document
Consumer actors, allowing it to publish imaging studies to and retrieve studies from
a document registry/repository.

#### Actors and Transactions

| Actor | Transaction | Direction | Support |
|-------|-----------|-----------|---------|
| Imaging Document Source | ITI-41 (Provide and Register Document Set-b) | Outbound | Yes |
| Imaging Document Source | RAD-68 (Provide and Register Imaging Document Set) | Outbound | Yes |
| Imaging Document Consumer | ITI-43 (Retrieve Document Set) | Outbound | Yes |
| Imaging Document Consumer | RAD-69 (Retrieve Imaging Document Set) | Outbound | Yes |

#### Key Features

- KOS (Key Object Selection) document generation for imaging manifests
- XDS metadata extraction from DICOM datasets (patient, study, accession)
- Document submission to XDS Repository with full metadata envelope
- Imaging study retrieval via RAD-69 transaction
- Configurable repository and registry endpoints

#### Implementation References

- `include/pacs/services/xds/imaging_document_source.hpp`
- `include/pacs/services/xds/imaging_document_consumer.hpp`
- `tests/services/xds/xds_imaging_test.cpp`

---

### 2.2 AIRA -- AI Result Assessment

#### Overview

Implements the IHE Radiology AIRA (AI Result Assessment) profile, enabling
clinicians to formally assess AI-generated results (accept, modify, or reject).
Assessments are stored as DICOM Structured Reports following the AIRA template.

#### Actors and Transactions

| Actor | Transaction | Direction | Support |
|-------|-----------|-----------|---------|
| Assessment Creator | Create Assessment (RAD-Y1) | Outbound (SR creation) | Yes |

#### Assessment Types

| Type | Description | DICOM Code |
|------|------------|------------|
| Accept | Clinician accepts AI result as-is | Coded value via `assessment_type_to_code()` |
| Modify | Clinician modifies AI result (e.g., edits segmentation contours) | Coded value with modification details |
| Reject | Clinician rejects AI result with documented reason | Coded value with rejection reason |

#### Assessment Lifecycle

| Status | Description |
|--------|-------------|
| Draft | Assessment in progress, not yet finalized |
| Final | Assessment finalized and signed off |
| Amended | Previously finalized assessment has been amended |

#### Key Features

- Assessment creation as DICOM SR with structured content
- Assessed result references via SOP Instance UID
- Modification tracking (original vs. modified regions)
- Rejection reasons with coded vocabulary
- Assessment status lifecycle (draft -> final -> amended)
- Integration with existing AI result handler pipeline

#### Implementation References

- `include/pacs/ai/aira_assessment.hpp`
- `include/pacs/ai/aira_assessment_manager.hpp`
- `tests/ai/aira_assessment_test.cpp`

---

### 2.3 PIR -- Patient Information Reconciliation

#### Overview

Implements the IHE Radiology PIR (Patient Information Reconciliation) profile
as the Image Manager/Archive actor. When patient demographics are corrected
after image acquisition (e.g., wrong patient ID, name change after marriage),
PIR ensures all stored DICOM instances are updated consistently.

#### Actors and Transactions

| Actor | Transaction | Direction | Support |
|-------|-----------|-----------|---------|
| Image Manager/Archive | RAD-12 (Patient Update) | Inbound | Yes |

#### Supported HL7 v2.x Messages

| Message | Event | Description |
|---------|-------|-------------|
| ADT^A08 | Update Patient Information | Update patient demographics (name, ID, DOB, sex) across all stored instances |
| ADT^A40 | Merge Patient | Reassign all instances from source patient to target patient |
| ADT^A24 | Link Patient Information | Link related patient records without merging |

#### Reconciliation Workflow

```
ADT System                           PACS System (Image Manager)
    |                                       |
    |  ADT^A08 (Demographics Update)        |
    |  Patient ID + updated fields          |
    |-------------------------------------->|
    |                                       |  Update all DICOM instances
    |                                       |  for matching Patient ID
    |                                       |
    |  ADT^A40 (Patient Merge)              |
    |  Source Patient -> Target Patient     |
    |-------------------------------------->|
    |                                       |  Reassign all instances
    |                                       |  from source to target
    |                                       |
```

#### Key Features

- Demographics update propagation across all stored instances for a patient
- Patient merge with complete instance reassignment
- Patient linking for related records
- Audit trail of all reconciliation operations
- Batch processing for large patient record sets

#### Implementation References

- `include/pacs/services/pir/patient_reconciliation_service.hpp`
- `tests/services/pir/patient_reconciliation_test.cpp`

---

## 3. Internet Communication Security

| Feature | Support |
|---------|---------|
| TLS 1.3 (BCP 195 Non-Downgrading Profile) | Yes |
| Audit Trail and Node Authentication (ATNA) | Via TLS transport security |
| Digital Signatures | Yes (RSA-SHA256) |

For full TLS and security details, see
[DICOM Conformance Statement Section 7](DICOM_CONFORMANCE_STATEMENT.md#7-security).

---

## 4. System Architecture

```
+------------------+           +------------------+           +------------------+
|                  |  RAD-68   |                  |  RAD-69   |                  |
|   Modality /     |---------->|   PACS System    |<----------|   External       |
|   AI Engine      |  DICOM    |                  |  XDS-I.b  |   XDS Registry   |
|                  |  C-STORE  | +==============+ |           +------------------+
+------------------+           | | XDS-I.b      | |
                               | | Source/      | |           +------------------+
+------------------+           | | Consumer     | |  ADT^A08  |                  |
|                  |  RAD-Y1   | +==============+ |  ADT^A40  |   ADT / MPI      |
|   Clinician      |---------->| +==============+ |<----------|   System         |
|   Workstation    |  AIRA     | | AIRA         | |  ADT^A24  |                  |
|                  |  Assess   | | Creator      | |           +------------------+
+------------------+           | +==============+ |
                               | +==============+ |
                               | | PIR          | |
                               | | Image Mgr    | |
                               | +==============+ |
                               +------------------+
```

---

## 5. Testing Status

| Profile | Status | Details |
|---------|--------|---------|
| XDS-I.b | Unit tested | `tests/services/xds/xds_imaging_test.cpp` |
| AIRA | Unit tested | `tests/ai/aira_assessment_test.cpp` |
| PIR | Unit tested | `tests/services/pir/patient_reconciliation_test.cpp` |

IHE Connectathon testing has not yet been performed. The profiles are implemented
according to the IHE Technical Framework specifications and validated through
unit and integration tests.

---

## 6. References

| Document | Title |
|----------|-------|
| IHE RAD TF-1 | RAD Technical Framework Volume 1: Integration Profiles |
| IHE RAD TF-2 | RAD Technical Framework Volume 2: Transactions |
| IHE RAD TF-3 | RAD Technical Framework Volume 3: Content Modules |
| IHE RAD Supplement AIRA | AI Result Assessment (2025-06-12) |
| IHE ITI TF-2 | IT Infrastructure Technical Framework Volume 2: Transactions |
| IHE PIR Profile | Patient Information Reconciliation |
| IHE XDS-I.b Profile | Cross-Enterprise Document Sharing for Imaging |
| HL7 v2.x | Health Level 7 Version 2.x Messaging Standard |

---

*IHE is a registered trademark of IHE International.*
