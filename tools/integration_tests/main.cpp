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

int main(int argc, char* argv[]) {
    // Catch owns stdout, including machine-readable discovery and XML reporters.
    return Catch::Session().run(argc, argv);
}
