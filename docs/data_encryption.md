# PACS System Data Encryption Guide

## Overview

The PACS system provides comprehensive encryption capabilities to protect patient data at rest and in transit, ensuring HIPAA compliance and data security.

## Encryption Features

- **AES-256 Encryption**: Industry-standard encryption for data at rest
- **Multiple Algorithms**: Support for AES-GCM, AES-CBC, and ChaCha20-Poly1305
- **Transparent Encryption**: Automatic encryption/decryption of DICOM files
- **Key Management**: Secure key generation, storage, and rotation
- **Database Encryption**: Support for encrypted database storage
- **Audit Trail**: All encryption operations are logged

## Configuration

### Basic Configuration
```json
{
  "storage": {
    "encryption": {
      "enabled": true,
      "algorithm": "AES_256_GCM"
    }
  },
  "security": {
    "encryption": {
      "enabled": true,
      "algorithm": "AES_256_GCM",
      "key_file": "./data/security/master.key"
    }
  }
}
```

### Encryption Algorithms

1. **AES-256-GCM** (Recommended)
   - Authenticated encryption
   - 256-bit key size
   - 96-bit nonce
   - 128-bit authentication tag

2. **AES-256-CBC**
   - Traditional block cipher mode
   - 256-bit key size
   - 128-bit IV
   - HMAC-SHA256 for authentication

3. **ChaCha20-Poly1305**
   - Stream cipher alternative
   - 256-bit key size
   - 96-bit nonce
   - 128-bit authentication tag

## Key Management

### Master Key Generation

The system automatically generates a secure master key on first run:
```bash
# Key is stored at: ./data/security/master.key
# Permissions are set to 600 (owner read/write only)
```

### Manual Key Generation
```bash
# Generate a 256-bit key
openssl rand -hex 32 > master.key
chmod 600 master.key
```

### Environment Variable
```bash
export PACS_MASTER_KEY=<hex-encoded-256-bit-key>
```

### Key Rotation

Keys should be rotated periodically:
```cpp
auto& cryptoManager = CryptoManager::getInstance();
auto result = cryptoManager.rotateKeys();
```

## DICOM File Encryption

### Automatic Encryption

All DICOM files are automatically encrypted when stored:
```cpp
auto& storage = EncryptedStorage::getInstance();
auto result = storage.storeDicomFile(sopInstanceUid, dicomData, userId);
```

### File Format

Encrypted DICOM files have the `.dcm.enc` extension and contain:
- Version header (4 bytes)
- Algorithm identifier (4 bytes)
- Key ID for rotation support
- Nonce/IV
- Authentication tag
- Encrypted DICOM data

### Transparent Decryption

Files are automatically decrypted when retrieved:
```cpp
auto result = storage.retrieveDicomFile(sopInstanceUid, userId);
```

## Database Encryption

### SQLite Encryption

For SQLite databases, use SQLCipher:
```json
{
  "database": {
    "type": "sqlite",
    "encryption": {
      "enabled": true,
      "cipher": "aes-256-cbc"
    }
  }
}
```

### PostgreSQL Encryption

For PostgreSQL, use transparent data encryption (TDE):
```json
{
  "database": {
    "type": "postgresql",
    "encryption": {
      "enabled": true,
      "method": "pgcrypto"
    }
  }
}
```

## Migration

### Encrypting Existing Files

To encrypt existing unencrypted DICOM files:
```cpp
auto& storage = EncryptedStorage::getInstance();
auto result = storage.migrateUnencryptedFiles("/path/to/unencrypted/files");
```

### Batch Migration Script
```bash
#!/bin/bash
# migrate_to_encrypted.sh

SOURCE_DIR="/data/dicom/unencrypted"
BACKUP_DIR="/data/dicom/backup"

# Create backup
cp -r $SOURCE_DIR $BACKUP_DIR

# Run migration
./pacs_migrate --source $SOURCE_DIR --encrypt
```

## Performance Considerations

### Encryption Overhead
- AES-256-GCM: ~5-10% CPU overhead
- Minimal impact on disk I/O
- Negligible impact on memory usage

### Optimization Tips
1. Use hardware AES acceleration when available
2. Enable encryption only for sensitive data
3. Use SSD storage for encrypted files
4. Configure appropriate thread pool size

## Security Best Practices

1. **Key Protection**
   - Store master key outside application directory
   - Use hardware security modules (HSM) for production
   - Implement key escrow for recovery

2. **Access Control**
   - Restrict file system permissions
   - Use separate encryption keys per tenant
   - Implement role-based decryption

3. **Monitoring**
   - Monitor encryption/decryption operations
   - Alert on failed decryption attempts
   - Track key usage patterns

4. **Compliance**
   - Document encryption procedures
   - Maintain encryption audit logs
   - Regular security assessments

## Troubleshooting

### Common Issues

1. **"Failed to initialize crypto manager"**
   - Check if master key file exists
   - Verify file permissions (should be 600)
   - Check available disk space

2. **"Decryption failed - data may be corrupted"**
   - Verify file integrity
   - Check if correct key is being used
   - Review audit logs for details

3. **Performance degradation**
   - Check CPU usage during encryption
   - Monitor disk I/O patterns
   - Consider hardware acceleration

### Debug Mode

Enable detailed crypto logging:
```json
{
  "config": {
    "log.level": "DEBUG",
    "crypto.debug": true
  }
}
```

## API Reference

### CryptoManager
```cpp
// Encrypt data
auto result = cryptoManager.encrypt(plaintext, associatedData);

// Decrypt data
auto result = cryptoManager.decrypt(encryptionResult, associatedData);

// Encrypt file
auto result = cryptoManager.encryptFile(inputPath, outputPath);
```

### EncryptedStorage
```cpp
// Store encrypted DICOM
auto result = storage.storeDicomFile(uid, data, userId);

// Retrieve and decrypt DICOM
auto result = storage.retrieveDicomFile(uid, userId);

// Check encryption status
bool encrypted = storage.isEncryptionEnabled();
```

## Compliance

### HIPAA Requirements
- ✓ Encryption of data at rest (AES-256)
- ✓ Access control and authentication
- ✓ Audit logging of access
- ✓ Integrity verification
- ✓ Secure key management

### GDPR Compliance
- ✓ Strong encryption for personal data
- ✓ Right to erasure (secure deletion)
- ✓ Data portability (decrypt for export)
- ✓ Privacy by design