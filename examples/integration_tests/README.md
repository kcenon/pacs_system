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
├── README.md                        # This file
├── CMakeLists.txt                   # Build configuration
├── main.cpp                         # Test main entry point
├── test_fixtures.hpp                # C++ test utilities and process launcher
├── test_data_generator.hpp          # Comprehensive DICOM data generators
├── test_data_generator.cpp          # Generator implementations
├── test_data_generator_test.cpp     # Generator unit tests
├── test_connectivity.cpp            # DICOM network connectivity tests
├── test_store_query.cpp             # Storage and query tests
├── test_worklist_mpps.cpp           # Worklist and MPPS tests
├── test_multimodal_workflow.cpp     # Multi-modal clinical workflow tests
├── test_stress.cpp                  # Stress and performance tests
├── test_error_recovery.cpp          # Error handling tests
├── test_xa_storage.cpp              # XA-specific storage tests
├── test_tls_integration.cpp         # TLS integration tests
├── test_stability.cpp               # Long-running stability tests
├── test_dicom_server_v2_integration.cpp  # V2 server integration tests (Issue #163)
├── generate_test_data.cpp           # DICOM test file generator
├── scripts/                         # Shell test scripts
│   ├── common.sh                    # Shared utility functions
│   ├── test_connectivity.sh         # Echo SCP/SCU tests
│   ├── test_store_retrieve.sh       # PACS storage workflow tests
│   ├── test_worklist_mpps.sh        # RIS workflow tests
│   ├── test_secure_dicom.sh         # TLS encrypted communication tests
│   └── run_all_binary_tests.sh      # Master test runner
└── test_data/                       # Minimal DICOM test files
    ├── ct_minimal.dcm               # CT Image IOD
    ├── mr_minimal.dcm               # MR Image IOD
    ├── xa_minimal.dcm               # XA Image IOD
    └── certs/                       # TLS test certificates
        ├── generate_test_certs.sh   # Certificate generation script
        ├── ca.crt / ca.key          # Test CA certificate
        ├── server.crt / server.key  # Server certificate
        ├── client.crt / client.key  # Client certificate (for mTLS)
        └── other_ca.crt             # Different CA for validation testing
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

## dicom_server_v2 Integration Tests (C++)

The `test_dicom_server_v2_integration.cpp` provides comprehensive integration testing for `dicom_server_v2`, which is the network_system-based implementation introduced in Issue #162.

### Running V2 Tests

```bash
# Run all V2 integration tests
./build/bin/pacs_integration_e2e "[v2]"

# Run specific V2 test categories
./build/bin/pacs_integration_e2e "[v2][integration]"  # Basic operations
./build/bin/pacs_integration_e2e "[v2][stress]"       # Stress tests
./build/bin/pacs_integration_e2e "[v2][migration]"    # V1 to V2 validation
./build/bin/pacs_integration_e2e "[v2][callbacks]"    # Callback testing

# Run TLS tests with V2 server
./build/bin/pacs_integration_e2e "[tls][v2]"
```

### V2 Test Scenarios

| Scenario | Tag | Description |
|----------|-----|-------------|
| C-ECHO Integration | `[v2][integration][echo]` | Single and multiple C-ECHO operations |
| C-STORE Integration | `[v2][integration][store]` | Single and batch image storage |
| Concurrent Storage | `[v2][stress][concurrent]` | 10 workers x 5 files concurrently |
| Rapid Connections | `[v2][stress][sequential]` | 30 sequential connections |
| Max Associations | `[v2][stress][limits]` | Connection limit enforcement |
| API Compatibility | `[v2][migration][api]` | V1 and V2 behavior comparison |
| Graceful Shutdown | `[v2][migration][shutdown]` | Shutdown with active connections |
| Callback Invocation | `[v2][callbacks]` | Association established/closed callbacks |
| Mixed Operations | `[v2][stress][mixed]` | Concurrent echo and store workers |
| TLS with V2 | `[tls][v2]` | TLS connections with V2 server |

### V2 Test Fixtures

The test file provides reusable V2 server fixtures:

