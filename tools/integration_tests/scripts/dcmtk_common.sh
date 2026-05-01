#!/bin/bash
#
# dcmtk_common.sh - DCMTK interoperability test helper functions
#
# Provides reusable shell functions for DCMTK CLI tools (echoscu, storescu,
# findscu, movescu, storescp) within the pacs_system test infrastructure.
#
# Source this file from individual test scripts to use shared functions.
#
# @see Issue #450 - DCMTK Process Launcher and Test Utilities
# @see Issue #449 - DCMTK Interoperability Test Automation Epic
#

# Source common.sh if not already sourced
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ -z "${COMMON_SH_SOURCED:-}" ]]; then
    source "${SCRIPT_DIR}/common.sh"
    COMMON_SH_SOURCED=1
fi

# =============================================================================
# DCMTK Detection
# =============================================================================

# Check if DCMTK is installed and available
# Returns 0 if DCMTK is available, 1 otherwise
check_dcmtk_available() {
    if ! command -v echoscu &>/dev/null; then
        log_fail "DCMTK not found. Install with:"
        log_info "  - macOS: brew install dcmtk"
        log_info "  - Ubuntu/Debian: apt install dcmtk"
        log_info "  - RHEL/CentOS: yum install dcmtk"
        return 1
    fi

    local version
    version=$(echoscu --version 2>&1 | head -1)
    log_info "DCMTK version: ${version}"
    return 0
}

# Get DCMTK version string
# Outputs the version string to stdout
get_dcmtk_version() {
    if command -v echoscu &>/dev/null; then
        echoscu --version 2>&1 | head -1
    else
        echo "Not installed"
    fi
}

# Skip test if DCMTK is not available
# Usage: skip_if_no_dcmtk || exit 0
skip_if_no_dcmtk() {
    if ! check_dcmtk_available; then
        log_warn "Skipping test: DCMTK not available"
        return 1
    fi
    return 0
}

# =============================================================================
# DICOM SCU Functions
# =============================================================================

# Run echoscu with standard options
# Usage: run_echoscu <host> <port> <called_ae> [calling_ae] [timeout]
# Returns: echoscu exit code
run_echoscu() {
    local host="$1"
    local port="$2"
    local called_ae="$3"
    local calling_ae="${4:-ECHOSCU}"
    local timeout="${5:-30}"

    log_info "Running C-ECHO: ${calling_ae} -> ${called_ae}@${host}:${port}"

    run_with_timeout "${timeout}" echoscu \
        -aec "${called_ae}" \
        -aet "${calling_ae}" \
        "${host}" "${port}" 2>&1
    return $?
}

# Run storescu with standard options
# Usage: run_storescu <host> <port> <called_ae> <file> [calling_ae] [timeout]
# Returns: storescu exit code
run_storescu() {
    local host="$1"
    local port="$2"
    local called_ae="$3"
    local file="$4"
    local calling_ae="${5:-STORESCU}"
    local timeout="${6:-60}"

    log_info "Running C-STORE: ${file} -> ${called_ae}@${host}:${port}"

    run_with_timeout "${timeout}" storescu \
        -aec "${called_ae}" \
        -aet "${calling_ae}" \
        "${host}" "${port}" "${file}" 2>&1
    return $?
}

# Run storescu with multiple files
# Usage: run_storescu_multi <host> <port> <called_ae> <file1> [file2] ... [-- calling_ae] [-- timeout]
# Returns: storescu exit code
run_storescu_multi() {
    local host="$1"
    local port="$2"
    local called_ae="$3"
    shift 3

    local files=()
    local calling_ae="STORESCU"
    local timeout="60"

    # Parse files and optional parameters
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --calling-ae)
                calling_ae="$2"
                shift 2
                ;;
            --timeout)
                timeout="$2"
                shift 2
                ;;
            *)
                files+=("$1")
                shift
                ;;
        esac
    done

    log_info "Running C-STORE: ${#files[@]} files -> ${called_ae}@${host}:${port}"

    run_with_timeout "${timeout}" storescu \
        -aec "${called_ae}" \
        -aet "${calling_ae}" \
        "${host}" "${port}" "${files[@]}" 2>&1
    return $?
}

# Run findscu with standard options
# Usage: run_findscu <host> <port> <called_ae> <level> [key1] [key2] ...
# Level: PATIENT, STUDY, SERIES, IMAGE
# Keys: TagName=Value format (e.g., PatientID=TEST001)
# Returns: findscu exit code
run_findscu() {
    local host="$1"
    local port="$2"
    local called_ae="$3"
    local level="$4"
    shift 4

    local key_args=()
    for key in "$@"; do
        key_args+=("-k" "${key}")
    done

    log_info "Running C-FIND (${level}): ${called_ae}@${host}:${port}"

    run_with_timeout 30 findscu \
        -aec "${called_ae}" \
        -aet "FINDSCU" \
        -W \
        -k "QueryRetrieveLevel=${level}" \
        "${key_args[@]}" \
        "${host}" "${port}" 2>&1
    return $?
}

