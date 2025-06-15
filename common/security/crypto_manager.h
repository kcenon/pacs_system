#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>

#include "core/result/result.h"

namespace pacs {
namespace common {
namespace security {

/**
 * @brief Encryption algorithms supported
 */
enum class EncryptionAlgorithm {
    AES_256_GCM,    // AES-256 in GCM mode (recommended)
    AES_256_CBC,    // AES-256 in CBC mode
    ChaCha20Poly1305 // ChaCha20-Poly1305
};

/**
 * @brief Key derivation functions
 */
enum class KeyDerivationFunction {
    PBKDF2_SHA256,  // PBKDF2 with SHA-256
    Argon2id,       // Argon2id (memory-hard)
    Scrypt          // Scrypt (memory-hard)
};

/**
 * @brief Encryption result containing ciphertext and metadata
 */
struct EncryptionResult {
    std::vector<uint8_t> ciphertext;
    std::vector<uint8_t> nonce;      // IV/Nonce
    std::vector<uint8_t> tag;        // Authentication tag (for GCM/Poly1305)
    std::vector<uint8_t> salt;       // Salt for key derivation
    EncryptionAlgorithm algorithm;
    std::string keyId;               // Key identifier for key rotation
};

/**
 * @brief Manages encryption/decryption operations for HIPAA compliance
 */
class CryptoManager {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the CryptoManager singleton
     */
    static CryptoManager& getInstance();
    
    /**
     * @brief Initialize the crypto manager
     * @return Result indicating success or failure
     */
    core::Result<void> initialize();
    
    /**
     * @brief Shutdown the crypto manager
     */
    void shutdown();
    
    /**
     * @brief Encrypt data using the default algorithm
     * @param plaintext Data to encrypt
     * @param associatedData Additional authenticated data (AAD)
     * @return Encryption result or error
     */
    core::Result<EncryptionResult> encrypt(
        const std::vector<uint8_t>& plaintext,
        const std::vector<uint8_t>& associatedData = {});
    
    /**
     * @brief Encrypt data using a specific algorithm
     * @param plaintext Data to encrypt
     * @param algorithm Encryption algorithm to use
     * @param associatedData Additional authenticated data (AAD)
     * @return Encryption result or error
     */
    core::Result<EncryptionResult> encrypt(
        const std::vector<uint8_t>& plaintext,
        EncryptionAlgorithm algorithm,
        const std::vector<uint8_t>& associatedData = {});
    
    /**
     * @brief Decrypt data
     * @param encryptionResult Encryption result containing ciphertext and metadata
     * @param associatedData Additional authenticated data (AAD) used during encryption
     * @return Decrypted plaintext or error
     */
    core::Result<std::vector<uint8_t>> decrypt(
        const EncryptionResult& encryptionResult,
        const std::vector<uint8_t>& associatedData = {});
    
    /**
     * @brief Encrypt a file
     * @param inputPath Path to the file to encrypt
     * @param outputPath Path to save the encrypted file
     * @return Result indicating success or failure
     */
    core::Result<void> encryptFile(
        const std::string& inputPath,
        const std::string& outputPath);
    
    /**
     * @brief Decrypt a file
     * @param inputPath Path to the encrypted file
     * @param outputPath Path to save the decrypted file
     * @return Result indicating success or failure
     */
    core::Result<void> decryptFile(
        const std::string& inputPath,
        const std::string& outputPath);
    
    /**
     * @brief Encrypt a string
     * @param plaintext String to encrypt
     * @return Base64-encoded encrypted string or error
     */
    core::Result<std::string> encryptString(const std::string& plaintext);
    
    /**
     * @brief Decrypt a string
     * @param ciphertext Base64-encoded encrypted string
     * @return Decrypted string or error
     */
    core::Result<std::string> decryptString(const std::string& ciphertext);
    
    /**
     * @brief Generate a cryptographically secure random key
     * @param keySize Key size in bytes (default: 32 for AES-256)
     * @return Generated key
     */
    std::vector<uint8_t> generateKey(size_t keySize = 32);
    
