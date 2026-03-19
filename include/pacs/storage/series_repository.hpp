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
 * @file series_repository.hpp
 * @brief Repository for series metadata persistence using base_repository pattern
 *
 * Extracted from index_database as part of the storage decomposition (Issue #896).
 * Provides focused CRUD operations for series records.
 *
 * @see Issue #896 - Decompose index_database into metadata and lifecycle stores
 * @see Issue #913 - Part 2: Extract series and instance metadata repositories
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "pacs/storage/series_record.hpp"

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
 * @brief Repository for series metadata persistence using base_repository pattern
 */
class series_repository : public base_repository<series_record, int64_t> {
public:
    explicit series_repository(std::shared_ptr<pacs_database_adapter> db);
    ~series_repository() override = default;

    series_repository(const series_repository&) = delete;
    auto operator=(const series_repository&) -> series_repository& = delete;
    series_repository(series_repository&&) noexcept = default;
    auto operator=(series_repository&&) noexcept -> series_repository& = default;

    [[nodiscard]] auto upsert_series(int64_t study_pk,
                                     std::string_view series_uid,
                                     std::string_view modality = "",
                                     std::optional<int> series_number = std::nullopt,
                                     std::string_view series_description = "",
                                     std::string_view body_part_examined = "",
                                     std::string_view station_name = "")
        -> Result<int64_t>;
    [[nodiscard]] auto upsert_series(const series_record& record)
        -> Result<int64_t>;
    [[nodiscard]] auto find_series(std::string_view series_uid)
        -> std::optional<series_record>;
    [[nodiscard]] auto find_series_by_pk(int64_t pk)
        -> std::optional<series_record>;
    [[nodiscard]] auto list_series(std::string_view study_uid)
        -> Result<std::vector<series_record>>;
    [[nodiscard]] auto search_series(const series_query& query)
        -> Result<std::vector<series_record>>;
    [[nodiscard]] auto delete_series(std::string_view series_uid)
        -> VoidResult;
    [[nodiscard]] auto series_count() -> Result<size_t>;
    [[nodiscard]] auto series_count(std::string_view study_uid)
        -> Result<size_t>;

protected:
    [[nodiscard]] auto map_row_to_entity(const database_row& row) const
        -> series_record override;
    [[nodiscard]] auto entity_to_row(const series_record& entity) const
        -> std::map<std::string, database_value> override;
    [[nodiscard]] auto get_pk(const series_record& entity) const
        -> int64_t override;
    [[nodiscard]] auto has_pk(const series_record& entity) const
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

#else  // !PACS_WITH_DATABASE_SYSTEM

struct sqlite3;

namespace kcenon::pacs::storage {

template <typename T>
using Result = kcenon::common::Result<T>;

using VoidResult = kcenon::common::VoidResult;

/**
 * @brief Repository for series metadata persistence (legacy SQLite interface)
 */
class series_repository {
public:
    explicit series_repository(sqlite3* db);
    ~series_repository();

    series_repository(const series_repository&) = delete;
    auto operator=(const series_repository&) -> series_repository& = delete;
    series_repository(series_repository&&) noexcept;
    auto operator=(series_repository&&) noexcept -> series_repository&;

    [[nodiscard]] auto upsert_series(int64_t study_pk,
                                     std::string_view series_uid,
                                     std::string_view modality = "",
                                     std::optional<int> series_number = std::nullopt,
                                     std::string_view series_description = "",
                                     std::string_view body_part_examined = "",
                                     std::string_view station_name = "")
        -> Result<int64_t>;
    [[nodiscard]] auto upsert_series(const series_record& record)
        -> Result<int64_t>;
    [[nodiscard]] auto find_series(std::string_view series_uid) const
        -> std::optional<series_record>;
    [[nodiscard]] auto find_series_by_pk(int64_t pk) const
        -> std::optional<series_record>;
    [[nodiscard]] auto list_series(std::string_view study_uid) const
        -> Result<std::vector<series_record>>;
    [[nodiscard]] auto search_series(const series_query& query) const
        -> Result<std::vector<series_record>>;
    [[nodiscard]] auto delete_series(std::string_view series_uid)
        -> VoidResult;
    [[nodiscard]] auto series_count() const -> Result<size_t>;
    [[nodiscard]] auto series_count(std::string_view study_uid) const
        -> Result<size_t>;

private:
    [[nodiscard]] auto parse_series_row(void* stmt) const -> series_record;
    [[nodiscard]] static auto to_like_pattern(std::string_view pattern)
        -> std::string;
    [[nodiscard]] static auto parse_timestamp(const std::string& str)
        -> std::chrono::system_clock::time_point;

    sqlite3* db_{nullptr};
};

}  // namespace kcenon::pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
