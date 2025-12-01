/**
 * @file config.hpp
 * @brief Configuration management for PACS Server sample
 *
 * Provides configuration structures and parsing utilities for the
 * complete PACS server sample application.
 *
 * @see Issue #106 - Full PACS Server Sample
 */

#ifndef PACS_EXAMPLE_PACS_SERVER_CONFIG_HPP
#define PACS_EXAMPLE_PACS_SERVER_CONFIG_HPP

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace pacs::example {

/**
 * @brief Server network configuration
 */
struct server_network_config {
    /// Application Entity Title for this server (max 16 chars)
    std::string ae_title{"MY_PACS"};

    /// Port to listen on
    uint16_t port{11112};

    /// Maximum concurrent associations (0 = unlimited)
    size_t max_associations{50};

    /// Idle timeout for associations in seconds (0 = no timeout)
    std::chrono::seconds idle_timeout{60};
};

/**
 * @brief Storage configuration
 */
struct storage_config {
    /// Root directory for DICOM file storage
    std::filesystem::path directory{"./archive"};

    /// File naming scheme: "hierarchical" or "flat"
    std::string naming{"hierarchical"};

    /// Duplicate handling policy: "reject", "replace", "ignore"
    std::string duplicate_policy{"reject"};
};

/**
 * @brief Database configuration
 */
struct database_config {
    /// Path to SQLite database file
    std::filesystem::path path{"./pacs.db"};

    /// Enable WAL (Write-Ahead Logging) mode for better concurrency
    bool wal_mode{true};
};

/**
 * @brief Logging configuration
 */
struct logging_config {
    /// Log level: "trace", "debug", "info", "warning", "error", "critical"
    std::string level{"info"};

    /// Log file path (empty for console only)
    std::filesystem::path file;

    /// Enable console output
    bool console{true};
};

/**
 * @brief Access control configuration
 */
struct access_control_config {
    /// Allowed AE titles (empty = accept all)
    std::vector<std::string> allowed_ae_titles;
};

/**
 * @brief Complete PACS server configuration
 */
struct pacs_server_config {
    /// Server network settings
    server_network_config server;

    /// Storage settings
    storage_config storage;

    /// Database settings
    database_config database;

    /// Logging settings
    logging_config logging;

    /// Access control settings
    access_control_config access_control;

    /**
     * @brief Parse configuration from command line arguments
     *
     * Supported options:
     *   --port <port>           Port to listen on (default: 11112)
     *   --ae-title <title>      AE title (default: MY_PACS)
     *   --storage-dir <path>    Storage directory (default: ./archive)
     *   --db-path <path>        Database path (default: ./pacs.db)
     *   --log-level <level>     Log level (default: info)
     *   --max-associations <n>  Max concurrent associations (default: 50)
     *   --help                  Show help message
     *
     * @param argc Argument count
     * @param argv Argument vector
     * @return Configuration or nullopt if --help was requested or error
     */
    static auto parse_args(int argc, char* argv[])
        -> std::optional<pacs_server_config>;

    /**
     * @brief Print help message to stdout
     */
    static void print_help();
};

}  // namespace pacs::example

#endif  // PACS_EXAMPLE_PACS_SERVER_CONFIG_HPP
