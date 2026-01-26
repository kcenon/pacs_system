/**
 * @file routing_repository.hpp
 * @brief Repository for routing rule persistence
 *
 * This file provides the routing_repository class for persisting routing rules
 * in the SQLite database. Supports CRUD operations, rule ordering, and
 * statistics tracking.
 *
 * @see Issue #539 - Implement Routing Manager for Auto-Forwarding
 * @see Issue #530 - PACS Client System Support (Parent Epic)
 */

#pragma once

#ifdef PACS_WITH_DATABASE_SYSTEM


#include "pacs/client/routing_types.hpp"

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
 * @brief Query options for listing routing rules
 */
struct routing_rule_query_options {
    std::optional<bool> enabled_only;         ///< Filter by enabled status
    bool order_by_priority{true};             ///< Order by priority (desc)
    size_t limit{100};                        ///< Maximum results
    size_t offset{0};                         ///< Result offset for pagination
};

/**
 * @brief Repository for routing rule persistence
 *
 * Provides database operations for storing and retrieving routing rules.
 * Uses SQLite for persistence.
 *
 * Thread Safety:
 * - This class is NOT thread-safe. External synchronization is required
 *   for concurrent access.
 *
 * @example
 * @code
 * auto repo = std::make_shared<routing_repository>(db_handle);
 *
 * // Save a rule
 * client::routing_rule rule;
 * rule.rule_id = generate_uuid();
 * rule.name = "CT to Remote PACS";
 * rule.conditions.push_back({client::routing_field::modality, "CT"});
 * rule.actions.push_back({"remote-pacs-1"});
 * repo->save(rule);
 *
 * // Find a rule
 * auto found = repo->find_by_id(rule.rule_id);
 * if (found) {
 *     std::cout << "Name: " << found->name << std::endl;
 * }
 *
 * // Update statistics
 * repo->increment_triggered(rule.rule_id);
 * repo->increment_success(rule.rule_id);
 * @endcode
 */
class routing_repository {
public:
    // =========================================================================
    // Construction / Destruction
    // =========================================================================

    /**
     * @brief Construct repository with SQLite handle
     *
     * @param db SQLite database handle (must remain valid for repository lifetime)
     */
    explicit routing_repository(sqlite3* db);

    /**
     * @brief Destructor
     */
    ~routing_repository();

    // Non-copyable, movable
    routing_repository(const routing_repository&) = delete;
    auto operator=(const routing_repository&) -> routing_repository& = delete;
    routing_repository(routing_repository&&) noexcept;
    auto operator=(routing_repository&&) noexcept -> routing_repository&;

    // =========================================================================
    // CRUD Operations
    // =========================================================================

    /**
     * @brief Save a routing rule
     *
     * If the rule already exists (by rule_id), updates it.
     * Otherwise, inserts a new record.
     *
     * @param rule The routing rule to save
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto save(const client::routing_rule& rule) -> VoidResult;

    /**
     * @brief Find a rule by its unique ID
     *
     * @param rule_id The rule ID to search for
     * @return Optional containing the rule if found
     */
    [[nodiscard]] auto find_by_id(std::string_view rule_id) const
        -> std::optional<client::routing_rule>;

    /**
     * @brief Find a rule by primary key
     *
     * @param pk The primary key
     * @return Optional containing the rule if found
     */
    [[nodiscard]] auto find_by_pk(int64_t pk) const
        -> std::optional<client::routing_rule>;

    /**
     * @brief List rules with query options
     *
     * @param options Query options for filtering and pagination
     * @return Vector of matching rules
     */
    [[nodiscard]] auto find_rules(const routing_rule_query_options& options = {}) const
        -> std::vector<client::routing_rule>;

    /**
     * @brief Find all enabled rules ordered by priority
     *
     * Returns rules in enabled status, ordered by priority (desc).
     *
     * @return Vector of enabled rules
     */
    [[nodiscard]] auto find_enabled_rules() const
        -> std::vector<client::routing_rule>;

    /**
     * @brief Delete a rule by ID
     *
     * @param rule_id The rule ID to delete
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto remove(std::string_view rule_id) -> VoidResult;

    /**
     * @brief Check if a rule exists
     *
     * @param rule_id The rule ID to check
     * @return true if the rule exists
     */
    [[nodiscard]] auto exists(std::string_view rule_id) const -> bool;

    // =========================================================================
    // Rule Ordering
    // =========================================================================

    /**
     * @brief Update rule priority
     *
     * @param rule_id The rule ID to update
     * @param priority New priority value
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto update_priority(std::string_view rule_id, int priority)
        -> VoidResult;

    /**
     * @brief Enable a rule
     *
     * @param rule_id The rule ID to enable
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto enable_rule(std::string_view rule_id) -> VoidResult;

    /**
     * @brief Disable a rule
     *
     * @param rule_id The rule ID to disable
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto disable_rule(std::string_view rule_id) -> VoidResult;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Increment triggered count for a rule
     *
     * Also updates last_triggered timestamp.
     *
     * @param rule_id The rule ID to update
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto increment_triggered(std::string_view rule_id) -> VoidResult;

    /**
     * @brief Increment success count for a rule
     *
     * @param rule_id The rule ID to update
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto increment_success(std::string_view rule_id) -> VoidResult;

    /**
     * @brief Increment failure count for a rule
     *
     * @param rule_id The rule ID to update
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto increment_failure(std::string_view rule_id) -> VoidResult;

    /**
     * @brief Reset statistics for a rule
     *
     * @param rule_id The rule ID to reset
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto reset_statistics(std::string_view rule_id) -> VoidResult;

    /**
     * @brief Get total rule count
     *
     * @return Number of rules in the repository
     */
    [[nodiscard]] auto count() const -> size_t;

    /**
     * @brief Get enabled rule count
     *
     * @return Number of enabled rules
     */
    [[nodiscard]] auto count_enabled() const -> size_t;

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
     * @brief Parse a routing rule from a prepared statement
     */
    [[nodiscard]] auto parse_row(void* stmt) const -> client::routing_rule;

    /**
     * @brief Serialize conditions vector to JSON
     */
    [[nodiscard]] static auto serialize_conditions(
        const std::vector<client::routing_condition>& conditions) -> std::string;

    /**
     * @brief Deserialize conditions from JSON
     */
    [[nodiscard]] static auto deserialize_conditions(
        std::string_view json) -> std::vector<client::routing_condition>;

    /**
     * @brief Serialize actions vector to JSON
     */
    [[nodiscard]] static auto serialize_actions(
        const std::vector<client::routing_action>& actions) -> std::string;

    /**
     * @brief Deserialize actions from JSON
     */
    [[nodiscard]] static auto deserialize_actions(
        std::string_view json) -> std::vector<client::routing_action>;

    /// SQLite database handle
    sqlite3* db_{nullptr};
};

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
