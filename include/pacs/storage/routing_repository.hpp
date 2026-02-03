/**
 * @file routing_repository.hpp
 * @brief Repository for routing rule persistence using base_repository pattern
 *
 * This file provides the routing_repository class for persisting routing rules
 * using the base_repository pattern. Supports CRUD operations, rule ordering,
 * and statistics tracking.
 *
 * @see Issue #539 - Implement Routing Manager for Auto-Forwarding
 * @see Issue #530 - PACS Client System Support (Parent Epic)
 * @see Issue #610 - Phase 4: Repository Migrations
 * @see Issue #650 - Part 2: Migrate annotation, routing, node repositories
 */

#pragma once

#include "pacs/client/routing_types.hpp"

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
 * @brief Query options for listing routing rules
 */
struct routing_rule_query_options {
    std::optional<bool> enabled_only;         ///< Filter by enabled status
    bool order_by_priority{true};             ///< Order by priority (desc)
    size_t limit{100};                        ///< Maximum results
    size_t offset{0};                         ///< Result offset for pagination
};

/**
 * @brief Repository for routing rule persistence using base_repository pattern
 *
 * Extends base_repository to inherit standard CRUD operations.
 * Provides database operations for storing and retrieving routing rules.
 *
 * Thread Safety:
 * - This class is NOT thread-safe. External synchronization is required
 *   for concurrent access.
 *
 * @example
 * @code
 * auto db = std::make_shared<pacs_database_adapter>("pacs.db");
 * db->connect();
 * auto repo = routing_repository(db);
 *
 * client::routing_rule rule;
 * rule.rule_id = generate_uuid();
 * rule.name = "CT to Remote PACS";
 * rule.conditions.push_back({client::routing_field::modality, "CT"});
 * rule.actions.push_back({"remote-pacs-1"});
 * repo.save(rule);
 *
 * auto found = repo.find_by_id(rule.rule_id);
 * @endcode
 */
