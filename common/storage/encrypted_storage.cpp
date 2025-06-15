#include "encrypted_storage.h"
#include "common/logger/logger.h"
#include "common/config/config_manager.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>
#include <chrono>

namespace pacs {
namespace common {
namespace storage {

EncryptedStorage& EncryptedStorage::getInstance() {
    static EncryptedStorage instance;
    return instance;
}

core::Result<void> EncryptedStorage::initialize(const std::string& storageRoot) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        return core::Result<void>::error("EncryptedStorage already initialized");
    }
    
    try {
        storageRoot_ = storageRoot;
        
        // Create storage root if it doesn't exist
        std::filesystem::create_directories(storageRoot_);
        
        // Initialize crypto manager
        auto& cryptoManager = security::CryptoManager::getInstance();
        if (!cryptoManager.isInitialized()) {
            auto result = cryptoManager.initialize();
            if (!result.isOk()) {
                return core::Result<void>::error("Failed to initialize crypto manager: " + result.error());
            }
        }
        
        // Check configuration for encryption setting
        auto& configManager = config::ConfigManager::getInstance();
        auto encryptionEnabled = configManager.getValue("storage.encryption.enabled", "true");
        encryptionEnabled_ = (encryptionEnabled == "true" || encryptionEnabled == "1");
        
        initialized_ = true;
        logger::logInfo("EncryptedStorage initialized with root: {}, encryption: {}", 
                        storageRoot_, encryptionEnabled_ ? "enabled" : "disabled");
        
        return core::Result<void>::ok();
    }
    catch (const std::exception& ex) {
        return core::Result<void>::error("Failed to initialize EncryptedStorage: " + std::string(ex.what()));
    }
}

core::Result<std::string> EncryptedStorage::storeDicomFile(
    const std::string& sopInstanceUid,
    const std::vector<uint8_t>& dicomData,
    const std::string& userId) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return core::Result<std::string>::error("EncryptedStorage not initialized");
    }
    
    try {
        // Sanitize UID
        std::string sanitizedUid = sanitizeUid(sopInstanceUid);
        
        // Create storage directory
        std::string directory = createStorageDirectory(sanitizedUid);
        std::filesystem::create_directories(directory);
        
        // Determine file path
        std::string filename = sanitizedUid + (encryptionEnabled_ ? ".dcm.enc" : ".dcm");
        std::string fullPath = directory + "/" + filename;
        
        if (encryptionEnabled_) {
            // Create temporary file
            std::string tempPath = fullPath + ".tmp";
            std::ofstream tempFile(tempPath, std::ios::binary);
            if (!tempFile.is_open()) {
                return core::Result<std::string>::error("Failed to create temporary file");
            }
            tempFile.write(reinterpret_cast<const char*>(dicomData.data()), dicomData.size());
            tempFile.close();
            
            // Encrypt the file
            auto& cryptoManager = security::CryptoManager::getInstance();
            auto result = cryptoManager.encryptFile(tempPath, fullPath);
            
            // Remove temporary file
            std::filesystem::remove(tempPath);
            
            if (!result.isOk()) {
                return core::Result<std::string>::error("Failed to encrypt DICOM file: " + result.error());
            }
        } else {
            // Store unencrypted
            std::ofstream file(fullPath, std::ios::binary);
            if (!file.is_open()) {
                return core::Result<std::string>::error("Failed to create file");
            }
            file.write(reinterpret_cast<const char*>(dicomData.data()), dicomData.size());
            file.close();
        }
        
        // Log audit event
        if (!userId.empty()) {
            audit::AUDIT_LOG_STUDY_ACCESS(userId, sopInstanceUid, "store", "Success");
        }
        
        logger::logInfo("Stored DICOM file: {} (encrypted: {})", sopInstanceUid, encryptionEnabled_);
        
        return core::Result<std::string>::success(fullPath);
    }
    catch (const std::exception& ex) {
        return core::Result<std::string>::error("Failed to store DICOM file: " + std::string(ex.what()));
    }
}

