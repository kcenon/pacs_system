#include "config_manager.h"

#include <fstream>
#include <iostream>
#include <cstdlib>
#include <string>
#include <stdexcept>
#include <filesystem>

// JSON handling
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using Result = pacs::core::Result<void>;

namespace pacs {
namespace common {
namespace config {

ConfigManager::ConfigManager() = default;

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

Result ConfigManager::initialize(const std::optional<std::string>& configFilePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Set default values first
        serviceConfig_ = ServiceConfig{};
        
        // Load from environment variables
        auto envResult = loadFromEnvironment();
        if (!envResult.isOk()) {
            return envResult;
        }
        
        // Load from file if provided, either via parameter or already set in config
        if (configFilePath.has_value()) {
            serviceConfig_.configFilePath = configFilePath;
        }
        
        if (serviceConfig_.configFilePath.has_value()) {
            auto fileResult = loadFromFile(serviceConfig_.configFilePath.value());
            if (!fileResult.isOk()) {
                return fileResult;
            }
        }
        
        // Create necessary directories
        auto dirResult = createDirectories();
        if (!dirResult.isOk()) {
            return dirResult;
        }
        
        initialized_ = true;
        return Result::ok();
    }
    catch (const std::exception& ex) {
        return Result::error("Failed to initialize configuration: " + std::string(ex.what()));
    }
}

Result ConfigManager::loadFromFile(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        if (!std::filesystem::exists(filePath)) {
            return Result::error("Configuration file does not exist: " + filePath);
        }
        
        return parseJsonConfig(filePath);
    }
    catch (const std::exception& ex) {
        return Result::error("Failed to load configuration from file: " + std::string(ex.what()));
    }
}

Result ConfigManager::parseJsonConfig(const std::string& filePath) {
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            return Result::error("Failed to open configuration file: " + filePath);
        }
        
        json configJson;
        file >> configJson;
        
        // Parse service config
        if (configJson.contains("service")) {
            auto& service = configJson["service"];
            
            if (service.contains("aeTitle")) {
                serviceConfig_.aeTitle = service["aeTitle"].get<std::string>();
            }
            
            if (service.contains("localPort")) {
                serviceConfig_.localPort = service["localPort"].get<int>();
            }
            
            if (service.contains("maxAssociations")) {
                serviceConfig_.maxAssociations = service["maxAssociations"].get<int>();
            }
            
            if (service.contains("associationTimeout")) {
                serviceConfig_.associationTimeout = service["associationTimeout"].get<int>();
            }
            
            if (service.contains("dimseTimeout")) {
                serviceConfig_.dimseTimeout = service["dimseTimeout"].get<int>();
            }
            
            if (service.contains("acseTimeout")) {
                serviceConfig_.acseTimeout = service["acseTimeout"].get<int>();
            }
            
            if (service.contains("connectionTimeout")) {
                serviceConfig_.connectionTimeout = service["connectionTimeout"].get<int>();
            }
            
            if (service.contains("dataDirectory")) {
                serviceConfig_.dataDirectory = service["dataDirectory"].get<std::string>();
            }
            
            if (service.contains("logDirectory")) {
                serviceConfig_.logDirectory = service["logDirectory"].get<std::string>();
            }
            
            if (service.contains("databaseDirectory")) {
                serviceConfig_.databaseDirectory = service["databaseDirectory"].get<std::string>();
            }
            
            if (service.contains("useTLS")) {
                serviceConfig_.useTLS = service["useTLS"].get<bool>();
            }
            
            if (service.contains("tlsCertificatePath")) {
                serviceConfig_.tlsCertificatePath = service["tlsCertificatePath"].get<std::string>();
            }
            
            if (service.contains("tlsPrivateKeyPath")) {
                serviceConfig_.tlsPrivateKeyPath = service["tlsPrivateKeyPath"].get<std::string>();
            }
            
            if (service.contains("allowedRemoteAETitles") && service["allowedRemoteAETitles"].is_array()) {
                std::vector<std::string> aeTitles;
                for (const auto& item : service["allowedRemoteAETitles"]) {
                    aeTitles.push_back(item.get<std::string>());
                }
                serviceConfig_.allowedRemoteAETitles = aeTitles;
            }
            
            if (service.contains("serviceSpecificConfig") && service["serviceSpecificConfig"].is_object()) {
                for (auto it = service["serviceSpecificConfig"].begin(); 
                     it != service["serviceSpecificConfig"].end(); ++it) {
                    serviceConfig_.serviceSpecificConfig[it.key()] = it.value().get<std::string>();
                }
            }
        }
        