class routing_repository
    : public base_repository<client::routing_rule, std::string> {
public:
    explicit routing_repository(std::shared_ptr<pacs_database_adapter> db);
    ~routing_repository() override = default;

    routing_repository(const routing_repository&) = delete;
    auto operator=(const routing_repository&) -> routing_repository& = delete;
    routing_repository(routing_repository&&) noexcept = default;
    auto operator=(routing_repository&&) noexcept
        -> routing_repository& = default;

    // =========================================================================
    // Domain-Specific Operations
    // =========================================================================

    /**
     * @brief Find a rule by primary key (integer pk)
     *
     * @param pk The primary key (integer)
     * @return Result containing the rule if found, or error
     */
    [[nodiscard]] auto find_by_pk(int64_t pk) -> result_type;

    /**
     * @brief List rules with query options
     *
     * @param options Query options for filtering and pagination
     * @return Result containing vector of matching rules or error
     */
    [[nodiscard]] auto find_rules(
        const routing_rule_query_options& options = {}) -> list_result_type;

    /**
     * @brief Find all enabled rules ordered by priority
     *
     * @return Result containing vector of enabled rules
     */
    [[nodiscard]] auto find_enabled_rules() -> list_result_type;

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
    [[nodiscard]] auto increment_triggered(std::string_view rule_id)
        -> VoidResult;

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
     * @brief Get enabled rule count
     *
     * @return Result containing number of enabled rules
     */
    [[nodiscard]] auto count_enabled() -> Result<size_t>;

protected:
    // =========================================================================
    // base_repository overrides
    // =========================================================================

    [[nodiscard]] auto map_row_to_entity(const database_row& row) const
        -> client::routing_rule override;

    [[nodiscard]] auto entity_to_row(const client::routing_rule& entity) const
        -> std::map<std::string, database_value> override;

    [[nodiscard]] auto get_pk(const client::routing_rule& entity) const
        -> std::string override;

    [[nodiscard]] auto has_pk(const client::routing_rule& entity) const
        -> bool override;

    [[nodiscard]] auto select_columns() const
        -> std::vector<std::string> override;

private:
    [[nodiscard]] auto parse_timestamp(const std::string& str) const
        -> std::chrono::system_clock::time_point;

    [[nodiscard]] auto format_timestamp(
        std::chrono::system_clock::time_point tp) const -> std::string;

    [[nodiscard]] auto format_optional_timestamp(
        const std::optional<std::chrono::system_clock::time_point>& tp) const
        -> std::string;

    [[nodiscard]] static auto serialize_conditions(
        const std::vector<client::routing_condition>& conditions) -> std::string;

    [[nodiscard]] static auto deserialize_conditions(std::string_view json)
        -> std::vector<client::routing_condition>;

    [[nodiscard]] static auto serialize_actions(
        const std::vector<client::routing_action>& actions) -> std::string;

    [[nodiscard]] static auto deserialize_actions(std::string_view json)
        -> std::vector<client::routing_action>;
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
 * @brief Query options for listing routing rules
 */
struct routing_rule_query_options {
    std::optional<bool> enabled_only;         ///< Filter by enabled status
    bool order_by_priority{true};             ///< Order by priority (desc)
    size_t limit{100};                        ///< Maximum results
    size_t offset{0};                         ///< Result offset for pagination
};

/**
 * @brief Repository for routing rule persistence (legacy SQLite interface)
 *
 * This is the legacy interface maintained for builds without database_system.
 * New code should use the base_repository version when PACS_WITH_DATABASE_SYSTEM
 * is defined.
 */
class routing_repository {
public:
    explicit routing_repository(sqlite3* db);
    ~routing_repository();

    routing_repository(const routing_repository&) = delete;
    auto operator=(const routing_repository&) -> routing_repository& = delete;
    routing_repository(routing_repository&&) noexcept;
    auto operator=(routing_repository&&) noexcept -> routing_repository&;

    [[nodiscard]] auto save(const client::routing_rule& rule) -> VoidResult;
    [[nodiscard]] auto find_by_id(std::string_view rule_id) const
        -> std::optional<client::routing_rule>;
    [[nodiscard]] auto find_by_pk(int64_t pk) const
        -> std::optional<client::routing_rule>;
    [[nodiscard]] auto find_rules(
        const routing_rule_query_options& options = {}) const
        -> std::vector<client::routing_rule>;
    [[nodiscard]] auto find_enabled_rules() const
        -> std::vector<client::routing_rule>;
    [[nodiscard]] auto remove(std::string_view rule_id) -> VoidResult;
    [[nodiscard]] auto exists(std::string_view rule_id) const -> bool;

    [[nodiscard]] auto update_priority(std::string_view rule_id, int priority)
        -> VoidResult;
    [[nodiscard]] auto enable_rule(std::string_view rule_id) -> VoidResult;
    [[nodiscard]] auto disable_rule(std::string_view rule_id) -> VoidResult;

    [[nodiscard]] auto increment_triggered(std::string_view rule_id)
        -> VoidResult;
    [[nodiscard]] auto increment_success(std::string_view rule_id) -> VoidResult;
    [[nodiscard]] auto increment_failure(std::string_view rule_id) -> VoidResult;
    [[nodiscard]] auto reset_statistics(std::string_view rule_id) -> VoidResult;

    [[nodiscard]] auto count() const -> size_t;
    [[nodiscard]] auto count_enabled() const -> size_t;
    [[nodiscard]] auto is_valid() const noexcept -> bool;

private:
    [[nodiscard]] auto parse_row(void* stmt) const -> client::routing_rule;

    [[nodiscard]] static auto serialize_conditions(
        const std::vector<client::routing_condition>& conditions) -> std::string;

    [[nodiscard]] static auto deserialize_conditions(std::string_view json)
        -> std::vector<client::routing_condition>;

    [[nodiscard]] static auto serialize_actions(
        const std::vector<client::routing_action>& actions) -> std::string;

    [[nodiscard]] static auto deserialize_actions(std::string_view json)
        -> std::vector<client::routing_action>;

    sqlite3* db_{nullptr};
};

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
