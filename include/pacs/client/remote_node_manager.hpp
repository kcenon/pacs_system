/**
 * @file remote_node_manager.hpp
 * @brief Remote PACS node manager for client operations
 *
 * This file provides the remote_node_manager class for managing connections
 * to multiple external PACS servers. Includes node registration, health
 * monitoring, connection pooling, and status callbacks.
 *
 * @see Issue #535 - Implement Remote Node Manager
 * @see DICOM PS3.7 Section 9.1.5 - C-ECHO Service
 * @see DICOM PS3.8 - Network Communication Support
 */

#pragma once

#include "pacs/client/remote_node.hpp"
#include "pacs/core/result.hpp"
#include "pacs/di/ilogger.hpp"

#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Forward declarations
namespace pacs::storage {
class node_repository;
}

namespace pacs::network {
class association;
}

namespace pacs::client {

// =============================================================================
// Remote Node Manager
// =============================================================================

/**
 * @brief Manager for remote PACS node connections
 *
 * Provides centralized management of remote PACS nodes including:
 * - Node CRUD operations (add, update, remove, list)
 * - Connection verification via C-ECHO
 * - Automatic health checking
 * - Connection pooling for efficiency
 * - Status change callbacks
 *
 * Thread Safety:
 * - All public methods are thread-safe
 * - Status callbacks are invoked from worker threads
 *
 * @example
 * @code
 * // Create manager with repository
 * auto repo = std::make_shared<node_repository>(db);
 * auto manager = std::make_unique<remote_node_manager>(repo);
 *
 * // Add a node
 * remote_node node;
 * node.node_id = "external-pacs";
 * node.ae_title = "EXT_PACS";
 * node.host = "192.168.1.100";
 * node.port = 104;
 * manager->add_node(node);
 *
 * // Verify connectivity
 * auto result = manager->verify_node("external-pacs");
 * if (result.is_ok()) {
 *     std::cout << "Node is online!" << std::endl;
 * }
 *
 * // Set status callback
 * manager->set_status_callback([](std::string_view id, node_status status) {
 *     std::cout << "Node " << id << " is now " << to_string(status) << std::endl;
 * });
 *
 * // Start automatic health checking
 * manager->start_health_check();
 * @endcode
 */
class remote_node_manager {
public:
    // =========================================================================
    // Construction / Destruction
    // =========================================================================

    /**
     * @brief Construct a remote node manager
     *
     * @param repo Repository for node persistence (required)
     * @param config Configuration options
     * @param logger Logger instance (optional, defaults to NullLogger)
     */
    explicit remote_node_manager(
        std::shared_ptr<storage::node_repository> repo,
        node_manager_config config = {},
        std::shared_ptr<di::ILogger> logger = nullptr);

    /**
     * @brief Destructor - stops health check if running
     */
    ~remote_node_manager();

    // Non-copyable, non-movable (due to internal threading)
    remote_node_manager(const remote_node_manager&) = delete;
    auto operator=(const remote_node_manager&) -> remote_node_manager& = delete;
    remote_node_manager(remote_node_manager&&) = delete;
    auto operator=(remote_node_manager&&) -> remote_node_manager& = delete;

    // =========================================================================
    // Node CRUD Operations
    // =========================================================================

    /**
     * @brief Add a new remote node
     *
     * @param node The node to add
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto add_node(const remote_node& node) -> pacs::VoidResult;

    /**
     * @brief Update an existing remote node
     *
     * @param node The node to update (node_id must match existing)
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto update_node(const remote_node& node) -> pacs::VoidResult;

    /**
     * @brief Remove a remote node
     *
     * @param node_id ID of the node to remove
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto remove_node(std::string_view node_id) -> pacs::VoidResult;

    /**
     * @brief Get a node by ID
     *
     * @param node_id ID of the node to retrieve
     * @return Optional containing the node if found
     */
    [[nodiscard]] auto get_node(std::string_view node_id) const
        -> std::optional<remote_node>;

    /**
     * @brief List all registered nodes
     *
     * @return Vector of all nodes
     */
    [[nodiscard]] auto list_nodes() const -> std::vector<remote_node>;

