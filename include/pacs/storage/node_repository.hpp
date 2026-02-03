/**
 * @file node_repository.hpp
 * @brief Repository for remote PACS node persistence using base_repository pattern
 *
 * This file provides the node_repository class for persisting remote node
 * configurations. Supports CRUD operations and status updates.
 *
 * @see Issue #535 - Implement Remote Node Manager
 * @see Issue #610 - Phase 4: Repository Migrations
 * @see Issue #650 - Part 2: Migrate annotation, routing, node repositories
 */

#pragma once

#include "pacs/client/remote_node.hpp"

#include <kcenon/common/patterns/result.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef PACS_WITH_DATABASE_SYSTEM

#include "pacs/storage/base_repository.hpp"

namespace pacs::storage {

/**
 * @brief Repository for remote node persistence using base_repository pattern
 *
 * Extends base_repository to inherit standard CRUD operations.
 * Provides database operations for storing and retrieving remote PACS node
 * configurations.
 *
 * Thread Safety:
 * - This class is NOT thread-safe. External synchronization is required
 *   for concurrent access.
 *
 * @example
 * @code
 * auto db = std::make_shared<pacs_database_adapter>("pacs.db");
 * db->connect();
 * auto repo = node_repository(db);
 *
 * client::remote_node node;
 * node.node_id = "external-pacs";
 * node.ae_title = "EXT_PACS";
 * node.host = "192.168.1.100";
 * repo.save(node);
 *
 * auto found = repo.find_by_id(node.node_id);
 * @endcode
 */
class node_repository
    : public base_repository<client::remote_node, std::string> {
public:
    explicit node_repository(std::shared_ptr<pacs_database_adapter> db);
    ~node_repository() override = default;

    node_repository(const node_repository&) = delete;
    auto operator=(const node_repository&) -> node_repository& = delete;
    node_repository(node_repository&&) noexcept = default;
    auto operator=(node_repository&&) noexcept -> node_repository& = default;

    // =========================================================================
    // Domain-Specific Operations
    // =========================================================================

    /**
     * @brief Find a node by primary key (integer pk)
     *
     * @param pk The primary key (integer)
     * @return Result containing the node if found, or error
     */
    [[nodiscard]] auto find_by_pk(int64_t pk) -> result_type;

    /**
     * @brief Get all nodes
     *
     * @return Result containing vector of all stored nodes
     */
    [[nodiscard]] auto find_all_nodes() -> list_result_type;

    /**
     * @brief Get nodes by status
     *
     * @param status The status to filter by
     * @return Result containing vector of nodes with the specified status
     */
    [[nodiscard]] auto find_by_status(client::node_status status)
        -> list_result_type;

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

protected:
    // =========================================================================
    // base_repository overrides
    // =========================================================================

    [[nodiscard]] auto map_row_to_entity(const database_row& row) const
        -> client::remote_node override;

    [[nodiscard]] auto entity_to_row(const client::remote_node& entity) const
        -> std::map<std::string, database_value> override;

    [[nodiscard]] auto get_pk(const client::remote_node& entity) const
        -> std::string override;

    [[nodiscard]] auto has_pk(const client::remote_node& entity) const
        -> bool override;

    [[nodiscard]] auto select_columns() const
        -> std::vector<std::string> override;

private:
    [[nodiscard]] auto parse_timestamp(const std::string& str) const
        -> std::chrono::system_clock::time_point;

    [[nodiscard]] auto format_timestamp(
        std::chrono::system_clock::time_point tp) const -> std::string;
};

}  // namespace pacs::storage

#else  // !PACS_WITH_DATABASE_SYSTEM

// Legacy interface for builds without database_system
struct sqlite3;

namespace pacs::storage {

template <typename T>
using Result = kcenon::common::Result<T>;

using VoidResult = kcenon::common::VoidResult;

/**
 * @brief Repository for remote node persistence (legacy SQLite interface)
 *
 * This is the legacy interface maintained for builds without database_system.
 * New code should use the base_repository version when PACS_WITH_DATABASE_SYSTEM
 * is defined.
 */
class node_repository {
public:
    explicit node_repository(sqlite3* db);
    ~node_repository();

    node_repository(const node_repository&) = delete;
    auto operator=(const node_repository&) -> node_repository& = delete;
    node_repository(node_repository&&) noexcept;
    auto operator=(node_repository&&) noexcept -> node_repository&;

    [[nodiscard]] auto upsert(const client::remote_node& node) -> Result<int64_t>;
    [[nodiscard]] auto find_by_id(std::string_view node_id) const
        -> std::optional<client::remote_node>;
    [[nodiscard]] auto find_by_pk(int64_t pk) const
        -> std::optional<client::remote_node>;
    [[nodiscard]] auto find_all() const -> std::vector<client::remote_node>;
    [[nodiscard]] auto find_by_status(client::node_status status) const
        -> std::vector<client::remote_node>;
    [[nodiscard]] auto remove(std::string_view node_id) -> VoidResult;
    [[nodiscard]] auto exists(std::string_view node_id) const -> bool;
    [[nodiscard]] auto count() const -> size_t;

    [[nodiscard]] auto update_status(
        std::string_view node_id,
        client::node_status status,
        std::string_view error_message = "") -> VoidResult;
    [[nodiscard]] auto update_last_verified(std::string_view node_id)
        -> VoidResult;

    [[nodiscard]] auto is_valid() const noexcept -> bool;

private:
    [[nodiscard]] auto parse_row(void* stmt) const -> client::remote_node;

    sqlite3* db_{nullptr};
};

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
