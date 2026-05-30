#!/bin/bash
#
# run_all_binary_tests.sh - Master Binary Integration Test Runner
#
# Executes all binary-level integration tests and produces a summary report.
# This script orchestrates the individual test scripts and collects results.
#
# Usage:
#   ./run_all_binary_tests.sh [build_dir] [options]
#
# Options:
#   --connectivity   Run only connectivity tests
#   --storage        Run only storage/retrieve tests
#   --worklist       Run only worklist/MPPS tests
#   --secure         Run only secure DICOM tests
#   --parallel       Run tests in parallel (experimental)
#   --verbose        Show detailed output from each test
#   --help           Show this help message
#
# Exit codes:
#   0 - All tests passed
#   1 - One or more tests failed
#

set -o pipefail

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"

# Test scripts
TEST_CONNECTIVITY="${SCRIPT_DIR}/test_connectivity.sh"
TEST_STORE_RETRIEVE="${SCRIPT_DIR}/test_store_retrieve.sh"
TEST_WORKLIST_MPPS="${SCRIPT_DIR}/test_worklist_mpps.sh"
TEST_SECURE_DICOM="${SCRIPT_DIR}/test_secure_dicom.sh"

# Options
RUN_CONNECTIVITY=1
RUN_STORAGE=1
RUN_WORKLIST=1
RUN_SECURE=1
PARALLEL=0
VERBOSE=0

# Results
declare -A TEST_RESULTS
declare -A TEST_TIMES
TOTAL_PASSED=0
TOTAL_FAILED=0
TOTAL_SKIPPED=0

# Colors
if [[ -t 1 ]]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    BLUE='\033[0;34m'
    BOLD='\033[1m'
    NC='\033[0m'
else
    RED=''
    GREEN=''
    YELLOW=''
    BLUE=''
    BOLD=''
    NC=''
fi

# Print usage
print_usage() {
    cat << EOF
Binary Integration Test Runner

Usage: $(basename "$0") [build_dir] [options]

Arguments:
  build_dir    Path to build directory (default: ./build)

Options:
  --connectivity   Run only connectivity tests
  --storage        Run only storage/retrieve tests
  --worklist       Run only worklist/MPPS tests
  --secure         Run only secure DICOM tests
  --parallel       Run tests in parallel (experimental)
  --verbose        Show detailed output from each test
  --help           Show this help message

Examples:
  $(basename "$0")                    # Run all tests
  $(basename "$0") --connectivity     # Run only connectivity tests
  $(basename "$0") ./build --verbose  # Run all tests with verbose output
  $(basename "$0") --storage --worklist  # Run storage and worklist tests

Exit codes:
  0 - All tests passed
  1 - One or more tests failed

EOF
}

# Parse command line arguments
parse_args() {
    local specific_test=0

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --connectivity)
                if [[ ${specific_test} -eq 0 ]]; then
                    RUN_STORAGE=0
                    RUN_WORKLIST=0
                    RUN_SECURE=0
                    specific_test=1
                fi
                RUN_CONNECTIVITY=1
                shift
                ;;
            --storage)
                if [[ ${specific_test} -eq 0 ]]; then
                    RUN_CONNECTIVITY=0
                    RUN_WORKLIST=0
                    RUN_SECURE=0
                    specific_test=1
                fi
                RUN_STORAGE=1
                shift
                ;;
            --worklist)
                if [[ ${specific_test} -eq 0 ]]; then
                    RUN_CONNECTIVITY=0
                    RUN_STORAGE=0
                    RUN_SECURE=0
                    specific_test=1
                fi
                RUN_WORKLIST=1
                shift
                ;;
            --secure)
                if [[ ${specific_test} -eq 0 ]]; then
                    RUN_CONNECTIVITY=0
                    RUN_STORAGE=0
                    RUN_WORKLIST=0
                    specific_test=1
                fi
                RUN_SECURE=1
                shift
                ;;
            --parallel)
                PARALLEL=1
                shift
                ;;
            --verbose)
                VERBOSE=1
                shift
                ;;
            --help|-h)
                print_usage
                exit 0
                ;;
            -*)
                echo "Unknown option: $1"
                print_usage
                exit 1
                ;;
            *)
                BUILD_DIR="$1"
                shift
                ;;
        esac
    done
}

# Log functions
log_header() {
    echo ""
    echo -e "${BLUE}${BOLD}$*${NC}"
    echo "============================================"
}

log_info() {
    echo "[INFO] $*"
}

log_pass() {
    echo -e "${GREEN}[PASS]${NC} $*"
}

