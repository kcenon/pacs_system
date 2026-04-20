// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file ups_repository.h
 * @brief Repository for UPS lifecycle and subscription persistence
 *
 * Persists Unified Procedure Step (UPS) work items and their global /
 * filtered subscriptions, as defined in DICOM PS3.4 Annex CC (Unified
 * Worklist and Procedure Step Service Class). Supports UPS state
 * transitions (SCHEDULED -> IN_PROGRESS -> COMPLETED / CANCELED) and
 * push notifications via subscribers.
 *
 * @see Issue #915 - Part 4: Extract UPS and audit lifecycle repositories
 * @see DICOM PS3.4 Annex CC - Unified Procedure Step Service Class
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "kcenon/pacs/storage/ups_workitem.h"

#include <kcenon/common/patterns/result.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef PACS_WITH_DATABASE_SYSTEM

#include "kcenon/pacs/storage/base_repository.h"

namespace kcenon::pacs::storage {

/**
 * @brief Persistence for Unified Procedure Step (UPS) work items and subscriptions
 *
 * Backed by the database_system abstraction. Covers the two tables
 * required by the UPS Push and UPS Watch SOP classes:
 * - UPS work items (state machine, metadata, performed steps)
 * - UPS subscriptions (global, filtered, or per-work-item)
 *
 * **Thread Safety:** Not thread-safe. External synchronization is required.
 */
class ups_repository : public base_repository<ups_workitem, int64_t> {
public:
    /**
     * @brief Construct a UPS repository bound to a database adapter
     * @param db Shared database adapter; must be connected before calls
     */
    explicit ups_repository(std::shared_ptr<pacs_database_adapter> db);
    ~ups_repository() override = default;

    ups_repository(const ups_repository&) = delete;
    auto operator=(const ups_repository&) -> ups_repository& = delete;
    ups_repository(ups_repository&&) noexcept = default;
    auto operator=(ups_repository&&) noexcept -> ups_repository& = default;

    /**
     * @brief Insert a new UPS work item (N-CREATE)
     *
     * Typically used to create a SCHEDULED work item. The transaction
     * UID is cleared on insert.
     *
     * @param workitem Fully populated work item
     * @return Result with the database primary key, or error
     */
    [[nodiscard]] auto create_ups_workitem(const ups_workitem& workitem)
        -> Result<int64_t>;

    /**
     * @brief Persist all fields of a UPS work item (N-SET)
     * @param workitem Updated work item; identified by workitem_uid
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto update_ups_workitem(const ups_workitem& workitem)
        -> VoidResult;

    /**
     * @brief Change the UPS Procedure Step State (N-ACTION Change State)
     *
     * Enforces the UPS state machine: SCHEDULED -> IN_PROGRESS ->
     * COMPLETED / CANCELED. The transaction UID must be supplied when
     * transitioning into or out of IN_PROGRESS as required by PS3.4.
     *
     * @param workitem_uid UPS SOP Instance UID
     * @param new_state Target state string
     * @param transaction_uid Transaction UID scoping this state change
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto change_ups_state(std::string_view workitem_uid,
                                        std::string_view new_state,
                                        std::string_view transaction_uid = "")
        -> VoidResult;

    /**
     * @brief Look up a UPS work item by SOP Instance UID
     * @param workitem_uid UPS UID
     * @return Work item if present, std::nullopt otherwise
     */
    [[nodiscard]] auto find_ups_workitem(std::string_view workitem_uid)
        -> std::optional<ups_workitem>;

    /**
     * @brief Search UPS work items with a structured query
     * @param query Multi-field query (state, modality, scheduled station, etc.)
     * @return Result containing matching work items or a database error
     */
    [[nodiscard]] auto search_ups_workitems(const ups_workitem_query& query)
        -> Result<std::vector<ups_workitem>>;

    /**
     * @brief Delete a UPS work item by SOP Instance UID
     *
     * Cascades to remove any per-work-item subscriptions for that UID.
     *
     * @param workitem_uid UPS UID
     * @return VoidResult, or error if the work item did not exist
     */
    [[nodiscard]] auto delete_ups_workitem(std::string_view workitem_uid)
        -> VoidResult;

    /**
     * @brief Total number of UPS work items currently stored
     * @return Result containing the row count
     */
    [[nodiscard]] auto ups_workitem_count() -> Result<size_t>;

    /**
     * @brief Number of UPS work items in a specific state
     * @param state State value to filter on
     * @return Result containing the filtered row count
     */
    [[nodiscard]] auto ups_workitem_count(std::string_view state)
        -> Result<size_t>;

    /**
     * @brief Register a UPS subscription (N-ACTION Subscribe)
     *
     * Supports all three subscription types defined in PS3.4 Annex CC:
     * Global Subscription, Filtered Global Subscription, and Subscription
     * to a single work item.
     *
     * @param subscription Subscription descriptor
     * @return Result with the database primary key, or error
     */
    [[nodiscard]] auto subscribe_ups(const ups_subscription& subscription)
        -> Result<int64_t>;

