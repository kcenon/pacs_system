# Binary Integration Test Scripts

Shell scripts for testing PACS binaries at the process level.

## Quick Start

```bash
# Run all tests
./run_all_binary_tests.sh

# Run individual test suites
./test_connectivity.sh      # Echo SCP/SCU tests
./test_store_retrieve.sh    # Storage workflow tests
./test_worklist_mpps.sh     # RIS workflow tests
./test_secure_dicom.sh      # TLS communication tests

# Run DCMTK interoperability tests
./test_dcmtk_echo.sh        # DCMTK C-ECHO tests
./test_dcmtk_store.sh       # DCMTK C-STORE tests
./test_dcmtk_find.sh        # DCMTK C-FIND tests
./test_dcmtk_move.sh        # DCMTK C-MOVE tests
```

## Script Descriptions

| Script | Purpose | Binaries/Tools Tested |
|--------|---------|----------------------|
| `common.sh` | Shared utilities (sourced by other scripts) | - |
| `dcmtk_common.sh` | DCMTK interoperability helpers (Issue #450) | - |
| `test_connectivity.sh` | Basic DICOM echo operations | echo_scp, echo_scu |
| `test_store_retrieve.sh` | Storage and query workflow | pacs_server, store_scu, query_scu |
| `test_worklist_mpps.sh` | Modality worklist and MPPS | worklist_scu, mpps_scu |
| `test_secure_dicom.sh` | TLS encrypted communication | echo_scp, echo_scu (with TLS) |
| `test_dcmtk_echo.sh` | DCMTK C-ECHO interoperability (Issue #451) | echoscu, pacs_server |
| `test_dcmtk_store.sh` | DCMTK C-STORE interoperability (Issue #452) | storescu, storescp, pacs_server |
| `test_dcmtk_find.sh` | DCMTK C-FIND interoperability (Issue #453) | findscu, pacs_server |
| `test_dcmtk_move.sh` | DCMTK C-MOVE interoperability (Issue #454) | movescu, storescp, pacs_server |
| `run_all_binary_tests.sh` | Master test runner | All |

## Options

```bash
./run_all_binary_tests.sh [OPTIONS] [build_dir]

Options:
  -v, --verbose    Show detailed test output
  -p, --parallel   Run test suites in parallel
  -h, --help       Show help message

Example:
  ./run_all_binary_tests.sh -v -p /path/to/build
```

## Exit Codes

- `0` - All tests passed
- `1` - One or more tests failed
- `2` - Prerequisites not met (binaries missing)

## See Also

- [Integration Tests README](../README.md) - Full documentation
- [Test Fixtures](../test_fixtures.hpp) - C++ utilities
