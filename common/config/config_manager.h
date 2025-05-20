#pragma once

#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <filesystem>
#include <optional>

#include "common/service_config.h"
#include "core/result/result.h"

namespace pacs {
namespace common {
namespace config {

/**
 * @brief Configuration manager singleton
 * 
 * This class provides centralized configuration management for the PACS system.
 * It supports loading configuration from files, environment variables, and
 * programmatic settings, with a hierarchical override system.
 */
class ConfigManager {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the ConfigManager singleton
     */
    static ConfigManager& getInstance();
    
    /**
     * @brief Initialize the configuration manager
     * @param configFilePath Optional path to a configuration file
     * @return Result object indicating success or failure
     */
    core::Result<void> initialize(const std::optional<std::string>& configFilePath = std::nullopt);
    
    /**
     * @brief Load configuration from a JSON file
     * @param filePath Path to the JSON configuration file
     * @return Result object indicating success or failure
     */
    core::Result<void> loadFromFile(const std::string& filePath);
    
    /**
     * @brief Load configuration from environment variables
     * @param prefix Optional prefix for environment variables (default: "PACS_")
     * @return Result object indicating success or failure
     */
    core::Result<void> loadFromEnvironment(const std::string& prefix = "PACS_");
    
    /**
     * @brief Get service configuration
     * @return Reference to the current service configuration
     */
    const ServiceConfig& getServiceConfig() const;
    
    /**
     * @brief Set service configuration
     * @param config Service configuration to set
     */
    void setServiceConfig(const ServiceConfig& config);
    
    /**
     * @brief Get a configuration value by key
     * @param key Configuration key
     * @param defaultValue Optional default value if key doesn't exist
     * @return Value as string or default value if not found
     */
    std::string getValue(const std::string& key, 
                         const std::string& defaultValue = "") const;
    
    /**
     * @brief Set a configuration value
     * @param key Configuration key
     * @param value Configuration value
     */
    void setValue(const std::string& key, const std::string& value);
    
    /**
     * @brief Check if a configuration key exists
     * @param key Configuration key to check
     * @return True if the key exists, false otherwise
     */
    bool hasValue(const std::string& key) const;
    
    /**
     * @brief Save current configuration to a file
     * @param filePath Path to save the configuration file
     * @return Result object indicating success or failure
     */
    core::Result<void> saveToFile(const std::string& filePath) const;
    
    /**
     * @brief Get data directory path
     * @return Data directory path
     */
    std::filesystem::path getDataDirectory() const;
    
    /**
     * @brief Get log directory path
     * @return Log directory path
     */
    std::filesystem::path getLogDirectory() const;
    
    /**
     * @brief Get database directory path
     * @return Database directory path
     */
    std::filesystem::path getDatabaseDirectory() const;
    
private:
    // Private constructor for singleton pattern
    ConfigManager();
    
    // Delete copy and move constructors and assignment operators
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    ConfigManager(ConfigManager&&) = delete;
    ConfigManager& operator=(ConfigManager&&) = delete;
    
    /**
     * @brief Create directories specified in configuration
     * @return Result object indicating success or failure
     */
    core::Result<void> createDirectories();
    
    /**
     * @brief Parse a JSON configuration file
     * @param filePath Path to the JSON file
     * @return Result object indicating success or failure
     */
    core::Result<void> parseJsonConfig(const std::string& filePath);
    
    ServiceConfig serviceConfig_;
    std::map<std::string, std::string> configValues_;
    mutable std::mutex mutex_;
    bool initialized_ = false;
};

} // namespace config
} // namespace common
} // namespace pacs