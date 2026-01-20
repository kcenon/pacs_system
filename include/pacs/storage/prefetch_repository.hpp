/**
 * @file prefetch_repository.hpp
 * @brief Repository for prefetch rule and history persistence
 *
 * This file provides the prefetch_repository class for persisting prefetch rules
 * and history records in the SQLite database. Supports CRUD operations and
 * statistics tracking.
 *
 * @see Issue #541 - Implement Prefetch Manager for Proactive Data Loading
 * @see Issue #530 - PACS Client System Support (Parent Epic)
 */

#pragma once

#include "pacs/client/prefetch_types.hpp"

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
 * @brief Query options for listing prefetch rules
 */
struct prefetch_rule_query_options {
    std::optional<bool> enabled_only;              ///< Filter by enabled status
    std::optional<client::prefetch_trigger> trigger;  ///< Filter by trigger type
    size_t limit{100};                             ///< Maximum results
    size_t offset{0};                              ///< Result offset for pagination
};

/**
 * @brief Query options for listing prefetch history
 */
struct prefetch_history_query_options {
    std::optional<std::string> patient_id;         ///< Filter by patient
    std::optional<std::string> rule_id;            ///< Filter by rule
    std::optional<std::string> status;             ///< Filter by status
    size_t limit{100};                             ///< Maximum results
    size_t offset{0};                              ///< Result offset for pagination
};

/**
 * @brief Repository for prefetch persistence
 *
 * Provides database operations for storing and retrieving prefetch rules
 * and history records. Uses SQLite for persistence.
 *
 * Thread Safety:
 * - This class is NOT thread-safe. External synchronization is required
 *   for concurrent access.
 *
 * @example
 * @code
 * auto repo = std::make_shared<prefetch_repository>(db_handle);
 *
 * // Save a rule
 * client::prefetch_rule rule;
 * rule.rule_id = generate_uuid();
 * rule.name = "CT Prior Studies";
 * rule.trigger = client::prefetch_trigger::prior_studies;
 * rule.source_node_ids = {"archive-pacs"};
 * repo->save_rule(rule);
 *
 * // Find a rule
 * auto found = repo->find_rule_by_id(rule.rule_id);
 * if (found) {
 *     std::cout << "Name: " << found->name << std::endl;
 * }
 *
 * // Record history
 * client::prefetch_history history;
 * history.patient_id = "PATIENT123";
 * history.study_uid = "1.2.3.4.5";
 * history.source_node_id = "archive-pacs";
 * history.status = "completed";
 * repo->save_history(history);
 * @endcode
 */
class prefetch_repository {
public:
    // =========================================================================
    // Construction / Destruction
    // =========================================================================

    /**
     * @brief Construct repository with SQLite handle
     *
     * @param db SQLite database handle (must remain valid for repository lifetime)
     */
    explicit prefetch_repository(sqlite3* db);

    /**
     * @brief Destructor
     */
    ~prefetch_repository();

    // Non-copyable, movable
    prefetch_repository(const prefetch_repository&) = delete;
    auto operator=(const prefetch_repository&) -> prefetch_repository& = delete;
    prefetch_repository(prefetch_repository&&) noexcept;
    auto operator=(prefetch_repository&&) noexcept -> prefetch_repository&;

    // =========================================================================
    // Rule CRUD Operations
    // =========================================================================

    /**
     * @brief Save a prefetch rule
     *
     * If the rule already exists (by rule_id), updates it.
     * Otherwise, inserts a new record.
     *
     * @param rule The rule to save
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto save_rule(const client::prefetch_rule& rule) -> VoidResult;

    /**
     * @brief Find a rule by its unique ID
     *
     * @param rule_id The rule ID to search for
     * @return Optional containing the rule if found
     */
    [[nodiscard]] auto find_rule_by_id(std::string_view rule_id) const
        -> std::optional<client::prefetch_rule>;

    /**
     * @brief Find a rule by primary key
     *
     * @param pk The primary key
     * @return Optional containing the rule if found
     */
    [[nodiscard]] auto find_rule_by_pk(int64_t pk) const
        -> std::optional<client::prefetch_rule>;

    /**
     * @brief List rules with query options
     *
     * @param options Query options for filtering and pagination
     * @return Vector of matching rules
     */
    [[nodiscard]] auto find_rules(const prefetch_rule_query_options& options = {}) const
        -> std::vector<client::prefetch_rule>;

