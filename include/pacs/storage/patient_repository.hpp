// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file patient_repository.hpp
 * @brief Repository for patient metadata persistence using base_repository pattern
 *
 * Extracted from index_database as part of the storage decomposition (Issue #896).
 * Provides focused CRUD operations for patient records.
 *
 * @see Issue #896 - Decompose index_database into metadata and lifecycle stores
 * @see Issue #912 - Part 1: Extract patient and study metadata repositories
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "pacs/storage/patient_record.hpp"

#include <kcenon/common/patterns/result.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef PACS_WITH_DATABASE_SYSTEM

#include "pacs/storage/base_repository.hpp"

namespace kcenon::pacs::storage {

/**
 * @brief Repository for patient metadata persistence using base_repository pattern
 *
 * Extends base_repository to provide standard CRUD operations for patient records.
 * Supports wildcard search, upsert semantics, and pagination.
 *
 * Thread Safety: NOT thread-safe. External synchronization required.
 *
 * @see Issue #912 - Extract patient repository from index_database
 */
class patient_repository : public base_repository<patient_record, int64_t> {
public:
    explicit patient_repository(std::shared_ptr<pacs_database_adapter> db);
    ~patient_repository() override = default;

    patient_repository(const patient_repository&) = delete;
    auto operator=(const patient_repository&) -> patient_repository& = delete;
    patient_repository(patient_repository&&) noexcept = default;
    auto operator=(patient_repository&&) noexcept -> patient_repository& = default;

    // =========================================================================
    // Domain-Specific Operations
    // =========================================================================

    /**
     * @brief Insert or update a patient record
     *
     * If a patient with the same patient_id exists, updates the record.
     * Otherwise, inserts a new record.
     *
     * @param patient_id Patient ID (required, max 64 chars)
     * @param patient_name Patient's name in DICOM PN format
     * @param birth_date Birth date in YYYYMMDD format
     * @param sex Sex code (M, F, O)
     * @return Result containing the patient's primary key or error
     */
    [[nodiscard]] auto upsert_patient(std::string_view patient_id,
                                      std::string_view patient_name = "",
                                      std::string_view birth_date = "",
                                      std::string_view sex = "")
        -> Result<int64_t>;

    /**
     * @brief Insert or update a patient record with full details
     *
     * @param record Complete patient record (pk field is ignored for insert)
     * @return Result containing the patient's primary key or error
     */
    [[nodiscard]] auto upsert_patient(const patient_record& record)
        -> Result<int64_t>;

    /**
     * @brief Find a patient by patient ID
     *
     * @param patient_id The patient ID to search for
     * @return Optional containing the patient record if found
     */
    [[nodiscard]] auto find_patient(std::string_view patient_id)
        -> std::optional<patient_record>;

    /**
     * @brief Find a patient by primary key
     *
     * @param pk The patient's primary key
     * @return Optional containing the patient record if found
     */
    [[nodiscard]] auto find_patient_by_pk(int64_t pk)
        -> std::optional<patient_record>;

    /**
     * @brief Search patients with query criteria
     *
     * Supports wildcard matching using '*' character.
     *
     * @param query Query parameters with optional filters
     * @return Result containing vector of matching patient records or error
     */
    [[nodiscard]] auto search_patients(const patient_query& query)
        -> Result<std::vector<patient_record>>;

    /**
     * @brief Delete a patient by patient ID
     *
     * @param patient_id The patient ID to delete
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto delete_patient(std::string_view patient_id)
        -> VoidResult;

    /**
     * @brief Get total patient count
     *
     * @return Result containing number of patients in the database or error
     */
    [[nodiscard]] auto patient_count() -> Result<size_t>;

protected:
    // =========================================================================
    // base_repository overrides
    // =========================================================================

    [[nodiscard]] auto map_row_to_entity(const database_row& row) const
        -> patient_record override;

    [[nodiscard]] auto entity_to_row(const patient_record& entity) const
        -> std::map<std::string, database_value> override;

    [[nodiscard]] auto get_pk(const patient_record& entity) const
        -> int64_t override;

    [[nodiscard]] auto has_pk(const patient_record& entity) const
        -> bool override;

    [[nodiscard]] auto select_columns() const
        -> std::vector<std::string> override;

private:
    [[nodiscard]] static auto to_like_pattern(std::string_view pattern)
        -> std::string;

    [[nodiscard]] auto parse_timestamp(const std::string& str) const
        -> std::chrono::system_clock::time_point;

    [[nodiscard]] auto format_timestamp(
        std::chrono::system_clock::time_point tp) const -> std::string;
};

}  // namespace kcenon::pacs::storage

#else  // !PACS_WITH_DATABASE_SYSTEM

struct sqlite3;

namespace kcenon::pacs::storage {

template <typename T>
using Result = kcenon::common::Result<T>;

using VoidResult = kcenon::common::VoidResult;

/**
 * @brief Repository for patient metadata persistence (legacy SQLite interface)
 */
class patient_repository {
public:
    explicit patient_repository(sqlite3* db);
    ~patient_repository();

    patient_repository(const patient_repository&) = delete;
    auto operator=(const patient_repository&) -> patient_repository& = delete;
    patient_repository(patient_repository&&) noexcept;
    auto operator=(patient_repository&&) noexcept -> patient_repository&;

    [[nodiscard]] auto upsert_patient(std::string_view patient_id,
                                      std::string_view patient_name = "",
                                      std::string_view birth_date = "",
                                      std::string_view sex = "")
        -> Result<int64_t>;
    [[nodiscard]] auto upsert_patient(const patient_record& record)
        -> Result<int64_t>;
    [[nodiscard]] auto find_patient(std::string_view patient_id) const
        -> std::optional<patient_record>;
    [[nodiscard]] auto find_patient_by_pk(int64_t pk) const
        -> std::optional<patient_record>;
    [[nodiscard]] auto search_patients(const patient_query& query) const
        -> Result<std::vector<patient_record>>;
    [[nodiscard]] auto delete_patient(std::string_view patient_id)
        -> VoidResult;
    [[nodiscard]] auto patient_count() const -> Result<size_t>;

private:
    [[nodiscard]] auto parse_patient_row(void* stmt) const -> patient_record;
    [[nodiscard]] static auto to_like_pattern(std::string_view pattern)
        -> std::string;

    sqlite3* db_{nullptr};
};

}  // namespace kcenon::pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
