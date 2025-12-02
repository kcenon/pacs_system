#!/bin/bash
#
# generate_test_certs.sh - Generate test certificates for TLS integration tests
#
# Creates a complete PKI setup for testing TLS-secured DICOM:
#   - CA certificate (self-signed root)
#   - Server certificate (signed by CA)
#   - Client certificate (signed by CA, for mutual TLS)
#   - Expired certificate (for validation testing)
#
# Usage:
#   ./generate_test_certs.sh [output_directory]
#
# Default output: current directory
#
# @see Issue #141 - TLS Integration Tests
#

set -e

# Configuration
CERT_DIR="${1:-.}"
DAYS_VALID=365
KEY_SIZE=2048
CA_KEY_SIZE=4096

# Certificate subjects
CA_SUBJECT="/CN=DICOM Test CA/O=PACS System/OU=Integration Test"
SERVER_SUBJECT="/CN=localhost/O=PACS System/OU=Test Server"
CLIENT_SUBJECT="/CN=Test Client/O=PACS System/OU=Test SCU"
EXPIRED_SUBJECT="/CN=Expired Test/O=PACS System/OU=Expired"

# Check OpenSSL is available
check_openssl() {
    if ! command -v openssl &> /dev/null; then
        echo "Error: OpenSSL is not installed"
        exit 1
    fi
}

# Generate CA certificate
generate_ca() {
    echo "Generating CA certificate..."

    openssl genrsa -out "$CERT_DIR/ca.key" $CA_KEY_SIZE 2>/dev/null
    openssl req -x509 -new -nodes \
        -key "$CERT_DIR/ca.key" \
        -sha256 \
        -days $DAYS_VALID \
        -out "$CERT_DIR/ca.crt" \
        -subj "$CA_SUBJECT"
}

# Generate server certificate
generate_server_cert() {
    echo "Generating server certificate..."

    openssl genrsa -out "$CERT_DIR/server.key" $KEY_SIZE 2>/dev/null

    cat > "$CERT_DIR/server.cnf" << EOF
[req]
distinguished_name = req_distinguished_name
req_extensions = v3_req
prompt = no

[req_distinguished_name]
CN = localhost
O = PACS System
OU = Test Server

[v3_req]
basicConstraints = CA:FALSE
keyUsage = nonRepudiation, digitalSignature, keyEncipherment
extendedKeyUsage = serverAuth
subjectAltName = @alt_names

[alt_names]
DNS.1 = localhost
IP.1 = 127.0.0.1
IP.2 = ::1
EOF

    openssl req -new \
        -key "$CERT_DIR/server.key" \
        -out "$CERT_DIR/server.csr" \
        -config "$CERT_DIR/server.cnf"

    openssl x509 -req \
        -in "$CERT_DIR/server.csr" \
        -CA "$CERT_DIR/ca.crt" \
        -CAkey "$CERT_DIR/ca.key" \
        -CAcreateserial \
        -out "$CERT_DIR/server.crt" \
        -days $DAYS_VALID \
        -sha256 \
        -extensions v3_req \
        -extfile "$CERT_DIR/server.cnf" 2>/dev/null

    rm -f "$CERT_DIR/server.csr" "$CERT_DIR/server.cnf"
}

# Generate client certificate
generate_client_cert() {
    echo "Generating client certificate..."

    openssl genrsa -out "$CERT_DIR/client.key" $KEY_SIZE 2>/dev/null

    cat > "$CERT_DIR/client.cnf" << EOF
[req]
distinguished_name = req_distinguished_name
req_extensions = v3_req
prompt = no

[req_distinguished_name]
CN = Test Client
O = PACS System
OU = Test SCU

[v3_req]
basicConstraints = CA:FALSE
keyUsage = nonRepudiation, digitalSignature, keyEncipherment
extendedKeyUsage = clientAuth
EOF

    openssl req -new \
        -key "$CERT_DIR/client.key" \
        -out "$CERT_DIR/client.csr" \
        -config "$CERT_DIR/client.cnf"

    openssl x509 -req \
        -in "$CERT_DIR/client.csr" \
        -CA "$CERT_DIR/ca.crt" \
        -CAkey "$CERT_DIR/ca.key" \
        -CAcreateserial \
        -out "$CERT_DIR/client.crt" \
        -days $DAYS_VALID \
        -sha256 \
        -extensions v3_req \
        -extfile "$CERT_DIR/client.cnf" 2>/dev/null

    rm -f "$CERT_DIR/client.csr" "$CERT_DIR/client.cnf"
}

