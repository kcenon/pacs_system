# PACS System Security Configuration Guide

## Overview

This guide explains how to configure security features in the PACS system for production deployment.

## Initial Setup

### First-Time Installation

1. **No Default Credentials**: The system no longer includes hard-coded default credentials for security reasons.

2. **Initial Admin Setup**: Run the setup script to create your initial admin user:
   ```bash
   ./scripts/setup_initial_admin.sh
   ```

3. **Environment Variables**: Alternatively, you can provide admin credentials via configuration:
   ```json
   {
     "security": {
       "admin": {
         "username": "your_admin_username",
         "password": "your_secure_password",
         "fullname": "Administrator Name",
         "email": "admin@yourhospital.com"
       }
     }
   }
   ```

### Password Requirements

- Minimum 8 characters
- Should include uppercase, lowercase, numbers, and special characters
- Automatically generated passwords are 16 characters with high entropy

## User Management

### User Storage

Users are stored in a JSON file specified by the configuration:
```json
{
  "security": {
    "users": {
      "file": "./data/security/users.json"
    }
  }
}
```

### User Roles

The system supports four user roles:
- **Admin**: Full system access
- **Operator**: Operational access (can modify data)
- **Viewer**: Read-only access
- **User**: Limited access

### Adding Users

Users can be added programmatically through the SecurityManager API:
```cpp
UserCredentials user;
user.username = "john_doe";
user.passwordHash = securityManager.hashPassword("secure_password");
user.role = UserRole::Operator;
user.fullName = "John Doe";
user.email = "john.doe@hospital.com";
user.enabled = true;

auto result = securityManager.addUser(user);
```

## Authentication Types

### Basic Authentication (Default)
- Username/password authentication
- Passwords hashed using PBKDF2-HMAC-SHA256
- 10,000 iterations for key derivation

### Token Authentication
Enable token-based authentication:
```json
{
  "security": {
    "auth": {
      "type": "token"
    }
  }
}
```

### Certificate Authentication
Enable certificate-based authentication:
```json
{
  "security": {
    "auth": {
      "type": "certificate"
    }
  }
}
```

### No Authentication (Development Only)
**WARNING**: Only use for development/testing:
```json
{
  "security": {
    "auth": {
      "type": "none"
    }
  }
}
```

## TLS Configuration

### Enabling TLS
```json
{
  "service": {
    "useTLS": true
  },
  "security": {
    "tls": {
      "enabled": true,
      "min_version": "1.2",
      "cert_verification": "require_peer",
      "key_file": "./data/security/private_key.pem",
      "cert_file": "./data/security/certificate.pem",
      "ca_file": "./data/security/ca_certificate.pem"
    }
  }
}
```

### Cipher Suites
The system uses secure cipher suites by default:
- ECDHE-RSA-AES256-GCM-SHA384
- ECDHE-RSA-AES128-GCM-SHA256
- AES256-GCM-SHA384
- AES128-GCM-SHA256

Weak ciphers (RC4, DES, MD5) are explicitly disabled.

## Security Best Practices

1. **Change Initial Password**: Always change the initial admin password immediately after setup.

2. **Use Strong Passwords**: Enforce strong password policies for all users.

3. **Enable TLS**: Always use TLS for production deployments.

4. **Restrict Access**: Create users with minimum necessary privileges.

5. **Regular Updates**: Keep the system and dependencies updated.

6. **Audit Logs**: Monitor security logs regularly (when audit logging is implemented).

7. **Backup User Database**: Regularly backup the users.json file.

8. **File Permissions**: Ensure proper file permissions on security files:
   ```bash
   chmod 600 ./data/security/users.json
   chmod 600 ./data/security/*.pem
   ```

## Migration from Previous Versions

If upgrading from a version with hard-coded credentials:

1. **Export Existing Users**: Save your current user configuration.

2. **Update Configuration**: Add the security section to your config file.

3. **Run Setup Script**: Use the setup script to create new admin credentials.

4. **Import Users**: Re-create other users with new secure passwords.

5. **Remove Old Files**: Delete any files containing old default credentials.

## Troubleshooting

### Cannot Login
- Verify the users.json file exists and is readable
- Check the configured authentication type
- Ensure passwords meet minimum requirements
- Check logs for authentication errors

### TLS Connection Failed
- Verify certificate files exist and are readable
- Check certificate validity dates
- Ensure certificate matches the hostname
- Verify TLS version compatibility

### Lost Admin Password
1. Stop the PACS server
2. Backup the current users.json file
3. Run the setup script to create a new admin user
4. Restore other users from backup
5. Update all user passwords