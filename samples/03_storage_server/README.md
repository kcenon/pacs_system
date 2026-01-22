# Level 3: Storage Server

A DICOM Storage Server (Storage SCP) sample demonstrating image reception, file storage, and database indexing. This builds on Level 2 concepts and introduces the core PACS archiving functionality.

## Learning Objectives

By completing this sample, you will understand:

- **Storage SCP** - Handling C-STORE requests from modalities
- **File Storage** - Organizing DICOM files in hierarchical directories
- **Index Database** - SQLite-based metadata storage for fast queries
- **Pre-store Validation** - Rejecting invalid or incomplete images
- **Post-store Hooks** - Triggering workflows after successful storage

## Prerequisites

- Completed Level 2: Echo Server
- C++20 compatible compiler (GCC 11+, Clang 14+, MSVC 2022+)
- pacs_system library built with samples enabled
- DCMTK installed (for testing with `storescu`)

## Build & Run

```bash
# Configure with samples enabled
cmake -B build -DPACS_BUILD_SAMPLES=ON

# Build this sample
cmake --build build --target sample_03_storage_server

# Run the server (default port 11112)
./build/samples/storage_server

# Or specify a custom port
./build/samples/storage_server 11113
```

## Testing

### Generate Test Data

First, create a test DICOM file using the Level 1 sample:

```bash
# Run Level 1 sample to generate hello_dicom_output.dcm
./build/samples/hello_dicom
```

### Send Images to Storage Server

With the server running, open another terminal:

```bash
# Verify connectivity with C-ECHO
echoscu -v -aec STORE_SCP localhost 11112

# Store a single image
storescu -v -aec STORE_SCP localhost 11112 hello_dicom_output.dcm

# Store multiple images from a directory
storescu -v -aec STORE_SCP localhost 11112 /path/to/dicom/files/*.dcm
```

### Verify Storage

```bash
# Check stored files
tree ./dicom_archive

# Query the database
sqlite3 ./dicom_archive/index.db "SELECT * FROM patients;"
sqlite3 ./dicom_archive/index.db "SELECT * FROM studies;"
sqlite3 ./dicom_archive/index.db "SELECT * FROM series;"
sqlite3 ./dicom_archive/index.db "SELECT * FROM instances;"
```

### Expected Output

**Server side:**
```
[14:30:22.456] [CONNECT] STORESCU -> STORE_SCP (active: 1)
[14:30:22.460] [C-STORE] From: STORESCU
  Patient:  DOE^JOHN
  Study:    Test Study
  Modality: OT
  Stored (#1) -> 1.2.3.4.5.6.7.8.9.dcm
[14:30:22.480] [RELEASE] STORESCU disconnected (active: 0)
```

**Client side (storescu -v):**
```
I: Requesting Association
I: Association Accepted
I: Sending C-STORE Request
I: Received C-STORE Response (Status: Success)
I: Releasing Association
```

## Key Concepts Explained

### C-STORE Operation

The C-STORE operation transfers DICOM images from an SCU (modality, workstation) to an SCP (PACS archive):

```
   Modality (SCU)                         PACS (SCP)
       |                                      |
       |  A-ASSOCIATE-RQ ────────────────────>|
       |<──────────────── A-ASSOCIATE-AC      |
       |                                      |
       |  C-STORE-RQ ────────────────────────>|
       |  [Command + Dataset with pixel data] |
       |                        Pre-validate  |
       |                        Store to disk |
       |                        Update index  |
       |<──────────────────── C-STORE-RSP     |
       |                      (Status: 0000)  |
       |                                      |
       |  A-RELEASE-RQ ──────────────────────>|
       |<─────────────────── A-RELEASE-RP     |
```

### File Storage Organization

The UID-hierarchical naming scheme organizes files by Study, Series, and Instance UIDs:

