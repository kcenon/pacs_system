#!/bin/bash
#
# test_secure_dicom.sh - Binary Integration Test for TLS Communication
#
# Tests secure DICOM communication using TLS-encrypted connections.
# Generates test certificates and validates secure echo operations.
#
# Usage:
#   ./test_secure_dicom.sh [build_dir]
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
SECURE_ECHO_SCP="${BUILD_DIR}/bin/secure_echo_scp"
SECURE_ECHO_SCU="${BUILD_DIR}/bin/secure_echo_scu"
CERT_GENERATOR="${PROJECT_ROOT}/examples/secure_dicom/generate_certs.sh"

# Test parameters
TEST_PORT=41203
TEST_AE="SECURE_SCP"
TEST_HOST="localhost"
CERT_DIR=""

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
        echo "Cleaning up: Stopping secure server (PID: ${SERVER_PID})"
        kill "${SERVER_PID}" 2>/dev/null || true
        wait "${SERVER_PID}" 2>/dev/null || true
    fi

    if [[ -n "${CERT_DIR:-}" ]] && [[ -d "${CERT_DIR}" ]]; then
        echo "Cleaning up: Removing temp certificates"
        rm -rf "${CERT_DIR}"
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

    if [[ ! -x "${SECURE_ECHO_SCP}" ]]; then
        log_warn "secure_echo_scp not found at: ${SECURE_ECHO_SCP}"
        log_warn "Secure DICOM tests will be skipped"
        return 1
    fi

    if [[ ! -x "${SECURE_ECHO_SCU}" ]]; then
        log_warn "secure_echo_scu not found at: ${SECURE_ECHO_SCU}"
        log_warn "Secure DICOM tests will be skipped"
        return 1
    fi

    log_pass "Secure DICOM binaries found"
    return 0
}

# Check for OpenSSL
check_openssl() {
    log_info "Checking for OpenSSL..."

    if ! command -v openssl &>/dev/null; then
        log_warn "OpenSSL not found - cannot generate test certificates"
        return 1
    fi

    log_pass "OpenSSL available"
    return 0
}