```cpp
#include "test_dicom_server_v2_integration.cpp"

// Basic V2 test server
test_server_v2 server(port, "TEST_SCP_V2");
server.register_service(std::make_shared<verification_scp>());
server.start();

// Stress test server with storage tracking
stress_test_server_v2 stress_server(port, "STRESS_V2");
stress_server.initialize();
stress_server.start();
// ... run stress tests
INFO("Stored count: " << stress_server.stored_count());

// Migration validation - compare V1 and V2
dicom_server server_v1(config_v1);
dicom_server_v2 server_v2(config_v2);
// Both should behave identically for same configuration
```

### Prerequisites

The V2 integration tests require `PACS_WITH_NETWORK_SYSTEM` to be defined during build:

```bash
cmake -DPACS_WITH_NETWORK_SYSTEM=ON ...
```

If `PACS_WITH_NETWORK_SYSTEM` is not defined, the tests will be skipped with a warning message.

## TLS Integration Tests (C++)

The `test_tls_integration.cpp` provides comprehensive C++ tests for TLS-secured DICOM communication. These tests verify TLS functionality using the `network_adapter` TLS configuration.

### Running TLS Tests

```bash
# Generate test certificates (run once)
./examples/integration_tests/test_data/certs/generate_test_certs.sh

# Run TLS integration tests
PACS_TEST_CERT_DIR=./examples/integration_tests/test_data/certs \
    ./build/bin/pacs_integration_e2e "[tls]"

# Run specific TLS test scenarios
./build/bin/pacs_integration_e2e "[tls][connectivity]"  # Basic TLS connection
./build/bin/pacs_integration_e2e "[tls][security]"      # Certificate validation
./build/bin/pacs_integration_e2e "[tls][mtls]"          # Mutual TLS
./build/bin/pacs_integration_e2e "[tls][version]"       # TLS version tests
./build/bin/pacs_integration_e2e "[tls][concurrent]"    # Concurrent TLS connections
./build/bin/pacs_integration_e2e "[tls][config]"        # Configuration validation
```

### TLS Test Scenarios

| Scenario | Tag | Description |
|----------|-----|-------------|
| Basic TLS Connection | `[tls][connectivity]` | TLS server accepts connection and responds to C-ECHO |
| Certificate Validation | `[tls][security]` | Valid/invalid certificate handling |
| Mutual TLS | `[tls][mtls]` | Client certificate authentication |
| TLS Version | `[tls][version]` | TLS 1.2 and 1.3 negotiation |
| Concurrent Connections | `[tls][concurrent]` | Multiple parallel TLS connections |
| Config Validation | `[tls][config]` | TLS configuration structure validation |

### Test Certificate Generation

The `generate_test_certs.sh` script creates a complete PKI for testing:

```bash
# Generate certificates in custom directory
./generate_test_certs.sh /path/to/output

# Files generated:
# - ca.crt / ca.key       - Root CA certificate
# - server.crt / server.key - Server certificate (localhost)
# - client.crt / client.key - Client certificate (for mTLS)
# - other_ca.crt / other_ca.key - Different CA for validation tests
```

### TLS Test Fixtures

The test file provides reusable TLS fixtures:

```cpp
#include "test_tls_integration.cpp"

// Get test certificate paths
auto certs = get_test_certificates();
if (!certs.all_exist()) {
    SKIP("Test certificates not available");
}

// Create TLS-enabled server
tls_config server_tls;
server_tls.enabled = true;
server_tls.cert_path = certs.server_cert;
server_tls.key_path = certs.server_key;
server_tls.ca_path = certs.ca_cert;
server_tls.verify_peer = true;  // Require client cert (mTLS)

tls_test_server server(port, "TLS_SCP", server_tls);
server.register_service(std::make_shared<verification_scp>());
server.start();

// Connect with TLS client
tls_config client_tls;
client_tls.enabled = true;
client_tls.cert_path = certs.client_cert;
client_tls.key_path = certs.client_key;
client_tls.ca_path = certs.ca_cert;

auto result = tls_test_client::connect(
    "localhost", port, server.ae_title(), "TLS_SCU", client_tls);
```

### Environment Variables

| Variable | Description |
|----------|-------------|
| `PACS_TEST_CERT_DIR` | Directory containing test certificates |

