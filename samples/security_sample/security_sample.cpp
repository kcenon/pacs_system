#include <iostream>
#include <filesystem>
#include <string>
#include <thread>
#include <chrono>

#include "common/config/config_manager.h"
#include "common/logger/logger.h"
#include "common/logger/logging_service.h"
#include "common/security/security_manager.h"
#include "common/security/tls_config.h"

using namespace pacs::common;
using namespace pacs::common::security;

// Helper function to print role as string
std::string roleToString(UserRole role) {
    switch (role) {
        case UserRole::Admin:
            return "Admin";
        case UserRole::Operator:
            return "Operator";
        case UserRole::Viewer:
            return "Viewer";
        case UserRole::User:
            return "User";
        default:
            return "Unknown";
    }
}

// Demonstrate user management
void demonstrateUserManagement(SecurityManager& securityManager) {
    std::cout << "\n--- User Management ---\n";
    
    // Add a few users
    std::cout << "Adding users..." << std::endl;
    
    UserCredentials operator1;
    operator1.username = "operator1";
    operator1.passwordHash = securityManager.hashPassword("password1");
    operator1.role = UserRole::Operator;
    operator1.fullName = "First Operator";
    operator1.email = "operator1@example.com";
    operator1.enabled = true;
    
    auto result = securityManager.addUser(operator1);
    std::cout << "Add operator1: " << (result.isSuccess() ? "Success" : "Failed") << std::endl;
    
    UserCredentials viewer1;
    viewer1.username = "viewer1";
    viewer1.passwordHash = securityManager.hashPassword("password2");
    viewer1.role = UserRole::Viewer;
    viewer1.fullName = "First Viewer";
    viewer1.email = "viewer1@example.com";
    viewer1.enabled = true;
    
    result = securityManager.addUser(viewer1);
    std::cout << "Add viewer1: " << (result.isSuccess() ? "Success" : "Failed") << std::endl;
    
    // Test role checks
    std::cout << "\nChecking roles..." << std::endl;
    std::cout << "admin has Admin role: " << (securityManager.userHasRole("admin", UserRole::Admin) ? "Yes" : "No") << std::endl;
    std::cout << "admin has Operator role: " << (securityManager.userHasRole("admin", UserRole::Operator) ? "Yes" : "No") << std::endl;
    std::cout << "operator1 has Admin role: " << (securityManager.userHasRole("operator1", UserRole::Admin) ? "Yes" : "No") << std::endl;
    std::cout << "operator1 has Operator role: " << (securityManager.userHasRole("operator1", UserRole::Operator) ? "Yes" : "No") << std::endl;
    std::cout << "viewer1 has Operator role: " << (securityManager.userHasRole("viewer1", UserRole::Operator) ? "Yes" : "No") << std::endl;
    std::cout << "viewer1 has Viewer role: " << (securityManager.userHasRole("viewer1", UserRole::Viewer) ? "Yes" : "No") << std::endl;
}

// Demonstrate authentication
void demonstrateAuthentication(SecurityManager& securityManager) {
    std::cout << "\n--- Authentication ---\n";
    
    // Test successful authentication
    std::cout << "Authenticating admin with correct password..." << std::endl;
    auto authResult = securityManager.authenticateUser("admin", "admin");
    std::cout << "Result: " << (authResult.authenticated ? "Success" : "Failure") << std::endl;
    if (authResult.authenticated) {
        std::cout << "  User ID: " << authResult.userId << std::endl;
        std::cout << "  Role: " << roleToString(authResult.role) << std::endl;
        if (!authResult.token.empty()) {
            std::cout << "  Token: " << authResult.token << std::endl;
        }
    } else {
        std::cout << "  Message: " << authResult.message << std::endl;
    }
    
    // Test failed authentication
    std::cout << "\nAuthenticating admin with incorrect password..." << std::endl;
    authResult = securityManager.authenticateUser("admin", "wrongpassword");
    std::cout << "Result: " << (authResult.authenticated ? "Success" : "Failure") << std::endl;
    if (!authResult.authenticated) {
        std::cout << "  Message: " << authResult.message << std::endl;
    }
    
    // Test non-existent user
    std::cout << "\nAuthenticating non-existent user..." << std::endl;
    authResult = securityManager.authenticateUser("nonexistent", "password");
    std::cout << "Result: " << (authResult.authenticated ? "Success" : "Failure") << std::endl;
    if (!authResult.authenticated) {
        std::cout << "  Message: " << authResult.message << std::endl;
    }
}

