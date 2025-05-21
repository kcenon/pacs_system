# PACS System

A comprehensive DICOM PACS (Picture Archiving and Communication System) implementation in C++.

## Project Structure

```
pacs_system/
├── common/                  # Common shared functionality
├── core/                    # Core implementation
│   ├── interfaces/          # Interface definitions
│   │   ├── mpps/
│   │   ├── storage/
│   │   ├── worklist/
│   │   ├── query_retrieve/
│   │   └── service_interface.h
│   ├── result/              # Result pattern for error handling
│   ├── thread/              # Thread system interface adapters
│   └── database/            # Database management
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
├── thread_system/           # Thread management submodule
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
- Thread pool system for concurrent operations
- Standardized error handling with Result pattern
- C++20 standards compliance

## Recent Improvements

- Removed unnecessary thread manager implementation in favor of direct thread_system usage
- Implemented comprehensive service interfaces
- Optimized build system to reduce duplicate library warnings
- Updated thread_system integration
- Removed empty and unnecessary files for cleaner codebase

## Dependencies

- DCMTK (DICOM Toolkit) - Optional, automatically installed via vcpkg
- fmt and other utilities - Automatically installed via vcpkg
- C++20 compatible compiler

## Building

```bash
# Install dependencies
./dependency.sh

# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake ..

# Build
cmake --build .
```

## Usage

To start the PACS server:

```bash
./bin/pacs_server
```

## Documentation

See the `docs/` directory for detailed documentation on:

- System architecture
- Configuration options
- Deployment strategies
- Logging system
- Security features

## License

This project is licensed under the BSD 3-Clause License. See the LICENSE file for details.