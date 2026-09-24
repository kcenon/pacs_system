#!/bin/bash
#
# generate_certs.sh - Generate test certificates for DICOM TLS communication
#
# This script generates a complete PKI setup for testing TLS-secured DICOM:
#   - CA certificate (self-signed root)
#   - Server certificate (signed by CA)
#   - Client certificate (signed by CA, for mutual TLS)
#
# Usage:
#   ./generate_certs.sh [output_directory]
#
# Default output: ./certs/
#
# See Issue #110 - TLS DICOM Connection Sample
#

set -e

# Configuration
CERT_DIR="${1:-./certs}"
DAYS_VALID=365
KEY_SIZE=2048
CA_KEY_SIZE=4096

# Certificate subjects
CA_SUBJECT="/CN=DICOM Test CA/O=PACS System/OU=Test PKI"
SERVER_SUBJECT="/CN=localhost/O=PACS System/OU=DICOM Server"
CLIENT_SUBJECT="/CN=DICOM Client/O=PACS System/OU=DICOM SCU"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_header() {
    echo -e "${BLUE}"
    echo "========================================================"
    echo " DICOM TLS Certificate Generator"
    echo "========================================================"
    echo -e "${NC}"
}

print_step() {
    echo -e "${YELLOW}>>> $1${NC}"
}

print_success() {
    echo -e "${GREEN}[OK] $1${NC}"
}

print_error() {
    echo -e "${RED}[ERROR] $1${NC}"
}

# Check OpenSSL is available
check_openssl() {
    if ! command -v openssl &> /dev/null; then
        print_error "OpenSSL is not installed or not in PATH"
        exit 1
    fi

    local version=$(openssl version)
    print_success "Found: $version"
}

# Create output directory
create_output_dir() {
    print_step "Creating output directory: $CERT_DIR"
    mkdir -p "$CERT_DIR"
    print_success "Directory ready"
}

# Generate CA certificate
generate_ca() {
    print_step "Generating CA certificate (self-signed root)..."

    # Generate CA private key
    openssl genrsa -out "$CERT_DIR/ca.key" $CA_KEY_SIZE 2>/dev/null

    # Generate CA certificate
    openssl req -x509 -new -nodes \
        -key "$CERT_DIR/ca.key" \
        -sha256 \
        -days $DAYS_VALID \
        -out "$CERT_DIR/ca.crt" \
        -subj "$CA_SUBJECT"

    print_success "CA certificate generated"
    echo "    - Private key: $CERT_DIR/ca.key"
    echo "    - Certificate: $CERT_DIR/ca.crt"
}

# Generate server certificate
generate_server_cert() {
    print_step "Generating server certificate..."

    # Generate server private key
    openssl genrsa -out "$CERT_DIR/server.key" $KEY_SIZE 2>/dev/null

    # Create server CSR config with SAN
    cat > "$CERT_DIR/server.cnf" << EOF
[req]
distinguished_name = req_distinguished_name
req_extensions = v3_req
prompt = no

[req_distinguished_name]
CN = localhost
O = PACS System
OU = DICOM Server

[v3_req]
basicConstraints = CA:FALSE
keyUsage = nonRepudiation, digitalSignature, keyEncipherment
extendedKeyUsage = serverAuth
subjectAltName = @alt_names

[alt_names]
DNS.1 = localhost
DNS.2 = *.local
IP.1 = 127.0.0.1
IP.2 = ::1
EOF

    # Generate CSR
    openssl req -new \
        -key "$CERT_DIR/server.key" \
        -out "$CERT_DIR/server.csr" \
        -config "$CERT_DIR/server.cnf"

    # Sign with CA
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

    # Clean up CSR and config
    rm -f "$CERT_DIR/server.csr" "$CERT_DIR/server.cnf"

    print_success "Server certificate generated"
    echo "    - Private key:  $CERT_DIR/server.key"
    echo "    - Certificate:  $CERT_DIR/server.crt"
}

