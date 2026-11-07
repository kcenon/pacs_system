#!/bin/bash
# Usage: test_secure_dicom.sh [build_dir]
# Requires a server that verifies client certificates, with actual TLS on wire.
set -o pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/binary_common.sh"
init_suite secure "${1:-}" || exit 2
require_commands openssl || exit 2
check_cli_contract secure_echo_scp 1 '<port> <ae_title>' '--cert' '--key' || exit 2
check_cli_contract secure_echo_scu 1 '<host> <port> <called_ae>' '--cert' '--key' || exit 2
CERT_DIR="${WORK_DIR}/certs"
expect_exit 0 60 bash "${SCRIPT_DIR}/../../secure_dicom/generate_certs.sh" "${CERT_DIR}" || exit 2
expect_exit 0 15 openssl req -new -x509 -days 1 -nodes -newkey rsa:2048 \
    -keyout "${CERT_DIR}/wrong-ca.key" -out "${CERT_DIR}/wrong-ca.crt" -subj /CN=WrongCA || exit 2
PORT=$(find_available_port 41230) || exit 2
AE=SECURE_SCP
start_server secure-scp localhost "${PORT}" "${BINARY_DIR}/secure_echo_scp" "${PORT}" "${AE}" \
    --cert "${CERT_DIR}/server.crt" --key "${CERT_DIR}/server.key" --ca "${CERT_DIR}/ca.crt" || exit 1
secure_echo() {
    expect_exit 0 15 "${BINARY_DIR}/secure_echo_scu" localhost "${PORT}" "${AE}" \
        --cert "${CERT_DIR}/client.crt" --key "${CERT_DIR}/client.key" --ca "${CERT_DIR}/ca.crt" &&
        grep -Eiq 'Status: +SUCCESS' "${LAST_COMMAND_LOG}"
}
multiple_echoes() {
    local i
    for (( i=0; i<3; ++i )); do secure_echo || return 1; done
}
no_client_cert() {
    expect_exit 1 15 "${BINARY_DIR}/secure_echo_scu" localhost "${PORT}" "${AE}" \
        --ca "${CERT_DIR}/ca.crt" &&
        grep -Eiq 'certificate.required|peer did not return a certificate' "${LAST_COMMAND_LOG}" &&
        secure_echo
}
wrong_ca() {
    expect_exit 1 15 "${BINARY_DIR}/secure_echo_scu" localhost "${PORT}" "${AE}" \
        --cert "${CERT_DIR}/client.crt" --key "${CERT_DIR}/client.key" --ca "${CERT_DIR}/wrong-ca.crt" &&
        grep -Fq 'certificate verify failed' "${LAST_COMMAND_LOG}" && secure_echo
}
tls_version() {
    expect_exit 0 15 openssl s_client -brief -verify_return_error -verify_hostname localhost \
        -connect "localhost:${PORT}" -CAfile "${CERT_DIR}/ca.crt" \
        -cert "${CERT_DIR}/client.crt" -key "${CERT_DIR}/client.key" </dev/null &&
        grep -Eq 'Protocol version: TLSv1\.[23]' "${LAST_COMMAND_LOG}" &&
        grep -Fq 'Verification: OK' "${LAST_COMMAND_LOG}"
}
run_case 'Basic Secure Echo' secure_echo
run_case 'Multiple Secure Echos' multiple_echoes
run_case 'Missing Client Certificate Rejected' no_client_cert
run_case 'Wrong CA Rejected' wrong_ca
run_case 'TLS Version and Peer Verification' tls_version
finish_suite
