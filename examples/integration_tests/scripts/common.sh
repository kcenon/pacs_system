#!/bin/bash
#
# common.sh - Common functions for binary integration tests
#
# Source this file from individual test scripts to use shared functions.
#

# Colors for output (if terminal supports)
if [[ -t 1 ]]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    BLUE='\033[0;34m'
    BOLD='\033[1m'
    NC='\033[0m' # No Color
else
    RED=''
    GREEN=''
    YELLOW=''
    BLUE=''
    BOLD=''
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
            nc -z 127.0.0.1 "${port}" 2>/dev/null && return 0
        fi
    fi

    # Fallback to bash /dev/tcp (if available)
    if (echo >/dev/tcp/127.0.0.1/"${port}") 2>/dev/null; then
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

    while ! is_port_listening "${port}"; do
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
    echo "$(cd "${script_dir}/../../.." && pwd)"
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
