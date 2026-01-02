#!/bin/bash
#
# test_dcmtk_store.sh - C-STORE interoperability tests with DCMTK
#
# Tests bidirectional C-STORE compatibility between pacs_system and DCMTK:
# - Test A: pacs_system SCP <- DCMTK storescu
# - Test B: DCMTK storescp <- pacs_system store_scu
#
# @see Issue #452 - C-STORE Bidirectional Interoperability Test with DCMTK
# @see Issue #449 - DCMTK Interoperability Test Automation Epic
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"
source "$SCRIPT_DIR/dcmtk_common.sh"

# Configuration
PACS_PORT=41200
DCMTK_PORT=41201
PACS_AE="PACS_STORE"
DCMTK_AE="DCMTK_STORE"

TESTS_PASSED=0
TESTS_FAILED=0

# Find project root and build directory
PROJECT_ROOT="$(find_project_root "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_ROOT}/build"
TEST_DATA_DIR="${PROJECT_ROOT}/examples/integration_tests/test_data"

# Cleanup function
cleanup() {
    log_info "Cleaning up..."
    kill_port_process "$PACS_PORT" 2>/dev/null || true
    kill_port_process "$DCMTK_PORT" 2>/dev/null || true
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
    local modality="${2:-CT}"

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
# Test A: pacs_system SCP <- DCMTK storescu
#========================================
test_pacs_scp_dcmtk_scu() {
    log_info "=== Test A: pacs_system SCP <- DCMTK storescu ==="

    # Find available port
    PACS_PORT=$(find_available_port 41200)

    local storage_dir
    storage_dir=$(create_dcmtk_temp_dir "pacs_storage")

    local input_dir
    input_dir=$(create_dcmtk_temp_dir "store_input")

    # Check if pacs_server binary exists
    local server_bin="${BUILD_DIR}/bin/pacs_server"
    if [[ ! -x "$server_bin" ]]; then
        log_warn "pacs_server binary not found, skipping test"
        cleanup_dcmtk_temp_dir "$storage_dir"
        cleanup_dcmtk_temp_dir "$input_dir"
        return 77
    fi

    # Get test DICOM file
    local test_file="${input_dir}/test.dcm"
    if ! get_test_dicom "$test_file"; then
        log_warn "No test DICOM file, skipping test"
        cleanup_dcmtk_temp_dir "$storage_dir"
        cleanup_dcmtk_temp_dir "$input_dir"
        return 77
    fi

    # Start pacs_system server
    log_info "Starting pacs_system server on port $PACS_PORT..."
    "$server_bin" --port "$PACS_PORT" --ae-title "$PACS_AE" --storage-dir "$storage_dir" &
    local server_pid=$!

    if ! wait_for_port "127.0.0.1" "$PACS_PORT" 15; then
        log_fail "Failed to start pacs_system server"
        ((TESTS_FAILED++))
        kill "$server_pid" 2>/dev/null || true
        cleanup_dcmtk_temp_dir "$storage_dir"
        cleanup_dcmtk_temp_dir "$input_dir"
        return 1
    fi

    # Run DCMTK storescu
    log_info "Running DCMTK storescu..."
    if run_storescu localhost "$PACS_PORT" "$PACS_AE" "$test_file"; then
        # Verify file was stored
        if verify_received_dicom "$storage_dir" 1; then
            log_pass "C-STORE successful: DCMTK storescu -> pacs_system SCP"
            ((TESTS_PASSED++))
        else
            log_fail "C-STORE completed but no files stored"
            ((TESTS_FAILED++))
        fi
    else
        log_fail "C-STORE failed: DCMTK storescu -> pacs_system SCP"
        ((TESTS_FAILED++))
    fi

    # Cleanup
    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
    cleanup_dcmtk_temp_dir "$storage_dir"
    cleanup_dcmtk_temp_dir "$input_dir"
}

#========================================
# Test B: DCMTK storescp <- pacs_system store_scu
#========================================
test_dcmtk_scp_pacs_scu() {
    log_info "=== Test B: DCMTK storescp <- pacs_system store_scu ==="

    # Find available port
    DCMTK_PORT=$(find_available_port 41202)

    local storage_dir
    storage_dir=$(create_dcmtk_temp_dir "dcmtk_storage")

    local input_dir
    input_dir=$(create_dcmtk_temp_dir "pacs_input")

    # Start DCMTK storescp
    log_info "Starting DCMTK storescp on port $DCMTK_PORT..."
    local scp_pid
    scp_pid=$(start_storescp "$DCMTK_PORT" "$DCMTK_AE" "$storage_dir")

    if [[ -z "$scp_pid" ]]; then
        log_fail "Failed to start DCMTK storescp"
        ((TESTS_FAILED++))
        cleanup_dcmtk_temp_dir "$storage_dir"
        cleanup_dcmtk_temp_dir "$input_dir"
        return 1
    fi

    # Check if store_scu binary exists
    local scu_bin="${BUILD_DIR}/bin/store_scu"
    if [[ ! -x "$scu_bin" ]]; then
        log_warn "store_scu binary not found, skipping test"
        stop_storescp "$scp_pid"
        cleanup_dcmtk_temp_dir "$storage_dir"
        cleanup_dcmtk_temp_dir "$input_dir"
        return 77
    fi

    # Get test DICOM file
    local test_file="${input_dir}/test.dcm"
    if ! get_test_dicom "$test_file"; then
        log_warn "No test DICOM file, skipping test"
        stop_storescp "$scp_pid"
        cleanup_dcmtk_temp_dir "$storage_dir"
        cleanup_dcmtk_temp_dir "$input_dir"
        return 77
    fi

    # Run pacs_system store_scu
    log_info "Running pacs_system store_scu..."
    if "$scu_bin" localhost "$DCMTK_PORT" "$DCMTK_AE" "$test_file"; then
        # Wait for file to be written
        sleep 1

        # Verify file was stored
        if verify_received_dicom "$storage_dir" 1; then
            log_pass "C-STORE successful: pacs_system store_scu -> DCMTK storescp"
            ((TESTS_PASSED++))
        else
            log_fail "C-STORE completed but no files received"
            ((TESTS_FAILED++))
        fi
    else
        log_fail "C-STORE failed: pacs_system store_scu -> DCMTK storescp"
        ((TESTS_FAILED++))
    fi

    # Cleanup
    stop_storescp "$scp_pid"
    cleanup_dcmtk_temp_dir "$storage_dir"
    cleanup_dcmtk_temp_dir "$input_dir"
}

#========================================
# Test C: DCMTK baseline (storescp <- storescu)
#========================================
test_dcmtk_baseline() {
    log_info "=== Test C: DCMTK storescp <- DCMTK storescu (baseline) ==="

    local port
    port=$(find_available_port 41204)

    local storage_dir
    storage_dir=$(create_dcmtk_temp_dir "baseline_storage")

    local input_dir
    input_dir=$(create_dcmtk_temp_dir "baseline_input")

    # Start DCMTK storescp
    log_info "Starting DCMTK storescp on port $port..."
    local scp_pid
    scp_pid=$(start_storescp "$port" "BASELINE_SCP" "$storage_dir")

    if [[ -z "$scp_pid" ]]; then
        log_fail "Failed to start DCMTK storescp"
        ((TESTS_FAILED++))
        cleanup_dcmtk_temp_dir "$storage_dir"
        cleanup_dcmtk_temp_dir "$input_dir"
        return 1
    fi

    # Get test DICOM file
    local test_file="${input_dir}/test.dcm"
    if ! get_test_dicom "$test_file"; then
        log_warn "No test DICOM file, skipping baseline test"
        stop_storescp "$scp_pid"
        cleanup_dcmtk_temp_dir "$storage_dir"
        cleanup_dcmtk_temp_dir "$input_dir"
        return 77
    fi

    # Run DCMTK storescu
    log_info "Running DCMTK storescu..."
    if run_storescu localhost "$port" "BASELINE_SCP" "$test_file"; then
        sleep 1
        if verify_received_dicom "$storage_dir" 1; then
            log_pass "Baseline C-STORE successful: DCMTK storescu -> DCMTK storescp"
            ((TESTS_PASSED++))
        else
            log_fail "Baseline C-STORE completed but no files received"
            ((TESTS_FAILED++))
        fi
    else
        log_fail "Baseline C-STORE failed"
        ((TESTS_FAILED++))
    fi

    # Cleanup
    stop_storescp "$scp_pid"
    cleanup_dcmtk_temp_dir "$storage_dir"
    cleanup_dcmtk_temp_dir "$input_dir"
}

#========================================
# Test D: Multiple file storage
#========================================
test_multiple_files() {
    log_info "=== Test D: Multiple file storage ==="

    local port
    port=$(find_available_port 41206)

    local storage_dir
    storage_dir=$(create_dcmtk_temp_dir "multi_storage")

    local input_dir
    input_dir=$(create_dcmtk_temp_dir "multi_input")

    # Start DCMTK storescp
    local scp_pid
    scp_pid=$(start_storescp "$port" "MULTI_SCP" "$storage_dir")

    if [[ -z "$scp_pid" ]]; then
        log_fail "Failed to start DCMTK storescp"
        ((TESTS_FAILED++))
        cleanup_dcmtk_temp_dir "$storage_dir"
        cleanup_dcmtk_temp_dir "$input_dir"
        return 1
    fi

    # Create multiple test files
    local files=()
    for i in {1..3}; do
        local test_file="${input_dir}/test_${i}.dcm"
        if get_test_dicom "$test_file"; then
            files+=("$test_file")
        fi
    done

    if [[ ${#files[@]} -eq 0 ]]; then
        log_warn "No test DICOM files, skipping multi-file test"
        stop_storescp "$scp_pid"
        cleanup_dcmtk_temp_dir "$storage_dir"
        cleanup_dcmtk_temp_dir "$input_dir"
        return 77
    fi

    # Run storescu with multiple files
    log_info "Running DCMTK storescu with ${#files[@]} files..."
    if run_storescu_multi localhost "$port" "MULTI_SCP" "${files[@]}"; then
        sleep 1
        local stored_count
        stored_count=$(count_dicom_files "$storage_dir")
        if [[ "$stored_count" -ge "${#files[@]}" ]]; then
            log_pass "Multiple file C-STORE successful ($stored_count files)"
            ((TESTS_PASSED++))
        else
            log_fail "Expected ${#files[@]} files, got $stored_count"
            ((TESTS_FAILED++))
        fi
    else
        log_fail "Multiple file C-STORE failed"
        ((TESTS_FAILED++))
    fi

    # Cleanup
    stop_storescp "$scp_pid"
    cleanup_dcmtk_temp_dir "$storage_dir"
    cleanup_dcmtk_temp_dir "$input_dir"
}

#========================================
# Main
#========================================
main() {
    log_info "========================================"
    log_info "DCMTK C-STORE Interoperability Tests"
    log_info "========================================"

    # Check prerequisites
    if ! check_dcmtk_available; then
        log_warn "DCMTK not available, skipping all tests"
        exit 77
    fi

    # Register cleanup
    register_dcmtk_cleanup

    # Run tests
    test_dcmtk_baseline
    test_pacs_scp_dcmtk_scu
    test_dcmtk_scp_pacs_scu
    test_multiple_files

    # Summary
    log_info "========================================"
    log_info "Results: $TESTS_PASSED passed, $TESTS_FAILED failed"
    log_info "========================================"

    [[ $TESTS_FAILED -eq 0 ]]
}

main "$@"
