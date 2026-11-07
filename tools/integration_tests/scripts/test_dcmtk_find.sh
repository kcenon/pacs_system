#!/bin/bash
# Usage: test_dcmtk_find.sh [build_dir]
set -o pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/binary_common.sh"
init_suite dcmtk-find "${1:-}" || exit 2
require_commands echoscu storescu findscu dcmdump || exit 2
check_dcmtk_available || exit 2
check_cli_contract pacs_server 1 '--db-path' '--log-level' || exit 2
FILE="${TEST_DATA_DIR}/ct_minimal.dcm"
STUDY=$(dicom_value "${FILE}" 0020,000d) || exit 2
PATIENT=$(dicom_value "${FILE}" 0010,0020) || exit 2
PATIENT_NAME=$(dicom_value "${FILE}" 0010,0010) || exit 2
STUDY_DATE=$(dicom_value "${FILE}" 0008,0020) || exit 2
PORT=$(find_available_port 41300) || exit 2
start_pacs_server "${PORT}" PACS_FIND || exit 1
expect_exit 0 30 storescu -aec PACS_FIND localhost "${PORT}" "${FILE}" || exit 1
QUERY_NUMBER=0
query() {
    local model="$1" level="$2" tag="$3" expected="$4"
    shift 4
    QUERY_NUMBER=$((QUERY_NUMBER + 1))
    local output="${WORK_DIR}/query-${QUERY_NUMBER}"
    mkdir -p "${output}" || return 1
    expect_exit 0 30 findscu "${model}" -aec PACS_FIND -aet SHELL_FIND -X -od "${output}" \
        -k "QueryRetrieveLevel=${level}" -k "${tag}=" "$@" localhost "${PORT}" || return 1
    local expected_values=()
    [[ -n "${expected}" ]] && expected_values+=("${expected}")
    python3 "${SCRIPT_DIR}/dicom_inventory.py" query "${output}" "${tag}" "${expected_values[@]}"
}
query_levels() {
    query -S STUDY 0020,000d "${STUDY}" -k "PatientID=${PATIENT}" &&
        query -P PATIENT 0010,0020 "${PATIENT}" -k "PatientName=${PATIENT_NAME}"
}
connection_error() {
    local unused
    unused=$(find_available_port 41308) || return 1
    expect_exit 2 10 findscu -S -aec NONEXISTENT -k QueryRetrieveLevel=STUDY localhost "${unused}" &&
        grep -Eiq 'refused|connect' "${LAST_COMMAND_LOG}"
}
run_case 'Seeded study query' query -S STUDY 0020,000d "${STUDY}"
run_case 'Patient and study models' query_levels
run_case 'Wildcard query' query -S STUDY 0020,000d "${STUDY}" -k "PatientName=${PATIENT_NAME:0:3}*"
run_case 'Nonmatching wildcard query' query -S STUDY 0020,000d '' -k 'PatientName=NO_SUCH_PATIENT*'
run_case 'Date range query' query -S STUDY 0020,000d "${STUDY}" -k "StudyDate=${STUDY_DATE}-${STUDY_DATE}"
run_case 'Open start date range' query -S STUDY 0020,000d "${STUDY}" -k "StudyDate=-${STUDY_DATE}"
run_case 'Open end date range' query -S STUDY 0020,000d "${STUDY}" -k "StudyDate=${STUDY_DATE}-"
run_case 'Nonmatching date range' query -S STUDY 0020,000d '' -k 'StudyDate=19000101-19000102'
run_case 'Empty query result' query -S STUDY 0020,000d '' -k PatientID=NO_SUCH_PATIENT_1258
run_case 'Connection error' connection_error
finish_suite
