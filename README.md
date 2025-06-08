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

- **Full DCMTK Integration** - Complete DICOM toolkit integration with all major services
- **DICOM Storage Service** - Full C-STORE implementation for both SCP and SCU roles
- **Query/Retrieve Service** - C-FIND, C-MOVE, and C-GET operations
- **Modality Worklist** - Complete worklist management with C-FIND support
- **MPPS Service** - N-CREATE and N-SET for procedure step tracking
- **Compression Support** - JPEG, JPEG-LS, JPEG-2000, and RLE codecs
- **Network Communication** - Robust DIMSE protocol implementation
- **Thread Pool System** - Efficient concurrent operation handling
- **Error Handling** - Standardized Result pattern for all operations
- **C++20 Standards** - Modern C++ features and best practices

## DCMTK Integration (3-Week Implementation)

### Week 1: Foundation
- Enabled DCMTK in the build system
- Removed all placeholder implementations
- Implemented basic DICOM file I/O operations
- Added DICOM object and tag management

### Week 2: Network and Storage
- Implemented DICOM network communication (DIMSE)
- Added compression codec support
- Completed Storage SCP/SCU with C-STORE
- Added transfer syntax negotiation

### Week 3: Advanced Services
- Implemented Query/Retrieve with C-FIND, C-MOVE, C-GET
- Added Modality Worklist service
- Implemented MPPS with N-CREATE/N-SET
- Completed comprehensive testing framework

## Recent Improvements

- Complete DCMTK library integration replacing all placeholder code
- Full DICOM network protocol implementation
- Comprehensive codec management system
- Robust error handling and logging
- Removed all conditional compilation for cleaner codebase

## Dependencies

- **DCMTK** (DICOM Toolkit) - Core DICOM functionality
- **libxml2** - XML parsing for DCMTK
- **zlib** - Compression support
- **openssl** - TLS/Security support
- **libjpeg** - JPEG codec support
- **fmt** - Modern formatting library
- **gtest** - Unit testing framework
- **nlohmann-json** - JSON support
- **sqlite3** - Database support
- **cryptopp** - Cryptographic functions
- C++20 compatible compiler

All dependencies are automatically managed via vcpkg.

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