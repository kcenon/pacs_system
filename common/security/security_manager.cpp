#include "security_manager.h"
#include "common/config/config_manager.h"
#include "common/logger/logger.h"

#include <fstream>
#include <sstream>
#include <chrono>
#include <random>
#include <algorithm>
#include <regex>
#include <filesystem>
#include <iomanip>

// JSON handling
#include <nlohmann/json.hpp>

// Crypto++ for password hashing and token generation (if available)
#ifdef HAVE_CRYPTOPP
#include <cryptopp/sha.h>
#include <cryptopp/hex.h>
#include <cryptopp/pwdbased.h>
#include <cryptopp/osrng.h>
#include <cryptopp/base64.h>

using CryptoPP::SHA256;
using CryptoPP::AutoSeededRandomPool;
using CryptoPP::PKCS5_PBKDF2_HMAC;
using CryptoPP::byte;
#endif

using json = nlohmann::json;
using Result = pacs::core::Result<void>;

namespace pacs {
namespace common {
namespace security {

SecurityManager::SecurityManager() = default;

SecurityManager& SecurityManager::getInstance() {
    static SecurityManager instance;
    return instance;
}

pacs::core::Result<void> SecurityManager::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        if (initialized_) {
            return pacs::core::Result<void>::error("SecurityManager already initialized");
        }
        
        logger::logInfo("Initializing security manager");
        
        // Load TLS configuration
        tlsConfig_.loadFromConfig();
        
        // Load authentication type
        auto& configManager = config::ConfigManager::getInstance();
        auto authTypeStr = configManager.getValue("security.auth.type", "basic");
        
        if (authTypeStr == "none") {
            authType_ = AuthType::None;
            logger::logInfo("Authentication disabled");
        } else if (authTypeStr == "certificate") {
            authType_ = AuthType::Certificate;
            logger::logInfo("Using certificate-based authentication");
        } else if (authTypeStr == "token") {
            authType_ = AuthType::Token;
            logger::logInfo("Using token-based authentication");
        } else {
            authType_ = AuthType::Basic;
            logger::logInfo("Using basic authentication");
        }
        
        // Load users from file
        auto userFilePath = configManager.getValue("security.users.file", "");
        if (!userFilePath.empty()) {
            auto result = loadUsersFromFile(userFilePath);
            if (!result.isOk()) {
                // Check if we should create a default user
                auto createDefaultUser = configManager.getValue("security.create.default.user", "true");
                if (createDefaultUser == "true" || createDefaultUser == "1") {
                    logger::logInfo("Creating default admin user");
                    
                    UserCredentials admin;
                    admin.username = "admin";
                    admin.passwordHash = hashPassword("admin");
                    admin.role = UserRole::Admin;
                    admin.fullName = "Default Administrator";
                    admin.email = "admin@example.com";
                    admin.enabled = true;
                    
                    users_["admin"] = admin;
                    
                    // Save to file
                    if (!userFilePath.empty()) {
                        auto saveResult = saveUsersToFile(userFilePath);
                        if (!saveResult.isOk()) {
                            logger::logWarning("Failed to save default user to file");
                        }
                    }
                }
            }
        } else {
            logger::logWarning("No user file specified, using in-memory user database only");
            
            // Create default admin user
            UserCredentials admin;
            admin.username = "admin";
            admin.passwordHash = hashPassword("admin");
            admin.role = UserRole::Admin;
            admin.fullName = "Default Administrator";
            admin.email = "admin@example.com";
            admin.enabled = true;
            
            users_["admin"] = admin;
            
            logger::logWarning("Created default admin user with password 'admin'. Please change this!");
        }
        
        initialized_ = true;
        logger::logInfo("Security manager initialized successfully");
        