core::Result<std::vector<uint8_t>> EncryptedStorage::retrieveDicomFile(
    const std::string& sopInstanceUid,
    const std::string& userId) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return core::Result<std::vector<uint8_t>>::error("EncryptedStorage not initialized");
    }
    
    try {
        // Sanitize UID
        std::string sanitizedUid = sanitizeUid(sopInstanceUid);
        
        // Find the file
        std::string encryptedPath = getStoragePath(sopInstanceUid) + ".enc";
        std::string unencryptedPath = getStoragePath(sopInstanceUid);
        
        std::string actualPath;
        bool isEncrypted = false;
        
        if (std::filesystem::exists(encryptedPath)) {
            actualPath = encryptedPath;
            isEncrypted = true;
        } else if (std::filesystem::exists(unencryptedPath)) {
            actualPath = unencryptedPath;
            isEncrypted = false;
        } else {
            return core::Result<std::vector<uint8_t>>::error("DICOM file not found: " + sopInstanceUid);
        }
        
        std::vector<uint8_t> dicomData;
        
        if (isEncrypted) {
            // Create temporary file for decryption
            std::string tempPath = actualPath + ".dec.tmp";
            
            // Decrypt the file
            auto& cryptoManager = security::CryptoManager::getInstance();
            auto result = cryptoManager.decryptFile(actualPath, tempPath);
            
            if (!result.isOk()) {
                return core::Result<std::vector<uint8_t>>::error("Failed to decrypt DICOM file: " + result.error());
            }
            
            // Read decrypted file
            std::ifstream file(tempPath, std::ios::binary);
            if (!file.is_open()) {
                std::filesystem::remove(tempPath);
                return core::Result<std::vector<uint8_t>>::error("Failed to open decrypted file");
            }
            
            file.seekg(0, std::ios::end);
            size_t fileSize = file.tellg();
            file.seekg(0, std::ios::beg);
            
            dicomData.resize(fileSize);
            file.read(reinterpret_cast<char*>(dicomData.data()), fileSize);
            file.close();
            
            // Remove temporary file
            std::filesystem::remove(tempPath);
        } else {
            // Read unencrypted file
            std::ifstream file(actualPath, std::ios::binary);
            if (!file.is_open()) {
                return core::Result<std::vector<uint8_t>>::error("Failed to open file");
            }
            
            file.seekg(0, std::ios::end);
            size_t fileSize = file.tellg();
            file.seekg(0, std::ios::beg);
            
            dicomData.resize(fileSize);
            file.read(reinterpret_cast<char*>(dicomData.data()), fileSize);
            file.close();
        }
        
        // Log audit event
        if (!userId.empty()) {
            audit::AUDIT_LOG_STUDY_ACCESS(userId, sopInstanceUid, "retrieve", "Success");
        }
        
        return core::Result<std::vector<uint8_t>>::ok(std::move(dicomData));
    }
    catch (const std::exception& ex) {
        return core::Result<std::vector<uint8_t>>::error("Failed to retrieve DICOM file: " + std::string(ex.what()));
    }
}

core::Result<void> EncryptedStorage::deleteDicomFile(
    const std::string& sopInstanceUid,
    const std::string& userId) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return core::Result<void>::error("EncryptedStorage not initialized");
    }
    
    try {
        // Find and delete the file
        std::string encryptedPath = getStoragePath(sopInstanceUid) + ".enc";
        std::string unencryptedPath = getStoragePath(sopInstanceUid);
        
        bool deleted = false;
        
        if (std::filesystem::exists(encryptedPath)) {
            std::filesystem::remove(encryptedPath);
            deleted = true;
        }
        
        if (std::filesystem::exists(unencryptedPath)) {
            std::filesystem::remove(unencryptedPath);
            deleted = true;
        }
        
        if (!deleted) {
            return core::Result<void>::error("DICOM file not found: " + sopInstanceUid);
        }
        
        // Log audit event
        if (!userId.empty()) {
            audit::AUDIT_LOG_STUDY_ACCESS(userId, sopInstanceUid, "delete", "Success");
        }
        
        logger::logInfo("Deleted DICOM file: {}", sopInstanceUid);
        
        return core::Result<void>::ok();
    }
    catch (const std::exception& ex) {
        return core::Result<void>::error("Failed to delete DICOM file: " + std::string(ex.what()));
    }
}

bool EncryptedStorage::exists(const std::string& sopInstanceUid) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return false;
    }
    
    std::string encryptedPath = getStoragePath(sopInstanceUid) + ".enc";
    std::string unencryptedPath = getStoragePath(sopInstanceUid);
    
    return std::filesystem::exists(encryptedPath) || std::filesystem::exists(unencryptedPath);
}

std::string EncryptedStorage::getStoragePath(const std::string& sopInstanceUid) const {
    std::string sanitizedUid = sanitizeUid(sopInstanceUid);
    std::string directory = createStorageDirectory(sanitizedUid);
    return directory + "/" + sanitizedUid + ".dcm";
}

EncryptedStorage::StorageStats EncryptedStorage::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    StorageStats stats{};
    
    if (!initialized_) {
        return stats;
    }
    
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(storageRoot_)) {
            if (entry.is_regular_file()) {
                stats.totalFiles++;
                stats.totalSizeBytes += entry.file_size();
                
                if (entry.path().extension() == ".enc") {
                    stats.encryptedFiles++;
                } else if (entry.path().extension() == ".dcm") {
                    stats.unencryptedFiles++;
                }
            }
        }
    }
    catch (const std::exception& ex) {
        logger::logError("Failed to calculate storage stats: {}", ex.what());
    }
    
    return stats;
}

