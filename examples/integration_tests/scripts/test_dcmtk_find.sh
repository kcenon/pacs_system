#!/bin/bash
#
# test_dcmtk_find.sh - C-FIND (Query) interoperability tests with DCMTK
#
# Tests bidirectional C-FIND compatibility between pacs_system and DCMTK:
# - Test A: pacs_system SCP <- DCMTK findscu
# - Test B: Query response verification
#
# @see Issue #453 - C-FIND Interoperability Test with DCMTK
# @see Issue #449 - DCMTK Interoperability Test Automation Epic
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"
source "$SCRIPT_DIR/dcmtk_common.sh"

# Configuration
PACS_PORT=41300
PACS_AE="PACS_FIND"

TESTS_PASSED=0
TESTS_FAILED=0

# Find project root and build directory
PROJECT_ROOT="$(find_project_root "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_ROOT}/build"

# Cleanup function
cleanup() {
    log_info "Cleaning up..."
    kill_port_process "$PACS_PORT" 2>/dev/null || true
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
# Test A: pacs_system SCP <- DCMTK findscu
#========================================
test_pacs_scp_dcmtk_scu() {
    log_info "=== Test A: pacs_system SCP <- DCMTK findscu ==="

    # Find available port
    PACS_PORT=$(find_available_port 41300)

    # Check if pacs_server binary exists
    local server_bin="${BUILD_DIR}/bin/pacs_server"
    if [[ ! -x "$server_bin" ]]; then
        log_warn "pacs_server binary not found, skipping test"
        return 77
    fi

    # Start pacs_system server
    log_info "Starting pacs_system server on port $PACS_PORT..."
    "$server_bin" "$PACS_PORT" "$PACS_AE" &
    local server_pid=$!

    if ! wait_for_port "127.0.0.1" "$PACS_PORT" 15; then
        log_fail "Failed to start pacs_system server"
        ((TESTS_FAILED++))
        kill "$server_pid" 2>/dev/null || true
        return 1
    fi

    # Run DCMTK findscu with study query
    log_info "Running DCMTK findscu (STUDY level)..."
    if run_findscu localhost "$PACS_PORT" "$PACS_AE" "STUDY" \
        "PatientID=" "StudyInstanceUID="; then
        log_pass "C-FIND successful: DCMTK findscu -> pacs_system SCP"
        ((TESTS_PASSED++))
    else
        log_fail "C-FIND failed: DCMTK findscu -> pacs_system SCP"
        ((TESTS_FAILED++))
    fi

    # Cleanup
    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
}

#========================================
# Test B: Multiple query levels
#========================================
test_query_levels() {
    log_info "=== Test B: Multiple query levels ==="

    # Find available port
    PACS_PORT=$(find_available_port 41302)

    local server_bin="${BUILD_DIR}/bin/pacs_server"
    if [[ ! -x "$server_bin" ]]; then
        log_warn "pacs_server binary not found, skipping test"
        return 77
    fi

    log_info "Starting pacs_system server on port $PACS_PORT..."
    "$server_bin" "$PACS_PORT" "$PACS_AE" &
    local server_pid=$!

    if ! wait_for_port "127.0.0.1" "$PACS_PORT" 15; then
        log_fail "Failed to start pacs_system server"
        ((TESTS_FAILED++))
        kill "$server_pid" 2>/dev/null || true
        return 1
    fi

    local all_passed=true

    # Test STUDY level
    log_info "Testing STUDY level query..."
    if ! run_findscu localhost "$PACS_PORT" "$PACS_AE" "STUDY" \
        "PatientID=" "StudyInstanceUID="; then
        log_fail "STUDY level query failed"
        all_passed=false
    fi

    # Test PATIENT level
    log_info "Testing PATIENT level query..."
    if ! run_findscu localhost "$PACS_PORT" "$PACS_AE" "PATIENT" \
        "PatientID=" "PatientName="; then
        log_fail "PATIENT level query failed"
        all_passed=false
    fi

    if $all_passed; then
        log_pass "Multiple query levels test passed"
        ((TESTS_PASSED++))
    else
        log_fail "Multiple query levels test failed"
        ((TESTS_FAILED++))
    fi

    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
}

#========================================
# Test C: Wildcard query
#========================================
test_wildcard_query() {
    log_info "=== Test C: Wildcard query ==="

    PACS_PORT=$(find_available_port 41304)

    local server_bin="${BUILD_DIR}/bin/pacs_server"
    if [[ ! -x "$server_bin" ]]; then
        log_warn "pacs_server binary not found, skipping test"
        return 77
    fi

    log_info "Starting pacs_system server on port $PACS_PORT..."
    "$server_bin" "$PACS_PORT" "$PACS_AE" &
    local server_pid=$!

    if ! wait_for_port "127.0.0.1" "$PACS_PORT" 15; then
        log_fail "Failed to start pacs_system server"
        ((TESTS_FAILED++))
        kill "$server_pid" 2>/dev/null || true
        return 1
    fi

    # Run findscu with wildcard
    log_info "Running DCMTK findscu with wildcard..."
    if run_findscu localhost "$PACS_PORT" "$PACS_AE" "STUDY" \
        "PatientName=SMITH*" "StudyInstanceUID="; then
        log_pass "Wildcard query successful"
        ((TESTS_PASSED++))
    else
        log_fail "Wildcard query failed"
        ((TESTS_FAILED++))
    fi

    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
}

#========================================
# Test D: Date range query
#========================================
test_date_range_query() {
    log_info "=== Test D: Date range query ==="

    PACS_PORT=$(find_available_port 41306)

    local server_bin="${BUILD_DIR}/bin/pacs_server"
    if [[ ! -x "$server_bin" ]]; then
        log_warn "pacs_server binary not found, skipping test"
        return 77
    fi

    log_info "Starting pacs_system server on port $PACS_PORT..."
    "$server_bin" "$PACS_PORT" "$PACS_AE" &
    local server_pid=$!

    if ! wait_for_port "127.0.0.1" "$PACS_PORT" 15; then
        log_fail "Failed to start pacs_system server"
        ((TESTS_FAILED++))
        kill "$server_pid" 2>/dev/null || true
        return 1
    fi

    # Run findscu with date range
    log_info "Running DCMTK findscu with date range..."
    if run_findscu localhost "$PACS_PORT" "$PACS_AE" "STUDY" \
        "StudyDate=20231201-20240131" "StudyInstanceUID="; then
        log_pass "Date range query successful"
        ((TESTS_PASSED++))
    else
        log_fail "Date range query failed"
        ((TESTS_FAILED++))
    fi

    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
}

#========================================
# Test E: Connection error handling
#========================================
test_connection_error() {
    log_info "=== Test E: Connection error handling ==="

    local port
    port=$(find_available_port 41308)

    # Ensure nothing is listening
    if is_port_listening "$port"; then
        log_warn "Port $port is already in use, skipping error test"
        return 77
    fi

    log_info "Testing findscu to non-existent server..."
    if ! run_findscu localhost "$port" "NONEXISTENT" "STUDY" \
        "PatientID=" 5; then
        log_pass "Connection error handled correctly (findscu failed as expected)"
        ((TESTS_PASSED++))
    else
        log_fail "findscu should have failed for non-existent server"
        ((TESTS_FAILED++))
    fi
}

#========================================
# Main
#========================================
main() {
    log_info "========================================"
    log_info "DCMTK C-FIND Interoperability Tests"
    log_info "========================================"

    # Check prerequisites
    if ! check_dcmtk_available; then
        log_warn "DCMTK not available, skipping all tests"
        exit 77  # Skip code
    fi

    # Register cleanup
    register_dcmtk_cleanup

    # Run tests
    test_pacs_scp_dcmtk_scu
    test_query_levels
    test_wildcard_query
    test_date_range_query
    test_connection_error

    # Summary
    log_info "========================================"
    log_info "Results: $TESTS_PASSED passed, $TESTS_FAILED failed"
    log_info "========================================"

    [[ $TESTS_FAILED -eq 0 ]]
}

main "$@"
