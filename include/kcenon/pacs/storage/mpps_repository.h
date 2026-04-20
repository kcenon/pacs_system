// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file mpps_repository.h
 * @brief Repository for MPPS lifecycle persistence using base_repository pattern
 *
 * Persists Modality Performed Procedure Step (MPPS) records defined by
 * DICOM PS3.4 Annex F. MPPS reports the IN_PROGRESS, COMPLETED, and
 * DISCONTINUED phases of a study performed by an acquisition modality.
 *
 * @see Issue #914 - Part 3: Extract MPPS and worklist lifecycle repositories
 * @see DICOM PS3.4 Annex F - Modality Performed Procedure Step Service Class
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "kcenon/pacs/storage/mpps_record.h"

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
 * @brief Persistence for Modality Performed Procedure Step (MPPS) records
 *
 * Backed by the database_system abstraction. Supports the full MPPS
 * lifecycle: N-CREATE to open a record in IN_PROGRESS state, N-SET to
 * transition to COMPLETED or DISCONTINUED, plus lookups by MPPS UID,
 * study UID, and station AE title.
 *
 * **Thread Safety:** Not thread-safe. External synchronization is required.
 */
class mpps_repository : public base_repository<mpps_record, int64_t> {
public:
    /**
     * @brief Construct an MPPS repository bound to a database adapter
     * @param db Shared database adapter; must be connected before calls
     */
    explicit mpps_repository(std::shared_ptr<pacs_database_adapter> db);
    ~mpps_repository() override = default;

    mpps_repository(const mpps_repository&) = delete;
    auto operator=(const mpps_repository&) -> mpps_repository& = delete;
    mpps_repository(mpps_repository&&) noexcept = default;
    auto operator=(mpps_repository&&) noexcept -> mpps_repository& = default;

    /**
     * @brief Create a new MPPS record in IN_PROGRESS state (N-CREATE)
     *
     * Convenience overload that accepts the most common fields inline.
     *
     * @param mpps_uid SOP Instance UID for the MPPS (0008,0018); must be unique
     * @param station_ae Station AE Title of the acquisition modality
     * @param modality DICOM modality code (e.g., "CT", "MR", "US")
     * @param study_uid Study Instance UID referenced by this step
     * @param accession_no Accession Number from the scheduled procedure
     * @param start_datetime Performed Procedure Step Start Date/Time (DICOM DT)
     * @return Result with the database primary key, or error
     */
    [[nodiscard]] auto create_mpps(std::string_view mpps_uid,
                                   std::string_view station_ae = "",
                                   std::string_view modality = "",
                                   std::string_view study_uid = "",
                                   std::string_view accession_no = "",
                                   std::string_view start_datetime = "")
        -> Result<int64_t>;

    /**
     * @brief Create a new MPPS record from a fully populated value object
     * @param record Full MPPS record; primary key is ignored on insert
     * @return Result with the database primary key, or error
     */
    [[nodiscard]] auto create_mpps(const mpps_record& record) -> Result<int64_t>;

    /**
     * @brief Apply an MPPS status transition (N-SET)
     *
     * Typical transitions: IN_PROGRESS -> COMPLETED or IN_PROGRESS ->
     * DISCONTINUED. Once the record reaches a terminal state, further
     * status updates are rejected by the service layer.
     *
     * @param mpps_uid MPPS SOP Instance UID identifying the record
     * @param new_status Target status ("COMPLETED" or "DISCONTINUED")
     * @param end_datetime Performed Procedure Step End Date/Time, optional
     * @param performed_series Serialized Performed Series Sequence, optional
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto update_mpps(std::string_view mpps_uid,
                                   std::string_view new_status,
                                   std::string_view end_datetime = "",
                                   std::string_view performed_series = "")
        -> VoidResult;

    /**
     * @brief Persist all fields from an MPPS record (full update)
     * @param record MPPS record with the new field values
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto update_mpps(const mpps_record& record) -> VoidResult;

    /**
     * @brief Look up an MPPS record by MPPS SOP Instance UID
     * @param mpps_uid MPPS UID
     * @return Record if present, std::nullopt otherwise
     */
    [[nodiscard]] auto find_mpps(std::string_view mpps_uid)
        -> std::optional<mpps_record>;

    /**
     * @brief Look up an MPPS record by database primary key
     * @param pk Database primary key
     * @return Record if present, std::nullopt otherwise
     */
    [[nodiscard]] auto find_mpps_by_pk(int64_t pk)
        -> std::optional<mpps_record>;

    /**
     * @brief List currently active (non-terminal) MPPS records for a station
     * @param station_ae Station AE Title filter
     * @return Result containing active records or a database error
     */
    [[nodiscard]] auto list_active_mpps(std::string_view station_ae)
        -> Result<std::vector<mpps_record>>;

