#!/bin/bash
# Usage: test_dcmtk_move.sh [build_dir]
# qr_scp exposes destination registration through its documented --peer option.
set -o pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/binary_common.sh"
init_suite dcmtk-move "${1:-}" || exit 2
require_commands echoscu movescu storescp dcmdump || exit 2
check_dcmtk_available || exit 2
check_cli_contract qr_scp 1 '<port> <ae_title>' '--peer' '--storage-dir' || exit 2
mkdir -p "${WORK_DIR}/source" "${WORK_DIR}/destination"
FILE="${WORK_DIR}/source/ct_minimal.dcm"
cp "${TEST_DATA_DIR}/ct_minimal.dcm" "${FILE}" || exit 2
STUDY=$(dicom_value "${FILE}" 0020,000d) || exit 2
DEST_PORT=$(find_available_port 41402) || exit 2
start_server destination localhost "${DEST_PORT}" storescp -aet DEST_SCP -od "${WORK_DIR}/destination" "${DEST_PORT}" || exit 1
PORT=$(find_available_port 41400) || exit 2
start_server qr-scp localhost "${PORT}" "${BINARY_DIR}/qr_scp" "${PORT}" PACS_MOVE \
    --storage-dir "${WORK_DIR}/source" --index-db "${WORK_DIR}/index.db" \
    --peer "DEST_SCP:127.0.0.1:${DEST_PORT}" || exit 1
retrieve() {
    expect_exit 0 45 movescu -S -aec PACS_MOVE -aem DEST_SCP -k QueryRetrieveLevel=STUDY \
        -k "StudyInstanceUID=${STUDY}" localhost "${PORT}" &&
        verify_received_uids "${WORK_DIR}/destination" "${FILE}"
}
unknown_destination() {
    expect_exit 69 20 movescu -v -S -aec PACS_MOVE -aem UNKNOWN_DEST -k QueryRetrieveLevel=STUDY \
        -k "StudyInstanceUID=${STUDY}" localhost "${PORT}" &&
        grep -Eiq 'MoveDestinationUnknown|Move Destination unknown|a801' "${LAST_COMMAND_LOG}"
}
connection_error() {
    local unused
    unused=$(find_available_port 41410) || return 1
    expect_exit 61 10 movescu -S -aec NONEXISTENT -aem DEST_SCP -k QueryRetrieveLevel=STUDY \
        -k "StudyInstanceUID=${STUDY}" localhost "${unused}" &&
        grep -Eiq 'refused|connect' "${LAST_COMMAND_LOG}"
}
run_case 'C-MOVE to registered destination' retrieve
run_case 'Unknown destination rejected' unknown_destination
run_case 'Connection error' connection_error
finish_suite
