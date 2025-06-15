#include "crypto_manager.h"
#include "common/logger/logger.h"
#include "common/config/config_manager.h"

#include <fstream>
#include <filesystem>
#include <cstring>

#ifdef HAVE_CRYPTOPP
#include <cryptopp/aes.h>
#include <cryptopp/gcm.h>
#include <cryptopp/sha.h>
#include <cryptopp/hmac.h>
#include <cryptopp/pwdbased.h>
#include <cryptopp/osrng.h>
#include <cryptopp/hex.h>
#include <cryptopp/base64.h>
#include <cryptopp/files.h>
#include <cryptopp/filters.h>

using namespace CryptoPP;
#endif

namespace pacs {
namespace common {
namespace security {

// Implementation class
class CryptoManager::Impl {
public:
    Impl() {
#ifdef HAVE_CRYPTOPP
        // Initialize random number generator
        rng_.reset(new AutoSeededRandomPool());
#endif
    }
    
    ~Impl() {
        // Securely clear keys
        if (!masterKey_.empty()) {
            secureWipe(masterKey_);
        }
        for (auto& [id, key] : keyStore_) {
            secureWipe(key);
        }
    }
    
    void secureWipe(std::vector<uint8_t>& data) {
        if (!data.empty()) {
            volatile uint8_t* p = data.data();
            std::memset(const_cast<uint8_t*>(p), 0, data.size());
            // Prevent compiler optimization
            asm volatile("" : : "r"(p) : "memory");
        }
        data.clear();
    }
    
#ifdef HAVE_CRYPTOPP
    std::unique_ptr<AutoSeededRandomPool> rng_;
#endif
    std::vector<uint8_t> masterKey_;
    std::map<std::string, std::vector<uint8_t>> keyStore_;
    std::string currentKeyId_ = "default";
};

// EncryptedFile implementation
class EncryptedFile::Impl {
public:
    std::string path_;
    bool isWriting_{false};
    std::vector<uint8_t> buffer_;
    size_t position_{0};
    EncryptionResult metadata_;
};

// CryptoManager implementation
CryptoManager& CryptoManager::getInstance() {
    static CryptoManager instance;
    return instance;
}

CryptoManager::~CryptoManager() {
    shutdown();
}

core::Result<void> CryptoManager::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        return core::Result<void>::error("CryptoManager already initialized");
    }
    
    try {
        pImpl_ = std::make_unique<Impl>();
        
        // Load configuration
        auto& configManager = config::ConfigManager::getInstance();
        
        // Check if encryption is enabled
        auto encryptionEnabled = configManager.getValue("security.encryption.enabled", "true");
        if (encryptionEnabled != "true" && encryptionEnabled != "1") {
            logger::logWarning("Data encryption is disabled in configuration");
            return core::Result<void>::ok();
        }
        
        // Load master key
        auto keyPath = configManager.getValue("security.encryption.key_file", "");
        if (!keyPath.empty()) {
            auto result = loadMasterKeyFromFile(keyPath);
            if (!result.isOk()) {
                // Generate new key if not found
                logger::logInfo("Generating new master encryption key");
                pImpl_->masterKey_ = generateKey(32);
                
                // Save key to file
                std::filesystem::create_directories(std::filesystem::path(keyPath).parent_path());
                std::ofstream keyFile(keyPath, std::ios::binary);
                if (keyFile.is_open()) {
                    keyFile.write(reinterpret_cast<const char*>(pImpl_->masterKey_.data()), 
                                  pImpl_->masterKey_.size());
                    keyFile.close();
                    
                    // Set restrictive permissions
                    std::filesystem::permissions(keyPath, 
                        std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
                }
            }
        } else {
            // Use environment variable or generate temporary key
            const char* envKey = std::getenv("PACS_MASTER_KEY");
            if (envKey) {
                // Decode from hex
                std::string hexKey(envKey);
                pImpl_->masterKey_.resize(hexKey.length() / 2);
                for (size_t i = 0; i < pImpl_->masterKey_.size(); ++i) {
                    std::string byte = hexKey.substr(i * 2, 2);
                    pImpl_->masterKey_[i] = static_cast<uint8_t>(std::stoul(byte, nullptr, 16));
                }
            } else {
                logger::logWarning("No encryption key configured, generating temporary key");
                pImpl_->masterKey_ = generateKey(32);
            }
        }
        
        // Set default algorithm
        auto algorithm = configManager.getValue("security.encryption.algorithm", "AES_256_GCM");
        if (algorithm == "AES_256_CBC") {
            defaultAlgorithm_ = EncryptionAlgorithm::AES_256_CBC;
        } else if (algorithm == "ChaCha20Poly1305") {
            defaultAlgorithm_ = EncryptionAlgorithm::ChaCha20Poly1305;
        } else {
            defaultAlgorithm_ = EncryptionAlgorithm::AES_256_GCM;
        }
        
        initialized_ = true;
        logger::logInfo("CryptoManager initialized with {} encryption", algorithm);
        
        return core::Result<void>::ok();
    }
    catch (const std::exception& ex) {
        return core::Result<void>::error("Failed to initialize CryptoManager: " + std::string(ex.what()));
    }
}

void CryptoManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return;
    }
    