```
dicom_archive/
├── 1.2.840.113619.2.55.3.604688/           # Study UID
│   ├── 1.2.840.113619.2.55.3.604688.1/     # Series UID
│   │   ├── 1.2.840.113619.2.55.3.604688.1.1.dcm
│   │   ├── 1.2.840.113619.2.55.3.604688.1.2.dcm
│   │   └── ...
│   └── 1.2.840.113619.2.55.3.604688.2/
│       └── ...
├── 1.2.840.113619.2.55.3.604689/
│   └── ...
└── index.db                                 # SQLite database
```

### Index Database Schema

The database tracks DICOM metadata hierarchically:

| Table | Key Fields | Purpose |
|-------|------------|---------|
| patients | patient_id, name, birth_date | Patient demographics |
| studies | study_uid, patient_pk, date | Study-level metadata |
| series | series_uid, study_pk, modality | Series-level metadata |
| instances | sop_uid, series_pk, file_path | Instance + file location |

### Handler Callbacks

```cpp
// Pre-store handler - validate incoming data
storage_scp->set_pre_store_handler([](const auto& dataset) -> bool {
    // Return false to reject
    if (dataset.get_string(tags::patient_id).empty()) {
        return false;
    }
    return true;
});

// Main storage handler - save to disk and database
storage_scp->set_handler([](const auto& dataset, const auto& calling_ae,
                            const auto& sop_class, const auto& sop_uid) {
    // Store dataset and update index
    return storage_status::success;
});

// Post-store handler - trigger workflows
storage_scp->set_post_store_handler([](const auto& dataset,
                                       const auto& patient_id,
                                       const auto& study_uid,
                                       const auto& series_uid,
                                       const auto& sop_uid) {
    // Notify downstream systems
});
```

## Storage Status Codes

| Status | Code | Description |
|--------|------|-------------|
| Success | 0x0000 | Image stored successfully |
| Duplicate | 0x0111 | SOP Instance already exists |
| Out of Resources | 0xA700 | Storage system full or unavailable |
| Cannot Understand | 0xC000 | Invalid or corrupt dataset |
| Storage Error | 0xC001 | General storage failure |

## Configuration Options

### File Storage

| Option | Default | Description |
|--------|---------|-------------|
| `naming` | uid_hierarchical | File organization scheme |
| `duplicate` | reject | Duplicate handling policy |
| `create_directories` | true | Auto-create directories |
| `file_extension` | .dcm | Extension for saved files |

### Naming Schemes

- **uid_hierarchical**: `{StudyUID}/{SeriesUID}/{SOPUID}.dcm`
- **date_hierarchical**: `YYYY/MM/DD/{StudyUID}/{SOPUID}.dcm`
- **flat**: `{SOPUID}.dcm` (all files in root)

### Duplicate Policies

- **reject**: Return error if SOP Instance exists
- **replace**: Overwrite existing instance
- **ignore**: Accept silently, don't re-store

## Troubleshooting

### Storage Rejected (Status 0xC001)

Check pre-store validation:
- Patient ID must not be empty
- Study Instance UID must be present

### Database Errors

```bash
# Check database integrity
sqlite3 ./dicom_archive/index.db "PRAGMA integrity_check;"

# Rebuild database (requires re-indexing files)
rm ./dicom_archive/index.db
# Restart server - files will remain, database recreated
```

### Disk Space Issues

```bash
# Check available space
df -h ./dicom_archive

# Check archive size
du -sh ./dicom_archive
```

## Next Steps

Proceed to **Level 4: Mini PACS** to learn:
- C-FIND operation for querying stored images
- C-MOVE operation for retrieving images
- Patient/Study/Series/Instance query levels
- Result pagination and matching rules

## Related Documentation

- [DICOM PS3.4 Section B - Storage Service Class](http://dicom.nema.org/medical/dicom/current/output/chtml/part04/chapter_B.html)
- [DICOM PS3.7 Section 9.1.1 - C-STORE Service](http://dicom.nema.org/medical/dicom/current/output/chtml/part07/sect_9.html)
- [SQLite Documentation](https://www.sqlite.org/docs.html)
