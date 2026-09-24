// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file worklist_repository.h
 * @brief Repository for modality worklist persistence using base_repository pattern
 *
 * Provides CRUD and query operations for DICOM Modality Worklist (MWL)
 * items as defined in DICOM PS3.4 Annex K. The worklist supplies imaging
 * modalities with scheduled procedure step (SPS) information for upcoming
 * studies.
 *
 * @see Issue #914 - Part 3: Extract MPPS and worklist lifecycle repositories
 * @see DICOM PS3.4 Annex K - Modality Worklist Service Class
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "kcenon/pacs/storage/worklist_record.h"

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
 * @brief Persistence layer for DICOM Modality Worklist (MWL) items
 *
 * Backed by the database_system abstraction. Supports adding scheduled
 * procedure steps, updating their status through the SPS lifecycle,
 * querying by scheduled AE title / modality / date / patient, and
 * administrative cleanup of completed or expired entries.
 *
 * **Thread Safety:** Not thread-safe. The owning service must serialize
 * concurrent access.
 */
class worklist_repository : public base_repository<worklist_item, int64_t> {
public:
    /**
     * @brief Construct a worklist repository bound to a database adapter
     * @param db Shared database adapter; must be connected before calls
     */
    explicit worklist_repository(std::shared_ptr<pacs_database_adapter> db);
    ~worklist_repository() override = default;

    worklist_repository(const worklist_repository&) = delete;
    auto operator=(const worklist_repository&) -> worklist_repository& = delete;
    worklist_repository(worklist_repository&&) noexcept = default;
    auto operator=(worklist_repository&&) noexcept
        -> worklist_repository& = default;

    /**
     * @brief Insert a new worklist item
     * @param item Fully populated worklist_item; auto-generated primary
     *             key is ignored on insert
     * @return Result with the newly assigned primary key, or error
     */
    [[nodiscard]] auto add_worklist_item(const worklist_item& item)
        -> Result<int64_t>;

    /**
     * @brief Update the status of a worklist item in place
     *
     * Typical transitions are SCHEDULED -> IN_PROGRESS -> COMPLETED or
     * CANCELED, matching the DICOM SPS status values.
     *
     * @param step_id Scheduled Procedure Step ID (0040,0009)
     * @param accession_no Accession Number (0008,0050)
     * @param new_status New SPS status string
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto update_worklist_status(std::string_view step_id,
                                              std::string_view accession_no,
                                              std::string_view new_status)
        -> VoidResult;

    /**
     * @brief Query worklist items matching a filter
     * @param query Structured query (AE title, modality, date range, etc.)
     * @return Result containing matching items or a database error
     */
    [[nodiscard]] auto query_worklist(const worklist_query& query)
        -> Result<std::vector<worklist_item>>;

    /**
     * @brief Look up a single worklist item by (step_id, accession)
     * @param step_id Scheduled Procedure Step ID
     * @param accession_no Accession Number
     * @return Item if present, std::nullopt otherwise (not found is not an error)
     */
    [[nodiscard]] auto find_worklist_item(std::string_view step_id,
                                          std::string_view accession_no)
        -> std::optional<worklist_item>;

    /**
     * @brief Look up a worklist item by database primary key
     * @param pk Database primary key
     * @return Item if present, std::nullopt otherwise
     */
    [[nodiscard]] auto find_worklist_by_pk(int64_t pk)
        -> std::optional<worklist_item>;

    /**
     * @brief Delete a worklist item identified by (step_id, accession)
     * @param step_id Scheduled Procedure Step ID
     * @param accession_no Accession Number
     * @return VoidResult, or error if the item did not exist
     */
    [[nodiscard]] auto delete_worklist_item(std::string_view step_id,
                                            std::string_view accession_no)
        -> VoidResult;

    /**
     * @brief Remove worklist items older than the given age
     *
     * Age is measured from the scheduled procedure date. Intended for
     * retention/cleanup jobs.
     *
     * @param age Maximum age of items to retain
     * @return Result with the number of rows removed
     */
    [[nodiscard]] auto cleanup_old_worklist_items(std::chrono::hours age)
        -> Result<size_t>;

