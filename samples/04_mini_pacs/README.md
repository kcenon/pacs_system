# Level 4: Mini PACS

A complete Mini PACS implementation integrating all core DICOM services into a single server.

## Learning Objectives

After completing this sample, you will understand:

1. **Service Integration** - How to combine multiple SCPs in one DICOM server
2. **Query/Retrieve (Q/R)** - C-FIND and C-MOVE/C-GET operations
3. **Modality Worklist (MWL)** - Scheduled procedure queries
4. **MPPS** - Modality Performed Procedure Step tracking
5. **Class-Based Design** - Encapsulating PACS functionality in a reusable class

## Prerequisites

- Completed Level 1-3 samples
- Understanding of DICOM networking (associations, DIMSE)
- Familiarity with C-STORE and C-ECHO operations
- DCMTK tools for testing (optional but recommended)

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        Mini PACS                            │
│                                                             │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐         │
│  │  Storage    │  │   Index     │  │   Server    │         │
│  │  Backend    │  │  Database   │  │   Config    │         │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘         │
│         │                │                │                │
│  ┌──────▼────────────────▼────────────────▼──────┐         │
│  │                DICOM Server                    │         │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐          │         │
│  │  │Verify   │ │Storage  │ │Query    │          │         │
│  │  │SCP      │ │SCP      │ │SCP      │          │         │
│  │  └─────────┘ └─────────┘ └─────────┘          │         │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐          │         │
│  │  │Retrieve │ │Worklist │ │MPPS     │          │         │
│  │  │SCP      │ │SCP      │ │SCP      │          │         │
│  │  └─────────┘ └─────────┘ └─────────┘          │         │
│  └────────────────────────────────────────────────┘         │
└─────────────────────────────────────────────────────────────┘
```

## Services Overview

| Service | Operation | Purpose |
|---------|-----------|---------|
| Verification SCP | C-ECHO | Network connectivity testing |
| Storage SCP | C-STORE | Receive and archive DICOM images |
| Query SCP | C-FIND | Search at Patient/Study/Series/Image levels |
| Retrieve SCP | C-MOVE/C-GET | Send images to destinations |
| Worklist SCP | MWL C-FIND | Return scheduled procedures to modalities |
| MPPS SCP | N-CREATE/N-SET | Track procedure progress |

## Build

```bash
# From repository root
cmake -B build -DPACS_BUILD_SAMPLES=ON
cmake --build build --target mini_pacs
```

## Run

```bash
# Default port 11112
./build/samples/mini_pacs

# Custom port
./build/samples/mini_pacs 4242
```

## Test Commands

### 1. Connectivity Test (C-ECHO)

```bash
echoscu -v -aec MINI_PACS localhost 11112
```

Expected output: `Received Echo Response (Success)`

### 2. Store Images (C-STORE)

```bash
# Store a single image
storescu -v -aec MINI_PACS localhost 11112 image.dcm

# Store multiple images
storescu -v -aec MINI_PACS localhost 11112 *.dcm

# Store from Level 1 sample output
storescu -v -aec MINI_PACS localhost 11112 hello_dicom_output.dcm
```

### 3. Query at Patient Level (C-FIND)

```bash
# Find all patients
findscu -v -aec MINI_PACS -P \
    -k QueryRetrieveLevel=PATIENT \
    -k PatientName="*" \
    -k PatientID \
    localhost 11112

# Find specific patient
findscu -v -aec MINI_PACS -P \
    -k QueryRetrieveLevel=PATIENT \
    -k PatientName="DOE*" \
    localhost 11112
```

### 4. Query at Study Level (C-FIND)

```bash
# Find all studies for a patient
findscu -v -aec MINI_PACS -S \
    -k QueryRetrieveLevel=STUDY \
    -k PatientID="PAT001" \
    -k StudyDate \
    -k StudyDescription \
    -k AccessionNumber \
    localhost 11112

# Find studies by date range
findscu -v -aec MINI_PACS -S \
    -k QueryRetrieveLevel=STUDY \
    -k StudyDate="20240101-20241231" \
    localhost 11112
```

### 5. Query at Series Level (C-FIND)

```bash
findscu -v -aec MINI_PACS -S \
    -k QueryRetrieveLevel=SERIES \
    -k StudyInstanceUID="1.2.3..." \
    -k Modality \
    -k SeriesDescription \
    localhost 11112
```

### 6. Query Worklist (MWL C-FIND)

```bash
# Query all scheduled procedures
findscu -v -aec MINI_PACS -W \
    -k ScheduledProcedureStepStartDate="" \
    -k Modality="" \
    -k PatientName="" \
    -k PatientID="" \
    localhost 11112

# Query CT procedures only
findscu -v -aec MINI_PACS -W \
    -k Modality="CT" \
    -k PatientName="" \
    localhost 11112
```

### 7. Retrieve Study (C-MOVE)

```bash
# Start a receiver first
storescp -v -aet DEST_SCP 11113

# Then retrieve (in another terminal)
movescu -v -aec MINI_PACS -aem DEST_SCP \
    -k QueryRetrieveLevel=STUDY \
    -k StudyInstanceUID="1.2.3..." \
    localhost 11112
