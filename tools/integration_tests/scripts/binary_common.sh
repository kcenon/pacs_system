#!/bin/bash
# Shared lifecycle and evidence for process-level integration tests.
# shellcheck disable=SC2034 # Public variables are read by the calling suites.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/common.sh"
source "${SCRIPT_DIR}/dcmtk_common.sh"

init_suite() {
    SUITE_NAME="$1"
    BUILD_DIR="${2:-$(find_project_root "${SCRIPT_DIR}")/build}"
    [[ -d "${BUILD_DIR}/bin" ]] || { log_fail "Missing build: ${BUILD_DIR}"; return 1; }
    BUILD_DIR="$(cd "${BUILD_DIR}" && pwd)"
    BINARY_DIR="${BUILD_DIR}/bin"
    TEST_DATA_DIR="${SCRIPT_DIR}/../test_data"
    command -v python3 >/dev/null || { log_fail 'python3 is required'; return 1; }
    local log_root="${PACS_TEST_LOG_DIR:-${BUILD_DIR}/test-results/shell}"
    mkdir -p "${log_root}" || return 1
    SUITE_LOG_DIR=$(mktemp -d "${log_root}/${SUITE_NAME}.XXXXXX") || return 1
    WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/pacs-${SUITE_NAME}.XXXXXX") || return 1
    SERVER_PIDS=()
    TESTS_RUN=0 TESTS_PASSED=0 TESTS_FAILED=0 COMMAND_NUMBER=0
    trap 'cleanup_suite "$?"' EXIT
    trap 'exit 130' INT
    trap 'exit 143' TERM
    log_info "Logs: ${SUITE_LOG_DIR}"
}

stop_server() {
    local pid="$1" count=0 status=0 sent_term=0
    if kill -0 "${pid}" 2>/dev/null; then
        kill "${pid}" 2>/dev/null && sent_term=1
        while kill -0 "${pid}" 2>/dev/null && (( count < 50 )); do
            [[ $(ps -o stat= -p "${pid}" 2>/dev/null) == *Z* ]] && break
            sleep 0.1
            count=$((count + 1))
        done
        if kill -0 "${pid}" 2>/dev/null; then
            kill -KILL "${pid}" 2>/dev/null || true
        fi
    fi
    wait "${pid}" 2>/dev/null || status=$?
    printf '%s\t%s\n' "${pid}" "${status}" >> "${SUITE_LOG_DIR}/server-exits.tsv"
    # DCMTK storescp uses the default SIGTERM handler. Crashes, sanitizer
    # aborts, and a forced SIGKILL are failures even after successful requests.
    if [[ ${status} -eq 0 || ( ${sent_term} -eq 1 && ${status} -eq 143 ) ]]; then return 0; fi
    log_fail "Server PID ${pid} exited unexpectedly (${status})"
    return 1
}

cleanup_suite() {
    local pid status="$1"
    trap - EXIT
    for pid in "${SERVER_PIDS[@]}"; do stop_server "${pid}" || status=1; done
    rm -rf "${WORK_DIR}"
    log_info "Preserved logs: ${SUITE_LOG_DIR}"
    exit "${status}"
}

# Retain the real command status, complete output and argv independently.
# Call directly (not in command substitution) to retain LAST_COMMAND_LOG.
run_cli() {
    local seconds="$1" status=0
    shift
    COMMAND_NUMBER=$((COMMAND_NUMBER + 1))
    LAST_COMMAND_LOG="${SUITE_LOG_DIR}/command-${COMMAND_NUMBER}.log"
    printf '%q ' "$@" > "${LAST_COMMAND_LOG}.command"
    printf '\n' >> "${LAST_COMMAND_LOG}.command"
    run_with_timeout "${seconds}" "$@" > "${LAST_COMMAND_LOG}" 2>&1 || status=$?
    printf '%s\n' "${status}" > "${LAST_COMMAND_LOG}.exit"
    cat "${LAST_COMMAND_LOG}"
    log_info "Command exit: ${status} (${LAST_COMMAND_LOG})"
    return "${status}"
}

expect_exit() {
    local expected="$1" status=0
    shift
    run_cli "$@" || status=$?
    [[ ${status} -eq ${expected} ]] || {
        log_fail "Expected exit ${expected}, received ${status}"
        return 1
    }
}

# Several legacy tools return 1 or 2 for --help. Pin the observed help
# contract explicitly; never accept an arbitrary failing process as help.
check_cli_contract() {
    local name="$1" expected="$2" marker
    shift 2
    check_binary "${BINARY_DIR}/${name}" "${name}" || return 1
    expect_exit "${expected}" 5 "${BINARY_DIR}/${name}" --help || return 1
    for marker in 'Usage:' "$@"; do
        grep -Fq -- "${marker}" "${LAST_COMMAND_LOG}" || {
            log_fail "${name} help is missing: ${marker}"; return 1;
        }
    done
}

require_commands() {
    local name
    for name in "$@"; do
        command -v "${name}" >/dev/null || { log_fail "Required tool missing: ${name}"; return 1; }
    done
}

# Must be called in the parent shell so EXIT cleanup owns every started PID.
start_server() {
    local name="$1" host="$2" port="$3" status=0 deadline=$((SECONDS + 30))
    shift 3
    if is_port_listening "${port}"; then
        log_fail "Port ${port} already in use; refusing to reuse another server"
        return 1
    fi
    local output="${SUITE_LOG_DIR}/${name}.log"
    printf '%q ' "$@" > "${output}.command"
    printf '\n' >> "${output}.command"
    "$@" > "${output}" 2>&1 &
    SERVER_PID=$!
    SERVER_PIDS+=("${SERVER_PID}")
    while (( SECONDS < deadline )); do
        if ! kill -0 "${SERVER_PID}" 2>/dev/null; then
            wait "${SERVER_PID}" || status=$?
            unset "SERVER_PIDS[$((${#SERVER_PIDS[@]} - 1))]"
            printf '%s\n' "${status}" > "${output}.exit"
            log_fail "${name} exited during startup (${status}); check arguments and configuration"
            cat "${output}"
            return 1
        fi
        if wait_for_port "${host}" "${port}" 1; then
            kill -0 "${SERVER_PID}" 2>/dev/null || continue
            log_pass "${name} listening on ${host}:${port} (PID ${SERVER_PID})"
            return 0
        fi
    done
    log_fail "${name} failed to listen within 30 seconds"
    cat "${output}"
    return 1
}

start_pacs_server() {
    local port="$1" ae="$2"
    mkdir -p "${WORK_DIR}/storage" || return 1
    start_server pacs-server localhost "${port}" "${BINARY_DIR}/pacs_server" \
        --port "${port}" --ae-title "${ae}" --storage-dir "${WORK_DIR}/storage" \
        --db-path "${WORK_DIR}/pacs.db" --max-associations 10 --log-level warning
}

run_case() {
    local name="$1"
    shift
    TESTS_RUN=$((TESTS_RUN + 1))
    log_info "Test: ${name}"
    if "$@"; then
        TESTS_PASSED=$((TESTS_PASSED + 1))
        log_pass "${name}"
    else
        TESTS_FAILED=$((TESTS_FAILED + 1))
        log_fail "${name}"
    fi
}

finish_suite() {
    printf 'Tests run: %d\nTests passed: %d\nTests failed: %d\n' \
        "${TESTS_RUN}" "${TESTS_PASSED}" "${TESTS_FAILED}"
    [[ ${TESTS_RUN} -gt 0 && ${TESTS_FAILED} -eq 0 ]]
}
