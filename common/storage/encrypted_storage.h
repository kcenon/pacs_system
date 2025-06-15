#pragma once

#include <string>
#include <memory>
#include "core/result/result.h"
#include "common/security/crypto_manager.h"
#include "common/audit/audit_logger.h"

namespace pacs {
namespace common {
namespace storage {

/**
 * @brief Encrypted storage wrapper for DICOM files
 * 
 * This class provides transparent encryption/decryption of DICOM files
 * stored on disk, ensuring HIPAA compliance for data at rest.
 */
class EncryptedStorage {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the EncryptedStorage singleton
     */
    static EncryptedStorage& getInstance();
    
    /**
     * @brief Initialize the encrypted storage
     * @param storageRoot Root directory for encrypted storage
     * @return Result indicating success or failure
     */
    core::Result<void> initialize(const std::string& storageRoot);
    
    /**
     * @brief Store a DICOM file with encryption
     * @param sopInstanceUid SOP Instance UID
     * @param dicomData DICOM file data
     * @param userId User ID for audit logging
     * @return Result containing the storage path or error
     */
    core::Result<std::string> storeDicomFile(
        const std::string& sopInstanceUid,
        const std::vector<uint8_t>& dicomData,
        const std::string& userId = "");
    
    /**
     * @brief Retrieve a DICOM file with decryption
     * @param sopInstanceUid SOP Instance UID
     * @param userId User ID for audit logging
     * @return Result containing the DICOM data or error
     */
    core::Result<std::vector<uint8_t>> retrieveDicomFile(
        const std::string& sopInstanceUid,
        const std::string& userId = "");
    
    /**
     * @brief Delete an encrypted DICOM file
     * @param sopInstanceUid SOP Instance UID
     * @param userId User ID for audit logging
     * @return Result indicating success or failure
     */
    core::Result<void> deleteDicomFile(
        const std::string& sopInstanceUid,
        const std::string& userId = "");
    
    /**
     * @brief Check if a DICOM file exists
     * @param sopInstanceUid SOP Instance UID
     * @return True if the file exists
     */
    bool exists(const std::string& sopInstanceUid) const;
    
    /**
     * @brief Get the storage path for a SOP Instance UID
     * @param sopInstanceUid SOP Instance UID
     * @return Full path to the encrypted file
     */
    std::string getStoragePath(const std::string& sopInstanceUid) const;
    
    /**
     * @brief Get storage statistics
     * @return Storage statistics (size, file count, etc.)
     */
    struct StorageStats {
        size_t totalFiles;
        size_t totalSizeBytes;
        size_t encryptedFiles;
        size_t unencryptedFiles;
    };
    StorageStats getStats() const;
    
    /**
     * @brief Migrate unencrypted files to encrypted storage
     * @param sourceDirectory Directory containing unencrypted DICOM files
     * @return Result indicating success or failure
     */
    core::Result<void> migrateUnencryptedFiles(const std::string& sourceDirectory);
    
    /**
     * @brief Enable or disable encryption
     * @param enabled True to enable encryption
     */
    void setEncryptionEnabled(bool enabled);
    
    /**
     * @brief Check if encryption is enabled
     * @return True if encryption is enabled
     */
    bool isEncryptionEnabled() const { return encryptionEnabled_; }

private:
    EncryptedStorage() = default;
    
    // Delete copy and move constructors and assignment operators
    EncryptedStorage(const EncryptedStorage&) = delete;
    EncryptedStorage& operator=(const EncryptedStorage&) = delete;
    EncryptedStorage(EncryptedStorage&&) = delete;
    EncryptedStorage& operator=(EncryptedStorage&&) = delete;
    
    std::string storageRoot_;
    bool initialized_{false};
    bool encryptionEnabled_{true};
    mutable std::mutex mutex_;
    
    /**
     * @brief Create directory structure for storage
     * @param sopInstanceUid SOP Instance UID
     * @return Directory path
     */
    std::string createStorageDirectory(const std::string& sopInstanceUid) const;
    
    /**
     * @brief Sanitize SOP Instance UID for filesystem use
     * @param sopInstanceUid SOP Instance UID
     * @return Sanitized UID
     */
    std::string sanitizeUid(const std::string& sopInstanceUid) const;
};

/**
 * @brief RAII wrapper for temporary decrypted files
 */
class TempDecryptedFile {
public:
    /**
     * @brief Create a temporary decrypted file
     * @param sopInstanceUid SOP Instance UID to decrypt
     * @param userId User ID for audit logging
     */
    TempDecryptedFile(const std::string& sopInstanceUid, const std::string& userId = "");
    
    ~TempDecryptedFile();
    
    /**
     * @brief Get the path to the temporary decrypted file
     * @return Path to the file, or empty if decryption failed
     */
    const std::string& getPath() const { return tempPath_; }
    
    /**
     * @brief Check if the decryption was successful
     * @return True if the file was successfully decrypted
     */
    bool isValid() const { return !tempPath_.empty(); }

private:
    std::string tempPath_;
    std::string sopInstanceUid_;
};

} // namespace storage
} // namespace common
} // namespace pacs