    // Clear sensitive data
    if (pImpl_) {
        pImpl_.reset();
    }
    
    initialized_ = false;
    logger::logInfo("CryptoManager shutdown");
}

core::Result<EncryptionResult> CryptoManager::encrypt(
    const std::vector<uint8_t>& plaintext,
    const std::vector<uint8_t>& associatedData) {
    
    return encrypt(plaintext, defaultAlgorithm_, associatedData);
}

core::Result<EncryptionResult> CryptoManager::encrypt(
    const std::vector<uint8_t>& plaintext,
    EncryptionAlgorithm algorithm,
    const std::vector<uint8_t>& associatedData) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || pImpl_->masterKey_.empty()) {
        return core::Result<EncryptionResult>::error("CryptoManager not initialized or no key available");
    }
    
#ifdef HAVE_CRYPTOPP
    try {
        EncryptionResult result;
        result.algorithm = algorithm;
        result.keyId = currentKeyId_;
        
        switch (algorithm) {
            case EncryptionAlgorithm::AES_256_GCM: {
                // Generate nonce (96 bits for GCM)
                result.nonce.resize(12);
                pImpl_->rng_->GenerateBlock(result.nonce.data(), result.nonce.size());
                
                // Encrypt
                GCM<AES>::Encryption encryptor;
                encryptor.SetKeyWithIV(pImpl_->masterKey_.data(), pImpl_->masterKey_.size(),
                                       result.nonce.data(), result.nonce.size());
                
                // Add associated data
                if (!associatedData.empty()) {
                    encryptor.Update(associatedData.data(), associatedData.size());
                }
                
                // Encrypt plaintext
                result.ciphertext.resize(plaintext.size());
                encryptor.ProcessData(result.ciphertext.data(), plaintext.data(), plaintext.size());
                
                // Get authentication tag
                result.tag.resize(16);  // 128-bit tag
                encryptor.TruncatedFinal(result.tag.data(), result.tag.size());
                
                break;
            }
            
            case EncryptionAlgorithm::AES_256_CBC: {
                // Generate IV (128 bits for CBC)
                result.nonce.resize(16);
                pImpl_->rng_->GenerateBlock(result.nonce.data(), result.nonce.size());
                
                // Encrypt
                CBC_Mode<AES>::Encryption encryptor;
                encryptor.SetKeyWithIV(pImpl_->masterKey_.data(), pImpl_->masterKey_.size(),
                                       result.nonce.data(), result.nonce.size());
                
                // Add padding and encrypt
                StringSource ss(plaintext.data(), plaintext.size(), true,
                    new StreamTransformationFilter(encryptor,
                        new StringSinkTemplate<std::vector<uint8_t>>(result.ciphertext)));
                
                // Calculate HMAC for authentication
                HMAC<SHA256> hmac(pImpl_->masterKey_.data(), pImpl_->masterKey_.size());
                hmac.Update(result.nonce.data(), result.nonce.size());
                hmac.Update(result.ciphertext.data(), result.ciphertext.size());
                if (!associatedData.empty()) {
                    hmac.Update(associatedData.data(), associatedData.size());
                }
                result.tag.resize(32);
                hmac.Final(result.tag.data());
                
                break;
            }
            
            case EncryptionAlgorithm::ChaCha20Poly1305: {
                // Generate nonce (96 bits for ChaCha20-Poly1305)
                result.nonce.resize(12);
                pImpl_->rng_->GenerateBlock(result.nonce.data(), result.nonce.size());
                
                // Note: ChaCha20-Poly1305 implementation would go here
                // For now, fall back to AES-GCM
                return encrypt(plaintext, EncryptionAlgorithm::AES_256_GCM, associatedData);
            }
            
            default:
                return core::Result<EncryptionResult>::error("Unsupported encryption algorithm");
        }
        
        return core::Result<EncryptionResult>::ok(std::move(result));
    }
    catch (const std::exception& ex) {
        return core::Result<EncryptionResult>::error("Encryption failed: " + std::string(ex.what()));
    }