// Demonstrate token-based authentication
void demonstrateTokenAuthentication(SecurityManager& securityManager) {
    std::cout << "\n--- Token Authentication ---\n";
    
    // First, authenticate with username/password
    auto authResult = securityManager.authenticateUser("admin", "admin");
    if (!authResult.authenticated || authResult.token.empty()) {
        std::cout << "Token-based authentication not enabled or initial authentication failed" << std::endl;
        return;
    }
    
    std::string token = authResult.token;
    std::cout << "Obtained token: " << token << std::endl;
    
    // Now authenticate with the token
    std::cout << "\nAuthenticating with token..." << std::endl;
    auto tokenResult = securityManager.authenticateToken(token);
    std::cout << "Result: " << (tokenResult.authenticated ? "Success" : "Failure") << std::endl;
    if (tokenResult.authenticated) {
        std::cout << "  User ID: " << tokenResult.userId << std::endl;
        std::cout << "  Role: " << roleToString(tokenResult.role) << std::endl;
    } else {
        std::cout << "  Message: " << tokenResult.message << std::endl;
    }
    
    // Try with invalid token
    std::cout << "\nAuthenticating with invalid token..." << std::endl;
    tokenResult = securityManager.authenticateToken("invalid-token");
    std::cout << "Result: " << (tokenResult.authenticated ? "Success" : "Failure") << std::endl;
    if (!tokenResult.authenticated) {
        std::cout << "  Message: " << tokenResult.message << std::endl;
    }
}

// Demonstrate TLS configuration
void demonstrateTLSConfig(SecurityManager& securityManager) {
    std::cout << "\n--- TLS Configuration ---\n";
    
    const auto& tlsConfig = securityManager.getTLSConfig();
    
    std::cout << "TLS enabled: " << (tlsConfig.isEnabled() ? "Yes" : "No") << std::endl;
    
    if (tlsConfig.isEnabled()) {
        std::cout << "Certificate path: " << tlsConfig.getCertificatePath() << std::endl;
        std::cout << "Private key path: " << tlsConfig.getPrivateKeyPath() << std::endl;
        
        if (tlsConfig.getCACertificatePath()) {
            std::cout << "CA certificate path: " << *tlsConfig.getCACertificatePath() << std::endl;
        }
        
        if (tlsConfig.getCACertificateDir()) {
            std::cout << "CA certificate directory: " << *tlsConfig.getCACertificateDir() << std::endl;
        }
        
        std::cout << "Verification mode: " << static_cast<int>(tlsConfig.getVerificationMode()) << std::endl;
        std::cout << "Minimum protocol version: " << static_cast<int>(tlsConfig.getMinimumProtocolVersion()) << std::endl;
        std::cout << "Client authentication: " << (tlsConfig.useClientAuthentication() ? "Enabled" : "Disabled") << std::endl;
        
        const auto& trustedCerts = tlsConfig.getTrustedCertificates();
        std::cout << "Trusted certificates: " << trustedCerts.size() << std::endl;
        for (const auto& cert : trustedCerts) {
            std::cout << "  - " << cert << std::endl;
        }
    }
}

int main() {
    try {
        std::cout << "PACS Security Sample" << std::endl;
        std::cout << "====================" << std::endl << std::endl;
        
        // Create directories
        std::filesystem::create_directories("./data");
        std::filesystem::create_directories("./logs");
        std::filesystem::create_directories("./data/security");
        std::filesystem::create_directories("./data/certs");
        
        // Initialize logging service
        auto& loggingService = logger::LoggingService::getInstance();
        auto logResult = loggingService.initialize("SECURITY_SAMPLE");
        
        if (logResult.has_value()) {
            std::cerr << "Failed to initialize logging service: " << *logResult << std::endl;
            return 1;
        }
        
        // Set up configuration
        auto& configManager = config::ConfigManager::getInstance();
        
        // Configure security settings
        configManager.setValue("security.auth.type", "token");
        configManager.setValue("security.users.file", "./data/security/users.json");
        configManager.setValue("security.create.default.user", "true");
        
        // Configure TLS settings (using self-signed certificates for demonstration)
        configManager.setValue("tls.certificate", "./data/certs/server.crt");
        configManager.setValue("tls.private.key", "./data/certs/server.key");
        configManager.setValue("tls.ca.certificate", "./data/certs/ca.crt");
        configManager.setValue("tls.verification.mode", "relaxed");
        configManager.setValue("tls.min.protocol", "tlsv1.2");
        configManager.setValue("tls.client.authentication", "false");
        
        // Create sample security files if they don't exist
        if (!std::filesystem::exists("./data/security/users.json")) {
            logger::logInfo("Creating sample user file");
            
            // We'll let the SecurityManager create the default admin user
        }
        
        // Initialize security manager
        logger::logInfo("Initializing security manager");
        auto& securityManager = SecurityManager::getInstance();
        auto result = securityManager.initialize();
        
        if (!result.isSuccess()) {
            logger::logError("Failed to initialize security manager: {}", result.getMessage());
            std::cerr << "Failed to initialize security manager: " << result.getMessage() << std::endl;
            return 1;
        }
        
        logger::logInfo("Security manager initialized successfully");
        
        // Demonstrate security features
        demonstrateUserManagement(securityManager);
        demonstrateAuthentication(securityManager);
        demonstrateTokenAuthentication(securityManager);
        demonstrateTLSConfig(securityManager);
        
        // Clean up
        logger::logInfo("Security sample completed");
        loggingService.shutdown();
        
        std::cout << "\nSecurity sample completed successfully." << std::endl;
        
        return 0;
    }
    catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
}