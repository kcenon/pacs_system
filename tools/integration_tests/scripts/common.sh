#!/bin/bash
#
# common.sh - Common functions for binary integration tests
#
# Source this file from individual test scripts to use shared functions.
#
# @see Issue #450 - DCMTK Process Launcher and Test Utilities
#
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

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

# Cross-platform timeout command wrapper
# Usage: run_with_timeout <seconds> <command> [args...]
# Returns: command exit code, or 124 on timeout
# Works on macOS (gtimeout from coreutils), Linux (timeout), and fallback
run_with_timeout() {
    local timeout_seconds="$1"
    shift

    # Try GNU timeout (Linux native, macOS with coreutils in PATH)
    if command -v timeout &>/dev/null; then
        timeout --kill-after=2 "${timeout_seconds}" "$@"
        return $?
    fi

    # Try gtimeout (macOS with coreutils installed via Homebrew)
    if command -v gtimeout &>/dev/null; then
        gtimeout --kill-after=2 "${timeout_seconds}" "$@"
        return $?
    fi

    # Python is also available on our macOS/Ubuntu CI runners. Never silently
    # drop the deadline when GNU coreutils is absent.
    python3 "${SCRIPT_DIR}/run_with_timeout.py" "${timeout_seconds}" "$@"
}

# Check if port is listening (cross-platform)
# Works on macOS, Linux, and other Unix-like systems
is_port_listening() {
    local port="$1"

    # Try lsof first (works on macOS and Linux)
    if command -v lsof &>/dev/null; then
        lsof -i TCP:"${port}" -sTCP:LISTEN >/dev/null 2>&1 && return 0
    fi

    # Fallback to netcat
    if command -v nc &>/dev/null; then
        # macOS nc requires -G for timeout
        if [[ "$(uname)" == "Darwin" ]]; then
            nc -z -G 1 127.0.0.1 "${port}" 2>/dev/null && return 0
        else
            nc -z -w 1 127.0.0.1 "${port}" 2>/dev/null && return 0
        fi
    fi

    # Fallback to bash /dev/tcp (if available)
    # shellcheck disable=SC2016 # Expanded by the child bash with positional args.
    if run_with_timeout 2 bash -c 'echo > /dev/tcp/127.0.0.1/"$1"' _ "${port}" 2>/dev/null; then
        return 0
    fi

    return 1
}

# Wait for port to be listening
# Usage: wait_for_port <host> <port> [timeout_seconds]
wait_for_port() {
    local host="$1"
    local port="$2"
    local timeout="${3:-15}"
    local elapsed=0

    while ! python3 -c 'import socket,sys; socket.create_connection((sys.argv[1],int(sys.argv[2])),0.2).close()' "${host}" "${port}" 2>/dev/null; do
        sleep 0.5
        elapsed=$((elapsed + 1))
        if [[ ${elapsed} -ge $((timeout * 2)) ]]; then
            return 1
        fi
    done
    return 0
}

# Find project root from script location
find_project_root() {
    local script_dir="$1"
    local current_dir="${script_dir}"

    # Walk up directory tree looking for CMakeLists.txt with pacs_system
    while [[ "${current_dir}" != "/" ]]; do
        if [[ -f "${current_dir}/CMakeLists.txt" ]] && \
           grep -q "pacs_system" "${current_dir}/CMakeLists.txt" 2>/dev/null; then
            echo "${current_dir}"
            return 0
        fi
        current_dir="$(dirname "${current_dir}")"
    done

    # Fallback: assume 3 levels up from script
    (cd "${script_dir}/../../.." && pwd)
}

# Check if a binary exists and is executable
check_binary() {
    local path="$1"
    local name="$2"

    if [[ ! -x "${path}" ]]; then
        log_fail "${name} not found at: ${path}"
        return 1
    fi
    return 0
}

# Mark common.sh as sourced for dependency tracking
# shellcheck disable=SC2034 # Read by dcmtk_common.sh when sourced independently.
COMMON_SH_SOURCED=1