#else
    return core::Result<EncryptionResult>::error("Encryption support not available");
#endif
}

core::Result<std::vector<uint8_t>> CryptoManager::decrypt(
    const EncryptionResult& encryptionResult,
    const std::vector<uint8_t>& associatedData) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || pImpl_->masterKey_.empty()) {
        return core::Result<std::vector<uint8_t>>::error("CryptoManager not initialized or no key available");
    }
    
#ifdef HAVE_CRYPTOPP
    try {
        std::vector<uint8_t> plaintext;
        
        // Get key for the specified key ID
        std::vector<uint8_t> key = pImpl_->masterKey_;  // For now, use master key
        
        switch (encryptionResult.algorithm) {
            case EncryptionAlgorithm::AES_256_GCM: {
                GCM<AES>::Decryption decryptor;
                decryptor.SetKeyWithIV(key.data(), key.size(),
                                       encryptionResult.nonce.data(), encryptionResult.nonce.size());
                
                // Add associated data
                if (!associatedData.empty()) {
                    decryptor.Update(associatedData.data(), associatedData.size());
                }
                
                // Decrypt and verify
                plaintext.resize(encryptionResult.ciphertext.size());
                decryptor.ProcessData(plaintext.data(), 
                                      encryptionResult.ciphertext.data(), 
                                      encryptionResult.ciphertext.size());
                
                // Verify authentication tag
                if (!decryptor.TruncatedVerify(encryptionResult.tag.data(), encryptionResult.tag.size())) {
                    return core::Result<std::vector<uint8_t>>::error("Authentication failed - data may be corrupted");
                }
                
                break;
            }
            
            case EncryptionAlgorithm::AES_256_CBC: {
                // Verify HMAC first
                HMAC<SHA256> hmac(key.data(), key.size());
                hmac.Update(encryptionResult.nonce.data(), encryptionResult.nonce.size());
                hmac.Update(encryptionResult.ciphertext.data(), encryptionResult.ciphertext.size());
                if (!associatedData.empty()) {
                    hmac.Update(associatedData.data(), associatedData.size());
                }
                
                std::vector<uint8_t> calculatedTag(32);
                hmac.Final(calculatedTag.data());
                
                if (calculatedTag != encryptionResult.tag) {
                    return core::Result<std::vector<uint8_t>>::error("Authentication failed - data may be corrupted");
                }
                
                // Decrypt
                CBC_Mode<AES>::Decryption decryptor;
                decryptor.SetKeyWithIV(key.data(), key.size(),
                                       encryptionResult.nonce.data(), encryptionResult.nonce.size());
                
                StringSource ss(encryptionResult.ciphertext.data(), encryptionResult.ciphertext.size(), true,
                    new StreamTransformationFilter(decryptor,
                        new StringSinkTemplate<std::vector<uint8_t>>(plaintext)));
                
                break;
            }
            
            default:
                return core::Result<std::vector<uint8_t>>::error("Unsupported encryption algorithm");
        }
        
        return core::Result<std::vector<uint8_t>>::ok(std::move(plaintext));
    }
    catch (const std::exception& ex) {
        return core::Result<std::vector<uint8_t>>::error("Decryption failed: " + std::string(ex.what()));
    }
