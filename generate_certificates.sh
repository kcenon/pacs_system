#!/bin/bash
# Generate self-signed certificates for PACS system TLS testing

# Exit on any error
set -e

# Create directories
mkdir -p ./data/certs

# Move to certificates directory
cd ./data/certs

echo "Generating certificates for PACS system TLS testing..."

# Generate CA key and certificate
echo "Generating CA certificate..."
openssl genrsa -out ca.key 2048
openssl req -x509 -new -nodes -key ca.key -sha256 -days 3650 -out ca.crt \
    -subj "/C=US/ST=State/L=City/O=PACS System/OU=Security/CN=PACS-CA"

# Generate server key and certificate signing request
echo "Generating server certificate..."
openssl genrsa -out server.key 2048
openssl req -new -key server.key -out server.csr \
    -subj "/C=US/ST=State/L=City/O=PACS System/OU=Server/CN=pacs-server"

# Create server certificate extension configuration file
cat > server.ext << EOF
authorityKeyIdentifier=keyid,issuer
basicConstraints=CA:FALSE
keyUsage = digitalSignature, nonRepudiation, keyEncipherment, dataEncipherment
extendedKeyUsage = serverAuth
subjectAltName = @alt_names

[alt_names]
DNS.1 = localhost
DNS.2 = pacs-server
EOF

# Generate server certificate
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial \
    -out server.crt -days 825 -sha256 -extfile server.ext

# Generate client key and certificate signing request
echo "Generating client certificate..."
openssl genrsa -out client.key 2048
openssl req -new -key client.key -out client.csr \
    -subj "/C=US/ST=State/L=City/O=PACS System/OU=Client/CN=pacs-client"

# Create client certificate extension configuration file
cat > client.ext << EOF
authorityKeyIdentifier=keyid,issuer
basicConstraints=CA:FALSE
keyUsage = digitalSignature, nonRepudiation, keyEncipherment, dataEncipherment
extendedKeyUsage = clientAuth
subjectAltName = @alt_names

[alt_names]
DNS.1 = localhost
DNS.2 = pacs-client
EOF

# Generate client certificate
openssl x509 -req -in client.csr -CA ca.crt -CAkey ca.key -CAcreateserial \
    -out client.crt -days 825 -sha256 -extfile client.ext

# Clean up temporary files
rm -f server.csr client.csr server.ext client.ext

# Set permissions
chmod 600 *.key
chmod 644 *.crt

echo "Certificates generated successfully in ./data/certs/"
echo "Files:"
ls -la

# Return to original directory
cd ../../

echo "Done."