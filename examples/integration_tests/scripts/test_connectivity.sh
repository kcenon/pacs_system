#!/bin/bash
#
# test_connectivity.sh - Binary Integration Test for DICOM Connectivity
#
# Tests Echo SCP and SCU binaries for basic network connectivity.
# Uses process-level testing to verify actual socket communication.
#
# Usage:
#   ./test_connectivity.sh [build_dir]
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

# Binaries
ECHO_SCP="${BUILD_DIR}/bin/echo_scp"
ECHO_SCU="${BUILD_DIR}/bin/echo_scu"

# Test parameters
TEST_PORT=41200
TEST_AE="TEST_SCP"
TEST_HOST="localhost"

# Colors for output (if terminal supports)
if [[ -t 1 ]]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    NC='\033[0m' # No Color
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
    if [[ -n "${SCP_PID:-}" ]] && kill -0 "${SCP_PID}" 2>/dev/null; then
        echo "Cleaning up: Stopping Echo SCP (PID: ${SCP_PID})"
        kill "${SCP_PID}" 2>/dev/null || true
        wait "${SCP_PID}" 2>/dev/null || true
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

    if [[ ! -x "${ECHO_SCP}" ]]; then
        log_fail "echo_scp not found at: ${ECHO_SCP}"
        log_info "Please build the project first with: cmake --build ${BUILD_DIR}"
        return 1
    fi

    if [[ ! -x "${ECHO_SCU}" ]]; then
        log_fail "echo_scu not found at: ${ECHO_SCU}"
        log_info "Please build the project first with: cmake --build ${BUILD_DIR}"
        return 1
    fi

    log_pass "All required binaries found"
    return 0
}

# Wait for port to be listening
wait_for_port() {
    local host="$1"
    local port="$2"
    local timeout="${3:-10}"
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

# Start Echo SCP
start_echo_scp() {
    local port="$1"
    local ae_title="$2"

    log_info "Starting Echo SCP on port ${port} with AE title ${ae_title}..."

    "${ECHO_SCP}" "${port}" "${ae_title}" &
    SCP_PID=$!

    # Wait for server to start
    if wait_for_port "${TEST_HOST}" "${port}" 5; then
        log_pass "Echo SCP started successfully (PID: ${SCP_PID})"
        return 0
    else
        log_fail "Echo SCP failed to start within timeout"
        return 1
    fi
}

# Stop Echo SCP
stop_echo_scp() {
    if [[ -n "${SCP_PID:-}" ]] && kill -0 "${SCP_PID}" 2>/dev/null; then
        log_info "Stopping Echo SCP (PID: ${SCP_PID})..."
        kill "${SCP_PID}" 2>/dev/null
        wait "${SCP_PID}" 2>/dev/null || true
        log_pass "Echo SCP stopped"
        SCP_PID=""
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

# Test 1: Basic Echo Test
test_basic_echo() {
    log_info "Sending C-ECHO request..."

    local output
    output=$("${ECHO_SCU}" "${TEST_HOST}" "${TEST_PORT}" "${TEST_AE}" 2>&1)
    local exit_code=$?

    if [[ ${exit_code} -eq 0 ]] && echo "${output}" | grep -q "SUCCESS"; then
        log_info "Echo response received successfully"
        return 0
    else
        log_fail "Echo failed with exit code: ${exit_code}"
        echo "${output}"
        return 1
    fi
}

# Test 2: Multiple Sequential Echos
test_multiple_echos() {
    log_info "Sending multiple C-ECHO requests..."

    local i
    for i in 1 2 3; do
        if ! "${ECHO_SCU}" "${TEST_HOST}" "${TEST_PORT}" "${TEST_AE}" >/dev/null 2>&1; then
            log_fail "Echo ${i} failed"
            return 1
        fi
        log_info "Echo ${i}/3 successful"
    done

    return 0
}

# Test 3: Echo with Custom Calling AE
test_custom_calling_ae() {
    local calling_ae="MY_SCU"

    log_info "Testing with custom calling AE: ${calling_ae}"

    if "${ECHO_SCU}" "${TEST_HOST}" "${TEST_PORT}" "${TEST_AE}" "${calling_ae}" >/dev/null 2>&1; then
        return 0
    else
        return 1
    fi
}

# Test 4: Connection to Non-Existent Server (should fail)
test_connection_failure() {
    log_info "Testing connection to non-existent server (should fail)..."

    # Use a port that's unlikely to be in use
    if "${ECHO_SCU}" "${TEST_HOST}" 59999 "FAKE_SCP" 2>&1 | grep -q -i "fail\|error\|refused"; then
        log_info "Connection correctly failed as expected"
        return 0
    else
        log_fail "Expected connection failure but got unexpected result"
        return 1
    fi
}

# Test 5: Invalid AE Title Length (should fail at client)
test_invalid_ae_title() {
    log_info "Testing invalid AE title (too long)..."

    # AE titles > 16 characters should be rejected
    local long_ae="THIS_AE_TITLE_IS_WAY_TOO_LONG"

    if ! "${ECHO_SCU}" "${TEST_HOST}" "${TEST_PORT}" "${long_ae}" 2>&1 | grep -q -i "error\|invalid"; then
        log_warn "Client should reject AE titles > 16 characters"
        return 1
    fi

    return 0
}

# Main test execution
main() {
    echo ""
    echo "============================================"
    echo "  Binary Integration Test: Connectivity"
    echo "============================================"
    echo ""
    echo "Project root: ${PROJECT_ROOT}"
    echo "Build dir:    ${BUILD_DIR}"
    echo ""

    # Check prerequisites
    if ! check_binaries; then
        exit 1
    fi

    # Start Echo SCP
    if ! start_echo_scp "${TEST_PORT}" "${TEST_AE}"; then
        exit 1
    fi

    # Give server a moment to be ready
    sleep 1

    # Run tests
    run_test "Basic Echo" test_basic_echo
    run_test "Multiple Sequential Echos" test_multiple_echos
    run_test "Custom Calling AE" test_custom_calling_ae
    run_test "Connection Failure (Expected)" test_connection_failure
    run_test "Invalid AE Title" test_invalid_ae_title

    # Stop server
    stop_echo_scp

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
        log_pass "All connectivity tests passed!"
        exit 0
    else
        log_fail "${TESTS_FAILED} test(s) failed"
        exit 1
    fi
}

main "$@"