        return pacs::core::Result<void>::ok();
    }
    catch (const std::exception& ex) {
        return pacs::core::Result<void>::error("Failed to initialize security manager: " + std::string(ex.what()));
    }
}

const TLSConfig& SecurityManager::getTLSConfig() const {
    return tlsConfig_;
}

AuthResult SecurityManager::authenticateUser(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    AuthResult result;
    result.authenticated = false;
    
    // Check if authentication is disabled
    if (authType_ == AuthType::None) {
        result.authenticated = true;
        result.userId = username;
        result.role = UserRole::Admin; // Grant admin role when auth is disabled
        result.message = "Authentication disabled";
        
        logger::logWarning("Authenticated user {} with disabled authentication", username);
        return result;
    }
    
    // Check if user exists
    auto it = users_.find(username);
    if (it == users_.end()) {
        result.message = "User not found";
        logger::logWarning("Authentication failed: user {} not found", username);
        return result;
    }
    
    // Check if user is enabled
    if (!it->second.enabled) {
        result.message = "User is disabled";
        logger::logWarning("Authentication failed: user {} is disabled", username);
        return result;
    }
    
    // Verify password
    if (!verifyPassword(password, it->second.passwordHash)) {
        result.message = "Invalid password";
        logger::logWarning("Authentication failed: invalid password for user {}", username);
        return result;
    }
    
    // Authentication successful
    result.authenticated = true;
    result.userId = username;
    result.role = it->second.role;
    result.message = "Authentication successful";
    
    // Generate token if token authentication is enabled
    if (authType_ == AuthType::Token) {
        auto tokenOpt = generateToken(username);
        if (tokenOpt) {
            result.token = *tokenOpt;
        } else {
            logger::logError("Failed to generate token for user {}", username);
        }
    }
    
    logger::logInfo("User {} authenticated successfully", username);
    return result;
}

AuthResult SecurityManager::authenticateToken(const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    AuthResult result;
    result.authenticated = false;
    
    // Check if token authentication is enabled
    if (authType_ != AuthType::Token) {
        result.message = "Token authentication not enabled";
        logger::logWarning("Token authentication failed: not enabled");
        return result;
    }
    
    // Find token
    auto it = tokens_.find(token);
    if (it == tokens_.end()) {
        result.message = "Invalid token";
        logger::logWarning("Token authentication failed: invalid token");
        return result;
    }
    
    // Get username
    std::string username = it->second;
    
    // Check if user exists
    auto userIt = users_.find(username);
    if (userIt == users_.end()) {
        result.message = "User not found";
        logger::logWarning("Token authentication failed: user {} not found", username);
        return result;
    }
    
    // Check if user is enabled
    if (!userIt->second.enabled) {
        result.message = "User is disabled";
        logger::logWarning("Token authentication failed: user {} is disabled", username);
        return result;
    }
    
    // Authentication successful
    result.authenticated = true;
    result.userId = username;
    result.role = userIt->second.role;
    result.token = token;
    result.message = "Authentication successful";
    
    logger::logInfo("User {} authenticated successfully via token", username);
    return result;
}

pacs::core::Result<void> SecurityManager::addUser(const UserCredentials& credentials) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Validate username (alphanumeric, dash, underscore only)
        std::regex usernameRegex("^[a-zA-Z0-9_-]+$");
        if (!std::regex_match(credentials.username, usernameRegex)) {
            return pacs::core::Result<void>::error("Invalid username - only alphanumeric characters, dash (-) and underscore (_) are allowed");
        }
        
        // Add/update user
        users_[credentials.username] = credentials;
        
        logger::logInfo("User {} added/updated", credentials.username);
        
        // Update user file if configured
        auto& configManager = config::ConfigManager::getInstance();
        auto userFilePath = configManager.getValue("security.users.file", "");
        
        if (!userFilePath.empty()) {
            return saveUsersToFile(userFilePath);
        }
        
        return pacs::core::Result<void>::ok();
    }
    catch (const std::exception& ex) {
        return pacs::core::Result<void>::error("Failed to add/update user: " + std::string(ex.what()));
    }
}

pacs::core::Result<void> SecurityManager::removeUser(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Check if user exists
        auto it = users_.find(username);
        if (it == users_.end()) {
            return pacs::core::Result<void>::error("User not found");
        }
        
        // Remove user
        users_.erase(it);
        
        // Remove any tokens for this user
        for (auto it = tokens_.begin(); it != tokens_.end();) {
            if (it->second == username) {
                it = tokens_.erase(it);
            } else {
                ++it;
            }
        }
        
        logger::logInfo("User {} removed", username);
        
        // Update user file if configured
        auto& configManager = config::ConfigManager::getInstance();
        auto userFilePath = configManager.getValue("security.users.file", "");
        
        if (!userFilePath.empty()) {
            return saveUsersToFile(userFilePath);
        }
        
        return pacs::core::Result<void>::ok();
    }
    catch (const std::exception& ex) {
        return pacs::core::Result<void>::error("Failed to remove user: " + std::string(ex.what()));
    }
}