#else
    return core::Result<std::vector<uint8_t>>::error("Encryption support not available");
#endif
}

core::Result<void> CryptoManager::encryptFile(
    const std::string& inputPath,
    const std::string& outputPath) {
    
    try {
        // Read input file
        std::ifstream inputFile(inputPath, std::ios::binary);
        if (!inputFile.is_open()) {
            return core::Result<void>::error("Failed to open input file: " + inputPath);
        }
        
        // Get file size
        inputFile.seekg(0, std::ios::end);
        size_t fileSize = inputFile.tellg();
        inputFile.seekg(0, std::ios::beg);
        
        // Read file content
        std::vector<uint8_t> plaintext(fileSize);
        inputFile.read(reinterpret_cast<char*>(plaintext.data()), fileSize);
        inputFile.close();
        
        // Add file metadata as associated data
        std::string metadata = "file:" + std::filesystem::path(inputPath).filename().string();
        std::vector<uint8_t> associatedData(metadata.begin(), metadata.end());
        
        // Encrypt
        auto encryptResult = encrypt(plaintext, associatedData);
        if (!encryptResult.isOk()) {
            return core::Result<void>::error("Failed to encrypt file: " + encryptResult.error());
        }
        
        // Write encrypted file
        std::ofstream outputFile(outputPath, std::ios::binary);
        if (!outputFile.is_open()) {
            return core::Result<void>::error("Failed to create output file: " + outputPath);
        }
        
        // Write header
        uint32_t version = 1;
        outputFile.write(reinterpret_cast<const char*>(&version), sizeof(version));
        
        // Write algorithm
        uint32_t algo = static_cast<uint32_t>(encryptResult.value().algorithm);
        outputFile.write(reinterpret_cast<const char*>(&algo), sizeof(algo));
        
        // Write key ID length and key ID
        uint32_t keyIdLen = encryptResult.value().keyId.size();
        outputFile.write(reinterpret_cast<const char*>(&keyIdLen), sizeof(keyIdLen));
        outputFile.write(encryptResult.value().keyId.data(), keyIdLen);
        
        // Write nonce length and nonce
        uint32_t nonceLen = encryptResult.value().nonce.size();
        outputFile.write(reinterpret_cast<const char*>(&nonceLen), sizeof(nonceLen));
        outputFile.write(reinterpret_cast<const char*>(encryptResult.value().nonce.data()), nonceLen);
        
        // Write tag length and tag
        uint32_t tagLen = encryptResult.value().tag.size();
        outputFile.write(reinterpret_cast<const char*>(&tagLen), sizeof(tagLen));
        outputFile.write(reinterpret_cast<const char*>(encryptResult.value().tag.data()), tagLen);
        
        // Write ciphertext length and ciphertext
        uint64_t ciphertextLen = encryptResult.value().ciphertext.size();
        outputFile.write(reinterpret_cast<const char*>(&ciphertextLen), sizeof(ciphertextLen));
        outputFile.write(reinterpret_cast<const char*>(encryptResult.value().ciphertext.data()), ciphertextLen);
        
        outputFile.close();
        
        // Set restrictive permissions on encrypted file
        std::filesystem::permissions(outputPath,
            std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
        
        return core::Result<void>::ok();
    }
    catch (const std::exception& ex) {
        return core::Result<void>::error("Failed to encrypt file: " + std::string(ex.what()));
    }
}

core::Result<void> CryptoManager::decryptFile(
    const std::string& inputPath,
    const std::string& outputPath) {
    
    try {
        // Read encrypted file
        std::ifstream inputFile(inputPath, std::ios::binary);
        if (!inputFile.is_open()) {
            return core::Result<void>::error("Failed to open encrypted file: " + inputPath);
        }
        
        // Read header
        uint32_t version;
        inputFile.read(reinterpret_cast<char*>(&version), sizeof(version));
        if (version != 1) {
            return core::Result<void>::error("Unsupported encrypted file version");
        }
        
        // Read algorithm
        uint32_t algo;
        inputFile.read(reinterpret_cast<char*>(&algo), sizeof(algo));
        
        EncryptionResult encResult;
        encResult.algorithm = static_cast<EncryptionAlgorithm>(algo);
        
        // Read key ID
        uint32_t keyIdLen;
        inputFile.read(reinterpret_cast<char*>(&keyIdLen), sizeof(keyIdLen));
        encResult.keyId.resize(keyIdLen);
        inputFile.read(encResult.keyId.data(), keyIdLen);
        
        // Read nonce
        uint32_t nonceLen;
        inputFile.read(reinterpret_cast<char*>(&nonceLen), sizeof(nonceLen));
        encResult.nonce.resize(nonceLen);
        inputFile.read(reinterpret_cast<char*>(encResult.nonce.data()), nonceLen);
        
        // Read tag
        uint32_t tagLen;
        inputFile.read(reinterpret_cast<char*>(&tagLen), sizeof(tagLen));
        encResult.tag.resize(tagLen);
        inputFile.read(reinterpret_cast<char*>(encResult.tag.data()), tagLen);
        
        // Read ciphertext
        uint64_t ciphertextLen;
        inputFile.read(reinterpret_cast<char*>(&ciphertextLen), sizeof(ciphertextLen));
        encResult.ciphertext.resize(ciphertextLen);
        inputFile.read(reinterpret_cast<char*>(encResult.ciphertext.data()), ciphertextLen);
        
        inputFile.close();
        
        // Add file metadata as associated data
        std::string metadata = "file:" + std::filesystem::path(inputPath).filename().string();
        std::vector<uint8_t> associatedData(metadata.begin(), metadata.end());
        
        // Decrypt
        auto decryptResult = decrypt(encResult, associatedData);
        if (!decryptResult.isOk()) {
            return core::Result<void>::error("Failed to decrypt file: " + decryptResult.error());
        }
        
        // Write decrypted file
        std::ofstream outputFile(outputPath, std::ios::binary);
        if (!outputFile.is_open()) {
            return core::Result<void>::error("Failed to create output file: " + outputPath);
        }
        
        outputFile.write(reinterpret_cast<const char*>(decryptResult.value().data()), 
                         decryptResult.value().size());
        outputFile.close();
        
        return core::Result<void>::ok();
    }
    catch (const std::exception& ex) {
        return core::Result<void>::error("Failed to decrypt file: " + std::string(ex.what()));
    }
}

core::Result<std::string> CryptoManager::encryptString(const std::string& plaintext) {
    std::vector<uint8_t> data(plaintext.begin(), plaintext.end());
    
    auto result = encrypt(data);
    if (!result.isOk()) {
        return core::Result<std::string>::error(result.error());
    }
    
    // Serialize to base64
    const auto& encResult = result.value();
    std::stringstream ss;
    
    // Format: algorithm|keyId|nonce|tag|ciphertext (all base64)
    ss << static_cast<int>(encResult.algorithm) << "|";
    ss << encResult.keyId << "|";
    
#ifdef HAVE_CRYPTOPP
    std::string nonceB64, tagB64, ciphertextB64;
    
    Base64Encoder nonceEncoder(new StringSink(nonceB64), false);
    nonceEncoder.Put(encResult.nonce.data(), encResult.nonce.size());
    nonceEncoder.MessageEnd();
    
    Base64Encoder tagEncoder(new StringSink(tagB64), false);
    tagEncoder.Put(encResult.tag.data(), encResult.tag.size());
    tagEncoder.MessageEnd();
    
    Base64Encoder ctEncoder(new StringSink(ciphertextB64), false);
    ctEncoder.Put(encResult.ciphertext.data(), encResult.ciphertext.size());
    ctEncoder.MessageEnd();
    
    ss << nonceB64 << "|" << tagB64 << "|" << ciphertextB64;
#endif
    
    return core::Result<std::string>::success(ss.str());
}

core::Result<std::string> CryptoManager::decryptString(const std::string& ciphertext) {
    // Parse base64 format
    std::vector<std::string> parts;
    std::stringstream ss(ciphertext);
    std::string part;
    
    while (std::getline(ss, part, '|')) {
        parts.push_back(part);
    }
    
    if (parts.size() != 5) {
        return core::Result<std::string>::error("Invalid encrypted string format");
    }
    
    EncryptionResult encResult;
    encResult.algorithm = static_cast<EncryptionAlgorithm>(std::stoi(parts[0]));
    encResult.keyId = parts[1];
    
#ifdef HAVE_CRYPTOPP
    // Decode base64
    Base64Decoder nonceDecoder(new StringSinkTemplate<std::vector<uint8_t>>(encResult.nonce));
    nonceDecoder.Put(reinterpret_cast<const byte*>(parts[2].data()), parts[2].size());
    nonceDecoder.MessageEnd();
    
    Base64Decoder tagDecoder(new StringSinkTemplate<std::vector<uint8_t>>(encResult.tag));
    tagDecoder.Put(reinterpret_cast<const byte*>(parts[3].data()), parts[3].size());
    tagDecoder.MessageEnd();
    
    Base64Decoder ctDecoder(new StringSinkTemplate<std::vector<uint8_t>>(encResult.ciphertext));
    ctDecoder.Put(reinterpret_cast<const byte*>(parts[4].data()), parts[4].size());
    ctDecoder.MessageEnd();
#endif
    
    auto result = decrypt(encResult);
    if (!result.isOk()) {
        return core::Result<std::string>::error(result.error());
    }
    
    return core::Result<std::string>::success(std::string(result.value().begin(), result.value().end()));
}

std::vector<uint8_t> CryptoManager::generateKey(size_t keySize) {
    std::vector<uint8_t> key(keySize);
    
#ifdef HAVE_CRYPTOPP
    if (pImpl_ && pImpl_->rng_) {
        pImpl_->rng_->GenerateBlock(key.data(), key.size());
    }
#endif
    
    return key;
}

std::vector<uint8_t> CryptoManager::deriveKey(
    const std::string& password,
    const std::vector<uint8_t>& salt,
    KeyDerivationFunction kdf,
    size_t keySize) {
    
    std::vector<uint8_t> key(keySize);
    
#ifdef HAVE_CRYPTOPP
    switch (kdf) {
        case KeyDerivationFunction::PBKDF2_SHA256: {
            PKCS5_PBKDF2_HMAC<SHA256> pbkdf;
            pbkdf.DeriveKey(key.data(), key.size(), 0,
                            reinterpret_cast<const byte*>(password.data()), password.size(),
                            salt.data(), salt.size(), 100000);  // 100,000 iterations
            break;
        }
        
        case KeyDerivationFunction::Argon2id:
        case KeyDerivationFunction::Scrypt:
            // TODO: Implement Argon2id and Scrypt
            // For now, fall back to PBKDF2
            return deriveKey(password, salt, KeyDerivationFunction::PBKDF2_SHA256, keySize);
    }
#endif
    
    return key;
}

std::vector<uint8_t> CryptoManager::generateSalt(size_t saltSize) {
    return generateKey(saltSize);  // Same as generating a random key
}

core::Result<void> CryptoManager::setMasterKey(const std::vector<uint8_t>& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (key.size() != 32) {
        return core::Result<void>::error("Master key must be 32 bytes for AES-256");
    }
    
    if (pImpl_) {
        pImpl_->masterKey_ = key;
    }
    
    return core::Result<void>::ok();
}

core::Result<void> CryptoManager::loadMasterKeyFromFile(const std::string& keyPath) {
    try {
        if (!std::filesystem::exists(keyPath)) {
            return core::Result<void>::error("Key file not found: " + keyPath);
        }
        
        std::ifstream keyFile(keyPath, std::ios::binary);
        if (!keyFile.is_open()) {
            return core::Result<void>::error("Failed to open key file: " + keyPath);
        }
        
        // Read key
        keyFile.seekg(0, std::ios::end);
        size_t keySize = keyFile.tellg();
        keyFile.seekg(0, std::ios::beg);
        
        if (keySize != 32) {
            return core::Result<void>::error("Invalid key size, expected 32 bytes");
        }
        
        std::vector<uint8_t> key(keySize);
        keyFile.read(reinterpret_cast<char*>(key.data()), keySize);
        keyFile.close();
        
        return setMasterKey(key);
    }
    catch (const std::exception& ex) {
        return core::Result<void>::error("Failed to load key file: " + std::string(ex.what()));
    }
}

core::Result<void> CryptoManager::rotateKeys() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Generate new key
    auto newKey = generateKey(32);
    
    // Generate new key ID
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    std::string newKeyId = "key_" + std::to_string(timestamp);
    
    // Store old key
    if (pImpl_ && !pImpl_->masterKey_.empty()) {
        pImpl_->keyStore_[currentKeyId_] = pImpl_->masterKey_;
    }
    
    // Set new key
    if (pImpl_) {
        pImpl_->masterKey_ = newKey;
    }
    currentKeyId_ = newKeyId;
    
    logger::logInfo("Encryption keys rotated, new key ID: {}", newKeyId);
    
    return core::Result<void>::ok();
}

