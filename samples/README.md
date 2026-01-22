# PACS System Developer Samples

Progressive learning samples for building PACS applications with pacs_system. Each level introduces new concepts while building on previous knowledge.

## Quick Start

```bash
# Clone and build with samples enabled
git clone https://github.com/kcenon/pacs_system.git
cd pacs_system
cmake -B build -DPACS_BUILD_SAMPLES=ON
cmake --build build

# Run your first sample
./build/samples/hello_dicom

# Test network samples (in separate terminals)
./build/samples/echo_server 11112          # Start server
echoscu -aec ECHO_SCP localhost 11112      # Test with DCMTK
```

## Learning Path

| Level | Sample | Description | Key Concepts | Time |
|:-----:|--------|-------------|--------------|:----:|
| 1 | [Hello DICOM](01_hello_dicom/) | Core data structures | Tags, VR, Dataset, File I/O | 30 min |
| 2 | [Echo Server](02_echo_server/) | Network basics | Server config, Association, C-ECHO | 1 hour |
| 3 | [Storage Server](03_storage_server/) | File storage & DB | C-STORE, File storage, SQLite index | 2 hours |
| 4 | [Mini PACS](04_mini_pacs/) | Complete workflow | Q/R, Worklist, MPPS, Service integration | 4 hours |
| 5 | Production PACS | Enterprise features | TLS, RBAC, REST API, Monitoring | Coming Soon |

## Prerequisites

### Required

- **C++20 compiler**: GCC 11+, Clang 14+, or MSVC 2022
- **CMake**: 3.20 or higher
- **pacs_system library**: Built from source

### Recommended

- **DCMTK**: For testing DICOM communication
  ```bash
  # Ubuntu/Debian
  apt-get install dcmtk

  # macOS
  brew install dcmtk

  # Windows (vcpkg)
  vcpkg install dcmtk
  ```

## Directory Structure

```
samples/
├── CMakeLists.txt              # Root build configuration
├── README.md                   # This file
├── common/                     # Shared utilities for all samples
│   ├── signal_handler.hpp/cpp  # Cross-platform signal handling
│   ├── console_utils.hpp/cpp   # Formatted console output
│   └── test_data_generator.hpp/cpp # Test DICOM data generation
├── 01_hello_dicom/             # Level 1: DICOM basics
├── 02_echo_server/             # Level 2: Network connectivity
├── 03_storage_server/          # Level 3: Image archiving
├── 04_mini_pacs/               # Level 4: Full PACS functionality
└── 05_production_pacs/         # Level 5: Enterprise features (planned)
```

## Concepts by Level

### Level 1: Hello DICOM

Foundational DICOM data structures and file operations.

- **DICOM Tags**: (Group, Element) pairs that identify data elements
- **Value Representations (VR)**: Data types (PN, DA, UI, US, etc.)
- **Datasets**: Collections of DICOM elements
- **Part 10 Files**: Standard DICOM file format

### Level 2: Echo Server

Introduction to DICOM network communication.

- **Server Configuration**: AE Title, port, timeouts
- **Association Management**: Connection lifecycle
- **C-ECHO**: Verification service for connectivity testing
- **Event Callbacks**: Monitoring connections

### Level 3: Storage Server

Core PACS archiving functionality.

- **C-STORE**: Receiving images from modalities
- **File Storage**: Hierarchical file organization
- **Index Database**: SQLite-based metadata indexing
- **Handler Callbacks**: Pre/post-store validation

### Level 4: Mini PACS

Complete PACS implementation with all core services.

- **Query/Retrieve**: C-FIND and C-MOVE operations
- **Modality Worklist (MWL)**: Scheduled procedures
- **MPPS**: Procedure status tracking
- **Service Integration**: Multiple SCPs in one server

### Level 5: Production PACS (Coming Soon)

Enterprise-ready features for production deployment.

- **TLS Security**: Encrypted communication
- **Access Control**: Role-based permissions
- **Anonymization**: De-identification profiles
- **REST API**: Web-based PACS access
- **Health Monitoring**: Service health checks

## Build Options

```bash
# Build all samples
cmake -B build -DPACS_BUILD_SAMPLES=ON
cmake --build build

# Build specific sample
cmake --build build --target sample_01_hello_dicom
cmake --build build --target sample_02_echo_server
cmake --build build --target sample_03_storage_server
cmake --build build --target mini_pacs
```

## Testing with DCMTK

DCMTK provides command-line tools for testing DICOM communication.

### Common Commands