log_fail() {
    echo -e "${RED}[FAIL]${NC} $*"
}

log_skip() {
    echo -e "${YELLOW}[SKIP]${NC} $*"
}

# Check prerequisites
check_prerequisites() {
    log_header "Checking Prerequisites"

    # Check build directory
    if [[ ! -d "${BUILD_DIR}" ]]; then
        log_fail "Build directory not found: ${BUILD_DIR}"
        log_info "Please build the project first with: cmake --build build"
        return 1
    fi

    # Check for required binaries
    local required_bins=("echo_scp" "echo_scu" "pacs_server")
    local missing=0

    for bin in "${required_bins[@]}"; do
        if [[ ! -x "${BUILD_DIR}/bin/${bin}" ]]; then
            log_fail "Required binary not found: ${bin}"
            missing=1
        fi
    done

    if [[ ${missing} -eq 1 ]]; then
        log_info "Please build the project with: cmake --build ${BUILD_DIR}"
        return 1
    fi

    # Check for nc (netcat) - required for port checking
    if ! command -v nc &>/dev/null; then
        log_fail "netcat (nc) not found - required for port checking"
        return 1
    fi

    log_pass "All prerequisites satisfied"
    return 0
}

# Run a single test suite
run_test_suite() {
    local name="$1"
    local script="$2"

    log_header "Running ${name}"

    if [[ ! -x "${script}" ]]; then
        log_skip "${name} - script not found or not executable"
        TEST_RESULTS["${name}"]="SKIP"
        TOTAL_SKIPPED=$((TOTAL_SKIPPED + 1))
        return 0
    fi

    local start_time
    start_time=$(date +%s)

    local output
    local exit_code

    if [[ ${VERBOSE} -eq 1 ]]; then
        # Show output in real-time
        "${script}" "${BUILD_DIR}"
        exit_code=$?
    else
        # Capture output
        output=$("${script}" "${BUILD_DIR}" 2>&1)
        exit_code=$?

        # Show summary only
        if echo "${output}" | grep -q "Tests run:"; then
            echo "${output}" | grep -A3 "Tests run:"
        fi
    fi

    local end_time
    end_time=$(date +%s)
    local duration=$((end_time - start_time))
    TEST_TIMES["${name}"]="${duration}s"

    if [[ ${exit_code} -eq 0 ]]; then
        log_pass "${name} completed in ${duration}s"
        TEST_RESULTS["${name}"]="PASS"
        TOTAL_PASSED=$((TOTAL_PASSED + 1))
        return 0
    else
        log_fail "${name} failed (exit code: ${exit_code})"
        TEST_RESULTS["${name}"]="FAIL"
        TOTAL_FAILED=$((TOTAL_FAILED + 1))

        # Show output on failure if not verbose
        if [[ ${VERBOSE} -eq 0 ]]; then
            echo "--- Test Output ---"
            echo "${output}" | tail -30
            echo "-------------------"
        fi
        return 1
    fi
}

# Run tests in parallel
run_tests_parallel() {
    log_header "Running Tests in Parallel"
    log_info "Starting all test suites..."

    local pids=()
    local names=()
    local temp_files=()

    # Start each test in background
    if [[ ${RUN_CONNECTIVITY} -eq 1 ]]; then
        local tmp=$(mktemp)
        temp_files+=("${tmp}")
        names+=("Connectivity")
        "${TEST_CONNECTIVITY}" "${BUILD_DIR}" > "${tmp}" 2>&1 &
        pids+=($!)
    fi

    if [[ ${RUN_STORAGE} -eq 1 ]]; then
        local tmp=$(mktemp)
        temp_files+=("${tmp}")
        names+=("Storage/Retrieve")
        "${TEST_STORE_RETRIEVE}" "${BUILD_DIR}" > "${tmp}" 2>&1 &
        pids+=($!)
    fi

    if [[ ${RUN_WORKLIST} -eq 1 ]]; then
        local tmp=$(mktemp)
        temp_files+=("${tmp}")
        names+=("Worklist/MPPS")
        "${TEST_WORKLIST_MPPS}" "${BUILD_DIR}" > "${tmp}" 2>&1 &
        pids+=($!)
    fi

    if [[ ${RUN_SECURE} -eq 1 ]]; then
        local tmp=$(mktemp)
        temp_files+=("${tmp}")
        names+=("Secure DICOM")
        "${TEST_SECURE_DICOM}" "${BUILD_DIR}" > "${tmp}" 2>&1 &
        pids+=($!)
    fi

    # Wait for all tests and collect results
    local i=0
    for pid in "${pids[@]}"; do
        wait "${pid}"
        local exit_code=$?

        if [[ ${exit_code} -eq 0 ]]; then
            log_pass "${names[$i]}"
            TEST_RESULTS["${names[$i]}"]="PASS"
            TOTAL_PASSED=$((TOTAL_PASSED + 1))
        else
            log_fail "${names[$i]}"
            TEST_RESULTS["${names[$i]}"]="FAIL"
            TOTAL_FAILED=$((TOTAL_FAILED + 1))

            if [[ ${VERBOSE} -eq 1 ]]; then
                cat "${temp_files[$i]}"
            fi
        fi

        rm -f "${temp_files[$i]}"
        i=$((i + 1))
    done
}