    /**
     * @brief Find all enabled rules
     *
     * @return Vector of enabled rules
     */
    [[nodiscard]] auto find_enabled_rules() const
        -> std::vector<client::prefetch_rule>;

    /**
     * @brief Delete a rule by ID
     *
     * @param rule_id The rule ID to delete
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto remove_rule(std::string_view rule_id) -> VoidResult;

    /**
     * @brief Check if a rule exists
     *
     * @param rule_id The rule ID to check
     * @return true if the rule exists
     */
    [[nodiscard]] auto rule_exists(std::string_view rule_id) const -> bool;

    // =========================================================================
    // Rule Statistics
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
     * @brief Increment studies prefetched count for a rule
     *
     * @param rule_id The rule ID to update
     * @param count Number of studies to add (default: 1)
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto increment_studies_prefetched(
        std::string_view rule_id, size_t count = 1) -> VoidResult;

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
    // History Operations
    // =========================================================================

    /**
     * @brief Save a prefetch history record
     *
     * @param history The history record to save
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto save_history(const client::prefetch_history& history) -> VoidResult;

    /**
     * @brief Find history records with query options
     *
     * @param options Query options for filtering and pagination
     * @return Vector of matching history records
     */
    [[nodiscard]] auto find_history(const prefetch_history_query_options& options = {}) const
        -> std::vector<client::prefetch_history>;

    /**
     * @brief Check if a study has been prefetched
     *
     * @param study_uid Study Instance UID to check
     * @return true if the study has been prefetched
     */
    [[nodiscard]] auto is_study_prefetched(std::string_view study_uid) const -> bool;

    /**
     * @brief Get count of prefetches completed today
     *
     * @return Number of completed prefetches today
     */
    [[nodiscard]] auto count_completed_today() const -> size_t;

    /**
     * @brief Get count of prefetches failed today
     *
     * @return Number of failed prefetches today
     */
    [[nodiscard]] auto count_failed_today() const -> size_t;

    /**
     * @brief Update history status
     *
     * @param study_uid Study Instance UID
     * @param status New status
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto update_history_status(
        std::string_view study_uid,
        std::string_view status) -> VoidResult;

    /**
     * @brief Cleanup old history records
     *
     * @param max_age Maximum age of records to keep
     * @return Number of deleted records or error
     */
    [[nodiscard]] auto cleanup_old_history(std::chrono::hours max_age)
        -> Result<size_t>;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get total rule count
     *
     * @return Number of rules in the repository
     */
    [[nodiscard]] auto rule_count() const -> size_t;

    /**
     * @brief Get enabled rule count
     *
     * @return Number of enabled rules
     */
    [[nodiscard]] auto enabled_rule_count() const -> size_t;

    /**
     * @brief Get total history count
     *
     * @return Number of history records
     */
    [[nodiscard]] auto history_count() const -> size_t;

    // =========================================================================
    // Database Information
    // =========================================================================

    /**
     * @brief Check if the database connection is valid
     *
     * @return true if the database handle is valid
     */
    [[nodiscard]] auto is_valid() const noexcept -> bool;

    /**
     * @brief Initialize database tables
     *
     * Creates the prefetch_rules and prefetch_history tables if they don't exist.
     *
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto initialize_tables() -> VoidResult;

private:
    // =========================================================================
    // Private Implementation
    // =========================================================================

    /**
     * @brief Parse a prefetch rule from a prepared statement
     */
    [[nodiscard]] auto parse_rule_row(void* stmt) const -> client::prefetch_rule;

    /**
     * @brief Parse a prefetch history from a prepared statement
     */
    [[nodiscard]] auto parse_history_row(void* stmt) const -> client::prefetch_history;

    /**
     * @brief Serialize modalities vector to JSON
     */
    [[nodiscard]] static auto serialize_modalities(
        const std::vector<std::string>& modalities) -> std::string;

    /**
     * @brief Deserialize modalities from JSON
     */
    [[nodiscard]] static auto deserialize_modalities(
        std::string_view json) -> std::vector<std::string>;

    /**
     * @brief Serialize node IDs vector to JSON
     */
    [[nodiscard]] static auto serialize_node_ids(
        const std::vector<std::string>& node_ids) -> std::string;

    /**
     * @brief Deserialize node IDs from JSON
     */
    [[nodiscard]] static auto deserialize_node_ids(
        std::string_view json) -> std::vector<std::string>;

    /// SQLite database handle
    sqlite3* db_{nullptr};
};

}  // namespace pacs::storage
