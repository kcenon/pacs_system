/**
 * @file remote_node.hpp
 * @brief Remote PACS node data structures for client operations
 *
 * This file provides data structures for representing external PACS nodes,
 * including connection parameters, supported services, and runtime status.
 *
 * @see Issue #535 - Implement Remote Node Manager
 * @see DICOM PS3.7 Section 9.1.5 - C-ECHO Service
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace pacs::client {

// =============================================================================
// Node Status
// =============================================================================

/**
 * @brief Status of a remote PACS node
 *
 * Represents the current connectivity and health status of a remote node.
 */
enum class node_status {
    unknown,     ///< Status not yet determined
    online,      ///< Node is responding to C-ECHO
    offline,     ///< Node is not responding
    error,       ///< Node returned an error
    verifying    ///< Verification in progress
};

/**
 * @brief Convert node_status to string representation
 * @param status The status to convert
 * @return String representation of the status
 */
[[nodiscard]] constexpr const char* to_string(node_status status) noexcept {
    switch (status) {
        case node_status::unknown: return "unknown";
        case node_status::online: return "online";
        case node_status::offline: return "offline";
        case node_status::error: return "error";
        case node_status::verifying: return "verifying";
        default: return "unknown";
    }
}

/**
 * @brief Parse node_status from string
 * @param str The string to parse
 * @return Parsed status, or unknown if invalid
 */
[[nodiscard]] inline node_status node_status_from_string(std::string_view str) noexcept {
    if (str == "online") return node_status::online;
    if (str == "offline") return node_status::offline;
    if (str == "error") return node_status::error;
    if (str == "verifying") return node_status::verifying;
    return node_status::unknown;
}

// =============================================================================
// TLS Configuration
// =============================================================================

/**
 * @brief TLS configuration for secure DICOM connections
 */
struct tls_config {
    std::string cert_path;     ///< Path to client certificate
    std::string key_path;      ///< Path to private key
    std::string ca_path;       ///< Path to CA certificate

    /**
     * @brief Check if TLS is configured
     * @return true if at least cert_path is set
     */
    [[nodiscard]] bool is_configured() const noexcept {
        return !cert_path.empty();
    }
};

// =============================================================================
// Remote Node
// =============================================================================

/**
 * @brief Remote PACS node configuration and status
 *
 * Represents a remote PACS server that can be connected to for DICOM
 * operations. Includes connection parameters, supported services, and
 * runtime status information.
 *
 * @example
 * @code
 * remote_node node;
 * node.node_id = "external-pacs-1";
 * node.name = "Hospital PACS";
 * node.ae_title = "HOSP_PACS";
 * node.host = "192.168.1.100";
 * node.port = 104;
 * @endcode
 */
struct remote_node {
    // =========================================================================
    // Identification
    // =========================================================================

    std::string node_id;           ///< Unique identifier for this node
    std::string name;              ///< Human-readable display name
    std::string ae_title;          ///< DICOM Application Entity Title
    std::string host;              ///< IP address or hostname
    uint16_t port{104};            ///< DICOM port (default: 104)

    // =========================================================================
    // Supported Services
    // =========================================================================

    bool supports_find{true};      ///< C-FIND support (Query)
    bool supports_move{true};      ///< C-MOVE support (Retrieve)
    bool supports_get{false};      ///< C-GET support (alternative retrieve)
    bool supports_store{true};     ///< C-STORE support (Send)
    bool supports_worklist{false}; ///< Modality Worklist support

    // =========================================================================
    // Connection Settings
    // =========================================================================

    std::chrono::seconds connection_timeout{30};  ///< TCP connection timeout
    std::chrono::seconds dimse_timeout{60};       ///< DIMSE operation timeout
    size_t max_associations{4};                   ///< Max concurrent associations

    // =========================================================================
    // TLS Settings (Optional)
    // =========================================================================

    std::optional<tls_config> tls;  ///< TLS configuration (if secure)

    // =========================================================================
    // Runtime Status
    // =========================================================================

    node_status status{node_status::unknown};     ///< Current connectivity status
    std::chrono::system_clock::time_point last_verified;   ///< Last successful verification
    std::chrono::system_clock::time_point last_error;      ///< Last error time
    std::string last_error_message;               ///< Last error description

    // =========================================================================
    // Database Fields
    // =========================================================================

    int64_t pk{0};                 ///< Primary key (0 if not persisted)
    std::chrono::system_clock::time_point created_at;  ///< Creation timestamp
    std::chrono::system_clock::time_point updated_at;  ///< Last update timestamp

    // =========================================================================
    // Convenience Methods
    // =========================================================================

    /**
     * @brief Check if node supports any query/retrieve operation
     */
    [[nodiscard]] bool supports_query_retrieve() const noexcept {
        return supports_find && (supports_move || supports_get);
    }

    /**
     * @brief Check if node is currently reachable
     */
    [[nodiscard]] bool is_online() const noexcept {
        return status == node_status::online;
    }

    /**
     * @brief Check if TLS is enabled for this node
     */
    [[nodiscard]] bool has_tls() const noexcept {
        return tls.has_value() && tls->is_configured();
    }

    /**
     * @brief Get connection address string (host:port)
     */
    [[nodiscard]] std::string address() const {
        return host + ":" + std::to_string(port);
    }
};

// =============================================================================
// Status Callback
// =============================================================================

/**
 * @brief Callback function type for node status changes
 *
 * Invoked when a node's status changes (e.g., online -> offline).
 *
 * @param node_id The ID of the node whose status changed
 * @param status The new status
 */
using node_status_callback = std::function<void(std::string_view node_id, node_status status)>;

// =============================================================================
// Node Statistics
// =============================================================================

/**
 * @brief Statistics for a remote node
 *
 * Tracks connection and operation metrics for monitoring and optimization.
 */
struct node_statistics {
    size_t total_connections{0};           ///< Total connections made
    size_t active_connections{0};          ///< Currently active connections
    size_t successful_operations{0};       ///< Successful DIMSE operations
    size_t failed_operations{0};           ///< Failed DIMSE operations
    std::chrono::milliseconds avg_response_time{0};  ///< Average response time
    std::chrono::milliseconds min_response_time{0};  ///< Minimum response time
    std::chrono::milliseconds max_response_time{0};  ///< Maximum response time
    std::chrono::system_clock::time_point last_activity;  ///< Last activity time
};

// =============================================================================
// Node Manager Configuration
// =============================================================================

/**
 * @brief Configuration for the remote node manager
 */
struct node_manager_config {
    /// Interval between automatic health checks
    std::chrono::seconds health_check_interval{60};

    /// Maximum pooled connections per node
    size_t max_pool_connections_per_node{4};

    /// Time-to-live for pooled connections
    std::chrono::seconds pool_connection_ttl{300};

    /// Start health check automatically on construction
    bool auto_start_health_check{true};

    /// Our AE Title for outgoing associations
    std::string local_ae_title{"PACS_CLIENT"};
};

}  // namespace pacs::client