    /**
     * @brief Remove a UPS subscription (N-ACTION Unsubscribe)
     * @param subscriber_ae Subscriber AE title
     * @param workitem_uid Specific work item UID, or empty to remove all
     *                     subscriptions owned by subscriber_ae
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto unsubscribe_ups(std::string_view subscriber_ae,
                                       std::string_view workitem_uid = "")
        -> VoidResult;

    /**
     * @brief List every subscription owned by a subscriber AE
     * @param subscriber_ae Subscriber AE title
     * @return Result containing all matching subscription rows
     */
    [[nodiscard]] auto get_ups_subscriptions(std::string_view subscriber_ae)
        -> Result<std::vector<ups_subscription>>;

    /**
     * @brief List every subscriber AE that should be notified for a work item
     *
     * Includes global and filtered subscribers that match the given UID.
     *
     * @param workitem_uid UPS UID
     * @return Result containing the distinct subscriber AE titles
     */
    [[nodiscard]] auto get_ups_subscribers(std::string_view workitem_uid)
        -> Result<std::vector<std::string>>;

protected:
    [[nodiscard]] auto map_row_to_entity(const database_row& row) const
        -> ups_workitem override;
    [[nodiscard]] auto entity_to_row(const ups_workitem& entity) const
        -> std::map<std::string, database_value> override;
    [[nodiscard]] auto get_pk(const ups_workitem& entity) const
        -> int64_t override;
    [[nodiscard]] auto has_pk(const ups_workitem& entity) const
        -> bool override;
    [[nodiscard]] auto select_columns() const -> std::vector<std::string> override;

private:
    [[nodiscard]] static auto to_like_pattern(std::string_view pattern)
        -> std::string;
    [[nodiscard]] auto parse_timestamp(const std::string& str) const
        -> std::chrono::system_clock::time_point;
    [[nodiscard]] auto format_timestamp(
        std::chrono::system_clock::time_point tp) const -> std::string;
};

}  // namespace kcenon::pacs::storage

#else

struct sqlite3;

namespace kcenon::pacs::storage {

template <typename T>
using Result = kcenon::common::Result<T>;

using VoidResult = kcenon::common::VoidResult;

/**
 * @brief Legacy direct-SQLite fallback when database_system is disabled
 *
 * Exposes the same API surface as the database_system-backed variant.
 * Used only when PACS_WITH_DATABASE_SYSTEM is not defined at build time.
 */
class ups_repository {
public:
    /**
     * @brief Construct repository bound to a raw sqlite3 handle
     * @param db Non-owning pointer to an open sqlite3 connection
     */
    explicit ups_repository(sqlite3* db);
    ~ups_repository();

    ups_repository(const ups_repository&) = delete;
    auto operator=(const ups_repository&) -> ups_repository& = delete;
    ups_repository(ups_repository&&) noexcept;
    auto operator=(ups_repository&&) noexcept -> ups_repository&;

    /// @brief Insert a new UPS work item (N-CREATE)
    [[nodiscard]] auto create_ups_workitem(const ups_workitem& workitem)
        -> Result<int64_t>;
    /// @brief Persist all fields of a UPS work item (N-SET)
    [[nodiscard]] auto update_ups_workitem(const ups_workitem& workitem)
        -> VoidResult;
    /// @brief Change the UPS Procedure Step State (N-ACTION Change State)
    [[nodiscard]] auto change_ups_state(std::string_view workitem_uid,
                                        std::string_view new_state,
                                        std::string_view transaction_uid = "")
        -> VoidResult;
    /// @brief Look up a UPS work item by SOP Instance UID
    [[nodiscard]] auto find_ups_workitem(std::string_view workitem_uid) const
        -> std::optional<ups_workitem>;
    /// @brief Search UPS work items with a structured query
    [[nodiscard]] auto search_ups_workitems(const ups_workitem_query& query) const
        -> Result<std::vector<ups_workitem>>;
    /// @brief Delete a UPS work item by SOP Instance UID
    [[nodiscard]] auto delete_ups_workitem(std::string_view workitem_uid)
        -> VoidResult;
    /// @brief Total number of UPS work items currently stored
    [[nodiscard]] auto ups_workitem_count() const -> Result<size_t>;
    /// @brief Number of UPS work items in a specific state
    [[nodiscard]] auto ups_workitem_count(std::string_view state) const
        -> Result<size_t>;
    /// @brief Register a UPS subscription (N-ACTION Subscribe)
    [[nodiscard]] auto subscribe_ups(const ups_subscription& subscription)
        -> Result<int64_t>;
    /// @brief Remove a UPS subscription (N-ACTION Unsubscribe)
    [[nodiscard]] auto unsubscribe_ups(std::string_view subscriber_ae,
                                       std::string_view workitem_uid = "")
        -> VoidResult;
    /// @brief List every subscription owned by a subscriber AE
    [[nodiscard]] auto get_ups_subscriptions(std::string_view subscriber_ae) const
        -> Result<std::vector<ups_subscription>>;
    /// @brief List subscriber AEs that should be notified for a work item
    [[nodiscard]] auto get_ups_subscribers(std::string_view workitem_uid) const
        -> Result<std::vector<std::string>>;

private:
    [[nodiscard]] auto parse_ups_workitem_row(void* stmt) const -> ups_workitem;
    [[nodiscard]] static auto to_like_pattern(std::string_view pattern)
        -> std::string;
    [[nodiscard]] static auto parse_timestamp(const std::string& str)
        -> std::chrono::system_clock::time_point;

    sqlite3* db_{nullptr};
};

}  // namespace kcenon::pacs::storage

#endif