## C++ Test Utilities

### test_data_generator Class

The `test_data_generator` class provides comprehensive DICOM dataset generators for testing. This supports all modalities, multi-frame images, and edge cases.

```cpp
#include "test_data_generator.hpp"

using namespace pacs::integration_test;

// Single modality datasets
auto ct_dataset = test_data_generator::ct();
auto mr_dataset = test_data_generator::mr();
auto xa_dataset = test_data_generator::xa();
auto us_dataset = test_data_generator::us();

// Multi-frame datasets
auto xa_cine = test_data_generator::xa_cine(30);    // 30 frames
auto us_cine = test_data_generator::us_cine(60);    // 60 frames
auto enhanced_ct = test_data_generator::enhanced_ct(100);
auto enhanced_mr = test_data_generator::enhanced_mr(50);

// Clinical workflow - multi-modal patient study
auto study = test_data_generator::patient_journey(
    "PATIENT001",
    {"CT", "MR", "XA"}
);
// All datasets share same Study Instance UID

// Edge case datasets
auto large = test_data_generator::large(10);  // ~10MB dataset
auto unicode = test_data_generator::unicode(); // Korean patient name
auto private_tags = test_data_generator::with_private_tags("MY_CREATOR");
auto invalid = test_data_generator::invalid(
    invalid_dataset_type::missing_sop_class_uid
);
```

#### Supported Generators

| Generator | Description | SOP Class |
|-----------|-------------|-----------|
| `ct()` | CT Image | 1.2.840.10008.5.1.4.1.1.2 |
| `mr()` | MR Image | 1.2.840.10008.5.1.4.1.1.4 |
| `xa()` | XA Image (single frame) | 1.2.840.10008.5.1.4.1.1.12.1 |
| `us()` | US Image (single frame) | 1.2.840.10008.5.1.4.1.1.6.1 |
| `xa_cine()` | XA Multi-frame | 1.2.840.10008.5.1.4.1.1.12.1 |
| `us_cine()` | US Multi-frame | 1.2.840.10008.5.1.4.1.1.6.2 |
| `enhanced_ct()` | Enhanced CT | 1.2.840.10008.5.1.4.1.1.2.1 |
| `enhanced_mr()` | Enhanced MR | 1.2.840.10008.5.1.4.1.1.4.1 |
| `worklist()` | Modality Worklist | N/A |

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

## Multi-Modal Workflow Tests

The `test_multimodal_workflow.cpp` contains comprehensive tests for multi-modality clinical workflows. These tests verify that the PACS system correctly handles complex real-world scenarios involving multiple imaging modalities.

### Test Scenarios

| Scenario | Description | Modalities |
|----------|-------------|------------|
| Complete Patient Journey | Full diagnostic workup with multiple studies | CT, MR |
| Interventional Workflow | XA cine acquisition with procedure tracking | XA (30-frame) |
| Emergency Multi-Modality | Trauma case with rapid multi-modal imaging | CT, XA, follow-up CT |
| Concurrent Operations | Thread safety with parallel storage | CT, MR, XA, US |

### Running Workflow Tests

```bash
# Run all multi-modal workflow tests
./bin/pacs_integration_e2e "[workflow][multimodal]"

# Run with MPPS tracking tests
./bin/pacs_integration_e2e "[workflow][mpps]"

# Run stress tests (hidden by default)
./bin/pacs_integration_e2e "[.stress]"
```

### Workflow Verification Helper

The `workflow_verification` class provides utilities for validating workflow consistency:

```cpp
#include "test_multimodal_workflow.cpp"

// After storing datasets, verify the workflow
workflow_verification verifier(database);

// Check patient exists
REQUIRE(verifier.patient_exists("PATIENT001"));

// Verify study count
REQUIRE(verifier.study_count("PATIENT001") == 1);

// Check modality is present in study
REQUIRE(verifier.study_has_modality(study_uid, "CT"));
REQUIRE(verifier.study_has_modality(study_uid, "MR"));

// Verify series and instance counts
REQUIRE(verifier.series_count(study_uid) == 2);
REQUIRE(verifier.instance_count(study_uid) >= 2);

// Check for duplicate UIDs
REQUIRE_FALSE(verifier.has_duplicate_uids());
```

