# PACS System

A comprehensive DICOM PACS (Picture Archiving and Communication System) implementation in C++.

## Project Structure

```
pacs_system/
├── common/                  # Common shared functionality
├── core/                    # Core implementation
│   ├── thread/              # Thread management
│   ├── result/              # Result handling
│   └── interfaces/          # Interface definitions
│       ├── mpps/
│       ├── storage/
│       ├── worklist/
│       └── query_retrieve/
├── modules/                 # DICOM service modules
│   ├── mpps/                # Modality Performed Procedure Step
│   │   ├── scp/             # Service Class Provider
│   │   └── scu/             # Service Class User
│   ├── storage/             # DICOM Storage
│   │   ├── scp/
│   │   └── scu/
│   ├── worklist/            # Modality Worklist
│   │   ├── scp/
│   │   └── scu/
│   └── query_retrieve/      # Query/Retrieve
│       ├── scp/
│       └── scu/
├── apps/                    # Applications
│   └── pacs_server.cpp      # Sample PACS server
├── CMakeLists.txt           # Main CMake configuration
├── vcpkg.json               # vcpkg dependencies
└── README.md                # This readme file
```

## Features

- MPPS (Modality Performed Procedure Step) Implementation
- DICOM Storage SCP/SCU
- Modality Worklist SCP/SCU
- Query/Retrieve SCP/SCU
- Internal thread pooling for concurrent operations
- C++20 standards compliance

## Dependencies

- DCMTK (DICOM Toolkit) - Optional, automatically installed via vcpkg
- fmt and other utilities - Automatically installed via vcpkg
- C++20 compatible compiler

## Building

```bash
# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake .. -DCMAKE_TOOLCHAIN_FILE="../../vcpkg/scripts/buildsystems/vcpkg.cmake"

# Build
cmake --build .
```

## Usage

To start the PACS server:

```bash
./bin/pacs_server
```