# PACS System Security Documentation

This document describes the security features of the PACS System.

## Overview

The PACS System provides several security features to ensure the confidentiality, integrity, and availability of medical imaging data:

1. **TLS Encryption**: Secure communication using Transport Layer Security
2. **Authentication**: User authentication using various methods
3. **Authorization**: Role-based access control
4. **Password Security**: Secure password storage with salting and hashing
5. **Token-based Authentication**: Support for token-based authentication
6. **Certificate-based Authentication**: Support for client certificate authentication

## TLS Encryption

The PACS System supports TLS encryption for all DICOM communications.

### Configuration

TLS is configured through the configuration system with the following parameters:

- `tls.certificate`: Path to the server certificate file
- `tls.private.key`: Path to the server private key file
- `tls.ca.certificate`: Path to the CA certificate file
- `tls.ca.directory`: Path to the directory containing CA certificates
- `tls.verification.mode`: Certificate verification mode (none, relaxed, required)
- `tls.min.protocol`: Minimum TLS protocol version (tlsv1.0, tlsv1.1, tlsv1.2, tlsv1.3, auto)
- `tls.ciphers`: Cipher suite list (OpenSSL format)
- `tls.client.authentication`: Whether to require client certificates

### Certificate Generation

For testing and development, self-signed certificates can be generated using the `generate_certificates.sh` script:

```bash
./generate_certificates.sh
```

This will create the following files in the `./data/certs` directory:

- `ca.crt` and `ca.key`: Certificate authority
- `server.crt` and `server.key`: Server certificate and key
- `client.crt` and `client.key`: Client certificate and key

For production use, certificates should be obtained from a trusted certificate authority.

## Authentication

The PACS System supports several authentication methods:

- **None**: No authentication (not recommended for production)
- **Basic**: Username and password authentication
- **Certificate**: TLS client certificate authentication
- **Token**: Token-based authentication

### Configuration

Authentication is configured through the configuration system:

- `security.auth.type`: Authentication type (none, basic, certificate, token)
- `security.users.file`: Path to the user database file
- `security.create.default.user`: Whether to create a default admin user if the user file doesn't exist

### User Management

Users are managed through the SecurityManager interface:

```cpp
auto& securityManager = pacs::common::security::SecurityManager::getInstance();

// Add a user
pacs::common::security::UserCredentials user;
user.username = "username";
user.passwordHash = securityManager.hashPassword("password");
user.role = pacs::common::security::UserRole::Admin;
user.fullName = "Full Name";
user.email = "email@example.com";
user.enabled = true;
securityManager.addUser(user);

// Remove a user
securityManager.removeUser("username");

// Check user role
bool isAdmin = securityManager.userHasRole("username", pacs::common::security::UserRole::Admin);
```

### Authentication Process

To authenticate a user:

```cpp
auto& securityManager = pacs::common::security::SecurityManager::getInstance();

// Basic authentication
auto result = securityManager.authenticateUser("username", "password");
if (result.authenticated) {
    // Success
    std::string userId = result.userId;
    pacs::common::security::UserRole role = result.role;
} else {
    // Failure
    std::string errorMessage = result.message;
}

// Token authentication
auto tokenResult = securityManager.authenticateToken(token);
if (tokenResult.authenticated) {
    // Success
}
```

## Authorization

The PACS System uses role-based access control with the following roles:

- **Admin**: Full access to all functions
- **Operator**: Operational access, but cannot manage users or system configuration
- **Viewer**: Read-only access to data
- **User**: Basic access to specific functions

Role checks are performed using the SecurityManager:

```cpp
bool hasPermission = securityManager.userHasRole(username, requiredRole);
```

## Password Security

Passwords are stored using PBKDF2 with SHA-256, a strong hashing algorithm designed for password storage:

1. A random salt is generated for each password
2. The password is hashed with PBKDF2-HMAC-SHA256 using 10,000 iterations
3. The salt and hash are stored together in the format `pbkdf2sha256$iterations$salt$hash`

The hash verification checks:
1. The password with the stored salt
2. Using the same number of iterations
3. Comparing the resulting hash with the stored hash

## Token-based Authentication

When token-based authentication is enabled:

1. The user authenticates with username and password
2. A token is generated and returned
3. The token can be used for subsequent authentication

Tokens are random, cryptographically secure identifiers that are mapped to users in the SecurityManager.

## Certificate-based Authentication

When certificate-based authentication is enabled:

1. TLS client authentication is required
2. The client must present a valid certificate
3. The certificate must be signed by a trusted CA
4. The certificate's subject common name is used as the username

## Deployment Recommendations

For production environments:

1. Always use TLS encryption with strong certificates
2. Use a minimum protocol version of TLS 1.2
3. Implement proper user management with strong passwords
4. Consider using client certificate authentication for sensitive operations
5. Apply the principle of least privilege with appropriate user roles
6. Store user data and certificates securely
7. Regularly rotate passwords and certificates
8. Keep the system updated with security patches

## Implementation Details

The security system is implemented in the following files:

- `common/security/tls_config.h/cpp`: TLS configuration
- `common/security/security_manager.h/cpp`: User and authentication management
- `common/security/dcmtk_tls_adapter.h/cpp`: Integration with DCMTK's TLS support