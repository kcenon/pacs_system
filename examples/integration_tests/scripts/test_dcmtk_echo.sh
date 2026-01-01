#!/bin/bash
#
# test_dcmtk_echo.sh - C-ECHO (Verification) interoperability tests with DCMTK
#
# Tests bidirectional C-ECHO compatibility between pacs_system and DCMTK:
# - Test A: pacs_system SCP <- DCMTK echoscu
# - Test B: DCMTK storescp <- pacs_system echo_scu
#
# @see Issue #451 - C-ECHO Bidirectional Interoperability Test with DCMTK
# @see Issue #449 - DCMTK Interoperability Test Automation Epic
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"
source "$SCRIPT_DIR/dcmtk_common.sh"

# Configuration
PACS_PORT=41112
DCMTK_PORT=41113
PACS_AE="PACS_ECHO"
DCMTK_AE="DCMTK_ECHO"

TESTS_PASSED=0
TESTS_FAILED=0

# Find project root and build directory
PROJECT_ROOT="$(find_project_root "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_ROOT}/build"

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

#========================================
# Test A: pacs_system SCP <- DCMTK echoscu
#========================================
test_pacs_scp_dcmtk_scu() {
    log_info "=== Test A: pacs_system SCP <- DCMTK echoscu ==="

    # Find available port
    PACS_PORT=$(find_available_port 41112)

    # Check if echo_scp binary exists
    local echo_scp_bin="${BUILD_DIR}/bin/echo_scp"
    if [[ ! -x "$echo_scp_bin" ]]; then
        # Try alternative location
        echo_scp_bin="${BUILD_DIR}/bin/pacs_echo_scp"
    fi

    if [[ ! -x "$echo_scp_bin" ]]; then
        log_warn "echo_scp binary not found, using dicom_server_sample"
        # Use dicom_server_sample if echo_scp is not available
        echo_scp_bin="${BUILD_DIR}/bin/dicom_server_sample"
    fi

    if [[ ! -x "$echo_scp_bin" ]]; then
        log_warn "No suitable echo server binary found, skipping test"
        return 77  # Skip
    fi

    # Start pacs_system echo server
    log_info "Starting pacs_system echo server on port $PACS_PORT..."
    "$echo_scp_bin" "$PACS_PORT" "$PACS_AE" &
    local server_pid=$!

    if ! wait_for_port "127.0.0.1" "$PACS_PORT" 10; then
        log_fail "Failed to start pacs_system echo server"
        ((TESTS_FAILED++))
        kill "$server_pid" 2>/dev/null || true
        return 1
    fi

    # Run DCMTK echoscu
    log_info "Running DCMTK echoscu..."
    if run_echoscu localhost "$PACS_PORT" "$PACS_AE"; then
        log_pass "C-ECHO successful: DCMTK echoscu -> pacs_system SCP"
        ((TESTS_PASSED++))
    else
        log_fail "C-ECHO failed: DCMTK echoscu -> pacs_system SCP"
        ((TESTS_FAILED++))
    fi

    # Cleanup
    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
}

#========================================
# Test A2: Multiple consecutive echoes
#========================================
test_pacs_scp_multiple_echoes() {
    log_info "=== Test A2: Multiple consecutive echoes to pacs_system ==="

    # Find available port
    PACS_PORT=$(find_available_port 41114)

    local echo_scp_bin="${BUILD_DIR}/bin/echo_scp"
    if [[ ! -x "$echo_scp_bin" ]]; then
        echo_scp_bin="${BUILD_DIR}/bin/dicom_server_sample"
    fi

    if [[ ! -x "$echo_scp_bin" ]]; then
        log_warn "No suitable echo server binary found, skipping test"
        return 77
    fi

    log_info "Starting pacs_system echo server on port $PACS_PORT..."
    "$echo_scp_bin" "$PACS_PORT" "$PACS_AE" &
    local server_pid=$!

    if ! wait_for_port "127.0.0.1" "$PACS_PORT" 10; then
        log_fail "Failed to start pacs_system echo server"
        ((TESTS_FAILED++))
        kill "$server_pid" 2>/dev/null || true
        return 1
    fi

    local all_passed=true
    for i in {1..5}; do
        log_info "Running echo $i/5..."
        if ! run_echoscu localhost "$PACS_PORT" "$PACS_AE" "ECHOSCU_$i" 10; then
            log_fail "C-ECHO $i failed"
            all_passed=false
            break
        fi
    done

    if $all_passed; then
        log_pass "Multiple C-ECHO successful (5/5)"
        ((TESTS_PASSED++))
    else
        log_fail "Multiple C-ECHO failed"
        ((TESTS_FAILED++))
    fi

    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
}