# Run movescu with standard options
# Usage: run_movescu <host> <port> <called_ae> <dest_ae> <level> [key1] [key2] ...
# Returns: movescu exit code
run_movescu() {
    local host="$1"
    local port="$2"
    local called_ae="$3"
    local dest_ae="$4"
    local level="$5"
    shift 5

    local key_args=()
    for key in "$@"; do
        key_args+=("-k" "${key}")
    done

    log_info "Running C-MOVE (${level}): ${dest_ae} <- ${called_ae}@${host}:${port}"

    run_with_timeout 120 movescu \
        -aec "${called_ae}" \
        -aet "MOVESCU" \
        -aem "${dest_ae}" \
        -k "QueryRetrieveLevel=${level}" \
        "${key_args[@]}" \
        "${host}" "${port}" 2>&1
    return $?
}

# =============================================================================
# DICOM SCP Functions
# =============================================================================

# Stores PIDs of running DCMTK servers for cleanup
declare -a DCMTK_SERVER_PIDS=()

# Start storescp in background and wait for ready
# Usage: start_storescp <port> <ae_title> <output_dir> [timeout]
# Outputs: PID of the started process
# Returns: 0 on success, 1 on failure
start_storescp() {
    local port="$1"
    local ae_title="$2"
    local output_dir="$3"
    local timeout="${4:-10}"

    mkdir -p "${output_dir}"

    log_info "Starting storescp: ${ae_title} on port ${port}"

    storescp \
        -aet "${ae_title}" \
        -od "${output_dir}" \
        "${port}" &
    local pid=$!

    DCMTK_SERVER_PIDS+=("${pid}")

    if wait_for_port "127.0.0.1" "${port}" "${timeout}"; then
        log_pass "storescp started (PID: ${pid})"
        echo "${pid}"
        return 0
    else
        log_fail "storescp failed to start within ${timeout}s"
        kill "${pid}" 2>/dev/null
        return 1
    fi
}

# Start echoscp in background and wait for ready
# Usage: start_echoscp <port> <ae_title> [timeout]
# Outputs: PID of the started process
# Returns: 0 on success, 1 on failure
start_echoscp() {
    local port="$1"
    local ae_title="$2"
    local timeout="${3:-10}"

    log_info "Starting echoscp: ${ae_title} on port ${port}"

    echoscp \
        -aet "${ae_title}" \
        "${port}" &
    local pid=$!

    DCMTK_SERVER_PIDS+=("${pid}")

    if wait_for_port "127.0.0.1" "${port}" "${timeout}"; then
        log_pass "echoscp started (PID: ${pid})"
        echo "${pid}"
        return 0
    else
        log_fail "echoscp failed to start within ${timeout}s"
        kill "${pid}" 2>/dev/null
        return 1
    fi
}

# Stop a DCMTK server by PID
# Usage: stop_dcmtk_server <pid>
stop_dcmtk_server() {
    local pid="$1"

    if [[ -z "${pid}" ]]; then
        return 0
    fi

    if kill -0 "${pid}" 2>/dev/null; then
        log_info "Stopping DCMTK server (PID: ${pid})"
        kill "${pid}" 2>/dev/null

        # Wait for graceful shutdown
        local count=0
        while kill -0 "${pid}" 2>/dev/null && [[ ${count} -lt 50 ]]; do
            sleep 0.1
            count=$((count + 1))
        done

        # Force kill if still running
        if kill -0 "${pid}" 2>/dev/null; then
            log_warn "Force killing DCMTK server (PID: ${pid})"
            kill -9 "${pid}" 2>/dev/null
        fi

        wait "${pid}" 2>/dev/null
    fi
}

# Stop storescp by PID (alias for stop_dcmtk_server)
# Usage: stop_storescp <pid>
stop_storescp() {
    stop_dcmtk_server "$1"
}

# Stop echoscp by PID (alias for stop_dcmtk_server)
# Usage: stop_echoscp <pid>
stop_echoscp() {
    stop_dcmtk_server "$1"
}

# Stop all running DCMTK servers
# Usage: stop_all_dcmtk_servers
stop_all_dcmtk_servers() {
    log_info "Stopping all DCMTK servers"
    for pid in "${DCMTK_SERVER_PIDS[@]}"; do
        stop_dcmtk_server "${pid}"
    done
    DCMTK_SERVER_PIDS=()
}

