# Binary Integration Tests

This directory contains binary-level integration tests for the PACS system. These tests verify end-to-end functionality by launching actual server and client binaries, simulating real-world deployment scenarios.

## Overview

Binary integration tests complement library-level tests by verifying:
- Process lifecycle management (startup, shutdown, cleanup)
- Network socket binding and communication
- Cross-process DICOM protocol interactions
- Error handling at the system level
- TLS/SSL secure communication

## Directory Structure

```
examples/integration_tests/
├── README.md                    # This file
├── CMakeLists.txt              # Build configuration
├── test_fixtures.hpp           # C++ test utilities and process launcher
├── generate_test_data.cpp      # DICOM test file generator
├── scripts/                    # Shell test scripts
│   ├── common.sh               # Shared utility functions
│   ├── test_connectivity.sh    # Echo SCP/SCU tests
│   ├── test_store_retrieve.sh  # PACS storage workflow tests
│   ├── test_worklist_mpps.sh   # RIS workflow tests
│   ├── test_secure_dicom.sh    # TLS encrypted communication tests
│   └── run_all_binary_tests.sh # Master test runner
└── test_data/                  # Minimal DICOM test files
    ├── ct_minimal.dcm          # CT Image IOD
    ├── mr_minimal.dcm          # MR Image IOD
    └── xa_minimal.dcm          # XA Image IOD
```

## Prerequisites

### Build Requirements

Ensure all PACS binaries are built:

```bash
cmake --build build --target all
```

### Required Binaries

The tests expect these binaries in `build/bin/`:
- `echo_scp` / `echo_scu` - DICOM connectivity verification
- `pacs_server` - Main PACS server
- `store_scu` - DICOM C-STORE client
- `query_scu` - DICOM C-FIND/C-MOVE client
- `worklist_scu` - Modality Worklist client
- `mpps_scu` - Modality Performed Procedure Step client

## Running Tests

### Quick Start

Run all binary integration tests:

```bash
cd examples/integration_tests/scripts
./run_all_binary_tests.sh
```

### Individual Test Suites

Each test can be run independently:

```bash
# Test basic DICOM connectivity (Echo)
./test_connectivity.sh

# Test storage and retrieval workflow
./test_store_retrieve.sh

# Test worklist and MPPS workflow
./test_worklist_mpps.sh

# Test TLS secure communication
./test_secure_dicom.sh
```

### Options

```bash
# Specify custom build directory
./run_all_binary_tests.sh /path/to/build

# Run with verbose output
./run_all_binary_tests.sh -v

# Run tests in parallel
./run_all_binary_tests.sh -p

# Combine options
./run_all_binary_tests.sh -v -p /path/to/build
```

## Test Descriptions

### test_connectivity.sh

Tests basic DICOM network connectivity using C-ECHO:
- Server startup and port binding
- Single echo request/response
- Multiple sequential echo requests
- Custom AE title handling
- Connection failure handling
- Invalid AE title rejection

### test_store_retrieve.sh

Tests PACS storage workflow:
- Server initialization with storage directory
- C-STORE operations for CT, MR, XA modalities
- Query at Study/Series/Instance levels
- C-MOVE retrieval operations
- Multi-file storage sessions
- Storage directory cleanup

### test_worklist_mpps.sh

Tests RIS integration workflow:
- Scheduled procedure query (MWL)
- Patient name/ID matching
- Date range queries
- MPPS N-CREATE (procedure start)
- MPPS N-SET (procedure complete/discontinue)
- MPPS status transitions

### test_secure_dicom.sh

Tests TLS-encrypted DICOM communication:
- Certificate generation for testing
- TLS server startup
- Secure Echo operations
- Client certificate verification
- Cipher suite negotiation

## C++ Test Utilities

### process_launcher Class

The `test_fixtures.hpp` provides a cross-platform `process_launcher` class:

```cpp
#include "test_fixtures.hpp"

using namespace pacs::integration_test;

// Run a command with timeout
auto result = process_launcher::run(
    "/path/to/echo_scu",
    {"localhost", "11112", "PACS_SCP"},
    std::chrono::seconds{10}
);

if (result.exit_code == 0) {
    std::cout << "Success: " << result.stdout_output;
}

// Start a background server
auto pid = process_launcher::start_background(
    "/path/to/echo_scp",
    {"11112", "MY_SCP"}
);

// Wait for port to be available
if (process_launcher::wait_for_port(11112)) {
    // Server is ready
}

// Stop the server
process_launcher::stop_background(pid);
```

### RAII Guard

Use `background_process_guard` for automatic cleanup:

```cpp
{
    background_process_guard server(
        "/path/to/pacs_server",
        {"11112", "PACS_SCP", "/tmp/storage"}
    );

    if (server.wait_for_ready()) {
        // Run tests...
    }
}  // Server automatically stopped
```

## Test Data Generation

Generate minimal DICOM test files:

```bash
# Build the generator
cmake --build build --target generate_test_data

# Run generator
./build/bin/generate_test_data /path/to/output
```

Or use the CMake custom target:

```bash
cmake --build build --target generate_binary_test_data
```

## Cross-Platform Considerations

### Port Detection

The scripts use multiple fallback methods for detecting listening ports:
1. `lsof -i TCP:port` (preferred on macOS/Linux)
2. `nc -z host port` (netcat fallback)
3. `/dev/tcp/host/port` (bash built-in)

### Process Management

The C++ `process_launcher` class handles platform differences:
- POSIX: `fork()` + `exec()`, `kill()`, `waitpid()`
- Windows: `CreateProcess()`, `TerminateProcess()`

### Timeouts

All operations have configurable timeouts to prevent hanging tests:
- Server startup: 5-10 seconds
- Individual operations: 30 seconds
- Full test suite: 5 minutes

## Troubleshooting

### Server Not Starting

1. Check if port is already in use:
   ```bash
   lsof -i TCP:11112
   ```

2. Verify binary exists and is executable:
   ```bash
   ls -la build/bin/echo_scp
   ```

3. Check server logs for errors:
   ```bash
   ./echo_scp 11112 TEST_SCP 2>&1 | tee server.log
   ```

### Tests Failing

1. Run individual tests with verbose output:
   ```bash
   bash -x ./test_connectivity.sh
   ```

2. Increase timeouts for slow systems:
   ```bash
   # Edit the script timeout value
   TEST_TIMEOUT=20  # seconds
   ```

3. Check network configuration:
   ```bash
   nc -v localhost 11112
   ```

### TLS Certificate Issues

1. Regenerate test certificates:
   ```bash
   rm -rf /tmp/pacs_test_certs
   ./test_secure_dicom.sh
   ```

2. Verify OpenSSL is installed:
   ```bash
   openssl version
   ```

## Contributing

When adding new binary tests:

1. Create a new script in `scripts/` following the existing pattern
2. Source `common.sh` for shared utilities
3. Implement cleanup handlers with `trap`
4. Add the test to `run_all_binary_tests.sh`
5. Update this README

## Related Documentation

- [PACS System Architecture](../../docs/architecture.md)
- [DICOM Protocol Guide](../../docs/dicom-protocol.md)
- [Library Integration Tests](../tests/README.md)

## License

See the main project [LICENSE](../../LICENSE) file.
