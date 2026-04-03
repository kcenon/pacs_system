// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file series_repository.h
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

#include "kcenon/pacs/storage/series_record.h"

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