        // Parse general config values
        if (configJson.contains("config") && configJson["config"].is_object()) {
            for (auto it = configJson["config"].begin(); it != configJson["config"].end(); ++it) {
                configValues_[it.key()] = it.value().get<std::string>();
            }
        }
        
        return Result::ok();
    }
    catch (const json::exception& ex) {
        return Result::error("JSON parsing error: " + std::string(ex.what()));
    }
    catch (const std::exception& ex) {
        return Result::error("Failed to parse configuration file: " + std::string(ex.what()));
    }
}

Result ConfigManager::loadFromEnvironment(const std::string& prefix) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Function to get environment variable
        auto getEnv = [](const std::string& name) -> std::optional<std::string> {
            const char* value = std::getenv(name.c_str());
            if (value != nullptr) {
                return std::string(value);
            }
            return std::nullopt;
        };
        
        // Check for service config environment variables
        if (auto value = getEnv(prefix + "AE_TITLE")) {
            serviceConfig_.aeTitle = *value;
        }
        
        if (auto value = getEnv(prefix + "LOCAL_PORT")) {
            serviceConfig_.localPort = std::stoi(*value);
        }
        
        if (auto value = getEnv(prefix + "MAX_ASSOCIATIONS")) {
            serviceConfig_.maxAssociations = std::stoi(*value);
        }
        
        if (auto value = getEnv(prefix + "ASSOCIATION_TIMEOUT")) {
            serviceConfig_.associationTimeout = std::stoi(*value);
        }
        
        if (auto value = getEnv(prefix + "DIMSE_TIMEOUT")) {
            serviceConfig_.dimseTimeout = std::stoi(*value);
        }
        
        if (auto value = getEnv(prefix + "ACSE_TIMEOUT")) {
            serviceConfig_.acseTimeout = std::stoi(*value);
        }
        
        if (auto value = getEnv(prefix + "CONNECTION_TIMEOUT")) {
            serviceConfig_.connectionTimeout = std::stoi(*value);
        }
        
        if (auto value = getEnv(prefix + "DATA_DIRECTORY")) {
            serviceConfig_.dataDirectory = *value;
        }
        
        if (auto value = getEnv(prefix + "LOG_DIRECTORY")) {
            serviceConfig_.logDirectory = *value;
        }
        
        if (auto value = getEnv(prefix + "DATABASE_DIRECTORY")) {
            serviceConfig_.databaseDirectory = *value;
        }
        
        if (auto value = getEnv(prefix + "USE_TLS")) {
            serviceConfig_.useTLS = (*value == "1" || *value == "true" || *value == "TRUE");
        }
        
        if (auto value = getEnv(prefix + "TLS_CERTIFICATE_PATH")) {
            serviceConfig_.tlsCertificatePath = *value;
        }
        
        if (auto value = getEnv(prefix + "TLS_PRIVATE_KEY_PATH")) {
            serviceConfig_.tlsPrivateKeyPath = *value;
        }
        
        if (auto value = getEnv(prefix + "CONFIG_FILE_PATH")) {
            serviceConfig_.configFilePath = *value;
        }
        
        // Look for any environment variables with our prefix and add them to config values
        // This is just a placeholder - in a real implementation, we would need platform-specific
        // code to enumerate all environment variables
        
        return Result::ok();
    }
    catch (const std::exception& ex) {
        return Result::error("Failed to load configuration from environment: " + std::string(ex.what()));
    }
}

Result ConfigManager::createDirectories() {
    try {
        auto createDirIfSet = [](const std::optional<std::string>& dirPath) -> pacs::core::Result<void> {
            if (dirPath.has_value()) {
                try {
                    std::filesystem::create_directories(dirPath.value());
                    return Result::ok();
                }
                catch (const std::exception& ex) {
                    return Result::error("Failed to create directory: " + dirPath.value() + 
                                        " - " + std::string(ex.what()));
                }
            }
            return Result::ok();
        };
        
        // Create data directory if specified
        auto result = createDirIfSet(serviceConfig_.dataDirectory);
        if (!result.isOk()) {
            return result;
        }
        
        // Create log directory if specified
        result = createDirIfSet(serviceConfig_.logDirectory);
        if (!result.isOk()) {
            return result;
        }
        
        // Create database directory if specified
        result = createDirIfSet(serviceConfig_.databaseDirectory);
        if (!result.isOk()) {
            return result;
        }
        
        return Result::ok();
    }
    catch (const std::exception& ex) {
        return Result::error("Failed to create directories: " + std::string(ex.what()));
    }
}

