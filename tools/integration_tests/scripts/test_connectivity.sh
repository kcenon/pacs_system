#!/bin/bash
# Usage: test_connectivity.sh [build_dir]
set -o pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/binary_common.sh"
init_suite connectivity "${1:-}" || exit 2
check_cli_contract echo_scp 1 '<port> <ae_title>' || exit 2
check_cli_contract echo_scu 0 '[options] <peer> <port>' '-aec' '-aet' || exit 2
PORT=$(find_available_port 41200) || exit 2
AE=TEST_SCP
start_server echo-scp localhost "${PORT}" "${BINARY_DIR}/echo_scp" "${PORT}" "${AE}" || exit 1

echo_once() {
    expect_exit 0 15 "${BINARY_DIR}/echo_scu" -aec "${AE}" "$@" localhost "${PORT}" &&
        grep -q 'Status: SUCCESS' "${LAST_COMMAND_LOG}"
}
multiple_echoes() {
    local i
    for i in 1 2 3; do echo_once -aet "ECHOSCU_${i}" || return 1; done
}
connection_failure() {
    local unused
    unused=$(find_available_port 41900) || return 1
    expect_exit 1 10 "${BINARY_DIR}/echo_scu" -to 3 -aec FAKE_SCP localhost "${unused}" &&
        grep -Eiq 'connect|refused' "${LAST_COMMAND_LOG}"
}
invalid_ae() {
    expect_exit 2 10 "${BINARY_DIR}/echo_scu" -aec THIS_AE_TITLE_IS_WAY_TOO_LONG localhost "${PORT}" &&
        grep -Eiq 'AE.*(16|long|invalid)' "${LAST_COMMAND_LOG}"
}
run_case 'Basic Echo' echo_once
run_case 'Multiple Sequential Echos' multiple_echoes
run_case 'Custom Calling AE' echo_once -aet MY_SCU
run_case 'Connection Failure (Expected)' connection_failure
run_case 'Invalid AE Title' invalid_ae
finish_suite