std::string CryptoManager::getCurrentKeyId() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentKeyId_;
}

std::vector<uint8_t> CryptoManager::sha256(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> hash(32);
    
#ifdef HAVE_CRYPTOPP
    SHA256 sha;
    sha.Update(data.data(), data.size());
    sha.Final(hash.data());
#endif
    
    return hash;
}

std::vector<uint8_t> CryptoManager::hmacSha256(
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& data) {
    
    std::vector<uint8_t> mac(32);
    
#ifdef HAVE_CRYPTOPP
    HMAC<SHA256> hmac(key.data(), key.size());
    hmac.Update(data.data(), data.size());
    hmac.Final(mac.data());
#endif
    
    return mac;
}

void CryptoManager::secureWipe(std::vector<uint8_t>& data) {
    if (pImpl_) {
        pImpl_->secureWipe(data);
    }
}

// EncryptedFile implementation
EncryptedFile::EncryptedFile(const std::string& path)
    : pImpl_(std::make_unique<Impl>()) {
    pImpl_->path_ = path;
    pImpl_->isWriting_ = false;
    // TODO: Read and decrypt file
}

EncryptedFile::EncryptedFile(const std::string& path, bool forWriting)
    : pImpl_(std::make_unique<Impl>()) {
    pImpl_->path_ = path;
    pImpl_->isWriting_ = forWriting;
}