    /**
     * @brief Derive a key from a password
     * @param password Password to derive key from
     * @param salt Salt for key derivation
     * @param kdf Key derivation function to use
     * @param keySize Desired key size in bytes
     * @return Derived key
     */
    std::vector<uint8_t> deriveKey(
        const std::string& password,
        const std::vector<uint8_t>& salt,
        KeyDerivationFunction kdf = KeyDerivationFunction::PBKDF2_SHA256,
        size_t keySize = 32);
    
    /**
     * @brief Generate a cryptographically secure random salt
     * @param saltSize Salt size in bytes (default: 16)
     * @return Generated salt
     */
    std::vector<uint8_t> generateSalt(size_t saltSize = 16);
    
    /**
     * @brief Set the master encryption key
     * @param key Master key for data encryption
     * @return Result indicating success or failure
     */
    core::Result<void> setMasterKey(const std::vector<uint8_t>& key);
    
    /**
     * @brief Load master key from a secure key store
     * @param keyPath Path to the key file
     * @return Result indicating success or failure
     */
    core::Result<void> loadMasterKeyFromFile(const std::string& keyPath);
    
    /**
     * @brief Rotate encryption keys
     * @return Result indicating success or failure
     */
    core::Result<void> rotateKeys();
    
    /**
     * @brief Get current key ID for key rotation tracking
     * @return Current key identifier
     */
    std::string getCurrentKeyId() const;
    
    /**
     * @brief Calculate SHA-256 hash
     * @param data Data to hash
     * @return Hash value
     */
    std::vector<uint8_t> sha256(const std::vector<uint8_t>& data);
    
    /**
     * @brief Calculate HMAC-SHA256
     * @param key HMAC key
     * @param data Data to authenticate
     * @return HMAC value
     */
    std::vector<uint8_t> hmacSha256(
        const std::vector<uint8_t>& key,
        const std::vector<uint8_t>& data);
    
    /**
     * @brief Securely wipe memory
     * @param data Data to wipe
     */
    void secureWipe(std::vector<uint8_t>& data);
    
    /**
     * @brief Check if encryption is properly configured
     * @return True if encryption is ready to use
     */
    bool isInitialized() const { return initialized_; }

private:
    CryptoManager() = default;
    ~CryptoManager();
    
    // Delete copy and move constructors and assignment operators
    CryptoManager(const CryptoManager&) = delete;
    CryptoManager& operator=(const CryptoManager&) = delete;
    CryptoManager(CryptoManager&&) = delete;
    CryptoManager& operator=(CryptoManager&&) = delete;
    
    // Implementation details
    class Impl;
    std::unique_ptr<Impl> pImpl_;
    
    mutable std::mutex mutex_;
    bool initialized_{false};
    
    // Current configuration
    EncryptionAlgorithm defaultAlgorithm_{EncryptionAlgorithm::AES_256_GCM};
    std::string currentKeyId_;
};

/**
 * @brief RAII wrapper for encrypted file operations
 */
class EncryptedFile {
public:
    /**
     * @brief Open an encrypted file for reading
     * @param path Path to the encrypted file
     */
    explicit EncryptedFile(const std::string& path);
    
    /**
     * @brief Create a new encrypted file for writing
     * @param path Path for the new encrypted file
     * @param forWriting True to open for writing
     */
    EncryptedFile(const std::string& path, bool forWriting);
    
    ~EncryptedFile();
    
    /**
     * @brief Read data from the encrypted file
     * @param buffer Buffer to read into
     * @param size Number of bytes to read
     * @return Number of bytes actually read
     */
    size_t read(void* buffer, size_t size);
    
    /**
     * @brief Write data to the encrypted file
     * @param buffer Data to write
     * @param size Number of bytes to write
     * @return Number of bytes actually written
     */
    size_t write(const void* buffer, size_t size);
    
    /**
     * @brief Seek to a position in the file
     * @param offset Offset from the beginning
     * @return New position
     */
    size_t seek(size_t offset);
    
    /**
     * @brief Get current position in the file
     * @return Current position
     */
    size_t tell() const;
    
    /**
     * @brief Get file size
     * @return File size in bytes
     */
    size_t size() const;
    
    /**
     * @brief Check if file is open
     * @return True if file is open
     */
    bool isOpen() const;
    
    /**
     * @brief Close the file
     */
    void close();

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace security
} // namespace common
} // namespace pacs