bool SecurityManager::userHasRole(const std::string& username, UserRole role) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // If authentication is disabled, grant all roles
    if (authType_ == AuthType::None) {
        return true;
    }
    
    // Check if user exists
    auto it = users_.find(username);
    if (it == users_.end()) {
        return false;
    }
    
    // Check if user is enabled
    if (!it->second.enabled) {
        return false;
    }
    
    // Check role
    const UserCredentials& user = it->second;
    
    // Admin role has all permissions
    if (user.role == UserRole::Admin) {
        return true;
    }
    
    // For other roles, check specific permissions
    switch (role) {
        case UserRole::Admin:
            return user.role == UserRole::Admin;
        
        case UserRole::Operator:
            return user.role == UserRole::Admin || user.role == UserRole::Operator;
        
        case UserRole::Viewer:
            return user.role == UserRole::Admin || user.role == UserRole::Operator || 
                   user.role == UserRole::Viewer;
        
        case UserRole::User:
            return true; // All authenticated users have User role
        
        default:
            return false;
    }
}

std::string SecurityManager::hashPassword(const std::string& password) {
    // Generate a random salt
    AutoSeededRandomPool rng;
    byte salt[16];
    rng.GenerateBlock(salt, sizeof(salt));
    
    // Hash the password with PBKDF2
    byte derived[SHA256::DIGESTSIZE];
    PKCS5_PBKDF2_HMAC<SHA256> pbkdf;
    pbkdf.DeriveKey(derived, sizeof(derived), 0, 
                   reinterpret_cast<const byte*>(password.data()), password.size(),
                   salt, sizeof(salt), 10000);
    
    // Encode salt and derived key to Base64
    std::string saltB64, hashB64;
    CryptoPP::Base64Encoder saltEncoder(new CryptoPP::StringSink(saltB64), false);
    saltEncoder.Put(salt, sizeof(salt));
    saltEncoder.MessageEnd();
    
    CryptoPP::Base64Encoder hashEncoder(new CryptoPP::StringSink(hashB64), false);
    hashEncoder.Put(derived, sizeof(derived));
    hashEncoder.MessageEnd();
    
    // Format: algorithm$iterations$salt$hash
    return "pbkdf2sha256$10000$" + saltB64 + "$" + hashB64;
}

bool SecurityManager::verifyPassword(const std::string& password, const std::string& hash) {
    try {
        // Parse the hash string
        std::vector<std::string> parts;
        std::stringstream ss(hash);
        std::string part;
        
        while (std::getline(ss, part, '$')) {
            parts.push_back(part);
        }
        
        if (parts.size() != 4) {
            logger::logError("Invalid hash format");
            return false;
        }
        
        // Check algorithm
        if (parts[0] != "pbkdf2sha256") {
            logger::logError("Unsupported hash algorithm: {}", parts[0]);
            return false;
        }
        
        // Get iterations
        int iterations = std::stoi(parts[1]);
        
        // Decode salt
        std::string saltB64 = parts[2];
        std::string decodedSalt;
        CryptoPP::Base64Decoder saltDecoder(new CryptoPP::StringSink(decodedSalt));
        saltDecoder.Put(reinterpret_cast<const byte*>(saltB64.data()), saltB64.size());
        saltDecoder.MessageEnd();
        
        // Hash the password
        byte derived[SHA256::DIGESTSIZE];
        PKCS5_PBKDF2_HMAC<SHA256> pbkdf;
        pbkdf.DeriveKey(derived, sizeof(derived), 0, 
                       reinterpret_cast<const byte*>(password.data()), password.size(),
                       reinterpret_cast<const byte*>(decodedSalt.data()), decodedSalt.size(), 
                       iterations);
        
        // Encode derived key to Base64
        std::string derivedB64;
        CryptoPP::Base64Encoder encoder(new CryptoPP::StringSink(derivedB64), false);
        encoder.Put(derived, sizeof(derived));
        encoder.MessageEnd();
        
        // Compare with stored hash
        return derivedB64 == parts[3];
    }
    catch (const std::exception& ex) {
        logger::logError("Error verifying password: {}", ex.what());
        return false;
    }
}

