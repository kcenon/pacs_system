#!/bin/bash
# Storage/query CLI workflow; retrieval is exercised by test_dcmtk_move.sh.
# Usage: test_store_retrieve.sh [build_dir]
set -o pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/binary_common.sh"
init_suite storage "${1:-}" || exit 2
require_commands dcmdump dcmodify || exit 2
check_cli_contract pacs_server 1 '--log-level' 'warning' || exit 2
check_cli_contract echo_scu 0 '-aec' || exit 2
check_cli_contract store_scu 0 '[options] <peer> <port>' '-aec' || exit 2
check_cli_contract query_scu 2 '<host> <port> <called_ae>' '--model' || exit 2
FILE="${TEST_DATA_DIR}/ct_minimal.dcm"
PATIENT=$(dicom_value "${FILE}" 0010,0020) || exit 2
STUDY=$(dicom_value "${FILE}" 0020,000d) || exit 2
PORT=$(find_available_port 41210) || exit 2
AE=PACS_TEST
start_pacs_server "${PORT}" "${AE}" || exit 1
store_one() {
    expect_exit 0 30 "${BINARY_DIR}/store_scu" -aec "${AE}" -aet SHELL_STORE localhost "${PORT}" "${FILE}" &&
        verify_received_uids "${WORK_DIR}/storage" "${FILE}"
}
query_study() {
    expect_exit 0 30 "${BINARY_DIR}/query_scu" localhost "${PORT}" "${AE}" \
        --level STUDY --study-uid "${STUDY}" --format json &&
        grep -Fq "${STUDY}" "${LAST_COMMAND_LOG}"
}
query_patient() {
    expect_exit 0 30 "${BINARY_DIR}/query_scu" localhost "${PORT}" "${AE}" \
        --model patient --level PATIENT --patient-id "${PATIENT}" --format json &&
        grep -Fq "${PATIENT}" "${LAST_COMMAND_LOG}"
}
duplicate_rejected() {
    # pacs_server's default duplicate policy is reject.
    expect_exit 1 30 "${BINARY_DIR}/store_scu" -aec "${AE}" localhost "${PORT}" "${FILE}" &&
        grep -Fq 'Status: FAILURE' "${LAST_COMMAND_LOG}" &&
        verify_received_uids "${WORK_DIR}/storage" "${FILE}"
}
concurrent_stores() {
    local pids=() files=("${FILE}") i status=0 uid
    uid=$(dicom_value "${FILE}" 0008,0018) || return 1
    for i in 1 2 3; do
        local copy="${WORK_DIR}/concurrent-${i}.dcm"
        cp "${FILE}" "${copy}" || return 1
        expect_exit 0 10 dcmodify -nb -i "(0008,0018)=${uid}.${i}" "${copy}" || return 1
        files+=("${copy}")
    done
    # Three independent command logs: subshells cannot update the parent's counter.
    for i in 1 2 3; do
        ( SUITE_LOG_DIR="${SUITE_LOG_DIR}/concurrent-${i}"; mkdir -p "${SUITE_LOG_DIR}";
          expect_exit 0 30 "${BINARY_DIR}/store_scu" -aec "${AE}" -aet "STORE_${i}" \
              localhost "${PORT}" "${files[$i]}" ) &
        pids+=("$!")
    done
    for i in "${pids[@]}"; do wait "${i}" || status=1; done
    [[ ${status} -eq 0 ]] && verify_received_uids "${WORK_DIR}/storage" "${files[@]}"
}
run_case 'Echo Connectivity' expect_exit 0 15 "${BINARY_DIR}/echo_scu" -aec "${AE}" localhost "${PORT}"
run_case 'Store Single File' store_one
run_case 'Query Study Level' query_study
run_case 'Query Patient Level' query_patient
run_case 'Duplicate Rejected and Original Preserved' duplicate_rejected
run_case 'Concurrent Stores' concurrent_stores
finish_suite
