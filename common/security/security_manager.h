#pragma once

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <memory>
#include <functional>
#include <optional>

#include "tls_config.h"
#include "core/result/result.h"

namespace pacs {
namespace common {
namespace security {

/**
 * @brief Authentication type options
 */
enum class AuthType {
    None,           ///< No authentication
    Basic,          ///< Basic username/password authentication
    Certificate,    ///< TLS certificate-based authentication
    Token           ///< Token-based authentication (e.g., JWT)
};

/**
 * @brief User role options
 */
enum class UserRole {
    Admin,          ///< Administrator with full access
    Operator,       ///< Operator with operational access
    Viewer,         ///< Viewer with read-only access
    User            ///< Regular user with limited access
};

/**
 * @brief User credentials structure
 */
struct UserCredentials {
    std::string username;
    std::string passwordHash;
    UserRole role;
    std::string fullName;
    std::string email;
    bool enabled = true;
};

/**
 * @brief Authentication result structure
 */
struct AuthResult {
    bool authenticated = false;
    std::string userId;
    UserRole role = UserRole::User;
    std::string token;
    std::string message;
};

/**
 * @brief Singleton manager for security functions
 * 
 * This class manages security-related operations, including TLS configuration,
 * user authentication, and access control.
 */
class SecurityManager {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the SecurityManager singleton
     */
    static SecurityManager& getInstance();
    
    /**
     * @brief Initialize the security manager
     * @return Result object indicating success or failure
     */
    core::Result<void> initialize();
    
    /**
     * @brief Get the TLS configuration
     * @return TLS configuration object
     */
    const TLSConfig& getTLSConfig() const;
    
    /**
     * @brief Authenticate a user with username and password
     * @param username Username
     * @param password Password (plain text)
     * @return Authentication result
     */
    AuthResult authenticateUser(const std::string& username, const std::string& password);
    
    /**
     * @brief Authenticate a user with token
     * @param token Authentication token
     * @return Authentication result
     */
    AuthResult authenticateToken(const std::string& token);
    
    /**
     * @brief Add or update a user
     * @param credentials User credentials
     * @return Result object indicating success or failure
     */
    core::Result<void> addUser(const UserCredentials& credentials);
    
    /**
     * @brief Remove a user
     * @param username Username
     * @return Result object indicating success or failure
     */
    core::Result<void> removeUser(const std::string& username);
    
    /**
     * @brief Check if a user has a particular role
     * @param username Username
     * @param role Role to check
     * @return True if the user has the role, false otherwise
     */
    bool userHasRole(const std::string& username, UserRole role);
    
    /**
     * @brief Generate a password hash
     * @param password Plain text password
     * @return Hashed password
     */
    std::string hashPassword(const std::string& password);
    
    /**
     * @brief Verify a password against a hash
     * @param password Plain text password
     * @param hash Password hash
     * @return True if the password matches the hash, false otherwise
     */
    bool verifyPassword(const std::string& password, const std::string& hash);
    
    /**
     * @brief Generate an authentication token for a user
     * @param username Username
     * @return Authentication token or std::nullopt on failure
     */
    std::optional<std::string> generateToken(const std::string& username);
    
    /**
     * @brief Load users from a file
     * @param filePath Path to the user file
     * @return Result object indicating success or failure
     */
    core::Result<void> loadUsersFromFile(const std::string& filePath);
    
    /**
     * @brief Save users to a file
     * @param filePath Path to the user file
     * @return Result object indicating success or failure
     */
    core::Result<void> saveUsersToFile(const std::string& filePath);
    
    /**
     * @brief Generate a secure random password
     * @param length Password length (default: 16)
     * @return Generated password
     */
    std::string generateSecurePassword(size_t length = 16);

private:
    // Private constructor for singleton pattern
    SecurityManager();
    
    // Delete copy and move constructors and assignment operators
    SecurityManager(const SecurityManager&) = delete;
    SecurityManager& operator=(const SecurityManager&) = delete;
    SecurityManager(SecurityManager&&) = delete;
    SecurityManager& operator=(SecurityManager&&) = delete;
    
    TLSConfig tlsConfig_;
    std::map<std::string, UserCredentials> users_;
    std::map<std::string, std::string> tokens_; // token -> username
    AuthType authType_ = AuthType::Basic;
    mutable std::mutex mutex_;
    bool initialized_ = false;
};

} // namespace security
} // namespace common
} // namespace pacs