core::Result<void> EncryptedStorage::migrateUnencryptedFiles(const std::string& sourceDirectory) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return core::Result<void>::error("EncryptedStorage not initialized");
    }
    
    if (!encryptionEnabled_) {
        return core::Result<void>::error("Encryption is not enabled");
    }
    
    try {
        size_t migratedCount = 0;
        size_t errorCount = 0;
        
        for (const auto& entry : std::filesystem::recursive_directory_iterator(sourceDirectory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".dcm") {
                // Extract SOP Instance UID from filename
                std::string filename = entry.path().stem().string();
                
                // Read file
                std::ifstream file(entry.path(), std::ios::binary);
                if (!file.is_open()) {
                    errorCount++;
                    continue;
                }
                
                file.seekg(0, std::ios::end);
                size_t fileSize = file.tellg();
                file.seekg(0, std::ios::beg);
                
                std::vector<uint8_t> dicomData(fileSize);
                file.read(reinterpret_cast<char*>(dicomData.data()), fileSize);
                file.close();
                
                // Store with encryption
                auto result = storeDicomFile(filename, dicomData, "MIGRATION");
                if (result.isSuccess()) {
                    // Delete original unencrypted file
                    std::filesystem::remove(entry.path());
                    migratedCount++;
                } else {
                    errorCount++;
                    logger::logError("Failed to migrate file {}: {}", 
                                     entry.path().string(), result.error());
                }
            }
        }
        
        logger::logInfo("Migration completed: {} files migrated, {} errors", 
                        migratedCount, errorCount);
        
        if (errorCount > 0) {
            return core::Result<void>::error("Migration completed with " + 
                                             std::to_string(errorCount) + " errors");
        }
        
        return core::Result<void>::ok();
    }
    catch (const std::exception& ex) {
        return core::Result<void>::error("Migration failed: " + std::string(ex.what()));
    }
}

void EncryptedStorage::setEncryptionEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    encryptionEnabled_ = enabled;
    logger::logInfo("Encryption {}", enabled ? "enabled" : "disabled");
}

std::string EncryptedStorage::createStorageDirectory(const std::string& sopInstanceUid) const {
    // Use a hierarchical directory structure based on UID
    // Example: 1.2.3.4.5 -> storageRoot/1/2/3/4
    std::string sanitizedUid = sanitizeUid(sopInstanceUid);
    std::vector<std::string> parts;
    
    std::stringstream ss(sanitizedUid);
    std::string part;
    int count = 0;
    
    while (std::getline(ss, part, '.') && count < 4) {
        if (!part.empty()) {
            parts.push_back(part);
            count++;
        }
    }
    
    std::string path = storageRoot_;
    for (const auto& p : parts) {
        path += "/" + p;
    }
    
    return path;
}

std::string EncryptedStorage::sanitizeUid(const std::string& sopInstanceUid) const {
    // Remove any characters that might cause filesystem issues
    std::string sanitized = sopInstanceUid;
    
    // Replace problematic characters
    std::regex invalidChars("[^a-zA-Z0-9._-]");
    sanitized = std::regex_replace(sanitized, invalidChars, "_");
    
    // Ensure it doesn't start with a dot
    if (!sanitized.empty() && sanitized[0] == '.') {
        sanitized[0] = '_';
    }
    
    return sanitized;
}

// TempDecryptedFile implementation
TempDecryptedFile::TempDecryptedFile(const std::string& sopInstanceUid, const std::string& userId)
    : sopInstanceUid_(sopInstanceUid) {
    
    try {
        auto& storage = EncryptedStorage::getInstance();
        auto result = storage.retrieveDicomFile(sopInstanceUid, userId);
        
        if (result.isOk()) {
            // Create temporary file
            tempPath_ = std::filesystem::temp_directory_path() / 
                        ("dicom_" + sopInstanceUid + "_" + 
                         std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + 
                         ".dcm");
            
            std::ofstream file(tempPath_, std::ios::binary);
            if (file.is_open()) {
                const auto& data = result.value();
                file.write(reinterpret_cast<const char*>(data.data()), data.size());
                file.close();
            } else {
                tempPath_.clear();
            }
        }
    }
    catch (const std::exception& ex) {
        logger::logError("Failed to create temporary decrypted file: {}", ex.what());
        tempPath_.clear();
    }
}

TempDecryptedFile::~TempDecryptedFile() {
    if (!tempPath_.empty() && std::filesystem::exists(tempPath_)) {
        try {
            // Securely overwrite before deletion
            std::ofstream file(tempPath_, std::ios::binary);
            if (file.is_open()) {
                size_t fileSize = std::filesystem::file_size(tempPath_);
                std::vector<uint8_t> zeros(fileSize, 0);
                file.write(reinterpret_cast<const char*>(zeros.data()), zeros.size());
                file.close();
            }
            
            std::filesystem::remove(tempPath_);
        }
        catch (const std::exception& ex) {
            logger::logError("Failed to delete temporary file: {}", ex.what());
        }
    }
}

} // namespace storage
} // namespace common
} // namespace pacs