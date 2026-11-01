#!/bin/bash
# Run all selected binary integration suites; every selected suite is required.
set -o pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/common.sh"
BUILD_DIR="$(find_project_root "${SCRIPT_DIR}")/build"
SELECTED=()
VERBOSE=0
PARALLEL=0
usage() {
    cat <<USAGE
Usage: $(basename "$0") [build_dir] [options]
  --connectivity --storage --worklist --secure  Select suites (default: all)
  -v, --verbose   Print full suite output
  -p, --parallel  Run independent suites concurrently
  -h, --help      Show help
Exit 0 requires every selected suite to pass; missing prerequisites fail.
Logs: PACS_TEST_LOG_DIR, or <build_dir>/test-results/shell/<unique run>.
USAGE
}
while [[ $# -gt 0 ]]; do
    case "$1" in
        --connectivity) SELECTED+=(connectivity) ;;
        --storage) SELECTED+=(store_retrieve) ;;
        --worklist) SELECTED+=(worklist_mpps) ;;
        --secure) SELECTED+=(secure_dicom) ;;
        -v|--verbose) VERBOSE=1 ;;
        -p|--parallel) PARALLEL=1 ;;
        -h|--help) usage; exit 0 ;;
        -*) echo "Unknown option: $1"; usage; exit 2 ;;
        *) BUILD_DIR="$1" ;;
    esac
    shift
done
[[ -d "${BUILD_DIR}/bin" ]] || { log_fail "Missing build: ${BUILD_DIR}"; exit 2; }
BUILD_DIR="$(cd "${BUILD_DIR}" && pwd)"
if [[ ${#SELECTED[@]} -eq 0 ]]; then SELECTED=(connectivity store_retrieve worklist_mpps secure_dicom); fi
log_root="${PACS_TEST_LOG_DIR:-${BUILD_DIR}/test-results/shell}"
mkdir -p "${log_root}" || exit 2
PACS_TEST_LOG_DIR=$(mktemp -d "${log_root}/aggregate.XXXXXX") || exit 2
export PACS_TEST_LOG_DIR
log_info "Aggregate logs: ${PACS_TEST_LOG_DIR}"
passed=0
failed=0
pids=()
names=()
logs=()
run_suite() {
    local name="$1"
    run_with_timeout 240 bash "${SCRIPT_DIR}/test_${name}.sh" "${BUILD_DIR}"
}
record_result() {
    local name="$1" status="$2" output="$3"
    printf '%s\t%s\n' "${name}" "${status}" >> "${PACS_TEST_LOG_DIR}/suite-exits.tsv"
    if [[ ${VERBOSE} -eq 1 || ( ${status} -ne 0 && ${LIVE_OUTPUT:-0} -eq 0 ) ]]; then cat "${output}"; fi
    if [[ ${status} -eq 0 ]]; then
        passed=$((passed + 1)); log_pass "${name}"
    else
        failed=$((failed + 1)); log_fail "${name} (exit ${status}, ${output})"
    fi
}
for name in "${SELECTED[@]}"; do
    output="${PACS_TEST_LOG_DIR}/${#names[@]}-${name}.log"
    names+=("${name}")
    logs+=("${output}")
    if [[ ${PARALLEL} -eq 1 ]]; then
        run_suite "${name}" > "${output}" 2>&1 &
        pids+=("$!")
    else
        status=0
        if [[ ${VERBOSE} -eq 1 ]]; then
            run_suite "${name}" 2>&1 | tee "${output}"
            status=${PIPESTATUS[0]}
            # Output has already been shown live.
            VERBOSE=0 LIVE_OUTPUT=1 record_result "${name}" "${status}" "${output}"
        else
            run_suite "${name}" > "${output}" 2>&1 || status=$?
            record_result "${name}" "${status}" "${output}"
        fi
    fi
done
if [[ ${PARALLEL} -eq 1 ]]; then
    for (( i=0; i<${#pids[@]}; ++i )); do
        status=0
        wait "${pids[$i]}" || status=$?
        record_result "${names[$i]}" "${status}" "${logs[$i]}"
    done
fi
printf 'Total suites: %d\nPassed: %d\nFailed: %d\nSkipped: 0\n' "${#SELECTED[@]}" "${passed}" "${failed}"
log_info "Preserved logs: ${PACS_TEST_LOG_DIR}"
[[ ${failed} -eq 0 && ${passed} -gt 0 ]]
