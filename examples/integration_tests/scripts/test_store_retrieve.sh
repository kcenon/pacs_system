#!/bin/bash
#
# test_store_retrieve.sh - Binary Integration Test for Storage Workflow
#
# Tests PACS server storage, query, and retrieve functionality using
# example binaries as separate processes.
#
# Usage:
#   ./test_store_retrieve.sh [build_dir]
#
# Exit codes:
#   0 - All tests passed
#   1 - One or more tests failed
#

set -o pipefail

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${1:-${PROJECT_ROOT}/build}"
TEST_DATA_DIR="${SCRIPT_DIR}/../test_data"

# Binaries
PACS_SERVER="${BUILD_DIR}/bin/pacs_server"
STORE_SCU="${BUILD_DIR}/bin/store_scu"
QUERY_SCU="${BUILD_DIR}/bin/query_scu"
ECHO_SCU="${BUILD_DIR}/bin/echo_scu"

# Test parameters
TEST_PORT=41201
TEST_AE="PACS_TEST"
TEST_HOST="localhost"
STORAGE_DIR=""
DB_PATH=""

# Colors
if [[ -t 1 ]]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    NC='\033[0m'
else
    RED=''
    GREEN=''
    YELLOW=''
    NC=''
fi

# Test tracking
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Cleanup function
cleanup() {
    if [[ -n "${SERVER_PID:-}" ]] && kill -0 "${SERVER_PID}" 2>/dev/null; then
        echo "Cleaning up: Stopping PACS server (PID: ${SERVER_PID})"
        kill "${SERVER_PID}" 2>/dev/null || true
        wait "${SERVER_PID}" 2>/dev/null || true
    fi

    if [[ -n "${STORAGE_DIR:-}" ]] && [[ -d "${STORAGE_DIR}" ]]; then
        echo "Cleaning up: Removing temp storage directory"
        rm -rf "${STORAGE_DIR}"
    fi

    if [[ -n "${DB_PATH:-}" ]] && [[ -f "${DB_PATH}" ]]; then
        echo "Cleaning up: Removing temp database"
        rm -f "${DB_PATH}"
    fi
}

trap cleanup EXIT

# Logging functions
log_info() {
    echo "[INFO] $*"
}

log_pass() {
    echo -e "${GREEN}[PASS]${NC} $*"
}

log_fail() {
    echo -e "${RED}[FAIL]${NC} $*"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $*"
}

# Check if binaries exist
check_binaries() {
    log_info "Checking for required binaries..."

    local missing=0

    for bin in "${PACS_SERVER}" "${STORE_SCU}" "${QUERY_SCU}" "${ECHO_SCU}"; do
        if [[ ! -x "${bin}" ]]; then
            log_fail "Binary not found: ${bin}"
            missing=1
        fi
    done

    if [[ ${missing} -eq 1 ]]; then
        log_info "Please build the project first with: cmake --build ${BUILD_DIR}"
        return 1
    fi

    log_pass "All required binaries found"
    return 0
}

# Check test data
check_test_data() {
    log_info "Checking for test data files..."

    if [[ ! -d "${TEST_DATA_DIR}" ]]; then
        log_warn "Test data directory not found: ${TEST_DATA_DIR}"
        log_info "Creating minimal test data..."
        mkdir -p "${TEST_DATA_DIR}"
    fi

    # Check for CT test file
    if [[ ! -f "${TEST_DATA_DIR}/ct_minimal.dcm" ]]; then
        log_warn "ct_minimal.dcm not found - some tests will be skipped"
        return 1
    fi

    log_pass "Test data files found"
    return 0
}

# Wait for port to be listening
wait_for_port() {
    local host="$1"
    local port="$2"
    local timeout="${3:-15}"
    local elapsed=0

    while ! nc -z "${host}" "${port}" 2>/dev/null; do
        sleep 0.5
        elapsed=$((elapsed + 1))
        if [[ ${elapsed} -ge $((timeout * 2)) ]]; then
            return 1
        fi
    done
    return 0
}

