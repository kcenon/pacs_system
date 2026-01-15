/**
 * @file node_repository.hpp
 * @brief Repository for remote PACS node persistence
 *
 * This file provides the node_repository class for persisting remote node
 * configurations in the SQLite database. Supports CRUD operations and
 * status updates.
 *
 * @see Issue #535 - Implement Remote Node Manager
 */

#pragma once

#include "pacs/client/remote_node.hpp"

#include <kcenon/common/patterns/result.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Forward declaration
struct sqlite3;

namespace pacs::storage {

/// Result type alias
template <typename T>
using Result = kcenon::common::Result<T>;

/// Void result type alias
using VoidResult = kcenon::common::VoidResult;

/**
 * @brief Repository for remote node persistence
 *
 * Provides database operations for storing and retrieving remote PACS node
 * configurations. Uses SQLite for persistence.
 *
 * Thread Safety:
 * - This class is NOT thread-safe. External synchronization is required
 *   for concurrent access.
 *
 * @example
 * @code
 * auto repo = std::make_shared<node_repository>(db_handle);
 *
 * // Add a node
 * client::remote_node node;
 * node.node_id = "external-pacs";
 * node.ae_title = "EXT_PACS";
 * node.host = "192.168.1.100";
 * repo->upsert(node);
 *
 * // Find a node
 * auto found = repo->find_by_id("external-pacs");
 * if (found) {
 *     std::cout << "Found: " << found->name << std::endl;
 * }
 * @endcode
 */
class node_repository {
public:
    // =========================================================================
    // Construction / Destruction
    // =========================================================================

    /**
     * @brief Construct repository with SQLite handle
     *
     * @param db SQLite database handle (must remain valid for repository lifetime)
     */
    explicit node_repository(sqlite3* db);

    /**
     * @brief Destructor
     */
    ~node_repository();

    // Non-copyable, movable
    node_repository(const node_repository&) = delete;
    auto operator=(const node_repository&) -> node_repository& = delete;
    node_repository(node_repository&&) noexcept;
    auto operator=(node_repository&&) noexcept -> node_repository&;

    // =========================================================================
    // CRUD Operations
    // =========================================================================

    /**
     * @brief Insert or update a remote node
     *
     * If a node with the same node_id exists, updates it.
     * Otherwise, inserts a new record.
     *
     * @param node The node to upsert
     * @return Result containing the primary key or error
     */
    [[nodiscard]] auto upsert(const client::remote_node& node) -> Result<int64_t>;

    /**
     * @brief Find a node by its unique ID
     *
     * @param node_id The node ID to search for
     * @return Optional containing the node if found
     */
    [[nodiscard]] auto find_by_id(std::string_view node_id) const
        -> std::optional<client::remote_node>;

    /**
     * @brief Find a node by primary key
     *
     * @param pk The primary key
     * @return Optional containing the node if found
     */
    [[nodiscard]] auto find_by_pk(int64_t pk) const
        -> std::optional<client::remote_node>;

    /**
     * @brief Get all nodes
     *
     * @return Vector of all stored nodes
     */
    [[nodiscard]] auto find_all() const -> std::vector<client::remote_node>;

    /**
     * @brief Get nodes by status
     *
     * @param status The status to filter by
     * @return Vector of nodes with the specified status
     */
    [[nodiscard]] auto find_by_status(client::node_status status) const
        -> std::vector<client::remote_node>;

    /**
     * @brief Delete a node by ID
     *
     * @param node_id The node ID to delete
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto remove(std::string_view node_id) -> VoidResult;

    /**
     * @brief Check if a node exists
     *
     * @param node_id The node ID to check
     * @return true if the node exists
     */
    [[nodiscard]] auto exists(std::string_view node_id) const -> bool;

    /**
     * @brief Get the count of all nodes
     *
     * @return Number of nodes in the repository
     */
    [[nodiscard]] auto count() const -> size_t;

    // =========================================================================
    // Status Updates
    // =========================================================================

    /**
     * @brief Update node status
     *
     * @param node_id The node ID to update
     * @param status New status
     * @param error_message Error message (optional, for error status)
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto update_status(
        std::string_view node_id,
        client::node_status status,
        std::string_view error_message = "") -> VoidResult;

    /**
     * @brief Update last verified timestamp
     *
     * @param node_id The node ID to update
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto update_last_verified(std::string_view node_id)
        -> VoidResult;

    // =========================================================================
    // Database Information
    // =========================================================================

    /**
     * @brief Check if the database connection is valid
     *
     * @return true if the database handle is valid
     */
    [[nodiscard]] auto is_valid() const noexcept -> bool;

private:
    // =========================================================================
    // Private Implementation
    // =========================================================================

    /**
     * @brief Parse a node from a prepared statement
     */
    [[nodiscard]] auto parse_row(void* stmt) const -> client::remote_node;

    /// SQLite database handle
    sqlite3* db_{nullptr};
};

}  // namespace pacs::storage