## Long-Running Stability Tests

The `test_stability.cpp` provides comprehensive tests for system reliability under extended operation. These tests are designed for 24-hour continuous operation testing but include configurable durations.

### Test Scenarios

| Scenario | Tag | Description |
|----------|-----|-------------|
| Continuous Store/Query | `[stability][.slow]` | Extended store/query operations with configurable duration |
| Memory Stability | `[stability][memory]` | Verify no memory leaks over 100 iterations |
| Connection Pool Exhaustion | `[stability][network]` | Open/close many connections, verify recovery |
| Database Integrity | `[stability][database]` | Concurrent writes, verify no data corruption |
| Smoke Test | `[stability][smoke]` | Quick 10-second validation for CI |

### Running Stability Tests

```bash
# Run quick smoke test (10 seconds) - suitable for CI
./bin/pacs_integration_e2e "[stability][smoke]"

# Run memory stability test
./bin/pacs_integration_e2e "[stability][memory]"

# Run all stability tests except long-running ones
./bin/pacs_integration_e2e "[stability]" "~[.slow]"

# Run 24-hour continuous operation test
PACS_STABILITY_TEST_DURATION=1440 ./bin/pacs_integration_e2e "[stability][.slow]"
```

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `PACS_STABILITY_TEST_DURATION` | 60 | Test duration in minutes |
| `PACS_STABILITY_STORE_RATE` | 5.0 | Store operations per second |
| `PACS_STABILITY_QUERY_RATE` | 1.0 | Query operations per second |
| `PACS_STABILITY_STORE_WORKERS` | 4 | Number of concurrent store workers |
| `PACS_STABILITY_QUERY_WORKERS` | 2 | Number of concurrent query workers |

### Stability Metrics

The tests collect and report metrics including:
- Store/query success and failure counts
- Connection statistics (opened, closed, errors)
- Memory usage (initial, peak, growth)
- Operation rates

Reports are automatically saved to `/tmp/stability_test_report.txt`.

### Memory Monitoring

Cross-platform memory monitoring is implemented:
- **Linux**: Reads `/proc/self/status` for VmRSS
- **macOS**: Uses `mach_task_basic_info` for resident memory
- **Windows**: Uses `GetProcessMemoryInfo` for working set

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

## CI/CD Pipeline

Integration tests are automatically executed via GitHub Actions. The pipeline is defined in `.github/workflows/integration-tests.yml`.

### Workflow Jobs

| Job | Trigger | Description |
|-----|---------|-------------|
| `build-and-unit-tests` | All PRs | Build and run unit tests on Ubuntu and macOS |
| `integration-tests` | All PRs | Run library-level integration tests |
| `stability-smoke-tests` | All PRs | Quick 10-second stability validation |
| `binary-integration-tests` | All PRs | Run shell-based binary integration tests |
| `stress-tests` | Main only | Extended stress tests (5 min) |
| `test-summary` | Always | Aggregate and report results |

### Running Tests Locally with CTest Labels

```bash
# Run only unit tests
ctest -L unit --output-on-failure

# Run only integration tests
ctest -L integration --output-on-failure

# Exclude slow tests
ctest -LE slow --output-on-failure

# Run with verbose output
ctest -L integration -V
```

### Test Categories (Catch2 Tags)

| Tag | Description | CI Behavior |
|-----|-------------|-------------|
| `[connectivity]` | Basic DICOM network tests | All PRs |
| `[workflow]` | Clinical workflow tests | All PRs |
| `[multimodal]` | Multi-modality tests | All PRs |
| `[tls]` | TLS/SSL security tests | All PRs |
| `[stability]` | Stability tests | All PRs (smoke only) |
| `[stress]` | Stress/load tests | Main only |
| `[.slow]` | Long-running tests (hidden) | Manual only |

### Viewing Test Results

- **PR Checks**: JUnit reports appear directly in PR check results
- **Workflow Runs**: Detailed logs in Actions tab
- **Artifacts**: XML test reports downloadable for 30 days

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