std::optional<std::string> SecurityManager::generateToken(const std::string& username) {
    try {
        // Generate a random token
        AutoSeededRandomPool rng;
        byte tokenBytes[32];
        rng.GenerateBlock(tokenBytes, sizeof(tokenBytes));
        
        // Convert to hex string
        std::string token;
        CryptoPP::HexEncoder encoder(new CryptoPP::StringSink(token), false);
        encoder.Put(tokenBytes, sizeof(tokenBytes));
        encoder.MessageEnd();
        
        // Store token
        tokens_[token] = username;
        
        return token;
    }
    catch (const std::exception& ex) {
        logger::logError("Error generating token: {}", ex.what());
        return std::nullopt;
    }
}

pacs::core::Result<void> SecurityManager::loadUsersFromFile(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Check if file exists
        if (!std::filesystem::exists(filePath)) {
            logger::logWarning("User file does not exist: {}", filePath);
            return pacs::core::Result<void>::error("User file does not exist");
        }
        
        // Open file
        std::ifstream file(filePath);
        if (!file.is_open()) {
            logger::logError("Failed to open user file: {}", filePath);
            return pacs::core::Result<void>::error("Failed to open user file");
        }
        
        // Parse JSON
        json usersJson;
        file >> usersJson;
        
        // Clear existing users
        users_.clear();
        
        // Load users
        for (const auto& userJson : usersJson) {
            UserCredentials user;
            user.username = userJson["username"].get<std::string>();
            user.passwordHash = userJson["password_hash"].get<std::string>();
            
            std::string roleStr = userJson["role"].get<std::string>();
            if (roleStr == "admin") {
                user.role = UserRole::Admin;
            } else if (roleStr == "operator") {
                user.role = UserRole::Operator;
            } else if (roleStr == "viewer") {
                user.role = UserRole::Viewer;
            } else {
                user.role = UserRole::User;
            }
            
            user.fullName = userJson.value("full_name", "");
            user.email = userJson.value("email", "");
            user.enabled = userJson.value("enabled", true);
            
            users_[user.username] = user;
        }
        
        logger::logInfo("Loaded {} users from file: {}", users_.size(), filePath);
        
        return pacs::core::Result<void>::ok();
    }
    catch (const std::exception& ex) {
        logger::logError("Failed to load users from file: {}", ex.what());
        return pacs::core::Result<void>::error("Failed to load users from file: " + std::string(ex.what()));
    }
}

pacs::core::Result<void> SecurityManager::saveUsersToFile(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Create directory if it doesn't exist
        std::filesystem::path path(filePath);
        std::filesystem::create_directories(path.parent_path());
        
        // Create JSON array
        json usersJson = json::array();
        
        // Add users
        for (const auto& [username, user] : users_) {
            json userJson;
            userJson["username"] = user.username;
            userJson["password_hash"] = user.passwordHash;
            
            switch (user.role) {
                case UserRole::Admin:
                    userJson["role"] = "admin";
                    break;
                case UserRole::Operator:
                    userJson["role"] = "operator";
                    break;
                case UserRole::Viewer:
                    userJson["role"] = "viewer";
                    break;
                default:
                    userJson["role"] = "user";
                    break;
            }
            
            userJson["full_name"] = user.fullName;
            userJson["email"] = user.email;
            userJson["enabled"] = user.enabled;
            
            usersJson.push_back(userJson);
        }
        
        // Write to file
        std::ofstream file(filePath);
        if (!file.is_open()) {
            logger::logError("Failed to open user file for writing: {}", filePath);
            return pacs::core::Result<void>::error("Failed to open user file for writing");
        }
        
        file << usersJson.dump(4); // Pretty print with 4-space indentation
        
        logger::logInfo("Saved {} users to file: {}", users_.size(), filePath);
        
        return pacs::core::Result<void>::ok();
    }
    catch (const std::exception& ex) {
        logger::logError("Failed to save users to file: {}", ex.what());
        return pacs::core::Result<void>::error("Failed to save users to file: " + std::string(ex.what()));
    }
}

} // namespace security
} // namespace common
} // namespace pacs