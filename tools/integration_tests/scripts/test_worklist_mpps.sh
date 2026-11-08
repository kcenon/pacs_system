#!/bin/bash
# Usage: test_worklist_mpps.sh [build_dir]
set -o pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/binary_common.sh"
init_suite worklist-mpps "${1:-}" || exit 2
check_cli_contract pacs_server 1 '--log-level' 'warning' || exit 2
check_cli_contract echo_scu 0 '-aec' || exit 2
check_cli_contract worklist_scu 2 '[options] <peer> <port>' '-aec' '--output' || exit 2
check_cli_contract mpps_scu 2 '<host> <port> <called_ae> <command>' "'create' or 'set'" || exit 2
PORT=$(find_available_port 41220) || exit 2
AE=RIS_TEST
MPPS_UID=''
start_pacs_server "${PORT}" "${AE}" || exit 1
empty_worklist() {
    # A fresh database has no scheduled items. Exit 1 is documented for an
    # empty successful query; additionally require the actual final success.
    expect_exit 1 30 "${BINARY_DIR}/worklist_scu" -aec "${AE}" -aet SHELL_MWL \
        --verbose --output text "$@" localhost "${PORT}" &&
        grep -Fq 'Query completed successfully.' "${LAST_COMMAND_LOG}" &&
        grep -Eq 'Total items: +0' "${LAST_COMMAND_LOG}"
}
create_mpps() {
    expect_exit 0 30 "${BINARY_DIR}/mpps_scu" localhost "${PORT}" "${AE}" create \
        --study-uid 1.2.826.0.1.3680043.10.543.1258 --patient-id MPPS_TEST_001 \
        --patient-name 'TEST^MPPS^PATIENT' || return 1
    MPPS_UID=$(sed -n 's/^  MPPS UID: *\([0-9.]*\)$/\1/p' "${LAST_COMMAND_LOG}")
    [[ "${MPPS_UID}" =~ ^[0-9]+(\.[0-9]+)+$ ]]
}
complete_mpps() {
    [[ -n "${MPPS_UID}" ]] || { log_fail 'No UID from a successful N-CREATE'; return 1; }
    expect_exit 0 30 "${BINARY_DIR}/mpps_scu" localhost "${PORT}" "${AE}" set \
        --mpps-uid "${MPPS_UID}" --status COMPLETED \
        --series-uid 1.2.826.0.1.3680043.10.543.1258.1
}
ris_workflow() {
    empty_worklist --modality CT && create_mpps && complete_mpps
}
run_case 'Echo Connectivity' expect_exit 0 15 "${BINARY_DIR}/echo_scu" -aec "${AE}" localhost "${PORT}"
run_case 'Worklist Query (empty success)' empty_worklist --modality CT
run_case 'Worklist Date Filter (empty success)' empty_worklist --date "$(date +%Y%m%d)"
run_case 'MPPS Create' create_mpps
run_case 'MPPS Complete' complete_mpps
run_case 'Full RIS Workflow' ris_workflow
finish_suite