EncryptedFile::~EncryptedFile() {
    close();
}

size_t EncryptedFile::read(void* buffer, size_t size) {
    if (!pImpl_ || pImpl_->isWriting_) {
        return 0;
    }
    
    // TODO: Implement encrypted read
    return 0;
}

size_t EncryptedFile::write(const void* buffer, size_t size) {
    if (!pImpl_ || !pImpl_->isWriting_) {
        return 0;
    }
    
    // TODO: Implement encrypted write
    return 0;
}

size_t EncryptedFile::seek(size_t offset) {
    if (!pImpl_) {
        return 0;
    }
    
    pImpl_->position_ = offset;
    return pImpl_->position_;
}

size_t EncryptedFile::tell() const {
    return pImpl_ ? pImpl_->position_ : 0;
}

size_t EncryptedFile::size() const {
    return pImpl_ ? pImpl_->buffer_.size() : 0;
}

bool EncryptedFile::isOpen() const {
    return pImpl_ != nullptr;
}

void EncryptedFile::close() {
    if (!pImpl_) {
        return;
    }
    
    if (pImpl_->isWriting_ && !pImpl_->buffer_.empty()) {
        // Encrypt and write buffer to file
        auto& crypto = CryptoManager::getInstance();
        auto result = crypto.encryptFile(pImpl_->path_ + ".tmp", pImpl_->path_);
        if (result.isOk()) {
            std::filesystem::remove(pImpl_->path_ + ".tmp");
        }
    }
    
    pImpl_.reset();
}

} // namespace security
} // namespace common
} // namespace pacs