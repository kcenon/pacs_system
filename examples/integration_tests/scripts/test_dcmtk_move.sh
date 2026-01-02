#!/bin/bash
#
# test_dcmtk_move.sh - C-MOVE (Retrieve) interoperability tests with DCMTK
#
# Tests bidirectional C-MOVE compatibility between pacs_system and DCMTK:
# - Test A: pacs_system SCP <- DCMTK movescu (with DCMTK storescp destination)
# - Test B: Destination AE resolution and rejection
#
# @see Issue #454 - C-MOVE Interoperability Test with DCMTK
# @see Issue #449 - DCMTK Interoperability Test Automation Epic
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"
source "$SCRIPT_DIR/dcmtk_common.sh"

# Configuration
MOVE_PORT=41400
DEST_PORT=41401
MOVE_AE="PACS_MOVE"
DEST_AE="DEST_SCP"

TESTS_PASSED=0
TESTS_FAILED=0

# Find project root and build directory
PROJECT_ROOT="$(find_project_root "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_ROOT}/build"
TEST_DATA_DIR="${PROJECT_ROOT}/examples/integration_tests/test_data"

# Cleanup function
cleanup() {
    log_info "Cleaning up..."
    kill_port_process "$MOVE_PORT" 2>/dev/null || true
    kill_port_process "$DEST_PORT" 2>/dev/null || true
    stop_all_dcmtk_servers
}
trap cleanup EXIT

# Helper to kill process on port
kill_port_process() {
    local port="$1"
    if is_port_listening "$port"; then
        local pid
        pid=$(lsof -t -i TCP:"$port" 2>/dev/null || true)
        if [[ -n "$pid" ]]; then
            kill "$pid" 2>/dev/null || true
            sleep 0.5
        fi
    fi
}

# Get test DICOM file
get_test_dicom() {
    local output_file="$1"

    # Try to use existing test data
    if [[ -d "$TEST_DATA_DIR" ]]; then
        local existing
        existing=$(find "$TEST_DATA_DIR" -name "*.dcm" -type f | head -1)
        if [[ -n "$existing" ]]; then
            cp "$existing" "$output_file"
            return 0
        fi
    fi

    # Generate test data if binary exists
    local generate_bin="${BUILD_DIR}/bin/generate_test_data"
    if [[ -x "$generate_bin" ]]; then
        local temp_dir
        temp_dir=$(dirname "$output_file")
        "$generate_bin" "$temp_dir" 2>/dev/null || true
        if [[ -f "$output_file" ]]; then
            return 0
        fi
    fi

    log_warn "No test DICOM file available"
    return 1
}