# Register cleanup trap to stop servers on exit
# Usage: register_dcmtk_cleanup
register_dcmtk_cleanup() {
    trap 'stop_all_dcmtk_servers' EXIT INT TERM
}

# =============================================================================
# Verification Functions
# =============================================================================

# Verify DICOM files were received in output directory
# Usage: verify_received_dicom <output_dir> [expected_count]
# Returns: 0 if expected files received, 1 otherwise
verify_received_dicom() {
    local output_dir="$1"
    local expected_count="${2:-1}"

    local actual_count
    actual_count=$(find "${output_dir}" -name "*.dcm" -type f 2>/dev/null | wc -l | tr -d ' ')

    if [[ "${actual_count}" -ge "${expected_count}" ]]; then
        log_pass "Received ${actual_count} DICOM file(s) (expected >= ${expected_count})"
        return 0
    else
        log_fail "Received ${actual_count} DICOM file(s) (expected >= ${expected_count})"
        return 1
    fi
}

# Count DICOM files in a directory
# Usage: count_dicom_files <directory>
# Outputs: Number of .dcm files
count_dicom_files() {
    local directory="$1"
    find "${directory}" -name "*.dcm" -type f 2>/dev/null | wc -l | tr -d ' '
}

# List DICOM files in a directory
# Usage: list_dicom_files <directory>
# Outputs: One file path per line
list_dicom_files() {
    local directory="$1"
    find "${directory}" -name "*.dcm" -type f 2>/dev/null | sort
}

# Verify C-ECHO was successful
# Usage: verify_echo_success <exit_code>
# Returns: 0 on success, 1 on failure
verify_echo_success() {
    local exit_code="$1"

    if [[ "${exit_code}" -eq 0 ]]; then
        log_pass "C-ECHO verification successful"
        return 0
    else
        log_fail "C-ECHO verification failed (exit code: ${exit_code})"
        return 1
    fi
}

# Verify C-STORE was successful
# Usage: verify_store_success <exit_code>
# Returns: 0 on success, 1 on failure
verify_store_success() {
    local exit_code="$1"

    if [[ "${exit_code}" -eq 0 ]]; then
        log_pass "C-STORE transfer successful"
        return 0
    else
        log_fail "C-STORE transfer failed (exit code: ${exit_code})"
        return 1
    fi
}

# =============================================================================
# Utility Functions
# =============================================================================

# Create a temporary directory for DCMTK tests
# Usage: create_dcmtk_temp_dir [prefix]
# Outputs: Path to temporary directory
create_dcmtk_temp_dir() {
    local prefix="${1:-dcmtk_test}"
    local temp_dir
    temp_dir=$(mktemp -d -t "${prefix}.XXXXXX")
    echo "${temp_dir}"
}

# Clean up a temporary directory
# Usage: cleanup_dcmtk_temp_dir <directory>
cleanup_dcmtk_temp_dir() {
    local directory="$1"
    if [[ -d "${directory}" ]]; then
        rm -rf "${directory}"
    fi
}

# Find an available port for testing
# Usage: find_available_port [start_port]
# Outputs: Available port number
find_available_port() {
    local start_port="${1:-41200}"
    local port="${start_port}"

    while is_port_listening "${port}"; do
        port=$((port + 1))
        if [[ "${port}" -gt $((start_port + 100)) ]]; then
            log_fail "Could not find available port"
            return 1
        fi
    done

    echo "${port}"
}

# =============================================================================
# Test Framework Integration
# =============================================================================

# Run a DCMTK-based test with setup and cleanup
# Usage: run_dcmtk_test <test_name> <test_function>
run_dcmtk_test() {
    local test_name="$1"
    local test_function="$2"

    log_info "Starting test: ${test_name}"

    # Check DCMTK availability
    if ! check_dcmtk_available; then
        log_warn "Skipping test: ${test_name} (DCMTK not available)"
        return 77  # Skip code
    fi

    # Register cleanup
    register_dcmtk_cleanup

    # Run the test
    local start_time
    start_time=$(date +%s)

    if "${test_function}"; then
        local end_time
        end_time=$(date +%s)
        local duration=$((end_time - start_time))
        log_pass "Test passed: ${test_name} (${duration}s)"
        return 0
    else
        local end_time
        end_time=$(date +%s)
        local duration=$((end_time - start_time))
        log_fail "Test failed: ${test_name} (${duration}s)"
        return 1
    fi
}

# Mark DCMTK common as sourced
DCMTK_COMMON_SH_SOURCED=1
