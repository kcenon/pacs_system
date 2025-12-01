/**
 * @file main.cpp
 * @brief Integration Test Suite entry point
 *
 * Main entry point for the PACS integration test suite using Catch2.
 * Executes end-to-end workflow tests validating complete DICOM operations.
 *
 * @see Issue #111 - Integration Test Suite
 *
 * Test Categories:
 * - [connectivity] - Basic DICOM connectivity tests (C-ECHO)
 * - [store_query]  - Store and query workflow tests
 * - [worklist]     - Worklist and MPPS workflow tests
 * - [stress]       - Multi-association stress tests
 * - [error]        - Error recovery tests
 *
 * Usage:
 *   integration_tests                    # Run all tests
 *   integration_tests [connectivity]     # Run only connectivity tests
 *   integration_tests --list-tests       # List all available tests
 */

#define CATCH_CONFIG_RUNNER
#include <catch2/catch_session.hpp>

#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << R"(
  ___ _   _ _____ _____ ____ ____      _  _____ ___ ___  _   _
 |_ _| \ | |_   _| ____/ ___|  _ \    / \|_   _|_ _/ _ \| \ | |
  | ||  \| | | | |  _|| |  _| |_) |  / _ \ | |  | | | | |  \| |
  | || |\  | | | | |__| |_| |  _ <  / ___ \| |  | | |_| | |\  |
 |___|_| \_| |_| |_____\____|_| \_\/_/   \_\_| |___\___/|_| \_|

                    PACS Integration Test Suite
                    ===========================

)" << "\n";

    Catch::Session session;

    // Apply command line arguments
    int result = session.applyCommandLine(argc, argv);
    if (result != 0) {
        return result;
    }

    // Run tests
    result = session.run();

    std::cout << "\n=== Integration Test Suite Complete ===\n";

    return result;
}