# Run tests sequentially
run_tests_sequential() {
    if [[ ${RUN_CONNECTIVITY} -eq 1 ]]; then
        run_test_suite "Connectivity" "${TEST_CONNECTIVITY}"
    fi

    if [[ ${RUN_STORAGE} -eq 1 ]]; then
        run_test_suite "Storage/Retrieve" "${TEST_STORE_RETRIEVE}"
    fi

    if [[ ${RUN_WORKLIST} -eq 1 ]]; then
        run_test_suite "Worklist/MPPS" "${TEST_WORKLIST_MPPS}"
    fi

    if [[ ${RUN_SECURE} -eq 1 ]]; then
        run_test_suite "Secure DICOM" "${TEST_SECURE_DICOM}"
    fi
}

# Print summary
print_summary() {
    local total=$((TOTAL_PASSED + TOTAL_FAILED + TOTAL_SKIPPED))

    echo ""
    echo ""
    echo -e "${BOLD}============================================${NC}"
    echo -e "${BOLD}        BINARY INTEGRATION TEST SUMMARY     ${NC}"
    echo -e "${BOLD}============================================${NC}"
    echo ""

    # Individual results
    printf "%-20s %-10s %-10s\n" "Test Suite" "Result" "Duration"
    printf "%-20s %-10s %-10s\n" "----------" "------" "--------"

    for name in "${!TEST_RESULTS[@]}"; do
        local result="${TEST_RESULTS[$name]}"
        local duration="${TEST_TIMES[$name]:-N/A}"

        case "${result}" in
            PASS)
                printf "%-20s ${GREEN}%-10s${NC} %-10s\n" "${name}" "${result}" "${duration}"
                ;;
            FAIL)
                printf "%-20s ${RED}%-10s${NC} %-10s\n" "${name}" "${result}" "${duration}"
                ;;
            SKIP)
                printf "%-20s ${YELLOW}%-10s${NC} %-10s\n" "${name}" "${result}" "${duration}"
                ;;
        esac
    done

    echo ""
    echo "--------------------------------------------"
    printf "%-20s %d\n" "Total test suites:" "${total}"
    printf "%-20s ${GREEN}%d${NC}\n" "Passed:" "${TOTAL_PASSED}"
    printf "%-20s ${RED}%d${NC}\n" "Failed:" "${TOTAL_FAILED}"
    printf "%-20s ${YELLOW}%d${NC}\n" "Skipped:" "${TOTAL_SKIPPED}"
    echo "============================================"
    echo ""

    if [[ ${TOTAL_FAILED} -eq 0 ]]; then
        echo -e "${GREEN}${BOLD}All binary integration tests passed!${NC}"
        return 0
    else
        echo -e "${RED}${BOLD}${TOTAL_FAILED} test suite(s) failed${NC}"
        return 1
    fi
}

# Main function
main() {
    parse_args "$@"

    echo ""
    echo -e "${BOLD}============================================${NC}"
    echo -e "${BOLD}     PACS BINARY INTEGRATION TESTS          ${NC}"
    echo -e "${BOLD}============================================${NC}"
    echo ""
    echo "Project root: ${PROJECT_ROOT}"
    echo "Build dir:    ${BUILD_DIR}"
    echo "Date:         $(date '+%Y-%m-%d %H:%M:%S')"
    echo ""

    # Check prerequisites
    if ! check_prerequisites; then
        exit 1
    fi

    # Make test scripts executable
    chmod +x "${SCRIPT_DIR}"/*.sh 2>/dev/null

    # Run tests
    if [[ ${PARALLEL} -eq 1 ]]; then
        run_tests_parallel
    else
        run_tests_sequential
    fi

    # Print summary
    print_summary
    exit $?
}

main "$@"