# Generate client certificate
generate_client_cert() {
    print_step "Generating client certificate (for mutual TLS)..."

    # Generate client private key
    openssl genrsa -out "$CERT_DIR/client.key" $KEY_SIZE 2>/dev/null

    # Create client CSR config
    cat > "$CERT_DIR/client.cnf" << EOF
[req]
distinguished_name = req_distinguished_name
req_extensions = v3_req
prompt = no

[req_distinguished_name]
CN = DICOM Client
O = PACS System
OU = DICOM SCU

[v3_req]
basicConstraints = CA:FALSE
keyUsage = nonRepudiation, digitalSignature, keyEncipherment
extendedKeyUsage = clientAuth
EOF

    # Generate CSR
    openssl req -new \
        -key "$CERT_DIR/client.key" \
        -out "$CERT_DIR/client.csr" \
        -config "$CERT_DIR/client.cnf"

    # Sign with CA
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

    # Clean up CSR and config
    rm -f "$CERT_DIR/client.csr" "$CERT_DIR/client.cnf"

    print_success "Client certificate generated"
    echo "    - Private key:  $CERT_DIR/client.key"
    echo "    - Certificate:  $CERT_DIR/client.crt"
}

# Verify certificates
verify_certificates() {
    print_step "Verifying certificate chain..."

    # Verify server cert
    if openssl verify -CAfile "$CERT_DIR/ca.crt" "$CERT_DIR/server.crt" > /dev/null 2>&1; then
        print_success "Server certificate: Valid"
    else
        print_error "Server certificate verification failed!"
        exit 1
    fi

    # Verify client cert
    if openssl verify -CAfile "$CERT_DIR/ca.crt" "$CERT_DIR/client.crt" > /dev/null 2>&1; then
        print_success "Client certificate: Valid"
    else
        print_error "Client certificate verification failed!"
        exit 1
    fi
}

# Print summary and usage
print_summary() {
    echo ""
    echo -e "${BLUE}========================================================"
    echo " Certificate Generation Complete!"
    echo "========================================================${NC}"
    echo ""
    echo "Generated files in $CERT_DIR/:"
    echo "  ca.crt      - CA certificate (distribute to all parties)"
    echo "  ca.key      - CA private key (keep secure!)"
    echo "  server.crt  - Server certificate"
    echo "  server.key  - Server private key"
    echo "  client.crt  - Client certificate"
    echo "  client.key  - Client private key"
    echo ""
    echo -e "${GREEN}Usage Examples:${NC}"
    echo ""
    echo "Start secure DICOM server:"
    echo "  ./secure_echo_scp 2762 MY_PACS \\"
    echo "      --cert $CERT_DIR/server.crt \\"
    echo "      --key $CERT_DIR/server.key \\"
    echo "      --ca $CERT_DIR/ca.crt"
    echo ""
    echo "Connect with secure client (server verification only):"
    echo "  ./secure_echo_scu localhost 2762 MY_PACS \\"
    echo "      --ca $CERT_DIR/ca.crt"
    echo ""
    echo "Connect with mutual TLS (client certificate):"
    echo "  ./secure_echo_scu localhost 2762 MY_PACS \\"
    echo "      --cert $CERT_DIR/client.crt \\"
    echo "      --key $CERT_DIR/client.key \\"
    echo "      --ca $CERT_DIR/ca.crt"
    echo ""
    echo -e "${YELLOW}Note: These certificates are for TESTING ONLY.${NC}"
    echo "For production, use certificates from a trusted CA."
    echo ""
}

# Set appropriate permissions
set_permissions() {
    print_step "Setting secure file permissions..."

    # Private keys should only be readable by owner
    chmod 600 "$CERT_DIR"/*.key

    # Certificates can be world-readable
    chmod 644 "$CERT_DIR"/*.crt

    print_success "Permissions set"
}

# Clean up serial file
cleanup() {
    rm -f "$CERT_DIR/ca.srl"
}

# Main execution
main() {
    print_header

    check_openssl
    create_output_dir

    echo ""
    generate_ca
    echo ""
    generate_server_cert
    echo ""
    generate_client_cert
    echo ""
    verify_certificates
    echo ""
    set_permissions
    cleanup

    print_summary
}

main "$@"