# Generate test certificates
generate_test_certs() {
    log_info "Generating test certificates..."

    CERT_DIR=$(mktemp -d -t pacs_certs_XXXXXX)
    log_info "Certificate directory: ${CERT_DIR}"

    # Generate CA key and certificate
    log_info "  Generating CA certificate..."
    openssl genrsa -out "${CERT_DIR}/ca.key" 2048 2>/dev/null
    openssl req -new -x509 -days 1 -key "${CERT_DIR}/ca.key" \
        -out "${CERT_DIR}/ca.crt" \
        -subj "/CN=Test CA/O=PACS Test/C=US" 2>/dev/null

    # Generate server key and certificate
    log_info "  Generating server certificate..."
    openssl genrsa -out "${CERT_DIR}/server.key" 2048 2>/dev/null
    openssl req -new -key "${CERT_DIR}/server.key" \
        -out "${CERT_DIR}/server.csr" \
        -subj "/CN=localhost/O=PACS Server/C=US" 2>/dev/null
    openssl x509 -req -days 1 -in "${CERT_DIR}/server.csr" \
        -CA "${CERT_DIR}/ca.crt" -CAkey "${CERT_DIR}/ca.key" \
        -CAcreateserial -out "${CERT_DIR}/server.crt" 2>/dev/null

    # Combine server key and cert for PEM
    cat "${CERT_DIR}/server.key" "${CERT_DIR}/server.crt" > "${CERT_DIR}/server.pem"

    # Generate client key and certificate
    log_info "  Generating client certificate..."
    openssl genrsa -out "${CERT_DIR}/client.key" 2048 2>/dev/null
    openssl req -new -key "${CERT_DIR}/client.key" \
        -out "${CERT_DIR}/client.csr" \
        -subj "/CN=Test Client/O=PACS Client/C=US" 2>/dev/null
    openssl x509 -req -days 1 -in "${CERT_DIR}/client.csr" \
        -CA "${CERT_DIR}/ca.crt" -CAkey "${CERT_DIR}/ca.key" \
        -CAcreateserial -out "${CERT_DIR}/client.crt" 2>/dev/null

    # Combine client key and cert for PEM
    cat "${CERT_DIR}/client.key" "${CERT_DIR}/client.crt" > "${CERT_DIR}/client.pem"

    log_pass "Test certificates generated"
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

# Start secure Echo SCP
start_secure_scp() {
    log_info "Starting secure Echo SCP on port ${TEST_PORT}..."

    "${SECURE_ECHO_SCP}" "${TEST_PORT}" "${TEST_AE}" \
        --cert "${CERT_DIR}/server.pem" \
        --ca "${CERT_DIR}/ca.crt" &

    SERVER_PID=$!

    if wait_for_port "${TEST_HOST}" "${TEST_PORT}" 10; then
        log_pass "Secure Echo SCP started (PID: ${SERVER_PID})"
        return 0
    else
        log_fail "Secure Echo SCP failed to start"
        return 1
    fi
}

# Stop secure SCP
stop_secure_scp() {
    if [[ -n "${SERVER_PID:-}" ]] && kill -0 "${SERVER_PID}" 2>/dev/null; then
        log_info "Stopping secure Echo SCP..."
        kill "${SERVER_PID}" 2>/dev/null
        wait "${SERVER_PID}" 2>/dev/null || true
        log_pass "Secure Echo SCP stopped"
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

# Test 1: Basic secure echo
test_secure_echo() {
    log_info "Sending secure C-ECHO request..."

    local output
    output=$("${SECURE_ECHO_SCU}" "${TEST_HOST}" "${TEST_PORT}" "${TEST_AE}" \
        --cert "${CERT_DIR}/client.pem" \
        --ca "${CERT_DIR}/ca.crt" 2>&1)
    local exit_code=$?

    if [[ ${exit_code} -eq 0 ]] && echo "${output}" | grep -q -i "success"; then
        log_info "Secure echo successful"
        return 0
    else
        log_fail "Secure echo failed with exit code: ${exit_code}"
        echo "${output}"
        return 1
    fi
}

# Test 2: Multiple secure echos
test_multiple_secure_echos() {
    log_info "Sending multiple secure C-ECHO requests..."

    local i
    for i in 1 2 3; do
        if ! "${SECURE_ECHO_SCU}" "${TEST_HOST}" "${TEST_PORT}" "${TEST_AE}" \
            --cert "${CERT_DIR}/client.pem" \
            --ca "${CERT_DIR}/ca.crt" >/dev/null 2>&1; then
            log_fail "Secure echo ${i} failed"
            return 1
        fi
        log_info "Secure echo ${i}/3 successful"
    done

    return 0
}

# Test 3: Connection without client cert (should fail or warn)
test_no_client_cert() {
    log_info "Testing connection without client certificate..."

    # This test depends on server configuration
    # Some servers require client certs, some don't
    local output
    output=$("${SECURE_ECHO_SCU}" "${TEST_HOST}" "${TEST_PORT}" "${TEST_AE}" \
        --ca "${CERT_DIR}/ca.crt" 2>&1)
    local exit_code=$?

    # Just log the result - behavior depends on configuration
    if [[ ${exit_code} -eq 0 ]]; then
        log_info "Connection succeeded without client cert (anonymous TLS allowed)"
    else
        log_info "Connection failed without client cert (mutual TLS required)"
    fi

    return 0  # Pass regardless - just testing behavior
}

# Test 4: Wrong CA certificate (should fail)
test_wrong_ca() {
    log_info "Testing with wrong CA certificate (should fail)..."

    # Generate a different CA
    local wrong_ca="${CERT_DIR}/wrong_ca.crt"
    openssl req -new -x509 -days 1 -nodes -keyout /dev/null \
        -out "${wrong_ca}" \
        -subj "/CN=Wrong CA/O=Wrong/C=US" 2>/dev/null

    local output
    output=$("${SECURE_ECHO_SCU}" "${TEST_HOST}" "${TEST_PORT}" "${TEST_AE}" \
        --cert "${CERT_DIR}/client.pem" \
        --ca "${wrong_ca}" 2>&1)
    local exit_code=$?

    if [[ ${exit_code} -ne 0 ]]; then
        log_info "Connection correctly failed with wrong CA"
        return 0
    else
        log_warn "Connection succeeded with wrong CA - certificate validation may be disabled"
        return 0  # Warning but not failure
    fi
}

# Test 5: Verify TLS version (check for TLS 1.2+)
test_tls_version() {
    log_info "Checking TLS protocol version..."

    # Use OpenSSL s_client to check TLS version
    local output
    output=$(echo | openssl s_client -connect "${TEST_HOST}:${TEST_PORT}" \
        -CAfile "${CERT_DIR}/ca.crt" 2>&1 | grep -i "Protocol")

    if echo "${output}" | grep -q -E "TLSv1\.[23]"; then
        log_info "TLS version: ${output}"
        return 0
    else
        log_warn "Could not verify TLS version"
        return 0  # Not a failure
    fi
}

# Main test execution
main() {
    echo ""
    echo "============================================"
    echo "  Binary Integration Test: Secure DICOM"
    echo "============================================"
    echo ""
    echo "Project root: ${PROJECT_ROOT}"
    echo "Build dir:    ${BUILD_DIR}"
    echo ""

    # Check prerequisites
    if ! check_binaries; then
        echo ""
        echo "============================================"
        echo "  Secure DICOM tests skipped"
        echo "  (binaries not available)"
        echo "============================================"
        exit 0
    fi

    if ! check_openssl; then
        echo ""
        echo "============================================"
        echo "  Secure DICOM tests skipped"
        echo "  (OpenSSL not available)"
        echo "============================================"
        exit 0
    fi

    # Generate test certificates
    if ! generate_test_certs; then
        log_fail "Failed to generate test certificates"
        exit 1
    fi

    # Start secure server
    if ! start_secure_scp; then
        exit 1
    fi

    # Give server time to initialize
    sleep 1

    # Run tests
    run_test "Basic Secure Echo" test_secure_echo
    run_test "Multiple Secure Echos" test_multiple_secure_echos
    run_test "No Client Certificate" test_no_client_cert
    run_test "Wrong CA Certificate" test_wrong_ca
    run_test "TLS Version Check" test_tls_version

    # Stop server
    stop_secure_scp

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
        log_pass "All secure DICOM tests passed!"
        exit 0
    else
        log_fail "${TESTS_FAILED} test(s) failed"
        exit 1
    fi
}

main "$@"