const ServiceConfig& ConfigManager::getServiceConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return serviceConfig_;
}

void ConfigManager::setServiceConfig(const ServiceConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    serviceConfig_ = config;
}

std::string ConfigManager::getValue(const std::string& key, 
                                   const std::string& defaultValue) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = configValues_.find(key);
    if (it != configValues_.end()) {
        return it->second;
    }
    return defaultValue;
}

void ConfigManager::setValue(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    configValues_[key] = value;
}

bool ConfigManager::hasValue(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return configValues_.find(key) != configValues_.end();
}

Result ConfigManager::saveToFile(const std::string& filePath) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Create the JSON structure
        json configJson;
        
        // Add service configuration
        json serviceJson;
        serviceJson["aeTitle"] = serviceConfig_.aeTitle;
        serviceJson["localPort"] = serviceConfig_.localPort;
        serviceJson["maxAssociations"] = serviceConfig_.maxAssociations;
        serviceJson["associationTimeout"] = serviceConfig_.associationTimeout;
        serviceJson["dimseTimeout"] = serviceConfig_.dimseTimeout;
        serviceJson["acseTimeout"] = serviceConfig_.acseTimeout;
        serviceJson["connectionTimeout"] = serviceConfig_.connectionTimeout;
        
        if (serviceConfig_.dataDirectory.has_value()) {
            serviceJson["dataDirectory"] = serviceConfig_.dataDirectory.value();
        }
        
        if (serviceConfig_.logDirectory.has_value()) {
            serviceJson["logDirectory"] = serviceConfig_.logDirectory.value();
        }
        
        if (serviceConfig_.databaseDirectory.has_value()) {
            serviceJson["databaseDirectory"] = serviceConfig_.databaseDirectory.value();
        }
        
        if (serviceConfig_.useTLS.has_value()) {
            serviceJson["useTLS"] = serviceConfig_.useTLS.value();
        }
        
        if (serviceConfig_.tlsCertificatePath.has_value()) {
            serviceJson["tlsCertificatePath"] = serviceConfig_.tlsCertificatePath.value();
        }
        
        if (serviceConfig_.tlsPrivateKeyPath.has_value()) {
            serviceJson["tlsPrivateKeyPath"] = serviceConfig_.tlsPrivateKeyPath.value();
        }
        
        if (serviceConfig_.allowedRemoteAETitles.has_value()) {
            serviceJson["allowedRemoteAETitles"] = serviceConfig_.allowedRemoteAETitles.value();
        }
        
        if (!serviceConfig_.serviceSpecificConfig.empty()) {
            json specificConfig;
            for (const auto& [key, value] : serviceConfig_.serviceSpecificConfig) {
                specificConfig[key] = value;
            }
            serviceJson["serviceSpecificConfig"] = specificConfig;
        }
        
        configJson["service"] = serviceJson;
        
        // Add general configuration values
        if (!configValues_.empty()) {
            json generalConfig;
            for (const auto& [key, value] : configValues_) {
                generalConfig[key] = value;
            }
            configJson["config"] = generalConfig;
        }
        
        // Write to file
        std::ofstream file(filePath);
        if (!file.is_open()) {
            return Result::error("Failed to open file for writing: " + filePath);
        }
        
        file << configJson.dump(4); // Pretty print with 4-space indentation
        file.close();
        
        return Result::ok();
    }
    catch (const std::exception& ex) {
        return Result::error("Failed to save configuration to file: " + std::string(ex.what()));
    }
}

std::filesystem::path ConfigManager::getDataDirectory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (serviceConfig_.dataDirectory.has_value()) {
        return serviceConfig_.dataDirectory.value();
    }
    return "./data";  // Default data directory
}

std::filesystem::path ConfigManager::getLogDirectory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (serviceConfig_.logDirectory.has_value()) {
        return serviceConfig_.logDirectory.value();
    }
    return "./logs";  // Default log directory
}

std::filesystem::path ConfigManager::getDatabaseDirectory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (serviceConfig_.databaseDirectory.has_value()) {
        return serviceConfig_.databaseDirectory.value();
    }
    return "./data/db";  // Default database directory
}

} // namespace config
} // namespace common
} // namespace pacs