/**
 * @file config_loader.hpp
 * @brief YAML configuration loader for Production PACS
 *
 * This file provides configuration loading from YAML files for the Production
 * PACS sample. Uses a simple YAML parser suitable for basic configuration files.
 *
 * @see DICOM PS3.2 - Conformance
 */

#pragma once

#include <pacs/security/anonymization_profile.hpp>

#include <kcenon/common/patterns/result.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace pacs::samples {

// =============================================================================
// TLS Configuration
// =============================================================================

/**
 * @brief TLS security configuration for DICOM connections
 */
struct tls_config {
    /// Enable TLS for DICOM connections
    bool enabled{false};

    /// Path to server certificate file (PEM format)
    std::filesystem::path certificate;

    /// Path to private key file (PEM format)
    std::filesystem::path private_key;

    /// Path to CA certificate file (PEM format)
    std::filesystem::path ca_certificate;

    /// Require client certificate (mutual TLS)
    bool require_client_cert{false};

    /// Minimum TLS version ("1.2" or "1.3")
    std::string min_version{"1.2"};
};

// =============================================================================
// Security Configuration
// =============================================================================

/**
 * @brief Access control configuration
 */
struct access_control_config {
    /// Enable access control enforcement
    bool enabled{true};

    /// Default role for unknown AE titles
    std::string default_role{"viewer"};
};

/**
 * @brief Anonymization configuration
 */
struct anonymization_config {
    /// Automatically anonymize incoming images
    bool auto_anonymize{false};

    /// Anonymization profile to use
    security::anonymization_profile profile{security::anonymization_profile::basic};
};

/**
 * @brief Complete security configuration
 */
struct security_config {
    /// Access control settings
    access_control_config access_control;

    /// Allowed AE titles (supports wildcards with *)
    std::vector<std::string> allowed_ae_titles;

    /// Anonymization settings
    anonymization_config anonymization;
};

// =============================================================================
// REST API Configuration
// =============================================================================

/**
 * @brief REST API server configuration
 */
struct rest_api_config {
    /// Enable REST API server
    bool enabled{true};

    /// HTTP port for REST API
    std::uint16_t port{8080};

    /// Enable CORS for web clients
    bool cors_enabled{true};
};

// =============================================================================
// Monitoring Configuration
// =============================================================================

/**
 * @brief Health monitoring configuration
 */
struct monitoring_config {
    /// Interval between health checks (seconds)
    std::chrono::seconds health_check_interval{30};

    /// Enable Prometheus-style metrics endpoint
    bool metrics_enabled{true};
};

// =============================================================================
// Database Configuration
// =============================================================================

/**
 * @brief Database configuration for index storage
 */
struct database_config {
    /// Path to SQLite database file
    std::filesystem::path path{"./pacs_data/index.db"};
};

// =============================================================================
// Storage Configuration
// =============================================================================

/**
 * @brief File storage configuration
 */
struct storage_config {
    /// Root path for DICOM file storage
    std::filesystem::path root_path{"./pacs_data"};

    /// Naming scheme: "uid_flat", "uid_hierarchical", "date_based"
    std::string naming_scheme{"uid_hierarchical"};

    /// Duplicate handling: "reject", "replace", "rename"
    std::string duplicate_policy{"replace"};

    /// Database configuration
    database_config database;
};

// =============================================================================
// Server Configuration
// =============================================================================

/**
 * @brief DICOM server configuration
 */
struct server_config {
    /// Application Entity title (max 16 characters)
    std::string ae_title{"PROD_PACS"};

    /// TCP port for DICOM connections
    std::uint16_t port{11112};

    /// Maximum concurrent associations
    std::size_t max_associations{100};

    /// Idle timeout for associations (seconds)
    std::chrono::seconds idle_timeout{300};

    /// TLS configuration
    tls_config tls;
};

// =============================================================================
// Logging Configuration
// =============================================================================

/**
 * @brief Logging configuration
 */
struct logging_config {
    /// Log level: "debug", "info", "warn", "error"
    std::string level{"info"};

    /// Path to audit log file
    std::filesystem::path audit_log_path;
};

// =============================================================================
// Production Configuration
// =============================================================================

/**
 * @brief Complete production PACS configuration
 *
 * Aggregates all configuration sections for a production PACS deployment.
 */
struct production_config {
    /// DICOM server settings
    server_config server;

    /// Storage settings
    storage_config storage;

    /// Security settings
    security_config security;

    /// REST API settings
    rest_api_config rest_api;

    /// Monitoring settings
    monitoring_config monitoring;

    /// Logging settings
    logging_config logging;
};

// =============================================================================
// Config Loader
// =============================================================================

/**
 * @brief YAML configuration file loader
 *
 * Provides static methods for loading configuration from YAML files.
 * Supports a subset of YAML syntax suitable for configuration files.
 *
 * @example
 * @code
 * auto result = config_loader::load("config/pacs_config.yaml");
 * if (result.is_ok()) {
 *     auto config = result.value();
 *     std::cout << "AE Title: " << config.server.ae_title << "\n";
 * } else {
 *     std::cerr << "Error: " << result.error() << "\n";
 * }
 * @endcode
 */
class config_loader {
public:
    /**
     * @brief Load configuration from a YAML file
     * @param path Path to the YAML configuration file
     * @return Result containing configuration or error message
     */
    [[nodiscard]] static auto load(const std::filesystem::path& path)
        -> kcenon::common::Result<production_config>;

    /**
     * @brief Load configuration from a YAML string
     * @param yaml_content YAML content as a string
     * @return Result containing configuration or error message
     */
    [[nodiscard]] static auto load_from_string(std::string_view yaml_content)
        -> kcenon::common::Result<production_config>;

    /**
     * @brief Create a default configuration
     * @return Default production configuration
     */
    [[nodiscard]] static auto create_default() -> production_config;

    /**
     * @brief Validate a configuration
     * @param config Configuration to validate
     * @return Result indicating success or error
     */
    [[nodiscard]] static auto validate(const production_config& config)
        -> kcenon::common::VoidResult;

private:
    /**
     * @brief Parse a YAML value (simple types)
     */
    static auto parse_value(std::string_view value) -> std::string;

    /**
     * @brief Parse a boolean value
     */
    static auto parse_bool(std::string_view value) -> bool;

    /**
     * @brief Parse an integer value
     */
    static auto parse_int(std::string_view value) -> int;

    /**
     * @brief Parse anonymization profile
     */
    static auto parse_anonymization_profile(std::string_view value)
        -> security::anonymization_profile;

    /**
     * @brief Trim whitespace from string
     */
    static auto trim(std::string_view str) -> std::string;
};

}  // namespace pacs::samples