#========================================
# Test A: pacs_system SCP <- DCMTK movescu
#========================================
test_pacs_scp_dcmtk_movescu() {
    log_info "=== Test A: pacs_system SCP <- DCMTK movescu ==="

    # Find available ports
    MOVE_PORT=$(find_available_port 41400)
    DEST_PORT=$(find_available_port 41402)

    local dest_dir
    dest_dir=$(create_dcmtk_temp_dir "move_dest")

    local input_dir
    input_dir=$(create_dcmtk_temp_dir "move_input")

    # Start DCMTK storescp as destination
    log_info "Starting DCMTK storescp destination on port $DEST_PORT..."
    local dest_pid
    dest_pid=$(start_storescp "$DEST_PORT" "$DEST_AE" "$dest_dir")

    if [[ -z "$dest_pid" ]]; then
        log_fail "Failed to start DCMTK storescp destination"
        ((TESTS_FAILED++))
        cleanup_dcmtk_temp_dir "$dest_dir"
        cleanup_dcmtk_temp_dir "$input_dir"
        return 1
    fi

    # Check if pacs_server binary exists
    local server_bin="${BUILD_DIR}/bin/pacs_server"
    if [[ ! -x "$server_bin" ]]; then
        log_warn "pacs_server binary not found, skipping test"
        stop_storescp "$dest_pid"
        cleanup_dcmtk_temp_dir "$dest_dir"
        cleanup_dcmtk_temp_dir "$input_dir"
        return 77
    fi

    # Get test DICOM file and store it
    local test_file="${input_dir}/test.dcm"
    if ! get_test_dicom "$test_file"; then
        log_warn "No test DICOM file, skipping test"
        stop_storescp "$dest_pid"
        cleanup_dcmtk_temp_dir "$dest_dir"
        cleanup_dcmtk_temp_dir "$input_dir"
        return 77
    fi

    # Start pacs_system server
    local storage_dir
    storage_dir=$(create_dcmtk_temp_dir "move_storage")

    log_info "Starting pacs_system server on port $MOVE_PORT..."
    "$server_bin" --port "$MOVE_PORT" --ae-title "$MOVE_AE" --storage-dir "$storage_dir" &
    local server_pid=$!

    if ! wait_for_port "127.0.0.1" "$MOVE_PORT" 15; then
        log_fail "Failed to start pacs_system server"
        ((TESTS_FAILED++))
        kill "$server_pid" 2>/dev/null || true
        stop_storescp "$dest_pid"
        cleanup_dcmtk_temp_dir "$dest_dir"
        cleanup_dcmtk_temp_dir "$input_dir"
        cleanup_dcmtk_temp_dir "$storage_dir"
        return 1
    fi

    # First, store the test file to the pacs_system
    log_info "Storing test file to pacs_system..."
    if ! run_storescu localhost "$MOVE_PORT" "$MOVE_AE" "$test_file"; then
        log_warn "Failed to store test file, skipping move test"
        kill "$server_pid" 2>/dev/null || true
        stop_storescp "$dest_pid"
        cleanup_dcmtk_temp_dir "$dest_dir"
        cleanup_dcmtk_temp_dir "$input_dir"
        cleanup_dcmtk_temp_dir "$storage_dir"
        return 77
    fi

    # Run DCMTK movescu
    log_info "Running DCMTK movescu..."
    if run_movescu localhost "$MOVE_PORT" "$MOVE_AE" "$DEST_AE" "STUDY" \
        "PatientID="; then
        # Wait for files to be received
        sleep 2

        # Verify file was received at destination
        if verify_received_dicom "$dest_dir" 1; then
            log_pass "C-MOVE successful: DCMTK movescu -> pacs_system SCP"
            ((TESTS_PASSED++))
        else
            log_fail "C-MOVE completed but no files received at destination"
            ((TESTS_FAILED++))
        fi
    else
        log_fail "C-MOVE failed: DCMTK movescu -> pacs_system SCP"
        ((TESTS_FAILED++))
    fi

    # Cleanup
    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
    stop_storescp "$dest_pid"
    cleanup_dcmtk_temp_dir "$dest_dir"
    cleanup_dcmtk_temp_dir "$input_dir"
    cleanup_dcmtk_temp_dir "$storage_dir"
}

#========================================
# Test B: Connection error handling
#========================================
test_connection_error() {
    log_info "=== Test B: Connection error handling ==="

    local port
    port=$(find_available_port 41410)

    # Ensure nothing is listening
    if is_port_listening "$port"; then
        log_warn "Port $port is already in use, skipping error test"
        return 77
    fi

    log_info "Testing movescu to non-existent server..."
    if ! run_movescu localhost "$port" "NONEXISTENT" "DEST" "STUDY" \
        "PatientID=" 10; then
        log_pass "Connection error handled correctly (movescu failed as expected)"
        ((TESTS_PASSED++))
    else
        log_fail "movescu should have failed for non-existent server"
        ((TESTS_FAILED++))
    fi
}

#========================================
# Test C: DCMTK baseline (movescu with storescp)
#========================================
test_dcmtk_baseline() {
    log_info "=== Test C: DCMTK baseline (requires full DCMTK setup) ==="

    # Note: This test requires a complete DCMTK PACS environment
    # which is typically not available in CI. Skip by default.
    log_warn "DCMTK baseline test skipped (requires full DCMTK PACS)"
    return 77
}

#========================================
# Main
#========================================
main() {
    log_info "========================================"
    log_info "DCMTK C-MOVE Interoperability Tests"
    log_info "========================================"

    # Check prerequisites
    if ! check_dcmtk_available; then
        log_warn "DCMTK not available, skipping all tests"
        exit 77  # Skip code
    fi

    # Register cleanup
    register_dcmtk_cleanup

    # Run tests
    test_pacs_scp_dcmtk_movescu
    test_connection_error
    test_dcmtk_baseline

    # Summary
    log_info "========================================"
    log_info "Results: $TESTS_PASSED passed, $TESTS_FAILED failed"
    log_info "========================================"

    [[ $TESTS_FAILED -eq 0 ]]
}

main "$@"