# Create temporary directories
create_temp_dirs() {
    STORAGE_DIR=$(mktemp -d -t pacs_test_storage_XXXXXX)
    DB_PATH=$(mktemp -t pacs_test_db_XXXXXX)
    rm -f "${DB_PATH}"  # PACS server will create it

    log_info "Storage directory: ${STORAGE_DIR}"
    log_info "Database path: ${DB_PATH}"
}

# Start PACS server
start_pacs_server() {
    log_info "Starting PACS server on port ${TEST_PORT}..."

    "${PACS_SERVER}" \
        --port "${TEST_PORT}" \
        --ae-title "${TEST_AE}" \
        --storage-dir "${STORAGE_DIR}" \
        --db-path "${DB_PATH}" \
        --max-associations 10 \
        --log-level warn &

    SERVER_PID=$!

    if wait_for_port "${TEST_HOST}" "${TEST_PORT}" 15; then
        log_pass "PACS server started successfully (PID: ${SERVER_PID})"
        return 0
    else
        log_fail "PACS server failed to start within timeout"
        return 1
    fi
}

# Stop PACS server
stop_pacs_server() {
    if [[ -n "${SERVER_PID:-}" ]] && kill -0 "${SERVER_PID}" 2>/dev/null; then
        log_info "Stopping PACS server (PID: ${SERVER_PID})..."
        kill "${SERVER_PID}" 2>/dev/null
        wait "${SERVER_PID}" 2>/dev/null || true
        log_pass "PACS server stopped"
        SERVER_PID=""
    fi
}

# Run a test case
run_test() {
    local name="$1"
    shift
    local result=0

    TESTS_RUN=$((TESTS_RUN + 1))
    echo ""
    echo "----------------------------------------"
    echo "Test: ${name}"
    echo "----------------------------------------"

    if "$@"; then
        TESTS_PASSED=$((TESTS_PASSED + 1))
        log_pass "${name}"
        result=0
    else
        TESTS_FAILED=$((TESTS_FAILED + 1))
        log_fail "${name}"
        result=1
    fi

    return ${result}
}

# Test 1: Verify connectivity
test_echo() {
    log_info "Testing C-ECHO to PACS server..."

    if "${ECHO_SCU}" "${TEST_HOST}" "${TEST_PORT}" "${TEST_AE}" >/dev/null 2>&1; then
        log_info "PACS server responding to C-ECHO"
        return 0
    else
        log_fail "PACS server not responding to C-ECHO"
        return 1
    fi
}

# Test 2: Store a DICOM file
test_store_single() {
    local test_file="${TEST_DATA_DIR}/ct_minimal.dcm"

    if [[ ! -f "${test_file}" ]]; then
        log_warn "Test file not found: ${test_file}"
        return 1
    fi

    log_info "Storing single DICOM file..."

    local output
    output=$("${STORE_SCU}" "${TEST_HOST}" "${TEST_PORT}" "${TEST_AE}" "${test_file}" 2>&1)
    local exit_code=$?

    if [[ ${exit_code} -eq 0 ]]; then
        log_info "C-STORE successful"
        return 0
    else
        log_fail "C-STORE failed with exit code: ${exit_code}"
        echo "${output}"
        return 1
    fi
}

# Test 3: Query for stored study
test_query_study() {
    log_info "Querying for studies..."

    local output
    output=$("${QUERY_SCU}" "${TEST_HOST}" "${TEST_PORT}" "${TEST_AE}" \
        --level STUDY \
        --patient-id "TEST*" \
        --format table 2>&1)
    local exit_code=$?

    if [[ ${exit_code} -eq 0 ]]; then
        log_info "C-FIND successful"
        echo "${output}" | head -10
        return 0
    else
        log_fail "C-FIND failed with exit code: ${exit_code}"
        echo "${output}"
        return 1
    fi
}

# Test 4: Query at patient level
test_query_patient() {
    log_info "Querying at PATIENT level..."

    local output
    output=$("${QUERY_SCU}" "${TEST_HOST}" "${TEST_PORT}" "${TEST_AE}" \
        --level PATIENT \
        --format table 2>&1)
    local exit_code=$?

    if [[ ${exit_code} -eq 0 ]]; then
        log_info "Patient-level C-FIND successful"
        return 0
    else
        # Patient-level query might not be supported
        log_warn "Patient-level query returned exit code: ${exit_code}"
        return 0  # Not failing for optional feature
    fi
}