#========================================
# Test B: DCMTK SCP <- pacs_system echo_scu
#========================================
test_dcmtk_scp_pacs_scu() {
    log_info "=== Test B: DCMTK storescp <- pacs_system echo_scu ==="

    # Find available port
    DCMTK_PORT=$(find_available_port 41116)

    local temp_dir
    temp_dir=$(create_dcmtk_temp_dir "dcmtk_echo")

    # Start DCMTK storescp (also accepts echo)
    log_info "Starting DCMTK storescp on port $DCMTK_PORT..."
    local scp_pid
    scp_pid=$(start_storescp "$DCMTK_PORT" "$DCMTK_AE" "$temp_dir")

    if [[ -z "$scp_pid" ]]; then
        log_fail "Failed to start DCMTK storescp"
        ((TESTS_FAILED++))
        cleanup_dcmtk_temp_dir "$temp_dir"
        return 1
    fi

    # Check if echo_scu binary exists
    local echo_scu_bin="${BUILD_DIR}/bin/echo_scu"
    if [[ ! -x "$echo_scu_bin" ]]; then
        echo_scu_bin="${BUILD_DIR}/bin/pacs_echo_scu"
    fi

    if [[ ! -x "$echo_scu_bin" ]]; then
        log_warn "echo_scu binary not found, skipping test"
        stop_storescp "$scp_pid"
        cleanup_dcmtk_temp_dir "$temp_dir"
        return 77
    fi

    # Run pacs_system echo_scu
    log_info "Running pacs_system echo_scu..."
    if "$echo_scu_bin" localhost "$DCMTK_PORT" "$DCMTK_AE"; then
        log_pass "C-ECHO successful: pacs_system echo_scu -> DCMTK SCP"
        ((TESTS_PASSED++))
    else
        log_fail "C-ECHO failed: pacs_system echo_scu -> DCMTK SCP"
        ((TESTS_FAILED++))
    fi

    # Cleanup
    stop_storescp "$scp_pid"
    cleanup_dcmtk_temp_dir "$temp_dir"
}

#========================================
# Test C: DCMTK echoscp <- DCMTK echoscu (baseline)
#========================================
test_dcmtk_baseline() {
    log_info "=== Test C: DCMTK echoscp <- DCMTK echoscu (baseline) ==="

    # Find available port
    local port
    port=$(find_available_port 41118)

    # Start DCMTK echoscp
    log_info "Starting DCMTK echoscp on port $port..."
    local scp_pid
    scp_pid=$(start_echoscp "$port" "BASELINE_SCP")

    if [[ -z "$scp_pid" ]]; then
        log_fail "Failed to start DCMTK echoscp"
        ((TESTS_FAILED++))
        return 1
    fi

    # Run DCMTK echoscu
    log_info "Running DCMTK echoscu..."
    if run_echoscu localhost "$port" "BASELINE_SCP"; then
        log_pass "Baseline C-ECHO successful: DCMTK echoscu -> DCMTK echoscp"
        ((TESTS_PASSED++))
    else
        log_fail "Baseline C-ECHO failed"
        ((TESTS_FAILED++))
    fi

    # Cleanup
    stop_echoscp "$scp_pid"
}

#========================================
# Test D: Connection error handling
#========================================
test_connection_error() {
    log_info "=== Test D: Connection error handling ==="

    local port
    port=$(find_available_port 41120)

    # Ensure nothing is listening
    if is_port_listening "$port"; then
        log_warn "Port $port is already in use, skipping error test"
        return 77
    fi

    log_info "Testing echoscu to non-existent server..."
    if ! run_echoscu localhost "$port" "NONEXISTENT" "ECHOSCU" 5; then
        log_pass "Connection error handled correctly (echoscu failed as expected)"
        ((TESTS_PASSED++))
    else
        log_fail "echoscu should have failed for non-existent server"
        ((TESTS_FAILED++))
    fi
}

#========================================
# Main
#========================================
main() {
    log_info "========================================"
    log_info "DCMTK C-ECHO Interoperability Tests"
    log_info "========================================"

    # Check prerequisites
    if ! check_dcmtk_available; then
        log_warn "DCMTK not available, skipping all tests"
        exit 77  # Skip code
    fi

    # Register cleanup
    register_dcmtk_cleanup

    # Run tests
    test_dcmtk_baseline
    test_pacs_scp_dcmtk_scu
    test_pacs_scp_multiple_echoes
    test_dcmtk_scp_pacs_scu
    test_connection_error

    # Summary
    log_info "========================================"
    log_info "Results: $TESTS_PASSED passed, $TESTS_FAILED failed"
    log_info "========================================"

    [[ $TESTS_FAILED -eq 0 ]]
}

main "$@"
