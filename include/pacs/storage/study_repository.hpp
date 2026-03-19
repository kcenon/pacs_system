// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file study_repository.hpp
 * @brief Repository for study metadata persistence using base_repository pattern
 *
 * Extracted from index_database as part of the storage decomposition (Issue #896).
 * Provides focused CRUD operations for study records.
 *
 * @see Issue #896 - Decompose index_database into metadata and lifecycle stores
 * @see Issue #912 - Part 1: Extract patient and study metadata repositories
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "pacs/storage/study_record.hpp"

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
 * @brief Repository for study metadata persistence using base_repository pattern
 *
 * Extends base_repository to provide standard CRUD operations for study records.
 * Supports wildcard search, upsert semantics, patient-filtered queries,
 * and modalities_in_study denormalization.
 *
 * Thread Safety: NOT thread-safe. External synchronization required.
 *
 * @see Issue #912 - Extract study repository from index_database
 */
class study_repository : public base_repository<study_record, int64_t> {
public:
    explicit study_repository(std::shared_ptr<pacs_database_adapter> db);
    ~study_repository() override = default;

    study_repository(const study_repository&) = delete;
    auto operator=(const study_repository&) -> study_repository& = delete;
    study_repository(study_repository&&) noexcept = default;
    auto operator=(study_repository&&) noexcept -> study_repository& = default;

    // =========================================================================
    // Domain-Specific Operations
    // =========================================================================

    /**
     * @brief Insert or update a study record
     */
    [[nodiscard]] auto upsert_study(int64_t patient_pk,
                                    std::string_view study_uid,
                                    std::string_view study_id = "",
                                    std::string_view study_date = "",
                                    std::string_view study_time = "",
                                    std::string_view accession_number = "",
                                    std::string_view referring_physician = "",
                                    std::string_view study_description = "")
        -> Result<int64_t>;

    /**
     * @brief Insert or update a study record with full details
     */
    [[nodiscard]] auto upsert_study(const study_record& record)
        -> Result<int64_t>;

    /**
     * @brief Find a study by Study Instance UID
     */
    [[nodiscard]] auto find_study(std::string_view study_uid)
        -> std::optional<study_record>;

    /**
     * @brief Find a study by primary key
     */
    [[nodiscard]] auto find_study_by_pk(int64_t pk)
        -> std::optional<study_record>;

    /**
     * @brief Search studies with query criteria
     *
     * Supports wildcard matching, patient-level filters, date ranges,
     * and modality filtering.
     */
    [[nodiscard]] auto search_studies(const study_query& query)
        -> Result<std::vector<study_record>>;

    /**
     * @brief Delete a study by Study Instance UID
     */
    [[nodiscard]] auto delete_study(std::string_view study_uid) -> VoidResult;

    /**
     * @brief Get total study count
     */
    [[nodiscard]] auto study_count() -> Result<size_t>;

    /**
     * @brief Get study count for a specific patient
     */
    [[nodiscard]] auto study_count_for_patient(int64_t patient_pk)
        -> Result<size_t>;

    /**
     * @brief Update modalities in study (denormalized field)
     *
     * Fetches distinct modalities from series table and updates the
     * modalities_in_study field with backslash-separated values.
     */
    [[nodiscard]] auto update_modalities_in_study(int64_t study_pk)
        -> VoidResult;

protected:
    // =========================================================================
    // base_repository overrides
    // =========================================================================

    [[nodiscard]] auto map_row_to_entity(const database_row& row) const
        -> study_record override;

    [[nodiscard]] auto entity_to_row(const study_record& entity) const
        -> std::map<std::string, database_value> override;

    [[nodiscard]] auto get_pk(const study_record& entity) const
        -> int64_t override;

    [[nodiscard]] auto has_pk(const study_record& entity) const
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
 * @brief Repository for study metadata persistence (legacy SQLite interface)
 */
class study_repository {
public:
    explicit study_repository(sqlite3* db);
    ~study_repository();

    study_repository(const study_repository&) = delete;
    auto operator=(const study_repository&) -> study_repository& = delete;
    study_repository(study_repository&&) noexcept;
    auto operator=(study_repository&&) noexcept -> study_repository&;

    [[nodiscard]] auto upsert_study(int64_t patient_pk,
                                    std::string_view study_uid,
                                    std::string_view study_id = "",
                                    std::string_view study_date = "",
                                    std::string_view study_time = "",
                                    std::string_view accession_number = "",
                                    std::string_view referring_physician = "",
                                    std::string_view study_description = "")
        -> Result<int64_t>;
    [[nodiscard]] auto upsert_study(const study_record& record)
        -> Result<int64_t>;
    [[nodiscard]] auto find_study(std::string_view study_uid) const
        -> std::optional<study_record>;
    [[nodiscard]] auto find_study_by_pk(int64_t pk) const
        -> std::optional<study_record>;
    [[nodiscard]] auto search_studies(const study_query& query) const
        -> Result<std::vector<study_record>>;
    [[nodiscard]] auto delete_study(std::string_view study_uid) -> VoidResult;
    [[nodiscard]] auto study_count() const -> Result<size_t>;
    [[nodiscard]] auto study_count_for_patient(int64_t patient_pk) const
        -> Result<size_t>;
    [[nodiscard]] auto update_modalities_in_study(int64_t study_pk)
        -> VoidResult;

private:
    [[nodiscard]] auto parse_study_row(void* stmt) const -> study_record;
    [[nodiscard]] static auto to_like_pattern(std::string_view pattern)
        -> std::string;

    sqlite3* db_{nullptr};
};

}  // namespace kcenon::pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