    /**
     * @brief Remove worklist items scheduled before an absolute time point
     * @param before Items scheduled strictly before this time are removed
     * @return Result with the number of rows removed
     */
    [[nodiscard]] auto cleanup_worklist_items_before(
        std::chrono::system_clock::time_point before) -> Result<size_t>;

    /**
     * @brief Total number of worklist items currently stored
     * @return Result containing the row count
     */
    [[nodiscard]] auto worklist_count() -> Result<size_t>;

    /**
     * @brief Number of worklist items in a specific SPS status
     * @param status SPS status value to filter on
     * @return Result containing the filtered row count
     */
    [[nodiscard]] auto worklist_count(std::string_view status)
        -> Result<size_t>;

protected:
    [[nodiscard]] auto map_row_to_entity(const database_row& row) const
        -> worklist_item override;
    [[nodiscard]] auto entity_to_row(const worklist_item& entity) const
        -> std::map<std::string, database_value> override;
    [[nodiscard]] auto get_pk(const worklist_item& entity) const
        -> int64_t override;
    [[nodiscard]] auto has_pk(const worklist_item& entity) const
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
class worklist_repository {
public:
    /**
     * @brief Construct repository bound to a raw sqlite3 handle
     * @param db Non-owning pointer to an open sqlite3 connection
     */
    explicit worklist_repository(sqlite3* db);
    ~worklist_repository();

    worklist_repository(const worklist_repository&) = delete;
    auto operator=(const worklist_repository&) -> worklist_repository& = delete;
    worklist_repository(worklist_repository&&) noexcept;
    auto operator=(worklist_repository&&) noexcept -> worklist_repository&;

    /// @brief Insert a new worklist item; returns the new primary key
    [[nodiscard]] auto add_worklist_item(const worklist_item& item)
        -> Result<int64_t>;
    /// @brief Update SPS status for a worklist item identified by (step_id, accession)
    [[nodiscard]] auto update_worklist_status(std::string_view step_id,
                                              std::string_view accession_no,
                                              std::string_view new_status)
        -> VoidResult;
    /// @brief Query worklist items matching a structured filter
    [[nodiscard]] auto query_worklist(const worklist_query& query) const
        -> Result<std::vector<worklist_item>>;
    /// @brief Look up a single worklist item by (step_id, accession)
    [[nodiscard]] auto find_worklist_item(std::string_view step_id,
                                          std::string_view accession_no) const
        -> std::optional<worklist_item>;
    /// @brief Look up a worklist item by database primary key
    [[nodiscard]] auto find_worklist_by_pk(int64_t pk) const
        -> std::optional<worklist_item>;
    /// @brief Delete a worklist item by (step_id, accession)
    [[nodiscard]] auto delete_worklist_item(std::string_view step_id,
                                            std::string_view accession_no)
        -> VoidResult;
    /// @brief Remove worklist items older than the given age
    [[nodiscard]] auto cleanup_old_worklist_items(std::chrono::hours age)
        -> Result<size_t>;
    /// @brief Remove worklist items scheduled before an absolute time point
    [[nodiscard]] auto cleanup_worklist_items_before(
        std::chrono::system_clock::time_point before) -> Result<size_t>;
    /// @brief Total number of worklist items currently stored
    [[nodiscard]] auto worklist_count() const -> Result<size_t>;
    /// @brief Number of worklist items in a specific SPS status
    [[nodiscard]] auto worklist_count(std::string_view status) const
        -> Result<size_t>;

private:
    [[nodiscard]] auto parse_worklist_row(void* stmt) const -> worklist_item;
    [[nodiscard]] static auto to_like_pattern(std::string_view pattern)
        -> std::string;
    [[nodiscard]] static auto parse_timestamp(const std::string& str)
        -> std::chrono::system_clock::time_point;

    sqlite3* db_{nullptr};
};

}  // namespace kcenon::pacs::storage

#endif