```

## Code Walkthrough

### Configuration

```cpp
mini_pacs_config config;
config.ae_title = "MINI_PACS";      // Application Entity title
config.port = 11112;                 // TCP port
config.storage_path = "./pacs_data"; // File storage root
config.enable_worklist = true;       // Enable MWL service
config.enable_mpps = true;           // Enable MPPS service
config.verbose_logging = true;       // Print operations to console
```

### Service Integration

The `mini_pacs` class initializes all services in `initialize_services()`:

```cpp
void mini_pacs::initialize_services() {
    // Verification SCP - Always enabled
    verification_scp_ = std::make_shared<services::verification_scp>();

    // Storage SCP - With custom handler
    storage_scp_ = std::make_shared<services::storage_scp>();
    storage_scp_->set_handler([this](auto& ds, auto& ae, auto& sop_class, auto& sop_uid) {
        return handle_store(ds, ae, sop_class, sop_uid);
    });

    // Query SCP - C-FIND at all levels
    query_scp_ = std::make_shared<services::query_scp>();
    query_scp_->set_handler([this](auto level, auto& keys, auto& ae) {
        return handle_query(level, keys, ae);
    });

    // ... more services
}
```

### Query Handler Implementation

```cpp
std::vector<core::dicom_dataset> mini_pacs::handle_query(
    services::query_level level,
    const core::dicom_dataset& query_keys,
    const std::string& calling_ae) {

    std::vector<core::dicom_dataset> results;

    switch (level) {
        case services::query_level::patient:
            // Query patient table
            auto patients = index_db_->query_patients(...);
            // Build response datasets
            break;

        case services::query_level::study:
            // Query study table
            break;

        case services::query_level::series:
            // Query series table
            break;

        case services::query_level::image:
            // Query instance table
            break;
    }

    return results;
}
```

### Worklist Management

```cpp
// Add worklist items programmatically
worklist_entry wl;
wl.patient_id = "PAT001";
wl.patient_name = "DOE^JOHN";
wl.modality = "CT";
wl.scheduled_date = "20240115";
wl.procedure_description = "CT CHEST";
pacs.add_worklist_item(wl);

// Query handler returns matching items
std::vector<core::dicom_dataset> handle_worklist_query(
    const core::dicom_dataset& query_keys,
    const std::string& calling_ae) {
    // Filter worklist_items_ based on query_keys
    // Return matching items as datasets
}
```

## Query/Retrieve Information Model

### Patient Root

```
PATIENT (0010,0020) PatientID
  └─ STUDY (0020,000D) StudyInstanceUID
       └─ SERIES (0020,000E) SeriesInstanceUID
            └─ IMAGE (0008,0018) SOPInstanceUID
```

### Query Keys by Level

| Level | Required Keys | Optional Keys |
|-------|---------------|---------------|
| PATIENT | PatientID or PatientName | BirthDate, Sex |
| STUDY | PatientID | StudyDate, Modality, AccessionNumber |
| SERIES | StudyInstanceUID | Modality, SeriesNumber |
| IMAGE | SeriesInstanceUID | InstanceNumber |

## MPPS Workflow

```
Modality                         PACS (MPPS SCP)
   │                                  │
   │  [Exam Started]                  │
   │                                  │
   │  N-CREATE-RQ                     │
   │  Status: "IN PROGRESS"           │
   │─────────────────────────────────►│
   │                                  │
   │  N-CREATE-RSP (Success)          │
   │◄─────────────────────────────────│
   │                                  │
   │  [Exam Completed]                │
   │                                  │
   │  N-SET-RQ                        │
   │  Status: "COMPLETED"             │
   │─────────────────────────────────►│
   │                                  │
   │  N-SET-RSP (Success)             │
   │◄─────────────────────────────────│
```

## Database Schema

The Mini PACS uses SQLite for indexing. View the data:

```bash
# List patients
sqlite3 ./pacs_data/index.db "SELECT * FROM patients;"

# List studies
sqlite3 ./pacs_data/index.db "SELECT * FROM studies;"

# List series
sqlite3 ./pacs_data/index.db "SELECT * FROM series;"

# List instances
sqlite3 ./pacs_data/index.db "SELECT * FROM instances;"

# Count images by modality
sqlite3 ./pacs_data/index.db \
    "SELECT modality, COUNT(*) FROM series GROUP BY modality;"
```

## File Structure

```
samples/04_mini_pacs/
├── CMakeLists.txt      # Build configuration
├── mini_pacs.hpp       # Mini PACS class header
├── mini_pacs.cpp       # Mini PACS implementation
├── main.cpp            # Application entry point
└── README.md           # This documentation
```

## Statistics

The Mini PACS tracks operation statistics:

```cpp
const auto& stats = pacs.statistics();
std::cout << "C-STORE: " << stats.c_store_count << "\n";
std::cout << "C-FIND:  " << stats.c_find_count << "\n";
std::cout << "C-MOVE:  " << stats.c_move_count << "\n";
std::cout << "MWL:     " << stats.mwl_count << "\n";
```

## Troubleshooting

### Connection Refused

- Verify the server is running
- Check the port number
- Ensure no firewall is blocking

### Association Rejected

- Verify AE Title matches (case-sensitive)
- Check the called AE in your command

### Query Returns No Results

- Verify images have been stored first
- Check query key spelling
- Use wildcards (`*`) for partial matching

### C-MOVE Fails

- Ensure destination AE is resolvable
- Start a receiver (storescp) before moving

## Next Steps

Proceed to **Level 5: Production PACS** to learn:

- TLS/SSL encryption for secure communication
- Role-based access control (RBAC)
- Anonymization/De-identification
- REST API for web integration
- Health monitoring and metrics
- Event-driven architecture

## References

- [DICOM PS3.4 - Storage Service Class](https://dicom.nema.org/medical/dicom/current/output/html/part04.html#chapter_B)
- [DICOM PS3.4 - Query/Retrieve Service Class](https://dicom.nema.org/medical/dicom/current/output/html/part04.html#chapter_C)
- [DICOM PS3.4 - Modality Worklist](https://dicom.nema.org/medical/dicom/current/output/html/part04.html#chapter_K)
- [DICOM PS3.4 - MPPS Service Class](https://dicom.nema.org/medical/dicom/current/output/html/part04.html#chapter_F)
