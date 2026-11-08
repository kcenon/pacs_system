#!/bin/bash
# Usage: test_dcmtk_store.sh [build_dir]
set -o pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/binary_common.sh"
init_suite dcmtk-store "${1:-}" || exit 2
require_commands echoscu storescu storescp dcmdump || exit 2
check_dcmtk_available || exit 2
check_cli_contract pacs_server 1 '--storage-dir' '--db-path' || exit 2
check_cli_contract store_scu 0 '-aec' '[options] <peer> <port>' || exit 2
FILE="${TEST_DATA_DIR}/ct_minimal.dcm"
dicom_value "${FILE}" 0008,0018 >/dev/null || exit 2
PACS_PORT=$(find_available_port 41200) || exit 2
start_pacs_server "${PACS_PORT}" PACS_STORE || exit 1
start_receiver() {
    local name="$1"
    RECEIVED_DIR="${WORK_DIR}/${name}"
    mkdir -p "${RECEIVED_DIR}" || return 1
    DCMTK_PORT=$(find_available_port 41204) || return 1
    start_server "${name}" localhost "${DCMTK_PORT}" storescp -aet DCMTK_STORE -od "${RECEIVED_DIR}" "${DCMTK_PORT}"
}
baseline() {
    start_receiver baseline || return 1
    expect_exit 0 30 storescu -aec DCMTK_STORE localhost "${DCMTK_PORT}" "${FILE}" &&
        verify_received_uids "${RECEIVED_DIR}" "${FILE}"
}
dcmtk_to_pacs() {
    expect_exit 0 30 storescu -aec PACS_STORE localhost "${PACS_PORT}" "${FILE}" &&
        verify_received_uids "${WORK_DIR}/storage" "${FILE}"
}
pacs_to_dcmtk() {
    start_receiver pacs-to-dcmtk || return 1
    expect_exit 0 30 "${BINARY_DIR}/store_scu" -aec DCMTK_STORE -aet PACS_STORE localhost "${DCMTK_PORT}" "${FILE}" &&
        verify_received_uids "${RECEIVED_DIR}" "${FILE}"
}
multiple_files() {
    start_receiver multiple-files || return 1
    # Distinct instances: copying one file three times only stores one UID.
    local files=("${FILE}" "${TEST_DATA_DIR}/mr_minimal.dcm" "${TEST_DATA_DIR}/xa_minimal.dcm")
    expect_exit 0 45 storescu -aec DCMTK_STORE localhost "${DCMTK_PORT}" "${files[@]}" &&
        verify_received_uids "${RECEIVED_DIR}" "${files[@]}"
}
run_case 'DCMTK baseline C-STORE' baseline
run_case 'DCMTK SCU to PACS SCP' dcmtk_to_pacs
run_case 'PACS SCU to DCMTK SCP' pacs_to_dcmtk
run_case 'Three distinct instances' multiple_files
finish_suite
