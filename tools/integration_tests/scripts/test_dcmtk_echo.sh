#!/bin/bash
# Usage: test_dcmtk_echo.sh [build_dir]
set -o pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/binary_common.sh"
init_suite dcmtk-echo "${1:-}" || exit 2
require_commands echoscu storescp || exit 2
check_dcmtk_available || exit 2
check_cli_contract echo_scp 1 '<port> <ae_title>' || exit 2
check_cli_contract echo_scu 0 '-aec' '-aet' || exit 2
PACS_PORT=$(find_available_port 41112) || exit 2
start_server pacs-echo localhost "${PACS_PORT}" "${BINARY_DIR}/echo_scp" "${PACS_PORT}" PACS_ECHO || exit 1
DCMTK_PORT=$(find_available_port 41116) || exit 2
mkdir -p "${WORK_DIR}/dcmtk"
# storescp implements C-ECHO and ships in DCMTK packages without echoscp.
start_server dcmtk-echo localhost "${DCMTK_PORT}" storescp -aet DCMTK_ECHO -od "${WORK_DIR}/dcmtk" "${DCMTK_PORT}" || exit 1
multiple_echoes() {
    local i
    for i in 1 2 3 4 5; do
        expect_exit 0 15 echoscu -aec PACS_ECHO -aet "ECHOSCU_${i}" localhost "${PACS_PORT}" || return 1
    done
}
connection_error() {
    local unused
    unused=$(find_available_port 41120) || return 1
    expect_exit 1 10 echoscu -aec NONEXISTENT localhost "${unused}" &&
        grep -Eiq 'refused|connect' "${LAST_COMMAND_LOG}"
}
run_case 'DCMTK baseline C-ECHO' expect_exit 0 15 echoscu -aec DCMTK_ECHO localhost "${DCMTK_PORT}"
run_case 'DCMTK SCU to PACS SCP' expect_exit 0 15 echoscu -aec PACS_ECHO localhost "${PACS_PORT}"
run_case 'Five consecutive echoes' multiple_echoes
run_case 'PACS SCU to DCMTK SCP' expect_exit 0 15 "${BINARY_DIR}/echo_scu" -aec DCMTK_ECHO -aet PACS_SCU localhost "${DCMTK_PORT}"
run_case 'Connection error' connection_error
finish_suite