    /**
     * @brief Find all MPPS records referencing a given Study Instance UID
     * @param study_uid Study Instance UID
     * @return Result containing matching records or a database error
     */
    [[nodiscard]] auto find_mpps_by_study(std::string_view study_uid)
        -> Result<std::vector<mpps_record>>;

    /**
     * @brief Search MPPS records using a structured query
     * @param query Multi-field query (status, modality, date range, etc.)
     * @return Result containing matching records or a database error
     */
    [[nodiscard]] auto search_mpps(const mpps_query& query)
        -> Result<std::vector<mpps_record>>;

    /**
     * @brief Delete an MPPS record by MPPS SOP Instance UID
     * @param mpps_uid MPPS UID
     * @return VoidResult, or error if the record did not exist
     */
    [[nodiscard]] auto delete_mpps(std::string_view mpps_uid) -> VoidResult;

    /**
     * @brief Total number of MPPS records currently stored
     * @return Result containing the row count
     */
    [[nodiscard]] auto mpps_count() -> Result<size_t>;

    /**
     * @brief Number of MPPS records in a specific status
     * @param status Status value to filter on
     * @return Result containing the filtered row count
     */
    [[nodiscard]] auto mpps_count(std::string_view status) -> Result<size_t>;

protected:
    [[nodiscard]] auto map_row_to_entity(const database_row& row) const
        -> mpps_record override;
    [[nodiscard]] auto entity_to_row(const mpps_record& entity) const
        -> std::map<std::string, database_value> override;
    [[nodiscard]] auto get_pk(const mpps_record& entity) const
        -> int64_t override;
    [[nodiscard]] auto has_pk(const mpps_record& entity) const
        -> bool override;
    [[nodiscard]] auto select_columns() const -> std::vector<std::string> override;

private:
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
class mpps_repository {
public:
    /**
     * @brief Construct repository bound to a raw sqlite3 handle
     * @param db Non-owning pointer to an open sqlite3 connection
     */
    explicit mpps_repository(sqlite3* db);
    ~mpps_repository();

    mpps_repository(const mpps_repository&) = delete;
    auto operator=(const mpps_repository&) -> mpps_repository& = delete;
    mpps_repository(mpps_repository&&) noexcept;
    auto operator=(mpps_repository&&) noexcept -> mpps_repository&;

    /// @brief Create a new MPPS record in IN_PROGRESS state (inline fields)
    [[nodiscard]] auto create_mpps(std::string_view mpps_uid,
                                   std::string_view station_ae = "",
                                   std::string_view modality = "",
                                   std::string_view study_uid = "",
                                   std::string_view accession_no = "",
                                   std::string_view start_datetime = "")
        -> Result<int64_t>;
    /// @brief Create a new MPPS record from a populated value object
    [[nodiscard]] auto create_mpps(const mpps_record& record) -> Result<int64_t>;
    /// @brief Apply an MPPS status transition (N-SET)
    [[nodiscard]] auto update_mpps(std::string_view mpps_uid,
                                   std::string_view new_status,
                                   std::string_view end_datetime = "",
                                   std::string_view performed_series = "")
        -> VoidResult;
    /// @brief Persist all fields from an MPPS record
    [[nodiscard]] auto update_mpps(const mpps_record& record) -> VoidResult;
    /// @brief Look up an MPPS record by MPPS SOP Instance UID
    [[nodiscard]] auto find_mpps(std::string_view mpps_uid) const
        -> std::optional<mpps_record>;
    /// @brief Look up an MPPS record by database primary key
    [[nodiscard]] auto find_mpps_by_pk(int64_t pk) const
        -> std::optional<mpps_record>;
    /// @brief List non-terminal MPPS records for a station AE
    [[nodiscard]] auto list_active_mpps(std::string_view station_ae) const
        -> Result<std::vector<mpps_record>>;
    /// @brief Find all MPPS records referencing a Study Instance UID
    [[nodiscard]] auto find_mpps_by_study(std::string_view study_uid) const
        -> Result<std::vector<mpps_record>>;
    /// @brief Search MPPS records using a structured query
    [[nodiscard]] auto search_mpps(const mpps_query& query) const
        -> Result<std::vector<mpps_record>>;
    /// @brief Delete an MPPS record by MPPS SOP Instance UID
    [[nodiscard]] auto delete_mpps(std::string_view mpps_uid) -> VoidResult;
    /// @brief Total number of MPPS records currently stored
    [[nodiscard]] auto mpps_count() const -> Result<size_t>;
    /// @brief Number of MPPS records in a specific status
    [[nodiscard]] auto mpps_count(std::string_view status) const
        -> Result<size_t>;

private:
    [[nodiscard]] auto parse_mpps_row(void* stmt) const -> mpps_record;
    [[nodiscard]] static auto parse_timestamp(const std::string& str)
        -> std::chrono::system_clock::time_point;

    sqlite3* db_{nullptr};
};

}  // namespace kcenon::pacs::storage

#endif
