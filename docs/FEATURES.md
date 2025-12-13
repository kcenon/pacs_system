# PACS System Features

> **Version:** 0.1.8.0
> **Last Updated:** 2025-12-12
> **Language:** **English** | [한국어](FEATURES_KO.md)

This document provides comprehensive details on all features available in the PACS system.

---

## Table of Contents

- [DICOM Core Features](#dicom-core-features)
- [Network Protocol Features](#network-protocol-features)
- [DICOM Services](#dicom-services)
- [Storage Backend](#storage-backend)
- [Ecosystem Integration](#ecosystem-integration)
- [Security Features](#security-features)
- [Monitoring and Observability](#monitoring-and-observability)
- [Workflow Services](#workflow-services)
- [Planned Features](#planned-features)

---

## DICOM Core Features

### Data Element Processing

**Implementation**: Complete DICOM Data Element parser and encoder supporting all 27 Value Representations (VR).

**Features**:
- Parse and encode DICOM Data Elements (Tag, VR, Length, Value)
- Support for all 27 standard VR types (PS3.5)
- Automatic VR determination from Data Dictionary
- Little Endian and Big Endian byte ordering
- Explicit and Implicit VR encoding support

**Classes**:
- `dicom_element` - Core data element representation
- `dicom_tag` - Tag identification (group, element)
- `vr_type` - Value Representation enumeration
- `vr_codec` - VR-specific encoding/decoding

**Example**:
```cpp
#include <pacs/core/dicom_element.h>

using namespace pacs::core;

// Create a Patient Name element
auto patient_name = dicom_element::create(
    tags::PatientName,           // (0010,0010)
    vr_type::PN,                 // Person Name
    "Doe^John"                   // Value
);

// Access element properties
auto tag = patient_name.tag();           // 0x00100010
auto vr = patient_name.vr();             // PN
auto value = patient_name.as_string();   // "Doe^John"
```

### Value Representation Types

All 27 DICOM VR types are supported with appropriate container_system mappings:

| Category | VR Types | Description |
|----------|----------|-------------|
| **String** | AE, AS, CS, DA, DS, DT, IS, LO, LT, PN, SH, ST, TM, UI, UT | Text-based values |
| **Numeric** | FL, FD, SL, SS, UL, US | Numeric values |
| **Binary** | OB, OD, OF, OL, OW, UN | Binary data |
| **Structured** | AT, SQ | Tags and Sequences |

**VR Features**:
- Automatic padding for fixed-length VRs
- Character set handling (ISO-IR 100, UTF-8)
- Multiplicity support with value delimiter `\`
- Validation of VR constraints (length, format)

### Data Set Operations

**Implementation**: Ordered collection of Data Elements with efficient lookup and manipulation.

**Features**:
- Create, read, update, delete data elements
- Iterate elements in tag order
- Search by tag, keyword, or path expression
- Deep copy and move semantics
- Nested sequence support
- Data set merging with conflict resolution

**Classes**:
- `dicom_dataset` - Main data set container
- `dataset_iterator` - Forward iterator
- `dataset_path` - Path expression for nested access

**Example**:
```cpp
#include <pacs/core/dicom_dataset.h>

using namespace pacs::core;

// Create a data set
dicom_dataset dataset;

// Add elements
dataset.set_string(tags::PatientName, "Doe^John");
dataset.set_string(tags::PatientID, "12345");
dataset.set_string(tags::StudyDate, "20250130");
dataset.set_uint16(tags::Rows, 512);
dataset.set_uint16(tags::Columns, 512);

// Get values
auto name = dataset.get_string(tags::PatientName);
auto rows = dataset.get_uint16(tags::Rows);

// Check existence
if (dataset.contains(tags::PixelData)) {
    auto pixels = dataset.get_bytes(tags::PixelData);
}

// Iterate all elements
for (const auto& element : dataset) {
    std::cout << element.tag() << ": " << element.as_string() << "\n";
}
```

### DICOM File Handling (Part 10)

**Implementation**: Complete DICOM file format support per PS3.10.

**Features**:
- Read DICOM files with 128-byte preamble
- Validate DICM prefix
- Parse File Meta Information (Group 0002)
- Support multiple Transfer Syntaxes
- Write DICOM Part 10 compliant files
- Handle File Meta Information Header

**Classes**:
- `dicom_file` - File reader/writer
- `file_meta_info` - Group 0002 elements
- `transfer_syntax` - Transfer Syntax handling

**Example**:
```cpp
#include <pacs/core/dicom_file.h>

using namespace pacs::core;

// Read DICOM file
auto result = dicom_file::read("/path/to/image.dcm");
if (result.is_ok()) {
    auto& file = result.value();
    auto& dataset = file.dataset();
    auto transfer_syntax = file.transfer_syntax();

    std::cout << "Patient: " << dataset.get_string(tags::PatientName) << "\n";
    std::cout << "Transfer Syntax: " << transfer_syntax.uid() << "\n";
}

// Write DICOM file
dicom_file file;
file.set_dataset(dataset);
file.set_transfer_syntax(transfer_syntax::explicit_vr_little_endian());

auto write_result = file.write("/path/to/output.dcm");
if (write_result.is_err()) {
    std::cerr << "Write failed: " << write_result.error().message << "\n";
}
```

### Transfer Syntax Support

**Supported Transfer Syntaxes**:

| Transfer Syntax | UID | Status |
|-----------------|-----|--------|
| Implicit VR Little Endian | 1.2.840.10008.1.2 | ✅ Implemented |
| Explicit VR Little Endian | 1.2.840.10008.1.2.1 | ✅ Implemented |
| Explicit VR Big Endian | 1.2.840.10008.1.2.2 | ✅ Implemented |
| JPEG Baseline | 1.2.840.10008.1.2.4.50 | ✅ Implemented |
| JPEG Lossless | 1.2.840.10008.1.2.4.70 | ✅ Implemented |
| JPEG 2000 Lossless | 1.2.840.10008.1.2.4.90 | ✅ Implemented |
| JPEG 2000 | 1.2.840.10008.1.2.4.91 | ✅ Implemented |
| RLE Lossless | 1.2.840.10008.1.2.5 | ✅ Implemented |
| JPEG-LS Lossless | 1.2.840.10008.1.2.4.80 | ✅ Implemented |
| JPEG-LS Near-Lossless | 1.2.840.10008.1.2.4.81 | ✅ Implemented |

---

## Network Protocol Features

### Upper Layer Protocol (PDU)

**Implementation**: Complete DICOM Upper Layer Protocol per PS3.8.

**PDU Types Supported**:

| PDU Type | Code | Description |
|----------|------|-------------|
| A-ASSOCIATE-RQ | 0x01 | Association Request |
| A-ASSOCIATE-AC | 0x02 | Association Accept |
| A-ASSOCIATE-RJ | 0x03 | Association Reject |
| P-DATA-TF | 0x04 | Data Transfer |
| A-RELEASE-RQ | 0x05 | Release Request |
| A-RELEASE-RP | 0x06 | Release Response |
| A-ABORT | 0x07 | Abort |

**Classes**:
- `pdu_encoder` - PDU serialization
- `pdu_decoder` - PDU deserialization
- `associate_rq_pdu` - A-ASSOCIATE-RQ structure
- `associate_ac_pdu` - A-ASSOCIATE-AC structure
- `p_data_pdu` - P-DATA-TF structure

### Association Management

**Implementation**: Full association state machine with presentation context negotiation.

**Features**:
- 8-state association state machine (PS3.8 Figure 9-1)
- Presentation context negotiation
- Abstract Syntax and Transfer Syntax matching
- Maximum PDU size negotiation
- Multiple simultaneous associations
- Association timeout handling
- Extended negotiation support

**Association States**:
```
┌──────────────────────────────────────────────────────────┐
│                  Association State Machine                │
├──────────────────────────────────────────────────────────┤
│  Sta1 (Idle) ─────────────────────────────────────────►  │
│       │                                                  │
│       ▼                                                  │
│  Sta2 (Transport Connection Open) ────────────────────►  │
│       │                                                  │
│       ▼                                                  │
│  Sta3 (Awaiting Local A-ASSOCIATE Response) ──────────►  │
│       │                                                  │
│       ▼                                                  │
│  Sta6 (Association Established) ◄─────────────────────►  │
│       │                                                  │
│       ▼                                                  │
│  Sta7 (Awaiting A-RELEASE Response) ──────────────────►  │
│       │                                                  │
│       ▼                                                  │
│  Sta1 (Idle) ◄────────────────────────────────────────── │
└──────────────────────────────────────────────────────────┘
```

**Example**:
```cpp
#include <pacs/network/association.h>

using namespace pacs::network;

// Configure association
association_config config;
config.calling_ae_title = "MY_SCU";
config.called_ae_title = "PACS_SCP";
config.max_pdu_size = 16384;

// Add presentation contexts
config.add_context(
    sop_class::ct_image_storage,
    {transfer_syntax::explicit_vr_little_endian()}
);

// Create association
auto result = association::connect("192.168.1.100", 11112, config);
if (result.is_ok()) {
    auto& assoc = result.value();

    // Perform DIMSE operations...

    assoc.release();  // Graceful release
}
```

### DIMSE Message Exchange

**Implementation**: Complete DIMSE-C and DIMSE-N message support.

**DIMSE-C Services**:

| Service | Status | Description |
|---------|--------|-------------|
| C-ECHO | ✅ Implemented | Verification |
| C-STORE | ✅ Implemented | Storage |
| C-FIND | ✅ Implemented | Query |
| C-MOVE | ✅ Implemented | Retrieve (Move) |
| C-GET | ✅ Implemented | Retrieve (Get) |

**DIMSE-N Services**:

| Service | Status | Description |
|---------|--------|-------------|
| N-CREATE | ✅ Implemented | Create managed SOP instance (MPPS, Print) |
| N-SET | ✅ Implemented | Modify object attributes (MPPS) |
| N-GET | ✅ Implemented | Get attribute values with selective retrieval |
| N-EVENT-REPORT | ✅ Implemented | Event notification (Storage Commitment) |
| N-ACTION | ✅ Implemented | Action request (Storage Commitment) |
| N-DELETE | ✅ Implemented | Delete managed SOP instance (Print) |

---

## DICOM Services

### Verification Service (C-ECHO)

**Implementation**: DICOM Verification SOP Class for connectivity testing.

**Features**:
- Verification SCP (responder)
- Verification SCU (initiator)
- Connection health checking
- Association validation

**SOP Class**: Verification SOP Class (1.2.840.10008.1.1)

**Example**:
```cpp
#include <pacs/services/verification_scu.h>

using namespace pacs::services;

// Test connectivity
verification_scu scu("MY_SCU");
auto result = scu.echo("PACS_SCP", "192.168.1.100", 11112);

if (result.is_ok()) {
    std::cout << "DICOM Echo successful!\n";
} else {
    std::cerr << "Echo failed: " << result.error().message << "\n";
}
```

### Storage Service (C-STORE)

**Implementation**: Storage SCP/SCU for receiving and sending DICOM images.

**Features**:
- Receive images from modalities (SCP)
- Send images to PACS/viewers (SCU)
- Multiple storage SOP classes
- Concurrent image reception
- Duplicate detection
- Storage commitment (planned)

**Supported SOP Classes**:

| SOP Class | UID | Status |
|-----------|-----|--------|
| CT Image Storage | 1.2.840.10008.5.1.4.1.1.2 | ✅ |
| MR Image Storage | 1.2.840.10008.5.1.4.1.1.4 | ✅ |
| CR Image Storage | 1.2.840.10008.5.1.4.1.1.1 | ✅ |
| DX Image Storage | 1.2.840.10008.5.1.4.1.1.1.1 | ✅ |
| Secondary Capture | 1.2.840.10008.5.1.4.1.1.7 | ✅ |
| US Image Storage | 1.2.840.10008.5.1.4.1.1.6.1 | ✅ |
| US Multi-frame Image Storage | 1.2.840.10008.5.1.4.1.1.6.2 | ✅ |
| XA Image Storage | 1.2.840.10008.5.1.4.1.1.12.1 | ✅ |
| Enhanced XA Image Storage | 1.2.840.10008.5.1.4.1.1.12.1.1 | ✅ |
| XRF Image Storage | 1.2.840.10008.5.1.4.1.1.12.2 | ✅ |
| X-Ray 3D Angiographic Image Storage | 1.2.840.10008.5.1.4.1.1.13.1.1 | ✅ |
| X-Ray 3D Craniofacial Image Storage | 1.2.840.10008.5.1.4.1.1.13.1.2 | ✅ |
| NM Image Storage | 1.2.840.10008.5.1.4.1.1.20 | ✅ |
| NM Image Storage (Retired) | 1.2.840.10008.5.1.4.1.1.5 | ✅ |
| PET Image Storage | 1.2.840.10008.5.1.4.1.1.128 | ✅ |
| Enhanced PET Image Storage | 1.2.840.10008.5.1.4.1.1.130 | ✅ |
| Legacy Converted Enhanced PET Image Storage | 1.2.840.10008.5.1.4.1.1.128.1 | ✅ |
| RT Plan Storage | 1.2.840.10008.5.1.4.1.1.481.5 | ✅ |
| RT Dose Storage | 1.2.840.10008.5.1.4.1.1.481.2 | ✅ |
| RT Structure Set Storage | 1.2.840.10008.5.1.4.1.1.481.3 | ✅ |
| RT Image Storage | 1.2.840.10008.5.1.4.1.1.481.1 | ✅ |
| RT Beams Treatment Record Storage | 1.2.840.10008.5.1.4.1.1.481.4 | ✅ |
| RT Brachy Treatment Record Storage | 1.2.840.10008.5.1.4.1.1.481.6 | ✅ |
| RT Treatment Summary Record Storage | 1.2.840.10008.5.1.4.1.1.481.7 | ✅ |
| RT Ion Plan Storage | 1.2.840.10008.5.1.4.1.1.481.8 | ✅ |
| RT Ion Beams Treatment Record Storage | 1.2.840.10008.5.1.4.1.1.481.9 | ✅ |
| Segmentation Storage | 1.2.840.10008.5.1.4.1.1.66.4 | ✅ |
| Surface Segmentation Storage | 1.2.840.10008.5.1.4.1.1.66.5 | ✅ |
| Basic Text SR Storage | 1.2.840.10008.5.1.4.1.1.88.11 | ✅ |
| Enhanced SR Storage | 1.2.840.10008.5.1.4.1.1.88.22 | ✅ |
| Comprehensive SR Storage | 1.2.840.10008.5.1.4.1.1.88.33 | ✅ |
| Comprehensive 3D SR Storage | 1.2.840.10008.5.1.4.1.1.88.34 | ✅ |
| Extensible SR Storage | 1.2.840.10008.5.1.4.1.1.88.35 | ✅ |
| Key Object Selection Document Storage | 1.2.840.10008.5.1.4.1.1.88.59 | ✅ |
| Mammography CAD SR Storage | 1.2.840.10008.5.1.4.1.1.88.50 | ✅ |
| Chest CAD SR Storage | 1.2.840.10008.5.1.4.1.1.88.65 | ✅ |
| Colon CAD SR Storage | 1.2.840.10008.5.1.4.1.1.88.69 | ✅ |
| X-Ray Radiation Dose SR Storage | 1.2.840.10008.5.1.4.1.1.88.67 | ✅ |

**Example**:
```cpp
#include <pacs/services/storage_scp.h>

using namespace pacs::services;

// Configure Storage SCP
storage_scp_config config;
config.ae_title = "PACS_SCP";
config.port = 11112;
config.storage_path = "/data/dicom";
config.max_associations = 10;

// Start server
storage_scp server(config);
server.set_handler([](const dicom_dataset& dataset) {
    std::cout << "Received: "
              << dataset.get_string(tags::SOPInstanceUID) << "\n";
    return storage_status::success;
});

auto result = server.start();
if (result.is_ok()) {
    server.wait_for_shutdown();
}
```

### Query/Retrieve Service (C-FIND, C-MOVE)

**Implementation**: Query and retrieve images based on Patient/Study/Series/Image hierarchy.

**Query Levels**:

| Level | Key Attributes | Optional Attributes |
|-------|---------------|---------------------|
| Patient | PatientID | PatientName, PatientBirthDate |
| Study | StudyInstanceUID | StudyDate, Modality, AccessionNumber |
| Series | SeriesInstanceUID | SeriesNumber, SeriesDescription |
| Image | SOPInstanceUID | InstanceNumber |

**Information Models**:
- Patient Root Query/Retrieve Information Model
- Study Root Query/Retrieve Information Model

**Example**:
```cpp
#include <pacs/services/query_scu.h>

using namespace pacs::services;

// Query for studies
query_scu scu("MY_SCU");
auto assoc = scu.connect("PACS_SCP", "192.168.1.100", 11112);

// Build query
dicom_dataset query;
query.set_string(tags::PatientName, "Doe*");
query.set_string(tags::StudyDate, "20250101-20250130");
query.set_string(tags::QueryRetrieveLevel, "STUDY");

// Execute query
auto results = scu.find(assoc, query);
for (const auto& study : results) {
    std::cout << "Study: " << study.get_string(tags::StudyInstanceUID) << "\n";
}
```

### Modality Worklist Service

**Implementation**: Query scheduled procedures from RIS/HIS systems.

**Features**:
- Scheduled Procedure Step information
- Patient demographics
- Scheduled station AE Title filtering
- Date range queries

**SOP Class**: Modality Worklist Information Model (1.2.840.10008.5.1.4.31)

### MPPS Service

**Implementation**: Track procedure execution status.

**Features**:
- Create MPPS instance (N-CREATE)
- Update procedure status (N-SET)
- Track IN PROGRESS, COMPLETED, DISCONTINUED states
- Link performed series to scheduled procedures

**SOP Class**: Modality Performed Procedure Step (1.2.840.10008.3.1.2.3.3)

---

## Storage Backend

### File System Storage

**Implementation**: Hierarchical file storage with configurable naming.

**Features**:
- Configurable directory structure
- Patient/Study/Series/Image hierarchy
- Atomic file operations
- Duplicate detection
- File locking for concurrent access

**Directory Structure Options**:
```
# Option 1: UID-based (default)
/storage/
  └── {StudyInstanceUID}/
      └── {SeriesInstanceUID}/
          └── {SOPInstanceUID}.dcm

# Option 2: Date-based
/storage/
  └── {YYYY}/
      └── {MM}/
          └── {DD}/
              └── {SOPInstanceUID}.dcm
```

### Index Database

**Implementation**: SQLite-based index for fast queries.

**Schema**:
```sql
-- Patient table
CREATE TABLE patient (
    patient_id TEXT PRIMARY KEY,
    patient_name TEXT,
    patient_birth_date TEXT,
    patient_sex TEXT
);

-- Study table
CREATE TABLE study (
    study_instance_uid TEXT PRIMARY KEY,
    patient_id TEXT REFERENCES patient(patient_id),
    study_date TEXT,
    study_time TEXT,
    accession_number TEXT,
    study_description TEXT,
    modalities_in_study TEXT
);

-- Series table
CREATE TABLE series (
    series_instance_uid TEXT PRIMARY KEY,
    study_instance_uid TEXT REFERENCES study(study_instance_uid),
    series_number INTEGER,
    modality TEXT,
    series_description TEXT
);

-- Instance table
CREATE TABLE instance (
    sop_instance_uid TEXT PRIMARY KEY,
    series_instance_uid TEXT REFERENCES series(series_instance_uid),
    sop_class_uid TEXT,
    instance_number INTEGER,
    file_path TEXT
);
```

### S3 Cloud Storage (Mock Implementation)

**Implementation**: S3-compatible cloud storage backend with mock client for testing.

**Features**:
- AWS S3 and S3-compatible storage (MinIO, etc.)
- Hierarchical object key structure (Study/Series/SOP)
- Multipart upload support for large files (placeholder)
- Progress callbacks for upload/download monitoring
- Thread-safe operations with shared_mutex
- Configurable connection and timeout settings

**Configuration**:
```cpp
#include <pacs/storage/s3_storage.hpp>

using namespace pacs::storage;

cloud_storage_config config;
config.bucket_name = "my-dicom-bucket";
config.region = "us-east-1";
config.access_key_id = "AKIAIOSFODNN7EXAMPLE";
config.secret_access_key = "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY";

// For MinIO local testing
config.endpoint_url = "http://localhost:9000";

// Multipart upload settings
config.multipart_threshold = 100 * 1024 * 1024;  // 100MB
config.part_size = 10 * 1024 * 1024;  // 10MB
```

**Usage Example**:
```cpp
s3_storage storage{config};

// Store DICOM dataset
core::dicom_dataset ds;
// ... populate dataset with UIDs ...

auto store_result = storage.store(ds);
if (store_result.is_ok()) {
    std::cout << "Stored successfully\n";
}

// Retrieve by SOP Instance UID
auto retrieve_result = storage.retrieve("1.2.3.4.5.6.7.8.9");
if (retrieve_result.is_ok()) {
    auto& dataset = retrieve_result.value();
    std::cout << "Patient: " << dataset.get_string(tags::patient_name) << "\n";
}

// Store with progress tracking
auto progress = [](std::size_t transferred, std::size_t total) -> bool {
    std::cout << "Progress: " << (100 * transferred / total) << "%\n";
    return true;  // Continue upload
};
storage.store_with_progress(ds, progress);
```

**Object Key Structure**:
```
{bucket}/
  └── {StudyInstanceUID}/
      └── {SeriesInstanceUID}/
          └── {SOPInstanceUID}.dcm
```

**Note**: This is currently a mock implementation for API validation and testing. Full AWS SDK C++ integration will be added in a future release.

### Azure Blob Storage (Mock Implementation)

**Implementation**: Azure Blob storage backend with mock client for testing.

**Features**:
- Azure Blob Storage container support
- Block blob upload for large files (parallel block staging)
- Access tier management (Hot, Cool, Archive)
- Progress callbacks for upload/download monitoring
- Thread-safe operations with shared_mutex
- Azurite emulator support for local testing
- Configurable connection and timeout settings

**Configuration**:
```cpp
#include <pacs/storage/azure_blob_storage.hpp>

using namespace pacs::storage;

azure_storage_config config;
config.container_name = "dicom-container";
config.connection_string = "DefaultEndpointsProtocol=https;AccountName=...";

// For Azurite local testing
config.endpoint_url = "http://127.0.0.1:10000/devstoreaccount1";

// Block blob upload settings
config.block_upload_threshold = 100 * 1024 * 1024;  // 100MB
config.block_size = 4 * 1024 * 1024;  // 4MB

// Access tier
config.access_tier = "Hot";  // Hot, Cool, or Archive
```

**Usage Example**:
```cpp
azure_blob_storage storage{config};

// Store DICOM dataset
core::dicom_dataset ds;
// ... populate dataset with UIDs ...

auto store_result = storage.store(ds);
if (store_result.is_ok()) {
    std::cout << "Stored successfully\n";
}

// Retrieve by SOP Instance UID
auto retrieve_result = storage.retrieve("1.2.3.4.5.6.7.8.9");
if (retrieve_result.is_ok()) {
    auto& dataset = retrieve_result.value();
    std::cout << "Patient: " << dataset.get_string(tags::patient_name) << "\n";
}

// Change access tier for archival
storage.set_access_tier("1.2.3.4.5.6.7.8.9", "Archive");

// Store with progress tracking
auto progress = [](std::size_t transferred, std::size_t total) -> bool {
    std::cout << "Progress: " << (100 * transferred / total) << "%\n";
    return true;  // Continue upload
};
storage.store_with_progress(ds, progress);
```

**Blob Naming Structure**:
```
{container}/
  └── {StudyInstanceUID}/
      └── {SeriesInstanceUID}/
          └── {SOPInstanceUID}.dcm
```

**Note**: This is currently a mock implementation for API validation and testing. Full Azure SDK C++ integration will be added in a future release.

### Hierarchical Storage Management (HSM)

**Implementation**: Three-tier storage system with automatic age-based data migration between tiers.

**Features**:
- Three storage tiers: Hot (fast access), Warm (medium), Cold (archival)
- Automatic background migration based on configurable age policies
- Transparent data retrieval across all tiers
- Statistics tracking and monitoring
- Thread-safe operations with shared_mutex
- Progress and error callbacks for migration operations
- Integration with thread_system for parallel migrations

**Tier Characteristics**:

| Tier | Use Case | Typical Backend | Access Pattern |
|------|----------|-----------------|----------------|
| Hot | Active studies | Local SSD, File System | Frequent read/write |
| Warm | Recent archives | Network storage, S3 | Occasional access |
| Cold | Long-term archive | Azure Archive, Glacier | Rare access |

**Configuration**:
```cpp
#include <pacs/storage/hsm_storage.hpp>
#include <pacs/storage/hsm_types.hpp>

using namespace pacs::storage;

// Configure tier policies
tier_policy hot_policy;
hot_policy.tier = storage_tier::hot;
hot_policy.migration_age = std::chrono::days{30};   // Migrate after 30 days

tier_policy warm_policy;
warm_policy.tier = storage_tier::warm;
warm_policy.migration_age = std::chrono::days{180}; // Migrate after 180 days

tier_policy cold_policy;
cold_policy.tier = storage_tier::cold;
// No migration from cold tier (final destination)

// Create HSM storage with backends
auto hot_backend = std::make_shared<file_storage>(hot_path);
auto warm_backend = std::make_shared<s3_storage>(warm_config);
auto cold_backend = std::make_shared<azure_blob_storage>(cold_config);

hsm_storage storage{
    hot_backend, hot_policy,
    warm_backend, warm_policy,
    cold_backend, cold_policy
};
```

**Usage Example**:
```cpp
// Store always goes to hot tier
core::dicom_dataset ds;
// ... populate dataset ...
auto store_result = storage.store(ds);

// Retrieve transparently searches all tiers
auto retrieve_result = storage.retrieve("1.2.3.4.5.6.7.8.9");
if (retrieve_result.is_ok()) {
    auto& dataset = retrieve_result.value();
    std::cout << "Patient: " << dataset.get_string(tags::patient_name) << "\n";
}

// Check which tier contains the instance
auto tier_result = storage.get_tier("1.2.3.4.5.6.7.8.9");
if (tier_result.is_ok()) {
    std::cout << "Stored in: " << to_string(tier_result.value()) << "\n";
}

// Manual migration to specific tier
storage.migrate("1.2.3.4.5.6.7.8.9", storage_tier::cold);

// Get storage statistics
auto stats = storage.get_statistics();
std::cout << "Hot tier: " << stats.hot_count << " instances\n";
std::cout << "Warm tier: " << stats.warm_count << " instances\n";
std::cout << "Cold tier: " << stats.cold_count << " instances\n";
```

**Background Migration Service**:
```cpp
#include <pacs/storage/hsm_migration_service.hpp>

// Configure migration service
migration_service_config config;
config.migration_interval = std::chrono::hours{1};  // Run every hour
config.max_concurrent_migrations = 4;
config.auto_start = true;

config.on_cycle_complete = [](const migration_result& r) {
    std::cout << "Migrated " << r.instances_migrated << " instances\n";
    std::cout << "Bytes transferred: " << r.bytes_migrated << "\n";
};

config.on_migration_error = [](const std::string& uid, const std::string& err) {
    std::cerr << "Migration failed for " << uid << ": " << err << "\n";
};

// Create and start migration service
hsm_migration_service service{storage, config};
service.start();

// Monitor progress
auto time_left = service.time_until_next_cycle();
std::cout << "Next cycle in: " << time_left->count() << " seconds\n";

// Trigger immediate migration
service.trigger_cycle();

// Get statistics
auto stats = service.get_cumulative_stats();
std::cout << "Total migrated: " << stats.instances_migrated << "\n";

// Graceful shutdown
service.stop();
```

**Migration Result Structure**:
```cpp
struct migration_result {
    std::size_t instances_migrated{0};   // Successfully migrated
    std::size_t bytes_migrated{0};       // Total bytes transferred
    std::chrono::milliseconds duration;  // Time taken
    std::size_t instances_skipped{0};    // Not yet eligible
    std::vector<std::string> failed_uids; // Failed migrations
};
```

---

## Ecosystem Integration

### container_system Integration

**Purpose**: DICOM data serialization and type-safe value handling.

**Integration Points**:
- VR types mapped to container value types
- Binary serialization for DICOM encoding
- Nested containers for SQ (Sequence) elements
- SIMD acceleration for large arrays

**Example**:
```cpp
#include <pacs/integration/container_adapter.h>

// Convert DICOM dataset to container
auto container = container_adapter::to_container(dataset);

// Serialize
auto binary = container->serialize();

// Convert back
auto restored_dataset = container_adapter::from_container(container);
```

### network_system Integration

**Purpose**: TCP/TLS transport for DICOM communication.

**Integration Points**:
- `messaging_server` → DICOM SCP
- `messaging_client` → DICOM SCU
- `messaging_session` → DICOM Association
- TLS support for secure DICOM

### thread_system Integration

**Purpose**: Concurrent processing of DICOM operations.

**Status:** ✅ **Fully Migrated (v1.1.0)** - See [MIGRATION_COMPLETE.md](MIGRATION_COMPLETE.md)

**Integration Points**:
- `accept_worker` (inherits from `thread_base`) for accept loop with jthread support
- `thread_adapter` pool for association worker management with load balancing
- `cancellation_token` for cooperative graceful shutdown
- Unified thread statistics and monitoring

**Migration Benefits (Epic #153):**

| Metric | Before | After | Improvement |
|--------|--------|-------|-------------|
| Graceful shutdown | ~5,000 ms | 110 ms | 45x faster |
| Thread management | Manual lifecycle | Automatic jthread | Zero leaks |
| Monitoring | None | Unified statistics | Full visibility |
| Cancellation | Not supported | cancellation_token | Cooperative |

**Example (v1.1.0+):**
```cpp
#include <pacs/network/dicom_server.hpp>

// Server now uses thread_system internally
dicom_server server{config};
server.register_service(std::make_unique<verification_scp>());
server.start();

// Graceful shutdown with timeout (uses cancellation_token)
server.stop(std::chrono::seconds{5});  // Typically completes in ~110ms
```

### network_system V2 Integration (Optional)

**Purpose**: Alternative DICOM server using `messaging_server` for TCP management.

**Status:** ✅ **Available (Optional)**

**Compile Flag:** `PACS_WITH_NETWORK_SYSTEM`

**Components:**
- `dicom_server_v2` - Uses `messaging_server` for connection management
- `dicom_association_handler` - Per-session PDU framing and dispatching

**Example:**
```cpp
#include <pacs/network/v2/dicom_server_v2.hpp>

// Same API, different implementation
dicom_server_v2 server{config};
server.register_service(std::make_unique<verification_scp>());
server.start();
```

### logger_system Integration

**Purpose**: Comprehensive logging and audit trail.

**Log Categories**:
- Association events
- DIMSE operations
- Storage operations
- Error conditions
- Security events (audit)

### monitoring_system Integration

**Purpose**: Performance metrics and health monitoring.

**Metrics**:
- Active associations count
- Images stored per minute
- Query response latency
- Storage throughput (MB/s)
- Error rates

---

## Security Features

### TLS Support

**Implementation**: Secure DICOM communication per PS3.15.

**Features**:
- TLS 1.2 and TLS 1.3
- Modern cipher suites
- Certificate validation
- Mutual authentication (optional)

### Access Control (RBAC)

**Documentation**: [SECURITY.md](SECURITY.md)

**Features**:
- **Role-Based Access Control**: Pre-defined roles (Viewer, Technologist, Radiologist, Administrator).
- **Granular Permissions**: Resource-level control (Study, Metadata, System, Audit).
- **User Management**: Creating users and assigning roles via API.
- **API Enforcement**: Secure REST API endpoints with permission checks.
- **AE Title Security**: Association whitelisting (existing).

### Audit Logging

**Features**:
- All DICOM operations logged
- PHI access tracking
- HIPAA-compliant audit trail
- Tamper-evident logging

---

## Monitoring and Observability

### Health Checks

**Endpoints**:
- `/health` - Basic health status
- `/ready` - Readiness probe
- `/live` - Liveness probe

### Metrics

**Prometheus-compatible metrics**:
```
# Association metrics
pacs_associations_active{ae_title="PACS_SCP"}
pacs_associations_total{ae_title="PACS_SCP"}

# Storage metrics
pacs_images_stored_total{sop_class="CT"}
pacs_storage_bytes_total{sop_class="CT"}
pacs_storage_latency_seconds{quantile="0.95"}

# Query metrics
pacs_queries_total{level="STUDY"}
pacs_query_latency_seconds{quantile="0.95"}
```

### Distributed Tracing

**Span Types**:
- Association lifecycle
- DIMSE operation
- Storage operation
- Database query

### REST API Server

**Implementation**: REST API server using Crow web framework for PACS administration and monitoring.

**Features**:
- System health status and metrics endpoints
- Configuration management
- CORS support for web browser access
- Integration with `pacs_monitoring` module

**Classes**:
- `rest_server` - Main server class with async/sync modes
- `rest_server_config` - Configuration (port, CORS, TLS options)
- `rest_server_context` - Shared context for endpoints

**Endpoints**:

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/v1/system/status` | GET | System health status (via health_checker) |
| `/api/v1/system/metrics` | GET | Performance metrics |
| `/api/v1/system/config` | GET | Current server configuration |
| `/api/v1/system/config` | PUT | Update configuration |
| `/api/v1/system/version` | GET | API version info |

**Example**:
```cpp
#include <pacs/web/rest_server.hpp>

using namespace pacs::web;

// Configure and start REST server
rest_server_config config;
config.port = 8080;
config.concurrency = 4;
config.enable_cors = true;

rest_server server(config);
server.set_health_checker(health_checker_instance);
server.start_async();  // Non-blocking

// Later: graceful shutdown
server.stop();
```

---

## Workflow Services

### Auto Prefetch Service

**Implementation**: Background service for automatically prefetching prior patient studies from remote PACS when patients appear in the modality worklist.

**Features**:
- Worklist-triggered prefetch: Automatically queues prefetch requests when patients appear in MWL
- Configurable selection criteria: Filter priors by modality, body part, lookback period
- Multi-source support: Can prefetch from multiple remote PACS servers
- Parallel processing: Uses thread_pool for concurrent prefetch operations
- Rate limiting: Prevents overloading remote PACS with requests
- Retry logic: Automatically retries failed prefetches with configurable delay
- Deduplication: Prevents duplicate prefetch requests for the same patient

**Classes**:
- `auto_prefetch_service` - Main service class for background prefetching
- `prefetch_service_config` - Service configuration options
- `prefetch_criteria` - Selection criteria for prior studies
- `prefetch_result` - Statistics for prefetch operations
- `prior_study_info` - Information about a prior study candidate
- `prefetch_request` - Request for prefetching patient priors

**Configuration Options**:

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `enabled` | bool | true | Enable/disable prefetch service |
| `prefetch_interval` | seconds | 300 | Interval between prefetch cycles |
| `max_concurrent_prefetches` | size_t | 4 | Maximum parallel prefetch operations |
| `lookback_period` | days | 365 | How far back to look for prior studies |
| `max_studies_per_patient` | size_t | 10 | Maximum priors per patient |
| `prefer_same_modality` | bool | true | Prefer priors with same modality |
| `rate_limit_per_minute` | size_t | 0 | Max prefetches per minute (0=unlimited) |

**Example**:
```cpp
#include <pacs/workflow/auto_prefetch_service.hpp>
#include <pacs/workflow/prefetch_config.hpp>

using namespace pacs::workflow;

// Configure prefetch service
prefetch_service_config config;
config.prefetch_interval = std::chrono::minutes{5};
config.max_concurrent_prefetches = 4;
config.criteria.lookback_period = std::chrono::days{365};
config.criteria.max_studies_per_patient = 10;
config.criteria.prefer_same_modality = true;

// Add remote PACS sources
remote_pacs_config remote;
remote.ae_title = "ARCHIVE_PACS";
remote.host = "192.168.1.100";
remote.port = 11112;
config.remote_pacs.push_back(remote);

// Set callbacks
config.on_cycle_complete = [](const prefetch_result& result) {
    std::cout << "Prefetched " << result.studies_prefetched
              << " studies for " << result.patients_processed << " patients\n";
};

// Create and start service
auto_prefetch_service service{database, config};
service.start();

// Trigger prefetch for worklist items
service.trigger_for_worklist(worklist_items);

// Manual prefetch for a specific patient
auto result = service.prefetch_priors("PATIENT123", std::chrono::days{180});

// Monitor statistics
auto stats = service.get_cumulative_stats();
std::cout << "Total prefetched: " << stats.studies_prefetched << "\n";

// Stop service
service.stop();
```

**Integration Points**:
- **MWL SCP**: Automatically notified when worklist queries complete
- **index_database**: Checks local storage before initiating prefetch
- **thread_pool**: Parallel C-MOVE operations
- **logger_adapter**: Audit logging for prefetch operations
- **monitoring_adapter**: Metrics for prefetch success/failure rates

### Task Scheduler Service

**Implementation**: Background service for scheduling and executing automated maintenance tasks including cleanup, archive, and data verification.

**Features**:
- Flexible scheduling: Interval-based, cron-like expressions, and one-time execution
- Built-in task types: Cleanup, Archive, Verification, Custom
- Task lifecycle management: Pause, Resume, Cancel, Trigger
- Execution history tracking with configurable retention
- Task persistence for recovery across restarts
- Statistics and monitoring capabilities
- Concurrent execution with configurable limits

**Classes**:
- `task_scheduler` - Main service class for scheduling and execution
- `task_scheduler_config` - Service configuration options
- `cleanup_config` - Configuration for storage cleanup tasks
- `archive_config` - Configuration for study archival tasks
- `verification_config` - Configuration for data integrity verification
- `scheduled_task` - Task definition with schedule and callbacks
- `cron_schedule` - Cron-like schedule expression

**Schedule Types**:

| Type | Description | Example |
|------|-------------|---------|
| `interval_schedule` | Fixed interval between executions | Every 1 hour |
| `cron_schedule` | Cron-like schedule with minute/hour/day | Daily at 2:00 AM |
| `one_time_schedule` | Single execution at specific time | 2025-12-15 03:00 |

**Task Types**:

| Type | Purpose | Configuration |
|------|---------|---------------|
| Cleanup | Remove old studies based on retention policy | `cleanup_config` |
| Archive | Move studies to archive storage | `archive_config` |
| Verification | Check data integrity (checksums, DB consistency) | `verification_config` |
| Custom | User-defined maintenance tasks | Custom callback |

**Configuration Options**:

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `enabled` | bool | true | Enable/disable scheduler |
| `auto_start` | bool | false | Start on construction |
| `max_concurrent_tasks` | size_t | 4 | Maximum parallel task executions |
| `check_interval` | seconds | 60 | Interval for checking due tasks |
| `persistence_path` | string | "" | Path for task persistence (empty=disabled) |

**Example**:
```cpp
#include <pacs/workflow/task_scheduler.hpp>
#include <pacs/workflow/task_scheduler_config.hpp>

using namespace pacs::workflow;

// Configure scheduler
task_scheduler_config config;
config.max_concurrent_tasks = 4;
config.check_interval = std::chrono::seconds{60};

// Configure cleanup task
cleanup_config cleanup;
cleanup.default_retention = std::chrono::days{365};
cleanup.modality_retention["CR"] = std::chrono::days{180};
cleanup.modality_retention["CT"] = std::chrono::days{730};
cleanup.cleanup_schedule = cron_schedule::daily_at(2, 0);  // 2:00 AM
config.cleanup = cleanup;

// Configure archive task
archive_config archive;
archive.archive_after = std::chrono::days{90};
archive.destination_type = archive_destination_type::cloud_s3;
archive.destination = "s3://bucket/archive";
archive.verify_after_archive = true;
archive.archive_schedule = cron_schedule::daily_at(3, 0);  // 3:00 AM
config.archive = archive;

// Configure verification task
verification_config verification;
verification.check_checksums = true;
verification.check_db_consistency = true;
verification.verification_schedule = cron_schedule::weekly_on(0, 4, 0);  // Sunday 4:00 AM
config.verification = verification;

// Set callbacks
config.on_task_complete = [](const task_id& id, const task_execution_record& record) {
    std::cout << "Task " << id << " completed in "
              << record.duration()->count() << "ms\n";
};

config.on_task_error = [](const task_id& id, const std::string& error) {
    std::cerr << "Task " << id << " failed: " << error << "\n";
};

// Create and start scheduler
task_scheduler scheduler{database, config};
scheduler.start();

// Add custom task
auto custom_id = scheduler.schedule(
    "daily-backup",
    "Database Backup",
    cron_schedule::daily_at(1, 0),  // 1:00 AM
    []() -> std::optional<std::string> {
        // Perform backup
        return std::nullopt;  // Success
    }
);

// Task management
scheduler.pause_task(custom_id);
scheduler.resume_task(custom_id);
scheduler.trigger_task(custom_id);  // Execute immediately

// Get task info
auto task = scheduler.get_task(custom_id);
if (task) {
    std::cout << "Task: " << task->name << "\n";
    std::cout << "State: " << to_string(task->state) << "\n";
    std::cout << "Executions: " << task->execution_count << "\n";
}

// List all tasks
for (const auto& t : scheduler.list_tasks()) {
    std::cout << t.id << ": " << t.name << " [" << to_string(t.state) << "]\n";
}

// Get statistics
auto stats = scheduler.get_stats();
std::cout << "Scheduled: " << stats.scheduled_tasks << "\n";
std::cout << "Running: " << stats.running_tasks << "\n";
std::cout << "Total executions: " << stats.total_executions << "\n";

// Graceful shutdown
scheduler.stop(true);  // Wait for running tasks
```

**Cron Schedule Helpers**:
```cpp
// Every 15 minutes
auto every_15_min = cron_schedule::every_minutes(15);

// Every 4 hours
auto every_4_hours = cron_schedule::every_hours(4);

// Daily at 3:30 AM
auto daily = cron_schedule::daily_at(3, 30);

// Weekly on Sunday at 2:00 AM
auto weekly = cron_schedule::weekly_on(0, 2, 0);

// Parse cron expression
auto custom = cron_schedule::parse("0 2 * * 1-5");  // 2:00 AM weekdays
```

**Integration Points**:
- **index_database**: Query studies for cleanup/archive eligibility
- **file_storage**: Delete or archive DICOM files
- **thread_pool**: Parallel task execution
- **logger_adapter**: Audit logging for all task operations
- **monitoring_adapter**: Metrics for task success/failure rates

---

## Recently Completed Features (v1.2.0 - 2025-12-13)

| Feature | Description | Issue | Status |
|---------|-------------|-------|--------|
| Task Scheduler Service | Automated task scheduling for cleanup, archive, and verification | #207 | ✅ Complete |
| Auto Prefetch Service | Automatic prior study prefetch based on worklist queries | #206 | ✅ Complete |
| Hierarchical Storage Management | Three-tier HSM with automatic age-based migration | #200 | ✅ Complete |
| Azure Blob Storage | Azure Blob storage backend (mock implementation) with block blob upload | #199 | ✅ Complete |
| Segmentation (SEG) and Structured Report (SR) | SEG/SR SOP classes for AI/CAD outputs with IOD validation | #187 | ✅ Complete |
| Radiation Therapy (RT) Modality | RT Plan, RT Dose, RT Structure Set support with IOD validation | #186 | ✅ Complete |
| RLE Lossless Codec | RLE compression codec (pure C++) | #182 | ✅ Complete |
| REST API Server | Web administration API with Crow framework | #194 | ✅ Complete |
| S3 Cloud Storage | S3-compatible storage backend (mock implementation) | #198 | ✅ Complete |
| Thread System Migration | std::thread → thread_system | #153 | ✅ Complete |
| Network System V2 | Optional messaging_server integration | #163 | ✅ Complete |
| DIMSE-N Services | N-GET, N-ACTION, N-EVENT-REPORT, N-DELETE | #127 | ✅ Complete |
| Ultrasound Storage | US, US-MF SOP classes | #128 | ✅ Complete |
| XA Image Storage | XA, Enhanced XA, XRF SOP classes | #129 | ✅ Complete |
| Explicit VR Big Endian | Transfer syntax support | #126 | ✅ Complete |

---

## Planned Features

### Short Term (Next Release)

| Feature | Description | Target |
|---------|-------------|--------|
| ~~Additional SOP Classes (PET/NM)~~ | ~~NM, PET support~~ | ✅ Complete |
| ~~Additional SOP Classes (RT)~~ | ~~RT Plan, RT Structure Set, etc.~~ | ✅ Complete |
| Connection Pooling | Reuse associations | Phase 3 |
| Enhanced Metrics | Per-association timing | Phase 3 |

### Medium Term

| Feature | Description | Target |
|---------|-------------|--------|
| WADO-RS | DICOMweb support | Future |
| Clustering | Multi-node PACS | Future |

### Long Term

| Feature | Description | Target |
|---------|-------------|--------|
| AI Integration | Inference pipeline | Future |
| Cloud Storage (Full AWS SDK) | Production AWS S3 integration | Future |
| Cloud Storage (Full Azure SDK) | Production Azure Blob Storage integration | Future |
| FHIR Integration | Healthcare interop | Future |

---

## Document History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0.0 | 2025-11-30 | kcenon | Initial release |
| 1.1.0 | 2025-12-04 | kcenon | Updated DIMSE status |
| 1.2.0 | 2025-12-07 | kcenon | Added: Thread migration, Network V2, DIMSE-N, Ultrasound, XA; Updated ecosystem integration |
| 1.3.0 | 2025-12-09 | raphaelshin | Added: S3 Cloud Storage (mock implementation) for Issue #198 |
| 1.4.0 | 2025-12-09 | raphaelshin | Added: REST API Server foundation with Crow framework for Issue #194 |
| 1.5.0 | 2025-12-09 | raphaelshin | Added: RLE Lossless codec for Issue #182; Updated compression codec status |
| 1.6.0 | 2025-12-10 | raphaelshin | Added: RT modality support (RT Plan, RT Dose, RT Structure Set) with IOD validation for Issue #186 |
| 1.7.0 | 2025-12-10 | raphaelshin | Added: SEG/SR support (Segmentation, Structured Reports) for AI/CAD integration for Issue #187 |
| 1.8.0 | 2025-12-12 | raphaelshin | Added: Azure Blob Storage (mock implementation) with block blob upload for Issue #199 |
| 1.9.0 | 2025-12-12 | raphaelshin | Added: Hierarchical Storage Management (HSM) with three-tier storage and automatic migration for Issue #200 |
| 2.0.0 | 2025-12-13 | raphaelshin | Added: Auto Prefetch Service for worklist-triggered prior study prefetch for Issue #206 |
| 2.1.0 | 2025-12-13 | raphaelshin | Added: Task Scheduler Service for automated cleanup, archive, and verification for Issue #207 |

---

*Document Version: 0.2.0.0*
*Created: 2025-11-30*
*Updated: 2025-12-13*
*Author: kcenon@naver.com*