    /**
     * @brief List nodes filtered by status
     *
     * @param status The status to filter by
     * @return Vector of nodes with the specified status
     */
    [[nodiscard]] auto list_nodes_by_status(node_status status) const
        -> std::vector<remote_node>;

    // =========================================================================
    // Connection Verification (C-ECHO)
    // =========================================================================

    /**
     * @brief Verify a node's connectivity synchronously
     *
     * Sends a C-ECHO to the specified node and waits for response.
     * Updates the node's status based on the result.
     *
     * @param node_id ID of the node to verify
     * @return VoidResult indicating success (online) or error (offline/error)
     */
    [[nodiscard]] auto verify_node(std::string_view node_id) -> pacs::VoidResult;

    /**
     * @brief Verify a node's connectivity asynchronously
     *
     * Returns immediately with a future that will contain the result.
     *
     * @param node_id ID of the node to verify
     * @return Future containing the verification result
     */
    [[nodiscard]] auto verify_node_async(std::string_view node_id)
        -> std::future<pacs::VoidResult>;

    /**
     * @brief Verify all nodes asynchronously
     *
     * Initiates verification of all registered nodes.
     * Results are reported via status callbacks.
     */
    void verify_all_nodes_async();

    // =========================================================================
    // Association Pool Management
    // =========================================================================

    /**
     * @brief Acquire an association from the pool
     *
     * Gets a pooled association or creates a new one if none available.
     * The association is configured for the specified SOP classes.
     *
     * @param node_id ID of the node to connect to
     * @param sop_classes SOP Class UIDs needed for the operation
     * @return Result containing the association or error
     */
    [[nodiscard]] auto acquire_association(
        std::string_view node_id,
        std::span<const std::string> sop_classes)
        -> pacs::Result<std::unique_ptr<network::association>>;

    /**
     * @brief Release an association back to the pool
     *
     * Returns an association to the pool for reuse.
     * If the pool is full or the association is in a bad state,
     * it will be closed instead.
     *
     * @param node_id ID of the node the association is for
     * @param assoc The association to release
     */
    void release_association(
        std::string_view node_id,
        std::unique_ptr<network::association> assoc);

    // =========================================================================
    // Health Check Scheduler
    // =========================================================================

    /**
     * @brief Start the automatic health check scheduler
     *
     * Begins periodic verification of all nodes at the configured interval.
     * Does nothing if already running.
     */
    void start_health_check();

    /**
     * @brief Stop the automatic health check scheduler
     *
     * Stops periodic verification. Does nothing if not running.
     */
    void stop_health_check();

    /**
     * @brief Check if health check is running
     *
     * @return true if the health check scheduler is active
     */
    [[nodiscard]] auto is_health_check_running() const noexcept -> bool;

    // =========================================================================
    // Status Monitoring
    // =========================================================================

    /**
     * @brief Get the current status of a node
     *
     * @param node_id ID of the node
     * @return Current status (unknown if node not found)
     */
    [[nodiscard]] auto get_status(std::string_view node_id) const -> node_status;

    /**
     * @brief Set the status change callback
     *
     * The callback is invoked whenever a node's status changes.
     * Only one callback can be set; setting a new one replaces the old.
     *
     * @param callback Function to call on status change
     */
    void set_status_callback(node_status_callback callback);

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get statistics for a node
     *
     * @param node_id ID of the node
     * @return Statistics for the node
     */
    [[nodiscard]] auto get_statistics(std::string_view node_id) const
        -> node_statistics;

    /**
     * @brief Reset statistics for a node
     *
     * @param node_id ID of the node (empty to reset all)
     */
    void reset_statistics(std::string_view node_id = "");

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Get the current configuration
     *
     * @return Current manager configuration
     */
    [[nodiscard]] auto config() const noexcept -> const node_manager_config&;

    /**
     * @brief Update the configuration
     *
     * Note: Some changes may not take effect until health check is restarted.
     *
     * @param new_config New configuration to apply
     */
    void set_config(node_manager_config new_config);

private:
    // =========================================================================
    // Private Implementation
    // =========================================================================

    struct impl;
    std::unique_ptr<impl> impl_;
};

}  // namespace pacs::client