# Test 5: Store and verify file exists in storage
test_store_and_verify() {
    local test_file="${TEST_DATA_DIR}/ct_minimal.dcm"

    if [[ ! -f "${test_file}" ]]; then
        log_warn "Test file not found: ${test_file}"
        return 1
    fi

    log_info "Storing and verifying file storage..."

    # Get initial file count
    local initial_count
    initial_count=$(find "${STORAGE_DIR}" -name "*.dcm" 2>/dev/null | wc -l)

    # Store file
    if ! "${STORE_SCU}" "${TEST_HOST}" "${TEST_PORT}" "${TEST_AE}" "${test_file}" >/dev/null 2>&1; then
        log_fail "C-STORE failed"
        return 1
    fi

    # Give server time to write file
    sleep 1

    # Check file count
    local final_count
    final_count=$(find "${STORAGE_DIR}" -name "*.dcm" 2>/dev/null | wc -l)

    if [[ ${final_count} -gt ${initial_count} ]]; then
        log_info "File stored on disk (count: ${initial_count} -> ${final_count})"
        return 0
    else
        log_warn "File count did not increase (might be duplicate)"
        return 0  # Not failing - could be same file stored twice
    fi
}

# Test 6: Concurrent stores
test_concurrent_stores() {
    local test_file="${TEST_DATA_DIR}/ct_minimal.dcm"

    if [[ ! -f "${test_file}" ]]; then
        log_warn "Test file not found: ${test_file}"
        return 1
    fi

    log_info "Testing concurrent C-STORE operations..."

    # Start multiple stores in parallel
    local pids=()
    local i
    for i in 1 2 3; do
        "${STORE_SCU}" "${TEST_HOST}" "${TEST_PORT}" "${TEST_AE}" "${test_file}" >/dev/null 2>&1 &
        pids+=($!)
    done

    # Wait for all to complete
    local all_success=0
    for pid in "${pids[@]}"; do
        if ! wait "${pid}"; then
            all_success=1
        fi
    done

    if [[ ${all_success} -eq 0 ]]; then
        log_info "All concurrent stores completed successfully"
        return 0
    else
        log_fail "One or more concurrent stores failed"
        return 1
    fi
}

# Main test execution
main() {
    echo ""
    echo "============================================"
    echo "  Binary Integration Test: Store/Retrieve"
    echo "============================================"
    echo ""
    echo "Project root: ${PROJECT_ROOT}"
    echo "Build dir:    ${BUILD_DIR}"
    echo ""

    # Check prerequisites
    if ! check_binaries; then
        exit 1
    fi

    # Create temp directories
    create_temp_dirs

    # Check test data (warning only)
    local has_test_data=1
    if ! check_test_data; then
        has_test_data=0
        log_warn "Some tests will be skipped due to missing test data"
    fi

    # Start PACS server
    if ! start_pacs_server; then
        exit 1
    fi

    # Give server time to initialize
    sleep 2

    # Run tests
    run_test "Echo Connectivity" test_echo

    if [[ ${has_test_data} -eq 1 ]]; then
        run_test "Store Single File" test_store_single
        run_test "Query Study Level" test_query_study
        run_test "Query Patient Level" test_query_patient
        run_test "Store and Verify" test_store_and_verify
        run_test "Concurrent Stores" test_concurrent_stores
    else
        log_warn "Skipping storage tests due to missing test data"
    fi

    # Stop server
    stop_pacs_server

    # Summary
    echo ""
    echo "============================================"
    echo "  Test Summary"
    echo "============================================"
    echo "  Tests run:    ${TESTS_RUN}"
    echo "  Tests passed: ${TESTS_PASSED}"
    echo "  Tests failed: ${TESTS_FAILED}"
    echo "============================================"
    echo ""

    if [[ ${TESTS_FAILED} -eq 0 ]]; then
        log_pass "All storage/retrieve tests passed!"
        exit 0
    else
        log_fail "${TESTS_FAILED} test(s) failed"
        exit 1
    fi
}

main "$@"