```bash
# Connectivity test (C-ECHO)
echoscu -v -aec <AE_TITLE> localhost <PORT>

# Send images (C-STORE)
storescu -v -aec <AE_TITLE> localhost <PORT> image.dcm

# Query patients (C-FIND)
findscu -v -aec <AE_TITLE> -P -k PatientName="*" localhost <PORT>

# Query studies
findscu -v -aec <AE_TITLE> -S -k StudyDate="20240101-20241231" localhost <PORT>

# Query worklist
findscu -v -aec <AE_TITLE> -W -k Modality="CT" localhost <PORT>

# Retrieve images (C-MOVE)
movescu -v -aec <AE_TITLE> -aem <DEST_AE> -k StudyInstanceUID="..." localhost <PORT>

# Dump DICOM file
dcmdump file.dcm
```

### Sample Testing Workflow

```bash
# Terminal 1: Start Mini PACS
./build/samples/mini_pacs 11112

# Terminal 2: Generate and store test data
./build/samples/hello_dicom                              # Creates test file
storescu -v -aec MINI_PACS localhost 11112 hello_dicom_output.dcm

# Terminal 2: Query stored data
findscu -v -aec MINI_PACS -P -k PatientName="*" localhost 11112
```

## Troubleshooting

### Connection Refused

- Verify the server is running
- Check the port number matches
- Ensure no firewall is blocking the connection

### Association Rejected

- Verify AE Title matches (case-sensitive)
- Check the `-aec` parameter in your DCMTK command
- Review presentation context negotiation

### Query Returns No Results

- Ensure images have been stored first
- Check query key spelling and wildcards
- Use `*` for partial matching

### Storage Fails

- Check disk space
- Verify write permissions on storage directory
- Review pre-store validation rules

### Build Errors

```bash
# Ensure samples are enabled
cmake -B build -DPACS_BUILD_SAMPLES=ON

# Clean rebuild
rm -rf build && cmake -B build -DPACS_BUILD_SAMPLES=ON && cmake --build build
```

## Architecture Reference

```
┌─────────────────────────────────────────────────────────────┐
│                     Application Layer                        │
│   (Samples, Custom Applications)                            │
├─────────────────────────────────────────────────────────────┤
│                     Services Layer                           │
│   Verification SCP │ Storage SCP │ Query SCP │ Retrieve SCP │
├─────────────────────────────────────────────────────────────┤
│                     Network Layer                            │
│   DICOM Server │ Association │ PDU │ DIMSE                  │
├─────────────────────────────────────────────────────────────┤
│                     Storage Layer                            │
│   File Storage │ Index Database │ Query Builder              │
├─────────────────────────────────────────────────────────────┤
│                     Encoding Layer                           │
│   Transfer Syntax │ VR Handlers │ Pixel Codecs              │
├─────────────────────────────────────────────────────────────┤
│                      Core Layer                              │
│   DICOM Tag │ Dataset │ Element │ File                      │
└─────────────────────────────────────────────────────────────┘
```

## Common Utilities

The `common/` directory provides shared utilities used across all samples:

| Utility | Purpose |
|---------|---------|
| `signal_handler` | Cross-platform graceful shutdown (SIGINT/SIGTERM) |
| `console_utils` | Formatted output with colors and DICOM data display |
| `test_data_generator` | Generate realistic test DICOM datasets |

See [common/README.md](common/README.md) for detailed API documentation.

## Further Reading

### DICOM Standard

- [Part 3: Information Object Definitions](https://dicom.nema.org/medical/dicom/current/output/html/part03.html)
- [Part 4: Service Class Specifications](https://dicom.nema.org/medical/dicom/current/output/html/part04.html)
- [Part 5: Data Structures and Encoding](https://dicom.nema.org/medical/dicom/current/output/html/part05.html)
- [Part 7: Message Exchange](https://dicom.nema.org/medical/dicom/current/output/html/part07.html)
- [Part 8: Network Communication](https://dicom.nema.org/medical/dicom/current/output/html/part08.html)
- [Part 10: Media Storage](https://dicom.nema.org/medical/dicom/current/output/html/part10.html)

### Project Resources

- [pacs_system Repository](https://github.com/kcenon/pacs_system)
- [kcenon Ecosystem](https://github.com/kcenon)

## Contributing

Contributions to the samples are welcome! Please ensure:

1. Code follows existing style conventions
2. Includes educational comments explaining DICOM concepts
3. Works on Linux, macOS, and Windows
4. Includes README with learning objectives
