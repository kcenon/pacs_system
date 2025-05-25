# PACS System Samples

This directory contains sample applications demonstrating how to use the PACS system modules. Each sample shows how to implement both the Service Class Provider (SCP) and Service Class User (SCU) components for the various DICOM services.

## Sample Structure

```
samples/
├── storage_sample/                # DICOM Storage SCP/SCU sample
├── worklist_sample/               # Modality Worklist SCP/SCU sample
├── query_retrieve_sample/         # Query/Retrieve SCP/SCU sample
├── mpps_sample/                   # MPPS SCP/SCU sample
└── integrated_sample/             # Complete integrated PACS server/client
```

## Building the Samples

```bash
# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake ../samples -DCMAKE_TOOLCHAIN_FILE="../../vcpkg/scripts/buildsystems/vcpkg.cmake"

# Build
cmake --build .
```

## Running the Samples

### Storage Sample

This sample demonstrates DICOM Storage functionality.

1. Start the Storage SCP:
```bash
./bin/storage_scp_sample
```

2. In a separate terminal, run the Storage SCU to send DICOM files:
```bash
./bin/storage_scu_sample /path/to/dicom/files
```

### Worklist Sample

This sample demonstrates Modality Worklist functionality.

1. Start the Worklist SCP:
```bash
./bin/worklist_scp_sample
```

2. In a separate terminal, query the worklist:
```bash
./bin/worklist_scu_sample
```

### Query/Retrieve Sample

This sample demonstrates Query/Retrieve functionality.

1. Start the Query/Retrieve SCP:
```bash
./bin/query_retrieve_scp_sample
```

2. In a separate terminal, query and retrieve DICOM objects:
```bash
./bin/query_retrieve_scu_sample
```

### MPPS Sample

This sample demonstrates Modality Performed Procedure Step functionality.

1. Start the MPPS SCP:
```bash
./bin/mpps_scp_sample
```

2. In a separate terminal, create and update MPPS:
```bash
./bin/mpps_scu_sample
```

### Integrated Sample

This sample demonstrates a complete PACS workflow with all components working together.

1. Start the integrated PACS server:
```bash
./bin/integrated_pacs_server
```

2. In a separate terminal, run the client:
```bash
./bin/integrated_pacs_client
```

The integrated client provides a menu-driven interface to:
- Query the worklist
- Create and update MPPS
- Store DICOM files
- Query studies
- Retrieve images
- Run a complete workflow demonstration

## Customization

Each sample can be customized by modifying the server AE Title, port, and host settings. See the source code for each sample for more details.

## Notes

- These samples use placeholder implementations when DCMTK is not available
- For a complete DICOM-compatible implementation, ensure DCMTK is installed and properly configured
- The samples create their own data directories for storing files and logs