# Generate expired certificate for testing
generate_expired_cert() {
    echo "Generating expired certificate..."

    openssl genrsa -out "$CERT_DIR/expired.key" $KEY_SIZE 2>/dev/null

    cat > "$CERT_DIR/expired.cnf" << EOF
[req]
distinguished_name = req_distinguished_name
req_extensions = v3_req
prompt = no

[req_distinguished_name]
CN = Expired Test
O = PACS System
OU = Expired

[v3_req]
basicConstraints = CA:FALSE
keyUsage = nonRepudiation, digitalSignature, keyEncipherment
extendedKeyUsage = serverAuth, clientAuth
EOF

    openssl req -new \
        -key "$CERT_DIR/expired.key" \
        -out "$CERT_DIR/expired.csr" \
        -config "$CERT_DIR/expired.cnf"

    # Create certificate that expired yesterday
    openssl x509 -req \
        -in "$CERT_DIR/expired.csr" \
        -CA "$CERT_DIR/ca.crt" \
        -CAkey "$CERT_DIR/ca.key" \
        -CAcreateserial \
        -out "$CERT_DIR/expired.crt" \
        -days -1 \
        -sha256 \
        -extensions v3_req \
        -extfile "$CERT_DIR/expired.cnf" 2>/dev/null || {
        # Fallback: create with faketime if available
        echo "Note: Could not create backdated certificate"
        rm -f "$CERT_DIR/expired.key" "$CERT_DIR/expired.csr" "$CERT_DIR/expired.cnf"
        return
    }

    rm -f "$CERT_DIR/expired.csr" "$CERT_DIR/expired.cnf"
}

# Generate different CA for wrong-CA testing
generate_different_ca() {
    echo "Generating different CA certificate..."

    openssl genrsa -out "$CERT_DIR/other_ca.key" $CA_KEY_SIZE 2>/dev/null
    openssl req -x509 -new -nodes \
        -key "$CERT_DIR/other_ca.key" \
        -sha256 \
        -days $DAYS_VALID \
        -out "$CERT_DIR/other_ca.crt" \
        -subj "/CN=Different CA/O=Other Organization/OU=PKI"
}

# Verify certificates
verify_certs() {
    echo "Verifying certificates..."

    if openssl verify -CAfile "$CERT_DIR/ca.crt" "$CERT_DIR/server.crt" > /dev/null 2>&1; then
        echo "  Server certificate: OK"
    else
        echo "  Server certificate: FAILED"
        exit 1
    fi

    if openssl verify -CAfile "$CERT_DIR/ca.crt" "$CERT_DIR/client.crt" > /dev/null 2>&1; then
        echo "  Client certificate: OK"
    else
        echo "  Client certificate: FAILED"
        exit 1
    fi
}

# Set permissions
set_permissions() {
    chmod 600 "$CERT_DIR"/*.key 2>/dev/null || true
    chmod 644 "$CERT_DIR"/*.crt 2>/dev/null || true
}

# Cleanup serial file
cleanup() {
    rm -f "$CERT_DIR/ca.srl"
}

# Main
main() {
    check_openssl
    mkdir -p "$CERT_DIR"

    generate_ca
    generate_server_cert
    generate_client_cert
    generate_expired_cert
    generate_different_ca
    verify_certs
    set_permissions
    cleanup

    echo ""
    echo "Test certificates generated in: $CERT_DIR"
    echo "Files: ca.crt, ca.key, server.crt, server.key, client.crt, client.key"
}

main "$@"
