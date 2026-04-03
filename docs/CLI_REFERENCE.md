---
doc_id: "PAC-API-004"
doc_title: "CLI Reference"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "pacs_system"
category: "API"
---

# CLI Reference

> **Language:** **English** | [한국어](CLI_REFERENCE.kr.md)

Complete documentation for all 32 CLI tools included in the PACS System. For a quick overview, see the [README](../README.md#cli-tools--examples).

## Build Examples

```bash
cmake -S . -B build -DPACS_BUILD_EXAMPLES=ON
cmake --build build
```

---

## Table of Contents

### File Utilities
- [DCM Dump](#dcm-dump) - DICOM file inspection
- [DCM Info](#dcm-info) - DICOM file summary
- [DCM Modify](#dcm-modify) - Tag modification
- [DCM Conv](#dcm-conv) - Transfer Syntax conversion
- [DCM Anonymize](#dcm-anonymize) - De-identification (PS3.15)
- [DCM Dir](#dcm-dir) - DICOMDIR creation/management (PS3.10)
- [DCM to JSON](#dcm-to-json) - DICOM to JSON (PS3.18)
- [JSON to DCM](#json-to-dcm) - JSON to DICOM (PS3.18)
- [DCM to XML](#dcm-to-xml) - DICOM to XML (PS3.19)
- [XML to DCM](#xml-to-dcm) - XML to DICOM (PS3.19)
- [Img to DCM](#img-to-dcm) - Image to DICOM conversion
- [DCM Extract](#dcm-extract) - Pixel data extraction
- [DB Browser](#db-browser) - PACS index database viewer

### Network Services
- [Echo SCP](#echo-scp) - Verification server
- [Echo SCU](#echo-scu) - Verification client
- [Secure Echo SCU/SCP](#secure-echo-scuscp) - TLS-secured DICOM
- [Storage SCU](#storage-scu) - Image sender (C-STORE)
- [Query SCU](#query-scu) - Query client (C-FIND)
- [Find SCU](#find-scu) - dcmtk-compatible C-FIND
- [Retrieve SCU](#retrieve-scu) - Retrieve client (C-MOVE/C-GET)
- [Move SCU](#move-scu) - dcmtk-compatible C-MOVE
- [Get SCU](#get-scu) - dcmtk-compatible C-GET
- [Print SCU](#print-scu) - Print Management client

### Server Applications
- [Store SCP](#store-scp) - Storage server
- [QR SCP](#qr-scp) - Query/Retrieve server (C-FIND/C-MOVE/C-GET)
- [Worklist SCU](#worklist-scu) - Modality Worklist query client
- [Worklist SCP](#worklist-scp) - Modality Worklist server
- [MPPS SCU](#mpps-scu) - MPPS client
- [MPPS SCP](#mpps-scp) - MPPS server
- [PACS Server](#pacs-server) - Full PACS server

### Testing & Integration
- [Integration Tests](#integration-tests) - End-to-end workflow tests
- [Event Bus Integration](#event-bus-integration) - Inter-module communication

---

## File Utilities

### DCM Dump

DICOM file inspection utility for viewing metadata.

```bash
# Dump DICOM file metadata
./build/bin/dcm_dump image.dcm

# Filter specific tags
./build/bin/dcm_dump image.dcm --tags PatientName,PatientID,Modality

# Show pixel data information
./build/bin/dcm_dump image.dcm --pixel-info

# JSON output for integration
./build/bin/dcm_dump image.dcm --format json

# Scan directory recursively with summary
./build/bin/dcm_dump ./dicom_folder/ --recursive --summary
```

### DCM Info

DICOM file summary utility.

```bash
# Display DICOM file summary
./build/bin/dcm_info image.dcm

# Verbose mode with all available fields
./build/bin/dcm_info image.dcm --verbose

# JSON output for scripting
./build/bin/dcm_info image.dcm --format json

# Quick view of multiple files
./build/bin/dcm_info ./dicom_folder/ -r -q
```

### DCM Modify

dcmtk-compatible DICOM tag modification utility supporting numeric tag format `(GGGG,EEEE)` and keyword format.

```bash
# Insert tag (creates if not exists) - supports both tag formats
./build/bin/dcm_modify -i "(0010,0010)=Anonymous" patient.dcm
./build/bin/dcm_modify -i PatientName=Anonymous -o modified.dcm patient.dcm

# Modify existing tag (error if not exists)
./build/bin/dcm_modify -m "(0010,0020)=NEW_ID" patient.dcm

# Delete tag
./build/bin/dcm_modify -e "(0010,1000)" patient.dcm
./build/bin/dcm_modify -e OtherPatientIDs patient.dcm

# Delete all matching tags (including in sequences)
./build/bin/dcm_modify -ea "(0010,1001)" patient.dcm

# Delete all private tags
./build/bin/dcm_modify -ep patient.dcm

# Regenerate UIDs
./build/bin/dcm_modify -gst -gse -gin -o anonymized.dcm patient.dcm

# Use script file for batch modifications
./build/bin/dcm_modify --script modify.txt *.dcm

# In-place modification (creates .bak backup)
./build/bin/dcm_modify -i PatientID=NEW_ID patient.dcm

# In-place without backup (DANGEROUS!)
./build/bin/dcm_modify -i PatientID=NEW_ID -nb patient.dcm

# Process directory recursively
./build/bin/dcm_modify -i PatientName=Anonymous -r ./dicom_folder/ -o ./output/
```

Script file format (`modify.txt`):
```
# Comments start with #
i (0010,0010)=Anonymous     # Insert/modify tag
m (0008,0050)=ACC001        # Modify existing tag
e (0010,1000)               # Erase tag
ea (0010,1001)              # Erase all matching tags
```

### DCM Conv

Transfer Syntax conversion utility.

```bash
# Convert to Explicit VR Little Endian (default)
./build/bin/dcm_conv image.dcm converted.dcm --explicit

# Convert to Implicit VR Little Endian
./build/bin/dcm_conv image.dcm output.dcm --implicit

# Convert to JPEG Baseline with quality setting
./build/bin/dcm_conv image.dcm compressed.dcm --jpeg-baseline -q 85

# Convert directory recursively with verification
./build/bin/dcm_conv ./input_dir/ ./output_dir/ --recursive --verify

# List all supported Transfer Syntaxes
./build/bin/dcm_conv --list-syntaxes

# Convert with explicit Transfer Syntax UID
./build/bin/dcm_conv image.dcm output.dcm -t 1.2.840.10008.1.2.4.50
```

### DCM Anonymize

DICOM de-identification utility compliant with DICOM PS3.15 Security Profiles.

```bash
# Basic anonymization (removes direct identifiers)
./build/bin/dcm_anonymize patient.dcm anonymous.dcm

# HIPAA Safe Harbor compliance (18-identifier removal)
./build/bin/dcm_anonymize --profile hipaa_safe_harbor patient.dcm output.dcm

# GDPR-compliant pseudonymization
./build/bin/dcm_anonymize --profile gdpr_compliant patient.dcm output.dcm

# Keep specific tags unchanged
./build/bin/dcm_anonymize -k PatientSex -k PatientAge patient.dcm output.dcm

# Replace tags with custom values
./build/bin/dcm_anonymize -r "InstitutionName=Research Hospital" patient.dcm output.dcm

# Set new patient identifiers
./build/bin/dcm_anonymize --patient-id "STUDY001_001" --patient-name "Anonymous" patient.dcm

# Use UID mapping for consistent anonymization across study files
./build/bin/dcm_anonymize -m mapping.json patient.dcm output.dcm

# Shift dates for longitudinal studies
./build/bin/dcm_anonymize --profile retain_longitudinal --date-offset -30 patient.dcm

# Batch processing with directory recursion
./build/bin/dcm_anonymize --recursive -o anonymized/ ./originals/

# Dry-run mode to preview changes
./build/bin/dcm_anonymize --dry-run --verbose patient.dcm

# Verify anonymization completeness
./build/bin/dcm_anonymize --verify patient.dcm anonymous.dcm
```

Available anonymization profiles:
- `basic` - Remove direct patient identifiers (default)
- `clean_pixel` - Remove burned-in annotations from pixel data
- `clean_descriptions` - Clean free-text fields that may contain PHI
- `retain_longitudinal` - Preserve temporal relationships with date shifting
- `retain_patient_characteristics` - Keep demographics (sex, age, size, weight)
- `hipaa_safe_harbor` - Full HIPAA 18-identifier removal
- `gdpr_compliant` - GDPR pseudonymization requirements

### DCM Dir

Create and manage DICOMDIR files for DICOM media storage following DICOM PS3.10 standard.

```bash
# Create DICOMDIR from directory
./build/bin/dcm_dir create ./patient_data/

# Create with custom output path and file-set ID
./build/bin/dcm_dir create -o DICOMDIR --file-set-id "STUDY001" ./patient_data/

# Create with verbose output
./build/bin/dcm_dir create -v ./patient_data/

# List DICOMDIR contents in tree format
./build/bin/dcm_dir list DICOMDIR

# List with detailed information
./build/bin/dcm_dir list -l DICOMDIR

# List as flat file list
./build/bin/dcm_dir list --flat DICOMDIR

# Verify DICOMDIR structure
./build/bin/dcm_dir verify DICOMDIR

# Verify with file existence check
./build/bin/dcm_dir verify --check-files DICOMDIR

# Verify with consistency check (duplicate SOP Instance UID detection)
./build/bin/dcm_dir verify --check-consistency DICOMDIR

# Update DICOMDIR by adding new files
./build/bin/dcm_dir update -a ./new_study/ DICOMDIR
```

DICOMDIR structure follows DICOM hierarchy:
- PATIENT → STUDY → SERIES → IMAGE (hierarchical record structure)
- Referenced File IDs use backslash separator for ISO 9660 compatibility
- Supports standard record types: PATIENT, STUDY, SERIES, IMAGE

### DCM to JSON

Convert DICOM files to JSON format following the DICOM PS3.18 JSON representation standard.

```bash
# Convert DICOM to JSON (stdout)
./build/bin/dcm_to_json image.dcm

# Convert to file with pretty formatting
./build/bin/dcm_to_json image.dcm output.json --pretty

# Compact output (no formatting)
./build/bin/dcm_to_json image.dcm output.json --compact

# Include binary data as Base64
./build/bin/dcm_to_json image.dcm output.json --bulk-data inline

# Save binary data to separate files with URI references
./build/bin/dcm_to_json image.dcm output.json --bulk-data uri --bulk-data-dir ./bulk/

# Exclude pixel data
./build/bin/dcm_to_json image.dcm output.json --no-pixel

# Filter specific tags
./build/bin/dcm_to_json image.dcm -t 0010,0010 -t 0010,0020

# Process directory recursively
./build/bin/dcm_to_json ./dicom_folder/ --recursive --no-pixel
```

Output format (DICOM PS3.18):
```json
{
  "00100010": {
    "vr": "PN",
    "Value": [{"Alphabetic": "DOE^JOHN"}]
  },
  "00100020": {
    "vr": "LO",
    "Value": ["12345678"]
  }
}
```

### JSON to DCM

Convert JSON files (DICOM PS3.18 format) back to DICOM format.

```bash
# Convert JSON to DICOM
./build/bin/json_to_dcm metadata.json output.dcm

# Use template DICOM for pixel data and missing tags
./build/bin/json_to_dcm metadata.json output.dcm --template original.dcm

# Specify transfer syntax
./build/bin/json_to_dcm metadata.json output.dcm -t 1.2.840.10008.1.2.1

# Resolve BulkDataURI from specific directory
./build/bin/json_to_dcm metadata.json output.dcm --bulk-data-dir ./bulk/
```

### DCM to XML

Convert DICOM files to DICOM Native XML format (PS3.19).

```bash
# Convert DICOM to XML (stdout)
./build/bin/dcm_to_xml image.dcm

# Convert to file with pretty formatting
./build/bin/dcm_to_xml image.dcm output.xml --pretty

# Include binary data as Base64
./build/bin/dcm_to_xml image.dcm output.xml --bulk-data inline

# Save binary data to separate files with URI references
./build/bin/dcm_to_xml image.dcm output.xml --bulk-data uri --bulk-data-dir ./bulk/

# Exclude pixel data
./build/bin/dcm_to_xml image.dcm output.xml --no-pixel

# Filter specific tags
./build/bin/dcm_to_xml image.dcm -t 0010,0010 -t 0010,0020

# Process directory recursively
./build/bin/dcm_to_xml ./dicom_folder/ --recursive --no-pixel
```

Output format (DICOM Native XML PS3.19):
```xml
<?xml version="1.0" encoding="UTF-8"?>
<NativeDicomModel xmlns="http://dicom.nema.org/PS3.19/models/NativeDICOM">
  <DicomAttribute tag="00100010" vr="PN" keyword="PatientName">
    <PersonName number="1">
      <Alphabetic>
        <FamilyName>DOE</FamilyName>
        <GivenName>JOHN</GivenName>
      </Alphabetic>
    </PersonName>
  </DicomAttribute>
</NativeDicomModel>
```

### XML to DCM

Convert XML files (DICOM Native XML PS3.19 format) back to DICOM format.

```bash
# Convert XML to DICOM
./build/bin/xml_to_dcm metadata.xml output.dcm

# Use template DICOM for pixel data and missing tags
./build/bin/xml_to_dcm metadata.xml output.dcm --template original.dcm

# Specify transfer syntax
./build/bin/xml_to_dcm metadata.xml output.dcm -t 1.2.840.10008.1.2.1

# Resolve BulkData URI from specific directory
./build/bin/xml_to_dcm metadata.xml output.dcm --bulk-data-dir ./bulk/
```

### Img to DCM

Convert standard image files (JPEG) to DICOM format using Secondary Capture SOP Class.

```bash
# Basic conversion
./build/bin/img_to_dcm photo.jpg output.dcm

# With patient metadata
./build/bin/img_to_dcm photo.jpg output.dcm \
  --patient-name "DOE^JOHN" \
  --patient-id "12345" \
  --study-description "Photograph"

# Convert directory of images
./build/bin/img_to_dcm ./photos/ ./dicom/ --recursive

# With verbose output
./build/bin/img_to_dcm photo.jpg output.dcm -v

# Overwrite existing files
./build/bin/img_to_dcm ./photos/ ./dicom/ --recursive --overwrite
```

Features:
- Converts JPEG images to DICOM Secondary Capture format
- Automatic UID generation for Study, Series, and Instance
- Customizable patient and study metadata
- Batch processing with recursive directory support
- Requires libjpeg-turbo for JPEG support

### DCM Extract

Extract pixel data from DICOM files to standard image formats.

```bash
# Show pixel data information
./build/bin/dcm_extract image.dcm --info

# Extract to raw binary format
./build/bin/dcm_extract image.dcm output.raw --raw

# Extract to JPEG (requires libjpeg)
./build/bin/dcm_extract image.dcm output.jpg --jpeg -q 90

# Extract to PNG (requires libpng)
./build/bin/dcm_extract image.dcm output.png --png

# Extract to PPM/PGM format
./build/bin/dcm_extract image.dcm output.ppm --ppm

# Apply window level transformation
./build/bin/dcm_extract image.dcm output.jpg --jpeg --window 40 400

# Batch extraction from directory
./build/bin/dcm_extract ./dicom/ ./images/ --recursive --jpeg
```

Features:
- Supports RAW, JPEG, PNG, and PPM/PGM output formats
- Window level (center/width) transformation
- Automatic 16-bit to 8-bit conversion
- MONOCHROME1/MONOCHROME2 handling
- Batch processing with recursive directory support

### DB Browser

PACS index database viewer.

```bash
# List all patients
./build/bin/db_browser pacs.db patients

# List studies for a specific patient
./build/bin/db_browser pacs.db studies --patient-id "12345"

# Filter studies by date range
./build/bin/db_browser pacs.db studies --from 20240101 --to 20241231

# List series for a study
./build/bin/db_browser pacs.db series --study-uid "1.2.3.4.5"

# Show database statistics
./build/bin/db_browser pacs.db stats

# Database maintenance
./build/bin/db_browser pacs.db vacuum
./build/bin/db_browser pacs.db verify
```

---

## Network Services

### Echo SCP

DICOM Verification SCP server.

```bash
# Run Echo SCP
./build/bin/echo_scp --port 11112 --ae-title MY_ECHO
```

### Echo SCU

dcmtk-compatible DICOM connectivity verification tool.

```bash
# Basic connectivity test
./build/bin/echo_scu localhost 11112

# With custom AE Titles
./build/bin/echo_scu -aet MY_SCU -aec PACS_SCP localhost 11112

# Verbose output with custom timeout
./build/bin/echo_scu -v -to 60 localhost 11112

# Repeat test for connectivity monitoring
./build/bin/echo_scu -r 10 --repeat-delay 1000 localhost 11112

# Quiet mode (exit code only)
./build/bin/echo_scu -q localhost 11112

# Show all options
./build/bin/echo_scu --help
```

### Secure Echo SCU/SCP

TLS-secured DICOM connectivity testing with support for TLS 1.2/1.3 and mutual TLS.

```bash
# Generate test certificates first
cd examples/secure_dicom
./generate_certs.sh

# Start secure server (TLS)
./build/bin/secure_echo_scp 2762 MY_PACS \
    --cert certs/server.crt \
    --key certs/server.key \
    --ca certs/ca.crt

# Test secure connectivity (server verification only)
./build/bin/secure_echo_scu localhost 2762 MY_PACS \
    --ca certs/ca.crt

# Test with mutual TLS (client certificate)
./build/bin/secure_echo_scu localhost 2762 MY_PACS \
    --cert certs/client.crt \
    --key certs/client.key \
    --ca certs/ca.crt

# Use TLS 1.3
./build/bin/secure_echo_scu localhost 2762 MY_PACS \
    --ca certs/ca.crt \
    --tls-version 1.3
```

### Storage SCU

DICOM Storage SCU for sending images to a remote SCP.

```bash
# Send single DICOM file
./build/bin/store_scu localhost 11112 image.dcm

# Send with custom AE Titles
./build/bin/store_scu -aet MYSCU -aec PACS localhost 11112 image.dcm

# Send all files in directory (recursive) with progress
./build/bin/store_scu -r --progress localhost 11112 ./dicom_folder/

# Specify transfer syntax preference
./build/bin/store_scu --prefer-lossless localhost 11112 *.dcm

# Verbose output with timeout
./build/bin/store_scu -v -to 60 localhost 11112 image.dcm

# Generate transfer report
./build/bin/store_scu --report-file transfer.log localhost 11112 ./data/

# Quiet mode (minimal output)
./build/bin/store_scu -q localhost 11112 image.dcm

# Show help
./build/bin/store_scu --help
```

### Query SCU

DICOM Query SCU client (C-FIND).

```bash
# Query studies by patient name (wildcards supported)
./build/bin/query_scu localhost 11112 PACS_SCP --level STUDY --patient-name "DOE^*"

# Query by date range
./build/bin/query_scu localhost 11112 PACS_SCP --level STUDY --study-date "20240101-20241231"

# Query series for a specific study
./build/bin/query_scu localhost 11112 PACS_SCP --level SERIES --study-uid "1.2.3.4.5"

# Output as JSON for integration
./build/bin/query_scu localhost 11112 PACS_SCP --patient-id "12345" --format json

# Export to CSV
./build/bin/query_scu localhost 11112 PACS_SCP --modality CT --format csv > results.csv
```

### Find SCU

dcmtk-compatible C-FIND SCU utility.

```bash
# Patient Root Query - find all studies for a patient
./build/bin/find_scu -P -L STUDY -k "0010,0010=Smith*" localhost 11112

# Study Root Query - find CT studies in date range
./build/bin/find_scu -S -L STUDY \
  -aec PACS_SCP \
  -k "0008,0060=CT" \
  -k "0008,0020=20240101-20241231" \
  localhost 11112

# Output as JSON
./build/bin/find_scu -S -L SERIES -k "0020,000D=1.2.840..." -o json localhost 11112

# Read query keys from file
./build/bin/find_scu -f query_keys.txt localhost 11112
```

### Retrieve SCU

DICOM Retrieve SCU client (C-MOVE/C-GET).

```bash
# C-GET: Retrieve study directly to local machine
./build/bin/retrieve_scu localhost 11112 PACS_SCP --mode get --study-uid "1.2.3.4.5" -o ./downloads

# C-MOVE: Transfer study to another PACS/workstation
./build/bin/retrieve_scu localhost 11112 PACS_SCP --mode move --dest-ae LOCAL_SCP --study-uid "1.2.3.4.5"

# Retrieve specific series
./build/bin/retrieve_scu localhost 11112 PACS_SCP --level SERIES --series-uid "1.2.3.4.5.6"

# Retrieve all studies for a patient
./build/bin/retrieve_scu localhost 11112 PACS_SCP --level PATIENT --patient-id "12345"

# Flat storage structure (all files in one directory)
./build/bin/retrieve_scu localhost 11112 PACS_SCP --study-uid "1.2.3.4.5" --structure flat
```

### Move SCU

dcmtk-compatible C-MOVE SCU utility.

```bash
# Move study to third-party workstation
./build/bin/move_scu -aem WORKSTATION \
  -L STUDY \
  -k "0020,000D=1.2.840..." \
  pacs.example.com 104

# Move series with progress display
./build/bin/move_scu -aem ARCHIVE \
  --progress \
  -L SERIES \
  -k "0020,000E=1.2.840..." \
  localhost 11112
```

### Get SCU

dcmtk-compatible C-GET SCU utility.

```bash
# Get entire study directly (no separate storage SCP needed)
./build/bin/get_scu -L STUDY \
  -k "0020,000D=1.2.840..." \
  --progress \
  -od ./study_data/ \
  pacs.example.com 104

# Get single instance with lossless preference
./build/bin/get_scu --prefer-lossless \
  -L IMAGE \
  -k "0008,0018=1.2.840..." \
  -od ./retrieved/ \
  localhost 11112
```

### Print SCU

DICOM Print Management client for sending print requests to remote Print SCP systems (PS3.4 Annex H).

```bash
# Full print workflow: Create Session -> Create Film Box -> Print -> Delete
./build/bin/print_scu localhost 10400 PRINTER_SCP print \
  --copies 1 \
  --priority HIGH \
  --medium "BLUE FILM" \
  --format "STANDARD\\1,1" \
  --orientation PORTRAIT \
  --film-size 8INX10IN

# Query printer status
./build/bin/print_scu localhost 10400 PRINTER_SCP status

# With custom AE title and verbose output
./build/bin/print_scu -aet MY_WORKSTATION -v localhost 10400 PRINTER_SCP print

# Show help
./build/bin/print_scu --help
```

Features:
- **Film Session**: N-CREATE/N-DELETE for session lifecycle management
- **Film Box**: N-CREATE/N-ACTION (print) for film layout and printing
- **Image Box**: N-SET for setting pixel data on image positions
- **Printer Status**: N-GET for querying printer status (NORMAL, WARNING, FAILURE)
- **Meta SOP Class**: Supports Basic Grayscale and Color Print Meta SOP Classes

---

## Server Applications

### Store SCP

DICOM Storage SCP server. See the `store_scp` example for full implementation.

### QR SCP

Lightweight Query/Retrieve SCP server for serving DICOM files from a storage directory.

```bash
# Basic usage - serve files from a directory
./build/bin/qr_scp 11112 MY_PACS --storage-dir ./dicom

# With persistent index database (faster restart)
./build/bin/qr_scp 11112 MY_PACS --storage-dir ./dicom --index-db ./pacs.db

# With known peers for C-MOVE destinations
./build/bin/qr_scp 11112 MY_PACS --storage-dir ./dicom --peer VIEWER:192.168.1.10:11113

# Multiple peers for multi-destination C-MOVE
./build/bin/qr_scp 11112 MY_PACS --storage-dir ./dicom \
  --peer WS1:10.0.0.1:104 --peer WS2:10.0.0.2:104

# Scan and index storage without starting server
./build/bin/qr_scp 11112 MY_PACS --storage-dir ./dicom --scan-only

# Customize association limits
./build/bin/qr_scp 11112 MY_PACS --storage-dir ./dicom --max-assoc 20 --timeout 600
```

Features:
- **C-FIND**: Query at Patient, Study, Series, and Image levels
- **C-MOVE**: Send images to configured destination AE titles
- **C-GET**: Direct image retrieval over the same association
- **Automatic Indexing**: SQLite-based fast query responses
- **Graceful Shutdown**: Signal handling (SIGINT, SIGTERM)

### Worklist SCU

Modality Worklist Query client.

```bash
# Query worklist for CT modality
./build/bin/worklist_scu localhost 11112 RIS_SCP --modality CT

# Query worklist for today's scheduled procedures
./build/bin/worklist_scu localhost 11112 RIS_SCP --modality MR --date today

# Query by station AE title
./build/bin/worklist_scu localhost 11112 RIS_SCP --station "CT_SCANNER_01" --date 20241215

# Query with patient filter
./build/bin/worklist_scu localhost 11112 RIS_SCP --patient-name "DOE^*" --modality CT

# Output as JSON for integration
./build/bin/worklist_scu localhost 11112 RIS_SCP --modality CT --format json > worklist.json

# Export to CSV
./build/bin/worklist_scu localhost 11112 RIS_SCP --modality CT --format csv > worklist.csv
```

### Worklist SCP

A standalone Modality Worklist (MWL) server for providing scheduled procedure information to modalities.

```bash
# Basic usage with JSON worklist file
./build/bin/worklist_scp 11112 MY_WORKLIST --worklist-file ./worklist.json

# Load worklist from directory
./build/bin/worklist_scp 11112 MY_WORKLIST --worklist-dir ./worklist_data

# With result limit
./build/bin/worklist_scp 11112 MY_WORKLIST --worklist-file ./worklist.json --max-results 100
```

**Sample Worklist JSON** (`worklist.json`):
```json
[
  {
    "patientId": "12345",
    "patientName": "DOE^JOHN",
    "patientBirthDate": "19800101",
    "patientSex": "M",
    "studyInstanceUid": "1.2.3.4.5.6.7.8.9",
    "accessionNumber": "ACC001",
    "scheduledStationAeTitle": "CT_01",
    "scheduledProcedureStepStartDate": "20241220",
    "scheduledProcedureStepStartTime": "100000",
    "modality": "CT",
    "scheduledProcedureStepId": "SPS001",
    "scheduledProcedureStepDescription": "CT Abdomen"
  }
]
```

Features:
- **MWL C-FIND**: Responds to Modality Worklist queries
- **JSON Data Source**: Load worklist items from JSON files
- **Filtering**: Patient ID, name, modality, station AE, scheduled date
- **Graceful Shutdown**: Signal handling (SIGINT, SIGTERM)

### MPPS SCU

MPPS client for sending procedure status updates.

```bash
# Create new MPPS instance (start procedure)
./build/bin/mpps_scu localhost 11112 RIS_SCP create \
  --patient-id "12345" \
  --patient-name "Doe^John" \
  --modality CT

# Complete the procedure
./build/bin/mpps_scu localhost 11112 RIS_SCP set \
  --mpps-uid "1.2.3.4.5.6.7.8" \
  --status COMPLETED \
  --series-uid "1.2.3.4.5.6.7.8.9"

# Discontinue (cancel) a procedure
./build/bin/mpps_scu localhost 11112 RIS_SCP set \
  --mpps-uid "1.2.3.4.5.6.7.8" \
  --status DISCONTINUED \
  --reason "Patient refused"

# Verbose output for debugging
./build/bin/mpps_scu localhost 11112 RIS_SCP create \
  --patient-id "12345" \
  --modality MR \
  --verbose
```

### MPPS SCP

A standalone MPPS server for receiving procedure status updates from modality devices.

```bash
# Basic usage - listen for MPPS messages
./build/bin/mpps_scp 11112 MY_MPPS

# Store MPPS records to individual JSON files
./build/bin/mpps_scp 11112 MY_MPPS --output-dir ./mpps_records

# Append MPPS records to a single JSON file
./build/bin/mpps_scp 11112 MY_MPPS --output-file ./mpps.json

# With custom association limits
./build/bin/mpps_scp 11112 MY_MPPS --max-assoc 20 --timeout 600

# Show help
./build/bin/mpps_scp --help
```

Features:
- **N-CREATE**: Receive procedure start notifications (status = IN PROGRESS)
- **N-SET**: Receive procedure completion (COMPLETED) or cancellation (DISCONTINUED)
- **JSON Storage**: Store MPPS records to JSON files for integration
- **Statistics**: Display session statistics on shutdown
- **Graceful Shutdown**: Signal handling (SIGINT, SIGTERM)

### PACS Server

Full PACS server example with configuration.

```bash
# Run with configuration
./build/examples/pacs_server/pacs_server --config pacs_server.yaml
```

**Sample Configuration** (`pacs_server.yaml`):
```yaml
server:
  ae_title: MY_PACS
  port: 11112
  max_associations: 50

storage:
  directory: ./archive
  naming: hierarchical

database:
  path: ./pacs.db
  wal_mode: true
```

---

## Testing & Integration

### Integration Tests

End-to-end integration test suite.

```bash
# Run all integration tests
./build/bin/pacs_integration_e2e

# Run specific test category
./build/bin/pacs_integration_e2e "[connectivity]"    # Basic C-ECHO tests
./build/bin/pacs_integration_e2e "[store_query]"     # Store and query workflow
./build/bin/pacs_integration_e2e "[worklist]"        # Worklist and MPPS workflow
./build/bin/pacs_integration_e2e "[workflow][multimodal]"  # Multi-modal clinical workflows
./build/bin/pacs_integration_e2e "[xa]"              # XA Storage tests
./build/bin/pacs_integration_e2e "[tls]"             # TLS integration tests
./build/bin/pacs_integration_e2e "[stability][smoke]"  # Quick stability smoke test
./build/bin/pacs_integration_e2e "[stress]"          # Multi-association stress tests

# List available tests
./build/bin/pacs_integration_e2e --list-tests

# Run with verbose output
./build/bin/pacs_integration_e2e --success

# Generate JUnit XML report for CI/CD
./build/bin/pacs_integration_e2e --reporter junit --out results.xml
```

**Test Scenarios**:
- **Connectivity**: C-ECHO, multiple associations, timeout handling
- **Store & Query**: Store files, query by patient/study/series, wildcard matching
- **XA Storage**: X-Ray Angiographic image storage and retrieval
- **Multi-Modal Workflow**: Complete patient journey with CT, MR, XA modalities
- **Worklist/MPPS**: Scheduled procedures, MPPS IN PROGRESS/COMPLETED workflow
- **TLS Security**: Certificate validation, mutual TLS, secure communication
- **Stability**: Memory leak detection, connection pool exhaustion, long-running operations
- **Stress**: Concurrent SCUs, rapid connections, large datasets
- **Error Recovery**: Invalid SOP class, server restart, abort handling

### Event Bus Integration

The PACS system integrates with `common_system` Event Bus for inter-module communication and event-driven workflows.

```cpp
#include "pacs/core/events.hpp"
#include <kcenon/common/patterns/event_bus.h>

// Subscribe to image storage events
auto& bus = kcenon::common::get_event_bus();
auto sub_id = bus.subscribe<pacs::events::image_received_event>(
    [](const pacs::events::image_received_event& evt) {
        std::cout << "Received image: " << evt.sop_instance_uid
                  << " from " << evt.calling_ae << std::endl;
        // Trigger workflow, update cache, send notification, etc.
    }
);

// Subscribe to association events
bus.subscribe<pacs::events::association_established_event>(
    [](const pacs::events::association_established_event& evt) {
        std::cout << "New association: " << evt.calling_ae
                  << " -> " << evt.called_ae << std::endl;
    }
);

// Cleanup when done
bus.unsubscribe(sub_id);
```

**Available Event Types**:
- `association_established_event` / `association_released_event` / `association_aborted_event`
- `image_received_event` / `storage_failed_event`
- `query_executed_event` / `query_failed_event`
- `retrieve_started_event` / `retrieve_completed_event`
